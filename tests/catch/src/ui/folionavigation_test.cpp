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
#include "../../../../sources/diagramview.h"
#include "../../../../sources/folionavigationbuttons.h"
#include "../../../../sources/projectview.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/dialogwaiting.h"

#include <catch2/catch.hpp>

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QPointF>
#include <QSignalSpy>
#include <QTabBar>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWidget>

/*
	Back and Forward retrace the way the reader went through the folios of a
	project, as the buttons of a web browser do with pages. The rule of the
	walk is in navigationhistory_test of C_unittests; these cases prove the
	three things that rule cannot see:

	  - the project view: every way of changing folio is a step, Back and
	    Forward are not, and a folio removed from the project leaves the walk
	    before its view is deleted;
	  - the mouse: the two side buttons, pressed anywhere in the window - the
	    folio included, which takes every button it is pressed with - and
	    nowhere else;
	  - the keys: the folio leaves Ctrl+Alt with an arrow to a window action
	    while a component is selected on it, and does not move it. Alt with
	    an arrow does move it, finely: that is why the keys are Ctrl+Alt.

	The editor window itself is not built here - no case of this suite builds
	one -, so the two actions are made by each case the way the editor makes
	them. That the editor registers them with these keys and puts them in
	the Edit menu is left to the screen (CU-49.4).

	Labelled T49 and not CU-49.x for that reason.
*/

namespace
{
	/**
		The waiting dialog of an opening, held the way the editor holds it
		around the building of a project view (openAndAddProject): opened
		before, dropped after.

		Without it the view opens the dialog itself, and nobody drops it.
		The next project read in the same process then shows it, modal, and
		leaves it up (QETProject::readDiagramsXml): the cases that ran after
		these ones in the suite found a modal window open, and a window that
		would not close.
	*/
	class WaitingDialog
	{
		public:
			WaitingDialog() {
				DialogWaiting::instance();
			}
			~WaitingDialog() {
				DialogWaiting::dropInstance();
			}
	};

		/// A widget that counts the presses and releases it is given
	class PressCounter : public QWidget
	{
		public:
			using QWidget::QWidget;

			int presses = 0;
			int releases = 0;

		protected:
			void mousePressEvent(QMouseEvent *event) override {
				++presses;
				event->accept();
			}
			void mouseDoubleClickEvent(QMouseEvent *event) override {
				++presses;
				event->accept();
			}
			void mouseReleaseEvent(QMouseEvent *event) override {
				++releases;
				event->accept();
			}
	};

	/**
		A mouse button event of @a type for @a button, given to @a widget the
		way the application gives one: through QApplication::notify, where
		the application's event filters see it first.
	*/
	void sendButton(QWidget *widget, QEvent::Type type, Qt::MouseButton button)
	{
		const QPointF local(5, 5);
		QMouseEvent event(type, local, widget->mapToGlobal(local), button,
				  type == QEvent::MouseButtonRelease ? Qt::MouseButtons()
								     : Qt::MouseButtons(button),
				  Qt::NoModifier);
		QApplication::sendEvent(widget, &event);
	}

	/**
		A click of @a button, press and release, on @a widget.
	*/
	void clickButton(QWidget *widget, Qt::MouseButton button)
	{
		sendButton(widget, QEvent::MouseButtonPress, button);
		sendButton(widget, QEvent::MouseButtonRelease, button);
	}

	/**
		A window holding a project view, with Back and Forward made the way
		the editor makes them: actions of the window, with Ctrl+Alt and an
		arrow, and the side buttons of the mouse given to them.
	*/
	class Window : public QWidget
	{
		public:
			explicit Window(QETProject *project) :
				waiting(),
				view(new ProjectView(project, this)),
				back(this),
				forward(this)
			{
				auto layout = new QVBoxLayout(this);
				layout->addWidget(view);

				back.setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));
				forward.setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right));
				addAction(&back);
				addAction(&forward);
				QObject::connect(&back, &QAction::triggered, view, &ProjectView::goBack);
				QObject::connect(&forward, &QAction::triggered, view, &ProjectView::goForward);

				new FolioNavigationButtons(this, &back, &forward);
			}

			WaitingDialog waiting;
			ProjectView *view;
			QAction back;
			QAction forward;
	};
}

