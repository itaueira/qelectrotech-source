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
#include "../../../sources/location/locationcontainment.h"
#include "qt_catch_tostring.h"

#include <QHash>
#include <QSizeF>
#include <QString>

namespace
{
		/// @return a rectangle drawn on the folio, standing for a location
	LocationArea drawn(qreal x, qreal y, qreal w, qreal h,
			   const char *path, qreal z = 0)
	{
		LocationArea a;
		a.rect = QRectF(x, y, w, h);
		a.path = QString::fromUtf8(path);
		a.z = z;
		return a;
	}

		/// @return a component of the given size, centred where asked
	LocatableItem sized(int id, qreal cx, qreal cy, qreal w, qreal h,
			    const char *path = "")
	{
		LocatableItem i;
		i.id = id;
		i.rect = QRectF(cx - w / 2, cy - h / 2, w, h);
		i.path = QString::fromUtf8(path);
		return i;
	}

		/// @return a component the size a push button is drawn at
	LocatableItem at(int id, qreal cx, qreal cy)
	{
		return sized(id, cx, cy, 10, 10);
	}

		/// @return a component that already carries a path
	LocatableItem carrying(int id, qreal cx, qreal cy, const char *path)
	{
		return sized(id, cx, cy, 10, 10, path);
	}

		/// @brief Move a component, keeping its size and its path
	void moveTo(LocatableItem &item, qreal cx, qreal cy)
	{
		const QSizeF size = item.rect.size();
		item.rect = QRectF(cx - size.width() / 2,
				   cy - size.height() / 2,
				   size.width(), size.height());
	}

		/**
			@brief Write the answers onto the components, the way the undo
			command does once it is pushed.
			@return how many components the rule had something to say about
		*/
	int applyTo(QVector<LocatableItem> &items,
		    const QVector<LocationContainmentChange> &changes)
	{
		QHash<int, QString> by_id;
		for (const auto &change : changes) {
			by_id.insert(change.id, change.path);
		}
		for (auto &item : items) {
			if (by_id.contains(item.id)) {
				item.path = by_id.value(item.id);
			}
		}
		return changes.count();
	}

		/// @return how many components read as mounted at that path
	int countAt(const QVector<LocatableItem> &items, const char *path)
	{
		const QString wanted = QString::fromUtf8(path);
		int total = 0;
		for (const auto &item : items) {
			if (item.path == wanted) {
				++total;
			}
		}
		return total;
	}
}

TEST_CASE("CU-32.2 - six push buttons, one dragged out, one dragged in",
	  "[location][containment]")
{
	//The use case, played out on data. The rectangle is drawn round six
	//buttons; one leaves; another comes back in. Nobody assigns anything by
	//hand at any point, which is the whole claim being tested here.
	const QVector<LocationArea> areas{
		drawn(0, 0, 100, 100, "Painel Remoto")
	};

	QVector<LocatableItem> buttons{
		at(1, 10, 10), at(2, 25, 25), at(3, 40, 40),
		at(4, 55, 55), at(5, 70, 70), at(6, 85, 85)
	};

	//The rectangle has just been drawn: everything under it is claimed.
	applyTo(buttons, locationContainmentChanges(areas, buttons));
	REQUIRE(countAt(buttons, "Painel Remoto") == 6);

	SECTION("the count goes six, five, six")
	{
		//Drag the third one out.
		moveTo(buttons[2], 300, 300);
		applyTo(buttons, locationContainmentChanges(areas, buttons));
		CHECK(countAt(buttons, "Painel Remoto") == 5);

		//And bring it back in, at a spot it never occupied before.
		moveTo(buttons[2], 60, 20);
		applyTo(buttons, locationContainmentChanges(areas, buttons));
		CHECK(countAt(buttons, "Painel Remoto") == 6);
	}

	SECTION("the component that left carries nothing, not the old path")
	{
		moveTo(buttons[2], 300, 300);
		applyTo(buttons, locationContainmentChanges(areas, buttons));

		//A different defect from a wrong count, and the screen test asks
		//for the two to be told apart, so it is pinned separately.
		CHECK(buttons.at(2).path.isEmpty());
	}

	SECTION("only the component that moved is touched")
	{
		moveTo(buttons[2], 300, 300);
		const auto changes = locationContainmentChanges(areas, buttons);

		//One entry, not six. It is what lets the undo caption say what it
		//actually did, and what stops a drag rewriting the whole folio.
		REQUIRE(changes.count() == 1);
		CHECK(changes.first().id == 3);
		CHECK(changes.first().path.isEmpty());
	}

	SECTION("moving something that did not change location changes nothing")
	{
		//Inside, and still inside afterwards.
		moveTo(buttons[0], 90, 15);
		const auto changes = locationContainmentChanges(areas, buttons);

		//Empty, so the caller pushes no command at all. A rule that
		//answered here would drop an empty step into the undo stack on
		//every single drag.
		CHECK(changes.isEmpty());
	}
}

