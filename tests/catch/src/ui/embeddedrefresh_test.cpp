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
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/factory/elementpicturefactory.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUuid>

#include <memory>

/*
	Bringing into a project the symbols changed in the editable collections,
	and the notice given to whoever saves one.

	Everything here goes through the functions the button and the symbol
	editor call - EmbeddedElementRefresh::bringFromCollections() and
	noticeFor() - on a real example opened from its own directory, with two
	temporary directories standing for the company and the user collections.
	What stays out is the click: the button and the message box are windows,
	and a modal box under the offscreen platform waits forever.

	A symbol is corrected the way the symbol editor saves one: the drawing
	changes and the definition gets a new uuid. The terminals get new uuids
	of their own too, as a copy saved elsewhere would carry; it is what makes
	the save-and-reopen case able to fail if the uuids of the project's
	terminals were not kept.

	Whether a drawing changed is read off the folio: ink counted inside the
	element's box. The correction fills the whole box, which no font and no
	machine can mistake for the old drawing.

	Every case runs on two examples, because the copies of a project come in
	two kinds and the origin of each is recorded differently: those of
	741.qet carry no uuid, as in many old projects, and those of
	schema_unifilaire_voltaique2.qet all carry one. A bench of the first kind
	alone hid a copy corrected from a folio being taken for an untouched
	one, as soon as the correction gave it a uuid.
*/

namespace
{
	const QStringList examples = {QStringLiteral("741.qet"),
								  QStringLiteral("schema_unifilaire_voltaique2.qet")};
	const QString import_prefix = QStringLiteral("import/");

	/// A placed element drawn from a copy under import/, with a conductor on it
	Element *wiredImportedElement(QETProject *project)
	{
		for (Diagram *diagram : project->diagrams())
		{
			const DiagramContent content(diagram, false);
			for (Element *element : content.m_elements)
			{
				if (element->location().collectionPath(false).startsWith(import_prefix)
						&& !element->conductors().isEmpty()) {
					return element;
				}
			}
		}
		return nullptr;
	}

