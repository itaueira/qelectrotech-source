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

#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/dataBase/ui/elementquerywidget.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/projectdbmodel.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/qetgraphicsheaderitem.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QImage>
#include <QPainter>
#include <QString>
#include <QStringList>

/*
	A list that could not be established, and the day it stopped looking like
	a list with nothing in it.

	The two used to be the same drawing. ProjectDBModel::columnCount() took
	its answer from the first row of the result, so a query that returned no
	row answered zero columns - and a query that could not be run at all,
	whose failure went to qDebug() and no further, answered zero columns too.
	On the folio both drew a frame of no width, no column and no row: nothing.
	A draughtsman looking at it had no way to tell "this project has no such
	component" from "this table asks for a column the data base does not
	have", and the second is the one that has happened here twice already,
	when a key was added to elementInfoKeys() and forgotten in the view.

	What is measured below is the distinction, at the three places it has to
	hold: the number of columns, the error carried by the model, and the ink
	on the folio. The ink is counted by colour rather than by shape - the
	message is the only thing this item ever draws in red, and how many
	pixels a sentence takes depends on the font of the machine, which is why
	the assertion is "there is red" and never "there is this much red".
*/

namespace {
	/// A project of the current format, with components on its sheets.
	const QString reference_example = QStringLiteral("741.qet");

	/// A query that is valid and that no project can answer with a row.
	const QString empty_result_query = QStringLiteral(
		"SELECT label, designation FROM element_nomenclature_view"
		" WHERE label = 'aucun composant ne porte ce label'");

	/// A query the data base refuses: there is no such column in the view.
	const QString broken_query = QStringLiteral(
		"SELECT no_such_column FROM element_nomenclature_view");

	/// A query every project with a component answers.
	const QString working_query = QStringLiteral(
		"SELECT label, designation FROM element_nomenclature_view");

