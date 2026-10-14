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
#include "../../../../sources/diagramview.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/undocommand/changeelementinformationcommand.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QPointF>
#include <QSettings>
#include <QUndoStack>

#include <cmath>

/*
	The label of a duplicated element sits where the label of the original
	sits, whatever the alignment of the text.

	A pasted element has its label, comment and location emptied (the
	"erase label on copy" preference, on by default), and the label is
	filled again afterwards: by the numbering the copy follows, by whoever
	types it, or by the master a slave is linked to. Filling a text realigns
	it from the rectangle it had before, so that its anchor - the left edge,
	the centre or the right edge - stays where it was. The emptying used to
	be done with that realignment blocked: the empty text kept its top left
	corner and lost the anchor, and the filling then pushed a centred label
	by half its width and a right-aligned one by its whole width.

	The duplication goes through DiagramView::duplicate(), which is what
	Ctrl+D runs, and that ends in PasteDiagramCommand, which Ctrl+V runs too.
	industrial.qet has labels that follow a numbering (8F1 is one) and
	linked slaves; all its texts are aligned to the left, so each case sets
	the alignment it needs on the original before copying it.
*/

namespace
{
	const QString example = QStringLiteral("industrial.qet");

	/// A preference of the paste command set while the guard lives, put back on the way out
	class Preference
	{
		public:
			Preference(const QString &key, bool value) :
				m_key(QStringLiteral("diagramcommands/") + key)
			{
				QSettings settings;
				m_was_stored = settings.contains(m_key);
				m_stored = settings.value(m_key);
				settings.setValue(m_key, value);
			}

			~Preference()
			{
				QSettings settings;
				if (m_was_stored) {
					settings.setValue(m_key, m_stored);
				} else {
					settings.remove(m_key);
				}
			}

			Preference(const Preference &) = delete;
			Preference &operator=(const Preference &) = delete;

		private:
			const QString m_key;
			bool m_was_stored = false;
			QVariant m_stored;
	};

	/// The text of @p element that shows its label
	DynamicElementTextItem *labelText(Element *element)
	{
		const QList<DynamicElementTextItem *> texts = element->dynamicTextItems();
		for (DynamicElementTextItem *text : texts)
		{
			if (text->textFrom() == DynamicElementTextItem::ElementInfo
					&& text->infoName() == QLatin1String("label")) {
				return text;
			}
		}
		return nullptr;
	}

	/// The element of the project labelled @p label
	Element *labelled(UiBench::Project &project, const QString &label)
	{
		for (Diagram *diagram : project.diagrams())
		{
			const DiagramContent content(diagram, false);
			for (Element *element : content.m_elements) {
				if (element->actualLabel() == label) {
					return element;
				}
			}
		}
		return nullptr;
	}

	/// A slave of the project linked to a master, with a text showing the label
	Element *linkedSlave(UiBench::Project &project)
	{
		for (Diagram *diagram : project.diagrams())
		{
			const DiagramContent content(diagram, false);
			for (Element *element : content.m_elements)
			{
				if (element->linkType() == Element::Slave && !element->isFree()
						&& labelText(element)
						&& !labelText(element)->toPlainText().isEmpty()) {
					return element;
				}
			}
		}
		return nullptr;
	}

	/**
		The point of @p text its alignment keeps in place, in the coordinates
		of its element: the left edge, the centre or the right edge, and the
		top, the middle or the bottom, as DiagramTextItem::finishAlignment()
		reads them.
	*/
	QPointF anchor(const DynamicElementTextItem *text)
	{
		const QRectF rect = text->boundingRect();
		const Qt::Alignment alignment = text->alignment();

		qreal x = rect.left();
		if (alignment & Qt::AlignRight) {
			x = rect.right();
		} else if (alignment & Qt::AlignHCenter) {
			x = rect.center().x();
		}

		qreal y = rect.top();
		if (alignment & Qt::AlignBottom) {
			y = rect.bottom();
		} else if (alignment & Qt::AlignVCenter) {
			y = rect.center().y();
		}

		return text->mapToParent(QPointF(x, y));
	}

