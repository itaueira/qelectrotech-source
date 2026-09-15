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
#include "projectrevision.h"

namespace
{
	/*
		The same format the title block already writes its date in
		(TitleBlockProperties::exportDate). This data ends up in the same
		place, and two date formats in one project file is one too many.
	*/
	const QString DATE_FORMAT = QStringLiteral("yyyyMMdd");

	const QString ATTRIBUTE_ID = QStringLiteral("id");
	const QString ATTRIBUTE_STATE = QStringLiteral("state");
	const QString ATTRIBUTE_DATE = QStringLiteral("date");
	const QString ATTRIBUTE_NAME = QStringLiteral("name");

	const QString TAG_INFORMATIONS = QStringLiteral("informations");
	const QString TAG_INFORMATION = QStringLiteral("information");

	const QString INFO_APPROVED_BY = QStringLiteral("approved_by");
	const QString INFO_DESCRIPTION = QStringLiteral("description");

	const QString STATE_OPEN = QStringLiteral("open");
	const QString STATE_CLOSED = QStringLiteral("closed");

	/*
		Prefixed on purpose: toSettings is handed the prefix of whoever
		calls it, and these five keys have to stay together and stay
		recognisable inside somebody else group of settings.
	*/
	const QString SETTING_ID = QStringLiteral("revision_id");
	const QString SETTING_STATE = QStringLiteral("revision_state");
	const QString SETTING_DATE = QStringLiteral("revision_date");
	const QString SETTING_APPROVED_BY = QStringLiteral("revision_approved_by");
	const QString SETTING_DESCRIPTION = QStringLiteral("revision_description");
}

/**
	@brief ProjectRevision::ProjectRevision
	A new project is born in revision A, open, with nothing filled in and
	no date.
*/
ProjectRevision::ProjectRevision()
{}

/**
	@brief ProjectRevision::ProjectRevision
	@param identifier : the letter of this revision. An empty one falls
	back to the first, because a revision with no name could not be written
	to the project file and found again.
*/
ProjectRevision::ProjectRevision(const QString &identifier)
{
	const QString trimmed = identifier.trimmed();
	if (!trimmed.isEmpty()) {
		m_identifier = trimmed;
	}
}

/**
	@brief ProjectRevision::nextIdentifier
	@param identifier : the revision in force
	@return the revision that follows it: A becomes B, Z becomes AA, AZ
	becomes BA.

	The wrap is what this function exists for. A project that reaches Z has
	to keep going, and the alternative - a number that overflows into a
	character nobody can read, or a second Z colliding with the first - is
	only ever found by the one project that got there.

	Anything that is not a run of letters falls back to the first
	identifier, which is the only answer that cannot make two revisions
	share a name.
*/
QString ProjectRevision::nextIdentifier(const QString &identifier)
{
	const QString current = identifier.trimmed().toUpper();
	if (current.isEmpty()) {
		return firstIdentifier();
	}

	for (const QChar &character : current)
	{
			//Compared as plain characters and not as QChar, so that the
			//answer does not depend on which Qt is compiling this
		const char latin = character.toLatin1();
		if (latin < 'A' || latin > 'Z') {
			return firstIdentifier();
		}
	}

	QString next = current;
	for (int index = next.length() - 1 ; index >= 0 ; --index)
	{
		const char latin = next.at(index).toLatin1();
		if (latin != 'Z')
		{
			next[index] = QChar::fromLatin1(char(latin + 1));
			return next;
		}
		next[index] = QChar::fromLatin1('A');
	}

		//Every letter was a Z, so the run grew by one: ZZ becomes AAA
	next.prepend(QChar::fromLatin1('A'));
	return next;
}

/**
	@brief ProjectRevision::setIdentifier
	@return true if the change was applied
*/
bool ProjectRevision::setIdentifier(const QString &identifier)
{
	const QString trimmed = identifier.trimmed();
	if (m_closed || trimmed.isEmpty()) {
		return false;
	}

	m_identifier = trimmed;
	return true;
}

