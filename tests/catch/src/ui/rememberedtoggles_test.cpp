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

#include "../../../../sources/diagram.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/configpage/generalconfigurationpage.h"

#include <QDoubleSpinBox>

#include <catch2/catch.hpp>

#include <QImage>
#include <QPainter>
#include <QSettings>
#include <QVariant>
#include <QtMath>

/*
	The buttons that switch something on and off start the way they were
	left: the auto conductor, the grid and the guides. The
	buttons themselves live in QETDiagramEditor, which this suite cannot
	build (see contextmenuassign_test.cpp): what is checked here is what
	each button stores and what every project or folio opened afterwards
	reads from it. That the button stores it is one line in its handler,
	and is seen on the screen.

	And, at the end, the grid of 1 that the person who asked for this
	works with, drawn on a folio: it is the reason the grid was switched
	off every time.
*/

namespace
{
	const QString small_example = QStringLiteral("convertisseur.qet");
	const QString auto_conductor_key = QStringLiteral("diagrameditor/auto_conductor");
	const QString grid_key = QStringLiteral("diagrameditor/grid_display_startup");
	const QString guides_key = QStringLiteral("diagrameditor/guides_display_startup");

	/// Puts a setting back the way it was, whatever the case did to it
	class KeepSetting
	{
		public:
			explicit KeepSetting(const QString &key) :
				m_key(key),
				m_had(QSettings().contains(key)),
				m_value(QSettings().value(key))
			{}
			~KeepSetting()
			{
				QSettings settings;
				if (m_had) {
					settings.setValue(m_key, m_value);
				} else {
					settings.remove(m_key);
				}
			}

		private:
			QString m_key;
			bool m_had;
			QVariant m_value;
	};

