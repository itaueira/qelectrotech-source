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
#include "catalogproperty.h"

#include <QColor>
#include <QCoreApplication>
#include <QDate>
#include <QRegularExpression>
#include <QUrl>

/**
	@brief CatalogProperty::CatalogProperty
	Default constructor. Builds a free text property with no name, which is
	not valid on purpose: a property has to be named before being saved.
*/
CatalogProperty::CatalogProperty()
{}

/**
	@brief CatalogProperty::CatalogProperty
	@param key : stable machine key
	@param name : user visible name
	@param type : type of the property
*/
CatalogProperty::CatalogProperty(const QString &key,
				 const QString &name,
				 CatalogPropertyType type) :
	key(key),
	name(name),
	type(type)
{}

/**
	@brief CatalogProperty::isNull
	@return true when this property carries nothing, i.e. it was default
	constructed and never filled.
*/
bool CatalogProperty::isNull() const
{
	return key.isEmpty() && name.isEmpty();
}

/**
	@brief CatalogProperty::isValid
	@param error : when not nullptr, receives a translated reason on failure
	@return true when this property can be saved in the catalog
*/
bool CatalogProperty::isValid(QString *error) const
{
	if (key.isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogProperty",
							     "La propriété doit avoir une clé.");
		}
		return false;
	}

	static const QRegularExpression valid_key(QStringLiteral("^[a-z0-9_]+$"));
	if (!valid_key.match(key).hasMatch())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogProperty",
							     "La clé « %1 » ne peut contenir que des minuscules, des chiffres et le tiret bas.")
				 .arg(key);
		}
		return false;
	}

	if (name.isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogProperty",
							     "La propriété doit avoir un nom.");
		}
		return false;
	}

	if (list_behaviour == CatalogListBehaviour::Mandatory
	    && list_name.isEmpty()
	    && options.isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogProperty",
							     "Une liste obligatoire doit avoir des valeurs.");
		}
		return false;
	}

	if (!default_value.isEmpty() && !acceptsValue(default_value))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogProperty",
							     "La valeur initiale « %1 » n'est pas dans la liste obligatoire.")
				 .arg(default_value);
		}
		return false;
	}

	return true;
}

/**
	@brief CatalogProperty::acceptsValue
	@param value
	@return true when @a value may be stored in this property.
	Only a mandatory list refuses a value; every other behaviour accepts
	anything, because refusing free text is how a catalog stops being filled.
*/
bool CatalogProperty::acceptsValue(const QString &value) const
{
	if (list_behaviour != CatalogListBehaviour::Mandatory) {
		return true;
	}
	if (value.isEmpty()) {
		return true;    // empty means "not filled", never a list violation
	}
	return options.contains(value);
}

/**
	@brief CatalogProperty::isOutsideList
	@param value
	@return true when @a value is not one of the offered values, whatever the
	behaviour. A suggested list uses this to highlight a value the user typed
	instead of picking - the point of a suggested list is to notice, not to
	forbid.
*/
bool CatalogProperty::isOutsideList(const QString &value) const
{
	if (list_behaviour == CatalogListBehaviour::None) {
		return false;
	}
	if (value.isEmpty()) {
		return false;
	}
	return !options.contains(value);
}

/**
	@brief CatalogProperty::toVariant
	@param raw : the value as stored in the catalog database
	@return the value converted to the type of this property. An unparsable
	value yields an invalid QVariant instead of a zero, so that "no measure"
	and "zero millimetre" stay distinguishable.
*/
QVariant CatalogProperty::toVariant(const QString &raw) const
{
	if (raw.isEmpty()) {
		return QVariant();
	}

	bool ok = false;
	switch (type)
	{
		case CatalogPropertyType::Integer:
		{
			const int value = raw.toInt(&ok);
			return ok ? QVariant(value) : QVariant();
		}
		case CatalogPropertyType::Decimal:
		case CatalogPropertyType::Currency:
		case CatalogPropertyType::Measure:
		case CatalogPropertyType::Angle:
		{
			const double value = raw.toDouble(&ok);
			return ok ? QVariant(value) : QVariant();
		}
		case CatalogPropertyType::Boolean:
		{
			const QString lowered = raw.toLower();
			if (lowered == QStringLiteral("1")
			    || lowered == QStringLiteral("true")) {
				return QVariant(true);
			}
			if (lowered == QStringLiteral("0")
			    || lowered == QStringLiteral("false")) {
				return QVariant(false);
			}
			return QVariant();
		}
		case CatalogPropertyType::Date:
		{
			const QDate date = QDate::fromString(raw, Qt::ISODate);
			return date.isValid() ? QVariant(date) : QVariant();
		}
		case CatalogPropertyType::Color:
		{
			const QColor color(raw);
			return color.isValid() ? QVariant(color) : QVariant();
		}
		case CatalogPropertyType::Link:
		{
			const QUrl url(raw);
			return url.isValid() ? QVariant(url) : QVariant();
		}
		case CatalogPropertyType::Text:
		case CatalogPropertyType::File:
		case CatalogPropertyType::Folder:
		case CatalogPropertyType::Image:
			break;
	}
	return QVariant(raw);
}

