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
#ifndef CATALOGVALUEORIGIN_H
#define CATALOGVALUEORIGIN_H

#include <QCoreApplication>
#include <QString>

/**
	@brief Which record a value shown for a part was written on.

	Catalog::effectiveValues answers what a part is worth and flattens away
	where each number was written: a width typed on the part and a width the
	part never had, taken from the initial value of the property, leave it as
	the same string. That is the right shape for a bill of material, which
	only has to print the number, and the wrong shape for a screen, which
	has to be able to say whether editing the class would change what is on
	it.

	@par Unread is not a synonym of Unset

	The difference is the one the catalogue already keeps between an empty
	cell and a zero, asked one level up: nobody looked is not the same
	answer as nobody filled it in. A measure read without the origins asked
	for carries Unread, so that a panel cannot answer "not filled in" to a
	question it never put.
*/
enum class CatalogValueSource
{
	Unread, ///< nothing was read about where the value comes from
	Unset,  ///< read, and neither the part nor a class of it holds one
	Part,   ///< typed on the part itself
	Class   ///< the initial value of the property, declared on a class
};

/**
	@brief Where the value of one key of one part was written, and on which
	class when it was inherited.

	The class is named and not only pointed at, because the sentence a panel
	has to be able to write is "clearance of 100 mm inherited from class
	Drive" - and a screen that has to reopen the catalogue to turn an
	identifier into that name is a screen that will show the identifier.

	@par The class named is the one that declares the property

	Not the class of the part. An initial value belongs to the declaration,
	and a declaration is inherited whole: a part of class Contactor whose
	width comes from a property declared on Component has to name Component,
	because Component is where somebody would go to change it. Naming the
	class of the part would send them to a screen with nothing to edit on
	it.
*/
class CatalogValueOrigin
{
	Q_DECLARE_TR_FUNCTIONS(CatalogValueOrigin)

	public:
		CatalogValueOrigin() {}

			/// @return the origin of a value typed on the part itself
		static CatalogValueOrigin fromPart();
		/**
			@brief The origin of a value taken from a declaration.
			@param declaring_class_id the class the property is
			declared on, and not the class of the part
			@param declaring_class_key its stable key
			@param declaring_class_name its user visible name
		*/
		static CatalogValueOrigin fromClass(int declaring_class_id,
						    const QString &declaring_class_key,
						    const QString &declaring_class_name);
			/// @return the origin of a value nobody wrote anywhere
		static CatalogValueOrigin unset();

			/// @return true when somebody looked where the value comes from
		bool isRead() const;
			/// @return true when the value was typed on the part
		bool isFromPart() const;
			/// @return true when the value is the initial value of a declaration
		bool isFromClass() const;
		/**
			@return true when the origin names a record holding a
			value: the part, or a class.

			False for Unread and for Unset alike, which is what the
			caller asking "is there a number behind this" needs: the
			two differ in what may be said about the absence, not in
			whether there is something to show.
		*/
		bool holdsValue() const;

		/**
			@return the class to name in a sentence: its name, its
			key when it has no name, its identifier when it has
			neither. Empty when the origin is not a class.
		*/
		QString className() const;
		/**
			@return one sentence saying where the value comes from,
			empty when nothing was read about it.

			Empty rather than a sentence for Unread, on purpose: a
			panel pasting this after a number has to say nothing
			rather than claim the number was never filled in.
		*/
		QString describe() const;

			/// which record the value comes from
		CatalogValueSource source = CatalogValueSource::Unread;
			/// the class declaring the property, when inherited
		int class_id = 0;
			/// its stable key, empty when the origin is not a class
		QString class_key;
			/// its user visible name
		QString class_name;
};

#endif // CATALOGVALUEORIGIN_H
