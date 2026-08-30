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
#ifndef IOWIRING_H
#define IOWIRING_H

#include "iocommon.h"

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>

class Element;
class QETProject;
class Terminal;

/**
	@brief Wiring the commons of a project's cards in one gesture (CU-11.8).

	IoCommon is the arithmetic and knows nothing of a project; this class is
	the half that does. It reads the drawing - which elements are marked as
	a default bar, which terminals of the cards are commons, which of those
	already carry a conductor -, hands plain values to IoCommon::plan(), and
	then draws what the plan says.

	The two halves are kept apart on purpose. Everything that can be wrong
	about the wiring is arithmetic, and arithmetic stands on a bench; what
	is left here is reading and drawing, which needs a project open and a
	scene under it. That is also why this file is registered in
	cmake/qet_compilation_vars.cmake and not in the test binary.

	Nothing new is written to the file. A default bar is an element carrying
	the plc_bus element information, which stock QElectroTech reads and
	writes like any other, and a wire is an ordinary conductor. The whole
	batch goes into one macro on the project's undo stack, so eight cards
	come back with one Ctrl+Z.

	The call order matters, because the plan holds identities and this class
	holds what they stand for:

	@code
		IoWiring wiring(project);
		QStringList problems;
		const IoCommon::Plan plan = wiring.plan(cards, &problems);
		// show plan.text() and problems, let the user say yes
		const IoWiring::Report report = wiring.wire(plan);
	@endcode
*/
class IoWiring
{
	Q_DECLARE_TR_FUNCTIONS(IoWiring)

	public:
		/**
			@brief What a wiring actually drew.
		*/
		struct Report
		{
			Report() {}

			/// how many commons came out wired
			int wired = 0;
			/// on how many folios a wire was drawn
			int folios = 0;
			/// what could not be done, said one line at a time
			QStringList problems;

			/// @return the whole of it said in one paragraph
			QString text() const;
		};

		explicit IoWiring(QETProject *project);

		/**
			@brief Every element of the project that can stand as a
			default bar, marked ones and the folios that repeat them.
			@return the candidates, in the order the folios come
		*/
		QList<IoCommon::Bus> buses();

		/**
			@brief The commons of @a cards, from the terminals and
			from the parts assigned to them.
			@param cards
			@param problems : gets one line per card that has none
			@return the commons, in card then terminal order
		*/
		QList<IoCommon::Common> commons(const QList<Element *> &cards,
						QStringList *problems = nullptr);

		/**
			@brief What wiring @a cards would draw.
			@param cards
			@param problems : gets one line per card that has none
			@return the plan, which wire() then draws
		*/
		IoCommon::Plan plan(const QList<Element *> &cards,
				    QStringList *problems = nullptr);

		/**
			@brief Draw @a plan, in one macro on the project stack.
			@param plan : as returned by plan(), on this same object
			@return what was drawn, and what got in the way
		*/
		Report wire(const IoCommon::Plan &plan);

		/// @return which default bar the drawing marks @a element as
		static IoCommon::BusKind busOf(const Element *element);
		/// @return how @a element names itself, its symbol failing that
		static QString labelOf(Element *element);

	private:
		IoCommon::Bus barOf(Element *element, IoCommon::BusKind kind,
				    int folio, bool marked);
		static Terminal *barTerminalFor(Element *bar, Terminal *common);

		QETProject *m_project = nullptr;
		/// the terminal each Common::id stands for, filled by commons()
		QHash<QString, QPointer<Terminal> > m_terminals;
		/// the element each Bus::id stands for, filled by buses()
		QHash<QString, QPointer<Element> > m_bars;
};

#endif // IOWIRING_H
