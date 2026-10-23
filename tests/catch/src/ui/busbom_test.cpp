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

#include "../../../../sources/TerminalStrip/UndoCommand/bridgeterminalscommand.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/terminalstripbridge.h"
#include "../../../../sources/bomexport.h"
#include "../../../../sources/cli_export.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetgraphicsitem/terminalbusitem.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/wirearrangement.h"
#include "../../../../sources/wirecolorname.h"

#include <catch2/catch.hpp>

#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QUndoStack>

/*
	The bridge of a strip in the bill of materials (T87): the comb or the
	jumper the fitter puts on the terminals is bought apart, so each bridge
	is a line of the list - what it is, its poles, its strip -, made, changed
	and undone with the bridge, and kept by the file.

	The bench of terminalbus_test.cpp, with definitions of its own names: a
	strip X4 of six terminals 1.1 to 3.2 in two levels, a terminal 5 of a
	strip X1 feeding the commons, three boards on the left and three loads
	on the right.
*/
namespace
{
	QString definition(const char *name, const QString &body, bool terminal)
	{
		return QStringLiteral(
			"<element name=\"%1\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"20\" hotspot_x=\"10\" hotspot_y=\"10\""
			" orientation=\"dnnn\"%3>"
			"<names><name lang=\"en\">%1</name></names>"
			"<description>"
			"<rect x=\"-6\" y=\"-6\" width=\"12\" height=\"12\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"%2"
			"</description>"
			"</definition>"
			"</element>")
			.arg(QLatin1String(name), body,
			     terminal ? QStringLiteral(" link_type=\"terminal\"") : QStringLiteral(" link_type=\"simple\""));
	}

	QString element(const char *def, int x, int y, const QString &label, const QString &comment, int n)
	{
		return QStringLiteral(
			"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\" freezeLabel=\"false\""
			" orientation=\"0\" type=\"embed://bench/%3\""
			" uuid=\"{87b00000-0000-4000-8000-0000000000%4}\">"
			"<terminals/><inputs/>"
			"<elementInformations>"
			"<elementInformation show=\"1\" name=\"label\">%5</elementInformation>"
			"<elementInformation show=\"0\" name=\"comment\">%6</elementInformation>"
			"</elementInformations>"
			"<dynamic_texts>"
			"<dynamic_elmt_text Halignment=\"AlignLeft\" Valignment=\"AlignTop\""
			" font=\"Sans Serif,6,-1,5,50,0,0,0,0,0\" frame=\"false\""
			" keep_visual_rotation=\"false\" rotation=\"0\" rotation_point_center=\"false\""
			" text_from=\"ElementInfo\" text_width=\"-1\""
			" uuid=\"{87b00000-0000-4000-8000-0000000001%4}\" x=\"8\" y=\"-4\">"
			"<text/><info_name>label</info_name></dynamic_elmt_text>"
			"</dynamic_texts><texts_groups/>"
			"</element>")
			.arg(x).arg(y).arg(QLatin1String(def)).arg(n, 2, 10, QLatin1Char('0')).arg(label, comment);
	}

