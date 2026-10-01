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
#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/catalog/catalog.h"
#include "../../../../sources/catalog/catalogpart.h"
#include "../../../../sources/catalog/ui/catalogpartdialog.h"
#include "../../../../sources/editor/editorcommands.h"
#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/graphicspart/partimage.h"
#include "../../../../sources/editor/graphicspart/partline.h"
#include "../../../../sources/editor/graphicspart/partrectangle.h"
#include "../../../../sources/editor/graphicspart/partterminal.h"
#include "../../../../sources/editor/ui/rectangleeditor.h"
#include "../../../../sources/editor/ui/imageeditor.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"
#include "../../../../sources/factory/elementpicturefactory.h"
#include <catch2/catch.hpp>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QDialog>
#include <QDomDocument>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTabBar>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

/*
	The layout_drawing block of an element definition: the drawing of the
	part as it sits on a mounting plate, in millimetre, beside the
	description that draws the symbol.

	Two things are proved here, and they live in two places. The reading
	needs no scene: ElementPictureFactory::layoutDrawing() turns the block
	into a picture and a rectangle, and that is what the plate will use.
	The keeping needs the element editor, because the trap is there:
	ElementScene::toXml() builds its document from nothing on every save,
	so a block the scene does not keep is lost the first time anybody
	saves the symbol - with the symbol itself perfectly fine, which is why
	nobody would notice.

	The editor is a real one. Its parts reach the scene through
	element_editor->elementScene() without a null check, so a scene built
	without an editor would test a configuration the program never runs.
*/

namespace {
	const QString shape_style = QStringLiteral(
		"antialias=\"false\" "
		"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"");

	/// A symbol of one line and one terminal, with @a layout_drawing
	/// appended to the definition as it is given. The uuid is a parameter
	/// because ElementPictureFactory caches by it: two files sharing one
	/// would be drawn from whichever was read first.
	QDomDocument symbol(const QString &layout_drawing,
						const QString &uuid = QStringLiteral("{0b3c2c8e-0c4f-4a43-9a52-1d1f5f2b7a10}"))
	{
		const QString xml = QStringLiteral(
			"<definition type=\"element\" width=\"40\" height=\"20\""
			" hotspot_x=\"20\" hotspot_y=\"10\" link_type=\"simple\">"
			"<uuid uuid=\"%3\"/>"
			"<names><name lang=\"fr\">essai</name></names>"
			"<elementInformations/>"
			"<description>"
			"<line x1=\"-10\" y1=\"0\" x2=\"10\" y2=\"0\" end1=\"none\" end2=\"none\""
			" length1=\"1.5\" length2=\"1.5\" %1/>"
			"<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"1\"/>"
			"</description>%2</definition>").arg(shape_style, layout_drawing, uuid);
		QDomDocument doc;
		REQUIRE(doc.setContent(xml));
		return doc;
	}

	/// A rectangle of 45 x 90 mm cut by a line, plus a terminal and a
	/// dynamic text, which are not shapes and must draw nothing.
	QString drawing45x90()
	{
		return QStringLiteral(
			"<layout_drawing>"
			"<rect x=\"0\" y=\"0\" width=\"45\" height=\"90\" rx=\"0\" ry=\"0\" %1/>"
			"<line x1=\"0\" y1=\"45\" x2=\"45\" y2=\"45\" end1=\"none\" end2=\"none\""
			" length1=\"1.5\" length2=\"1.5\" %1/>"
			"<terminal x=\"0\" y=\"0\" orientation=\"n\" name=\"9\"/>"
			"<dynamic_text x=\"0\" y=\"0\" z=\"1\" text_from=\"UserText\""
			" uuid=\"{11111111-2222-3333-4444-555555555555}\" frame=\"false\""
			" text_width=\"-1\"><text>X</text></dynamic_text>"
			"</layout_drawing>").arg(shape_style);
	}

	QString serialized(const QDomElement &element)
	{
		QString out;
		QTextStream stream(&out);
		element.save(stream, -1);
		return out;
	}

	QDomElement layoutBlock(const QDomDocument &doc)
	{
		return doc.documentElement().firstChildElement(
					ElementPictureFactory::layoutDrawingTagName());
	}
}

TEST_CASE("T44 — the layout drawing is read in millimetre, and only its shapes count",
	  "[t44][layoutdrawing]")
{
	const QDomDocument doc = symbol(drawing45x90());
	const auto drawing = ElementPictureFactory::layoutDrawing(doc.documentElement());

		//The rectangle and the line. The terminal and the dynamic text are
		//in the block too, and draw nothing: that is the count proving it.
	CHECK(drawing.parts == 2);
	CHECK(drawing.bounds == QRectF(0, 0, 45, 90));
	CHECK_FALSE(drawing.isNull());
	CHECK_FALSE(drawing.picture.isNull());

	SECTION("no block is no drawing")
	{
		const auto none = ElementPictureFactory::layoutDrawing(
					symbol(QString()).documentElement());
		CHECK(none.isNull());
		CHECK(none.bounds.isNull());
	}

	SECTION("a block holding nothing but what is not a shape is no drawing")
	{
		const auto none = ElementPictureFactory::layoutDrawing(
					symbol(QStringLiteral(
						   "<layout_drawing>"
						   "<terminal x=\"0\" y=\"0\" orientation=\"n\" name=\"9\"/>"
						   "</layout_drawing>")).documentElement());
		CHECK(none.isNull());
	}

	SECTION("a shape its parser refuses is not counted")
	{
			//A rectangle with no height: parseRect() returns before
			//drawing, and a count by tag would call this a drawing.
		const auto none = ElementPictureFactory::layoutDrawing(
					symbol(QStringLiteral(
						   "<layout_drawing><rect x=\"0\" y=\"0\" width=\"45\" %1/>"
						   "</layout_drawing>").arg(shape_style)).documentElement());
		CHECK(none.parts == 0);
		CHECK(none.isNull());
	}

	SECTION("the fraction of a millimetre is kept")
	{
			//22.5 mm is the width of a common modular device, and a whole
			//unit off is the size of the gap the plate warns about.
		const auto narrow = ElementPictureFactory::layoutDrawing(
					symbol(QStringLiteral(
						   "<layout_drawing><rect x=\"0\" y=\"0\" width=\"22.5\""
						   " height=\"90\" rx=\"0\" ry=\"0\" %1/></layout_drawing>")
						   .arg(shape_style)).documentElement());
		CHECK(narrow.bounds.width() == Approx(22.5));
		CHECK(narrow.bounds.height() == Approx(90));
	}

	SECTION("a half arc is bounded by what it draws, not by its whole ellipse")
	{
			//From 0 to 180 degrees, counterclockwise from three o'clock:
			//the top half of a circle 40 mm across.
		const auto arc = ElementPictureFactory::layoutDrawing(
					symbol(QStringLiteral(
						   "<layout_drawing><arc x=\"0\" y=\"0\" width=\"40\" height=\"40\""
						   " start=\"0\" angle=\"180\" %1/></layout_drawing>")
						   .arg(shape_style)).documentElement());
		REQUIRE(arc.parts == 1);
		CHECK(arc.bounds.width() == Approx(40));
		CHECK(arc.bounds.height() == Approx(20));
		CHECK(arc.bounds.top() == Approx(0).margin(1e-9));
	}
}

/*
	From here on the editor is driven the way the person driving it does:
	opened from a symbol file, or on the drawing of a part of the
	catalogue, a shape drawn with the real tool of the tool bar, saved by
	its action. The mouse goes to the scene as scene events, which is where
	the tools read it: going through the view would only add the geometry
	of a window nobody sees.
*/
namespace {
	QString writeSymbol(const QTemporaryDir &dir, const QString &name, const QDomDocument &doc)
	{
		const QString path = dir.filePath(name);
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		file.write(doc.toByteArray());
		file.close();
		return path;
	}

