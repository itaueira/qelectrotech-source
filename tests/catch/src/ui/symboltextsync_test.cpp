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

#include "../../../../sources/ElementsCollection/elementorigin.h"
#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/ElementsCollection/embeddedelementrefresh.h"
#include "../../../../sources/ElementsCollection/xmlelementcollection.h"
#include "../../../../sources/NameList/nameslist.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/symboltextsync.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/syncsymboltextscommand.h"

#include <catch2/catch.hpp>

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QHash>
#include <QLineF>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUuid>

#include <cmath>
#include <memory>

/*
	The elements of a project brought in line with their symbol, without
	removing anything (T58).

	A placed element keeps the texts its symbol gave it when it was placed;
	these cases change the symbol - a text added, changed, removed, a
	terminal renamed, another kind of element - and check what the placed
	elements show afterwards, through the same functions the button
	"Reload the drawings of the elements" and "Reset the texts of the
	symbol" call. And the survey that decides what to bring: a change that
	keeps the uuid, a symbol changed in the project and in the collection, a
	symbol moved to another directory, a copy placed nowhere.

	Every case runs on two examples, as for the T45: the copies of 741.qet
	carry no uuid, those of schema_unifilaire_voltaique2.qet all do.
*/

namespace
{
	const QStringList examples = {QStringLiteral("741.qet"),
								  QStringLiteral("schema_unifilaire_voltaique2.qet")};
	const QString import_prefix = QStringLiteral("import/");
	const QString small_font = QStringLiteral("Sans Serif,9,-1,5,50,0,0,0,0,0");

		/// The path under import/ of a symbol with terminals placed the most
		/// often, at least twice
	QString busiestPath(QETProject *project)
	{
		QHash<QString, int> placed;
		for (Diagram *diagram : project->diagrams()) {
			for (Element *element : DiagramContent(diagram, false).m_elements) {
				const QString path = element->location().collectionPath(false);
				if (path.startsWith(import_prefix) && !element->terminals().isEmpty()) {
					++placed[path];
				}
			}
		}
		QString best;
		for (auto it = placed.cbegin() ; it != placed.cend() ; ++it) {
			if (it.value() >= 2 && (best.isEmpty() || it.value() > placed.value(best))) {
				best = it.key();
			}
		}
		return best;
	}

	Element *elementWithUuid(QETProject *project, const QUuid &uuid)
	{
		for (Diagram *diagram : project->diagrams()) {
			for (Element *element : DiagramContent(diagram, false).m_elements) {
				if (element->uuid() == uuid) {
					return element;
				}
			}
		}
		return nullptr;
	}

	int conductorCount(QETProject *project)
	{
		int count = 0;
		for (Diagram *diagram : project->diagrams()) {
			count += DiagramContent(diagram, false).conductors().size();
		}
		return count;
	}

	QDomDocument documentOf(const QDomElement &element)
	{
		QDomDocument document;
		document.appendChild(document.importNode(element, true));
		return document;
	}

	/// The project's copy of the symbol at @p path, in a document of its own
	QDomDocument copyAt(QETProject *project, const QString &path)
	{
		return documentOf(ElementsLocation(path, project).xml());
	}

	QDomElement descriptionOf(const QDomDocument &definition)
	{
		return definition.documentElement().firstChildElement(QStringLiteral("description"));
	}

	/// What the symbol editor does on saving: a new uuid for the symbol only
	void newRevision(QDomDocument &definition)
	{
		QDomElement root = definition.documentElement();
		QDomElement uuid = root.firstChildElement(QStringLiteral("uuid"));
		if (uuid.isNull()) {
			uuid = definition.createElement(QStringLiteral("uuid"));
			root.insertBefore(uuid, root.firstChild());
		}
		uuid.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
	}

	/// A text of the symbol, as the symbol editor writes one
	void addText(QDomDocument &definition, const QUuid &uuid, const QString &from,
				 const QString &value, qreal x, qreal y, const QString &font = small_font)
	{
		QDomElement text = definition.createElement(QStringLiteral("dynamic_text"));
		text.setAttribute(QStringLiteral("uuid"), uuid.toString());
		text.setAttribute(QStringLiteral("x"), x);
		text.setAttribute(QStringLiteral("y"), y);
		text.setAttribute(QStringLiteral("z"), 5);
		text.setAttribute(QStringLiteral("rotation"), 0);
		text.setAttribute(QStringLiteral("frame"), QStringLiteral("false"));
		text.setAttribute(QStringLiteral("text_width"), -1);
		text.setAttribute(QStringLiteral("font"), font);
		text.setAttribute(QStringLiteral("keep_visual_rotation"), QStringLiteral("true"));
		text.setAttribute(QStringLiteral("rotation_point_center"), QStringLiteral("false"));
		text.setAttribute(QStringLiteral("text_from"), from);
		text.setAttribute(QStringLiteral("Halignment"), QStringLiteral("AlignLeft"));
		text.setAttribute(QStringLiteral("Valignment"), QStringLiteral("AlignTop"));
		QDomElement content = definition.createElement(QStringLiteral("text"));
		if (from == QLatin1String("UserText")) {
			content.appendChild(definition.createTextNode(value));
		}
		text.appendChild(content);
		if (from == QLatin1String("ElementInfo"))
		{
			QDomElement info = definition.createElement(QStringLiteral("info_name"));
			info.appendChild(definition.createTextNode(value));
			text.appendChild(info);
		}
		descriptionOf(definition).appendChild(text);
	}

	QDomElement textOf(const QDomDocument &definition, const QUuid &uuid)
	{
		for (QDomElement text = descriptionOf(definition).firstChildElement(QStringLiteral("dynamic_text"));
			 !text.isNull();
			 text = text.nextSiblingElement(QStringLiteral("dynamic_text"))) {
			if (QUuid(text.attribute(QStringLiteral("uuid"))) == uuid) {
				return text;
			}
		}
		return QDomElement();
	}

