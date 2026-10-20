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
#ifndef FOLIOGRID_H
#define FOLIOGRID_H

#include <QString>
#include <QVariant>
#include <QtGlobal>
#include <cmath>

/**
	The folio grid: the step, in pixels, that symbols snap to. Both steps
	are kept in the settings, and may be a fraction - 0.5, 0.25 -, down to
	@ref smallest; a hand-edited or damaged settings file can hold 0, a
	negative number or text, and the code that divides by the step or
	loops over it must never see such a value.
*/
namespace FolioGrid
{
		/// QSettings keys holding the two steps.
	inline const QString x_key{QStringLiteral("diagrameditor/Xgrid")};
	inline const QString y_key{QStringLiteral("diagrameditor/Ygrid")};

		/// The finest step the preferences offer, and the code accepts
	inline constexpr qreal smallest = 0.1;
		/// Far more than any folio is wide
	inline constexpr qreal largest = 1000000;

	/**
		@return the step held in @p value, a settings entry; or @p fallback,
		the built-in step, when the entry is missing, not a number, less
		than @ref smallest, or more than @ref largest.
	*/
	inline qreal step(const QVariant &value, qreal fallback)
	{
		bool ok = false;
		const double s = value.toDouble(&ok);
		if (!ok || !std::isfinite(s) || s < smallest - 1e-9 || s > largest)
			return fallback;
		return s;
	}

	/**
		@return @p v on the grid of @p step: the nearest multiple of it,
		cleared of the remainder a fractional step leaves in the last
		digits - 103 times 0.1 is not 10.3 in floating point, and a
		position that is a hair off the grid reads as off the grid to
		every comparison after it.
	*/
	inline qreal snap(qreal v, qreal step)
	{
		if (step <= 0) return v;
		const qreal snapped = std::round(v / step) * step;
		return std::round(snapped * 1e6) / 1e6;
	}

	/**
		@return the step a symbol is placed on with Ctrl held, freely: a
		whole pixel, as before the grid could be a fraction, or the grid
		step when it is finer than that, so that Ctrl never places more
		coarsely than the grid does.
	*/
	inline qreal freeStep(qreal grid)
	{
		return qMin<qreal>(1, grid);
	}
}

#endif // FOLIOGRID_H
