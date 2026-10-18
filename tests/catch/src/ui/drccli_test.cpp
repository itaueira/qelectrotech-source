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

#include "../../../../sources/cli_export.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QFile>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

/*
	The design rule check from the command line (--check-project), the way
	a build server runs it: a project on disk, an exit code, and a report a
	script reads line by line.

	The fixture is ShellyParts.qet, an example of the source tree: three
	folios, twelve simple symbols, twelve different tags and no conductor.
	Neither of the rules born as errors - two components with one tag, a
	potential with two wire numbers - has anything to say about it as
	shipped; drctagrules_test.cpp proves that silence on the engine, and
	here it is what makes the clean project clean. The error is planted in
	the text of the file and nowhere else: the tag of SHPRO1, on folio 3,
	is rewritten SHSW1, the tag of a component of folio 1. Nothing is
	opened in this process: the command line opens the project itself, as
	it does on a build server.

	What is asserted is the exit code and the lines, never how many
	findings there are. Every rule the checker gains adds lines to a
	project like this one, and a count would turn each of them into a
	failure here that says nothing about the command line.

	The report is read from the file it can be written to, because the
	standard output of this process belongs to the test runner. The lines
	on the standard output are the same list, written by the same loop.
*/

namespace
{
	/// The example the cases run on, as the source tree ships it.
	QString fixtureName()
	{
		return QStringLiteral("ShellyParts.qet");
	}

	/// The line of a .qet that gives a component its tag.
	QString tagLine(const QString &tag)
	{
		return QStringLiteral(
			"<elementInformation show=\"1\" name=\"label\">%1"
			"</elementInformation>").arg(tag);
	}

	/**
		A symbol file --check-elements passes: a definition of an element,
		with a size and one named terminal.

		Single quotes around the attributes, which XML allows, so that the
		literal carries no escape to get wrong.
	*/
	QString symbolXml()
	{
		return QStringLiteral(
			"<definition type='element' version='0.100.0'"
			" width='20' height='20' hotspot_x='10' hotspot_y='10'"
			" link_type='simple'>"
			"<names><name lang='en'>bench</name></names>"
			"<informations/>"
			"<description>"
			"<line x1='-5' y1='0' x2='5' y2='0' length1='1.5'"
			" length2='1.5' end1='none' end2='none'"
			" style='line-style:normal;line-weight:normal;"
			"filling:none;color:black' antialias='false'/>"
			"<terminal x='0' y='-10' orientation='n' name='1'/>"
			"</description>"
			"</definition>");
	}

	bool writeFile(const QString &path, const QString &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write(content.toUtf8()) >= 0;
	}

