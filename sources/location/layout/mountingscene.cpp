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
#include "mountingscene.h"

#include "../../undocommand/alignmountedpartscommand.h"
#include "../../undocommand/mountpartcommand.h"
#include "../../undocommand/movemountedpartcommand.h"
#include "../../undocommand/movemountedrailcommand.h"
#include "../../undocommand/stretchmountedprofilecommand.h"
#include "../mountingclip.h"
#include "../mountingmeasure.h"
#include "mountedpartitem.h"
#include "mountedprofileitem.h"

#include <QColor>
#include <QGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QUndoCommand>

#include <cmath>
#include <limits>
#include <utility>

namespace
{
	/// millimetre: the spacing of the grid drawn over the face
	const qreal GRID_STEP = 10.0;

	/// millimetre: how much room is left around the face, so a part can be
	/// dragged off the plate and still be seen sitting off it
	const qreal SURROUNDING_MARGIN = 20.0;

	/// below this zoom the grid is a grey wash rather than a grid, so it is
	/// not drawn at all
	const qreal GRID_VISIBLE_ZOOM = 0.2;

	/**
		@brief isAPosition
		@param position a position in millimetre
		@return true when both numbers are numbers

		A position that is not one is refused rather than repaired here:
		this is the mutator, and the mutators refuse and say why. The
		tolerant repair belongs to the read.
	*/
	bool isAPosition(const QPointF &position)
	{
		return std::isfinite(position.x()) && std::isfinite(position.y());
	}

	/**
		@brief isAFootprint
		@param footprint the room something takes, millimetre
		@return true when all four numbers are numbers

		A size of zero passes, and that is not an oversight: a piece
		nobody has cut yet takes no room, and the rules underneath
		already read that as "not measured". What is refused here is a
		number that is not one.
	*/
	bool isAFootprint(const QRectF &footprint)
	{
		return std::isfinite(footprint.x())
		       && std::isfinite(footprint.y())
		       && std::isfinite(footprint.width())
		       && std::isfinite(footprint.height());
	}
}

/**
	@brief MountingScene::MountingScene
	@param parent parent QObject
*/
MountingScene::MountingScene(QObject *parent) :
	QGraphicsScene(parent)
{
	updateSceneRect();
}

MountingScene::~MountingScene()
{}

/**
	@brief MountingScene::setSurface
	@param surface the face and everything mounted on it
*/
void MountingScene::setSurface(const MountingSurface &surface)
{
	m_surface = surface;
	rebuild();
	m_undo_stack.clear();
	updateSceneRect();
	emit surfaceChanged();
}

/**
	@brief MountingScene::surface
	@return the face as it now stands, positions read off the drawing

	Read off the items and not off the copy kept here, so that what this
	answers is what a person is looking at - the middle of a drag or of a
	cut included. Two truths about where a part is would be one truth too
	many.

	The size is taken from the drawing as well as the corner, and that is
	what makes a piece in the middle of being cut read as the length it is
	being cut to. It is the size of the MODEL the item carries and never the
	rectangle it paints - the hatched square of a part nobody has measured
	stays on the screen, and the size in the file stays absent.

	The rest of the item is left alone on purpose. Two entries of one list
	may name the same identity - a file built by hand can say so, and the
	read repairs rather than refuses - and one item drawn for both of them
	would then answer for both. Overwriting the two entries whole would make
	the second one lose its mark and its profile; overwriting the geometry
	only keeps the damage where it already was.
*/
MountingSurface MountingScene::surface() const
{
	MountingSurface surface = m_surface;

	for (int index = 0 ; index < surface.items.size() ; ++ index)
	{
		MountedPartItem *part = partItem(surface.items.at(index).uuid);
		if (part)
		{
			surface.items[index].position = part->millimetrePosition();
			surface.items[index].size = part->mountedItem().size;
		}
	}

	return surface;
}

/**
	@brief MountingScene::area
	@return how much room there is to mount on, millimetre
*/
MountingArea MountingScene::area() const
{
	return m_surface.area;
}

/**
	@brief MountingScene::isAreaMeasured
	@return true when the area of the face is a usable pair of lengths

	An unmeasured face is a state and not an error: the parts are drawn, and
	nothing is drawn under them. That is honest - the program was refused
	permission to guess how much room a cabinet has - and it is also what
	the person sees when a location has been created and not yet measured.
*/
bool MountingScene::isAreaMeasured() const
{
	return m_surface.area.isValid();
}

