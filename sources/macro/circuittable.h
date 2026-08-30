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
#ifndef CIRCUITTABLE_H
#define CIRCUITTABLE_H

#include "macroparameterset.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class QDomDocument;
class QDomElement;

/**
	@brief One line of the table: one macro, and the values it is inserted
	with.

	The macro is held as text - what ElementsLocation::toString() gives - and
	not as an ElementsLocation, so that a row can be read back from a project
	saved on another machine, and so that the table itself stays out of the
	half of the program that opens files.
*/
class CircuitRow
{
	public:
		CircuitRow();
		explicit CircuitRow(const QString &macro_path);

		bool isNull() const;

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
		static QString valueTagName();

	public:
			/// stable between sessions: what a regenerated row is found by
		QString id;
			/// the macro, as ElementsLocation::toString() writes it
		QString macro_path;
			/// parameter name to value, including values a change of macro
			/// has made inert - see CircuitTable::isInert()
		QHash<QString, QString> values;
};

/**
	@brief The twenty motor feeders, before they are drawn.

	A table of rows, each row a macro and its values, plus the columns that
	follow from them: the union of the parameters of the macros in use, in
	declaration order, first macro first. A cell of a column the row's macro
	does not declare is *inert* - it asks for nothing and refuses to be
	typed in - and inert is not the same as empty: the value stays where it
	was, so switching a row from the reversing starter to the direct one and
	back does not cost the person the four fields only the reversing starter
	has. That is CU-08.4, and it is the reason values live in a hash keyed by
	parameter name rather than in a list keyed by column.

	The table never opens a file. It is handed the parameter set of each
	macro through setParameters(), by whoever did open it, which is what lets
	the test binary link it next to MacroParameterSet instead of next to
	QETApp.
*/
class CircuitTable
{
	public:
		/**
			@brief A row the generator cannot draw, and why.
		*/
		struct Problem
		{
				/// index in the table, zero based; text() says row + 1
			int row = -1;
				/// CircuitRow::id, which survives a sort
			QString id;
				/// required parameters left blank, in declaration order
			QStringList missing;
				/// values the parameter type refuses, by parameter name
			QStringList refused;
				/// no macro chosen at all
			bool no_macro = false;

			QString text() const;
		};

		CircuitTable();

			//the macros in use, and their parameters
		void setParameters(const QString &macro_path,
				   const MacroParameterSet &parameters);
		bool hasParameters(const QString &macro_path) const;
		MacroParameterSet parameters(const QString &macro_path) const;
		QStringList macroPaths() const;

			//rows
		bool isEmpty() const;
		int rowCount() const;
		QList<CircuitRow> rows() const;
		CircuitRow row(int index) const;
		int indexOfId(const QString &id) const;
		int appendRow(const CircuitRow &row);
		int appendRow(const QString &macro_path);
		bool insertRow(int index, const CircuitRow &row);
		bool removeRow(int index);
		void clear();
		QString macroPath(int index) const;
		bool setMacroPath(int index, const QString &macro_path);

			//columns
		QStringList columns() const;
		int columnCount() const;
		QString columnLabel(const QString &column) const;
		MacroParameter parameterFor(int index, const QString &column) const;
		bool isInert(int index, const QString &column) const;

			//cells
		QString value(int index, const QString &column) const;
		QHash<QString, QString> values(int index) const;
		bool setValue(int index,
			      const QString &column,
			      const QString &value,
			      QString *error = nullptr);

			//what the generator has to refuse
		QList<Problem> problems() const;

			//persistence
		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
		static QString newId();

	private:
		QList<CircuitRow> m_rows;
		QHash<QString, MacroParameterSet> m_parameters;
};

#endif // CIRCUITTABLE_H