	Element *elementWithUuid(QETProject *project, const QUuid &uuid)
	{
		for (Diagram *diagram : project->diagrams())
		{
			const DiagramContent content(diagram, false);
			for (Element *element : content.m_elements) {
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

	/// A document of its own holding the project's copy of @p element
	QDomDocument copyOf(const Element *element)
	{
		QDomDocument definition;
		definition.appendChild(definition.importNode(element->location().xml(), true));
		return definition;
	}

	QUuid uuidOf(const QDomDocument &definition)
	{
		return ElementOrigin::definitionUuid(definition.documentElement());
	}

	QUuid copyUuid(QETProject *project, const QString &embedded_path)
	{
		return ElementsLocation(embedded_path, project).uuid();
	}

	QUuid copyOrigin(QETProject *project, const QString &embedded_path)
	{
		return ElementOrigin::of(ElementsLocation(embedded_path, project)
								 .xml().parentNode().toElement());
	}

	/// What a save elsewhere does: a new uuid, for the symbol and its terminals
	void resave(QDomDocument &definition)
	{
		QDomElement root = definition.documentElement();
		QDomElement uuid = root.firstChildElement(QStringLiteral("uuid"));
		if (uuid.isNull()) {
			uuid = definition.createElement(QStringLiteral("uuid"));
			root.appendChild(uuid);
		}
		uuid.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());

		const QDomElement description = root.firstChildElement(QStringLiteral("description"));
		for (QDomElement terminal = description.firstChildElement(QStringLiteral("terminal"));
			 !terminal.isNull();
			 terminal = terminal.nextSiblingElement(QStringLiteral("terminal"))) {
			terminal.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
		}
	}

	/// The whole box of the symbol filled, terminals where they were
	QDomDocument withBlock(const Element *element)
	{
		QDomDocument definition = copyOf(element);
		QDomElement root = definition.documentElement();
		QDomElement rect = definition.createElement(QStringLiteral("rect"));
		rect.setAttribute(QStringLiteral("x"), -root.attribute(QStringLiteral("hotspot_x")).toInt());
		rect.setAttribute(QStringLiteral("y"), -root.attribute(QStringLiteral("hotspot_y")).toInt());
		rect.setAttribute(QStringLiteral("width"), root.attribute(QStringLiteral("width")).toInt());
		rect.setAttribute(QStringLiteral("height"), root.attribute(QStringLiteral("height")).toInt());
		rect.setAttribute(QStringLiteral("rx"), 0);
		rect.setAttribute(QStringLiteral("ry"), 0);
		rect.setAttribute(QStringLiteral("style"),
						  QStringLiteral("line-style:normal;line-weight:normal;filling:black;color:black"));
		rect.setAttribute(QStringLiteral("antialias"), QStringLiteral("false"));
		root.firstChildElement(QStringLiteral("description")).appendChild(rect);
		resave(definition);
		return definition;
	}

	/// The same drawing with one terminal more, on the top left corner
	QDomDocument withExtraTerminal(const Element *element)
	{
		QDomDocument definition = copyOf(element);
		QDomElement root = definition.documentElement();
		QDomElement terminal = definition.createElement(QStringLiteral("terminal"));
		terminal.setAttribute(QStringLiteral("x"), -root.attribute(QStringLiteral("hotspot_x")).toInt());
		terminal.setAttribute(QStringLiteral("y"), -root.attribute(QStringLiteral("hotspot_y")).toInt());
		terminal.setAttribute(QStringLiteral("orientation"), QStringLiteral("n"));
		terminal.setAttribute(QStringLiteral("type"), QStringLiteral("Generic"));
		root.firstChildElement(QStringLiteral("description")).appendChild(terminal);
		resave(definition);
		return definition;
	}

	/// What the symbol editor does on saving: a new uuid for the symbol only,
	/// the terminals keeping theirs. The editor always writes one, so a copy
	/// of 741.qet, which has none, gets it here
	void newRevision(QDomDocument &definition)
	{
		QDomElement root = definition.documentElement();
		QDomElement uuid = root.firstChildElement(QStringLiteral("uuid"));
		if (uuid.isNull()) {
			uuid = definition.createElement(QStringLiteral("uuid"));
			root.appendChild(uuid);
		}
		uuid.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
	}

	QDomElement terminalsOf(const QDomDocument &definition)
	{
		return definition.documentElement().firstChildElement(QStringLiteral("description"));
	}

	/// The box filled, and the box ten units taller upwards: the hotspot moves
	/// with it, so every terminal stays where it was relative to the hotspot -
	/// what the symbol editor's recomputed box does to a symbol
	QDomDocument withBox(const Element *element)
	{
		QDomDocument definition = copyOf(element);
		QDomElement root = definition.documentElement();
		QDomElement rect = definition.createElement(QStringLiteral("rect"));
		rect.setAttribute(QStringLiteral("x"), -root.attribute(QStringLiteral("hotspot_x")).toInt());
		rect.setAttribute(QStringLiteral("y"), -root.attribute(QStringLiteral("hotspot_y")).toInt());
		rect.setAttribute(QStringLiteral("width"), root.attribute(QStringLiteral("width")).toInt());
		rect.setAttribute(QStringLiteral("height"), root.attribute(QStringLiteral("height")).toInt());
		rect.setAttribute(QStringLiteral("rx"), 0);
		rect.setAttribute(QStringLiteral("ry"), 0);
		rect.setAttribute(QStringLiteral("style"),
						  QStringLiteral("line-style:normal;line-weight:normal;filling:black;color:black"));
		rect.setAttribute(QStringLiteral("antialias"), QStringLiteral("false"));
		terminalsOf(definition).appendChild(rect);
		root.setAttribute(QStringLiteral("hotspot_y"),
						  root.attribute(QStringLiteral("hotspot_y")).toInt() + 10);
		root.setAttribute(QStringLiteral("height"),
						  root.attribute(QStringLiteral("height")).toInt() + 10);
		newRevision(definition);
		return definition;
	}

	/// Every terminal ten units to the right, each keeping its uuid, and the
	/// box ten units wider
	QDomDocument withMovedTerminals(const Element *element)
	{
		QDomDocument definition = copyOf(element);
		QDomElement root = definition.documentElement();
		for (QDomElement terminal = terminalsOf(definition).firstChildElement(QStringLiteral("terminal"));
			 !terminal.isNull();
			 terminal = terminal.nextSiblingElement(QStringLiteral("terminal"))) {
			terminal.setAttribute(QStringLiteral("x"),
								  terminal.attribute(QStringLiteral("x")).toDouble() + 10);
		}
		root.setAttribute(QStringLiteral("width"),
						  root.attribute(QStringLiteral("width")).toInt() + 10);
		newRevision(definition);
		return definition;
	}

	/// The same symbol without the terminal of uuid @p gone
	QDomDocument withoutTerminal(const Element *element, const QUuid &gone)
	{
		QDomDocument definition = copyOf(element);
		QDomElement description = terminalsOf(definition);
		for (QDomElement terminal = description.firstChildElement(QStringLiteral("terminal"));
			 !terminal.isNull();
			 terminal = terminal.nextSiblingElement(QStringLiteral("terminal"))) {
			if (QUuid(terminal.attribute(QStringLiteral("uuid"))) == gone) {
				description.removeChild(terminal);
				break;
			}
		}
		newRevision(definition);
		return definition;
	}

	/// A terminal of @p element with a conductor on it
	Terminal *wiredTerminal(const Element *element)
	{
		for (Terminal *terminal : element->terminals()) {
			if (!terminal->conductors().isEmpty()) {
				return terminal;
			}
		}
		return nullptr;
	}

	QByteArray bytesOf(const QString &file_path)
	{
		QFile file(file_path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

	/// Write @p definition where a collection rooted at @p dir keeps the symbol of @p embedded_path
	bool writeInto(const QString &dir, const QString &embedded_path, const QDomDocument &definition)
	{
		const QString file_path = QDir(dir).absoluteFilePath(embedded_path.mid(import_prefix.size()));
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
	bool saveOverCopy(QETProject *project, const QString &embedded_path, const QDomDocument &definition)
	{
		return ElementsLocation(embedded_path, project).setXml(definition);
	}

	/// The redraw the button does after bringing, for one symbol
	void redraw(QETProject *project, const QString &embedded_path)
	{
		ElementPictureFactory::instance()->dropCache(ElementsLocation(embedded_path, project));
		for (Element *element : EmbeddedElementRefresh::placedElements(project, embedded_path)) {
			element->reloadPicture();
		}
	}

	int inkOn(Element *element)
	{
		const UiBench::Rendering rendering(element->diagram());
		return rendering.ink(element->sceneBoundingRect());
	}

	/// The project, its wired element, and the two editable collections
	struct Bench
	{
		explicit Bench(const QString &example) :
			project(UiBench::fileContent(UiBench::examplePath(example)), example)
		{
			if (project.isOpen()) {
				element = wiredImportedElement(project.project());
			}
			if (element) {
				path = element->location().collectionPath(false);
			}
		}

		QStringList dirs() const {return {company.path(), user.path()};}
		QETProject *operator->() const {return project.project();}
		QETProject *get() const {return project.project();}

		UiBench::ScratchProject project;
		QTemporaryDir company;
		QTemporaryDir user;
		Element *element = nullptr;
		QString path;
	};

	/// The cases CU-45.3 and CU-45.4 are the same but for the directory
	void bringsFrom(Bench &bench, const QString &dir)
	{
		QETProject *project = bench.get();
		const int placed = EmbeddedElementRefresh::placedElements(project, bench.path).size();
		const int ink_before = inkOn(bench.element);
		REQUIRE_FALSE(project->projectWasModified());

		const QDomDocument corrected = withBlock(bench.element);
		REQUIRE(writeInto(dir, bench.path, corrected));

		const EmbeddedElementRefresh::Report report =
				EmbeddedElementRefresh::bringFromCollections(project, bench.dirs());

		CHECK(report.brought_symbols == 1);
		CHECK(report.brought_elements == placed);
		CHECK(copyUuid(project, bench.path) == uuidOf(corrected));
		CHECK(copyOrigin(project, bench.path) == uuidOf(corrected));
		CHECK(inkOn(bench.element) > ink_before);
		CHECK(project->projectWasModified());
		CHECK(EmbeddedElementRefresh::reportText(report).contains(QString::number(placed)));
	}
}

TEST_CASE("CU-45.3 — A symbol corrected in the company collection comes in and is redrawn",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.project.isOpen());
	REQUIRE(bench.element);
	bringsFrom(bench, bench.company.path());
}

TEST_CASE("CU-45.4 — A symbol corrected in the user collection comes in and is redrawn",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.project.isOpen());
	REQUIRE(bench.element);
	bringsFrom(bench, bench.user.path());
}

TEST_CASE("CU-45.5 — Undo puts the project's copy and the drawing back, redo brings them again",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const QUuid old_uuid = copyUuid(project, bench.path);
	const int ink_before = inkOn(bench.element);

	const QDomDocument corrected = withBlock(bench.element);
	REQUIRE(writeInto(bench.company.path(), bench.path, corrected));
	REQUIRE(EmbeddedElementRefresh::bringFromCollections(project, bench.dirs()).brought_symbols == 1);
	const int ink_after = inkOn(bench.element);
	REQUIRE(ink_after > ink_before);

	project->undoStack()->undo();
	CHECK(copyUuid(project, bench.path) == old_uuid);
	CHECK(copyOrigin(project, bench.path) == old_uuid);
	CHECK(inkOn(bench.element) == ink_before);

	project->undoStack()->redo();
	CHECK(copyUuid(project, bench.path) == uuidOf(corrected));
	CHECK(inkOn(bench.element) == ink_after);
}

TEST_CASE("CU-45.6 — Saved and opened again, the new drawing stays and no wire comes loose",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	const QUuid element_uuid = bench.element->uuid();
	const int wires_on_it = bench.element->conductors().size();
	const int wires = conductorCount(bench.get());
	const int ink_before = inkOn(bench.element);
	REQUIRE(wires_on_it > 0);

	const QDomDocument corrected = withBlock(bench.element);
	REQUIRE(writeInto(bench.company.path(), bench.path, corrected));
	REQUIRE(EmbeddedElementRefresh::bringFromCollections(bench.get(), bench.dirs()).brought_symbols == 1);

	REQUIRE(bench.project.saveAndReopen());
	QETProject *project = bench.get();
	Element *reopened = elementWithUuid(project, element_uuid);
	REQUIRE(reopened);

	CHECK(copyUuid(project, bench.path) == uuidOf(corrected));
	CHECK(copyOrigin(project, bench.path) == uuidOf(corrected));
	CHECK(inkOn(reopened) > ink_before);
	CHECK(reopened->conductors().size() == wires_on_it);
	CHECK(conductorCount(project) == wires);
}

TEST_CASE("CU-45.7 — A new version whose terminals changed is not brought, and is named",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const QUuid old_uuid = copyUuid(project, bench.path);
	const int placed = EmbeddedElementRefresh::placedElements(project, bench.path).size();
	const int ink_before = inkOn(bench.element);

	REQUIRE(writeInto(bench.company.path(), bench.path, withExtraTerminal(bench.element)));
	const EmbeddedElementRefresh::Report report =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs());

	CHECK(EmbeddedElementRefresh::needingReload(project, bench.dirs()).size() == placed);
	CHECK(report.brought_symbols == 0);
	CHECK(report.terminals_changed.size() == placed);
	CHECK(report.terminals_changed.contains(EmbeddedElementRefresh::describe(bench.element)));
	CHECK(copyUuid(project, bench.path) == old_uuid);
	CHECK(inkOn(bench.element) == ink_before);
	CHECK(project->undoStack()->count() == 0);
	CHECK_FALSE(EmbeddedElementRefresh::reportText(report).isEmpty());
	CHECK(EmbeddedElementRefresh::reportDetails(report).contains(
			  EmbeddedElementRefresh::describe(bench.element)));
}

TEST_CASE("CU-45.8 — The same path in both collections is not guessed",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const QUuid old_uuid = copyUuid(project, bench.path);

	REQUIRE(writeInto(bench.company.path(), bench.path, withBlock(bench.element)));
	REQUIRE(writeInto(bench.user.path(), bench.path, withBlock(bench.element)));
	const EmbeddedElementRefresh::Report report =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs());

	CHECK(report.brought_symbols == 0);
	CHECK(report.ambiguous.size() == 1);
	CHECK(copyUuid(project, bench.path) == old_uuid);
	CHECK(project->undoStack()->count() == 0);
	CHECK_FALSE(EmbeddedElementRefresh::reportText(report).isEmpty());
}

TEST_CASE("CU-45.9 — Nothing is brought from outside the editable collections",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();

		//The two collections are empty: every copy of the example came from
		//somewhere else, which is the case of the QET collection
	const QList<EmbeddedElementRefresh::Entry> entries =
			EmbeddedElementRefresh::survey(project, bench.dirs());
	REQUIRE_FALSE(entries.isEmpty());
	for (const EmbeddedElementRefresh::Entry &entry : entries) {
		CHECK(entry.outcome == EmbeddedElementRefresh::Outcome::NoSource);
	}

	const EmbeddedElementRefresh::Report report =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs());
	CHECK(report.brought_symbols == 0);
	CHECK(project->undoStack()->count() == 0);
	CHECK(EmbeddedElementRefresh::reportText(report).isEmpty());
}

