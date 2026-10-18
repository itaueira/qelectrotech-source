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
#include "uibench.h"

#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/ElementsCollection/embeddedelementrefresh.h"
#include "../../../../sources/editor/UndoCommand/addpartcommand.h"
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/graphicspart/partline.h"
#include "../../../../sources/editor/graphicspart/partterminal.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QByteArray>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QIODevice>
#include <QLatin1String>
#include <QLineF>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUuid>

#include <memory>

/*
	A symbol whose drawing does not hold the point (0, 0), its insertion
	point on a folio, opened in the symbol editor and saved with no change.

	The editor moved such a drawing to the origin on every save
	(ElementScene::toXml(), centerElementToOrigin()), its terminals with it:
	saved untouched, the symbol came back with its terminals somewhere else
	relative to the insertion point, every element placed from it jumped on
	its folio, and the project asked to be reloaded for an edit nobody made.
	Symbols of the collection the program ships with are drawn so, and so
	can be any symbol of a user.

	Unchanged, the drawing now stays where its file has it, and the box
	written around it is widened to the insertion point. Changed - an edit
	on the undo stack - or drawn from nothing, it is moved to the origin as
	before: the cases prove both sides, so that a save that moves nothing
	cannot pass for one that moves the right symbols only.

	Two symbols, one drawn wholly left of the insertion point and one wholly
	right of it. The box is the other half of the defect, and each fails it
	its own way: computed from the drawing alone, the first one's hotspot
	comes out past its width - which Element::setHotspot() cuts down, the
	box then leaving part of the drawing out - and the second one's comes
	out negative, the insertion point outside the box.

	A copy is written for the editor to open, and not the file of the
	collection: a read-only file opens the editor's read-only warning, a
	modal box nothing here could close.
*/

namespace
{
	/// Symbols of the collection the program ships with whose drawing does
	/// not hold the point (0, 0): the first wholly left of it, the second
	/// wholly right of it
	const QStringList off_origin = {
		QStringLiteral("10_electric/91_en_60617/en_60617_06/en_60617_06_09/en_60617_06_09_11.elmt"),
		QStringLiteral("10_electric/20_manufacturers_articles/siemens/01_PLC_controllers/"
					   "6es7-1200/s71200_cpu_1211_ac_dc_rel_03.elmt")};

	/// Where a placed copy lives in the project of projectXml()
	const QString embedded_path = QStringLiteral("import/sample.elmt");

