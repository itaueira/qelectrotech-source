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
#include "../../../sources/location/mountinglayout.h"
#include "../../../sources/location/mountingprofile.h"
#include "qt_catch_tostring.h"

#include <cmath>

#include <QHash>
#include <QList>
#include <QPointF>
#include <QSet>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QUuid>

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

		/// @return how many metres the whole list comes to
	double metres(const BomCollector::Result &result)
	{
		return BomCollector::totalOf(result.lines,
					     BomMeasure::defaultLengthUnit());
	}

		/// @return the line of the bar of @p profile, an empty one when
		/// there is none
	LocationTree::BomLine barOf(const BomCollector::Result &result,
				    const MountingProfile &profile)
	{
		const int found = LocationTree::indexOfBomLine(
					result.lines, profile.key(), 0);
		return found < 0 ? LocationTree::BomLine()
				 : result.lines.at(found);
	}

		/// @return how many pieces of the plate @p rows stand for
	int piecesIn(const QList<BomCollector::MountedRow> &rows)
	{
		int total = 0;
		for (const BomCollector::MountedRow &row : rows) {
			total += row.pieces;
		}
		return total;
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

/*
	The third half: what is screwed to a plate and drawn on no folio.

	A panel laid out before its schematic, and the parts nobody ever draws -
	an end bracket, a distribution block - are on the plate and nowhere
	else, and a list that walks the folios and the locations comes back
	without them. The hard part is "once": a part whose symbol is on a
	folio is counted by that symbol, and the plate must not count it again.
	A plate that did would change no line count at all - the part's line
	is there either way - and would read one piece more inside a total that
	stays plausible. So each case asserts the number of lines and the
	total, and the first one measures the careless join beside the right
	one.
*/

namespace
{
		/// The element of the folio the contactor K1 is drawn as.
	QString contactorElement()
	{
		return QStringLiteral("{3f1d2c4b-5a69-4788-9a0b-1c2d3e4f5a01}");
	}

		/// One part screwed to a plate: its identity, its tag, its code,
		/// and the element of the folio it is, when it is one.
	MountedItem plated(const char *uuid,
			   const char *label,
			   const char *code,
			   const QString &element = QString())
	{
		MountedItem item(QString::fromUtf8(label), QPointF(10.0, 10.0),
				 QSizeF(20.0, 40.0));
		item.uuid = QString::fromUtf8(uuid);
		item.part_code = QString::fromUtf8(code);
		item.element = element;
		return item;
	}

		/// Half a metre of rail cut on the bench: no code, no symbol.
	MountedItem railPiece(const char *uuid)
	{
		MountedItem rail;
		rail.uuid = QString::fromUtf8(uuid);
		rail.profile = MountingProfile::rail(35.0, 7.5);
		rail.position = QPointF(10.0, 200.0);
		rail.size = rail.profile.sizeFor(500.0, MountingRun::Across);
		return rail;
	}

	/**
		Two enclosures. On the plate of QCM1: the contactor K1, tied to
		its symbol; two terminal blocks and a second contactor of the same
		part, laid from the catalogue with no symbol; a piece of rail. On
		the plate of QCM2: one more terminal block with no symbol.
	*/
	MountingLayout benchLayout()
	{
		MountingLayout layout;
		const QString first = layout.appendSurface(
					MountingSurface(QStringLiteral("QCM1"),
							QStringLiteral("plate"),
							MountingArea(600.0, 800.0)));
		const QString second = layout.appendSurface(
					MountingSurface(QStringLiteral("QCM2"),
							QStringLiteral("plate"),
							MountingArea(400.0, 600.0)));
		REQUIRE_FALSE(first.isEmpty());
		REQUIRE_FALSE(second.isEmpty());

		const QList<MountedItem> on_first {
			plated("k1", "K1", "XB-200", contactorElement()),
			plated("t1", "", "XT-100"),
			plated("t2", "", "XT-100"),
			plated("k2", "", "XB-200"),
			railPiece("rail")
		};
		for (const MountedItem &item : on_first) {
			REQUIRE_FALSE(layout.mountItem(first, item).isEmpty());
		}
		REQUIRE_FALSE(layout.mountItem(second, plated("t3", "", "XT-100"))
			      .isEmpty());
		return layout;
	}
}

TEST_CASE("T19 — a peça posta na placa sem símbolo entra uma vez na lista, e a que tem símbolo não conta de novo",
	  "[bom][t19]")
{
		//The schematic draws two components, and one of them, K1, is on
		//the plate too, tied to its symbol. No revision is typed on the
		//symbols, so they share the line of the plate's parts, which keep
		//none.
	QList<BomCollector::ElementRow> rows;
	rows << drawn("XB-200", "", "Contacteur 9 A", "K1")
	     << drawn("XC-300", "", "Disjoncteur 3P 25 A", "Q1");

	const MountingLayout layout = benchLayout();
	const QSet<QUuid> drawn_elements {QUuid(contactorElement())};

	const QList<BomCollector::MountedRow> mounted =
			BomCollector::mountedRowsOf(layout, drawn_elements);
		//Four parts - not K1, which its symbol counts - and then the
		//rail: one row of half a metre of its bar, never a piece.
	REQUIRE(mounted.size() == 5);
	const BomCollector::MountedRow bar = mounted.at(4);
	CHECK(bar.part_code == MountingProfile::rail(35.0, 7.5).key());
	CHECK(bar.unit == BomMeasure::defaultLengthUnit());
	CHECK(bar.quantity == Approx(0.5));
	CHECK(bar.pieces == 1);
	CHECK(bar.where == QString("QCM1"));

	const BomCollector::Result drawn_only =
			BomCollector::collect(rows, QList<LocationTree::BomLine>());
	const BomCollector::Result result =
			BomCollector::collect(rows, QList<LocationTree::BomLine>(),
					      mounted);

		//What the list says without the plate: the four parts screwed
		//to it are not bought, and nothing in the file says so.
	CHECK(drawn_only.lines.size() == 2);
	CHECK(pieces(drawn_only) == 2.0);
	CHECK(metres(drawn_only) == 0.0);

		//Four lines says the terminal block and the rail arrived; six
		//pieces says K1 did not arrive twice and the rail was not
		//counted by the piece; half a metre says it was counted by its
		//bar instead.
	REQUIRE(result.lines.size() == 4);
	CHECK(pieces(result) == 6.0);
	CHECK(metres(result) == Approx(0.5));

	const LocationTree::BomLine rail =
			barOf(result, MountingProfile::rail(35.0, 7.5));
	CHECK(rail.quantity == Approx(0.5));
	CHECK(rail.unit == BomMeasure::defaultLengthUnit());
	CHECK(rail.name == MountingProfile::rail(35.0, 7.5).designation());
	CHECK(rail.paths == QStringList({QStringLiteral("QCM1")}));

	const LocationTree::BomLine contactor = lineOf(result, "XB-200");
		//K1 from its symbol, the second contactor from the plate
	CHECK(contactor.quantity == 2.0);
		//Named by the symbol: the plate keeps no name of the product
	CHECK(contactor.name == QString("Contacteur 9 A"));
		//K1 once, and the part with no tag by the place it is in
	CHECK(contactor.paths == QStringList({QStringLiteral("K1"),
					      QStringLiteral("QCM1")}));

	const LocationTree::BomLine block = lineOf(result, "XT-100");
	CHECK(block.quantity == 3.0);
	CHECK(block.unit == BomMeasure::countUnit());
		//Two plates added into one line, and each place said once and
		//not once per terminal block
	CHECK(block.paths == QStringList({QStringLiteral("QCM1"),
					  QStringLiteral("QCM2")}));
	CHECK(block.name.isEmpty());

	CHECK(lineOf(result, "XC-300").quantity == 1.0);
		//The rail is neither a line with no code nor a pendency
	CHECK(LocationTree::indexOfBomLine(result.lines, QString(), 0) < 0);
	CHECK(result.pendings.isEmpty());
	CHECK(result.drawn == 2);
		//Pieces of the plate, the piece of rail among them, and never
		//its metres
	CHECK(result.mounted == 5);

	SECTION("o que segura o total é o vínculo, e a contagem de linhas não o vê")
	{
			//The careless join, built by hand: every part of the plate
			//that is not a cut piece, its link ignored, and the bar as
			//the right join gives it. The same four lines, one piece
			//more - which is why the case asserts the total and not
			//only the lines.
		QList<BomCollector::MountedRow> careless;
		for (const QString &uuid : layout.mountedItemUuids())
		{
			const MountedItem item = layout.item(uuid);
			if (item.isCutToLength()) {
				continue;
			}
			BomCollector::MountedRow row;
			row.part_code = item.part_code;
			row.label = item.label;
			row.name = item.designation();
			careless << row;
		}
		careless << bar;

		const BomCollector::Result twice =
				BomCollector::collect(rows,
						      QList<LocationTree::BomLine>(),
						      careless);
		CHECK(twice.lines.size() == result.lines.size());
		CHECK(pieces(twice) == 7.0);
		CHECK(metres(twice) == Approx(0.5));
	}

	SECTION("o símbolo apagado passa a conta para a placa, e o total não muda")
	{
			//K1 deleted from its folio: it has no row any more, and no
			//folio holds its element. The part is still on the plate,
			//and is still bought once - by the plate now.
		QList<BomCollector::ElementRow> without_k1;
		without_k1 << rows.at(1);
		const QList<BomCollector::MountedRow> now =
				BomCollector::mountedRowsOf(layout, QSet<QUuid>());
		REQUIRE(now.size() == 6);

		const BomCollector::Result moved =
				BomCollector::collect(without_k1,
						      QList<LocationTree::BomLine>(),
						      now);
		REQUIRE(moved.lines.size() == 4);
		CHECK(pieces(moved) == 6.0);
		CHECK(metres(moved) == Approx(0.5));
		CHECK(lineOf(moved, "XB-200").quantity == 2.0);
		CHECK(lineOf(moved, "XB-200").paths
		      == QStringList({QStringLiteral("K1"), QStringLiteral("QCM1")}));
		CHECK(moved.drawn == 1);
		CHECK(moved.mounted == 6);
	}

	SECTION("a caixa marcada no símbolo vale também para a peça dele na placa")
	{
			//Kept out of the parts list on the folio. The plate does not
			//bring it back: the symbol decides, whatever it decides.
		QList<BomCollector::ElementRow> kept_out = rows;
		kept_out[0].exclude_from_bom = QStringLiteral("true");

		const BomCollector::Result result_kept =
				BomCollector::collect(kept_out,
						      QList<LocationTree::BomLine>(),
						      mounted);
		REQUIRE(result_kept.lines.size() == 4);
		CHECK(pieces(result_kept) == 5.0);
		CHECK(metres(result_kept) == Approx(0.5));
		CHECK(lineOf(result_kept, "XB-200").quantity == 1.0);
		CHECK(result_kept.excluded == 1);
	}
}

TEST_CASE("T19 — sem folha nenhuma a lista sai da placa, e a peça sem código sai nomeada onde está",
	  "[bom][t19]")
{
		//The layout before the schematic: not one folio, one enclosure
		//with a plate and a door, and every part laid from the catalogue.
		//One of them was laid with no code at all.
	MountingLayout layout;
	const QString plate = layout.appendSurface(
				MountingSurface(QStringLiteral("QCM1"),
						QStringLiteral("plate"),
						MountingArea(600.0, 800.0)));
	const QString door = layout.appendSurface(
				MountingSurface(QStringLiteral("QCM1"),
						QStringLiteral("door"),
						MountingArea(600.0, 800.0)));
	REQUIRE_FALSE(plate.isEmpty());
	REQUIRE_FALSE(door.isEmpty());

	for (const MountedItem &item : {plated("t1", "", "XT-100"),
					plated("t2", "", "XT-100"),
					railPiece("rail"),
					plated("loose", "", "")})
	{
		REQUIRE_FALSE(layout.mountItem(plate, item).isEmpty());
	}
	REQUIRE_FALSE(layout.mountItem(door, plated("h1", "H1", "XL-022"))
		      .isEmpty());

	const QList<BomCollector::MountedRow> mounted =
			BomCollector::mountedRowsOf(layout, QSet<QUuid>());
		//The parts face by face, in the order they were laid, the rail
		//not among them: t1, t2, loose, then the door. And then the bar
		//the rail was cut from, which has no code and is still no
		//pendency: its key is its profile.
	REQUIRE(mounted.size() == 5);
	CHECK(mounted.at(3).label == QString("H1"));
	CHECK(mounted.at(3).where == QString("QCM1"));
	CHECK(mounted.at(4).part_code == MountingProfile::rail(35.0, 7.5).key());

	const BomCollector::Result result =
			BomCollector::collect(QList<BomCollector::ElementRow>(),
					      QList<LocationTree::BomLine>(),
					      mounted);

		//The list comes out with no schematic behind it at all
	REQUIRE(result.lines.size() == 3);
	CHECK(pieces(result) == 3.0);
	CHECK(metres(result) == Approx(0.5));
	CHECK(lineOf(result, "XT-100").quantity == 2.0);
	CHECK(lineOf(result, "XL-022").quantity == 1.0);
	CHECK(barOf(result, MountingProfile::rail(35.0, 7.5)).quantity
	      == Approx(0.5));
		//A part with a tag is named by its tag, as a symbol is
	CHECK(lineOf(result, "XL-022").paths == QStringList({QStringLiteral("H1")}));

		//The part with no code is not a line with an empty code, which
		//nobody can order from: it is a pendency, named by the plate's
		//own name for it and by the enclosure it is in
	CHECK(LocationTree::indexOfBomLine(result.lines, QString(), 0) < 0);
	REQUIRE(BomCollector::countOf(result, BomCollector::Pendency::NoPart)
		== 1);
	CHECK(result.pendings.size() == 1);
	CHECK(result.pendings.at(0).name == QString("loose"));
	CHECK(result.pendings.at(0).where == QString("QCM1"));

		//Every piece that came in left through one door - five here, the
		//piece of rail among them, and each pendency is one piece
	REQUIRE(piecesIn(mounted) == 5);
	CHECK(result.mounted + result.pendings.size() == piecesIn(mounted));
}

/*
	The bar rail and duct are cut from (T22, step 11).

	They are not bought by the piece and have no catalogue part: what the
	shop orders is the length used, one line per profile. Two numbers can
	go wrong here, and each case asserts both. The count of lines catches a
	bar split over two lines or two bars run into one; only the metres
	catch a length added into the wrong line, or a breaker's width added
	into a rail - which changes no count and comes out as a total that
	still reads like one somebody measured.
*/

namespace
{
		/// One piece cut from a bar of @p profile, @p length long
	MountedItem cutPiece(const MountingProfile &profile,
			     MountingRun run,
			     qreal x,
			     qreal y,
			     qreal length)
	{
		MountedItem piece;
		piece.profile = profile;
		piece.run = run;
		piece.position = QPointF(x, y);
		piece.size = profile.sizeFor(length, run);
		return piece;
	}

		/// One piece of a bar of @p profile that nobody has given a
		/// length to: on the plate, and not cut yet
	MountedItem uncutPiece(const MountingProfile &profile)
	{
		MountedItem piece;
		piece.profile = profile;
		piece.position = QPointF(10.0, 700.0);
		return piece;
	}

	MountingProfile standardRail()
	{
		return MountingProfile::rail(35.0, 7.5);
	}

		/// The same width on the plate, and another bar on the shelf
	MountingProfile deepRail()
	{
		return MountingProfile::rail(35.0, 15.0);
	}

	MountingProfile duct()
	{
		return MountingProfile::duct(40.0, 60.0);
	}
}

TEST_CASE("T22 — trilho e canaleta entram na lista pelo metro somado, uma linha por perfil",
	  "[bom][t22]")
{
		//A plate and a door of one enclosure. On the plate: three pieces
		//of standard rail, two of duct, one of a deeper rail of the same
		//width, and a breaker bought as a piece with no symbol. On the
		//door: one more piece of standard rail.
	MountingLayout layout;
	const QString plate = layout.appendSurface(
				MountingSurface(QStringLiteral("QCM1"),
						QStringLiteral("plate"),
						MountingArea(600.0, 800.0)));
	const QString door = layout.appendSurface(
				MountingSurface(QStringLiteral("QCM1/PORTE"),
						QStringLiteral("door"),
						MountingArea(500.0, 700.0)));
	REQUIRE_FALSE(plate.isEmpty());
	REQUIRE_FALSE(door.isEmpty());

	for (const MountedItem &item :
	     {cutPiece(standardRail(), MountingRun::Across, 10.0, 100.0, 480.0),
	      cutPiece(standardRail(), MountingRun::Across, 10.0, 300.0, 480.0),
	      cutPiece(standardRail(), MountingRun::Across, 10.0, 500.0, 240.0),
	      cutPiece(duct(), MountingRun::Down, 560.0, 10.0, 560.0),
	      cutPiece(duct(), MountingRun::Across, 10.0, 10.0, 540.0),
	      cutPiece(deepRail(), MountingRun::Across, 10.0, 600.0, 300.0),
	      plated("q1", "", "XQ-100")})
	{
		REQUIRE_FALSE(layout.mountItem(plate, item).isEmpty());
	}
	REQUIRE_FALSE(layout.mountItem(door,
				       cutPiece(standardRail(),
						MountingRun::Across,
						20.0, 20.0, 300.0))
		      .isEmpty());

	const QList<BomCollector::MountedRow> mounted =
			BomCollector::mountedRowsOf(layout, QSet<QUuid>());
		//The breaker, then the three bars of the plate, then the one of
		//the door: one row per profile and face, never one per piece.
	REQUIRE(mounted.size() == 5);
	CHECK(piecesIn(mounted) == 8);

	const BomCollector::Result result =
			BomCollector::collect(QList<BomCollector::ElementRow>(),
					      QList<LocationTree::BomLine>(),
					      mounted);

		//Four lines: the breaker, and one per profile - the rail of the
		//door on the line of the rail of the plate, and the deeper rail
		//on a line of its own.
	REQUIRE(result.lines.size() == 4);
		//And the totals, which are what a wrong join would hide in: 1,5
		//+ 1,1 + 0,3 metres, and the breaker one piece and not one
		//millimetre.
	CHECK(metres(result) == Approx(2.9));
	CHECK(pieces(result) == 1.0);
	CHECK(lineOf(result, "XQ-100").quantity == 1.0);

	const LocationTree::BomLine rail = barOf(result, standardRail());
	CHECK(rail.quantity == Approx(1.5));
	CHECK(rail.unit == BomMeasure::defaultLengthUnit());
	CHECK(rail.name == standardRail().designation());
	CHECK(rail.part_revision == 0);
		//Each face it was cut on said once
	CHECK(rail.paths == QStringList({QStringLiteral("QCM1"),
					 QStringLiteral("QCM1/PORTE")}));

	CHECK(barOf(result, duct()).quantity == Approx(1.1));
	CHECK(barOf(result, duct()).name == duct().designation());
	CHECK(barOf(result, deepRail()).quantity == Approx(0.3));
	CHECK(barOf(result, deepRail()).unit == BomMeasure::defaultLengthUnit());

		//The list cannot come to another total than the layout's own:
		//every bar of the project is one line, and that line says what
		//MountingLayout::profileTotals() says.
	const QList<MountingProfileTotal> totals = layout.profileTotals();
	REQUIRE(totals.size() == 3);
	for (const MountingProfileTotal &total : totals)
	{
		INFO(total.profile.key().toStdString());
		CHECK(barOf(result, total.profile).quantity
		      == Approx(total.metres()));
	}

	CHECK(result.pendings.isEmpty());
		//Pieces of the plate and never metres: the breaker, four pieces
		//of standard rail, two of duct, one of deep rail.
	CHECK(result.mounted == 8);

	SECTION("a linha se consolida pela chave, nunca pelo número")
	{
			//Built by hand, so that the numbers are chosen to be the
			//trap: the same metres under two keys are two lines, and
			//the same key on two faces is one line of the sum - not
			//one line of 0,6 for having read the same number twice.
		BomCollector::MountedRow first;
		first.part_code = standardRail().key();
		first.name = standardRail().designation();
		first.designation = first.name;
		first.where = QStringLiteral("QCM1");
		first.quantity = 0.6;
		first.unit = BomMeasure::defaultLengthUnit();

		BomCollector::MountedRow second = first;
		second.where = QStringLiteral("QCM2");

		BomCollector::MountedRow other = first;
		other.part_code = duct().key();
		other.name = duct().designation();
		other.designation = other.name;

		const BomCollector::Result joined =
				BomCollector::collect(
					QList<BomCollector::ElementRow>(),
					QList<LocationTree::BomLine>(),
					{first, second, other});

		REQUIRE(joined.lines.size() == 2);
		CHECK(metres(joined) == Approx(1.8));
		CHECK(barOf(joined, standardRail()).quantity == Approx(1.2));
		CHECK(barOf(joined, duct()).quantity == Approx(0.6));
		CHECK(joined.mounted == 3);
	}
}

TEST_CASE("T22 — o pedaço de trilho sem comprimento é pendência, e nunca zero metro",
	  "[bom][t22]")
{
		//One piece of rail cut to 480, one piece of the same rail nobody
		//gave a length to, and a piece of duct nobody gave a length to
		//either - the only piece of duct on the plate.
	MountingLayout layout;
	const QString plate = layout.appendSurface(
				MountingSurface(QStringLiteral("QCM1"),
						QStringLiteral("plate"),
						MountingArea(600.0, 800.0)));
	REQUIRE_FALSE(plate.isEmpty());
	for (const MountedItem &item :
	     {cutPiece(standardRail(), MountingRun::Across, 10.0, 100.0, 480.0),
	      uncutPiece(standardRail()),
	      uncutPiece(duct())})
	{
		REQUIRE_FALSE(layout.mountItem(plate, item).isEmpty());
	}

	const QList<BomCollector::MountedRow> mounted =
			BomCollector::mountedRowsOf(layout, QSet<QUuid>());
		//The measured rail, and one row per piece nobody measured: the
		//duct has no row of metres at all, and no row of zero metres.
	REQUIRE(mounted.size() == 3);
	int unmeasured = 0;
	for (const BomCollector::MountedRow &row : mounted)
	{
		CHECK(row.quantity != 0.0);
		if (std::isnan(row.quantity)) {
			++ unmeasured;
		}
	}
	CHECK(unmeasured == 2);

	const BomCollector::Result result =
			BomCollector::collect(QList<BomCollector::ElementRow>(),
					      QList<LocationTree::BomLine>(),
					      mounted);

		//One line, of the 480 that were measured, and the duct on none:
		//a line of zero metres would read as a duct somebody measured.
	REQUIRE(result.lines.size() == 1);
	CHECK(metres(result) == Approx(0.48));
	CHECK(barOf(result, standardRail()).quantity == Approx(0.48));
	CHECK(LocationTree::indexOfBomLine(result.lines, duct().key(), 0) < 0);
	for (const LocationTree::BomLine &line : result.lines) {
		CHECK(line.quantity > 0.0);
	}

		//The two pieces nobody measured, each a pendency of its own,
		//saying which bar and where - and with no tag, which a bar
		//does not carry, rather than its profile written twice.
	REQUIRE(BomCollector::countOf(result,
				      BomCollector::Pendency::UnreadableQuantity)
		== 2);
	CHECK(result.pendings.size() == 2);
	QStringList said;
	for (const BomCollector::Pending &pending : result.pendings)
	{
		CHECK(pending.name.isEmpty());
		CHECK(pending.where == QString("QCM1"));
		said << pending.designation;
	}
	said.sort();
	QStringList expected {duct().designation(),
			      standardRail().designation()};
	expected.sort();
	CHECK(said == expected);

		//As many as the layout itself counts apart
	int uncut = 0;
	const QList<MountingProfileTotal> totals = layout.profileTotals();
	for (const MountingProfileTotal &total : totals) {
		uncut += total.uncut;
	}
	CHECK(uncut == 2);

		//Every piece left through one door: one on a line, two pending.
	CHECK(result.mounted == 1);
	CHECK(result.mounted + result.pendings.size() == piecesIn(mounted));

	SECTION("zero metro escrito à mão também não vira linha")
	{
			//The same refusal a symbol whose quantity field says zero
			//gets: zero looks like an answer and is not one a list can
			//order from.
		BomCollector::MountedRow zero;
		zero.part_code = standardRail().key();
		zero.name = standardRail().designation();
		zero.designation = zero.name;
		zero.where = QStringLiteral("QCM1");
		zero.quantity = 0.0;
		zero.unit = BomMeasure::defaultLengthUnit();

		const BomCollector::Result refused =
				BomCollector::collect(
					QList<BomCollector::ElementRow>(),
					QList<LocationTree::BomLine>(),
					{zero});
		CHECK(refused.lines.isEmpty());
		CHECK(BomCollector::countOf(
			      refused, BomCollector::Pendency::UnreadableQuantity)
		      == 1);
		CHECK(refused.mounted == 0);
	}
}
