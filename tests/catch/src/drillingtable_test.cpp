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
#include "../../../sources/location/drillingtable.h"
#include "../../../sources/location/mountingmeasure.h"
#include "qt_catch_tostring.h"

#include <QLocale>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>

/*
	The drilling table, written before there is a drilling view to fill it.

	Two things are settled here and they are of different kinds.

	The first is the frame. A coordinate in this file is a coordinate in
	the frame MountingArea declares - millimetre, x right, y DOWN, origin
	at the top left corner of the surface - and the way that is nailed is
	by building the table's numbers with MountingMeasure::coordinateOf
	rather than by typing them: if the two files ever disagree about the
	sign of y, these cases fail rather than the workshop finding out.

	The second is the writing. A drilling table is a grid of numbers with
	two columns of text a person typed into, and one of those columns -
	the destination - routinely holds a semicolon, because that is how a
	person writes "passage de câble ; côté gauche". A cell holding the
	separator shifts every column of its row, and a shifted row opens in a
	spreadsheet looking exactly as well formed as a right one: purchasing
	reads a diameter where the destination should be, and the panel is
	drilled from it.

	So the reading here is done by a reader and not by a split. Splitting
	the emitted line on the separator is what a naive consumer does, and
	it is wrong on a quoted file by construction; parsing it the way RFC
	4180 says is what a spreadsheet does, and it is the only check that
	says anything about whether the file is right.
*/

namespace
{
	/**
		@brief An RFC-4180 reader, on the whole text.
		@param text the emitted table
		@param separator the delimiter it was written with
		@return one QStringList per row, cells unquoted

		On the whole text and not line by line, on purpose: a cell
		holding an end of line is written as one quoted cell spanning
		two lines of the file, and a reader that split on "\n" first
		would count it as two rows. Counting rows is one of the things
		these cases check, so the reader has to be able to get it right.
	*/
	QList<QStringList> parseTable(const QString &text,
				      const QString &separator)
	{
		QList<QStringList> rows;
		QStringList cells;
		QString cell;
		bool quoted = false;
		int i = 0;

		while (i < text.size())
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
						i += 2;
						continue;
					}
					quoted = false;
					++ i;
					continue;
				}
				cell += c;
				++ i;
				continue;
			}

			if (c == QLatin1Char('"') && cell.isEmpty())
			{
				quoted = true;
				++ i;
				continue;
			}

			if (c == QLatin1Char('\n'))
			{
				cells << cell;
				cell.clear();
				rows << cells;
				cells.clear();
				++ i;
				continue;
			}

			if (!separator.isEmpty()
			    && text.mid(i, separator.size()) == separator)
			{
				cells << cell;
				cell.clear();
				i += separator.size();
				continue;
			}

			cell += c;
			++ i;
		}

		cells << cell;
		rows << cells;
		return rows;
	}

		/// The destination as somebody types it, semicolon included.
	const char *purpose_with_separator = "passage de câble ; côté gauche";

		/// No locale in the way of the numbers being read as written.
	QLocale plain()
	{
		return QLocale::c();
	}
}

TEST_CASE("CU-22.14 — un point-virgule dans la destination ne décale aucune "
	  "colonne",
	  "[location][mounting][drilling]")
{
	const QString destination = QString::fromUtf8(purpose_with_separator);

	DrillingHole gland = DrillingHole::drilled(QPointF(120.0, 65.0),
						   22.0,
						   destination);
	gland.component = QStringLiteral("-X1");

	const QString text = DrillingTable::toDelimitedText(QList<DrillingHole>{gland},
							    QStringLiteral(";"),
							    plain());

	const QList<QStringList> rows = parseTable(text, QStringLiteral(";"));

	SECTION("The row is one row, and it has the columns the header has")
	{
			//frame, header, one hole
		REQUIRE(rows.size() == 3);
		REQUIRE(rows.at(2).size() == DrillingTable::columnCount());
	}

	SECTION("Whoever reads the file sees the whole destination")
	{
		REQUIRE(rows.at(2).size() == DrillingTable::columnCount());
		CHECK(rows.at(2).last() == destination);
	}

	SECTION("The other cells are where they were, which is what shifting "
		"would have moved")
	{
		const QStringList cells = rows.at(2);
		REQUIRE(cells.size() == DrillingTable::columnCount());
		CHECK(cells.at(0) == QStringLiteral("-X1"));
		CHECK(cells.at(1) == QStringLiteral("120"));
		CHECK(cells.at(2) == QStringLiteral("65"));
	}

	SECTION("It was quoted, and only because it had to be")
	{
			//The proof that the row survived quoting rather than
			//having had the semicolon taken out of it.
		CHECK(text.contains(QLatin1Char('"')));
		CHECK(text.contains(destination));
	}

	SECTION("The same destination costs nothing when the separator is a "
		"tabulation")
	{
		const QString tabbed =
			DrillingTable::toDelimitedText(QList<DrillingHole>{gland},
						       QStringLiteral("\t"),
						       plain());
		const QList<QStringList> tabbed_rows =
			parseTable(tabbed, QStringLiteral("\t"));

		REQUIRE(tabbed_rows.size() == 3);
		REQUIRE(tabbed_rows.at(2).size() == DrillingTable::columnCount());
		CHECK(tabbed_rows.at(2).last() == destination);

			//A semicolon is not an odd character in a tabulated
			//file, and a rule that escaped a written-in ';' would
			//quote it anyway - which is how a copy of the rule that
			//knows one separator gives itself away.
		CHECK_FALSE(tabbed.contains(QLatin1Char('"')));
	}
}

