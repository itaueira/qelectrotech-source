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
#include "iecstructure.h"

#include "../diagramcontext.h"
#include "../location/locationtree.h"

#include <QCoreApplication>

/**
	@param text
	@return true when @a text is nothing but digits
*/
static bool isNumber(const QString &text)
{
	if (text.isEmpty()) {
		return false;
	}
	for (const QChar &character : text) {
		if (!character.isDigit()) {
			return false;
		}
	}
	return true;
}

/**
	@param path : a path of codes down the location tree, "QCM1/PORTE"
	@return the `+` part as this class stores it - "QCM1+PORTE" - empty when
	the path names no place at all

	The tree already knows how to write a path the way the norm does, with
	its separators in place: `+QCM1+PORTE`, the prefix repeated once per
	level rather than the levels nested. The location member here holds the
	middle of that instead, because the leading `+` is written by toFullTag()
	and by toContextTag() - `tag += '+' + location`. The two forms differ by
	exactly that one character, so it is dropped here and the repeated prefix
	form is not spelled out a second time. One conversion means a path of one
	level and a path of five go through the same code, which is what keeps a
	deep path from losing a separator on the way in.
*/
static QString locationFromPath(const QString &path)
{
	QString designation = LocationTree::iecTag(path);
	if (designation.startsWith(QLatin1Char('+'))) {
		designation.remove(0, 1);
	}
	return designation;
}

/**
	@brief IecStructure::IecStructure
*/
IecStructure::IecStructure()
{}

/**
	@brief IecStructure::IecStructure
	@param plant : the `=` part
	@param location : the `+` part
	@param product : the `-` part
	@param connection : the `:` part
*/
IecStructure::IecStructure(const QString &plant,
			   const QString &location,
			   const QString &product,
			   const QString &connection) :
	plant(plant),
	location(location),
	product(product),
	connection(connection)
{}

/**
	@brief IecStructure::isEmpty
	@return true when none of the parts carries anything
*/
bool IecStructure::isEmpty() const
{
	return plant.isEmpty() && location.isEmpty() && product.isEmpty()
	       && connection.isEmpty();
}

/**
	@brief IecStructure::operator==
	@param other
	@return true when the parts are the same
*/
bool IecStructure::operator==(const IecStructure &other) const
{
	return plant == other.plant
	       && location == other.location
	       && product == other.product
	       && connection == other.connection;
}

/**
	@brief IecStructure::operator!=
	@param other
	@return true when anything differs
*/
bool IecStructure::operator!=(const IecStructure &other) const
{
	return !(*this == other);
}

/**
	@brief IecStructure::inherit
	@param parent
	@param child
	@return @a parent with every non empty field of @a child on top
*/
IecStructure IecStructure::inherit(const IecStructure &parent, const IecStructure &child)
{
	IecStructure result = parent;

	// Field by field, and not all or nothing: moving one component to another
	// cabinet overrides its `+` and leaves it inheriting the `=` of the
	// project. All-or-nothing inheritance would make the norm pure typing,
	// which is the thing it is supposed to save.
	if (!child.plant.isEmpty()) {
		result.plant = child.plant;
	}
	if (!child.location.isEmpty()) {
		result.location = child.location;
	}
	if (!child.product.isEmpty()) {
		result.product = child.product;
	}
	if (!child.connection.isEmpty()) {
		result.connection = child.connection;
	}

	return result;
}

/**
	@brief IecStructure::toFullTag
	@return =CT1+A1-K3
*/
QString IecStructure::toFullTag() const
{
	QString tag;
	if (!plant.isEmpty()) {
		tag += QLatin1Char('=') + plant;
	}
	if (!location.isEmpty()) {
		tag += QLatin1Char('+') + location;
	}
	if (!product.isEmpty())
	{
		// The product part is written with its dash, unless it already
		// carries one - a tag typed as "-K3" must not become "--K3".
		tag += product.startsWith(QLatin1Char('-'))
		       ? product
		       : QLatin1Char('-') + product;
	}
	if (!connection.isEmpty()) {
		tag += QLatin1Char(':') + connection;
	}
	return tag;
}

/**
	@brief IecStructure::toShortTag
	@param with_dash
	@return -K3, or K3
*/
QString IecStructure::toShortTag(bool with_dash) const
{
	if (product.isEmpty()) {
			//A connection with no product is a terminal whose strip the
			//drawing does not name here: it is `7`. The dash would put a
			//product where there is none, and `-:7` a separator with nothing
			//on either side of it.
		return connection;
	}

	QString bare = product;
	while (bare.startsWith(QLatin1Char('-'))) {
		bare.remove(0, 1);
	}
	if (!connection.isEmpty()) {
		bare += QLatin1Char(':') + connection;
	}
	return with_dash ? QLatin1Char('-') + bare : bare;
}

