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
#ifndef WIRELANES_H
#define WIRELANES_H

#include <QVector>

/**
	@brief Where the bridge of each conductor goes, in the space it crosses.

	The automatic path of a conductor between two terminals that face each
	other (Conductor::generateConductorPath()) leaves one terminal, runs to
	a bridge, and arrives at the other one. The bridge is put half way
	between the two terminal ends, whatever lies there: two conductors
	between the same two columns draw their bridges on top of each other,
	and a bridge often runs along the side of a frame drawn around a group
	of symbols.

	This picks the coordinate of each bridge instead, without changing the
	shape of the conductor: in the middle of the free space, outside the
	frames when there is room between them, and the bridges that would
	touch on separate lanes, in the order that does not make them cross.

	Pure on purpose: numbers in, numbers out. Everything is in the frame of
	a vertical bridge: "along" is the coordinate chosen for it (x), and
	"across" the direction the bridge runs (y). A horizontal bridge is the
	same with the two axes swapped, which is the caller's business.
*/
namespace WireLanes
{
		/// One conductor, as the arrangement sees it
	struct Wire
	{
			/// The room for the bridge, between the two terminal ends:
			/// low < high
		double low = 0;
		double high = 0;
			/// Across, the run that comes from the low end, and the run
			/// that goes to the high end
		double from = 0;
		double to = 0;
	};

		/// Where no bridge may run: the body of a symbol with its margin,
		/// or the bridge of a conductor that is not being arranged
	struct Block
	{
		double low;
		double high;
		double from;
		double to;
	};

		/// A shape drawn on the folio, by the box around it. Its two sides
		/// cut the room in pieces, and a piece inside it is inside a frame.
		/// A line has two equal sides.
	struct Frame
	{
		double low;
		double high;
		double from;
		double to;
	};

	struct Request
	{
		QVector<Wire> wires;
		QVector<Block> blocks;
		QVector<Frame> frames;
			/// A piece of room where the lanes would be closer than this
			/// is too narrow, and the next one is tried
		double min_pitch = 4;
			/// Two bridges closer than this across are in conflict
		double gap = 10;
			/// The lanes, and the room between them, are whole numbers of
			/// this: 1, or the grid of the folio when it is finer
		double unit = 1;
	};

	struct Result
	{
			/// For each wire, the along coordinate of its bridge, or NaN
			/// for a wire left as it is: straight, or with no room
		QVector<double> at;
			/// For each wire, the group it was arranged with, or -1. Two
			/// wires of one group were placed together; changing one may
			/// move the others.
		QVector<int> group;
	};

	Result arrange(const Request &request);

		/// A piece of the room, and how many frames it is inside
	struct Piece
	{
		double low;
		double high;
		int frames;
	};

	QVector<Piece> pieces(double low, double high, double from, double to,
			      const QVector<Block> &blocks, const QVector<Frame> &frames);
	bool inConflict(const Wire &a, const Wire &b, double gap);
	int orderCost(const Wire &left, const Wire &right, double gap);
	QVector<int> lanes(const QVector<Wire> &wires, double gap);
	QVector<double> positions(int lane_count, double low, double high, double unit = 1);

		/// A straight line: at the across coordinate @a at, from @a a to
		/// @a b along - or, among the lines that run across, at the along
		/// coordinate @a at, from @a a to @a b across
	struct Line
	{
		double at;
		double a;
		double b;
	};

		/// The end of a terminal, which a wire must not pass close to
	struct Spot
	{
		double along;
		double across;
	};

		/// What a straight piece of a wire keeps clear of, besides the
		/// wires being arranged with it
	struct Clearance
	{
			/// Lines in the direction of the piece: closer than @a line,
			/// they would read as one wire with it
		QVector<Line> parallel;
			/// Lines across the piece, which it may neither cross nor pass
			/// closer than @a line to the end of
		QVector<Line> crossing;
		QVector<Spot> spots;
		QVector<Block> bodies;
		double line = 3;
		double spot = 5;
	};

	bool isClear(double at, double a, double b, const Clearance &clearance);
	double runAt(double start, double outward, double a, double b,
		     const Clearance &clearance, double longest, double step = 1);
}

#endif // WIRELANES_H