TEST_CASE("T22 — every line of the file has the same number of cells",
	  "[location][mounting][drilling]")
{
	QList<DrillingHole> holes;

	DrillingHole ihm = DrillingHole::cutOut(QPointF(200.0, 150.0),
						QSizeF(92.0, 92.0),
						QString::fromUtf8("découpe IHM"));
	ihm.component = QStringLiteral("-A1");
	holes << ihm;

	DrillingHole stop = DrillingHole::drilled(QPointF(300.0, 150.0),
						  22.0,
						  QStringLiteral("arrêt d'urgence"));
	stop.component = QStringLiteral("-S1");
	holes << stop;

		//A hole belonging to nothing, which is the common one: a fixing
		//hole for a duct cut on the bench.
	holes << DrillingHole::drilled(QPointF(40.0, 40.0),
				       6.5,
				       QStringLiteral("fixation goulotte"));

	SECTION("Frame, header and every hole, with the separator in a cell")
	{
		const QString text =
			DrillingTable::toDelimitedText(holes,
						       QStringLiteral(";"),
						       plain());
		const QList<QStringList> rows = parseTable(text, QStringLiteral(";"));

		REQUIRE(rows.size() == holes.size() + 2);
		for (const QStringList &cells : rows) {
			CHECK(cells.size() == DrillingTable::columnCount());
		}
	}

	SECTION("The frame line is the first one, and it says where zero is")
	{
		const QString text =
			DrillingTable::toDelimitedText(holes,
						       QStringLiteral(";"),
						       plain());
		const QList<QStringList> rows = parseTable(text, QStringLiteral(";"));

		REQUIRE(rows.size() >= 2);
		CHECK(rows.at(0).at(0) == DrillingTable::referenceFrameText());

			//It holds a semicolon of its own, so the line that
			//declares the frame is the first line the quoting has
			//to carry. Without it the file loses a column before a
			//single hole is in it.
		CHECK(DrillingTable::referenceFrameText()
		      .contains(QLatin1Char(';')));

		CHECK(rows.at(1) == DrillingTable::header());
	}

	SECTION("A destination holding an end of line does not become a row")
	{
		DrillingHole split = DrillingHole::drilled(QPointF(10.0, 10.0),
							   8.0,
							   QStringLiteral("haut\nbas"));
		split.component = QStringLiteral("-X2");
		holes << split;

		const QString text =
			DrillingTable::toDelimitedText(holes,
						       QStringLiteral(";"),
						       plain());
		const QList<QStringList> rows = parseTable(text, QStringLiteral(";"));

			//The one form of the defect that changes how many rows
			//there are, which is why it is counted and not looked at.
		REQUIRE(rows.size() == holes.size() + 2);
		CHECK(rows.last().last() == QStringLiteral("haut\nbas"));
	}

	SECTION("A destination holding a double quote comes back with one")
	{
		DrillingHole quoted = DrillingHole::drilled(QPointF(10.0, 10.0),
							    8.0,
							    QStringLiteral("perçage \"provisoire\""));
		const QString text =
			DrillingTable::toDelimitedText(QList<DrillingHole>{quoted},
						       QStringLiteral(";"),
						       plain());
		const QList<QStringList> rows = parseTable(text, QStringLiteral(";"));

		REQUIRE(rows.size() == 3);
		REQUIRE(rows.at(2).size() == DrillingTable::columnCount());
		CHECK(rows.at(2).last() == QStringLiteral("perçage \"provisoire\""));
	}

	SECTION("A separator nobody passed quotes nothing")
	{
			//The guard that has to be written out, because
			//QString::contains answers true for the empty string:
			//without it every cell of the file would come out
			//quoted. Checked through this function and not only in
			//the rule, because a caller can defeat a guard by
			//passing something else than what it was written for.
		const QString text =
			DrillingTable::toDelimitedText(holes,
						       QString(),
						       plain());
		CHECK_FALSE(text.contains(QLatin1Char('"')));
	}
}