	void setUserText(QDomElement text, const QString &value)
	{
		QDomElement content = text.firstChildElement(QStringLiteral("text"));
		while (content.hasChildNodes()) {
			content.removeChild(content.firstChild());
		}
		content.appendChild(content.ownerDocument().createTextNode(value));
	}

	QDomElement firstTerminal(const QDomDocument &definition)
	{
		return descriptionOf(definition).firstChildElement(QStringLiteral("terminal"));
	}

	DynamicElementTextItem *textFrom(const Element *element, const QUuid &symbol_text)
	{
		for (DynamicElementTextItem *text : element->allDynamicTextItems()) {
			if (text->symbolTextUuid() == symbol_text) {
				return text;
			}
		}
		return nullptr;
	}

	DynamicElementTextItem *textSaying(const Element *element, const QString &value)
	{
		for (DynamicElementTextItem *text : element->allDynamicTextItems()) {
			if (text->toPlainText() == value) {
				return text;
			}
		}
		return nullptr;
	}

	bool samePlace(const QPointF &a, const QPointF &b)
	{
		return QLineF(a, b).length() < 0.05;
	}

	/// Write @p definition where a collection rooted at @p dir keeps @p relative
	bool writeFile(const QString &dir, const QString &relative, const QDomDocument &definition)
	{
		const QString file_path = QDir(dir).absoluteFilePath(relative);
		if (!QDir().mkpath(QFileInfo(file_path).absolutePath())) {
			return false;
		}
		QFile file(file_path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			return false;
		}
		return file.write(definition.toByteArray()) > 0;
	}

	/// What the "Edit the element" of a folio does: save over the project's copy
	bool saveOverCopy(QETProject *project, const QString &path, const QDomDocument &definition)
	{
		return ElementsLocation(path, project).setXml(definition);
	}

	/// The texts of @p elements brought in line with @p definition, as one Ctrl+Z
	SyncSymbolTextsCommand *sync(QETProject *project, const QList<Element *> &elements,
								 const QDomDocument &definition,
								 SymbolTextSync::Mode mode = SymbolTextSync::Mode::Merge)
	{
		QList<SymbolTextSync::Plan> plans;
		for (Element *element : elements) {
			plans << SymbolTextSync::plan(element, definition.documentElement(), mode);
		}
		auto *command = new SyncSymbolTextsCommand(plans);
		project->undoStack()->push(command);
		return command;
	}

	/// The button, in place: bring and bring the texts in line, one Ctrl+Z
	EmbeddedElementRefresh::Report update(QETProject *project, const QStringList &dirs,
										   const QSet<QString> &taken = QSet<QString>())
	{
		EmbeddedElementRefresh::Report report;
		if (QUndoCommand *command = EmbeddedElementRefresh::updateCommand(project, dirs, taken, &report)) {
			project->undoStack()->push(command);
		}
		return report;
	}

	/// An embedded copy of @p definition at @p path, placed on no folio
	bool addCopy(QETProject *project, const QString &path, const QDomDocument &definition)
	{
		XmlElementCollection *collection = project->embeddedElementCollection();
		const QStringList parts = path.split(QLatin1Char('/'));
		QString parent;
		for (int i = 0 ; i < parts.size() - 1 ; ++i)
		{
			const QString directory = parent.isEmpty() ? parts.at(i) : parent + QLatin1Char('/') + parts.at(i);
			if (collection->directory(directory).isNull()
					&& !collection->createDir(parent, parts.at(i), NamesList())) {
				return false;
			}
			parent = directory;
		}
		if (!collection->addElementDefinition(parent, parts.last(), definition.documentElement())) {
			return false;
		}
		ElementOrigin::stampMissing(collection->root());
		return true;
	}

	EmbeddedElementRefresh::Entry entryFor(const QList<EmbeddedElementRefresh::Entry> &entries,
										   const QString &path)
	{
		for (const auto &entry : entries) {
			if (entry.embedded_path == path) {
				return entry;
			}
		}
		return EmbeddedElementRefresh::Entry();
	}

	/// The project, a symbol placed at least twice, and the two editable collections
	struct Bench
	{
		explicit Bench(const QString &example) :
			project(UiBench::fileContent(UiBench::examplePath(example)), example)
		{
			if (project.isOpen()) {
				path = busiestPath(project.project());
			}
		}

		QStringList dirs() const {return {company.path(), user.path()};}
		QETProject *get() const {return project.project();}
		QList<Element *> placed() const
		{
			return EmbeddedElementRefresh::placedElements(get(), path);
		}
		QString relative() const {return EmbeddedElementRefresh::collectionRelativePath(path);}

		UiBench::ScratchProject project;
		QTemporaryDir company;
		QTemporaryDir user;
		QString path;
	};

		/// The texts the cases add to the symbol
	const QUuid added_text("{a1a1a1a1-0000-4000-8000-000000000001}");
	const QUuid fixed_text("{a1a1a1a1-0000-4000-8000-000000000002}");
	const QUuid plant_text("{a1a1a1a1-0000-4000-8000-000000000003}");

	/// The copy of the bench's symbol with a fixed text "NEW" and a fixed text
	/// "VD", as a new revision
	QDomDocument withTwoTexts(const Bench &bench)
	{
		QDomDocument definition = copyAt(bench.get(), bench.path);
		addText(definition, added_text, QStringLiteral("UserText"), QStringLiteral("NEW"), 5, 5);
		addText(definition, fixed_text, QStringLiteral("UserText"), QStringLiteral("VD"), 5, -27);
		newRevision(definition);
		return definition;
	}
}

