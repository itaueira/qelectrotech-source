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
#include "catalogpart.h"

#include <QCoreApplication>
#include <QSet>

/**
	@brief CatalogPin::roleToString
	@param role
	@return the stable string written in the catalog database
*/
QString CatalogPin::roleToString(CatalogPinRole role)
{
	switch (role)
	{
		case CatalogPinRole::Unknown:        return QStringLiteral("unknown");
		case CatalogPinRole::Terminal:       return QStringLiteral("terminal");
		case CatalogPinRole::Coil:           return QStringLiteral("coil");
		case CatalogPinRole::ContactNo:      return QStringLiteral("contact_no");
		case CatalogPinRole::ContactNc:      return QStringLiteral("contact_nc");
		case CatalogPinRole::PowerContactNo: return QStringLiteral("power_contact_no");
		case CatalogPinRole::Input:          return QStringLiteral("input");
		case CatalogPinRole::Output:         return QStringLiteral("output");
		case CatalogPinRole::InputAnalog:    return QStringLiteral("input_analog");
		case CatalogPinRole::OutputAnalog:   return QStringLiteral("output_analog");
		case CatalogPinRole::OutputRelay:    return QStringLiteral("output_relay");
		case CatalogPinRole::CommPort:       return QStringLiteral("comm_port");
		case CatalogPinRole::SupplyCommon:   return QStringLiteral("supply_common");
		case CatalogPinRole::ReturnCommon:   return QStringLiteral("return_common");
	}
	return QStringLiteral("unknown");
}

/**
	@brief CatalogPin::roleFromString
	@param string
	@return the role @a string names, Unknown when it names nothing known
*/
CatalogPinRole CatalogPin::roleFromString(const QString &string)
{
	const QList<CatalogPinRole> roles = allRoles();
	for (const CatalogPinRole role : roles)
	{
		if (roleToString(role) == string) {
			return role;
		}
	}
	return CatalogPinRole::Unknown;
}

/**
	@brief CatalogPin::translatedRoleName
	@param role
	@return the name of @a role in the user language
*/
QString CatalogPin::translatedRoleName(CatalogPinRole role)
{
	switch (role)
	{
		case CatalogPinRole::Unknown:
			return QCoreApplication::translate("CatalogPin", "Non déclaré");
		case CatalogPinRole::Terminal:
			return QCoreApplication::translate("CatalogPin", "Borne");
		case CatalogPinRole::Coil:
			return QCoreApplication::translate("CatalogPin", "Bobine");
		case CatalogPinRole::ContactNo:
			return QCoreApplication::translate("CatalogPin", "Contact NO");
		case CatalogPinRole::ContactNc:
			return QCoreApplication::translate("CatalogPin", "Contact NF");
		case CatalogPinRole::PowerContactNo:
			return QCoreApplication::translate("CatalogPin", "Contact de puissance NO");
		case CatalogPinRole::Input:
			return QCoreApplication::translate("CatalogPin", "Entrée");
		case CatalogPinRole::Output:
			return QCoreApplication::translate("CatalogPin", "Sortie");
		case CatalogPinRole::InputAnalog:
			return QCoreApplication::translate("CatalogPin", "Entrée analogique");
		case CatalogPinRole::OutputAnalog:
			return QCoreApplication::translate("CatalogPin", "Sortie analogique");
		case CatalogPinRole::OutputRelay:
			return QCoreApplication::translate("CatalogPin", "Sortie relais");
		case CatalogPinRole::CommPort:
			return QCoreApplication::translate("CatalogPin", "Port de communication");
		case CatalogPinRole::SupplyCommon:
			return QCoreApplication::translate("CatalogPin", "Commun d'alimentation");
		case CatalogPinRole::ReturnCommon:
			return QCoreApplication::translate("CatalogPin", "Commun de retour");
	}
	return QString();
}

/**
	@brief CatalogPin::allRoles
	@return every role, in the order they are offered to the user
*/
QList<CatalogPinRole> CatalogPin::allRoles()
{
	return { CatalogPinRole::Unknown,
		 CatalogPinRole::Terminal,
		 CatalogPinRole::Coil,
		 CatalogPinRole::ContactNo,
		 CatalogPinRole::ContactNc,
		 CatalogPinRole::PowerContactNo,
		 CatalogPinRole::Input,
		 CatalogPinRole::Output,
		 CatalogPinRole::InputAnalog,
		 CatalogPinRole::OutputAnalog,
		 CatalogPinRole::OutputRelay,
		 CatalogPinRole::CommPort,
		 CatalogPinRole::SupplyCommon,
		 CatalogPinRole::ReturnCommon };
}

