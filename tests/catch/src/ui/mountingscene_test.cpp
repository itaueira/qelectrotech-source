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
#include "../qt_catch_tostring.h"

#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountingscene.h"
#include "../../../../sources/undocommand/movemountedpartcommand.h"

#include <catch2/catch.hpp>

#include <QGraphicsItem>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QTransform>
#include <QUndoStack>

#include <limits>

/*
	The scene a mounting surface is laid out on: what it counts in, what it
	refuses to make an item of, and what a drag leaves on the undo stack.

	It is in this suite and not beside the pure rules because a QGraphicsScene
	needs a QApplication, which is what main.cpp of this binary builds - and
	because the hand-written source list of the other suite is a contract
	saying those rules compute themselves with no scene at all. The
	arithmetic of the layout - what it keeps, what it refuses, what a change
	of enclosure costs - is proved there, in src/mountinglayout_test.cpp, and
	none of it is repeated here.

	What is here is the half a list of rectangles cannot have: that the
	millimetre survives the drawing, that the plate is not an object anybody
	can pick up, and that what is painted for a part nobody measured never
	becomes a measurement.
*/

namespace
{
	/// The same not-a-number the rules underneath are proved against.
	const qreal not_a_number = std::numeric_limits<qreal>::quiet_NaN();

	/// A breaker of a real width, at a place a folio grid would round off.
	MountedItem breaker()
	{
		MountedItem item(QStringLiteral("-Q1"),
				 QPointF(10.5, 20.25),
				 QSizeF(22.5, 85.0));
		item.uuid = QStringLiteral("q1");
		item.part_code = QStringLiteral("A9F74210");
		return item;
	}

	/// A part in the project that nobody has measured yet.
	MountedItem unmeasuredRelay()
	{
		MountedItem item(QStringLiteral("-KA1"),
				 QPointF(200.0, 40.0),
				 QSizeF());
		item.uuid = QStringLiteral("ka1");
		item.part_code = QStringLiteral("RXM4AB2BD");
		return item;
	}

	/// A mounting plate of 600 by 800, with the two of them on it.
	MountingSurface plate()
	{
		MountingSurface surface(QStringLiteral("QCM1"),
					QStringLiteral("plate"),
					MountingArea(600.0, 800.0));
		surface.uuid = QStringLiteral("face");
		surface.name = QStringLiteral("Platine");
		surface.items << breaker() << unmeasuredRelay();
		return surface;
	}
}

TEST_CASE("T19 — a cena da placa conta em milímetro",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setSurface(plate());

	SECTION("o milímetro do modelo é o milímetro da cena")
	{
		REQUIRE(scene.partCount() == 2);

		MountedPartItem *part = scene.partItem(QStringLiteral("q1"));
		REQUIRE(part != nullptr);

			//22,5 mm and 20,25 mm are exactly the numbers a folio
			//grid destroys: it rounds to whole folio units, so a
			//breaker drawn through QetGraphicsItem::setPos comes
			//back at 20 or at 25. Nothing here rounds anything.
		CHECK(part->millimetrePosition().x() == Approx(10.5));
		CHECK(part->millimetrePosition().y() == Approx(20.25));
		CHECK(part->drawnRect().width() == Approx(22.5));
		CHECK(part->drawnRect().height() == Approx(85.0));
	}

	SECTION("o retângulo desenhado e a pegada do modelo são o mesmo")
	{
		MountedPartItem *part = scene.partItem(QStringLiteral("q1"));
		REQUIRE(part != nullptr);

		const QRectF drawn = part->mapRectToScene(part->drawnRect());
		const QRectF footprint = scene.surface()
					 .item(QStringLiteral("q1"))
					 .footprint();

		CHECK(drawn.x() == Approx(footprint.x()));
		CHECK(drawn.y() == Approx(footprint.y()));
		CHECK(drawn.width() == Approx(footprint.width()));
		CHECK(drawn.height() == Approx(footprint.height()));
	}

	SECTION("a conversão para a cena é a identidade, e está num lugar só")
	{
		const QPointF position(37.5, 12.25);

		CHECK(MountingScene::sceneFromMillimetre(position) == position);
		CHECK(MountingScene::millimetreFromScene(position) == position);
	}

	SECTION("a chapa é fundo, e não há item nenhum que seja ela")
	{
		const QList<QGraphicsItem *> items = scene.items();
		CHECK(items.size() == 2);

		for (QGraphicsItem *item : items) {
			CHECK(item->type() == MountedPartItem::Type);
		}

			//A point well inside the plate and far from both parts:
			//there is nothing there to click on, because the plate
			//is painted as background and is not an object at all.
		CHECK(scene.itemAt(QPointF(500.0, 700.0), QTransform())
		      == nullptr);
		CHECK(scene.itemAt(QPointF(11.0, 21.0), QTransform())
		      == static_cast<QGraphicsItem *>(
			      scene.partItem(QStringLiteral("q1"))));
	}

	SECTION("a placa que ninguém mediu ainda mostra o que está montada nela")
	{
		MountingSurface unmeasured = plate();
		unmeasured.area = MountingArea();
		scene.setSurface(unmeasured);

		CHECK_FALSE(scene.isAreaMeasured());
		CHECK(scene.partCount() == 2);
		CHECK(scene.itemUuids().size() == 2);
	}
}

