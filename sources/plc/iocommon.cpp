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
#include "iocommon.h"

#include "../catalog/catalogassignment.h"

/// how many refused commons a paragraph names before it sums the rest up
static const int MAX_SAID_REFUSED = 6;
/// what the plc_bus element information holds for each of the two bars
static const char SUPPLY_VALUE[] = "supply";
static const char RETURN_VALUE[] = "return";

/**
	@brief Which of two candidate bars a common should rather reach.

	Marked before unmarked, because a drawing that says which bar it means
	has said it. Then the one that still has a free terminal, and among
	those the one with the most of them, so that eight commons do not all
	pile onto the same point. Ties go to the smaller id, so that the same
	plan run twice draws the same wires.

	@param candidate
	@param best : the best found so far
	@return true when @a candidate should replace @a best
*/
static bool betterBus(const IoCommon::Bus &candidate, const IoCommon::Bus &best)
{
	if (candidate.marked != best.marked) {
		return candidate.marked;
	}
	if (candidate.free_terminals != best.free_terminals) {
		return candidate.free_terminals > best.free_terminals;
	}
	return candidate.id < best.id;
}

/**
	@brief IoCommon::Plan::wires
	@param kind
	@return how many wires of this plan reach a bar of @a kind
*/
int IoCommon::Plan::wires(BusKind kind) const
{
	int count = 0;
	for (const Link &link : links)
	{
		if (link.bus == kind) {
			++count;
		}
	}
	return count;
}

/**
	@brief IoCommon::Plan::busLabels
	@return the bars this plan lands on, each one named once
*/
QStringList IoCommon::Plan::busLabels() const
{
	QStringList labels;
	for (const Link &link : links)
	{
		if (!link.bus_label.isEmpty() && !labels.contains(link.bus_label)) {
			labels.append(link.bus_label);
		}
	}
	return labels;
}

/**
	@brief IoCommon::Plan::text
	@return the whole of the plan said in one paragraph, refusals included
*/
QString IoCommon::Plan::text() const
{
	QStringList lines;

	if (links.isEmpty()) {
		lines << tr("Aucun commun ne sera raccordé.");
	}
	else
	{
		lines << tr("%1 commun(s) seront raccordés : %2 sur l'alimentation, "
			    "%3 sur le retour.")
			 .arg(int(links.count()))
			 .arg(wires(SupplyBus))
			 .arg(wires(ReturnBus));

		const QStringList labels = busLabels();
		if (!labels.isEmpty()) {
			lines << tr("Barres utilisées : %1.")
				 .arg(labels.join(QStringLiteral(", ")));
		}
	}

	if (!rejected.isEmpty())
	{
		lines << tr("%1 commun(s) ne seront pas raccordés :")
			 .arg(int(rejected.count()));

		int said = 0;
		for (const Rejected &refused : rejected)
		{
			if (said >= MAX_SAID_REFUSED) {
				lines << tr("  et %1 autre(s).")
					 .arg(int(rejected.count()) - said);
				break;
			}
			lines << QStringLiteral("  ")
				 + refusalText(refused.reason, refused.label);
			++said;
		}
	}

	return lines.join(QLatin1Char('\n'));
}

/**
	@brief IoCommon::busToString
	@param bus
	@return what the plc_bus element information holds for @a bus
*/
QString IoCommon::busToString(BusKind bus)
{
	switch (bus)
	{
		case SupplyBus: return QString::fromLatin1(SUPPLY_VALUE);
		case ReturnBus: return QString::fromLatin1(RETURN_VALUE);
		case NoBus: break;
	}
	return QString();
}

/**
	@brief IoCommon::busFromString
	@param value
	@return the bar @a value names, NoBus when it names none
*/
IoCommon::BusKind IoCommon::busFromString(const QString &value)
{
	const QString said = value.trimmed().toLower();
	if (said == QLatin1String(SUPPLY_VALUE)) {
		return SupplyBus;
	}
	if (said == QLatin1String(RETURN_VALUE)) {
		return ReturnBus;
	}
	return NoBus;
}

