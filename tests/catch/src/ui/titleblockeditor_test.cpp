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

#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/titleblock/qettemplateeditor.h"
#include "../../../../sources/titleblock/templatecellwidget.h"
#include "../../../../sources/titleblock/templateview.h"
#include "../../../../sources/titleblock/templatevisualcell.h"
#include "../../../../sources/titleblockcell.h"
#include "../../../../sources/titleblocktemplate.h"

#include <catch2/catch.hpp>

#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFile>
#include <QGraphicsScene>
#include <QIODevice>
#include <QImage>
#include <QLineEdit>
#include <QPainter>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QStyleOptionGraphicsItem>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

/*
	The title block editor, and the cell that showed nothing.

	The rule itself is arithmetic over strings and is proved next door, in
	C_unittests (titleblockauthoring_test.cpp). What is proved here is the
	other half, and it needs the real thing: a TitleBlockTemplate read from a
	file, whose cells produce their final text through the same substitution
	the folio uses, and the preview item that paints them. TitleBlockTemplate
	reaches QETApp - one call, for the font of a cell - so it only links here,
	where qelectrotech_objects brings the whole program in.

	The three ways a variable reaches a cell are all in the template below,
	because each of them was broken in its own way:

	  - braced, in the value  - was found, and drawn blank;
	  - bare, in the value    - was not even found: listOfVariables()
	                            collected the braced form alone, and the
	                            title block shipped with the program is
	                            written with nothing but the bare one;
	  - braced, in the label  - was not found either, although
	                            finalTextForCell() does substitute in the
	                            label, so the project never offered a field
	                            for it.
*/

namespace
{
	const int kCellWidth  = 200;
	const int kCellHeight = 25;

	/**
		A title block small enough to read at a glance, carrying one cell of
		each kind above, one fixed text - so that "nothing else was touched"
		has something to be said about - and the pair of cells the painting
		section compares.
	*/
	QString benchTemplate()
	{
		return QStringLiteral(
			"<titleblocktemplate name=\"bancada\">\n"
			"  <information></information>\n"
			"  <logos/>\n"
			"  <grid cols=\"t50%;t50%;\" rows=\"25;25;25;\">\n"
			"    <field row=\"0\" col=\"0\" name=\"autoria\""
			" displaylabel=\"true\" align=\"left\" valign=\"center\">\n"
			"      <value><translation lang=\"en\">%author"
			"</translation></value>\n"
			"      <label><translation lang=\"en\">Autor"
			"</translation></label>\n"
			"    </field>\n"
			"    <field row=\"0\" col=\"1\" name=\"revisao\""
			" displaylabel=\"true\" align=\"left\" valign=\"center\">\n"
			"      <value><translation lang=\"en\">%{revisor_4}"
			"</translation></value>\n"
			"      <label><translation lang=\"en\">Revisor"
			"</translation></label>\n"
			"    </field>\n"
			"    <field row=\"1\" col=\"0\" name=\"area\""
			" displaylabel=\"true\" align=\"left\" valign=\"center\">\n"
			"      <value><translation lang=\"en\">Quadro"
			"</translation></value>\n"
			"      <label><translation lang=\"en\">%{setor}"
			"</translation></label>\n"
			"    </field>\n"
			"    <field row=\"1\" col=\"1\" name=\"fixo\""
			" displaylabel=\"false\" align=\"left\" valign=\"center\">\n"
			"      <value><translation lang=\"en\">Escala 100%"
			"</translation></value>\n"
			"    </field>\n"
			"    <field row=\"2\" col=\"0\" name=\"pintavel\""
			" displaylabel=\"false\" align=\"left\" valign=\"center\">\n"
			"      <value><translation lang=\"en\">%{revisor_4}"
			"</translation></value>\n"
			"    </field>\n"
			"    <field row=\"2\" col=\"1\" name=\"impintavel\""
			" displaylabel=\"false\" align=\"left\" valign=\"center\">\n"
			"      <value><translation lang=\"en\">%{Cliente}"
			"</translation></value>\n"
			"    </field>\n"
			"  </grid>\n"
			"</titleblocktemplate>\n");
	}

