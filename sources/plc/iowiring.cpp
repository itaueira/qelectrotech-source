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
#include "iowiring.h"

#include "../ElementsCollection/elementslocation.h"
#include "../catalog/catalog.h"
#include "../catalog/catalogassignment.h"
#include "../diagram.h"
#include "../diagramcontext.h"
#include "../qetapp.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "../utils/conductorcreator.h"

#include <QLineF>
#include <QPointF>
#include <QSet>
#include <QUndoStack>
#include <QUuid>

namespace
{
	/// One element of the project, and the folio it is drawn on.
	struct Candidate
	{
		Element *element = nullptr;
		int folio = -1;
	};

	/**
		@brief Every element of the project, each with its folio.
		@param project
		@return the elements, folio by folio
	*/
	QList<Candidate> everyElement(QETProject *project)
	{
		QList<Candidate> found;
		if (!project) {
			return found;
		}

		const QList<Diagram *> diagrams = project->diagrams();
		for (Diagram *diagram : diagrams)
		{
			if (!diagram) {
				continue;
			}

			const int folio = diagram->folioIndex();
			const QList<Element *> elements = diagram->elements();
			for (Element *element : elements)
			{
				if (!element) {
					continue;
				}

				Candidate one;
				one.element = element;
				one.folio = folio;
				found.append(one);
			}
		}
		return found;
	}

	/**
		@brief The element informations of @a element as plain strings,
		which is what the catalogue matches a part against.
		@param element
		@return the informations, key by key
	*/
	QHash<QString, QString> informationOf(const Element *element)
	{
		QHash<QString, QString> values;
		if (!element) {
			return values;
		}

		const DiagramContext context = element->elementInformations();
		const QList<QString> keys = context.keys();
		for (const QString &key : keys) {
			values.insert(key, context.value(key).toString());
		}
		return values;
	}

	/**
		@brief An identity for one terminal, unique in the project.

		Not Terminal::stableUuid(): when the symbol declares no uuid of
		its own, that one is derived from where the terminal sits inside
		the element, so every instance of the same symbol repeats it.
		The element's uuid is made per instance
		(sources/qetgraphicsitem/element.cpp:133), so this one is unique
		wherever the symbol is drawn twice.

		@param element
		@param index : which terminal of it, in terminals() order
		@return the identity
	*/
	QString terminalId(Element *element, int index)
	{
		return element->uuid().toString() + QLatin1Char('#')
				+ QString::number(index);
	}

	/**
		@brief How a message names one terminal of one card.
		@param card : how the card names itself
		@param name : how the terminal names itself
		@return the two of them, or whichever there is
	*/
	QString terminalLabel(const QString &card, const QString &name)
	{
		if (card.isEmpty()) {
			return name;
		}
		if (name.isEmpty()) {
			return card;
		}
		return card + QLatin1Char(':') + name;
	}
}

/**
	@brief IoWiring::Report::text
	@return what was drawn said in one paragraph, problems included
*/
QString IoWiring::Report::text() const
{
	QStringList lines;

	if (wired > 0)
	{
		lines << tr("%1 commun(s) raccordé(s) sur %2 folio(s).")
			 .arg(wired)
			 .arg(folios);
	}
	else {
		lines << tr("Aucun commun n'a été raccordé.");
	}

	lines << problems;

	return lines.join(QLatin1Char('\n'));
}

/**
	@brief IoWiring::IoWiring
	@param project : the project whose commons are to be wired
*/
IoWiring::IoWiring(QETProject *project) :
	m_project(project)
{}

/**
	@brief IoWiring::busOf
	@param element
	@return which default bar the drawing marks @a element as, NoBus when
	it marks it as none
*/
IoCommon::BusKind IoWiring::busOf(const Element *element)
{
	if (!element) {
		return IoCommon::NoBus;
	}

	const DiagramContext information = element->elementInformations();
	return IoCommon::busFromString(
			information.value(QETInformation::ELMT_PLC_BUS).toString());
}

/**
	@brief IoWiring::labelOf
	@param element
	@return the label the folio shows, the name of its symbol failing that
*/
QString IoWiring::labelOf(Element *element)
{
	if (!element) {
		return QString();
	}

	const QString label = element->actualLabel();
	return label.isEmpty() ? element->name() : label;
}

