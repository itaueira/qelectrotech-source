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

#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QSettings>

#include <cmath>

/*
	The magnet of the wire (T72): a symbol dragged until a terminal of its
	is closer than half a step of the grid to the line of the terminal its
	wire goes to goes onto that line, past the grid.

	The module has its terminal drawn at y 0.17, as the input module of the
	project measured - placed at (100, 100), the end of the terminal is at
	(110, 100.17). The arrow has its own at a whole number, on its left.
	The grid is 1, as on the machine of the person who asked.
*/
namespace
{
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

	QString definition(const char *name, const QString &terminal)
	{
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"20\" hotspot_x=\"10\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%2"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name), terminal);
	}

	QString instance(const char *definition, int x, int y, const char *label, int index)
	{
		return QStringLiteral(
			"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://bench/%3\""
			" uuid=\"{72000000-0000-4000-8000-0000000000%4}\">"
			"<terminals/><inputs/>"
			"<elementInformations>"
			"<elementInformation show=\"1\" name=\"label\">%5</elementInformation>"
			"</elementInformations>"
			"<dynamic_texts/><texts_groups/>"
			"</element>")
			.arg(x).arg(y)
			.arg(QLatin1String(definition))
			.arg(index, 2, 10, QLatin1Char('0'))
			.arg(QLatin1String(label));
	}

		/// The module at (100, 100), and the arrow at (200, @a arrow_y)
	QString fixtureXml(int arrow_y)
	{
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"600\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(definition("module.elmt",
					QStringLiteral("<terminal x=\"10\" y=\"0.17\" orientation=\"e\"/>"))
				 + definition("arrow.elmt",
					      QStringLiteral("<terminal x=\"-10\" y=\"0\" orientation=\"w\"/>")),
			     instance("module.elmt", 100, 100, "M", 0)
				 + instance("arrow.elmt", 200, arrow_y, "A", 1));
	}

	Element *tagged(Diagram *sheet, const char *label)
	{
		const auto elements = sheet->elements();
		for (Element *element : elements) {
			if (element->actualLabel() == QLatin1String(label)) return element;
		}
		return nullptr;
	}

	struct Bench
	{
		explicit Bench(int arrow_y) : project(fixtureXml(arrow_y), QStringLiteral("wiremagnet.qet")) {}

		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			if (!sheet) return false;
			module = tagged(sheet, "M");
			arrow = tagged(sheet, "A");
			if (!module || !arrow) return false;
			if (module->terminals().isEmpty() || arrow->terminals().isEmpty()) return false;
			wire = new Conductor(module->terminals().first(), arrow->terminals().first());
			sheet->addItem(wire);
			sheet->clearSelection();
			arrow->setSelected(true);
			return true;
		}

			/// The arrow dragged by mouse until its position is @a to
		void drag(const QPointF &to)
		{
			const QPointF from = arrow->pos();
			const auto send = [&](QEvent::Type type, const QPointF &at) {
				QGraphicsSceneMouseEvent event(type);
				event.setButton(Qt::LeftButton);
				event.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton
											   : Qt::LeftButton);
				event.setScenePos(at);
				event.setButtonDownScenePos(Qt::LeftButton, from);
				sheet->sendEvent(arrow, &event);
			};
			send(QEvent::GraphicsSceneMousePress, from);
			send(QEvent::GraphicsSceneMouseMove, to);
			send(QEvent::GraphicsSceneMouseRelease, to);
		}

			/// The height of the wire, from its highest point to its lowest
		qreal wireHeight() const
		{
			return wire->mapToScene(wire->path()).boundingRect().height();
		}

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
		Element *module = nullptr;
		Element *arrow = nullptr;
		Conductor *wire = nullptr;
	};

	void setGrid(qreal grid)
	{
		QSettings().setValue(QStringLiteral("diagrameditor/Xgrid"), grid);
		QSettings().setValue(QStringLiteral("diagrameditor/Ygrid"), grid);
	}
}

TEST_CASE("CU-72.1 — The magnet lines the wire up", "[T72][magnet]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	setGrid(1);
	Bench bench(105);
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open());
		//The fixture is what it says: the end of the module's terminal is
		//off the grid by 0.17
	REQUIRE(bench.module->terminals().first()->dockConductor().y() == Approx(100.17));
	REQUIRE(bench.wireHeight() > 4);

		//Dragged to 100, 0.17 from the line: onto it, and the wire straight
	bench.drag(QPointF(200, 100));
	CHECK(bench.arrow->pos().y() == Approx(100.17));
	CHECK(bench.arrow->terminals().first()->dockConductor().y() == Approx(100.17));
	CHECK(bench.wireHeight() < 0.01);
}