	/**
		@return the context the editor builds for @a cell, through the very
		object that paints the preview - and not through a copy of its rule
		written here, which would go on agreeing with itself long after the
		program stopped agreeing with it.
	*/
	DiagramContext editorContextFor(TitleBlockTemplate *tbt,
					TitleBlockCell *cell)
	{
		TitleBlockTemplateVisualCell visual;
		visual.setTemplateCell(tbt, cell);
		return visual.authoringContext();
	}

	/** @return how many pixels of @a image are not the white it started as */
	int inkIn(const QImage &image)
	{
		const QColor white(Qt::white);
		int ink = 0;
		for (int y = 0 ; y < image.height() ; ++ y) {
			for (int x = 0 ; x < image.width() ; ++ x) {
				if (image.pixelColor(x, y) != white) {
					++ ink;
				}
			}
		}
		return ink;
	}

	/**
		@return the ink @a cell leaves when the preview item paints it,
		which is the one place the authoring context is actually put to
		use. Everything else in this file asks the item for the context;
		this asks the paint path whether it uses it.
	*/
	int inkPaintedFor(TitleBlockTemplate *tbt, TitleBlockCell *cell)
	{
		QImage image(kCellWidth, kCellHeight, QImage::Format_ARGB32);
		image.fill(Qt::white);
		{
			QPainter painter(&image);
			TitleBlockTemplateVisualCell visual;
			visual.setTemplateCell(tbt, cell);
			visual.setGeometry(QRectF(0, 0,
						  image.width(),
						  image.height()));
			QStyleOptionGraphicsItem option;
			visual.paint(&painter, &option, nullptr);
		}
		return inkIn(image);
	}

	/**
		@return whether this build draws letters at all.

		Measured, and not assumed: on this machine the Qt5 build rasterises
		no glyph without a screen, so an assertion over ink passes on one
		build and fails on the other for a reason that has nothing to do
		with the title block. The section that counts ink asks this first.
	*/
	bool rasterisesGlyphs()
	{
		QImage image(60, 20, QImage::Format_ARGB32);
		image.fill(Qt::white);
		{
			QPainter painter(&image);
			painter.setPen(Qt::black);
			painter.drawText(QRect(0, 0, 60, 20),
					 Qt::AlignLeft | Qt::AlignVCenter,
					 QStringLiteral("mmm"));
		}
		return inkIn(image) > 0;
	}
}