/**
	@brief MountingScene::partCount
	@return how many parts are drawn
*/
int MountingScene::partCount() const
{
	return m_items.size();
}

/**
	@brief MountingScene::itemUuids
	@return the identity of every part drawn
*/
QStringList MountingScene::itemUuids() const
{
	QStringList uuids;

	for (const MountedItem &item : std::as_const(m_surface.items))
	{
		if (m_items.contains(item.uuid)) {
			uuids << item.uuid;
		}
	}

	return uuids;
}

/**
	@brief MountingScene::partItem
	@param item_uuid which part
	@return the item drawn for it, nullptr when there is none
*/
MountedPartItem *MountingScene::partItem(const QString &item_uuid) const
{
	return m_items.value(item_uuid, nullptr);
}

/**
	@brief MountingScene::mountedItem
	@param item_uuid which part
	@return what is mounted under it, a default one when nothing is
*/
MountedItem MountingScene::mountedItem(const QString &item_uuid) const
{
	MountedPartItem *part = partItem(item_uuid);

	return part ? part->mountedItem() : MountedItem();
}

/**
	@brief MountingScene::indexOfItem
	@param item_uuid which part
	@return where it sits in the face, -1 when it is not on it
*/
int MountingScene::indexOfItem(const QString &item_uuid) const
{
	return m_surface.indexOfItem(item_uuid);
}

/**
	@brief MountingScene::mountItem
	@param item what is mounted, millimetre
	@param error filled with why nothing was mounted
	@return true when a step was pushed on the stack
*/
bool MountingScene::mountItem(const MountedItem &item, QString *error)
{
	if (error) {
		error->clear();
	}

	if (item.uuid.isEmpty())
	{
			//Refused rather than given one here. The identity is
			//what the whole layout stitches a component to, and a
			//drawing that made one up would be handing out a second
			//identity for a part that already has one somewhere
			//else in the project.
		if (error) {
			*error = tr("Un composant posé sur une platine a besoin "
				    "d'un identifiant.");
		}
		return false;
	}

	if (m_items.contains(item.uuid))
	{
		if (error) {
			*error = tr("« %1 » est déjà posé sur cette platine.")
				 .arg(item.designation());
		}
		return false;
	}

	if (!isAPosition(item.position))
	{
		if (error) {
			*error = tr("Une position est deux nombres en "
				    "millimètre");
		}
		return false;
	}

	MountPartCommand *command =
			new MountPartCommand(this, item, MountPartCommand::Mount);

	if (command->isNull())
	{
		delete command;
		return false;
	}

	m_undo_stack.push(command);
	return true;
}

/**
	@brief MountingScene::unmountItem
	@param item_uuid which part
	@param error filled with why nothing was taken off
	@return true when a step was pushed on the stack
*/
bool MountingScene::unmountItem(const QString &item_uuid, QString *error)
{
	if (error) {
		error->clear();
	}

	MountedPartItem *part = partItem(item_uuid);
	if (!part)
	{
		if (error) {
			*error = tr("Aucun composant de cet identifiant sur "
				    "cette platine");
		}
		return false;
	}

	MountPartCommand *command =
			new MountPartCommand(this, part->mountedItem(),
					     MountPartCommand::Unmount,
					     indexOfItem(item_uuid));

	if (command->isNull())
	{
		delete command;
		return false;
	}

	m_undo_stack.push(command);
	return true;
}