TEST_CASE("CU-45.10 — When nothing changed, nothing is stacked and the project stays clean",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	REQUIRE_FALSE(project->projectWasModified());

		//The very version the project holds, as the collection would keep it
	REQUIRE(writeInto(bench.company.path(), bench.path, copyOf(bench.element)));
	const EmbeddedElementRefresh::Report report =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs());

	CHECK(report.brought_symbols == 0);
	CHECK(project->undoStack()->count() == 0);
	CHECK_FALSE(project->projectWasModified());
	CHECK(EmbeddedElementRefresh::reportText(report).isEmpty());
}

TEST_CASE("CU-45.14 — A copy corrected from a folio is not undone by the collection",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const int ink_before = inkOn(bench.element);

		//The collection keeps the version the project took; then the copy is
		//corrected from the folio, which gives it a new uuid
	REQUIRE(writeInto(bench.company.path(), bench.path, copyOf(bench.element)));
	const QDomDocument corrected = withBlock(bench.element);
	REQUIRE(saveOverCopy(project, bench.path, corrected));

	const EmbeddedElementRefresh::Report report =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs());
	CHECK(report.brought_symbols == 0);
	CHECK(report.changed_in_project.isEmpty());
	CHECK(copyUuid(project, bench.path) == uuidOf(corrected));

	redraw(project, bench.path);
	CHECK(inkOn(bench.element) > ink_before);
}

