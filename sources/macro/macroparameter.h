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
#ifndef MACROPARAMETER_H
#define MACROPARAMETER_H

#include <QList>
#include <QString>
#include <QStringList>

class QDomDocument;
class QDomElement;

/**
	@brief The type of a macro parameter.
	The type drives which widget edits the value and which values are
	accepted; it is stored in the .qetmak as the stable string returned by
	MacroParameter::typeToString, never as the numeric value of the
	enumerator. Renumbering the enumerator must not change what an existing
	macro file means.
*/
enum class MacroParameterType
{
	Text,   ///< free text, and the fallback for anything unknown
	Number, ///< a number, shown with the unit when one is declared
	List,   ///< one of the values declared in <choice> children
	Part    ///< a catalog part code; see the note below
};

/**
	@brief One variable declared by a parameterised macro.

	A macro carries its variables in a <parameters> block, and the drawing
	inside it refers to them by marker. The marker syntax is ${NAME}, not
	%{NAME}: %{...} is already QElectroTech's own variable syntax for title
	blocks and dynamic texts, and QETInformation::stripUnresolvedVariables()
	deletes any %{...} left standing. A macro parameter written that way
	would not be refused, it would be silently erased. See
	todo/tarefas/T05-macro-parametrizada-modelo.md.

	Part behaves as Text here: this class stores the code and validates its
	shape, and resolving the code against the catalog belongs to the dialog
	of T06, where the catalog is open and a wrong code can be told to the
	user while they are still typing it.
*/
class MacroParameter
{
	public:
		MacroParameter();
		MacroParameter(const QString &name,
			       const QString &label,
			       MacroParameterType type);

		bool isNull() const;
		bool isValid(QString *error = nullptr) const;

		/**
			@return the marker this parameter answers to in the drawing,
			"${TAG}" for a parameter named TAG.
		*/
		QString marker() const;

		/**
			Whether @a value may be stored in this parameter. A List refuses
			anything outside its choices; the other types accept any text,
			because a macro is a drawing aid and not a data entry form.
		*/
		bool acceptsValue(const QString &value) const;

		bool toXml(QDomDocument &document, QDomElement &element) const;
		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
		static QString choiceTagName();

		static QString typeToString(MacroParameterType type);
		static MacroParameterType typeFromString(const QString &string,
							 bool *ok = nullptr);
		static QString translatedTypeName(MacroParameterType type);
		static QList<MacroParameterType> allTypes();

		/**
			@brief Whether @a name may be declared as a parameter name.
			Accepted: a letter or underscore, then letters, digits and
			underscores. The scanner that looks for orphan markers is
			deliberately more permissive than this, so that a marker written
			wrong - "${MINHA VARIAVEL}" - can still be named back to the user
			instead of being passed over in silence.
		*/
		static bool isValidName(const QString &name);

	public:
		QString name;            ///< the marker name, without ${ }
		QString label;           ///< user visible label, falls back to name
		MacroParameterType type = MacroParameterType::Text;
		QString default_value;   ///< value the parameter starts with
		QString unit;            ///< free unit label, for Number
		bool required = false;   ///< a blank value refuses the insertion
		QStringList choices;     ///< the values a List offers
		QString description;
};

#endif // MACROPARAMETER_H
