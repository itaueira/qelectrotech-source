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
#ifndef MOUNTINGLAYOUT_H
#define MOUNTINGLAYOUT_H

#include "enclosuretransfer.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

class QDomDocument;
class QDomElement;

/**
	@brief One face things are screwed to - a mounting plate, a door, a side
	panel - and everything mounted on it.

	The area and the list are the two halves of it, and both are in
	millimetre, in the frame MountingArea declares: x runs right, y runs
	down, origin at the top left corner of the face. Nothing here holds a
	scale factor, and nothing here multiplies by one: pixels belong to
	whoever paints, and a factor stored next to a length is a length nobody
	can compare with a panel any more.

	The face is tied to a location of the project by the path of codes -
	QCM1/PORTE - and never by geometry. That is the same tie a component
	carries, for the same reason: it survives a file round trip, it reads as
	itself in the list the storeroom is handed, and it does not make the
	layout depend on where anybody drew a rectangle.

	kind says which face of the location this is. It is a token and not a
	sentence, so that the file stays readable and the wording stays with
	whoever shows it. Two faces of the same kind in one location are allowed
	on purpose: a tall enclosure really does have an upper and a lower
	mounting plate, and a cabinet really does have a left and a right side
	panel. What tells those two apart is name, which is a person's words.

	The area may be unusable, and that is a state and not an error. A
	location whose useful mounting area nobody has measured yet carries the
	face, carries what is mounted on it, and answers "not measured" when
	asked how big it is - which is exactly what the catalogue was refused
	permission to guess. Every item then reads MountingFit::NoArea, which is
	a report and not a loss.
*/
class MountingSurface
{
	Q_DECLARE_TR_FUNCTIONS(MountingSurface)

	public:
		MountingSurface() {}
		MountingSurface(const QString &surface_location_path,
				const QString &surface_kind,
				const MountingArea &surface_area = MountingArea());

			/// @return true when nothing here says which face this is
		bool isNull() const;
		int itemCount() const;
			/// @return where @a item_uuid sits in items, -1 when it is not here
		int indexOfItem(const QString &item_uuid) const;
			/// @return the item of @a item_uuid, a default one when there is none
		MountedItem item(const QString &item_uuid) const;
		QStringList itemUuids() const;

		/**
			@brief What changing this face to @a new_area would do.
			@param new_area the face of the enclosure taking its place
			@return one entry per item, none of them dropped

			The one call this whole module exists to make. The rule
			that answers it is arithmetic that was written, tested and
			then had no caller at all - so every position it preserves
			and every item it names was, until here, a promise about a
			layout the program could not hold.
		*/
		EnclosureTransferPlan planFor(const MountingArea &new_area) const;

		/**
			@return how to call this face out loud: its name, failing
			that its location and kind, failing those its identifier.
			Never empty, for the reason MountedItem::designation is
			never empty.
		*/
		QString designation() const;

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
			/// @return the tag one mounted item is written under
		static QString itemTagName();
			/// @return the face a file that names none is read as
		static QString defaultKind();

		bool operator==(const MountingSurface &other) const;
		bool operator!=(const MountingSurface &other) const;

			/// identity, given when the face enters a layout
		QString uuid;
			/// the location this face belongs to, as a path of codes
		QString location_path;
			/// which face of that location: plate, door, side
		QString kind;
			/// what a person calls it
		QString name;
			/// how much room there is to mount on, millimetre
		MountingArea area;
			/// what is screwed to it, millimetre, in no special order
		QList<MountedItem> items;
};

/**
	@brief Everything this project mounts things on, and what is mounted
	where.

	The layout belongs to the project for the reason the tree of locations
	does: a mounting plate outlives every window that ever showed it, and a
	panel is frequently laid out before its first folio is drawn. It is
	stored flat, each face naming its location by path, because that is what
	survives a file round trip and what makes moving a face one assignment.

	Written to the .qet only when it holds a face, so a project that never
	opened the layout keeps opening in an unmodified QElectroTech.

	One identifier, one place: an item uuid is mounted on at most one face of
	the whole layout, and mounting it again is refused rather than
	duplicated. That is the invariant worth having, because the uuid is what
	stitches a mounted item to the symbol of the same component on a folio -
	the same component screwed to two plates at once would be counted twice
	in the bill of material and pointed at from two places.

	What is deliberately not refused: an item nobody has measured, and an
	item that does not fit where it was put. Both are states a person passes
	through while laying a panel out, and both are reported by the rule -
	refusing them here would be this file deciding that what it cannot
	describe does not exist, which is the failure the family of classes
	underneath it was written against.

	Reading is tolerant and writing is strict. A file may hold two faces with
	one identifier, an item with none, a face belonging to nothing; the read
	repairs all of it and keeps everything, because a project that opens with
	a repaired layout is worth more than a project that does not open. The
	mutators refuse instead, and say why.

	This file is arithmetic, text and QDomDocument. No project, no element,
	no widget - which is what lets a change of enclosure be proved on a
	bench.
*/
class MountingLayout
{
	Q_DECLARE_TR_FUNCTIONS(MountingLayout)

	public:
		MountingLayout();