TEST_CASE("T19 — mover uma peça na placa é um passo de desfazer",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setSurface(plate());
	REQUIRE(scene.undoStack().count() == 0);

	SECTION("o milímetro novo entra no modelo, e desfazer devolve o antigo")
	{
		REQUIRE(scene.moveItem(QStringLiteral("q1"),
				       QPointF(120.75, 300.5)));
		REQUIRE(scene.undoStack().count() == 1);

		CHECK(scene.surface().item(QStringLiteral("q1"))
		      .position.x() == Approx(120.75));
		CHECK(scene.surface().item(QStringLiteral("q1"))
		      .position.y() == Approx(300.5));
		CHECK(scene.partItem(QStringLiteral("q1"))
		      ->millimetrePosition().x() == Approx(120.75));

		scene.undoStack().undo();

		CHECK(scene.surface().item(QStringLiteral("q1"))
		      .position.x() == Approx(10.5));
		CHECK(scene.surface().item(QStringLiteral("q1"))
		      .position.y() == Approx(20.25));
		CHECK(scene.partItem(QStringLiteral("q1"))
		      ->millimetrePosition().y() == Approx(20.25));

		scene.undoStack().redo();

		CHECK(scene.surface().item(QStringLiteral("q1"))
		      .position.x() == Approx(120.75));
		CHECK(scene.partItem(QStringLiteral("q1"))
		      ->millimetrePosition().y() == Approx(300.5));
	}

	SECTION("mover para o mesmo milímetro não deixa passo nenhum")
	{
		CHECK_FALSE(scene.moveItem(QStringLiteral("q1"),
					   QPointF(10.5, 20.25)));
		CHECK(scene.undoStack().count() == 0);
	}

	SECTION("mover o que não está na placa é recusado, e diz por quê")
	{
		QString error;

		CHECK_FALSE(scene.moveItem(QStringLiteral("nao-existe"),
					   QPointF(30.0, 30.0), &error));
		CHECK_FALSE(error.isEmpty());
		CHECK(scene.undoStack().count() == 0);
	}

	SECTION("uma posição que não é um número é recusada")
	{
		QString error;

		CHECK_FALSE(scene.moveItem(QStringLiteral("q1"),
					   QPointF(not_a_number, 12.0), &error));
		CHECK_FALSE(error.isEmpty());
		CHECK(scene.undoStack().count() == 0);
		CHECK(scene.partItem(QStringLiteral("q1"))
		      ->millimetrePosition().x() == Approx(10.5));
	}

	SECTION("sair da chapa é permitido, porque sair da chapa é o aviso")
	{
			//The rule already has a name for a part that does not
			//fit where it was put, and the report only works if the
			//part is allowed to be there to be reported.
		REQUIRE(scene.moveItem(QStringLiteral("q1"),
				       QPointF(1200.0, 40.0)));

		const MountedItem moved = scene.surface()
					  .item(QStringLiteral("q1"));
		CHECK(moved.position.x() == Approx(1200.0));
		CHECK_FALSE(scene.area().holds(moved.footprint()));
	}
}

