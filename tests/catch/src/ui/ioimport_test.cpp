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
#include "../../../../sources/plc/iolist.h"
#include "../../../../sources/plc/iopoint.h"
#include "../../../../sources/plc/ui/ioimportdialog.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QUndoStack>

/*
	Bringing a sheet of I/O points into a project, through the window a person
	uses to do it.

	The reading of a grid is already proved without a project open, in
	iosheet_test.cpp: which column feeds which field, how a blank row is
	counted, how a type written in three languages is understood. The merge is
	proved in io_test.cpp: the key cascade, what an import may never blank, what
	it reports instead of deleting. None of that is repeated here.

	What is proved here is the half that only exists once a window and a project
	are open, and it is the half the queued cases are about: that the paragraph
	shown under the tables is the merge itself and not a promise - it is read
	before the button is pressed, and what it announces is what the button then
	does -, that the points reach the project, that nothing at all is drawn on
	the folio by an import, and that one Ctrl+Z takes the whole list back.

	Nothing here is modal. IoImportDialog is built, filled and clicked inside
	this process, with the offscreen platform: the clipboard is written from
	the test, and the paste button reads it from there exactly as it would read
	a spreadsheet.

	What stays out: opening a file. That path goes through QFileDialog, which
	is a modal window of the operating system, and an offscreen suite must not
	enter one. The paste path and the file path meet at setGrid(), one line
	apart, so what is proved of the first is true of the second everywhere but
	in the file chooser itself.
*/

namespace
{
	/// A sheet as a spreadsheet copies it: cells by tab, rows by newline.
	QString sheetText(const QList<QStringList> &rows)
	{
		QStringList lines;
		lines.reserve(rows.count());
		for (const QStringList &row : rows) {
			lines << row.join(QLatin1Char('\t'));
		}
		return lines.join(QLatin1Char('\n'));
	}

	/**
		The two column sheet of CU-11.1: a type and a description, and not one
		line of configuration anywhere.
		@param count how many points it describes
		@param blank_at the row number, counted the way the spreadsheet counts
		them, of a row left entirely empty - 0 for a sheet with no hole in it
	*/
	QString twoColumnSheet(int count, int blank_at = 0)
	{
		QList<QStringList> rows;
		int written = 0;
		int number = 1;

		while (written < count)
		{
			if (number == blank_at)
			{
					//Two empty cells and not an empty line: that is what a
					//spreadsheet puts on the clipboard for a row somebody
					//left blank in the middle of the range they copied.
				rows << (QStringList() << QString() << QString());
				++ number;
				continue;
			}

			rows << (QStringList()
				 << QStringLiteral("DI")
				 << QStringLiteral("Sensor %1").arg(written + 1, 2, 10,
								    QLatin1Char('0')));
			++ written;
			++ number;
		}

		return sheetText(rows);
	}

	/**
		The fuller sheet the automation department sends the second time: it
		names every point, which is what lets a description be corrected
		instead of read as a point nobody had heard of.
		@param count how many of the original points it still carries
		@param changed how many of them come back with another description
		@param added how many points it brings that the project has never seen
	*/
	QString taggedSheet(int count, int changed = 0, int added = 0)
	{
		QList<QStringList> rows;
		rows << (QStringList() << QStringLiteral("Tag")
				       << QStringLiteral("Tipo")
				       << QStringLiteral("Descricao"));

		for (int index = 0 ; index < count ; ++ index)
		{
			const QString description =
				index < changed
				? QStringLiteral("Sensor %1, revised").arg(index + 1, 2, 10,
									  QLatin1Char('0'))
				: QStringLiteral("Sensor %1").arg(index + 1, 2, 10,
								  QLatin1Char('0'));

			rows << (QStringList()
				 << QStringLiteral("S%1").arg(index + 1, 2, 10, QLatin1Char('0'))
				 << QStringLiteral("DI")
				 << description);
		}

		for (int index = 0 ; index < added ; ++ index)
		{
			rows << (QStringList()
				 << QStringLiteral("N%1").arg(index + 1, 2, 10, QLatin1Char('0'))
				 << QStringLiteral("DI")
				 << QStringLiteral("New point %1").arg(index + 1));
		}

		return sheetText(rows);
	}

