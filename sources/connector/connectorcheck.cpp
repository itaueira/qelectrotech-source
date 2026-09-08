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
#include "connectorcheck.h"

#include "../autoNum/renumberplan.h"
#include "../catalog/catalog.h"
#include "../catalog/catalogassignment.h"
#include "../catalog/catalogclass.h"
#include "../catalog/catalogpart.h"
#include "../catalog/ui/catalogprojectactions.h"
#include "../diagram.h"
#include "../diagramcontext.h"
#include "../qetgraphicsitem/element.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "../undocommand/changeelementinformationcommand.h"

#include <algorithm>

#include <QHash>
#include <QMap>
#include <QPair>
#include <QPointer>
#include <QUndoStack>

namespace
{
	/// The catalogue class a part has to be under for its component to be a pin
	const QString connector_class = QStringLiteral("connector");
	/// The class the symbol builder writes on a symbol it made
	const QString symbol_class_key = QStringLiteral("catalog_class");

	/// What one component of the project says about connectors.
	struct PinRead
	{
		Element *element = nullptr;
		/**
			Only the folio and the position of it are filled.

			Built to be handed to Renumberer::readingOrderLessThan
			rather than to be renumbered: the report has to name a
			connector after the same pin the renumbering names it
			after, and two orderings of the same folio would be two
			answers to "which spelling wins".
		*/
		RenumberInput order;
		QString connector;
		QString key;
		QString label;
		QString part_code;
		int folio = 0;
	};

	/**
		Top to bottom, then left to right, which is the default the
		renumbering dialog offers. A person who works the other way round
		changes it there, and the only thing that moves here is which
		spelling of a doubly spelled connector names it - which the report
		says out loud anyway.
	*/
	bool byReadingOrder(const PinRead &first, const PinRead &second)
	{
		return Renumberer::readingOrderLessThan(first.order, second.order, false);
	}

	/**
		Whether a component drawing no connector name is a pin all the
		same.

		Asked of the catalogue, in two places and in this order: the class
		of the part it carries, and failing that the class its symbol was
		built with. A pin is not a kind of symbol in this program - the
		fifty seven connector drawings of the library are simple elements,
		like a lamp - so the class is the only thing that answers, and a
		catalogue that is not open answers nothing at all.
	*/
	bool isConnectorPin(const Catalog &catalog, const QString &part_code,
			    const QString &symbol_class)
	{
		if (!catalog.isOpen()) {
			return false;
		}

		if (!part_code.isEmpty())
		{
			const CatalogPart part = catalog.partByCode(part_code);
			if (!part.isNull()
					&& catalog.isDescendantOf(part.class_id, connector_class)) {
				return true;
			}
		}

		if (!symbol_class.isEmpty())
		{
			const CatalogClass built = catalog.classByKey(symbol_class);
			if (built.id > 0
					&& catalog.isDescendantOf(built.id, connector_class)) {
				return true;
			}
		}

		return false;
	}
}

/**
	@brief ConnectorCheck::DrawnConnector::describe
	@return why the ways cannot be counted, in one sentence
*/
QString ConnectorCheck::DrawnConnector::describe() const
{
	switch (state)
	{
		case Counted:
			return QString();
		case Unread:
			return tr("catalogue non lu");
		case NoPart:
			return tr("aucune de ses broches ne porte de pièce");
		case UnknownPart:
			return tr("pièce « %1 » absente du catalogue").arg(part_code);
		case NoPinout:
				//Said in full, because the number this state is
				//standing in for is the one thing a crimping guide
				//may not invent: a part nobody gave a pinout to has
				//no reserve, and a reserve of nought is a different
				//sentence that happens to look the same.
			return tr("la pièce « %1 » ne déclare aucune broche : "
				  "le nombre de voies reste inconnu").arg(part_code);
		case MixedParts:
			return tr("ses broches portent %1 pièces différentes : %2")
			       .arg(part_codes.size()).arg(part_codes.join(QStringLiteral(", ")));
	}
	return QString();
}

/**
	@brief ConnectorCheck::DrawnConnector::isCounted
	@return true when a part with a pinout is assigned
*/
bool ConnectorCheck::DrawnConnector::isCounted() const
{
	return state == Counted;
}

