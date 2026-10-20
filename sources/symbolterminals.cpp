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
#include "symbolterminals.h"

#include <QString>

#include <cmath>

namespace {

const QString x_attribute = QStringLiteral("x");
const QString y_attribute = QStringLiteral("y");

	//Every <terminal> of @p definition, in every <description> of it, as
	//the program reads them when it builds an element
QList<QDomElement> terminalsOf(const QDomElement &definition)
{
	QList<QDomElement> terminals;
	for (QDomElement description = definition.firstChildElement(QStringLiteral("description")) ;
		 !description.isNull() ;
		 description = description.nextSiblingElement(QStringLiteral("description")))
	{
		for (QDomElement terminal = description.firstChildElement(QStringLiteral("terminal")) ;
			 !terminal.isNull() ;
			 terminal = terminal.nextSiblingElement(QStringLiteral("terminal")))
		{
			terminals << terminal;
		}
	}
	return terminals;
}

	//The place written on @p terminal, false when x or y is not a number:
	//such a terminal is not read by the program either, and is left alone
bool placeOf(const QDomElement &terminal, QPointF *place)
{
	bool x_ok = false;
	bool y_ok = false;
	const qreal x = terminal.attribute(x_attribute).toDouble(&x_ok);
	const qreal y = terminal.attribute(y_attribute).toDouble(&y_ok);
	if (!x_ok || !y_ok || !std::isfinite(x) || !std::isfinite(y)) {
		return false;
	}
	*place = QPointF(x, y);
	return true;
}

	//Whether @p a and @p b are one point, as Element::savedTerminalUuid()
	//tells two places apart
bool samePlace(const QPointF &a, const QPointF &b)
{
	return qAbs(a.x() - b.x()) < 1e-6 && qAbs(a.y() - b.y()) < 1e-6;
}

	//What putting @p definition on whole numbers does, and the terminals to
	//move, with their place before and after
SymbolTerminals::Rounding survey(const QDomElement &definition,
								 QList<SymbolTerminals::Move> *moves)
{
	SymbolTerminals::Rounding rounding;
	QList<QPointF> before;
	QList<QPointF> after;
	const QList<QDomElement> terminals = terminalsOf(definition);
	for (const QDomElement &terminal : terminals)
	{
		QPointF place;
		if (!placeOf(terminal, &place)) {
			continue;
		}
		const QPointF whole(SymbolTerminals::nearestWhole(place.x()),
							SymbolTerminals::nearestWhole(place.y()));
			//Two terminals the symbol already puts on one point are its own
			//business; two that only rounding brings together are not
		for (int i = 0 ; i < after.size() ; ++i) {
			if (samePlace(after.at(i), whole) && !samePlace(before.at(i), place)) {
				rounding.merged = true;
			}
		}
		before << place;
		after << whole;
		if (!SymbolTerminals::isWhole(place.x()) || !SymbolTerminals::isWhole(place.y()))
		{
			++rounding.terminals;
			if (moves) {
				moves->append(SymbolTerminals::Move{terminal, place, whole});
			}
		}
	}
	return rounding;
}

	//@p value, known to be whole, as a whole number is written: "-16", not
	//"-16.0" nor "-1.6e+01"
QString wholeText(qreal value)
{
	return QString::number(static_cast<qlonglong>(value));
}

}

/**
	@brief SymbolTerminals::isWhole
	@return true when @p value is a whole number, exactly: a terminal at
	-15.9999 is off, and is put on -16
*/
bool SymbolTerminals::isWhole(qreal value)
{
	return std::isfinite(value) && value == std::round(value);
}

/**
	@brief SymbolTerminals::nearestWhole
	@return the whole number nearest to @p value, half always up: 6.17
	gives 6, -15.83 gives -16, 0.5 gives 1 and -0.5 gives 0. Up whatever
	the sign, as the editor of symbols rounds (PartTerminal::wholePoint()):
	a terminal mirrored twice about an axis on a quarter comes back to its
	place, which half away from zero does not give - 22 mirrored about 0.25
	goes to -22, and back to 23
*/
qreal SymbolTerminals::nearestWhole(qreal value)
{
	return std::floor(value + 0.5);
}

/**
	@brief SymbolTerminals::planned
	What putOnWholeNumbers() would do to @p definition, without doing it.
	@param definition : the <definition> of a symbol
	@return the count of the terminals off whole numbers, and whether two of
	them would meet
*/
SymbolTerminals::Rounding SymbolTerminals::planned(const QDomElement &definition)
{
	return survey(definition, nullptr);
}

/**
	@brief SymbolTerminals::putOnWholeNumbers
	Put every terminal of @p definition on whole numbers: x and y, each one
	that is off, to the nearest whole number. The attribute already whole is
	not written again - "10.0" stays as it is -, and nothing but x and y is
	touched: the uuid, the name, the orientation and the drawing stay.

	When two terminals would meet on one point, nothing at all is changed,
	and the result says so.
	@param definition : the <definition> of a symbol, changed in place
	@param moves : filled with the terminals moved, when given - what whoever
	keeps the place of a terminal elsewhere has to move with it
	@return the count of the terminals moved, none when two would meet
*/
SymbolTerminals::Rounding SymbolTerminals::putOnWholeNumbers(const QDomElement &definition,
															 QList<Move> *moves)
{
	QList<Move> planned_moves;
	Rounding rounding = survey(definition, &planned_moves);
	if (rounding.merged) {
		rounding.terminals = 0;
		return rounding;
	}
	for (Move &move : planned_moves)
	{
		if (!isWhole(move.from.x())) {
			move.terminal.setAttribute(x_attribute, wholeText(move.to.x()));
		}
		if (!isWhole(move.from.y())) {
			move.terminal.setAttribute(y_attribute, wholeText(move.to.y()));
		}
	}
	if (moves) {
		*moves << planned_moves;
	}
	return rounding;
}