/**
	@brief CatalogProperty::fromVariant
	@param value
	@return @a value serialised the way the catalog database stores it.
	Writing is strict: a date always goes out ISO 8601, a color always
	#rrggbb, whatever the current locale is.
*/
QString CatalogProperty::fromVariant(const QVariant &value) const
{
	if (!value.isValid() || value.isNull()) {
		return QString();
	}

	switch (type)
	{
		case CatalogPropertyType::Boolean:
			return value.toBool() ? QStringLiteral("1") : QStringLiteral("0");
		case CatalogPropertyType::Date:
			return value.toDate().toString(Qt::ISODate);
		case CatalogPropertyType::Color:
			return value.value<QColor>().name();
		case CatalogPropertyType::Integer:
			return QString::number(value.toInt());
		case CatalogPropertyType::Decimal:
		case CatalogPropertyType::Currency:
		case CatalogPropertyType::Measure:
		case CatalogPropertyType::Angle:
			return QString::number(value.toDouble(), 'g', 10);
		default:
			return value.toString();
	}
}

/**
	@brief CatalogProperty::typeToString
	@param type
	@return the stable string written in the catalog database
*/
QString CatalogProperty::typeToString(CatalogPropertyType type)
{
	switch (type)
	{
		case CatalogPropertyType::Text:     return QStringLiteral("text");
		case CatalogPropertyType::Integer:  return QStringLiteral("integer");
		case CatalogPropertyType::Decimal:  return QStringLiteral("decimal");
		case CatalogPropertyType::Currency: return QStringLiteral("currency");
		case CatalogPropertyType::Date:     return QStringLiteral("date");
		case CatalogPropertyType::File:     return QStringLiteral("file");
		case CatalogPropertyType::Folder:   return QStringLiteral("folder");
		case CatalogPropertyType::Color:    return QStringLiteral("color");
		case CatalogPropertyType::Boolean:  return QStringLiteral("boolean");
		case CatalogPropertyType::Measure:  return QStringLiteral("measure");
		case CatalogPropertyType::Angle:    return QStringLiteral("angle");
		case CatalogPropertyType::Link:     return QStringLiteral("link");
		case CatalogPropertyType::Image:    return QStringLiteral("image");
	}
	return QStringLiteral("text");
}

/**
	@brief CatalogProperty::typeFromString
	@param string
	@param ok : when not nullptr, set to false for an unknown string
	@return the type @a string names, Text when it names nothing known.
	Reading is tolerant: a database written by a newer version that knows a
	type this build does not is read as text instead of being refused.
*/
CatalogPropertyType CatalogProperty::typeFromString(const QString &string, bool *ok)
{
	if (ok) {
		*ok = true;
	}
	const QList<CatalogPropertyType> types = allTypes();
	for (const CatalogPropertyType type : types)
	{
		if (typeToString(type) == string) {
			return type;
		}
	}
	if (ok) {
		*ok = false;
	}
	return CatalogPropertyType::Text;
}

/**
	@brief CatalogProperty::translatedTypeName
	@param type
	@return the name of @a type in the user language
*/
QString CatalogProperty::translatedTypeName(CatalogPropertyType type)
{
	switch (type)
	{
		case CatalogPropertyType::Text:
			return QCoreApplication::translate("CatalogProperty", "Texte");
		case CatalogPropertyType::Integer:
			return QCoreApplication::translate("CatalogProperty", "Entier");
		case CatalogPropertyType::Decimal:
			return QCoreApplication::translate("CatalogProperty", "Décimal");
		case CatalogPropertyType::Currency:
			return QCoreApplication::translate("CatalogProperty", "Monnaie");
		case CatalogPropertyType::Date:
			return QCoreApplication::translate("CatalogProperty", "Date");
		case CatalogPropertyType::File:
			return QCoreApplication::translate("CatalogProperty", "Fichier");
		case CatalogPropertyType::Folder:
			return QCoreApplication::translate("CatalogProperty", "Dossier");
		case CatalogPropertyType::Color:
			return QCoreApplication::translate("CatalogProperty", "Couleur");
		case CatalogPropertyType::Boolean:
			return QCoreApplication::translate("CatalogProperty", "Booléen");
		case CatalogPropertyType::Measure:
			return QCoreApplication::translate("CatalogProperty", "Mesure");
		case CatalogPropertyType::Angle:
			return QCoreApplication::translate("CatalogProperty", "Angle");
		case CatalogPropertyType::Link:
			return QCoreApplication::translate("CatalogProperty", "Lien");
		case CatalogPropertyType::Image:
			return QCoreApplication::translate("CatalogProperty", "Image");
	}
	return QString();
}

