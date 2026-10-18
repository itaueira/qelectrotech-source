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
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QChar>
#include <QDomDocument>
#include <QFile>
#include <QList>
#include <QLocale>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

/*
	Two purchase lists of one project, exported before and after a change,
	and compared from the command line (--compare-purchase) the way a shop
	keeps the list it ordered from and asks what the revision changed.

	The project is ShellyParts.qet, an example of the source tree: three
	folios, twelve components, no conductor and no part code anywhere. The
	parts are planted in the text of the file and nowhere else - three
	components given a code, and a mounting plate with two pieces of rail
	cut on it, 2.5 m in all, so that the list carries a line by the metre.
	The change is planted the same way, and it is three items:

	- SHSW1 ("Shelly 1", SH-0100) is deleted from its folio: a line goes;
	- SHPLUS1, which had no part, is given SH-0101 and described with the
	  same words as the one deleted, "Shelly 1": a line comes, and a
	  comparison by description would see nothing at all;
	- SHPRO2 (SH-0200) goes from one piece to three: a line changes.

	SHDM2 (SH-0300) and the rail stay as they were, and are the lines the
	report must not mention.

	Both lists are written by --export-purchase in this process - the
	project opened by the command line itself, as on a build server - and
	read back by a reader written in this file, never by the code under
	test. The report is read from the file it can be written to, because
	the standard output of this process belongs to the test runner.

	The rule itself - the key, the separators, the refusals - is proved on
	texts written by hand in purchasecomparison_test.cpp, in the pure
	suite. What is proved here is the road: the export the program writes
	is the file the comparison reads.
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
		Where the informations of the component tagged @a tag are in
		@a xml: from its <elementInformations> up to, and not including,
		the closing tag.

		@return false when the tag is not on exactly one component
	*/
	bool informationsOf(const QString &xml, const QString &tag,
			    int *start, int *end)
	{
		const QString label = tagLine(tag);
		if (xml.count(label) != 1) {
			return false;
		}
		const int at = int(xml.indexOf(label));
		*start = int(xml.lastIndexOf(QStringLiteral("<elementInformations>"), at));
		*end = int(xml.indexOf(QStringLiteral("</elementInformations>"), at));
		return *start >= 0 && *end > at;
	}

	/**
		Set the information @a name of the component tagged @a tag to
		@a value: replaced when the component has one, added otherwise.

		@return false, the text untouched, when the component is not found
		or names that information twice
	*/
	bool setInformation(QString *xml, const QString &tag,
			    const QString &name, const QString &value)
	{
		int start = -1;
		int end = -1;
		if (!informationsOf(*xml, tag, &start, &end)) {
			return false;
		}
		QString block = xml->mid(start, end - start);

		const QString opening = QStringLiteral("name=\"%1\">").arg(name);
		const QString closing = QStringLiteral("</elementInformation>");
		if (block.count(opening) > 1) {
			return false;
		}
		const int found = int(block.indexOf(opening));
		if (found >= 0)
		{
			const int value_start = found + int(opening.size());
			const int value_end = int(block.indexOf(closing, value_start));
			if (value_end < 0) {
				return false;
			}
			block.replace(value_start, value_end - value_start,
				      value.toHtmlEscaped());
		}
		else
		{
			block += QStringLiteral(
				"<elementInformation show=\"0\" name=\"%1\">%2"
				"</elementInformation>")
				.arg(name, value.toHtmlEscaped());
		}
		xml->replace(start, end - start, block);
		return true;
	}

	/**
		Delete the component tagged @a tag from its folio: its whole
		<element>, terminals and texts included. The example draws no
		conductor, so no wire is left pointing at it.

		@return false, the text untouched, when the tag is not on exactly
		one component
	*/
	bool removeElement(QString *xml, const QString &tag)
	{
		const QString label = tagLine(tag);
		if (xml->count(label) != 1) {
			return false;
		}
		const int at = int(xml->indexOf(label));
		const int start = int(xml->lastIndexOf(QStringLiteral("<element "), at));
		const QString closing = QStringLiteral("</element>");
		const int end = int(xml->indexOf(closing, at));
		if (start < 0 || end < 0) {
			return false;
		}
		xml->remove(start, end + int(closing.size()) - start);
		return xml->count(label) == 0;
	}

	/// The rail the plate is cut from: 35 wide, 7,5 deep
	MountingProfile standardRail()
	{
		return MountingProfile::rail(35.0, 7.5);
	}

	/**
		@return the <mounting_layout> of the fixture: one plate with two
		pieces of rail, 1100 and 1400 long, and no code - the line of a
		bar is keyed by its profile. Empty when the layout refused them.

		Built through MountingLayout and written by its own toXml(), as
		purchaselist_test.cpp builds its plates: the file a person's
		plate is saved as is what the export reads.
	*/
	QString plateXml()
	{
		MountingLayout layout;
		const QString face = layout.appendSurface(
			MountingSurface(QStringLiteral("QCM1/PLACA"),
					QStringLiteral("plate"),
					MountingArea(1600.0, 1000.0)));
		if (face.isEmpty()) {
			return QString();
		}
		const MountingProfile rail = standardRail();
		const qreal lengths[] = {1100.0, 1400.0};
		qreal y = 200.0;
		for (const qreal length : lengths)
		{
			MountedItem item;
			item.profile = rail;
			item.run = MountingRun::Across;
			item.position = QPointF(80.0, y);
			item.size = rail.sizeFor(length, MountingRun::Across);
			if (layout.mountItem(face, item).isEmpty()) {
				return QString();
			}
			y += 200.0;
		}

		QDomDocument document;
		document.appendChild(layout.toXml(document));
		return document.toString(-1);
	}

	/**
		The example as shipped, with the parts of the list before the
		change planted in it: SHSW1, SHPRO2 and SHDM2 given a code, and the
		plate. Each plant is counted before it is trusted - a replacement
		that found nothing would hand the case a project with no list.

		@return the text, empty when a plant missed, having said where
	*/
	QString beforeXml()
	{
		const QString example = UiBench::examplePath(fixtureName());
		QString reason;
		if (!UiBench::opensWithoutDialog(example, &reason))
		{
			FAIL_CHECK(reason.toStdString());
			return QString();
		}
		QString xml = UiBench::fileContent(example);
		if (xml.isEmpty())
		{
			FAIL_CHECK("the example could not be read");
			return QString();
		}

		const QString code = QStringLiteral("part_code");
		if (!setInformation(&xml, QStringLiteral("SHSW1"), code,
				    QStringLiteral("SH-0100"))
		    || !setInformation(&xml, QStringLiteral("SHPRO2"), code,
				       QStringLiteral("SH-0200"))
		    || !setInformation(&xml, QStringLiteral("SHDM2"), code,
				       QStringLiteral("SH-0300")))
		{
			FAIL_CHECK("a part code could not be planted");
			return QString();
		}
		if (xml.count(QStringLiteral("name=\"part_code\"")) != 3)
		{
			FAIL_CHECK("the example carries part codes of its own");
			return QString();
		}

		const QString plate = plateXml();
		const QString anchor = QStringLiteral("</newdiagrams>");
		if (plate.isEmpty() || xml.count(anchor) != 1)
		{
			FAIL_CHECK("the plate could not be planted");
			return QString();
		}
		xml.replace(anchor, anchor + plate);
		return xml;
	}

	/**
		@return @a before with the three items changed: SHSW1 deleted,
		SHPLUS1 given SH-0101 and the words of the one deleted, SHPRO2
		from one piece to three. Empty when a change missed.
	*/
	QString afterXml(const QString &before)
	{
		QString xml = before;
		if (!removeElement(&xml, QStringLiteral("SHSW1"))
		    || !setInformation(&xml, QStringLiteral("SHPLUS1"),
				       QStringLiteral("part_code"),
				       QStringLiteral("SH-0101"))
		    || !setInformation(&xml, QStringLiteral("SHPLUS1"),
				       QStringLiteral("designation"),
				       QStringLiteral("Shelly 1"))
		    || !setInformation(&xml, QStringLiteral("SHPRO2"),
				       QStringLiteral("quantity"),
				       QStringLiteral("3")))
		{
			FAIL_CHECK("a change could not be planted");
			return QString();
		}
		if (xml.count(QStringLiteral("name=\"part_code\"")) != 3)
		{
			FAIL_CHECK("the change did not leave three codes");
			return QString();
		}
		return xml;
	}

	bool writeFile(const QString &path, const QString &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write(content.toUtf8()) >= 0;
	}

	/**
		@return the file as it is, a null string when there is none

		Read in binary, and not as text: the end of each row is part of
		what is asserted.
	*/
	QString readFile(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QString();
		}
		return QString::fromUtf8(file.readAll());
	}

	/**
		The reader of this file: ';' between cells, '"' around the cells
		that need it, CR before the LF that ends a row. Written here and
		not borrowed from the code under test, so that the files are
		checked against the format rather than against the comparison's
		own idea of it. An empty row comes back as a row of one empty
		cell, as a spreadsheet would show it.
	*/
	QVector<QStringList> parseCsv(const QString &text)
	{
		QVector<QStringList> rows;
		QStringList row;
		QString cell;
		bool quoted = false;
		bool cell_started = false;

		for (int i = 0 ; i < text.size() ; ++i)
		{
			const QChar c = text.at(i);

			if (quoted)
			{
				if (c == QLatin1Char('"'))
				{
					if (i + 1 < text.size()
					    && text.at(i + 1) == QLatin1Char('"'))
					{
						cell += QLatin1Char('"');
						++i;
					}
					else
					{
						quoted = false;
					}
					continue;
				}
				cell += c;
				continue;
			}

			if (c == QLatin1Char('"') && !cell_started)
			{
				quoted = true;
				cell_started = true;
				continue;
			}
			if (c == QLatin1Char(';'))
			{
				row << cell;
				cell.clear();
				cell_started = false;
				continue;
			}
			if (c == QLatin1Char('\r')) {
				continue;
			}
			if (c == QLatin1Char('\n'))
			{
				row << cell;
				cell.clear();
				cell_started = false;
				rows << row;
				row.clear();
				continue;
			}
			cell += c;
			cell_started = true;
		}
		if (cell_started || !row.isEmpty())
		{
			row << cell;
			rows << row;
		}
		return rows;
	}

	/// What one run of --export-purchase left behind.
	struct PurchaseFile
	{
		int code = -1;
		bool written = false;
		QString text;
		/// The rows of the lines block, its header left out
		QVector<QStringList> lines;
	};

	/// The first row of the lines block, written here and not taken from
	/// the code under test
	QStringList lineHeader()
	{
		return QStringList {QStringLiteral("part_code"),
				    QStringLiteral("part_revision"),
				    QStringLiteral("designation"),
				    QStringLiteral("quantity"),
				    QStringLiteral("unit"),
				    QStringLiteral("tags")};
	}

	/// Export the purchase list of @a project_path to @a csv_path.
	PurchaseFile exportOf(const QString &project_path, const QString &csv_path)
	{
		PurchaseFile file;
		file.code = CLIExport::run(QStringList()
					   << QStringLiteral("--export-purchase")
					   << project_path << csv_path);
		file.written = QFile::exists(csv_path);
		file.text = readFile(csv_path);

		const QVector<QStringList> rows = parseCsv(file.text);
		const int header = int(rows.indexOf(lineHeader()));
		if (header < 0) {
			return file;
		}
		for (int i = header + 1 ; i < rows.size() ; ++i)
		{
			const QStringList &row = rows.at(i);
			if (row.size() == 1 && row.first().isEmpty()) {
				break;
			}
			file.lines << row;
		}
		return file;
	}

	/// @return the cell under @a column of the line of @a code; a null
	/// string when the file has no such line
	QString lineCell(const PurchaseFile &file, const QString &code,
			 const QString &column)
	{
		const int index = int(lineHeader().indexOf(column));
		for (const QStringList &row : file.lines)
		{
			if (row.value(0) == code) {
				return row.value(index);
			}
		}
		return QString();
	}

	/// @return the cells under @a column of every line, sorted
	QStringList columnOf(const PurchaseFile &file, const QString &column)
	{
		const int index = int(lineHeader().indexOf(column));
		QStringList cells;
		for (const QStringList &row : file.lines) {
			cells << row.value(index);
		}
		cells.sort();
		return cells;
	}

	/// What one run of --compare-purchase left behind.
	struct Comparison
	{
		int code = -1;
		bool written = false;
		QString text;
		QVector<QStringList> rows;
	};

	/// Compare @a before with @a after, the report written to @a report.
	Comparison compareOf(const QString &before, const QString &after,
			     const QString &report)
	{
		Comparison run;
		run.code = CLIExport::run(QStringList()
					  << QStringLiteral("--compare-purchase")
					  << before << after << report);
		run.written = QFile::exists(report);
		run.text = readFile(report);
		run.rows = parseCsv(run.text);
		return run;
	}

	/// The first row of the report, written out
	QStringList reportHeader()
	{
		return QStringList {QStringLiteral("change"),
				    QStringLiteral("part_code"),
				    QStringLiteral("part_revision"),
				    QStringLiteral("designation"),
				    QStringLiteral("quantity_before"),
				    QStringLiteral("unit_before"),
				    QStringLiteral("quantity_after"),
				    QStringLiteral("unit_after")};
	}

	/**
		The shape of every report: the format of the export - CRLF after
		every row, no byte order mark - and every row as wide as the
		header, which is the first row.
	*/
	void checkShape(const Comparison &run)
	{
		CHECK_FALSE(run.text.startsWith(QChar(0xFEFF)));
		CHECK(run.text.endsWith(QStringLiteral("\r\n")));
		CHECK(run.text.count(QStringLiteral("\r\n"))
		      == run.text.count(QLatin1Char('\n')));
		REQUIRE_FALSE(run.rows.isEmpty());
		CHECK(run.rows.first() == reportHeader());
		for (const QStringList &row : run.rows) {
			CHECK(row.size() == reportHeader().size());
		}
	}

	/**
		The three rows the change has to come to, and no other, in the
		order of the report: removed, added, changed. The units are the
		ones each export wrote for the line, read from the file and not
		assumed - the example gives most components a unit of "1".
	*/
	void checkThreeChanges(const Comparison &run,
			       const PurchaseFile &before,
			       const PurchaseFile &after)
	{
		REQUIRE(run.rows.size() == 4);
		CHECK(run.rows.at(1) == QStringList({
			QStringLiteral("removed"), QStringLiteral("SH-0100"),
			QString(), QStringLiteral("Shelly 1"),
			QStringLiteral("1"),
			lineCell(before, QStringLiteral("SH-0100"),
				 QStringLiteral("unit")),
			QString(), QString()}));
		CHECK(run.rows.at(2) == QStringList({
			QStringLiteral("added"), QStringLiteral("SH-0101"),
			QString(), QStringLiteral("Shelly 1"),
			QString(), QString(),
			QStringLiteral("1"),
			lineCell(after, QStringLiteral("SH-0101"),
				 QStringLiteral("unit"))}));
		CHECK(run.rows.at(3) == QStringList({
			QStringLiteral("changed"), QStringLiteral("SH-0200"),
			QString(), QStringLiteral("Shelly Pro 2"),
			QStringLiteral("1"),
			lineCell(before, QStringLiteral("SH-0200"),
				 QStringLiteral("unit")),
			QStringLiteral("3"),
			lineCell(after, QStringLiteral("SH-0200"),
				 QStringLiteral("unit"))}));
	}

	/// The default locale of the process for one scope, the previous
	/// one back at its end however the scope ends.
	class DefaultLocale
	{
		public:
			explicit DefaultLocale(const QLocale &locale)
				: m_previous(QLocale())
			{
				QLocale::setDefault(locale);
			}
			~DefaultLocale()
			{
				QLocale::setDefault(m_previous);
			}
			DefaultLocale(const DefaultLocale &) = delete;
			DefaultLocale &operator=(const DefaultLocale &) = delete;

		private:
			QLocale m_previous;
	};
}

