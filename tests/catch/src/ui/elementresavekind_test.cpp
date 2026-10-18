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

#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/graphicspart/customelementpart.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"
#include "../../../../sources/properties/elementdata.h"
#include "../../../../sources/qetgraphicsitem/element.h"

#include <catch2/catch.hpp>

#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QDomElement>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QIODevice>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>

/*
	A symbol opened in the symbol editor and saved with no change keeps its
	kind informations, and a project placed from it does not ask for it to
	be reloaded.

	The editor wrote back only the kind informations it has a field for:
	the type and the limit of slaves of a master, the type, the state and
	the number of a contact, the type, the function and the link role of a
	terminal - and nothing at all for a thumbnail or a simple element. In
	the collection the program ships with, 443 symbols lost some on being
	saved: 420 thumbnails their sub-type (the "sectional" of 400 terminal
	thumbnails), 13 masters the state and the number of their symbol, 10
	simple elements a type and a function. The project compared the kind
	of what it was built from with the symbol saved, found another one,
	and asked for the reload; and the file had lost what it said.

	A symbol is saved here the way the editor does it: its definition
	given to the scene of a QETElementEditor (ElementScene::fromXml()), and
	the scene written (ElementScene::toXml()), which is what the editor's
	fromFile() and toFile() call. The few symbols of the visible case go
	through fromFile() and toFile() themselves, on a writable copy: a
	read-only one would open the editor's read-only warning, a modal box
	nothing here could close. The sweep of the whole collection reuses one
	editor, its scene emptied between two symbols as the editor's own
	reload does it (and the parts deleted), and its element data reset: a
	new editor for each of the some 8800 symbols would take minutes more.
	The visible case checks that both ways write the same kind
	informations. A scene with no
	editor would not do: a part reaches its scene through the editor
	(CustomElementPart::elementScene()), and a dynamic text asks for it as
	soon as it is added.

	The editor moves to the origin a drawing that does not hold it
	(ElementScene::toXml()), and the terminals with it - but only a symbol
	that changed: one saved untouched keeps its drawing where its file has
	it (elementsaveorigin_test.cpp). So the sweep takes any terminal that
	moved for a failure too.

	Every message is reduced to ASCII: Catch2 v2 wraps a long message to
	print it, and with a multi-byte character in it the wrap can throw
	std::out_of_range and end the run without a summary - one path of the
	collection has an accent.
*/

namespace
{
	QString collectionDir()
	{
		return QDir(QStringLiteral(QET_EXAMPLES_DIR "/../elements")).absolutePath();
	}

	std::string ascii(const QString &text)
	{
		std::string reduced;
		reduced.reserve(static_cast<std::size_t>(text.size()));
		for (const QChar c : text) {
			reduced += c.unicode() < 128 ? static_cast<char>(c.unicode()) : '?';
		}
		return reduced;
	}

	QDomDocument definitionAt(const QString &path)
	{
		QDomDocument document;
		QFile file(path);
		if (file.open(QIODevice::ReadOnly)) {
			document.setContent(&file);
		}
		return document;
	}

	/**
		@return a writable copy of @a definition, named @a name in @a dir
	*/
	QString writtenCopy(const QTemporaryDir &dir, const QString &name, const QDomDocument &definition)
	{
		const QString path = dir.filePath(name);
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
		const QByteArray content = definition.toByteArray(4);
		REQUIRE(file.write(content) == content.size());
		return path;
	}

	/// The kind informations of @a definition, as written: "name=text"
	/// each, in the order of the file
	QStringList kindInformationsOf(const QDomDocument &definition)
	{
		QStringList infos;
		for (QDomElement info = definition.documentElement()
									.firstChildElement(QStringLiteral("kindInformations"))
									.firstChildElement(QStringLiteral("kindInformation"));
			 !info.isNull();
			 info = info.nextSiblingElement(QStringLiteral("kindInformation"))) {
			infos << info.attribute(QStringLiteral("name")) + QLatin1Char('=') + info.text();
		}
		return infos;
	}

