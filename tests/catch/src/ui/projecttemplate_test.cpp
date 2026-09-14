/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "uibench.h"

#include "../qt_catch_tostring.h"

#include "../../../../sources/bordertitleblock.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/environment/projectlock.h"
#include "../../../../sources/environment/qetenvironment.h"
#include "../../../../sources/project/projecttemplate.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetresult.h"

#include <catch2/catch.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

/*
	Starting a project from a template project (T26).

	The template is a whole .qet - the cover sheet, the index and the folios
	the workshop always starts from - and a new project is a copy of it. The
	case that has to hold, and the only one that can destroy work, is the
	third one below: the copy must not keep pointing at the template, or the
	first Ctrl+S of a busy afternoon writes the job over the skeleton, and
	nobody notices until the next job starts from it.

	Everything here is text in a temporary directory, opened by the very
	QETProject the program uses. No screen, and no file of the source tree is
	written to.
*/

namespace
{
	/**
		The template: two folios of different sizes, title blocks filled in,
		project-wide defaults of its own, and - deliberately - the two title
		block variables that name the file it was saved as. Those last two are
		what a template dragged along until now.
	*/
	QString templateXml()
	{
		return QStringLiteral(
			       "<project version=\"0.80\" title=\"MODELO PADRÃO\">"
			       "<properties>"
			       "<property name=\"savedfilename\" show=\"1\">modelo-padrao</property>"
			       "<property name=\"savedfilepath\" show=\"1\">/partilha/modelos/modelo-padrao.qet</property>"
			       "</properties>"
			       "<newdiagrams>"
			       "<border cols=\"13\" colsize=\"60\" rows=\"9\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\"/>"
			       "<inset author=\"Projetista\" plant=\"QUADRO GERAL\" displayAt=\"bottom\"/>"
			       "</newdiagrams>"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"Capa\" order=\"1\" author=\"Projetista\""
			       " plant=\"QUADRO GERAL\" height=\"600\""
			       " cols=\"10\" colsize=\"60\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "<diagram title=\"Esquema\" order=\"2\" author=\"Projetista\""
			       " plant=\"QUADRO GERAL\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	/// The bytes of a file, so that "the template was not touched" is a
	/// comparison and not an impression.
	QByteArray bytesOf(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QByteArray();
		}
		return file.readAll();
	}

	/**
		A template file written into a directory of its own.

		Not UiBench::ScratchProject, which opens what it writes: here the
		file has to stay closed, because the whole point is what happens to a
		project that was never opened as itself.
	*/
	class TemplateFile
	{
		public:
			explicit TemplateFile(const QString &xml = templateXml(),
					      const QString &file_name
					      = QStringLiteral("modelo.qet"))
			{
				QETProject::setBackupEnabled(false);

				if (!m_dir.isValid()) {
					m_error = QStringLiteral("no temporary directory: %1")
						  .arg(m_dir.errorString());
					return;
				}

				m_path = QDir(m_dir.path()).absoluteFilePath(file_name);
				QFile file(m_path);
				if (!file.open(QIODevice::WriteOnly)) {
					m_error = QStringLiteral("cannot write %1: %2")
						  .arg(m_path, file.errorString());
					return;
				}
				file.write(xml.toUtf8());
				file.close();
			}

			bool isValid() const {return m_error.isEmpty();}
			QString error() const {return m_error;}
			QString path() const {return m_path;}
			QString siblingPath(const QString &file_name) const
			{
				return QDir(m_dir.path()).absoluteFilePath(file_name);
			}

		private:
			QTemporaryDir m_dir;
			QString m_path;
			QString m_error;
	};

	/// Puts the environment setting back where it found it.
	class SettingsGuard
	{
		public:
			SettingsGuard()
			{
				QSettings settings;
				m_previous = settings.value(QStringLiteral("environment/path")).toString();
			}

			~SettingsGuard()
			{
				QSettings settings;
				if (m_previous.isEmpty()) {
					settings.remove(QStringLiteral("environment/path"));
				} else {
					settings.setValue(QStringLiteral("environment/path"), m_previous);
				}
			}

		private:
			QString m_previous;
	};
}

TEST_CASE("T26 — o projeto criado a partir de um modelo não aponta para o modelo",
	  "[projecttemplate][T26]")
{
	TemplateFile source;
	INFO(source.error().toStdString());
	REQUIRE(source.isValid());

	QString error;
	QETProject *project = ProjectTemplate::openAsNewProject(source.path(),
								nullptr, &error);
	INFO(error.toStdString());
	REQUIRE(project != nullptr);
	CHECK(error.isEmpty());

		//The one thing that keeps a template from being overwritten: with no
		//path, ProjectView::doSave() turns "Save" into "Save as".
	CHECK(project->filePath().isEmpty());

		//And the backstop under it: write() refuses outright rather than
		//picking a file of its own.
	const QETResult written = project->write();
	CHECK_FALSE(written.isOk());

		//projectOptionsWereModified() is the m_modified flag itself; what it
		//says here is that closing this project asks before throwing it away.
	CHECK(project->projectOptionsWereModified());

		//Nothing was written beside the template either: no in-use lock is
		//taken on it, because it was read and not opened.
	CHECK(QFileInfo::exists(source.path()));
	CHECK_FALSE(ProjectLock(source.path()).exists());

	delete project;
}

TEST_CASE("T26 — o modelo entrega as folhas, as bordas e os carimbos prontos",
	  "[projecttemplate][T26]")
{
	TemplateFile source;
	REQUIRE(source.isValid());

	QString error;
	QETProject *project = ProjectTemplate::openAsNewProject(source.path(),
								nullptr, &error);
	INFO(error.toStdString());
	REQUIRE(project != nullptr);

		//The folios of the template, in the order the template has them.
	REQUIRE(project->diagrams().count() == 2);
	Diagram *cover = project->diagrams().at(0);
	Diagram *schematic = project->diagrams().at(1);
	CHECK(cover->title() == QStringLiteral("Capa"));
	CHECK(schematic->title() == QStringLiteral("Esquema"));

		//The border of each folio, which is what makes a cover sheet look
		//like a cover sheet and not like a schematic.
	CHECK(cover->border_and_titleblock.columnsCount() == 10);
	CHECK(cover->border_and_titleblock.rowsCount() == 6);
	CHECK(schematic->border_and_titleblock.columnsCount() == 17);
	CHECK(schematic->border_and_titleblock.rowsCount() == 8);

		//The title block properties of each folio.
	CHECK(cover->border_and_titleblock.author() == QStringLiteral("Projetista"));
	CHECK(schematic->border_and_titleblock.plant() == QStringLiteral("QUADRO GERAL"));

		//And the project-wide defaults, which is the half that copying folio
		//by folio - the way duplicateDiagram() does inside one project -
		//cannot carry: the next folio added to this project is born with the
		//size and the title block the template prescribes.
	CHECK(project->defaultBorderProperties().columns_count == 13);
	CHECK(project->defaultBorderProperties().rows_count == 9);
	CHECK(project->defaultTitleBlockProperties().plant == QStringLiteral("QUADRO GERAL"));

	delete project;
}

TEST_CASE("T26 — salvar o projeto novo escreve outro arquivo e não toca no modelo",
	  "[projecttemplate][T26]")
{
	TemplateFile source;
	REQUIRE(source.isValid());

	const QByteArray before = bytesOf(source.path());
	REQUIRE_FALSE(before.isEmpty());

	QString error;
	QETProject *project = ProjectTemplate::openAsNewProject(source.path(),
								nullptr, &error);
	INFO(error.toStdString());
	REQUIRE(project != nullptr);

		//What the user does next: "Save" asks where, and this is the answer.
	const QString job = source.siblingPath(QStringLiteral("obra-do-cliente.qet"));
	project->setFilePath(job);
	const QETResult written = project->write();
	INFO(written.errorMessage().toStdString());
	CHECK(written.isOk());
	CHECK(QFileInfo::exists(job));

		//The template, byte for byte. This is the case the whole step exists
		//for: an afternoon of drawing followed by Ctrl+S used to land here.
	CHECK(bytesOf(source.path()) == before);

	delete project;

		//And what was written is a project of its own, with the folios of
		//the template in it.
	QETProject saved(job);
	REQUIRE(saved.state() == QETProject::Ok);
	CHECK(saved.diagrams().count() == 2);
	CHECK(saved.filePath() == job);
}

TEST_CASE("T26 — o nome do arquivo do modelo não vaza para as variáveis do carimbo",
	  "[projecttemplate][T26]")
{
		//A title block that prints %{savedfilename} would otherwise print
		//the name of the template on every folio of every job started from
		//it - the kind of defect that is only seen on paper, after printing.
	TemplateFile source;
	REQUIRE(source.isValid());

	QString error;
	QETProject *project = ProjectTemplate::openAsNewProject(source.path(),
								nullptr, &error);
	INFO(error.toStdString());
	REQUIRE(project != nullptr);

	CHECK(project->projectProperties().value(QStringLiteral("savedfilename"))
	      .toString().isEmpty());
	CHECK(project->projectProperties().value(QStringLiteral("savedfilepath"))
	      .toString().isEmpty());

		//The title of the template does come across, and on purpose: it is
		//content the workshop wrote, and the project properties dialog is
		//where it gets changed.
	CHECK(project->title() == QStringLiteral("MODELO PADRÃO"));

	delete project;
}

TEST_CASE("T26 — desligar um projeto do arquivo dele é uma operação própria",
	  "[projecttemplate][T26]")
{
		//detachFromFile() is exercised on its own here, and not only through
		//openAsNewProject(), because it is what a future "file this project
		//as the house template" would reuse - and because the signal is the
		//only way to watch the flag being set.
	UiBench::ScratchProject bench(templateXml(), QStringLiteral("aberto.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	QETProject *project = bench.project();
	REQUIRE_FALSE(project->filePath().isEmpty());

	QSignalSpy modified(project, &QETProject::projectModified);
	QSignalSpy path_changed(project, &QETProject::projectFilePathChanged);
		//A spy that failed to connect counts zero, which reads exactly like
		//a signal that was never emitted.
	REQUIRE(modified.isValid());
	REQUIRE(path_changed.isValid());
		//The project was just read from a file and nobody has touched it,
		//so the flag below is being watched from a known state.
	REQUIRE_FALSE(project->projectOptionsWereModified());

	ProjectTemplate::detachFromFile(project);

	CHECK(project->filePath().isEmpty());
	CHECK(path_changed.count() == 1);
	CHECK(project->projectOptionsWereModified());
	REQUIRE(modified.count() >= 1);
	CHECK(modified.last().at(1).toBool());

		//Twice is harmless: there is no path left to drop, so nothing is
		//said a second time.
	const int said = path_changed.count();
	ProjectTemplate::detachFromFile(project);
	CHECK(path_changed.count() == said);
}

TEST_CASE("T26 — um modelo somente leitura produz um projeto editável",
	  "[projecttemplate][T26]")
{
		//The house template lives on a share where most people may only read
		//it - that is what makes it the house template. A project born from
		//it that inherited read-only would be one nobody can draw in.
	TemplateFile source;
	REQUIRE(source.isValid());

	const QFile::Permissions original = QFile::permissions(source.path());
	QFile::setPermissions(source.path(),
			      QFile::ReadOwner | QFile::ReadUser | QFile::ReadGroup);

	if (QFileInfo(source.path()).isWritable())
	{
			//Calibration before assertion: a filesystem that ignores the
			//read-only bit would make this case prove nothing at all, and
			//saying so is better than a green tick.
		WARN("this filesystem keeps the file writable; read-only template not exercised");
	}
	else
	{
		QString error;
		QETProject *project = ProjectTemplate::openAsNewProject(source.path(),
									nullptr, &error);
		CHECK(project != nullptr);
		if (project)
		{
			CHECK_FALSE(project->isReadOnly());
			delete project;
		}
	}

		//Put the bit back, or the temporary directory cannot be emptied.
	QFile::setPermissions(source.path(), original);
}

TEST_CASE("T26 — um modelo ilegível é recusado com uma razão, e não meio aberto",
	  "[projecttemplate][T26]")
{
	SECTION("um arquivo que não existe")
	{
		TemplateFile source;
		REQUIRE(source.isValid());

		QString error;
		QETProject *project = ProjectTemplate::openAsNewProject(
					      source.siblingPath(QStringLiteral("nao-existe.qet")),
					      nullptr, &error);
		CHECK(project == nullptr);
		CHECK_FALSE(error.isEmpty());
		CHECK(error.contains(QStringLiteral("nao-existe.qet")));
	}

	SECTION("um arquivo que não é um projeto")
	{
		TemplateFile source(QStringLiteral("isto não é um projeto"),
				    QStringLiteral("qualquer.qet"));
		REQUIRE(source.isValid());

		QString error;
		QETProject *project = ProjectTemplate::openAsNewProject(source.path(),
									nullptr, &error);
		CHECK(project == nullptr);
		CHECK_FALSE(error.isEmpty());
	}

	SECTION("nenhum caminho")
	{
		QString error;
		QETProject *project = ProjectTemplate::openAsNewProject(QString(),
									nullptr, &error);
		CHECK(project == nullptr);
		CHECK_FALSE(error.isEmpty());
	}
}

TEST_CASE("T26 — os modelos moram na pasta compartilhada, com o resto",
	  "[projecttemplate][T26][T38]")
{
		//Pointing the program at the shared folder has to bring the
		//templates along with the symbols and the title blocks; a setting of
		//their own would be one more path to get wrong.
	SettingsGuard guard;
	QTemporaryDir share;
	REQUIRE(share.isValid());

	QString error;
	REQUIRE(QETEnvironment::setPath(share.path(), &error));

	const QString templates = ProjectTemplate::templatesDir();
	CHECK(templates.startsWith(QDir(share.path()).absolutePath()));
		//Asked for once, made at once: nobody has to create it by hand
		//before filing the first template.
	CHECK(QFileInfo(templates).isDir());
}