TEST_CASE("CU-18.13 — reexportar depois de trocar três itens: a comparação "
	  "mostra exatamente a saída, a entrada e a mudança de quantidade",
	  "[uibench][purchase][compare][cli][t18]")
{
		//The command line of the program runs without backups (main.cpp),
		//and so does this case.
	QETProject::setBackupEnabled(false);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString before_qet = dir.filePath(QStringLiteral("before.qet"));
	const QString after_qet = dir.filePath(QStringLiteral("after.qet"));
	const QString before_csv = dir.filePath(QStringLiteral("before.csv"));
	const QString after_csv = dir.filePath(QStringLiteral("after.csv"));
	const QString report = dir.filePath(QStringLiteral("report.csv"));

	const QString before_xml = beforeXml();
	REQUIRE_FALSE(before_xml.isEmpty());
	const QString after_xml = afterXml(before_xml);
	REQUIRE_FALSE(after_xml.isEmpty());
	REQUIRE(writeFile(before_qet, before_xml));
	REQUIRE(writeFile(after_qet, after_xml));

	CHECK(CLIExport::isExportRequest(QStringList()
					 << QStringLiteral("--compare-purchase")
					 << before_csv << after_csv));

	const PurchaseFile before = exportOf(before_qet, before_csv);
	const PurchaseFile after = exportOf(after_qet, after_csv);
	INFO(before.text.toStdString());
	INFO(after.text.toStdString());
	REQUIRE(before.written);
	REQUIRE(after.written);
		//The components of the example that carry no part - nine before,
		//eight after - are pending in both: written, with pending items.
		//The comparison is of the lines, and the pending block is not its
		//business.
	CHECK(before.code == 3);
	CHECK(after.code == 3);

		//What the plants made of the two lists, read from the files: the
		//three codes and the rail before, the swap after.
	const QString rail = standardRail().key();
	QStringList before_codes {QStringLiteral("SH-0100"),
				  QStringLiteral("SH-0200"),
				  QStringLiteral("SH-0300"), rail};
	before_codes.sort();
	QStringList after_codes {QStringLiteral("SH-0101"),
				 QStringLiteral("SH-0200"),
				 QStringLiteral("SH-0300"), rail};
	after_codes.sort();
	REQUIRE(columnOf(before, QStringLiteral("part_code")) == before_codes);
	REQUIRE(columnOf(after, QStringLiteral("part_code")) == after_codes);
	CHECK(lineCell(before, QStringLiteral("SH-0200"), QStringLiteral("quantity"))
	      == QStringLiteral("1"));
	CHECK(lineCell(after, QStringLiteral("SH-0200"), QStringLiteral("quantity"))
	      == QStringLiteral("3"));
	CHECK(lineCell(before, rail, QStringLiteral("unit")) == QStringLiteral("m"));
		//The trap is armed: the two lists carry the very same words, so
		//a comparison by designation would answer "nothing changed".
	CHECK(columnOf(before, QStringLiteral("designation"))
	      == columnOf(after, QStringLiteral("designation")));

	SECTION("as três diferenças, e nenhuma outra: 1")
	{
		const Comparison run = compareOf(before_csv, after_csv, report);
		INFO(run.text.toStdString());
		CHECK(run.code == 1);
		REQUIRE(run.written);
		checkShape(run);
		checkThreeChanges(run, before, after);
	}

	SECTION("a mesma lista dos dois lados: 0, e o relatório é só o cabeçalho")
	{
		const Comparison run = compareOf(before_csv, before_csv, report);
		INFO(run.text.toStdString());
		CHECK(run.code == 0);
		REQUIRE(run.written);
		checkShape(run);
		CHECK(run.rows.size() == 1);
	}

	SECTION("sem arquivo de relatório, o código é o mesmo")
	{
		CHECK(CLIExport::run(QStringList()
				     << QStringLiteral("--compare-purchase")
				     << before_csv << after_csv) == 1);
		CHECK(CLIExport::run(QStringList()
				     << QStringLiteral("--compare-purchase")
				     << after_csv << after_csv) == 0);
	}
}

