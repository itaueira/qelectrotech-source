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

#include "../../../../sources/QetGraphicsItemModeler/qetgraphicshandleritem.h"
#include "../../../../sources/TerminalStrip/GraphicsItem/terminalstripitem.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/terminalstripautoimport.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/elementsmover.h"
#include "../../../../sources/factory/propertieseditorfactory.h"
#include "../../../../sources/cable/cablepart.h"
#include "../../../../sources/PropertiesEditor/propertieseditorwidget.h"
#include "../../../../sources/location/bomcollector.h"
#include "../../../../sources/location/locationtree.h"
#include "../../../../sources/location/projectlocation.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/conductortextitem.h"
#include "../../../../sources/qetgraphicsitem/crossrefitem.h"
#include "../../../../sources/qetgraphicsitem/diagramimageitem.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/locationareaitem.h"
#include "../../../../sources/qetgraphicsitem/independenttextitem.h"
#include "../../../../sources/qetgraphicsitem/qetshapeitem.h"
#include "../../../../sources/qetgraphicsitem/stripframeitem.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/mountinglayoutviewitem.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/qetgraphicsheaderitem.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountedprofileitem.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/wiringlistexport.h"

#include <catch2/catch.hpp>

#include <QSqlQuery>
#include <QUndoStack>

/*
	The field region (T85): a location rectangle whose location is out of
	the project, painted as a grey mask, and what sits in it out of every
	list.

	The bench, one folio, y = 200: Q1 at x = 100 and X1 at 300, in the
	panel; B1 at 600 and B2 at 750, the sensors in the field. Three wires:
	Q1 - X1, X1 - B1, X1 - B2. Below, at y = 450, two terminals of a strip
	not put in a strip yet: X1:1 at 300, in the panel, and X2:1 at 680, in
	the field. The region is drawn from (550, 150) to (820, 500), round the
	two sensors and X2:1.

	The location of the region, CAMPO, "Elementos de Campo", is marked out
	of the project in the tree. Nothing else is: the lists are asked before
	the region exists, after it is drawn, after a sensor leaves it, and
	after the location is brought back into the project.
*/
namespace
{
	const QString field_code = QStringLiteral("CAMPO");
	const QString field_name = QStringLiteral("Elementos de Campo");

	QString fixture()
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		const QString definitions = QStringLiteral(
			"<element name=\"part.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"30\" height=\"20\" hotspot_x=\"15\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">part</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			"<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			"</description>"
			"</definition>"
			"</element>"
			"<element name=\"strip.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"20\" hotspot_x=\"10\" hotspot_y=\"10\""
			" orientation=\"dnnn\" link_type=\"terminal\">"
			"<names><name lang=\"en\">strip</name></names>"
			"<description>"
			"<rect x=\"-6\" y=\"-6\" width=\"12\" height=\"12\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"</description>"
			"</definition>"
			"</element>");