	/**
		@return the kind informations of @a original that @a saved does not
		have, or has in another order; empty when @a saved has every one of
		them in their order, whatever it adds - the editor may add one it
		manages, which the project compares as the program reads it
	*/
	QStringList notKept(const QStringList &original, const QStringList &saved)
	{
		QStringList missing;
		qsizetype from = 0;
		for (const QString &info : original)
		{
			const qsizetype at = saved.indexOf(info, from);
			if (at < 0) {
				missing << info;
			} else {
				from = at + 1;
			}
		}
		return missing;
	}

	QString linkTypeOf(const QDomDocument &definition)
	{
		return definition.documentElement().attribute(QStringLiteral("link_type"));
	}

	/// Whether ElementScene::toXml() moves the drawing of @a scene to the
	/// origin before writing it, which moves its terminals as well
	bool movedOnSaving(ElementScene *scene)
	{
		const QRectF drawing = scene->elementSceneGeometricRect();
		return !drawing.contains(0, 0)
				&& (qRound(drawing.center().x()) != 0 || qRound(drawing.center().y()) != 0);
	}

	struct Resaved
	{
		QDomDocument saved;
		bool moved = false;
	};

	/// @a definition saved by @a editor, its scene emptied first: what the
	/// sweep does to every symbol of the collection.
	/// The parts of the symbol before are deleted, and not only taken off
	/// the scene as ElementScene::reset() does it: some 420 000 parts left
	/// alive until the end of the sweep would weigh hundreds of megabytes.
	Resaved resavedBy(QETElementEditor &editor, const QDomDocument &definition)
	{
		ElementScene *scene = editor.elementScene();
		QList<QGraphicsItem *> previous;
		const QList<QGraphicsItem *> items = scene->items();
		for (QGraphicsItem *item : items) {
			if (!item->parentItem() && dynamic_cast<CustomElementPart *>(item)) {
				previous << item;
			}
		}
		qDeleteAll(previous);
		scene->reset();
		scene->setElementData(ElementData());
		scene->fromXml(definition);

		Resaved result;
		result.moved = movedOnSaving(scene);
			//Written and read back, as the file is
		result.saved.setContent(scene->toXml().toString(4));
		return result;
	}

	/// What EmbeddedElementRefresh::needingReload() asks of every element
	/// placed from @a original, the symbol being saved as @a saved
	struct Verdict
	{
		bool compared = false;
		bool same_kind = false;
		bool same_terminals = false;
	};

	Verdict verdict(const QDomDocument &original, const QDomDocument &saved)
	{
		Verdict v;
		std::unique_ptr<Element> element(Element::buildDetached(original.documentElement()));
		if (!element || element->symbolBase().isNull()) {
			return v;
		}
		v.compared = true;
		v.same_kind = element->definitionKindMatches(saved.documentElement());
		v.same_terminals = element->definitionTerminalsMatch(saved.documentElement());
		return v;
	}

	/// A symbol of the collection the program ships with, and the kind
	/// informations it is chosen for
	struct Sample
	{
		QString path;
		QStringList kind;
	};

