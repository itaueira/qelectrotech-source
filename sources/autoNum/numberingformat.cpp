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
#include "numberingformat.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QDomElement>

/**
	@brief NumberingFormat::NumberingFormat
*/
NumberingFormat::NumberingFormat()
{}

/**
	@brief NumberingFormat::NumberingFormat
	@param name
	@param pattern
*/
NumberingFormat::NumberingFormat(const QString &name, const QString &pattern) :
	name(name),
	pattern(pattern)
{}

/**
	@brief NumberingFormat::isNull
	@return true when this format carries nothing
*/
bool NumberingFormat::isNull() const
{
	return name.isEmpty() && pattern.isEmpty();
}

/**
	@brief NumberingFormat::isValid
	@param error
	@return true when this format can number something
*/
bool NumberingFormat::isValid(QString *error) const
{
	if (name.trimmed().isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("NumberingFormat",
							     "Le format doit avoir un nom.");
		}
		return false;
	}
	if (pattern.trimmed().isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("NumberingFormat",
							     "Le format doit avoir un motif.");
		}
		return false;
	}
	if (!pattern.contains(QStringLiteral("%{n}")))
	{
		// Without the counter, every object of the class gets the same label,
		// which is the one thing numbering must never produce.
		if (error) {
			*error = QCoreApplication::translate("NumberingFormat",
							     "Le motif doit contenir %{n}, sinon tous les objets "
							     "reçoivent le même repère.");
		}
		return false;
	}
	if (step == 0)
	{
		if (error) {
			*error = QCoreApplication::translate("NumberingFormat",
							     "Le pas ne peut pas être nul.");
		}
		return false;
	}
	return true;
}

/**
	@brief NumberingFormat::tokens
	@return the tokens a pattern may use
*/
QStringList NumberingFormat::tokens()
{
	return { QStringLiteral("%{root}"),
		 QStringLiteral("%{n}"),
		 QStringLiteral("%{folio}"),
		 QStringLiteral("%{rung}"),
		 QStringLiteral("%{location}") };
}

/**
	@brief NumberingFormat::render
	@param root
	@param counter
	@param context
	@return the label to write
*/
QString NumberingFormat::render(const QString &root,
				int counter,
				const QHash<QString, QString> &context) const
{
	const int value = start + (counter * step);
	QString number = QString::number(qAbs(value));
	if (digits > 1) {
		number = number.rightJustified(digits, QLatin1Char('0'));
	}
	if (value < 0) {
		number.prepend(QLatin1Char('-'));
	}

	QString label = pattern;
	label.replace(QStringLiteral("%{root}"), root);
	label.replace(QStringLiteral("%{n}"), number);
	label.replace(QStringLiteral("%{folio}"), context.value(QStringLiteral("folio")));
	label.replace(QStringLiteral("%{rung}"), context.value(QStringLiteral("rung")));
	label.replace(QStringLiteral("%{location}"), context.value(QStringLiteral("location")));

	// A token nobody filled leaves nothing behind rather than the token
	// itself: a label reading "M%{rung}1" on a folio with no line numbering
	// is worse than "M1".
	const QStringList all = tokens();
	for (const QString &token : all) {
		label.remove(token);
	}

	return label;
}

/**
	@brief NumberingFormat::builtinFormats
	@return the four formats the specification lists
*/
QList<NumberingFormat> NumberingFormat::builtinFormats()
{
	QList<NumberingFormat> formats;

	NumberingFormat sequential(
		QCoreApplication::translate("NumberingFormat", "Séquentiel"),
		QStringLiteral("%{root}%{n}"));
	sequential.scope = NumberingScope::Project;
	formats.append(sequential);

	NumberingFormat with_folio(
		QCoreApplication::translate("NumberingFormat", "Avec le numéro de folio"),
		QStringLiteral("%{root}%{folio}%{n}"));
	with_folio.scope = NumberingScope::Folio;
	with_folio.digits = 2;
	formats.append(with_folio);

	NumberingFormat by_rung(
		QCoreApplication::translate("NumberingFormat", "Par ligne"),
		QStringLiteral("%{root}%{rung}"));
	by_rung.scope = NumberingScope::Rung;
	// The pattern of a rung format still needs the counter, for the second
	// symbol on the same line.
	by_rung.pattern = QStringLiteral("%{root}%{rung}%{n}");
	formats.append(by_rung);

	NumberingFormat with_location(
		QCoreApplication::translate("NumberingFormat", "Avec la localisation"),
		QStringLiteral("%{location}%{root}%{n}"));
	with_location.scope = NumberingScope::Location;
	formats.append(with_location);

	return formats;
}