		struct Part { int x; const char *label; };
		const Part parts[] = {{100, "Q1"}, {300, "X1"}, {600, "B1"}, {750, "B2"}};
		QString instances;
		for (int index = 0 ; index < 4 ; ++index)
		{
			instances += QStringLiteral(
				"<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\""
				" freezeLabel=\"false\" orientation=\"0\""
				" type=\"embed://bench/part.elmt\""
				" uuid=\"{85500000-0000-4000-8000-00000000000%2}\">"
				"<terminals>"
				"<terminal x=\"%3\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
				"<terminal x=\"%5\" y=\"0\" orientation=\"3\" id=\"%6\"/>"
				"</terminals>"
				"<inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%7</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(parts[index].x)
				.arg(index)
				.arg(east_dock)
				.arg(index * 2)
				.arg(west_dock)
				.arg(index * 2 + 1)
				.arg(QLatin1String(parts[index].label));
		}

		struct Loose { int x; const char *label; };
		const Loose loose[] = {{300, "X1:1"}, {680, "X2:1"}};
		for (int index = 0 ; index < 2 ; ++index)
		{
			instances += QStringLiteral(
				"<element x=\"%1\" y=\"450\" z=\"10\" prefix=\"\""
				" freezeLabel=\"false\" orientation=\"0\""
				" type=\"embed://bench/strip.elmt\""
				" uuid=\"{85500000-0000-4000-8000-00000000001%2}\">"
				"<terminals/><inputs/>"
				"<elementInformations>"
				"<elementInformation show=\"1\" name=\"label\">%3</elementInformation>"
				"</elementInformations>"
				"<dynamic_texts/><texts_groups/>"
				"</element>")
				.arg(loose[index].x)
				.arg(index)
				.arg(QLatin1String(loose[index].label));
		}

			//Q1 east (0) to X1 west (3); X1 east (2) to B1 west (5) and to
			//B2 west (7)
		const QString conductors = QStringLiteral(
			"<conductor terminal1=\"0\" terminal2=\"3\" num=\"W1\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"
			"<conductor terminal1=\"2\" terminal2=\"5\" num=\"W2\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"
			"<conductor terminal1=\"2\" terminal2=\"7\" num=\"W3\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>");

		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" folio=\"1\""
			" height=\"600\" cols=\"17\" colsize=\"50\" rows=\"8\""
			" rowsize=\"80\" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors>%3</conductors>"
			"</diagram>"
			"</project>")
			.arg(definitions, instances, conductors);
	}

	struct Bench
	{
		Bench() : project(fixture()) {}

			/// Open, and put CAMPO out of the project in the tree
		bool open()
		{
			if (!project.isOpen()) return false;
			sheet = project.diagram(0);
			LocationTree tree = project->locationTree();
			ProjectLocation field(field_code, field_name);
			field.outside_project = true;
			if (tree.append(field).isEmpty()) return false;
			project->setLocationTree(tree);
			return sheet->elements().size() == 6 && sheet->conductors().size() == 3;
		}

			/// The region round the sensors, as editProperty() makes it once
			/// the person chose CAMPO in its properties
		LocationAreaItem *drawRegion()
		{
			auto *area = new LocationAreaItem(QPointF(550, 150), QPointF(820, 500));
			sheet->addItem(area);
			area->applyProperties(field_code, QColor());
			return area;
		}

		Element *element(const QString &label) const
		{
			for (Element *e : sheet->elements()) {
				if (e->actualLabel() == label) return e;
			}
			return nullptr;
		}

		QString pathOf(const QString &label) const
		{
			Element *e = element(label);
			return e ? e->elementInformations().value(QETInformation::ELMT_LOCATION_PATH).toString()
				 : QStringLiteral("?");
		}

		int count(const QString &sql) const
		{
			project->dataBase()->updateDB();
			QSqlQuery query = project->dataBase()->newQuery(sql);
			int rows = 0;
			if (!query.exec()) return -1;
			while (query.next()) ++rows;
			return rows;
		}

			/// The rows of the bill of materials
		int bom() const {return count(QStringLiteral("SELECT label FROM element_nomenclature_view"));}
			/// The pieces the layout is made of
		int layout() const
		{
			project->dataBase()->updateDB();
			return project->dataBase()->bomElements().size();
		}
			/// The drawn rows of the purchase list
		int purchase() const {return count(BomCollector::selectStatement());}
			/// The rows of the wiring list of the data base
		int wiringView() const {return count(QStringLiteral("SELECT conductor_uuid FROM wiring_list_view"));}
			/// The wires of the exported wiring list, header left out
		int wiringCsv() const
		{
			WiringListExport exporter(project.project());
			const QString csv = exporter.toCsvString();
			return csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts).size() - 1;
		}

		void dragTo(Element *element, const QPointF &target)
		{
			sheet->clearSelection();
			element->setSelected(true);
			ElementsMover mover;
			REQUIRE(mover.beginMovement(sheet) >= 1);
			mover.continueMovement(target - element->pos());
			mover.endMovement();
			sheet->clearSelection();
		}

		UiBench::ScratchProject project;
		Diagram *sheet = nullptr;
	};
}

