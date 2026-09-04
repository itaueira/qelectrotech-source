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
#ifndef CABLEWIRE_H
#define CABLEWIRE_H

#include "../properties/propertiesinterface.h"

#include <QPointer>
#include <QString>
#include <QUuid>

class Conductor;

/**
	@brief One wire of a cable: what it is, and which conductor it carries.

	A plain value, held by its Cable in a vector and copied out of it. There
	is no shared pointer and no back reference on purpose. RealTerminal
	needs both because a physical terminal groups several of them in levels
	and a bridge crosses them, so a member has an identity of its own that
	several owners point at; a wire of a cable belongs to exactly one cable,
	is never grouped and is never shared.

	The conductor is held by uuid and nothing else. The pointer beside it is
	a cache the resolver fills - Cable::resolve() - and is never what the
	wire is; it is not written to the file and it is null until somebody
	resolves. This is the same shape RealTerminal writes
	(realterminal.cpp, element_uuid) and the same shape IoWiring uses for
	the terminal each of its identities stands for.

	The three states are the whole point of the class:

	- @ref Reserved is a wire that names no conductor. It is a spare: it
	  exists in the cable without existing on any folio, which is what
	  RealTerminal already documents for a reserved terminal.
	- @ref Occupied is a wire whose conductor was found.
	- @ref Orphan is a wire that names a conductor and did not find it. It
	  is kept, not dropped, and it is *not* the same thing as Reserved -
	  telling them apart is the difference between "the designer left this
	  one empty" and "this one lost its wire and nobody was told".

	A wire whose uuid was set but which has not been resolved yet reads as
	Orphan, not Occupied. The safe direction is the honest one: the wire
	says it does not have its conductor until something proves it does.
*/
class CableWire : public PropertiesInterface
{
	public:
		/**
			@brief What this wire has to say about its conductor.
		*/
		enum State
		{
			Reserved,   ///< names no conductor: a spare, by the designer's choice
			Occupied,   ///< the conductor it names was found in the project
			Orphan      ///< names a conductor that is not there: a marked spare
		};

		CableWire();
		explicit CableWire(const QString &number);

		void toSettings(QSettings &/*settings*/, const QString = QString()) const override {}
		void fromSettings(const QSettings &/*settings*/, const QString = QString()) override {}

		QDomElement toXml(QDomDocument &xml_document) const override;
		bool fromXml(const QDomElement &xml_element) override;

		static QString xmlTagName() {return QStringLiteral("cable_wire");}

		QString number() const {return m_number;}
		void setNumber(const QString &number) {m_number = number;}

		/**
			The colour as a cable list prints it - a name or an IEC 60757
			code such as BK or GNYE - and not a QColor. Text, because that
			is what ConductorProperties already stores for the same thing
			(m_wire_color, written as the conductor_color attribute), and a
			cable list has to print the code the drawing shows.
		*/
		QString color() const {return m_color;}
		void setColor(const QString &color) {m_color = color;}

		/// The cross section, as text, like ConductorProperties::m_wire_section
		QString section() const {return m_section;}
		void setSection(const QString &section) {m_section = section;}

		/// What the wire is for, as text, like ConductorProperties::m_function
		QString function() const {return m_function;}
		void setFunction(const QString &function) {m_function = function;}

		QUuid conductorUuid() const {return m_conductor_uuid;}

		/**
			@brief Say which conductor this wire carries, by identity alone.

			The state becomes Orphan for a uuid that is not null, because
			nothing has proved yet that such a conductor exists; a resolver
			promotes it to Occupied. A null uuid frees the wire and makes
			it a spare.
		*/
		void setConductorUuid(const QUuid &uuid);

		/**
			@brief Say which conductor this wire carries, having it at hand.

			Takes the identity from @a conductor and keeps the pointer as
			the resolved one, so the wire is Occupied straight away.
			A null @a conductor frees the wire: it becomes a spare, with no
			uuid left behind.
		*/
		void setConductor(Conductor *conductor);

		/// @return the conductor, or nullptr while the wire is not Occupied
		Conductor *conductor() const;

		State state() const {return m_state;}
		bool isReserved() const {return m_state == Reserved;}
		bool isOccupied() const {return m_state == Occupied;}
		/// @return true for a wire that names a conductor which is not there
		bool hasLostConductor() const {return m_state == Orphan;}

		/**
			@brief Mark this wire as having lost its conductor.

			Keeps the uuid, so that the wire can be linked again the day
			the conductor comes back - an undone deletion, for instance.
			Called by Cable::resolve(); there is no reason for anything
			else to call it.
		*/
		void setConductorLost();

	private:
		QString m_number,
			m_color,
			m_section,
			m_function;
		QUuid m_conductor_uuid;
		State m_state = Reserved;
			/// Cache filled by Cable::resolve(); never the identity, never written
		QPointer<Conductor> m_conductor;
};

#endif // CABLEWIRE_H