	/**
		The item drawn on its own, on white, with nothing else on the
		image. The header is a child item and is not drawn here: what is
		being looked at is what the table itself puts on the folio.
	*/
	QImage draw(QetGraphicsTableItem &item)
	{
		QImage image(600, 400, QImage::Format_RGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		item.paint(&painter, nullptr, nullptr);
		painter.end();
		return image;
	}

	/**
		Whether this build can rasterise a glyph at all, drawn exactly
		the way the message is drawn.

		An instrument nobody calibrated proves nothing, and this one
		reads zero for two different reasons: a message that was not
		written, and a machine that writes no message. Measured on this
		machine, the second is not hypothetical - the Qt6 build draws
		the text and the Qt5 build draws none of it offscreen, on the
		same code, and two cases of T11 fail there for the same reason,
		comparing the ink of a card before and after sixteen labels are
		written on it and finding the very same number.
	*/
	bool drawsText()
	{
		QImage image(120, 40, QImage::Format_RGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		painter.drawText(QRectF(0, 0, 120, 40),
				 Qt::AlignCenter | Qt::TextWordWrap,
				 QStringLiteral("Ax"));
		painter.end();

		for (int y = 0 ; y < image.height() ; ++y) {
			for (int x = 0 ; x < image.width() ; ++x) {
				if (image.pixel(x, y) != qRgb(255, 255, 255)) {
					return true;
				}
			}
		}
		return false;
	}

	/// Pixels that are not the white background.
	int ink(const QImage &image)
	{
		int count = 0;
		for (int y = 0 ; y < image.height() ; ++y) {
			for (int x = 0 ; x < image.width() ; ++x) {
				if (image.pixel(x, y) != qRgb(255, 255, 255)) {
					++count;
				}
			}
		}
		return count;
	}

	/**
		Pixels with more red in them than green and blue, whatever the
		antialiasing did to the shade.

		Not "red above a threshold, green and blue below one": the
		message is thin text, and on a machine whose rasteriser blends
		it more the darkest pixel of a stroke can still be pale. That
		measurement passed under Qt6 and found nothing under Qt5, on the
		same drawing. What tells the message apart from everything else
		this item draws is that nothing else is coloured at all - the
		frame is black and the background white, and both have their
		three channels equal.
	*/
	int redInk(const QImage &image)
	{
		int count = 0;
		for (int y = 0 ; y < image.height() ; ++y) {
			for (int x = 0 ; x < image.width() ; ++x) {
				const QRgb pixel = image.pixel(x, y);
				if (qRed(pixel) > qGreen(pixel) + 20 &&
					qRed(pixel) > qBlue(pixel) + 20) {
					++count;
				}
			}
		}
		return count;
	}
}

TEST_CASE("T16 — a consulta que não roda deixa de desenhar o mesmo que a lista sem itens",
	  "[uibench][listengine]")
{
	UiBench::Project project(reference_example);
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	SECTION("uma consulta que roda e não acha nada guarda as suas colunas")
	{
		ProjectDBModel model(project.project());
		model.setQuery(empty_result_query);

		REQUIRE(model.lastError().isEmpty());
		REQUIRE(model.rowCount() == 0);
		REQUIRE(model.columnCount() == 2);
		REQUIRE(model.columnNames()
			== QStringList({QStringLiteral("label"),
					QStringLiteral("designation")}));
	}

	SECTION("uma consulta que não roda não tem coluna nenhuma, e diz por quê")
	{
		ProjectDBModel model(project.project());
		model.setQuery(broken_query);

		INFO(model.lastError().toStdString());
		REQUIRE(model.rowCount() == 0);
		REQUIRE(model.columnCount() == 0);
		REQUIRE_FALSE(model.lastError().isEmpty());
		// The sentence has to name what the data base refused, or it
		// sends the reader back to the folio with nothing to act on.
		REQUIRE(model.lastError().contains(QStringLiteral("no_such_column")));
	}

	SECTION("consulta nenhuma é dita com outras palavras que consulta quebrada")
	{
		ProjectDBModel model(project.project());
		model.setQuery(QString());

		REQUIRE(model.columnCount() == 0);
		REQUIRE_FALSE(model.lastError().isEmpty());

		ProjectDBModel broken(project.project());
		broken.setQuery(broken_query);
		REQUIRE(model.lastError() != broken.lastError());
	}

	SECTION("o erro é anunciado nas duas direções, e uma vez em cada")
	{
		ProjectDBModel model(project.project());
		QStringList announced;
		QObject::connect(&model, &ProjectDBModel::queryErrorChanged,
				 [&announced](const QString &error)
		{
			announced << error;
		});

		model.setQuery(working_query);
		REQUIRE(model.rowCount() > 0);
		// A model that runs from the start has nothing to announce.
		REQUIRE(announced.isEmpty());

		model.setQuery(broken_query);
		REQUIRE(announced.size() == 1);
		REQUIRE_FALSE(announced.last().isEmpty());

		model.setQuery(working_query);
		REQUIRE(announced.size() == 2);
		REQUIRE(announced.last().isEmpty());
	}

	SECTION("uma coluna nula no meio não desloca as colunas seguintes")
	{
		// Every row is as wide as the table, and the value of a column
		// is the value of that column. This is the property
		// columnCount() rests on, and it is written down as a guard
		// rather than as a repair : the row is now filled by the count
		// of the record, and the loop it replaced - one that stopped at
		// the first value the driver reads as invalid - passes this
		// section too. Planted and measured: SQLite gives a NULL back
		// as a null variant that is still valid, so the old loop does
		// not stop at it, and the truncation it was suspected of does
		// not happen on this driver.
		ProjectDBModel model(project.project());
		model.setQuery(QStringLiteral(
			"SELECT label, NULL AS middle, 'derniere' AS tail"
			" FROM element_nomenclature_view LIMIT 3"));

		REQUIRE(model.lastError().isEmpty());
		REQUIRE(model.columnCount() == 3);
		REQUIRE(model.rowCount() > 0);

		for (int row = 0 ; row < model.rowCount() ; ++row)
		{
			// The value of the last column is what a short row loses,
			// and a written constant is what makes its loss visible:
			// an empty cell would pass for a cell that is empty.
			INFO("row " << row);
			REQUIRE(model.index(row, 1).data().toString().isEmpty());
			REQUIRE(model.index(row, 2).data().toString()
				== QStringLiteral("derniere"));
		}
	}

	SECTION("na folha, o bloco escreve o erro; a lista sem itens não escreve em vermelho")
	{
		ProjectDBModel broken_model(project.project());
		broken_model.setQuery(broken_query);
		QetGraphicsTableItem broken_table;
		broken_table.setModel(&broken_model);

		ProjectDBModel empty_model(project.project());
		empty_model.setQuery(empty_result_query);
		QetGraphicsTableItem empty_table;
		empty_table.setModel(&empty_model);

		const QImage broken_image = draw(broken_table);
		const QImage empty_image = draw(empty_table);

		INFO("box " << broken_table.tableRect().width()
		     << "x" << broken_table.tableRect().height()
		     << ", ink " << ink(broken_image)
		     << ", red " << redInk(broken_image));

		// The box has a width of its own: there is no column to take
		// one from, and a frame of no width is what used to be drawn.
		REQUIRE(broken_table.tableRect().width() > 0);
		REQUIRE(broken_table.modelError() == broken_model.lastError());
		// The box itself: a frame where there used to be a line of no
		// width. It is there whether or not a glyph can be drawn.
		REQUIRE(ink(broken_image) > 0);

		if (drawsText()) {
			REQUIRE(redInk(broken_image) > 0);
		} else {
			SUCCEED("this build rasterises no glyph offscreen:"
				" the message cannot be looked for in the ink");
		}

		// A list that ran and found nothing says nothing in red - and
		// it still has its columns, so the header above it names them.
		REQUIRE(empty_table.modelError().isEmpty());
		REQUIRE(redInk(empty_image) == 0);
		REQUIRE(empty_table.headerItem()->rect().width() > 0);
	}

	SECTION("o seletor de colunas recusa montar uma consulta sem coluna")
	{
		// This is the state the window opens in: the list of chosen
		// columns is empty, and the SELECT built from it had nothing
		// between SELECT and FROM.
		ElementQueryWidget widget;
		REQUIRE(widget.queryStr().isEmpty());

		ProjectDBModel model(project.project());
		model.setQuery(widget.queryStr());
		REQUIRE_FALSE(model.lastError().isEmpty());
	}
}
