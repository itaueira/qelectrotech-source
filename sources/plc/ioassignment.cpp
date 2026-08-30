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
#include "ioassignment.h"

#include <QSet>

/**
	@brief How many refusals the summary spells out before counting the rest.
*/
static const int MAX_SAID_REFUSALS = 6;
/**
	@brief How many channel names the summary lists before trailing off.
*/
static const int MAX_SAID_CHANNELS = 8;

/**
	@brief IoAssignment::Plan::text
	@return the plan said in a paragraph, the way IoSheet::Report does it:
	one line for what happens, then one line per point that stays out.
*/
QString IoAssignment::Plan::text() const
{
	QStringList lines;

	if (pairs.isEmpty())
	{
		lines << tr("Aucun point d'E/S ne sera affecté.");
	}
	else
	{
		QStringList channels;
		for (const Pair &pair : pairs)
		{
			if (channels.count() == MAX_SAID_CHANNELS)
			{
				channels << QStringLiteral("…");
				break;
			}
			channels << pair.channel;
		}
		lines << tr("%1 point(s) d'E/S affecté(s) aux voies : %2.")
			 .arg(int(pairs.count()))
			 .arg(channels.join(QStringLiteral(", ")));
	}

	if (!rejected.isEmpty())
	{
		lines << tr("%1 point(s) laissé(s) de côté :")
			 .arg(int(rejected.count()));

		int said = 0;
		for (const Rejected &one : rejected)
		{
			if (said == MAX_SAID_REFUSALS)
			{
				lines << tr("… et %1 autre(s).")
					 .arg(int(rejected.count()) - said);
				break;
			}
			lines << QStringLiteral("  ")
				 + refusalText(one.reason, one.label);
			++said;
		}
	}

	return lines.join(QLatin1Char('\n'));
}

/**
	@brief IoAssignment::isInput
	@param type
	@return true when @a type is an input, digital, analogue or universal
*/
bool IoAssignment::isInput(ElementData::PlcIOType type)
{
	return type == ElementData::EntreeDigitale
	       || type == ElementData::EntreeAnalogique
	       || type == ElementData::EntreeUniverselle;
}

/**
	@brief IoAssignment::isUniversal
	@param type
	@return true when @a type takes both digital and analogue
*/
bool IoAssignment::isUniversal(ElementData::PlcIOType type)
{
	return type == ElementData::EntreeUniverselle
	       || type == ElementData::SortieUniverselle;
}

/**
	@brief IoAssignment::accepts
	@param channel the type of the row of the card
	@param point the type of the point that wants it
	@return true when the point may go in

	Direction first, and it is never negotiable: an output never lands in an
	input channel, whatever else the two have in common. Then the flavour,
	where a universal channel takes anything of its direction, and a point
	of universal type goes in any channel of its direction. A line of the
	sheet whose type was not recognised was read as a digital input long
	before it got here, so nothing special happens to it.
*/
bool IoAssignment::accepts(ElementData::PlcIOType channel,
			   ElementData::PlcIOType point)
{
	if (isInput(channel) != isInput(point)) {
		return false;
	}
	if (isUniversal(channel) || isUniversal(point)) {
		return true;
	}
	return channel == point;
}

/**
	@brief IoAssignment::channelName
	@param ios the channels of the card
	@param io_index the row wanted
	@return the name that row answers to

	Its address when the card gives one, failing that the first terminal the
	card actually names, failing that the row number counted from one.

	PlcIO::effectiveTerminals() is deliberately not used: it fabricates T1,
	T2, T3 for a row that names nothing, and those placeholders would make
	every card in the project have the same four channel names.
*/
QString IoAssignment::channelName(const QVector<ElementData::PlcIO> &ios,
				  int io_index)
{
	if (io_index < 0 || io_index >= ios.count()) {
		return QString();
	}

	const ElementData::PlcIO &row = ios.at(io_index);
	if (!row.address.isEmpty()) {
		return row.address;
	}
	for (const QString &terminal : row.terminals)
	{
		if (!terminal.isEmpty()) {
			return terminal;
		}
	}
	return QStringLiteral("#") + QString::number(io_index + 1);
}