	/// The file as it is, in binary: a null string when there is none.
	QString readFile(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QString();
		}
		return QString::fromUtf8(file.readAll());
	}

	/// What one run of --check-project left behind.
	struct CheckRun
	{
		int code = -1;
		bool written = false;
		QString text;
		/// One entry per line of the report, split at the tabs.
		QList<QStringList> lines;
	};

	/**
		Run --check-project on @a project_path, the report written to
		@a report_path, and read the report back.

		The reader is written here and not borrowed from the code under
		test: a line is what comes before a line feed, and a field what
		comes between two tabs. A report ending without its last line feed
		would leave a line the split below keeps, and checkShape() refuses.
	*/
	CheckRun checkOf(const QString &project_path, const QString &report_path)
	{
		CheckRun run;
		run.code = CLIExport::run(QStringList()
					  << QStringLiteral("--check-project")
					  << project_path << report_path);
		run.written = QFile::exists(report_path);
		run.text = readFile(report_path);

		QStringList rows = run.text.split(QLatin1Char('\n'));
		if (!rows.isEmpty() && rows.last().isEmpty()) {
			rows.removeLast();
		}
		for (const QString &row : rows) {
			run.lines << row.split(QLatin1Char('\t'));
		}
		return run;
	}

	/// The lines of @a run whose first field is @a severity.
	QList<QStringList> linesOfSeverity(const CheckRun &run,
					   const QString &severity)
	{
		QList<QStringList> lines;
		for (const QStringList &line : run.lines)
		{
			if (line.value(0) == severity) {
				lines << line;
			}
		}
		return lines;
	}

	/// The lines of @a run whose rule field is @a rule.
	QList<QStringList> linesOfRule(const CheckRun &run, const QString &rule)
	{
		QList<QStringList> lines;
		for (const QStringList &line : run.lines)
		{
			if (line.value(3) == rule) {
				lines << line;
			}
		}
		return lines;
	}

	/**
		The shape every report has, whatever the project: one finding per
		line, a line feed after each and no carriage return, five fields,
		a severity among the three machine names, a folio that is either
		empty or a sheet number, a rule and a sentence.
	*/
	void checkShape(const CheckRun &run)
	{
		CHECK_FALSE(run.text.contains(QLatin1Char('\r')));
		if (!run.text.isEmpty()) {
			CHECK(run.text.endsWith(QLatin1Char('\n')));
		}

		const QStringList severities {QStringLiteral("error"),
					      QStringLiteral("warning"),
					      QStringLiteral("information")};
		for (const QStringList &line : run.lines)
		{
			INFO(line.join(QStringLiteral(" | ")).toStdString());
			REQUIRE(line.size() == 5);
			CHECK(severities.contains(line.at(0)));
			if (!line.at(1).isEmpty())
			{
				bool number = false;
				const int folio = line.at(1).toInt(&number);
				CHECK(number);
				CHECK(folio > 0);
			}
			CHECK_FALSE(line.at(3).isEmpty());
			CHECK_FALSE(line.at(4).isEmpty());
		}
	}

	/**
		The shipped example written to @a clean_path, and the same text
		with the planted error to @a planted_path.

		The plant is counted before it is trusted: a replacement that
		found nothing would hand the error case a clean project, and the
		case would then prove that a clean project fails.

		@return false, having said why, when either file is not what the
		cases expect.
	*/
	bool writeFixtures(const QString &clean_path, const QString &planted_path)
	{
		const QString example = UiBench::examplePath(fixtureName());
		QString reason;
		if (!UiBench::opensWithoutDialog(example, &reason))
		{
			FAIL_CHECK(reason.toStdString());
			return false;
		}
		const QString shipped = UiBench::fileContent(example);
		if (shipped.isEmpty())
		{
			FAIL_CHECK("the example could not be read");
			return false;
		}

		const QString from = tagLine(QStringLiteral("SHPRO1"));
		const QString to = tagLine(QStringLiteral("SHSW1"));
		CHECK(shipped.count(from) == 1);
		CHECK(shipped.count(to) == 1);
		if (shipped.count(from) != 1 || shipped.count(to) != 1) {
			return false;
		}

		QString planted = shipped;
		planted.replace(from, to);
		CHECK(planted.count(to) == 2);
		if (planted.count(to) != 2) {
			return false;
		}

		return writeFile(clean_path, shipped)
		       && writeFile(planted_path, planted);
	}
}

TEST_CASE("CU-23.6 — --check-project sai 1 no projeto com erro e 0 no limpo, "
	  "e o --check-elements continua validando símbolo sem se confundir",
	  "[uibench][drc][cli][t23]")
{
		//The command line of the program runs without backups (main.cpp),
		//and so does this case.
	QETProject::setBackupEnabled(false);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString clean_path = dir.filePath(QStringLiteral("clean.qet"));
	const QString planted_path = dir.filePath(QStringLiteral("planted.qet"));
	REQUIRE(writeFixtures(clean_path, planted_path));

		//The flag is one the program hands to the command line, before
		//any window is built - main.cpp asks this very function.
	CHECK(CLIExport::isExportRequest(QStringList()
					 << QStringLiteral("--check-project")
					 << clean_path));

	SECTION("projeto limpo: 0, e os avisos saem listados sem reprovar")
	{
		const CheckRun run =
			checkOf(clean_path, dir.filePath(QStringLiteral("clean.txt")));
		INFO(run.text.toStdString());
		CHECK(run.code == 0);
		REQUIRE(run.written);
		checkShape(run);

		CHECK(linesOfSeverity(run, QStringLiteral("error")).isEmpty());
			//What makes the 0 above say "warnings do not fail", and
			//not merely "nothing was found": the example raises
			//warnings of the factory profile. Which ones is the
			//business of the cases of each rule.
		CHECK_FALSE(linesOfSeverity(run, QStringLiteral("warning")).isEmpty());
	}

	SECTION("dois componentes com a mesma tag: 1, e a linha do achado diz a "
		"severidade, a folha, a casa, a regra e a tag")
	{
		const CheckRun run =
			checkOf(planted_path, dir.filePath(QStringLiteral("planted.txt")));
		INFO(run.text.toStdString());
		CHECK(run.code == 1);
		REQUIRE(run.written);
		checkShape(run);

			//The machine name written out, and not asked of the rule: it
			//is what a script greps the report for, and renaming it
			//breaks every script that does.
		const QList<QStringList> duplicates =
				linesOfRule(run, QStringLiteral("duplicate_tag"));
		REQUIRE(duplicates.count() == 1);
		const QStringList &line = duplicates.first();
		INFO(line.join(QStringLiteral(" | ")).toStdString());
		CHECK(line.at(0) == QStringLiteral("error"));
			//The finding points at one of the two components that carry
			//the tag: SHSW1 on folio 1, the plant on folio 3.
		CHECK((line.at(1) == QStringLiteral("1")
		       || line.at(1) == QStringLiteral("3")));
		CHECK_FALSE(line.at(2).isEmpty());
		CHECK(line.at(4).contains(QStringLiteral("SHSW1")));
	}

	SECTION("sem arquivo de relatório, a lista vai para a saída padrão e o "
		"código é o mesmo")
	{
		CHECK(CLIExport::run(QStringList()
				     << QStringLiteral("--check-project")
				     << clean_path) == 0);
		CHECK(CLIExport::run(QStringList()
				     << QStringLiteral("--check-project")
				     << planted_path) == 1);
	}

	SECTION("--check-elements no mesmo projeto: lê como arquivo de símbolo, "
		"recusa, e um símbolo continua passando")
	{
			//The very file the new flag passes with 0. The old one reads
			//it as a symbol file, which it is not - what it always did
			//with a project.
		CHECK(CLIExport::run(QStringList()
				     << QStringLiteral("--check-elements")
				     << clean_path) == 1);

		const QString symbol_path = dir.filePath(QStringLiteral("bench.elmt"));
		REQUIRE(writeFile(symbol_path, symbolXml()));
		CHECK(CLIExport::run(QStringList()
				     << QStringLiteral("--check-elements")
				     << symbol_path) == 0);
	}
}