TEST_CASE("inside means the centre is inside", "[location][containment]")
{
	const QVector<LocationArea> areas{
		drawn(0, 0, 100, 100, "QCM1")
	};

	SECTION("a hair inside is inside, a hair outside is outside")
	{
		CHECK(locationPathAt(areas, QRectF(94, 45, 10, 10)) == "QCM1");
		CHECK(locationPathAt(areas, QRectF(96, 45, 10, 10)).isEmpty());
	}

	SECTION("a centre exactly on the border is inside")
	{
		//A stated tie rather than an accident: the boundary belongs to
		//the rectangle that drew it.
		CHECK(locationPathAt(areas, QRectF(95, 45, 10, 10)) == "QCM1");
	}

	SECTION("a component hanging over the edge is still mounted there")
	{
		//Where a terminal block goes. Full containment would answer
		//nothing here, and the enclosure would list none of its
		//terminals.
		CHECK(locationPathAt(areas, QRectF(90, 45, 30, 10)) == "QCM1");
	}

	SECTION("a component larger than the enclosure is mounted in it")
	{
		//Full containment cannot answer this case at all.
		const QRectF big(-50, -50, 200, 200);
		REQUIRE_FALSE(areas.first().rect.contains(big));
		CHECK(locationPathAt(areas, big) == "QCM1");
	}

	SECTION("outside everything is no answer")
	{
		CHECK(locationPathAt(areas, QRectF(500, 500, 10, 10)).isEmpty());
	}
}

TEST_CASE("the smallest area containing the centre wins",
	  "[location][containment]")
{
	SECTION("a cabinet drawn inside a room is the more specific answer")
	{
		const QVector<LocationArea> areas{
			drawn(0, 0, 400, 400, "SALA"),
			drawn(100, 100, 100, 100, "SALA/QCM1")
		};
		CHECK(locationPathAt(areas, QRectF(145, 145, 10, 10))
		      == "SALA/QCM1");

		//And a component in the room but outside the cabinet stays in
		//the room.
		CHECK(locationPathAt(areas, QRectF(300, 300, 10, 10)) == "SALA");
	}

	SECTION("the order the areas arrive in does not decide")
	{
		//The same two rectangles, listed the other way round. A rule
		//that took the first match would answer SALA here.
		const QVector<LocationArea> areas{
			drawn(100, 100, 100, 100, "SALA/QCM1"),
			drawn(0, 0, 400, 400, "SALA")
		};
		CHECK(locationPathAt(areas, QRectF(145, 145, 10, 10))
		      == "SALA/QCM1");
	}

	SECTION("three deep, and the innermost answers")
	{
		const QVector<LocationArea> areas{
			drawn(0, 0, 400, 400, "SALA"),
			drawn(50, 50, 200, 200, "SALA/QCM1"),
			drawn(80, 80, 40, 40, "SALA/QCM1/PLACA")
		};
		CHECK(locationPathAt(areas, QRectF(95, 95, 10, 10))
		      == "SALA/QCM1/PLACA");
	}

	SECTION("two areas of the same size: the one on top wins")
	{
		//Two enclosures drawn over each other by mistake, or a copy
		//that landed on its original. What the person sees is the top
		//one, so that is the answer given.
		const QVector<LocationArea> areas{
			drawn(0, 0, 100, 100, "QCM1", 1),
			drawn(0, 0, 100, 100, "QCM2", 2)
		};
		CHECK(locationPathAt(areas, QRectF(45, 45, 10, 10)) == "QCM2");
	}

	SECTION("the tie is broken by z and not by order")
	{
		const QVector<LocationArea> areas{
			drawn(0, 0, 100, 100, "QCM2", 2),
			drawn(0, 0, 100, 100, "QCM1", 1)
		};
		CHECK(locationPathAt(areas, QRectF(45, 45, 10, 10)) == "QCM2");
	}
}

