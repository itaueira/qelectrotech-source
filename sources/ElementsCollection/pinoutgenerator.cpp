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
#include "pinoutgenerator.h"

#include "../catalog/catalogassignment.h"

#include <QCoreApplication>
#include <QHash>
#include <QMap>
#include <QPolygonF>
#include <QUuid>

#include <algorithm>

namespace
{
		/// how far the number of a terminal is drawn from the connection
		/// point: a fifth of a step, near enough to belong to it and far
		/// enough not to be crossed by the conductor plugged into it
	qreal labelDistance(const SymbolGrid &grid)
	{
		return grid.main_step / 5.0;
	}

		/// and how much further in the label the manufacturer prints
		/// beside the number goes, when there is one
	qreal secondaryDistance(const SymbolGrid &grid)
	{
		return labelDistance(grid) + grid.main_step * 0.8;
	}

		/// the size that second label is written in: under the tag, and
		/// under nothing else
	const int SECONDARY_FONT_SIZE = 6;

		/// what groups two pins into something that may not be cut. A
		/// channel wins over a pair, because a channel is the bigger
		/// promise: it is one point of the field, wiring and all.
	QString unitKeyOf(const CatalogPin &pin)
	{
		if (!pin.channel.isEmpty()) {
			return QStringLiteral("c/") + pin.channel;
		}
		if (!pin.pair.isEmpty()) {
			return QStringLiteral("p/") + pin.pair;
		}
		return QString();
	}
}

bool PinoutGenerator::isValid(QString *error) const
{
	if (!grid.isValid())
	{
		if (error) {
			*error = QCoreApplication::translate("PinoutGenerator",
					"La grille du symbole est invalide : "
					"aucune longueur ne peut en sortir.");
		}
		return false;
	}
	if (!block_template.isValid(error)) {
		return false;
	}
	if (!convention.isValid(error)) {
		return false;
	}
	if (class_key.trimmed().isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("PinoutGenerator",
					"Un bloc doit appartenir a une classe, "
					"sinon le symbole produit ne peut pas "
					"etre enregistre.");
		}
		return false;
	}
	return true;
}

QList<CatalogPin> PinoutGenerator::selectedPins(
		const CatalogPart &part, const QStringList &pin_labels) const
{
	QList<CatalogPin> selected;
	for (const CatalogPin &pin : part.pins)
	{
		if (pin_labels.isEmpty() || pin_labels.contains(pin.label)) {
			selected << pin;
		}
	}

		//Stable, and by the order the catalogue declares. A block whose
		//terminals came out in a different order because the part was
		//read from the database instead of the file would be a second
		//drawing for the same data, and nobody would know which of the
		//two is the one on the sheet.
	std::stable_sort(selected.begin(), selected.end(),
			 [](const CatalogPin &first, const CatalogPin &second)
	{
		return first.order_index < second.order_index;
	});
	return selected;
}

QList<QList<CatalogPin> > PinoutGenerator::split(
		const CatalogPart &part, const QStringList &pin_labels) const
{
	const QList<CatalogPin> pins = selectedPins(part, pin_labels);
	QList<QList<CatalogPin> > blocks;
	if (pins.isEmpty()) {
		return blocks;
	}

		//First the units that may not be cut, in the order their first
		//pin appears. A channel cut in two puts the input on one block
		//and the common that returns it on another, and no list joins
		//them again; a pair cut in two is a contact with one side, which
		//SymbolDefinition::problems() reports as PairIncomplete and
		//refuses to save.
	QList<QList<CatalogPin> > units;
	QHash<QString, int> index_of;
	for (const CatalogPin &pin : pins)
	{
		const QString key = unitKeyOf(pin);
		if (key.isEmpty())
		{
			units << (QList<CatalogPin>() << pin);
			continue;
		}
		if (!index_of.contains(key))
		{
			index_of.insert(key, units.size());
			units << QList<CatalogPin>();
		}
		units[index_of.value(key)] << pin;
	}

	const int ceiling = block_template.max_terminals;
	if (ceiling <= 0)
	{
			//No ceiling means one block, however long. A card of
			//thirty two points drawn as one symbol is a card the
			//sheet can hold; saying so is the class's business.
		QList<CatalogPin> whole;
		for (const QList<CatalogPin> &unit : units) {
			whole << unit;
		}
		blocks << whole;
		return blocks;
	}

	QList<CatalogPin> current;
	for (const QList<CatalogPin> &unit : units)
	{
			//A unit longer than a whole block gets a block to itself
			//rather than being cut: the ceiling is a preference, and
			//the unit is a fact.
		if (unit.size() > ceiling)
		{
			if (!current.isEmpty())
			{
				blocks << current;
				current.clear();
			}
			blocks << unit;
			continue;
		}
		if (current.size() + unit.size() > ceiling)
		{
			blocks << current;
			current.clear();
		}
		current << unit;
	}
	if (!current.isEmpty()) {
		blocks << current;
	}
	return blocks;
}

