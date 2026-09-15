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
#include "../../../sources/location/drillingorigin.h"
#include "../../../sources/location/drillingtable.h"
#include "../../../sources/location/mountinglayout.h"
#include "qt_catch_tostring.h"

#include <QDomDocument>
#include <QDomElement>
#include <QLocale>
#include <QPointF>
#include <QSet>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <cmath>

/*
	Which corner of the plate the workshop measures from.

	The defect these cases exist against is the one that cannot be found by
	looking at a hole. Read a plate from the wrong corner and every
	coordinate on it is plausible, every coordinate agrees with every other,
	the table adds up, the cotes close - and the panel comes out a mirror of
	the one that was drawn. There is no number in the document that looks
	wrong, which is why the check has to be made here rather than on a
	bench.

	So three things are nailed and they are of different kinds.

	The first is the arithmetic. The same hole is measured from all four
	corners and the four answers are written out; then the answers that
	could have come from a sign slipping, from an axis being flipped
	against the wrong dimension or from the flip never happening are named
	one by one, so that none of them can be arrived at by accident. Over the
	top of that goes the invariant that does not depend on any of the four
	numbers being typed right: a coordinate plus the distance to the
	opposite edge is the plate.

	The second is that the choice cannot silently become a different one. A
	project written before there was a choice opens measured from the corner
	it was drawn against; a file naming a corner this version does not know
	opens rather than being refused; and the default is not written down at
	all, so a project that never went near the setting comes back out as the
	file it went in as.

	The third is the sentence at the head of the table. It is the only
	thing on the sheet that says which of the four was used, so a sentence
	that named the corner and went on quoting the axes of a different one
	would be worse than no sentence. That is checked by taking the corner
	name out of two sentences and requiring that what is left still differs.
*/

namespace
{
		/// No locale in the way of the numbers being read as written.
	QLocale plain()
	{
		return QLocale::c();
	}

		/// A mounting plate of a size a catalogue really sells.
	MountingArea plate()
	{
		return MountingArea(600.0, 800.0);
	}

		/// One hole, somewhere with no symmetry to hide a swap.
	QPointF holePosition()
	{
		return QPointF(120.0, 65.0);
	}

	DrillingFrame frameAt(DrillingCorner corner,
			      const QPointF &offset = QPointF())
	{
		return DrillingFrame(DrillingOrigin(corner, offset), plate());
	}

		/// The surface element as a file holds it, attributes and all.
	QDomElement writtenFace(QDomDocument &document,
				const DrillingOrigin &origin)
	{
		MountingSurface face(QStringLiteral("QCM1"),
				     QStringLiteral("plate"),
				     plate());
		face.uuid = QStringLiteral("{face-1}");
		face.name = QStringLiteral("Platine");
		face.drilling_origin = origin;

		QDomElement element = face.toXml(document);
		document.appendChild(element);
		return element;
	}
}