TEST_CASE("T22 — a number written into a file that separates on its decimal "
	  "point stays one cell",
	  "[location][mounting][drilling]")
{
	const QLocale french(QLocale::French);

	DrillingHole hole = DrillingHole::drilled(QPointF(120.5, 65.0),
						  22.0,
						  QStringLiteral("presse-étoupe"));
	hole.component = QStringLiteral("-X1");

	const QString text = DrillingTable::toDelimitedText(QList<DrillingHole>{hole},
							    QStringLiteral(","),
							    french);
	const QList<QStringList> rows = parseTable(text, QStringLiteral(","));

		//A French locale writes 120,5 and a comma separated file cuts
		//on commas. The pair is not refused, because refusing it would
		//be this program deciding what a person may export - it is
		//quoted, by the same rule that quotes the destination.
	REQUIRE(rows.size() == 3);
	REQUIRE(rows.at(2).size() == DrillingTable::columnCount());
	CHECK(rows.at(2).at(1) == QStringLiteral("120,5"));
	CHECK(rows.at(2).at(2) == QStringLiteral("65"));
}

TEST_CASE("T22 — the coordinate in the table is the one the mounting frame "
	  "declares",
	  "[location][mounting][drilling]")
{
		//The surface starts somewhere in whatever the caller draws in;
		//the table never prints that, it prints the distance from the
		//top left corner of the surface.
	const QPointF surface_origin(1000.0, 500.0);
	const MountingArea surface(600.0, 800.0);

	SECTION("A hole 30 mm below the top edge reads 30, and none of the "
		"three plausible wrong answers")
	{
		const QPointF drawn(1000.0, 530.0);
		const QPointF coordinate =
			MountingMeasure::coordinateOf(drawn, surface_origin);

		const DrillingHole hole = DrillingHole::drilled(coordinate, 22.0);
		const QStringList cells = DrillingTable::row(hole, plain());

		REQUIRE(cells.size() == DrillingTable::columnCount());
		CHECK(cells.at(2) == QStringLiteral("30"));

			//Named one by one so that none of them can be reached
			//by accident: the sign flipped, the frame read y-up as
			//technical drawing habitually is, and the origin of the
			//surface never subtracted at all.
		CHECK(cells.at(2) != QStringLiteral("-30"));
		CHECK(cells.at(2) != QStringLiteral("770"));
		CHECK(cells.at(2) != QStringLiteral("530"));

			//and the y-up answer is the one worth spelling out,
			//because it is the one a reader of the DXF writer of
			//this program would think right
		CHECK(surface.height - 30.0 == Approx(770.0));
	}

	SECTION("A hole above the top edge keeps its sign instead of being hidden")
	{
		const QPointF drawn(1000.0, 480.0);
		const DrillingHole hole = DrillingHole::drilled(
			MountingMeasure::coordinateOf(drawn, surface_origin), 22.0);
		const QStringList cells = DrillingTable::row(hole, plain());

		CHECK(cells.at(2) == QStringLiteral("-20"));
	}

	SECTION("The extents are centred on the coordinate, and the halving is "
		"done once")
	{
		const DrillingHole hole = DrillingHole::drilled(QPointF(100.0, 50.0),
								22.0);
		const QRectF extents = hole.boundingRect();

		CHECK(extents.left() == Approx(89.0));
		CHECK(extents.top() == Approx(39.0));
		CHECK(extents.width() == Approx(22.0));
		CHECK(extents.height() == Approx(22.0));
		CHECK(extents.center().x() == Approx(100.0));
		CHECK(extents.center().y() == Approx(50.0));
	}
}