	/**
		@return @p area of @p folio drawn at @p zoom pixels per folio unit
	*/
	QImage drawn(Diagram *folio, const QRectF &area, int zoom)
	{
		QImage image(area.size().toSize() * zoom, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		folio->render(&painter, QRectF(QPointF(0, 0), image.size()), area);
		painter.end();
		return image;
	}

	/// The pixels that are not white: on an empty folio, the dots of the grid
	int inkedPixels(const QImage &image)
	{
		int inked = 0;
		for (int y = 0 ; y < image.height() ; ++y) {
			for (int x = 0 ; x < image.width() ; ++x) {
				inked += image.pixel(x, y) != qRgb(255, 255, 255) ? 1 : 0;
			}
		}
		return inked;
	}
}

TEST_CASE("CU-60.1 — The auto conductor starts the way it was last left",
		  "[T60][autoconductor]")
{
	KeepSetting keep(auto_conductor_key);

	SECTION("never chosen: on, as it always was")
	{
		QSettings().remove(auto_conductor_key);
		CHECK(QETProject::defaultAutoConductor());
		QETProject created;
		CHECK(created.autoConductor());
	}

	SECTION("left off: a new project and one opened from disk start off")
	{
		QETProject::setDefaultAutoConductor(false);
		CHECK_FALSE(QSettings().value(auto_conductor_key, true).toBool());

		QETProject created;
		CHECK_FALSE(created.autoConductor());

		UiBench::Project opened(small_example);
		INFO(opened.error().toStdString());
		REQUIRE(opened.isOpen());
		CHECK_FALSE(opened->autoConductor());
	}

	SECTION("left on again: on")
	{
		QETProject::setDefaultAutoConductor(false);
		QETProject::setDefaultAutoConductor(true);

		QETProject created;
		CHECK(created.autoConductor());

		UiBench::Project opened(small_example);
		REQUIRE(opened.isOpen());
		CHECK(opened->autoConductor());
	}

	SECTION("a project already open keeps its own")
	{
		QETProject::setDefaultAutoConductor(true);
		QETProject before;
		QETProject::setDefaultAutoConductor(false);
		CHECK(before.autoConductor());
	}
}

TEST_CASE("CU-60.2 — The grid and the guides start the way they were last left",
		  "[T60][grid]")
{
	KeepSetting keep_grid(grid_key);
	KeepSetting keep_guides(guides_key);

		//Each one the opposite of the other, so neither can pass by
		//following the other's setting
	const bool grid = GENERATE(false, true);
	const bool guides = !grid;
	CAPTURE(grid, guides);
	QSettings().setValue(grid_key, grid);
	QSettings().setValue(guides_key, guides);

	UiBench::Project opened(small_example);
	INFO(opened.error().toStdString());
	REQUIRE(opened.isOpen());
	REQUIRE(opened.diagramCount() > 0);
	for (Diagram *folio : opened.diagrams()) {
		CAPTURE(folio->title().toStdString());
		CHECK(folio->displayGrid() == grid);
		CHECK(folio->displayGuides() == guides);
	}
}

TEST_CASE("CU-60.4 — On a folio, a grid of 1 seen from afar draws one dot in five steps",
		  "[T60][grid]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	QSettings().setValue(QStringLiteral("diagrameditor/Xgrid"), 1);
	QSettings().setValue(QStringLiteral("diagrameditor/Ygrid"), 1);

	QETProject project;
	Diagram *folio = project.addNewDiagram();
	REQUIRE(folio != nullptr);
	folio->setDisplayGrid(true);

		//An empty square of the folio, away from its border and headers
	const QRectF inside = folio->border_and_titleblock.insideBorderRect();
	const QRectF area(QPointF(qCeil(inside.x()) + 20, qCeil(inside.y()) + 20), QSizeF(100, 100));
	REQUIRE(inside.contains(area));

	SECTION("at zoom 1: a dot every 5 pixels, about 20 x 20, where every pixel was one")
	{
		const int inked = inkedPixels(drawn(folio, area, 1));
		CAPTURE(inked);
		CHECK(inked >= 19 * 19);
		CHECK(inked <= 21 * 21);
	}

	SECTION("at zoom 6: every point of the grid, about 100 x 100")
	{
		const int inked = inkedPixels(drawn(folio, area, 6));
		CAPTURE(inked);
		CHECK(inked >= 99 * 99);
		CHECK(inked <= 101 * 101);
	}
}

TEST_CASE("CU-69.2 — A symbol snaps to a grid of 0.5", "[T69][grid]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	QSettings().setValue(QStringLiteral("diagrameditor/Xgrid"), 0.5);
	QSettings().setValue(QStringLiteral("diagrameditor/Ygrid"), 0.5);

	CHECK(Diagram::snapToGrid(QPointF(10.3, 7.8), false) == QPointF(10.5, 8));
		//With Ctrl: the nearest pixel would be coarser than the grid
	CHECK(Diagram::snapToGrid(QPointF(10.3, 7.8), true) == QPointF(10.5, 8));

	SECTION("on a grid of 10, Ctrl still gives the nearest pixel")
	{
		QSettings().setValue(QStringLiteral("diagrameditor/Xgrid"), 10);
		QSettings().setValue(QStringLiteral("diagrameditor/Ygrid"), 10);
		CHECK(Diagram::snapToGrid(QPointF(10.3, 7.8), false) == QPointF(10, 10));
		CHECK(Diagram::snapToGrid(QPointF(10.3, 7.8), true) == QPointF(10, 8));
	}
}

TEST_CASE("T69 — The box of the grid in the preferences takes 0.5", "[T69][grid]")
{
		//Read back as the page opens. Applying the page is not done here:
		//it also resets the collections and the screen scaling
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	QSettings().setValue(QStringLiteral("diagrameditor/Xgrid"), 0.5);
	QSettings().setValue(QStringLiteral("diagrameditor/Ygrid"), 0.25);

	GeneralConfigurationPage page;
	auto *x = page.findChild<QDoubleSpinBox *>(QStringLiteral("DiagramEditor_xGrid_sb"));
	auto *y = page.findChild<QDoubleSpinBox *>(QStringLiteral("DiagramEditor_yGrid_sb"));
	REQUIRE(x);
	REQUIRE(y);
	CHECK(x->value() == Approx(0.5));
	CHECK(y->value() == Approx(0.25));
	CHECK(x->minimum() == Approx(0.1));
	CHECK(x->decimals() == 2);
	x->setValue(0.05);
	CHECK(x->value() == Approx(0.1));
}

TEST_CASE("CU-69.3 — On a folio, the dots of a grid of 0.5", "[T69][grid]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	QSettings().setValue(QStringLiteral("diagrameditor/Xgrid"), 0.5);
	QSettings().setValue(QStringLiteral("diagrameditor/Ygrid"), 0.5);

	QETProject project;
	Diagram *folio = project.addNewDiagram();
	REQUIRE(folio != nullptr);
	folio->setDisplayGrid(true);

	const QRectF inside = folio->border_and_titleblock.insideBorderRect();

	SECTION("at zoom 10: a dot every half unit, 5 pixels apart, about 40 x 40")
	{
		const QRectF area(QPointF(qCeil(inside.x()) + 20, qCeil(inside.y()) + 20), QSizeF(20, 20));
		REQUIRE(inside.contains(area));
		const int inked = inkedPixels(drawn(folio, area, 10));
		CAPTURE(inked);
		CHECK(inked >= 39 * 39);
		CHECK(inked <= 41 * 41);
	}

	SECTION("at zoom 1: one in ten, a dot every 5 units, as for a grid of 1")
	{
		const QRectF area(QPointF(qCeil(inside.x()) + 20, qCeil(inside.y()) + 20), QSizeF(100, 100));
		REQUIRE(inside.contains(area));
		const int inked = inkedPixels(drawn(folio, area, 1));
		CAPTURE(inked);
		CHECK(inked >= 19 * 19);
		CHECK(inked <= 21 * 21);
	}
}
