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
#ifndef MACROPARAMETERSET_H
#define MACROPARAMETERSET_H

#include "macroparameter.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class QDomDocument;
class QDomElement;

/**
	@brief A named set of values for the parameters of one macro.

	This is what turns a parameterised macro into something worth using
	twice: "Partida direta 7,5 cv" fills every variable at once, and what it
	does not declare keeps the value it already had.

	A value here is literal. It is never read as a reference to another
	parameter, and a value that was just substituted is never scanned again -
	both would ask for an evaluation order and a cycle check, and would pay
	for them with a new way for a macro to come out half done without saying
	so.
*/
class MacroValueSet
{
	public:
		MacroValueSet();
		explicit MacroValueSet(const QString &name);

		bool isNull() const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
		static QString valueTagName();

	public:
		QString name;                    ///< user visible name of the set
		QHash<QString, QString> values;  ///< parameter name to value
};

/**
	@brief The parameters one macro declares, in the order it declares them.

	Order is the file order, and it is what the dialog of T06 will read the
	fields from, so it is kept rather than sorted: a macro author puts the
	marking first and the conductor section last because that is the order
	someone fills them in.

	This class knows nothing about Diagram, Element or QETApp - it reads and
	writes QDomElement and holds QString. That is on purpose: it is what
	lets the test binary link it with a handful of sources instead of half
	the program.
*/
class MacroParameterSet
{
	public:
		MacroParameterSet();

		bool isEmpty() const;
		int count() const;

		QList<MacroParameter> parameters() const;
		QStringList names() const;
		bool contains(const QString &name) const;
		MacroParameter parameter(const QString &name) const;

		bool append(const MacroParameter &parameter);
		void clear();

		/**
			@return every declared parameter mapped to its default value,
			which is the state the T06 dialog opens on.
		*/
		QHash<QString, QString> defaults() const;

		/**
			@brief The required parameters @a values leaves blank.
			@return every one of them, in declaration order - not the first.
			Being told one missing field at a time is the slowest way to fill
			a form, and a macro is meant to save time.
		*/
		QStringList missingRequired(const QHash<QString, QString> &values) const;

		/**
			@brief The names in @a values that no parameter declares.
			A value passed for a variable nobody declared is almost always a
			name typed wrong, so it is reported rather than ignored: failing
			out loud beats substituting nothing and not saying why.
		*/
		QStringList undeclared(const QHash<QString, QString> &values) const;

		QStringList valueSetNames() const;
		bool hasValueSet(const QString &name) const;
		MacroValueSet valueSet(const QString &name) const;
		bool appendValueSet(const MacroValueSet &value_set);

		/**
			@brief Lay the value set @a name over @a current.
			@return the merged values; @a current untouched when @a name
			names no set. What the set declares wins, what it does not
			declare keeps the value it had.
		*/
		QHash<QString, QString> applyValueSet(const QString &name,
						      const QHash<QString, QString> &current) const;

		bool fromXml(const QDomElement &qet_macro_root);
		void appendToXml(QDomDocument &document, QDomElement &qet_macro_root) const;
		QDomElement parametersToXml(QDomDocument &document) const;
		QDomElement valueSetsToXml(QDomDocument &document) const;

		static QString parametersTagName();
		static QString valueSetsTagName();

	private:
		QList<MacroParameter> m_parameters;
		QList<MacroValueSet> m_value_sets;
};

#endif // MACROPARAMETERSET_H
