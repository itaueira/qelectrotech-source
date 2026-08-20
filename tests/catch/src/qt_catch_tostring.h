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
#ifndef QT_CATCH_TOSTRING_H
#define QT_CATCH_TOSTRING_H

#include <catch2/catch.hpp>

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <string>

/*
	Teach Catch2 to print the Qt string types.

	Without this, Catch2 sees a QString as a range of QChar it cannot print
	and a failed comparison reads

	    { {?} } == { {?}, {?}, {?} }

	which says that something differs and nothing else. Include this header
	instead of catch2/catch.hpp in a test that compares Qt strings.
*/
namespace Catch
{
	template<>
	struct StringMaker<QString>
	{
		static std::string convert(const QString &value)
		{
			return '"' + std::string(value.toUtf8().constData()) + '"';
		}
	};

	template<>
	struct StringMaker<QStringList>
	{
		static std::string convert(const QStringList &value)
		{
			std::string out("{");
			for (int index = 0 ; index < value.size() ; ++index)
			{
				if (index > 0) {
					out += ", ";
				}
				out += StringMaker<QString>::convert(value.at(index));
			}
			out += "}";
			return out;
		}
	};

	template<>
	struct StringMaker<QByteArray>
	{
		static std::string convert(const QByteArray &value)
		{
			return '"' + std::string(value.constData(), value.size()) + '"';
		}
	};
}

#endif // QT_CATCH_TOSTRING_H