	QString fixture()
	{
		const QString sides = QStringLiteral("<terminal x=\"-14\" y=\"0\" orientation=\"w\"/>"
						    "<terminal x=\"14\" y=\"0\" orientation=\"e\"/>");
		const QString definitions =
			definition("bom-strip.elmt", sides, true)
			+ definition("bom-board.elmt", QStringLiteral("<terminal x=\"14\" y=\"0\" orientation=\"e\"/>"), false)
			+ definition("bom-load.elmt", QStringLiteral("<terminal x=\"-14\" y=\"0\" orientation=\"w\"/>"), false);
		QString xml;
		const QStringList labels {QStringLiteral("1.1"), QStringLiteral("1.2"), QStringLiteral("2.1"),
					  QStringLiteral("2.2"), QStringLiteral("3.1"), QStringLiteral("3.2")};
		for (int i = 0 ; i < labels.size() ; ++i) {
			xml += element("bom-strip.elmt", 400, 200 + 20 * i, labels.at(i), QStringLiteral("X4"), i);
		}
		xml += element("bom-strip.elmt", 300, 120, QStringLiteral("5"), QStringLiteral("X1"), 10);
		for (int i = 0 ; i < 3 ; ++i)
		{
			xml += element("bom-board.elmt", 240, 200 + 40 * i, QStringLiteral("Q%1").arg(i + 1), QString(), 20 + i);
			xml += element("bom-load.elmt", 520, 220 + 40 * i, QStringLiteral("Y%1").arg(i + 1), QString(), 30 + i);
		}
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"bench\">%1</category></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"800\" folio=\"1\""
			" cols=\"17\" colsize=\"60\" rows=\"10\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements><inputs/><conductors/>"
			"</diagram>"
			"</project>")
			.arg(definitions, xml);
	}

		/// No arranging of the wires the bench draws
	class Setup
	{
		public:
			Setup() : m_arrange(WireArrangement::arrangesAutomatically()) {
				WireArrangement::setArrangesAutomatically(false);
			}
			~Setup() {WireArrangement::setArrangesAutomatically(m_arrange);}
		private:
			bool m_arrange;
	};

	Element *byLabel(Diagram *sheet, const QString &label)
	{
		for (Element *e : sheet->elements()) {
			if (e->actualLabel() == label) return e;
		}
		return nullptr;
	}

	Conductor *wire(Diagram *sheet, Terminal *a, Terminal *b)
	{
		auto *conductor = new Conductor(a, b);
		sheet->addItem(conductor);
		return conductor;
	}

		/// The bench wired as in terminalbus_test.cpp: the feed from X1:5
		/// to 1.2, from 1.2 a wire to each other common, the field wires on
		/// the right of the commons, the outputs on the left of the signals
	struct Wired
	{
		explicit Wired(Diagram *sheet_) : sheet(sheet_)
		{
			for (const QString &label : {QStringLiteral("1.1"), QStringLiteral("1.2"), QStringLiteral("2.1"),
						     QStringLiteral("2.2"), QStringLiteral("3.1"), QStringLiteral("3.2")}) {
				x4 << byLabel(sheet, label);
			}
			x1 = byLabel(sheet, QStringLiteral("5"));
			if (x4.contains(nullptr) || !x1) return;
			feed = wire(sheet, x1->terminals().at(1), x4.at(1)->terminals().at(0));
			wire(sheet, x4.at(1)->terminals().at(0), x4.at(3)->terminals().at(0));
			wire(sheet, x4.at(1)->terminals().at(0), x4.at(5)->terminals().at(0));
			for (int i = 0 ; i < 3 ; ++i)
			{
				wire(sheet, x4.at(2 * i + 1)->terminals().at(1),
				     byLabel(sheet, QStringLiteral("Y%1").arg(i + 1))->terminals().at(0));
				wire(sheet, byLabel(sheet, QStringLiteral("Q%1").arg(i + 1))->terminals().at(0),
				     x4.at(2 * i)->terminals().at(0));
			}
		}
		QList<Element *> commons() const {return {x4.at(1), x4.at(3), x4.at(5)};}
		QList<Element *> signalTerminals() const {return {x4.at(0), x4.at(2), x4.at(4)};}

		Diagram *sheet;
		QList<Element *> x4;
		Element *x1 = nullptr;
		Conductor *feed = nullptr;
	};

		/// The designation the list gives a bridge of @a n poles, in the
		/// words of the data base, whatever the language loaded
	QString bridgeOf(int n)
	{
		return projectDataBase::tr("Bridge, %1 poles",
					   "bill of materials: the comb or jumper joining %1 terminals of a strip")
			.arg(n);
	}

		/// One line of a bridge, as the nomenclature view holds it
	struct BridgeLine
	{
		QString label;
		QString designation;
		QString quantity;
		QString description;
		QString type;
	};

		/// The lines of the bridges in the nomenclature view, the view
		/// every bill of materials of the program reads
	QList<BridgeLine> bridgeLines(QETProject *project)
	{
		QList<BridgeLine> lines;
		QSqlQuery query = project->dataBase()->newQuery(QStringLiteral(
			"SELECT label, designation, quantity, description, element_type"
			" FROM element_nomenclature_view WHERE element_sub_type = 'bridge'"
			" ORDER BY diagram_position, position, designation"));
		if (!query.exec()) {
			return lines;
		}
		while (query.next())
		{
			BridgeLine line;
			line.label = query.value(0).toString();
			line.designation = query.value(1).toString();
			line.quantity = query.value(2).toString();
			line.description = query.value(3).toString();
			line.type = query.value(4).toString();
			lines << line;
		}
		return lines;
	}

		/// How many rows of the default export query carry @a designation
	int exportRows(QETProject *project, const QString &designation)
	{
		QSqlQuery query = project->dataBase()->newQuery(BomExport::defaultQuery());
		if (!query.exec()) {
			return -1;
		}
		const int column = query.record().indexOf(QStringLiteral("designation"));
		int rows = 0;
		while (query.next()) {
			if (query.value(column).toString() == designation) ++rows;
		}
		return rows;
	}

	QString readFile(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			return QString();
		}
		return QString::fromUtf8(file.readAll());
	}
}