/**
	@brief MountingScene::stretchItem
	@param item_uuid which piece
	@param footprint_mm the room it takes afterwards, millimetre
	@param error filled with why nothing was cut
	@return true when a step was pushed on the stack
*/
bool MountingScene::stretchItem(const QString &item_uuid,
				const QRectF &footprint_mm,
				QString *error)
{
	if (error) {
		error->clear();
	}

	MountedPartItem *part = partItem(item_uuid);
	if (!part)
	{
		if (error) {
			*error = tr("Aucun composant de cet identifiant sur "
				    "cette platine");
		}
		return false;
	}

	const MountedItem mounted = part->mountedItem();
	if (!mounted.isCutToLength())
	{
			//A bought part is the size the catalogue says it is.
			//Letting a drawing change that would make the plate and
			//the product disagree, and the plate is not the one the
			//workshop unpacks.
		if (error) {
			*error = tr("« %1 » est un article du catalogue : il se "
				    "pose, il ne se coupe pas.")
				 .arg(mounted.designation());
		}
		return false;
	}

	if (!isAFootprint(footprint_mm))
	{
		if (error) {
			*error = tr("Une coupe est quatre nombres en "
				    "millimètre");
		}
		return false;
	}

	StretchMountedProfileCommand *command =
			new StretchMountedProfileCommand(this, item_uuid,
							 mounted.footprint(),
							 footprint_mm);

	if (command->isNull())
	{
		delete command;
		return false;
	}

	m_undo_stack.push(command);
	return true;
}

/**
	@brief MountingScene::moveItem
	@param item_uuid which part
	@param position_mm where its top left corner goes, millimetre
	@param error filled with why nothing was moved
	@return true when a step was pushed on the stack
*/
bool MountingScene::moveItem(const QString &item_uuid,
			     const QPointF &position_mm,
			     QString *error)
{
	MountedPartItem *part = partItem(item_uuid);
	if (!part)
	{
		if (error) {
			*error = tr("Aucun composant de cet identifiant sur "
				    "cette platine");
		}
		return false;
	}

	if (!isAPosition(position_mm))
	{
		if (error) {
			*error = tr("Une position est deux nombres en "
				    "millimètre");
		}
		return false;
	}

	return pushMove(item_uuid, part->millimetrePosition(), position_mm);
}

/**
	@brief MountingScene::setPartAxes
	@param axis_by_part_code the axis offset of each product code, millimetre
*/
void MountingScene::setPartAxes(const QHash<QString, QPointF> &axis_by_part_code)
{
	m_axes = axis_by_part_code;
}

/**
	@brief MountingScene::partAxes
	@return where the axis of each product sits inside its own body
*/
QHash<QString, QPointF> MountingScene::partAxes() const
{
	return m_axes;
}

/**
	@brief MountingScene::carrierOf
	@param item_uuid which part
	@return the identity of the rail that carries it, empty when none does

	The part is looked up before the question is asked, and that guard is
	not decoration: an identity nothing is drawn for reads back as an empty
	MountedItem, which sits at the origin - and the origin is a place a rail
	can very well be. Without the guard, asking about a part that is not
	there would name whatever rail passes through the top left corner of the
	plate.
*/
QString MountingScene::carrierOf(const QString &item_uuid) const
{
	if (!partItem(item_uuid)) {
		return QString();
	}

	return MountingClip::carrierOf(mountedItem(item_uuid),
				       itemsWith(QString(), QPointF()),
				       m_axes);
}

/**
	@brief MountingScene::carriedBy
	@param rail_uuid which rail
	@return what it carries where it stands, in order along it
*/
QStringList MountingScene::carriedBy(const QString &rail_uuid) const
{
	MountedPartItem *rail = partItem(rail_uuid);
	if (!rail) {
		return QStringList();
	}

	return carriedBy(rail_uuid, rail->millimetrePosition());
}

/**
	@brief MountingScene::carriedBy
	@param rail_uuid which rail
	@param rail_position_mm where its top left corner would be, millimetre
	@return what it would carry from there, in order along it
*/
QStringList MountingScene::carriedBy(const QString &rail_uuid,
				     const QPointF &rail_position_mm) const
{
	if (!partItem(rail_uuid) || !isAPosition(rail_position_mm)) {
		return QStringList();
	}

	return MountingClip::carried(rail_uuid,
				     itemsWith(rail_uuid, rail_position_mm),
				     m_axes);
}

/**
	@brief MountingScene::clipTarget
	@param item_uuid which part
	@return where it would land if it were clipped where it stands
*/
QPointF MountingScene::clipTarget(const QString &item_uuid) const
{
	if (!partItem(item_uuid))
	{
		const qreal not_a_number =
				std::numeric_limits<qreal>::quiet_NaN();

		return QPointF(not_a_number, not_a_number);
	}

	return MountingClip::clippedPosition(mountedItem(item_uuid),
					     itemsWith(QString(), QPointF()),
					     m_axes);
}

