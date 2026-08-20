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
#ifndef SYMBOLBUILDER_H
#define SYMBOLBUILDER_H

#include <QDomDocument>
#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QUuid>

#include "../catalog/catalogpart.h"
#include "../qet.h"

/**
	@brief The two grids a symbol is drawn on.

	The main grid is the one the connection points must land on: a conductor
	docks where the terminal is, and the sheet moves conductors in steps of
	the main grid. A terminal half a step off is a symbol whose wire never
	quite touches, which is the single most common way a hand made symbol
	comes out broken.

	The sub grid is finer and only for the drawing itself. Nothing about the
	drawing has to line up with anything, so it is free to be as fine as the
	shape needs.
*/
class SymbolGrid
{
	public:
		SymbolGrid() {}
		SymbolGrid(qreal main_step, qreal sub_step) :
			main_step(main_step), sub_step(sub_step) {}

		/// the step connection points and the insertion point must land on
		qreal main_step = 10.0;
		/// the finer step the drawing itself may use
		qreal sub_step = 1.0;

		QPointF snapToMain(const QPointF &point) const;
		QPointF snapToSub(const QPointF &point) const;
		bool isOnMain(const QPointF &point) const;
		/// how far @a point sits from the nearest main grid crossing
		qreal distanceToMain(const QPointF &point) const;

		bool isValid() const;
};

/**
	@brief What kind of drawing a shape is.
	The four the sheet can draw, which are also the four the element
	definition can hold. The correspondence is exact and deliberate: a shape
	drawn on the sheet becomes the same shape inside the symbol, not an
	approximation of it.
*/
enum class SymbolShapeType
{
	Line,
	Rectangle,
	Ellipse,
	Polygon
};

/**
	@brief One drawn shape of a symbol.
	@a points holds two points for a line, the two opposite corners for a
	rectangle, the bounding box for an ellipse, and every vertex for a
	polygon. @a style is the css like string the element definition uses,
	carried verbatim so the appearance survives the round trip.
*/
class SymbolShape
{
	public:
		SymbolShape() {}
		SymbolShape(SymbolShapeType type, const QPolygonF &points) :
			type(type), points(points) {}

		SymbolShapeType type = SymbolShapeType::Line;
		QPolygonF points;
		/// polygons only: whether the last vertex joins the first
		bool closed = false;
		bool antialias = false;
		/// rounded rectangles
		qreal x_radius = 0.0;
		qreal y_radius = 0.0;
		QString style = defaultStyle();

		QRectF bounds() const;
		bool isValid() const;

		static QString defaultStyle();
		static QString typeToString(SymbolShapeType type);
		static SymbolShapeType typeFromString(const QString &string);
		/// the element definition tag this shape is written as
		static QString xmlTagFor(SymbolShapeType type);
};

/**
	@brief One connection point of a symbol.

	@a label is the provisional number the symbol carries — 1, 2, A1, L1.
	Provisional by nature: the same contactor symbol serves twenty different
	contactors, and the real numbers arrive with the catalog part assigned to
	the component (T13). Writing a manufacturer number here would make the
	symbol wrong for every other product.

	@a role and @a pair are the contact semantics the sheet has no way to
	state today. Two terminals sharing a non empty @a pair, both carrying the
	same @a role, are one contact. That is what lets a check count free
	contacts, warn that the schematic uses more contacts than the part has,
	and tell a power pole apart from a control contact.

	The role uses the same vocabulary as the catalog part on purpose
	(CatalogPinRole): the symbol says "this symbol has one NO and one NC",
	the part says "this product has two NO", and a check compares the two
	directly. Two enumerations would need a mapping table between them, and
	a mapping table is a thing that drifts.
*/
class SymbolTerminal
{
	public:
		SymbolTerminal() {}
		SymbolTerminal(const QPointF &position, Qet::Orientation orientation) :
			position(position), orientation(orientation) {}

		QPointF position;
		Qet::Orientation orientation = Qet::North;
		QString label;
		CatalogPinRole role = CatalogPinRole::Unknown;
		QString pair;
		QUuid uuid;

		/**
			@brief The letter the element definition writes for @a orientation.

			The same four letters Qet::orientationToString produces, written
			again here rather than called: that function lives in a file that
			drags the icons and the shortcut manager along with it, and this
			one has to build without a window so the symbol can be tested
			without one.

			Duplicating a mapping is normally how mappings drift. Not this
			one: the four letters are the file format. Changing them would
			stop every .elmt ever written from loading, so they are as fixed
			as anything in the program gets — and a test pins them anyway.
		*/
		static QString orientationToString(Qet::Orientation orientation);
		static Qet::Orientation orientationFromString(const QString &string);
		static QString translatedOrientation(Qet::Orientation orientation);
		static QList<Qet::Orientation> allOrientations();
};

/**
	@brief One text field attached to a symbol.

	@a info_key names which piece of the component information the field
	shows: the tag, the part code, the current of the fuse, or any property
	the class of the symbol declares. Empty @a info_key with a non empty
	@a text is free text.
*/
class SymbolText
{
	public:
		SymbolText() {}
		SymbolText(const QPointF &position, const QString &info_key) :
			position(position), info_key(info_key) {}

		QPointF position;
		QString info_key;
		QString text;
		qreal rotation = 0.0;
		int font_size = 8;

		/// the field every symbol gets: the tag of the component
		static SymbolText tagField(const QPointF &position);
};