TEST_CASE("T22 — the same hole read from the four corners gives four numbers, "
	  "and none of the plausible wrong ones",
	  "[location][mounting][drilling][origin]")
{
	const QPointF hole = holePosition();

	const QPointF top_left =
		frameAt(DrillingCorner::TopLeft).coordinateOf(hole);
	const QPointF top_right =
		frameAt(DrillingCorner::TopRight).coordinateOf(hole);
	const QPointF bottom_left =
		frameAt(DrillingCorner::BottomLeft).coordinateOf(hole);
	const QPointF bottom_right =
		frameAt(DrillingCorner::BottomRight).coordinateOf(hole);

	SECTION("Each corner answers the distance from itself")
	{
		CHECK(top_left.x() == Approx(120.0));
		CHECK(top_left.y() == Approx(65.0));

		CHECK(top_right.x() == Approx(480.0));
		CHECK(top_right.y() == Approx(65.0));

		CHECK(bottom_left.x() == Approx(120.0));
		CHECK(bottom_left.y() == Approx(735.0));

		CHECK(bottom_right.x() == Approx(480.0));
		CHECK(bottom_right.y() == Approx(735.0));
	}

	SECTION("The four wrong answers are named so none can be reached by "
		"accident")
	{
			//1. the sign turned over at the corner instead of the
			//   axis being measured from the far edge
		CHECK_FALSE(top_right.x() == Approx(-120.0));
		CHECK_FALSE(bottom_left.y() == Approx(-65.0));

			//2. the flip never happening at all, which is what a
			//   corner read but not applied looks like
		CHECK_FALSE(top_right.x() == Approx(120.0));
		CHECK_FALSE(bottom_left.y() == Approx(65.0));

			//3. the two dimensions of the plate swapped: the height
			//   subtracted from x, the width from y. It is the one
			//   wrong answer that is still a positive number of the
			//   right order of magnitude, and on a square plate it
			//   would be invisible - which is why this plate is not
			//   square.
		CHECK_FALSE(top_right.x() == Approx(800.0 - 120.0));
		CHECK_FALSE(bottom_left.y() == Approx(600.0 - 65.0));

			//4. both axes flipped when only one was asked for
		CHECK_FALSE(top_right.y() == Approx(735.0));
		CHECK_FALSE(bottom_left.x() == Approx(480.0));
	}

	SECTION("A coordinate and the distance to the opposite edge are the "
		"plate")
	{
			//The check that does not depend on any of the eight
			//numbers above having been typed right: whatever the
			//hole is, what one corner reads plus what the corner
			//across from it reads is the dimension of the plate.
		CHECK(top_left.x() + top_right.x() == Approx(plate().width));
		CHECK(top_left.y() + bottom_left.y() == Approx(plate().height));

		CHECK(bottom_right.x() + bottom_left.x() == Approx(plate().width));
		CHECK(bottom_right.y() + top_right.y() == Approx(plate().height));
	}

	SECTION("A hole on the plate is a positive pair from every corner")
	{
			//Because every axis grows into the plate. A corner that
			//answered a negative number would be a corner nobody can
			//hook a tape over.
		CHECK(top_left.x() >= 0.0);
		CHECK(top_left.y() >= 0.0);
		CHECK(top_right.x() >= 0.0);
		CHECK(top_right.y() >= 0.0);
		CHECK(bottom_left.x() >= 0.0);
		CHECK(bottom_left.y() >= 0.0);
		CHECK(bottom_right.x() >= 0.0);
		CHECK(bottom_right.y() >= 0.0);
	}
}

TEST_CASE("T22 — a project that never chose an origin measures from the top "
	  "left corner",
	  "[location][mounting][drilling][origin]")
{
	const DrillingOrigin untouched;

	SECTION("The default is the corner the model frame already starts at")
	{
		CHECK(untouched.corner == DrillingCorner::TopLeft);
		CHECK(untouched.isDefault());
		CHECK_FALSE(untouched.hasOffset());
		CHECK(untouched.xGrowsRight());
		CHECK(untouched.yGrowsDown());
	}

	SECTION("Measuring from it changes nothing, and needs no plate")
	{
			//The state of every panel being laid out: dimensions
			//nobody has typed yet. The default origin has to go on
			//answering through it, or the table would go blank the
			//moment somebody opened a fresh face.
		const DrillingFrame unmeasured;
		CHECK_FALSE(unmeasured.origin.needsSurfaceSize());
		CHECK(unmeasured.canMeasure());

		const QPointF hole = holePosition();
		CHECK(unmeasured.coordinateOf(hole).x() == Approx(hole.x()));
		CHECK(unmeasured.coordinateOf(hole).y() == Approx(hole.y()));
	}

	SECTION("The table prints what it printed before the corner was a "
		"choice")
	{
		const DrillingHole hole =
			DrillingHole::drilled(holePosition(), 22.0);

		const QStringList before = DrillingTable::row(hole, plain());
		const QStringList after =
			DrillingTable::row(hole, plain(), DrillingFrame());

		CHECK(before == after);
		REQUIRE(before.size() == DrillingTable::columnCount());
		CHECK(before.at(1) == QStringLiteral("120"));
		CHECK(before.at(2) == QStringLiteral("65"));
	}
}

