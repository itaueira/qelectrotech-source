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
#include "drcstripnumbers.h"

#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <utility>

/*
	No terminalstrip.h and no element.h here: this file is handed numbers,
	places on the rail, folios and the fields of a strip's name, never the
	objects they came from. That is what lets it be proved in the pure
	suite, which does not link the program.
*/

/**
	@brief DrcStripNumbers::key
	@param number : a number as the plan draws it
	@return what two numbers must share to be the same number
*/
QString DrcStripNumbers::key(const QString &number)
{
	return number.trimmed().toCaseFolded();
}

/**
	@brief DrcStripNumbers::groups
	@param marks : the levels of one strip, in the order of its rail
	@return the indexes of the terminals of each number carried by more
	than one terminal
*/
QList<QList<int>> DrcStripNumbers::groups(const QList<Mark> &marks)
{
		//Every number, in the order its first terminal is met, so that the
		//findings come out in the order of the rail and not in the order a
		//hash hands its keys out. Beside each, the terminals already in it:
		//a second level of one of them carrying the same number is the same
		//piece, and is not counted again.
	QList<QList<int>> every_number;
	QList<QSet<int>> terminals_of;
	QHash<QString, int> index_of;

	for (int index = 0 ; index < marks.count() ; ++index)
	{
		const Mark &mark = marks.at(index);
		const QString number_key = key(mark.number);
		if (number_key.isEmpty()) {
			continue;
		}

		const auto known = index_of.constFind(number_key);
		if (known == index_of.constEnd())
		{
			index_of.insert(number_key, int(every_number.count()));
			every_number.append(QList<int>({index}));
			terminals_of.append(QSet<int>({mark.physical}));
			continue;
		}

		const int at = known.value();
		if (terminals_of.at(at).contains(mark.physical)) {
			continue;
		}
		every_number[at].append(index);
		terminals_of[at].insert(mark.physical);
	}

	QList<QList<int>> repeated;
	for (const QList<int> &members : std::as_const(every_number))
	{
		if (members.count() > 1) {
			repeated.append(members);
		}
	}
	return repeated;
}

/**
	@brief DrcStripNumbers::stripName
	@param installation : the installation of the strip
	@param location : its location
	@param name : its own name
	@return the strip as a sentence names it
*/
QString DrcStripNumbers::stripName(const QString &installation,
				   const QString &location,
				   const QString &name)
{
	QStringList parts;
	if (!installation.trimmed().isEmpty()) {
		parts << installation.trimmed();
	}
	if (!location.trimmed().isEmpty()) {
		parts << location.trimmed();
	}

	const QString own = name.trimmed();
	parts << (own.isEmpty()
		  ? QCoreApplication::translate("DrcStripNumbers", "(no name)")
		  : own);

		//A space, as the header of the plan joins them. Not a translated
		//joiner: it carries no word.
	return parts.join(QLatin1Char(' '));
}

/**
	@brief DrcStripNumbers::stripKey
	@param installation : the installation of the strip
	@param location : its location
	@param name : its own name
	@return what two strips must share to have the same name
*/
QString DrcStripNumbers::stripKey(const QString &installation,
				  const QString &location,
				  const QString &name)
{
	const QString own = name.trimmed();
	if (own.isEmpty()) {
		return QString();
	}

		//Field by field, kept apart by a character nobody types: joined by a
		//space, "=A" "+B" and "=A +B" "" would read as one name, and they
		//are two strips somebody filled in differently.
	const QChar apart(0x1F);
	const QString fields = installation.trimmed() + apart
			       + location.trimmed() + apart
			       + own;
	return fields.toCaseFolded();
}

/**
	@brief DrcStripNumbers::sameNameGroups
	@param keys : one stripKey() per strip
	@return the indexes of the strips of each name carried more than once
*/
QList<QList<int>> DrcStripNumbers::sameNameGroups(const QStringList &keys)
{
	QList<QList<int>> every_name;
	QHash<QString, int> index_of;

	for (int index = 0 ; index < keys.count() ; ++index)
	{
		const QString &name_key = keys.at(index);
		if (name_key.isEmpty()) {
			continue;
		}

		const auto known = index_of.constFind(name_key);
		if (known == index_of.constEnd())
		{
			index_of.insert(name_key, int(every_name.count()));
			every_name.append(QList<int>({index}));
		}
		else
		{
			every_name[known.value()].append(index);
		}
	}

	QList<QList<int>> repeated;
	for (const QList<int> &members : std::as_const(every_name))
	{
		if (members.count() > 1) {
			repeated.append(members);
		}
	}
	return repeated;
}

