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

#include "../../../../sources/cli_export.h"

#include <catch2/catch.hpp>

#include <QDir>
#include <QFile>
#include <QPicture>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>

/*
	The folio exported to SVG by the command line, as the arrangement of
	the conductors was checked with (T67). Every symbol came out at 72/96
	of its size around its own origin - a QPicture played on the 72 dpi of
	the generator is scaled by the ratio - while the conductors kept
	theirs, so in the file a terminal stood beside and above the end of
	the lead it belongs to, and the page was a third wider in millimetres
	than the folio.
*/
namespace
{
	QString projectXml()
	{
		const QString definition = QStringLiteral(
			"<element name=\"lead.elmt\">"
			"<definition type=\"element\" version=\"0.80\" width=\"20\" height=\"40\""
			" hotspot_x=\"10\" hotspot_y=\"20\" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">lead</name></names>"
			"<description>"
			"<rect x=\"-8\" y=\"-10\" width=\"16\" height=\"20\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<line x1=\"0\" y1=\"-20\" x2=\"0\" y2=\"-10\" antialias=\"false\""
			" end1=\"none\" end2=\"none\" length1=\"1.5\" length2=\"1.5\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"0\" y=\"-20\" orientation=\"n\"/>"
			"</description></definition></element>");
		return QStringLiteral(
			"<project title=\"svg\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Svg\" order=\"1\" height=\"400\" folio=\"1\""
			" cols=\"6\" colsize=\"60\" rows=\"4\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>"
			"<element x=\"100\" y=\"120\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://bench/lead.elmt\""
			" uuid=\"{69000000-0000-4000-8000-000000000001}\">"
			"<terminals/><inputs/><elementInformations/>"
			"<dynamic_texts/><texts_groups/></element>"
			"</elements><inputs/><conductors/>"
			"</diagram></project>")
			.arg(definition);
	}
}

TEST_CASE("T67 — The SVG of the command line keeps the symbols at their size", "[T67][svg]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("svg.qet"));
	{
		QFile file(project_path);
		REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
		QTextStream(&file) << projectXml();
	}
	const QString out_dir = dir.filePath(QStringLiteral("svg"));
	REQUIRE(CLIExport::run(QStringList() << QStringLiteral("--export-svg")
					     << project_path << out_dir) == 0);

	const QStringList files = QDir(out_dir).entryList({QStringLiteral("*.svg")}, QDir::Files);
	REQUIRE(files.size() == 1);
	QFile svg_file(QDir(out_dir).filePath(files.first()));
	REQUIRE(svg_file.open(QIODevice::ReadOnly | QIODevice::Text));
	const QString svg = QTextStream(&svg_file).readAll();

		//The lead of the symbol, "0,-20 0,-10" in its own terms, drawn by
		//a group whose matrix keeps lengths: a turn, never a shrink
	static const QRegularExpression group(
		QStringLiteral("<g [^>]*transform=\"matrix\\(([^)]*)\\)\"[^>]*>(.*?)</g>"),
		QRegularExpression::DotMatchesEverythingOption);
	int leads = 0;
	auto groups = group.globalMatch(svg);
	while (groups.hasNext())
	{
		const auto g = groups.next();
		if (!g.captured(2).contains(QStringLiteral("points=\"0,-20 0,-10"))) continue;
		++leads;
		const QStringList m = g.captured(1).split(QLatin1Char(','));
		REQUIRE(m.size() == 6);
		CAPTURE(g.captured(1));
		CHECK(std::hypot(m.at(0).toDouble(), m.at(1).toDouble()) == Approx(1));
		CHECK(std::hypot(m.at(2).toDouble(), m.at(3).toDouble()) == Approx(1));
	}
	CHECK(leads == 1);

		//And the page as wide in millimetres as the folio is, at the
		//resolution the symbols are recorded at
	static const QRegularExpression size(
		QStringLiteral("<svg width=\"([0-9.]+)mm\"[^>]*viewBox=\"0 0 ([0-9.]+) "),
		QRegularExpression::DotMatchesEverythingOption);
	const auto s = size.match(svg);
	REQUIRE(s.hasMatch());
	CAPTURE(s.captured(1), s.captured(2));
	CHECK(s.captured(1).toDouble()
	      == Approx(s.captured(2).toDouble() / QPicture().logicalDpiX() * 25.4).margin(0.5));
}