TEST_CASE("T22 — the zero moves off the corner, and it moves into the plate "
	  "from whichever corner was picked",
	  "[location][mounting][drilling][origin]")
{
		//A datum: a fixing hole 35 mm in from one edge and 40 mm in
		//from the other, which is how a shop that marks out by hand
		//works.
	const QPointF datum(35.0, 40.0);
	const QPointF hole = holePosition();

	SECTION("Every corner takes the shift off after it has measured")
	{
		const QPointF from_top_left =
			frameAt(DrillingCorner::TopLeft, datum).coordinateOf(hole);
		CHECK(from_top_left.x() == Approx(85.0));
		CHECK(from_top_left.y() == Approx(25.0));

		const QPointF from_top_right =
			frameAt(DrillingCorner::TopRight, datum).coordinateOf(hole);
		CHECK(from_top_right.x() == Approx(445.0));
		CHECK(from_top_right.y() == Approx(25.0));

		const QPointF from_bottom_right =
			frameAt(DrillingCorner::BottomRight, datum).coordinateOf(hole);
		CHECK(from_bottom_right.x() == Approx(445.0));
		CHECK(from_bottom_right.y() == Approx(695.0));

			//The shift applied before the flip instead of after it,
			//which is the error that comes out right for the top
			//left corner and wrong for the other three - the worst
			//way for a defect to behave, because the case anybody
			//tries first is the one that works.
		CHECK_FALSE(from_top_right.x() == Approx(600.0 - 85.0));
	}

	SECTION("The datum itself reads zero, wherever the corner is")
	{
		const QList<DrillingCorner> all = DrillingOrigin::corners();
		for (DrillingCorner corner : all)
		{
			const DrillingFrame frame = frameAt(corner, datum);
			const QPointF at_datum = frame.positionOf(QPointF(0.0, 0.0));

			CHECK(frame.coordinateOf(at_datum).x() == Approx(0.0));
			CHECK(frame.coordinateOf(at_datum).y() == Approx(0.0));
		}
	}

	SECTION("A datum measured from the bottom right is near the bottom "
		"right")
	{
			//Stated as a position on the plate, because that is the
			//claim a person would check by looking at the drawing:
			//35 mm in from the RIGHT edge of a 600 mm plate is
			//x = 565, and not x = 35.
		const QPointF at_datum =
			frameAt(DrillingCorner::BottomRight, datum)
			.positionOf(QPointF(0.0, 0.0));

		CHECK(at_datum.x() == Approx(565.0));
		CHECK(at_datum.y() == Approx(760.0));

			//the shift read in the model frame instead of along the
			//axes this origin declares
		CHECK_FALSE(at_datum.x() == Approx(35.0));
		CHECK_FALSE(at_datum.y() == Approx(40.0));
	}
}

TEST_CASE("T22 — the way back from a coordinate is the way out, from every "
	  "corner",
	  "[location][mounting][drilling][origin]")
{
		//Two points with nothing round about them, so a rounding that
		//only shows on halves shows here.
	const QPointF first(13.5, 456.25);
	const QPointF second(599.75, 0.125);

	const QList<DrillingCorner> all = DrillingOrigin::corners();
	for (DrillingCorner corner : all)
	{
		const QPointF shifts[2] = { QPointF(), QPointF(35.0, 40.0) };
		for (const QPointF &shift : shifts)
		{
			const DrillingFrame frame = frameAt(corner, shift);

			const QPointF back_first =
				frame.positionOf(frame.coordinateOf(first));
			CHECK(back_first.x() == Approx(first.x()));
			CHECK(back_first.y() == Approx(first.y()));

			const QPointF back_second =
				frame.positionOf(frame.coordinateOf(second));
			CHECK(back_second.x() == Approx(second.x()));
			CHECK(back_second.y() == Approx(second.y()));
		}
	}
}

TEST_CASE("T22 — a corner that needs the plate says so instead of guessing",
	  "[location][mounting][drilling][origin]")
{
	const DrillingFrame unmeasured(DrillingOrigin(DrillingCorner::BottomLeft),
				       MountingArea());

	SECTION("It refuses rather than falling back to the top left corner")
	{
		CHECK(unmeasured.origin.needsSurfaceSize());
		CHECK_FALSE(unmeasured.canMeasure());

		const QPointF answer = unmeasured.coordinateOf(holePosition());
		CHECK(std::isnan(answer.x()));
		CHECK(std::isnan(answer.y()));

			//The wrong answer is not a wrong number, it is a right
			//looking number: the model position handed back as if it
			//had been measured from the bottom left corner.
		CHECK_FALSE(answer.y() == Approx(65.0));
	}

	SECTION("The table says the plate is missing and not the hole")
	{
		const DrillingHole hole =
			DrillingHole::drilled(holePosition(), 22.0);
		const QStringList cells =
			DrillingTable::row(hole, plain(), unmeasured);

		REQUIRE(cells.size() == DrillingTable::columnCount());
		CHECK(cells.at(1) == QStringLiteral("?"));
		CHECK(cells.at(2) == QStringLiteral("?"));

			//and the diameter is still there, which is what makes
			//the row readable as "this plate has no dimensions"
			//instead of "this hole was never measured"
		CHECK_FALSE(cells.at(4) == QStringLiteral("?"));
	}

	SECTION("The head of the table carries the warning")
	{
		const QString warned =
			DrillingTable::referenceFrameText(unmeasured, plain());
		const QString quiet =
			DrillingTable::referenceFrameText(
				frameAt(DrillingCorner::BottomLeft), plain());

		CHECK(warned != quiet);
		CHECK(warned.length() > quiet.length());
	}
}