TEST_CASE("CU-45.15 — Changed in the project and in the collection: the project's is kept, and named",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();

		//Two different corrections: since the T58 the same correction made
		//on both sides is no conflict, the content being the same
	const QDomDocument in_project = withBlock(bench.element);
	REQUIRE(saveOverCopy(project, bench.path, in_project));
	REQUIRE(writeInto(bench.company.path(), bench.path, withBox(bench.element)));

	const EmbeddedElementRefresh::Report report =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs());
	CHECK(report.brought_symbols == 0);
	CHECK(report.changed_in_project.size() == 1);
	CHECK(copyUuid(project, bench.path) == uuidOf(in_project));
	CHECK(project->undoStack()->count() == 0);
	CHECK_FALSE(EmbeddedElementRefresh::reportText(report).isEmpty());
}

TEST_CASE("T45 — The origin of a copy is stamped on opening and survives saving",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	const QDomElement collection_element =
			ElementsLocation(bench.path, bench.get()).xml().parentNode().toElement();

		//The example was saved before the record existed: opening it took
		//every copy as the version it came from
	CHECK(collection_element.hasAttribute(QStringLiteral("origin_uuid")));
	CHECK(copyOrigin(bench.get(), bench.path) == copyUuid(bench.get(), bench.path));

		//Corrected from the folio, the copy keeps the origin it had
	const QUuid origin = copyOrigin(bench.get(), bench.path);
	REQUIRE(saveOverCopy(bench.get(), bench.path, withBlock(bench.element)));
	CHECK(copyOrigin(bench.get(), bench.path) == origin);
	CHECK(copyUuid(bench.get(), bench.path) != origin);

	REQUIRE(bench.project.saveAndReopen());
	CHECK(copyOrigin(bench.get(), bench.path) == origin);
}