/**
	@brief IoCommon::busName
	@param bus
	@return the name of @a bus, for a message or a menu
*/
QString IoCommon::busName(BusKind bus)
{
	switch (bus)
	{
		case SupplyBus: return tr("Alimentation");
		case ReturnBus: return tr("Retour");
		case NoBus: break;
	}
	return QString();
}

/**
	@brief IoCommon::busOfRole
	@param role
	@return the bar a catalogue role belongs on, NoBus for every other role
*/
IoCommon::BusKind IoCommon::busOfRole(CatalogPinRole role)
{
	if (role == CatalogPinRole::SupplyCommon) {
		return SupplyBus;
	}
	if (role == CatalogPinRole::ReturnCommon) {
		return ReturnBus;
	}
	return NoBus;
}

/**
	@brief IoCommon::busesOfPart
	@param part
	@param group
	@param terminal_count
	@return one bar kind per terminal, in terminal order
*/
QList<IoCommon::BusKind> IoCommon::busesOfPart(const CatalogPart &part,
					       const QString &group,
					       int terminal_count)
{
	QList<BusKind> buses;
	const QList<CatalogPin> pins =
			CatalogAssignment::terminalPins(part, group,
							terminal_count);
	for (const CatalogPin &pin : pins) {
		buses.append(busOfRole(pin.role));
	}
	return buses;
}

/**
	@brief IoCommon::busFor
	@param buses
	@param kind
	@param folio
	@return the bar of @a kind a card on @a folio reaches, a default built
	Bus when the folio has none
*/
IoCommon::Bus IoCommon::busFor(const QList<Bus> &buses, BusKind kind,
			       int folio)
{
	if (kind == NoBus) {
		return Bus();
	}

	Bus best;
	bool found = false;
	for (const Bus &bus : buses)
	{
		if (bus.kind != kind || bus.folio != folio) {
			continue;
		}
		if (!found || betterBus(bus, best))
		{
			best = bus;
			found = true;
		}
	}
	return found ? best : Bus();
}

/**
	@brief IoCommon::plan
	@param commons
	@param buses
	@return what a wiring would draw, and what it would leave alone
*/
IoCommon::Plan IoCommon::plan(const QList<Common> &commons,
			      const QList<Bus> &buses)
{
	Plan result;
	for (const Common &common : commons)
	{
		// The catalogue says nothing about this point, so nothing is
		// drawn. Guessing from the name would wire a whole batch wrong.
		if (common.bus == NoBus) {
			result.rejected.append(Rejected(common.id, common.label,
							NotACommon));
			continue;
		}

		// Running the action twice must not double the wires, which is
		// also what lets one mark a bar late and run it again.
		if (common.wired) {
			result.rejected.append(Rejected(common.id, common.label,
							AlreadyWired));
			continue;
		}

		const Bus bus = busFor(buses, common.bus, common.folio);
		if (bus.kind == NoBus) {
			result.rejected.append(Rejected(common.id, common.label,
							NoBusOnFolio));
			continue;
		}
		if (bus.terminals <= 0) {
			result.rejected.append(Rejected(common.id, common.label,
							NoTerminalOnBus));
			continue;
		}

		Link link;
		link.common_id = common.id;
		link.common_label = common.label;
		link.bus_id = bus.id;
		link.bus_label = bus.label;
		link.bus = common.bus;
		link.folio = common.folio;
		result.links.append(link);
	}
	return result;
}

/**
	@brief IoCommon::refusalText
	@param reason
	@param label
	@return @a reason said out loud, about @a label
*/
QString IoCommon::refusalText(Refusal reason, const QString &label)
{
	switch (reason)
	{
		case NotACommon:
			return tr("%1 : la pièce affectée ne dit pas que cette "
				  "borne est un commun.").arg(label);
		case AlreadyWired:
			return tr("%1 : déjà raccordé.").arg(label);
		case NoBusOnFolio:
			return tr("%1 : aucune barre par défaut sur ce folio.")
					.arg(label);
		case NoTerminalOnBus:
			return tr("%1 : la barre par défaut n'a aucune borne.")
					.arg(label);
		case NoRefusal:
			break;
	}
	return QString();
}