/**
	@brief IecStructure::toContextTag
	@param context
	@return the parts @a context does not already say
*/
QString IecStructure::toContextTag(const IecStructure &context) const
{
	QString tag;
	if (!plant.isEmpty() && plant != context.plant) {
		tag += QLatin1Char('=') + plant;
	}
	if (!location.isEmpty() && location != context.location) {
		tag += QLatin1Char('+') + location;
	}

		//The product part is dropped only when the context is the product
		//itself - the terminal strip whose head says `-X10` once, seen from
		//its own graphic. Everywhere else the product is what identifies the
		//thing, and a drawing that omits it says nothing at all.
	const bool said_by_context = !connection.isEmpty()
				     && !context.product.isEmpty()
				     && product == context.product;
	if (!product.isEmpty() && !said_by_context)
	{
		tag += product.startsWith(QLatin1Char('-'))
		       ? product
		       : QLatin1Char('-') + product;
	}
	if (!connection.isEmpty())
	{
			//A leading colon says nothing: inside the strip the terminal is
			//`7`, not `:7`. The separator exists to join, so it is written
			//only when there is something on its left.
		if (!tag.isEmpty()) {
			tag += QLatin1Char(':');
		}
		tag += connection;
	}
	return tag;
}

/**
	@brief IecStructure::isDesignation
	@param text
	@return true when @a text has the shape of a designation of the norm
*/
bool IecStructure::isDesignation(const QString &text)
{
		//A designation opens with the letter code of the norm - K, Q, X, PS -
		//so anything opening with a digit, a parenthesis or a space is not one.
	if (text.isEmpty() || !text.at(0).isLetter()) {
		return false;
	}

	bool has_digit = false;
	for (const QChar &character : text)
	{
		if (character.isDigit()) {
			has_digit = true;
			continue;
		}
			//A dash, a dot and an underscore inside are part of the tag -
			//"K3-1" is the second contactor of a pair, "Q1.1" a sub number.
			//A space, a slash, a parenthesis are not: they are what free text
			//is made of.
		if (!character.isLetter()
		    && character != QLatin1Char('.')
		    && character != QLatin1Char('_')
		    && character != QLatin1Char('-')) {
			return false;
		}
	}

		//A letter code with no number is not a designation either: "Reserva",
		//"Terra", "PE" name something, they do not identify it.
	return has_digit;
}

/**
	@brief IecStructure::fromTag
	@param tag
	@return the structure @a tag describes
*/
IecStructure IecStructure::fromTag(const QString &tag)
{
	IecStructure structure;
	const QString trimmed = tag.trimmed();
	if (trimmed.isEmpty()) {
		return structure;
	}

	// A tag with no separator at all is the product part alone, which is what
	// every tag written before the norm was turned on looks like. Reading it
	// any other way would lose the tag of every existing project.
	if (!trimmed.contains(QLatin1Char('='))
	    && !trimmed.contains(QLatin1Char('+'))
	    && !trimmed.contains(QLatin1Char(':'))
	    && !trimmed.startsWith(QLatin1Char('-')))
	{
			//A number alone is a connection and not a product - see below.
		if (isNumber(trimmed)) {
			structure.connection = trimmed;
		} else {
			structure.product = trimmed;
		}
		return structure;
	}

	QString *current = nullptr;
	for (int index = 0 ; index < trimmed.length() ; ++index)
	{
		const QChar character = trimmed.at(index);
		if (character == QLatin1Char('=')) {
			current = &structure.plant;
		} else if (character == QLatin1Char('+'))
		{
			// The norm writes one level inside another by repeating the
			// prefix: `+QCM1+PORTE` is the door of cabinet QCM1, one
			// place and not two. So a second plus sign continues the
			// location part instead of restarting it - restarting it
			// would read those two levels back as "QCM1PORTE", a place
			// no project has, and would make this the one function that
			// cannot read what toFullTag() writes.
			if (current == &structure.location
			    && !structure.location.isEmpty()) {
				structure.location.append(QLatin1Char('+'));
			}
			current = &structure.location;
		} else if (character == QLatin1Char(':')) {
			// `:` is the connection: `-X10:7` is terminal 7 of strip X10.
			current = &structure.connection;
		} else if (character == QLatin1Char('-') && index == 0) {
			current = &structure.product;
		} else if (character == QLatin1Char('-') && current != &structure.product) {
			// A dash after the other separators starts the product part; a
			// dash inside a part - "K3-1" - belongs to that part.
			current = &structure.product;
		} else if (current) {
			current->append(character);
		} else {
			// Text before any separator: the product part, so that
			// "K3+A1" is read the way a person would read it.
			structure.product.append(character);
		}
	}

		//The `-` of the norm marks a product, and a number is not one: it is a
		//connection, the `:` part. A terminal labelled "7" - 173 of them in the
		//14 folio project - is terminal 7 of the strip it sits in, and writing
		//"-7" beside it claims the number is a product, which is a claim the
		//norm does not make. Which strip is the terminal plan's job, and T33's.
	if (structure.connection.isEmpty() && isNumber(structure.product)) {
		structure.connection = structure.product;
		structure.product.clear();
	}

		//A separator with nothing after it separates nothing: somebody typed
		//`+A1+` and meant one place. Kept out of the loop above because there
		//the character has not been read yet.
	while (structure.location.endsWith(QLatin1Char('+'))) {
		structure.location.chop(1);
	}

	return structure;
}

