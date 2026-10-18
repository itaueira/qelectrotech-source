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
#include "../../../../sources/editor/graphicspart/partarc.h"
#include "../../../../sources/editor/graphicspart/partdynamictextfield.h"
#include "../../../../sources/editor/graphicspart/partline.h"
#include "../../../../sources/editor/graphicspart/partpolygon.h"
#include "../../../../sources/editor/graphicspart/partterminal.h"
#include "../../../../sources/editor/graphicspart/parttext.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"

#include <catch2/catch.hpp>

#include <QByteArray>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QGraphicsRectItem>
#include <QIODevice>
#include <QLatin1String>
#include <QList>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <new>
#include <vector>

/*
	The order in which the symbol editor writes the parts of a symbol.

	ElementScene::zItems() sorts the drawing by z value and puts the
	terminals after it. The terminals used to come in the order
	QGraphicsScene::items() lists them, and so did the parts of the same z
	value. A scene without an index - the element editor uses none - lists
	its items in the order they were added only until one item is removed,
	and from then on sorted by address (QGraphicsSceneLinearIndex::
	removeItem()). The order of the terminals in the saved file was then
	the order the heap had given them: the same symbol, saved twice with no
	change, could come out as two different files.

	The cases open a power contact of the collection the program ships
	with: eight terminals closing its description, and a drawing whose z
	values follow the order of the file. They make the scene list its items
	by address, as any removal does - a selection handle is enough -, and
	save twice. Both files must list the parts in the order of the source,
	and be the same.

	Whether the heap gave the terminals addresses out of the order of the
	file is measured on each run, since that is what the defect needed to
	show: a run where it never happens could not have caught it, and says
	so.
*/