TEST_CASE("T45 — A copy taken into a project records the version it came from",
		  "[embeddedrefresh]")
{
		//Two projects, so that one can take a symbol from the other through
		//the two doors an import goes through: addElement() for a first drop,
		//copy() for the import dialog's "replace"
	const QString example = GENERATE(from_range(examples));
	Bench source(example);
	Bench target(example);
	REQUIRE(source.project.isOpen());
	REQUIRE(target.element);
		//Taken from the source at the target's own path, so that copy()
		//replaces: the two benches do not pick the same element, the order
		//of a scene being no order, and a different path would only add one
	ElementsLocation from(target.path, source.get());
	REQUIRE(from.exist());
	XmlElementCollection *collection = target->embeddedElementCollection();

	const QString added = collection->addElement(from);
	REQUIRE_FALSE(added.isEmpty());
	CHECK(collection->element(added).hasAttribute(QStringLiteral("origin_uuid")));

		//The target's copy corrected from its folio first, so that its record
		//and its uuid differ before it is replaced
	REQUIRE(saveOverCopy(target.get(), target.path, withBlock(target.element)));
	REQUIRE(copyOrigin(target.get(), target.path) != copyUuid(target.get(), target.path));
	ElementsLocation parent = ElementsLocation(target.path, target.get()).parent();
		//What copy() returns is addressed by the number QETApp gives an open
		//project, and a bench project has none: the location comes back
		//null even though the copy was made. So the copy is read off the
		//target's collection instead
	collection->copy(from, parent);
	CAPTURE(example, target.path);
	REQUIRE(copyUuid(target.get(), target.path) == copyUuid(source.get(), target.path));
	CHECK(collection->element(target.path).hasAttribute(QStringLiteral("origin_uuid")));
	CHECK(copyOrigin(target.get(), target.path) == copyUuid(target.get(), target.path));
}

