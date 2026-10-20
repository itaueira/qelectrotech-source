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
#ifndef CONDUCTORTEXTFIT_H
#define CONDUCTORTEXTFIT_H

#include <QtGlobal>

#include <functional>

/**
	The size the name of a conductor is drawn at, so that it fits the run
	it is written along: the longest segment, or the whole conductor when
	it is straight (Conductor::posForText()).

	The size the user chose stays the one written in the project file
	(ConductorProperties::text_size); this only says how large the text
	is drawn. It only ever makes the text smaller, never larger, and never
	smaller than @ref smallest: a name that does not fit even then is drawn
	at that size and runs past the end of the run.

	Numbers in, a number out: the width of the text at a given size comes
	from the caller, which measures it on the text item itself.
*/
namespace ConductorTextFit
{
		/// The smallest size, in points, a name is reduced to
	inline constexpr int smallest = 5;

		/**
			Room kept free at each end of the run, in scene units, between
			the box of the text and the corner or the terminal the run ends
			at. The box is what Conductor::calculateTextItemPosition() pushes
			away from the conductor until the two no longer touch, and at a
			corner the next segment is a stroke 1 wide - 5 under the mouse -,
			so half of it, up to 2.5, lies on the run's side of the corner.
			A box that reached the corner would touch it, and be pushed off
			the wire along that next segment. 3 clears the widest stroke with
			half a unit to spare; the box itself already carries the blank
			margin of the text document on each side, so the letters end
			further in than this.
		*/
	inline constexpr qreal end_clearance = 3.0;

	/**
		@return the length of a run of @p run_length that a text may take:
		the run less @ref end_clearance at each end; 0 when nothing is left,
		and for a length that is not a number.
	*/
	inline qreal room(qreal run_length)
	{
		const qreal r = qAbs(run_length) - 2 * end_clearance;
		return r > 0 ? r : 0;
	}

	/**
		@return the size to draw a text at: the largest whole size from
		@p recorded down to @ref smallest at which @p width_at says the text
		takes no more than @p room; @ref smallest when none of them does.
		A @p recorded size already at or below @ref smallest is returned as
		it is, and is not measured: the rule reduces, it never enlarges.

		@param recorded the size the user chose, as written in the file
		@param room the length the text may take, see room()
		@param width_at the length the text takes, along the run, when
		drawn at the size it is given; a length that is not a number never
		fits
	*/
	inline int fittingSize(int recorded,
				   qreal room,
				   const std::function<qreal(int)> &width_at)
	{
		if (recorded <= smallest)
			return recorded;
		for (int size = recorded ; size > smallest ; --size)
		{
			if (width_at(size) <= room)
				return size;
		}
		return smallest;
	}
}

#endif // CONDUCTORTEXTFIT_H