namespace
{
	QByteArray fileContent(const QString &path)
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		return file.readAll();
	}

	/// The power contact: eight terminals, the last eight lines of its description.
	QByteArray contactSymbol()
	{
		const QByteArray bytes = fileContent(QStringLiteral(
			QET_EXAMPLES_DIR "/../elements/10_electric/10_allpole/"
			"310_relays_contactors_contacts/02_contacts_cross_referencing/"
			"02_power_contacts/com_puiss6.elmt"));
		REQUIRE_FALSE(bytes.isEmpty());
		return bytes;
	}

	/**
		@return a writable copy of @a content, named @a name in @a dir.
		Written rather than QFile::copy'd from the collection: a read-only
		copy would open the editor's read-only warning, a modal box nothing
		here could close.
	*/
	QString writtenCopy(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
	{
		const QString path = dir.filePath(name);
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		REQUIRE(file.write(content) == content.size());
		return path;
	}

	/**
		@return what names a part of a symbol in a file: the uuid of a
		terminal or of a dynamic text, which both come from the file; the
		text of a static text, whose uuid does not when the file is older
		than it; the geometry of a shape, whose uuid does not either.
	*/
	QString identity(const QDomElement &part)
	{
		const QString tag = part.tagName();
		if (tag == QLatin1String("terminal") || tag == QLatin1String("dynamic_text")) {
			return tag + QLatin1Char(' ') + part.attribute(QStringLiteral("uuid"));
		}
		if (tag == QLatin1String("text")) {
			return tag + QLatin1Char(' ') + part.attribute(QStringLiteral("text"));
		}
		QString id = tag;
		static const char *const geometry[] = {"x", "y", "x1", "y1", "x2", "y2", "width", "height"};
		for (const char *name : geometry) {
			const QString attribute = QString::fromLatin1(name);
			if (part.hasAttribute(attribute)) {
				id += QStringLiteral(" %1=%2").arg(
					attribute, QString::number(part.attribute(attribute).toDouble()));
			}
		}
		return id;
	}

	/// @return the parts of the <description> of @a file, in the order of the file
	QStringList partsInOrder(const QByteArray &file)
	{
		QDomDocument document;
		REQUIRE(document.setContent(file));
		const QDomElement description =
			document.documentElement().firstChildElement(QStringLiteral("description"));
		REQUIRE_FALSE(description.isNull());
		QStringList parts;
		for (QDomElement part = description.firstChildElement() ; !part.isNull() ;
		     part = part.nextSiblingElement()) {
			parts << identity(part);
		}
		return parts;
	}

	/// @return the terminals of @a parts, in their order
	QStringList terminalsOf(const QStringList &parts)
	{
		QStringList terminals;
		for (const QString &part : parts) {
			if (part.startsWith(QLatin1String("terminal "))) {
				terminals << part;
			}
		}
		return terminals;
	}

	/// @return the <description> of @a file, as it is written
	QByteArray descriptionOf(const QByteArray &file)
	{
		const qsizetype from = file.indexOf("<description>");
		const qsizetype to = file.indexOf("</description>");
		REQUIRE(from >= 0);
		REQUIRE(to > from);
		return file.mid(from, to - from);
	}

	/**
		Frees blocks of the size of the parts of a symbol, in a shuffled
		order, just before one is read: its parts are then likely to land in
		them, at addresses that do not follow the order they are created in.
		A heap that hands back the small block last freed first (most do), or
		one at random (the low fragmentation heap of Windows), makes the
		order by address differ from the order of the file - which is what
		the defect needed to show, and what a fresh heap handing out
		increasing addresses would have hidden.
	*/
	void shuffleTheHeap(QRandomGenerator &random)
	{
		const std::size_t sizes[] = {
			sizeof(PartTerminal), sizeof(PartLine), sizeof(PartArc),
			sizeof(PartPolygon), sizeof(PartText), sizeof(PartDynamicTextField)};
		std::vector<void *> blocks;
		blocks.reserve(std::size(sizes) * 32);
		for (const std::size_t size : sizes) {
			for (int i = 0 ; i < 32 ; ++ i) {
				blocks.push_back(::operator new(size));
			}
		}
		std::shuffle(blocks.begin(), blocks.end(), random);
		for (void *block : blocks) {
			::operator delete(block);
		}
	}

	/**
		Adds an item to @a scene and removes it, the least a person does
		with a selection handle: from then on a scene without an index
		lists its items by address (QGraphicsSceneLinearIndex::removeItem()).
		A rectangle, which ElementScene::zItems() counts as a tool of the
		editor and never saves.
	*/
	void listByAddress(ElementScene *scene)
	{
		QGraphicsRectItem probe(0, 0, 1, 1);
		scene->addItem(&probe);
		scene->removeItem(&probe);
	}

	/**
		@return the terminals of @a scene, in the order
		QGraphicsScene::items() lists them: the order the editor used to
		save them in
	*/
	QStringList terminalsAsListed(ElementScene *scene)
	{
		QStringList listed;
		const QList<QGraphicsItem *> items = scene->items();
		for (QGraphicsItem *item : items) {
			if (auto terminal = qgraphicsitem_cast<PartTerminal *>(item)) {
				listed << QStringLiteral("terminal ") + terminal->uuid().toString();
			}
		}
		return listed;
	}

	/// @return the terminal of @a scene that has @a uuid, nullptr if none
	PartTerminal *terminalWith(ElementScene *scene, const QUuid &uuid)
	{
		const QList<QGraphicsItem *> items = scene->items();
		for (QGraphicsItem *item : items) {
			auto terminal = qgraphicsitem_cast<PartTerminal *>(item);
			if (terminal && terminal->uuid() == uuid) {
				return terminal;
			}
		}
		return nullptr;
	}

	/**
		Saves @a editor twice, listing the scene by address again between
		the two, and returns the two files in @a first and @a second.
	*/
	void saveTwice(QETElementEditor &editor, const QTemporaryDir &dir, int round,
		       QByteArray *first, QByteArray *second)
	{
		const QString first_path = dir.filePath(QStringLiteral("first-%1.elmt").arg(round));
		REQUIRE(editor.toFile(first_path));
		listByAddress(editor.elementScene());
		const QString second_path = dir.filePath(QStringLiteral("second-%1.elmt").arg(round));
		REQUIRE(editor.toFile(second_path));
		*first = fileContent(first_path);
		*second = fileContent(second_path);
	}
}