/**
	@brief IecStructure::fromElementInformation
	@param label
	@param info
	@return the structure the component describes
*/
IecStructure IecStructure::fromElementInformation(const QString &label,
						  const DiagramContext &info,
						  bool location_from_field)
{
		//The path down the location tree of the project comes first, and it
		//comes first whatever the project said about the field below. The two
		//are not two spellings of one value: this one is a place that exists
		//in the project, that was assigned rather than typed, and whose
		//designation is the conversion of the path. There is nothing here for
		//a project to opt into - with a location assigned the `+` is right by
		//construction, and asking for a second confirmation would only be a
		//way of getting it wrong.
	QString location =
			locationFromPath(info.value(locationPathKey()).toString());

		//The free text field is the fallback, and it only becomes the `+` of
		//the norm when the project says it is one. It is older than the norm
		//here: in the project measured it holds the terminal strip the wiring
		//of the component lands on, and reading that as a place wrote `+X1-`
		//on 23 components - information nobody put there. That is what the
		//switch protects against, which is also why it has no business
		//guarding the path above.
	if (location.isEmpty() && location_from_field) {
		location = info.value(locationKey()).toString();
	}

		//A tag typed with the separators already in it is read apart rather
		//than escaped, so a project where somebody wrote "=CT1+A1-K3" by hand
		//does not end up saying it twice.
	return inherit(fromTag(label),
		       IecStructure(info.value(plantKey()).toString(),
				    location,
				    QString()));
}

/**
	@brief IecStructure::fromFolioInformation
	@param info
	@return the structure the folio describes
*/
IecStructure IecStructure::fromFolioInformation(const DiagramContext &info)
{
	return IecStructure(info.value(plantKey()).toString(),
			    info.value(folioLocationKey()).toString(),
			    QString());
}

/**
	@brief IecStructure::plantKey
	@return the element information key of the `=` part
*/
QString IecStructure::plantKey()
{
	return QStringLiteral("plant");
}

/**
	@brief IecStructure::locationKey
	@return the element information key of the `+` part
*/
QString IecStructure::locationKey()
{
	return QStringLiteral("location");
}

/**
	@brief IecStructure::productKey
	@return the element information key of the `-` part, which is the tag
	itself and not the article number the specification named by mistake
*/
QString IecStructure::productKey()
{
	return QStringLiteral("label");
}

/**
	@brief IecStructure::locationPathKey
	@return the element information key of the path down the location tree

	The literal is repeated here rather than taken from the vocabulary of
	element information, exactly as the three keys above do, so that this
	file keeps depending on Qt alone and stays testable on a bench. The test
	asserts the two spellings agree, which is what the repetition costs.
*/
QString IecStructure::locationPathKey()
{
	return QStringLiteral("location_path");
}

/**
	@brief IecStructure::folioLocationKey
	@return the folio information key of the `+` part
*/
QString IecStructure::folioLocationKey()
{
	return QStringLiteral("locmach");
}

/*
	IecStructureSettings
*/

IecStructureSettings::IecStructureSettings()
{
}

QString IecStructureSettings::xmlTagName()
{
	return QStringLiteral("iec_structure");
}

