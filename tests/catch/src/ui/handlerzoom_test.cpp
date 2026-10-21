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
#include "../../../../sources/QetGraphicsItemModeler/qetgraphicshandleritem.h"

#include <catch2/catch.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QPixmap>

/*
	A handle drawn smaller when the drawing is zoomed out (T76): at a fixed
	size on screen it covered a symbol drawn smaller than itself.
*/
namespace
{
		/// How many pixels of the handle's green a view of @a zoom shows
	int greenPixels(qreal zoom)
	{
			//Smaller than the view at every zoom asked: the view then
			//centres it, with nothing to scroll
		QGraphicsScene scene(-20, -20, 40, 40);
		auto *handle = new QetGraphicsHandlerItem(10);
		handle->setColor(QColor(0, 200, 0));
		scene.addItem(handle);
		QGraphicsView view(&scene);
		view.resize(200, 200);
		view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		view.scale(zoom, zoom);
		const QImage image = view.viewport()->grab().toImage();
		int count = 0;
		for (int y = 0 ; y < image.height() ; ++y) {
			for (int x = 0 ; x < image.width() ; ++x) {
				const QColor c = image.pixelColor(x, y);
				if (c.green() > 120 && c.red() < 100 && c.blue() < 100) {
					++count;
				}
			}
		}
		return count;
	}
}

TEST_CASE("CU-76.1 — A handle is drawn smaller when zoomed out", "[T76][handle]")
{
	const int full = greenPixels(1.0);
	CAPTURE(full);
		//The fixture is what it says: the handle is drawn at all
	REQUIRE(full > 30);

	SECTION("at 50%, about a quarter of its surface")
	{
		const int half = greenPixels(0.5);
		CAPTURE(half);
		CHECK(half < full * 0.5);
		CHECK(half > 0);
	}
	SECTION("at 20%, no smaller than at 40%: still seen")
	{
		const int fifth = greenPixels(0.2);
		const int two_fifths = greenPixels(0.4);
		CAPTURE(fifth, two_fifths);
		CHECK(fifth > 0);
		CHECK(fifth == two_fifths);
	}
	SECTION("zoomed in, as it always was")
	{
		CHECK(greenPixels(3.0) == full);
	}
}