TEST_CASE("T22 — the sentence at the head of the table names the corner that "
	  "was used, and quotes its axes",
	  "[location][mounting][drilling][origin]")
{
	SECTION("A caller with no frame in hand prints the default one")
	{
		CHECK(DrillingTable::referenceFrameText()
		      == DrillingTable::referenceFrameText(DrillingFrame()));
	}

	SECTION("The four corners give four different sentences")
	{
		QSet<QString> sentences;
		const QList<DrillingCorner> all = DrillingOrigin::corners();
		for (DrillingCorner corner : all)
		{
			const QString text = DrillingTable::referenceFrameText(
				frameAt(corner), plain());

				//it has to name the corner by the same words a
				//selector would offer, or the sheet and the
				//dialogue call one corner two things
			CHECK(text.contains(DrillingOrigin::cornerName(corner)));
			sentences.insert(text);
		}
		CHECK(sentences.size() == all.size());
	}

	SECTION("Take the corner name out and the sentences still differ")
	{
			//The defect this guards against is the worst one
			//available here: a sentence that names the right corner
			//and goes on quoting the axes of the default. It reads
			//as correct, it is half correct, and the half that is
			//wrong is the half the workshop measures by.
		QString from_top_left = DrillingTable::referenceFrameText(
			frameAt(DrillingCorner::TopLeft), plain());
		QString from_bottom_right = DrillingTable::referenceFrameText(
			frameAt(DrillingCorner::BottomRight), plain());

		from_top_left.remove(
			DrillingOrigin::cornerName(DrillingCorner::TopLeft));
		from_bottom_right.remove(
			DrillingOrigin::cornerName(DrillingCorner::BottomRight));

		CHECK(from_top_left != from_bottom_right);
	}

	SECTION("A shifted zero is written down, and an unshifted one is not")
	{
		const QString shifted = DrillingTable::referenceFrameText(
			frameAt(DrillingCorner::TopLeft, QPointF(35.0, 40.0)),
			plain());
		const QString square = DrillingTable::referenceFrameText(
			frameAt(DrillingCorner::TopLeft), plain());

		CHECK(shifted != square);
		CHECK(shifted.contains(QStringLiteral("35")));
		CHECK(shifted.contains(QStringLiteral("40")));

			//A shift of nothing is not announced: a line that says
			//"shifted by 0" on every sheet is a line people stop
			//reading, and this one has to be read on the sheets
			//where it is the difference between two holes.
		CHECK_FALSE(square.contains(QStringLiteral("0 mm")));
	}

	SECTION("It still carries a separator of its own")
	{
			//The first line of every exported file, and the one the
			//quoting has to carry before a hole is in it.
		CHECK(DrillingTable::referenceFrameText(
			      frameAt(DrillingCorner::BottomRight), plain())
		      .contains(QLatin1Char(';')));
	}
}