TEST_CASE("CU-85.5 - The field region is a grey mask, named in its corner", "[T85][fieldregion]")
{
	Bench bench;
	REQUIRE(bench.open());

	LocationAreaItem *region = bench.drawRegion();
	REQUIRE(region->locationPath() == field_code);

	SECTION("light grey and see-through by default, under the symbols and the wires")
	{
		CHECK(region->isOutsideProject());
		CHECK(region->maskColor() == QColor());
		CHECK(region->fillColor() == LocationAreaItem::defaultMaskColor());
		CHECK(region->fillColor().alpha() > 0);
		CHECK(region->fillColor().alpha() < 255);
		const QColor grey = region->fillColor();
		CHECK(grey.red() == grey.green());
		CHECK(grey.green() == grey.blue());
		for (Element *e : bench.sheet->elements()) CHECK(region->zValue() < e->zValue());
		for (Conductor *c : bench.sheet->conductors()) CHECK(region->zValue() < c->zValue());
	}

	SECTION("the name of the region in its corner, and not a code")
	{
		CHECK(region->captionText() == field_name);
	}

	SECTION("the two sensors and the terminal in it take its location")
	{
		CHECK(bench.pathOf(QStringLiteral("B1")) == field_code);
		CHECK(bench.pathOf(QStringLiteral("B2")) == field_code);
		CHECK(bench.pathOf(QStringLiteral("X2:1")) == field_code);
		CHECK(bench.pathOf(QStringLiteral("Q1")).isEmpty());
		CHECK(bench.pathOf(QStringLiteral("X1")).isEmpty());
	}

	SECTION("another colour and transparency, one undo step, saved with the folio")
	{
		const QColor chosen(200, 60, 40, 90);
		const int steps = bench.sheet->undoStack().count();
		region->applyProperties(field_code, chosen);
		CHECK(bench.sheet->undoStack().count() == steps + 1);
		CHECK(region->fillColor() == chosen);

		bench.sheet->undoStack().undo();
		CHECK(region->maskColor() == QColor());
		CHECK(region->fillColor() == LocationAreaItem::defaultMaskColor());
		bench.sheet->undoStack().redo();
		CHECK(region->fillColor() == chosen);

		REQUIRE(bench.project.saveAndReopen());
		bench.sheet = bench.project.diagram(0);
		LocationAreaItem *reopened = nullptr;
		for (QGraphicsItem *item : bench.sheet->items()) {
			if (auto *area = qgraphicsitem_cast<LocationAreaItem *>(item)) reopened = area;
		}
		REQUIRE(reopened != nullptr);
		CHECK(reopened->maskColor() == chosen);
		CHECK(reopened->isOutsideProject());
		CHECK(bench.project->locationTree().isOutsideProject(field_code));
	}

	SECTION("an area of the project keeps the faint wash")
	{
		LocationTree tree = bench.project->locationTree();
		ProjectLocation field = tree.location(tree.uuidOfPath(field_code));
		field.outside_project = false;
		REQUIRE(tree.update(field));
		bench.project->setLocationTree(tree);
		CHECK_FALSE(region->isOutsideProject());
		CHECK(region->fillColor() != LocationAreaItem::defaultMaskColor());
		CHECK(region->fillColor().alpha() < LocationAreaItem::defaultMaskColor().alpha());
		CHECK(region->captionText() == QStringLiteral("+") + field_code);
	}
}

TEST_CASE("CU-85.6 - What is in the field region leaves every list", "[T85][fieldregion]")
{
	Bench bench;
	REQUIRE(bench.open());

		//Before: six components, three wires
	CHECK(bench.bom() == 6);
	CHECK(bench.layout() == 6);
	CHECK(bench.purchase() == 6);
	CHECK(bench.wiringView() == 3);
	CHECK(bench.wiringCsv() == 3);

	bench.drawRegion();

		//After: B1, B2 and X2:1 out; the two wires to the sensors out
	CHECK(bench.bom() == 3);
	CHECK(bench.layout() == 3);
	CHECK(bench.purchase() == 3);
	CHECK(bench.wiringView() == 1);
	CHECK(bench.wiringCsv() == 1);
	CHECK(bench.project->dataBase()->namelessComponentCount() == 0);

	SECTION("the terminal in the field goes to no strip")
	{
		const auto result = TerminalStripAutoImport::importFreeTerminals(bench.project.project());
		CHECK(result.terminals == 1);
		QStringList names;
		for (TerminalStrip *strip : bench.project->terminalStrip()) names << strip->name();
		CHECK(names == QStringList{QStringLiteral("X1")});
	}

	SECTION("the location back in the project brings everything back")
	{
		LocationTree tree = bench.project->locationTree();
		ProjectLocation field = tree.location(tree.uuidOfPath(field_code));
		field.outside_project = false;
		REQUIRE(tree.update(field));
		bench.project->setLocationTree(tree);
		CHECK(bench.bom() == 6);
		CHECK(bench.purchase() == 6);
		CHECK(bench.wiringView() == 3);
		CHECK(bench.wiringCsv() == 3);
	}
}

