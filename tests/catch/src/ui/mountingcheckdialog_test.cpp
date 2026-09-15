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

#include "../../../../sources/catalog/catalog.h"
#include "../../../../sources/location/enclosuretransfer.h"
#include "../../../../sources/location/mountingcheck.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/location/ui/mountingcheckdialog.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QPointF>
#include <QSignalSpy>
#include <QSizeF>
#include <QString>
#include <QStringList>

/*
	The panel that says what is wrong with a plate, over a project.

	The arithmetic is proved a floor below, in src/mountingcheck_test: what
	overlaps what, how far the shorter way out is, how much air is missing,
	and which of the four answers a part gets. None of it is repeated here,
	and nothing here re-derives a number.

	What is proved here is the wire a person actually uses: that the panel
	lists as many complaints as the rule found, that a complaint the rule
	holds and the panel forgot to draw is caught, that the four answers of
	MountingFit come out as four different sentences, and that a clean
	plate is never shown as clean without the sentence saying what the
	answer rests on.

	The two numbers are asserted together and never one of them: how many
	rows there are catches a complaint drawn twice, and how many the rule
	found catches one that was never drawn at all. Either failure leaves
	the other number exactly as it was.

	The catalogue is handed in as an empty in-memory one rather than being
	fetched from the application. Reaching for the program's catalogue
	would open - and, on a machine that never ran the program, create - the
	real file of whoever runs the suite, and a test that writes into the
	environment is a test that passes for a reason nobody can see.

	What is deliberately not here is the panel as a person sees it.
	Whether a starved drive stands out from a duct that overlaps a rail,
	whether the second sentence is read at all - those are a person's
	answers and stay in the manual script.
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

	/// An empty catalogue that lives and dies with the case.
	class OpenCatalog
	{
		public:
			OpenCatalog()
			{
				QString error;
				const bool opened = catalog.openInMemory(&error);
				REQUIRE(error.isEmpty());
				REQUIRE(opened);
			}

			Catalog catalog;
	};

	/**
		@brief part
		@param uuid its identity
		@param label what the sheet shows
		@param position top left corner, millimetre
		@param size the room it takes, millimetre
		@return the item
	*/
	MountedItem part(const char *uuid,
			 const char *label,
			 const QPointF &position,
			 const QSizeF &size)
	{
		MountedItem item(QString::fromLatin1(label), position, size);
		item.uuid = QString::fromLatin1(uuid);
		return item;
	}

	/// A plate of 600 by 800, deliberately not square.
	MountingSurface plate()
	{
		return MountingSurface(QStringLiteral("QCM1"),
				       QStringLiteral("plate"),
				       MountingArea(600.0, 800.0));
	}
}