	bool same(const QPointF &a, const QPointF &b)
	{
		return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
	}

	/// @p point as a failure report prints it
	QString shown(const QPointF &point)
	{
		return QStringLiteral("(%1, %2)").arg(point.x()).arg(point.y());
	}

	/// Ctrl+D on @p element alone; returns the copy, which the paste leaves selected
	Element *duplicate(Element *element)
	{
		Diagram *diagram = element->diagram();
		diagram->clearSelection();
		element->setSelected(true);

		DiagramView view(diagram);
		view.duplicate(QPoint(0, 20));
		QCoreApplication::processEvents();

		const QList<QGraphicsItem *> selection = diagram->selectedItems();
		for (QGraphicsItem *item : selection)
		{
			auto *copy = dynamic_cast<Element *>(item);
			if (copy && copy != element) {
				return copy;
			}
		}
		return nullptr;
	}

	/// The label of @p element changed the way the element properties dialog changes it
	void typeLabel(Element *element, const QString &label)
	{
		const DiagramContext old_info = element->elementInformations();
		DiagramContext new_info = old_info;
		new_info.addValue(QStringLiteral("label"), label);
		element->diagram()->undoStack().push(
				new ChangeElementInformationCommand(element, old_info, new_info));
	}
}

TEST_CASE("CU-53.1, CU-53.2 and CU-53.3 — A numbered copy has its label where the original has it",
	  "[paste][duplicate][alignment]")
{
	const Qt::Alignment horizontal = GENERATE(Qt::AlignHCenter, Qt::AlignRight, Qt::AlignLeft);
	CAPTURE(int(horizontal));

	Preference erase(QStringLiteral("erase-label-on-copy"), true);
	Preference number(QStringLiteral("autonumber-pasted-elements"), true);

	UiBench::Project project(example);
	REQUIRE(project.isOpen());

	Element *original = labelled(project, QStringLiteral("8F1"));
	REQUIRE(original);
	DynamicElementTextItem *original_text = labelText(original);
	REQUIRE(original_text);
	original_text->setAlignment(horizontal | Qt::AlignTop);

	Element *copy = duplicate(original);
	REQUIRE(copy);
	DynamicElementTextItem *copy_text = labelText(copy);
	REQUIRE(copy_text);

		//The numbering ran: the copy has a label of its own, not the original's
	CHECK_FALSE(copy_text->toPlainText().isEmpty());
	CHECK(copy_text->toPlainText() != original_text->toPlainText());

	CAPTURE(shown(anchor(original_text)), shown(anchor(copy_text)));
	CHECK(same(anchor(copy_text), anchor(original_text)));
}

TEST_CASE("CU-53.4 — A label typed after the duplication sits where the original's sits",
	  "[paste][duplicate][alignment]")
{
	const Qt::Alignment horizontal = GENERATE(Qt::AlignHCenter, Qt::AlignRight);
	CAPTURE(int(horizontal));

	Preference erase(QStringLiteral("erase-label-on-copy"), true);
	Preference number(QStringLiteral("autonumber-pasted-elements"), false);

	UiBench::Project project(example);
	REQUIRE(project.isOpen());

	Element *original = labelled(project, QStringLiteral("8F1"));
	REQUIRE(original);
	DynamicElementTextItem *original_text = labelText(original);
	REQUIRE(original_text);
	original_text->setAlignment(horizontal | Qt::AlignVCenter);

	Element *copy = duplicate(original);
	REQUIRE(copy);
	DynamicElementTextItem *copy_text = labelText(copy);
	REQUIRE(copy_text);
	REQUIRE(copy_text->toPlainText().isEmpty());

	typeLabel(copy, original_text->toPlainText());
	REQUIRE(copy_text->toPlainText() == original_text->toPlainText());

		//The same text, so the same place: anchor and corner alike
	CAPTURE(shown(anchor(original_text)), shown(anchor(copy_text)));
	CHECK(same(anchor(copy_text), anchor(original_text)));
	CHECK(same(copy_text->pos(), original_text->pos()));
}