/**
	@brief CatalogProperty::allTypes
	@return every type, in the order they are offered to the user
*/
QList<CatalogPropertyType> CatalogProperty::allTypes()
{
	return { CatalogPropertyType::Text,
		 CatalogPropertyType::Integer,
		 CatalogPropertyType::Decimal,
		 CatalogPropertyType::Currency,
		 CatalogPropertyType::Date,
		 CatalogPropertyType::File,
		 CatalogPropertyType::Folder,
		 CatalogPropertyType::Color,
		 CatalogPropertyType::Boolean,
		 CatalogPropertyType::Measure,
		 CatalogPropertyType::Angle,
		 CatalogPropertyType::Link,
		 CatalogPropertyType::Image };
}

/**
	@brief CatalogProperty::listBehaviourToString
	@param behaviour
	@return the stable string written in the catalog database
*/
QString CatalogProperty::listBehaviourToString(CatalogListBehaviour behaviour)
{
	switch (behaviour)
	{
		case CatalogListBehaviour::None:      return QStringLiteral("none");
		case CatalogListBehaviour::Suggested: return QStringLiteral("suggested");
		case CatalogListBehaviour::Mandatory: return QStringLiteral("mandatory");
	}
	return QStringLiteral("none");
}

/**
	@brief CatalogProperty::listBehaviourFromString
	@param string
	@return the behaviour @a string names, None when it names nothing known
*/
CatalogListBehaviour CatalogProperty::listBehaviourFromString(const QString &string)
{
	if (string == QStringLiteral("suggested")) {
		return CatalogListBehaviour::Suggested;
	}
	if (string == QStringLiteral("mandatory")) {
		return CatalogListBehaviour::Mandatory;
	}
	return CatalogListBehaviour::None;
}

/**
	@brief CatalogProperty::translatedListBehaviourName
	@param behaviour
	@return the name of @a behaviour in the user language
*/
QString CatalogProperty::translatedListBehaviourName(CatalogListBehaviour behaviour)
{
	switch (behaviour)
	{
		case CatalogListBehaviour::None:
			return QCoreApplication::translate("CatalogProperty", "Aucune");
		case CatalogListBehaviour::Suggested:
			return QCoreApplication::translate("CatalogProperty", "Suggérée");
		case CatalogListBehaviour::Mandatory:
			return QCoreApplication::translate("CatalogProperty", "Obligatoire");
	}
	return QString();
}

/**
	@brief CatalogProperty::keyFromName
	@param name
	@return a stable machine key derived from @a name : lower case, accents
	folded, anything else turned into a single underscore.
*/
QString CatalogProperty::keyFromName(const QString &name)
{
	const QString decomposed = name.normalized(QString::NormalizationForm_KD).toLower();

	QString stripped;
	stripped.reserve(decomposed.size());
	for (const QChar character : decomposed)
	{
		// Drop the combining marks left behind by the decomposition, so
		// that an accented name and its unaccented spelling produce the
		// very same key.
		if (character.category() == QChar::Mark_NonSpacing) {
			continue;
		}
		stripped.append(character);
	}

	static const QRegularExpression not_allowed(QStringLiteral("[^a-z0-9]+"));
	stripped.replace(not_allowed, QStringLiteral("_"));
	while (stripped.startsWith(QLatin1Char('_'))) {
		stripped.remove(0, 1);
	}
	while (stripped.endsWith(QLatin1Char('_'))) {
		stripped.chop(1);
	}
	return stripped;
}

/**
	@brief CatalogProperty::dateFromSpreadsheetSerial
	@param serial
	@return the date @a serial means, invalid when it means nothing
*/
QDate CatalogProperty::dateFromSpreadsheetSerial(qint64 serial)
{
		//Below 1 there is no date, and above roughly the year 9999 there is
		//no date either - a serial that large is a quantity, a code or a
		//price that landed in the wrong column.
	if (serial < 1 || serial > 2958465) {
		return QDate();
	}
		//30/12/1899 and not 01/01/1900: the spreadsheet format counts a
		//29/02/1900 that never existed, and moving the epoch back one day is
		//how every reader compensates for it.
	return QDate(1899, 12, 30).addDays(serial);
}

/**
	@brief CatalogProperty::fromSpreadsheetCell
	@param raw
	@return @a raw, converted when this property is a date and @a raw is a
	serial number
*/
QString CatalogProperty::fromSpreadsheetCell(const QString &raw) const
{
	if (type != CatalogPropertyType::Date) {
		return raw;
	}
	const QString trimmed = raw.trimmed();
	if (trimmed.isEmpty()) {
		return raw;
	}

	bool is_number = false;
	const qint64 serial = trimmed.toLongLong(&is_number);
	if (!is_number) {
			//Already a date written as text. Left exactly as it is: the
			//person who typed it knows the format they meant.
		return raw;
	}

	const QDate date = dateFromSpreadsheetSerial(serial);
	if (!date.isValid()) {
			//A number that is not a plausible date. Kept, so it shows up in
			//the field and can be seen to be wrong, instead of quietly
			//becoming a date in the year 1900.
		return raw;
	}
	return date.toString(Qt::ISODate);
}