/**
	@brief CatalogPin::isIoRole
	@param role
	@return true when @a role is a point of the field
*/
bool CatalogPin::isIoRole(CatalogPinRole role)
{
	switch (role)
	{
		case CatalogPinRole::Input:
		case CatalogPinRole::Output:
		case CatalogPinRole::InputAnalog:
		case CatalogPinRole::OutputAnalog:
		case CatalogPinRole::OutputRelay:
			return true;
		default:
			return false;
	}
}

/**
	@brief CatalogPart::CatalogPart
	Default constructor. Builds a part with no code, which is not valid on
	purpose: the code is the only mandatory field of a part.
*/
CatalogPart::CatalogPart()
{}

/**
	@brief CatalogPart::CatalogPart
	@param code : the part code
	@param class_id : the class the part belongs to
*/
CatalogPart::CatalogPart(const QString &code, int class_id) :
	class_id(class_id),
	code(code)
{}

/**
	@brief CatalogPart::isNull
	@return true when this part carries nothing
*/
bool CatalogPart::isNull() const
{
	return id == 0 && code.isEmpty();
}

/**
	@brief CatalogPart::isValid
	@param error : when not nullptr, receives a translated reason on failure
	@return true when this part can be saved in the catalog
*/
bool CatalogPart::isValid(QString *error) const
{
	if (code.trimmed().isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPart",
							     "La pièce doit avoir une référence.");
		}
		return false;
	}

	if (class_id <= 0)
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPart",
							     "La pièce doit appartenir à une classe.");
		}
		return false;
	}

	if (revision < 1)
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPart",
							     "La révision doit être supérieure ou égale à 1.");
		}
		return false;
	}

	return true;
}

/**
	@brief CatalogPart::value
	@param key
	@return the value stored for @a key, empty when the part never had that
	field filled. Falling back to the default of the property is the job of
	Catalog::effectiveValues(), which is the only place that knows the class.
*/
QString CatalogPart::value(const QString &key) const
{
	return values.value(key);
}

/**
	@brief CatalogPart::hasValue
	@param key
	@return true when this part has an own value for @a key
*/
bool CatalogPart::hasValue(const QString &key) const
{
	return values.contains(key);
}

/**
	@brief CatalogPart::setValue
	@param key
	@param value
*/
void CatalogPart::setValue(const QString &key, const QString &value)
{
	values.insert(key, value);
}

/**
	@brief CatalogPart::clearValue
	@param key
	Forget the own value of @a key, so that the part falls back to the
	default of the property again.
*/
void CatalogPart::clearValue(const QString &key)
{
	values.remove(key);
}

/**
	@brief CatalogPart::pinsWithRole
	@param role
	@return every pin of this part having @a role, in pin order
*/
QList<CatalogPin> CatalogPart::pinsWithRole(CatalogPinRole role) const
{
	QList<CatalogPin> found;
	for (const CatalogPin &pin : pins)
	{
		if (pin.role == role) {
			found.append(pin);
		}
	}
	return found;
}

/**
	@brief CatalogPart::pinLabels
	@return the label of every pin, in pin order
*/
QStringList CatalogPart::pinLabels() const
{
	QStringList labels;
	labels.reserve(pins.size());
	for (const CatalogPin &pin : pins) {
		labels.append(pin.label);
	}
	return labels;
}

/**
	@brief CatalogPart::channelKeys
	@return the channel of every point of the part, once each, in pin order
*/
QStringList CatalogPart::channelKeys() const
{
	QStringList keys;
	for (const CatalogPin &pin : pins)
	{
		if (pin.channel.isEmpty() || keys.contains(pin.channel)) {
			continue;
		}
		keys.append(pin.channel);
	}
	return keys;
}

/**
	@brief CatalogPart::pinsInChannel
	@param channel
	@return every pin of @a channel, in pin order. An empty @a channel
	matches nothing: a pin with no channel is not a point.
*/
QList<CatalogPin> CatalogPart::pinsInChannel(const QString &channel) const
{
	QList<CatalogPin> found;
	if (channel.isEmpty()) {
		return found;
	}
	for (const CatalogPin &pin : pins)
	{
		if (pin.channel == channel) {
			found.append(pin);
		}
	}
	return found;
}

/**
	@brief CatalogPart::contactCount
	@param role
	@return how many contacts of @a role the part has
*/
int CatalogPart::contactCount(CatalogPinRole role) const
{
	QSet<QString> pairs;
	int unpaired = 0;

	for (const CatalogPin &pin : pins)
	{
		if (pin.role != role) {
			continue;
		}
		if (pin.pair.isEmpty()) {
			++unpaired;
		} else {
			pairs.insert(pin.pair);
		}
	}

	return pairs.size() + unpaired;
}