/**
	@brief ConnectorCheck::Report::uncountable
	@return the connectors whose ways cannot be counted, in name order
*/
QList<ConnectorCheck::DrawnConnector> ConnectorCheck::Report::uncountable() const
{
	QList<DrawnConnector> found;
	for (const DrawnConnector &connector : connectors)
	{
			//Unread is not in it, and that is the whole guard: a
			//catalogue that is down would otherwise report every
			//connector of the project as uncatalogued.
		switch (connector.state)
		{
			case DrawnConnector::NoPart:
			case DrawnConnector::UnknownPart:
			case DrawnConnector::NoPinout:
			case DrawnConnector::MixedParts:
				found << connector;
				break;
			case DrawnConnector::Counted:
			case DrawnConnector::Unread:
				break;
		}
	}
	return found;
}

/**
	@brief ConnectorCheck::Report::countedConnectors
	@return how many connectors have a part with a pinout
*/
int ConnectorCheck::Report::countedConnectors() const
{
	int counted = 0;
	for (const DrawnConnector &connector : connectors)
	{
		if (connector.isCounted()) {
			++counted;
		}
	}
	return counted;
}

/**
	@brief ConnectorCheck::Report::reserveWays
	@return how many spare ways the project has
*/
int ConnectorCheck::Report::reserveWays() const
{
	int reserve = 0;
	for (const DrawnConnector &connector : connectors)
	{
			//The guard, and not a formality: reserve_count reads
			//ConnectorWays::unknownCount() on every other state, and
			//a total that added it would come out one short per
			//connector nobody catalogued - a wrong number wearing the
			//face of a right one.
		if (connector.isCounted()) {
			reserve += connector.reserve_count;
		}
	}
	return reserve;
}

/**
	@brief ConnectorCheck::Report::mismatchedConnectors
	@return how many counted connectors draw a label their part has no way for
*/
int ConnectorCheck::Report::mismatchedConnectors() const
{
	int mismatched = 0;
	for (const DrawnConnector &connector : connectors)
	{
		if (connector.isCounted() && !connector.not_on_part.isEmpty()) {
			++mismatched;
		}
	}
	return mismatched;
}