/**
	@brief IoAssignment::isTaken
	@param ios the channels of the card
	@param list the project list
	@param master_uuid the uuid of the element that owns the card
	@param io_index the row wanted
	@return true when that row is spoken for

	Two ways of being taken, and the second one is the one that matters: a
	point of the list holds the row, or the row carries a function text
	nobody claims. That second text was typed by somebody, and an import is
	not allowed to write over it.
*/
bool IoAssignment::isTaken(const QVector<ElementData::PlcIO> &ios,
			   const IoList &list,
			   const QString &master_uuid,
			   int io_index)
{
	if (io_index < 0 || io_index >= ios.count()) {
		return true;
	}
	if (!ios.at(io_index).functionText.isEmpty()) {
		return true;
	}
	if (master_uuid.isEmpty()) {
		return false;
	}

	for (int i = 0 ; i < list.count() ; ++i)
	{
		const IoPoint &point = list.at(i);
		if (point.isAssigned()
		    && point.master_uuid == master_uuid
		    && point.io_index == io_index) {
			return true;
		}
	}
	return false;
}

/**
	@brief IoAssignment::freeChannels
	@param ios the channels of the card
	@param list the project list
	@param master_uuid the uuid of the element that owns the card
	@param also_taken rows the caller already knows are spoken for,
	a channel wired to a drawn element being the case that matters
	@return the rows still free, in the order of the card
*/
QList<int> IoAssignment::freeChannels(const QVector<ElementData::PlcIO> &ios,
				      const IoList &list,
				      const QString &master_uuid,
				      const QList<int> &also_taken)
{
	QList<int> free_channels;
	for (int i = 0 ; i < ios.count() ; ++i)
	{
		if (also_taken.contains(i)) {
			continue;
		}
		if (!isTaken(ios, list, master_uuid, i)) {
			free_channels.append(i);
		}
	}
	return free_channels;
}

/**
	@brief IoAssignment::plan
	@param list the project list, read and never written
	@param point_ids the points to place, in the order they are given
	@param ios the channels of the card
	@param master_uuid the uuid of the element that owns the card
	@param also_taken rows the caller keeps out of reach
	@return where each point would go, and who stays out

	The first free channel of a compatible type wins, which is what makes a
	whole sixteen point card fill in one gesture and in the order of the
	sheet. A channel handed out leaves the free list at once, so two points
	never land on the same row.
*/
IoAssignment::Plan IoAssignment::plan(const IoList &list,
				      const QStringList &point_ids,
				      const QVector<ElementData::PlcIO> &ios,
				      const QString &master_uuid,
				      const QList<int> &also_taken)
{
	Plan plan;
	QList<int> free_channels =
			freeChannels(ios, list, master_uuid, also_taken);
	QSet<QString> seen;

	for (const QString &id : point_ids)
	{
			// a point named twice is one point, not two refusals
		if (seen.contains(id)) {
			continue;
		}
		seen.insert(id);

		const int index = list.indexOfId(id);
		if (index < 0)
		{
			plan.rejected << Rejected(id, id, PointNotFound);
			continue;
		}

		const IoPoint point = list.point(index);
		const QString label = pointLabel(point);
		if (point.isAssigned())
		{
			plan.rejected << Rejected(id, label, AlreadyAssigned);
			continue;
		}

		int wanted = -1;
		for (int i = 0 ; i < free_channels.count() ; ++i)
		{
			if (accepts(ios.at(free_channels.at(i)).type, point.type))
			{
				wanted = i;
				break;
			}
		}
		if (wanted < 0)
		{
			plan.rejected << Rejected(id, label, NoFreeChannel);
			continue;
		}

		const int io_index = free_channels.takeAt(wanted);
		QString channel = channelName(ios, io_index);
			// a row the card does not address takes the address of
			// the point, because apply() is about to write it there:
			// the plan says the name the channel will end up with
		if (ios.at(io_index).address.isEmpty() && !point.address.isEmpty()) {
			channel = point.address;
		}
		plan.pairs << Pair(id, io_index, channel);
	}

	return plan;
}

/**
	@brief IoAssignment::apply
	@param plan what plan() worked out
	@param list written: the points learn the card, the row and the name
	@param ios written: the rows learn what the sheet says
	@param master_uuid the uuid of the element that owns the card
	@return how many points went in

	An empty cell of the card is filled; a cell with something in it is left
	exactly as it is. The row type is never touched at all - the card is
	talking about its own hardware there, and a spreadsheet does not get to
	contradict it.
*/
int IoAssignment::apply(const Plan &plan,
			IoList &list,
			QVector<ElementData::PlcIO> &ios,
			const QString &master_uuid)
{
	if (master_uuid.isEmpty()) {
		return 0;
	}

	int done = 0;
	for (const Pair &pair : plan.pairs)
	{
		if (pair.io_index < 0 || pair.io_index >= ios.count()) {
			continue;
		}
		const int index = list.indexOfId(pair.point_id);
		if (index < 0) {
			continue;
		}

		IoPoint point = list.point(index);
		if (point.isAssigned()) {
			continue;
		}

		ElementData::PlcIO &row = ios[pair.io_index];
		const QString function_text = functionTextOf(point);
		if (row.functionText.isEmpty() && !function_text.isEmpty()) {
			row.functionText = function_text;
		}
		if (row.comment.isEmpty() && !point.comment.isEmpty()) {
			row.comment = point.comment;
		}
		if (row.address.isEmpty() && !point.address.isEmpty()) {
			row.address = point.address;
		}

		point.master_uuid = master_uuid;
		point.io_index = pair.io_index;
		point.channel = pair.channel;
		if (list.setPoint(index, point)) {
			++done;
		}
	}
	return done;
}