	QDomDocument readFile(const QString &path)
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		QDomDocument doc;
		REQUIRE(doc.setContent(file.readAll()));
		return doc;
	}

	QAction *editorAction(QETElementEditor &editor, const QString &text)
	{
		for (QAction *action : editor.findChildren<QAction *>()) {
			if (action->text() == text) {
				return action;
			}
		}
		FAIL("no action named " << text.toStdString());
		return nullptr;
	}

	QAction *saveAction(QETElementEditor &editor)
	{
		QAction *save = editor.findChild<QAction *>(QStringLiteral("m_save_action"));
		REQUIRE(save);
		return save;
	}

	void sendMouse(ElementScene *scene, QEvent::Type type, const QPointF &pos)
	{
		QGraphicsSceneMouseEvent event(type);
		event.setScenePos(pos);
		event.setButton(type == QEvent::GraphicsSceneMouseMove ? Qt::NoButton : Qt::LeftButton);
		event.setButtons(type == QEvent::GraphicsSceneMousePress ? Qt::LeftButton : Qt::NoButton);
		QApplication::sendEvent(scene, &event);
	}

	void clickAt(ElementScene *scene, const QPointF &pos)
	{
		sendMouse(scene, QEvent::GraphicsSceneMousePress, pos);
		sendMouse(scene, QEvent::GraphicsSceneMouseRelease, pos);
	}

	/// What a person does with a two-click tool of the bar: pick the
	/// tool, click the first corner, move, click the second.
	void drawWith(QETElementEditor &editor, const QString &tool, const QPointF &from, const QPointF &to)
	{
		editorAction(editor, tool)->trigger();
		ElementScene *scene = editor.elementScene();
		clickAt(scene, from);
		sendMouse(scene, QEvent::GraphicsSceneMouseMove, to);
		clickAt(scene, to);
	}

	template <typename Part>
	QList<Part *> partsOf(ElementScene *scene)
	{
		QList<Part *> found;
		for (QGraphicsItem *item : scene->items()) {
			if (auto part = dynamic_cast<Part *>(item)) {
				found << part;
			}
		}
		return found;
	}