TEST_CASE("CU-18.7 — depois de uma revisão carimbada em todas as folhas, a "
	  "comparação mostra só o que mudou",
	  "[uibench][purchase][compare][cli][t18]")
{
	QETProject::setBackupEnabled(false);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString before_qet = dir.filePath(QStringLiteral("before.qet"));
	const QString after_qet = dir.filePath(QStringLiteral("after.qet"));
	const QString before_csv = dir.filePath(QStringLiteral("before.csv"));
	const QString report = dir.filePath(QStringLiteral("report.csv"));

	const QString before_xml = beforeXml();
	REQUIRE_FALSE(before_xml.isEmpty());
	const QString after_xml = afterXml(before_xml);
	REQUIRE_FALSE(after_xml.isEmpty());
	REQUIRE(writeFile(before_qet, before_xml));
	REQUIRE(writeFile(after_qet, after_xml));

	const PurchaseFile before = exportOf(before_qet, before_csv);
	INFO(before.text.toStdString());
	REQUIRE(before.written);

	/**
		Stamp revision B on every folio of @a input, as a revision is
		stamped, into @a output - and check that it was: a stamp that
		missed would leave this case comparing two unrevised projects.
	*/
	auto revise = [](const QString &input, const QString &output)
	{
		const int code = CLIExport::run(QStringList()
						<< QStringLiteral("--set-titleblock")
						<< input << output
						<< QStringLiteral("revision=B"));
		CHECK(code == 0);
		const QString revised = readFile(output);
		CHECK(revised.count(QStringLiteral("<diagram ")) == 3);
		CHECK(revised.count(QStringLiteral("indexrev=\"B\"")) >= 3);
		return code == 0;
	};

	SECTION("a revisão sozinha, sem nada trocado: 0, nenhuma diferença")
	{
			//The stamp changes every folio and no part: "everything
			//changed" is the answer a comparison hung on the revision
			//would give, and it is the wrong one.
		const QString revised_qet = dir.filePath(QStringLiteral("before_b.qet"));
		const QString revised_csv = dir.filePath(QStringLiteral("before_b.csv"));
		REQUIRE(revise(before_qet, revised_qet));
		const PurchaseFile revised = exportOf(revised_qet, revised_csv);
		INFO(revised.text.toStdString());
		REQUIRE(revised.written);

		const Comparison run = compareOf(before_csv, revised_csv, report);
		INFO(run.text.toStdString());
		CHECK(run.code == 0);
		REQUIRE(run.written);
		checkShape(run);
		CHECK(run.rows.size() == 1);
	}

	SECTION("a revisão com os três itens trocados: 1, e as três diferenças")
	{
		const QString revised_qet = dir.filePath(QStringLiteral("after_b.qet"));
		const QString revised_csv = dir.filePath(QStringLiteral("after_b.csv"));
		REQUIRE(revise(after_qet, revised_qet));
		const PurchaseFile revised = exportOf(revised_qet, revised_csv);
		INFO(revised.text.toStdString());
		REQUIRE(revised.written);

		const Comparison run = compareOf(before_csv, revised_csv, report);
		INFO(run.text.toStdString());
		CHECK(run.code == 1);
		REQUIRE(run.written);
		checkShape(run);
		checkThreeChanges(run, before, revised);
	}
}