TEST_CASE("T45 — The notice counts the elements placed from a saved copy",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const int placed = EmbeddedElementRefresh::placedElements(project, bench.path).size();

	const QDomDocument corrected = withBlock(bench.element);
	REQUIRE(saveOverCopy(project, bench.path, corrected));
	const QString notice = EmbeddedElementRefresh::savedNotice(
				ElementsLocation(bench.path, project), {project}, bench.dirs());

	CHECK(notice.contains(QString::number(placed)));
	CHECK(notice.contains(QStringLiteral("Recharger les dessins des éléments")));
	CHECK_FALSE(notice.contains(QStringLiteral("bornes")));
}

TEST_CASE("T45 — No notice for a symbol no open project has placed",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	const QString notice = EmbeddedElementRefresh::noticeFor(
				QStringLiteral("import/nowhere/nothing.elmt"),
				copyOf(bench.element).documentElement(),
				{bench.get()}, true, bench.dirs());
	CHECK(notice.isEmpty());
}

TEST_CASE("T45 — The notice says when the terminals changed",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	const QString notice = EmbeddedElementRefresh::noticeFor(
				bench.path, withExtraTerminal(bench.element).documentElement(),
				{bench.get()}, true, bench.dirs());
	CHECK(notice.contains(QStringLiteral("bornes")));
	CHECK(notice.contains(QStringLiteral("recharger le projet")));
	CHECK_FALSE(notice.contains(QStringLiteral("supprimer et les réinsérer")));
}

TEST_CASE("T45 — The notice says nothing about a new box, which is applied in place",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	const QString notice = EmbeddedElementRefresh::noticeFor(
				bench.path, withBox(bench.element).documentElement(),
				{bench.get()}, true, bench.dirs());
	CHECK_FALSE(notice.isEmpty());
	CHECK_FALSE(notice.contains(QStringLiteral("bornes")));
}

