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

#include <QDomElement>
#include <QString>

class DiagramContext;

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
	| `+` location | `location_path`, else `location` | `locmach` |
	| `-` product | `label` — the tag itself | — |
	| `:` connection | inside `label`, as `X10:7` | — |

	Two keys for the `+` because there are two kinds of answer, not two
	spellings of one. `location_path` is the path down the location tree of
	the project: a place that exists, that somebody picked from a list, and
	whose designation is produced by converting that path. `location` is a
	free text field older than the norm here, which holds whatever anybody
	typed. The first is preferred wherever it is filled - see
	fromElementInformation.

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
		IecStructure(const QString &plant, const QString &location, const QString &product,
			     const QString &connection = QString());

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

		/// =CT1+A1-K3, or =CT1+A1-X10:7 for a connection
		QString toFullTag() const;
		/// -K3, or K3 when the product part carries no dash. A terminal with
		/// no product - label "7" - is its number, with no dash: the dash of
		/// the norm marks a product, and a number is a connection.
		QString toShortTag(bool with_dash = true) const;

		/**
			@param text
			@return true when @a text has the shape of a designation of the
			norm: a letter code and a number, "K3", "X10", "Q1.1", "K3-1".

			What this is for: the `-` of the norm marks a product, so it is
			written in front of something that is one. The same field holds
			whatever anybody typed, and a schematic is full of text that is
			not a designation at all - "Nota 1", "10A / 3P / F", "Tomada
			Industrial". Measured on a real 14 folio panel project on
			21/08/2026: of 230 composed labels, 45 were designations, 12 were
			free text and 173 were bare terminal numbers. Gluing a dash to the
			185 that are not designations is decoration, not the norm.

			A number alone is deliberately **not** a designation: it is a
			connection, the `:` part, and `fromTag` reads it as one.
		*/
		static bool isDesignation(const QString &text);

		/**
			@param context : what the drawing already says around this - the
			folio, or the terminal strip the terminal sits in
			@return the parts of this structure that @a context does **not**
			already say.

			One rule, and the norm's own: a prefix obvious from the context
			is omitted, the full designation being carried by the bill of
			material and the terminal plan. On a folio that says plant and
			location, a component reads `-K3`; the one that sits in another
			cabinet reads `+QCM2-K3`, the part that differs and only it.

			The same rule addresses a terminal. Inside the strip, whose head
			says `-X10` once, terminal 7 reads `7`. Anywhere else it reads
			`-X10:7`, with the colon the norm keeps for connections.

			Measured on a real 14 folio panel project on 21/08/2026: the
			full form on every symbol is +19% of drawn characters, illegible
			on the terminal folios, and ambiguous - two different terminals
			numbered 7 both became `=CT1+QCM-7`. This is the answer to
			that, and the reason the display has a third mode.
		*/
		QString toContextTag(const IecStructure &context) const;

		/**
			@param tag
			@return the structure a tag like "=CT1+A1-K3" describes. A tag
			with no separator at all is taken as the product part alone,
			which is what every tag written before the norm was turned on
			looks like.
		*/
		static IecStructure fromTag(const QString &tag);

		/**
			@param label : the tag the component carries
			@param info : the component information
			@return the structure the component describes.

			Read here and not at each caller: the drawing composes the tag in
			Element, and the dialog of the settings shows a preview of it. Two
			copies of this reading is two copies free to drift, and the whole
			promise of that preview is that it says what the drawing will do.

			Note it is `inherit` doing the work, and not an `if` per field
			written again: what the component sets itself goes on top of what
			its tag already said, which is the same rule the norm uses from
			project to folio to component.

			The preferred source of the `+` is the path down the location tree
			of the project - see locationPathKey. A component that carries one
			has been assigned a place that exists in the project, and the
			designation is the conversion of that path rather than anybody's
			typing, so it is right by construction. It is therefore read
			whatever @a location_from_field says.

			@param location_from_field : whether the free text `location`
			field of the component is the `+` of the norm. Consulted only for
			a component with no path. **Off by default**, and the reason is
			data, not taste: that field is older than the norm in
			QElectroTech and holds free text. In the 14 folio panel project
			measured it holds the terminal strip the wiring of that component
			lands on - X1, X5, X10 - and reading it as a place put `+X1-` on
			23 components, which is 23 wrong statements on a drawing. A
			project where the field really is a place says so, once, in the
			settings.

			So this parameter guards the free text and nothing else. A place
			typed into the tag itself - `+QCM2-K3` - is read whatever it says,
			because there it is explicit; and a place assigned from the tree
			is read whatever it says, because there it is not text at all.
		*/
		static IecStructure fromElementInformation(
				const QString &label,
				const DiagramContext &info,
				bool location_from_field = false);

		/**
			@param info : the folio information
			@return the structure the folio describes.

			Its own function, because the folio keeps the location part under
			another key than the component does - see the table above. That
			asymmetry is the one thing here worth having in a single place.
		*/
		static IecStructure fromFolioInformation(const DiagramContext &info);

		/// The element information keys the three parts are stored in
		static QString plantKey();
		static QString locationKey();
		static QString productKey();

		/**
			@return the element information key of the path down the location
			tree of the project

			A key of its own, and not a better spelling of locationKey(): a
			component can carry both, and they do not mean the same thing.
			The path names a place the project has; the other field names
			whatever somebody wrote. Which of the two becomes the `+` is
			decided in one place, fromElementInformation.
		*/
		static QString locationPathKey();

		/// The folio information key of the location part
		static QString folioLocationKey();

	public:
		QString plant;     ///< `=` function
		QString location;  ///< `+` location
		QString product;   ///< `-` product, i.e. the tag
		QString connection;///< `:` connection, the terminal inside the product
};

