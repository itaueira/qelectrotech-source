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
#include "../../../sources/location/bomcollector.h"
#include "../../../sources/location/bommeasure.h"
#include "../../../sources/location/locationtree.h"
#include "qt_catch_tostring.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

/*
	The list somebody orders a panel from, and the half of it nobody drew.

	Roughly half of what a cabinet costs is the cabinet: the enclosure, the
	door, the mounting plate. None of them is on a schematic, so none of
	them has a row in the element table, so a list built by walking the
	element rows comes back complete, ordered, coherent and missing them.
	Nothing about that file looks wrong, which is what makes it the worst
	way to fail.

	@par Why every case asserts two numbers and not one

	The number of lines is the cheap question, and it catches the coarse
	failure: an item that becomes a line of its own shows up as one line
	more. It does not catch the expensive one. An item added into the line
	beside it changes no count at all - the list still has the lines it had
	- and disappears inside a total that goes on looking plausible: one
	thousand seven hundred and eighty two where one thousand seven hundred
	and sixty was right. So each case asserts the number of groups and the
	total those groups add up to, and neither alone would do.

	@par The case that decides whether the join is any good

	A part code that both halves use. That is where a careless join goes
	wrong in both directions at once - two lines for one part, or one line
	that quietly dropped one of the two contributions - and it is the only
	case whose failure the other cases cannot see.
*/

namespace
{
		/// One drawn component, the way the view hands it over.
	BomCollector::ElementRow drawn(const char *code,
				       const char *revision,
				       const char *designation,
				       const char *label,
				       const char *quantity = "",
				       const char *unit = "")
	{
		BomCollector::ElementRow row;
		row.part_code = QString::fromUtf8(code);
		row.part_revision = QString::fromUtf8(revision);
		row.designation = QString::fromUtf8(designation);
		row.label = QString::fromUtf8(label);
		row.quantity = QString::fromUtf8(quantity);
		row.unit = QString::fromUtf8(unit);
		row.folio = QStringLiteral("1");
		return row;
	}

		/// One line of the half nobody drew, as the tree answers it.
	LocationTree::BomLine enclosure(const char *code,
					int revision,
					const char *name,
					double quantity,
					const QStringList &paths,
					const QString &unit
						= BomMeasure::countUnit())
	{
		LocationTree::BomLine line;
		line.part_code = QString::fromUtf8(code);
		line.part_revision = revision;
		line.name = QString::fromUtf8(name);
		line.quantity = quantity;
		line.unit = unit;
		line.paths = paths;
		return line;
	}

		/// @return the line of that part, an empty one when there is none
	LocationTree::BomLine lineOf(const BomCollector::Result &result,
				     const char *code,
				     int revision = 0)
	{
		const int found = LocationTree::indexOfBomLine(
					result.lines, QString::fromUtf8(code),
					revision);
		return found < 0 ? LocationTree::BomLine()
				 : result.lines.at(found);
	}

		/// @return how many pieces the whole list comes to
	double pieces(const BomCollector::Result &result)
	{
		return BomCollector::totalOf(result.lines,
					     BomMeasure::countUnit());
	}
}

TEST_CASE("T18 — as duas metades com o mesmo código de peça viram uma linha só",
	  "[bom]")
{
		//Three of the part were drawn, and two more of the same part are
		//the cabinet and its door, which nobody drew. The purchase list
		//has to ask for five.
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XA-100", "1", "Coffret 300x400", "A1")
	     << drawn("XA-100", "1", "Coffret 300x400", "A2")
	     << drawn("XA-100", "1", "Coffret 300x400", "A3");

	QList<LocationTree::BomLine> enclosures;
	enclosures << enclosure("XA-100", 1, "Coffret principal", 2.0,
				QStringList{QStringLiteral("QCM1"),
					    QStringLiteral("QCM2")});

	const BomCollector::Result result =
			BomCollector::collect(rows, enclosures);

		//The two assertions, and the reason both are here. One line is
		//what says the two halves were joined rather than stacked; five
		//is what says nothing was lost inside the line that survived.
		//A join that duplicated would keep the total at five and show
		//two lines; a join that swallowed one half would keep one line
		//and read three.
	REQUIRE(result.lines.size() == 1);
	CHECK(pieces(result) == 5.0);

	const LocationTree::BomLine line = result.lines.at(0);
	CHECK(line.part_code == QString("XA-100"));
	CHECK(line.part_revision == 1);
	CHECK(line.quantity == 5.0);

		//The drawn half names the line, because "Coffret 300x400" is
		//what a supplier is asked for and "Coffret principal" is what a
		//plan calls the place it stands in.
	CHECK(line.name == QString("Coffret 300x400"));

		//And the line says which five, both halves included.
	CHECK(line.paths.size() == 5);
	CHECK(line.paths.contains(QString("A1")));
	CHECK(line.paths.contains(QString("QCM2")));

	CHECK(result.drawn == 3);
	CHECK(result.enclosures == 1);
	CHECK(result.pendings.isEmpty());
}

