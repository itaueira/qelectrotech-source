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

#include "../../../../sources/diagram.h"
#include "../../../../sources/plc/ioassignment.h"
#include "../../../../sources/plc/iolist.h"
#include "../../../../sources/plc/iopoint.h"
#include "../../../../sources/plc/ui/ioassigndialog.h"
#include "../../../../sources/plc/ui/iolistdialog.h"
#include "../../../../sources/properties/elementdata.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUndoStack>

/*
	Putting the imported points into the channels of a card that is drawn on a
	folio, through the two windows a person uses to do it.

	Which point may go into which channel is already proved without a project
	open, in ioassignment_test.cpp: the direction rule, the universal channel,
	the name a channel answers to, the cell that is never overwritten. That is
	seven cases and two hundred and forty four assertions, and none of it is
	repeated here.

	What is proved here is what only exists once the card is an Element on a
	sheet: that ticking the points in the window writes the descriptions into
	the card that is drawn, that the sheet then draws more than it drew, that
	one Ctrl+Z takes back the card half and the list half together, and that
	correcting a description in the list window changes what the folio says
	without anybody reopening the drawing.

	The ink is measured and not looked at. Comparing the sheet against a stored
	image would go red for the font of the machine, which is why uibench does
	not do that anywhere; what is compared here is the sheet against itself,
	before and after, and the direction of the change is the assertion. That
	the ink comes back to exactly the number it started from is the strongest
	thing this bench can say about an undo, because it is measured on the
	drawing and not on the model that feeds it.

	The two checks on that ink pass in the Qt6 build and fail in the Qt5 one,
	and the difference was measured rather than guessed. Rendering the same
	sheet three ways - no channel table, a table of sixteen empty channels, a
	table of sixteen descriptions - gives 10769 / 23492 / 26508 pixels of ink
	under Qt6 and 8834 / 17630 / 17630 under Qt5. So the grid of the table is
	drawn in both, and in the Qt5 build the text inside it adds nothing at
	all. Whether that is the empty font family PlcMasterData starts its
	headerFont and cellFont with, or a font engine the Qt5 offscreen platform
	of this machine does not have, is not settled here - but the model side
	of the same case passes in both builds, so what differs is the drawing
	and not the assignment.
*/

namespace
{
	const QString card_uuid =
		QStringLiteral("{cafe0000-0000-4000-8000-0000000000a1}");

	/**
		The address the card gives its channel @a index, as an automation
		writes it. Concatenated and not built with arg(): the per cent sign
		is what a place marker starts with, and a channel called %%I0.0 would
		still contain %I0.0 - so a test written that way would pass while
		measuring an address nobody uses.
	*/
	QString channelAddress(int index)
	{
		return QLatin1String("%I0.") + QString::number(index);
	}

	/// The channels a card of @a count digital inputs offers, empty of function.
	QString channelsXml(int count)
	{
		QString rows;
		for (int index = 0 ; index < count ; ++ index)
		{
			rows += QStringLiteral("<plcIO type=\"entree_digitale\""
					       " address=\"%1\" functionText=\"\""
					       " comment=\"\" crossRef=\"\""
					       " terminalCount=\"1\"/>")
				.arg(channelAddress(index));
		}

		return QStringLiteral("<plcMasterData rowHeight=\"8.00\">"
				      "<breakPositions/><columnWidths/>"
				      "<columnVisibility/>"
				      "<plcIOs>%1</plcIOs>"
				      "</plcMasterData>").arg(rows);
	}