/**
	@brief ProjectRevision::setApprovedBy
	@return true if the change was applied
*/
bool ProjectRevision::setApprovedBy(const QString &approved_by)
{
	if (m_closed) {
		return false;
	}

	m_approved_by = approved_by;
	return true;
}

/**
	@brief ProjectRevision::setDescription
	@return true if the change was applied
*/
bool ProjectRevision::setDescription(const QString &description)
{
	if (m_closed) {
		return false;
	}

	m_description = description;
	return true;
}

/**
	@brief ProjectRevision::canBeClosed
	@return whether this revision has what closing demands: a name, somebody
	who approved it, and what it was emitted for.

	Measured on the trimmed text and stored untrimmed: a field holding one
	space is nobody, and accepting it would put an empty line in the title
	block of a drawing that has been emitted.
*/
bool ProjectRevision::canBeClosed() const
{
	return !m_closed
		&& isValid()
		&& !m_approved_by.trimmed().isEmpty()
		&& !m_description.trimmed().isEmpty();
}

/**
	@brief ProjectRevision::close
	@param closing_date : the day this revision was emitted
	@return true if the revision was closed.

	An invalid date is refused along with the rest: a closed revision
	without a readable date is what the title block prints as rubbish, and
	it is the one field nobody can fill in afterwards, since a closed
	revision changes no more.
*/
bool ProjectRevision::close(const QDate &closing_date)
{
	if (!canBeClosed() || !closing_date.isValid()) {
		return false;
	}

	m_date = closing_date;
	m_closed = true;
	return true;
}

/**
	@brief ProjectRevision::toXml
	@param xml_document
	@return the element that carries this revision.

	Strict: the identifier and the state are always written, and the date
	only when there is one. An open revision leaves the attribute out
	altogether rather than writing an empty one, so that reading it back
	cannot turn the absence of a date into a date of some kind.
*/
QDomElement ProjectRevision::toXml(QDomDocument &xml_document) const
{
	QDomElement root_elmt = xml_document.createElement(xmlTagName());

	root_elmt.setAttribute(ATTRIBUTE_ID, m_identifier);
	root_elmt.setAttribute(ATTRIBUTE_STATE, m_closed ? STATE_CLOSED : STATE_OPEN);

	const QString date_string = dateToString(m_date);
	if (!date_string.isEmpty()) {
		root_elmt.setAttribute(ATTRIBUTE_DATE, date_string);
	}

	QDomElement info_elmt = xml_document.createElement(TAG_INFORMATIONS);
	root_elmt.appendChild(info_elmt);

	if (!m_approved_by.isEmpty()) {
		info_elmt.appendChild(infoToXml(xml_document, INFO_APPROVED_BY, m_approved_by));
	}
	if (!m_description.isEmpty()) {
		info_elmt.appendChild(infoToXml(xml_document, INFO_DESCRIPTION, m_description));
	}

	return root_elmt;
}

/**
	@brief ProjectRevision::fromXml
	@param xml_element
	@return true when a revision was read, false when there was none to
	read - see the note in the header, because false is not a failure.
*/
bool ProjectRevision::fromXml(const QDomElement &xml_element)
{
	if (xml_element.isNull() || xml_element.tagName() != xmlTagName()) {
		return false;
	}

	const QString identifier = xml_element.attribute(ATTRIBUTE_ID).trimmed();
	m_identifier = identifier.isEmpty() ? firstIdentifier() : identifier;

		//A date that does not parse comes back invalid, never today
	m_date = dateFromString(xml_element.attribute(ATTRIBUTE_DATE));

	const QString state = xml_element.attribute(ATTRIBUTE_STATE);
	if (state == STATE_CLOSED) {
		m_closed = true;
	} else if (state == STATE_OPEN) {
		m_closed = false;
	} else {
			//Nothing readable was written: the date is the only thing
			//that can have been stamped, so it is what answers
		m_closed = m_date.isValid();
	}

		//Cleared before reading, so that a revision read into an object
		//that already held one does not keep the fields the new element
		//happens not to carry
	m_approved_by.clear();
	m_description.clear();

	const QDomElement informations = xml_element.firstChildElement(TAG_INFORMATIONS);
	for (QDomElement info = informations.firstChildElement(TAG_INFORMATION) ;
	     !info.isNull() ;
	     info = info.nextSiblingElement(TAG_INFORMATION))
	{
		const QString name = info.attribute(ATTRIBUTE_NAME);
		if (name == INFO_APPROVED_BY) {
			m_approved_by = info.text();
		} else if (name == INFO_DESCRIPTION) {
			m_description = info.text();
		}
	}

	return true;
}

