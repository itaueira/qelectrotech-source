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
#include "symbolbuilder.h"

#include <algorithm>

#include <QCoreApplication>
#include <QHash>
#include <QLineF>
#include <QMap>
#include <QMetaEnum>
#include <QSet>
#include <QtMath>

namespace
{
	/**
		The stub a terminal draws beyond its connection point. Copied from
		Terminal::terminalSize instead of included: this file must stay free
		of the graphics items so the whole of it can be tested without a
		window, and the number is part of the file format anyway.
	*/
	const qreal TERMINAL_STUB = 4.0;

	/// half a unit of slack, so a coordinate written as 9.999999 is on grid
	const qreal GRID_TOLERANCE = 0.001;

	/// the size a terminal name is written in, one step under the
	/// text fields, because thirty two of them share one block
	const int TERMINAL_LABEL_FONT_SIZE = 6;

	/// the two attributes a text alignment is written as, in the key
	/// names the dynamic text fields have always used
	void writeAlignment(QDomElement &element, Qt::Alignment alignment)
	{
		const QMetaEnum meta = QMetaEnum::fromType<Qt::Alignment>();
		Qt::AlignmentFlag horizontal = Qt::AlignLeft;
		if (alignment & Qt::AlignRight) {
			horizontal = Qt::AlignRight;
		} else if (alignment & Qt::AlignHCenter) {
			horizontal = Qt::AlignHCenter;
		}
		Qt::AlignmentFlag vertical = Qt::AlignTop;
		if (alignment & Qt::AlignBottom) {
			vertical = Qt::AlignBottom;
		} else if (alignment & Qt::AlignVCenter) {
			vertical = Qt::AlignVCenter;
		}
		element.setAttribute(QStringLiteral("Halignment"),
				     QString::fromLatin1(meta.valueToKey(horizontal)));
		element.setAttribute(QStringLiteral("Valignment"),
				     QString::fromLatin1(meta.valueToKey(vertical)));
	}

	/// the alignment of a plain text, absent meaning the top left the
	/// element editor has written since before the attribute existed
	Qt::Alignment alignmentFromXml(const QDomElement &element)
	{
		const QMetaEnum meta = QMetaEnum::fromType<Qt::Alignment>();
		Qt::Alignment alignment;
		if (element.hasAttribute(QStringLiteral("Halignment"))) {
			alignment |= Qt::Alignment(meta.keyToValue(
					element.attribute(QStringLiteral("Halignment"))
							.toLatin1().constData()));
		} else {
			alignment |= Qt::AlignLeft;
		}
		if (element.hasAttribute(QStringLiteral("Valignment"))) {
			alignment |= Qt::Alignment(meta.keyToValue(
					element.attribute(QStringLiteral("Valignment"))
							.toLatin1().constData()));
		} else {
			alignment |= Qt::AlignTop;
		}
		return alignment;
	}

	/// the font the element editor writes on a text field
	QString fontString(int size)
	{
		return QStringLiteral("Liberation Sans,%1,-1,5,50,0,0,0,0,0,Regular")
				.arg(size);
	}

	/// two decimals, the precision the element definition is written with
	QString num(qreal value)
	{
		return QString::number(qRound(value * 100.0) / 100.0);
	}

	qreal snapValue(qreal value, qreal step)
	{
		if (step <= 0.0) {
			return value;
		}
		return qRound(value / step) * step;
	}

	/**
		The direction the terminal stub points to. Used to size the symbol:
		the stub is drawn outside the connection point, so it is part of what
		the symbol occupies.
	*/
	QPointF stubOffset(Qet::Orientation orientation)
	{
		switch (orientation) {
			case Qet::North: return QPointF(0.0, -TERMINAL_STUB);
			case Qet::South: return QPointF(0.0,  TERMINAL_STUB);
			case Qet::East:  return QPointF( TERMINAL_STUB, 0.0);
			case Qet::West:  return QPointF(-TERMINAL_STUB, 0.0);
		}
		return QPointF();
	}

	/// the declarations of a css like style string, by name
	QHash<QString, QString> styleDeclarations(const QString &style)
	{
		QHash<QString, QString> declarations;
		const QStringList parts = style.split(QLatin1Char(';'),
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
						     Qt::SkipEmptyParts);
#else
						     QString::SkipEmptyParts);
#endif
		for (const QString &part : parts) {
			const int colon = part.indexOf(QLatin1Char(':'));
			if (colon <= 0) {
				continue;
			}
			declarations.insert(part.left(colon).trimmed(),
					    part.mid(colon + 1).trimmed());
		}
		return declarations;
	}

	/**
		Whether @a point sits on the body of the symbol rather than outside it.

		A closed shape - a rectangle, an ellipse, a closed polygon - is the
		body of the thing being drawn. The end of a line that lands on it or
		inside it is where the line joins the body, and no conductor arrives
		there. The end that lands outside is the one waiting for a wire.

		Judged on the bounding box, which is exact for a rectangle and
		generous for an ellipse: a point in the corner of an ellipse's box is
		outside the ellipse and counted as inside anyway. Generous is the
		right way to be wrong here - a suggestion too few is a row the
		projectist adds, a suggestion too many is a terminal that ships.
	*/
	bool onBody(const QPointF &point, const QList<SymbolShape> &shapes)
	{
		for (const SymbolShape &shape : shapes) {
			const bool closed =
					shape.type == SymbolShapeType::Rectangle ||
					shape.type == SymbolShapeType::Ellipse ||
					(shape.type == SymbolShapeType::Polygon && shape.closed);
			if (!closed) {
				continue;
			}
			const QRectF body = shape.bounds().normalized();
			if (body.isNull()) {
				continue;
			}
			if (point.x() >= body.left() - GRID_TOLERANCE &&
					point.x() <= body.right() + GRID_TOLERANCE &&
					point.y() >= body.top() - GRID_TOLERANCE &&
					point.y() <= body.bottom() + GRID_TOLERANCE) {
				return true;
			}
		}
		return false;
	}

	/**
		Which way a connection point faces: outwards, away from the drawing.
		Decided by which edge of the bounding box the point is nearest, and
		the vertical axis wins a tie because a schematic is read from the
		top: a point at a corner of a coil belongs to the wire coming down
		into it, not to one arriving sideways.
	*/
	Qet::Orientation orientationOf(const QPointF &point, const QRectF &box)
	{
		if (box.isNull()) {
			return Qet::North;
		}
		const qreal to_top    = qAbs(point.y() - box.top());
		const qreal to_bottom = qAbs(point.y() - box.bottom());
		const qreal to_left   = qAbs(point.x() - box.left());
		const qreal to_right  = qAbs(point.x() - box.right());
		const qreal nearest = qMin(qMin(to_top, to_bottom),
					   qMin(to_left, to_right));

		if (qFuzzyCompare(nearest + 1.0, to_top + 1.0))    return Qet::North;
		if (qFuzzyCompare(nearest + 1.0, to_bottom + 1.0)) return Qet::South;
		if (qFuzzyCompare(nearest + 1.0, to_left + 1.0))   return Qet::West;
		return Qet::East;
	}
}

