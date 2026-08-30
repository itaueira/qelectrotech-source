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
#ifndef PROJECTLOCATION_H
#define PROJECTLOCATION_H

#include <QCoreApplication>
#include <QString>

class QDomDocument;
class QDomElement;

/**
	@brief One place a component can be mounted in: an enclosure, a plate
	inside it, a door, a remote box.

	QElectroTech has a notion of location already, but it is an attribute of
	a folio: locmach reads "Localisation (+)" in the folio properties, and
	that is what the title block prints. An enclosure is not a folio. It
	appears on fourteen of them, and the fourteen are not the enclosure - so
	the thing itself has to exist somewhere else, which is what this class
	is.

	Two fields carry the identity, and they are not the same field:

	- code is the IEC 81346 designation that goes on the drawing after the
	  plus sign, QCM1 or PORTE. It has to be unique among its siblings,
	  because it is what the path is built out of.
	- name is the sentence the storeroom reads.

	A location is also a catalogue part, of the class the seed model already
	creates - so its dimensions, its rating and its material are catalogue
	properties rather than fields added here. That is what gives the
	enclosure swap something to compare.
*/
class ProjectLocation
{
	Q_DECLARE_TR_FUNCTIONS(ProjectLocation)

	public:
		ProjectLocation();
		explicit ProjectLocation(const QString &location_code,
					 const QString &location_name = QString());

			/// @return true when there is no code, so nothing can point here
		bool isNull() const;

		/**
			@brief Whether this location is a line of the bill of material.

			Deliberately two states folded into one question, and not a
			deduction from the code alone. A door bought with its enclosure
			has a part code, was paid for, and still must not appear as a
			line - the enclosure line already contains it. Deducing "no code
			means virtual" would cover the sub-plate somebody has not typed
			yet and miss that door entirely.

			virtual_part therefore means: do not bill me, code or no code.
			The code stays recorded, because it serves the assembly even
			when it does not serve the purchase.
		*/
		bool isVirtual() const;

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();
			/// @return the character that joins two codes into a path
		static QString separator();

		/**
			@brief Whether a code can be used as one step of a path.
			@param location_code the code as typed
			@param error filled with what is wrong when it is not
			@return true when the code is usable

			Rejects the empty code, the path separator, and the four
			characters IEC 81346 gives its own meaning to - a code holding
			a plus sign would make +QCM1+PORTE impossible to read back.
		*/
		static bool isValidCode(const QString &location_code,
					QString *error = nullptr);

			/// @return the code with the spaces a person leaves behind folded away
		static QString sanitizeCode(const QString &location_code);

		bool operator==(const ProjectLocation &other) const;
		bool operator!=(const ProjectLocation &other) const;

	public:
			/**
				Given when the location enters a tree and never afterwards.
				A component points at the path and not at this - see the task
				file, decision C - but the tree itself moves and renames by
				uuid, so that renaming an enclosure is one operation and not a
				search for everything that happened to be called QCM1.
			*/
		QString uuid;
			/// the location this one sits inside, empty when it sits in nothing
		QString parent_uuid;

			/// the designation that follows the plus sign on the drawing
		QString code;
			/// what the storeroom reads
		QString name;
			/// anything about the place that the two fields above have no room for
		QString description;

			/// the catalogue part this location was bought as, empty when none
		QString part_code;
			/// which revision of it, 0 when whatever is current will do
		int part_revision = 0;
			/// true when this location must not produce a line of its own
		bool virtual_part = false;
};

#endif // PROJECTLOCATION_H
