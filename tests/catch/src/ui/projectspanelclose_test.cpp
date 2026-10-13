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

#include "../../../../sources/diagram.h"
#include "../../../../sources/elementspanel.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetresult.h"
#include "../../../../sources/titleblock/templatescollection.h"

#include <catch2/catch.hpp>

#include <QDir>
#include <QFile>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

/*
	A project closed while another one stays open used to come back to the
	projects panel as a second entry, after the one that stayed. The entry
	held the address of a project about to be deleted, and a right click on
	it read that freed memory: QElectroTech went down with a heap corruption.

	The panel connects to the project when it lists it, and every signal it
	connects to adds the project again when the project is not there - which
	is right while the project is open, and wrong once the panel was told it
	is closed. These cases make the project speak after that moment, and check
	that the panel no longer answers.
*/

namespace
{
	/**
		Records which of the signals the projects panel listens to a project
		sent. A case that expects "the panel did not add it back" must also
		know the project did say something, or it would pass on a project
		that never spoke.
	*/
	class Listener
	{
		public:
			explicit Listener(QETProject *project)
			{
				auto heard = [this](const char *name) {
					return [this, name]() { m_heard << QString::fromLatin1(name); };
				};
				QObject::connect(project, &QETProject::projectInformationsChanged,
						 &m_context, heard("projectInformationsChanged"));
				QObject::connect(project, &QETProject::readOnlyChanged,
						 &m_context, heard("readOnlyChanged"));
				QObject::connect(project, &QETProject::diagramAdded,
						 &m_context, heard("diagramAdded"));
				QObject::connect(project, &QETProject::diagramRemoved,
						 &m_context, heard("diagramRemoved"));
				QObject::connect(project, &QETProject::projectDiagramsOrderChanged,
						 &m_context, heard("projectDiagramsOrderChanged"));
				QObject::connect(project, &QETProject::diagramUsedTemplate,
						 &m_context, heard("diagramUsedTemplate"));
			}

			QStringList heard() const {return m_heard;}

		private:
			QObject m_context;
			QStringList m_heard;
	};
}

TEST_CASE("T48 — A project the panel was told is closed does not come back when it speaks",
	  "[ui][T48]")
{
	UiBench::Project kept(QStringLiteral("741.qet"));
	UiBench::Project closed(QStringLiteral("affuteuse_250h.qet"));
	REQUIRE(kept.isOpen());
	REQUIRE(closed.isOpen());
	REQUIRE(closed.diagramCount() >= 2);

	ElementsPanel panel;
	panel.projectWasOpened(kept.project());
	panel.projectWasOpened(closed.project());
	REQUIRE(panel.itemForProject(kept.project()));
	REQUIRE(panel.itemForProject(closed.project()));

	panel.projectWasClosed(closed.project());
	REQUIRE_FALSE(panel.itemForProject(closed.project()));

	Listener listener(closed.project());

	SECTION("its title changes") {
		closed->setTitle(QStringLiteral("renamed after closing"));
	}
	SECTION("it is marked modified") {
		closed->setModified(true);
	}
	SECTION("it turns read-only") {
		closed->setReadOnly(true);
	}
	SECTION("a folio is added") {
		closed->addNewDiagram();
	}
	SECTION("a folio is removed") {
		closed->removeDiagram(closed.diagram(1));
	}

	INFO("signals the project sent: " << listener.heard().join(QStringLiteral(", ")).toStdString());
	REQUIRE_FALSE(listener.heard().isEmpty());
	CHECK_FALSE(panel.itemForProject(closed.project()));
	CHECK(panel.itemForProject(kept.project()));
}

TEST_CASE("T48 — The title block templates of a closed project are not listed again",
	  "[ui][T48]")
{
	UiBench::Project closed(QStringLiteral("affuteuse_250h.qet"));
	REQUIRE(closed.isOpen());
	TitleBlockTemplatesProjectCollection *templates =
			closed->embeddedTitleBlockTemplatesCollection();
	REQUIRE(templates);

	ElementsPanel panel;
	panel.projectWasOpened(closed.project());
	REQUIRE(panel.itemForTemplatesCollection(templates));

	panel.projectWasClosed(closed.project());
	REQUIRE_FALSE(panel.itemForTemplatesCollection(templates));

		//The collection dies with the project: an entry made for it now
		//would outlive it, the way the project entry did
	int changes = 0;
	QObject context;
	QObject::connect(templates, &TitleBlockTemplatesCollection::changed,
			 &context, [&changes]() { ++changes; });
	templates->removeTemplate(QStringLiteral("no such template"));

	REQUIRE(changes == 1);
	CHECK_FALSE(panel.itemForTemplatesCollection(templates));
}

TEST_CASE("T48 — Saved at closing and then destroyed, a project leaves no entry in the panel",
	  "[ui][T48]")
{
	QETProject::setBackupEnabled(false);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = QDir(dir.path()).absoluteFilePath(QStringLiteral("closing.qet"));
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		file.write(UiBench::fileContent(UiBench::examplePath(QStringLiteral("affuteuse_250h.qet"))).toUtf8());
	}

	UiBench::Project kept(QStringLiteral("741.qet"));
	REQUIRE(kept.isOpen());

	auto *project = new QETProject(path);
	REQUIRE(project->state() == QETProject::Ok);

	ElementsPanel panel;
	panel.projectWasOpened(kept.project());
	panel.projectWasOpened(project);

		//What the closing question does when the answer is "save": the
		//project was edited, and is written before the window closes
	project->setModified(true);
	REQUIRE(project->write().isOk());

	panel.projectWasClosed(project);
	REQUIRE_FALSE(panel.itemForProject(project));

		//What deleteLater() does once the event loop comes back. The address
		//is kept only to look it up: the panel's map is keyed by pointer and
		//never dereferences the key
	Listener listener(project);
	QETProject *const address = project;
	delete project;

	INFO("signals the project sent while destroyed: "
	     << listener.heard().join(QStringLiteral(", ")).toStdString());
	CHECK_FALSE(panel.itemForProject(address));
	CHECK(panel.itemForProject(kept.project()));
}