TEST_CASE("T18 — a mesma lista exportada numa máquina em português e numa em "
	  "inglês não é diferença", "[uibench][purchase][compare][cli][t18]")
{
	QETProject::setBackupEnabled(false);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project = dir.filePath(QStringLiteral("project.qet"));
	const QString english_csv = dir.filePath(QStringLiteral("english.csv"));
	const QString portuguese_csv = dir.filePath(QStringLiteral("portuguese.csv"));
	const QString report = dir.filePath(QStringLiteral("report.csv"));

	const QString xml = beforeXml();
	REQUIRE_FALSE(xml.isEmpty());
	REQUIRE(writeFile(project, xml));

	PurchaseFile english;
	{
		const DefaultLocale locale(QLocale(QLocale::English,
						   QLocale::UnitedStates));
		english = exportOf(project, english_csv);
	}
	PurchaseFile portuguese;
	{
		const DefaultLocale locale(QLocale(QLocale::Portuguese,
						   QLocale::Brazil));
		portuguese = exportOf(project, portuguese_csv);
	}
	INFO(english.text.toStdString());
	INFO(portuguese.text.toStdString());
	REQUIRE(english.written);
	REQUIRE(portuguese.written);

		//What makes this case prove something: the two files do not say
		//the rail the same way.
	const QString rail = standardRail().key();
	CHECK(lineCell(english, rail, QStringLiteral("quantity"))
	      == QStringLiteral("2.5"));
	CHECK(lineCell(portuguese, rail, QStringLiteral("quantity"))
	      == QStringLiteral("2,5"));
	CHECK(english.text != portuguese.text);

	const Comparison forth = compareOf(english_csv, portuguese_csv, report);
	INFO(forth.text.toStdString());
	CHECK(forth.code == 0);
	CHECK(forth.rows.size() == 1);

	const Comparison back = compareOf(portuguese_csv, english_csv, report);
	CHECK(back.code == 0);
	CHECK(back.rows.size() == 1);
}