	const QList<Sample> &samples()
	{
		static const QList<Sample> list = {
				//A thumbnail of a sectional terminal: its sub-type
			{QStringLiteral("10_electric/20_manufacturers_articles/wago/02_front/0000_terminals/"
							"0285_terminals/wago_0285-0131-0000-0000.elmt"),
			 {QStringLiteral("type=sectional"), QStringLiteral("function=generic")}},
				//A thumbnail of a contact
			{QStringLiteral("10_electric/98_graphics/99_assembly_plan/01_thumbnails_mounting_plate/"
							"hager/02-disjoncteurs/hag_mfs701.elmt"),
			 {QStringLiteral("type=power"), QStringLiteral("state=NO"), QStringLiteral("number=1")}},
				//A master with the state and the number of a contact,
				//before its type
			{QStringLiteral("10_electric/10_allpole/500_home_installation/20_home_appliances/divers10.elmt"),
			 {QStringLiteral("number=1"), QStringLiteral("type=protection"), QStringLiteral("state=NO")}},
				//A master whose type is empty: read as a coil, and written
				//back empty, as it was
			{QStringLiteral("10_electric/20_manufacturers_articles/knx/powersupplies/voeding-320ma.elmt"),
			 {QStringLiteral("type=")}},
				//A simple element with the type and the function of a terminal
			{QStringLiteral("10_electric/20_manufacturers_articles/gce/gce-ipx800V4-mini.elmt"),
			 {QStringLiteral("type=generic"), QStringLiteral("function=generic")}},
				//A terminal with no kind information at all: none is added
			{QStringLiteral("10_electric/10_allpole/130_terminals_terminal_strips/"
							"95_terminals_spring_box/wire-connector-2-port.elmt"),
			 {}},
				//A contact whose kind informations are in another order
				//than the editor writes them
			{QStringLiteral("10_electric/20_manufacturers_articles/sofrel/01_PLC_controllers/"
							"s500/8ai_input.elmt"),
			 {QStringLiteral("number=1"), QStringLiteral("type=simple"), QStringLiteral("state=NO")}}};
		return list;
	}
}

	//Kept under 79 characters: Catch2 v2 wraps a longer name to print a
	//failure, and with a multi-byte character in it the wrap can throw
	//std::out_of_range and end the run without a summary
TEST_CASE("T62 — Kind informations the editor does not manage, saved back",
		  "[elementresavekind][T62]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
		//The sweep's way of saving, on an editor that has saved another
		//symbol before
	QETElementEditor reused;

	for (const Sample &sample : samples())
	{
		CAPTURE(ascii(sample.path));
		const QDomDocument original = definitionAt(QDir(collectionDir()).filePath(sample.path));
		REQUIRE(original.documentElement().tagName() == QLatin1String("definition"));
			//What the sample was chosen for: without it the case proves nothing
		REQUIRE(kindInformationsOf(original) == sample.kind);

		const QString copy = writtenCopy(dir, QFileInfo(sample.path).fileName(), original);
		QETElementEditor editor;
		editor.fromFile(copy);
		REQUIRE(editor.elementScene() != nullptr);
			//Moved, its terminals would move too, and that is not what this
			//case is about: another sample would have to be chosen
		REQUIRE_FALSE(movedOnSaving(editor.elementScene()));
		const QString saved_path = dir.filePath(QStringLiteral("saved-") + QFileInfo(sample.path).fileName());
		REQUIRE(editor.toFile(saved_path));
		const QDomDocument saved = definitionAt(saved_path);

			//Written back as they were, in their order, none added
		CHECK(kindInformationsOf(saved) == sample.kind);
		CHECK(linkTypeOf(saved) == linkTypeOf(original));

		const Verdict v = verdict(original, saved);
		REQUIRE(v.compared);
		CHECK(v.same_kind);
		CHECK(v.same_terminals);

			//The sweep writes what the editor writes
		const Resaved resaved = resavedBy(reused, original);
		CHECK(kindInformationsOf(resaved.saved) == kindInformationsOf(saved));
		CHECK(linkTypeOf(resaved.saved) == linkTypeOf(saved));
	}
}

