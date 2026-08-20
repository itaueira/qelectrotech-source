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
#ifndef CATALOGPROPERTY_H
#define CATALOGPROPERTY_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

/**
	@brief The type of a catalog property.
	The type drives editing (which widget), validation (which values are
	accepted) and rendering in the part lists. It is stored in the catalog
	database as the stable string returned by CatalogProperty::typeToString,
	never as the numeric value of the enumerator: renumbering the enumerator
	must not change what an existing database means.
*/
enum class CatalogPropertyType
{
	Text,      ///< free text, the fallback type
	Integer,   ///< whole number
	Decimal,   ///< real number
	Currency,  ///< amount of money, rendered with the locale currency
	Date,      ///< calendar date, stored ISO 8601
	File,      ///< path to a file
	Folder,    ///< path to a directory
	Color,     ///< color, stored as #rrggbb
	Boolean,   ///< true / false
	Measure,   ///< length, stored in millimetre, shown in the environment unit
	Angle,     ///< angle in degree
	Link,      ///< URL, typically the manufacturer data sheet
	Image      ///< path to a picture of the part
};

/**
	@brief How a property offers its list of values to the user.
*/
enum class CatalogListBehaviour
{
	None,      ///< free field, no list at all
	Suggested, ///< the user picks from the list or types something else
	Mandatory  ///< the user can only pick from the list
};

/**
	@brief The CatalogProperty class
	One typed property declared on one catalog class. Properties are created
	by the user, not by the code: adding a field to the catalog must never
	require recompiling QElectroTech nor migrating the database by hand.

	A property belongs to exactly one class and is inherited by every
	subclass of it, so declaring it high in the tree is how a field reaches
	every component at once. See Catalog::effectiveProperties().
*/
class CatalogProperty
{
	public:
		CatalogProperty();
		CatalogProperty(const QString &key,
				const QString &name,
				CatalogPropertyType type);

		bool isNull() const;
		bool isValid(QString *error = nullptr) const;

		/**
			Whether @a value may be stored in this property. A mandatory
			list refuses anything outside the list; a suggested list
			accepts everything but the caller may want to warn.
		*/
		bool acceptsValue(const QString &value) const;
		bool isOutsideList(const QString &value) const;

		QVariant toVariant(const QString &raw) const;
		QString fromVariant(const QVariant &value) const;

		static QString typeToString(CatalogPropertyType type);
		static CatalogPropertyType typeFromString(const QString &string,
							  bool *ok = nullptr);
		static QString translatedTypeName(CatalogPropertyType type);
		static QList<CatalogPropertyType> allTypes();

		static QString listBehaviourToString(CatalogListBehaviour behaviour);
		static CatalogListBehaviour listBehaviourFromString(const QString &string);
		static QString translatedListBehaviourName(CatalogListBehaviour behaviour);

		/**
			Turn a user visible name into a stable machine key:
			"Código interno ACME" becomes "codigo_interno_acme".
			The key is what the project file and the lists use, so it must
			not change when the user renames the property.
		*/
		static QString keyFromName(const QString &name);

	public:
		int id = 0;              ///< database identifier, 0 when not saved yet
		int class_id = 0;        ///< owning class
		QString key;             ///< stable machine key
		QString name;            ///< user visible name
		CatalogPropertyType type = CatalogPropertyType::Text;
		CatalogListBehaviour list_behaviour = CatalogListBehaviour::None;
		QString list_name;       ///< controlled list to read the values from
		QStringList options;     ///< inline values, used when list_name is empty
		QString default_value;   ///< value a new part starts with
		int order_index = 0;     ///< position in the class, and column order
		QString unit;            ///< free unit label, for Measure and Decimal
		QString description;
};

#endif // CATALOGPROPERTY_H