/**
	@brief IoAssignment::release
	@param list written: the points go back to the unassigned set
	@param ios written: the rows are blanked, where blanking is safe
	@param master_uuid the uuid of the element that owns the card
	@param io_indexes the rows to take back
	@return how many points were freed

	A cell is blanked only where it still says exactly what the point wrote
	there. Edited since, it is somebody's work: taking a point out of a
	channel is not a reason to lose it.
*/
int IoAssignment::release(IoList &list,
			  QVector<ElementData::PlcIO> &ios,
			  const QString &master_uuid,
			  const QList<int> &io_indexes)
{
	if (master_uuid.isEmpty() || io_indexes.isEmpty()) {
		return 0;
	}

	int done = 0;
	for (int index = 0 ; index < list.count() ; ++index)
	{
		IoPoint point = list.point(index);
		if (!point.isAssigned()
		    || point.master_uuid != master_uuid
		    || !io_indexes.contains(point.io_index)) {
			continue;
		}

		if (point.io_index < ios.count())
		{
			ElementData::PlcIO &row = ios[point.io_index];
			const QString function_text = functionTextOf(point);
			if (!function_text.isEmpty()
			    && row.functionText == function_text) {
				row.functionText.clear();
			}
			if (!point.comment.isEmpty()
			    && row.comment == point.comment) {
				row.comment.clear();
			}
			if (!point.address.isEmpty()
			    && row.address == point.address) {
				row.address.clear();
			}
		}

		point.master_uuid.clear();
		point.io_index = -1;
		point.channel.clear();
		if (list.setPoint(index, point)) {
			++done;
		}
	}
	return done;
}

/**
	@brief IoAssignment::pointsOf
	@param list the project list
	@param master_uuid the uuid of the element that owns the card
	@return the indexes of the points that card holds, in list order
*/
QList<int> IoAssignment::pointsOf(const IoList &list,
				  const QString &master_uuid)
{
	QList<int> indexes;
	if (master_uuid.isEmpty()) {
		return indexes;
	}

	for (int i = 0 ; i < list.count() ; ++i)
	{
		const IoPoint &point = list.at(i);
		if (point.isAssigned() && point.master_uuid == master_uuid) {
			indexes.append(i);
		}
	}
	return indexes;
}

/**
	@brief IoAssignment::functionTextOf
	@param point
	@return what the card row should read: the description, failing that
	the tag, failing that nothing at all - and nothing at all means apply()
	writes nothing, rather than blanking a row with an empty string.
*/
QString IoAssignment::functionTextOf(const IoPoint &point)
{
	if (!point.description.isEmpty()) {
		return point.description;
	}
	return point.tag;
}

/**
	@brief IoAssignment::pointLabel
	@param point
	@return how a message names that point: its tag, failing that its
	address, failing that its description, failing that its id - which is
	never empty, so a message always has something to say.
*/
QString IoAssignment::pointLabel(const IoPoint &point)
{
	if (!point.tag.isEmpty()) {
		return point.tag;
	}
	if (!point.address.isEmpty()) {
		return point.address;
	}
	if (!point.description.isEmpty()) {
		return point.description;
	}
	return point.id;
}

/**
	@brief IoAssignment::refusalText
	@param reason
	@param label how the point is named
	@return the refusal said in a sentence
*/
QString IoAssignment::refusalText(Refusal reason, const QString &label)
{
	switch (reason)
	{
		case PointNotFound:
			return tr("%1 : ce point n'est plus dans la liste.")
			       .arg(label);
		case AlreadyAssigned:
			return tr("%1 : déjà affecté à une voie.")
			       .arg(label);
		case NoFreeChannel:
			return tr("%1 : la carte n'a plus de voie libre de ce type.")
			       .arg(label);
		case NoRefusal:
			break;
	}
	return QString();
}
