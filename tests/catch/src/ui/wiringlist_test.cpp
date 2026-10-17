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

#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/wiringlistexport.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QDomElement>
#include <QList>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

/*
	What the wiring list says about a wire, measured cell by cell.

	The list is what goes to the bench, and the failure it was written
	against is the one nobody reports: a column that is there, that is
	filled, and that says nothing. Measured on a real project of fourteen
	sheets, exported with --export-cables: 250 rows of cable, and the Page
	column of all 250 of them reading the literal "%id/%total". The file
	opened, the row count was right, and the one column that says which
	drawing to open held a template. Every example project shipped with
	QElectroTech stores the same template, so this was not one project's
	habit.

	@par Why the assertions are about cells and not about row counts

	A case that counts 250 rows cannot tell a page number from a template,
	and would have passed every day this defect existed. So what is asserted
	below is the content of the Page cell of every row - all of them, not a
	sample - and, for the properties of the wire, the exact text of each of
	the five columns against what was put in. The row count is asserted too,
	because it catches the coarse contamination the cell assertions would
	miss - a row that should not be in the list at all - but it is never
	asserted alone.

	@par The counter-needle

	Both cases start by proving the fixture can fail. For the page number,
	that the project really does store "%id" in the folio of a sheet: over a
	project whose sheets are numbered by hand, the whole case would pass
	without the substitution existing at all. For the wire properties, that
	the conductor really carries them, since an empty column and a column
	read from an empty property look the same from here.
*/

