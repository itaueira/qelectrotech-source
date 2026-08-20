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
*/
IecStructure::IecStructure(const QString &plant,
			   const QString &location,
			   const QString &product) :
	plant(plant),
	location(location),
	product(product)
{}

/**
	@brief IecStructure::isEmpty
	@return true when none of the three parts carries anything
*/
bool IecStructure::isEmpty() const
{
	return plant.isEmpty() && location.isEmpty() && product.isEmpty();
}

/**
	@brief IecStructure::operator==
	@param other
	@return true when the three parts are the same
*/
bool IecStructure::operator==(const IecStructure &other) const
{
	return plant == other.plant
	       && location == other.location
	       && product == other.product;
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
		return QString();
	}
	QString bare = product;
	while (bare.startsWith(QLatin1Char('-'))) {
		bare.remove(0, 1);
	}
	return with_dash ? QLatin1Char('-') + bare : bare;
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
	    && !trimmed.startsWith(QLatin1Char('-')))
	{
		structure.product = trimmed;
		return structure;
	}

	QString *current = nullptr;
	for (int index = 0 ; index < trimmed.length() ; ++index)
	{
		const QChar character = trimmed.at(index);
		if (character == QLatin1Char('=')) {
			current = &structure.plant;
		} else if (character == QLatin1Char('+')) {
			current = &structure.location;
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

	return structure;
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
	@brief IecStructure::folioLocationKey
	@return the folio information key of the `+` part
*/
QString IecStructure::folioLocationKey()
{
	return QStringLiteral("locmach");
}