QString IecStructureSettings::displayToString(IecTagDisplay display)
{
	switch (display) {
		case IecTagDisplay::Full:
			return QStringLiteral("full");
		case IecTagDisplay::Context:
			return QStringLiteral("context");
		case IecTagDisplay::Short:
			break;
	}
	return QStringLiteral("short");
}

IecTagDisplay IecStructureSettings::displayFromString(const QString &string)
{
		//Anything unknown reads as the short form: a project written by a
		//later version that invents a third display still opens, and opens
		//showing the tag the shop floor is used to.
	if (string == QLatin1String("full")) {
		return IecTagDisplay::Full;
	}
	if (string == QLatin1String("context")) {
		return IecTagDisplay::Context;
	}
	return IecTagDisplay::Short;
}

QString IecStructureSettings::translatedDisplay(IecTagDisplay display)
{
	switch (display) {
		case IecTagDisplay::Short:
			return QCoreApplication::translate("IecStructureSettings",
				"Courte : -K3");
		case IecTagDisplay::Context:
			return QCoreApplication::translate("IecStructureSettings",
				"Selon le contexte : -K3, +QCM2-K3 pour ce qui diffère");
		case IecTagDisplay::Full:
			return QCoreApplication::translate("IecStructureSettings",
				"Complète : =CT1+A1-K3");
	}
	return QString();
}

QString IecStructureSettings::displayedTag(const IecStructure &folio,
					   const IecStructure &element) const
{
	if (!enabled) {
			//Not "almost the same": the same. This return is what makes a
			//delivered project safe to open. `toShortTag(false)` composes
			//nothing - it puts the parts of the label back together, which for
			//a numbered terminal is its number. Element returns earlier than
			//this; the branch is what the preview of the dialog shows.
		return element.toShortTag(false);
	}

		//An element that carries no designation shows none. Composing
		//anyway draws "=CT1+A1" on its own - the plant and the location
		//of something that was never named - and that reads on the
		//drawing like a real designation. Measured on the 14 folio
		//project: 99 texts of that kind. What is asked here is product
		//**and** connection, because a terminal labelled "7" has only the
		//second one and it is a designation all the same. The guard sits
		//here, and not in toFullTag(), because a folio tag legitimately
		//has no product part.
	if (element.product.isEmpty() && element.connection.isEmpty()) {
		return QString();
	}

		//Free text is not a designation, and the `-` of the norm marks a
		//product. Somebody typed "Nota 1" or "10A / 3P / F" in the field the
		//tag lives in - 12 of them in the 14 folio project - and "-Nota 1" is
		//not the norm, it is decoration. Shown as typed, which is also what
		//the drawing showed before the structure was turned on. A component
		//that carries separators of its own said what it wanted explicitly,
		//and that is composed as asked.
	if (element.plant.isEmpty()
	    && element.location.isEmpty()
	    && element.connection.isEmpty()
	    && !IecStructure::isDesignation(element.product)) {
		return element.product;
	}

	const IecStructure full = IecStructure::inherit(folio, element);
	switch (display) {
		case IecTagDisplay::Full:
			return full.toFullTag();
		case IecTagDisplay::Context:
				//The folio is the context: what it already says is not
				//said again beside every symbol.
			return full.toContextTag(folio);
		case IecTagDisplay::Short:
			break;
	}
	return full.toShortTag();
}

bool IecStructureSettings::operator==(const IecStructureSettings &other) const
{
	return enabled == other.enabled && display == other.display
	       && location_from_element == other.location_from_element;
}

bool IecStructureSettings::operator!=(const IecStructureSettings &other) const
{
	return !(*this == other);
}

QDomElement IecStructureSettings::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(xmlTagName());
	element.setAttribute(QStringLiteral("enabled"),
			     enabled ? QStringLiteral("true")
				     : QStringLiteral("false"));
	element.setAttribute(QStringLiteral("display"), displayToString(display));
	element.setAttribute(QStringLiteral("location_from_element"),
			     location_from_element ? QStringLiteral("true")
						  : QStringLiteral("false"));
	return element;
}

void IecStructureSettings::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != xmlTagName()) {
			//A project saved before this existed says nothing, and silence
			//means off. Reading is tolerant; that is the rule.
		return;
	}
	enabled = element.attribute(QStringLiteral("enabled")) ==
			QLatin1String("true");
	display = displayFromString(element.attribute(QStringLiteral("display")));
		//Silence means off here too: a project saved before this attribute
		//existed must not gain 23 places on its drawing by being opened.
	location_from_element =
			element.attribute(QStringLiteral("location_from_element")) ==
				QLatin1String("true");
}
