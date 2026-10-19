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
#include "wirelanes.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	using WireLanes::Wire;

	constexpr double eps = 1e-6;

	double spanLow(const Wire &w) { return std::min(w.from, w.to); }
	double spanHigh(const Wire &w) { return std::max(w.from, w.to); }

		/// A wire whose bridge is shorter than one unit is straight
	bool hasBridge(const Wire &w)
	{
		return std::abs(w.to - w.from) >= 1.0 && w.high - w.low > eps;
	}

		/// A run of a wire: along the folio at the across coordinate @a at,
		/// from @a a to @a b along
	struct Run
	{
		double at;
		double a;
		double b;
	};

		/// The two runs of @a w when its bridge is at @a x
	void runs(const Wire &w, double x, Run &first, Run &second)
	{
		first = {w.from, std::min(w.low, x), std::max(w.low, x)};
		second = {w.to, std::min(x, w.high), std::max(x, w.high)};
	}

		/// Whether the run @a r crosses the bridge at @a x of @a w, inside
		/// both and not at an end
	bool crosses(const Run &r, double x, const Wire &w)
	{
		return x > r.a + eps && x < r.b - eps
				&& r.at > spanLow(w) + eps && r.at < spanHigh(w) - eps;
	}

		/// Whether two runs go side by side, closer than @a gap
	bool alongside(const Run &r, const Run &s, double gap)
	{
		return std::abs(r.at - s.at) < gap
				&& std::min(r.b, s.b) - std::max(r.a, s.a) > eps;
	}

		/// The order to fall back on, when the geometry does not choose:
		/// two wires that go down, the lower one on the left; two that go
		/// up, the upper one; otherwise the one that starts higher.
	bool leftFirst(const Wire &a, const Wire &b)
	{
		const bool a_down = a.to > a.from;
		const bool b_down = b.to > b.from;
		if (a_down && b_down) return a.from > b.from + eps;
		if (!a_down && !b_down) return a.from < b.from - eps;
		return spanLow(a) < spanLow(b) - eps;
	}

	int find(QVector<int> &parent, int i)
	{
		while (parent[i] != i) {
			parent[i] = parent[parent[i]];
			i = parent[i];
		}
		return i;
	}
}

/**
	@brief WireLanes::pieces
	The room between @a low and @a high, cut by the sides of the frames and
	without the blocks, for a bridge running across from @a from to @a to.
	Only what lies across that run counts. A block that covers the whole
	room is a frame drawn as one symbol around the wire, and counts as a
	frame.
	@return the pieces, in order along, each with the number of frames it
	is inside
*/
QVector<WireLanes::Piece> WireLanes::pieces(double low, double high, double from, double to,
					     const QVector<Block> &blocks,
					     const QVector<Frame> &frames)
{
	const double span_low = std::min(from, to);
	const double span_high = std::max(from, to);
	const auto acrossOverlap = [&](double f, double t) {
		return std::min(span_high, std::max(f, t)) - std::max(span_low, std::min(f, t)) > eps;
	};

	QVector<Frame> around;
	QVector<double> cuts;
	for (const Frame &f : frames)
	{
		if (!acrossOverlap(f.from, f.to)) continue;
		around << f;
		for (double side : {f.low, f.high}) {
			if (side > low + eps && side < high - eps) cuts << side;
		}
	}
	std::sort(cuts.begin(), cuts.end());

	QVector<std::pair<double, double>> free;
	double start = low;
	for (double cut : cuts) {
		if (cut - start > eps) free << std::make_pair(start, cut);
		start = std::max(start, cut);
	}
	if (high - start > eps) free << std::make_pair(start, high);

	for (const Block &b : blocks)
	{
		if (!acrossOverlap(b.from, b.to)) continue;
		const double b_low = std::min(b.low, b.high);
		const double b_high = std::max(b.low, b.high);
		if (b_low <= low + eps && b_high >= high - eps) {
			around << Frame{b_low, b_high, b.from, b.to};
			continue;
		}
		QVector<std::pair<double, double>> left;
		for (const auto &p : free)
		{
			if (b_high <= p.first + eps || b_low >= p.second - eps) {
				left << p;
				continue;
			}
			if (b_low - p.first > eps) left << std::make_pair(p.first, b_low);
			if (p.second - b_high > eps) left << std::make_pair(b_high, p.second);
		}
		free = left;
	}

	const double across_middle = (span_low + span_high) / 2;
	QVector<Piece> result;
	for (const auto &p : free)
	{
		const double middle = (p.first + p.second) / 2;
		int inside = 0;
		for (const Frame &f : around) {
			if (middle > std::min(f.low, f.high) + eps && middle < std::max(f.low, f.high) - eps
				&& across_middle > std::min(f.from, f.to) + eps
				&& across_middle < std::max(f.from, f.to) - eps) {
				++inside;
			}
		}
		result << Piece{p.first, p.second, inside};
	}
	return result;
}