TEST_CASE("CU-58.1 — A text added to the symbol appears on every placed element, and Ctrl+Z takes it off",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	const QList<Element *> placed = bench.placed();
	REQUIRE(placed.size() >= 2);

	const QDomDocument symbol = withTwoTexts(bench);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), symbol));

	const EmbeddedElementRefresh::Report report = update(project, bench.dirs());
	CHECK(report.brought_symbols == 1);
	CHECK(report.added_texts >= 2 * placed.size());
	for (Element *element : placed)
	{
		DynamicElementTextItem *text = textFrom(element, added_text);
		REQUIRE(text);
		CHECK(text->toPlainText() == QStringLiteral("NEW"));
		CHECK(samePlace(text->pos(), QPointF(5, 5)));
		CHECK(textFrom(element, fixed_text));
	}

	project->undoStack()->undo();
	for (Element *element : placed) {
		CHECK_FALSE(textFrom(element, added_text));
	}
	CHECK(ElementOrigin::contentHash(copyAt(project, bench.path).documentElement())
		  != ElementOrigin::contentHash(symbol.documentElement()));

	project->undoStack()->redo();
	for (Element *element : placed) {
		CHECK(textFrom(element, added_text));
	}
}

TEST_CASE("CU-58.1 and CU-58.2 — On an element turned on its folio, a text added or moved reads as its others do",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();

	const QDomDocument first = withTwoTexts(bench);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), first));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);

		//Turned a quarter after its texts were given: they keep reading
		//horizontally, so their own angle is the opposite of the element's
	Element *element = bench.placed().first();
	element->setRotation(element->rotation() + 90);
	DynamicElementTextItem *moved = textFrom(element, added_text);
	REQUIRE(moved);
	REQUIRE(moved->keepVisualRotation());
	auto readsHorizontally = [element](const DynamicElementTextItem *text) {
		return qAbs(std::remainder(text->rotation() + element->rotation(), 360.0)) < 0.01;
	};
	REQUIRE(readsHorizontally(moved));

	QDomDocument second = documentOf(first.documentElement());
	textOf(second, added_text).setAttribute(QStringLiteral("x"), 15);
	addText(second, plant_text, QStringLiteral("UserText"), QStringLiteral("PLANT"), -20, 12);
	newRevision(second);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), second));
	update(project, bench.dirs());

		//Not a change of its own: turning the element is not touching the text
	CHECK(samePlace(moved->pos(), QPointF(15, 5)));
	CHECK(readsHorizontally(moved));
	DynamicElementTextItem *added = textFrom(element, plant_text);
	REQUIRE(added);
	CHECK(samePlace(added->pos(), QPointF(-20, 12)));
	CHECK(readsHorizontally(added));
}

TEST_CASE("CU-58.2 and CU-58.3 — A text changed in the symbol changes on the elements, but what was changed on one stays",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	const QList<Element *> placed = bench.placed();
	REQUIRE(placed.size() >= 2);

	const QDomDocument first = withTwoTexts(bench);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), first));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);

		//On the folio: one element's "NEW" moved, its "VD" rewritten
	Element *touched = placed.first();
	DynamicElementTextItem *moved = textFrom(touched, added_text);
	DynamicElementTextItem *rewritten = textFrom(touched, fixed_text);
	REQUIRE(moved);
	REQUIRE(rewritten);
	moved->setPos(moved->pos() + QPointF(20, 0));
	rewritten->setText(QStringLiteral("VM"));
	const QPointF moved_anchor = SymbolTextSync::anchorOf(moved);

		//In the symbol: "NEW" says "CHANGED", ten units to the right, in 12;
		//"VD" ten units lower
	QDomDocument second = documentOf(first.documentElement());
	QDomElement new_text = textOf(second, added_text);
	setUserText(new_text, QStringLiteral("CHANGED"));
	new_text.setAttribute(QStringLiteral("x"), 15);
	new_text.setAttribute(QStringLiteral("font"), QStringLiteral("Sans Serif,12,-1,5,50,0,0,0,0,0"));
	textOf(second, fixed_text).setAttribute(QStringLiteral("y"), -17);
	newRevision(second);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), second));

	const EmbeddedElementRefresh::Report report = update(project, bench.dirs());
	CHECK(report.brought_symbols == 1);
	CHECK(report.changed_texts >= 2 * placed.size() - 1);

	for (Element *element : placed)
	{
		CAPTURE(element == touched);
		DynamicElementTextItem *text = textFrom(element, added_text);
		DynamicElementTextItem *fixed = textFrom(element, fixed_text);
		REQUIRE(text);
		REQUIRE(fixed);
		CHECK(text->toPlainText() == QStringLiteral("CHANGED"));
		CHECK(text->font().pointSize() == 12);
		CHECK(samePlace(fixed->pos(), QPointF(5, -17)));
		if (element == touched)
		{
				//Moved on the folio: it stays where it was left
			CHECK(samePlace(SymbolTextSync::anchorOf(text), moved_anchor));
				//Rewritten on the folio: its words stay
			CHECK(fixed->toPlainText() == QStringLiteral("VM"));
		}
		else
		{
			CHECK(samePlace(text->pos(), QPointF(15, 5)));
			CHECK(fixed->toPlainText() == QStringLiteral("VD"));
		}
	}
}

TEST_CASE("CU-58.4 — A text removed from the symbol goes, a text added on a folio stays",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	const QList<Element *> placed = bench.placed();

	const QDomDocument first = withTwoTexts(bench);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), first));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);

	Element *element = placed.first();
	auto *own = new DynamicElementTextItem(element);
	own->setText(QStringLiteral("MINE"));
	element->addDynamicTextItem(own);
	REQUIRE(own->symbolTextUuid().isNull());

	QDomDocument second = documentOf(first.documentElement());
	descriptionOf(second).removeChild(textOf(second, fixed_text));
	newRevision(second);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), second));

	const EmbeddedElementRefresh::Report report = update(project, bench.dirs());
	CHECK(report.removed_texts == placed.size());
	for (Element *placed_element : placed) {
		CHECK_FALSE(textFrom(placed_element, fixed_text));
		CHECK(textFrom(placed_element, added_text));
	}
	CHECK(textSaying(element, QStringLiteral("MINE")) == own);

	project->undoStack()->undo();
	for (Element *placed_element : placed) {
		CHECK(textFrom(placed_element, fixed_text));
	}
}

