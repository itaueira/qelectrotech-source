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
#include "catalogvalueorigin.h"

/**
	@brief CatalogValueOrigin::fromPart
	@return the origin of a value typed on the part itself
*/
CatalogValueOrigin CatalogValueOrigin::fromPart()
{
	CatalogValueOrigin origin;
	origin.source = CatalogValueSource::Part;
	return origin;
}

/**
	@brief CatalogValueOrigin::fromClass
	@param declaring_class_id the class the property is declared on
	@param declaring_class_key its stable key
	@param declaring_class_name its user visible name
	@return the origin of a value taken from that declaration
*/
CatalogValueOrigin CatalogValueOrigin::fromClass(int declaring_class_id,
						 const QString &declaring_class_key,
						 const QString &declaring_class_name)
{
	CatalogValueOrigin origin;
	origin.source     = CatalogValueSource::Class;
	origin.class_id   = declaring_class_id;
	origin.class_key  = declaring_class_key;
	origin.class_name = declaring_class_name;
	return origin;
}

/**
	@brief CatalogValueOrigin::unset
	@return the origin of a value nobody wrote anywhere
*/
CatalogValueOrigin CatalogValueOrigin::unset()
{
	CatalogValueOrigin origin;
	origin.source = CatalogValueSource::Unset;
	return origin;
}

/**
	@brief CatalogValueOrigin::isRead
	@return true when somebody looked where the value comes from
*/
bool CatalogValueOrigin::isRead() const
{
	return source != CatalogValueSource::Unread;
}

/**
	@brief CatalogValueOrigin::isFromPart
	@return true when the value was typed on the part
*/
bool CatalogValueOrigin::isFromPart() const
{
	return source == CatalogValueSource::Part;
}

/**
	@brief CatalogValueOrigin::isFromClass
	@return true when the value is the initial value of a declaration
*/
bool CatalogValueOrigin::isFromClass() const
{
	return source == CatalogValueSource::Class;
}

/**
	@brief CatalogValueOrigin::holdsValue
	@return true when the origin names a record holding a value
*/
bool CatalogValueOrigin::holdsValue() const
{
	return isFromPart() || isFromClass();
}

/**
	@brief CatalogValueOrigin::className
	@return the class to name in a sentence, empty when the origin is not a
	class

	Three fallbacks for one name because the sentence is written whatever
	happens: a catalogue whose class cache does not hold the declaring class
	is a catalogue in trouble, and "inherited from class 42" is a sentence
	somebody can act on, while "inherited from class " is not.
*/
QString CatalogValueOrigin::className() const
{
	if (!isFromClass()) {
		return QString();
	}
	if (!class_name.isEmpty()) {
		return class_name;
	}
	if (!class_key.isEmpty()) {
		return class_key;
	}
	return class_id > 0 ? QString::number(class_id) : QString();
}

/**
	@brief CatalogValueOrigin::describe
	@return one sentence saying where the value comes from, empty when
	nothing was read about it
*/
QString CatalogValueOrigin::describe() const
{
	switch (source)
	{
		case CatalogValueSource::Part:
			return tr("valeur saisie sur la pièce");
		case CatalogValueSource::Class:
			return tr("valeur héritée de la classe %1").arg(className());
		case CatalogValueSource::Unset:
			return tr("valeur non renseignée");
		case CatalogValueSource::Unread:
			break;
	}
	return QString();
}
