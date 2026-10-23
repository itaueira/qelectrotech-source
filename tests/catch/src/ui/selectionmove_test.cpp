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
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/elementsmover.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/elementtextitemgroup.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QPointF>
#include <QUndoStack>

#include <cmath>

/*
	Dragging a selection moves every item by the drag, in the folio.

	An element's text is a child of the element, and its position is kept in
	the element's own coordinates. Selected without its element - which is
	what a rubber band does when it takes a whole text and only part of the
	element's body - the text used to receive the drag in folio coordinates,
	so on an element turned by 90 degrees a horizontal drag moved it
	vertically. The same sum was redone by undo and redo.

	2612_ats_singlephase.qet has texts on elements turned by 90, 180 and 270
	degrees. The drag goes through ElementsMover, the code the folio runs
	while the mouse is held down, and the undo through the command it pushes.
*/

namespace
{
	const QString example = QStringLiteral("2612_ats_singlephase.qet");
	const QPointF drag(40, 0);

	/// An element of @p diagram turned by @p degrees, with a text of its own;
	/// not a terminal of a strip, whose texts are selected by no click
	/// since T87
	Element *turnedWithText(Diagram *diagram, int degrees)
	{
		const DiagramContent content(diagram, false);
		for (Element *element : content.m_elements)
		{
			if (qRound(element->rotation()) % 360 == degrees
					&& element->elementData().m_type != ElementData::Terminal
					&& !element->dynamicTextItems().isEmpty()) {
				return element;
			}
		}
		return nullptr;
	}

	/// The first folio of the project holding such an element; else an
	/// element with a text, turned here - the example turns by 180 and 270
	/// degrees only terminals of strips
	Element *turnedWithText(UiBench::Project &project, int degrees)
	{
		for (Diagram *diagram : project.diagrams()) {
			if (Element *element = turnedWithText(diagram, degrees)) {
				return element;
			}
		}
		for (Diagram *diagram : project.diagrams())
		{
			const DiagramContent content(diagram, false);
			for (Element *element : content.m_elements)
			{
				if (element->elementData().m_type != ElementData::Terminal
				    && !element->dynamicTextItems().isEmpty())
				{
					element->setRotation(degrees);
					return element;
				}
			}
		}
		return nullptr;
	}

	/// Another element of the same folio, so that the selection is a mixed one
	Element *anotherElement(Element *element)
	{
		const DiagramContent content(element->diagram(), false);
		for (Element *other : content.m_elements) {
			if (other != element) {
				return other;
			}
		}
		return nullptr;
	}

	/// One drag of the current selection, through the code the folio runs
	void dragSelection(Diagram *diagram, const QPointF &movement)
	{
		ElementsMover mover;
		REQUIRE(mover.beginMovement(diagram) > 0);
		mover.continueMovement(movement);
		mover.endMovement();
			//The undo command animates the items, with a duration of 0
		QCoreApplication::processEvents();
	}

	bool same(const QPointF &a, const QPointF &b)
	{
		return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
	}
}

TEST_CASE("CU-46.1 and CU-46.2 — A text of a turned element, selected without it, moves with the drag",
		  "[selectionmove]")
{
	const int degrees = GENERATE(90, 180, 270);
	CAPTURE(degrees);
	UiBench::Project project(example);
	REQUIRE(project.isOpen());
	Element *element = turnedWithText(project, degrees);
	REQUIRE(element);
	Diagram *diagram = element->diagram();
	DynamicElementTextItem *text = element->dynamicTextItems().first();
	Element *other = anotherElement(element);
	REQUIRE(other);

	diagram->clearSelection();
	text->setSelected(true);
	other->setSelected(true);
	REQUIRE_FALSE(element->isSelected());

	const QPointF text_before = text->scenePos();
	const QPointF other_before = other->scenePos();
	const QPointF element_before = element->scenePos();
	dragSelection(diagram, drag);

	CHECK(same(text->scenePos(), text_before + drag));
	CHECK(same(other->scenePos(), other_before + drag));
		//The element was not selected: it stays
	CHECK(same(element->scenePos(), element_before));
}

TEST_CASE("CU-46.3 — Undo puts the text back where it was, redo where the drag left it",
		  "[selectionmove]")
{
	const int degrees = GENERATE(90, 180, 270);
	CAPTURE(degrees);
	UiBench::Project project(example);
	REQUIRE(project.isOpen());
	Element *element = turnedWithText(project, degrees);
	REQUIRE(element);
	Diagram *diagram = element->diagram();
	DynamicElementTextItem *text = element->dynamicTextItems().first();
	Element *other = anotherElement(element);
	REQUIRE(other);

	diagram->clearSelection();
	text->setSelected(true);
	other->setSelected(true);
	const QPointF before = text->scenePos();
	dragSelection(diagram, drag);
	const QPointF after = text->scenePos();
	REQUIRE(same(after, before + drag));

	diagram->undoStack().undo();
	QCoreApplication::processEvents();
	CHECK(same(text->scenePos(), before));

	diagram->undoStack().redo();
	QCoreApplication::processEvents();
	CHECK(same(text->scenePos(), after));
}

TEST_CASE("CU-46.4 — A text whose element is selected moves once, with the element",
		  "[selectionmove]")
{
	UiBench::Project project(example);
	REQUIRE(project.isOpen());
	Element *element = turnedWithText(project, 90);
	REQUIRE(element);
	Diagram *diagram = element->diagram();
	DynamicElementTextItem *text = element->dynamicTextItems().first();

	diagram->clearSelection();
	element->setSelected(true);
	text->setSelected(true);
	const QPointF text_before = text->scenePos();
	const QPointF element_before = element->scenePos();
	dragSelection(diagram, drag);

	CHECK(same(element->scenePos(), element_before + drag));
	CHECK(same(text->scenePos(), text_before + drag));

	diagram->undoStack().undo();
	QCoreApplication::processEvents();
	CHECK(same(element->scenePos(), element_before));
	CHECK(same(text->scenePos(), text_before));
}

TEST_CASE("CU-46.5 — A group of texts of a turned element moves with the drag",
		  "[selectionmove]")
{
	const int degrees = GENERATE(90, 180, 270);
	CAPTURE(degrees);
	UiBench::Project project(example);
	REQUIRE(project.isOpen());
	Element *element = turnedWithText(project, degrees);
	REQUIRE(element);
	Diagram *diagram = element->diagram();

		//No example has a group on a turned element: one is made here, the
		//way the text editor of the element makes it
	ElementTextItemGroup *group = element->addTextGroup(QStringLiteral("T46"));
	REQUIRE(group);
	REQUIRE(element->addTextToGroup(element->dynamicTextItems().first(), group));
	Element *other = anotherElement(element);
	REQUIRE(other);

	diagram->clearSelection();
	group->setSelected(true);
	other->setSelected(true);
	const QPointF before = group->scenePos();
	dragSelection(diagram, drag);
	CHECK(same(group->scenePos(), before + drag));

	diagram->undoStack().undo();
	QCoreApplication::processEvents();
	CHECK(same(group->scenePos(), before));
}