/**
	@brief MountingScene::clipItem
	@param item_uuid which part
	@param error filled with why nothing was clipped
	@return true when a step was pushed on the stack
*/
bool MountingScene::clipItem(const QString &item_uuid, QString *error)
{
		//Emptied first, so that "false with no reason" is a state a
		//caller can read rather than a sentence in a comment: what is
		//left in there otherwise is whatever the caller last asked
		//about.
	if (error) {
		error->clear();
	}

	MountedPartItem *part = partItem(item_uuid);
	if (!part)
	{
		if (error) {
			*error = tr("Aucun composant de cet identifiant sur "
				    "cette platine");
		}
		return false;
	}

	if (carrierOf(item_uuid).isEmpty())
	{
		if (error) {
			*error = tr("Aucun rail ne passe sous ce composant");
		}
		return false;
	}

		//No reason given when this returns false from here on: the part
		//is already where the rail holds it, which is the same "nothing
		//to do" moveItem reports with an empty error.
	return pushMove(item_uuid, part->millimetrePosition(),
			clipTarget(item_uuid));
}

/**
	@brief MountingScene::selectedUuids
	@return the identity of every part selected on this scene
*/
QStringList MountingScene::selectedUuids() const
{
	QStringList uuids;

	const QList<QGraphicsItem *> chosen = selectedItems();

	for (QGraphicsItem *item : chosen)
	{
			//Both type numbers, and not qgraphicsitem_cast to the
			//base: that cast compares the type number for equality,
			//so a piece cut to length - which IS a MountedPartItem,
			//with a type number of its own - would come back null
			//and every rail would silently drop out of the
			//selection. Whether a rail takes part in a gesture is
			//the rule's decision, and it cannot make it about a
			//rail it was never given.
		if (!item || (item->type() != MountedPartItem::Type
			      && item->type() != MountedProfileItem::Type)) {
			continue;
		}

		MountedPartItem *part = static_cast<MountedPartItem *>(item);

		if (!part->uuid().isEmpty()) {
			uuids << part->uuid();
		}
	}

	return uuids;
}

/**
	@brief MountingScene::alignItems
	@param item_uuids which parts
	@param alignment which line they end up sharing
	@param error filled with why nothing was lined up
	@return true when a step was pushed on the stack
*/
bool MountingScene::alignItems(const QStringList &item_uuids,
			       MountingAlignment alignment,
			       QString *error)
{
	if (error) {
		error->clear();
	}

	const QList<MountedItem> chosen = itemsOf(item_uuids);
	const QStringList movable = MountingAlign::movableUuids(chosen);

	if (movable.count() < 2)
	{
		if (error) {
			*error = tr("Il faut au moins deux composants pour les "
				    "aligner ; les rails et les goulottes n'en "
				    "sont pas.");
		}
		return false;
	}

	const QHash<QString, QPointF> targets =
			MountingAlign::aligned(chosen, alignment);

		//Empty and no reason: they were all on the line already, which
		//is not a failure and must not leave a step on the stack.
	if (targets.isEmpty()) {
		return false;
	}

	AlignMountedPartsCommand *command =
			new AlignMountedPartsCommand(this, targets,
						     alignCaption(alignment,
								  targets.count()));

	if (command->isNull())
	{
		delete command;
		return false;
	}

	m_undo_stack.push(command);
	return true;
}