TEST_CASE("T87 - A bridge is a line of the bill of materials", "[T87][bom]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture(), QStringLiteral("busbom.qet"));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	CHECK(bridgeLines(project.project()).isEmpty());

	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	const QSharedPointer<RealTerminal> first = TerminalBusItem::realTerminalOf(wired.x4.at(1));
	REQUIRE(first);
	const QSharedPointer<TerminalStripBridge> bridge = first->bridge();
	REQUIRE(bridge);

		//One line: the bridge of three poles of the strip X4, one piece,
		//listed with the terminals
	QList<BridgeLine> lines = bridgeLines(project.project());
	REQUIRE(lines.size() == 1);
	CHECK(lines.first().label == QStringLiteral("X4"));
	CHECK(lines.first().designation == bridgeOf(3));
	CHECK(lines.first().quantity == QStringLiteral("1"));
	CHECK(lines.first().type == QStringLiteral("terminal"));
	CHECK(lines.first().description == WireColorName::name(bridge->color()));
	CHECK_FALSE(lines.first().description.isEmpty());
		//And the export takes it: neither its kind nor the junction rule
		//keeps it out
	CHECK(exportRows(project.project(), bridgeOf(3)) == 1);

		//One undo: no strip, no bridge, no line
	sheet->undoStack().undo();
	CHECK(project->terminalStrip().isEmpty());
	CHECK(bridgeLines(project.project()).isEmpty());
	CHECK(exportRows(project.project(), bridgeOf(3)) == 0);

		//And again
	sheet->undoStack().redo();
	lines = bridgeLines(project.project());
	REQUIRE(lines.size() == 1);
	CHECK(lines.first().designation == bridgeOf(3));
	CHECK(lines.first().quantity == QStringLiteral("1"));
}

