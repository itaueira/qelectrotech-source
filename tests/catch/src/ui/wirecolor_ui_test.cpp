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

#include "../../../../sources/cli_export.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/conductortextitem.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/wirecolorname.h"

#include <catch2/catch.hpp>

#include <QColor>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <cmath>

/*
	The square of the colour of a wire, drawn over the marker %wq puts in
	its text, and the colour of the wire in the lists (T70).

	The bench is two wires: a blue one running across, between two
	components side by side, and a green and yellow one running down,
	between two components one above the other - so that its text is turned
	by 270 degrees, as the text of every vertical stretch is, and the square
	has to turn with it.

	The colours are read out of pixels, never compared against a stored
	image: the font the text is drawn with is whatever the machine has (see
	uibench.h), and what does not move with it is where the square is,
	which the item itself says (ConductorTextItem::colorSquareRects()).
*/
namespace
{
		/// A component with a terminal on each side, and one with a
		/// terminal above and one below
	QString definitions()
	{
		auto symbol = [](const QString &name, const QString &terminals)
		{
			return QStringLiteral(
				"<element name=\"%1\">"
				"<definition type=\"element\" version=\"0.80\""
				" width=\"20\" height=\"20\" hotspot_x=\"10\" hotspot_y=\"10\""
				" orientation=\"dnnn\" link_type=\"simple\">"
				"<names><name lang=\"en\">%1</name></names>"
				"<description>"
				"<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\" antialias=\"false\""
				" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
				"%2"
				"</description>"
				"</definition>"
				"</element>")
				.arg(name, terminals);
		};
		return symbol(QStringLiteral("across.elmt"),
			      QStringLiteral("<terminal x=\"10\" y=\"0\" orientation=\"e\"/>"
					     "<terminal x=\"-10\" y=\"0\" orientation=\"w\"/>"))
			+ symbol(QStringLiteral("upright.elmt"),
				 QStringLiteral("<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"
						"<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"));
	}

	/**
		One component of the bench. Its terminals are written at their
		docking point, four units inside the point the symbol declares,
		because that is what the loading path matches an instance terminal
		to a symbol terminal by (see wirenumberlist_test.cpp).
		@param across : the component with a terminal on each side
		@param first_id : the identifier of its first terminal; the second
		one is the next
	*/
	QString instance(bool across, int x, int y, int first_id, const char *label)
	{
		const qreal near_end = 10. - Terminal::terminalSize;
		const QString terminals = across
			? QStringLiteral("<terminal x=\"%1\" y=\"0\" orientation=\"1\" id=\"%2\"/>"
					 "<terminal x=\"%3\" y=\"0\" orientation=\"3\" id=\"%4\"/>")
				  .arg(near_end).arg(first_id).arg(-near_end).arg(first_id + 1)
			: QStringLiteral("<terminal x=\"0\" y=\"%1\" orientation=\"0\" id=\"%2\"/>"
					 "<terminal x=\"0\" y=\"%3\" orientation=\"2\" id=\"%4\"/>")
				  .arg(-near_end).arg(first_id).arg(near_end).arg(first_id + 1);
		return QStringLiteral(
			"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://bench/%3\""
			" uuid=\"{70700000-0000-4000-8000-0000000000%4}\">"
			"<terminals>%5</terminals><inputs/>"
			"<elementInformations>"
			"<elementInformation show=\"1\" name=\"label\">%6</elementInformation>"
			"</elementInformations>"
			"<dynamic_texts/><texts_groups/>"
			"</element>")
			.arg(x).arg(y)
			.arg(across ? QStringLiteral("across.elmt") : QStringLiteral("upright.elmt"))
			.arg(first_id, 2, 10, QLatin1Char('0'))
			.arg(terminals, QLatin1String(label));
	}

