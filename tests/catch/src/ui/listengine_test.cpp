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
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QImage>
#include <QPainter>
#include <QSqlQuery>
#include <QSqlRecord>
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

	/// The columns a view publishes, in the order it publishes them.
	QStringList viewColumns(QETProject *project, const QString &view)
	{
		QSqlQuery query = project->dataBase()->newQuery(
					QStringLiteral("SELECT * FROM %1 LIMIT 1").arg(view));
		if (!query.exec()) {
			return QStringList();
		}

			//Read from the statement and not from a row: a view of a
			//project with no component still publishes its columns.
		QStringList columns;
		const QSqlRecord record_ = query.record();
		for (int i = 0 ; i < record_.count() ; ++i) {
			columns << record_.fieldName(i);
		}
		return columns;
	}

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

TEST_CASE("T16 — a visão do banco nasce da lista canônica, e publica o que publicava antes",
	  "[uibench][listengine][database]")
{
	/*
		There were two lists of element information columns, and they had to
		agree: elementInfoKeys(), which creates the columns of element_info and
		drives the insert, and the body of the two element views, written out
		by hand. The second was forgotten twice - plc_unit and plc_bus were
		keys the column selector offered and the view could not select, and a
		table asking for one came back with no row and no column at all, empty
		on the folio and silent. Both repairs added the line that had been left
		out, which leaves the next key to be forgotten in the same way.

		The view body is now generated from elementInfoKeys(), so there is no
		second list. What that must not do is quietly republish a different set
		of columns: every caller selects by name, and a name that moved or
		vanished takes a parts list with it. So the set below is the one the
		hand-written view published before this change, read off the previous
		revision of projectdatabase.cpp and not off the new output - copying
		the new output would only prove the generator agrees with itself.
	*/
	UiBench::Project project(reference_example);
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	REQUIRE(project.project()->dataBase() != nullptr);

		//The 67 columns element_nomenclature_view published while its body
		//was written out by hand.
	const QStringList published_before = {
		QStringLiteral("label"), QStringLiteral("plant"),
		QStringLiteral("location"), QStringLiteral("location_path"),
		QStringLiteral("connector"), QStringLiteral("comment"),
		QStringLiteral("function"), QStringLiteral("description"),
		QStringLiteral("designation"), QStringLiteral("manufacturer"),
		QStringLiteral("manufacturer_reference"),
		QStringLiteral("machine_manufacturer_reference"),
		QStringLiteral("supplier"), QStringLiteral("quantity"),
		QStringLiteral("unity"), QStringLiteral("auxiliary1"),
		QStringLiteral("description_auxiliary1"),
		QStringLiteral("designation_auxiliary1"),
		QStringLiteral("manufacturer_auxiliary1"),
		QStringLiteral("manufacturer_reference_auxiliary1"),
		QStringLiteral("machine_manufacturer_reference_auxiliary1"),
		QStringLiteral("supplier_auxiliary1"),
		QStringLiteral("quantity_auxiliary1"),
		QStringLiteral("unity_auxiliary1"), QStringLiteral("auxiliary2"),
		QStringLiteral("description_auxiliary2"),
		QStringLiteral("designation_auxiliary2"),
		QStringLiteral("manufacturer_auxiliary2"),
		QStringLiteral("manufacturer_reference_auxiliary2"),
		QStringLiteral("machine_manufacturer_reference_auxiliary2"),
		QStringLiteral("supplier_auxiliary2"),
		QStringLiteral("quantity_auxiliary2"),
		QStringLiteral("unity_auxiliary2"), QStringLiteral("auxiliary3"),
		QStringLiteral("description_auxiliary3"),
		QStringLiteral("designation_auxiliary3"),
		QStringLiteral("manufacturer_auxiliary3"),
		QStringLiteral("manufacturer_reference_auxiliary3"),
		QStringLiteral("machine_manufacturer_reference_auxiliary3"),
		QStringLiteral("supplier_auxiliary3"),
		QStringLiteral("quantity_auxiliary3"),
		QStringLiteral("unity_auxiliary3"), QStringLiteral("auxiliary4"),
		QStringLiteral("description_auxiliary4"),
		QStringLiteral("designation_auxiliary4"),
		QStringLiteral("manufacturer_auxiliary4"),
		QStringLiteral("manufacturer_reference_auxiliary4"),
		QStringLiteral("machine_manufacturer_reference_auxiliary4"),
		QStringLiteral("supplier_auxiliary4"),
		QStringLiteral("quantity_auxiliary4"),
		QStringLiteral("unity_auxiliary4"), QStringLiteral("exclude_from_bom"),
		QStringLiteral("part_code"), QStringLiteral("part_revision"),
		QStringLiteral("plc_type"), QStringLiteral("plc_address"),
		QStringLiteral("plc_function"), QStringLiteral("plc_comment"),
		QStringLiteral("plc_crossref"), QStringLiteral("plc_unit"),
		QStringLiteral("plc_bus"), QStringLiteral("diagram_position"),
		QStringLiteral("element_type"), QStringLiteral("element_sub_type"),
		QStringLiteral("title"), QStringLiteral("folio"),
		QStringLiteral("position")};

	const QStringList published_now =
			viewColumns(project.project(),
				    QStringLiteral("element_nomenclature_view"));
	REQUIRE_FALSE(published_now.isEmpty());

	SECTION("nenhuma coluna publicada antes sumiu nem mudou de nome")
	{
		QStringList lost;
		for (const QString &column : published_before) {
			if (!published_now.contains(column)) {
				lost << column;
			}
		}

			//Named and not counted: a total says one column is gone and
			//sends whoever reads it back to a list of sixty-seven.
		INFO("columns the generated view stopped publishing: "
		     << lost.join(QStringLiteral(", ")).toStdString());
		CHECK(lost.isEmpty());
	}

	SECTION("a única coluna a mais é justamente a que faltava")
	{
		QStringList added;
		for (const QString &column : published_now) {
			if (!published_before.contains(column)) {
				added << column;
			}
		}

			//formula was the one key of elementInfoKeys() the hand-written
			//view left out. It is the gain of the change, and it is checked
			//by name so that a second unplanned column cannot ride in on a
			//count that happens to match.
		INFO("columns the generated view added: "
		     << added.join(QStringLiteral(", ")).toStdString());
		CHECK(added == QStringList({QStringLiteral("formula")}));
	}

	SECTION("toda chave de informação é selecionável, sem exceção")
	{
			//The property the generation buys, stated over both views
			//because both are built from the same body.
		for (const QString &view : {QStringLiteral("element_nomenclature_view"),
					    QStringLiteral("element_label_view")})
		{
			QStringList unselectable;
			const QStringList keys = QETInformation::elementInfoKeys();
			REQUIRE_FALSE(keys.isEmpty());

			for (const QString &key : keys)
			{
				QSqlQuery query = project.project()->dataBase()->newQuery(
						QStringLiteral("SELECT %1 FROM %2 LIMIT 1")
						.arg(key, view));
				if (!query.exec()) {
					unselectable << key;
				}
			}

			INFO("view " << view.toStdString()
			     << ", keys it cannot select: "
			     << unselectable.join(QStringLiteral(", ")).toStdString());
			CHECK(unselectable.isEmpty());
		}

			//The probe can still fail: without this, a view that answered
			//every question would pass the loop above for the wrong reason.
		QSqlQuery absent = project.project()->dataBase()->newQuery(
				QStringLiteral("SELECT no_such_column"
					       " FROM element_nomenclature_view LIMIT 1"));
		CHECK_FALSE(absent.exec());
	}

	SECTION("as duas visões publicam as mesmas colunas")
	{
			//They differ by the bill of materials filter and by nothing
			//else. A label collector reading element_label_view has to find
			//every column the parts list finds.
		CHECK(viewColumns(project.project(),
				  QStringLiteral("element_label_view"))
		      == published_now);
	}

	SECTION("a ordem publicada é a da lista canônica, e nada mais")
	{
		/*
			The order did change here, and this pins it rather than hides
			it: exclude_from_bom, part_code and part_revision used to be
			written before the plc_ columns and now follow them, because
			that is where elementInfoKeys() puts them. No caller can see it
			- every query in the program and in the tests names the columns
			it wants, and none of them selects * from these views - but the
			next reordering should be a decision and not a surprise.
		*/
		QStringList expected = QETInformation::elementInfoKeys();
		expected << QStringLiteral("diagram_position")
			 << QStringLiteral("element_type")
			 << QStringLiteral("element_sub_type")
			 << QStringLiteral("title")
			 << QStringLiteral("folio")
			 << QStringLiteral("position");

		CHECK(published_now == expected);

			//And the view carries one column per key, so a key added
			//tomorrow cannot be missing from it.
		int info_columns = 0;
		for (const QString &column : published_now) {
			if (QETInformation::elementInfoKeys().contains(column)) {
				++info_columns;
			}
		}
		CHECK(info_columns == QETInformation::elementInfoKeys().count());
	}
}