TEST_CASE("CU-85.7 - Out of the field region, back in the lists", "[T85][fieldregion]")
{
	Bench bench;
	REQUIRE(bench.open());
	bench.drawRegion();
	REQUIRE(bench.bom() == 3);
	REQUIRE(bench.wiringView() == 1);

	Element *b1 = bench.element(QStringLiteral("B1"));
	REQUIRE(b1 != nullptr);
	const int steps = bench.sheet->undoStack().count();
	bench.dragTo(b1, QPointF(450, 300));

	CHECK(bench.pathOf(QStringLiteral("B1")).isEmpty());
	CHECK(bench.bom() == 4);
	CHECK(bench.wiringView() == 2);
	CHECK(bench.wiringCsv() == 2);
	CHECK(bench.sheet->undoStack().count() == steps + 1);

	bench.sheet->undoStack().undo();
	CHECK(bench.pathOf(QStringLiteral("B1")) == field_code);
	CHECK(bench.bom() == 3);
	CHECK(bench.wiringView() == 1);

	bench.sheet->undoStack().redo();
	CHECK(bench.pathOf(QStringLiteral("B1")).isEmpty());
	CHECK(bench.bom() == 4);
	CHECK(bench.wiringView() == 2);
}

TEST_CASE("T85 - A drawn region, selected, is no cable to the properties dock", "[T85][fieldregion]")
{
	Bench bench;
	REQUIRE(bench.open());
	LocationAreaItem *region = bench.drawRegion();

		//What the dock does when the selection changes: it read the region
		//as a cable part, the two types having the same number, and fell on
		//the cable a region does not have
	bench.sheet->clearSelection();
	region->setSelected(true);
	PropertiesEditorWidget *editor = PropertiesEditorFactory::propertiesEditor(
			bench.sheet->selectedItems(), nullptr, nullptr);
	CHECK(editor == nullptr);
	if (editor) CHECK(QString(editor->metaObject()->className()) != QStringLiteral("CablePropertiesEditorWidget"));
	delete editor;
	CHECK(qgraphicsitem_cast<CablePart *>(static_cast<QGraphicsItem *>(region)) == nullptr);
}

TEST_CASE("T85 - Every item of a folio has a type of its own", "[T85][fieldregion]")
{
		//A type shared by two items makes each switch on item->type() take
		//one for the other - the location area and the cable part of the
		//upstream did, and drawing a region crashed the program
	const QList<std::pair<const char *, int>> types {
		{"Element", Element::Type},
		{"Conductor", Conductor::Type},
		{"Terminal", Terminal::Type},
		{"DiagramTextItem", DiagramTextItem::Type},
		{"IndependentTextItem", IndependentTextItem::Type},
		{"ConductorTextItem", ConductorTextItem::Type},
		{"DiagramImageItem", DiagramImageItem::Type},
		{"QetShapeItem", QetShapeItem::Type},
		{"CrossRefItem", CrossRefItem::Type},
		{"DynamicElementTextItem", DynamicElementTextItem::Type},
		{"TerminalStripItem", TerminalStripItem::Type},
		{"CablePart", CablePart::Type},
		{"LocationAreaItem", LocationAreaItem::Type},
		{"StripFrameItem", StripFrameItem::Type},
		{"QetGraphicsHandlerItem", QetGraphicsHandlerItem::Type},
		{"QetGraphicsTableItem", QetGraphicsTableItem::Type},
		{"QetGraphicsHeaderItem", QetGraphicsHeaderItem::Type},
		{"MountingLayoutViewItem", MountingLayoutViewItem::Type},
		{"MountedPartItem", MountedPartItem::Type},
		{"MountedProfileItem", MountedProfileItem::Type}};
	QStringList shared;
	for (int i = 0 ; i < types.size() ; ++i) {
		for (int j = i + 1 ; j < types.size() ; ++j) {
			if (types.at(i).second == types.at(j).second) {
				shared << QStringLiteral("%1 = %2").arg(QLatin1String(types.at(i).first),
									QLatin1String(types.at(j).first));
			}
		}
	}
	CHECK(shared == QStringList());
}