/*
	SymbolGrid
*/

QPointF SymbolGrid::snapToMain(const QPointF &point) const
{
	return QPointF(snapValue(point.x(), main_step),
		       snapValue(point.y(), main_step));
}

QPointF SymbolGrid::snapToSub(const QPointF &point) const
{
	return QPointF(snapValue(point.x(), sub_step),
		       snapValue(point.y(), sub_step));
}

bool SymbolGrid::isOnMain(const QPointF &point) const
{
	return distanceToMain(point) <= GRID_TOLERANCE;
}

qreal SymbolGrid::distanceToMain(const QPointF &point) const
{
	const QPointF snapped = snapToMain(point);
	return QLineF(point, snapped).length();
}

bool SymbolGrid::isValid() const
{
	return main_step > 0.0 && sub_step > 0.0 && sub_step <= main_step;
}

/*
	SymbolShape
*/

QColor SymbolShape::colorFor(const QString &name)
{
	static const QHash<QString, QColor> named = {
		{QStringLiteral("black"),     QColor(Qt::black)},
		{QStringLiteral("white"),     QColor(Qt::white)},
		{QStringLiteral("green"),     QColor(Qt::green)},
		{QStringLiteral("red"),       QColor(Qt::red)},
		{QStringLiteral("blue"),      QColor(Qt::blue)},
		{QStringLiteral("gray"),      QColor(Qt::gray)},
		{QStringLiteral("yellow"),    QColor(Qt::yellow)},
		{QStringLiteral("cyan"),      QColor(Qt::cyan)},
		{QStringLiteral("magenta"),   QColor(Qt::magenta)},
		{QStringLiteral("lightgray"), QColor(Qt::lightGray)}
	};
	if (named.contains(name)) {
		return named.value(name);
	}
		//The definition format has a long tail of HTML colour names this
		//does not carry. Letting QColor try is free, and what it does not
		//know comes back invalid for the caller to fall back on.
	const QColor guess(name);
	return guess.isValid() ? guess : QColor();
}

QPen SymbolShape::penFor(const QString &style)
{
	const QHash<QString, QString> declarations = styleDeclarations(style);

	QPen pen(Qt::black);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);

	const QString line_style = declarations.value(QStringLiteral("line-style"));
	if (line_style == QLatin1String("dashed")) {
		pen.setStyle(Qt::DashLine);
	} else if (line_style == QLatin1String("dotted")) {
		pen.setStyle(Qt::DotLine);
	} else if (line_style == QLatin1String("dashdotted")) {
		pen.setStyle(Qt::DashDotLine);
	} else {
		pen.setStyle(Qt::SolidLine);
	}

		//The same four steps styleOf() chooses between, read back to the
		//middle of each range: a symbol exploded and made into a block again
		//comes out with the weight it went in with.
	const QString weight = declarations.value(QStringLiteral("line-weight"));
	if (weight == QLatin1String("none")) {
		pen.setStyle(Qt::NoPen);
	} else if (weight == QLatin1String("thin")) {
		pen.setWidthF(0.4);
	} else if (weight == QLatin1String("hight")) {
		pen.setWidthF(2.0);
	} else if (weight == QLatin1String("eleve")) {
		pen.setWidthF(4.0);
	} else {
		pen.setWidthF(1.0);
	}

	const QColor color = colorFor(declarations.value(QStringLiteral("color")));
	pen.setColor(color.isValid() ? color : QColor(Qt::black));
	return pen;
}

QBrush SymbolShape::brushFor(const QString &style)
{
	const QHash<QString, QString> declarations = styleDeclarations(style);
	const QString filling = declarations.value(QStringLiteral("filling"));
	if (filling.isEmpty() || filling == QLatin1String("none")) {
		return QBrush(Qt::NoBrush);
	}
	const QColor color = colorFor(filling);
	if (!color.isValid()) {
		return QBrush(Qt::NoBrush);
	}
	return QBrush(color, Qt::SolidPattern);
}

QString SymbolShape::defaultStyle()
{
	return QStringLiteral(
		"line-style:normal;line-weight:normal;filling:none;color:black");
}

QRectF SymbolShape::bounds() const
{
	if (points.isEmpty()) {
		return QRectF();
	}
	return points.boundingRect();
}

bool SymbolShape::isValid() const
{
	switch (type) {
		case SymbolShapeType::Line:
		case SymbolShapeType::Rectangle:
		case SymbolShapeType::Ellipse:
				//Two points define all three, and the two must not be the
				//same point: a line of zero length and a rectangle of zero
				//area are drawings nobody can see and nobody can select.
			return points.size() == 2 &&
					QLineF(points.at(0), points.at(1)).length() > 0.0;
		case SymbolShapeType::Polygon:
			return points.size() >= 2;
	}
	return false;
}

QString SymbolShape::typeToString(SymbolShapeType type)
{
	switch (type) {
		case SymbolShapeType::Line:      return QStringLiteral("line");
		case SymbolShapeType::Rectangle: return QStringLiteral("rectangle");
		case SymbolShapeType::Ellipse:   return QStringLiteral("ellipse");
		case SymbolShapeType::Polygon:   return QStringLiteral("polygon");
	}
	return QStringLiteral("line");
}