TEST_CASE("T19 — a peça sem medida vira marcador, e o marcador não vira medida",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setSurface(plate());

	MountedPartItem *relay = scene.partItem(QStringLiteral("ka1"));
	REQUIRE(relay != nullptr);

	SECTION("desenha-se um marcador, de um lado que este arquivo nomeia")
	{
		CHECK_FALSE(relay->hasDeclaredSize());
		CHECK(relay->drawnRect().width()
		      == Approx(MountedPartItem::unmeasuredMarkerSide()));
		CHECK(relay->drawnRect().height()
		      == Approx(MountedPartItem::unmeasuredMarkerSide()));
	}

	SECTION("e o modelo continua dizendo que ninguém a mediu")
	{
			//This is the whole case. A branch of a third party drew
			//an unmeasured part as a box of 20 units and let that
			//box answer for its size: at its own scale that is a
			//part of 10 mm nobody ever bought, and it passes a fit
			//check that it then fails on the bench.
		const MountedItem mounted = scene.surface()
					    .item(QStringLiteral("ka1"));

		CHECK_FALSE(mounted.hasDeclaredSize());
		CHECK(mounted.declaredSize().width() == Approx(0.0));
		CHECK(mounted.declaredSize().height() == Approx(0.0));
		CHECK(mounted.footprint().width() == Approx(0.0));
		CHECK(relay->drawnRect().width()
		      != Approx(mounted.declaredSize().width()));
	}

	SECTION("mover a peça não a mede")
	{
		REQUIRE(scene.moveItem(QStringLiteral("ka1"),
				       QPointF(300.0, 500.0)));

		const MountedItem mounted = scene.surface()
					    .item(QStringLiteral("ka1"));

		CHECK(mounted.position.x() == Approx(300.0));
		CHECK_FALSE(mounted.hasDeclaredSize());
		CHECK(mounted.declaredSize().width() == Approx(0.0));
	}
}

TEST_CASE("T19 — o passo de mover nomeia a peça e sabe quando não é passo",
	  "[uibench][calepinagem]")
{
	MountingScene scene;
	scene.setSurface(plate());

	SECTION("o texto do desfazer nomeia a peça pelo rótulo dela")
	{
		MoveMountedPartCommand command(&scene, QStringLiteral("q1"),
					       QPointF(10.5, 20.25),
					       QPointF(60.0, 20.25));

		CHECK_FALSE(command.isNull());
		CHECK(command.text().contains(QStringLiteral("-Q1")));
		CHECK(command.itemUuid() == QStringLiteral("q1"));
		CHECK(command.after().x() == Approx(60.0));
	}

	SECTION("o mesmo milímetro nos dois lados não é passo")
	{
		MoveMountedPartCommand command(&scene, QStringLiteral("q1"),
					       QPointF(10.5, 20.25),
					       QPointF(10.5, 20.25));

		CHECK(command.isNull());
	}

	SECTION("sem cena, ou sem peça, não é passo")
	{
		MoveMountedPartCommand orphan(nullptr, QStringLiteral("q1"),
					      QPointF(0.0, 0.0),
					      QPointF(50.0, 0.0));
		MoveMountedPartCommand nameless(&scene, QString(),
						QPointF(0.0, 0.0),
						QPointF(50.0, 0.0));

		CHECK(orphan.isNull());
		CHECK(nameless.isNull());
	}
}
