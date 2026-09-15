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

#include "../../../../sources/location/drillingorigin.h"
#include "../../../../sources/location/drillingtable.h"
#include "../../../../sources/location/layout/mountinglayouteditor.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/location/ui/drillingtabledialog.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QLocale>
#include <QPointF>
#include <QSignalSpy>
#include <QSizeF>
#include <QString>
#include <QStringList>

/*
	The drilling table as a window, over a project.

	The arithmetic of it is proved a floor below, in src/drillingtable_test
	and src/drillingorigin_test: what a coordinate is in which frame, what
	a row of cells says, and how the corner of the plate changes what is
	printed without changing what is stored. None of that is repeated here.

	What is proved here is the half that needs a project open: that the
	window lists the face the project holds, that the columns it shows are
	the columns the file is written with, that the sentence at its head is
	the sentence at the head of the file, and that the filter narrows what
	is looked at without narrowing what is handed over. That last one is
	the case worth having: a drilling list exported short produces a plate
	drilled short, and nothing in the document that lacks a hole says it
	was ever there.

	Two assertions are made about every group, never one. How many groups
	there are catches a hole that became a line of its own; how many holes
	they add up to catches a hole folded into a group it does not belong
	to - and that second failure leaves the number of groups exactly as it
	was.

	What is deliberately not here is the window as a person sees it.
	Whether the frame sentence is legible over the table, whether the
	filter is where a hand looks for it, whether a row struck red reads as
	a warning - those are a person's answers and stay in the manual script.
*/

namespace
{
	/// The smallest project that opens: one sheet, nothing on it.
	QString projectXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"Sheet\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	/**
		@brief plate
		@param layout where to put it
		@param origin the corner the shop measures from
		@return the identifier of the face

		600 by 800 and deliberately not square: swapping the two
		dimensions is the one wrong answer that comes out positive and
		of the right order of magnitude, and a square plate would not
		denounce it.
	*/
	QString plate(MountingLayout *layout,
		      const DrillingOrigin &origin = DrillingOrigin())
	{
		MountingSurface face(QStringLiteral("QCM1"),
				     QStringLiteral("plate"),
				     MountingArea(600.0, 800.0));
		face.name = QStringLiteral("Main plate");
		face.drilling_origin = origin;

		MountedItem breaker(QStringLiteral("-Q1"),
				    QPointF(10.0, 20.0),
				    QSizeF(22.5, 85.0));
		breaker.uuid = QStringLiteral("q1");
		face.items << breaker;

		return layout->appendSurface(face);
	}

	/**
		@brief eightHoles
		@return eight holes over five tools

		Five groups and eight holes, and the two numbers are different
		on purpose: an assertion that only counted groups would pass on
		a list that lost a hole into a neighbouring group.
	*/
	QList<DrillingHole> eightHoles()
	{
		QList<DrillingHole> holes;

		DrillingHole lamp = DrillingHole::drilled(QPointF(40.0, 60.0),
							  22.0,
							  QStringLiteral("pilot light ; left side"));
		lamp.component = QStringLiteral("-H1");
		lamp.component_uuid = QStringLiteral("q1");
		holes << lamp;

		holes << DrillingHole::drilled(QPointF(80.0, 60.0), 22.0);
		holes << DrillingHole::drilled(QPointF(120.0, 60.0), 22.0);
		holes << DrillingHole::drilled(QPointF(160.0, 60.0), 16.0);
		holes << DrillingHole::drilled(QPointF(200.0, 60.0), 16.0);
		holes << DrillingHole::cutOut(QPointF(300.0, 200.0),
					      QSizeF(92.0, 92.0));
		holes << DrillingHole::cutOut(QPointF(300.0, 400.0),
					      QSizeF(45.0, 45.0));

			//The one nobody measured: it stays in the list, in a
			//group of its own, rather than being dropped.
		DrillingHole unmeasured;
		unmeasured.position = QPointF(500.0, 700.0);
		unmeasured.shape = DrillingShape::Round;
		holes << unmeasured;

		return holes;
	}
}