SymbolShapeType SymbolShape::typeFromString(const QString &string)
{
	if (string == QLatin1String("rectangle")) {
		return SymbolShapeType::Rectangle;
	}
	if (string == QLatin1String("ellipse")) {
		return SymbolShapeType::Ellipse;
	}
	if (string == QLatin1String("polygon")) {
		return SymbolShapeType::Polygon;
	}
	return SymbolShapeType::Line;
}

QString SymbolShape::xmlTagFor(SymbolShapeType type)
{
	switch (type) {
		case SymbolShapeType::Line:      return QStringLiteral("line");
		case SymbolShapeType::Rectangle: return QStringLiteral("rect");
		case SymbolShapeType::Ellipse:   return QStringLiteral("ellipse");
		case SymbolShapeType::Polygon:   return QStringLiteral("polygon");
	}
	return QStringLiteral("line");
}

QString SymbolTerminal::orientationToString(Qet::Orientation orientation)
{
	switch (orientation) {
		case Qet::North: return QStringLiteral("n");
		case Qet::East:  return QStringLiteral("e");
		case Qet::South: return QStringLiteral("s");
		case Qet::West:  return QStringLiteral("w");
	}
	return QStringLiteral("n");
}

Qet::Orientation SymbolTerminal::orientationFromString(const QString &string)
{
	if (string == QLatin1String("e")) return Qet::East;
	if (string == QLatin1String("s")) return Qet::South;
	if (string == QLatin1String("w")) return Qet::West;
	return Qet::North;
}

QString SymbolTerminal::translatedOrientation(Qet::Orientation orientation)
{
	switch (orientation) {
		case Qet::North:
			return QCoreApplication::translate("SymbolTerminal", "haut");
		case Qet::East:
			return QCoreApplication::translate("SymbolTerminal", "droite");
		case Qet::South:
			return QCoreApplication::translate("SymbolTerminal", "bas");
		case Qet::West:
			return QCoreApplication::translate("SymbolTerminal", "gauche");
	}
	return QString();
}

QList<Qet::Orientation> SymbolTerminal::allOrientations()
{
	return {Qet::North, Qet::East, Qet::South, Qet::West};
}

QPointF SymbolTerminal::suggestedLabelPos(Qet::Orientation orientation,
						  qreal distance)
{
		//Away from where the conductor comes from, which is the
		//opposite of where the terminal points.
	switch (orientation) {
		case Qet::North: return QPointF(0.0, distance);
		case Qet::South: return QPointF(0.0, -distance);
		case Qet::West:  return QPointF(distance, 0.0);
		case Qet::East:  return QPointF(-distance, 0.0);
	}
	return QPointF(0.0, distance);
}

Qt::Alignment SymbolTerminal::labelHAlignment(Qet::Orientation orientation)
{
		//The name grows away from the terminal on the two sides where
		//it runs sideways, and stays centred on the two where it does
		//not. Centring a column of numbers is what lets the eye read
		//the column instead of each number.
	switch (orientation) {
		case Qet::North:
		case Qet::South: return Qt::AlignHCenter;
		case Qet::West:  return Qt::AlignLeft;
		case Qet::East:  return Qt::AlignRight;
	}
	return Qt::AlignHCenter;
}

Qt::Alignment SymbolTerminal::labelVAlignment(Qet::Orientation orientation)
{
	switch (orientation) {
		case Qet::North: return Qt::AlignTop;
		case Qet::South: return Qt::AlignBottom;
		case Qet::West:
		case Qet::East:  return Qt::AlignVCenter;
	}
	return Qt::AlignTop;
}

/*
	SymbolText
*/

SymbolText SymbolText::tagField(const QPointF &position)
{
	SymbolText text(position, QStringLiteral("label"));
	return text;
}

/*
	SymbolDefinition
*/

SymbolDefinition::SymbolDefinition() :
	uuid(QUuid::createUuid())
{
}

bool SymbolDefinition::isNull() const
{
	return shapes.isEmpty() && terminals.isEmpty() && texts.isEmpty();
}

QRectF SymbolDefinition::bounds() const
{
	QRectF rect;
	for (const SymbolShape &shape : shapes) {
		rect |= shape.bounds();
	}
		//The connection point counts, and so does the stub drawn beyond it:
		//that stub is what the eye aims the conductor at.
	for (const SymbolTerminal &terminal : terminals) {
		QRectF stub(terminal.position, terminal.position + stubOffset(terminal.orientation));
		rect |= stub.normalized();
	}
		//Text fields are left out on purpose, the same way the element editor
		//leaves them out of the size of the element: how much room a field
		//takes depends on the value it shows, which is not known yet.
	return rect;
}

QPointF SymbolDefinition::suggestedHotspot(const SymbolGrid &grid) const
{
	const QRectF rect = bounds();
	if (rect.isNull()) {
		return QPointF();
	}
		//The first connection point is the better guess when there is one:
		//that is where the projectist expects the cursor to hold the symbol,
		//and it is already on the grid.
	if (!terminals.isEmpty()) {
		QPointF best = terminals.first().position;
		for (const SymbolTerminal &terminal : terminals) {
			if (terminal.position.y() < best.y() ||
					(qFuzzyCompare(terminal.position.y() + 1.0, best.y() + 1.0) &&
					 terminal.position.x() < best.x())) {
				best = terminal.position;
			}
		}
		return grid.snapToMain(best);
	}
	return grid.snapToMain(rect.center());
}