TEST_CASE("T26 — a célula diz o nome do atributo no editor, e a folha não muda",
	  "[titleblock][ui]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());

	const QString path = dir.filePath(QStringLiteral("bancada.titleblock"));
	QFile file(path);
	REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
	REQUIRE(file.write(benchTemplate().toUtf8()) > 0);
	file.close();

	TitleBlockTemplate tbt;
	REQUIRE(tbt.loadFromXmlFile(path));
	REQUIRE(tbt.columnsCount() == 2);
	REQUIRE(tbt.rowsCount() == 3);

	TitleBlockCell *bare_in_value   = tbt.cell(0, 0);
	TitleBlockCell *braced_in_value = tbt.cell(0, 1);
	TitleBlockCell *braced_in_label = tbt.cell(1, 0);
	TitleBlockCell *fixed_text      = tbt.cell(1, 1);
	TitleBlockCell *nameable        = tbt.cell(2, 0);
	TitleBlockCell *unnameable      = tbt.cell(2, 1);
	REQUIRE(bare_in_value   != nullptr);
	REQUIRE(braced_in_value != nullptr);
	REQUIRE(braced_in_label != nullptr);
	REQUIRE(fixed_text      != nullptr);
	REQUIRE(nameable        != nullptr);
	REQUIRE(unnameable      != nullptr);

	SECTION("a lista de variáveis do carimbo alcança as três formas")
	{
		const QStringList variables = tbt.listOfVariables();

		CHECK(variables.contains(QStringLiteral("author")));
		CHECK(variables.contains(QStringLiteral("revisor_4")));
		CHECK(variables.contains(QStringLiteral("setor")));

		// "Cliente" is named too, and on purpose: it is a reference, the
		// folio erases it, and naming it is what says so. The context is
		// where it is dropped - see the painting section below.
		CHECK(variables.contains(QStringLiteral("Cliente")));

		// And the fixed cell says "Escala 100%": a per cent sign that is
		// not a reference must not become a field the project asks
		// somebody to fill in.
		CHECK(variables.count() == 4);
	}

	SECTION("na folha, o campo que ninguém preencheu continua vazio")
	{
			// A cell showing its label joins it to the value with " %1: %2",
			// the English source the upstream writes since 2026-10 - the
			// suite runs with no translation. Portuguese keeps " %1 : %2".
		const DiagramContext nothing_filled_in;

		CHECK(tbt.finalTextForCell(*bare_in_value, nothing_filled_in)
		      == QStringLiteral(" Autor: "));
		CHECK(tbt.finalTextForCell(*braced_in_value, nothing_filled_in)
		      == QStringLiteral(" Revisor: "));
		CHECK(tbt.finalTextForCell(*braced_in_label, nothing_filled_in)
		      == QStringLiteral(" : Quadro"));
		CHECK(tbt.finalTextForCell(*fixed_text, nothing_filled_in)
		      == QStringLiteral(" Escala 100%"));
	}

	SECTION("no editor, a mesma célula diz o nome do atributo, entre colchetes")
	{
			// The brackets are the answer to P120, decided 06/09/2026: without
			// them a cell holding the literal word "author" and a cell holding
			// %{author} draw identically, and whoever is drawing the title
			// block cannot tell a field from fixed text until the first
			// project is printed. They exist in the editor preview only - the
			// section above proves the printed sheet is untouched.
		CHECK(tbt.finalTextForCell(
			      *bare_in_value,
			      editorContextFor(&tbt, bare_in_value))
		      == QStringLiteral(" Autor: [author]"));
		CHECK(tbt.finalTextForCell(
			      *braced_in_value,
			      editorContextFor(&tbt, braced_in_value))
		      == QStringLiteral(" Revisor: [revisor_4]"));
		CHECK(tbt.finalTextForCell(
			      *braced_in_label,
			      editorContextFor(&tbt, braced_in_label))
		      == QStringLiteral(" [setor]: Quadro"));

		// And the fixed text is fixed in the editor too: nothing in it is
		// a reference, so there is no name to put anywhere.
		CHECK(tbt.finalTextForCell(
			      *fixed_text,
			      editorContextFor(&tbt, fixed_text))
		      == QStringLiteral(" Escala 100%"));
	}

	SECTION("o valor do projeto continua ganhando, que é o que imprime")
	{
		DiagramContext project;
		REQUIRE(project.addValue(QStringLiteral("author"),
					 QStringLiteral("Fulano de Tal")));
		REQUIRE(project.addValue(QStringLiteral("revisor_4"),
					 QStringLiteral("Beltrano")));
		REQUIRE(project.addValue(QStringLiteral("setor"),
					 QStringLiteral("Automação")));

		CHECK(tbt.finalTextForCell(*bare_in_value, project)
		      == QStringLiteral(" Autor: Fulano de Tal"));
		CHECK(tbt.finalTextForCell(*braced_in_value, project)
		      == QStringLiteral(" Revisor: Beltrano"));
		CHECK(tbt.finalTextForCell(*braced_in_label, project)
		      == QStringLiteral(" Automação: Quadro"));
		CHECK(tbt.finalTextForCell(*fixed_text, project)
		      == QStringLiteral(" Escala 100%"));
	}

	SECTION("a célula sem célula nenhuma não inventa contexto")
	{
		// Asking the preview for the context of nothing has to give
		// nothing, rather than the whole vocabulary of the title block.
		TitleBlockTemplateVisualCell visual;
		visual.setTemplateCell(&tbt, nullptr);
		CHECK(visual.authoringContext().keys().isEmpty());
	}

	SECTION("e o desenho da prévia usa esse contexto, não um vazio")
	{
		// This is the one assertion that goes through paint(). Both cells
		// below are the same size, the same alignment and the same font,
		// and hold nothing but a braced reference with no label. The
		// difference is that the editor can name one of them and not the
		// other: "Cliente" has a capital letter, and
		// DiagramContext::validKeyRegExp() is "^[a-z0-9-_]+$", so no
		// context can ever hold it and no project can ever give it a
		// value. So the second cell is what an attribute cell looked like
		// before this step - blank - and the first is what it looks like
		// now.
		if (!rasterisesGlyphs()) {
			SUCCEED("this build rasterises no glyph without a "
				"screen; the ink comparison would measure the "
				"platform and not the title block");
			return;
		}

		const int blank_cell = inkPaintedFor(&tbt, unnameable);
		const int named_cell = inkPaintedFor(&tbt, nameable);

		INFO("ink: blank cell " << blank_cell
		     << ", named cell " << named_cell);
		CHECK(named_cell > blank_cell);
	}
}