	/// The drawing an editor hands back when its "Enregistrer" is used
	QString savedBy(QETElementEditor &editor)
	{
		QString block;
		bool emitted = false;
		const auto connection = QObject::connect(&editor, &QETElementEditor::partDrawingSaved,
							 [&](const QString &saved) {
			block = saved;
			emitted = true;
		});
		saveAction(editor)->trigger();
		QObject::disconnect(connection);
		REQUIRE(emitted);
		return block;
	}

	/// Answers the message boxes that come up while it lives, with
	/// @a button, and counts them. A modal window that is no message box -
	/// the editor itself, opened modal over the window of a part - is left
	/// alone.
	class Answerer
	{
		public:
			explicit Answerer(QMessageBox::StandardButton button) : m_button(button)
			{
				m_timer.setInterval(20);
				QObject::connect(&m_timer, &QTimer::timeout, [this]() {
					auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
					if (!box) {
						return;
					}
					if (QAbstractButton *answer = box->button(m_button)) {
						++m_answered;
						answer->click();
					}
				});
				m_timer.start();
			}
			int answered() const {return m_answered;}

		private:
			QTimer m_timer;
			QMessageBox::StandardButton m_button;
			int m_answered = 0;
	};

	QPushButton *pushButton(QWidget &window, const QString &text)
	{
		for (QPushButton *button : window.findChildren<QPushButton *>()) {
			if (button->text() == text) {
				return button;
			}
		}
		FAIL("no button named " << text.toStdString());
		return nullptr;
	}

	/// The editor "Modifier le dessin 2D…" opens over the window of a part.
	/// The window is shown modal first, the way the program runs it by
	/// exec(): a closing that refuses while any modal window is up passed
	/// the bench where nothing was modal, and would have locked the editor
	/// for good in the program - measured, by planting it back.
	QETElementEditor *openDrawingFrom(CatalogPartDialog &dialog)
	{
		if (!dialog.isVisible()) {
			dialog.setWindowModality(Qt::ApplicationModal);
			dialog.show();
		}
		auto *edit = dialog.findChild<QPushButton *>(QStringLiteral("edit_drawing"));
		REQUIRE(edit);
		edit->click();
		for (QETElementEditor *editor : dialog.findChildren<QETElementEditor *>()) {
			if (editor->isVisible() && editor->isPartDrawingMode()) {
					//Modal over the part: nobody edits the part beneath
				CHECK(editor->windowModality() == Qt::WindowModal);
				return editor;
			}
		}
		return nullptr;
	}

	QPixmap previewOf(CatalogPartDialog &dialog)
	{
		auto *preview = dialog.findChild<QLabel *>(QStringLiteral("drawing_preview"));
		REQUIRE(preview);
		return preview->pixmap();
	}

	QString summaryOf(CatalogPartDialog &dialog)
	{
		auto *summary = dialog.findChild<QLabel *>(QStringLiteral("drawing_summary"));
		REQUIRE(summary);
		return summary->text();
	}

	/// A catalogue in memory holding one part @a code, of the contactor class
	struct OnePart
	{
		explicit OnePart(const QString &code, const QString &drawing = QString())
		{
			QString error;
			REQUIRE(catalog.openInMemory(&error));
			const int contactor = catalog.classByKey(QStringLiteral("contactor")).id;
			REQUIRE(contactor > 0);
			CatalogPart part(code, contactor);
			part.layout_drawing = drawing;
			REQUIRE(catalog.savePart(part, &error));
		}
		Catalog catalog;
	};

	const QString add_rectangle = QStringLiteral("Ajouter un rectangle");
	const QString add_line      = QStringLiteral("Ajouter une ligne");
}

TEST_CASE("CU-44.4 — the symbol is a symbol again: no 2D tab, and no drawing in the file",
	  "[t44][layoutdrawing]")
{
		//A symbol of the first version of this task, with its block
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = writeSymbol(dir, QStringLiteral("ancien.elmt"), symbol(drawing45x90()));

	QETElementEditor editor;
	editor.fromFile(path);
	CHECK_FALSE(editor.isPartDrawingMode());
	CHECK_FALSE(editor.isLayoutDrawingActive());
		//The docks still tab, and their bars are tab bars: what must be
		//gone is the one that switched to the drawing
	for (QTabBar *bar : editor.findChildren<QTabBar *>()) {
		for (int index = 0 ; index < bar->count() ; ++index) {
			CHECK(bar->tabText(index) != QStringLiteral("Dessin 2D"));
		}
	}
	CHECK(editor.layoutDrawingScene()->items().isEmpty());

	REQUIRE(editor.toFile(path));
	const QDomDocument saved = readFile(path);
	CHECK(layoutBlock(saved).isNull());
		//And the symbol is the symbol it was
	const QDomElement description = saved.documentElement().firstChildElement("description");
	CHECK(description.elementsByTagName("line").size() == 1);
	CHECK(description.elementsByTagName("terminal").size() == 1);
}