TEST_CASE("T87 - The bridge line follows its poles and its colour", "[T87][bom]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture(), QStringLiteral("busbompoles.qet"));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	REQUIRE(project->terminalStrip().size() == 1);
	TerminalStrip *strip = project->terminalStrip().first();
	const QSharedPointer<RealTerminal> last = TerminalBusItem::realTerminalOf(wired.x4.at(5));
	REQUIRE(last);
	const QSharedPointer<TerminalStripBridge> bridge = last->bridge();
	REQUIRE(bridge);

		//3.2 off the bridge: two poles
	sheet->undoStack().push(new UnBridgeTerminalsCommand(strip, {last}));
	REQUIRE_FALSE(last->bridge());
	QList<BridgeLine> lines = bridgeLines(project.project());
	REQUIRE(lines.size() == 1);
	CHECK(lines.first().designation == bridgeOf(2));
		//Back on it: three
	sheet->undoStack().undo();
	lines = bridgeLines(project.project());
	REQUIRE(lines.size() == 1);
	CHECK(lines.first().designation == bridgeOf(3));
	sheet->undoStack().redo();
	lines = bridgeLines(project.project());
	REQUIRE(lines.size() == 1);
	CHECK(lines.first().designation == bridgeOf(2));

		//Another colour, in the description
	bridge->setColor(Qt::red);
	lines = bridgeLines(project.project());
	REQUIRE(lines.size() == 1);
	CHECK(lines.first().description == WireColorName::name(QColor(Qt::red)));

		//Saved and opened again: the same line
	INFO(project.error().toStdString());
	REQUIRE(project.saveAndReopen());
	lines = bridgeLines(project.project());
	REQUIRE(lines.size() == 1);
	CHECK(lines.first().label == QStringLiteral("X4"));
	CHECK(lines.first().designation == bridgeOf(2));
	CHECK(lines.first().quantity == QStringLiteral("1"));
	CHECK(lines.first().description == WireColorName::name(QColor(Qt::red)));
}

TEST_CASE("T87 - Equal bridges add up in the exported bill of materials", "[T87][bom]")
{
	Setup setup;
	UiBench::ScratchProject project(fixture(), QStringLiteral("busbomsum.qet"));
	REQUIRE(project.isOpen());
	Diagram *sheet = project.diagram(0);
	Wired wired(sheet);
	REQUIRE(wired.feed);
		//The commons and the signals: two bridges of three poles on X4
	REQUIRE(TerminalBusItem::join(sheet, wired.commons(), QString()));
	QString why;
	REQUIRE(TerminalBusItem::join(sheet, wired.signalTerminals(), QString(), &why));
	CHECK(why.isEmpty());
	const QList<BridgeLine> lines = bridgeLines(project.project());
	REQUIRE(lines.size() == 2);
	CHECK(lines.at(0).designation == bridgeOf(3));
	CHECK(lines.at(1).designation == bridgeOf(3));

		//The command line, on a copy of the file saved: it opens the
		//project by itself, and the list groups the parts that are alike
	INFO(project.error().toStdString());
	REQUIRE(project.saveAndReopen());
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
		//Under the name it was saved with: the suffix says the format
	const QString copy = dir.filePath(QFileInfo(project.filePath()).fileName());
	const QString csv_path = dir.filePath(QStringLiteral("bom.csv"));
	REQUIRE(QFile::copy(project.filePath(), copy));
	REQUIRE(CLIExport::run(QStringList() << QStringLiteral("--export-bom") << copy << csv_path) == 0);

	const QString csv = readFile(csv_path);
	INFO(csv.toStdString());
	const QStringList rows = csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
	REQUIRE(rows.size() > 1);
	const QStringList header = rows.first().split(QLatin1Char(';'));
	const int designation = static_cast<int>(header.indexOf(QStringLiteral("designation")));
	const int count = static_cast<int>(header.indexOf(QStringLiteral("designation_qty")));
	const int label = static_cast<int>(header.indexOf(QStringLiteral("label")));
	REQUIRE(designation >= 0);
	REQUIRE(count >= 0);
	REQUIRE(label >= 0);
	QStringList counts;
	for (int i = 1 ; i < rows.size() ; ++i)
	{
		const QStringList cells = rows.at(i).split(QLatin1Char(';'));
		if (cells.value(designation) == bridgeOf(3))
		{
			counts << cells.value(count);
			CHECK(cells.value(label) == QStringLiteral("X4"));
		}
	}
		//One line of two: the same part on the same strip
	CHECK(counts == QStringList{QStringLiteral("2")});
}