	/**
		A project holding one PLC card, drawn on one sheet.

		The card is a master element of the plc kind, which is what makes the
		program draw its channel table on the folio at all - and what makes the
		assignment window offer it in the first place.
	*/
	QString fixtureXml(int channels)
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">"
			       "<element name=\"plccard.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"40\" height=\"60\" hotspot_x=\"20\" hotspot_y=\"30\""
			       " orientation=\"dnnn\" link_type=\"master\">"
			       "<names><name lang=\"en\">PLC card</name></names>"
			       "<kindInformations>"
			       "<kindInformation name=\"type\">plc</kindInformation>"
			       "</kindInformations>"
			       "<description>"
			       "<rect x=\"-15\" y=\"-25\" width=\"30\" height=\"50\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"0\" y=\"-25\" orientation=\"n\"/>"
			       "<terminal x=\"0\" y=\"25\" orientation=\"s\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>"
			       "<element x=\"120\" y=\"120\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/plccard.elmt\""
			       " uuid=\"%1\">"
			       "<terminals/><inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">A1"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "%2"
			       "</element>"
			       "</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(card_uuid, channelsXml(channels));
	}

	/// @a count digital input points, named the way an automation sheet names them.
	IoList pointList(int count)
	{
		IoList list;
		for (int index = 0 ; index < count ; ++ index)
		{
			IoPoint point;
			point.type = ElementData::EntreeDigitale;
			point.tag = QStringLiteral("S%1").arg(index + 1, 2, 10,
							      QLatin1Char('0'));
			point.description = QStringLiteral("Sensor %1")
					    .arg(index + 1, 2, 10, QLatin1Char('0'));
			list.append(point);
		}
		return list;
	}

	Element *cardOf(UiBench::ScratchProject &bench)
	{
		const QList<Diagram *> sheets = bench.diagrams();
		for (Diagram *sheet : sheets)
		{
			const QList<Element *> elements = sheet->elements();
			for (Element *element : elements)
			{
				if (element->uuid().toString() == card_uuid) {
					return element;
				}
			}
		}
		return nullptr;
	}

	/// The function text of every channel of @a card, in channel order.
	QStringList functionTextsOf(Element *card)
	{
		QStringList texts;
		if (!card) {
			return texts;
		}
		const QVector<ElementData::PlcIO> ios =
			card->elementData().plcMasterData().ios;
		for (const ElementData::PlcIO &io : ios) {
			texts << io.functionText;
		}
		return texts;
	}

	int filledChannels(Element *card)
	{
		int filled = 0;
		const QStringList texts = functionTextsOf(card);
		for (const QString &text : texts)
		{
			if (!text.isEmpty()) {
				++ filled;
			}
		}
		return filled;
	}

	int assignedPoints(const IoList &list)
	{
		int assigned = 0;
		for (int index = 0 ; index < list.count() ; ++ index)
		{
			if (list.at(index).isAssigned()) {
				++ assigned;
			}
		}
		return assigned;
	}

	QPushButton *buttonNamed(QWidget &parent, const QString &text)
	{
		const QList<QPushButton *> buttons = parent.findChildren<QPushButton *>();
		for (QPushButton *button : buttons)
		{
			if (button->text() == text) {
				return button;
			}
		}
		return nullptr;
	}

	/**
		The table whose first column is called @a header.

		By the header and not by the order the two tables were built in: the
		left one holds the free points and the right one the channels of the
		card, and reading one for the other would make every count below mean
		the opposite of what it says.
	*/
	QTableWidget *tableHeaded(QWidget &parent, const QString &header)
	{
		const QList<QTableWidget *> tables = parent.findChildren<QTableWidget *>();
		for (QTableWidget *table : tables)
		{
			QTableWidgetItem *item = table->horizontalHeaderItem(0);
			if (item && item->text() == header) {
				return table;
			}
		}
		return nullptr;
	}

	/// The paragraph the window writes under the two tables.
	QString summaryOf(QWidget &parent, const QString &needle)
	{
		const QList<QLabel *> labels = parent.findChildren<QLabel *>();
		for (QLabel *label : labels)
		{
			if (label->text().contains(needle)) {
				return label->text();
			}
		}
		return QString();
	}

	/// Tick every row of the free points table, the way a person ticks them.
	void tickAll(QTableWidget *points)
	{
		for (int row = 0 ; row < points->rowCount() ; ++ row)
		{
			QTableWidgetItem *item = points->item(row, 0);
			if (item) {
				item->setCheckState(Qt::Checked);
			}
		}
	}

	int columnHeaded(QTreeWidget *tree, const QString &header)
	{
		QTreeWidgetItem *labels = tree ? tree->headerItem() : nullptr;
		if (!labels) {
			return -1;
		}
		for (int column = 0 ; column < tree->columnCount() ; ++ column)
		{
			if (labels->text(column) == header) {
				return column;
			}
		}
		return -1;
	}

	/// The row of the tree whose first cell reads @a name, at any depth.
	QTreeWidgetItem *rowNamed(QTreeWidget *tree, const QString &name)
	{
		const QList<QTreeWidgetItem *> found =
			tree->findItems(name, Qt::MatchExactly | Qt::MatchRecursive, 0);
		return found.isEmpty() ? nullptr : found.first();
	}
}