TEST_CASE("T18 — o armário que ninguém desenhou entra na lista", "[bom]")
{
		//The case as the shop meets it: a perfectly ordinary schematic,
		//and a cabinet that is not on it.
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XB-200", "1", "Contacteur 9 A", "K1")
	     << drawn("XB-200", "1", "Contacteur 9 A", "K2")
	     << drawn("XC-300", "1", "Disjoncteur 3P 25 A", "Q1");

	QList<LocationTree::BomLine> enclosures;
	enclosures << enclosure("XE-900", 0, "Armoire", 1.0,
				QStringList{QStringLiteral("QCM1")})
		   << enclosure("XE-910", 0, "Porte", 1.0,
				QStringList{QStringLiteral("QCM1/PORTE")});

	const BomCollector::Result drawn_only =
			BomCollector::collect(rows, QList<LocationTree::BomLine>());
	const BomCollector::Result whole =
			BomCollector::collect(rows, enclosures);

		//What the program does today, measured side by side with what it
		//has to do: the same file, two lines and two pieces short, with
		//nothing in it saying so.
	CHECK(drawn_only.lines.size() == 2);
	CHECK(pieces(drawn_only) == 3.0);

	REQUIRE(whole.lines.size() == 4);
	CHECK(pieces(whole) == 5.0);
	CHECK(lineOf(whole, "XE-900").quantity == 1.0);
	CHECK(lineOf(whole, "XE-910").name == QString("Porte"));
	CHECK(whole.enclosures == 2);
}

TEST_CASE("T18 — a chave é o código da peça, e nunca a descrição", "[bom]")
{
		//Two breakers of different ratings, described in the same words
		//by two people on two days. Grouped by the sentence they are one
		//line, with the right total and one of the two codes - and the
		//wrong one arrives in the right quantity.
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XC-300", "1", "Disjoncteur tripolaire", "Q1")
	     << drawn("XC-400", "1", "Disjoncteur tripolaire", "Q2");

	const BomCollector::Result result =
			BomCollector::collect(rows, QList<LocationTree::BomLine>());

	REQUIRE(result.lines.size() == 2);
	CHECK(pieces(result) == 2.0);
		//The revision is part of the key, and drawn() above built both
		//rows with revision "1": asking for the default 0 finds neither,
		//which is the collector answering correctly to the wrong question.
	CHECK(lineOf(result, "XC-300", 1).quantity == 1.0);
	CHECK(lineOf(result, "XC-400", 1).quantity == 1.0);

	SECTION("e uma peça descrita de dois jeitos continua uma linha")
	{
		QList<BomCollector::ElementRow> twice;
		twice << drawn("XC-300", "1", "Disjoncteur 3P 25 A", "Q1")
		      << drawn("XC-300", "1", "Disjoncteur tripolaire 25 A",
			       "Q2");

		const BomCollector::Result one =
				BomCollector::collect(
					twice, QList<LocationTree::BomLine>());
		REQUIRE(one.lines.size() == 1);
		CHECK(pieces(one) == 2.0);
	}
}

TEST_CASE("T18 — a quantidade é somada do campo, e não contada de linhas",
	  "[bom]")
{
		//One symbol standing for five pieces, which is what the quantity
		//field is for. Counting rows answers a different question in the
		//same column: one, plausible, and short by four.
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XD-500", "1", "Borne 4 mm2", "X1", "5")
	     << drawn("XD-500", "1", "Borne 4 mm2", "X2", "12");

	const BomCollector::Result result =
			BomCollector::collect(rows, QList<LocationTree::BomLine>());

	REQUIRE(result.lines.size() == 1);
	CHECK(pieces(result) == 17.0);
	CHECK(result.drawn == 2);

	SECTION("e o campo em branco continua valendo uma peça")
	{
			//The normal state of a real project: almost nobody fills
			//the field in, and a symbol on a folio is at least the one
			//piece somebody drew.
		QList<BomCollector::ElementRow> plain;
		plain << drawn("XD-500", "1", "Borne 4 mm2", "X1")
		      << drawn("XD-500", "1", "Borne 4 mm2", "X2");

		const BomCollector::Result result_plain =
				BomCollector::collect(
					plain, QList<LocationTree::BomLine>());
		REQUIRE(result_plain.lines.size() == 1);
		CHECK(pieces(result_plain) == 2.0);
	}
}