	/**
		@param blue_text : the text of the blue wire, A1 to B1
		@param two_colour_text : the text of the green and yellow wire, A2
		to B2
		@return the whole .qet
	*/
	QString fixtureXml(const QString &blue_text, const QString &two_colour_text)
	{
		const QString elements = instance(true, 100, 150, 0, "A1")
			+ instance(true, 300, 150, 2, "B1")
			+ instance(false, 500, 100, 4, "A2")
			+ instance(false, 500, 400, 6, "B2");

		auto conductor = [](int from, int to, const QString &text, const QString &colours)
		{
			return QStringLiteral(
				"<conductor terminal1=\"%1\" terminal2=\"%2\" num=\"%3\" type=\"multi\""
				" freezeLabel=\"false\" displaytext=\"1\" numsize=\"9\" condsize=\"1\""
				" vertirotatetext=\"270\" horizrotatetext=\"0\" %4/>")
				.arg(QString::number(from), QString::number(to),
				     text.toHtmlEscaped(), colours);
		};
			//A1 east to B1 west, across; A2 south to B2 north, down
		const QString conductors =
			conductor(0, 3, blue_text,
				  QStringLiteral("color=\"#0000ff\" bicolor=\"false\""))
			+ conductor(5, 6, two_colour_text,
				    QStringLiteral("color=\"#00ff00\" bicolor=\"true\" color2=\"#ffff00\""));

		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"600\""
			" cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors>%3</conductors>"
			"</diagram>"
			"</project>")
			.arg(definitions(), elements, conductors);
	}

	Conductor *wireOfColour(Diagram *sheet, const QColor &colour)
	{
		const QList<Conductor *> wires = sheet->conductors();
		for (Conductor *wire : wires) {
			if (wire->properties().color == colour) return wire;
		}
		return nullptr;
	}

		/// The formula of @a wire set to @a formula, as the properties
		/// dialogue sets it
	void numberWith(Conductor *wire, const QString &formula)
	{
		ConductorProperties properties = wire->properties();
		properties.m_formula = formula;
		wire->setProperties(properties);
	}

		/// A part of the folio drawn into an image, the way the export
		/// draws a folio (Diagram::render()), @a scale pixels to a unit
	struct Picture
	{
		QImage image;
		QPointF origin;
		int scale = 1;

			/// @return the pixel under @a point of the folio, 0 outside
		QRgb at(const QPointF &point) const
		{
			const QPointF p = (point - origin) * scale;
			const int x = int(std::floor(p.x()));
			const int y = int(std::floor(p.y()));
			return image.valid(x, y) ? image.pixel(x, y) : QRgb(0);
		}
	};

	Picture drawn(Diagram *sheet, const QGraphicsItem *item, int scale)
	{
			//Whole units, so that the image is exactly the source scaled
		const QRectF source(item->sceneBoundingRect().adjusted(-4, -4, 4, 4).toAlignedRect());
		Picture picture;
		picture.origin = source.topLeft();
		picture.scale = scale;
		picture.image = QImage(source.size().toSize() * scale, QImage::Format_ARGB32);
		picture.image.fill(Qt::white);

			//As the export does: no grid
		const bool grid = sheet->displayGrid();
		sheet->setDisplayGrid(false);
		QPainter painter(&picture.image);
		painter.setRenderHint(QPainter::Antialiasing, true);
		sheet->render(&painter, QRectF(QPointF(0, 0), source.size() * scale),
			      source, Qt::KeepAspectRatio);
		painter.end();
		sheet->setDisplayGrid(grid);
		return picture;
	}

		/// The point at @a u across and @a v down of @a square, from 0 to
		/// 1 on each side, in the coordinates of the folio
	QPointF inSquare(const QGraphicsItem *text, const QRectF &square, qreal u, qreal v)
	{
		return text->mapToScene(square.topLeft()
					+ QPointF(u * square.width(), v * square.height()));
	}

	QString describe(QRgb pixel)
	{
		return QColor(pixel).name();
	}

	bool writeFile(const QString &path, const QString &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		file.write(content.toUtf8());
		return true;
	}

		/// The text of @a path, without the byte order mark the wiring
		/// list starts with
	QString readFile(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QString();
		}
		QString text = QString::fromUtf8(file.readAll());
		if (text.startsWith(QChar(0xFEFF))) {
			text.remove(0, 1);
		}
		text.remove(QLatin1Char('\r'));
		return text;
	}

		/// The rows of @a csv, cut at the ';'; none of the cells of this
		/// bench holds one, nor a quote
	QList<QStringList> rowsOf(const QString &csv)
	{
		QList<QStringList> rows;
		const QStringList lines = csv.split(QLatin1Char('\n'));
		for (const QString &line : lines) {
			if (!line.isEmpty()) rows << line.split(QLatin1Char(';'));
		}
		return rows;
	}
}