/**
	@brief How much of the identification structure is written next to a
	component.

	The full tag is correct and long: `=CT1+A1-K3` beside every symbol on a
	crowded folio is a folio nobody reads. The short form `-K3` is what a
	schematic normally carries, with the function and the location said once,
	in the title block. Both are the norm; which one belongs on the drawing
	is a decision of the person drawing it.

	Between them is the one the norm actually describes, and the one a real
	IEC drawing uses: write what the context does not already say. Same
	folio, same cabinet - `-K3`. The component moved to another cabinet -
	`+QCM2-K3`. Nothing repeated, nothing lost, and the full designation
	still on the bill of material and the terminal plan.
*/
enum class IecTagDisplay
{
	Short,   ///< -K3
	Context, ///< -K3 here, +QCM2-K3 for what differs from the folio
	Full     ///< =CT1+A1-K3
};

/**
	@brief Whether one project uses the identification structure, and how it
	shows it.

	**Off by default, and off means nothing changes.** A project drawn before
	the norm existed, opened with the switch off, shows exactly the tags it
	always showed - not "almost the same", the same. That is the whole reason
	the switch exists rather than the structure being always on, and it is
	the case that protects work already delivered.

	Belongs to the project and not to the application: two customers, two
	requirements, and the same workstation draws for both.
*/
class IecStructureSettings
{
	public:
		IecStructureSettings();

		bool enabled = false;
		IecTagDisplay display = IecTagDisplay::Short;

		/**
			@brief Whether the `location` field of a component is the `+` of
			the norm.

			Off by default. The field is older than the norm here and holds
			free text: projects write in it the terminal strip the wiring
			lands on, the panel, a note. Turning the structure on must not
			turn all of that into places of the norm behind the drawer's
			back - it belongs to the project, next to the switch itself,
			and the preview of the dialog shows what it does.
		*/
		bool location_from_element = false;

		/**
			@brief The tag to write next to a component.
			@param folio : the structure the folio carries
			@param element : the structure the component carries, its tag
			included as the product part
			@return with the structure off, the product part alone - the tag
			as it always was. With it on, the short or the full form of what
			the component inherits from the folio.
		*/
		QString displayedTag(const IecStructure &folio,
				     const IecStructure &element) const;

		bool operator==(const IecStructureSettings &other) const;
		bool operator!=(const IecStructureSettings &other) const;

		QDomElement toXml(QDomDocument &document) const;
		void fromXml(const QDomElement &element);
		/// the tag name toXml() writes and fromXml() reads
		static QString xmlTagName();

		static QString displayToString(IecTagDisplay display);
		static IecTagDisplay displayFromString(const QString &string);
		static QString translatedDisplay(IecTagDisplay display);
};

#endif // IECSTRUCTURE_H