TEST_CASE("T18 — uma peça em duas revisões dá duas linhas", "[bom]")
{
		//catalog_part is UNIQUE(code, revision): a revision that went up
		//is another part. Added together, the shop is asked for the old
		//one in the quantity of the new.
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XF-700", "1", "Relais", "KA1")
	     << drawn("XF-700", "1", "Relais", "KA2")
	     << drawn("XF-700", "2", "Relais", "KA3");

	const BomCollector::Result result =
			BomCollector::collect(rows, QList<LocationTree::BomLine>());

	REQUIRE(result.lines.size() == 2);
	CHECK(pieces(result) == 3.0);
	CHECK(lineOf(result, "XF-700", 1).quantity == 2.0);
	CHECK(lineOf(result, "XF-700", 2).quantity == 1.0);

	SECTION("e a revisão em branco é a que estiver corrente")
	{
		CHECK(BomCollector::revisionOf(QString()) == 0);
		CHECK(BomCollector::revisionOf(QStringLiteral(" 2 ")) == 2);
			//Nothing usable reads as "whatever is current", which is
			//the answer a project that never used revisions needs.
		CHECK(BomCollector::revisionOf(QStringLiteral("B")) == 0);
		CHECK(BomCollector::revisionOf(QStringLiteral("-1")) == 0);
	}
}

TEST_CASE("T18 — componente sem peça sai nomeado, nunca como célula vazia",
	  "[bom]")
{
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XB-200", "1", "Contacteur 9 A", "K1")
	     << drawn("", "", "Voyant 22 mm", "H1")
	     << drawn("", "", "Voyant 22 mm", "H2");

	const BomCollector::Result result =
			BomCollector::collect(rows, QList<LocationTree::BomLine>());

		//One line and one piece: the two without a part are not on the
		//list, and the total does not pretend they are.
	REQUIRE(result.lines.size() == 1);
	CHECK(pieces(result) == 1.0);
		//And no line with an empty code, which is the shape the defect
		//takes when it is not caught: a row on the file carrying a
		//quantity and no way to order it.
	CHECK(LocationTree::indexOfBomLine(result.lines, QString(), 0) < 0);

	REQUIRE(BomCollector::countOf(result, BomCollector::Pendency::NoPart)
		== 2);
	const BomCollector::Pending pending = result.pendings.at(0);
	CHECK(pending.name == QString("H1"));
	CHECK(pending.designation == QString("Voyant 22 mm"));
	CHECK(pending.where == QString("1"));

		//Every row leaves through exactly one door, so the three counts
		//add up to what came in. A row that went missing cannot be
		//written down.
	CHECK(result.drawn + result.pendings.size() + result.excluded
	      == rows.size());
}

TEST_CASE("T18 — o item tirado da nomenclatura é contado, nunca calado",
	  "[bom]")
{
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XB-200", "1", "Contacteur 9 A", "K1")
	     << drawn("XB-200", "1", "Contacteur 9 A", "K2");
	for (int i = 0 ; i < 7 ; ++ i)
	{
		BomCollector::ElementRow row =
				drawn("XG-800", "1", "Repère", "W1");
		row.exclude_from_bom = QStringLiteral("true");
		rows << row;
	}

	const BomCollector::Result result =
			BomCollector::collect(rows, QList<LocationTree::BomLine>());

		//The file comes out without them, exactly as it does today - and
		//with the number in front of it, which is the whole difference
		//between a short list and a wrong one.
	REQUIRE(result.lines.size() == 1);
	CHECK(pieces(result) == 2.0);
	CHECK(result.excluded == 7);
	CHECK(result.drawn + result.pendings.size() + result.excluded
	      == rows.size());

	SECTION("e a leitura da caixa é a mesma da vista, letra por letra")
	{
			//Deliberately this literal and no looser. The
			//nomenclature view compares the cell against 'true' and
			//nothing else, so a collector that also accepted "True"
			//or "1" would drop a row the printed nomenclature still
			//shows - two lists describing two different projects,
			//neither able to say so.
		CHECK(BomCollector::isExcluded(QStringLiteral("true")));
		CHECK_FALSE(BomCollector::isExcluded(QStringLiteral("True")));
		CHECK_FALSE(BomCollector::isExcluded(QStringLiteral("1")));
		CHECK_FALSE(BomCollector::isExcluded(QString()));
		CHECK_FALSE(BomCollector::isExcluded(QStringLiteral("false")));
	}
}

