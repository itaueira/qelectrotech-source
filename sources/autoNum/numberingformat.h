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
#ifndef NUMBERINGFORMAT_H
#define NUMBERINGFORMAT_H

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief Where a numbering format restarts its counter.
	This is the difference between M1, M2, M3 across a whole project and
	M201, M202 on folio 2, and it is the choice the office makes - not the
	program.
*/
enum class NumberingScope
{
	Project,   ///< one counter for the whole project
	Folio,     ///< the counter restarts on each folio
	Rung,      ///< numbered by the line of the schematic the symbol sits on
	Location   ///< the counter restarts in each location
};

/**
	@brief The NumberingFormat class
	A named numbering format, created by the user and attached to a class.

	> **Decision of the specification, made concrete here:** numbering is not
	> a command, it is a property of the object, and the rule of how to number
	> lives on the **class** - not in the command. So the format is a named
	> object: one that includes the folio number, one that uses the line
	> number, one plain sequential, and the choice belongs to the house
	> standard.

	The pattern is filled from three sources, and nothing else, so that a
	format is readable by whoever writes it:

	| Token | Comes from |
	|---|---|
	| `%{root}` | the tag root of the class, house or IEC (T12) |
	| `%{n}` | the counter, padded to `digits` |
	| `%{folio}` `%{rung}` `%{location}` | the context of the object |
*/
class NumberingFormat
{
	public:
		NumberingFormat();
		NumberingFormat(const QString &name, const QString &pattern);

		bool isNull() const;
		bool isValid(QString *error = nullptr) const;

		/**
			@param root : the tag root of the class
			@param counter : which object this is, within the scope
			@param context : folio, rung and location, already resolved
			@return the label to write

			The context is passed already resolved on purpose: working out
			which line of the schematic a symbol sits on needs the folio and
			its coordinate system, and keeping that out of here is what makes
			the format testable without a project.
		*/
		QString render(const QString &root,
			       int counter,
			       const QHash<QString, QString> &context = QHash<QString, QString>()) const;

		QString toXml() const;
		static NumberingFormat fromXml(const QString &xml);

		/// The four formats the specification lists, ready to use
		static QList<NumberingFormat> builtinFormats();
		static QString scopeToString(NumberingScope scope);
		static NumberingScope scopeFromString(const QString &string);
		static QString translatedScopeName(NumberingScope scope);
		/// The tokens a pattern may use, for the interface to offer
		static QStringList tokens();

	public:
		QString name;
		QString pattern = QStringLiteral("%{root}%{n}");
		int start = 1;
		int step = 1;
		/// Zero padding of %{n}; 0 means none, so M1 and not M01
		int digits = 0;
		NumberingScope scope = NumberingScope::Project;
};

#endif // NUMBERINGFORMAT_H
