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
#include "navigatechoicedialog.h"

#include "../diagram.h"
#include "../diagramposition.h"
#include "../qetgraphicsitem/element.h"

#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QSize>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

/**
	@brief NavigateChoiceDialog::NavigateChoiceDialog
	@param targets : the destinations to choose from
	@param parent
*/
NavigateChoiceDialog::NavigateChoiceDialog(const QList<Element *> &targets,
					   QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Naviguer : choisir la destination", "window title"));
	setMinimumWidth(460);

	QVBoxLayout *layout = new QVBoxLayout(this);

	QLabel *explanation = new QLabel(
		tr("Cet objet est représenté à plusieurs endroits du projet. "
		   "Choisissez où aller : Échap reste ici."), this);
	explanation->setWordWrap(true);
	layout->addWidget(explanation);

	m_list = new QListWidget(this);
		//The picture of the component is what a draughtsman recognises first
		//- a coil and a contact do not read alike - so it is worth a line
		//tall enough to tell them apart.
	m_list->setIconSize(QSize(32, 32));
	layout->addWidget(m_list);

	const QList<Element *> ordered = inReadingOrder(targets);
	for (Element *target : ordered)
	{
		m_targets.append(QPointer<Element>(target));

		QListWidgetItem *line = new QListWidgetItem(describe(target), m_list);
		const QPixmap picture = target->pixmap();
		if (!picture.isNull()) {
			line->setIcon(QIcon(picture));
		}
	}

	if (m_list->count() > 0) {
		m_list->setCurrentRow(0);
	}
	m_list->setFocus();

	QDialogButtonBox *box = new QDialogButtonBox(
				QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	if (QPushButton *ok = box->button(QDialogButtonBox::Ok))
	{
		ok->setDefault(true);
			//Nothing to go to means nothing to press: the command does not
			//open this window on an empty list, and a button that would do
			//nothing is worse than no button.
		ok->setEnabled(m_list->count() > 0);
	}
	layout->addWidget(box);

		//Both ways in end at the same place. Double clicking a line is how
		//this kind of list is used, and itemActivated is also what Enter
		//raises on a list that has the focus; the button is what somebody
		//who came with the mouse looks for.
	connect(m_list, &QListWidget::itemActivated,
		this, &NavigateChoiceDialog::chooseCurrent);
	connect(box, &QDialogButtonBox::accepted,
		this, &NavigateChoiceDialog::chooseCurrent);
	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

/**
	@brief NavigateChoiceDialog::chosenTarget
	@return the destination that was picked, nullptr when none was
*/
Element *NavigateChoiceDialog::chosenTarget() const
{
	return m_chosen.data();
}

/**
	@brief NavigateChoiceDialog::targetCount
	@return how many destinations are offered
*/
int NavigateChoiceDialog::targetCount() const
{
	return m_targets.count();
}

/**
	@brief NavigateChoiceDialog::chooseCurrent
	Take the highlighted line as the answer and close.
*/
void NavigateChoiceDialog::chooseCurrent()
{
	const int row = m_list ? m_list->currentRow() : -1;
	if (row < 0 || row >= m_targets.count()) {
		return;
	}

		//A QPointer and not a raw one: the window is modal, but a component
		//can still go away under it - undoing an insertion from the keyboard
		//does exactly that - and jumping to a deleted component would crash
		//rather than say no.
	if (m_targets.at(row).isNull()) {
		return;
	}

	m_chosen = m_targets.at(row);
	accept();
}

/**
	@brief NavigateChoiceDialog::inReadingOrder
	@param targets
	@return the same destinations, in the order the binder is read in
*/
QList<Element *> NavigateChoiceDialog::inReadingOrder(
		const QList<Element *> &targets)
{
	QList<Element *> ordered;
	for (Element *target : targets)
	{
		if (target) {
			ordered.append(target);
		}
	}

	std::sort(ordered.begin(), ordered.end(),
		  [](Element *first, Element *second)
	{
			//A component that is on no folio sorts before every one that is,
			//rather than landing wherever the comparison happens to put it.
		const int first_folio = first->diagram()
				? first->diagram()->folioIndex() : -1;
		const int second_folio = second->diagram()
				? second->diagram()->folioIndex() : -1;
		if (first_folio != second_folio) {
			return first_folio < second_folio;
		}

		const QPointF first_pos = first->scenePos();
		const QPointF second_pos = second->scenePos();
		if (first_pos.y() != second_pos.y()) {
			return first_pos.y() < second_pos.y();
		}
		if (first_pos.x() != second_pos.x()) {
			return first_pos.x() < second_pos.x();
		}

			//Two components at the same point of the same folio is rare and
			//legal - one drawn on top of another. Without this the order
			//between them would be whatever std::sort felt like, and the
			//list would change between two runs of the same project.
		return first->uuid().toString() < second->uuid().toString();
	});

	return ordered;
}

/**
	@brief NavigateChoiceDialog::describe
	@param target
	@return the line the list shows for @a target
*/
QString NavigateChoiceDialog::describe(Element *target)
{
	if (!target) {
		return QString();
	}

		//The tag the person wrote, and the kind of component behind it. The
		//tag alone says K3 twice when a coil and its contact carry it; the
		//name alone says "contact" three times over. Together they read.
	const QString label = target->elementInformations()
			      .value(QStringLiteral("label")).toString();
	const QString name = target->name();
	const QString identity = label.isEmpty()
			? name
			: (label + QStringLiteral(" — ") + name);

	Diagram *folio = target->diagram();
	if (!folio) {
		return identity;
	}

	const QString title = folio->title();
	const QString folio_text = title.isEmpty()
			? tr("folio %1").arg(folio->folioIndex() + 1)
			: tr("folio %1 « %2 »")
			  .arg(QString::number(folio->folioIndex() + 1), title);

		//The coordinate of the border, which is how a printed schematic is
		//referred to out loud, and not the scene coordinate nobody reads.
	DiagramPosition position = folio->convertPosition(target->scenePos());

	return tr("%1 — %2 (%3)").arg(identity, folio_text, position.toString());
}