	QByteArray fileContent(const QString &path)
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		return file.readAll();
	}

	QByteArray collectionSymbol(const QString &relative_path)
	{
		const QByteArray bytes = fileContent(
			QDir(QStringLiteral(QET_EXAMPLES_DIR "/../elements")).absoluteFilePath(relative_path));
		REQUIRE_FALSE(bytes.isEmpty());
		return bytes;
	}

	QDomDocument definitionOf(const QByteArray &content)
	{
		QDomDocument document;
		REQUIRE(document.setContent(content));
		REQUIRE(document.documentElement().tagName() == QLatin1String("definition"));
		return document;
	}

	QString writtenCopy(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
	{
		const QString path = dir.filePath(name);
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
		REQUIRE(file.write(content) == content.size());
		return path;
	}

	/// A terminal as it is compared here: its uuid, and its place to the
	/// hundredth, as the editor writes it (TerminalData::toXml())
	QString terminalAt(const QString &uuid, const QPointF &place)
	{
		return QStringLiteral("%1 at %2, %3").arg(QUuid::fromString(uuid).toString(),
							 QString::number(place.x(), 'f', 2),
							 QString::number(place.y(), 'f', 2));
	}

	/// The terminals of @a definition, moved by @a offset, sorted
	QStringList terminalsOf(const QDomElement &definition, const QPointF &offset = QPointF())
	{
		QStringList terminals;
		for (QDomElement terminal = definition.firstChildElement(QStringLiteral("description"))
										   .firstChildElement(QStringLiteral("terminal"));
		     !terminal.isNull();
		     terminal = terminal.nextSiblingElement(QStringLiteral("terminal")))
		{
			const QPointF place(terminal.attribute(QStringLiteral("x")).toDouble(),
					    terminal.attribute(QStringLiteral("y")).toDouble());
			terminals << terminalAt(terminal.attribute(QStringLiteral("uuid")), place + offset);
		}
		terminals.sort();
		return terminals;
	}

	/// The box of a definition: what Element::buildFromXml() reads
	struct Box
	{
		int width = 0;
		int height = 0;
		int hotspot_x = 0;
		int hotspot_y = 0;
	};

	Box boxOf(const QDomElement &definition)
	{
		Box box;
		bool ok[4] = {false, false, false, false};
		box.width     = definition.attribute(QStringLiteral("width")).toInt(&ok[0]);
		box.height    = definition.attribute(QStringLiteral("height")).toInt(&ok[1]);
		box.hotspot_x = definition.attribute(QStringLiteral("hotspot_x")).toInt(&ok[2]);
		box.hotspot_y = definition.attribute(QStringLiteral("hotspot_y")).toInt(&ok[3]);
		REQUIRE((ok[0] && ok[1] && ok[2] && ok[3]));
		return box;
	}

	/**
		Checks that @a box holds the insertion point - so that
		Element::setHotspot() keeps the hotspot as it is written - and
		@a drawing, to the rounding of the box to whole units
	*/
	void checkBoxHolds(const Box &box, const QRectF &drawing)
	{
		INFO("box " << box.width << " x " << box.height << ", hotspot "
		     << box.hotspot_x << ", " << box.hotspot_y);
		CHECK(box.hotspot_x >= 0);
		CHECK(box.hotspot_x <= box.width);
		CHECK(box.hotspot_y >= 0);
		CHECK(box.hotspot_y <= box.height);
		CHECK(-box.hotspot_x <= drawing.left() + 1);
		CHECK(box.width - box.hotspot_x >= drawing.right() - 1);
		CHECK(-box.hotspot_y <= drawing.top() + 1);
		CHECK(box.height - box.hotspot_y >= drawing.bottom() - 1);
	}

	/// What EmbeddedElementRefresh::needingReload() asks of an element
	/// placed from @a original, the symbol being saved as @a saved
	bool sameTerminals(const QDomElement &original, const QDomElement &saved)
	{
		std::unique_ptr<Element> element(Element::buildDetached(original));
		REQUIRE(element.get() != nullptr);
		REQUIRE_FALSE(element->symbolBase().isNull());
		return element->definitionTerminalsMatch(saved);
	}

	/// The offset ElementScene::centerElementToOrigin() moves @a drawing by
	QPointF offsetToOrigin(const QRectF &drawing)
	{
		return QPointF(-qRound(drawing.center().x()), -qRound(drawing.center().y()));
	}

	/**
		A line across the middle of the drawing of @a scene, added the way
		the editor adds one: pushed on the undo stack (ESEventAddLine). It
		stays inside the drawing, so that the drawing still does not hold
		the origin and is moved by the same offset.
	*/
	void addLineInside(QETElementEditor &editor, ElementScene *scene)
	{
		const QRectF drawing = scene->elementSceneGeometricRect();
		const QPointF middle = drawing.center();
		const QPointF half_way(drawing.width() / 4, 0);
		auto *line = new PartLine(&editor);
		line->setLine(QLineF(middle - half_way, middle + half_way));
		scene->undoStack().push(new AddPartCommand(QStringLiteral("line"), scene, line));
		REQUIRE(scene->elementSceneGeometricRect() == drawing);
	}

	/// A project holding @a definition as its copy at embedded_path, placed once
	QString projectXml(const QDomDocument &definition)
	{
			//A document of its own, which carries nothing before the
			//definition: the file of the collection may start with an XML
			//declaration, which cannot stand inside the project
		QDomDocument alone;
		alone.appendChild(alone.importNode(definition.documentElement(), true));
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"import\"><element name=\"sample.elmt\">")
			+ alone.toString(-1)
			+ QStringLiteral(
			"</element></category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"600\""
			" cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\"><elements>"
			"<element x=\"300\" y=\"300\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://import/sample.elmt\""
			" uuid=\"{5a7e0000-0000-4000-8000-000000000001}\">"
			"<terminals/><inputs/><elementInformations/><dynamic_texts/><texts_groups/>"
			"</element></elements><inputs/><conductors/></diagram></project>");
	}
}

TEST_CASE("A symbol off the origin, saved untouched: its terminals stay put",
	  "[elementeditor][elementsaveorigin][ui]")
{
	const QString symbol = GENERATE(from_range(off_origin));
	CAPTURE(symbol);
	const QByteArray source = collectionSymbol(symbol);
	const QDomDocument original = definitionOf(source);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QETElementEditor editor;
	editor.fromFile(writtenCopy(dir, QStringLiteral("symbol.elmt"), source));
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);

		//What the case is about: a drawing off the origin, which the editor
		//moved, and nothing changed since it was read
	const QRectF drawing = scene->elementSceneGeometricRect();
	REQUIRE_FALSE(drawing.contains(0, 0));
	REQUIRE(offsetToOrigin(drawing) != QPointF());
	REQUIRE(scene->undoStack().isClean());
	REQUIRE_FALSE(terminalsOf(original.documentElement()).isEmpty());

	const QString saved_path = dir.filePath(QStringLiteral("saved.elmt"));
	REQUIRE(editor.toFile(saved_path));
	const QDomDocument saved = definitionOf(fileContent(saved_path));

		//Every terminal where the file has it, relative to the insertion
		//point, and the drawing not moved in the editor either
	CHECK(terminalsOf(saved.documentElement()) == terminalsOf(original.documentElement()));
	CHECK(scene->elementSceneGeometricRect() == drawing);
	CHECK(sameTerminals(original.documentElement(), saved.documentElement()));

		//The box holds the insertion point and the drawing
	checkBoxHolds(boxOf(saved.documentElement()), drawing);
}