			/// @return how many faces the project mounts on
		int count() const;
		bool isEmpty() const;
		void clear();
			/// @return how many items are mounted, every face added up
		int itemCount() const;

		const MountingSurface &at(int index) const;
			/// @return the face of @a surface_uuid, a default one when there is none
		MountingSurface surface(const QString &surface_uuid) const;
		int indexOfSurface(const QString &surface_uuid) const;
		QStringList surfaceUuids() const;
			/// @return the faces of one location, in the order they were added
		QStringList surfacesOfLocation(const QString &location_path) const;

		/**
			@brief Put a face in the layout.
			@param surface the face, its identifier given here when it
			has none, and the identifier of every item on it likewise
			@param error filled with why nothing was added
			@return the identifier of the face added, empty when refused

			Refuses a face belonging to no location, a face that does
			not say which face it is, an identifier the layout already
			uses, and an item already mounted somewhere else - the four
			things that would leave the layout unable to answer where a
			component is.
		*/
		QString appendSurface(MountingSurface surface,
				      QString *error = nullptr);

		/**
			@brief Write a face back, area, name and items included.
			@param surface the face as it should now be, matched by uuid
			@param error filled with why nothing was written
			@return true when the layout was changed

			One entry point, for the reason LocationTree::update is
			one: an undo command wants one operation and not four.
		*/
		bool updateSurface(const MountingSurface &surface,
				   QString *error = nullptr);

		/**
			@brief Take a face out of the layout.
			@param surface_uuid which face
			@param unmounted filled with the identifier of every item
			that was mounted on it
			@return true when something was removed

			What becomes of the components that were mounted there is
			not decided here: the caller is handed their identifiers
			and answers for them, because deleting a face must not
			delete a component from the project - it only stops saying
			where that component is screwed.
		*/
		bool removeSurface(const QString &surface_uuid,
				   QStringList *unmounted = nullptr);

		/**
			@brief Screw an item to a face.
			@param surface_uuid which face
			@param item the item, its identifier given here when it has
			none
			@param error filled with why nothing was mounted
			@return the identifier of the item mounted, empty when refused
		*/
		QString mountItem(const QString &surface_uuid,
				  MountedItem item,
				  QString *error = nullptr);

		/**
			@brief Write an item back where it is already mounted.
			@param item the item as it should now be, matched by uuid
			@param error filled with why nothing was written
			@return true when the layout was changed
		*/
		bool updateItem(const MountedItem &item, QString *error = nullptr);

		/**
			@brief Move an item from the face it is on to another one.
			@param item_uuid which item
			@param surface_uuid which face it goes to
			@param error filled with why nothing was moved
			@return true when the layout was changed

			The position travels with it untouched, in millimetre. A
			door and a plate have their own origins, so the same pair of
			numbers means another place on the other face - and guessing
			a new position here would be this file laying the panel out,
			which is a person's decision.
		*/
		bool moveItem(const QString &item_uuid,
			      const QString &surface_uuid,
			      QString *error = nullptr);

			/// @return true when the item was mounted and no longer is
		bool unmountItem(const QString &item_uuid);

			/// @return the item of @a item_uuid wherever it is mounted
		MountedItem item(const QString &item_uuid) const;
			/// @return the face @a item_uuid is mounted on, empty when none
		QString surfaceOfItem(const QString &item_uuid) const;
		bool holdsItem(const QString &item_uuid) const;
			/// @return every item of the layout, face by face
		QStringList mountedItemUuids() const;

		/**
			@brief What changing one face to @a new_area would do.
			@param surface_uuid which face
			@param new_area the face of the enclosure taking its place
			@return the plan, an empty one when there is no such face

			Asked before anything is applied, which is the whole point:
			the loss is reported while it can still be refused.
		*/
		EnclosureTransferPlan planForSurface(const QString &surface_uuid,
						     const MountingArea &new_area) const;

		/**
			@brief Change the room one face has, and keep what is on it.
			@param surface_uuid which face
			@param new_area how much room it has now, millimetre
			@param plan filled with what the change did, always, the
			refusal included
			@param error filled with why nothing was written
			@return true when the layout was changed

			Nothing is ever removed here. An item that no longer fits
			stays exactly where it is and is named in the plan, because
			a layout lost to a number typed into a box is the failure
			the rule underneath was written against. Applying anyway is
			the caller's decision to take, and it can only take it after
			reading what it costs.

			An area with no usable dimension is refused, and the plan is
			handed back all the same: it then says NoArea about
			everything, which is the answer the caller has to show.
		*/
		bool applyArea(const QString &surface_uuid,
			       const MountingArea &new_area,
			       EnclosureTransferPlan *plan = nullptr,
			       QString *error = nullptr);

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
		static QString newId();

		bool operator==(const MountingLayout &other) const;
		bool operator!=(const MountingLayout &other) const;

	private:
		int indexOfItemOwner(const QString &item_uuid) const;
		bool takenUuid(const QString &uuid,
			       const QString &except_surface_uuid = QString()) const;

		QVector<MountingSurface> m_surfaces;
};

#endif // MOUNTINGLAYOUT_H