TEST_CASE("CU-58.5 — A text of the symbol deleted on a folio does not come back, even after reopening",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();

	const QDomDocument symbol = withTwoTexts(bench);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), symbol));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);

	Element *element = bench.placed().first();
	const QUuid element_uuid = element->uuid();
	DynamicElementTextItem *deleted = textFrom(element, added_text);
	REQUIRE(deleted);
	element->removeDynamicTextItem(deleted);
	if (deleted->scene()) {
		deleted->scene()->removeItem(deleted);
	}
	delete deleted;
	CHECK(element->removedSymbolTexts().contains(added_text));

	REQUIRE(bench.project.saveAndReopen());
	project = bench.get();
	element = elementWithUuid(project, element_uuid);
	REQUIRE(element);
	CHECK(element->removedSymbolTexts().contains(added_text));
	CHECK_FALSE(textFrom(element, added_text));
	CHECK(textFrom(element, fixed_text));

	update(project, bench.dirs());
	CHECK_FALSE(textFrom(element, added_text));
	for (Element *other : bench.placed()) {
		if (other != element) {
			CHECK(textFrom(other, added_text));
		}
	}
}

TEST_CASE("CU-58.6 — An element saved before the link: a fixed text rewritten in place is kept, nothing is doubled",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();

	QDomDocument symbol = withTwoTexts(bench);
	addText(symbol, plant_text, QStringLiteral("ElementInfo"), QStringLiteral("plant"), -20, 12);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), symbol));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);

		//As a project saved before texts remembered their symbol text: no
		//link, "VD" rewritten "VM" where it is, and the text of the plant
		//never given
	Element *element = bench.placed().first();
	DynamicElementTextItem *plant = textFrom(element, plant_text);
	REQUIRE(plant);
	element->removeDynamicTextItem(plant);
	if (plant->scene()) {
		plant->scene()->removeItem(plant);
	}
	delete plant;
	textFrom(element, fixed_text)->setText(QStringLiteral("VM"));
	for (DynamicElementTextItem *text : element->allDynamicTextItems()) {
		text->setSymbolTextUuid(QUuid());
	}
	element->setRemovedSymbolTexts({});
	const int texts_before = element->allDynamicTextItems().size();

	const SymbolTextSync::Plan plan =
			SymbolTextSync::plan(element, copyAt(project, bench.path).documentElement(),
								 SymbolTextSync::Mode::Merge);
	CHECK(plan.added.size() == 1);
	sync(project, {element}, copyAt(project, bench.path));

	CHECK(element->allDynamicTextItems().size() == texts_before + 1);
	CHECK(textFrom(element, plant_text));
	DynamicElementTextItem *fixed = textFrom(element, fixed_text);
	REQUIRE(fixed);
	CHECK(fixed->toPlainText() == QStringLiteral("VM"));
	CHECK_FALSE(textSaying(element, QStringLiteral("VD")));
	CHECK(textFrom(element, added_text));
}

TEST_CASE("CU-58.6 — Elements placed from an older symbol: what most share follows it, a text given another role takes its place",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	const QList<Element *> placed = bench.placed();
	REQUIRE(placed.size() >= 3);

	const QUuid role_text("{a1a1a1a1-0000-4000-8000-000000000011}");
	const QUuid place_text("{a1a1a1a1-0000-4000-8000-000000000012}");
	QDomDocument first = copyAt(project, bench.path);
	addText(first, role_text, QStringLiteral("ElementInfo"), QStringLiteral("comment"), 5, -15);
	addText(first, place_text, QStringLiteral("ElementInfo"), QStringLiteral("function"), 7, -4);
	newRevision(first);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), first));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);

		//As elements saved before texts were linked, placed from an older
		//symbol: both texts elsewhere, the same on every element but one,
		//whose texts were moved by hand
	Element *by_hand = placed.first();
	for (Element *element : placed)
	{
		DynamicElementTextItem *role = textFrom(element, role_text);
		DynamicElementTextItem *place = textFrom(element, place_text);
		REQUIRE(role);
		REQUIRE(place);
		role->setPos(element == by_hand ? QPointF(30, -10) : QPointF(4, -10));
		place->setPos(element == by_hand ? QPointF(40, -6) : QPointF(7, -6));
		for (DynamicElementTextItem *text : element->allDynamicTextItems()) {
			text->setSymbolTextUuid(QUuid());
		}
	}

		//The symbol now: the first text is the plant's, elsewhere; the second
		//only moves
	QDomDocument second = documentOf(first.documentElement());
	QDomElement role_definition = textOf(second, role_text);
	QDomElement info = role_definition.firstChildElement(QStringLiteral("info_name"));
	info.removeChild(info.firstChild());
	info.appendChild(second.createTextNode(QStringLiteral("plant")));
	role_definition.setAttribute(QStringLiteral("x"), -20);
	role_definition.setAttribute(QStringLiteral("y"), -12);
	textOf(second, place_text).setAttribute(QStringLiteral("y"), -20);
	newRevision(second);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), second));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);

	for (Element *element : placed)
	{
		CAPTURE(element == by_hand);
		DynamicElementTextItem *role = textFrom(element, role_text);
		DynamicElementTextItem *place = textFrom(element, place_text);
		REQUIRE(role);
		REQUIRE(place);
			//Another text now: where the old one was put says nothing
		CHECK(role->infoName() == QStringLiteral("plant"));
		CHECK(samePlace(role->pos(), QPointF(-20, -12)));
			//The same text moved in the symbol: the place most elements share
			//was the older symbol's, the other one is a hand's
		CHECK(samePlace(place->pos(), element == by_hand ? QPointF(40, -6) : QPointF(7, -20)));
	}
}

