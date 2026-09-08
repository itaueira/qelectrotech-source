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
#ifndef CONNECTORCHECK_H
#define CONNECTORCHECK_H

#include "connectorways.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>

class Catalog;
class Element;
class QETProject;

/**
	@brief What the connectors of an open project still need, read before
	the project is closed.

	Two questions are asked here, and they are answered in two lists that
	never merge:

	- which connectors the folios draw whose ways cannot be counted, and
	- which pins are drawn belonging to no connector at all.

	They are not two shades of one problem. A connector nobody catalogued
	is a question for whoever buys and registers products; a pin belonging
	to no connector is a question for whoever drew the folio, and it is the
	acceptance criterion of T34 - "no pin exists outside a connector". One
	list holding both would be one heading over two problems, and the count
	under it would mean neither. The report next door,
	CatalogProjectActions::physicalViewReport, keeps the same fence with
	its own sibling, for the same reason.

	Nothing here computes a truth of its own. The pins of a connector are
	grouped with Renumberer::connectorKey, the one that already decides
	that "CN1", "cn1" and "CN1 " are one connector; the ways are counted by
	ConnectorWays; the census of components is
	CatalogProjectActions::components, the same walk the two catalogue
	reports do. Three answers to "how many connectors has this project"
	would be two answers too many.
*/
namespace ConnectorCheck
{
	/**
		@brief One connector as the folios draw it: a name written on
		pins, and the pins that carry it.

		A connector is not an object of the project yet - it is what a
		set of pins say they belong to - so this is a reading and not a
		record, and it is thrown away when the project is read again.
	*/
	class DrawnConnector
	{
		Q_DECLARE_TR_FUNCTIONS(DrawnConnector)

		public:
			/// Why the ways of this connector can or cannot be counted
			enum State
			{
				Counted,     ///< a part is assigned and it declares a pinout
				Unread,      ///< the catalogue did not answer, so nothing is claimed
				NoPart,      ///< no pin of it carries a product code
				UnknownPart, ///< the catalogue holds no such code
				NoPinout,    ///< it holds it, and the part declares no pin at all
				MixedParts   ///< its pins point at more than one product
			};

			/**
				@return why the ways cannot be counted, in one
				sentence; empty for a connector whose ways can be.

				Four sentences and not one, because the move that
				settles each of them is a different move: buy and
				register a product, correct a code, type the pinout
				of a product already registered, or agree on which
				product the pins are. A row reading only "no part"
				would send the same person to the same wrong window
				three times out of four.
			*/
			QString describe() const;

			/// @return true when a part with a pinout is assigned
			bool isCounted() const;

				/// the spelling of the first pin in reading order
			QString name;
				/// Renumberer::connectorKey(name), what groups the pins
			QString key;
			/**
				every distinct spelling met, name first.

				More than one is not an error and is not corrected
				here - the registered decision of step 1 is that the
				field is free text, and step 3 states that
				renumbering does not rewrite what the user typed -
				but it is worth saying, because the difference
				between one connector and two has nowhere else to
				show.
			*/
			QStringList spellings;
				/// the product code its pins carry, empty when none does
			QString part_code;
				/// every distinct product code its pins carry
			QStringList part_codes;
				/// its pins, in reading order
			QList<Element *> pins;
				/// 1-based folio of the first pin, 0 when it has none
			int folio = 0;
				/// why the ways can or cannot be counted
			State state = Unread;

			/**
				how many ways the part has, how many of them are
				drawn, and how many are spare.

				ConnectorWays::unknownCount(), and never nought,
				while the state is not Counted. Whoever prints one of
				these asks isCounted() first, or prints "-1 reserve"
				on a crimping guide.
			*/
			int way_count     = ConnectorWays::unknownCount();
			int used_count    = ConnectorWays::unknownCount();
			int reserve_count = ConnectorWays::unknownCount();
			/**
				how many distinct pin labels the folios draw on it.

				The one count that survives every state, because it
				reads the drawing and not the catalogue: "nine pins
				drawn and no part registered" is exactly the sentence
				a row of this report has to be able to say.
			*/
			int drawn_count = 0;
			/**
				the labels drawn that the part has no way for.

				Left as ConnectorWays gives it, name by name and
				unnormalised, because that is what makes a "9 " typed
				against a part that says "9" visible instead of
				silently eating a spare way.
			*/
			QStringList not_on_part;
	};

	/**
		@brief What one reading of a project says about its connectors.

		The two lists are held apart; the arithmetic over them is held
		here rather than in the window, so that the sentence on screen and
		the numbers a test reads cannot drift apart.
	*/
	class Report
	{
		public:
			/// @return the connectors whose ways cannot be counted, in name order
			QList<DrawnConnector> uncountable() const;
			/// @return how many connectors have a part with a pinout
			int countedConnectors() const;
			/**
				@return how many spare ways the project has.

				Summed over the counted connectors alone. A
				connector whose pinout nobody wrote has no reserve to
				add - not a reserve of nought - and adding
				ConnectorWays::unknownCount() into a total would take
				one away from it.
			*/
			int reserveWays() const;
			/**
				@return how many counted connectors draw a label
				their part has no way for.

				Counted and not listed, for the reason the report
				next door counts an inherited size instead of listing
				it: those connectors do have a part, so a row of them
				among the ones without a part would make the column
				say two things. But a way drawn that the product does
				not have is a mistake on one side or the other, and a
				report that never mentioned it would let it through.
			*/
			int mismatchedConnectors() const;

				/// every connector the folios draw, in name order
			QList<DrawnConnector> connectors;
			/**
				the pins that belong to no connector, in reading
				order, folio by folio.

				The other question, in its own list. Empty while the
				catalogue has not answered: what makes a component a
				pin, when its own field is empty, is the class of its
				part, and a share that is down would turn every
				component of the project into a suspect.
			*/
			QList<Element *> pins_without_connector;
				/// how many pins belong to a connector
			int pins = 0;
			/**
				true when the catalogue answered at all.

				False leaves both lists empty rather than filled with
				every component of the project - a false alarm that
				looks exactly like a finding.
			*/
			bool catalog_read = false;
	};

	/**
		@brief Read the connectors of @a project.
		@param project
		@param catalog the catalogue to ask about the parts and the classes
		@return the report

		The pinout comes from the part the pins carry, whatever class that
		part was filed under. A connector registered as a component is a
		bookkeeping slip of the shop, and refusing to count its ways
		because of it would be refusing the right answer for the wrong
		reason - the same call linkAccessory makes when it warns about a
		class instead of refusing on it.

		The class is read for the other list, and only there, because
		there it is all that is left: a pin whose own field is empty says
		nothing about being a pin, and the class of its part - or the
		class its symbol was built with - is what remains to ask.
	*/
	Report report(QETProject *project, const Catalog &catalog);

	/**
		@brief Write @a connector on @a pins, through the undo stack.
		@param pins
		@param connector the connector name, as it is to be written
		@return how many pins were touched

		One command for the whole selection, because putting six pins
		into XS1 is one gesture on the designer's side and has to be one
		step on the stack.

		The ends of @a connector are trimmed and nothing else is done to
		it. This is a place that writes the field without a person typing
		it, so it writes no spelling of its own; trimming only keeps it
		from writing blanks that Renumberer::connectorKey would have to
		fold away later. A name with nothing but blanks on it is refused,
		because it would make a connector no report can name.
	*/
	int assignConnector(const QList<Element *> &pins, const QString &connector);
}

#endif // CONNECTORCHECK_H
