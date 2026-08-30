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
#include "iopoint.h"

#include <QDomDocument>
#include <QDomElement>
#include <QStringList>

namespace
{
		/**
			The two letters that go in the file and come out of the sheet.
			Kept here and not taken from ElementData::plcIOTypeToString because
			that one writes the name of the enumerator, which is French and is
			the program's business, while this one writes what the automation
			department has always written in its own column.
		*/
	struct IoTypeCode
	{
		const char *code;
		ElementData::PlcIOType type;
	};

	const IoTypeCode io_type_codes[] = {
		{"di", ElementData::EntreeDigitale},
		{"do", ElementData::SortieDigitale},
		{"ai", ElementData::EntreeAnalogique},
		{"ao", ElementData::SortieAnalogique},
		{"ui", ElementData::EntreeUniverselle},
		{"uo", ElementData::SortieUniverselle},
			//The same six as a Brazilian hand writes them, entrada and saida
			//first, which is why the letters are the other way round.
		{"ed", ElementData::EntreeDigitale},
		{"sd", ElementData::SortieDigitale},
		{"ea", ElementData::EntreeAnalogique},
		{"sa", ElementData::SortieAnalogique},
		{"eu", ElementData::EntreeUniverselle},
		{"su", ElementData::SortieUniverselle}
	};

	bool isInputWord(const QString &word)
	{
		return word == QLatin1String("entrada")
			|| word == QLatin1String("entradas")
			|| word == QLatin1String("entree")
			|| word == QLatin1String("entrees")
			|| word == QLatin1String("input")
			|| word == QLatin1String("inputs")
			|| word == QLatin1String("in");
	}

	bool isOutputWord(const QString &word)
	{
		return word == QLatin1String("saida")
			|| word == QLatin1String("saidas")
			|| word == QLatin1String("sortie")
			|| word == QLatin1String("sorties")
			|| word == QLatin1String("output")
			|| word == QLatin1String("outputs")
			|| word == QLatin1String("out");
	}
}

/**
	@brief IoPoint::IoPoint
	A digital input, because that is what most of a panel is.
*/
IoPoint::IoPoint()
{}

/**
	@brief IoPoint::IoPoint
	@param description
*/
IoPoint::IoPoint(const QString &description) :
	description(description)
{}

/**
	@brief IoPoint::isNull
	@return true when the point says nothing about itself
	A line of a sheet that has neither a tag, nor an address, nor a
	description is a blank line, and the import drops it instead of
	turning it into a point nobody can recognise.
*/
bool IoPoint::isNull() const
{
	return tag.isEmpty() && address.isEmpty() && description.isEmpty();
}

/**
	@brief IoPoint::isAssigned
	@return true when this point is already in a card
*/
bool IoPoint::isAssigned() const
{
	return !master_uuid.isEmpty() && io_index >= 0;
}

/**
	@brief IoPoint::toXml
	@param document
	@return the point as one element, empty fields left out
	Attributes rather than child elements, and only the ones that hold
	something: a project with ninety-six points writes ninety-six lines
	this way instead of a page each.
*/
QDomElement IoPoint::toXml(QDomDocument &document) const
{
	QDomElement element = document.createElement(tagName());

	if (!id.isEmpty()) {
		element.setAttribute(QStringLiteral("id"), id);
	}
	element.setAttribute(QStringLiteral("type"), typeToString(type));
	if (!tag.isEmpty()) {
		element.setAttribute(QStringLiteral("tag"), tag);
	}
	if (!description.isEmpty()) {
		element.setAttribute(QStringLiteral("description"), description);
	}
	if (!address.isEmpty()) {
		element.setAttribute(QStringLiteral("address"), address);
	}
	if (!card.isEmpty()) {
		element.setAttribute(QStringLiteral("card"), card);
	}
	if (!connect_to.isEmpty()) {
		element.setAttribute(QStringLiteral("connect"), connect_to);
	}
	if (needs_terminal) {
		element.setAttribute(QStringLiteral("terminal"), QStringLiteral("true"));
	}
	if (!comment.isEmpty()) {
		element.setAttribute(QStringLiteral("comment"), comment);
	}

		//The assignment travels together or not at all: a master without an
		//index would come back as a point that believes it is in a card and
		//cannot say where.
	if (isAssigned())
	{
		element.setAttribute(QStringLiteral("master"), master_uuid);
		element.setAttribute(QStringLiteral("io"), QString::number(io_index));
		if (!channel.isEmpty()) {
			element.setAttribute(QStringLiteral("channel"), channel);
		}
	}

	return element;
}