TEST_CASE("The symbol editor saves the terminals of a symbol in the order of its file",
	  "[elementeditor][ui]")
{
	const QByteArray source = contactSymbol();
	const QStringList in_file = partsInOrder(source);
	const QStringList terminals_in_file = terminalsOf(in_file);
	REQUIRE(terminals_in_file.size() == 8);
		// The terminals close the description, as the editor writes them:
		// the whole file is then the order a save must give back.
	REQUIRE(in_file.mid(in_file.size() - 8) == terminals_in_file);
	const QUuid third = QUuid::fromString(
		terminals_in_file.at(2).mid(QStringLiteral("terminal ").size()));
	REQUIRE_FALSE(third.isNull());

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QRandomGenerator random(20261018);
	const int rounds = 10;
	int out_of_order = 0;

	for (int round = 0 ; round < rounds ; ++ round)
	{
		INFO("round " << round);
		const QString symbol = writtenCopy(
			dir, QStringLiteral("contact-%1.elmt").arg(round), source);

		QETElementEditor editor;
		shuffleTheHeap(random);
		editor.fromFile(symbol);
		ElementScene *scene = editor.elementScene();
		REQUIRE(scene != nullptr);

		listByAddress(scene);
		if (terminalsAsListed(scene) != terminals_in_file) {
			++ out_of_order;
		}

			// A terminal deleted and the deletion undone: the scene lists it
			// last, after the others, and it must still be saved third.
		PartTerminal *terminal = terminalWith(scene, third);
		REQUIRE(terminal != nullptr);
		scene->clearSelection();
		terminal->setSelected(true);
		scene->slot_delete();
		REQUIRE(terminal->scene() == nullptr);
		scene->undoStack().undo();
		REQUIRE(terminal->scene() == scene);
		scene->clearSelection();

		QByteArray first;
		QByteArray second;
		saveTwice(editor, dir, round, &first, &second);
		CHECK(partsInOrder(first) == in_file);
		CHECK(partsInOrder(second) == in_file);
		CHECK(descriptionOf(first) == descriptionOf(second));
	}

	if (out_of_order == 0) {
		WARN("In none of the " << rounds << " rounds did the heap give the terminals "
		     "addresses out of the order of the file: this run proves the deletion "
		     "undone, not the order by address.");
	}
}

TEST_CASE("The symbol editor saves the parts of the same z value in the order of their file",
	  "[elementeditor][ui]")
{
	const QByteArray source = contactSymbol();
	const QStringList in_file = partsInOrder(source);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QRandomGenerator random(20261019);

	for (int round = 0 ; round < 5 ; ++ round)
	{
		INFO("round " << round);
		const QString symbol = writtenCopy(
			dir, QStringLiteral("contact-%1.elmt").arg(round), source);

		QETElementEditor editor;
		shuffleTheHeap(random);
		editor.fromFile(symbol);
		ElementScene *scene = editor.elementScene();
		REQUIRE(scene != nullptr);

			// Every part of the drawing on one z value, as a paste gives
			// the parts it brings the z values the others already have:
			// only the order of the file is left to tell them apart.
		int drawing = 0;
		const QList<QGraphicsItem *> items = scene->items();
		for (QGraphicsItem *item : items) {
			if (dynamic_cast<CustomElementPart *>(item)
			    && !qgraphicsitem_cast<PartTerminal *>(item)) {
				item->setZValue(1);
				++ drawing;
			}
		}
		REQUIRE(drawing == in_file.size() - 8);
		listByAddress(scene);

		QByteArray first;
		QByteArray second;
		saveTwice(editor, dir, round, &first, &second);
		CHECK(partsInOrder(first) == in_file);
		CHECK(partsInOrder(second) == in_file);
		CHECK(descriptionOf(first) == descriptionOf(second));
	}
}