TEST_CASE("T45 — The notice of a collection save names a copy changed in the project, and an ambiguous path",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const QDomDocument saved = withBlock(bench.element);

	const QString plain = EmbeddedElementRefresh::noticeFor(
				bench.path, saved.documentElement(), {project}, true, bench.dirs());
	CHECK_FALSE(plain.isEmpty());
	CHECK_FALSE(plain.contains(QStringLiteral("conservée")));
	CHECK_FALSE(plain.contains(QStringLiteral("même chemin")));

	REQUIRE(saveOverCopy(project, bench.path, withBlock(bench.element)));
	REQUIRE(writeInto(bench.company.path(), bench.path, saved));
	REQUIRE(writeInto(bench.user.path(), bench.path, saved));
	const QString both = EmbeddedElementRefresh::noticeFor(
				bench.path, saved.documentElement(), {project}, true, bench.dirs());
	CHECK(both.contains(QStringLiteral("conservée")));
	CHECK(both.contains(QStringLiteral("même chemin")));
}

TEST_CASE("CU-45.16 — Only the box changed: brought in place, and undo puts the box back",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const QDomDocument original = copyOf(bench.element);
	const QUuid element_uuid = bench.element->uuid();
	const int wires = conductorCount(project);
	const int ink_before = inkOn(bench.element);

	const QDomDocument boxed = withBox(bench.element);
	REQUIRE_FALSE(bench.element->definitionGeometryMatches(boxed.documentElement()));
	REQUIRE(bench.element->definitionTerminalsMatch(boxed.documentElement()));
	REQUIRE(writeInto(bench.company.path(), bench.path, boxed));

	CHECK(EmbeddedElementRefresh::needingReload(project, bench.dirs()).isEmpty());
	const EmbeddedElementRefresh::Report report =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs());
	CHECK(report.brought_symbols == 1);
	CHECK(report.terminals_changed.isEmpty());
	CHECK(bench.element->definitionGeometryMatches(boxed.documentElement()));
	CHECK(inkOn(bench.element) > ink_before);

	project->undoStack()->undo();
	CHECK(bench.element->definitionGeometryMatches(original.documentElement()));
	CHECK(inkOn(bench.element) == ink_before);

	project->undoStack()->redo();
	CHECK(bench.element->definitionGeometryMatches(boxed.documentElement()));

	REQUIRE(bench.project.saveAndReopen());
	CHECK(conductorCount(bench.get()) == wires);
	Element *reopened = elementWithUuid(bench.get(), element_uuid);
	REQUIRE(reopened);
	CHECK(reopened->definitionGeometryMatches(boxed.documentElement()));
}

TEST_CASE("CU-45.21 — Corrected from a folio with another box: redrawn in place, nothing stacked",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const int ink_before = inkOn(bench.element);

	const QDomDocument boxed = withBox(bench.element);
	REQUIRE(saveOverCopy(project, bench.path, boxed));
	CHECK(EmbeddedElementRefresh::needingReload(project, bench.dirs()).isEmpty());

		//Without the box, the redraw refuses
	CHECK(bench.element->reloadPicture() == Element::ReloadPictureResult::GeometryChanged);

	CHECK(EmbeddedElementRefresh::adoptBoxes(project)
		  == EmbeddedElementRefresh::placedElements(project, bench.path).size());
	ElementPictureFactory::instance()->dropCache(ElementsLocation(bench.path, project));
	CHECK(bench.element->reloadPicture() == Element::ReloadPictureResult::Reloaded);
	CHECK(bench.element->definitionGeometryMatches(boxed.documentElement()));
	CHECK(inkOn(bench.element) > ink_before);
	CHECK(project->undoStack()->count() == 0);
}

TEST_CASE("T45 — A copy corrected from a folio with another terminal needs the reload",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const int placed = EmbeddedElementRefresh::placedElements(project, bench.path).size();

	REQUIRE(saveOverCopy(project, bench.path, withExtraTerminal(bench.element)));
	const QStringList needing = EmbeddedElementRefresh::needingReload(project, bench.dirs());
	CHECK(needing.size() == placed);
	CHECK(needing.contains(EmbeddedElementRefresh::describe(bench.element)));
}

