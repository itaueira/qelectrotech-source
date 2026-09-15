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

#include "../../../../sources/location/layout/mountedpartitem.h"
#include "../../../../sources/location/layout/mountinglayouteditor.h"
#include "../../../../sources/location/layout/mountingscene.h"
#include "../../../../sources/location/layout/mountingview.h"
#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QTransform>
#include <QUndoStack>
#include <QWidget>

/*
	The wire between the drawing and the file.

	The scene of the previous half was proved on its own, in
	src/ui/mountingscene_test.cpp: it counts in millimetre, it makes no item
	of the plate, and a drag leaves one step on its stack. Every one of
	those cases stays green with no project in sight - which is exactly the
	hole this file exists to close. A drag that never reaches
	QETProject::mountingLayout() is a drag that disappears the moment the
	project is saved and opened again, and nothing over there can see it.

	So what is proved here is the other end: that a move made on the scene
	is in the project before anything is saved, that it is in the file after
	it is, that undoing it travels the same way, and - the half that is
	easiest to break by accident - that opening the window over a project
	nobody has touched leaves it a project nobody has touched.

	What is deliberately not here is the window as a person sees it. No
	suite of this fork shows a window, and nothing below shows one either:
	the editor is built, driven through its own methods and destroyed,
	entirely inside this process. Whether the plate is legible, whether the
	grid gets in the way, whether the drag feels right - those are a
	person's answers, and they stay in the manual script.
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

	/// A breaker of a real width, mounted where a folio grid would round off.
	MountedItem breaker()
	{
		MountedItem item(QStringLiteral("-Q1"),
				 QPointF(10.5, 20.25),
				 QSizeF(22.5, 85.0));
		item.uuid = QStringLiteral("q1");
		item.part_code = QStringLiteral("A9F74210");
		return item;
	}

	/// A mounting plate of 600 by 800 with that breaker on it.
	MountingLayout plateWithBreaker(QString *face_uuid)
	{
		MountingLayout layout;
		const QString face = layout.appendSurface(
					MountingSurface(QStringLiteral("QCM1"),
							QStringLiteral("plate"),
							MountingArea(600.0, 800.0)));
		layout.mountItem(face, breaker());

		if (face_uuid) {
			*face_uuid = face;
		}
		return layout;
	}
}

TEST_CASE("T19 — o que se arrasta na janela de calepinagem chega ao projeto",
	  "[uibench][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("layout-editor.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	QString face;
	scratch->setMountingLayout(plateWithBreaker(&face));
	REQUIRE_FALSE(face.isEmpty());

	SECTION("a janela abre sobre a face que o projeto guarda")
	{
		MountingLayoutEditor editor(scratch.project());

		CHECK(editor.project() == scratch.project());
		CHECK(editor.shownSurface() == face);
		CHECK(editor.scene()->partCount() == 1);
		CHECK(editor.scene()->isAreaMeasured());

			//The millimetre of the file is the millimetre of the
			//drawing, with nothing in between - which is the whole
			//reason this editor is not a folio.
		MountedPartItem *part = editor.scene()
					->partItem(QStringLiteral("q1"));
		REQUIRE(part != nullptr);
		CHECK(part->millimetrePosition().x() == Approx(10.5));
		CHECK(part->millimetrePosition().y() == Approx(20.25));

		editor.close();
	}

	SECTION("mover chega ao projeto sem que ninguém feche nem salve nada")
	{
		MountingLayoutEditor editor(scratch.project());
		REQUIRE(editor.shownSurface() == face);

		REQUIRE(editor.scene()->moveItem(QStringLiteral("q1"),
						 QPointF(137.5, 240.25)));

			//Nothing was closed and nothing was saved: this is the
			//project as it stands in memory, which is what a backup
			//serialises and what a crash copy holds.
		const MountedItem moved = scratch->mountingLayout()
					  .item(QStringLiteral("q1"));
		CHECK(moved.position.x() == Approx(137.5));
		CHECK(moved.position.y() == Approx(240.25));

		editor.close();
	}

	SECTION("e sobrevive a salvar e reabrir, que é onde ele sumia")
	{
		{
			MountingLayoutEditor editor(scratch.project());
			REQUIRE(editor.shownSurface() == face);
			REQUIRE(editor.scene()->moveItem(QStringLiteral("q1"),
							 QPointF(137.5, 240.25)));
			editor.close();
		}

		REQUIRE(scratch.saveAndReopen());

		const MountingLayout reread = scratch->mountingLayout();
		REQUIRE(reread.itemCount() == 1);

		const MountedItem moved = reread.item(QStringLiteral("q1"));
		CHECK(moved.position.x() == Approx(137.5));
		CHECK(moved.position.y() == Approx(240.25));
		CHECK(moved.size.width() == Approx(22.5));
		CHECK(moved.part_code == QString("A9F74210"));

			//Read back through the window as well, because the two
			//ends being right one at a time is not the same as the
			//two ends being tied together.
		MountingLayoutEditor reopened(scratch.project());
		MountedPartItem *part = reopened.scene()
					->partItem(QStringLiteral("q1"));
		REQUIRE(part != nullptr);
		CHECK(part->millimetrePosition().x() == Approx(137.5));
		reopened.close();
	}

	SECTION("desfazer faz o mesmo caminho de volta")
	{
		MountingLayoutEditor editor(scratch.project());
		REQUIRE(editor.shownSurface() == face);
		REQUIRE(editor.scene()->moveItem(QStringLiteral("q1"),
						 QPointF(300.0, 400.0)));
		REQUIRE(scratch->mountingLayout()
			.item(QStringLiteral("q1")).position.x() == Approx(300.0));

		editor.scene()->undoStack().undo();

		const MountedItem back = scratch->mountingLayout()
					 .item(QStringLiteral("q1"));
		CHECK(back.position.x() == Approx(10.5));
		CHECK(back.position.y() == Approx(20.25));

		editor.scene()->undoStack().redo();
		CHECK(scratch->mountingLayout()
		      .item(QStringLiteral("q1")).position.x() == Approx(300.0));

		editor.close();
	}

	SECTION("a face que saiu do projeto não é reescrita por cima")
	{
		MountingLayoutEditor editor(scratch.project());
		REQUIRE(editor.shownSurface() == face);

			//Somebody else takes the face away while this window is
			//open. Writing it back would resurrect it, which is
			//worse than losing the drag: the person deleted it.
		MountingLayout layout = scratch->mountingLayout();
		REQUIRE(layout.removeSurface(face));
		scratch->setMountingLayout(layout);

		CHECK_FALSE(editor.commitToProject());
		CHECK(scratch->mountingLayout().isEmpty());

		editor.close();
	}
}

TEST_CASE("T19 — abrir a janela de calepinagem não faz o projeto pedir para ser salvo",
	  "[uibench][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("layout-untouched.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	QString face;
	scratch->setMountingLayout(plateWithBreaker(&face));

		//Through the file, so that what is being opened is a project in
		//the state a person finds it in, and not one this test has just
		//been writing into.
	REQUIRE(scratch.saveAndReopen());

	SECTION("olhar e fechar não muda a bandeira de modificado")
	{
		/*
			Measured against what the project says before, rather
			than against false: what a freshly opened project answers
			is its business, and the property being proved here is
			that this window does not change the answer. A window
			that marked the project modified for having been opened
			would teach people to say no to the question that
			matters.
		*/
		const bool before = scratch->projectOptionsWereModified();

		MountingLayoutEditor editor(scratch.project());
		REQUIRE(editor.shownSurface() == face);
		editor.view()->zoomFit();
		editor.view()->zoomIn();
		editor.close();

		CHECK(scratch->projectOptionsWereModified() == before);
	}

	SECTION("um passo que não move nada não chega ao projeto")
	{
		MountingLayoutEditor editor(scratch.project());
		const bool before = scratch->projectOptionsWereModified();

			//The same millimetre it is already at: the scene refuses
			//to push a step, so there is nothing for this window to
			//write either.
		CHECK_FALSE(editor.scene()->moveItem(QStringLiteral("q1"),
						     QPointF(10.5, 20.25)));
		CHECK(scratch->projectOptionsWereModified() == before);

		editor.close();
	}
}