/**
	@brief IoPoint::fromXml
	@param element
	@return true when the element was one of ours
*/
bool IoPoint::fromXml(const QDomElement &element)
{
	if (element.isNull() || element.tagName() != tagName()) {
		return false;
	}

	id          = element.attribute(QStringLiteral("id"));
	type        = typeFromString(element.attribute(QStringLiteral("type")));
	tag         = element.attribute(QStringLiteral("tag"));
	description = element.attribute(QStringLiteral("description"));
	address     = element.attribute(QStringLiteral("address"));
	card        = element.attribute(QStringLiteral("card"));
	connect_to  = element.attribute(QStringLiteral("connect"));
	needs_terminal = element.attribute(QStringLiteral("terminal"))
			 == QLatin1String("true");
	comment     = element.attribute(QStringLiteral("comment"));

	master_uuid = element.attribute(QStringLiteral("master"));
	io_index    = element.attribute(QStringLiteral("io"),
					QStringLiteral("-1")).toInt();
	channel     = element.attribute(QStringLiteral("channel"));
	if (master_uuid.isEmpty()) {
		io_index = -1;
		channel.clear();
	}

	return true;
}

/**
	@brief IoPoint::tagName
	@return the name of the element that holds one point
*/
QString IoPoint::tagName()
{
	return QStringLiteral("io_point");
}

bool IoPoint::operator==(const IoPoint &other) const
{
	return id == other.id
		&& type == other.type
		&& tag == other.tag
		&& description == other.description
		&& address == other.address
		&& card == other.card
		&& connect_to == other.connect_to
		&& needs_terminal == other.needs_terminal
		&& comment == other.comment
		&& master_uuid == other.master_uuid
		&& io_index == other.io_index
		&& channel == other.channel;
}

bool IoPoint::operator!=(const IoPoint &other) const
{
	return !(*this == other);
}

/**
	@brief IoPoint::typeToString
	@param type
	@return the two letters the sheet writes
*/
QString IoPoint::typeToString(ElementData::PlcIOType type)
{
	switch (type)
	{
		case ElementData::SortieDigitale:    return QStringLiteral("DO");
		case ElementData::EntreeAnalogique:  return QStringLiteral("AI");
		case ElementData::SortieAnalogique:  return QStringLiteral("AO");
		case ElementData::EntreeUniverselle: return QStringLiteral("UI");
		case ElementData::SortieUniverselle: return QStringLiteral("UO");
		case ElementData::EntreeDigitale:    break;
	}
	return QStringLiteral("DI");
}

/**
	@brief IoPoint::typeFromString
	@param text what the cell holds
	@param ok set to false when nothing matched
	@return the type, EntreeDigitale when nothing matched
*/
ElementData::PlcIOType IoPoint::typeFromString(const QString &text, bool *ok)
{
	if (ok) {
		*ok = true;
	}

	const QString key = normalize(text);
	if (key.isEmpty())
	{
		if (ok) {
			*ok = false;
		}
		return ElementData::EntreeDigitale;
	}

	for (const IoTypeCode &candidate : io_type_codes)
	{
		if (key == QLatin1String(candidate.code)) {
			return candidate.type;
		}
	}

		//Spelled out. Two independent questions - which way it points and
		//what kind of signal it is - because the sheets that spell it out do
		//so in three languages and in either order.
	bool is_output = false;
	bool direction_found = false;
	bool analogue = false;
	bool universal = false;
	bool kind_found = false;

	const QStringList words = key.split(QLatin1Char(' '));
	for (const QString &word : words)
	{
		if (isInputWord(word))
		{
			is_output = false;
			direction_found = true;
		}
		else if (isOutputWord(word))
		{
			is_output = true;
			direction_found = true;
		}

		if (word.startsWith(QLatin1String("analog")))
		{
			analogue = true;
			universal = false;
			kind_found = true;
		}
		else if (word.startsWith(QLatin1String("digital"))
			 || word.startsWith(QLatin1String("discret"))
			 || word.startsWith(QLatin1String("binari"))
			 || word.startsWith(QLatin1String("bool")))
		{
			analogue = false;
			universal = false;
			kind_found = true;
		}
		else if (word.startsWith(QLatin1String("univers"))
			 || word.startsWith(QLatin1String("mist"))
			 || word.startsWith(QLatin1String("mix"))
			 || word.startsWith(QLatin1String("configurav")))
		{
			universal = true;
			kind_found = true;
		}
	}

	if (!direction_found || !kind_found)
	{
		if (ok) {
			*ok = false;
		}
		return ElementData::EntreeDigitale;
	}

	if (universal) {
		return is_output ? ElementData::SortieUniverselle
				 : ElementData::EntreeUniverselle;
	}
	if (analogue) {
		return is_output ? ElementData::SortieAnalogique
				 : ElementData::EntreeAnalogique;
	}
	return is_output ? ElementData::SortieDigitale
			 : ElementData::EntreeDigitale;
}

/**
	@brief IoPoint::normalize
	@param text
	@return the text folded down to what two people typing the same thing share

	No case, no accent, no double space. The decomposition is what removes
	the accent without a table: normalized form D splits an accented letter
	into the letter and the mark, and the marks are then dropped. It behaves
	the same on Qt 5 and Qt 6, which a locale-aware fold would not.
*/
QString IoPoint::normalize(const QString &text)
{
	const QString decomposed = text.normalized(QString::NormalizationForm_D);

	QString folded;
	folded.reserve(decomposed.size());
	for (const QChar &character : decomposed)
	{
		if (character.category() == QChar::Mark_NonSpacing) {
			continue;
		}
		folded.append(character);
	}

	return folded.simplified().toLower();
}