/**
	@brief IoWiring::barOf
	@param element
	@param kind
	@param folio
	@param marked : true when the drawing marks it, false when it only
	carries the label of a bar marked on another folio
	@return @a element described as a candidate bar, and remembered
*/
IoCommon::Bus IoWiring::barOf(Element *element, IoCommon::BusKind kind,
			      int folio, bool marked)
{
	IoCommon::Bus bus;
	bus.id = element->uuid().toString();
	bus.label = labelOf(element);
	bus.kind = kind;
	bus.folio = folio;
	bus.marked = marked;

	const QList<Terminal *> terminals = element->terminals();
	bus.terminals = int(terminals.count());
	for (Terminal *terminal : terminals)
	{
		if (terminal && terminal->conductorsCount() == 0) {
			++bus.free_terminals;
		}
	}

	m_bars.insert(bus.id, element);
	return bus;
}

/**
	@brief IoWiring::buses
	@return every element that can stand as a default bar
*/
QList<IoCommon::Bus> IoWiring::buses()
{
	QList<IoCommon::Bus> found;
	m_bars.clear();

	const QList<Candidate> all = everyElement(m_project);

	// What the drawing itself marks. A marked bar also lends its label to
	// the other folios, which is the second pass.
	QHash<QString, IoCommon::BusKind> marked_labels;
	for (const Candidate &one : all)
	{
		const IoCommon::BusKind kind = busOf(one.element);
		if (kind == IoCommon::NoBus) {
			continue;
		}

		found.append(barOf(one.element, kind, one.folio, true));

		const QString label = labelOf(one.element);
		if (!label.isEmpty()) {
			marked_labels.insert(label, kind);
		}
	}

	if (marked_labels.isEmpty()) {
		return found;
	}

	// And the folios that repeat it: a drawing made to IEC carries the same
	// rail from folio to folio under the same name, and marking every one
	// of them by hand is the work this step exists to save.
	for (const Candidate &one : all)
	{
		if (busOf(one.element) != IoCommon::NoBus) {
			continue;
		}

		const QString label = labelOf(one.element);
		if (label.isEmpty() || !marked_labels.contains(label)) {
			continue;
		}

		found.append(barOf(one.element, marked_labels.value(label),
				   one.folio, false));
	}

	return found;
}

/**
	@brief IoWiring::commons
	@param cards
	@param problems : gets one line per card that declares no common at all
	@return the commons of @a cards, in card then terminal order
*/
QList<IoCommon::Common> IoWiring::commons(const QList<Element *> &cards,
					  QStringList *problems)
{
	QList<IoCommon::Common> found;
	m_terminals.clear();

	Catalog *catalog = QETApp::catalog();

	for (Element *card : cards)
	{
		if (!card) {
			continue;
		}

		const QList<Terminal *> terminals = card->terminals();
		const int count = int(terminals.count());
		if (count <= 0) {
			continue;
		}

		// The second source, read once per card: the pins of the part
		// the component was assigned. It is not a spare - no symbol of
		// the installed base declares a role yet - and it is matched to
		// the terminals the same way the assignment itself matches them.
		QList<IoCommon::BusKind> from_part;
		if (catalog)
		{
			const CatalogPart part = CatalogAssignment::partFromValues(
					*catalog, informationOf(card));
			if (!part.isNull()) {
				from_part = IoCommon::busesOfPart(
						part, card->location().path(),
						count);
			}
		}

		const Diagram *diagram = card->diagram();
		const int folio = diagram ? diagram->folioIndex() : -1;
		const QString card_label = labelOf(card);
		int mine = 0;

		for (int index = 0 ; index < count ; ++index)
		{
			Terminal *terminal = terminals.at(index);
			if (!terminal) {
				continue;
			}

			// The first source is the terminal itself (T35), and the
			// part answers only where the symbol said nothing.
			IoCommon::BusKind kind =
					IoCommon::busOfRole(terminal->role());
			if (kind == IoCommon::NoBus
			    && index < int(from_part.count())) {
				kind = from_part.at(index);
			}

			// A channel is not a refusal. Only what the catalogue
			// does call a common comes out of here, so that the
			// list the user reads is the list of its own commons.
			if (kind == IoCommon::NoBus) {
				continue;
			}

			IoCommon::Common common;
			common.id = terminalId(card, index);
			common.label = terminalLabel(card_label, terminal->name());
			common.bus = kind;
			common.wired = terminal->conductorsCount() > 0;
			common.card_label = card_label;
			common.folio = folio;

			found.append(common);
			m_terminals.insert(common.id, terminal);
			++mine;
		}

		if (mine == 0 && problems)
		{
			*problems << tr("%1 : aucune borne n'est déclarée comme "
					"commun, ni par le symbole ni par la "
					"pièce affectée.")
				     .arg(card_label);
		}
	}

	return found;
}