/**
	@brief WireLanes::inConflict
	@return true if the bridges of @a a and @a b could meet: their rooms
	overlap, and across they overlap or come closer than @a gap - two
	bridges on one line with a small gap between them read as one wire.
*/
bool WireLanes::inConflict(const Wire &a, const Wire &b, double gap)
{
	return std::min(a.high, b.high) - std::max(a.low, b.low) > eps
			&& std::min(spanHigh(a), spanHigh(b)) - std::max(spanLow(a), spanLow(b)) > -gap;
}

/**
	@brief WireLanes::orderCost
	What it costs to put the bridge of @a left on the left of the bridge of
	@a right: two for each time a run of one crosses the bridge of the
	other, and one for each pair of runs that go side by side closer than
	@a gap.
*/
int WireLanes::orderCost(const Wire &left, const Wire &right, double gap)
{
	const double low = std::max(left.low, right.low);
	const double high = std::min(left.high, right.high);
	const double xl = low + (high - low) / 3;
	const double xr = low + 2 * (high - low) / 3;

	Run l1, l2, r1, r2;
	runs(left, xl, l1, l2);
	runs(right, xr, r1, r2);

	int cost = 0;
	for (const Run &r : {l1, l2}) {
		if (crosses(r, xr, right)) cost += 2;
	}
	for (const Run &r : {r1, r2}) {
		if (crosses(r, xl, left)) cost += 2;
	}
	for (const Run &r : {l1, l2}) {
		for (const Run &s : {r1, r2}) {
			if (alongside(r, s, gap)) cost += 1;
		}
	}
	return cost;
}

/**
	@brief WireLanes::lanes
	The lane of each of @a wires, counted from the left. Two wires in
	conflict get different lanes, in the order that costs less (orderCost());
	two that are not may share one.
*/
QVector<int> WireLanes::lanes(const QVector<Wire> &wires, double gap)
{
	const int n = wires.size();
	QVector<QVector<bool>> left_of(n, QVector<bool>(n, false));
	QVector<int> before(n, 0);
	for (int i = 0 ; i < n ; ++i)
	{
		for (int j = i + 1 ; j < n ; ++j)
		{
			if (!inConflict(wires.at(i), wires.at(j), gap)) continue;
			const int ij = orderCost(wires.at(i), wires.at(j), gap);
			const int ji = orderCost(wires.at(j), wires.at(i), gap);
			bool i_first = ij < ji;
			if (ij == ji) {
				i_first = leftFirst(wires.at(i), wires.at(j))
						|| !leftFirst(wires.at(j), wires.at(i));
			}
			if (i_first) {
				left_of[i][j] = true;
				++before[j];
			} else {
				left_of[j][i] = true;
				++before[i];
			}
		}
	}

		// One after another, left to right: always a wire with nothing
		// left to put on its left, the fallback order choosing among them.
		// Should the costs ever go round in a circle, the wire with the
		// fewest left is taken anyway.
	QVector<int> order;
	QVector<bool> placed(n, false);
	while (order.size() < n)
	{
		int best = -1;
		for (int i = 0 ; i < n ; ++i)
		{
			if (placed.at(i)) continue;
			if (best < 0 || before.at(i) < before.at(best)
				|| (before.at(i) == before.at(best) && leftFirst(wires.at(i), wires.at(best)))) {
				best = i;
			}
		}
		placed[best] = true;
		order << best;
		for (int j = 0 ; j < n ; ++j) {
			if (left_of.at(best).at(j)) --before[j];
		}
	}

	QVector<int> lane(n, 0);
	for (int k = 0 ; k < n ; ++k)
	{
		const int v = order.at(k);
		for (int m = 0 ; m < k ; ++m)
		{
			const int u = order.at(m);
			if (inConflict(wires.at(u), wires.at(v), gap)) {
				lane[v] = std::max(lane.at(v), lane.at(u) + 1);
			}
		}
	}
	return lane;
}