TEST_CASE("T18 — localização sem peça conta um por lugar, e não um por linha",
	  "[bom]")
{
		//LocationTree::bomLines() puts every location nobody assigned a
		//part to on one code-less line. Reporting that line as one item
		//would announce one missing cabinet where there are three.
	QList<LocationTree::BomLine> enclosures;
	enclosures << enclosure("", 0, "QCM1", 3.0,
				QStringList{QStringLiteral("QCM1"),
					    QStringLiteral("QCM2"),
					    QStringLiteral("QCM2/PORTE")});

	const BomCollector::Result result =
			BomCollector::collect(QList<BomCollector::ElementRow>(),
					      enclosures);

	CHECK(result.lines.isEmpty());
	CHECK(pieces(result) == 0.0);
	REQUIRE(BomCollector::countOf(result, BomCollector::Pendency::NoPart)
		== 3);
	CHECK(result.pendings.at(2).name == QString("PORTE"));
	CHECK(result.pendings.at(2).where == QString("QCM2/PORTE"));
}

TEST_CASE("T18 — metro não entra na conta de peça, e é anunciado", "[bom]")
{
		//A duct bought by the metre and a place bought by the piece,
		//under one part code. Added together the total stays plausible
		//and means nothing, which is why the second one is announced
		//instead of added - and instead of being split onto a line of
		//its own, because the key of a line is the part and its
		//revision, and a line per unit would be a second key nobody
		//declared.
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XH-010", "1", "Goulotte 60x60", "G1", "2.5", "m");

	QList<LocationTree::BomLine> enclosures;
	enclosures << enclosure("XH-010", 1, "Goulotte", 1.0,
				QStringList{QStringLiteral("QCM1")});

	const BomCollector::Result result =
			BomCollector::collect(rows, enclosures);

	REQUIRE(result.lines.size() == 1);
	CHECK(result.lines.at(0).unit == QString("m"));
	CHECK(BomCollector::totalOf(result.lines, QStringLiteral("m"))
	      == Approx(2.5));
		//Nothing of it reads as a piece, which is the assertion that
		//would fail if the two had been added.
	CHECK(pieces(result) == 0.0);
	CHECK(BomCollector::countOf(result,
				    BomCollector::Pendency::UnitConflict) == 1);

	SECTION("e milímetro não se soma com metro")
	{
		QList<BomCollector::ElementRow> two;
		two << drawn("XH-010", "1", "Goulotte", "G1", "2.5", "m")
		    << drawn("XH-020", "1", "Rail", "R1", "300", "mm");

		const BomCollector::Result mixed =
				BomCollector::collect(
					two, QList<LocationTree::BomLine>());
		REQUIRE(mixed.lines.size() == 2);
		CHECK(BomCollector::totalOf(mixed.lines, QStringLiteral("m"))
		      == Approx(2.5));
		CHECK(BomCollector::totalOf(mixed.lines, QStringLiteral("mm"))
		      == Approx(300.0));
	}
}

TEST_CASE("T18 — quantidade que não é número vira pendência, não vira peça",
	  "[bom]")
{
	bool ok = false;

	CHECK(BomCollector::quantityOf(QString(), &ok) == 1.0);
	CHECK(ok);
	CHECK(BomCollector::quantityOf(QStringLiteral(" 5 "), &ok) == 5.0);
	CHECK(ok);
	CHECK(BomCollector::quantityOf(QStringLiteral("2.5"), &ok)
	      == Approx(2.5));
	CHECK(ok);

		//Refused: none of the three is a quantity somebody can be handed.
		//The comma is the trap of the three - read in the C locale "1,5"
		//is not a number, and a reader that quietly took the 1 would
		//order half a metre less with nothing on paper to show for it.
	BomCollector::quantityOf(QStringLiteral("1,5"), &ok);
	CHECK_FALSE(ok);
	BomCollector::quantityOf(QStringLiteral("deux"), &ok);
	CHECK_FALSE(ok);
	BomCollector::quantityOf(QStringLiteral("0"), &ok);
	CHECK_FALSE(ok);

	QList<BomCollector::ElementRow> rows;
	rows << drawn("XB-200", "1", "Contacteur 9 A", "K1")
	     << drawn("XB-200", "1", "Contacteur 9 A", "K2", "1,5");

	const BomCollector::Result result =
			BomCollector::collect(rows, QList<LocationTree::BomLine>());

	REQUIRE(result.lines.size() == 1);
	CHECK(pieces(result) == 1.0);
	CHECK(BomCollector::countOf(
		      result, BomCollector::Pendency::UnreadableQuantity) == 1);
	CHECK(result.pendings.at(0).name == QString("K2"));
}

