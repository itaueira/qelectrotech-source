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
#include "wireendformat.h"

#include <QRegularExpression>

QString WireEndFormat::format(const QString &pattern, const WireEnd &end)
{
		//The longest names first in the alternation: %position must not be
		//read as %point followed by "sition"
	static const QRegularExpression variable_{
		QStringLiteral("%(description|position|folio|point|wire|tag)")};
	static const QRegularExpression empty_brackets_{QStringLiteral("\\(\\s*\\)")};

	QString text_;
	qsizetype last_ = 0;
	auto it_ = variable_.globalMatch(pattern);
	while (it_.hasNext())
	{
		const auto match_ = it_.next();
		text_ += pattern.mid(last_, match_.capturedStart() - last_);
		last_ = match_.capturedEnd();

		const QString name_ = match_.captured(1);
		const QString value_ = name_ == QLatin1String("description") ? end.description
							 : name_ == QLatin1String("position")    ? end.position
							 : name_ == QLatin1String("folio")       ? end.folio
							 : name_ == QLatin1String("point")       ? end.point
							 : name_ == QLatin1String("wire")        ? end.wire
							 :                                         end.tag;
		if (value_.isEmpty())
		{
				//"K1:" for a point with no name: the separator goes with it
			if (text_.endsWith(QLatin1Char(':')) || text_.endsWith(QLatin1Char('-'))
				|| text_.endsWith(QLatin1Char('/'))) {
				text_.chop(1);
			}
			continue;
		}
		text_ += value_;
	}
	text_ += pattern.mid(last_);
	text_.remove(empty_brackets_);
	return text_.simplified();
}