/*
	The double click on a cell.

	Everything above asks the preview what it would draw. What follows asks
	the editor window what a double click does, and that takes the window
	itself: the view, the dock holding the properties panel, and the focus.
	The window is shown - under the offscreen platform, so on nobody's
	screen - because a dock and a focus are only what they are in a window
	that exists; and the double click is sent to the viewport of the view as
	mouse events, the way Qt hands a person's to it, and not to a slot.

	Nothing here opens a modal dialog. The double click on a cell opens none
	- the panel is a dock. The helper cells around the grid, giving the width
	of a column or the height of a row, do open one, through exec(), which
	would wait for ever under the offscreen platform; they are not clicked.
*/
namespace
{
	/**
		Four cells: three text cells whose values differ from their names
		and labels, so the value field can be told from every other field
		of the panel by what it shows; and one position the file leaves
		out, which the template fills with an empty cell.
	*/
	QString doubleClickTemplate()
	{
		return QStringLiteral(
			"<titleblocktemplate name=\"duploclique\">\n"
			"  <information></information>\n"
			"  <logos/>\n"
			"  <grid cols=\"t50%;t50%;\" rows=\"25;25;\">\n"
			"    <field row=\"0\" col=\"0\" name=\"autoria\""
			" displaylabel=\"true\" align=\"left\" valign=\"center\">\n"
			"      <value><translation lang=\"en\">%{author}"
			"</translation></value>\n"
			"      <label><translation lang=\"en\">Autor"
			"</translation></label>\n"
			"    </field>\n"
			"    <field row=\"0\" col=\"1\" name=\"data\""
			" displaylabel=\"true\" align=\"left\" valign=\"center\">\n"
			"      <value><translation lang=\"en\">%{date}"
			"</translation></value>\n"
			"      <label><translation lang=\"en\">Data"
			"</translation></label>\n"
			"    </field>\n"
			"    <field row=\"1\" col=\"0\" name=\"titulo\""
			" displaylabel=\"false\" align=\"left\" valign=\"center\">\n"
			"      <value><translation lang=\"en\">%{title}"
			"</translation></value>\n"
			"    </field>\n"
			"  </grid>\n"
			"</titleblocktemplate>\n");
	}