QList<SymbolTerminal> SymbolDefinition::suggestedTerminals(
		const SymbolGrid &grid) const
{
	QList<SymbolTerminal> found;
	if (shapes.isEmpty()) {
		return found;
	}

		//Collect the endpoints of everything that has ends. A rectangle and
		//an ellipse have none: they are closed, no wire arrives in the middle
		//of them.
	QList<QPointF> ends;
	for (const SymbolShape &shape : shapes) {
		if (shape.type == SymbolShapeType::Line) {
			if (shape.points.size() >= 2) {
				ends << shape.points.first() << shape.points.last();
			}
		} else if (shape.type == SymbolShapeType::Polygon && !shape.closed) {
			if (shape.points.size() >= 2) {
				ends << shape.points.first() << shape.points.last();
			}
		}
	}

		//An endpoint another shape also touches is a corner of the drawing,
		//not a place for a conductor.
	const QRectF box = bounds();
	QList<QPointF> free_ends;
	for (int i = 0 ; i < ends.size() ; ++i) {
		bool shared = false;
		for (int j = 0 ; j < ends.size() ; ++j) {
			if (i == j) {
				continue;
			}
			if (QLineF(ends.at(i), ends.at(j)).length() <= GRID_TOLERANCE) {
				shared = true;
				break;
			}
		}
		if (shared) {
			continue;
		}
		if (onBody(ends.at(i), shapes)) {
			continue;
		}
		const QPointF snapped = grid.snapToMain(ends.at(i));
		bool already = false;
		for (const QPointF &kept : free_ends) {
			if (QLineF(kept, snapped).length() <= GRID_TOLERANCE) {
				already = true;
				break;
			}
		}
		if (!already) {
			free_ends << snapped;
		}
	}

		//Reading order, so the numbering the drawer types into the table runs
		//the way the eye runs: top to bottom, then left to right.
	std::sort(free_ends.begin(), free_ends.end(),
		  [](const QPointF &a, const QPointF &b)
	{
		if (!qFuzzyCompare(a.y() + 1.0, b.y() + 1.0)) {
			return a.y() < b.y();
		}
		return a.x() < b.x();
	});

	for (const QPointF &point : free_ends) {
		found << SymbolTerminal(point, orientationOf(point, box));
	}
	return found;
}

SymbolSnapReport SymbolDefinition::snapToGrid(const SymbolGrid &grid)
{
	SymbolSnapReport report;
	for (SymbolTerminal &terminal : terminals) {
		const QPointF snapped = grid.snapToMain(terminal.position);
		const qreal distance = QLineF(terminal.position, snapped).length();
		if (distance > GRID_TOLERANCE) {
			report.moved++;
			report.largest_move = qMax(report.largest_move, distance);
			terminal.position = snapped;
		}
	}

	const QPointF snapped_hotspot = grid.snapToMain(hotspot);
	if (QLineF(hotspot, snapped_hotspot).length() > GRID_TOLERANCE) {
		report.hotspot_moved = true;
		hotspot = snapped_hotspot;
	}
	return report;
}

QStringList SymbolDefinition::pairNames() const
{
	QStringList names;
	for (const SymbolTerminal &terminal : terminals) {
		if (!terminal.pair.isEmpty() && !names.contains(terminal.pair)) {
			names << terminal.pair;
		}
	}
	return names;
}

int SymbolDefinition::contactCount(CatalogPinRole role) const
{
	int count = 0;
	QSet<QString> counted_pairs;
	for (const SymbolTerminal &terminal : terminals) {
		if (terminal.role != role) {
			continue;
		}
		if (terminal.pair.isEmpty()) {
				//A terminal carrying a role but no pair is one contact on its
				//own. A part sheet that only says "2 NO" gives exactly that,
				//and refusing to count it would make the symbol look emptier
				//than it is.
			count++;
			continue;
		}
		if (!counted_pairs.contains(terminal.pair)) {
			counted_pairs.insert(terminal.pair);
			count++;
		}
	}
	return count;
}

QList<SymbolProblem> SymbolDefinition::problems(const SymbolGrid &grid) const
{
	QList<SymbolProblem> found;

	if (name.trimmed().isEmpty()) {
		found << SymbolProblem::NoName;
	}
	if (class_key.trimmed().isEmpty()) {
		found << SymbolProblem::NoClass;
	}
		//A shape has to be a shape: a line of zero length and a rectangle of
		//zero area are in the file and on no screen. Counting them as a
		//drawing would let a symbol be saved with nothing visible in it.
	int drawn = 0;
	for (const SymbolShape &shape : shapes) {
		if (shape.isValid()) {
			drawn++;
		}
	}
	if (drawn == 0) {
		found << SymbolProblem::NoShape;
	}
	if (terminals.isEmpty()) {
		found << SymbolProblem::NoTerminal;
	}

	QList<QPointF> seen_positions;
	bool off_grid = false;
	bool overlap = false;
	for (const SymbolTerminal &terminal : terminals) {
		if (!grid.isOnMain(terminal.position)) {
			off_grid = true;
		}
		for (const QPointF &position : seen_positions) {
			if (QLineF(position, terminal.position).length() <= GRID_TOLERANCE) {
				overlap = true;
			}
		}
		seen_positions << terminal.position;
	}
	if (off_grid) {
		found << SymbolProblem::TerminalOffGrid;
	}
	if (overlap) {
		found << SymbolProblem::TerminalsOverlap;
	}
	if (!terminals.isEmpty() && !grid.isOnMain(hotspot)) {
		found << SymbolProblem::HotspotOffGrid;
	}

		//Pair checks. A pair is what turns two connection points into one
		//contact, so a half declared pair is worse than none: a check would
		//count a contact that does not exist.
	QMap<QString, QList<CatalogPinRole>> pairs;
	for (const SymbolTerminal &terminal : terminals) {
		if (terminal.pair.isEmpty()) {
			continue;
		}
		pairs[terminal.pair] << terminal.role;
	}
	bool incomplete = false, too_big = false, mismatch = false, missing = false;
	for (auto it = pairs.constBegin() ; it != pairs.constEnd() ; ++it) {
		const QList<CatalogPinRole> &roles = it.value();
		if (roles.size() == 1) {
			incomplete = true;
		} else if (roles.size() > 2) {
			too_big = true;
		}
		for (CatalogPinRole role : roles) {
			if (role != roles.first()) {
				mismatch = true;
			}
			if (role == CatalogPinRole::Unknown) {
				missing = true;
			}
		}
	}
	if (incomplete) found << SymbolProblem::PairIncomplete;
	if (too_big)    found << SymbolProblem::PairTooBig;
	if (mismatch)   found << SymbolProblem::PairRoleMismatch;
	if (missing)    found << SymbolProblem::PairRoleMissing;

	return found;
}

