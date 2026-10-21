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
#include "alignpartscommand.h"

#include "../../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../../alignment.h"
#include "../elementscene.h"
#include "../graphicspart/customelementpart.h"
#include "../graphicspart/partdynamictextfield.h"
#include "../graphicspart/parttext.h"
#include "../graphicspart/partterminal.h"
#include "../partletters.h"

#include <QGraphicsTextItem>

#include <algorithm>
#include <cmath>

namespace
{
		/// A part selected in the editor, as the rule of alignment sees it
	struct Piece
	{
		QGraphicsObject *object = nullptr;
		bool text = false;
		Alignment::Item item;
	};

	QList<Piece> piecesOf(ElementScene *scene)
	{
		QList<Piece> pieces;
		if (!scene) {
			return pieces;
		}
		const QList<QGraphicsItem *> selected = scene->selectedItems();
		for (QGraphicsItem *item : selected)
		{
			CustomElementPart *part = dynamic_cast<CustomElementPart *>(item);
			QGraphicsObject *object = item->toGraphicsObject();
			if (!part || !object) {
				continue;
			}
			Piece piece;
			piece.object = object;
			if (item->type() == PartText::Type || item->type() == PartDynamicTextField::Type)
			{
				const QRectF letters = PartLetters::lettersInScene(
							static_cast<QGraphicsTextItem *>(item));
				piece.text = true;
				piece.item = Alignment::Item{letters, letters.center()};
			}
			else if (item->type() == PartTerminal::Type)
			{
					//A terminal is a point: where its wires end
				const QPointF point = item->scenePos();
				piece.item = Alignment::Item{QRectF(point, QSizeF(0, 0)), point};
			}
			else
			{
					//Normalized: a line drawn from right to left gives its
					//two ends, the left one second
				const QRectF box = part->sceneGeometricRect().normalized();
				piece.item = Alignment::Item{box, box.center()};
			}
			pieces << piece;
		}
		return pieces;
	}

		/// The centre of @a pieces taken as one: of the box around all of
		/// them, points included - QRectF::united() drops an empty one -,
		/// or the point of a single one
	QPointF centreOf(const QList<Piece> &pieces)
	{
		if (pieces.size() == 1) {
			return pieces.first().item.ref;
		}
		qreal left = pieces.first().item.edges.left();
		qreal right = pieces.first().item.edges.right();
		qreal top = pieces.first().item.edges.top();
		qreal bottom = pieces.first().item.edges.bottom();
		for (const Piece &piece : pieces)
		{
			left = qMin(left, piece.item.edges.left());
			right = qMax(right, piece.item.edges.right());
			top = qMin(top, piece.item.edges.top());
			bottom = qMax(bottom, piece.item.edges.bottom());
		}
		return QPointF((left + right) / 2, (top + bottom) / 2);
	}

	QPointF whole(const QPointF &offset)
	{
		return QPointF(std::round(offset.x()), std::round(offset.y()));
	}
}

/**
	@brief AlignPartsCommand::AlignPartsCommand
	Lines up the parts selected in @a scene by @a mode. Moves nothing - no
	child - when fewer parts than the mode needs are selected, or when they
	are already where the mode puts them.
*/
AlignPartsCommand::AlignPartsCommand(ElementScene *scene, Mode mode, QUndoCommand *parent) :
	QUndoCommand(parent)
{
	const QList<Piece> pieces = piecesOf(scene);
	if (pieces.size() < partsNeeded(mode)) {
		return;
	}
	QList<Alignment::Item> items;
	for (const Piece &piece : pieces) {
		items << piece.item;
	}

	QList<QPointF> offsets;
	switch (mode)
	{
		case AlignLeft:    offsets = Alignment::alignOffsets(items, Alignment::Left);    break;
		case AlignHCenter: offsets = Alignment::alignOffsets(items, Alignment::HCenter); break;
		case AlignRight:   offsets = Alignment::alignOffsets(items, Alignment::Right);   break;
		case AlignTop:     offsets = Alignment::alignOffsets(items, Alignment::Top);     break;
		case AlignVCenter: offsets = Alignment::alignOffsets(items, Alignment::VCenter); break;
		case AlignBottom:  offsets = Alignment::alignOffsets(items, Alignment::Bottom);  break;
		case DistributeVertical:
		case DistributeHorizontal:
		{
				//As the folio distributes (T65): centre to centre, the
				//first one staying, at the tightest pitch at which none
				//touches the next - on the editor's grid of 1
			const Alignment::Direction direction = mode == DistributeVertical
					? Alignment::Vertical : Alignment::Horizontal;
			const int pitch = Alignment::distributionPitch(items, direction, 1.0, 0.0);
			offsets = Alignment::distributeOffsets(items, direction, qreal(pitch));
			break;
		}
		case CentreOnVerticalAxis:
		case CentreOnHorizontalAxis:
		{
				//The selection moves as one, its centre onto the axis
			const QPointF centre = centreOf(pieces);
			QPointF offset = mode == CentreOnVerticalAxis ? QPointF(-centre.x(), 0)
								      : QPointF(0, -centre.y());
			const bool only_texts = std::all_of(pieces.cbegin(), pieces.cend(),
							    [](const Piece &piece) {return piece.text;});
			if (!only_texts) {
				offset = whole(offset);
			}
			for (int i = 0 ; i < pieces.size() ; ++i) {
				offsets << offset;
			}
			break;
		}
	}

	const bool centring = mode == CentreOnVerticalAxis || mode == CentreOnHorizontalAxis;
	for (int i = 0 ; i < pieces.size() && i < offsets.size() ; ++i)
	{
		const Piece &piece = pieces.at(i);
			//A text by the exact amount, its letters onto the line; any
			//other part by whole numbers, as the grid places it
		const QPointF offset = piece.text || centring ? offsets.at(i) : whole(offsets.at(i));
		if (std::abs(offset.x()) < 1e-9 && std::abs(offset.y()) < 1e-9) {
			continue;
		}
		const QPointF from = piece.object->pos();
		new QPropertyUndoCommand(piece.object, "pos", from, from + offset, this);
	}

	if (centring) {
		setText(QObject::tr("Centre %n items on the axis of the symbol", "", childCount()));
	} else if (mode == DistributeVertical || mode == DistributeHorizontal) {
		setText(QObject::tr("Distribute %n items", "", childCount()));
	} else {
		setText(QObject::tr("Align %n items", "", childCount()));
	}
}

/**
	@brief AlignPartsCommand::partCount
	@return how many parts of @a scene are selected and can be lined up
*/
int AlignPartsCommand::partCount(ElementScene *scene)
{
	return piecesOf(scene).size();
}

/**
	@brief AlignPartsCommand::partsNeeded
	@return how many parts @a mode needs: two to line up or space out, one
	to centre on the axis
*/
int AlignPartsCommand::partsNeeded(Mode mode)
{
	return mode == CentreOnVerticalAxis || mode == CentreOnHorizontalAxis ? 1 : 2;
}
