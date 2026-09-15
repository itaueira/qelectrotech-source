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
#ifndef NAVIGATECHOICEDIALOG_H
#define NAVIGATECHOICEDIALOG_H

#include <QDialog>
#include <QList>
#include <QPointer>
#include <QString>

class Element;
class QListWidget;

/**
	@brief Which of several places to go to, when Navigate finds more than one.

	@par Why the command needs it at all

	A coil is drawn once and its contacts three times, which is the ordinary
	shape of a real project rather than an exception. Until this existed the
	Navigate command was only offered when there was exactly one other
	representation, so on the very drawings it was written for it was greyed
	out.

	Going silently to one of the three was never on the table: a reference
	that lands somewhere the reader did not choose is the one thing the task
	this belongs to exists to make impossible. So the answer to "several" is
	to ask, not to guess.

	@par A list, and what each line says

	One line per destination, carrying the picture of the component, its
	identification, the folio it is drawn on and the coordinate of the folio
	border - the four things a draughtsman recognises a destination by. It is
	a plain modal list on purpose: the tree of links in the side panel needs a
	hierarchy decided in advance, and a menu at the cursor cannot be proved
	without a free screen. Nothing here is written to the project, so trading
	the list for a tree later undoes nothing.

	@par Reading order, and why it is not the order it was asked in

	The destinations arrive in the order the folio happened to hand its items
	over, which is not stable between two runs of the same project. They are
	shown by folio, then top to bottom, then left to right - the order the
	person reads the binder in - with the identifier as the last tie break so
	that the same project always offers the same list.

	@par Not JumpToElementDialog

	That one is a quick open: type part of a name and it filters the
	components of the folio you are already on. This one is handed a list
	somebody else computed, spans folios, and has nothing to filter - two or
	three lines is the whole of it.
*/
class NavigateChoiceDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit NavigateChoiceDialog(const QList<Element *> &targets,
					      QWidget *parent = nullptr);

		/**
			@return the destination the reader picked, nullptr when the
			dialog was cancelled - and also when it was accepted on a
			component that was deleted while it stood open.
		*/
		Element *chosenTarget() const;

		/// How many destinations the list offers.
		int targetCount() const;

		/**
			@param targets
			@return @a targets in the order the list shows them: by folio,
			then top to bottom, then left to right, then by identifier.

			Static and separate from the window so that the order can be
			pinned by a test without a dialog being built.
		*/
		static QList<Element *> inReadingOrder(const QList<Element *> &targets);

		/**
			@param target
			@return the one line the list shows for @a target: what it is
			called, the folio it is on and the coordinate of the border.

			A component that is on no folio answers with its identification
			alone rather than with an invented position.
		*/
		static QString describe(Element *target);

	private slots:
		void chooseCurrent();

	private:
		QList<QPointer<Element> > m_targets;
		QPointer<Element> m_chosen;
		QListWidget *m_list = nullptr;
};

#endif // NAVIGATECHOICEDIALOG_H