/**
	@brief MountingScene::distributeItems
	@param item_uuids which parts
	@param run along which axis they are spread
	@param error filled with why nothing was spread
	@return true when a step was pushed on the stack
*/
bool MountingScene::distributeItems(const QStringList &item_uuids,
				    MountingRun run,
				    QString *error)
{
	if (error) {
		error->clear();
	}

	const QList<MountedItem> chosen = itemsOf(item_uuids);
	const QStringList movable = MountingAlign::movableUuids(chosen);

	if (movable.count() < 3)
	{
		if (error) {
			*error = tr("Il faut au moins trois composants pour les "
				    "répartir : les deux du bout ne bougent "
				    "pas.");
		}
		return false;
	}

	const qreal gap = MountingAlign::spreadGap(chosen, run);

		//The arithmetic has an answer for a row that does not fit - an
		//evenly overlapping one - and that answer is refused here
		//rather than shown. A uniform overlap is the one wrong drawing
		//that looks deliberate.
	if (!std::isfinite(gap))
	{
		if (error) {
			*error = tr("Ces composants ne se répartissent pas : "
				    "l'un d'eux n'est pas à une position "
				    "mesurable.");
		}
		return false;
	}

	if (gap < 0.0)
	{
			//How much room is missing, and not how negative the gap
			//is: what a person can act on is the millimetre they
			//have to make, and that is the gap times the number of
			//spaces there are between the parts.
		const qreal missing = -gap * qreal(movable.count() - 1);

		if (error) {
			*error = tr("Ces composants ne tiennent pas dans la "
				    "place qu'ils occupent : il manque %1 mm.")
				 .arg(missing, 0, 'f', 1);
		}
		return false;
	}

	const QHash<QString, QPointF> targets =
			MountingAlign::spread(chosen, run);

	if (targets.isEmpty()) {
		return false;
	}

	AlignMountedPartsCommand *command =
			new AlignMountedPartsCommand(this, targets,
						     spreadCaption(run,
								   targets.count()));

	if (command->isNull())
	{
		delete command;
		return false;
	}

	m_undo_stack.push(command);
	return true;
}

/**
	@brief MountingScene::itemsOf
	@param item_uuids which parts
	@return what is drawn for each of them, read off the drawing

	Silently short of what was asked for when an identity names nothing:
	the rules underneath count what they were given, and a default item
	standing in for a part that is not there would be counted as a part at
	the origin.
*/
QList<MountedItem> MountingScene::itemsOf(const QStringList &item_uuids) const
{
	QList<MountedItem> chosen;
	QStringList taken;

	for (const QString &item_uuid : item_uuids)
	{
		if (item_uuid.isEmpty() || taken.contains(item_uuid)
		    || !partItem(item_uuid)) {
			continue;
		}

		taken << item_uuid;
		chosen << mountedItem(item_uuid);
	}

	return chosen;
}

/**
	@brief MountingScene::alignCaption
	@param alignment which line the parts end up sharing
	@param count how many of them move
	@return what the person reads in the undo list
*/
QString MountingScene::alignCaption(MountingAlignment alignment,
				    int count) const
{
	switch (alignment)
	{
		case MountingAlignment::LeftEdges:
			return tr("Aligner %1 composant(s) à gauche")
			       .arg(count);
		case MountingAlignment::RightEdges:
			return tr("Aligner %1 composant(s) à droite")
			       .arg(count);
		case MountingAlignment::TopEdges:
			return tr("Aligner %1 composant(s) en haut").arg(count);
		case MountingAlignment::BottomEdges:
			return tr("Aligner %1 composant(s) en bas").arg(count);
		case MountingAlignment::VerticalAxes:
			return tr("Centrer %1 composant(s) verticalement")
			       .arg(count);
		case MountingAlignment::HorizontalAxes:
			return tr("Centrer %1 composant(s) horizontalement")
			       .arg(count);
	}

	return tr("Aligner %1 composant(s) sur la platine").arg(count);
}

/**
	@brief MountingScene::spreadCaption
	@param run along which axis the parts are spread
	@param count how many of them move
	@return what the person reads in the undo list
*/
QString MountingScene::spreadCaption(MountingRun run, int count) const
{
	if (run == MountingRun::Down) {
		return tr("Répartir %1 composant(s) verticalement").arg(count);
	}

	return tr("Répartir %1 composant(s) horizontalement").arg(count);
}

/**
	@brief MountingScene::itemsWith
	@param moved_uuid which part is displaced, empty for none
	@param position_mm where its top left corner is put, millimetre
	@return everything on the face, read off the drawing
*/
QList<MountedItem> MountingScene::itemsWith(const QString &moved_uuid,
					    const QPointF &position_mm) const
{
	QList<MountedItem> items = surface().items;

	if (moved_uuid.isEmpty()) {
		return items;
	}

	for (int index = 0 ; index < items.size() ; ++ index)
	{
		if (items.at(index).uuid == moved_uuid) {
			items[index].position = position_mm;
		}
	}

	return items;
}