namespace
{
	/**
		One row of a delimited file, as a reader gets it back.

		Written here on purpose rather than taken from the writer under
		test: an assertion made with the writer's own rules agrees with
		itself. RFC 4180 - a doubled quote inside a quoted field is one
		quote, a separator inside a quoted field is text.
	*/
	QStringList parseRow(const QString &line, QChar separator = QLatin1Char(';'))
	{
		QStringList cells;
		QString cell;
		bool quoted = false;

		for (int i = 0 ; i < line.size() ; ++i)
		{
			const QChar c = line.at(i);
			if (quoted)
			{
				if (c != QLatin1Char('"')) {
					cell.append(c);
				} else if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
					cell.append(QLatin1Char('"'));
					++i;
				} else {
					quoted = false;
				}
			}
			else if (c == QLatin1Char('"')) {
				quoted = true;
			}
			else if (c == separator) {
				cells << cell;
				cell.clear();
			}
			else {
				cell.append(c);
			}
		}
		cells << cell;
		return cells;
	}

	/// The rows of @a csv, header included, empty trailing line dropped.
	/// A record ends on a line break outside quotes: a label written on
	/// two lines in the folio is one quoted cell, and splitting on every
	/// line break cut it into two broken rows - 97 rows for 77 wires,
	/// measured on the example after the upstream sync of 2026-09-30.
	QList<QStringList> parseCsv(const QString &csv)
	{
		QList<QStringList> rows;
		QString record;
		bool quoted = false;
		for (const QChar c : csv) {
			if (c == QLatin1Char('"')) {
				quoted = !quoted;
			}
			if (c == QLatin1Char('\n') && !quoted) {
				if (!record.isEmpty()) {
					rows << parseRow(record);
				}
				record.clear();
				continue;
			}
			record.append(c);
		}
		if (!record.isEmpty()) {
			rows << parseRow(record);
		}
		return rows;
	}

	/// Whether any sheet of @a project stores a folio variable rather than a number.
	bool storesAFolioTemplate(QETProject *project)
	{
		const QDomDocument doc = project->toXml();
		for (QDomElement sheet = doc.documentElement().firstChildElement() ;
		     !sheet.isNull() ;
		     sheet = sheet.nextSiblingElement())
		{
			if (sheet.tagName().toLower() != QLatin1String("diagram")) {
				continue;
			}
			if (sheet.attribute(QStringLiteral("folio")).contains(QLatin1String("%id"))) {
				return true;
			}
		}
		return false;
	}

	/**
		One sheet of three components, Q1, X1 and Y1, each with two
		terminals, wired by @a conductors.

		The terminal identifiers are 2n for the east terminal of the n-th
		component and 2n + 1 for its west one: Q1 is 0 and 1, X1 is 2 and
		3, Y1 is 4 and 5.
	*/
	QString fixtureXml(const QString &conductors)
	{
			//The docking point of a terminal, which is what the instance
			//stores. Taken from Terminal rather than written down, so that
			//a change of the constant does not leave this fixture with
			//conductors that bind to nothing.
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		const QString definition = QStringLiteral(
			"<element name=\"part.elmt\">"   //the .elmt suffix is not decoration:
			//an embedded element whose name lacks it is not found by the
			//instances below, so no element is built, so no terminal exists,
			//so no conductor is built either - and the case fails far from
			//here, on a count of conductors, saying nothing about the cause.
			"<definition type=\"element\" version=\"0.80\""
			" width=\"30\" height=\"20\" hotspot_x=\"15\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">part</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;"
			"filling:none;color:black\"/>"
			"<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			"<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			"</description>"
			"</definition>"
			"</element>");

		const char *labels[] = {"Q1", "X1", "Y1"};
		QString instances;
		for (int index = 0 ; index < 3 ; ++index)
		{
			instances += QStringLiteral(
				"<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\""
				" freezeLabel=\"false\" orientation=\"0\""
				" type=\"embed://bench/part.elmt\""
				" uuid=\"{c0ffee00-0000-4000-8000-00000000000%2}\">"
				"<terminals>"
				"<terminal x=\"%3\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
				"<terminal x=\"%5\" y=\"0\" orientation=\"3\" id=\"%6\"/>"
				"</terminals>"
				"<inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%7"
				"</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(100 + index * 200)
				.arg(index)
				.arg(east_dock)
				.arg(index * 2)
				.arg(west_dock)
				.arg(index * 2 + 1)
				.arg(QLatin1String(labels[index]));
		}

			//The folio of the sheet is the default template, on purpose:
			//it is what makes the last section below able to fail.
			//QString::arg leaves %id and %total alone - a marker is a per
			//cent sign followed by a digit - so the three numbered
			//placeholders are the only ones substituted here.
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection>"
			"<category name=\"bench\">%1</category>"
			"</collection>"
			"<diagram title=\"Bench\" order=\"1\" folio=\"%id/%total\""
			" height=\"600\" cols=\"17\" colsize=\"50\" rows=\"8\""
			" rowsize=\"80\" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements>"
			"<inputs/>"
			"<conductors>%3</conductors>"
			"</diagram>"
			"</project>")
			.arg(definition, instances, conductors);
	}

	/**
		A project of three components and two wires, one wire carrying every
		property a wiring list is about and one carrying none of them.

		The empty one is not padding: a column written from a property
		nobody filled has to come out empty, and a binder that wrote a
		placeholder - a dash, a zero, the word "none" - would pass a case
		built only on the filled wire.

		Three components and not two, because the exported list has no wire
		number column: the only way to tell the two rows apart from outside
		is by the pair of components they name.
	*/
	QString fixtureXml()
	{
			//W1 joins Q1 to X1 and carries the five properties; W2 joins Q1
			//to Y1 and carries none.
		return fixtureXml(QStringLiteral(
			"<conductor terminal1=\"0\" terminal2=\"3\" num=\"W1\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\""
			" conductor_section=\"1,5\" conductor_color=\"Bleu clair\""
			" function=\"Commande\" tension_protocol=\"24 V DC\"/>"
			"<conductor terminal1=\"1\" terminal2=\"4\" num=\"W2\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"));
	}

	/**
		Three wires on the sheet of fixtureXml(), told apart by the colour
		they declare, since the list has no wire number column and the
		component columns of a hand-written sheet are empty (see the CSV
		section of the case on wire properties): an ordinary wire from Q1
		to X1, a bridge from X1 to Y1 under the name the list always
		recognised, and a bridge from Y1 back to Q1 under the Portuguese
		name, which the list used to print as an ordinary wire.
	*/
	QString bridgeFixtureXml()
	{
		return fixtureXml(QStringLiteral(
			"<conductor terminal1=\"0\" terminal2=\"3\" num=\"W1\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\""
			" conductor_color=\"ordinary\"/>"
			"<conductor terminal1=\"2\" terminal2=\"5\" num=\"Brücke\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\""
			" conductor_color=\"german\"/>"
			"<conductor terminal1=\"4\" terminal2=\"1\" num=\"PONTE\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\""
			" conductor_color=\"portuguese\"/>"));
	}

	/**
		Two sheets joined by three pairs of folio report arrows, one wire
		crossing on each pair: a component on the first sheet wired to a
		going arrow, the coming arrow linked to it on the second sheet
		wired to a component there.

		The three pairs differ only in the sections their two halves
		declare, which is the whole point:
		- pair 0, Q1 to K1: "1,5" on the first sheet, "2,5" on the second
		  - the contradiction under test;
		- pair 1, Q2 to K2: "4" on both - the counter-needle. It has to
		  come out as ONE row spanning both pages, which proves that the
		  arrows are linked and recognised as reports. Without it, a
		  fixture whose arrows were never joined would give two rows for
		  the contradicting pair too, and the case would pass for the
		  wrong reason;
		- pair 2, Q3 to K3: "6" on the first sheet, nothing on the second -
		  a half that declares no section is not a contradiction, and is
		  joined as before.

		Unlike the sheet of fixtureXml(), the terminals of these symbols
		carry a uuid. That is what makes the program write the uuid of the
		symbol at each end of a wire when the folio is serialised, and the
		list recognises a report arrow, and pairs the two halves, by that
		uuid alone.
	*/
	QString crossingFixtureXml()
	{
			//The docking point of a terminal, as in fixtureXml().
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		auto symbol = [](const QString &name,
				 const QString &link_type,
				 const QString &terminals)
		{
			return QStringLiteral(
				"<element name=\"%1\">"
				"<definition type=\"element\" version=\"0.80\""
				" width=\"30\" height=\"20\" hotspot_x=\"15\" hotspot_y=\"10\""
				" orientation=\"dnnn\" link_type=\"%2\">"
				"<names><name lang=\"en\">%1</name></names>"
				"<description>"
				"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
				" antialias=\"false\""
				" style=\"line-style:normal;line-weight:normal;"
				"filling:none;color:black\"/>"
				"%3"
				"</description>"
				"</definition>"
				"</element>")
				.arg(name, link_type, terminals);
		};

		const QString definitions =
			symbol(QStringLiteral("part.elmt"), QStringLiteral("simple"),
			       QStringLiteral(
				       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\""
				       " uuid=\"{7e000000-0000-4000-8000-000000000001}\"/>"
				       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\""
				       " uuid=\"{7e000000-0000-4000-8000-000000000002}\"/>"))
			+ symbol(QStringLiteral("going.elmt"), QStringLiteral("next_report"),
				 QStringLiteral(
					 "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"1\""
					 " uuid=\"{7e000000-0000-4000-8000-000000000003}\"/>"))
			+ symbol(QStringLiteral("coming.elmt"), QStringLiteral("previous_report"),
				 QStringLiteral(
					 "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\""
					 " uuid=\"{7e000000-0000-4000-8000-000000000004}\"/>"));

		auto uuid = [](int code) {
			return QStringLiteral("{c0ffee00-0000-4000-8000-%1}")
				.arg(code, 12, 16, QLatin1Char('0'));
		};

		auto terminal = [](qreal x, int orientation, int id) {
			return QStringLiteral(
				"<terminal x=\"%1\" y=\"0\" orientation=\"%2\" id=\"%3\"/>")
				.arg(x).arg(orientation).arg(id);
		};

		auto instance = [](const QString &type, const QString &element_uuid,
				   int x, int y, const QString &terminals,
				   const QString &label, const QString &linked_uuid)
		{
			const QString informations = label.isEmpty()
				? QStringLiteral("<elementInformations/>")
				: QStringLiteral(
					  "<elementInformations>"
					  "<elementInformation show=\"1\" name=\"label\">%1"
					  "</elementInformation>"
					  "</elementInformations>").arg(label);
			const QString links = linked_uuid.isEmpty()
				? QString()
				: QStringLiteral(
					  "<links_uuids><link_uuid uuid=\"%1\"/></links_uuids>")
					  .arg(linked_uuid);
			return QStringLiteral(
				"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
				" freezeLabel=\"false\" orientation=\"0\""
				" type=\"embed://bench/%3\" uuid=\"%4\">"
				"<terminals>%5</terminals>"
				"<inputs/>%6%7"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(x).arg(y)
				.arg(type, element_uuid, terminals, links, informations);
		};

		auto conductor = [](int from, int to, const QString &section) {
			return QStringLiteral(
				"<conductor terminal1=\"%1\" terminal2=\"%2\" num=\"X\""
				" displaytext=\"1\" type=\"multi\" condsize=\"1\""
				" conductor_section=\"%3\"/>")
				.arg(from).arg(to).arg(section);
		};

		struct Pair
		{
			const char *left;
			const char *right;
			const char *left_section;
			const char *right_section;
		};
		const Pair pairs[] = {
			{"Q1", "K1", "1,5", "2,5"},
			{"Q2", "K2", "4", "4"},
			{"Q3", "K3", "6", ""}};

		QString left_elements, left_wires, right_elements, right_wires;
		int id = 0;
		for (int p = 0 ; p < 3 ; ++p)
		{
			const Pair &pair = pairs[p];
			const int y = 100 + 200 * p;
			const QString going = uuid(0x200 + p);
			const QString coming = uuid(0x300 + p);

				//First sheet: the component's east terminal to the
				//going arrow.
			const int left_east = id++;
			const int left_west = id++;
			const int going_end = id++;
			left_elements += instance(QStringLiteral("part.elmt"), uuid(0x100 + p),
						  100, y,
						  terminal(east_dock, 1, left_east)
						  + terminal(west_dock, 3, left_west),
						  QString::fromLatin1(pair.left), QString());
			left_elements += instance(QStringLiteral("going.elmt"), going,
						  300, y,
						  terminal(west_dock, 3, going_end),
						  QString(), coming);
			left_wires += conductor(left_east, going_end,
						QString::fromUtf8(pair.left_section));

				//Second sheet: the coming arrow to the component's
				//west terminal.
			const int coming_end = id++;
			const int right_east = id++;
			const int right_west = id++;
			right_elements += instance(QStringLiteral("coming.elmt"), coming,
						   100, y,
						   terminal(east_dock, 1, coming_end),
						   QString(), going);
			right_elements += instance(QStringLiteral("part.elmt"), uuid(0x400 + p),
						   300, y,
						   terminal(east_dock, 1, right_east)
						   + terminal(west_dock, 3, right_west),
						   QString::fromLatin1(pair.right), QString());
			right_wires += conductor(coming_end, right_west,
						 QString::fromUtf8(pair.right_section));
		}

		auto sheet = [](int order, const QString &elements, const QString &wires) {
			return QStringLiteral(
				"<diagram title=\"Bench\" order=\"%1\" folio=\"%id/%total\""
				" height=\"600\" cols=\"17\" colsize=\"50\" rows=\"8\""
				" rowsize=\"80\" displaycols=\"true\" displayrows=\"true\">"
				"<elements>%2</elements>"
				"<inputs/>"
				"<conductors>%3</conductors>"
				"</diagram>")
				.arg(order).arg(elements, wires);
		};

		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection>"
			"<category name=\"bench\">%1</category>"
			"</collection>"
			"%2%3"
			"</project>")
			.arg(definitions,
			     sheet(1, left_elements, left_wires),
			     sheet(2, right_elements, right_wires));
	}

	/// The data row of @a rows whose colour column reads @a colour, empty when none.
	QStringList rowWithColour(const QList<QStringList> &rows,
				  const QString &colour)
	{
		for (int index = 1 ; index < rows.size() ; ++index) {
			if (rows.at(index).value(6) == colour) {
				return rows.at(index);
			}
		}
		return QStringList();
	}

	/// The data row of @a rows whose section column reads @a section, empty when
	/// none. Used instead of rowByPartner on a hand-written fixture: see the
	/// comment at the call site for why the component columns cannot be the key.
	QStringList rowWithSection(const QList<QStringList> &rows,
				   const QString &section)
	{
		for (int index = 1 ; index < rows.size() ; ++index) {
			if (rows.at(index).value(7) == section) {
				return rows.at(index);
			}
		}
		return QStringList();
	}

	/// The row of @a rows whose second component is @a partner, empty when none.
	QStringList rowByPartner(const QList<QStringList> &rows, const QString &partner)
	{
		for (const QStringList &row : rows) {
			if (row.value(3) == partner) {
				return row;
			}
		}
		return QStringList();
	}
}

