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
#ifndef SYMBOLTEXTSYNC_H
#define SYMBOLTEXTSYNC_H

#include <QDomDocument>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QSet>
#include <QUuid>

class DynamicElementTextItem;
class Element;

/**
	@brief The SymbolTextSync namespace
	Brings the dynamic texts of a placed element in line with its symbol.

	A placed element is given the texts of its symbol once, when it is
	placed. From then on they are the element's own: they are saved with it,
	and loading the element puts them back in place of the ones its symbol
	gives (Element::fromXml()). So a text added to a symbol, or moved, or
	given another font, reaches none of the elements already placed - not
	even when the project is opened again.

	Each text made from the symbol remembers which text of the symbol it was
	made from (DynamicElementTextItem::symbolTextUuid()), and each element
	what its symbol was when it was built (Element::symbolBase()). Then, for
	each property of a text, three values are compared: the symbol's before,
	the symbol's now, and the element's. Whatever the element still has as
	the symbol had it follows the symbol; whatever is different was changed
	on the folio, and stays. The position is compared by the anchor point of
	the text (TextAnchor), not by its top-left corner, which a centred or
	right-aligned text moves by itself when its content changes.

	A text the symbol no longer has goes; a text new in the symbol is added,
	unless it was removed from this element on a folio
	(Element::removedSymbolTexts()); a text added on a folio, which comes
	from no text of the symbol, stays.

	An element saved before texts remembered their symbol text is linked by
	the kind and the content of its texts, then by their kind and where they
	are: a fixed text rewritten in place is still the same text. What it was
	placed from is not known, and the project's copy may have changed since
	without its texts following: so a value most elements of the copy share
	is taken for the value of the symbol they were placed from, and not for
	a change made on a folio (consensus()).

	Reset mode is the other way round: whatever the symbol says wins, and
	only the values of the element (its label, its informations) and the
	texts added on a folio stay.

	The texts the element is compared with are those of an element built
	from the definition and given the informations of the placed one
	(Element::buildDetached()): their content, so their size, is that of the
	placed element's texts.

	Nothing is changed here: plan() says what to do, and
	SyncSymbolTextsCommand does it, undoably.
*/
namespace SymbolTextSync
{
	enum class Mode {
		Merge, ///< what the element did not change follows the symbol
		Reset  ///< the symbol wins
	};

	/// One text of the element to change
	struct Change
	{
		DynamicElementTextItem *text = nullptr;
		QDomDocument target;   ///< the text as it becomes (dynamic_elmt_text), but its place
		QPointF anchor;        ///< where its anchor point goes, in element coordinates
		bool move = true;      ///< false for a text in a group, which the group places
		bool visible = false;  ///< false when only the link to the symbol text is written
	};

	/// What bringing one element in line with its symbol does
	struct Plan
	{
		Element *element = nullptr;
		QList<Change> changed;
		QList<QDomDocument> added;              ///< dynamic_elmt_text, placed as the symbol places them
		QList<DynamicElementTextItem *> removed;
		QDomDocument new_base;                  ///< the symbol the element is in line with afterwards
		QSet<QUuid> removed_symbol_texts;       ///< Element::removedSymbolTexts() afterwards

		bool isEmpty() const {return changed.isEmpty() && added.isEmpty() && removed.isEmpty();}
	};

	/// What most elements of one copy share for one text of the symbol
	struct Shared
	{
		QHash<QString, QString> fields; ///< by property, see plan()
		bool has_anchor = false;
		QPointF anchor;
	};
	/// By text of the symbol
	using Consensus = QHash<QUuid, Shared>;

	Consensus consensus(const QList<Element *> &elements);
	Plan plan(Element *element, const QDomElement &definition, Mode mode,
			  const Consensus &common = Consensus());
	QPointF anchorOf(const DynamicElementTextItem *text);
	QPointF posForAnchor(const DynamicElementTextItem *text, const QPointF &anchor);
	bool syncs(const Element *element);
}

#endif // SYMBOLTEXTSYNC_H