TEST_CASE("T23 — --check-project recusa uso errado com 2, e o que não chega a "
	  "um veredito com 3",
	  "[uibench][drc][cli][t23]")
{
	QETProject::setBackupEnabled(false);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString clean_path = dir.filePath(QStringLiteral("clean.qet"));
	const QString planted_path = dir.filePath(QStringLiteral("planted.qet"));
	REQUIRE(writeFixtures(clean_path, planted_path));
	const QString report_path = dir.filePath(QStringLiteral("report.txt"));

	SECTION("sem projeto: 2")
	{
		CHECK(CLIExport::run(QStringList()
				     << QStringLiteral("--check-project")) == 2);
	}

	SECTION("projeto que não existe: 2, como os outros verbos, e nenhum "
		"relatório")
	{
		const int code = CLIExport::run(
			QStringList() << QStringLiteral("--check-project")
				      << dir.filePath(QStringLiteral("none.qet"))
				      << report_path);
		CHECK(code == 2);
		CHECK_FALSE(QFile::exists(report_path));
	}

	SECTION("opção no lugar do relatório: 2, e não um arquivo com o nome dela")
	{
		const int code = CLIExport::run(
			QStringList() << QStringLiteral("--check-project")
				      << planted_path
				      << QStringLiteral("--errors-only"));
		CHECK(code == 2);
			//Taken for a report, it would have been written where the
			//run stands.
		CHECK_FALSE(QFile::exists(QStringLiteral("--errors-only")));
	}

	SECTION("argumento a mais: 2, e nenhum relatório")
	{
		const int code = CLIExport::run(
			QStringList() << QStringLiteral("--check-project")
				      << planted_path << report_path
				      << QStringLiteral("extra"));
		CHECK(code == 2);
		CHECK_FALSE(QFile::exists(report_path));
	}

	SECTION("projeto ilegível: 3, e não o 1 de quem achou erro")
	{
		const QString broken_path = dir.filePath(QStringLiteral("broken.qet"));
		REQUIRE(writeFile(broken_path,
				  QStringLiteral("this is not a project")));
		const int code = CLIExport::run(
			QStringList() << QStringLiteral("--check-project")
				      << broken_path << report_path);
		CHECK(code == 3);
		CHECK_FALSE(QFile::exists(report_path));
	}

	SECTION("relatório que não pode ser escrito: 3 mesmo com erro achado, "
		"e nenhum arquivo")
	{
			//A folder that does not exist. The project has an error, and
			//the code is still 3: a 1 would send whoever reads it to a
			//report that is not there.
		const QString unwritable =
			dir.filePath(QStringLiteral("missing/report.txt"));
		const int code = CLIExport::run(
			QStringList() << QStringLiteral("--check-project")
				      << planted_path << unwritable);
		CHECK(code == 3);
		CHECK_FALSE(QFile::exists(unwritable));
	}
}