TEST_CASE("CU-44.5 — the editor of the drawing of a part has the drawing, and nothing else",
	  "[t44][layoutdrawing]")
{
	QETElementEditor editor;
	editor.editPartDrawing(QStringLiteral("LC1D09"), QString());

	CHECK(editor.isPartDrawingMode());
	CHECK(editor.isLayoutDrawingActive());
	CHECK(editor.elementScene() == editor.layoutDrawingScene());
	CHECK(editor.windowTitle().contains(QStringLiteral("LC1D09")));

		//No terminal and no dynamic text on a plate; the shapes all there
	CHECK_FALSE(editorAction(editor, QStringLiteral("Ajouter une borne"))->isEnabled());
	CHECK_FALSE(editorAction(editor, QStringLiteral("Ajouter un champ texte dynamique"))->isEnabled());
	CHECK(editorAction(editor, add_rectangle)->isEnabled());
	CHECK(editorAction(editor, add_line)->isEnabled());

		//None of the actions of a symbol file, and the save that hands the
		//drawing back
	for (const char *name : {"m_new_action", "m_open_action", "m_open_from_file_action",
				 "m_save_as_action", "m_save_as_file_action", "m_reload_action",
				 "m_import_dxf", "m_edit_names_action", "m_edit_author_action",
				 "m_edit_element_properties_action"})
	{
		INFO(name);
		QAction *action = editor.findChild<QAction *>(QString::fromLatin1(name));
		REQUIRE(action);
		CHECK_FALSE(action->isVisible());
		CHECK_FALSE(action->isEnabled());
	}
	CHECK(saveAction(editor)->isVisible());
}

TEST_CASE("T44 — the editor of a drawing hands back what was drawn, and opens it again",
	  "[t44][layoutdrawing]")
{
	QString saved;
	{
		QETElementEditor editor;
		editor.editPartDrawing(QStringLiteral("LC1D09"), QString());
		drawWith(editor, add_rectangle, QPointF(0, 0), QPointF(45, 90));
		drawWith(editor, add_line,      QPointF(0, 45), QPointF(45, 45));
		saved = savedBy(editor);
			//Handed back is saved: nothing left to ask on closing
		CHECK(editor.layoutDrawingScene()->undoStack().isClean());
	}
	const auto drawing = ElementPictureFactory::layoutDrawingOf(saved);
	CHECK(drawing.parts == 2);
	CHECK(drawing.bounds == QRectF(0, 0, 45, 90));

	QETElementEditor reopened;
	reopened.editPartDrawing(QStringLiteral("LC1D09"), saved);
	const auto rects = partsOf<PartRectangle>(reopened.layoutDrawingScene());
	REQUIRE(rects.size() == 1);
	CHECK(rects.first()->sceneGeometricRect() == QRectF(0, 0, 45, 90));
	CHECK(partsOf<PartLine>(reopened.layoutDrawingScene()).size() == 1);
		//Opening is not an edit
	CHECK_FALSE(reopened.layoutDrawingScene()->undoStack().canUndo());

		//And a drawing emptied hands back nothing, not an empty block. Saved
		//once with a line first: with nothing changed the action is off,
		//which is right and is not what is asked here.
	QETElementEditor empty;
	empty.editPartDrawing(QStringLiteral("LC1D12"), QString());
	drawWith(empty, add_line, QPointF(0, 0), QPointF(10, 0));
	CHECK_FALSE(savedBy(empty).isEmpty());
	empty.layoutDrawingScene()->undoStack().undo();
	CHECK(savedBy(empty).isEmpty());
}

TEST_CASE("CU-44.1 — the drawing of a part survives saving and reopening it in the catalogue",
	  "[t44][layoutdrawing]")
{
	OnePart bench(QStringLiteral("LC1D09"));
	{
		CatalogPartDialog dialog(&bench.catalog, bench.catalog.partByCode(QStringLiteral("LC1D09")));
		CHECK(previewOf(dialog).isNull());
		QETElementEditor *editor = openDrawingFrom(dialog);
		REQUIRE(editor);
		CHECK(editor->windowTitle().contains(QStringLiteral("LC1D09")));
		drawWith(*editor, add_rectangle, QPointF(0, 0), QPointF(45, 90));
		drawWith(*editor, add_line,      QPointF(0, 45), QPointF(45, 45));
		saveAction(*editor)->trigger();
		const QPointer<QETElementEditor> closing(editor);
		CHECK(editor->close());

			//The window of the part has it and shows it - and nothing is in
			//the catalogue before its own "Enregistrer"
		CHECK_FALSE(previewOf(dialog).isNull());
		CHECK(summaryOf(dialog).contains(QStringLiteral("45")));
		CHECK(bench.catalog.partByCode(QStringLiteral("LC1D09")).layout_drawing.isEmpty());

		Answerer yes(QMessageBox::Yes);
		pushButton(dialog, QStringLiteral("Enregistrer"))->click();
		CHECK(yes.answered() == 1);
	}
	const CatalogPart saved = bench.catalog.partByCode(QStringLiteral("LC1D09"));
	const auto drawing = ElementPictureFactory::layoutDrawingOf(saved.layout_drawing);
	CHECK(drawing.parts == 2);
	CHECK(drawing.bounds == QRectF(0, 0, 45, 90));

		//Opened again: the window shows it, and the editor has the two parts
		//where they were
	CatalogPartDialog again(&bench.catalog, saved);
	CHECK_FALSE(previewOf(again).isNull());
	QETElementEditor *reopened = openDrawingFrom(again);
	REQUIRE(reopened);
	const auto rects = partsOf<PartRectangle>(reopened->layoutDrawingScene());
	REQUIRE(rects.size() == 1);
	CHECK(rects.first()->sceneGeometricRect() == QRectF(0, 0, 45, 90));
	CHECK(partsOf<PartLine>(reopened->layoutDrawingScene()).size() == 1);
	CHECK(reopened->close());
}

