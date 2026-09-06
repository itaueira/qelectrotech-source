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
#ifndef TERMINALSTRIPSTRUCTURE_H
#define TERMINALSTRIPSTRUCTURE_H

#include "../properties/elementdata.h"

#include <QString>
#include <QVector>

/**
	@brief One level of one physical terminal, as plain data.

	The mirror of RealTerminal, minus everything that needs a folio: no
	Element, no pointer to one, no conductor. That is what lets the rules
	below be checked without a project open, and it is also what a terminal
	strip needs anyway, since a terminal can be decided on before anybody
	draws it.

	@par wire is one number, and a terminal that is not a through type has two

	wire is the number the automatic bridging compares. On a through
	terminal both sides carry it, so a single field tells the whole truth.
	On a fuse or a sectional terminal the two sides carry different numbers
	and this field cannot say which of the two it holds - which is precisely
	why such a terminal is never proposed for automatic bridging. Teaching
	this structure about sides is somebody else's step; refusing to guess is
	this one's.

	@par Which side of the strip a terminal faces is deliberately absent

	Nothing here says whether a terminal faces the inside of an enclosure or
	the field, and nothing here assumes an answer either. An installation
	where every terminal is internal and one where the distinction matters
	are both described by these fields. The day the distinction has to be
	stored it arrives as one more optional member, with the common case as
	its default, and no saved project has to be migrated - which is the
	cheap direction. Writing the assumption down now would be the expensive
	one.
*/
struct StripLevel
{
		/// The bridge identifier of a level that no bridge holds.
	static constexpr int NoBridge = -1;

		/// What is written on the terminal, "7" or "21".
	QString label;

		/// The wire number this level carries. Empty means not known.
	QString wire;

		/// Through, fuse, sectional, diode or ground.
	ElementData::TerminalType type = ElementData::TTGeneric;

		/// Phase, neutral, or nothing in particular.
	ElementData::TerminalFunction function = ElementData::TFGeneric;

		/**
			An opaque identifier of the bridge already holding this level,
			or NoBridge. Opaque on purpose: the rules need to know whether
			two levels are held by the same bridge and never which bridge
			it is, so this layer stays clear of TerminalStripBridge.
		*/
	int bridge = NoBridge;
};

/**
	@brief One physical terminal: a position on the strip, and the levels
	stacked on it.

	The mirror of PhysicalTerminal. The index in levels is the level itself,
	base zero, and level zero is the one nearest the mounting plate - the
	same convention PhysicalTerminal documents and the drawer counts from.
	The number the person reading the drawing sees is not this one: see
	terminalLevelLabel.

	A single level terminal is a vector of one entry, and not a special
	case: a rule that had to branch on it would be a rule with a path only
	multilevel strips ever exercise.
*/
struct StripTerminal
{
	QVector<StripLevel> levels;
};

/**
	@brief One terminal strip, in the order it is mounted.

	The index in terminals is the position on the rail, so two terminals are
	neighbours when their indices differ by one. Everything the bridging
	rule needs to answer is in that sentence: a bridge is a comb, a comb
	spans terminals that touch, and it cannot step over one.

	This is a description and not a store. It is built from whatever knows
	the strip, used, and thrown away; nothing here is written to a project
	file, which is what leaves the question of how a strip is saved open for
	the step that has to answer it.
*/
struct TerminalStripStructure
{
	QVector<StripTerminal> terminals;
};

/**
	@brief The label a level index is shown as.
	@param level a level index, base zero
	@return T1 for level zero, T2 for level one, and so on; an empty string
	for a negative index

	The one place where the internal count and the displayed one meet, and
	it exists so that there is only one. The count stays base zero because
	that is what the strip, the drawer and the saved file already use; the
	label starts at one because that is what is printed on the strip and
	what is already written on existing folios.

	Not translated, and that is a decision rather than an oversight: this
	mark has to match what a fitter reads off the physical terminal and what
	earlier drawings already say. A translator free to turn T into something
	else would silently break that match on the drawings, not in the
	software.
*/
QString terminalLevelLabel(int level);

/**
	@brief How many levels the tallest terminal of a strip has.
	@return zero for a strip with no terminal at all
*/
int maximumLevelCount(const TerminalStripStructure &structure);

/**
	@brief Whether both sides of a terminal of this type carry the same wire.
	@return true for a through terminal

	A through terminal is one continuous conductor with a screw at each end:
	the wire arriving and the wire leaving are the same wire, and they carry
	the same number. A ground terminal is one too, through the rail instead
	of through the body, and its two sides are still the same potential.

	A fuse, a sectional and a diode terminal are not: something sits between
	the two sides - a cartridge, a knife, a junction - so the two sides are
	two different potentials with two different numbers, and no reading of
	one side says anything about the other.

	The diode is the one worth stating, because the specification names only
	the fuse and the sectional. It is counted as not through on purpose, and
	the asymmetry of being wrong decides it: refusing to bridge automatically
	costs somebody a bridge they can still make by hand, while bridging
	automatically across a diode asserts a connection nobody asked for and
	that the schematic then shows as fact.
*/
bool isThroughTerminalType(ElementData::TerminalType type);

/**
	@brief One bridge that "bridge everything on the same wire" would make.

	level and positions say where the comb goes: one level, and the
	positions it spans, in order and always adjacent. wire is the number
	that justified it, so that whoever is asked to confirm the operation is
	told why, and not only what.
*/
struct StripBridgeProposal
{
	int level = 0;
	QString wire;
	QVector<int> positions;
};

/**
	@brief The bridges that bridging by wire number would make on this strip.
	@param structure the strip, in mounting order
	@return one proposal per comb, ordered by level and then by position

	The command behind CU-33.2 and the fourth heading of the specification:
	terminals sharing a potential are nearly always bridged, so the program
	offers to bridge everything that shares a wire number. What it must
	never do is offer it for a terminal whose two sides are two different
	wires.

	@par The non through terminal breaks the run, and does not merely leave it

	Skipping it would not be enough, and skipping it would be wrong. Three
	terminals on wire L1 with a fuse in the middle cannot be combed one to
	three: a comb is a continuous piece of metal, it has to pass over the
	middle terminal, and TerminalStrip::isBridgeable refuses a set of
	positions that is not consecutive anyway. So the fuse cuts the run in
	two, and each half has to earn its own bridge.

	@par An unknown wire number bridges nothing

	Two terminals whose number nobody has filled in are not two terminals on
	the same potential; they are two terminals nobody has said anything
	about yet. Bridging them would turn an absence of information into an
	assertion on the drawing, and the person reading it would have no way of
	telling the two apart.

	@par What comes out is what the strip will accept

	Every proposal satisfies, by construction, the four conditions
	TerminalStrip::isBridgeable checks: at least two terminals, all at the
	same level, one per physical terminal, positions consecutive, at most
	one bridge already involved and at least one terminal still free. A run
	already held whole by one bridge is dropped rather than proposed, since
	making it again would change nothing.
*/
QVector<StripBridgeProposal> proposeBridgesByWire(const TerminalStripStructure &structure);

#endif // TERMINALSTRIPSTRUCTURE_H
