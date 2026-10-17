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
#ifndef BOMCOLLECTOR_H
#define BOMCOLLECTOR_H

#include "locationtree.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUuid>

class MountingLayout;

/**
	@brief What a panel is made of - drawn, enclosing it, or only screwed to
	its plate - added up into the one list somebody orders from.

	@par Why there are two halves

	A component is drawn: it sits on a folio, it has a row in the project
	data base, and every list the program writes today walks over those
	rows. An enclosure is not drawn. Nobody puts a cabinet, a door or a
	mounting plate on a schematic - they are said, once, in the location
	tree, and the part each was bought as is written on it there. So a
	collector that walks the element rows alone comes back with a list that
	is complete, coherent, ordered, and missing about half of what the shop
	has to buy. Nothing about it looks wrong. That is the failure this file
	exists against, and it is the reason the two halves are joined here
	rather than in whichever window happens to want a list: joined once,
	they cannot be joined differently twice.

	LocationTree::bomLines() answers the enclosure half already. This joins
	it to the drawn half on the key both halves are bought by.

	@par And the part on a plate that no folio draws

	A panel is often laid out before its schematic is drawn, and some parts
	are never drawn at all - an end bracket, a distribution block, an earth
	bar picked from the catalogue and screwed straight to the plate. They
	are bought all the same, and neither half above finds them: they have
	no element row, and they are not a location. The layout is a third
	source of lines, read by mountedRowsOf() and joined on the same key.

	The hard part of it is "once". A part whose symbol is on a folio is
	already counted by the drawn half, from the row of that symbol, and the
	plate must not count it again; a part whose symbol is drawn later must
	move from the plate's count to the symbol's, and never be in both. What
	decides is the link a mounted part keeps to the element of the folio it
	is, and the rule reading that link is UndrawnPart's - the one the plate
	window lists what is off the folios with - so that this list and that
	window cannot disagree about which parts no folio draws.

	@par The key is the part, never the words describing it

	A line is (part_code, part_revision) and nothing else. Two products
	described with the same sentence - and people describe products with
	the same sentence all the time - are two lines; one product somebody
	described two ways is one line. A revision is part of the key because
	the catalogue says so: catalog_part is UNIQUE(code, revision), so a
	part whose revision went up is another part, and a list that added the
	two together would have the shop order the old one in the quantity of
	the new.

	The consolidation itself is LocationTree::indexOfBomLine, which already
	existed for the enclosure half and is used unchanged here. One spelling
	of the rule, for the same reason its own comment gives.

	@par The quantity is summed, never counted

	Five symbols of one part are five, and so is one symbol whose quantity
	field says five. Counting rows answers a different question - how many
	blocks were drawn - and answers it in the same column, with a plausible
	number, which is what makes the two impossible to tell apart on paper.

	@par Nothing is dropped in silence

	Every row that cannot go on a line is handed back named, in @ref
	Pending, with the reason. A component nobody assigned a part to, a
	quantity that is not a number, a part the two halves disagree about the
	unit of: all three are answers, and none of them is an empty cell in
	the file. The one thing counted rather than named is the exclusion -
	see @ref Result::excluded.
*/
namespace BomCollector
{
	/**
		@brief One drawn component, as a row of the view holds it.

		Text and nothing else, because that is what the data base hands
		over and because reading it is a rule that has to be provable
		without a project open. What the text means - one revision, one
		quantity, one box ticked - is the three readers below.
	*/
	struct ElementRow
	{
			/// the catalogue part it was assigned, empty when none was
		QString part_code;
			/// the revision of that part, as it was typed
		QString part_revision;
			/// what the drawing calls it
		QString designation;
			/// how many of it this one symbol stands for, empty for one
		QString quantity;
			/// the unit that quantity is in, empty for pieces
		QString unit;
			/// the tag on the folio, for naming it in a pendency
		QString label;
			/// which folio it is drawn on, for the same reason
		QString folio;
			/// the box that keeps it out of the parts list
		QString exclude_from_bom;
	};

	/**
		@brief One part screwed to a face of the layout that no folio
		draws, as this list reads it.

		Text, like ElementRow and for the same reason: the rule that joins
		it to the other halves has to be provable on rows built by hand.
		There is no quantity and no revision, because the layout keeps
		neither - a part on the plate is one piece of whatever revision is
		current - and no designation of the product, because the layout
		keeps the code it was laid as and not what the catalogue calls it.
	*/
	struct MountedRow
	{
			/// the catalogue part it was laid as, empty when none was
		QString part_code;
			/// the tag it carries on the plate, empty for a part laid
			/// straight from the catalogue
		QString label;
			/// how to call it out loud, never empty - for naming it
			/// in a pendency (MountedItem::designation)
		QString name;
			/// the location of the face it is screwed to, as a path
		QString where;
	};