/**
	@brief MountingScene::pushMove
	@param item_uuid which part
	@param before_mm where it was, millimetre
	@param after_mm where it goes, millimetre
	@return true when a step was pushed on the stack

	The one place a move reaches the stack, whether it came from a mouse or
	from a number. What counts as no move at all is asked of the command
	itself: two places deciding that would be two places to keep in step,
	and the day they disagree the stack grows a step that undoes nothing.

	It is also the one place a rail is told apart from everything else, and
	that is why the two paths - a number typed and a mouse let go - can
	never disagree about whether the breakers travelled. What the rail
	carries is asked of where it WAS: by the time a drag ends, the rail is
	already at its new place and covers nothing it left behind.
*/
bool MountingScene::pushMove(const QString &item_uuid,
			     const QPointF &before_mm,
			     const QPointF &after_mm)
{
	const QStringList carried = carriedBy(item_uuid, before_mm);

	QUndoCommand *command = nullptr;

	if (carried.isEmpty())
	{
		MoveMountedPartCommand *move =
				new MoveMountedPartCommand(this, item_uuid,
							   before_mm, after_mm);

		if (move->isNull())
		{
			delete move;
			return false;
		}

		command = move;
	}
	else
	{
		MoveMountedRailCommand *move =
				new MoveMountedRailCommand(this, item_uuid,
							   before_mm, after_mm,
							   carried);

		if (move->isNull())
		{
			delete move;
			return false;
		}

		command = move;
	}

	m_undo_stack.push(command);
	return true;
}

/**
	@brief MountingScene::applyItemPosition
	@param item_uuid which part
	@param position_mm where its top left corner goes, millimetre
	@return true when there was such a part
*/
bool MountingScene::applyItemPosition(const QString &item_uuid,
				      const QPointF &position_mm)
{
	MountedPartItem *part = partItem(item_uuid);
	if (!part || !isAPosition(position_mm)) {
		return false;
	}

	part->setMillimetrePosition(position_mm);

	const int index = m_surface.indexOfItem(item_uuid);
	if (index >= 0) {
		m_surface.items[index].position = part->millimetrePosition();
	}

	updateSceneRect();
	emit itemMoved(item_uuid);
	emit surfaceChanged();

	return true;
}

/**
	@brief MountingScene::applyItemMounting
	@param item what is mounted, millimetre
	@param index where it goes in the list of the face, -1 for the end
	@return true when it was drawn
*/
bool MountingScene::applyItemMounting(const MountedItem &item, int index)
{
	if (item.uuid.isEmpty() || m_items.contains(item.uuid)) {
		return false;
	}

	const int count = int(m_surface.items.count());
	const int at = (index >= 0 && index <= count) ? index : count;

	m_surface.items.insert(at, item);
	drawItem(item);

	updateSceneRect();
	emit itemMounted(item.uuid);
	emit surfaceChanged();

	return true;
}

/**
	@brief MountingScene::applyItemUnmounting
	@param item_uuid which part
	@return true when there was such a part

	Taken off the scene before it is deleted, and not the other way round:
	leaving the scene is what makes the item drop anything it had drawn
	beside itself - the handles of a piece that was selected - and a handle
	left behind on a scene whose item has gone is a blue dot nothing can
	select and nothing can delete.
*/
bool MountingScene::applyItemUnmounting(const QString &item_uuid)
{
	MountedPartItem *part = m_items.value(item_uuid, nullptr);
	if (!part) {
		return false;
	}

	m_items.remove(item_uuid);
	removeItem(part);
	delete part;

	const int index = m_surface.indexOfItem(item_uuid);
	if (index >= 0) {
		m_surface.items.removeAt(index);
	}

	updateSceneRect();
	emit itemUnmounted(item_uuid);
	emit surfaceChanged();

	return true;
}

/**
	@brief MountingScene::applyItemGeometry
	@param item_uuid which part
	@param footprint_mm the room it takes, millimetre
	@return true when there was such a part
*/
bool MountingScene::applyItemGeometry(const QString &item_uuid,
				      const QRectF &footprint_mm)
{
	MountedPartItem *part = partItem(item_uuid);
	if (!part || !isAFootprint(footprint_mm)) {
		return false;
	}

	const QRectF piece = footprint_mm.normalized();

	MountedItem item = part->mountedItem();
	item.position = piece.topLeft();
	item.size     = piece.size();
	part->setMountedItem(item);

	const int index = m_surface.indexOfItem(item_uuid);
	if (index >= 0) {
		m_surface.items[index] = part->mountedItem();
	}

	updateSceneRect();
	emit itemStretched(item_uuid);
	emit surfaceChanged();

	return true;
}

