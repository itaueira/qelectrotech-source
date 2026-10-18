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
#include "drctitleblock.h"

#include "../diagramcontext.h"
#include "../qetinformation.h"

#include <QCoreApplication>
#include <QSet>

/*
	No diagram.h and no bordertitleblock.h here: this file is handed the
	context a title block is drawn from, never the folio it belongs to.
	That is what lets it be proved in the pure suite, which does not link
	the program.
*/

/**
	@brief DrcTitleBlock::factoryRequiredKeys
	@return the fields the factory profile asks every folio to fill in
*/
QStringList DrcTitleBlock::factoryRequiredKeys()
{
	return {QStringLiteral("title"), QStringLiteral("author")};
}

/**
	@brief DrcTitleBlock::isBlank
	@param value : the value of one field, as the context holds it
	@return whether it prints as nothing

	QString::trimmed() takes off every character QChar::isSpace() answers
	for, the no-break space included - which is the one a value pasted from
	a spreadsheet brings along, and the one a plain test for ' ' would miss.
*/
bool DrcTitleBlock::isBlank(const QVariant &value)
{
	return value.toString().trimmed().isEmpty();
}

/**
	@brief DrcTitleBlock::emptyKeys
	@param context : what the title block of one folio is drawn from
	@param required : the fields that must be filled in
	@return the blank ones, in the order of @a required, each one once
*/
QStringList DrcTitleBlock::emptyKeys(const DiagramContext &context,
				     const QStringList &required)
{
	QStringList empty;
	QSet<QString> seen;
	for (const QString &key : required)
	{
		if (key.isEmpty() || seen.contains(key)) {
			continue;
		}
		seen.insert(key);

			//A key the context does not hold reads as an invalid
			//QVariant, whose text is empty: absent and blank print the
			//same, so they are the same here.
		if (isBlank(context.value(key))) {
			empty << key;
		}
	}
	return empty;
}

/**
	@brief DrcTitleBlock::fieldName
	@param key : the key of a field of the title block
	@return the name the sentence calls it by
*/
QString DrcTitleBlock::fieldName(const QString &key)
{
	const QString name = QETInformation::translatedInfoKey(key);
	return name.isEmpty() ? key : name;
}

/**
	@brief DrcTitleBlock::emptyFieldsText
	@param empty_keys : the blank fields of one folio
	@return the sentence of the finding, or an empty string when there is
	nothing to report

	Two sentences and not one with %n: the source language is the one
	shown when no translation is installed, and a numerus form written for
	one field and for many would print "1 empty fields" there.
*/
QString DrcTitleBlock::emptyFieldsText(const QStringList &empty_keys)
{
	if (empty_keys.isEmpty()) {
		return QString();
	}

	QStringList names;
	for (const QString &key : empty_keys) {
		names << fieldName(key);
	}

		//The names are joined before they go in, and go in through a
		//single arg(): a field of the user's own is free text, and one
		//holding a "%1" must not be rewritten by a later substitution.
		//Not a translated joiner: ", " carries no word.
	const QString list = names.join(QStringLiteral(", "));
	if (empty_keys.count() == 1)
	{
		return QCoreApplication::translate("DrcTitleBlock",
						   "Empty title block field: %1")
		       .arg(list);
	}
	return QCoreApplication::translate("DrcTitleBlock",
					   "Empty title block fields: %1")
	       .arg(list);
}