QStringList SymbolDefinition::problemMessages(const SymbolGrid &grid) const
{
	QStringList messages;
	const QList<SymbolProblem> found = problems(grid);
	for (SymbolProblem problem : found) {
		messages << translatedProblem(problem);
	}
	return messages;
}

bool SymbolDefinition::canBeSaved(const SymbolGrid &grid) const
{
	return problems(grid).isEmpty();
}

QString SymbolDefinition::translatedProblem(SymbolProblem problem)
{
	switch (problem) {
		case SymbolProblem::NoName:
			return QCoreApplication::translate("SymbolDefinition",
				"Le symbole n'a pas de nom.");
		case SymbolProblem::NoClass:
			return QCoreApplication::translate("SymbolDefinition",
				"Le symbole ne dit pas à quelle classe il appartient.");
		case SymbolProblem::NoShape:
			return QCoreApplication::translate("SymbolDefinition",
				"Rien n'est dessiné.");
		case SymbolProblem::NoTerminal:
			return QCoreApplication::translate("SymbolDefinition",
				"Le symbole n'a aucun point de raccordement : "
				"aucun conducteur ne pourra s'y brancher.");
		case SymbolProblem::TerminalOffGrid:
			return QCoreApplication::translate("SymbolDefinition",
				"Un point de raccordement n'est pas sur la grille "
				"principale : le conducteur ne s'y accrochera pas.");
		case SymbolProblem::HotspotOffGrid:
			return QCoreApplication::translate("SymbolDefinition",
				"Le point d'insertion n'est pas sur la grille principale.");
		case SymbolProblem::TerminalsOverlap:
			return QCoreApplication::translate("SymbolDefinition",
				"Deux points de raccordement sont au même endroit.");
		case SymbolProblem::PairIncomplete:
			return QCoreApplication::translate("SymbolDefinition",
				"Une paire de contact n'a qu'un seul point de "
				"raccordement.");
		case SymbolProblem::PairTooBig:
			return QCoreApplication::translate("SymbolDefinition",
				"Une paire de contact a plus de deux points de "
				"raccordement.");
		case SymbolProblem::PairRoleMismatch:
			return QCoreApplication::translate("SymbolDefinition",
				"Les deux points d'une même paire ne déclarent pas le même "
				"type de contact.");
		case SymbolProblem::PairRoleMissing:
			return QCoreApplication::translate("SymbolDefinition",
				"Une paire a été formée sans dire de quel contact il "
				"s'agit.");
	}
	return QString();
}

QString SymbolDefinition::linkTypeToString(SymbolLinkType type)
{
	switch (type) {
		case SymbolLinkType::Master: return QStringLiteral("master");
		case SymbolLinkType::Slave:  return QStringLiteral("slave");
		case SymbolLinkType::Simple: break;
	}
	return QStringLiteral("simple");
}

SymbolLinkType SymbolDefinition::linkTypeFromString(const QString &string)
{
	if (string == QLatin1String("master")) {
		return SymbolLinkType::Master;
	}
	if (string == QLatin1String("slave")) {
		return SymbolLinkType::Slave;
	}
	return SymbolLinkType::Simple;
}

QString SymbolDefinition::translatedLinkType(SymbolLinkType type)
{
	switch (type) {
		case SymbolLinkType::Simple:
			return QCoreApplication::translate("SymbolDefinition",
				"Symbole seul");
		case SymbolLinkType::Master:
			return QCoreApplication::translate("SymbolDefinition",
				"Maître du renvoi de folio (bobine, corps de l'appareil)");
		case SymbolLinkType::Slave:
			return QCoreApplication::translate("SymbolDefinition",
				"Esclave du renvoi de folio (contact auxiliaire)");
	}
	return QString();
}

QString SymbolDefinition::fileNameFor(const QString &name)
{
	QString file_name;
	for (const QChar &character : name.trimmed()) {
		if (character.isLetterOrNumber()) {
			file_name += character.toLower();
		} else if (character == QLatin1Char('-') ||
			   character == QLatin1Char('_') ||
			   character.isSpace()) {
			if (!file_name.endsWith(QLatin1Char('_'))) {
				file_name += QLatin1Char('_');
			}
		}
	}
	while (file_name.endsWith(QLatin1Char('_'))) {
		file_name.chop(1);
	}
	return file_name;
}

