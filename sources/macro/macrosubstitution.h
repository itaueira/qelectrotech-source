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
#ifndef MACROSUBSTITUTION_H
#define MACROSUBSTITUTION_H

#include <QHash>
#include <QString>
#include <QStringList>

class QDomElement;

/**
	@brief Replaces the ${NAME} markers of a macro by the values chosen for
	them.

	The syntax is "${NAME}", and the choice of "$" rather than "%" is not a
	matter of taste. "%{...}" is already the variable syntax of QElectroTech:
	QETInformation builds those markers, the dynamic texts test for them, the
	title block editor offers about twenty of them, and
	QETInformation::stripUnresolvedVariables() deletes every "%{...}" it
	meets, unconditionally, at the end of every title block interpretation.
	A macro marker written that way would be silently erased by the program
	itself. Measured on the files this fork exists to serve, "%{...}" appears
	28 times in the real project and 97 times in the symbol collection, and
	"${...}" appears nowhere - which is exactly what makes it safe: nothing
	in QElectroTech today reads "${", so a .qetmak carrying markers still
	opens in the official program, and inserts the circuit as it always did.

	Two rules keep the whole thing predictable:

	- "$${" writes a literal "${". One escape, and no escape for the escape.
	- A value that has just been substituted is never scanned again. A value
	  is therefore literal, never a reference to another variable - which is
	  what spares this code an evaluation order and a cycle check, and spares
	  the user a new way for a macro to come out half done without saying so.

	This is a scanner, not a regular expression: the escape rule falls out of
	a single left to right pass, and does not have to be expressed twice.

	The engine knows nothing about Diagram, Element or QETApp. It walks a
	QDomElement and holds QString, which is what lets the test binary link it
	with a handful of sources instead of half the program.
*/
namespace MacroSubstitution
{
	/**
		@brief What one pass over a subtree did, and what it could not do.
	*/
	struct Result
	{
			/// true when nothing was left unresolved
		bool ok = false;
			/// how many markers were replaced by a value
		int replacements = 0;
			/**
				Every marker still standing after the pass, distinct and
				in the order it was met. Usually a name nobody declared;
				it can also be a marker someone formatted halfway
				through, which the file stores split across style runs
				and which no scanner can put back together. Either way it
				did not become a value, and either way the insertion has
				to say so instead of drawing "${TAG}" on the sheet.
			*/
		QStringList orphans;
			/// set when the pass could not even start
		QString error;

		QString errorText() const;
	};

	/**
		@brief Substitute every marker of @a subtree, in place.
		@param subtree : the node to walk - every attribute value and every
		text node under it, without a whitelist of fields. A whitelist would
		have to know that conductor/@element1_label mirrors the label of the
		element it is tied to, and would go stale the first time upstream
		adds a field.
		@param values : marker name to value
		@return what was replaced, and what was left behind

		Only the subtree handed over is touched. The element definitions a
		.qetmak carries in its <collection> are deliberately out of reach:
		parameterising the drawing of a symbol is another task.
	*/
	Result apply(QDomElement &subtree, const QHash<QString, QString> &values);

	/**
		@brief Substitute every marker of one string.
		@param input
		@param values
		@param replacements : when not nullptr, incremented once per
		replacement - incremented, not assigned, so a caller can count a
		whole tree in one variable
		@param orphans : when not nullptr, receives the names left behind,
		distinct and in the order they were met
		@return the string with the markers resolved
	*/
	QString substitute(const QString &input,
			   const QHash<QString, QString> &values,
			   int *replacements = nullptr,
			   QStringList *orphans = nullptr);

	/**
		@brief The marker names @a input carries.
		@return the distinct names, in the order they appear. "$${" counts
		as a literal and is not one of them.
	*/
	QStringList markersIn(const QString &input);

	/**
		@brief @a input with anything between < and > removed.
		Used to read a rich text field as the words someone typed, so a
		marker broken by formatting can at least be named.
	*/
	QString stripTags(const QString &input);

	/**
		@brief @a name written as the marker that answers to it, "${TAG}".
		This is the one definition of the syntax; everything else asks here.
	*/
	QString marker(const QString &name);

	/**
		@brief @a input with every "${" written so it survives a pass.
		For a value that has to reach the drawing with a marker in it.
	*/
	QString escapeMarkers(const QString &input);
}

#endif // MACROSUBSTITUTION_H