TEST_CASE("T22 — every hole handed over is counted by exactly one tool",
	  "[location][mounting][drilling]")
{
	QList<DrillingHole> holes;
	holes << DrillingHole::drilled(QPointF(10.0, 10.0), 22.0);
	holes << DrillingHole::drilled(QPointF(50.0, 10.0), 22.0);
	holes << DrillingHole::drilled(QPointF(90.0, 10.0), 22.0);
	holes << DrillingHole::drilled(QPointF(10.0, 60.0), 6.5);
	holes << DrillingHole::cutOut(QPointF(200.0, 100.0), QSizeF(92.0, 92.0));
	holes << DrillingHole::cutOut(QPointF(320.0, 100.0), QSizeF(92.0, 92.0));

		//Two nobody has measured, of two different shapes. They are the
		//reason the sum is checked and not only the number of lines: a
		//hole quietly dropped between two groups leaves the number of
		//groups exactly as it was.
	holes << DrillingHole::drilled(QPointF(10.0, 120.0), 0.0,
				       QStringLiteral("à définir"));
	holes << DrillingHole::cutOut(QPointF(400.0, 120.0), QSizeF(),
				      QStringLiteral("à définir"));

	const QList<DrillingToolTotal> totals = DrillingTable::toolTotals(holes);

	SECTION("Three tools and two unmeasured groups make five lines")
	{
			//Ø22, Ø6.5, 92x92, round unmeasured, rectangular
			//unmeasured
		CHECK(totals.size() == 5);
	}

	SECTION("And the counts add up to every hole handed over")
	{
		int counted = 0;
		for (const DrillingToolTotal &total : totals) {
			counted += total.count;
		}
			//The assertion the count of groups cannot make. Losing
			//one hole out of eight changes this number and changes
			//nothing above it.
		CHECK(counted == holes.size());
		CHECK(counted == 8);
	}

	SECTION("The biggest group is the three of one diameter")
	{
		int biggest = 0;
		QString biggest_key;
		for (const DrillingToolTotal &total : totals)
		{
			if (total.count > biggest)
			{
				biggest = total.count;
				biggest_key = total.key;
			}
		}
		CHECK(biggest == 3);
		CHECK(biggest_key == QStringLiteral("d22"));
	}

	SECTION("What nobody measured is reported as such and not as zero")
	{
		int unmeasured_groups = 0;
		int unmeasured_holes = 0;
		for (const DrillingToolTotal &total : totals)
		{
			if (!total.measured)
			{
				++ unmeasured_groups;
				unmeasured_holes += total.count;
				CHECK(total.sizeText(plain())
				      == QStringLiteral("non mesuré"));
			}
		}
		CHECK(unmeasured_groups == 2);
		CHECK(unmeasured_holes == 2);
	}

	SECTION("The answer does not depend on the order the holes came in")
	{
		QList<DrillingHole> shuffled;
		for (int i = static_cast<int>(holes.size()) - 1 ; i >= 0 ; -- i) {
			shuffled << holes.at(i);
		}

		const QList<DrillingToolTotal> other =
			DrillingTable::toolTotals(shuffled);

		REQUIRE(other.size() == totals.size());
		for (int i = 0 ; i < other.size() ; ++ i)
		{
			CHECK(other.at(i).key == totals.at(i).key);
			CHECK(other.at(i).count == totals.at(i).count);
		}
	}
}

TEST_CASE("T22 — a hole nobody has measured stays in the table and says so",
	  "[location][mounting][drilling]")
{
	SECTION("A round hole with no diameter")
	{
		const DrillingHole hole = DrillingHole::drilled(QPointF(10.0, 10.0),
								0.0,
								QStringLiteral("à définir"));
		CHECK_FALSE(hole.hasSize());
		CHECK_FALSE(hole.isNull());
		CHECK(hole.sizeText(plain()) == QStringLiteral("non mesuré"));

			//Folded to zero and not to a guess, which is the answer
			//MountedItem gives for the same reason: an item checked
			//as a point stays in the list and gets reported, while
			//an item given an invented size passes and fails on the
			//bench.
		CHECK(hole.size().width() == Approx(0.0));
		CHECK(hole.size().height() == Approx(0.0));
	}

	SECTION("A cut-out with only one of its two numbers")
	{
		const DrillingHole hole = DrillingHole::cutOut(QPointF(10.0, 10.0),
							       QSizeF(92.0, 0.0));
			//Half of a cut-out is a record somebody started, not
			//half an answer. Completing it would mean inventing the
			//missing number.
		CHECK_FALSE(hole.hasSize());
		CHECK(hole.sizeText(plain()) == QStringLiteral("non mesuré"));
	}

	SECTION("It is still a row of the file, with its destination")
	{
		const DrillingHole hole = DrillingHole::drilled(QPointF(10.0, 10.0),
								0.0,
								QStringLiteral("à définir"));
		const QString text =
			DrillingTable::toDelimitedText(QList<DrillingHole>{hole},
						       QStringLiteral(";"),
						       plain());
		const QList<QStringList> rows = parseTable(text, QStringLiteral(";"));

		REQUIRE(rows.size() == 3);
		CHECK(rows.at(2).last() == QStringLiteral("à définir"));
	}

	SECTION("A record with nothing in it is null, and a hole at the corner "
		"of the plate is not")
	{
		CHECK(DrillingHole().isNull());

		const DrillingHole corner = DrillingHole::drilled(QPointF(0.0, 0.0),
								  6.5);
		CHECK_FALSE(corner.isNull());
	}

	SECTION("A hole belonging to nothing names nothing, and never an empty "
		"cell")
	{
		const DrillingHole hole = DrillingHole::drilled(QPointF(1.0, 1.0),
								6.5);
		CHECK_FALSE(hole.designation().isEmpty());

		DrillingHole known = hole;
		known.component_uuid = QStringLiteral("{4b3c}");
		CHECK(known.designation() == QStringLiteral("{4b3c}"));

		known.component = QStringLiteral("-Q1");
		CHECK(known.designation() == QStringLiteral("-Q1"));
	}
}