TEST_CASE("CU-44.3 — saving the part from its window keeps the drawing, and a new revision carries it",
	  "[t44][layoutdrawing]")
{
	const QString block = QStringLiteral(
		"<layout_drawing><rect x=\"0\" y=\"0\" width=\"45\" height=\"90\" %1/></layout_drawing>")
		.arg(shape_style);
	OnePart bench(QStringLiteral("GV2ME10"), block);
	{
			//Saved in place without opening the drawing
		CatalogPartDialog dialog(&bench.catalog, bench.catalog.partByCode(QStringLiteral("GV2ME10")));
		Answerer yes(QMessageBox::Yes);
		pushButton(dialog, QStringLiteral("Enregistrer"))->click();
		REQUIRE(yes.answered() == 1);
	}
	CHECK(bench.catalog.partByCode(QStringLiteral("GV2ME10")).layout_drawing == block);
	{
		CatalogPartDialog dialog(&bench.catalog, bench.catalog.partByCode(QStringLiteral("GV2ME10")));
		Answerer yes(QMessageBox::Yes);
		pushButton(dialog, QStringLiteral("Enregistrer comme nouvelle révision"))->click();
		REQUIRE(yes.answered() == 1);
	}
	const CatalogPart current = bench.catalog.partByCode(QStringLiteral("GV2ME10"));
	CHECK(current.revision == 2);
	CHECK(current.layout_drawing == block);
}

TEST_CASE("CU-44.6 — closing the drawing without saving leaves the part as it was",
	  "[t44][layoutdrawing]")
{
	OnePart bench(QStringLiteral("LC1D18"));
	CatalogPartDialog dialog(&bench.catalog, bench.catalog.partByCode(QStringLiteral("LC1D18")));

	QETElementEditor *first = openDrawingFrom(dialog);
	REQUIRE(first);
	drawWith(*first, add_line, QPointF(0, 0), QPointF(45, 0));
	{
		Answerer no(QMessageBox::No);
		CHECK(first->close());
		CHECK(no.answered() == 1);
	}
	CHECK(previewOf(dialog).isNull());
	CHECK(summaryOf(dialog).isEmpty());

		//"Annuler" keeps the editor, and what was drawn in it
	QETElementEditor *second = openDrawingFrom(dialog);
	REQUIRE(second);
	REQUIRE(second != first);
	drawWith(*second, add_line, QPointF(0, 0), QPointF(45, 0));
	{
		Answerer cancel(QMessageBox::Cancel);
		CHECK_FALSE(second->close());
		CHECK(cancel.answered() == 1);
	}
	CHECK(second->isVisible());
	CHECK(partsOf<PartLine>(second->layoutDrawingScene()).size() == 1);

		//And saving hands it to the window of the part
	saveAction(*second)->trigger();
	CHECK_FALSE(previewOf(dialog).isNull());
	CHECK(second->close());
}

TEST_CASE("T44 — the editor of a drawing refuses what is not a shape, pasted or loaded",
	  "[t44][layoutdrawing]")
{
	QETElementEditor editor;
		//Loaded: the fixture has a terminal and a dynamic text in its block
	QDomDocument document;
	document.appendChild(document.importNode(layoutBlock(symbol(drawing45x90())), true));
	editor.editPartDrawing(QStringLiteral("LC1D09"), document.toString(-1));
	CHECK(partsOf<PartRectangle>(editor.layoutDrawingScene()).size() == 1);
	CHECK(partsOf<PartLine>(editor.layoutDrawingScene()).size() == 1);
	CHECK(partsOf<PartTerminal>(editor.layoutDrawingScene()).isEmpty());

		//Pasted: a whole symbol leaves its line, and not its terminal
	editor.elementScene()->fromXml(symbol(QString()), QPointF(5, 5), false);
	CHECK(partsOf<PartLine>(editor.layoutDrawingScene()).size() == 2);
	CHECK(partsOf<PartTerminal>(editor.layoutDrawingScene()).isEmpty());
}