	/** @return the item the view draws for @a cell, found in its scene */
	TitleBlockTemplateVisualCell *visualCellOf(QGraphicsScene *scene,
						   const TitleBlockCell *cell)
	{
		const QList<QGraphicsItem *> items = scene -> items();
		for (QGraphicsItem *item : items) {
			auto *visual = dynamic_cast<TitleBlockTemplateVisualCell *>(item);
			if (visual && visual -> cell() == cell) {
				return visual;
			}
		}
		return nullptr;
	}

	/**
		@return the point of the viewport at the middle of @a cell, after
		making sure a click there lands on the cell: inside the viewport,
		and with nothing drawn over it.
	*/
	QPoint middleOf(TitleBlockTemplateView *view, const TitleBlockCell *cell)
	{
		TitleBlockTemplateVisualCell *visual =
			visualCellOf(view -> scene(), cell);
		REQUIRE(visual != nullptr);
		const QRectF rect = visual -> sceneBoundingRect();
		REQUIRE_FALSE(rect.isEmpty());
		const QPoint point = view -> mapFromScene(rect.center());
		REQUIRE(view -> viewport() -> rect().contains(point));
		REQUIRE(view -> itemAt(point) == static_cast<QGraphicsItem *>(visual));
		return point;
	}

	/**
		A double click as Qt hands a person's to a widget: press, release,
		double click, release. All four, because the press before the
		double click already selects on its own, and what is asserted has
		to hold after the whole gesture and not after its third event.
	*/
	void doubleClick(QWidget *viewport,
			 const QPoint &point,
			 Qt::KeyboardModifiers modifiers = Qt::NoModifier)
	{
		QTest::mousePress(viewport, Qt::LeftButton, modifiers, point);
		QTest::mouseRelease(viewport, Qt::LeftButton, modifiers, point);
		QTest::mouseDClick(viewport, Qt::LeftButton, modifiers, point);
		QTest::mouseRelease(viewport, Qt::LeftButton, modifiers, point);
	}

	/**
		@return the line of the properties panel that has the focus, if
		the focus of @a editor is a visible line of @a panel at all.
	*/
	QLineEdit *focusedLineOf(QETTitleBlockTemplateEditor &editor,
				 TitleBlockTemplateCellWidget *panel)
	{
		auto *line = qobject_cast<QLineEdit *>(editor.focusWidget());
		if (!line || !panel -> isAncestorOf(line) || !line -> isVisible()) {
			return nullptr;
		}
		return line;
	}
}