TEST_CASE("T62 — A link type nothing knows is saved as simple: same kind",
		  "[elementresavekind][T62]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());

		//None in the collection: the simple element of the samples, given a
		//link type nothing knows. ElementData reads it as simple, and the
		//program builds a simple element for it
	const Sample &simple = samples().at(4);
	QDomDocument unknown = definitionAt(QDir(collectionDir()).filePath(simple.path));
	REQUIRE(linkTypeOf(unknown) == QLatin1String("simple"));
	unknown.documentElement().setAttribute(QStringLiteral("link_type"), QStringLiteral("t62_unknown"));

	QETElementEditor editor;
	editor.fromFile(writtenCopy(dir, QStringLiteral("unknown.elmt"), unknown));
	const QString saved_path = dir.filePath(QStringLiteral("saved-unknown.elmt"));
	REQUIRE(editor.toFile(saved_path));
	const QDomDocument saved = definitionAt(saved_path);

		//What changed in the file, which is the difference this case is
		//about: without it the case proves nothing
	REQUIRE(linkTypeOf(saved) == QLatin1String("simple"));
	CHECK(kindInformationsOf(saved) == simple.kind);
	const Verdict v = verdict(unknown, saved);
	REQUIRE(v.compared);
	CHECK(v.same_kind);
	CHECK(v.same_terminals);
}

TEST_CASE("T62 — Another kind or another value is still a change on saving",
		  "[elementresavekind][T62]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());

	auto savedAfter = [&dir](const QString &path, const QString &name,
							 const std::function<void(ElementData &)> &change) {
		const QDomDocument original = definitionAt(QDir(collectionDir()).filePath(path));
		QETElementEditor editor;
		editor.fromFile(writtenCopy(dir, name, original));
		ElementData data = editor.elementScene()->elementData();
		change(data);
		editor.elementScene()->setElementData(data);
		const QString saved_path = dir.filePath(QStringLiteral("saved-") + name);
		REQUIRE(editor.toFile(saved_path));
		return std::make_pair(original, definitionAt(saved_path));
	};

	const QString master = samples().at(2).path;

	SECTION("another value of a kind information the editor manages")
	{
			//The protection made a coil: the type changes where it was, and
			//what the editor does not manage stays
		const auto [original, saved] = savedAfter(master, QStringLiteral("coil.elmt"),
				[](ElementData &data) {data.m_master_type = ElementData::Coil;});
		CHECK(kindInformationsOf(saved)
			  == QStringList({QStringLiteral("number=1"), QStringLiteral("type=coil"),
							  QStringLiteral("state=NO")}));
		const Verdict v = verdict(original, saved);
		REQUIRE(v.compared);
		CHECK_FALSE(v.same_kind);
	}

	SECTION("another link type: the kind informations of the old one go")
	{
		const auto [original, saved] = savedAfter(master, QStringLiteral("simple.elmt"),
				[](ElementData &data) {data.m_type = ElementData::Simple;});
		CHECK(linkTypeOf(saved) == QLatin1String("simple"));
		CHECK(kindInformationsOf(saved).isEmpty());
		const Verdict v = verdict(original, saved);
		REQUIRE(v.compared);
		CHECK_FALSE(v.same_kind);
	}

	SECTION("a thumbnail made a terminal has the kind informations of a terminal")
	{
		const auto [original, saved] = savedAfter(samples().at(0).path, QStringLiteral("terminal.elmt"),
				[](ElementData &data) {data.m_type = ElementData::Terminal;});
		CHECK(linkTypeOf(saved) == QLatin1String("terminal"));
		CHECK(kindInformationsOf(saved)
			  == QStringList({QStringLiteral("type=generic"), QStringLiteral("function=generic")}));
		const Verdict v = verdict(original, saved);
		REQUIRE(v.compared);
		CHECK_FALSE(v.same_kind);
	}

	SECTION("the link type given back: what was read with it comes back")
	{
			//Made a simple element and saved, then a master again - as an
			//undo of the change of type does - and saved again
		const QDomDocument original = definitionAt(QDir(collectionDir()).filePath(master));
		QETElementEditor editor;
		editor.fromFile(writtenCopy(dir, QStringLiteral("back.elmt"), original));
		ElementScene *scene = editor.elementScene();

		ElementData simple = scene->elementData();
		simple.m_type = ElementData::Simple;
		scene->setElementData(simple);
		const QString simple_path = dir.filePath(QStringLiteral("saved-simple-back.elmt"));
		REQUIRE(editor.toFile(simple_path));
		REQUIRE(kindInformationsOf(definitionAt(simple_path)).isEmpty());

		ElementData master_again = scene->elementData();
		master_again.m_type = ElementData::Master;
		scene->setElementData(master_again);
		const QString back_path = dir.filePath(QStringLiteral("saved-back.elmt"));
		REQUIRE(editor.toFile(back_path));
		const QDomDocument saved = definitionAt(back_path);

		CHECK(kindInformationsOf(saved) == samples().at(2).kind);
		const Verdict v = verdict(original, saved);
		REQUIRE(v.compared);
		CHECK(v.same_kind);
	}
}

	//Hidden: some 8800 symbols, each opened, saved, read back and built as
	//an element, which may take longer than the rest of a run should. Run
	//it with the tag [elementresavekind] or [sweep].