/**
	@brief ProjectRevision::toSettings
	@param settings
	@param prefix
*/
void ProjectRevision::toSettings(QSettings &settings, const QString prefix) const
{
	settings.setValue(prefix + SETTING_ID, m_identifier);
	settings.setValue(prefix + SETTING_STATE, m_closed ? STATE_CLOSED : STATE_OPEN);
	settings.setValue(prefix + SETTING_DATE, dateToString(m_date));
	settings.setValue(prefix + SETTING_APPROVED_BY, m_approved_by);
	settings.setValue(prefix + SETTING_DESCRIPTION, m_description);
}

/**
	@brief ProjectRevision::fromSettings
	@param settings
	@param prefix
	Tolerant in the same way, and for the same reasons, as fromXml.
*/
void ProjectRevision::fromSettings(const QSettings &settings, const QString prefix)
{
	const QString identifier = settings.value(prefix + SETTING_ID).toString().trimmed();
	m_identifier = identifier.isEmpty() ? firstIdentifier() : identifier;

	m_date = dateFromString(settings.value(prefix + SETTING_DATE).toString());

	const QString state = settings.value(prefix + SETTING_STATE).toString();
	if (state == STATE_CLOSED) {
		m_closed = true;
	} else if (state == STATE_OPEN) {
		m_closed = false;
	} else {
		m_closed = m_date.isValid();
	}

	m_approved_by = settings.value(prefix + SETTING_APPROVED_BY).toString();
	m_description = settings.value(prefix + SETTING_DESCRIPTION).toString();
}

bool ProjectRevision::operator==(const ProjectRevision &other) const
{
	return m_identifier == other.m_identifier
		&& m_closed == other.m_closed
		&& m_approved_by == other.m_approved_by
		&& m_description == other.m_description
		&& m_date == other.m_date;
}

bool ProjectRevision::operator!=(const ProjectRevision &other) const
{
	return !(*this == other);
}

/**
	@brief ProjectRevision::infoToXml
	One free text field, written as a text node rather than an attribute:
	the same shape TerminalStripData already uses for its own, and the one
	that does not have to be thought about again when a description arrives
	with a line break inside it.
*/
QDomElement ProjectRevision::infoToXml(QDomDocument &xml_document,
				       const QString &name,
				       const QString &value)
{
	QDomElement xml_elmt = xml_document.createElement(TAG_INFORMATION);
	xml_elmt.setAttribute(ATTRIBUTE_NAME, name);
	xml_elmt.appendChild(xml_document.createTextNode(value));

	return xml_elmt;
}

QString ProjectRevision::dateToString(const QDate &date)
{
	return date.isValid() ? date.toString(DATE_FORMAT) : QString();
}

QDate ProjectRevision::dateFromString(const QString &string)
{
	if (string.trimmed().isEmpty()) {
		return QDate();
	}

		//An unreadable date comes back invalid, which is exactly what is
		//wanted here: QDate::fromString says so by itself
	return QDate::fromString(string.trimmed(), DATE_FORMAT);
}