TEST_CASE("T19 — declarar uma placa pela janela põe a face no projeto",
	  "[uibench][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("layout-new-face.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch->mountingLayout().isEmpty());

	MountingLayoutEditor editor(scratch.project());
	CHECK(editor.shownSurface().isEmpty());

	SECTION("a face declarada entra no projeto e passa a ser a desenhada")
	{
		QString error;
		const QString face = editor.addSurface(
					QStringLiteral("QCM1"),
					QStringLiteral("plate"),
					QStringLiteral("Platine principale"),
					QSizeF(600.0, 800.0),
					&error);

		INFO(error.toStdString());
		REQUIRE_FALSE(face.isEmpty());

		CHECK(editor.shownSurface() == face);
		CHECK(editor.scene()->isAreaMeasured());
		CHECK(editor.scene()->area().width == Approx(600.0));
		CHECK(editor.scene()->area().height == Approx(800.0));

		const MountingLayout layout = scratch->mountingLayout();
		REQUIRE(layout.count() == 1);
		CHECK(layout.surface(face).location_path == QString("QCM1"));
		CHECK(layout.surface(face).name == QString("Platine principale"));

		REQUIRE(scratch.saveAndReopen());
		CHECK(scratch->mountingLayout().count() == 1);
	}

	SECTION("uma face que não pertence a localização nenhuma é recusada")
	{
		QString error;
		const QString face = editor.addSurface(QString(),
						       QStringLiteral("plate"),
						       QString(),
						       QSizeF(600.0, 800.0),
						       &error);

		CHECK(face.isEmpty());
		CHECK_FALSE(error.isEmpty());
		CHECK(scratch->mountingLayout().isEmpty());
	}

	SECTION("uma face sem medida é um estado, e não uma recusa")
	{
		/*
			Zero is what the two boxes of the dialogue start at, and
			it is not a measurement of zero: it is nobody having
			measured. The face is created, it carries what gets
			mounted on it, and the plate under the parts is simply
			not drawn - which is the answer the catalogue was refused
			permission to guess.
		*/
		QString error;
		const QString face = editor.addSurface(QStringLiteral("QCM1"),
						       QStringLiteral("plate"),
						       QString(),
						       QSizeF(0.0, 0.0),
						       &error);

		INFO(error.toStdString());
		REQUIRE_FALSE(face.isEmpty());
		CHECK(editor.shownSurface() == face);
		CHECK_FALSE(editor.scene()->isAreaMeasured());
		CHECK(scratch->mountingLayout().count() == 1);
	}

	editor.close();
}