TEST_CASE("CU-72.2 — The magnet does not pull from afar", "[T72][magnet]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	setGrid(1);
	Bench bench(105);
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open());

		//1.17 from the line, more than half a step of 1: on the grid
	bench.drag(QPointF(200, 99));
	CHECK(bench.arrow->pos().y() == Approx(99));

	SECTION("on a grid of 10, half a step is 5")
	{
		setGrid(10);
			//Let go at 102: on the grid of 10, at 100, 0.17 from the line
		bench.drag(QPointF(200, 102));
		CHECK(bench.arrow->pos().y() == Approx(100.17));
	}
}

TEST_CASE("CU-72.3 — With the arrows, and away again", "[T72][magnet]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	KeepSetting keep_ky(QStringLiteral("diagrameditor/key_Ygrid"));
	setGrid(1);
	QSettings().setValue(QStringLiteral("diagrameditor/key_Ygrid"), 1);
	Bench bench(101);
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open());

	const auto up = [&bench]() {
		QKeyEvent press(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
		QCoreApplication::sendEvent(bench.sheet, &press);
		QKeyEvent release(QEvent::KeyRelease, Qt::Key_Up, Qt::NoModifier);
		QCoreApplication::sendEvent(bench.sheet, &release);
	};

		//From 101, a step up to 100, 0.17 from the line: onto it
	up();
	CHECK(bench.arrow->pos().y() == Approx(100.17));
	CHECK(bench.wireHeight() < 0.01);
		//And a step up again takes it away: 1 from the line, not pulled back
	up();
	CHECK(bench.arrow->pos().y() == Approx(99.17));
}

TEST_CASE("T72 — Ctrl and an arrow on a grid of 10", "[T72][magnet]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	KeepSetting keep_kx(QStringLiteral("diagrameditor/key_Xgrid"));
	KeepSetting keep_fy(QStringLiteral("diagrameditor/key_fine_Ygrid"));
	setGrid(10);
	QSettings().setValue(QStringLiteral("diagrameditor/key_Xgrid"), 10);
	QSettings().setValue(QStringLiteral("diagrameditor/key_fine_Ygrid"), 1);
	Bench bench(101);
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open());

	const auto press = [&bench](Qt::Key key, Qt::KeyboardModifiers modifiers) {
		QKeyEvent down(QEvent::KeyPress, key, modifiers);
		QCoreApplication::sendEvent(bench.sheet, &down);
		QKeyEvent up(QEvent::KeyRelease, key, modifiers);
		QCoreApplication::sendEvent(bench.sheet, &up);
	};

	SECTION("a fine step onto the line is pulled onto it")
	{
			//From 101, the fine step of 1 lands at 100, 0.17 from the line
		press(Qt::Key_Up, Qt::ControlModifier);
		CHECK(bench.arrow->pos().y() == Approx(100.17));
		CHECK(bench.wireHeight() < 0.01);
	}

	SECTION("a fine step off the line is not pulled back")
	{
			//On the line, a fine step of 1 off it: half a step of the grid
			//is 5, and a reach of that much would pull it back at once,
			//so that Ctrl and an arrow never moved a symbol off a wire
		static_cast<QGraphicsItem *>(bench.arrow)->setPos(200, 100.17);
		press(Qt::Key_Down, Qt::ControlModifier);
		CHECK(bench.arrow->pos().y() == Approx(101.17));
			//And an arrow along the wire leaves that fine step alone
		press(Qt::Key_Right, Qt::NoModifier);
		CHECK(bench.arrow->pos().x() == Approx(210));
		CHECK(bench.arrow->pos().y() == Approx(101.17));
	}
}

TEST_CASE("T72 — Two symbols on the grid are not pulled", "[T72][magnet]")
{
	KeepSetting keep_x(QStringLiteral("diagrameditor/Xgrid"));
	KeepSetting keep_y(QStringLiteral("diagrameditor/Ygrid"));
	setGrid(1);
	Bench bench(105);
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.open());
		//Moved off the line by a whole step, the module's terminal is at
		//a whole number too: nothing is closer than half a step
	static_cast<QGraphicsItem *>(bench.module)->setPos(100, 99.83);
	REQUIRE(bench.module->terminals().first()->dockConductor().y() == Approx(100));
	bench.drag(QPointF(200, 101));
	CHECK(bench.arrow->pos().y() == Approx(101));
}