	/// One drawn component, so that "nothing was drawn" is a count and not a hope.
	QString fixtureXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">"
			       "<element name=\"contactor.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Contactor</name></names>"
			       "<description>"
			       "<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
			       " end1=\"none\" end2=\"none\" length1=\"1.5\" length2=\"1.5\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"
			       "<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>"
			       "<element x=\"100\" y=\"100\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/contactor.elmt\""
			       " uuid=\"{cafe0000-0000-4000-8000-000000000001}\">"
			       "<terminals/><inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">K1"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>"
			       "</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
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
		The paragraph under the two tables, found by what it says rather than
		by its place in the layout: the window holds three labels that all
		speak, and picking one by index would silently follow the layout the
		day somebody adds a fourth.
	*/
	QString summaryOf(QWidget &parent)
	{
		const QList<QLabel *> labels = parent.findChildren<QLabel *>();
		for (QLabel *label : labels)
		{
			if (label->text().contains(QStringLiteral("d'E/S lu"))) {
				return label->text();
			}
		}
		return QString();
	}

	/// Paste @a text the way a person does: through the clipboard and the button.
	bool paste(IoImportDialog &dialog, const QString &text)
	{
		QApplication::clipboard()->setText(text);
		if (QApplication::clipboard()->text() != text) {
			return false;
		}

		QPushButton *button = buttonNamed(dialog,
						  QString::fromUtf8("Coller depuis le "
								    "presse-papiers"));
		if (!button) {
			return false;
		}
		button->click();
		return true;
	}

	/// The description each point of @a list carries, in the order of the list.
	QStringList descriptionsOf(const IoList &list)
	{
		QStringList descriptions;
		for (int index = 0 ; index < list.count() ; ++ index) {
			descriptions << list.at(index).description;
		}
		return descriptions;
	}

	int elementCount(UiBench::ScratchProject &project)
	{
		int count = 0;
		const QList<Diagram *> sheets = project.diagrams();
		for (Diagram *sheet : sheets) {
			count += sheet->elements().count();
		}
		return count;
	}
}

TEST_CASE("T11 — a planilha de duas colunas entra inteira pelo diálogo, e não desenha nada",
	  "[uibench][plc]")
{
	UiBench::ScratchProject bench(fixtureXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 1);
	REQUIRE(elementCount(bench) == 1);
	REQUIRE(bench->ioList().count() == 0);

	IoImportDialog dialog(bench.project());
	REQUIRE(paste(dialog, twoColumnSheet(60)));

	QPushButton *import = buttonNamed(dialog, QStringLiteral("Importer"));
	REQUIRE(import != nullptr);

	SECTION("o resumo conta os sessenta antes de o botão ser tocado")
	{
			//The whole reason the window exists. What it announces here is
			//measured against what the button does, in the section below.
		const QString summary = summaryOf(dialog);
		INFO(summary.toStdString());
		CHECK(summary.contains(QStringLiteral("60 point(s) d'E/S lu(s).")));
		CHECK(summary.contains(QString::fromUtf8("60 point(s) ajouté(s), "
							 "0 mis à jour, "
							 "0 inchangé(s).")));

			//Announced, and still not written: the project has not been
			//touched by the reading.
		CHECK(bench->ioList().count() == 0);
		CHECK(import->isEnabled());
	}

	SECTION("importar põe os sessenta no projeto, e a folha continua com o que tinha")
	{
		const int items_before = bench.diagram(0)->items().count();

		import->click();

		REQUIRE(bench->ioList().count() == 60);
		CHECK(dialog.report().added.count() == 60);
		CHECK(dialog.report().updated.isEmpty());
		CHECK(dialog.report().ambiguous.isEmpty());

			//An imported point exists in the project before it exists on any
			//folio: that is the state the task needed and the program did
			//not have. One component before, one component after.
		CHECK(elementCount(bench) == 1);

			//Nor is a point drawn as anything else: the sheet holds exactly
			//the items it opened with, conductors and texts included.
		CHECK(bench.diagram(0)->items().count() == items_before);

		CHECK(bench->ioList().unassigned().count() == 60);
		CHECK(bench->ioList().at(0).description == QStringLiteral("Sensor 01"));
		CHECK(bench->ioList().at(59).description == QStringLiteral("Sensor 60"));
	}

	SECTION("um Ctrl+Z devolve a lista como estava, e diz o que desfaz")
	{
		import->click();
		REQUIRE(bench->ioList().count() == 60);

		CHECK(UiBench::undoTopText(bench.project())
		      == QStringLiteral("importer 60 point(s) d'E/S"));

		bench->undoStack()->undo();
		CHECK(bench->ioList().count() == 0);

			//And back again, because an import that could not be redone
			//would only be half an entry of the stack.
		bench->undoStack()->redo();
		CHECK(bench->ioList().count() == 60);
	}
}