TEST_CASE("T19 — o zoom é transformação da vista, e não toca no milímetro",
	  "[uibench][calepinagem]")
{
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("layout-zoom.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	QString face;
	scratch->setMountingLayout(plateWithBreaker(&face));

	MountingLayoutEditor editor(scratch.project());
	MountingView *view = editor.view();
	REQUIRE(view != nullptr);

	SECTION("um pixel por milímetro é onde a vista volta")
	{
		view->zoomIn();
		view->zoomActualSize();

		CHECK(view->pixelsPerMillimetre() == Approx(1.0));
	}

	SECTION("aproximar e afastar são inversos, e nada disso é guardado")
	{
		view->zoomActualSize();
		const QPointF before = scratch->mountingLayout()
				       .item(QStringLiteral("q1")).position;

		view->zoomIn();
		const qreal zoomed_in = view->pixelsPerMillimetre();
		CHECK(zoomed_in > 1.0);

		view->zoomOut();
		CHECK(view->pixelsPerMillimetre() == Approx(1.0));

			//The whole point: the file said 10,5 mm before somebody
			//zoomed and it says 10,5 mm after. A factor stored next
			//to a length would be a length nobody can compare with a
			//panel any more, and there is none here to store.
		const QPointF after = scratch->mountingLayout()
				      .item(QStringLiteral("q1")).position;
		CHECK(after.x() == Approx(before.x()));
		CHECK(after.y() == Approx(before.y()));
		CHECK(after.x() == Approx(10.5));
	}

	SECTION("o zoom não sai do intervalo em que a vista sabe desenhar")
	{
		/*
			Driving a QGraphicsView transform far enough crashes the
			editor, which is the measured reason the element view is
			clamped at both ends. Forty steps in each direction is
			far more than a person reaches with a wheel, and the
			transform has to stay a number the view can draw with.
		*/
		view->zoomActualSize();
		for (int step = 0 ; step < 40 ; ++ step) {
			view->zoomIn();
		}
		const qreal high = view->pixelsPerMillimetre();
		CHECK(high > 1.0);
		CHECK(high < 100.0);

		for (int step = 0 ; step < 80 ; ++ step) {
			view->zoomOut();
		}
		const qreal low = view->pixelsPerMillimetre();
		CHECK(low > 0.0);
		CHECK(low < high);
	}

	SECTION("ajustar à placa enquadra a chapa inteira, sem deformá-la")
	{
		/*
			This case caught a real one, and it is worth saying which
			so that nobody weakens it back.

			Fitting is mathematics: QGraphicsView::fitInView with
			KeepAspectRatio picks the largest single scale at which
			the rectangle it is handed still fits the viewport, so
			what comes out always contains what went in - whatever
			the viewport measures. That is what makes the question
			answerable here, in a suite where no window is ever
			shown and the viewport is whatever size Qt happened to
			leave a widget that was never laid out.

			It was not answerable in the first version of the view,
			and the failure said so with two numbers: a plate 840 mm
			tall came back with 560 mm of it visible. The scale that
			fitted was 0,0286 px/mm, the floor the wheel is clamped
			at is 0,05, and the fit was being held up to that floor -
			cropping the plate by a third in order to obey a rule
			that exists to keep a *repeated* step from running away.
			The view now clamps a fit at the top end only.
		*/
		view->zoomFit();

			//Two scales and one number: a plate drawn wider than it
			//is tall would be a plate nobody can measure off the
			//screen, and KeepAspectRatio is what the fit is asked
			//for.
		CHECK(view->transform().m11()
		      == Approx(view->transform().m22()));
		CHECK(view->pixelsPerMillimetre() > 0.0);

			//The one bound a fit does keep, and the direction a
			//transform overflows in.
		CHECK(view->pixelsPerMillimetre() <= 40.0);

		/*
			Qt gives up on fitting rather than fitting into nothing:
			it takes two pixels off each edge first and returns
			untouched if what is left is empty. Below that width a
			fit is not a wrong answer, it is no question - so it is
			said out loud rather than passed over in silence.
		*/
		const QRect port = view->viewport()->rect();
		if (port.width() > 4 && port.height() > 4)
		{
			const QRectF visible = view->mapToScene(port)
					       .boundingRect();
			const QRectF whole = editor.scene()->sceneRect();

			CHECK(visible.width() >= Approx(whole.width()).margin(0.5));
			CHECK(visible.height() >= Approx(whole.height()).margin(0.5));
		}
		else {
			WARN("this platform leaves a window that was never "
			     "shown a viewport too small to fit anything into; "
			     "framing was not verified");
		}
	}

	editor.close();
}