TEST_CASE("T19 — o painel diz na tela o que a regra da placa já sabe calcular",
	  "[uibench][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("plate-check.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	OpenCatalog bench;

	SECTION("uma placa limpa não é dada por limpa sem dizer no que ela se "
		"apoia")
	{
		MountingSurface face = plate();
		face.items << part("q1", "-Q1", QPointF(10.0, 10.0),
				   QSizeF(22.5, 85.0));
		face.items << part("q2", "-Q2", QPointF(40.0, 10.0),
				   QSizeF(22.5, 85.0));

		MountingLayout layout;
		const QString uuid = layout.appendSurface(face);
		scratch->setMountingLayout(layout);

		MountingCheckDialog dialog(scratch.project(), &bench.catalog);
		REQUIRE(dialog.shownSurface() == uuid);

		CHECK(dialog.report().isClean());
		CHECK(dialog.issueRowCount() == 0);
		CHECK(dialog.report().issueCount() == 0);
		CHECK_FALSE(dialog.summaryText().isEmpty());

			//Not modal: whoever drives this window on screen has to
			//know, because the modal dialogue detector of the
			//interface driver does not see it at all.
		CHECK_FALSE(dialog.isModal());

		dialog.close();
	}

	SECTION("duas peças no mesmo lugar viram uma linha, e a conta fecha nas "
		"duas pontas")
	{
		MountingSurface face = plate();
		face.items << part("q1", "-Q1", QPointF(10.0, 10.0),
				   QSizeF(50.0, 85.0));
		face.items << part("q2", "-Q2", QPointF(30.0, 10.0),
				   QSizeF(50.0, 85.0));

		MountingLayout layout;
		layout.appendSurface(face);
		scratch->setMountingLayout(layout);

		MountingCheckDialog dialog(scratch.project(), &bench.catalog);

		CHECK(dialog.report().overlaps.count() == 1);

			//The coarse assertion and the fine one, together. One
			//row drawn twice leaves the rule's count where it was;
			//one the rule found and nobody drew leaves the widget's
			//count where it was.
		CHECK(dialog.issueRowCount() == 1);
		CHECK(dialog.issueRowCount() == dialog.report().issueCount());

		CHECK(dialog.complainedAbout().count() == 2);
		CHECK(dialog.complainedAbout().contains(QString("q1")));
		CHECK(dialog.complainedAbout().contains(QString("q2")));

		dialog.close();
	}

	SECTION("arrastar para fora da chapa e não caber em chapa nenhuma são "
		"duas frases diferentes")
	{
			//The whole reason this step exists apart from the rule:
			//one of the two is dragged back in a second, and the
			//other is another enclosure or another product.
		CHECK(MountingCheckDialog::fitMessage(MountingFit::OutsideArea)
		      != MountingCheckDialog::fitMessage(
				      MountingFit::LargerThanArea));

		QStringList said;
		said << MountingCheckDialog::fitMessage(MountingFit::Fits)
		     << MountingCheckDialog::fitMessage(MountingFit::OutsideArea)
		     << MountingCheckDialog::fitMessage(MountingFit::LargerThanArea)
		     << MountingCheckDialog::fitMessage(MountingFit::NoArea);

		CHECK(said.count() == 4);
		for (const QString &sentence : said) {
			CHECK_FALSE(sentence.isEmpty());
		}

			//Four values, four sentences: removing the duplicates
			//must not shorten the list.
		said.removeDuplicates();
		CHECK(said.count() == 4);
	}

	SECTION("a peça fora da chapa e a peça maior que a chapa aparecem, cada "
		"uma com a sua resposta")
	{
		MountingSurface face = plate();
			//Off the plate where it stands: it would fit elsewhere.
		face.items << part("q1", "-Q1", QPointF(700.0, 10.0),
				   QSizeF(22.5, 85.0));
			//Bigger than the whole plate: no position holds it.
		face.items << part("q2", "-Q2", QPointF(0.0, 0.0),
				   QSizeF(900.0, 85.0));

		MountingLayout layout;
		layout.appendSurface(face);
		scratch->setMountingLayout(layout);

		MountingCheckDialog dialog(scratch.project(), &bench.catalog);

		const MountingSurfaceReport answer = dialog.report();
		CHECK(answer.itemCount() == 2);
		CHECK(answer.misfitCount() == 2);
		CHECK(dialog.issueRowCount() == answer.issueCount());

		const QList<MountingItemFit> misfits = answer.misfits();
		REQUIRE(misfits.count() == 2);
		CHECK(misfits.at(0).fit == MountingFit::OutsideArea);
		CHECK(misfits.at(1).fit == MountingFit::LargerThanArea);

		dialog.close();
	}

	SECTION("chapa sem medida responde que não sabe, e não que está limpa")
	{
		MountingSurface face(QStringLiteral("QCM1"),
				     QStringLiteral("door"),
				     MountingArea());
		face.items << part("q1", "-Q1", QPointF(10.0, 10.0),
				   QSizeF(22.5, 85.0));

		MountingLayout layout;
		layout.appendSurface(face);
		scratch->setMountingLayout(layout);

		MountingCheckDialog dialog(scratch.project(), &bench.catalog);

		CHECK_FALSE(dialog.report().isConclusive());
		CHECK_FALSE(dialog.summaryText().isEmpty());

		dialog.close();
	}

	SECTION("apontar uma linha diz de que peça ela é")
	{
		MountingSurface face = plate();
		face.items << part("q1", "-Q1", QPointF(10.0, 10.0),
				   QSizeF(50.0, 85.0));
		face.items << part("q2", "-Q2", QPointF(30.0, 10.0),
				   QSizeF(50.0, 85.0));

		MountingLayout layout;
		layout.appendSurface(face);
		scratch->setMountingLayout(layout);

		MountingCheckDialog dialog(scratch.project(), &bench.catalog);
		REQUIRE(dialog.issueRowCount() == 1);

		QSignalSpy spy(&dialog, &MountingCheckDialog::goToItem);
		REQUIRE(spy.isValid());

		CHECK(dialog.pointAtRow(0));
		REQUIRE(spy.count() >= 1);
		CHECK(spy.takeFirst().at(0).toString() == QString("q1"));

			//There is no second complaint to point at.
		CHECK_FALSE(dialog.pointAtRow(1));

		dialog.close();
	}

	SECTION("mover a peça no projeto tira a reclamação da lista")
	{
		MountingSurface face = plate();
		face.items << part("q1", "-Q1", QPointF(10.0, 10.0),
				   QSizeF(50.0, 85.0));
		face.items << part("q2", "-Q2", QPointF(30.0, 10.0),
				   QSizeF(50.0, 85.0));

		MountingLayout layout;
		const QString uuid = layout.appendSurface(face);
		scratch->setMountingLayout(layout);

		MountingCheckDialog dialog(scratch.project(), &bench.catalog);
		REQUIRE(dialog.issueRowCount() == 1);

			//The panel is meant to stay open while parts are
			//dragged: a person who fixes an overlap and is still
			//told about it stops reading the panel.
		MountingLayout moved = scratch->mountingLayout();
		MountedItem dragged = moved.item(QStringLiteral("q2"));
		REQUIRE_FALSE(dragged.isNull());
		dragged.position = QPointF(300.0, 10.0);
		REQUIRE(moved.updateItem(dragged));
		scratch->setMountingLayout(moved);

		CHECK(dialog.shownSurface() == uuid);
		CHECK(dialog.issueRowCount() == 0);
		CHECK(dialog.report().isClean());

		dialog.close();
	}
}