TEST_CASE("T11 — a linha vazia no meio não trunca a leitura, e o rodapé diz qual é",
	  "[uibench][plc]")
{
	UiBench::ScratchProject bench(fixtureXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	IoImportDialog dialog(bench.project());
	QPushButton *import = buttonNamed(dialog, QStringLiteral("Importer"));
	REQUIRE(import != nullptr);

	SECTION("os sessenta entram, e a linha 25 é apontada pelo número")
	{
		REQUIRE(paste(dialog, twoColumnSheet(60, 25)));

		const QString summary = summaryOf(dialog);
		INFO(summary.toStdString());

			//Importing twenty four of sixty and reporting success is the one
			//outcome the task forbids, so both halves are checked: the count
			//that did not stop at the hole, and the number of the row.
		CHECK(summary.contains(QStringLiteral("60 point(s) d'E/S lu(s).")));
		CHECK(summary.contains(QString::fromUtf8("1 ligne(s) vide(s) "
							 "ignorée(s) : 25.")));

		import->click();
		CHECK(bench->ioList().count() == 60);
	}

	SECTION("controle negativo — sem o buraco, o rodapé não fala de linha nenhuma")
	{
			//The other side of the boundary. Without it the check above would
			//pass on a window that mentions a blank row whatever it read.
		REQUIRE(paste(dialog, twoColumnSheet(60)));

		const QString summary = summaryOf(dialog);
		INFO(summary.toStdString());
		CHECK(summary.contains(QStringLiteral("60 point(s) d'E/S lu(s).")));
		CHECK_FALSE(summary.contains(QString::fromUtf8("vide(s) ignorée(s)")));
	}
}

TEST_CASE("T11 — a planilha revisada é anunciada antes do botão, e o desfazer devolve a lista",
	  "[uibench][plc]")
{
	UiBench::ScratchProject bench(fixtureXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

		//The first import, which is what makes the second one the dangerous
		//one: by the time the revised sheet arrives, sixty points are in the
		//project and some of them are already drawn.
	{
		IoImportDialog first(bench.project());
		REQUIRE(paste(first, taggedSheet(60)));
		QPushButton *import = buttonNamed(first, QStringLiteral("Importer"));
		REQUIRE(import != nullptr);
		import->click();
	}
	REQUIRE(bench->ioList().count() == 60);
	const QStringList before = descriptionsOf(bench->ioList());
	REQUIRE(before.count() == 60);

	IoImportDialog dialog(bench.project());
	REQUIRE(paste(dialog, taggedSheet(60, 3, 5)));
	QPushButton *import = buttonNamed(dialog, QStringLiteral("Importer"));
	REQUIRE(import != nullptr);

	SECTION("o resumo antecipa cinco novos e três alterados, antes de escrever nada")
	{
		const QString summary = summaryOf(dialog);
		INFO(summary.toStdString());
		CHECK(summary.contains(QStringLiteral("65 point(s) d'E/S lu(s).")));
		CHECK(summary.contains(QString::fromUtf8("5 point(s) ajouté(s), "
							 "3 mis à jour, "
							 "57 inchangé(s).")));

			//Read, announced, and not written.
		CHECK(bench->ioList().count() == 60);
		CHECK(descriptionsOf(bench->ioList()) == before);
	}

	SECTION("o botão faz o que o resumo anunciou")
	{
		import->click();

		REQUIRE(bench->ioList().count() == 65);
		CHECK(dialog.report().added.count() == 5);
		CHECK(dialog.report().updated.count() == 3);
		CHECK(dialog.report().unchanged.count() == 57);

			//The three that changed are the three the sheet rewrote, and the
			//fourth is untouched: a merge that rewrote everything would pass
			//the counts above and fail here.
		CHECK(bench->ioList().at(0).description
		      == QStringLiteral("Sensor 01, revised"));
		CHECK(bench->ioList().at(2).description
		      == QStringLiteral("Sensor 03, revised"));
		CHECK(bench->ioList().at(3).description == QStringLiteral("Sensor 04"));

			//The order the person typed is the order that survives: the
			//revised points stayed where they were and the new ones came
			//after them.
		CHECK(bench->ioList().at(60).tag == QStringLiteral("N01"));
	}

	SECTION("um Ctrl+Z devolve os sessenta de antes, com as descrições de antes")
	{
		import->click();
		REQUIRE(bench->ioList().count() == 65);

		CHECK(UiBench::undoTopText(bench.project())
		      == QStringLiteral("importer 8 point(s) d'E/S"));

		bench->undoStack()->undo();
		CHECK(bench->ioList().count() == 60);
		CHECK(descriptionsOf(bench->ioList()) == before);
	}
}