TEST_CASE("CU-58.7 — Reset the texts of the symbol: back to the symbol, the values and the own texts kept",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();

	const QDomDocument symbol = withTwoTexts(bench);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), symbol));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);

	Element *element = bench.placed().first();
	DynamicElementTextItem *moved = textFrom(element, added_text);
	DynamicElementTextItem *rewritten = textFrom(element, fixed_text);
	REQUIRE(moved);
	REQUIRE(rewritten);
	moved->setPos(moved->pos() + QPointF(20, 0));
	rewritten->setText(QStringLiteral("VM"));
	auto *own = new DynamicElementTextItem(element);
	own->setText(QStringLiteral("MINE"));
	element->addDynamicTextItem(own);
	const QString label = element->elementInformations().value(QStringLiteral("label")).toString();

	SyncSymbolTextsCommand *reset = sync(project, {element}, copyAt(project, bench.path),
										 SymbolTextSync::Mode::Reset);
	CHECK(reset->elements() == 1);
	CHECK(samePlace(moved->pos(), QPointF(5, 5)));
	CHECK(rewritten->toPlainText() == QStringLiteral("VD"));
	CHECK(textSaying(element, QStringLiteral("MINE")) == own);
	CHECK(element->elementInformations().value(QStringLiteral("label")).toString() == label);

	project->undoStack()->undo();
	CHECK(samePlace(moved->pos(), QPointF(25, 5)));
	CHECK(rewritten->toPlainText() == QStringLiteral("VM"));
}

TEST_CASE("CU-58.8 — A terminal renamed or turned in place asks for the reload, which applies it with the wires",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	const QList<Element *> placed = bench.placed();
	Element *element = placed.first();
	const QUuid element_uuid = element->uuid();
	const int wires = conductorCount(project);

	SECTION("renamed")
	{
		QDomDocument renamed = copyAt(project, bench.path);
		REQUIRE_FALSE(firstTerminal(renamed).isNull());
		firstTerminal(renamed).setAttribute(QStringLiteral("name"), QStringLiteral("T58"));
		newRevision(renamed);
		CHECK_FALSE(element->definitionTerminalsMatch(renamed.documentElement()));
		REQUIRE(writeFile(bench.company.path(), bench.relative(), renamed));
		CHECK(entryFor(EmbeddedElementRefresh::survey(project, bench.dirs()), bench.path).outcome
			  == EmbeddedElementRefresh::Outcome::TerminalsChanged);
		CHECK(EmbeddedElementRefresh::needingReload(project, bench.dirs()).size() == placed.size());

		REQUIRE(EmbeddedElementRefresh::bringFromCollections(project, bench.dirs(), true)
				.brought_symbols == 1);
		std::unique_ptr<QETProject> reloaded(EmbeddedElementRefresh::reopened(project));
		REQUIRE(reloaded);
		CHECK(conductorCount(reloaded.get()) == wires);
		Element *rebuilt = elementWithUuid(reloaded.get(), element_uuid);
		REQUIRE(rebuilt);
		QStringList names;
		for (Terminal *terminal : rebuilt->terminals()) {
			names << terminal->name();
		}
		CHECK(names.contains(QStringLiteral("T58")));
	}

	SECTION("turned")
	{
		QDomDocument turned = copyAt(project, bench.path);
		QDomElement terminal = firstTerminal(turned);
		REQUIRE_FALSE(terminal.isNull());
		const QString orientation = terminal.attribute(QStringLiteral("orientation"));
		terminal.setAttribute(QStringLiteral("orientation"),
							  orientation == QLatin1String("n") ? QStringLiteral("s")
																: QStringLiteral("n"));
		newRevision(turned);
		CHECK_FALSE(element->definitionTerminalsMatch(turned.documentElement()));
		REQUIRE(writeFile(bench.company.path(), bench.relative(), turned));
		CHECK(EmbeddedElementRefresh::needingReload(project, bench.dirs()).size() == placed.size());
	}
}

TEST_CASE("CU-58.9 — Another kind of element asks for the reload, and comes back as that kind",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	Element *element = bench.placed().first();
	const bool was_simple = element->linkType() == Element::Simple;
	const QUuid element_uuid = element->uuid();

		//A simple element made a master coil, or the other way round
	QDomDocument master = copyAt(project, bench.path);
	QDomElement root = master.documentElement();
	if (was_simple)
	{
		root.setAttribute(QStringLiteral("link_type"), QStringLiteral("master"));
		QDomElement kind = root.firstChildElement(QStringLiteral("kindInformations"));
		if (kind.isNull()) {
			kind = master.createElement(QStringLiteral("kindInformations"));
			root.appendChild(kind);
		}
		QDomElement type = master.createElement(QStringLiteral("kindInformation"));
		type.setAttribute(QStringLiteral("name"), QStringLiteral("type"));
		type.setAttribute(QStringLiteral("show"), 1);
		type.appendChild(master.createTextNode(QStringLiteral("coil")));
		kind.appendChild(type);
	}
	else {
		root.setAttribute(QStringLiteral("link_type"), QStringLiteral("simple"));
	}
	newRevision(master);

	CHECK(element->definitionTerminalsMatch(master.documentElement()));
	CHECK_FALSE(element->definitionKindMatches(master.documentElement()));
	REQUIRE(writeFile(bench.company.path(), bench.relative(), master));
	CHECK(EmbeddedElementRefresh::needingReload(project, bench.dirs()).contains(
			  EmbeddedElementRefresh::describe(element)));

	REQUIRE(EmbeddedElementRefresh::bringFromCollections(project, bench.dirs(), true)
			.brought_symbols == 1);
	std::unique_ptr<QETProject> reloaded(EmbeddedElementRefresh::reopened(project));
	REQUIRE(reloaded);
	Element *rebuilt = elementWithUuid(reloaded.get(), element_uuid);
	REQUIRE(rebuilt);
	CHECK(rebuilt->linkType() == (was_simple ? Element::Master : Element::Simple));
}

