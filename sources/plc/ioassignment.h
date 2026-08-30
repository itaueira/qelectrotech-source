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
#ifndef IOASSIGNMENT_H
#define IOASSIGNMENT_H

#include "../properties/elementdata.h"
#include "iolist.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief Putting an imported I/O point into a channel of a card.

	A PLC master element already carries a table of channels: the ios of its
	ElementData::PlcMasterData, drawn on the folio by PartPlcTable. Each row
	has a type, an address, a function text and a comment, and today every
	one of those is typed by hand, row by row, from a sheet somebody has
	open on the side.

	An IoPoint imported from that same sheet holds the same words. So
	assigning is not a new drawing operation: it is writing the point into
	one row of one card, and remembering in the point which row it took.
	That is what IoPoint::master_uuid and IoPoint::io_index are, and what
	makes IoPoint::isAssigned() true.

	The whole decision is made here, on plain values, so it can be proved on
	a bench: no Element, no Diagram, no project. What holds an Element and
	pushes the undo command is AssignIoPointsCommand; what asks the person
	which points go in is IoAssignDialog.

	Two refusals matter more than the rest, because they are the ones that
	protect work already done:

	- a channel that already carries a function text is taken, even when no
	  point in the list claims it. Somebody typed that, and an import is
	  not allowed to overwrite it;
	- a point already assigned is never assigned a second time. Moving a
	  point is releasing it and assigning it again, which is two decisions
	  and reads as two in the undo stack.
*/
class IoAssignment
{
	Q_DECLARE_TR_FUNCTIONS(IoAssignment)

	public:
		/// why one point could not go in
		enum Refusal {
			NoRefusal,
			/// no point of the list carries that id
			PointNotFound,
			/// the point is already in a card
			AlreadyAssigned,
			/// the card has no free channel of a compatible type left
			NoFreeChannel
		};

		/**
			@brief One point and the channel it is going to take.
		*/
		struct Pair
		{
			Pair() {}
			Pair(const QString &point_id, int io_index,
			     const QString &channel) :
				point_id(point_id),
				io_index(io_index),
				channel(channel) {}

			/// the id of the point, which is what never changes
			QString point_id;
			/// the row of the card table it goes into
			int io_index = -1;
			/// the name that row answers to, kept in the point
			QString channel;
		};

		/**
			@brief One point that stayed out, and why.
		*/
		struct Rejected
		{
			Rejected() {}
			Rejected(const QString &point_id, const QString &label,
				 Refusal reason) :
				point_id(point_id),
				label(label),
				reason(reason) {}

			QString point_id;
			/// how the sheet names it, so the message can say it out loud
			QString label;
			Refusal reason = NoRefusal;
		};

		/**
			@brief What an assignment would do, before it does it.

			Built by plan(), shown to the person, and only then handed
			to apply(). Nothing in it touches a card or a list.
		*/
		struct Plan
		{
			/// the points that go in, in the order they were given
			QList<Pair> pairs;
			/// and the ones that do not, each with its reason
			QList<Rejected> rejected;

			bool isEmpty() const {return pairs.isEmpty();}
			/// @return true when every point asked for got a channel
			bool isClean() const {return rejected.isEmpty();}
			/// @return the whole thing said in one paragraph
			QString text() const;
		};

		/// @return true when @a type is an input, of whatever flavour
		static bool isInput(ElementData::PlcIOType type);
		/// @return true when @a type takes both digital and analogue
		static bool isUniversal(ElementData::PlcIOType type);
		/**
			@brief May a point of type @a point go in a channel of
			type @a channel?

			Same direction, and then either the two agree or one of
			them is universal. A universal input channel takes a
			digital point and an analogue one; a digital channel
			never takes an output, whatever else is true.
		*/
		static bool accepts(ElementData::PlcIOType channel,
				    ElementData::PlcIOType point);

		/**
			@brief The name a channel answers to.
			@return its address when the card gives one, failing that
			the first terminal the card names, failing that the row
			number counted from one, written #3.

			Never the default T1, T2 of PlcIO::effectiveTerminals():
			those are placeholders the editor shows, and using them
			would give every card the same four channel names.
		*/
		static QString channelName(const QVector<ElementData::PlcIO> &ios,
					   int io_index);

		/**
			@brief Is that channel already spoken for?

			Either a point of @a list holds it, or it carries a
			function text nobody claims - which is somebody's typing,
			and is exactly as protected.
		*/
		static bool isTaken(const QVector<ElementData::PlcIO> &ios,
				    const IoList &list,
				    const QString &master_uuid,
				    int io_index);

		/**
			@brief The rows of @a ios that are free, in order.
			@param also_taken rows the caller knows are spoken for
			although nothing in @a list or @a ios says so.

			That last one is how a channel already wired to a drawn
			element stays out of reach. Linking a slave to a master
			does not write anything into the card table - it reads
			from it - so the model cannot see that link on its own,
			and whoever holds the Element has to say so.
		*/
		static QList<int> freeChannels(const QVector<ElementData::PlcIO> &ios,
					       const IoList &list,
					       const QString &master_uuid,
					       const QList<int> &also_taken = QList<int>());

		/**
			@brief Work out where each point would go.
			@param list the project list, read and not written
			@param point_ids the points to place, in order
			@param ios the channels of the card
			@param master_uuid the uuid of the element that owns them
			@return the pairs and the refusals

			Channels are taken in order, the first free one of a
			compatible type winning. Two points never land on the
			same row: the plan marks a row taken as soon as it hands
			it out.
		*/
		static Plan plan(const IoList &list,
				 const QStringList &point_ids,
				 const QVector<ElementData::PlcIO> &ios,
				 const QString &master_uuid,
				 const QList<int> &also_taken = QList<int>());

		/**
			@brief Carry out @a plan.
			@param list written: each point learns its card, its row
			and its channel name
			@param ios written: each row learns the description, the
			comment and the address the point carries
			@return how many points went in

			A field of the card that already has something in it is
			left alone. The card is the drawing, and the drawing wins
			over the sheet on everything but the empty cell.
		*/
		static int apply(const Plan &plan,
				 IoList &list,
				 QVector<ElementData::PlcIO> &ios,
				 const QString &master_uuid);

		/**
			@brief Take back the channels @a io_indexes of a card.
			@return how many points were freed

			The point goes back to the unassigned set. The row is
			blanked only where it still says what the point put
			there: a cell edited by hand since is somebody's work,
			and releasing a point is not a reason to lose it.
		*/
		static int release(IoList &list,
				   QVector<ElementData::PlcIO> &ios,
				   const QString &master_uuid,
				   const QList<int> &io_indexes);

		/// @return the indexes of the points of @a list held by that card
		static QList<int> pointsOf(const IoList &list,
					   const QString &master_uuid);

		/// @return the text a row of the card should read, empty when none
		static QString functionTextOf(const IoPoint &point);

		/// @return how the messages name a point: tag, address, description
		static QString pointLabel(const IoPoint &point);

		/// @return @a reason said in a sentence, naming @a label
		static QString refusalText(Refusal reason, const QString &label);
};

#endif // IOASSIGNMENT_H
