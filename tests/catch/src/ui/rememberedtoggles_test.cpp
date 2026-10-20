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
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/configpage/generalconfigurationpage.h"

#include <QDoubleSpinBox>

#include <catch2/catch.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QKeyEvent>
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

namespace
{
		/// A key pressed on @p folio, as the view hands it on
	void press(Diagram *folio, Qt::Key key, Qt::KeyboardModifiers modifiers)
	{
		QKeyEvent event(QEvent::KeyPress, key, modifiers);
		QCoreApplication::sendEvent(folio, &event);
		QKeyEvent release(QEvent::KeyRelease, key, modifiers);
		QCoreApplication::sendEvent(folio, &release);
	}

		/// The first symbol of @p folio that is not turned, with a text it
		/// can be dragged by
	Element *upright(Diagram *folio)
	{
		const QList<Element *> elements = folio->elements();
		for (Element *element : elements)
		{
			if (element->rotation() == 0 && !element->dynamicTextItems().isEmpty()) {
				return element;
			}
		}
		return nullptr;
	}
}

TEST_CASE("CU-69.6 — The arrows move a symbol on a grid of 0.5", "[T69][grid]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	KeepSetting keep_kx(QStringLiteral("diagrameditor/key_Xgrid"));
	KeepSetting keep_ky(QStringLiteral("diagrameditor/key_Ygrid"));
	KeepSetting keep_fx(QStringLiteral("diagrameditor/key_fine_Xgrid"));
	KeepSetting keep_fy(QStringLiteral("diagrameditor/key_fine_Ygrid"));
	QSettings settings;
	settings.setValue(QStringLiteral("diagrameditor/Xgrid"), 0.5);
	settings.setValue(QStringLiteral("diagrameditor/Ygrid"), 0.5);
	settings.setValue(QStringLiteral("diagrameditor/key_Xgrid"), 10);
	settings.setValue(QStringLiteral("diagrameditor/key_Ygrid"), 10);
		//The steps as they come: 1 for Ctrl, coarser than the grid
	settings.setValue(QStringLiteral("diagrameditor/key_fine_Xgrid"), 1);
	settings.setValue(QStringLiteral("diagrameditor/key_fine_Ygrid"), 1);

	UiBench::Project bench(small_example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *folio = bench.busiestSheet();
	REQUIRE(folio != nullptr);
	Element *element = upright(folio);
	REQUIRE(element != nullptr);
	folio->clearSelection();
	element->setSelected(true);
	const QPointF start = element->pos();

		//Ctrl and an arrow: half a step, the grid, and not the 1 of the
		//setting, which a grid of 0.5 would never reach half of
	press(folio, Qt::Key_Right, Qt::ControlModifier);
	CHECK(element->pos().x() == Approx(start.x() + 0.5));
	press(folio, Qt::Key_Down, Qt::ControlModifier);
	CHECK(element->pos().y() == Approx(start.y() + 0.5));
		//An arrow alone: the step of the setting
	press(folio, Qt::Key_Right, Qt::NoModifier);
	CHECK(element->pos().x() == Approx(start.x() + 10.5));

	SECTION("a step of a fraction, from the setting")
	{
		settings.setValue(QStringLiteral("diagrameditor/key_Xgrid"), 2.5);
		const qreal before = element->pos().x();
		press(folio, Qt::Key_Right, Qt::NoModifier);
		CHECK(element->pos().x() == Approx(before + 2.5));
	}

	SECTION("a step finer than the grid moves one step of it")
	{
		settings.setValue(QStringLiteral("diagrameditor/key_Xgrid"), 0.2);
		const qreal before = element->pos().x();
		press(folio, Qt::Key_Right, Qt::NoModifier);
		CHECK(element->pos().x() == Approx(before + 0.5));
	}
}

TEST_CASE("T69 — The boxes of the arrow steps take 0.5", "[T69][grid]")
{
	KeepSetting keep_kx(QStringLiteral("diagrameditor/key_Xgrid"));
	KeepSetting keep_fx(QStringLiteral("diagrameditor/key_fine_Xgrid"));
	QSettings().setValue(QStringLiteral("diagrameditor/key_Xgrid"), 2.5);
	QSettings().setValue(QStringLiteral("diagrameditor/key_fine_Xgrid"), 0.5);

	GeneralConfigurationPage page;
	auto *step = page.findChild<QDoubleSpinBox *>(QStringLiteral("DiagramEditor_xKeyGrid_sb"));
	auto *fine = page.findChild<QDoubleSpinBox *>(QStringLiteral("DiagramEditor_xKeyGridFine_sb"));
	REQUIRE(step);
	REQUIRE(fine);
	CHECK(step->value() == Approx(2.5));
	CHECK(fine->value() == Approx(0.5));
	CHECK(step->minimum() == Approx(0.1));
	CHECK(fine->minimum() == Approx(0.1));
	CHECK(fine->maximum() == Approx(9));
}

TEST_CASE("CU-69.7 — A text of a symbol dragged on a grid of 0.5", "[T69][grid]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	KeepSetting keep_text(QStringLiteral("diagrameditor/text_grid_divisor"));
	QSettings().setValue(QStringLiteral("diagrameditor/Xgrid"), 0.5);
	QSettings().setValue(QStringLiteral("diagrameditor/Ygrid"), 0.5);
		//The texts on the grid itself, and not on a finer one
	QSettings().setValue(QStringLiteral("diagrameditor/text_grid_divisor"), 1);

	UiBench::Project bench(small_example);
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *folio = bench.busiestSheet();
	REQUIRE(folio != nullptr);
	Element *element = upright(folio);
	REQUIRE(element != nullptr);
	DynamicElementTextItem *text = element->dynamicTextItems().first();
	REQUIRE((text->flags() & QGraphicsItem::ItemIsMovable));
		//From a place on the grid
	text->setPos(3, 0);
	const QPointF start = text->pos();
	const QPointF down = text->mapToScene(text->boundingRect().center());

		//Half a pixel to the right, Shift held - Shift drags the text and
		//not its symbol: on a grid of 0.5 the text goes half a step, and
		//not the whole pixel the move used to be rounded to before it met
		//the grid
	const auto send = [&](QEvent::Type type, const QPointF &at) {
		QGraphicsSceneMouseEvent event(type);
		event.setButton(Qt::LeftButton);
		event.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
		event.setScenePos(at);
		event.setButtonDownScenePos(Qt::LeftButton, down);
		event.setModifiers(Qt::ShiftModifier);
		folio->sendEvent(text, &event);
	};
	send(QEvent::GraphicsSceneMousePress, down);
	send(QEvent::GraphicsSceneMouseMove, down + QPointF(0.5, 0));
	CHECK(text->pos().x() == Approx(start.x() + 0.5));
	CHECK(text->pos().y() == Approx(start.y()));
	send(QEvent::GraphicsSceneMouseRelease, down + QPointF(0.5, 0));
}