TEST_CASE("CU-44.15 — the fields of the drawing say mm, and those of the symbol do not",
	  "[t44][layoutdrawing]")
{
		//The editor of a part is shown in the tools dock only once a part
		//of its kind is selected
	auto fieldsShownFor = [](QETElementEditor &editor) {
		QList<PartRectangle *> rectangles = partsOf<PartRectangle>(editor.elementScene());
		REQUIRE(rectangles.size() == 1);
		editor.elementScene()->clearSelection();
		rectangles.first()->setSelected(true);
		QList<QDoubleSpinBox *> fields;
		for (RectangleEditor *shown : editor.findChildren<RectangleEditor *>()) {
			fields << shown->findChildren<QDoubleSpinBox *>();
		}
		return fields;
	};

	QETElementEditor drawing;
	drawing.editPartDrawing(QStringLiteral("LC1D09"), QString());
	drawWith(drawing, add_rectangle, QPointF(0, 0), QPointF(45, 90));
	const QList<QDoubleSpinBox *> in_mm = fieldsShownFor(drawing);
		//x, y, width, height and the two radii
	REQUIRE(in_mm.size() == 6);
	for (QDoubleSpinBox *field : in_mm) {
		CHECK(field->suffix() == QStringLiteral(" mm"));
	}

		//On a symbol the unit is the folio's, and a millimetre there would
		//be a lie
	QETElementEditor symbol_editor;
	symbol_editor.elementScene()->fromXml(symbol(QString()));
	drawWith(symbol_editor, add_rectangle, QPointF(-5, -5), QPointF(5, 5));
	const QList<QDoubleSpinBox *> on_symbol = fieldsShownFor(symbol_editor);
	REQUIRE(on_symbol.size() == 6);
	for (QDoubleSpinBox *field : on_symbol) {
		CHECK(field->suffix().isEmpty());
	}
}

/*
	The image of the drawing: the photo or the datasheet drawing of the
	part, inside the part, at the size the person says the part has.
*/

namespace {
	/// A raster file of @a size pixels, all of @a colour, written at
	/// @a per_metre dots a metre so that its size in millimetre is known
	QByteArray rasterFile(const char *format, const QSize &size, const QColor &colour,
			      int per_metre = 10000)
	{
		QImage image(size, QImage::Format_RGB32);
		image.fill(colour);
		image.setDotsPerMeterX(per_metre);
		image.setDotsPerMeterY(per_metre);
		QByteArray bytes;
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, format);
		return bytes;
	}

	/// An SVG drawn in millimetre: a red box of 45 by 90
	QByteArray svgFile()
	{
		return QByteArrayLiteral(
			"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"45mm\" height=\"90mm\""
			" viewBox=\"0 0 45 90\">"
			"<rect x=\"0\" y=\"0\" width=\"45\" height=\"90\" fill=\"#ff0000\"/></svg>");
	}

	QString writeBytes(const QTemporaryDir &dir, const QString &name, const QByteArray &bytes)
	{
		const QString path = dir.filePath(name);
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		file.write(bytes);
		return path;
	}

	QDomElement imageElement(QDomDocument &doc, const QByteArray &bytes, const QString &format,
				 const QRectF &rect)
	{
		QDomElement image = doc.createElement(QStringLiteral("image"));
		image.setAttribute(QStringLiteral("x"), rect.x());
		image.setAttribute(QStringLiteral("y"), rect.y());
		image.setAttribute(QStringLiteral("width"), rect.width());
		image.setAttribute(QStringLiteral("height"), rect.height());
		image.setAttribute(QStringLiteral("format"), format);
		image.appendChild(doc.createTextNode(QString::fromLatin1(bytes.toBase64())));
		return image;
	}

	const QString insert_image = QStringLiteral("Insérer une image");
}

TEST_CASE("T44 — an image file says its size, from its resolution or its units",
	  "[t44][layoutdrawing]")
{
		//400 by 800 pixels at 10 000 a metre are 40 by 80 mm
	const QSizeF png = PartImage::fileSizeMm(rasterFile("PNG", QSize(400, 800), Qt::red), QStringLiteral("png"));
	CHECK(png.width()  == Approx(40.0));
	CHECK(png.height() == Approx(80.0));
	const QSizeF jpg = PartImage::fileSizeMm(rasterFile("JPG", QSize(400, 800), Qt::red), QStringLiteral("jpg"));
	CHECK(jpg.width()  == Approx(40.0).margin(0.1));
	CHECK(jpg.height() == Approx(80.0).margin(0.1));
		//And an SVG written in millimetre comes back at its size
	const QSizeF svg = PartImage::fileSizeMm(svgFile(), QStringLiteral("svg"));
	CHECK(svg.width()  == Approx(45.0).margin(0.05));
	CHECK(svg.height() == Approx(90.0).margin(0.05));

		//Bytes that are no such file, or a format that is not kept, are nothing
	CHECK(PartImage::fileSizeMm(QByteArrayLiteral("not an image"), QStringLiteral("png")).isEmpty());
	CHECK(PartImage::fileSizeMm(rasterFile("PNG", QSize(4, 4), Qt::red), QStringLiteral("bmp")).isEmpty());

	CHECK(PartImage::formatOfFile(QStringLiteral("/a/datasheet.PNG")) == QStringLiteral("png"));
	CHECK(PartImage::formatOfFile(QStringLiteral("photo.jpeg")) == QStringLiteral("jpg"));
	CHECK(PartImage::formatOfFile(QStringLiteral("drawing.svg")) == QStringLiteral("svg"));
	CHECK(PartImage::formatOfFile(QStringLiteral("scan.bmp")).isEmpty());
}

