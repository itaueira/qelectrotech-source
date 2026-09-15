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
#include "foliovariables.h"

namespace FolioVariables
{

QString resolveAutonum(const QString &folio, const QString &autonum)
{
	QString resolved = folio;
	resolved.replace(QStringLiteral("%autonum"), autonum);
	return resolved;
}

/*
	%id before %total, which is the order the title block has always used
	and which is not interchangeable: a numbering context can hand back a
	string carrying a variable of its own, and the order decides whether
	that string is substituted in turn or printed as it stands. Changing
	the order here would change what a project with a formula in its
	numbering prints, with nothing to say it had changed.
*/
QString resolveIndex(const QString &folio, int index, int total)
{
	QString resolved = folio;
	resolved.replace(QStringLiteral("%id"), QString::number(index));
	resolved.replace(QStringLiteral("%total"), QString::number(total));
	return resolved;
}

}
