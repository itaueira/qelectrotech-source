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
#include "macrosubstitution.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNamedNodeMap>
#include <QDomNode>

/**
	@brief Substitute one string of the tree, and note what did not resolve.
	@param text : the string as it stands in the file
	@param values : marker name to value
	@param result : receives the count and any marker left behind
	@return the string as it goes back into the file
*/
static QString substituteAndAudit(const QString &text,
				  const QHash<QString, QString> &values,
				  MacroSubstitution::Result &result)
{
	const QString substituted = MacroSubstitution::substitute(text,
								  values,
								  &result.replacements,
								  &result.orphans);

		//An <input text="..."> holds rich text, not plain text, so a marker
		//someone formatted halfway through is stored split across style
		//runs - "${TA<span ...>G}" - and no scanner can put it back
		//together. It will not be substituted; but read without its tags it
		//can at least be named, and being told why the macro was refused
		//beats finding ${TAG} drawn on the sheet.
		//The comparison is made on the text as it came in, and never on
		//the text as it goes out. A value the user typed may itself contain
		//a "${", and so does every escaped "$${" once the pass has written
		//it out: both leave the pass looking exactly like a marker nobody
		//resolved, and neither is one.
	if (!text.contains(QLatin1Char('<'))
	    || !text.contains(QLatin1Char('$')))
	{
		return substituted;
	}

	const QString stripped = MacroSubstitution::stripTags(text);
	if (stripped == text) {
		return substituted;
	}

		//What is reported is what only exists once the formatting is taken
		//away: plain to the reader, invisible to the scanner. A name that
		//is already a well formed marker in the text was handled above,
		//resolved or named, and is not one of these.
		//The one it cannot see is the same name twice in one field, well
		//formed once and broken once; that second one is left on the sheet.
	const QStringList visible = MacroSubstitution::markersIn(text);
	const QStringList hidden = MacroSubstitution::markersIn(stripped);
	for (const QString &name : hidden)
	{
		if (!visible.contains(name) && !result.orphans.contains(name)) {
			result.orphans << name;
		}
	}
	return substituted;
}

/**
	@brief Walk @a node and everything under it, substituting as it goes.
	@param node
	@param values
	@param result

	Every attribute value and every text node is scanned, without a list of
	the fields that may hold a marker. Such a list would have to know that
	conductor/@element1_label carries the label of the element the conductor
	is tied to, and it would go stale the first time upstream adds a field -
	quietly, by leaving a marker unsubstituted rather than by failing.
*/
static void walk(QDomNode node,
		 const QHash<QString, QString> &values,
		 MacroSubstitution::Result &result)
{
	if (node.isNull()) {
		return;
	}

	if (node.isElement())
	{
		QDomElement element = node.toElement();
		const QDomNamedNodeMap attributes = element.attributes();
			//The names are read first: writing an attribute back while
			//walking the map is asking the map to change under the walk.
		QStringList names;
		names.reserve(attributes.count());
		for (int i = 0 ; i < attributes.count() ; ++ i) {
			names << attributes.item(i).nodeName();
		}
		for (const QString &name : names)
		{
			const QString before = element.attribute(name);
			const QString after = substituteAndAudit(before, values, result);
			if (after != before) {
				element.setAttribute(name, after);
			}
		}
	}
	else if (node.isText())
	{
		const QString before = node.nodeValue();
		const QString after = substituteAndAudit(before, values, result);
		if (after != before) {
			node.setNodeValue(after);
		}
	}

	for (QDomNode child = node.firstChild() ;
	     !child.isNull() ;
	     child = child.nextSibling())
	{
		walk(child, values, result);
	}
}

/**
	@brief MacroSubstitution::Result::errorText
	@return why the pass is refused, empty when it is not
*/
QString MacroSubstitution::Result::errorText() const
{
	if (!error.isEmpty()) {
		return error;
	}
	if (orphans.isEmpty()) {
		return QString();
	}
	if (orphans.size() == 1)
	{
		return QCoreApplication::translate("MacroSubstitution",
						   "Le marqueur %1 n'a pas été remplacé.")
				.arg(MacroSubstitution::marker(orphans.first()));
	}

	QStringList markers;
	markers.reserve(orphans.size());
	for (const QString &name : orphans) {
		markers << MacroSubstitution::marker(name);
	}
	return QCoreApplication::translate("MacroSubstitution",
					   "Ces marqueurs n'ont pas été remplacés : %1.")
			.arg(markers.join(QStringLiteral(", ")));
}