TEST_CASE("T22 — the corner survives the file, and a file that named none "
	  "still opens",
	  "[location][mounting][drilling][origin]")
{
	SECTION("Every corner goes out and comes back as itself")
	{
		const QList<DrillingCorner> all = DrillingOrigin::corners();
		for (DrillingCorner corner : all)
		{
			const QString token = DrillingOrigin::cornerToken(corner);
			bool ok = false;
			CHECK(DrillingOrigin::cornerFromToken(token, &ok) == corner);
			CHECK(ok);
		}
	}

	SECTION("A face carries its corner and its shift through the .qet")
	{
		QDomDocument document;
		const DrillingOrigin chosen(DrillingCorner::BottomLeft,
					    QPointF(35.0, 40.0));
		const QDomElement element = writtenFace(document, chosen);

		MountingSurface read;
		REQUIRE(read.fromXml(element));

		CHECK(read.drilling_origin.corner == DrillingCorner::BottomLeft);
		CHECK(read.drilling_origin.offset.x() == Approx(35.0));
		CHECK(read.drilling_origin.offset.y() == Approx(40.0));
		CHECK(read.drilling_origin == chosen);

			//and the frame it hands out measures from there
		const DrillingFrame frame = read.drillingFrame();
		CHECK(frame.coordinateOf(holePosition()).y() == Approx(735.0 - 40.0));
	}

	SECTION("A project written before the corner was a choice opens "
		"measured from the top left")
	{
			//A face element as an older version wrote it: it says
			//nothing at all about a drilling origin.
		QDomDocument document;
		QDomElement element =
			document.createElement(MountingSurface::tagName());
		element.setAttribute(QStringLiteral("location"),
				     QStringLiteral("QCM1"));
		element.setAttribute(QStringLiteral("kind"),
				     QStringLiteral("plate"));
		element.setAttribute(QStringLiteral("width"),
				     QStringLiteral("600"));
		element.setAttribute(QStringLiteral("height"),
				     QStringLiteral("800"));
		document.appendChild(element);

		MountingSurface read;
		REQUIRE(read.fromXml(element));

		CHECK(read.drilling_origin.isDefault());
		CHECK(read.drillingFrame().canMeasure());
		CHECK(read.drillingFrame().coordinateOf(holePosition()).y()
		      == Approx(65.0));
	}

	SECTION("A corner this version does not know opens as the default")
	{
			//Tolerant reading, the rule of this whole module: a
			//project that will not open is a worse answer than a
			//project that opens with the corner it can be told
			//again in one click.
		QDomDocument document;
		QDomElement element =
			document.createElement(MountingSurface::tagName());
		element.setAttribute(QStringLiteral("location"),
				     QStringLiteral("QCM1"));
		element.setAttribute(QStringLiteral("drilling_origin"),
				     QStringLiteral("centre-of-the-plate"));
		document.appendChild(element);

		MountingSurface read;
		REQUIRE(read.fromXml(element));
		CHECK(read.drilling_origin.corner == DrillingCorner::TopLeft);

		bool ok = true;
		DrillingOrigin::cornerFromToken(
			QStringLiteral("centre-of-the-plate"), &ok);
		CHECK_FALSE(ok);
	}

	SECTION("The default is not written down")
	{
			//So a project saved by somebody who never opened the
			//setting comes back out as the file it went in as,
			//instead of reporting itself as modified.
		QDomDocument document;
		const QDomElement element = writtenFace(document, DrillingOrigin());

		CHECK_FALSE(element.hasAttribute(QStringLiteral("drilling_origin")));
		CHECK_FALSE(element.hasAttribute(QStringLiteral("drilling_origin_x")));
		CHECK_FALSE(element.hasAttribute(QStringLiteral("drilling_origin_y")));
	}

	SECTION("A shift is written as a point, both halves or neither")
	{
			//"35 mm in from the edge, and nothing said about how far
			//down" is not a datum anybody can find on a plate.
		QDomDocument document;
		const QDomElement element =
			writtenFace(document,
				    DrillingOrigin(DrillingCorner::TopLeft,
						   QPointF(35.0, 0.0)));

		CHECK(element.hasAttribute(QStringLiteral("drilling_origin_x")));
		CHECK(element.hasAttribute(QStringLiteral("drilling_origin_y")));

		MountingSurface read;
		REQUIRE(read.fromXml(element));
		CHECK(read.drilling_origin.offset.x() == Approx(35.0));
		CHECK(read.drilling_origin.offset.y() == Approx(0.0));
	}

	SECTION("Two faces of one panel are drilled from two corners")
	{
			//The reason the choice is on the face and not on the
			//project: a plate punched on a machine and a door
			//marked out by hand, in one enclosure.
		MountingSurface machine_plate(QStringLiteral("QCM1"),
					      QStringLiteral("plate"),
					      plate());
		machine_plate.drilling_origin =
			DrillingOrigin(DrillingCorner::BottomLeft);

		MountingSurface hand_marked_door(QStringLiteral("QCM1"),
						 QStringLiteral("door"),
						 plate());

		CHECK(machine_plate.drillingFrame().coordinateOf(holePosition()).y()
		      == Approx(735.0));
		CHECK(hand_marked_door.drillingFrame().coordinateOf(holePosition()).y()
		      == Approx(65.0));
	}
}

TEST_CASE("T22 — the frame is built from the face and never kept beside it",
	  "[location][mounting][drilling][origin]")
{
	MountingSurface face(QStringLiteral("QCM1"),
			     QStringLiteral("plate"),
			     plate());
	face.drilling_origin = DrillingOrigin(DrillingCorner::BottomLeft);

	CHECK(face.drillingFrame().coordinateOf(holePosition()).y()
	      == Approx(735.0));

		//The plate is swapped for a taller one. Every coordinate
		//measured from its bottom edge has just changed, and a frame
		//that had been kept in a member would go on answering about the
		//plate of yesterday - which is the one kind of stale number
		//nobody would think to check.
	face.area = MountingArea(600.0, 1000.0);

	CHECK(face.drillingFrame().coordinateOf(holePosition()).y()
	      == Approx(935.0));
	CHECK_FALSE(face.drillingFrame().coordinateOf(holePosition()).y()
		    == Approx(735.0));
}