TEST_CASE("T26 — o duplo clique numa célula abre as propriedades dela",
	  "[titleblock][ui]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = dir.filePath(QStringLiteral("duploclique.titleblock"));
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
		REQUIRE(file.write(doubleClickTemplate().toUtf8()) > 0);
	}

		// The editor works on the template it is given and never deletes
		// it, so the case owns it - declared before the editor, so that it
		// outlives the window whose cells point into it.
	std::unique_ptr<TitleBlockTemplate> tbt(new TitleBlockTemplate());
	REQUIRE(tbt -> loadFromXmlFile(path));
	REQUIRE(tbt -> columnsCount() == 2);
	REQUIRE(tbt -> rowsCount() == 2);

	TitleBlockCell *author = tbt -> cell(0, 0);
	TitleBlockCell *date   = tbt -> cell(0, 1);
	TitleBlockCell *title  = tbt -> cell(1, 0);
	TitleBlockCell *empty  = tbt -> cell(1, 1);
	REQUIRE(author != nullptr);
	REQUIRE(date   != nullptr);
	REQUIRE(title  != nullptr);
	REQUIRE(empty  != nullptr);
	REQUIRE(date  -> type() == TitleBlockCell::TextCell);
	REQUIRE(empty -> type() == TitleBlockCell::EmptyCell);

	QETTitleBlockTemplateEditor editor;
	REQUIRE(editor.edit(tbt.get()));
	editor.show();
	REQUIRE(QTest::qWaitForWindowExposed(&editor));

	auto *view = qobject_cast<TitleBlockTemplateView *>(editor.centralWidget());
	REQUIRE(view != nullptr);
	auto *dock = editor.findChild<QDockWidget *>(
				 QStringLiteral("tbt_celleditor_dock"));
	REQUIRE(dock != nullptr);
	auto *panel = editor.findChild<TitleBlockTemplateCellWidget *>();
	REQUIRE(panel != nullptr);

		// The person had closed the properties panel. Selecting a cell
		// fills the panel but does not reopen it, so this is where a
		// double click that only selected would be caught.
	dock -> close();
	REQUIRE_FALSE(dock -> isVisible());
	REQUIRE(view -> selectedCells().isEmpty());

		// Lay the grid and the window out, then fit the grid to the view
		// as it now is: the middle of every cell is then in the viewport.
	QCoreApplication::processEvents();
	view -> zoomFit();

	SECTION("o painel fechado volta, mostrando a célula, com o foco no valor")
	{
		doubleClick(view -> viewport(), middleOf(view, date));

		const QList<TitleBlockCell *> selected = view -> selectedCells();
		REQUIRE(selected.count() == 1);
		CHECK(selected.first() == date);

		CHECK(dock -> isVisible());
		CHECK(panel -> isVisible());

			// The focus is in the panel, on the value: the one line that
			// shows "%{date}". The name of the cell is "data" and its
			// label "Data", so no other line of the panel shows this.
		QLineEdit *line = focusedLineOf(editor, panel);
		REQUIRE(line != nullptr);
		CHECK(line -> text() == QStringLiteral("%{date}"));
	}

	SECTION("com Ctrl e outras células selecionadas, a do duplo clique fica sozinha")
	{
			// Two cells selected: the editor hides the panel for any
			// selection that is not exactly one cell.
		TitleBlockTemplateVisualCell *author_item =
			visualCellOf(view -> scene(), author);
		TitleBlockTemplateVisualCell *title_item =
			visualCellOf(view -> scene(), title);
		REQUIRE(author_item != nullptr);
		REQUIRE(title_item  != nullptr);
		author_item -> setSelected(true);
		title_item  -> setSelected(true);
		REQUIRE(view -> selectedCells().count() == 2);
		REQUIRE_FALSE(panel -> isVisibleTo(dock));

			// With Ctrl held, the click before the double click adds the
			// cell to the selection, and the scene, handed the double
			// click, would treat it as one more press and the release
			// after it as one more toggle - taking the cell out again.
			// Asked for its properties, the cell has to end up alone.
		doubleClick(view -> viewport(),
			    middleOf(view, date),
			    Qt::ControlModifier);

		const QList<TitleBlockCell *> selected = view -> selectedCells();
		INFO("cells selected after the double click: " << selected.count());
		REQUIRE(selected.count() == 1);
		CHECK(selected.first() == date);

		CHECK(dock -> isVisible());
		CHECK(panel -> isVisible());

		QLineEdit *line = focusedLineOf(editor, panel);
		REQUIRE(line != nullptr);
		CHECK(line -> text() == QStringLiteral("%{date}"));
	}

	SECTION("numa célula vazia, o foco vai para o tipo, que é tudo o que ela tem")
	{
		doubleClick(view -> viewport(), middleOf(view, empty));

		const QList<TitleBlockCell *> selected = view -> selectedCells();
		REQUIRE(selected.count() == 1);
		CHECK(selected.first() == empty);

		CHECK(dock -> isVisible());
		CHECK(panel -> isVisible());

			// The value line of a text cell is hidden for an empty cell,
			// and a hidden widget takes no focus: what gets it is the
			// type, the only input shown.
		auto *type = qobject_cast<QComboBox *>(editor.focusWidget());
		REQUIRE(type != nullptr);
		CHECK(panel -> isAncestorOf(type));
		CHECK(type -> isVisible());
		CHECK(type -> currentData().toInt() == TitleBlockCell::EmptyCell);
	}
}