TEST_CASE("T22 — a hole is judged with its size, which a coordinate could not be",
	  "[location][mounting][drilling]")
{
	const MountingArea door(600.0, 800.0);

	SECTION("A hole well inside fits")
	{
		const DrillingHole hole = DrillingHole::drilled(QPointF(300.0, 400.0),
								22.0);
		CHECK(DrillingTable::fitOf(hole, door) == MountingFit::Fits);
	}

	SECTION("A hole whose centre is on the surface but whose edge is not "
		"does not fit")
	{
			//The one a check on the centre alone would let through,
			//and the reason this function exists beside
			//MountingMeasure::fitOfCoordinate.
		const DrillingHole hole = DrillingHole::drilled(QPointF(595.0, 400.0),
								22.0);
		CHECK(MountingMeasure::fitOfCoordinate(hole.position, door)
		      == MountingFit::Fits);
		CHECK(DrillingTable::fitOf(hole, door) == MountingFit::OutsideArea);
	}

	SECTION("A cut-out wider than the door needs another door, and says so")
	{
		const DrillingHole hole = DrillingHole::cutOut(QPointF(300.0, 400.0),
							       QSizeF(700.0, 200.0));
			//LargerThanArea is the answer a coordinate can never
			//produce - it has no size to be too large - and it is
			//the one that separates dragging from buying.
		CHECK(DrillingTable::fitOf(hole, door) == MountingFit::LargerThanArea);
	}

	SECTION("A surface nobody has measured refuses rather than guessing")
	{
		const DrillingHole hole = DrillingHole::drilled(QPointF(300.0, 400.0),
								22.0);
		CHECK(DrillingTable::fitOf(hole, MountingArea())
		      == MountingFit::NoArea);
	}

	SECTION("A hole nobody has measured is judged on its centre, which is "
		"all that can honestly be said")
	{
		const DrillingHole inside = DrillingHole::drilled(QPointF(300.0, 400.0),
								  0.0);
		CHECK(DrillingTable::fitOf(inside, door) == MountingFit::Fits);

		const DrillingHole outside = DrillingHole::drilled(QPointF(900.0, 400.0),
								   0.0);
		CHECK(DrillingTable::fitOf(outside, door) == MountingFit::OutsideArea);
	}
}

TEST_CASE("T22 — a millimetre is written the way the material list writes one",
	  "[location][mounting][drilling]")
{
	SECTION("A whole number keeps no decimals")
	{
		const DrillingHole hole = DrillingHole::drilled(QPointF(0.0, 0.0), 22.0);
		CHECK(hole.sizeText(plain()) == QString::fromUtf8("Ø 22"));
	}

	SECTION("A fraction keeps the decimals it has, and no more")
	{
		const DrillingHole hole = DrillingHole::drilled(QPointF(0.0, 0.0), 6.5);
		CHECK(hole.sizeText(plain()) == QString::fromUtf8("Ø 6.5"));
	}

	SECTION("A cut-out says both of its numbers")
	{
		const DrillingHole hole = DrillingHole::cutOut(QPointF(0.0, 0.0),
							       QSizeF(92.0, 92.0));
		CHECK(hole.sizeText(plain()) == QStringLiteral("92 x 92"));
	}

	SECTION("The decimal separator is the locale's, as everywhere else")
	{
		const DrillingHole hole = DrillingHole::drilled(QPointF(0.0, 0.0), 6.5);
		CHECK(hole.sizeText(QLocale(QLocale::French))
		      == QString::fromUtf8("Ø 6,5"));
	}
}
