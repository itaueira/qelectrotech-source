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
#ifndef GRIDDOTS_H
#define GRIDDOTS_H

#include <QtGlobal>

/**
	Which dots of the folio grid are drawn. A dot is drawn with a cosmetic
	pen, a fixed number of pixels whatever the zoom, so a small grid step
	seen from afar puts the dots closer than their own width: a grid of 1
	at zoom 1 is one dot per pixel, and the folio turns into a grey wash.
	A dot cannot be drawn smaller than a pixel, so what keeps the grid
	readable is drawing fewer of them.
*/
namespace GridDots
{
		/// The narrowest gap, in pixels, between two dots drawn. The
		/// default grid of 10 is never closer than this where it is
		/// drawn at all, above zoom 0.5, so it is left as it always was.
	inline constexpr qreal min_gap_px = 5.0;

	/**
		@return every how many grid steps a dot is drawn: 1 draws every
		point of the grid, then 2, 5, 10, 20, 50... the smallest of those
		that keeps the dots @ref min_gap_px apart, and wider than twice
		their own width. The dots drawn are still points of the grid, only
		fewer of them, so what is seen stays where things snap.
		@param grid_step : the grid step, in folio units
		@param zoom : pixels per folio unit
		@param dot_width : the width of a dot, in pixels
	*/
	inline int stride(int grid_step, qreal zoom, int dot_width = 1)
	{
		if (grid_step <= 0 || zoom <= 0) {
			return 1;
		}
		const qreal wanted = qMax(min_gap_px, 2.0 * qMax(1, dot_width) + 1);
		int stride = 1;
		for (int decade = 1; decade <= 1000000; decade *= 10) {
			for (const int mantissa : {1, 2, 5}) {
				stride = mantissa * decade;
				if (grid_step * stride * zoom >= wanted) {
					return stride;
				}
			}
		}
		return stride;
	}
}

#endif // GRIDDOTS_H