QDomDocument SymbolDefinition::toXml() const
{
	QDomDocument document;
	QDomElement root = document.createElement(QStringLiteral("definition"));

		//Everything is written relative to the insertion point: that is what
		//the element definition means by the origin of its coordinates.
	const QPointF offset = -hotspot;
	QRectF size = bounds();
	if (!size.isNull()) {
		size.translate(offset);
	}

		//The same arithmetic the element editor uses, on purpose. A symbol
		//created here and a symbol created there must end up with the same
		//geometry, or the two paths would produce libraries that do not look
		//alike.
	int upwidth = ((qRound(size.width()) / 10) * 10) + 10;
	if ((qRound(size.width()) % 10) > 6) {
		upwidth += 10;
	}
	int upheight = ((qRound(size.height()) / 10) * 10) + 10;
	if ((qRound(size.height()) % 10) > 6) {
		upheight += 10;
	}
	const int xmargin = qRound(upwidth - size.width());
	const int ymargin = qRound(upheight - size.height());

	root.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
	root.setAttribute(QStringLiteral("width"), QString::number(upwidth));
	root.setAttribute(QStringLiteral("height"), QString::number(upheight));
	root.setAttribute(QStringLiteral("hotspot_x"),
			  QString::number(-(qRound(size.x() - (xmargin / 2)))));
	root.setAttribute(QStringLiteral("hotspot_y"),
			  QString::number(-(qRound(size.y() - (ymargin / 2)))));
	root.setAttribute(QStringLiteral("link_type"), linkTypeToString(link_type));

	QDomElement uuid_element = document.createElement(QStringLiteral("uuid"));
	uuid_element.setAttribute(QStringLiteral("uuid"), uuid.toString());
	root.appendChild(uuid_element);

	QDomElement names = document.createElement(QStringLiteral("names"));
	QDomElement name_element = document.createElement(QStringLiteral("name"));
		//Written under the language of the person who drew it, and under that
		//one only. The symbol was named in the words of the shop floor, and
		//inventing an English name for it would be inventing, not translating.
		//One language is enough because NamesList::name() falls back to the
		//first name in the list when it recognises none, so the name shows
		//whatever locale the program is running in.
	name_element.setAttribute(QStringLiteral("lang"),
				  QStringLiteral("pt_BR"));
	name_element.appendChild(document.createTextNode(name.trimmed()));
	names.appendChild(name_element);
	root.appendChild(names);

		//The class of the symbol, and the values of the default part when the
		//symbol is for one particular product, travel as element information:
		//that is what a component reads to know what it is.
	if (!class_key.trimmed().isEmpty() || !default_part_values.isEmpty()) {
		QDomElement informations =
				document.createElement(QStringLiteral("elementInformations"));
		if (!class_key.trimmed().isEmpty()) {
			QDomElement info =
					document.createElement(QStringLiteral("elementInformation"));
			info.setAttribute(QStringLiteral("name"),
					  QStringLiteral("catalog_class"));
			info.setAttribute(QStringLiteral("show"), QStringLiteral("0"));
			info.appendChild(document.createTextNode(class_key.trimmed()));
			informations.appendChild(info);
		}
			//Sorted, so the same symbol always produces the same file.
		QStringList value_keys = default_part_values.keys();
		value_keys.sort();
		for (const QString &key : value_keys) {
			QDomElement info =
					document.createElement(QStringLiteral("elementInformation"));
			info.setAttribute(QStringLiteral("name"), key);
			info.setAttribute(QStringLiteral("show"), QStringLiteral("1"));
			info.appendChild(document.createTextNode(
						 default_part_values.value(key)));
			informations.appendChild(info);
		}
		root.appendChild(informations);
	}

	if (!description.trimmed().isEmpty()) {
		QDomElement informations =
				document.createElement(QStringLiteral("informations"));
		informations.appendChild(
					document.createTextNode(description.trimmed()));
		root.appendChild(informations);
	}

	QDomElement description_element =
			document.createElement(QStringLiteral("description"));

	int z = 1;
	for (const SymbolText &text : texts) {
		description_element.appendChild(
					textToXml(document, text, offset, z++));
	}
	for (const SymbolShape &shape : shapes) {
		description_element.appendChild(shapeToXml(document, shape, offset));
	}
	for (const SymbolTerminal &terminal : terminals) {
		description_element.appendChild(
					terminalToXml(document, terminal, offset));
	}

	root.appendChild(description_element);
	document.appendChild(root);
	return document;
}

QDomElement SymbolDefinition::shapeToXml(QDomDocument &document,
					 const SymbolShape &shape,
					 const QPointF &offset)
{
	QDomElement element =
			document.createElement(SymbolShape::xmlTagFor(shape.type));
	element.setAttribute(QStringLiteral("style"), shape.style);
	element.setAttribute(QStringLiteral("antialias"),
			     shape.antialias ? QStringLiteral("true")
					     : QStringLiteral("false"));

	switch (shape.type) {
		case SymbolShapeType::Line: {
			const QPointF p1 = shape.points.value(0) + offset;
			const QPointF p2 = shape.points.value(1) + offset;
			element.setAttribute(QStringLiteral("x1"), num(p1.x()));
			element.setAttribute(QStringLiteral("y1"), num(p1.y()));
			element.setAttribute(QStringLiteral("x2"), num(p2.x()));
			element.setAttribute(QStringLiteral("y2"), num(p2.y()));
			element.setAttribute(QStringLiteral("end1"),
					     QStringLiteral("none"));
			element.setAttribute(QStringLiteral("end2"),
					     QStringLiteral("none"));
			element.setAttribute(QStringLiteral("length1"),
					     QStringLiteral("1.5"));
			element.setAttribute(QStringLiteral("length2"),
					     QStringLiteral("1.5"));
			break;
		}
		case SymbolShapeType::Rectangle:
		case SymbolShapeType::Ellipse: {
			QRectF rect(shape.points.value(0), shape.points.value(1));
			rect = rect.normalized();
			rect.translate(offset);
			element.setAttribute(QStringLiteral("x"), num(rect.x()));
			element.setAttribute(QStringLiteral("y"), num(rect.y()));
			element.setAttribute(QStringLiteral("width"),
					     num(rect.width()));
			element.setAttribute(QStringLiteral("height"),
					     num(rect.height()));
			if (shape.type == SymbolShapeType::Rectangle) {
				element.setAttribute(QStringLiteral("rx"),
						     num(shape.x_radius));
				element.setAttribute(QStringLiteral("ry"),
						     num(shape.y_radius));
			}
			break;
		}
		case SymbolShapeType::Polygon: {
			int index = 1;
			for (const QPointF &point : shape.points) {
				const QPointF moved = point + offset;
				element.setAttribute(QStringLiteral("x%1").arg(index),
						     num(moved.x()));
				element.setAttribute(QStringLiteral("y%1").arg(index),
						     num(moved.y()));
				index++;
			}
			element.setAttribute(QStringLiteral("closed"),
					     shape.closed ? QStringLiteral("true")
							  : QStringLiteral("false"));
			break;
		}
	}
	return element;
}

