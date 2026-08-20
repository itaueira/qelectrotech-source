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
#include "catalogclass.h"

#include "catalogproperty.h"

#include <QCoreApplication>

/**
	@brief CatalogClass::CatalogClass
	Default constructor. Builds a nameless class, which is not valid on
	purpose.
*/
CatalogClass::CatalogClass()
{}

/**
	@brief CatalogClass::CatalogClass
	@param key : stable machine key
	@param name : user visible name
*/
CatalogClass::CatalogClass(const QString &key, const QString &name) :
	key(key),
	name(name)
{}

/**
	@brief CatalogClass::isNull
	@return true when this class carries nothing
*/
bool CatalogClass::isNull() const
{
	return id == 0 && key.isEmpty() && name.isEmpty();
}

/**
	@brief CatalogClass::isValid
	@param error : when not nullptr, receives a translated reason on failure
	@return true when this class can be saved in the catalog
*/
bool CatalogClass::isValid(QString *error) const
{
	if (name.isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogClass",
							     "La classe doit avoir un nom.");
		}
		return false;
	}

	if (key.isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogClass",
							     "La classe doit avoir une clé.");
		}
		return false;
	}

	return true;
}

/**
	@brief CatalogClass::keyFromName
	@param name
	@return a stable machine key derived from @a name
*/
QString CatalogClass::keyFromName(const QString &name)
{
	return CatalogProperty::keyFromName(name);
}