TEST_CASE("T49 — Back and Forward retrace the way walked through the folios, not their order",
	  "[ui][T49]")
{
		//Two examples, so that the walk is not proved on one project only
	UiBench::Project bench(GENERATE(as<QString>{}, "affuteuse_250h.qet", "convertisseur.qet"));
	REQUIRE(bench.isOpen());

	const WaitingDialog waiting;
	ProjectView view(bench.project());
	const QList<DiagramView *> folios = view.diagram_views();
	REQUIRE(folios.size() >= 7);

		//The walk starts on the folio the project opens on, with nothing
		//behind it: the tabs made current while the project loaded are not
		//steps.
	REQUIRE(view.currentDiagram() == folios.at(0));
	CHECK_FALSE(view.canGoBack());
	CHECK_FALSE(view.canGoForward());

	QSignalSpy changes(&view, &ProjectView::historyChanged);
	view.showDiagram(folios.at(4));
	view.showDiagram(folios.at(2));
	view.showDiagram(folios.at(6));
	CHECK(changes.count() == 3);
	REQUIRE(view.canGoBack());
	CHECK_FALSE(view.canGoForward());

	view.goBack();
	CHECK(view.currentDiagram() == folios.at(2));
	view.goBack();
	CHECK(view.currentDiagram() == folios.at(4));
	view.goBack();
	CHECK(view.currentDiagram() == folios.at(0));
	CHECK_FALSE(view.canGoBack());
	REQUIRE(view.canGoForward());

	view.goForward();
	CHECK(view.currentDiagram() == folios.at(4));
	view.goForward();
	CHECK(view.currentDiagram() == folios.at(2));
	view.goForward();
	CHECK(view.currentDiagram() == folios.at(6));
	CHECK_FALSE(view.canGoForward());

	SECTION("going to another folio after going back drops what was ahead")
	{
		view.goBack();
		view.goBack();
		REQUIRE(view.currentDiagram() == folios.at(4));

			//PageDown, the walk in the order of the project, is a step like
			//any other way of changing folio.
		view.changeTabDown();
		REQUIRE(view.currentDiagram() == folios.at(5));
		CHECK_FALSE(view.canGoForward());

		view.goBack();
		CHECK(view.currentDiagram() == folios.at(4));
		view.goBack();
		CHECK(view.currentDiagram() == folios.at(0));
	}
}

TEST_CASE("T49 — A folio removed from the project leaves the walk", "[ui][T49]")
{
	UiBench::Project bench("affuteuse_250h.qet");
	REQUIRE(bench.isOpen());
	REQUIRE_FALSE(bench->isReadOnly());

	const WaitingDialog waiting;
	ProjectView view(bench.project());
	const QList<DiagramView *> folios = view.diagram_views();
	REQUIRE(folios.size() >= 7);

	view.showDiagram(folios.at(3));
	view.showDiagram(folios.at(5));
	DiagramView *const behind = folios.at(3);
	DiagramView *const current = folios.at(5);
	Diagram *const current_folio = current->diagram();

		//Removed the way the folio menu removes it: through the undo stack.
		//The view is deleted, and only its address is compared from here on.
	view.removeDiagram(behind, true);
	REQUIRE_FALSE(view.diagram_views().contains(behind));

	view.goBack();
	CHECK(view.currentDiagram() == folios.at(0));
	CHECK_FALSE(view.canGoBack());

	view.goForward();
	REQUIRE(view.currentDiagram() == current);

		//The folio the reader stands on: the tab widget shows the next one,
		//which is a step, and Back goes to the folio seen before the removed
		//one.
	view.removeDiagram(current, true);
	REQUIRE_FALSE(view.diagram_views().contains(current));
	CHECK(view.currentDiagram() == folios.at(6));
	view.goBack();
	CHECK(view.currentDiagram() == folios.at(0));
	CHECK_FALSE(view.canGoBack());
	view.goForward();
	CHECK(view.currentDiagram() == folios.at(6));
	CHECK_FALSE(view.canGoForward());

		//Undoing the removal brings the folio back on a new view, shown like
		//a folio just added: a step.
	bench->undoStack()->undo();
	REQUIRE(view.diagram_views().size() == folios.size() - 1);
	DiagramView *restored = view.currentDiagram();
	REQUIRE(restored != nullptr);
	CHECK(restored->diagram() == current_folio);
	view.goBack();
	CHECK(view.currentDiagram() == folios.at(6));
}

