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
#ifndef CIRCUITGENERATOR_H
#define CIRCUITGENERATOR_H

#include "circuitlayout.h"

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QStringList>

class CircuitTable;
class Diagram;
class DiagramContent;
class QDomElement;
class QGraphicsItem;
class MacroFile;
class QETProject;

/**
	@brief Draw a whole table of circuits into a project, in one undoable
	step.

	This is where the table of T08 becomes folios. Everything it needs to
	decide has already been decided elsewhere - the parameters by T05, the
	sequence of marks by T06, the geometry by CircuitLayout, the identity
	of what is copied by MacroUuid - so what is left here is the part that
	cannot be tested without a project: create the folios, paste the
	circuits, and stack it all under one command.

	One command is the point. Twenty circuits on four folios is a hundred
	and something objects; a person who looks at the result and does not
	like it must be able to say so with one Ctrl+Z, not eighty. That is
	possible because the folio creation upstream made undoable in August
	2026 pushes onto the same stack as the pastes - the project's, which is
	what Diagram::undoStack() returns.

	A row that cannot be generated is reported and skipped, and the others
	are drawn. Nineteen good circuits held hostage by the twentieth would
	be worse than the manual work this is replacing.
*/
class CircuitGenerator
{
	Q_DECLARE_TR_FUNCTIONS(CircuitGenerator)

	public:
		/**
			@brief What the person asked for, beyond the table itself.
		*/
	struct Options
	{
			/**
				How many circuits go on one folio before the next one starts,
				zero to fit as many as the border has room for. Ten feeders
				fit on an A3; a person who reviews four at a time wants four.
			*/
		int circuits_per_sheet = 0;
			/// written on the title block of every folio made, empty to leave the default
		QString sheet_title;
	};

		/**
			@brief What one generation did, and what it refused.
		*/
	struct Report
	{
			/// circuits drawn
		int generated = 0;
			/// folios made
		int sheets = 0;
			/// one line per row that was skipped or that drew with a caveat
		QStringList problems;
			/**
				Row id to the uuids that row's circuit now carries. This is
				how a single row is found again to be regenerated without
				touching the other nineteen, which is CU-08.5.
			*/
		QHash<QString, QStringList> issued;
			/**
				Row id to where its circuit actually landed, measured the way
				Diagram::fromXml measures it and not the way it was asked for:
				fromXml snaps the move to the grid, so the point handed in and
				the point arrived at differ by up to one grid step. Recording
				the arrival is what makes regenerating twice a fixed point.
			*/
		QHash<QString, QPointF> positions;

			/// @return the whole thing in one paragraph, ready to be shown
		QString text() const;
	};

	explicit CircuitGenerator(QETProject *project);

		/**
			Draw @a table into the project.
			@param table
			@param options
			@return what was drawn, and what was not
		*/
	Report generate(const CircuitTable &table, const Options &options);

		/**
			Draw @a rows again, each where its circuit already is, deleting the
			old one first. The rows not named are not touched - not their labels,
			not their wire numbers, not their position. That is CU-08.5.
			@param table the table, with the uuids of the last generation
			@param rows indexes into it; empty for every row already drawn
			@return what was redrawn, and what was refused
		*/
	Report regenerate(const CircuitTable &table,
			  const QList<int> &rows = QList<int>());

	private:
		/// @return the capacity of the folios this generation will make
	CircuitLayout::Sheet sheetFromProject() const;
		/// @return the width of @a file once drawn, in scene units
	qreal measure(MacroFile &file, const QHash<QString, QString> &values) const;
		/// @return a folio added to the project, titled as @a options asks
	Diagram *addSheet(const Options &options);
		/// @return the items of @a diagram whose own uuid is in @a uuids
	static QList<QGraphicsItem *> itemsOf(Diagram *diagram,
					      const QSet<QString> &uuids);
		/// @return the corner Diagram::fromXml would have measured, for @a items
	static QPointF anchorOf(const QList<QGraphicsItem *> &items);
		/// @return the same corner, for what fromXml has just added
	static QPointF anchorOf(const DiagramContent &content);
		/// @return the categories of @a node MacroUuid cannot give an identity to
	static QStringList unrenewable(const QDomElement &node);

	QETProject *m_project = nullptr;
};

#endif // CIRCUITGENERATOR_H
