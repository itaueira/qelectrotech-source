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
#include "../../../../sources/titleblock/templatevisualcell.h"
#include "../../../../sources/titleblockcell.h"
#include "../../../../sources/titleblocktemplate.h"

#include <catch2/catch.hpp>

#include <QColor>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QStyleOptionGraphicsItem>
#include <QTemporaryDir>

/*
	The title block editor, and the cell that showed nothing.

	The rule itself is arithmetic over strings and is proved next door, in
	C_unittests (titleblockauthoring_test.cpp). What is proved here is the
	other half, and it needs the real thing: a TitleBlockTemplate read from a
	file, whose cells produce their final text through the same substitution
	the folio uses, and the preview item that paints them. TitleBlockTemplate
	reaches QETApp - one call, for the font of a cell - so it only links here,
	where qet_core brings the whole program in.

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
		const DiagramContext nothing_filled_in;

		CHECK(tbt.finalTextForCell(*bare_in_value, nothing_filled_in)
		      == QStringLiteral(" Autor : "));
		CHECK(tbt.finalTextForCell(*braced_in_value, nothing_filled_in)
		      == QStringLiteral(" Revisor : "));
		CHECK(tbt.finalTextForCell(*braced_in_label, nothing_filled_in)
		      == QStringLiteral("  : Quadro"));
		CHECK(tbt.finalTextForCell(*fixed_text, nothing_filled_in)
		      == QStringLiteral(" Escala 100%"));
	}

	SECTION("no editor, a mesma célula diz o nome do atributo")
	{
		CHECK(tbt.finalTextForCell(
			      *bare_in_value,
			      editorContextFor(&tbt, bare_in_value))
		      == QStringLiteral(" Autor : author"));
		CHECK(tbt.finalTextForCell(
			      *braced_in_value,
			      editorContextFor(&tbt, braced_in_value))
		      == QStringLiteral(" Revisor : revisor_4"));
		CHECK(tbt.finalTextForCell(
			      *braced_in_label,
			      editorContextFor(&tbt, braced_in_label))
		      == QStringLiteral(" setor : Quadro"));

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
		      == QStringLiteral(" Autor : Fulano de Tal"));
		CHECK(tbt.finalTextForCell(*braced_in_value, project)
		      == QStringLiteral(" Revisor : Beltrano"));
		CHECK(tbt.finalTextForCell(*braced_in_label, project)
		      == QStringLiteral(" Automação : Quadro"));
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