TEST_CASE("A symbol off the origin, changed: still moved to the origin",
	  "[elementeditor][elementsaveorigin][ui]")
{
	const QString symbol = GENERATE(from_range(off_origin));
	CAPTURE(symbol);
	const QByteArray source = collectionSymbol(symbol);
	const QDomDocument original = definitionOf(source);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QETElementEditor editor;
	editor.fromFile(writtenCopy(dir, QStringLiteral("symbol.elmt"), source));
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);

	addLineInside(editor, scene);
	REQUIRE_FALSE(scene->undoStack().isClean());
	const QRectF drawing = scene->elementSceneGeometricRect();
	REQUIRE_FALSE(drawing.contains(0, 0));
	const QPointF offset = offsetToOrigin(drawing);
	REQUIRE(offset != QPointF());

	const QString saved_path = dir.filePath(QStringLiteral("saved.elmt"));
	REQUIRE(editor.toFile(saved_path));
	const QDomDocument saved = definitionOf(fileContent(saved_path));

		//Every terminal moved with the drawing, by the offset that brings
		//the middle of the drawing to the origin - as the editor always did
	CHECK(terminalsOf(saved.documentElement())
	      == terminalsOf(original.documentElement(), offset));
	CHECK(scene->elementSceneGeometricRect().contains(0, 0));
		//And the comparison the project makes sees it: what the first case
		//finds equal is not equal by construction
	CHECK_FALSE(sameTerminals(original.documentElement(), saved.documentElement()));
	checkBoxHolds(boxOf(saved.documentElement()), scene->elementSceneGeometricRect());
}

TEST_CASE("A new symbol drawn off the origin: moved to the origin",
	  "[elementeditor][elementsaveorigin][ui]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QETElementEditor editor;
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	REQUIRE(scene->undoStack().isClean());

		//A line and a terminal, away from the origin, added as the editor
		//adds them (ESEventAddLine, ESEventAddTerminal)
	auto *line = new PartLine(&editor);
	line->setLine(QLineF(QPointF(100, 80), QPointF(140, 80)));
	scene->undoStack().push(new AddPartCommand(QStringLiteral("line"), scene, line));
	auto *terminal = new PartTerminal(&editor);
	terminal->setPos(QPointF(100, 80));
	scene->undoStack().push(new AddPartCommand(QStringLiteral("terminal"), scene, terminal));
	const QString uuid = terminal->uuid().toString();

	const QRectF drawing = scene->elementSceneGeometricRect();
	REQUIRE_FALSE(drawing.contains(0, 0));
	const QPointF offset = offsetToOrigin(drawing);
	REQUIRE(offset != QPointF());

	const QString saved_path = dir.filePath(QStringLiteral("new.elmt"));
	REQUIRE(editor.toFile(saved_path));
	const QDomDocument saved = definitionOf(fileContent(saved_path));

	CHECK(terminalsOf(saved.documentElement())
	      == QStringList{terminalAt(uuid, QPointF(100, 80) + offset)});
	CHECK(scene->elementSceneGeometricRect().contains(0, 0));
	checkBoxHolds(boxOf(saved.documentElement()), scene->elementSceneGeometricRect());
}

TEST_CASE("A placed symbol off the origin, saved untouched: no reload asked",
	  "[elementeditor][elementsaveorigin][ui]")
{
	const QString symbol = GENERATE(from_range(off_origin));
	CAPTURE(symbol);
	const QDomDocument original = definitionOf(collectionSymbol(symbol));

	UiBench::ScratchProject bench(projectXml(original));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	const QList<Element *> placed = EmbeddedElementRefresh::placedElements(project, embedded_path);
	REQUIRE(placed.size() == 1);
	const ElementsLocation location(embedded_path, project);
	REQUIRE(location.exist());
		//The terminals of the copy as the project holds it, which is what
		//the placed element was built from
	const QStringList before = terminalsOf(location.xml());
	REQUIRE_FALSE(before.isEmpty());

	QETElementEditor editor;
	editor.fromLocation(location);
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	REQUIRE_FALSE(scene->elementSceneGeometricRect().contains(0, 0));

	SECTION("untouched: the terminals match, nothing to reload")
	{
		REQUIRE(scene->undoStack().isClean());
		REQUIRE(editor.toLocation(location));
		const QDomElement saved = location.xml();
		REQUIRE_FALSE(saved.isNull());

		CHECK(terminalsOf(saved) == before);
		CHECK(placed.first()->definitionTerminalsMatch(saved));
		CHECK(EmbeddedElementRefresh::needingReload(project, {}, {}, embedded_path).isEmpty());
	}

	SECTION("changed: moved, and the reload is asked")
	{
			//What makes the first section able to fail: the same save, with
			//a change, gives the comparison something to see
		addLineInside(editor, scene);
		REQUIRE(editor.toLocation(location));
		const QDomElement saved = location.xml();
		REQUIRE_FALSE(saved.isNull());

		CHECK(terminalsOf(saved) != before);
		CHECK_FALSE(placed.first()->definitionTerminalsMatch(saved));
		CHECK(EmbeddedElementRefresh::needingReload(project, {}, {}, embedded_path)
		      == QStringList{EmbeddedElementRefresh::describe(placed.first())});
	}
}