		/// Why an item could not be put on a line of the list.
	enum class Pendency
	{
		NoPart,             ///< no catalogue part was ever assigned to it
		UnreadableQuantity, ///< the quantity field does not say a positive number
		UnitConflict        ///< it is measured in something its line is not
	};

	/**
		@brief One item the list cannot answer for, and why.

		Both halves land in this same shape on purpose: a cabinet with no
		part assigned is exactly as missing from the purchase list as a
		contactor with none, and a report that only knew how to name the
		drawn one would be hiding half of its own subject.
	*/
	struct Pending
	{
			/// why it is here
		Pendency reason = Pendency::NoPart;
			/// the tag of a component, the code of a location
		QString name;
			/// what it says it is, when it says anything
		QString designation;
			/// the folio a component is drawn on, the path of a location
		QString where;
	};

	/**
		@brief What one collection answered.

		The lines are the file. Everything else is what has to be said in
		front of it, because a list that quietly stands for less than the
		project is worse than one that refuses to be written.
	*/
	struct Result
	{
			/// the list itself, ordered by part and then by revision
		QList<LocationTree::BomLine> lines;
			/// everything that could not go on a line, named
		QList<Pending> pendings;
		/**
			How many drawn components the user kept out of the parts
			list.

			A number and not the names, and that is the decision rather
			than laziness: the box means two things at once today - keep
			it off the drawing's nomenclature, and do not buy it - and
			splitting that in two is a field this file does not own.
			What it can do without owning anything is refuse to let the
			exclusion pass unmentioned, which is all the difference
			between a short list and a wrong one.
		*/
		int excluded = 0;
		/**
			How many drawn components reached a line.

			With @ref pendings and @ref excluded it accounts for every
			row that came in: a row leaves collect() through exactly
			one of the three, so the three add up to what was handed
			over. A row that went missing cannot be written down.
		*/
		int drawn = 0;
			/// how many lines of the enclosure half reached the list
		int enclosures = 0;
		/**
			How many parts of the layout reached a line, one piece
			each.

			With the pendencies named at a face it accounts for every
			MountedRow that came in, for the reason @ref drawn gives.
		*/
		int mounted = 0;
	};

	/**
		The view the drawn half is read from: element_label_view, and
		deliberately not element_nomenclature_view.

		The two carry the same columns and differ by one clause - the
		nomenclature view drops the rows whose exclude_from_bom is 'true'.
		Reading the filtered one would make the exclusion invisible here,
		and counting it would then need a second query that could disagree
		with the first. One question over the unfiltered view answers both
		the list and the number it left out, and the two cannot drift.
	*/
	QString viewName();

		/// The columns a purchase line is built from, in select order.
	QStringList columns();

	/**
		The whole SELECT, ordered by part, then revision, then by where the
		component sits.

		Ordered on purpose and not by accident of storage: two exports of
		the same project have to be comparable line by line, and an order
		that changes when somebody edits an unrelated component would make
		every line look moved.
	*/
	QString selectStatement();

	/**
		@brief Is this row one the user kept out of the parts list?
		@param raw the exclude_from_bom cell, as it is stored
		@return true for the one spelling the check box writes

		Compared against "true" exactly, which is what the check box
		writes and what the nomenclature view compares. Reading it more
		loosely here - accepting "True" or "1" - would be defensible on
		its own and wrong in company: this collector would then drop a row
		that the nomenclature the drawing prints still shows, and the two
		lists would describe different projects without either of them
		being able to say so.
	*/
	bool isExcluded(const QString &raw);

	/**
		@brief The revision a line is keyed by.
		@param raw the part_revision cell, as it was typed
		@return the number it says, 0 when it says nothing usable

		0 is what LocationTree::BomLine already means by "whatever is
		current will do", so a component with no revision typed and an
		enclosure with none land on the same line - which is the answer a
		project that never used revisions needs.
	*/
	int revisionOf(const QString &raw);