namespace
{
	/// Where one terminal is, in words: "position 3 (folio 2, B4)".
	QString placeOf(const DrcStripNumbers::Mark &mark)
	{
		QString sheet;
		if (mark.folio > 0 && !mark.cell.isEmpty())
		{
			sheet = QCoreApplication::translate("DrcStripNumbers",
							    "folio %1, %2")
				.arg(QString::number(mark.folio), mark.cell);
		}
		else if (mark.folio > 0)
		{
			sheet = QCoreApplication::translate("DrcStripNumbers",
							    "folio %1")
				.arg(QString::number(mark.folio));
		}
		else
		{
			sheet = mark.cell;
		}

		if (mark.physical < 0)
		{
			return sheet.isEmpty()
			       ? QCoreApplication::translate("DrcStripNumbers",
							     "place unknown")
			       : sheet;
		}

		if (sheet.isEmpty())
		{
			return QCoreApplication::translate("DrcStripNumbers",
							   "position %1")
			       .arg(QString::number(mark.physical));
		}
		return QCoreApplication::translate("DrcStripNumbers",
						   "position %1 (%2)")
		       .arg(QString::number(mark.physical), sheet);
	}
}

/**
	@brief DrcStripNumbers::duplicateText
	@param strip_name : the strip, as stripName() says it
	@param group : the marks of one number, one per terminal
	@return the sentence of the finding, or an empty string when there is
	nothing to report
*/
QString DrcStripNumbers::duplicateText(const QString &strip_name,
				       const QList<Mark> &group)
{
	if (group.count() < 2) {
		return QString();
	}

	const QString first_key = key(group.first().number);
	if (first_key.isEmpty()) {
		return QString();
	}

	const QString first_number = group.first().number.trimmed();
	bool spelled_alike = true;
	QSet<int> terminals;
	for (const Mark &mark : group)
	{
		if (key(mark.number) != first_key) {
			return QString();
		}
			//Two levels of one terminal are one piece: a group holding
			//both would report a terminal against itself.
		if (terminals.contains(mark.physical)) {
			return QString();
		}
		terminals.insert(mark.physical);
		if (mark.number.trimmed() != first_number) {
			spelled_alike = false;
		}
	}

		//When the members are not spelled alike - PE and pe - each place
		//says which spelling is there, or the reader sent to position 7 to
		//look for PE finds pe and wonders whether he is in the right place.
	QStringList places;
	for (const Mark &mark : group)
	{
		const QString place = placeOf(mark);
		if (spelled_alike)
		{
			places << place;
		}
		else
		{
				//Both values in one call: a number is text somebody typed,
				//and one holding a "%2" would otherwise be rewritten by the
				//second of two chained arg().
			places << QCoreApplication::translate("DrcStripNumbers",
							      "%1 at %2")
				  .arg(mark.number.trimmed(), place);
		}
	}

		//"; " and not ", ": a place is "position 3 (folio 2, B4)", so the
		//comma is already taken inside it.
	return QCoreApplication::translate("DrcStripNumbers",
					   "Number %1 is on %n terminals of "
					   "strip %2: %3.",
					   nullptr,
					   int(group.count()))
	       .arg(first_number, strip_name,
		    places.join(QStringLiteral("; ")));
}

/**
	@brief DrcStripNumbers::emptyStripText
	@param strip_name : the strip, as stripName() says it
	@return the sentence of the finding
*/
QString DrcStripNumbers::emptyStripText(const QString &strip_name)
{
	if (strip_name.isEmpty()) {
		return QString();
	}
	return QCoreApplication::translate("DrcStripNumbers",
					   "Terminal strip %1 has no terminal "
					   "on any folio.")
	       .arg(strip_name);
}

/**
	@brief DrcStripNumbers::sameNameText
	@param strip_name : the name the strips share, as stripName() says it
	@param count : how many strips carry it
	@return the sentence of the finding
*/
QString DrcStripNumbers::sameNameText(const QString &strip_name, int count)
{
	if (count < 2 || strip_name.isEmpty()) {
		return QString();
	}
	return QCoreApplication::translate("DrcStripNumbers",
					   "%n terminal strips are named %1.",
					   nullptr,
					   count)
	       .arg(strip_name);
}