TEST_CASE("CU-58.10 — The texts follow the symbol through a reload too",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	const QUuid element_uuid = bench.placed().first()->uuid();

	QDomDocument symbol = copyAt(project, bench.path);
	firstTerminal(symbol).setAttribute(QStringLiteral("name"), QStringLiteral("T58"));
	addText(symbol, added_text, QStringLiteral("UserText"), QStringLiteral("NEW"), 5, 5);
	newRevision(symbol);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), symbol));

		//What reloading the project from memory does, but the windows
	REQUIRE(EmbeddedElementRefresh::bringFromCollections(project, bench.dirs(), true)
			.brought_symbols == 1);
	auto *texts = new SyncSymbolTextsCommand(EmbeddedElementRefresh::textPlans(project));
	CHECK(texts->addedTexts() >= 1);
	project->undoStack()->push(texts);
	std::unique_ptr<QETProject> reloaded(EmbeddedElementRefresh::reopened(project));
	REQUIRE(reloaded);

	Element *rebuilt = elementWithUuid(reloaded.get(), element_uuid);
	REQUIRE(rebuilt);
	DynamicElementTextItem *text = textFrom(rebuilt, added_text);
	REQUIRE(text);
	CHECK(text->toPlainText() == QStringLiteral("NEW"));
}

TEST_CASE("CU-58.11 — A change that keeps the uuid is brought; a save without a change is not",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();

	SECTION("the content changed, the uuid did not, as the generator of symbols does")
	{
		QDomDocument same_uuid = copyAt(project, bench.path);
		addText(same_uuid, added_text, QStringLiteral("UserText"), QStringLiteral("NEW"), 5, 5);
		REQUIRE(ElementOrigin::definitionUuid(same_uuid.documentElement())
				== ElementOrigin::definitionUuid(copyAt(project, bench.path).documentElement()));
		REQUIRE(writeFile(bench.company.path(), bench.relative(), same_uuid));
		CHECK(entryFor(EmbeddedElementRefresh::survey(project, bench.dirs()), bench.path).outcome
			  == EmbeddedElementRefresh::Outcome::Pending);
		CHECK(update(project, bench.dirs()).brought_symbols == 1);
		CHECK(textFrom(bench.placed().first(), added_text));
	}

	SECTION("saved again without a change: a new uuid, the same content")
	{
		QDomDocument resaved = copyAt(project, bench.path);
		newRevision(resaved);
		REQUIRE(writeFile(bench.company.path(), bench.relative(), resaved));
		CHECK(entryFor(EmbeddedElementRefresh::survey(project, bench.dirs()), bench.path).outcome
			  == EmbeddedElementRefresh::Outcome::UpToDate);
	}
}

TEST_CASE("CU-58.12 — Changed in the project and in the collection: the collection's version when asked for",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();

	QDomDocument in_project = copyAt(project, bench.path);
	addText(in_project, plant_text, QStringLiteral("UserText"), QStringLiteral("PROJECT"), 0, 0);
	newRevision(in_project);
	REQUIRE(saveOverCopy(project, bench.path, in_project));

	QDomDocument in_collection = withTwoTexts(bench);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), in_collection));

	CHECK(EmbeddedElementRefresh::changedInBoth(project, bench.dirs()) == QStringList{bench.path});
	const EmbeddedElementRefresh::Report kept =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs());
	CHECK(kept.brought_symbols == 0);
	CHECK(kept.changed_in_project.size() == 1);

	const EmbeddedElementRefresh::Report taken =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs(), false,
														 {bench.path});
	CHECK(taken.brought_symbols == 1);
	CHECK(ElementOrigin::contentHash(copyAt(project, bench.path).documentElement())
		  == ElementOrigin::contentHash(in_collection.documentElement()));
	CHECK(EmbeddedElementRefresh::changedInBoth(project, bench.dirs()).isEmpty());

		//Undone: the project's version, and still changed in both
	project->undoStack()->undo();
	CHECK(ElementOrigin::contentHash(copyAt(project, bench.path).documentElement())
		  == ElementOrigin::contentHash(in_project.documentElement()));
	CHECK(EmbeddedElementRefresh::changedInBoth(project, bench.dirs()) == QStringList{bench.path});

		//The same change made on both sides is no conflict: nothing to choose
	QDomDocument same_change = documentOf(in_project.documentElement());
	newRevision(same_change);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), same_change));
	CHECK(EmbeddedElementRefresh::changedInBoth(project, bench.dirs()).isEmpty());
	CHECK(entryFor(EmbeddedElementRefresh::survey(project, bench.dirs()), bench.path).outcome
		  == EmbeddedElementRefresh::Outcome::UpToDate);
}