TEST_CASE("CU-45.17 — A moved terminal: the reloaded project keeps the wires on it",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const QUuid element_uuid = bench.element->uuid();
	const int placed = EmbeddedElementRefresh::placedElements(project, bench.path).size();
	const int wires_on_it = bench.element->conductors().size();
	const int wires = conductorCount(project);
	REQUIRE(wires_on_it > 0);

	const QDomDocument moved = withMovedTerminals(bench.element);
	REQUIRE_FALSE(bench.element->definitionTerminalsMatch(moved.documentElement()));
	REQUIRE(writeInto(bench.company.path(), bench.path, moved));
	CHECK(EmbeddedElementRefresh::needingReload(project, bench.dirs()).size() == placed);

	const EmbeddedElementRefresh::Report report =
			EmbeddedElementRefresh::bringFromCollections(project, bench.dirs(), true);
	CHECK(report.brought_symbols == 1);
	CHECK(report.brought_elements == placed);

	QString error;
	std::unique_ptr<QETProject> reloaded(EmbeddedElementRefresh::reopened(project, &error));
	CAPTURE(error);
	REQUIRE(reloaded);
	CHECK(reloaded->filePath() == project->filePath());
	CHECK(reloaded->projectWasModified());
	CHECK(EmbeddedElementRefresh::wiresLeftOut(reloaded.get()).isEmpty());
	CHECK(conductorCount(reloaded.get()) == wires);

	Element *rebuilt = elementWithUuid(reloaded.get(), element_uuid);
	REQUIRE(rebuilt);
	CHECK(rebuilt->definitionTerminalsMatch(moved.documentElement()));
	CHECK(rebuilt->conductors().size() == wires_on_it);
}

TEST_CASE("CU-45.18 — A terminal gone: its wires are left out and listed, the others stay",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const int wires = conductorCount(project);
	Terminal *wired = wiredTerminal(bench.element);
	REQUIRE(wired);
	REQUIRE_FALSE(wired->uuid().isNull());

		//The terminal goes from the symbol, so from every element placed
		//from it: each loses the conductors on its own terminal of that uuid
	QSet<Conductor *> lost;
	for (Element *element : EmbeddedElementRefresh::placedElements(project, bench.path)) {
		for (Terminal *terminal : element->terminals()) {
			if (terminal->uuid() == wired->uuid()) {
				for (Conductor *conductor : terminal->conductors()) {
					lost << conductor;
				}
			}
		}
	}
	const int removed = lost.size();
	REQUIRE(removed > 0);

	REQUIRE(writeInto(bench.company.path(), bench.path,
					  withoutTerminal(bench.element, wired->uuid())));
	REQUIRE(EmbeddedElementRefresh::bringFromCollections(project, bench.dirs(), true)
			.brought_symbols == 1);

	std::unique_ptr<QETProject> reloaded(EmbeddedElementRefresh::reopened(project));
	REQUIRE(reloaded);
	CHECK(EmbeddedElementRefresh::wiresLeftOut(reloaded.get()).size() == removed);
	CHECK(conductorCount(reloaded.get()) == wires - removed);
	CHECK(EmbeddedElementRefresh::reloadReportText({}, removed).contains(QString::number(removed)));
}

TEST_CASE("CU-45.19 — The reload needs no save and leaves the file on the disk as it was",
		  "[embeddedrefresh]")
{
	Bench bench(GENERATE(from_range(examples)));
	REQUIRE(bench.element);
	QETProject *project = bench.get();
	const QByteArray on_disk = bytesOf(project->filePath());
	REQUIRE_FALSE(on_disk.isEmpty());

		//Something not saved: it has to come through the reload
	project->setTitle(QStringLiteral("Not saved yet"));

	std::unique_ptr<QETProject> reloaded(EmbeddedElementRefresh::reopened(project));
	REQUIRE(reloaded);
	CHECK(reloaded->title() == QStringLiteral("Not saved yet"));
	CHECK(reloaded->filePath() == project->filePath());
	CHECK(reloaded->projectWasModified());
	CHECK_FALSE(reloaded->changedOnDisk());
	CHECK(bytesOf(project->filePath()) == on_disk);
}