/**
	@brief WireLanes::positions
	Where @a lane_count lanes go between @a low and @a high: centered, at
	most @a max_pitch apart, and with as much room left at each end as
	between two lanes when they do not fit at that pitch. On @a grid when
	they all fit on it, else on whole units, else as they are.
	On a grid the pitch is a whole number of its steps, the largest that
	fits, and the lanes are centered as one block: rounding each lane on
	its own would leave gaps of 5 and 6 side by side for a pitch of 5.85,
	and the eye reads that as uneven. Fitting on a grid also means no lane
	may lose more than a quarter of the room it would have had at either
	end, counted up to the pitch - five lanes on a grid of 10 between a
	frame and the terminals would otherwise be pushed against the frame,
	the thing the arrangement is for.
*/
QVector<double> WireLanes::positions(int lane_count, double low, double high,
				     double max_pitch, double grid)
{
	QVector<double> exact;
	if (lane_count <= 0) return exact;
	const double pitch = std::min(max_pitch, (high - low) / (lane_count + 1));
	const double middle = (low + high) / 2;
	for (int k = 0 ; k < lane_count ; ++k) {
		exact << middle + (k - (lane_count - 1) / 2.0) * pitch;
	}

	const double room = std::min(exact.first() - low, high - exact.last());
	const double least = std::min(room, max_pitch) * 3 / 4;

	QVector<double> steps;
	if (grid >= 1) steps << grid;
	if (grid != 1) steps << 1;
	for (double step : steps)
	{
		const double whole = std::floor(pitch / step + eps) * step;
		if (lane_count > 1 && whole < step - eps) continue;
		const double block = whole * (lane_count - 1);
		const double first = std::round((middle - block / 2) / step) * step;
		QVector<double> snapped;
		for (int k = 0 ; k < lane_count ; ++k) {
			snapped << first + k * whole;
		}
		if (snapped.first() > low + eps && snapped.last() < high - eps
			&& std::min(snapped.first() - low, high - snapped.last()) >= least - eps) {
			return snapped;
		}
	}
	return exact;
}

/**
	@brief WireLanes::arrange
	The bridge of each wire of @a request: the wires in conflict, directly
	or through others, form a group; the group gets lanes() in the room
	all of its wires share, in the piece of that room inside the fewest
	frames and then the widest, as long as the lanes are no closer than
	the minimum there; else in the widest piece.
*/
WireLanes::Result WireLanes::arrange(const Request &request)
{
	const int n = request.wires.size();
	Result result;
	result.at = QVector<double>(n, std::numeric_limits<double>::quiet_NaN());
	result.group = QVector<int>(n, -1);

	QVector<int> parent(n);
	for (int i = 0 ; i < n ; ++i) parent[i] = i;
	for (int i = 0 ; i < n ; ++i)
	{
		if (!hasBridge(request.wires.at(i))) continue;
		for (int j = i + 1 ; j < n ; ++j)
		{
			if (hasBridge(request.wires.at(j))
				&& inConflict(request.wires.at(i), request.wires.at(j), request.gap)) {
				parent[find(parent, j)] = find(parent, i);
			}
		}
	}

	QVector<QVector<int>> groups;
	QVector<int> group_of_root(n, -1);
	for (int i = 0 ; i < n ; ++i)
	{
		if (!hasBridge(request.wires.at(i))) continue;
		const int root = find(parent, i);
		if (group_of_root.at(root) < 0) {
			group_of_root[root] = groups.size();
			groups << QVector<int>();
		}
		groups[group_of_root.at(root)] << i;
	}

	int next_group = 0;
	const auto place = [&](const QVector<int> &members) {
		double low = -std::numeric_limits<double>::infinity();
		double high = std::numeric_limits<double>::infinity();
		double span_low = std::numeric_limits<double>::infinity();
		double span_high = -std::numeric_limits<double>::infinity();
		QVector<Wire> wires;
		for (int m : members)
		{
			const Wire &w = request.wires.at(m);
			low = std::max(low, w.low);
			high = std::min(high, w.high);
			span_low = std::min(span_low, spanLow(w));
			span_high = std::max(span_high, spanHigh(w));
			wires << w;
		}
		if (high - low <= eps) return false;

		const QVector<int> lane = lanes(wires, request.gap);
		const int count = *std::max_element(lane.cbegin(), lane.cend()) + 1;

		QVector<Piece> candidates = pieces(low, high, span_low, span_high,
						   request.blocks, request.frames);
		if (candidates.isEmpty()) return true;
		std::stable_sort(candidates.begin(), candidates.end(), [](const Piece &a, const Piece &b) {
			if (a.frames != b.frames) return a.frames < b.frames;
			return a.high - a.low > b.high - b.low;
		});
		const Piece *chosen = nullptr;
		for (const Piece &p : candidates)
		{
			if ((p.high - p.low) / (count + 1) >= request.min_pitch - eps) {
				chosen = &p;
				break;
			}
		}
		if (!chosen) {
			chosen = &*std::max_element(candidates.cbegin(), candidates.cend(),
						    [](const Piece &a, const Piece &b) {
				return a.high - a.low < b.high - b.low;
			});
		}

		const QVector<double> at = positions(count, chosen->low, chosen->high,
						     request.max_pitch, request.grid);
		const int id = next_group++;
		for (int k = 0 ; k < members.size() ; ++k)
		{
			result.at[members.at(k)] = at.at(lane.at(k));
			result.group[members.at(k)] = id;
		}
		return true;
	};

	for (const QVector<int> &members : groups)
	{
			// A chain whose two ends share no room: each wire on its own
		if (!place(members)) {
			for (int m : members) place({m});
		}
	}
	return result;
}