TEST_CASE("CU-58.13 — A symbol moved to another directory is found, a doubled import/ too",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	QTemporaryDir common;

		//A copy whose symbol was at "old" and is now at "new"
	const QString moved_path = QStringLiteral("import/t58_old/t58_symbol.elmt");
	QDomDocument old_version = copyAt(project, bench.path);
	REQUIRE_FALSE(firstTerminal(old_version).attribute(QStringLiteral("uuid")).isEmpty());
	REQUIRE(addCopy(project, moved_path, old_version));
	QDomDocument new_version = documentOf(old_version.documentElement());
	addText(new_version, added_text, QStringLiteral("UserText"), QStringLiteral("NEW"), 5, 5);
	newRevision(new_version);

	SECTION("one file of that name sharing its parts: it is the symbol")
	{
		REQUIRE(writeFile(bench.company.path(), QStringLiteral("t58_new/t58_symbol.elmt"), new_version));
		const auto entry = entryFor(EmbeddedElementRefresh::survey(project, bench.dirs(), common.path()),
									moved_path);
		CHECK(entry.moved);
		CHECK(entry.outcome == EmbeddedElementRefresh::Outcome::Pending);
	}

	SECTION("two of them: nothing is guessed")
	{
		REQUIRE(writeFile(bench.company.path(), QStringLiteral("t58_new/t58_symbol.elmt"), new_version));
		REQUIRE(writeFile(bench.user.path(), QStringLiteral("t58_other/t58_symbol.elmt"), new_version));
		CHECK(entryFor(EmbeddedElementRefresh::survey(project, bench.dirs(), common.path()), moved_path)
			  .outcome == EmbeddedElementRefresh::Outcome::Ambiguous);
	}

	SECTION("a symbol of the QElectroTech collection is never taken for one moved")
	{
		REQUIRE(writeFile(common.path(), QStringLiteral("t58_old/t58_symbol.elmt"), old_version));
		REQUIRE(writeFile(bench.company.path(), QStringLiteral("t58_new/t58_symbol.elmt"), new_version));
		CHECK(entryFor(EmbeddedElementRefresh::survey(project, bench.dirs(), common.path()), moved_path)
			  .outcome == EmbeddedElementRefresh::Outcome::NoSource);
	}

	SECTION("a copy with import/ twice is looked for without them")
	{
		const QString doubled = QStringLiteral("import/import/t58_dir/t58_doubled.elmt");
		REQUIRE(addCopy(project, doubled, old_version));
		REQUIRE(writeFile(bench.company.path(), QStringLiteral("t58_dir/t58_doubled.elmt"), new_version));
		const auto entry = entryFor(EmbeddedElementRefresh::survey(project, bench.dirs(), common.path()),
									doubled);
		CHECK_FALSE(entry.moved);
		CHECK(entry.outcome == EmbeddedElementRefresh::Outcome::Pending);
	}
}

TEST_CASE("CU-58.14 — A copy placed on no folio is brought too",
		  "[t58][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();

	const QString unplaced = QStringLiteral("import/t58_kept/t58_unplaced.elmt");
	QDomDocument old_version = copyAt(project, bench.path);
	REQUIRE(addCopy(project, unplaced, old_version));
	QDomDocument new_version = documentOf(old_version.documentElement());
	addText(new_version, added_text, QStringLiteral("UserText"), QStringLiteral("NEW"), 5, 5);
	newRevision(new_version);
	REQUIRE(writeFile(bench.company.path(), QStringLiteral("t58_kept/t58_unplaced.elmt"), new_version));

	CHECK(EmbeddedElementRefresh::bringFromCollections(project, bench.dirs()).brought_symbols == 1);
	CHECK(ElementOrigin::contentHash(copyAt(project, unplaced).documentElement())
		  == ElementOrigin::contentHash(new_version.documentElement()));
}

namespace
{
		/// The uuid the three texts of the cases of T82 share
	const QUuid repeated_text("{a1a1a1a1-0000-4000-8000-0000000000dd}");

		/// The copy of the bench's symbol with three texts of informations -
		/// the function, the description and the label - sharing one uuid, as
		/// a text copied and pasted in the symbol editor left them (T82)
	QDomDocument withRepeatedTexts(const Bench &bench)
	{
		QDomDocument definition = copyAt(bench.get(), bench.path);
		addText(definition, repeated_text, QStringLiteral("ElementInfo"), QStringLiteral("function"), -40, 2);
		addText(definition, repeated_text, QStringLiteral("ElementInfo"), QStringLiteral("description"), 10, 10);
		addText(definition, repeated_text, QStringLiteral("ElementInfo"), QStringLiteral("label"), -40, -19);
		newRevision(definition);
		return definition;
	}

		/// The texts of the symbol with the uuid of the cases, in order
	QList<QDomElement> repeatedTexts(const QDomDocument &definition)
	{
		QList<QDomElement> texts;
		for (QDomElement text = descriptionOf(definition).firstChildElement(QStringLiteral("dynamic_text"));
			 !text.isNull();
			 text = text.nextSiblingElement(QStringLiteral("dynamic_text"))) {
			if (QUuid(text.attribute(QStringLiteral("uuid"))) == repeated_text) {
				texts << text;
			}
		}
		return texts;
	}

		/// What each placed element shows: its own function, description and
		/// label, so that a text showing another's is seen
	void giveInformations(const QList<Element *> &placed)
	{
		for (int i = 0 ; i < placed.size() ; ++i)
		{
			DiagramContext informations = placed.at(i)->elementInformations();
			informations.remove(QStringLiteral("formula"));
			informations.addValue(QStringLiteral("function"), QStringLiteral("SOPRO%1").arg(i));
			informations.addValue(QStringLiteral("description"), QStringLiteral("DESC%1").arg(i));
			informations.addValue(QStringLiteral("label"), QStringLiteral("TAG%1").arg(i));
			placed.at(i)->setElementInformations(informations);
		}
	}

	int textsSaying(const Element *element, const QString &value)
	{
		int count = 0;
		for (DynamicElementTextItem *text : element->allDynamicTextItems()) {
			if (text->toPlainText() == value) ++count;
		}
		return count;
	}

		/// The three texts placed from the symbol, as the elements placed by
		/// an earlier program kept them: all remembering the repeated uuid
	void rememberRepeated(const QList<Element *> &placed, int i)
	{
		for (const QString &value : {QStringLiteral("SOPRO%1"), QStringLiteral("DESC%1"),
					     QStringLiteral("TAG%1")}) {
			if (DynamicElementTextItem *text = textSaying(placed.at(i), value.arg(i))) {
				text->setSymbolTextUuid(repeated_text);
			}
		}
	}
}

