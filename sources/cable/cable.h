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
#ifndef CABLE_H
#define CABLE_H

#include "cablewire.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUuid>
#include <QVector>

class Conductor;
class Diagram;
class QETProject;
class CableReport;

/**
	@brief A cable of the project: what it is, and which wire carries what.

	This class holds the data and the configuration of a cable but not the
	visual aspect, the same division TerminalStrip states for a terminal
	strip. It belongs to the project, not to a folio, which is what lets one
	cable enter on folio 3 and leave on folio 7 and still be one cable with
	one line in a cable list - the wires it carries are conductors of
	different folios, and the cable never notices.

	The mould is deliberately mixed, and it is worth saying which half came
	from where.

	From TerminalStrip: a data object owned by the project through QObject
	parenting, an xmlTagName() of its own, toXml()/fromXml() of its own, and
	a block written on the root of the .qet next to <terminal_strips>.

	From IoWiring: everything below the cable. A wire is a value in a
	vector, not a QSharedPointer with a back reference. The weight of
	TerminalStrip is that graph - RealTerminal, PhysicalTerminal, the levels
	and the bridges - and it exists because a member of a strip has an
	identity several owners point at. A wire of a cable has no such need: it
	belongs to one cable, is never grouped and is never shared. Taking the
	graph would have bought nothing and would have brought along the very
	defect this class exists to avoid, which lives in it.

	That defect is in TerminalStrip::fromXml(): a member uuid that does not
	match any free terminal is dropped with no else branch, and a physical
	terminal that loses all of its members disappears. Here a wire that does
	not find its conductor is kept as a marked spare - CableWire::Orphan -
	and the fact goes into a CableReport the caller can read. See
	@ref resolve().

	The conductors are never touched. A wire points at a conductor by uuid;
	no conductor points back, and nothing here writes inside a <conductor>
	element.
*/
class Cable : public QObject
{
	Q_OBJECT

	public:
		explicit Cable(QETProject *project);
		Cable(const QString &label, QETProject *project);

		QETProject *project() const;

		/// What the drawing calls this cable, "W1" and the like
		QString label() const {return m_label;}
		void setLabel(const QString &label) {m_label = label;}

		QUuid uuid() const {return m_uuid;}

		/// The part code of the cable in the catalogue, CatalogPart::code
		QString partCode() const {return m_part_code;}
		void setPartCode(const QString &code) {m_part_code = code;}

		bool isShielded() const {return m_shielded;}
		void setShielded(bool shielded) {m_shielded = shielded;}

		/**
			@return how many wires the cable has, spares included.

			There is no second count kept beside the vector, on purpose: a
			stored figure and a vector are two truths, and they part company
			on the first edit that forgets one of them. setWireCount() is
			what keeps them the same thing by having only one of them.
		*/
		int wireCount() const {return m_wires.count();}

		/**
			@brief Give the cable @a count wires.

			Wires are added as spares, numbered on from the ones already
			there, and removed from the end. Removing a wire that carries a
			conductor only forgets the link; the conductor itself is a
			conductor of a folio and is not this class's to delete.
		*/
		void setWireCount(int count);

		QVector<CableWire> wires() const {return m_wires;}
		/// @return the wire at @a index, or a default wire when out of range
		CableWire wire(int index) const;
		/// @return the wire numbered @a number, or a default wire when there is none
		CableWire wireByNumber(const QString &number) const;
		bool setWire(int index, const CableWire &wire);
		void appendWire(const CableWire &wire);

		/// @return the wires that name a conductor which is not there
		QVector<CableWire> lostWires() const;

		/**
			@return the folios this cable runs through, once each, in the
			order of the project.

			Empty for a cable whose wires are all spares. This is what makes
			"the same cable on two folios" answerable without a window: one
			cable, two folios, one entry in a list.
		*/
		QList<Diagram *> folios() const;

		static QString xmlTagName() {return QStringLiteral("cable");}
		QDomElement toXml(QDomDocument &xml_document) const;
		bool fromXml(const QDomElement &xml_element);

		/**
			@brief Every conductor of @a project, by uuid.

			@param project
			@param report : when not null, gets the count of the conductors
			that share a uuid with another one.

			Built once and handed to resolve() for each cable, rather than
			walked again per cable: a project of fifty folios has thousands
			of conductors and a handful of cables.
		*/
		static QHash<QUuid, Conductor *> conductorsOf(const QETProject *project,
							      CableReport *report = nullptr);

		/**
			@brief Link each wire to the conductor it names.

			@param index : as returned by conductorsOf()
			@param report : when not null, gets what this cable found

			A wire that names nothing stays a spare. A wire whose conductor
			is in @a index becomes Occupied. A wire whose conductor is not
			there becomes a marked spare and gets a line in @a report - it
			is not dropped, and it does not become an ordinary spare either,
			because "the designer left this empty" and "this lost its wire"
			are not the same fact and only one of them needs telling.

			Safe to call again at any time. Nothing is remembered between
			calls: the answer is recomputed from the uuids the wires hold, so
			a conductor that comes back - an undone deletion - makes its wire
			whole again on the next call.
		*/
		void resolve(const QHash<QUuid, Conductor *> &index,
			     CableReport *report = nullptr);

		/// Convenience: build the index from project() and resolve against it
		void resolve(CableReport *report = nullptr);

	private:
		QString m_label;
		QUuid m_uuid = QUuid::createUuid();
		QString m_part_code;
		bool m_shielded = false;
		QVector<CableWire> m_wires;
		QPointer<QETProject> m_project;
};

#endif // CABLE_H
