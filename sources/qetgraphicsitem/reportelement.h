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
#ifndef REPORTELEMENT_H
#define REPORTELEMENT_H

#include "element.h"

/**
	@brief The ReportElement class
 *this class represent an element that can be linked to an other ReportElement
	a folio report in a diagram is a element that show a wire go on an other folio
*/
class ReportElement : public Element
{
	Q_OBJECT

	public :
		/**
			@brief What the other end of this folio reference leads to.

			The arrow drawn on the folio says where the wire goes - folio,
			line, column - and says nothing about what is waiting there. A
			wire that simply continues, a wire that is one core of a cable,
			a terminal block and a component are four different things to
			find on the next folio, and from this side they all look the
			same.
		*/
		enum Referenced {
			/// No other arrow is tied to this one.
			NotLinked,
			/// Tied, and what continues on the other side is a wire.
			Wire,
			/// The wire that continues over there belongs to a cable.
			Cable,
			/// The wire over there ends on a terminal block.
			TerminalBlock,
			/// The wire over there ends on any other component.
			Component
		};
		Q_ENUM(Referenced)

		explicit ReportElement(
			const ElementsLocation &,
			const QString& link_type,
			QGraphicsItem * = nullptr,
			int * = nullptr);
		~ReportElement() override;
		void linkToElement(Element *) override;
		void unlinkAllElements() override;
		void unlinkElement(Element *elmt) override;

		/**
			@brief What is on the other side of this reference.

			@param name when not null, filled with the name of what was
			found - the label of the component or of the terminal, the name
			of the cable - and emptied when there is nothing to name.

			The order the four answers are decided in is a choice, and it is
			this one: the cable first, because belonging to a cable is a
			property of the very wire this reference continues, and it is
			the one the drawing never shows; then the terminal block, then
			any other component, because that is what the wire ends on; and
			the plain wire last, as the answer left when the other three
			have nothing to say.
		*/
		Referenced referenced(QString *name = nullptr) const;

		/**
			@brief One sentence naming what this reference leads to, shown
			as the tooltip of the arrow.

			A sentence and not a drawn mark: the arrow of a folio reference
			is about ten units wide and its own drawing fills it, so a glyph
			added inside is a glyph to be squinted at, and one drawn outside
			would be painted over a rectangle this item does not own.
		*/
		QString referenceToolTip() const;

	protected:
		void hoverEnterEvent(QGraphicsSceneHoverEvent *) override;

	private:
		int m_inverse_report;
};

#endif // REPORTELEMENT_H