TEST_CASE("a hand assignment is only retracted by the drawing that owns it",
	  "[location][containment]")
{
	SECTION("nobody drew anything, so nothing is taken away")
	{
		//A whole project assigned through the location manager, on
		//folios without a single rectangle. The rule must stay silent
		//here for ever, or step five destroys step three's work.
		const QVector<LocationArea> areas;
		QVector<LocatableItem> items{
			carrying(1, 10, 10, "QCM1"),
			carrying(2, 20, 20, "QCM1/PORTA")
		};
		CHECK(locationContainmentChanges(areas, items).isEmpty());
	}

	SECTION("an area exists elsewhere, and this path is not its")
	{
		//The folio has a rectangle for PORTA. A component assigned to
		//QCM1 by hand, outside it, is none of that rectangle's affair.
		const QVector<LocationArea> areas{
			drawn(0, 0, 100, 100, "PORTA")
		};
		QVector<LocatableItem> items{
			carrying(1, 500, 500, "QCM1")
		};
		CHECK(locationContainmentChanges(areas, items).isEmpty());
	}

	SECTION("the path is an area's, and the component is not in it")
	{
		//This is the retraction, and the only one there is.
		const QVector<LocationArea> areas{
			drawn(0, 0, 100, 100, "PORTA")
		};
		QVector<LocatableItem> items{
			carrying(1, 500, 500, "PORTA")
		};
		const auto changes = locationContainmentChanges(areas, items);
		REQUIRE(changes.count() == 1);
		CHECK(changes.first().path.isEmpty());
	}

	SECTION("the documented cost: the drawing beats the hand")
	{
		//Draw an area for QCM1, assign a component to QCM1 by hand
		//somewhere else, and the area wins. Written down in the header
		//as a behaviour, and pinned here so it cannot drift by
		//accident.
		const QVector<LocationArea> areas{
			drawn(0, 0, 100, 100, "QCM1")
		};
		QVector<LocatableItem> items{
			carrying(1, 900, 900, "QCM1")
		};
		const auto changes = locationContainmentChanges(areas, items);
		REQUIRE(changes.count() == 1);
		CHECK(changes.first().path.isEmpty());
	}

	SECTION("an area takes over a component assigned elsewhere by hand")
	{
		//Inside the QCM1 rectangle, but carrying QCM2 from the manager.
		//The drawing is the more recent statement of where it sits.
		const QVector<LocationArea> areas{
			drawn(0, 0, 100, 100, "QCM1")
		};
		QVector<LocatableItem> items{
			carrying(1, 50, 50, "QCM2")
		};
		const auto changes = locationContainmentChanges(areas, items);
		REQUIRE(changes.count() == 1);
		CHECK(changes.first().path == "QCM1");
	}
}

TEST_CASE("rectangles that are not rectangles yet", "[location][containment]")
{
	SECTION("a rectangle dragged right to left holds what it covers")
	{
		//Built from two handle points, so the width is negative. The
		//rule straightens it out instead of answering nothing.
		LocationArea a;
		a.rect = QRectF(QPointF(100, 100), QPointF(0, 0));
		a.path = QStringLiteral("QCM1");
		REQUIRE_FALSE(a.rect.isValid());

		CHECK(locationPathAt(QVector<LocationArea>{a},
				     QRectF(45, 45, 10, 10)) == "QCM1");
	}

	SECTION("a rectangle with no surface holds nothing")
	{
		//The state an area is in between the first click and the first
		//movement of the mouse. It must not swallow whatever happens to
		//sit under that click.
		const QVector<LocationArea> areas{
			drawn(50, 50, 0, 0, "QCM1")
		};
		CHECK(locationPathAt(areas, QRectF(45, 45, 10, 10)).isEmpty());
	}

	SECTION("an area with no location assigned takes part in nothing")
	{
		//A rectangle somebody drew and has not named. It cannot answer
		//the question, and it must not hide the enclosure it sits in.
		const QVector<LocationArea> areas{
			drawn(0, 0, 400, 400, "QCM1"),
			drawn(100, 100, 50, 50, "")
		};
		CHECK(locationPathAt(areas, QRectF(120, 120, 10, 10)) == "QCM1");
	}

	SECTION("an unnamed area retracts nothing either")
	{
		const QVector<LocationArea> areas{
			drawn(0, 0, 100, 100, "")
		};
		QVector<LocatableItem> items{
			carrying(1, 500, 500, "QCM1")
		};
		CHECK(locationContainmentChanges(areas, items).isEmpty());
	}

	SECTION("no areas at all, and nothing carried")
	{
		QVector<LocatableItem> items{at(1, 10, 10), at(2, 20, 20)};
		CHECK(locationContainmentChanges(QVector<LocationArea>(), items)
		      .isEmpty());
	}
}

TEST_CASE("the answer reads the same way twice", "[location][containment]")
{
	//A caption built from the result names components in a fixed order, so
	//the same drag must not produce a differently ordered list.
	const QVector<LocationArea> areas{
		drawn(0, 0, 100, 100, "QCM1")
	};
	QVector<LocatableItem> items{
		at(7, 10, 10), at(3, 20, 20), at(5, 30, 30)
	};

	const auto changes = locationContainmentChanges(areas, items);
	REQUIRE(changes.count() == 3);
	CHECK(changes.at(0).id == 7);
	CHECK(changes.at(1).id == 3);
	CHECK(changes.at(2).id == 5);
}