/**
	@brief ConnectorCheck::report
	@param project
	@param catalog
	@return what the connectors of @a project still need
*/
ConnectorCheck::Report ConnectorCheck::report(QETProject *project,
					      const Catalog &catalog)
{
	Report answer;
	answer.catalog_read = catalog.isOpen();

		//The same census the two catalogue reports walk, so that the
		//number of components cannot differ between three windows of one
		//menu. It leaves out folio reports, terminals and thumbnails,
		//which are drawing and not things that get bought.
	const QList<Element *> all = CatalogProjectActions::components(project);

	QList<PinRead> named;
	QList<PinRead> loose;

	for (Element *element : all)
	{
		if (!element) {
			continue;
		}

		const DiagramContext information = element->elementInformations();
		Diagram *diagram = element->diagram();
		QETProject *owner = diagram ? diagram->project() : nullptr;

		PinRead read;
		read.element = element;
		read.order.folio_index = owner ? owner->folioIndex(diagram) : 0;
		read.order.position = element->scenePos();
		read.folio = owner ? owner->folioIndex(diagram) + 1 : 0;
		read.connector = information.value(QETInformation::ELMT_CONNECTOR)
				 .toString().trimmed();
		read.label = information.value(QETInformation::ELMT_LABEL).toString();
		read.part_code = information.value(CatalogAssignment::partCodeKey())
				 .toString().trimmed();
		read.key = Renumberer::connectorKey(read.connector);

		if (!read.key.isEmpty())
		{
			named.append(read);
			continue;
		}

		if (isConnectorPin(catalog, read.part_code,
				   information.value(symbol_class_key).toString())) {
			loose.append(read);
		}
	}

	std::sort(named.begin(), named.end(), byReadingOrder);
	std::sort(loose.begin(), loose.end(), byReadingOrder);

	answer.pins = named.size();
	for (const PinRead &pin : loose) {
		answer.pins_without_connector << pin.element;
	}

		//Grouped by the key and not by the spelling, with the labels of
		//each group kept beside it: what the folios draw on one connector
		//is what ConnectorWays is fed with below.
	QHash<QString, int> index_of_key;
	QList<QStringList> drawn_by_index;

	for (const PinRead &pin : named)
	{
		int index = index_of_key.value(pin.key, -1);
		if (index < 0)
		{
			DrawnConnector born;
				//The first pin in reading order names the
				//connector, which is the rule the renumbering
				//already follows. Two windows picking a different
				//spelling of one connector would be two answers to
				//a question the user did not know he had asked.
			born.name  = pin.connector;
			born.key   = pin.key;
			born.folio = pin.folio;
			answer.connectors.append(born);
			drawn_by_index.append(QStringList());
			index = answer.connectors.size() - 1;
			index_of_key.insert(pin.key, index);
		}

		DrawnConnector &connector = answer.connectors[index];
		connector.pins.append(pin.element);
		drawn_by_index[index].append(pin.label);

		if (!connector.spellings.contains(pin.connector)) {
			connector.spellings << pin.connector;
		}
		if (!pin.part_code.isEmpty()
				&& !connector.part_codes.contains(pin.part_code)) {
			connector.part_codes << pin.part_code;
		}
	}

	for (int index = 0 ; index < answer.connectors.size() ; ++index)
	{
		DrawnConnector &connector = answer.connectors[index];
		QStringList pinout;
		bool part_found = false;

		if (!answer.catalog_read)
		{
			connector.state = DrawnConnector::Unread;
		}
		else if (connector.part_codes.isEmpty())
		{
			connector.state = DrawnConnector::NoPart;
		}
		else if (connector.part_codes.size() > 1)
		{
				//Two products under one name. The pinout cannot be
				//chosen without choosing one of them, and choosing
				//would be the report deciding what the shop bought.
			connector.state = DrawnConnector::MixedParts;
		}
		else
		{
				//Once per connector and not once per pin: a
				//sixteen way connector would otherwise ask the
				//data base the same question sixteen times.
			connector.part_code = connector.part_codes.first();
			const CatalogPart part = catalog.partByCode(connector.part_code);
			if (part.isNull())
			{
				connector.state = DrawnConnector::UnknownPart;
			}
			else
			{
				part_found = true;
				pinout = part.pinLabels();
			}
		}

			//Built in every state, because the count of what the
			//folios draw is a reading of the drawing: it is the one
			//number a connector with no part at all still has.
		const ConnectorWays ways =
				ConnectorWays::fromPinout(pinout, drawn_by_index.at(index));
		connector.drawn_count = ways.drawnCount();

		if (!part_found) {
			continue;
		}

		if (ways.isKnown())
		{
			connector.state         = DrawnConnector::Counted;
			connector.way_count     = ways.wayCount();
			connector.used_count    = ways.usedCount();
			connector.reserve_count = ways.reserveCount();
			connector.not_on_part   = ways.notOnPart();
		}
		else
		{
				//A part is assigned and it declares no pin at all.
				//The three counts stay unknown, on purpose - see the
				//table on ConnectorWays - and the sentence of this
				//state is the one that says so in words.
			connector.state = DrawnConnector::NoPinout;
		}
	}

		//By name, because the list is read down and worked through, and
		//the key rather than the spelling so that two runs over the same
		//project answer in the same order.
	std::sort(answer.connectors.begin(), answer.connectors.end(),
		  [](const DrawnConnector &left, const DrawnConnector &right)
	{
		return left.key < right.key;
	});

	return answer;
}

/**
	@brief ConnectorCheck::assignConnector
	@param pins
	@param connector
	@return how many pins were touched
*/
int ConnectorCheck::assignConnector(const QList<Element *> &pins,
				    const QString &connector)
{
	const QString written = connector.trimmed();
	if (pins.isEmpty() || written.isEmpty()) {
		return 0;
	}

	Diagram *diagram = nullptr;
	for (Element *pin : pins)
	{
		if (pin && pin->diagram())
		{
			diagram = pin->diagram();
			break;
		}
	}
	if (!diagram) {
		return 0;
	}

	QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext> > changes;
	for (Element *pin : pins)
	{
		if (!pin) {
			continue;
		}
		const DiagramContext before = pin->elementInformations();
		DiagramContext after = before;
		after.addValue(QETInformation::ELMT_CONNECTOR, written);
			//A pin already in that connector is not a change, and a
			//command holding it would make an undo step that undoes
			//nothing.
		if (after != before) {
			changes.insert(pin, qMakePair(before, after));
		}
	}

	if (changes.isEmpty()) {
		return 0;
	}

		//One command for all of them: the command already tells the
		//project data base on both the doing and the undoing, so the
		//parts list beside the drawing follows in one step too.
	diagram->undoStack().push(new ChangeElementInformationCommand(changes));
	return changes.size();
}