TEST_CASE("CU-82.1 — Texts sharing a uuid keep what they show on the elements",
		  "[t82][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	const bool reset = GENERATE(false, true);
	CAPTURE(reset);
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	const QList<Element *> placed = bench.placed();
	REQUIRE(placed.size() >= 2);
	giveInformations(placed);

	const QDomDocument first = withRepeatedTexts(bench);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), first));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);
	for (int i = 0 ; i < placed.size() ; ++i)
	{
		CAPTURE(i);
		REQUIRE(textsSaying(placed.at(i), QStringLiteral("SOPRO%1").arg(i)) == 1);
		REQUIRE(textsSaying(placed.at(i), QStringLiteral("TAG%1").arg(i)) == 1);
		rememberRepeated(placed, i);
	}

		//The symbol changed: its label drawn larger
	QDomDocument second = documentOf(first.documentElement());
	const QList<QDomElement> texts = repeatedTexts(second);
	REQUIRE(texts.size() == 3);
	QDomElement(texts.at(2)).setAttribute(QStringLiteral("font"), QStringLiteral("Sans Serif,12,-1,5,50,0,0,0,0,0"));
	newRevision(second);
	if (reset) {
		REQUIRE(saveOverCopy(project, bench.path, second));
		sync(project, placed, second, SymbolTextSync::Mode::Reset);
	} else {
		REQUIRE(writeFile(bench.company.path(), bench.relative(), second));
		update(project, bench.dirs());
	}

	for (int i = 0 ; i < placed.size() ; ++i)
	{
		const Element *element = placed.at(i);
		CAPTURE(i);
			//Each text still shows its own information, once
		CHECK(textsSaying(element, QStringLiteral("SOPRO%1").arg(i)) == 1);
		CHECK(textsSaying(element, QStringLiteral("DESC%1").arg(i)) == 1);
		CHECK(textsSaying(element, QStringLiteral("TAG%1").arg(i)) == 1);
			//The label took the change, and the function did not
		DynamicElementTextItem *label = textSaying(element, QStringLiteral("TAG%1").arg(i));
		DynamicElementTextItem *function = textSaying(element, QStringLiteral("SOPRO%1").arg(i));
		REQUIRE((label && function));
		CHECK(label->font().pointSize() == 12);
		CHECK(function->font().pointSize() == 9);
			//And each remembers a symbol text of its own
		DynamicElementTextItem *description = textSaying(element, QStringLiteral("DESC%1").arg(i));
		REQUIRE(description);
		CHECK(label->symbolTextUuid() != function->symbolTextUuid());
		CHECK(label->symbolTextUuid() != description->symbolTextUuid());
		CHECK(function->symbolTextUuid() != description->symbolTextUuid());
	}
}

TEST_CASE("CU-82.2 — The symbol given a uuid per text keeps the elements placed before",
		  "[t82][symboltextsync]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE_FALSE(bench.path.isEmpty());
	QETProject *project = bench.get();
	const QList<Element *> placed = bench.placed();
	REQUIRE(placed.size() >= 2);
	giveInformations(placed);

	const QDomDocument first = withRepeatedTexts(bench);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), first));
	REQUIRE(update(project, bench.dirs()).brought_symbols == 1);
	QList<int> before;
	for (int i = 0 ; i < placed.size() ; ++i)
	{
		rememberRepeated(placed, i);
		before << placed.at(i)->allDynamicTextItems().size();
	}

		//The symbol mended: the function and the description a uuid of their
		//own, and the label keeping the one they shared - which every text
		//of the elements still remembers, the function first
	QDomDocument mended = documentOf(first.documentElement());
	const QList<QDomElement> texts = repeatedTexts(mended);
	REQUIRE(texts.size() == 3);
	QDomElement(texts.at(0)).setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
	QDomElement(texts.at(1)).setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
		//And something else changed with it, as when the symbol is edited
		//and saved: uuids alone are no change of the symbol, and would not
		//be brought to the project at all
	QDomElement(texts.at(1)).setAttribute(QStringLiteral("x"), 12);
	newRevision(mended);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), mended));
	update(project, bench.dirs());

	for (int i = 0 ; i < placed.size() ; ++i)
	{
		const Element *element = placed.at(i);
		CAPTURE(i);
		CHECK(element->allDynamicTextItems().size() == before.at(i));
		CHECK(textsSaying(element, QStringLiteral("SOPRO%1").arg(i)) == 1);
		CHECK(textsSaying(element, QStringLiteral("DESC%1").arg(i)) == 1);
		CHECK(textsSaying(element, QStringLiteral("TAG%1").arg(i)) == 1);
	}

		//And the next change of the symbol reaches the right text: a text
		//linked to the wrong one of the symbol would take its change - the
		//function drawn as large as the label
	QDomDocument larger = documentOf(mended.documentElement());
	const QList<QDomElement> label = repeatedTexts(larger);
	REQUIRE(label.size() == 1);
	QDomElement(label.first()).setAttribute(QStringLiteral("font"),
						QStringLiteral("Sans Serif,12,-1,5,50,0,0,0,0,0"));
	newRevision(larger);
	REQUIRE(writeFile(bench.company.path(), bench.relative(), larger));
	update(project, bench.dirs());
	for (int i = 0 ; i < placed.size() ; ++i)
	{
		const Element *element = placed.at(i);
		CAPTURE(i);
		DynamicElementTextItem *tag = textSaying(element, QStringLiteral("TAG%1").arg(i));
		DynamicElementTextItem *function = textSaying(element, QStringLiteral("SOPRO%1").arg(i));
		REQUIRE((tag && function));
		CHECK(tag->font().pointSize() == 12);
		CHECK(function->font().pointSize() == 9);
	}
}