QList<SymbolDefinition> PinoutGenerator::generate(
		const CatalogPart &part, const QStringList &pin_labels) const
{
	QList<SymbolDefinition> symbols;
	if (!isValid() || part.isNull()) {
		return symbols;
	}

	const QList<QList<CatalogPin> > blocks = split(part, pin_labels);
	for (int index = 0 ; index < blocks.size() ; ++index) {
		symbols << generateOne(part, blocks.at(index),
				       index, blocks.size());
	}
	return symbols;
}

SymbolDefinition PinoutGenerator::generateOne(const CatalogPart &part,
					      const QList<CatalogPin> &pins,
					      int index, int total) const
{
	SymbolDefinition symbol;
	symbol.name = blockName(part, index, total);
	symbol.class_key = class_key;
	symbol.uuid = QUuid::createUuid();

		//The code of the part is the link to the catalogue, and the only
		//thing taken out of it. The values of the part stay where they
		//are: a second copy is a copy that drifts, and T11 joins on the
		//code, not on what happened to be true the day the block was
		//drawn.
		//
		//It has to travel as an element information, because that is the
		//only form of it that survives being written: toXml() writes the
		//class and the default values and never the field, and fromXml()
		//reads the code back out of the entry named part_code. Setting
		//the field alone would give a block that has a part until the
		//moment it is saved. The keys are asked of the assignment so
		//that a block and a part assigned by hand write the same two.
	symbol.default_part_code = part.code;
	symbol.default_part_values.insert(CatalogAssignment::partCodeKey(),
					  part.code);
	symbol.default_part_values.insert(CatalogAssignment::partRevisionKey(),
					  QString::number(part.revision));

	QMap<Qet::Orientation, QList<CatalogPin> > by_side;
	for (const CatalogPin &pin : pins) {
		by_side[block_template.sideOf(pin.role, convention)] << pin;
	}

		//The body is as long as its longest edge needs and never shorter
		//than the width the class asks for, which is therefore a minimum
		//and not a size. Both come out of the template, so both are a
		//whole number of grid steps.
	const qreal minimum = block_template.width(grid);
	const qreal body_width = qMax(minimum, qMax(
			block_template.lengthFor(by_side.value(Qet::North).size(),
						 grid),
			block_template.lengthFor(by_side.value(Qet::South).size(),
						 grid)));
	const qreal body_height = qMax(minimum, qMax(
			block_template.lengthFor(by_side.value(Qet::East).size(),
						 grid),
			block_template.lengthFor(by_side.value(Qet::West).size(),
						 grid)));

		//One rectangle, and nothing else. The terminals sit on its edge
		//and point outwards, without the little stub line a symbol drawn
		//by hand usually has: on a card of thirty two points those
		//thirty two stubs are the difference between a block and a
		//thicket.
	QPolygonF corners;
	corners << QPointF(0.0, 0.0) << QPointF(body_width, body_height);
	symbol.shapes << SymbolShape(SymbolShapeType::Rectangle, corners);

		//The tag above the block, centred on it. It is the one text a
		//block always carries, because a symbol nobody can name on the
		//sheet is a symbol nobody finds in the list.
	SymbolText tag = SymbolText::tagField(
			QPointF(body_width / 2.0, -grid.main_step / 2.0));
	tag.alignment = Qt::AlignHCenter | Qt::AlignBottom;
	symbol.texts << tag;

	const qreal label_distance = labelDistance(grid);
	const qreal secondary_distance = secondaryDistance(grid);

	for (const Qet::Orientation side : SymbolTerminal::allOrientations())
	{
		const QList<CatalogPin> on_side = by_side.value(side);
		for (int slot = 0 ; slot < on_side.size() ; ++slot)
		{
			const CatalogPin &pin = on_side.at(slot);
			const qreal offset = block_template.offsetOf(slot, grid);

			QPointF position;
			switch (side)
			{
				case Qet::North:
					position = QPointF(offset, 0.0);
					break;
				case Qet::South:
					position = QPointF(offset, body_height);
					break;
				case Qet::West:
					position = QPointF(0.0, offset);
					break;
				case Qet::East:
					position = QPointF(body_width, offset);
					break;
			}

			SymbolTerminal terminal(position, side);
			terminal.label = pin.label;
			terminal.role = pin.role;
			terminal.pair = pin.pair;
			terminal.uuid = QUuid::createUuid();

				//The number is drawn by the terminal that owns
				//it, and never as a loose text beside it: that
				//is the whole promise of these blocks, and the
				//reason the boards of the real project carry
				//248 texts around 197 terminals named "".
				//
				//Its position is deliberately not snapped to the
				//grid, and it is the one length here that is
				//not. A label is not a connection point; nothing
				//plugs into it, and rounding it to a whole step
				//would push the number a full step into the body
				//instead of leaving it beside the terminal.
			terminal.show_name = true;
			terminal.label_pos = SymbolTerminal::suggestedLabelPos(
					side, label_distance);
			symbol.terminals << terminal;

			if (pin.secondary_label.isEmpty()) {
				continue;
			}

				//And the name the manufacturer prints beside the
				//number, only where there is one. A free text,
				//because it belongs to the drawing and not to
				//the connection.
			SymbolText secondary;
			secondary.position = position
					+ SymbolTerminal::suggestedLabelPos(
						side, secondary_distance);
			secondary.text = pin.secondary_label;
			secondary.font_size = SECONDARY_FONT_SIZE;
			secondary.alignment =
					SymbolTerminal::labelHAlignment(side)
					| SymbolTerminal::labelVAlignment(side);
			symbol.texts << secondary;
		}
	}

		//The first terminal, which on a block is the first one on the
		//top edge. Asking the symbol itself keeps the answer the same as
		//the one a symbol drawn by hand gets.
	symbol.hotspot = symbol.suggestedHotspot(grid);
	return symbol;
}

QString PinoutGenerator::blockName(const CatalogPart &part,
				   int index, int total)
{
	if (total <= 1) {
		return part.code;
	}
	return QStringLiteral("%1 %2/%3").arg(part.code)
			.arg(index + 1).arg(total);
}

CatalogPinRole PinoutGenerator::ioRoleOf(const QList<CatalogPin> &pins)
{
	CatalogPinRole found = CatalogPinRole::Unknown;
	for (const CatalogPin &pin : pins)
	{
			//Everything that is not a point of the field is passed
			//over rather than counted as a second kind: the supply
			//and the common of a card are on every card there is,
			//and counting them would make every card mixed.
		if (!CatalogPin::isIoRole(pin.role)) {
			continue;
		}
		if (found == CatalogPinRole::Unknown)
		{
			found = pin.role;
			continue;
		}
		if (found != pin.role) {
			return CatalogPinRole::Unknown;
		}
	}
	return found;
}