TEST_CASE("CU-44.12 — inserting an image asks the real size of the part",
	  "[t44][layoutdrawing]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString png = writeBytes(dir, QStringLiteral("datasheet.png"),
				       rasterFile("PNG", QSize(400, 800), Qt::red));

		//On a symbol there is nothing to insert an image into
	const QString symbol_path = writeSymbol(dir, QStringLiteral("piece.elmt"), symbol(QString()));
	QETElementEditor symbol_editor;
	symbol_editor.fromFile(symbol_path);
	CHECK_FALSE(editorAction(symbol_editor, insert_image)->isEnabled());
	CHECK_FALSE(symbol_editor.insertImageFile(png));
	CHECK(partsOf<PartImage>(symbol_editor.elementScene()).isEmpty());

	QETElementEditor editor;
	editor.editPartDrawing(QStringLiteral("LC1D09"), QString());
	CHECK(editorAction(editor, insert_image)->isEnabled());

		//The question answered the way a person answers it: the width typed
		//in, the height following it in proportion, then the proportion let
		//go and the height typed in apart
	double asked_width = 0.0, asked_height = 0.0, followed_height = 0.0;
	bool answered = false;
	QTimer answer;
	answer.setInterval(20);
	int tries = 200;
	QObject::connect(&answer, &QTimer::timeout, [&]() {
		auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
		if (!dialog || dialog->objectName() != QStringLiteral("image_size_dialog")) {
			if (--tries <= 0) {
				answer.stop();
			}
			return;
		}
		answer.stop();
		auto *width  = dialog->findChild<QDoubleSpinBox *>(QStringLiteral("image_width"));
		auto *height = dialog->findChild<QDoubleSpinBox *>(QStringLiteral("image_height"));
		auto *keep   = dialog->findChild<QCheckBox *>(QStringLiteral("image_keep_proportion"));
		if (!width || !height || !keep) {
			dialog->reject();
			return;
		}
		asked_width  = width->value();
		asked_height = height->value();
		width->setValue(22.5);
		followed_height = height->value();
		keep->setChecked(false);
		height->setValue(90.0);
		answered = true;
		dialog->accept();
	});
	answer.start();
	REQUIRE(editor.insertImageFile(png));
	REQUIRE(answered);
		//It started at the size the file says, in its proportion
	CHECK(asked_width  == Approx(40.0));
	CHECK(asked_height == Approx(80.0));
	CHECK(followed_height == Approx(45.0));

	const auto images = partsOf<PartImage>(editor.layoutDrawingScene());
	REQUIRE(images.size() == 1);
	CHECK(images.first()->sceneGeometricRect() == QRectF(-11.25, -45, 22.5, 90));
	CHECK(images.first()->isSelected());
	CHECK_FALSE(images.first()->isNull());
		//Grabbed anywhere on it, where a rectangle is grabbed by its outline
	CHECK(images.first()->shape().contains(images.first()->rect().center()));

		//One step of undo takes it out again
	editor.layoutDrawingScene()->undoStack().undo();
	CHECK(partsOf<PartImage>(editor.layoutDrawingScene()).isEmpty());
}

TEST_CASE("CU-44.13 — the image is scaled freely, and stays inside the part",
	  "[t44][layoutdrawing]")
{
	const QByteArray png = rasterFile("PNG", QSize(40, 80), Qt::red);
	QString saved;
	{
		QETElementEditor editor;
		editor.editPartDrawing(QStringLiteral("LC1D09"), QString());
		PartImage *image = editor.addLayoutImage(png, QStringLiteral("png"), QSizeF(22.5, 90));
		REQUIRE(image);

			//The fields of the part, shown because the image is selected
		ImageEditor *fields = nullptr;
		for (ImageEditor *shown : editor.findChildren<ImageEditor *>()) {
			fields = shown;
		}
		REQUIRE(fields);
		auto *width  = fields->findChild<QDoubleSpinBox *>(QStringLiteral("m_width_sb"));
		auto *height = fields->findChild<QDoubleSpinBox *>(QStringLiteral("m_height_sb"));
		REQUIRE(width);
		REQUIRE(height);
		CHECK(width->suffix() == QStringLiteral(" mm"));

			//25 by 85: not the proportion of the file, and kept as asked
		width->setValue(25.0);
		emit width->editingFinished();
		height->setValue(85.0);
		emit height->editingFinished();
		CHECK(image->rect().size() == QSizeF(25, 85));
		saved = savedBy(editor);
	}

		//The drawing holds the image itself
	QDomDocument document;
	REQUIRE(document.setContent(saved));
	const QDomElement written = document.documentElement().firstChildElement(QStringLiteral("image"));
	REQUIRE_FALSE(written.isNull());
	CHECK(written.attribute(QStringLiteral("format")) == QStringLiteral("png"));
	CHECK(written.attribute(QStringLiteral("width"))  == QStringLiteral("25"));
	CHECK(written.attribute(QStringLiteral("height")) == QStringLiteral("85"));
	CHECK(QByteArray::fromBase64(written.text().toLatin1()) == png);

		//Opened again with no file of it anywhere on the disk
	QETElementEditor reopened;
	reopened.editPartDrawing(QStringLiteral("LC1D09"), saved);
	const auto images = partsOf<PartImage>(reopened.layoutDrawingScene());
	REQUIRE(images.size() == 1);
	CHECK(images.first()->rect().size() == QSizeF(25, 85));
	CHECK_FALSE(images.first()->isNull());
	CHECK(images.first()->fileData() == png);
}