TEST_CASE("T11 — dezesseis pontos entram no cartão desenhado, e um Ctrl+Z devolve as duas metades",
	  "[uibench][plc]")
{
	UiBench::ScratchProject bench(fixtureXml(16));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Element *card = cardOf(bench);
	REQUIRE(card != nullptr);
	REQUIRE(card->elementData().plcMasterData().ios.count() == 16);
	REQUIRE(filledChannels(card) == 0);

	bench->setIoList(pointList(16));
	REQUIRE(bench->ioList().count() == 16);
	REQUIRE(assignedPoints(bench->ioList()) == 0);

	IoAssignDialog dialog(bench.project());
	QTableWidget *points = tableHeaded(dialog, QStringLiteral("Point"));
	QTableWidget *channels = tableHeaded(dialog, QStringLiteral("Voie"));
	QPushButton *assign = buttonNamed(dialog, QStringLiteral("Affecter"));
	REQUIRE(points != nullptr);
	REQUIRE(channels != nullptr);
	REQUIRE(assign != nullptr);

	SECTION("a janela abre com os dezesseis livres de um lado e as dezesseis voies do outro")
	{
		CHECK(points->rowCount() == 16);
		CHECK(channels->rowCount() == 16);

			//Each channel answers to the address the card gives it, which is
			//the name every message below uses for it.
		REQUIRE(channels->item(0, 0) != nullptr);
		CHECK(channels->item(0, 0)->text() == QLatin1String("%I0.0"));
		REQUIRE(channels->item(15, 0) != nullptr);
		CHECK(channels->item(15, 0)->text() == QLatin1String("%I0.15"));

			//Nothing ticked yet, so there is nothing to write and the button
			//says so by being closed.
		CHECK_FALSE(assign->isEnabled());
	}

	SECTION("o resumo anuncia as voies antes de o botão ser tocado")
	{
		tickAll(points);

		const QString summary = summaryOf(dialog,
						  QString::fromUtf8("affecté(s) aux voies"));
		INFO(summary.toStdString());
		CHECK(summary.contains(QString::fromUtf8("16 point(s) d'E/S "
							 "affecté(s) aux voies")));

			//The channel a point is going to take is named by the address the
			//card gives it, and the channels are handed out in order from the
			//first free one. Written out rather than searched for one at a
			//time: the order is half of what the paragraph promises.
		CHECK(summary.contains(QString::fromUtf8("aux voies : ")
				       + QLatin1String("%I0.0, %I0.1, %I0.2, %I0.3, "
						       "%I0.4, %I0.5, %I0.6, %I0.7")));

			//Announced, and still not written.
		CHECK(filledChannels(card) == 0);
		CHECK(assignedPoints(bench->ioList()) == 0);
		CHECK(assign->isEnabled());
	}

	SECTION("o botão escreve os dezesseis no cartão, e a folha passa a desenhá-los")
	{
		const int ink_before = UiBench::Rendering(bench.diagram(0)).ink();
		REQUIRE(ink_before > 0);

		tickAll(points);
		assign->click();

		CHECK(filledChannels(card) == 16);
		CHECK(functionTextsOf(card).first() == QStringLiteral("Sensor 01"));
		CHECK(functionTextsOf(card).last() == QStringLiteral("Sensor 16"));

			//The points know which channel they took, which is the half a
			//card left on its own would not have.
		CHECK(assignedPoints(bench->ioList()) == 16);
		CHECK(bench->ioList().at(0).master_uuid == card_uuid);
		CHECK(bench->ioList().at(0).io_index == 0);
		CHECK(bench->ioList().at(15).io_index == 15);
		CHECK(bench->ioList().unassigned().isEmpty());

			//And the folio draws them: sixteen descriptions are sixteen more
			//lines of ink than the empty table had.
		const int ink_after = UiBench::Rendering(bench.diagram(0)).ink();
		INFO("ink before " << ink_before << ", after " << ink_after);
		CHECK(ink_after > ink_before);
	}

	SECTION("um Ctrl+Z devolve o cartão e a lista de uma vez, e a folha volta ao que era")
	{
		const int ink_before = UiBench::Rendering(bench.diagram(0)).ink();

		tickAll(points);
		assign->click();
		REQUIRE(filledChannels(card) == 16);
		REQUIRE(assignedPoints(bench->ioList()) == 16);

			//One entry of the stack, and it says how many points moved.
		CHECK(UiBench::undoTopText(bench.project())
		      == QString::fromUtf8("affecter 16 point(s) d'E/S"));
		REQUIRE(bench->undoStack()->count() == 1);

		bench->undoStack()->undo();

			//Half undone would be a point saying it is in a card that knows
			//nothing about it. Both halves are checked, and so is the drawing.
		CHECK(filledChannels(card) == 0);
		CHECK(assignedPoints(bench->ioList()) == 0);
		CHECK(bench->ioList().count() == 16);

		const int ink_undone = UiBench::Rendering(bench.diagram(0)).ink();
		INFO("ink before " << ink_before << ", after undo " << ink_undone);
		CHECK(ink_undone == ink_before);
	}
}