/**
	@brief NumberingFormat::scopeToString
	@param scope
	@return the stable string stored with the format
*/
QString NumberingFormat::scopeToString(NumberingScope scope)
{
	switch (scope)
	{
		case NumberingScope::Project:  return QStringLiteral("project");
		case NumberingScope::Folio:    return QStringLiteral("folio");
		case NumberingScope::Rung:     return QStringLiteral("rung");
		case NumberingScope::Location: return QStringLiteral("location");
	}
	return QStringLiteral("project");
}

/**
	@brief NumberingFormat::scopeFromString
	@param string
	@return the scope @a string names, Project when it names nothing known
*/
NumberingScope NumberingFormat::scopeFromString(const QString &string)
{
	if (string == QStringLiteral("folio")) {
		return NumberingScope::Folio;
	}
	if (string == QStringLiteral("rung")) {
		return NumberingScope::Rung;
	}
	if (string == QStringLiteral("location")) {
		return NumberingScope::Location;
	}
	return NumberingScope::Project;
}

/**
	@brief NumberingFormat::translatedScopeName
	@param scope
	@return the name of @a scope in the user language
*/
QString NumberingFormat::translatedScopeName(NumberingScope scope)
{
	switch (scope)
	{
		case NumberingScope::Project:
			return QCoreApplication::translate("NumberingFormat", "Tout le projet");
		case NumberingScope::Folio:
			return QCoreApplication::translate("NumberingFormat", "Par folio");
		case NumberingScope::Rung:
			return QCoreApplication::translate("NumberingFormat", "Par ligne");
		case NumberingScope::Location:
			return QCoreApplication::translate("NumberingFormat", "Par localisation");
	}
	return QString();
}

/**
	@brief NumberingFormat::toXml
	@return the format as a document
*/
QString NumberingFormat::toXml() const
{
	QDomDocument document;
	QDomElement root = document.createElement(QStringLiteral("numbering-format"));
	root.setAttribute(QStringLiteral("name"), name);
	root.setAttribute(QStringLiteral("pattern"), pattern);
	root.setAttribute(QStringLiteral("start"), QString::number(start));
	root.setAttribute(QStringLiteral("step"), QString::number(step));
	root.setAttribute(QStringLiteral("digits"), QString::number(digits));
	root.setAttribute(QStringLiteral("scope"), scopeToString(scope));
	document.appendChild(root);
	return document.toString();
}

/**
	@brief NumberingFormat::fromXml
	@param xml
	@return the format the document describes
*/
NumberingFormat NumberingFormat::fromXml(const QString &xml)
{
	NumberingFormat format;
	QDomDocument document;
	if (!document.setContent(xml)) {
		return format;
	}

	const QDomElement root = document.documentElement();
	format.name = root.attribute(QStringLiteral("name"));
	format.pattern = root.attribute(QStringLiteral("pattern"));
	format.scope = scopeFromString(root.attribute(QStringLiteral("scope")));

	bool ok = false;
	const int start = root.attribute(QStringLiteral("start")).toInt(&ok);
	if (ok) {
		format.start = start;
	}
	const int step = root.attribute(QStringLiteral("step")).toInt(&ok);
	if (ok && step != 0) {
		format.step = step;
	}
	const int digits = root.attribute(QStringLiteral("digits")).toInt(&ok);
	if (ok) {
		format.digits = digits;
	}

	return format;
}