/**
	@brief MacroSubstitution::apply
	@param subtree : the node to walk, modified in place
	@param values : marker name to value
	@return what was replaced, and what was left behind
*/
MacroSubstitution::Result MacroSubstitution::apply(QDomElement &subtree,
						   const QHash<QString, QString> &values)
{
	Result result;
	if (subtree.isNull())
	{
		result.error = QCoreApplication::translate("MacroSubstitution",
							   "Le macro ne contient aucun schéma.");
		return result;
	}

	walk(subtree, values, result);
	result.ok = result.orphans.isEmpty();
	return result;
}

/**
	@brief MacroSubstitution::substitute
	@param input
	@param values
	@param replacements : when not nullptr, incremented once per replacement
	@param orphans : when not nullptr, receives the names left behind
	@return @a input with its markers resolved

	One left to right pass, deliberately not a regular expression: the escape
	rule then falls out of the walk instead of having to be written a second
	time in a pattern. A value that has just been written out is never looked
	at again, so a value is literal and can never name another variable.
*/
QString MacroSubstitution::substitute(const QString &input,
				      const QHash<QString, QString> &values,
				      int *replacements,
				      QStringList *orphans)
{
		//The overwhelming majority of the strings in a diagram carry no
		//dollar at all, and this is the whole cost of them.
	if (!input.contains(QLatin1Char('$'))) {
		return input;
	}

	QString out;
	out.reserve(input.length());

	const int length = input.length();
	int i = 0;
	while (i < length)
	{
		const QChar current = input.at(i);
		if (current == QLatin1Char('$'))
		{
				//"$${" is how a drawing writes a literal "${"
			if (i + 2 < length
			    && input.at(i + 1) == QLatin1Char('$')
			    && input.at(i + 2) == QLatin1Char('{'))
			{
				out += QLatin1String("${");
				i += 3;
				continue;
			}

			if (i + 1 < length && input.at(i + 1) == QLatin1Char('{'))
			{
				const int close = input.indexOf(QLatin1Char('}'), i + 2);
				const QString name = (close == -1) ? QString()
								   : input.mid(i + 2, close - (i + 2));
					//A brace inside the name means this was never a
					//marker: something else opened it, and the text is
					//left exactly as the author wrote it.
					//An angle bracket means the same thing for another
					//reason - the marker was cut in half by formatting,
					//and "FA<span ...>BRICANTE" is not a name worth
					//showing anyone. Left alone here, it is named
					//properly by the pass over the tag stripped text.
				if (close != -1
				    && !name.contains(QLatin1Char('{'))
				    && !name.contains(QLatin1Char('<'))
				    && !name.contains(QLatin1Char('>')))
				{
					if (values.contains(name))
					{
						out += values.value(name);
						if (replacements) {
							++ (*replacements);
						}
					}
					else
					{
							//Left standing, and named. The marker in the
							//output is what lets a caller show the user
							//the drawing it refused to insert.
						if (orphans && !orphans->contains(name)) {
							*orphans << name;
						}
						out += input.mid(i, close + 1 - i);
					}
					i = close + 1;
					continue;
				}
			}
		}
		out += current;
		++ i;
	}
	return out;
}

/**
	@brief MacroSubstitution::markersIn
	@param input
	@return the distinct marker names @a input carries, in order
*/
QStringList MacroSubstitution::markersIn(const QString &input)
{
	QStringList found;
		//Scanning with no value at all makes every marker an orphan, which
		//is exactly the list wanted here - and keeps one scanner in the file
		//rather than two that have to agree with each other.
	substitute(input, QHash<QString, QString>(), nullptr, &found);
	return found;
}

/**
	@brief MacroSubstitution::stripTags
	@param input
	@return @a input without anything between < and >
*/
QString MacroSubstitution::stripTags(const QString &input)
{
	if (!input.contains(QLatin1Char('<'))) {
		return input;
	}

	QString out;
	out.reserve(input.length());
	bool inside_tag = false;
	for (int i = 0 ; i < input.length() ; ++ i)
	{
		const QChar current = input.at(i);
		if (current == QLatin1Char('<')) {
			inside_tag = true;
		} else if (current == QLatin1Char('>')) {
			inside_tag = false;
		} else if (!inside_tag) {
			out += current;
		}
	}
	return out;
}

/**
	@brief MacroSubstitution::marker
	@param name
	@return the marker that answers to @a name
*/
QString MacroSubstitution::marker(const QString &name)
{
	return QStringLiteral("${") + name + QStringLiteral("}");
}

/**
	@brief MacroSubstitution::escapeMarkers
	@param input
	@return @a input with every "${" written so a pass leaves it alone
*/
QString MacroSubstitution::escapeMarkers(const QString &input)
{
	QString out = input;
	out.replace(QLatin1String("${"), QLatin1String("$${"));
	return out;
}
