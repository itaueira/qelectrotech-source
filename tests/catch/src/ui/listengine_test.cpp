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
#include "../../../../sources/diagram.h"
#include "../../../../sources/label/componentlabelquery.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/projectdbmodel.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/qetgraphicsheaderitem.h"
#include "../../../../sources/qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QImage>
#include <QPainter>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QStringList>
#include <QUndoStack>

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

TEST_CASE("T16 — a visão de folhas nasce da mesma lista, e recupera as duas chaves que faltavam",
	  "[uibench][listengine][database]")
{
	/*
		The sibling of the case above, one table over. project_summary_view was
		written out by hand too, and it published seven of the nine keys that
		QETInformation::diagramInfoKeys() declares: filename and display_folio
		were missing, and had been since the view was first written. The table
		diagram_info they would be read from does carry them - it is generated
		from that same list - so the data was there and the view could not
		reach it.

		What had to be proved before generating it is not that the new columns
		arrive, but that the old ones do not leave: every caller selects by
		name, and a name that moved or vanished takes a folio table with it.
		So the set below is the one the hand-written view published, read off
		the previous revision of projectdatabase.cpp and not off the new
		output - copying the new output would only prove the generator agrees
		with itself.

		One of the two recovered columns is empty today, and this says so
		rather than hiding it: nothing in the program writes display_folio into
		the title block context, so the column exists and reads back null. That
		is a gap in whoever fills the context, not in this view, and the view
		carrying the column is what lets it be filled without a second repair
		here.
	*/
	UiBench::Project project(reference_example);
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	REQUIRE(project.project()->dataBase() != nullptr);

		//The 8 columns project_summary_view published while it was written
		//out by hand, in the order it wrote them.
	const QStringList published_before = {
		QStringLiteral("title"), QStringLiteral("author"),
		QStringLiteral("folio"), QStringLiteral("plant"),
		QStringLiteral("locmach"), QStringLiteral("indexrev"),
		QStringLiteral("date"), QStringLiteral("pos")};

	const QStringList published_now =
			viewColumns(project.project(),
				    QStringLiteral("project_summary_view"));
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
			//does not say which, and the two that were added would hide
			//one that left.
		INFO("columns the generated view stopped publishing: "
		     << lost.join(QStringLiteral(", ")).toStdString());
		CHECK(lost.isEmpty());
	}

	SECTION("as colunas a mais são exatamente as duas que faltavam")
	{
		QStringList added;
		for (const QString &column : published_now) {
			if (!published_before.contains(column)) {
				added << column;
			}
		}

			//Checked by name and in order, so that a third unplanned
			//column cannot ride in on a count that happens to match.
		INFO("columns the generated view added: "
		     << added.join(QStringLiteral(", ")).toStdString());
		CHECK(added == QStringList({QStringLiteral("filename"),
					    QStringLiteral("display_folio")}));
	}

	SECTION("toda chave de folha é selecionável, sem exceção")
	{
		QStringList unselectable;
		const QStringList keys = QETInformation::diagramInfoKeys();
		REQUIRE(keys.count() == 9);

		for (const QString &key : keys)
		{
			QSqlQuery query = project.project()->dataBase()->newQuery(
					QStringLiteral("SELECT %1 FROM project_summary_view"
						       " LIMIT 1").arg(key));
			if (!query.exec()) {
				unselectable << key;
			}
		}

		INFO("keys project_summary_view cannot select: "
		     << unselectable.join(QStringLiteral(", ")).toStdString());
		CHECK(unselectable.isEmpty());

			//The probe can still fail: without this, a view that answered
			//every question would pass the loop above for the wrong
			//reason.
		QSqlQuery absent = project.project()->dataBase()->newQuery(
				QStringLiteral("SELECT no_such_column"
					       " FROM project_summary_view LIMIT 1"));
		CHECK_FALSE(absent.exec());
	}

	SECTION("a ordem publicada é a da lista canônica, e a posição da folha ao fim")
	{
		/*
			The order did change, and this pins it rather than hides it:
			filename now sits between author and folio, and display_folio
			between date and pos, because that is where diagramInfoKeys()
			puts them. Every query in the program and in the tests names
			the columns it wants, so no caller can see the difference -
			but the next reordering should be a decision and not a
			surprise.
		*/
		QStringList expected = QETInformation::diagramInfoKeys();
		expected << QStringLiteral("pos");

		CHECK(published_now == expected);
	}

	SECTION("a consulta que lê por posição continua lendo as mesmas duas colunas")
	{
		/*
			The one consumer that reads this view by column number rather
			than by name: the label collector takes value(0) as the sheet
			position and value(1) as its revision index. That is safe only
			because the numbers are positions in its own SELECT list and
			not in the view, and widening a view is exactly the change
			that would break it if it were not. So the property is stated
			here instead of being reasoned about: two columns, pos then
			indexrev, whatever the view publishes around them.
		*/
		QSqlQuery query = project.project()->dataBase()->newQuery(
				ComponentLabelQuery::folioRevisionStatement());
		REQUIRE(query.exec());

		const QSqlRecord record_ = query.record();
		REQUIRE(record_.count() == 2);
		CHECK(record_.fieldName(0) == QStringLiteral("pos"));
		CHECK(record_.fieldName(1) == QStringLiteral("indexrev"));
	}
}