TEST_CASE("T18 — a mesma lista sai na mesma ordem, venha na ordem que vier",
	  "[bom]")
{
		//Two exports of one project are read side by side, line by line.
		//An order that depended on which half named a part first would
		//make every line look moved.
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XC-300", "1", "Disjoncteur", "Q1")
	     << drawn("XA-100", "1", "Coffret", "A1")
	     << drawn("XB-200", "2", "Contacteur", "K1")
	     << drawn("XB-200", "1", "Contacteur", "K2");

	QList<BomCollector::ElementRow> reversed;
	for (int i = int(rows.size()) - 1 ; i >= 0 ; -- i) {
		reversed << rows.at(i);
	}

	const BomCollector::Result first =
			BomCollector::collect(rows, QList<LocationTree::BomLine>());
	const BomCollector::Result second =
			BomCollector::collect(reversed,
					      QList<LocationTree::BomLine>());

	REQUIRE(first.lines.size() == 4);
	REQUIRE(second.lines.size() == first.lines.size());
	CHECK(pieces(first) == 4.0);
	CHECK(pieces(second) == pieces(first));

	QStringList keys;
	for (const LocationTree::BomLine &line : first.lines)
	{
		keys << line.part_code + QStringLiteral("/")
			+ QString::number(line.part_revision);
		const int same = LocationTree::indexOfBomLine(
					second.lines, line.part_code,
					line.part_revision);
		REQUIRE(same >= 0);
		CHECK(second.lines.at(same).quantity == line.quantity);
	}
	QStringList expected;
	expected << QStringLiteral("XA-100/1")
		 << QStringLiteral("XB-200/1")
		 << QStringLiteral("XB-200/2")
		 << QStringLiteral("XC-300/1");
	CHECK(keys == expected);
}

TEST_CASE("T18 — a pergunta é feita à vista que não filtra", "[bom]")
{
		//element_label_view and not element_nomenclature_view, and the
		//difference is the whole of the exclusion count: the filtered
		//view has already dropped those rows, so a collector reading it
		//could only count them by asking a second question - and two
		//questions can disagree.
	CHECK(BomCollector::viewName() == QString("element_label_view"));

	const QString statement = BomCollector::selectStatement();
	CHECK(statement.contains(QStringLiteral("element_label_view")));
	CHECK_FALSE(statement.contains(
			    QStringLiteral("element_nomenclature_view")));

		//The key of the line is selected, and so is the box - without it
		//there is nothing to count.
	CHECK(BomCollector::columns().contains(QStringLiteral("part_code")));
	CHECK(BomCollector::columns().contains(QStringLiteral("part_revision")));
	CHECK(BomCollector::columns().contains(
		      QStringLiteral("exclude_from_bom")));
	CHECK(statement.contains(QStringLiteral("ORDER BY")));

	SECTION("e uma linha da vista vira uma linha do coletor")
	{
		QHash<QString, QString> values;
		values.insert(QStringLiteral("part_code"),
			      QStringLiteral("XB-200"));
		values.insert(QStringLiteral("part_revision"),
			      QStringLiteral("2"));
		values.insert(QStringLiteral("designation"),
			      QStringLiteral("Contacteur 9 A"));
		values.insert(QStringLiteral("quantity"), QStringLiteral("3"));
		values.insert(QStringLiteral("unity"), QStringLiteral("un"));
		values.insert(QStringLiteral("label"), QStringLiteral("K1"));
		values.insert(QStringLiteral("folio"), QStringLiteral("4"));
		values.insert(QStringLiteral("exclude_from_bom"),
			      QStringLiteral("false"));

		const BomCollector::ElementRow row =
				BomCollector::rowFrom(values);
		CHECK(row.part_code == QString("XB-200"));
		CHECK(row.part_revision == QString("2"));
		CHECK(row.designation == QString("Contacteur 9 A"));
		CHECK(row.quantity == QString("3"));
		CHECK(row.label == QString("K1"));
		CHECK(row.folio == QString("4"));
		CHECK_FALSE(BomCollector::isExcluded(row.exclude_from_bom));
	}
}