/**
	@brief IoWiring::plan
	@param cards
	@param problems : gets one line per card that declares no common at all
	@return what wiring @a cards would draw, and what it would leave alone
*/
IoCommon::Plan IoWiring::plan(const QList<Element *> &cards,
			      QStringList *problems)
{
	const QList<IoCommon::Bus> bars = buses();
	const QList<IoCommon::Common> found = commons(cards, problems);
	return IoCommon::plan(found, bars);
}

/**
	@brief Which terminal of a bar a common should reach.

	A wired one before a free one, and that is not a detail: an element's
	terminals are not joined to each other, so a wire landing on a terminal
	that already carries the bar's potential adopts its properties and its
	number, and a wire landing on a free one opens a potential of its own.
	Eight commons of a folio have to come out on the same potential, which
	is the whole meaning of "the bar". Within a tier, the nearest one, so
	that the folio does not fill up with wires crossing each other.

	@param bar
	@param common : the terminal being wired
	@return the terminal to draw to, nullptr when the bar has none
*/
Terminal *IoWiring::barTerminalFor(Element *bar, Terminal *common)
{
	if (!bar || !common) {
		return nullptr;
	}

	Terminal *best = nullptr;
	bool best_wired = false;
	qreal best_distance = 0.0;

	const QPointF from = common->scenePos();
	const QList<Terminal *> terminals = bar->terminals();
	for (Terminal *terminal : terminals)
	{
		if (!terminal || terminal == common) {
			continue;
		}

		const bool wired = terminal->conductorsCount() > 0;
		const qreal distance =
				QLineF(from, terminal->scenePos()).length();

		if (!best
		    || (wired && !best_wired)
		    || (wired == best_wired && distance < best_distance))
		{
			best = terminal;
			best_wired = wired;
			best_distance = distance;
		}
	}

	return best;
}

/**
	@brief IoWiring::wire
	@param plan : as returned by plan(), on this same object
	@return what was drawn, and what got in the way
*/
IoWiring::Report IoWiring::wire(const IoCommon::Plan &plan)
{
	Report report;

	if (!m_project)
	{
		report.problems << tr("Aucun projet ouvert.");
		return report;
	}
	if (plan.links.isEmpty()) {
		return report;
	}

	QUndoStack *stack = m_project->undoStack();
	stack->beginMacro(tr("raccorder %1 commun(s)")
			  .arg(int(plan.links.count())));

	QSet<int> folios;

	for (const IoCommon::Link &link : plan.links)
	{
		Terminal *common = m_terminals.value(link.common_id);
		Element *bar = m_bars.value(link.bus_id);
		if (!common || !bar)
		{
			report.problems << tr("%1 : la borne ou la barre "
					      "n'existe plus.")
					   .arg(link.common_label);
			continue;
		}

		Terminal *target = barTerminalFor(bar, common);
		Diagram *diagram = common->diagram();
		if (!target || !diagram || diagram != target->diagram())
		{
			report.problems << tr("%1 : la barre %2 n'offre aucune "
					      "borne sur ce folio.")
					   .arg(link.common_label)
					   .arg(link.bus_label);
			continue;
		}

		// The constructor is the action: it adopts the properties and
		// the wire number of the potential the bar already carries, and
		// stacks one AddGraphicsObjectCommand inside its own macro,
		// which this one nests around
		// (sources/utils/conductorcreator.cpp:38).
		const QList<Terminal *> pair {common, target};
		ConductorCreator creator(diagram, pair);
		Q_UNUSED(creator)

		folios.insert(link.folio);
		++report.wired;
	}

	stack->endMacro();
	report.folios = int(folios.count());

	return report;
}