TEST_CASE("T49 — The side buttons of the mouse answer anywhere in the window, and only there",
	  "[ui][T49]")
{
	QWidget window;
	auto inside = new PressCounter(&window);
	QAction back(&window);
	QAction forward(&window);
	int backs = 0;
	int forwards = 0;
	QObject::connect(&back, &QAction::triggered, [&backs]() {++backs;});
	QObject::connect(&forward, &QAction::triggered, [&forwards]() {++forwards;});
	new FolioNavigationButtons(&window, &back, &forward);

	SECTION("Back and Forward trigger their action, and the widget never sees them")
	{
		clickButton(inside, Qt::BackButton);
		CHECK(backs == 1);
		CHECK(forwards == 0);

		clickButton(inside, Qt::ForwardButton);
		CHECK(backs == 1);
		CHECK(forwards == 1);

		CHECK(inside->presses == 0);
		CHECK(inside->releases == 0);
	}

	SECTION("a quick second press, which comes as a double click, goes back a second time")
	{
		sendButton(inside, QEvent::MouseButtonPress, Qt::BackButton);
		sendButton(inside, QEvent::MouseButtonRelease, Qt::BackButton);
		sendButton(inside, QEvent::MouseButtonDblClick, Qt::BackButton);
		sendButton(inside, QEvent::MouseButtonRelease, Qt::BackButton);
		CHECK(backs == 2);
		CHECK(inside->presses == 0);
	}

	SECTION("the other buttons are left to the widget")
	{
		clickButton(inside, Qt::LeftButton);
		clickButton(inside, Qt::RightButton);
		clickButton(inside, Qt::MiddleButton);
		CHECK(inside->presses == 3);
		CHECK(inside->releases == 3);
		CHECK(backs == 0);
		CHECK(forwards == 0);
	}

	SECTION("with nowhere to go back to, the press is still kept from the widget")
	{
		back.setEnabled(false);
		clickButton(inside, Qt::BackButton);
		CHECK(backs == 0);
		CHECK(inside->presses == 0);
	}

	SECTION("another window of the program is left alone")
	{
		QWidget elsewhere;
		auto other = new PressCounter(&elsewhere);
		clickButton(other, Qt::BackButton);
		CHECK(backs == 0);
		CHECK(other->presses == 1);
		CHECK(other->releases == 1);
	}

	SECTION("once the window is gone, nothing is triggered any more")
	{
		QWidget gone;
		auto counter = new PressCounter(&gone);
		{
			QWidget short_lived;
			QAction short_back(&short_lived);
			int short_backs = 0;
			QObject::connect(&short_back, &QAction::triggered, [&short_backs]() {++short_backs;});
			new FolioNavigationButtons(&short_lived, &short_back, nullptr);
		}
			//The filter died with its window, and left the application.
		clickButton(counter, Qt::BackButton);
		CHECK(counter->presses == 1);
	}
}