TEST_CASE("T44 — an image pasted on a symbol is refused, and kept on a drawing",
	  "[t44][layoutdrawing]")
{
		//What the clipboard holds after copying an image of a drawing
	QDomDocument pasted = symbol(QString());
	pasted.documentElement().firstChildElement(QStringLiteral("description")).appendChild(
				imageElement(pasted, rasterFile("PNG", QSize(10, 10), Qt::red),
					     QStringLiteral("png"), QRectF(0, 0, 10, 10)));

	QETElementEditor symbol_editor;
	ElementScene *symbol_scene = symbol_editor.elementScene();
	symbol_scene->fromXml(symbol(QString()));
	symbol_scene->fromXml(pasted, QPointF(5, 5), false);
	CHECK(partsOf<PartImage>(symbol_scene).isEmpty());

	QETElementEditor drawing;
	drawing.editPartDrawing(QStringLiteral("LC1D09"), QString());
	drawing.elementScene()->fromXml(pasted, QPointF(5, 5), false);
	CHECK(partsOf<PartImage>(drawing.layoutDrawingScene()).size() == 1);
}

TEST_CASE("T44 — an image this program cannot draw is kept as it came",
	  "[t44][layoutdrawing]")
{
		//A format of a later version, or bytes damaged on the way: drawn as
		//a crossed box, and written back untouched
	QDomDocument doc;
	QDomElement element = doc.createElement(QStringLiteral("image"));
	element.setAttribute(QStringLiteral("x"), 0);
	element.setAttribute(QStringLiteral("y"), 0);
	element.setAttribute(QStringLiteral("width"), 20);
	element.setAttribute(QStringLiteral("height"), 30);
	element.setAttribute(QStringLiteral("format"), QStringLiteral("webp"));
	element.appendChild(doc.createTextNode(QString::fromLatin1(QByteArrayLiteral("RIFF....WEBP").toBase64())));

	QETElementEditor editor;
	PartImage image(&editor);
	image.fromXml(element);
	CHECK(image.isNull());
	CHECK_FALSE(image.isUseless());
	const QDomElement back = image.toXml(doc);
	CHECK(back.attribute(QStringLiteral("format")) == QStringLiteral("webp"));
	CHECK(back.text() == element.text());

		//And the plate counts nothing it could not draw
	QDomDocument block;
	block.appendChild(block.createElement(QStringLiteral("layout_drawing")));
	block.documentElement().appendChild(block.importNode(element, true));
	CHECK(ElementPictureFactory::layoutDrawingOf(block.documentElement()).parts == 0);
}

TEST_CASE("T44 — rotating, flipping or mirroring leaves an image as it is drawn",
	  "[t44][layoutdrawing]")
{
		//Found on the screen on 2026-10-01: Space turned the image by its
		//transform, sideways, with the fields still saying the old width
		//and height, and nothing of the turn in the file
	QETElementEditor editor;
	editor.editPartDrawing(QStringLiteral("LC1D09"), QString());
	PartImage *image = editor.addLayoutImage(rasterFile("PNG", QSize(40, 80), Qt::red),
						 QStringLiteral("png"), QSizeF(45, 88));
	REQUIRE(image);
	const QRectF drawn = image->sceneGeometricRect();
	ElementScene *scene = editor.layoutDrawingScene();
	REQUIRE(scene->selectedItems() == QList<QGraphicsItem *>{image});

	scene->undoStack().push(new RotateElementsCommand(scene));
	CHECK(image->QGraphicsItem::rotation() == 0.0);
	CHECK(image->sceneGeometricRect() == drawn);
	scene->undoStack().undo();
	CHECK(image->QGraphicsItem::rotation() == 0.0);
	CHECK(image->sceneGeometricRect() == drawn);

	scene->undoStack().push(new FlipElementsCommand(scene));
	scene->undoStack().push(new MirrorElementsCommand(scene));
	CHECK(image->sceneGeometricRect() == drawn);
}