TEST_CASE("T18 — --compare-purchase recusa com 2 o uso errado e o arquivo que "
	  "não é uma lista de compra inteira, sem escrever relatório",
	  "[uibench][purchase][compare][cli][t18]")
{
	QETProject::setBackupEnabled(false);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project = dir.filePath(QStringLiteral("project.qet"));
	const QString list = dir.filePath(QStringLiteral("list.csv"));
	const QString report = dir.filePath(QStringLiteral("report.csv"));
	const QString flag = QStringLiteral("--compare-purchase");

	const QString xml = beforeXml();
	REQUIRE_FALSE(xml.isEmpty());
	REQUIRE(writeFile(project, xml));
	const PurchaseFile exported = exportOf(project, list);
	REQUIRE(exported.written);

		//A good list compared with itself is 0: what makes every 2 below
		//the fault of the one thing each section changes.
	REQUIRE(CLIExport::run(QStringList() << flag << list << list << report) == 0);
	REQUIRE(QFile::remove(report));

	SECTION("sem arquivo nenhum, e com um só: 2")
	{
		CHECK(CLIExport::run(QStringList() << flag) == 2);
		CHECK(CLIExport::run(QStringList() << flag << list) == 2);
	}

	SECTION("argumento a mais: 2, e nenhum relatório")
	{
		CHECK(CLIExport::run(QStringList() << flag << list << list
				     << report << QStringLiteral("extra")) == 2);
		CHECK_FALSE(QFile::exists(report));
	}

	SECTION("opção no lugar do relatório: 2, e não um arquivo com o nome dela")
	{
		CHECK(CLIExport::run(QStringList() << flag << list << list
				     << QStringLiteral("--report")) == 2);
		CHECK_FALSE(QFile::exists(QStringLiteral("--report")));
	}

	SECTION("lista que não existe: 2")
	{
		CHECK(CLIExport::run(QStringList() << flag
				     << dir.filePath(QStringLiteral("none.csv"))
				     << list << report) == 2);
		CHECK_FALSE(QFile::exists(report));
	}

	SECTION("o projeto no lugar da lista: 2")
	{
		CHECK(CLIExport::run(QStringList() << flag << project << list
				     << report) == 2);
		CHECK_FALSE(QFile::exists(report));
	}

	SECTION("a lista de material do --export-bom no lugar da lista de "
		"compra: 2")
	{
		const QString bom = dir.filePath(QStringLiteral("bom.csv"));
		CLIExport::run(QStringList() << QStringLiteral("--export-bom")
			       << project << bom);
		REQUIRE(QFile::exists(bom));
		CHECK(CLIExport::run(QStringList() << flag << list << bom
				     << report) == 2);
		CHECK_FALSE(QFile::exists(report));
	}

	SECTION("a lista cortada antes do bloco das pendências: 2")
	{
		QString cut = exported.text;
		const int pending = int(cut.indexOf(QStringLiteral("reason;")));
		REQUIRE(pending > 0);
		cut.truncate(pending);
		const QString cut_path = dir.filePath(QStringLiteral("cut.csv"));
		REQUIRE(writeFile(cut_path, cut));
		CHECK(CLIExport::run(QStringList() << flag << list << cut_path
				     << report) == 2);
		CHECK_FALSE(QFile::exists(report));
	}

	SECTION("uma linha apagada à mão: a contagem não fecha, 2, e não uma "
		"peça \"retirada\"")
	{
		const QStringList rows = exported.text.split(QStringLiteral("\r\n"));
		QStringList edited;
		int dropped = 0;
		for (const QString &row : rows)
		{
			if (row.startsWith(QStringLiteral("SH-0300;"))) {
				++dropped;
				continue;
			}
			edited << row;
		}
		REQUIRE(dropped == 1);
		const QString edited_path = dir.filePath(QStringLiteral("edited.csv"));
		REQUIRE(writeFile(edited_path, edited.join(QStringLiteral("\r\n"))));
		CHECK(CLIExport::run(QStringList() << flag << list << edited_path
				     << report) == 2);
		CHECK_FALSE(QFile::exists(report));
	}

	SECTION("relatório por cima de uma das listas: 2, e a lista intacta")
	{
		CHECK(CLIExport::run(QStringList() << flag << list << list
				     << list) == 2);
		CHECK(readFile(list) == exported.text);
	}

	SECTION("relatório que não pode ser escrito: 2, e nenhum arquivo")
	{
		const QString unwritable =
			dir.filePath(QStringLiteral("missing/report.csv"));
		CHECK(CLIExport::run(QStringList() << flag << list << list
				     << unwritable) == 2);
		CHECK_FALSE(QFile::exists(unwritable));
	}
}