/**
	@brief MountingScene::undoStack
	@return the stack the moves of this surface are on
*/
QUndoStack &MountingScene::undoStack()
{
	return m_undo_stack;
}

/**
	@brief MountingScene::sceneFromMillimetre
	@param position_mm a position in millimetre
	@return the same position
*/
QPointF MountingScene::sceneFromMillimetre(const QPointF &position_mm)
{
	return position_mm;
}

/**
	@brief MountingScene::millimetreFromScene
	@param scene_position a position on this scene
	@return the millimetre it stands for
*/
QPointF MountingScene::millimetreFromScene(const QPointF &scene_position)
{
	return scene_position;
}

/**
	@brief MountingScene::gridStep
	@return the spacing of the grid drawn on the face, millimetre
*/
qreal MountingScene::gridStep()
{
	return GRID_STEP;
}

/**
	@brief MountingScene::surroundingMargin
	@return the room left around the face on the scene, millimetre
*/
qreal MountingScene::surroundingMargin()
{
	return SURROUNDING_MARGIN;
}

/**
	@brief MountingScene::drawBackground
	@param painter the painter to use
	@param rect the part of the scene to be drawn

	The face is painted here, as background, and this is the whole of the
	rule that says it cannot be moved: there is no item to move. The grid is
	a grid of millimetres and nothing snaps to it - it is a ruler laid on
	the plate, and a part put at 22,5 mm stays at 22,5 mm.
*/
void MountingScene::drawBackground(QPainter *painter, const QRectF &rect)
{
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, false);

	painter->setPen(Qt::NoPen);
	painter->setBrush(QColor(246, 246, 246));
	painter->drawRect(rect);

	if (isAreaMeasured())
	{
		const QRectF plate = m_surface.area.rect();

		QPen edge(QColor(80, 80, 80));
		edge.setCosmetic(true);
		painter->setPen(edge);
		painter->setBrush(QColor(255, 255, 255));
		painter->drawRect(plate);

		const QRectF grid_rect = plate.intersected(rect);
		if (!grid_rect.isEmpty()
		    && painter->transform().m11() > GRID_VISIBLE_ZOOM)
		{
			QPen grid(QColor(214, 214, 214));
			grid.setCosmetic(true);
			painter->setPen(grid);

			const qreal step = gridStep();
			for (qreal x = std::ceil(grid_rect.left() / step) * step ;
			     x <= grid_rect.right() ;
			     x += step)
			{
				painter->drawLine(QPointF(x, grid_rect.top()),
						  QPointF(x, grid_rect.bottom()));
			}
			for (qreal y = std::ceil(grid_rect.top() / step) * step ;
			     y <= grid_rect.bottom() ;
			     y += step)
			{
				painter->drawLine(QPointF(grid_rect.left(), y),
						  QPointF(grid_rect.right(), y));
			}
		}
	}

	painter->restore();
}

/**
	@brief MountingScene::rebuild
	Draw the face from scratch.

	A part with no identity is drawn and cannot be addressed afterwards: the
	layout gives one to everything it mounts, so this is what becomes of a
	surface somebody built by hand. A repeated identity keeps the first, for
	the same reason - the model refuses to mount the same part twice, and
	silently drawing the second over the first would hide the day it stops
	refusing.
*/
void MountingScene::rebuild()
{
	clear();
	m_items.clear();

	for (const MountedItem &item : std::as_const(m_surface.items)) {
		drawItem(item);
	}
}

