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
#ifndef LOCATIONBOUNDARY_H
#define LOCATIONBOUNDARY_H

#include <QString>

/**
	@brief Whether a wire between two locations leaves the panel.
	@param path_a the location path of one end
	@param path_b the location path of the other end
	@return true when the two ends are in different places and neither
	place contains the other

	The whole of the dashed external wire, minus the painting. A free
	function over two strings on purpose: this is the part that can be
	checked without a scene, without a project open and without anybody
	looking at a screen, and a rule that can only be exercised by drawing a
	conductor across a folio is a rule nobody checks.

	@par The rule is ancestry, and it is compared segment by segment

	A wire is external when both paths say something, they say different
	things, and neither is an ancestor of the other. So the body of an
	enclosure and its door - QCM1 and QCM1/PORTE - are one place with two
	names and the wire between them is continuous, while two enclosures in
	one room - PLANTA/QCM1 and PLANTA/QCM2 - are two places and the wire
	between them is dashed even though they share a root.

	Equality is the degenerate case of ancestry and needs no branch of its
	own: a path is trivially an ancestor of itself, so the same comparison
	answers both.

	The comparison walks whole segments and never prefixes of text. QCM1 is
	not an ancestor of QCM10, and a string prefix test would say it is -
	silently, and only on the day somebody numbers an enclosure past nine.

	Case is ignored, because the tree ignores it: two sibling codes that
	differ only in case are one location as far as LocationTree is
	concerned (locationtree.cpp, indexOfSiblingCode). A case sensitive rule
	here would call a boundary between two paths the tree considers the same
	place, and the folio would contradict the location manager.

	@par Three other codings were possible, and two of them fail in silence

	Comparing only the last segment of each path - the leaf - dashes the
	wire that runs from the body of an enclosure to the door of that same
	enclosure. That is wrong information on the drawing, and it is the worst
	of the three, because the schematic then asserts something false with
	conviction.

	Comparing only the first segment - the root - leaves the wire between
	two sibling enclosures continuous. That is a silent omission: whoever
	reads the schematic sees nothing at all, and has no way to know that
	something was missed.

	Climbing the tree to the nearest non virtual ancestor is physically the
	most correct of the four, and it is the one that cannot be used here. It
	needs the virtual flag and the project tree, so it stops being a pure
	rule, stops being testable without a project assembled, and changes its
	answer when somebody ticks a box on another screen.

	@par An empty path never dashes, and that is conservatism

	An empty path means "not known yet" and not "nowhere" - that is settled
	elsewhere, and this rule only has to honour it. Dashing against an
	unknown would paint half of any existing project as external on the day
	the option is first switched on, before a single location has been
	assigned, and the draughtsman would learn to ignore the dashes before
	learning to read them. Both ends have to assert something before
	anything is drawn differently.
*/
bool crossesLocationBoundary(const QString &path_a, const QString &path_b);

#endif // LOCATIONBOUNDARY_H
