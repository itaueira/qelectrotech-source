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
#ifndef CATALOGPART_H
#define CATALOGPART_H

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief What a pin of a catalog part is for.
	The role is what lets a rule check count free contacts, warn that a
	project uses more contacts than the part has, and tell power apart from
	control. A symbol declares the same thing on its terminal pairs (T35),
	the part carries the real numbers of the manufacturer.

	Serialised by name and never by number, so a new value never
	reinterprets an old catalog. New values still go at the end: allRoles()
	is what fills the combo box of the part editor, and its order is what
	the user sees.
*/
enum class CatalogPinRole
{
	Unknown,        ///< role not declared
	Terminal,       ///< a plain connection point
	Coil,           ///< a coil terminal
	ContactNo,      ///< normally open control contact
	ContactNc,      ///< normally closed control contact
	PowerContactNo, ///< normally open power pole
	Input,          ///< input of a PLC, drive or controller
	Output,         ///< output of a PLC, drive or controller
	InputAnalog,    ///< analog input, a value read from the field
	OutputAnalog,   ///< analog output, a value driven to the field
	OutputRelay,    ///< output through a dry relay contact
	CommPort,       ///< a communication port, serial or field bus
	SupplyCommon,   ///< the common of the supply of a group of points
	ReturnCommon    ///< the common the field wiring returns to
};

/**
	@brief The CatalogPin class
	One pin of a catalog part, with the number the manufacturer prints on the
	product. Assigning a part to a component replaces the provisional labels
	of the symbol by these.
*/
class CatalogPin
{
	public:
		CatalogPin() {}
		CatalogPin(const QString &label, CatalogPinRole role) :
			label(label), role(role) {}

		QString label;      ///< the real pin number, "A1", "13", "L1"
		CatalogPinRole role = CatalogPinRole::Unknown;
		QString pair;       ///< pins sharing a non empty pair form one contact
		QString group;      ///< which sub symbol of the part the pin belongs to
		QString secondary_label; ///< what is printed beside the number, "STOP", "COM"
		/**
			The input or output point this pin belongs to. Pins sharing a
			non empty channel are one point of the field: a two wire input
			is one channel with two pins, its Input and its ReturnCommon.

			A field of its own and not a second use of pair, because a pair
			is two pins of the same role and a channel is neither limited to
			two nor to one role. What is typed here is the key the
			manufacturer sheet uses; the number of the channel is not typed,
			it comes from the order of the points in the part.
		*/
		QString channel;
		QString connector;  ///< the connector the pin sits on, "X1", "CN2"
		int order_index = 0;

		static QString roleToString(CatalogPinRole role);
		static CatalogPinRole roleFromString(const QString &string);
		static QString translatedRoleName(CatalogPinRole role);
		static QList<CatalogPinRole> allRoles();
		/**
			@param role
			@return true when @a role is a point of the field, that is what
			a channel groups: the four input and output roles and the relay
			output. A coil, a contact or a communication port is not.
		*/
		static bool isIoRole(CatalogPinRole role);
};

/**
	@brief The CatalogAccessory class
	An accessory a catalog part brings along. Saving a part together with its
	accessories records the set: next time that part is assigned, the
	accessories come with it. A fuse inside a fuse holder is the case that
	repeats in every cabinet.
*/
class CatalogAccessory
{
	public:
		CatalogAccessory() {}
		CatalogAccessory(const QString &code, double quantity) :
			code(code), quantity(quantity) {}

		QString code;            ///< part code of the accessory
		double quantity = 1.0;
};

/**
	@brief The CatalogPart class
	A part is a real product bought from a manufacturer: a code, the values
	of the properties of its class, the real numbers of its pins, its
	physical view and its accessories.

	A part is not a symbol and not a component. The same contactor symbol
	serves twenty different contactors; the tag comes from the class; the pin
	numbers come from the part.

	The part code is the only mandatory field.
*/
class CatalogPart
{
	public:
		CatalogPart();
		CatalogPart(const QString &code, int class_id);

		bool isNull() const;
		bool isValid(QString *error = nullptr) const;

		QString value(const QString &key) const;
		bool hasValue(const QString &key) const;
		void setValue(const QString &key, const QString &value);
		void clearValue(const QString &key);

		QList<CatalogPin> pinsWithRole(CatalogPinRole role) const;
		QStringList pinLabels() const;
		/**
			@return the channel of every point of the part, once each and in
			pin order. This is the list a generated block walks: one entry
			is one input or output, however many pins it takes.
		*/
		QStringList channelKeys() const;
		/**
			@param channel
			@return every pin of @a channel, in pin order
		*/
		QList<CatalogPin> pinsInChannel(const QString &channel) const;
		/**
			@return how many contacts of @a role the part has. A contact
			is a pair of pins, so two pins sharing a pair name count as
			one. Pins with a role but no pair name count as one each,
			which is what a part sheet that only lists "2 NO" gives.
		*/
		int contactCount(CatalogPinRole role) const;

	public:
		int id = 0;                ///< database identifier, 0 when not saved yet
		int class_id = 0;
		QString code;              ///< part code, the only mandatory field
		int revision = 1;
		bool is_current = true;    ///< false once a newer revision exists
		QString origin;            ///< where the data came from
		QString origin_date;       ///< when it came, ISO 8601
		QHash<QString, QString> values;
		QList<CatalogPin> pins;
		QList<CatalogAccessory> accessories;
};

#endif // CATALOGPART_H