TEST_CASE("T11 — o décimo sétimo ponto não cabe no cartão de dezesseis, e é dito pelo nome",
	  "[uibench][plc]")
{
	/*
		The other side of the boundary of the case above. Without it, "sixteen
		points went in" would also pass on a window that writes every point it
		is given wherever it can, which is the failure that costs a redrawn
		folio rather than an error message.
	*/
	UiBench::ScratchProject bench(fixtureXml(16));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Element *card = cardOf(bench);
	REQUIRE(card != nullptr);

	bench->setIoList(pointList(17));
	REQUIRE(bench->ioList().count() == 17);

	IoAssignDialog dialog(bench.project());
	QTableWidget *points = tableHeaded(dialog, QStringLiteral("Point"));
	QPushButton *assign = buttonNamed(dialog, QStringLiteral("Affecter"));
	REQUIRE(points != nullptr);
	REQUIRE(assign != nullptr);
	REQUIRE(points->rowCount() == 17);

	tickAll(points);

	SECTION("o resumo diz de saída que um fica de fora, e por quê")
	{
		const QString summary =
			summaryOf(dialog, QString::fromUtf8("affecté(s) aux voies"));
		INFO(summary.toStdString());
		CHECK(summary.contains(QString::fromUtf8("16 point(s) d'E/S "
							 "affecté(s) aux voies")));
		CHECK(summary.contains(QString::fromUtf8("1 point(s) laissé(s) de côté")));

			//By its name, and not by its number in a list nobody can see:
			//the seventeenth point is S17.
		CHECK(summary.contains(QStringLiteral("S17")));
	}

	SECTION("o cartão fica com dezesseis, e o que sobrou continua livre")
	{
		assign->click();

		CHECK(filledChannels(card) == 16);
		CHECK(assignedPoints(bench->ioList()) == 16);

			//The one that stayed out is the last one, and it is still there
			//to be put in another card.
		REQUIRE(bench->ioList().unassigned().count() == 1);
		CHECK(bench->ioList().at(bench->ioList().unassigned().first()).tag
		      == QStringLiteral("S17"));

			//And the caption counts what moved, not what was ticked.
		CHECK(UiBench::undoTopText(bench.project())
		      == QString::fromUtf8("affecter 16 point(s) d'E/S"));
	}
}

TEST_CASE("T11 — corrigir a descrição na lista muda o que a folha desenha, sem reabrir o desenho",
	  "[uibench][plc]")
{
	UiBench::ScratchProject bench(fixtureXml(16));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Element *card = cardOf(bench);
	REQUIRE(card != nullptr);
	bench->setIoList(pointList(16));

		//The point has to be in a channel before its description can be said
		//to reach the folio: this is the state a project is in from the
		//assignment onwards, and the one the case is about.
	{
		IoAssignDialog assignment(bench.project());
		QTableWidget *points = tableHeaded(assignment, QStringLiteral("Point"));
		QPushButton *assign = buttonNamed(assignment, QStringLiteral("Affecter"));
		REQUIRE(points != nullptr);
		REQUIRE(assign != nullptr);
		tickAll(points);
		assign->click();
	}
	REQUIRE(filledChannels(card) == 16);
	REQUIRE(functionTextsOf(card).at(2) == QStringLiteral("Sensor 03"));

	IoListDialog list_window(bench.project());
	QTreeWidget *tree = list_window.findChild<QTreeWidget *>();
	REQUIRE(tree != nullptr);

	const int description_column = columnHeaded(tree, QStringLiteral("Description"));
	REQUIRE(description_column == 4);

	QTreeWidgetItem *row = rowNamed(tree, QStringLiteral("S03"));
	REQUIRE(row != nullptr);
	REQUIRE(row->text(description_column) == QStringLiteral("Sensor 03"));

	SECTION("o texto digitado na lista chega ao cartão desenhado")
	{
		const int ink_before = UiBench::Rendering(bench.diagram(0)).ink();

		row->setText(description_column, QStringLiteral("Level switch, tank 2"));

			//The list half.
		CHECK(bench->ioList().at(2).description
		      == QStringLiteral("Level switch, tank 2"));

			//The card half, which is what the folio shows. Editing one without
			//the other leaves the drawing saying the old thing.
		CHECK(functionTextsOf(card).at(2) == QStringLiteral("Level switch, tank 2"));

			//And only that one: the neighbours are untouched.
		CHECK(functionTextsOf(card).at(1) == QStringLiteral("Sensor 02"));
		CHECK(functionTextsOf(card).at(3) == QStringLiteral("Sensor 04"));

		const int ink_after = UiBench::Rendering(bench.diagram(0)).ink();
		INFO("ink before " << ink_before << ", after " << ink_after);
		CHECK(ink_after != ink_before);
	}

	SECTION("um Ctrl+Z devolve as duas metades, e diz de qual ponto fala")
	{
		row->setText(description_column, QStringLiteral("Level switch, tank 2"));

		CHECK(UiBench::undoTopText(bench.project())
		      == QString::fromUtf8("modifier le point d'E/S S03"));

		bench->undoStack()->undo();

		CHECK(bench->ioList().at(2).description == QStringLiteral("Sensor 03"));
		CHECK(functionTextsOf(card).at(2) == QStringLiteral("Sensor 03"));
	}
}


