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
#ifndef IECSTRUCTURE_H
#define IECSTRUCTURE_H

#include <QString>

/**
	@brief The IecStructure class
	The identification structure of IEC 81346: `=` function, `+` location,
	`-` product.

	Optional per project, and **off by default**: nothing the team does today
	changes, and the day a customer or a supplier requires the norm the
	project turns it on instead of being redrawn.

	Where the three parts live in QElectroTech - checked in the code, because
	the specification of T10 had it wrong on one of them:

	| Part | Element information | Folio information |
	|---|---|---|
	| `=` function | `plant` | `plant` |
	| `+` location | `location` | `locmach` |
	| `-` product | `label` — the tag itself | — |

	The specification said `-` was the `designation` field. It is not:
	`designation` is what QElectroTech labels "Numéro d'article", a commercial
	field that has nothing to do with the norm. The `-` part **is** the
	component tag, which is `label`. Writing the structure into `designation`
	would have quietly destroyed the article number of every component of a
	project that turned the norm on.

	Inheritance is what makes the norm worth having: the value set on the
	project holds for the folio, the folio's holds for the component, unless
	something says otherwise. Without it the structure is only extra typing.
*/
class IecStructure
{
	public:
		IecStructure();
		IecStructure(const QString &plant, const QString &location, const QString &product);

		bool isEmpty() const;
		bool operator==(const IecStructure &other) const;
		bool operator!=(const IecStructure &other) const;

		/**
			@param parent : project, or folio
			@param child : folio, or component
			@return @a parent with every non empty field of @a child on top.

			Field by field, and not all or nothing: moving one component to
			another cabinet overrides its `+` and leaves it inheriting the
			`=` of the project.
		*/
		static IecStructure inherit(const IecStructure &parent, const IecStructure &child);

		/// =CT1+A1-K3
		QString toFullTag() const;
		/// -K3, or K3 when the product part carries no dash
		QString toShortTag(bool with_dash = true) const;

		/**
			@param tag
			@return the structure a tag like "=CT1+A1-K3" describes. A tag
			with no separator at all is taken as the product part alone,
			which is what every tag written before the norm was turned on
			looks like.
		*/
		static IecStructure fromTag(const QString &tag);

		/// The element information keys the three parts are stored in
		static QString plantKey();
		static QString locationKey();
		static QString productKey();
		/// The folio information key of the location part
		static QString folioLocationKey();

	public:
		QString plant;     ///< `=` function
		QString location;  ///< `+` location
		QString product;   ///< `-` product, i.e. the tag
};

#endif // IECSTRUCTURE_H