	/**
		@brief How many pieces one row stands for.
		@param raw the quantity cell, as it was typed
		@param ok filled with false when @p raw is not a usable quantity
		@return the number, 1 for an empty cell

		Empty is one, and that default carries most of a real project:
		almost nobody fills the field in, and a symbol that is on the
		folio stands for at least the one piece somebody drew.

		Zero and negative are refused along with text that is not a
		number. Zero looks like a legitimate answer and is not one this
		list can act on: "buy none of it" and "I typed into the wrong
		field" are the same keystroke, and the file cannot tell them
		apart. Refusing puts it in front of a person instead of deciding
		for them.

		Read in the C locale, like every other stored number of this
		program. A comma typed as a decimal separator therefore does not
		parse - and comes back as a pendency rather than as a piece,
		which is the point: 1,5 read as 1 would be half a metre of duct
		nobody ordered and nothing on paper to show for it.
	*/
	double quantityOf(const QString &raw, bool *ok = nullptr);

	/**
		@brief One row built from one row of selectStatement().
		@param values the row, column name to cell
		@return the row, its cells still text
	*/
	ElementRow rowFrom(const QHash<QString, QString> &values);

	/**
		@brief The parts of the layout this list has to count, because no
		folio does.
		@param layout the layout of the project
		@param drawn_elements the uuid of the elements the folios hold -
		at least every one UndrawnPart::namedElements() returns
		@return one row per part, face by face in the order of the layout

		Exactly the parts UndrawnPart::of() answers, less the pieces cut to
		length, and for both of its reasons:

		- a part laid with no symbol behind it is the layout drawn before
		  the schematic, and the plate is the only place that knows it has
		  to be bought;
		- a part whose symbol was deleted after it was mounted has no row
		  on any folio either, so the drawn half no longer counts it. The
		  plate is still what the shop builds, and leaving the part out
		  would have the shop build a plate it was never sent the part for.
		  Which of the two a part is stays the plate window's to say.

		A part whose symbol is on a folio is left out, and that is the
		whole of "once": the symbol decides - counted by the drawn half,
		kept out by the box ticked on it, or left out of the rows by
		whoever reads them - and the plate never brings back a part the
		drawing kept out.

		A piece of rail or duct is left out because it is not bought as a
		piece: it is cut from a bar, and the bar is bought by the length
		MountingLayout::profileTotals() adds up. Counted here, a plate
		with four rails would order four rails by the piece.

		The set is the one input this cannot check. An element a folio
		does hold, missing from it, reads as a symbol gone, and the part
		is counted twice - once from its symbol and once from here. Build
		it the way the plate window does, from the elements the layout
		names.
	*/
	QList<MountedRow> mountedRowsOf(const MountingLayout &layout,
					const QSet<QUuid> &drawn_elements);

	/**
		@brief Join the halves into the list.
		@param rows the drawn components, from selectStatement()
		@param enclosures the half nobody draws, from
		LocationTree::bomLines()
		@param mounted the parts on a plate that no folio draws, from
		mountedRowsOf(); none when the caller has no layout to read
		@return the lines, and everything that did not become one

		The drawn half is read first so that a line shared by both takes
		the designation of the product rather than the name of the place:
		a cabinet is called "Armoire 2000x800" on an order form and
		"Coffret principal" on a plan, and only one of the two can be
		looked up by whoever buys it. The plate names no product, so the
		order of the last two halves changes no name.

		A part code that appears in two halves gives one line with the
		quantities added. That case is the whole reason this function
		exists, and it is the one a careless join gets wrong in both
		directions at once - one line too many, or one contribution lost
		inside the line that survived.

		A part of the plate is one piece of revision 0, so it shares the
		line of a symbol of the same code only when nobody typed a
		revision on that symbol either - the key of a line is the part
		and its revision, whichever half it comes from.
	*/
	Result collect(const QList<ElementRow> &rows,
		       const QList<LocationTree::BomLine> &enclosures,
		       const QList<MountedRow> &mounted = QList<MountedRow>());

		/// @return how many pendencies of that kind the result holds
	int countOf(const Result &result, Pendency reason);

	/**
		@brief Add up the quantity column.
		@param lines the lines
		@param unit the unit to add up
		@return the total the lines carrying that unit come to

		By unit, and never over the whole list, because the quantity
		column of this list does not hold one kind of number: pieces and
		metres share it, with the unit beside them saying which. A single
		total over both would be a number nothing in the world
		corresponds to, printed in a place where people trust totals.

		Two units read as the same when neither is a length - a count is
		a count whatever word is beside it - and otherwise when they are
		the same length unit. Metres and millimetres are not added.
	*/
	double totalOf(const QList<LocationTree::BomLine> &lines,
		       const QString &unit);
}

#endif // BOMCOLLECTOR_H