/*
	Editing a cell of a list, and the one thing it must never do.

	The data base this model reads is derived: it is emptied and filled again
	from the sheets of the project. So a value written straight into it draws
	a list holding something the drawing does not hold, and loses it at the
	next fill - a parts list that says one thing on screen, another thing on
	the folio, and a third thing tomorrow. Every path out of a cell therefore
	goes through the undo command of the component, and it is the component
	that tells the base.

	What is measured below is that round trip, and the three places a cell has
	to refuse before it reaches it: a column no component stores, a row no
	single component answers for, and a label a formula drives.
*/

namespace {

	struct EditComponent
	{
		int x;
		const char *label;
		const char *designation;
		const char *comment;
		const char *formula;
		const char *location_path;
	};

	/**
		Five components on one sheet, and each is there for a question.

		K1 and K2 are ordinary: a label of their own, a designation, a
		comment, and nothing else claiming any of them. K1 also carries a
		location path, which is the one information stored in a form
		different from the form it is drawn in.

		Q1 carries a formula, so the label column of its row holds what
		the formula makes and not what the component stores.

		The last two are written the same way in every column: same label,
		same designation, same comment. Two rows the reader cannot tell
		apart, which is the case the model has to answer nullptr to rather
		than pick one of them.
	*/
	QString editFixtureXml()
	{
		const EditComponent components[] = {
			{200, "K1", "LC1D09",    "principal",  "",   "QCM1/PORTE"},
			{320, "K2", "LC1D12",    "auxiliaire", "",   ""},
			{440, "Q1", "GV2ME",     "moteur",     "Q1", ""},
			{560, "T1", "IDENTIQUE", "jumeau",     "",   ""},
			{680, "T1", "IDENTIQUE", "jumeau",     "",   ""}};

			//The docking point of a terminal, which is what the
			//instance stores.
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QString instances;
		int index = 0;
		for (const EditComponent &component : components)
		{
			QString informations;
			informations += QStringLiteral(
						"<elementInformation show=\"1\" name=\"label\">%1"
						"</elementInformation>")
					.arg(QLatin1String(component.label));
			informations += QStringLiteral(
						"<elementInformation show=\"1\" name=\"designation\">%1"
						"</elementInformation>")
					.arg(QLatin1String(component.designation));
			informations += QStringLiteral(
						"<elementInformation show=\"1\" name=\"comment\">%1"
						"</elementInformation>")
					.arg(QLatin1String(component.comment));
			if (component.formula[0] != '\0') {
				informations += QStringLiteral(
							"<elementInformation show=\"1\" name=\"formula\">%1"
							"</elementInformation>")
						.arg(QLatin1String(component.formula));
			}
			if (component.location_path[0] != '\0') {
				informations += QStringLiteral(
							"<elementInformation show=\"1\" name=\"location_path\">%1"
							"</elementInformation>")
						.arg(QLatin1String(component.location_path));
			}

			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/box.elmt\""
					     " uuid=\"{c0ffee00-0000-4000-8000-00000000000%2}\">"
					     "<terminals>"
					     "<terminal x=\"%3\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
					     "<terminal x=\"%5\" y=\"0\" orientation=\"3\" id=\"%6\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>%7</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(component.x)
				     .arg(index)
				     .arg(east_dock)
				     .arg(index * 2)
				     .arg(west_dock)
				     .arg(index * 2 + 1)
				     .arg(informations);
			++index;
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"box.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"30\" height=\"20\""
			       " hotspot_x=\"15\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Box</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(instances);
	}

	/// Where the query put the column named @a name, or -1.
	int columnOf(const ProjectDBModel &model, const QString &name)
	{
		return model.columnNames().indexOf(name);
	}

	/**
		Every row whose label cell reads @a label.

		Rows are looked for and never counted on: the order of a query is
		the data base's business, and a case that writes a row number down
		measures that order instead of what it meant to measure.
	*/
	QList<int> rowsWithLabel(const ProjectDBModel &model, const QString &label)
	{
		QList<int> rows;
		const int column = columnOf(model, QETInformation::ELMT_LABEL);
		if (column < 0) {
			return rows;
		}

		for (int row = 0 ; row < model.rowCount() ; ++row)
		{
			if (model.index(row, column).data().toString() == label) {
				rows << row;
			}
		}
		return rows;
	}

	/// The value the list shows for the component labelled @a label, in @a column.
	QString shownValue(QETProject *project,
			   const QString &query,
			   const QString &label,
			   const QString &column)
	{
		ProjectDBModel model(project);
		model.setQuery(query);
		const QList<int> rows = rowsWithLabel(model, label);
		if (rows.count() != 1) {
			return QStringLiteral("<%1 rows>").arg(rows.count());
		}
		return model.index(rows.first(), columnOf(model, column)).data().toString();
	}

	/// The columns the cases below ask for, in one place.
	const QString edit_query = QStringLiteral(
		"SELECT label, designation, comment, location_path, part_code, folio"
		" FROM element_nomenclature_view ORDER BY label, designation");
}

TEST_CASE("T16 — editar uma célula da lista passa pelo desfazer do componente",
	  "[uibench][listengine]")
{
	UiBench::ScratchProject scratch(editFixtureXml(), QStringLiteral("listedit.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 5);

	QUndoStack *stack = scratch.project()->undoStack();
	REQUIRE(stack != nullptr);

	ProjectDBModel model(scratch.project());
	model.setQuery(edit_query);
	INFO(model.lastError().toStdString());
	REQUIRE(model.lastError().isEmpty());
	REQUIRE(model.rowCount() == 5);
	REQUIRE(model.columnCount() == 6);

	SECTION("é editável a coluna que o componente guarda, e nenhuma outra")
	{
		const QList<int> found = rowsWithLabel(model, QStringLiteral("K1"));
		REQUIRE(found.count() == 1);
		const int row = found.first();
		REQUIRE(model.elementForRow(row) != nullptr);

			//Free text the reader types about a component, and the
			//command that writes it already exists.
		for (const QString &column : {QETInformation::ELMT_LABEL,
					      QETInformation::ELMT_DESIGNATION,
					      QETInformation::ELMT_COMMENT})
		{
			INFO("column " << column.toStdString());
			REQUIRE(columnOf(model, column) >= 0);
			CHECK(model.flags(model.index(row, columnOf(model, column)))
			      .testFlag(Qt::ItemIsEditable));
		}

			//location_path belongs to the location tree and is drawn
			//in a form it is not stored in; part_code is written by
			//the assignment of a catalogue part, with everything else
			//that describes that part; folio is a column of the join
			//and is no information of a component at all - there is
			//nothing for a cell of it to write to.
		for (const QString &column : {QETInformation::ELMT_LOCATION_PATH,
					      QETInformation::ELMT_PART_CODE,
					      QStringLiteral("folio")})
		{
			INFO("column " << column.toStdString());
			REQUIRE(columnOf(model, column) >= 0);
			CHECK_FALSE(model.flags(model.index(row, columnOf(model, column)))
				    .testFlag(Qt::ItemIsEditable));
		}
	}

	SECTION("o editor abre com o valor guardado, e não com o que a célula desenha")
	{
		const QList<int> found = rowsWithLabel(model, QStringLiteral("K1"));
		REQUIRE(found.count() == 1);
		const QModelIndex cell =
				model.index(found.first(),
					    columnOf(model, QETInformation::ELMT_LOCATION_PATH));

		INFO("drawn: " << cell.data(Qt::DisplayRole).toString().toStdString());
		INFO("stored: " << cell.data(Qt::EditRole).toString().toStdString());

			//The two forms of the same value, and the reason the edit
			//role cannot be left to answer the drawn one: a reader who
			//opens this cell and closes it without typing would write
			//the drawn form over the stored one.
		CHECK(cell.data(Qt::EditRole).toString() == QStringLiteral("QCM1/PORTE"));
		CHECK(cell.data(Qt::DisplayRole).toString()
		      != cell.data(Qt::EditRole).toString());
	}

	SECTION("a célula editada muda o componente, e a lista vem de volta do projeto")
	{
		const QList<int> found = rowsWithLabel(model, QStringLiteral("K1"));
		REQUIRE(found.count() == 1);
		const int column = columnOf(model, QETInformation::ELMT_DESIGNATION);
		Element *component = model.elementForRow(found.first());
		REQUIRE(component != nullptr);
		REQUIRE(component->elementInformations()
			.value(QETInformation::ELMT_DESIGNATION).toString()
			== QStringLiteral("LC1D09"));

		const int steps_before = stack->count();
		REQUIRE(model.setData(model.index(found.first(), column),
				      QStringLiteral("LC1D18"),
				      Qt::EditRole));

			//One step for one cell. The command of the object merges
			//with its own kind by design, which is what a properties
			//dialogue wants and what a table must not have: two cells
			//edited in a row would be one undo.
		CHECK(stack->count() == steps_before + 1);
		CHECK(component->elementInformations()
		      .value(QETInformation::ELMT_DESIGNATION).toString()
		      == QStringLiteral("LC1D18"));

			//And the list says it too - said by a second model built
			//from nothing, so that what is read is the project and
			//not a value this model wrote into its own record. Had the
			//cell written into the data base or into the record, this
			//witness would read the old value back.
		CHECK(shownValue(scratch.project(), edit_query,
				 QStringLiteral("K1"), QETInformation::ELMT_DESIGNATION)
		      == QStringLiteral("LC1D18"));

		stack->undo();
		CHECK(component->elementInformations()
		      .value(QETInformation::ELMT_DESIGNATION).toString()
		      == QStringLiteral("LC1D09"));
		CHECK(shownValue(scratch.project(), edit_query,
				 QStringLiteral("K1"), QETInformation::ELMT_DESIGNATION)
		      == QStringLiteral("LC1D09"));
	}

	SECTION("fechar a célula sem mudar nada não deixa passo nenhum na pilha")
	{
		const QList<int> found = rowsWithLabel(model, QStringLiteral("K2"));
		REQUIRE(found.count() == 1);
		const int column = columnOf(model, QETInformation::ELMT_DESIGNATION);
		const int steps_before = stack->count();

		CHECK_FALSE(model.setData(model.index(found.first(), column),
					  QStringLiteral("LC1D12"),
					  Qt::EditRole));
		CHECK(stack->count() == steps_before);
	}

	SECTION("a linha que nenhum componente reivindica sozinho não é editável")
	{
		const QList<int> twins = rowsWithLabel(model, QStringLiteral("T1"));
		REQUIRE(twins.count() == 2);
		const int column = columnOf(model, QETInformation::ELMT_DESIGNATION);
		const int steps_before = stack->count();

		for (int row : twins)
		{
			INFO("row " << row);
				//Two components written the same way in every
				//column the list shows are two components the
				//reader cannot tell apart either. Picking one of
				//them would write on a component nobody chose.
			CHECK(model.elementForRow(row) == nullptr);
			CHECK_FALSE(model.flags(model.index(row, column))
				    .testFlag(Qt::ItemIsEditable));
			CHECK_FALSE(model.setData(model.index(row, column),
						  QStringLiteral("CHOISI"),
						  Qt::EditRole));
		}

		CHECK(stack->count() == steps_before);
	}

	SECTION("o rótulo que uma fórmula escreve não é editável; o resto da linha é")
	{
		const QList<int> found = rowsWithLabel(model, QStringLiteral("Q1"));
		REQUIRE(found.count() == 1);
		const int row = found.first();
		REQUIRE(model.elementForRow(row) != nullptr);

			//The column carries what the formula makes, and
			//setElementInformations() writes that back over anything
			//put in its place: the edit would be taken, the undo step
			//would be on the stack, and the cell would go back to
			//saying what it said before.
		CHECK_FALSE(model.flags(model.index(row, columnOf(model, QETInformation::ELMT_LABEL)))
			    .testFlag(Qt::ItemIsEditable));

			//Measured beside it so that the refusal above is read as
			//being about the label and not about the row.
		CHECK(model.flags(model.index(row, columnOf(model, QETInformation::ELMT_DESIGNATION)))
		      .testFlag(Qt::ItemIsEditable));
	}

	SECTION("na lista agrupada, o total é somente leitura e o grupo de vários não é editável")
	{
		ProjectDBModel grouped(scratch.project());
		grouped.setQuery(QStringLiteral(
			"SELECT designation, COUNT(*) AS designation_qty"
			" FROM element_nomenclature_view GROUP BY designation"
			" ORDER BY designation"));
		INFO(grouped.lastError().toStdString());
		REQUIRE(grouped.lastError().isEmpty());
		REQUIRE(grouped.columnCount() == 2);
		REQUIRE(grouped.rowCount() == 4);

		int row_of_two = -1;
		int row_of_one = -1;
		for (int row = 0 ; row < grouped.rowCount() ; ++row)
		{
			const QString designation = grouped.index(row, 0).data().toString();
			if (designation == QStringLiteral("IDENTIQUE")) {
				row_of_two = row;
			} else if (designation == QStringLiteral("LC1D09")) {
				row_of_one = row;
			}
		}
		REQUIRE(row_of_two >= 0);
		REQUIRE(row_of_one >= 0);
		REQUIRE(grouped.index(row_of_two, 1).data().toString()
			== QStringLiteral("2"));

			//A row standing for two components has no component to
			//push a command against.
		CHECK(grouped.elementForRow(row_of_two) == nullptr);
		CHECK_FALSE(grouped.flags(grouped.index(row_of_two, 0))
			    .testFlag(Qt::ItemIsEditable));

			//A group of one does stand for one component, and its
			//designation is editable - which is what makes the next
			//assertion say something: the total beside it is read
			//only because it is a total, and not because the row
			//could not be traced back.
		CHECK(grouped.elementForRow(row_of_one) != nullptr);
		CHECK(grouped.flags(grouped.index(row_of_one, 0))
		      .testFlag(Qt::ItemIsEditable));
		CHECK_FALSE(grouped.flags(grouped.index(row_of_one, 1))
			    .testFlag(Qt::ItemIsEditable));
	}

	SECTION("o estilo do tabuleiro continua sendo lido e escrito na célula (0,0)")
	{
			//The roles the editor of the folio table writes there,
			//and which toXml() saves. The edit role now has a meaning
			//of its own on that very cell, so the two have to be
			//measured together or the next reader will not know they
			//coexist.
		const QModelIndex first = model.index(0, 0);
		REQUIRE(model.setData(first, QVariant(int(Qt::AlignRight)),
				      Qt::TextAlignmentRole));
		CHECK(first.data(Qt::TextAlignmentRole).toInt() == int(Qt::AlignRight));
		CHECK_FALSE(first.data(Qt::DisplayRole).toString().isEmpty());
	}
}