/**
	@brief MountingScene::drawItem
	@param item what is mounted, millimetre
	@return the item drawn for it

	The one place a mounted thing becomes a drawing, and the one place that
	decides which kind of drawing it is: a piece cut to length gets the item
	that can be cut again, everything else gets the plain part. Deciding it
	here rather than in each caller is what keeps a rail mounted by an undo
	step identical to a rail mounted by a person - the day the two differed,
	one of them would have no handles and nobody would know why.
*/
MountedPartItem *MountingScene::drawItem(const MountedItem &item)
{
	MountedPartItem *part = nullptr;

	if (item.isCutToLength())
	{
		MountedProfileItem *piece = new MountedProfileItem(item);

		connect(piece, &MountedProfileItem::stretched, this,
			[this, piece](const QRectF &previous_footprint_mm)
			{
				endStretch(piece, previous_footprint_mm);
			});

		part = piece;
	}
	else {
		part = new MountedPartItem(item);
	}

	addItem(part);

	if (!item.uuid.isEmpty() && !m_items.contains(item.uuid)) {
		m_items.insert(item.uuid, part);
	}

	connect(part, &MountedPartItem::dragged, this,
		[this, part](const QPointF &previous_position_mm)
		{
			endDrag(part, previous_position_mm);
		});

	return part;
}

/**
	@brief MountingScene::updateSceneRect
	Keep the face and everything on it reachable.

	The parts are taken in as well as the plate, on purpose: a part that
	does not fit where it was put sits off the plate, and a scene rectangle
	that stopped at the edge of the plate would put it where nobody can
	scroll to it. Being off the plate is the report, and the report has to
	be visible.
*/
void MountingScene::updateSceneRect()
{
	QRectF rect;

	if (isAreaMeasured()) {
		rect = m_surface.area.rect();
	}
	rect = rect.united(itemsBoundingRect());

	if (rect.isNull()) {
		rect = QRectF(0.0, 0.0, gridStep(), gridStep());
	}

	setSceneRect(rect.adjusted(-surroundingMargin(), -surroundingMargin(),
				   surroundingMargin(), surroundingMargin()));
}

/**
	@brief MountingScene::endDrag
	@param part the part the person has just let go of
	@param previous_position_mm where it was when the gesture began

	The part has already moved by now, so the step pushed here is the one
	that takes it back. A part with no identity is left where it was
	dropped and no step is pushed: there is nothing to address it by, and a
	step that cannot name what it moves cannot undo it either.

	Letting go over a rail is what clips a part onto it, so where the drag
	ends and where the part is put are two different places, and the step
	pushed is the one that goes to the second. One step and not two: a drag
	followed by a clip would take two undos to get back from, and the person
	made one gesture.

	A drag that ends a nanometre from where it began pushes nothing - there
	would be nothing to undo - and the clip still has to happen, because a
	part released over a rail has to end up on it whether or not the stack
	has anything to say about it. That is the branch below, and it is the
	one nobody would write from the drawing alone.
*/
void MountingScene::endDrag(MountedPartItem *part,
			    const QPointF &previous_position_mm)
{
	if (!part) {
		return;
	}

	const QString item_uuid = part->uuid();
	if (item_uuid.isEmpty() || !m_items.contains(item_uuid)) {
		return;
	}

	const QPointF dropped = part->millimetrePosition();
	const QPointF target = clipTarget(item_uuid);

	if (pushMove(item_uuid, previous_position_mm, target)) {
		return;
	}

	if (!MountingMeasure::isSameLength(target.x(), dropped.x())
	    || !MountingMeasure::isSameLength(target.y(), dropped.y())) {
		applyItemPosition(item_uuid, target);
	}
}

/**
	@brief MountingScene::endStretch
	@param piece the piece somebody has just finished cutting
	@param previous_footprint_mm the room it took when the gesture began

	The same shape as the end of a drag, and for the same reason: the piece
	has already been cut by the time the mouse is let go, so the step pushed
	here is the one that takes the cut back. A piece with no identity is left
	as it was cut and no step is pushed - there is nothing to address it by,
	and a step that cannot name what it changed cannot undo it either.
*/
void MountingScene::endStretch(MountedProfileItem *piece,
			       const QRectF &previous_footprint_mm)
{
	if (!piece) {
		return;
	}

	const QString item_uuid = piece->uuid();
	if (item_uuid.isEmpty() || !m_items.contains(item_uuid)) {
		return;
	}

	StretchMountedProfileCommand *command =
			new StretchMountedProfileCommand(this, item_uuid,
							 previous_footprint_mm,
							 piece->footprintRect());

	if (command->isNull())
	{
		delete command;
		return;
	}

	m_undo_stack.push(command);
}