TEST_CASE("T62 — Every symbol of the collection, saved untouched: same kind",
		  "[.][elementresavekind][sweep]")
{
	const QDir root(collectionDir());
	REQUIRE(root.exists());

	QStringList paths;
	QDirIterator it(root.absolutePath(), {QStringLiteral("*.elmt")},
					QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		paths << it.next();
	}
	paths.sort();

	QETElementEditor editor;
	REQUIRE(editor.elementScene() != nullptr);

	QElapsedTimer timer;
	timer.start();
	int checked = 0;
	int with_kind = 0;
	QStringList unread, not_compared, not_kept, other_kind, other_terminals, moved;

	for (const QString &path : std::as_const(paths))
	{
		const QString name = root.relativeFilePath(path);
		const QDomDocument original = definitionAt(path);
		if (original.documentElement().tagName() != QLatin1String("definition")) {
			unread << name;
			continue;
		}
		++checked;
		const QStringList kind = kindInformationsOf(original);
		if (!kind.isEmpty()) {
			++with_kind;
		}

		const Resaved resaved = resavedBy(editor, original);
		const QStringList missing = notKept(kind, kindInformationsOf(resaved.saved));
		if (!missing.isEmpty()) {
			not_kept << name + QStringLiteral(": ") + missing.join(QStringLiteral(", "));
		}

		const Verdict v = verdict(original, resaved.saved);
		if (!v.compared) {
			not_compared << name;
			continue;
		}
		if (!v.same_kind) {
			other_kind << name;
		}
		if (!v.same_terminals) {
				//Named apart when the drawing was off the origin: the one
				//way the editor used to move the terminals of an untouched
				//symbol
			(resaved.moved ? moved : other_terminals) << name;
		}
	}

	const qint64 elapsed = timer.elapsed();
	auto firstOf = [](const QStringList &list) {
		return ascii(list.mid(0, 20).join(QStringLiteral(" | ")));
	};

	INFO("checked " << checked << " symbols, " << with_kind << " with kind informations, in "
		 << elapsed / 1000 << " s");
	CHECK(checked > 8000);
	CHECK(with_kind > 1000);
	{
		INFO("not read: " << firstOf(unread));
		CHECK(unread.isEmpty());
	}
	{
		INFO("not built as an element: " << firstOf(not_compared));
		CHECK(not_compared.isEmpty());
	}
	{
		INFO(not_kept.size() << " lost kind informations: " << firstOf(not_kept));
		CHECK(not_kept.isEmpty());
	}
	{
		INFO(other_kind.size() << " taken for another kind: " << firstOf(other_kind));
		CHECK(other_kind.isEmpty());
	}
	{
		INFO(other_terminals.size() << " with other terminals: " << firstOf(other_terminals));
		CHECK(other_terminals.isEmpty());
	}

	{
		INFO(moved.size() << " moved to the origin by the editor, their terminals with them: "
			 << firstOf(moved));
		CHECK(moved.isEmpty());
	}
	WARN(checked << " symbols saved in " << elapsed / 1000 << " s");
}