QDomElement SymbolDefinition::terminalToXml(QDomDocument &document,
					    const SymbolTerminal &terminal,
					    const QPointF &offset)
{
	QDomElement element = document.createElement(QStringLiteral("terminal"));
	const QPointF position = terminal.position + offset;
	element.setAttribute(QStringLiteral("x"), num(position.x()));
	element.setAttribute(QStringLiteral("y"), num(position.y()));
	element.setAttribute(QStringLiteral("uuid"),
			     terminal.uuid.isNull() ? QUuid::createUuid().toString()
						    : terminal.uuid.toString());
	element.setAttribute(QStringLiteral("name"), terminal.label);
	element.setAttribute(QStringLiteral("orientation"),
			     SymbolTerminal::orientationToString(terminal.orientation));
	element.setAttribute(QStringLiteral("type"), QStringLiteral("Generic"));

		//The contact semantics, written only when declared. An attribute that
		//is absent when there is nothing to say keeps the file readable and
		//keeps a plain symbol byte identical to what the element editor
		//writes.
	if (terminal.role != CatalogPinRole::Unknown) {
		element.setAttribute(QStringLiteral("role"),
				     CatalogPin::roleToString(terminal.role));
	}
	if (!terminal.pair.isEmpty()) {
		element.setAttribute(QStringLiteral("pair"), terminal.pair);
	}

		//The name of the terminal, drawn by the terminal. Written only
		//when asked for, for the same reason as the two attributes
		//above: a symbol with nothing to say has to keep producing the
		//line it always produced. The alignment is not stored on the
		//terminal and is derived here, so that the side a terminal
		//points to and the way its name is laid out can never
		//disagree.
	if (terminal.show_name) {
		element.setAttribute(QStringLiteral("show_name"),
					 QStringLiteral("true"));
		element.setAttribute(QStringLiteral("label_x"),
					 num(terminal.label_pos.x()));
		element.setAttribute(QStringLiteral("label_y"),
					 num(terminal.label_pos.y()));
		element.setAttribute(QStringLiteral("label_font"),
					 fontString(TERMINAL_LABEL_FONT_SIZE));
		element.setAttribute(QStringLiteral("label_rotation"),
					 QStringLiteral("0"));
		element.setAttribute(QStringLiteral("label_halign"),
					 QString::number(static_cast<int>(
						 SymbolTerminal::labelHAlignment(
							 terminal.orientation))));
		element.setAttribute(QStringLiteral("label_valign"),
					 QString::number(static_cast<int>(
						 SymbolTerminal::labelVAlignment(
							 terminal.orientation))));
	}
	return element;
}

QDomElement SymbolDefinition::textToXml(QDomDocument &document,
					const SymbolText &text,
					const QPointF &offset,
					int z)
{
	const QPointF position = text.position + offset;

	if (text.info_key.isEmpty()) {
		QDomElement element = document.createElement(QStringLiteral("text"));
		element.setAttribute(QStringLiteral("text"), text.text);
		element.setAttribute(QStringLiteral("x"), num(position.x()));
		element.setAttribute(QStringLiteral("y"), num(position.y()));
		element.setAttribute(QStringLiteral("rotation"),
				     num(text.rotation));
		element.setAttribute(QStringLiteral("font"),
				     fontString(text.font_size));
		element.setAttribute(QStringLiteral("color"),
				     QStringLiteral("#000000"));

			//The alignment, written only when it is not the
			//historical top left, and under the names PartText
			//reads: a text nobody aligned keeps producing the
			//line it always produced, byte for byte.
		if (text.alignment != (Qt::AlignTop | Qt::AlignLeft)) {
			writeAlignment(element, text.alignment);
		}
		return element;
	}

	QDomElement element =
			document.createElement(QStringLiteral("dynamic_text"));
	element.setAttribute(QStringLiteral("x"), num(position.x()));
	element.setAttribute(QStringLiteral("y"), num(position.y()));
	element.setAttribute(QStringLiteral("z"), QString::number(z));
	element.setAttribute(QStringLiteral("text_width"), QStringLiteral("-1"));
		//The same two attributes, and the same default: a field nobody
		//aligned is the left aligned field this has always written. A
		//tag centred over a block of thirty two points is the reason
		//the field is asked at all instead of assumed.
	writeAlignment(element, text.alignment);
	element.setAttribute(QStringLiteral("frame"), QStringLiteral("false"));
	element.setAttribute(QStringLiteral("rotation"), num(text.rotation));
	element.setAttribute(QStringLiteral("keep_visual_rotation"),
			     QStringLiteral("false"));
	element.setAttribute(QStringLiteral("text_from"),
			     QStringLiteral("ElementInfo"));
	element.setAttribute(QStringLiteral("uuid"),
			     QUuid::createUuid().toString());
	element.setAttribute(QStringLiteral("font"), fontString(text.font_size));

	QDomElement text_node = document.createElement(QStringLiteral("text"));
	text_node.appendChild(document.createTextNode(text.text));
	element.appendChild(text_node);

	QDomElement info = document.createElement(QStringLiteral("info_name"));
	info.appendChild(document.createTextNode(text.info_key));
	element.appendChild(info);

	return element;
}