/**
	@brief How the symbol takes part in cross referencing.

	Simple is a symbol that stands alone. Master is the one that owns the
	cross reference — the coil of a contactor, the body of a relay. Slave is
	the one that reports back to it — the auxiliary contact drawn on another
	folio.

	It is here because declaring the contacts of a contactor (see
	SymbolTerminal) and leaving the symbol simple would be half a job: the
	declaration would be read by a check but the contactor would still not
	show the mirror of its contacts on the sheet.
*/
enum class SymbolLinkType
{
	Simple,
	Master,
	Slave
};

/**
	@brief What a symbol needs fixed before it can be saved.
	Kept apart from a plain string list so the dialog can tell a refusal from
	a warning, and so a test can name the problem it expects.
*/
enum class SymbolProblem
{
	NoName,             ///< a symbol without a name cannot be filed
	NoTerminal,         ///< nothing can be wired to it
	NoShape,            ///< nothing is drawn
	TerminalOffGrid,    ///< a connection point is not on the main grid
	HotspotOffGrid,     ///< the insertion point is not on the main grid
	TerminalsOverlap,   ///< two connection points share a position
	PairIncomplete,     ///< a pair name is carried by a single terminal
	PairTooBig,         ///< a pair name is carried by more than two terminals
	PairRoleMismatch,   ///< the two halves of a pair declare different roles
	PairRoleMissing,    ///< a pair was declared without saying what it is
	NoClass             ///< the symbol does not say which class it belongs to
};

/**
	@brief The result of pushing the connection points onto the main grid.
	Reported instead of applied in silence: a terminal that moved two units
	is a terminal that no longer sits at the end of the line drawn for it,
	and only the person who drew it can decide whether that matters.
*/
class SymbolSnapReport
{
	public:
		int moved = 0;              ///< how many connection points moved
		qreal largest_move = 0.0;   ///< the biggest distance any of them moved
		bool hotspot_moved = false;

		bool isEmpty() const { return moved == 0 && !hotspot_moved; }
};

/**
	@brief A symbol being built out of what is drawn on the sheet.

	Holds what the element definition needs and nothing about the scene, so
	the whole of it — the geometry, the validation, the contact semantics and
	the XML — is testable without a window.

	The coordinates are the ones the shapes had on the sheet. toXml() moves
	them so the insertion point falls on the origin, which is what the
	element definition format expects, and that translation is the only
	geometry the writer does.
*/
class SymbolDefinition
{
	public:
		SymbolDefinition();

		QString name;
		/// key of the catalog class the symbol belongs to (T12)
		QString class_key;
		SymbolLinkType link_type = SymbolLinkType::Simple;
		/// free text shown in the library, the "informations" of the definition
		QString description;
		/// where the cursor holds the symbol when inserting it
		QPointF hotspot;
		QUuid uuid;
		/// bumped when a used symbol is changed by revision instead of in place
		int revision = 1;

		QList<SymbolShape> shapes;
		QList<SymbolTerminal> terminals;
		QList<SymbolText> texts;

		bool isNull() const;

		/// bounding box of everything drawn, terminals included
		QRectF bounds() const;
		/// the insertion point the drawing suggests: its top left, on the grid
		QPointF suggestedHotspot(const SymbolGrid &grid) const;

		/**
			@brief The connection points the drawing itself suggests.

			The free ends of the lines and open polygons — an endpoint no
			other shape shares. That is where a conductor is meant to arrive:
			a line end that meets another line is a corner of the drawing,
			a line end that meets nothing is a wire waiting to be attached.

			Snapped to the main grid, and oriented outwards: a point on the
			top edge of the drawing faces north, one on the left edge faces
			west. Getting this right by hand is the step people skip, and a
			symbol whose conductor does not attach is the result.

			A suggestion, not a decision — the dialog shows them for the
			drawer to add to, remove from and name.
		*/
		QList<SymbolTerminal> suggestedTerminals(const SymbolGrid &grid) const;

		/**
			@brief Push every connection point and the insertion point onto
			the main grid.
			@return what moved and by how much, so the caller can say it out
			loud instead of changing the drawing behind the drawer's back.
		*/
		SymbolSnapReport snapToGrid(const SymbolGrid &grid);

		QList<SymbolProblem> problems(const SymbolGrid &grid) const;
		/// the same, said in a sentence a projectist can act on
		QStringList problemMessages(const SymbolGrid &grid) const;
		/// whether problems() holds anything that must stop a save
		bool canBeSaved(const SymbolGrid &grid) const;

		/// the pair names declared on the terminals, in order of appearance
		QStringList pairNames() const;
		/**
			@return how many contacts of @a role the symbol declares. A pair
			counts once. This is the number a check compares against what the
			catalog part offers.
		*/
		int contactCount(CatalogPinRole role) const;

		/// the element definition, ready to be written to a .elmt file
		QDomDocument toXml() const;
		static SymbolDefinition fromXml(const QDomElement &definition);

	private:
		static QDomElement shapeToXml(QDomDocument &document,
					      const SymbolShape &shape,
					      const QPointF &offset);
		static QDomElement terminalToXml(QDomDocument &document,
						 const SymbolTerminal &terminal,
						 const QPointF &offset);
		static QDomElement textToXml(QDomDocument &document,
					     const SymbolText &text,
					     const QPointF &offset,
					     int z);

	public:
		static QString translatedProblem(SymbolProblem problem);
		static QString linkTypeToString(SymbolLinkType type);
		static SymbolLinkType linkTypeFromString(const QString &string);
		static QString translatedLinkType(SymbolLinkType type);
		/// a file name for @a name, safe on every platform
		static QString fileNameFor(const QString &name);
};

#endif // SYMBOLBUILDER_H