TEST_CASE("T17 — a coluna Página do plano de fiação traz o número da folha, não o modelo",
	  "[uibench][wiringlist][t17]")
{
		//Three sheets and a cable on every one of them, which is what lets
		//"the same number for everybody" be told apart from "the right
		//number". A project whose cables all sit on one sheet - perceuse,
		//among the examples - cannot make that difference.
	UiBench::Project bench(QStringLiteral("tremie_vibrante.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	const int sheet_count = bench.diagramCount();
	REQUIRE(sheet_count == 3);

	int drawn = 0;
	const QList<Diagram *> sheets = bench.diagrams();
	for (Diagram *sheet : sheets) {
		REQUIRE(sheet->conductors().size() > 0);
		drawn += static_cast<int>(sheet->conductors().size());
	}

		//The counter-needle. Without it the case would pass over a project
		//whose sheets carry a typed-in page number, which is exactly the
		//kind of fixture that makes a regression invisible.
	REQUIRE(storesAFolioTemplate(bench.project()));

	WiringListExport exporter(bench.project(), nullptr);
	const QString csv = exporter.toCsvString();
	REQUIRE_FALSE(csv.isEmpty());

	const QList<QStringList> rows = parseCsv(csv);
	REQUIRE(rows.size() >= 2);

		//The header first, because every assertion below counts on the Page
		//column being the first one.
	CHECK(rows.first().size() == 11);
	CHECK(rows.first().value(0) == QStringLiteral("Page"));

	const QList<QStringList> data_rows = rows.mid(1);

		//The coarse assertions, bounded by the project rather than by a
		//number typed in here: one row per cable at most - two halves of a
		//wire drawn across a folio arrow are merged into one - and not
		//fewer than half of them, which would mean rows are being dropped.
		//And eleven cells on every row: a row short of one is a shifted row,
		//and a shifted row reads like a well-formed one.
	CHECK(data_rows.size() <= drawn);
	CHECK(data_rows.size() >= drawn / 2);

	int malformed = 0;
	for (const QStringList &row : data_rows) {
		if (row.size() != 11) {
			++malformed;
		}
	}
	CHECK(malformed == 0);

		//And the fine one, which is the case: the Page cell of every row,
		//not of a sample. A merged wire reports both of its sheets,
		//separated by a comma, so each part is judged on its own.
	int with_variable = 0;
	int out_of_range = 0;
	int empty_page = 0;
	QSet<QString> pages;

	for (const QStringList &row : data_rows)
	{
		const QString page = row.value(0);
		if (page.trimmed().isEmpty()) {
			++empty_page;
			continue;
		}
		if (page.contains(QLatin1Char('%'))) {
			++with_variable;
			continue;
		}

		const QStringList parts = page.split(QLatin1Char(','));
		for (const QString &part : parts)
		{
			pages << part.trimmed();
			const QStringList halves = part.trimmed().split(QLatin1Char('/'));
			bool ok = false;
			const int index = halves.value(0).toInt(&ok);
			if (!ok || index < 1 || index > sheet_count
			    || halves.value(1).toInt() != sheet_count) {
				++out_of_range;
			}
		}
	}

		//Reported as three numbers rather than as one verdict: "some cell
		//is wrong" does not say whether the substitution failed, produced a
		//number from outside the project, or produced nothing at all.
	CHECK(with_variable == 0);
	CHECK(out_of_range == 0);
	CHECK(empty_page == 0);

		//Every sheet draws a cable, so every sheet has to appear. This is
		//what catches a substitution that answers the same page to
		//everybody - "1/3" on all of them is as wrong as "%id/%total", and
		//none of the three counts above would say so.
	CHECK(pages.size() == sheet_count);
	CHECK(pages.contains(QStringLiteral("1/3")));
	CHECK(pages.contains(QStringLiteral("3/3")));
}

TEST_CASE("T17 — seção, cor, função e tensão do fio chegam ao banco e à lista",
	  "[uibench][wiringlist][database][t17]")
{
	UiBench::ScratchProject bench(fixtureXml(),
				      QStringLiteral("wiringlist.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->conductors().size() == 2);

		//The counter-needle: the wire really does carry the properties, so
		//an empty answer below is the writing and not the fixture.
	bool carried = false;
	const QList<Conductor *> wires = sheet->conductors();
	for (Conductor *wire : wires)
	{
		if (wire->properties().text != QLatin1String("W1")) {
			continue;
		}
		carried = wire->properties().m_wire_section == QLatin1String("1,5")
			&& wire->properties().m_wire_color == QLatin1String("Bleu clair")
			&& wire->properties().m_function == QLatin1String("Commande")
			&& wire->properties().m_tension_protocol == QLatin1String("24 V DC");
	}
	REQUIRE(carried);

	projectDataBase *data_base = bench.project()->dataBase();
	REQUIRE(data_base != nullptr);

		//The five mutable columns of one wire, in column order.
	auto columnsOf = [](projectDataBase *base, const QString &wire)
	{
		QStringList values;
		QSqlQuery query = base->newQuery(
					QStringLiteral(
						"SELECT conductor_section, conductor_color,"
						" function, tension_protocol, conductor_type"
						" FROM conductor WHERE text = '%1'").arg(wire));
		if (!query.exec()) {
			return QStringList(QStringLiteral("query failed: ")
					   + query.lastError().text());
		}
		if (!query.next()) {
			return QStringList(QStringLiteral("no row"));
		}
		for (int i = 0 ; i < 5 ; ++i) {
			values << query.value(i).toString();
		}
		return values;
	};

	const QStringList expected_w1 = {QStringLiteral("1,5"),
					 QStringLiteral("Bleu clair"),
					 QStringLiteral("Commande"),
					 QStringLiteral("24 V DC"),
					 QStringLiteral("multi")};
	const QStringList expected_w2 = {QString(), QString(), QString(),
					 QString(), QStringLiteral("multi")};

	SECTION("o povoamento da abertura escreve as cinco colunas")
	{
			//Coarse and fine together: the table holds one row per wire
			//and no more, and each of those rows says what the wire says.
		QSqlQuery count = data_base->newQuery(
					QStringLiteral("SELECT COUNT(*) FROM conductor"));
		REQUIRE(count.exec());
		REQUIRE(count.next());
		CHECK(count.value(0).toInt()
		      == static_cast<int>(sheet->conductors().size()));

		CHECK(columnsOf(data_base, QStringLiteral("W1")) == expected_w1);

			//And the wire that carries nothing comes back carrying
			//nothing - no placeholder invented on the way in.
		CHECK(columnsOf(data_base, QStringLiteral("W2")) == expected_w2);
	}

	SECTION("as cinco colunas sobrevivem a salvar e reabrir")
	{
		const bool reopened_ok = bench.saveAndReopen();
		INFO(bench.error().toStdString());
		REQUIRE(reopened_ok);
		REQUIRE(bench.isOpen());

		projectDataBase *reopened = bench.project()->dataBase();
		REQUIRE(reopened != nullptr);

		CHECK(columnsOf(reopened, QStringLiteral("W1")) == expected_w1);
		CHECK(columnsOf(reopened, QStringLiteral("W2")) == expected_w2);
	}

	SECTION("mudar a propriedade de um fio chega à tabela")
	{
			//The update path, which is the one that goes stale without a
			//word: the row is right when the project is opened and wrong
			//from the first edit onwards. Conductor::setProperties() is
			//what the properties dialogue calls, and watchConductor() is
			//what brings it here.
		Conductor *target = nullptr;
		const QList<Conductor *> all = sheet->conductors();
		for (Conductor *wire : all) {
			if (wire->properties().text == QLatin1String("W2")) {
				target = wire;
			}
		}
		REQUIRE(target != nullptr);

		ConductorProperties changed = target->properties();
		changed.m_wire_section = QStringLiteral("2,5");
		changed.m_wire_color = QStringLiteral("Noir");
		changed.m_function = QStringLiteral("Puissance");
		changed.m_tension_protocol = QStringLiteral("400 V AC");
		target->setProperties(changed);

		const QStringList expected = {QStringLiteral("2,5"),
					      QStringLiteral("Noir"),
					      QStringLiteral("Puissance"),
					      QStringLiteral("400 V AC"),
					      QStringLiteral("multi")};
		CHECK(columnsOf(data_base, QStringLiteral("W2")) == expected);

			//And the wire nobody touched is untouched, which an UPDATE
			//with a forgotten WHERE would break and which no assertion
			//above would notice.
		CHECK(columnsOf(data_base, QStringLiteral("W1")) == expected_w1);
	}

	SECTION("as mesmas cinco colunas saem preenchidas no CSV")
	{
			//The measurement that opened this task found those columns
			//empty in 100% of 250 rows of a real project. This says where
			//the emptiness comes from: the exporter fills them whenever
			//the wire carries them, so an empty column on a real project
			//is an unfilled property and not a broken export. Worth a
			//section of its own, because the two explanations lead to
			//completely different work.
		WiringListExport exporter(bench.project(), nullptr);
		const QList<QStringList> rows = parseCsv(exporter.toCsvString());
		REQUIRE(rows.size() == 3);

		QString dump;
		for (const QStringList &r : rows) {
			dump += QStringLiteral("[") + r.join(QStringLiteral("|"))
				+ QStringLiteral("] ");
		}
		INFO(dump.toStdString());

			//The row is found by what it CARRIES, not by the name of the
			//component at its end, and that is a measurement and not a
			//convenience. WiringListExport reads the component name from
			//attributes the application writes when it SAVES - element1_label,
			//falling back to element1_linked and then to a uuid map
			//(wiringlistexport.cpp:205-212). A fixture written by hand has
			//none of them, so both component columns come out empty here
			//while they are filled on a real project. Asserting on them would
			//be asserting on the fixture, not on the exporter.
		const QStringList row = rowWithSection(rows, QStringLiteral("1,5"));
		REQUIRE(row.size() == 11);

			//Columns 5 to 8: voltage, colour, section, function - the
			//order the header row declares.
		CHECK(row.value(5) == QStringLiteral("24 V DC"));
		CHECK(row.value(6) == QStringLiteral("Bleu clair"));
		CHECK(row.value(7) == QStringLiteral("1,5"));
		CHECK(row.value(8) == QStringLiteral("Commande"));

			//And the wire that carries nothing prints nothing, which is
			//the pair of the assertion above: a column filled with a
			//placeholder would satisfy the four checks above and lie here.
		const QStringList empty_row = rowWithSection(rows, QString());
		REQUIRE(empty_row.size() == 11);
		CHECK(empty_row.value(5).isEmpty());
		CHECK(empty_row.value(6).isEmpty());
		CHECK(empty_row.value(7).isEmpty());
		CHECK(empty_row.value(8).isEmpty());

			//And the page, resolved on a one-sheet project.
		CHECK(row.value(0) == QStringLiteral("1/1"));
		CHECK(empty_row.value(0) == QStringLiteral("1/1"));

			//Two ordinary wires: the Note column says nothing on either.
		CHECK(row.value(10).isEmpty());
		CHECK(empty_row.value(10).isEmpty());
	}
}

TEST_CASE("T17 — o que conta como ponte é o nome inteiro, sem caixa nem espaço em volta",
	  "[wiringlist][t17]")
{
		//The three names, in any case and with the spaces a hand leaves
		//around a typed word.
	CHECK(WiringListExport::isBridgeNumber(QStringLiteral("Brücke")));
	CHECK(WiringListExport::isBridgeNumber(QStringLiteral("BRÜCKE")));
	CHECK(WiringListExport::isBridgeNumber(QStringLiteral("PONTE")));
	CHECK(WiringListExport::isBridgeNumber(QStringLiteral("ponte")));
	CHECK(WiringListExport::isBridgeNumber(QStringLiteral("  Ponte ")));
	CHECK(WiringListExport::isBridgeNumber(QStringLiteral("Bridge")));

		//And what is not one. The name is compared whole: a numbered bridge
		//or a word that only starts like one is an ordinary wire, and the
		//empty number, the blank one and the placeholder the program
		//writes for a wire nobody numbered are not bridges either.
	CHECK_FALSE(WiringListExport::isBridgeNumber(QStringLiteral("PONTE 2")));
	CHECK_FALSE(WiringListExport::isBridgeNumber(QStringLiteral("Pontes")));
	CHECK_FALSE(WiringListExport::isBridgeNumber(QStringLiteral("Brücken")));
	CHECK_FALSE(WiringListExport::isBridgeNumber(QStringLiteral("W1")));
	CHECK_FALSE(WiringListExport::isBridgeNumber(QStringLiteral("_")));
	CHECK_FALSE(WiringListExport::isBridgeNumber(QStringLiteral("   ")));
	CHECK_FALSE(WiringListExport::isBridgeNumber(QString()));
}

TEST_CASE("T17 — nenhuma ponte some da lista de fiação, e as duas grafias saem marcadas igual",
	  "[uibench][wiringlist][t17]")
{
		//Half of CU-17.11: the wiring list. The other half - the bridge
		//marked in the list of terminals too, as the decision on where a
		//bridge appears asks - is not in this exporter, so the case is not
		//claimed here.
	UiBench::ScratchProject bench(bridgeFixtureXml(),
				      QStringLiteral("bridges.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->conductors().size() == 3);

		//The counter-needle: both bridges reach the drawing under their
		//names. Over a fixture whose numbers were lost on the way in, the
		//rows below would carry no mark for the right reason.
	QSet<QString> numbers;
	const QList<Conductor *> wires = sheet->conductors();
	for (Conductor *wire : wires) {
		numbers << wire->properties().text;
	}
	REQUIRE(numbers.contains(QStringLiteral("Brücke")));
	REQUIRE(numbers.contains(QStringLiteral("PONTE")));
	REQUIRE(numbers.contains(QStringLiteral("W1")));

	WiringListExport exporter(bench.project(), nullptr);
	const QString csv = exporter.toCsvString();
	INFO(csv.toStdString());
	const QList<QStringList> rows = parseCsv(csv);
	REQUIRE_FALSE(rows.isEmpty());

	CHECK(rows.first().size() == 11);
	CHECK(rows.first().value(10) == QStringLiteral("Note"));

		//The coarse assertion: one row per conductor drawn. The German
		//name used to be skipped, which made this two.
	CHECK(rows.size() - 1 == 3);

		//The fine ones, row by row, each found by the colour it declares.
	const QStringList german = rowWithColour(rows, QStringLiteral("german"));
	const QStringList portuguese = rowWithColour(rows, QStringLiteral("portuguese"));
	const QStringList ordinary = rowWithColour(rows, QStringLiteral("ordinary"));
	REQUIRE(german.size() == 11);
	REQUIRE(portuguese.size() == 11);
	REQUIRE(ordinary.size() == 11);

		//The same mark for the two names, which is what the case asks:
		//before, one of them vanished and the other passed for a wire.
	CHECK(german.value(10) == QStringLiteral("Bridge"));
	CHECK(portuguese.value(10) == QStringLiteral("Bridge"));

		//And the pair of those two: an ordinary wire carries no mark. A
		//note written on every row would satisfy both checks above.
	CHECK(ordinary.value(10).isEmpty());
}

TEST_CASE("CU-17.14 — fio de duas folhas com seções divergentes sai em linha própria, marcada",
	  "[uibench][wiringlist][t17]")
{
	UiBench::ScratchProject bench(crossingFixtureXml(),
				      QStringLiteral("crossing.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

		//Every folio built, and every wire drawn: two sheets, three wires
		//on each. A terminal that failed to match its symbol would drop
		//its wire silently, and the list would be measured on less than
		//the fixture says.
	REQUIRE(bench.diagramCount() == 2);
	REQUIRE(bench.diagram(0)->conductors().size() == 3);
	REQUIRE(bench.diagram(1)->conductors().size() == 3);

	WiringListExport exporter(bench.project(), nullptr);
	const QString csv = exporter.toCsvString();
	INFO(csv.toStdString());
	const QList<QStringList> rows = parseCsv(csv);
	REQUIRE(rows.size() >= 2);
	CHECK(rows.first().size() == 11);

		//The counter-needle, first: the pair whose halves agree comes out
		//as ONE row spanning both pages. That is what proves the arrows
		//are linked and recognised as reports - without it, the
		//contradicting pair below would give two rows only because nothing
		//was ever joined.
	const QStringList agreeing = rowWithSection(rows, QStringLiteral("4"));
	REQUIRE(agreeing.size() == 11);
	REQUIRE(agreeing.value(0) == QStringLiteral("1/2, 2/2"));
	CHECK(agreeing.value(10).isEmpty());

		//The coarse assertions. Three pairs, of which one splits: four
		//rows. And the sections the list prints are exactly the ones the
		//halves declare, each on its own: the old "1,5, 2,5" - one cell
		//holding both, which a spreadsheet reads as a section - is not
		//among them.
	const QList<QStringList> data_rows = rows.mid(1);
	CHECK(data_rows.size() == 4);

	QStringList sections;
	int malformed = 0;
	for (const QStringList &row : data_rows) {
		sections << row.value(7);
		if (row.size() != 11) {
			++malformed;
		}
	}
	sections.sort();
	CHECK(malformed == 0);
	CHECK(sections == QStringList({QStringLiteral("1,5"),
				       QStringLiteral("2,5"),
				       QStringLiteral("4"),
				       QStringLiteral("6")}));

		//The fine ones: each half of the contradicting wire on its own
		//row, on its own page, from its own component, and marked with
		//the page where the other half is.
	const QStringList first_half = rowWithSection(rows, QStringLiteral("1,5"));
	const QStringList second_half = rowWithSection(rows, QStringLiteral("2,5"));
	REQUIRE(first_half.size() == 11);
	REQUIRE(second_half.size() == 11);

	CHECK(first_half.value(0) == QStringLiteral("1/2"));
	CHECK(second_half.value(0) == QStringLiteral("2/2"));
	CHECK(first_half.value(1) == QStringLiteral("Q1"));
	CHECK(second_half.value(1) == QStringLiteral("K1"));
	CHECK(first_half.value(10)
	      == QStringLiteral("Section differs from the other half of this wire, on 2/2"));
	CHECK(second_half.value(10)
	      == QStringLiteral("Section differs from the other half of this wire, on 1/2"));

		//And a half that declares no section is not a contradiction: the
		//wire is joined as before, takes the section that was declared,
		//and carries no mark.
	const QStringList half_declared = rowWithSection(rows, QStringLiteral("6"));
	REQUIRE(half_declared.size() == 11);
	CHECK(half_declared.value(0) == QStringLiteral("1/2, 2/2"));
	CHECK(half_declared.value(10).isEmpty());
}