TEST_CASE("CU-53.5 — Undo and redo bring the copy back with its label in place",
	  "[paste][duplicate][alignment]")
{
	Preference erase(QStringLiteral("erase-label-on-copy"), true);
	Preference number(QStringLiteral("autonumber-pasted-elements"), true);

	UiBench::Project project(example);
	REQUIRE(project.isOpen());

	Element *original = labelled(project, QStringLiteral("8F1"));
	REQUIRE(original);
	DynamicElementTextItem *original_text = labelText(original);
	REQUIRE(original_text);
	original_text->setAlignment(Qt::AlignRight | Qt::AlignTop);

	Element *copy = duplicate(original);
	REQUIRE(copy);
	DynamicElementTextItem *copy_text = labelText(copy);
	REQUIRE(copy_text);
	const QString copy_label = copy_text->toPlainText();
	REQUIRE_FALSE(copy_label.isEmpty());
	const QPointF placed = copy_text->pos();

	QUndoStack &stack = original->diagram()->undoStack();
	stack.undo();
	QCoreApplication::processEvents();
	REQUIRE(copy->scene() == nullptr);

	stack.redo();
	QCoreApplication::processEvents();
	REQUIRE(copy->scene() == original->scene());
	REQUIRE(copy_text->toPlainText() == copy_label);

	CAPTURE(shown(anchor(original_text)), shown(anchor(copy_text)));
	CHECK(same(anchor(copy_text), anchor(original_text)));
	CHECK(same(copy_text->pos(), placed));
}

TEST_CASE("CU-53.6 — A slave duplicated alone and linked afterwards has its label where the original has it",
	  "[paste][duplicate][alignment]")
{
	Preference erase(QStringLiteral("erase-label-on-copy"), true);
	Preference number(QStringLiteral("autonumber-pasted-elements"), true);

	UiBench::Project project(example);
	REQUIRE(project.isOpen());

	Element *original = linkedSlave(project);
	REQUIRE(original);
	Element *master = original->linkedElements().value(0);
	REQUIRE(master);
	DynamicElementTextItem *original_text = labelText(original);
	REQUIRE(original_text);
	original_text->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

	Element *copy = duplicate(original);
	REQUIRE(copy);
	REQUIRE(copy->isFree());
	DynamicElementTextItem *copy_text = labelText(copy);
	REQUIRE(copy_text);
	REQUIRE(copy_text->toPlainText().isEmpty());

	copy->linkToElement(master);
	QCoreApplication::processEvents();
	REQUIRE_FALSE(copy->isFree());
	REQUIRE(copy_text->toPlainText() == original_text->toPlainText());

	CAPTURE(shown(anchor(original_text)), shown(anchor(copy_text)));
	CHECK(same(anchor(copy_text), anchor(original_text)));
}

TEST_CASE("CU-53.7 — With the labels kept on copy, the numbered copy has its label where the original has it",
	  "[paste][duplicate][alignment]")
{
	Preference erase(QStringLiteral("erase-label-on-copy"), false);
	Preference number(QStringLiteral("autonumber-pasted-elements"), true);

	UiBench::Project project(example);
	REQUIRE(project.isOpen());

	Element *original = labelled(project, QStringLiteral("8F1"));
	REQUIRE(original);
	DynamicElementTextItem *original_text = labelText(original);
	REQUIRE(original_text);
	original_text->setAlignment(Qt::AlignRight | Qt::AlignTop);

	Element *copy = duplicate(original);
	REQUIRE(copy);
	DynamicElementTextItem *copy_text = labelText(copy);
	REQUIRE(copy_text);
	CHECK(copy_text->toPlainText() != original_text->toPlainText());

	CAPTURE(shown(anchor(original_text)), shown(anchor(copy_text)));
	CHECK(same(anchor(copy_text), anchor(original_text)));
}