TEST_CASE("T49 — The Back button pressed on the folio itself goes back", "[ui][T49]")
{
	UiBench::Project bench("affuteuse_250h.qet");
	REQUIRE(bench.isOpen());

	Window window(bench.project());
	const QList<DiagramView *> folios = window.view->diagram_views();
	REQUIRE(folios.size() >= 7);

	window.view->showDiagram(folios.at(3));
	window.view->showDiagram(folios.at(6));

		//The viewport is where a press on the drawing lands, and a graphics
		//view takes every button it is pressed with.
	clickButton(folios.at(6)->viewport(), Qt::BackButton);
	CHECK(window.view->currentDiagram() == folios.at(3));

		//Over the tabs as well.
	QTabBar *tabs = window.view->findChild<QTabBar *>();
	REQUIRE(tabs != nullptr);
	clickButton(tabs, Qt::BackButton);
	CHECK(window.view->currentDiagram() == folios.at(0));

	clickButton(folios.at(0)->viewport(), Qt::ForwardButton);
	CHECK(window.view->currentDiagram() == folios.at(3));
}

TEST_CASE("T49 — Ctrl+Alt with an arrow is left to the window with a component selected, and does not move it",
	  "[ui][T49]")
{
	/*
		The keys cannot be pressed on a shown folio here: a folio view takes
		its window for the editor window as soon as it is shown, and this
		suite builds none (DiagramView::adjustGridToZoom). So the two things
		that decide what a press does are asked one by one, the way Qt asks
		them:

		  - before a press is given to the focused widget, Qt offers it as a
		    ShortcutOverride. The view and every parent of it decline it, or
		    a window action with those keys is triggered instead of the press
		    (QShortcutMap::tryShortcut). Declined means the action wins;
		  - had it not, the folio itself would have received the press. It
		    must not move the selection then either.
	*/
	UiBench::Project bench("affuteuse_250h.qet");
	REQUIRE(bench.isOpen());
	REQUIRE_FALSE(bench->isReadOnly());

	Window window(bench.project());
	Diagram *busiest = bench.busiestSheet();
	REQUIRE(busiest != nullptr);
	DiagramView *drawing = nullptr;
	for (DiagramView *folio : window.view->diagram_views()) {
		if (folio->diagram() == busiest) {
			drawing = folio;
		}
	}
	REQUIRE(drawing != nullptr);
	window.view->showDiagram(drawing);

	Element *component = busiest->elements().first();
	busiest->clearSelection();
	component->setSelected(true);
	REQUIRE(busiest->selectedItems().size() == 1);

	auto press = [busiest](Qt::Key key, Qt::KeyboardModifiers modifiers) {
		QKeyEvent down(QEvent::KeyPress, key, modifiers);
		QApplication::sendEvent(busiest, &down);
		QKeyEvent up(QEvent::KeyRelease, key, modifiers);
		QApplication::sendEvent(busiest, &up);
	};

	SECTION("the folio declines the keys, so the window action takes them")
	{
		for (Qt::Key key : {Qt::Key_Left, Qt::Key_Right})
		{
			QKeyEvent offered(QEvent::ShortcutOverride, key, Qt::ControlModifier | Qt::AltModifier);
			offered.ignore();
			QApplication::sendEvent(drawing, &offered);
			CHECK_FALSE(offered.isAccepted());
		}
	}

	SECTION("Ctrl and an arrow moves the selection, Ctrl+Alt and an arrow does not")
	{
		const QPointF where = component->pos();

			//The fine move - Ctrl and an arrow since the upstream swapped it
			//with Alt (110a16348). Without it, "it did not move" below would
			//prove nothing.
		press(Qt::Key_Right, Qt::ControlModifier);
		REQUIRE(component->pos() != where);
		const QPointF moved = component->pos();

		press(Qt::Key_Left, Qt::ControlModifier | Qt::AltModifier);
		press(Qt::Key_Right, Qt::ControlModifier | Qt::AltModifier);
		CHECK(component->pos() == moved);
		CHECK(component->isSelected());
	}
}