TEST_CASE("CU-70.5 — The square of the colour of the wire", "[T70][wirecolor]")
{
	UiBench::ScratchProject bench(fixtureXml(QStringLiteral("F1-1"), QStringLiteral("F1-2")),
				      QStringLiteral("wirecolor.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->conductors().size() == 2);
	REQUIRE(sheet->drawColoredConductors());

	Conductor *blue = wireOfColour(sheet, QColor(0, 0, 255));
	Conductor *two = wireOfColour(sheet, QColor(0, 255, 0));
	REQUIRE(blue != nullptr);
	REQUIRE(two != nullptr);
	REQUIRE(two->properties().m_bicolor);
	REQUIRE(two->properties().m_color_2 == QColor(255, 255, 0));

		//No marker, no square: the counter-needle of what follows
	REQUIRE(blue->textItem()->colorSquareRects().isEmpty());

	numberWith(blue, QString::fromUtf8("F%id-1 (%wq 1mm²)"));
	numberWith(two, QString::fromUtf8("F%id-2 (%wq 1mm²)"));

		//The variable puts the marker in the text, which is what the file
		//and the lists are given
	CHECK(blue->properties().text == QString::fromUtf8("F1-1 (■ 1mm²)"));
	CHECK(two->properties().text == QString::fromUtf8("F1-2 (■ 1mm²)"));

	ConductorTextItem *blue_text = blue->textItem();
	ConductorTextItem *two_text = two->textItem();
	const QList<QRectF> blue_squares = blue_text->colorSquareRects();
	const QList<QRectF> two_squares = two_text->colorSquareRects();
	REQUIRE(blue_squares.size() == 1);
	REQUIRE(two_squares.size() == 1);
		//A square with room inside it, and inside the text
	REQUIRE(blue_squares.first().width() > 2);
	REQUIRE(blue_squares.first().height() > 2);
	CHECK(blue_text->boundingRect().intersects(blue_squares.first()));
	CHECK(two_text->boundingRect().intersects(two_squares.first()));

		//The text of the stretch running down is turned, the other is not
	CHECK(blue_text->rotation() == Approx(0));
	CHECK(two_text->rotation() == Approx(270));

	SECTION("the blue wire: a blue square")
	{
		const Picture picture = drawn(sheet, blue_text, 8);
		const QRectF square = blue_squares.first();
		int samples = 0;
		int blue_pixels = 0;
		QStringList seen;
		for (int i = 0 ; i <= 4 ; ++i)
		{
			for (int j = 0 ; j <= 4 ; ++j)
			{
				const QRgb pixel = picture.at(inSquare(blue_text, square,
								       0.25 + i * 0.125,
								       0.25 + j * 0.125));
				++samples;
				if (pixel == qRgb(0, 0, 255)) {
					++blue_pixels;
				} else {
					seen << describe(pixel);
				}
			}
		}
		INFO(seen.join(QStringLiteral(" ")).toStdString());
		CHECK(samples == 25);
		CHECK(blue_pixels == samples);
	}

	SECTION("the green and yellow wire: green above the diagonal, yellow below")
	{
		const Picture picture = drawn(sheet, two_text, 8);
		const QRectF square = two_squares.first();
			//Inside the square, and away from its edge and from the
			//diagonal that rises to the right, in the coordinates of the
			//text: turned with it on the folio
		const QList<QPointF> above {{0.2, 0.2}, {0.4, 0.2}, {0.2, 0.4}, {0.3, 0.3}};
		const QList<QPointF> below {{0.8, 0.8}, {0.6, 0.8}, {0.8, 0.6}, {0.7, 0.7}};
		for (const QPointF &uv : above)
		{
			const QRgb pixel = picture.at(inSquare(two_text, square, uv.x(), uv.y()));
			INFO(describe(pixel).toStdString());
			CHECK(pixel == qRgb(0, 255, 0));
		}
		for (const QPointF &uv : below)
		{
			const QRgb pixel = picture.at(inSquare(two_text, square, uv.x(), uv.y()));
			INFO(describe(pixel).toStdString());
			CHECK(pixel == qRgb(255, 255, 0));
		}
	}

	SECTION("a folio that draws no colours draws the square black, like the wire")
	{
		sheet->setDrawColoredConductors(false);
		const Picture picture = drawn(sheet, blue_text, 8);
		sheet->setDrawColoredConductors(true);
		const QRgb pixel = picture.at(inSquare(blue_text, blue_squares.first(), 0.5, 0.5));
		INFO(describe(pixel).toStdString());
		CHECK(pixel == qRgb(0, 0, 0));
	}
}

TEST_CASE("CU-70.6 — The colour of the wire in the lists", "[T70][wirecolor][export]")
{
		//Written to a file of its own and never opened in this process:
		//the command line opens the project itself (see csvexport_test.cpp).
		//The texts carry the marker, as the program saves them.
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("wirecolor.qet"));
	REQUIRE(writeFile(project_path,
			  fixtureXml(QString::fromUtf8("F1-1 (■ 1mm²)"),
				     QString::fromUtf8("F1-2 (■ 1mm²)"))));

	const QString blue_name = QString::fromUtf8("F1-1 (1mm²)");
	const QString two_name = QString::fromUtf8("F1-2 (1mm²)");

	SECTION("the wiring list of the project database: the name and the colour")
	{
		const QString csv_path = dir.filePath(QStringLiteral("wiring.csv"));
		REQUIRE(CLIExport::run(QStringList() << QStringLiteral("--export-wiring")
				       << project_path << csv_path) == 0);
		const QString csv = readFile(csv_path);
		INFO(csv.toStdString());
		CHECK_FALSE(csv.contains(WireColorName::marker()));

		const QList<QStringList> rows = rowsOf(csv);
		REQUIRE(rows.size() == 3);
		const int number = rows.first().indexOf(QStringLiteral("wire_number"));
		const int colour = rows.first().indexOf(QStringLiteral("wire_color"));
		REQUIRE(number >= 0);
		REQUIRE(colour >= 0);

		QStringList pairs;
		for (int i = 1 ; i < rows.size() ; ++i) {
			pairs << rows.at(i).value(number) + QStringLiteral(" | ")
				 + rows.at(i).value(colour);
		}
		pairs.sort();
		CHECK(pairs == QStringList({blue_name + QStringLiteral(" | Blue"),
					    two_name + QStringLiteral(" | Green/Yellow")}));
	}

	SECTION("the wire markers to print: the name and the colour")
	{
		const QString csv_path = dir.filePath(QStringLiteral("wires.csv"));
		REQUIRE(CLIExport::run(QStringList() << QStringLiteral("--export-wires")
				       << project_path << csv_path) == 0);
		const QString csv = readFile(csv_path);
		INFO(csv.toStdString());
		CHECK_FALSE(csv.contains(WireColorName::marker()));

		QStringList rows = csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
		rows.sort();
		CHECK(rows == QStringList({blue_name + QStringLiteral(";Blue"),
					   two_name + QStringLiteral(";Green/Yellow")}));
	}

	SECTION("the wiring list of the drawing: the colour column")
	{
		const QString csv_path = dir.filePath(QStringLiteral("cables.csv"));
		REQUIRE(CLIExport::run(QStringList() << QStringLiteral("--export-cables")
				       << project_path << csv_path) == 0);
		const QString csv = readFile(csv_path);
		INFO(csv.toStdString());
		CHECK_FALSE(csv.contains(WireColorName::marker()));

		const QList<QStringList> rows = rowsOf(csv);
		REQUIRE(rows.size() == 3);
		const int colour = rows.first().indexOf(QStringLiteral("Wire color"));
		REQUIRE(colour >= 0);

		QStringList colours;
		for (int i = 1 ; i < rows.size() ; ++i) {
			colours << rows.at(i).value(colour);
		}
		colours.sort();
		CHECK(colours == QStringList({QStringLiteral("Blue"),
					      QStringLiteral("Green/Yellow")}));
	}
}
