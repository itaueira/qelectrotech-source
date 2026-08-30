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
#ifndef IOCOMMON_H
#define IOCOMMON_H

#include "../catalog/catalogpart.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief Wiring the commons of a card to the two default bars.

	A digital card does not only carry channels: it carries the commons
	that feed them, and the commons the field wiring returns to. Eight
	cards is sixteen of those, and drawing them one by one is half an hour
	of work that says nothing about the machine being drawn. This class is
	the arithmetic of doing it in one gesture (CU-11.8).

	It answers one question and no other: given the commons of the cards
	asked for, and the elements the drawing marks as a default bar, which
	common goes to which bar, and which one goes nowhere and why. No
	project, no folio, no element, no conductor - plain values only, so
	the whole of it stands on a bench.

	How a connection point comes to be called a common is decided before
	this class, and by two sources in this order: the terminal itself,
	which carries the role its symbol declared (T35), and the part the
	component was assigned (T13), whose pins say the same thing for a
	symbol that declares nothing. busesOfPart() is that second source, and
	it matches pin to terminal through CatalogAssignment::terminalPins(),
	so that pin and terminal are paired the same way everywhere in the
	program.

	What is deliberately not a source is the name. A terminal called COM,
	0V or M is a common nearly always, and nearly always is not enough
	here: a wrong wire drawn sixteen times over costs more than a refusal
	that names itself. A point the catalogue does not call a common stays
	out, and says so.

	A conductor joins two terminals of the same folio
	(sources/qetgraphicsitem/conductor.h:56), so "the default bar" cannot
	be one element for a whole project. busFor() resolves it per folio,
	and prefers a bar that was marked over one that only carries the same
	label - the caller hands over both, because a folio drawn to IEC
	repeats the same rail from folio to folio under the same name.
*/
class IoCommon
{
	Q_DECLARE_TR_FUNCTIONS(IoCommon)

	public:
		/// which of the two default bars something belongs on
		enum BusKind {
			NoBus,
			/// the bar feeding the commons of a group of points
			SupplyBus,
			/// the bar the field wiring returns to
			ReturnBus
		};

		/// why one common stays unwired
		enum Refusal {
			NoRefusal,
			/// the catalogue does not call this point a common
			NotACommon,
			/// it already carries a conductor, so it is already wired
			AlreadyWired,
			/// no bar of its kind is reachable on the folio of the card
			NoBusOnFolio,
			/// the bar is there, and has no terminal to take the wire
			NoTerminalOnBus
		};

		/**
			@brief One connection point of a card that is a common.
		*/
		struct Common
		{
			Common() {}

			/// what never changes about the terminal
			QString id;
			/// the number its symbol or its part gives it
			QString label;
			/// which of the two bars it belongs on
			BusKind bus = NoBus;
			/// true when it already carries a conductor
			bool wired = false;
			/// how the card names itself, for the messages
			QString card_label;
			/// which folio the card is drawn on
			int folio = -1;
		};

		/**
			@brief One element that can stand as a default bar.
		*/
		struct Bus
		{
			Bus() {}

			/// what never changes about the element
			QString id;
			/// how it names itself, which is what a folio repeats
			QString label;
			/// which of the two bars it is
			BusKind kind = NoBus;
			/// which folio it is drawn on
			int folio = -1;
			/// true when the drawing marks it, false when it only
			/// carries the label of a bar marked somewhere else
			bool marked = false;
			/// how many terminals it has at all
			int terminals = 0;
			/// and how many of those carry no conductor yet
			int free_terminals = 0;
		};

		/**
			@brief One common, and the bar it is going to reach.
		*/
		struct Link
		{
			Link() {}

			QString common_id;
			QString common_label;
			QString bus_id;
			QString bus_label;
			BusKind bus = NoBus;
			/// the folio both of them are drawn on
			int folio = -1;
		};

		/**
			@brief One common that stays unwired, and why.
		*/
		struct Rejected
		{
			Rejected() {}
			Rejected(const QString &id, const QString &name,
				 Refusal why) :
				common_id(id),
				label(name),
				reason(why) {}

			QString common_id;
			/// how the card names it, so the message can say it
			QString label;
			Refusal reason = NoRefusal;
		};

		/**
			@brief What a wiring would draw, before it draws it.
		*/
		struct Plan
		{
			/// the commons that get a wire, in the order they came
			QList<Link> links;
			/// and the ones that do not, each with its reason
			QList<Rejected> rejected;

			bool isEmpty() const {return links.isEmpty();}
			/// @return true when every common asked for gets a wire
			bool isClean() const {return rejected.isEmpty();}
			/// @return how many wires reach a bar of @a kind
			int wires(BusKind kind) const;
			/// @return the bars this plan lands on, each once
			QStringList busLabels() const;
			/// @return the whole of it said in one paragraph
			QString text() const;
		};

		/// @return what goes in the plc_bus element information
		static QString busToString(BusKind bus);
		/// @return the bar @a value names, NoBus when it names none
		static BusKind busFromString(const QString &value);
		/// @return the name of @a bus, for a message or a menu
		static QString busName(BusKind bus);

		/// @return the bar a catalogue role belongs on
		static BusKind busOfRole(CatalogPinRole role);

		/**
			@brief Which terminals of an element are commons,
			according to the part assigned to it.
			@param part
			@param group : which sub symbol of the part it draws
			@param terminal_count : how many terminals it has
			@return one bar kind per terminal, in terminal order,
			NoBus where the part says the terminal is something else
		*/
		static QList<BusKind> busesOfPart(const CatalogPart &part,
						  const QString &group,
						  int terminal_count);

		/**
			@brief Which bar of @a kind a card on @a folio reaches.
			@param buses : every candidate of the project
			@param kind
			@param folio
			@return the bar, or a default built Bus when there is none
		*/
		static Bus busFor(const QList<Bus> &buses, BusKind kind,
				  int folio);

		/**
			@brief Work out what @a commons would be wired to.
			@param commons : the commons of the cards asked for
			@param buses : every candidate bar of the project
			@return the wires, and the commons that stay unwired
		*/
		static Plan plan(const QList<Common> &commons,
				 const QList<Bus> &buses);

		/// @return @a reason said out loud, about @a label
		static QString refusalText(Refusal reason, const QString &label);
};

#endif // IOCOMMON_H