TEST_CASE("T22 — a tabela de furos aparece na tela com o quadro de referência",
	  "[uibench][furacao]")
{
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("drilling-table.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	MountingLayout layout;
	const QString face = plate(&layout);
	REQUIRE_FALSE(face.isEmpty());
	scratch->setMountingLayout(layout);

	SECTION("a janela abre sobre a face que o projeto guarda, e não é modal")
	{
		DrillingTableDialog dialog(scratch.project());

		CHECK(dialog.project() == scratch.project());
		CHECK(dialog.shownSurface() == face);

			//Not modal, and the assertion is here because whoever
			//drives this window on screen needs to know: the modal
			//dialogue detector of the interface driver does not see
			//it, and waits for something that never comes.
		CHECK_FALSE(dialog.isModal());

		dialog.close();
	}

	SECTION("as colunas da janela são as colunas do arquivo")
	{
		DrillingTableDialog dialog(scratch.project());

			//One spelling of a column, in one place. Two of them
			//read as two columns to anybody holding the window and
			//the exported file side by side.
		CHECK(dialog.columnNames() == DrillingTable::header());

		dialog.close();
	}

	SECTION("o cabeçalho diz de que canto a oficina mede")
	{
		DrillingTableDialog dialog(scratch.project());

		const MountingSurface shown = scratch->mountingLayout()
					      .surface(face);
		CHECK(dialog.referenceFrameText()
		      == DrillingTable::referenceFrameText(
				      shown.drillingFrame()));
		CHECK(dialog.referenceFrameText()
		      .contains(DrillingOrigin::cornerName(
					DrillingCorner::TopLeft)));

		dialog.close();
	}

	SECTION("trocar o canto da chapa troca a frase e a coordenada impressa")
	{
		MountingLayout other;
		const QString other_face =
			plate(&other,
			      DrillingOrigin(DrillingCorner::BottomLeft));
		scratch->setMountingLayout(other);

		DrillingTableDialog dialog(scratch.project());
		REQUIRE(dialog.shownSurface() == other_face);
		dialog.setHoles(eightHoles());

		CHECK(dialog.referenceFrameText()
		      .contains(DrillingOrigin::cornerName(
					DrillingCorner::BottomLeft)));

			//The y of the first hole read from the bottom of an
			//800 mm plate is 800 - 60. Built by the frame and not
			//typed, so that the window and the rule cannot disagree
			//about the sign.
		const MountingSurface shown = scratch->mountingLayout()
					      .surface(other_face);
		const QStringList expected =
			DrillingTable::row(eightHoles().first(), QLocale(),
					   shown.drillingFrame());
		CHECK(dialog.asText(QStringLiteral(";")).contains(expected.at(2)));

		dialog.close();
	}

	SECTION("agrupa por ferramenta, e a soma bate com o número de furos")
	{
		DrillingTableDialog dialog(scratch.project());
		dialog.setHoles(eightHoles());

		CHECK(dialog.holeCount() == 8);

		const QList<DrillingToolTotal> totals = dialog.toolTotals();

			//The coarse assertion: five tools, because there are
			//five different things to fit in the chuck - 22, 16,
			//92x92, 45x45, and the group of what nobody measured.
		CHECK(totals.count() == 5);

			//The fine one, and it is the one that matters: a hole
			//folded into the wrong group leaves the count of groups
			//exactly as it was.
		int summed = 0;
		for (const DrillingToolTotal &total : totals) {
			summed += total.count;
		}
		CHECK(summed == 8);
		CHECK(summed == dialog.holeCount());

		dialog.close();
	}

	SECTION("o filtro estreita o que se olha, e nunca o que se entrega")
	{
		DrillingTableDialog dialog(scratch.project());
		dialog.setHoles(eightHoles());

		REQUIRE(dialog.visibleRowCount() == 8);

			//The file keeps its frame line and its header line, so
			//eight holes make ten lines.
		const QStringList before =
			dialog.asText(QStringLiteral(";"))
			.split(QLatin1Char('\n'));
		CHECK(before.count() == 10);

			//Two round holes of 16 mm, and nothing else on this
			//plate says 16.
		dialog.setFilter(QStringLiteral("16"));
		CHECK(dialog.visibleRowCount() == 2);
		CHECK(dialog.holeCount() == 8);

			//Whatever the filter is showing, the export is the
			//plate: a list handed over short drills a plate short.
		const QStringList after =
			dialog.asText(QStringLiteral(";"))
			.split(QLatin1Char('\n'));
		CHECK(after == before);

		dialog.setFilter(QString());
		CHECK(dialog.visibleRowCount() == 8);

		dialog.close();
	}

	SECTION("furo fora da chapa é contado, e chapa sem medida não é acusada")
	{
		DrillingTableDialog dialog(scratch.project());

		QList<DrillingHole> holes = eightHoles();
		holes << DrillingHole::drilled(QPointF(5000.0, 5000.0), 22.0);
		dialog.setHoles(holes);

		CHECK(dialog.holeCount() == 9);
		CHECK(dialog.offSurfaceCount() == 1);

			//An unmeasured plate does not accuse anybody: a hole
			//there is unjudged, not misplaced, and the two must
			//never be reported as one number.
		MountingLayout blind;
		MountingSurface blind_face(QStringLiteral("QCM2"),
					   QStringLiteral("door"),
					   MountingArea());
		const QString blind_uuid = blind.appendSurface(blind_face);
		scratch->setMountingLayout(blind);

		DrillingTableDialog second(scratch.project());
		REQUIRE(second.shownSurface() == blind_uuid);
		second.setHoles(holes);

		CHECK(second.holeCount() == 9);
		CHECK(second.offSurfaceCount() == 0);

		second.close();
		dialog.close();
	}

	SECTION("os furos ficam com a face a que pertencem")
	{
		MountingLayout two;
		const QString first = plate(&two);
		MountingSurface door(QStringLiteral("QCM1"),
				     QStringLiteral("door"),
				     MountingArea(500.0, 700.0));
		const QString second = two.appendSurface(door);
		scratch->setMountingLayout(two);

		DrillingTableDialog dialog(scratch.project());
		REQUIRE(dialog.shownSurface() == first);

		dialog.setHoles(first, eightHoles());
		CHECK(dialog.holeCount() == 8);

		REQUIRE(dialog.showSurface(second));
		CHECK(dialog.holeCount() == 0);

			//And back again: looking at the door must not lose what
			//was handed over for the plate.
		REQUIRE(dialog.showSurface(first));
		CHECK(dialog.holeCount() == 8);

		dialog.close();
	}

	SECTION("apontar uma linha diz de que componente ela é")
	{
		DrillingTableDialog dialog(scratch.project());
		dialog.setHoles(eightHoles());

		QSignalSpy spy(&dialog, &DrillingTableDialog::goToComponent);
		REQUIRE(spy.isValid());

		CHECK(dialog.pointAtRow(0));
		REQUIRE(spy.count() == 1);
		CHECK(spy.takeFirst().at(0).toString() == QString("q1"));

			//A hole that belongs to nobody is a legitimate hole - a
			//gland, a fixing hole for a duct cut on the bench - and
			//says so instead of pointing at nothing.
		CHECK_FALSE(dialog.pointAtRow(1));
		CHECK(spy.count() == 0);

		dialog.close();
	}
}

TEST_CASE("T22 — a tabela de furos é alcançável a partir do editor de "
	  "calepinagem",
	  "[uibench][furacao]")
{
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("drilling-from-editor.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	MountingLayout layout;
	const QString face = plate(&layout);
	scratch->setMountingLayout(layout);

	MountingLayoutEditor editor(scratch.project());
	REQUIRE(editor.shownSurface() == face);

	SECTION("a janela abre sobre a face que o editor mostra")
	{
		DrillingTableDialog *dialog = editor.openDrillingTable();
		REQUIRE(dialog != nullptr);

		CHECK(dialog->shownSurface() == face);
		CHECK(dialog->project() == scratch.project());
		CHECK_FALSE(dialog->isModal());
	}

	SECTION("pedir duas vezes levanta a mesma janela, e não empilha outra")
	{
			//Two copies of the same worklist are two documents that
			//disagree the moment a part moves.
		DrillingTableDialog *first = editor.openDrillingTable();
		DrillingTableDialog *second = editor.openDrillingTable();
		CHECK(first == second);
	}

	editor.close();
}