SymbolDefinition SymbolDefinition::fromXml(const QDomElement &definition)
{
	SymbolDefinition symbol;
	if (definition.tagName() != QLatin1String("definition")) {
		return symbol;
	}

	const QPointF hotspot_offset(
			definition.attribute(QStringLiteral("hotspot_x")).toDouble(),
			definition.attribute(QStringLiteral("hotspot_y")).toDouble());
		//The file holds coordinates relative to the insertion point, so
		//reading them back puts the insertion point where the file says it is
		//inside its own box, and everything else around it. What comes back
		//is the geometry, not the place on the sheet the drawing came from -
		//the file never held that, and nothing needs it: what matters is that
		//the distance between two connection points is the distance that was
		//drawn.
	const QPointF offset = hotspot_offset;
	symbol.hotspot = hotspot_offset;
	symbol.link_type = linkTypeFromString(
				definition.attribute(QStringLiteral("link_type")));

	const QDomElement uuid_element =
			definition.firstChildElement(QStringLiteral("uuid"));
	if (!uuid_element.isNull()) {
		symbol.uuid = QUuid(uuid_element.attribute(QStringLiteral("uuid")));
	}

	const QDomElement names =
			definition.firstChildElement(QStringLiteral("names"));
	if (!names.isNull()) {
		QDomElement name_element =
				names.firstChildElement(QStringLiteral("name"));
		while (!name_element.isNull()) {
			if (symbol.name.isEmpty() ||
					name_element.attribute(QStringLiteral("lang")) ==
					QLatin1String("pt_BR")) {
				symbol.name = name_element.text();
			}
			name_element =
					name_element.nextSiblingElement(QStringLiteral("name"));
		}
	}

	const QDomElement informations =
			definition.firstChildElement(QStringLiteral("informations"));
	if (!informations.isNull()) {
		symbol.description = informations.text();
	}

	const QDomElement element_informations =
			definition.firstChildElement(QStringLiteral("elementInformations"));
	if (!element_informations.isNull()) {
		QDomElement info = element_informations.firstChildElement(
					QStringLiteral("elementInformation"));
		while (!info.isNull()) {
			const QString name = info.attribute(QStringLiteral("name"));
			if (name == QLatin1String("catalog_class")) {
				symbol.class_key = info.text();
			} else if (!name.isEmpty()) {
				symbol.default_part_values.insert(name, info.text());
				if (name == QLatin1String("part_code")) {
					symbol.default_part_code = info.text();
				}
			}
			info = info.nextSiblingElement(
						QStringLiteral("elementInformation"));
		}
	}

	const QDomElement description =
			definition.firstChildElement(QStringLiteral("description"));
	QDomElement child = description.firstChildElement();
	while (!child.isNull()) {
		const QString tag = child.tagName();
		if (tag == QLatin1String("terminal")) {
			SymbolTerminal terminal;
			terminal.position = QPointF(
					child.attribute(QStringLiteral("x")).toDouble(),
					child.attribute(QStringLiteral("y")).toDouble()) + offset;
			terminal.orientation = SymbolTerminal::orientationFromString(
					child.attribute(QStringLiteral("orientation")));
			terminal.label = child.attribute(QStringLiteral("name"));
			terminal.uuid =
					QUuid(child.attribute(QStringLiteral("uuid")));
			terminal.role = CatalogPin::roleFromString(
					child.attribute(QStringLiteral("role")));
			terminal.pair = child.attribute(QStringLiteral("pair"));
			terminal.show_name =
					child.attribute(QStringLiteral("show_name"))
						== QLatin1String("true");
			if (terminal.show_name) {
				terminal.label_pos = QPointF(
						child.attribute(
							QStringLiteral("label_x")).toDouble(),
						child.attribute(
							QStringLiteral("label_y")).toDouble());
			}
			symbol.terminals << terminal;
		} else if (tag == QLatin1String("line")) {
			SymbolShape shape;
			shape.type = SymbolShapeType::Line;
			shape.points << QPointF(
					child.attribute(QStringLiteral("x1")).toDouble(),
					child.attribute(QStringLiteral("y1")).toDouble()) + offset;
			shape.points << QPointF(
					child.attribute(QStringLiteral("x2")).toDouble(),
					child.attribute(QStringLiteral("y2")).toDouble()) + offset;
			shape.style = child.attribute(QStringLiteral("style"),
						      SymbolShape::defaultStyle());
			shape.antialias = child.attribute(QStringLiteral("antialias")) ==
					QLatin1String("true");
			symbol.shapes << shape;
		} else if (tag == QLatin1String("rect") ||
			   tag == QLatin1String("ellipse")) {
			SymbolShape shape;
			shape.type = tag == QLatin1String("rect")
					? SymbolShapeType::Rectangle
					: SymbolShapeType::Ellipse;
			const QPointF top_left(
					child.attribute(QStringLiteral("x")).toDouble(),
					child.attribute(QStringLiteral("y")).toDouble());
			const QSizeF size(
					child.attribute(QStringLiteral("width")).toDouble(),
					child.attribute(QStringLiteral("height")).toDouble());
			shape.points << top_left + offset;
			shape.points << top_left +
					QPointF(size.width(), size.height()) + offset;
			shape.x_radius = child.attribute(QStringLiteral("rx")).toDouble();
			shape.y_radius = child.attribute(QStringLiteral("ry")).toDouble();
			shape.style = child.attribute(QStringLiteral("style"),
						      SymbolShape::defaultStyle());
			shape.antialias = child.attribute(QStringLiteral("antialias")) ==
					QLatin1String("true");
			symbol.shapes << shape;
		} else if (tag == QLatin1String("polygon")) {
			SymbolShape shape;
			shape.type = SymbolShapeType::Polygon;
			int index = 1;
			while (child.hasAttribute(QStringLiteral("x%1").arg(index))) {
				shape.points << QPointF(
						child.attribute(QStringLiteral("x%1").arg(index)).toDouble(),
						child.attribute(QStringLiteral("y%1").arg(index)).toDouble()) + offset;
				index++;
			}
			shape.closed = child.attribute(QStringLiteral("closed")) ==
					QLatin1String("true");
			shape.style = child.attribute(QStringLiteral("style"),
						      SymbolShape::defaultStyle());
			shape.antialias = child.attribute(QStringLiteral("antialias")) ==
					QLatin1String("true");
			symbol.shapes << shape;
		} else if (tag == QLatin1String("dynamic_text")) {
			SymbolText text;
			text.position = QPointF(
					child.attribute(QStringLiteral("x")).toDouble(),
					child.attribute(QStringLiteral("y")).toDouble()) + offset;
			text.rotation =
					child.attribute(QStringLiteral("rotation")).toDouble();
			text.info_key = child.firstChildElement(
						QStringLiteral("info_name")).text();
			text.text = child.firstChildElement(
						QStringLiteral("text")).text();
			text.alignment = alignmentFromXml(child);
			symbol.texts << text;
		} else if (tag == QLatin1String("text")) {
			SymbolText text;
			text.position = QPointF(
					child.attribute(QStringLiteral("x")).toDouble(),
					child.attribute(QStringLiteral("y")).toDouble()) + offset;
			text.rotation =
					child.attribute(QStringLiteral("rotation")).toDouble();
			text.text = child.attribute(QStringLiteral("text"));
			text.alignment = alignmentFromXml(child);
			symbol.texts << text;
		}
		child = child.nextSiblingElement();
	}

	return symbol;
}
