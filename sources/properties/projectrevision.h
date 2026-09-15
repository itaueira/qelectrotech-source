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
#ifndef PROJECTREVISION_H
#define PROJECTREVISION_H

#include "propertiesinterface.h"

#include <QDate>
#include <QString>

/**
	@brief The revision a project is in, and whether it is still open.

	A project is always inside a revision: it is born in revision A, open,
	and an open revision is what may be edited. Closing one is the act that
	makes it final - from then on neither the drawing nor the fields of the
	revision itself may change, and the way forward is to open the next
	revision, never to reopen this one. That is why there is no reopen()
	here: the value it would restore is the one thing two copies of a
	closed revision are supposed to agree on.

	Four fields are typed in and one is not. The identifier (A, B, C) is
	machine text - never translated, never guessed - and the two free text
	fields are what closing demands: who approved the revision, and what it
	was emitted for. The date is the one nobody types: it is stamped by
	close(), and it stays null for as long as the revision is open. An open
	revision carrying a date would be a revision the title block prints as
	if it had been emitted.

	This is property data, not an item of the scene, which is why it
	implements PropertiesInterface. The revision cloud that marks what
	changed is the other half of the same feature and is a QGraphicsItem;
	it serialises through the contract of the graphics items, not through
	this one.

	@see PropertiesInterface, TitleBlockProperties
*/
class ProjectRevision : public PropertiesInterface
{
	public:
		ProjectRevision();
		explicit ProjectRevision(const QString &identifier);

		void toSettings(QSettings &settings,
				const QString prefix = QString()) const override;
		void fromSettings(const QSettings &settings,
				  const QString prefix = QString()) override;
		QDomElement toXml(QDomDocument &xml_document) const override;

		/**
			@brief fromXml
			Read a revision out of its own element - the element, not its
			parent.

			Returning false is not an error: it says that nothing was read,
			and that this object was left exactly as it was. That is the
			whole point for a project file written before this feature
			existed - there is no revision element to find, the read says
			so, and the object is still the valid revision A, open, that it
			was born as. A caller that treats false as a failure to open
			the project has misread the contract.

			Reading is tolerant and writing is strict: a missing identifier
			falls back to A, a date that does not parse is no date at all,
			and an unreadable state is resolved by the date - a revision
			carrying one has been closed, since that is the only thing that
			stamps it. Erring towards closed is deliberate: reading a
			closed revision as open would let somebody edit what has
			already been emitted, which is the failure this whole feature
			exists to prevent.
		*/
		bool fromXml(const QDomElement &xml_element) override;

		/// The tag this class writes and the only one it reads.
		static QString xmlTagName() {return QStringLiteral("project_revision");}
		/// The revision a new project is born in.
		static QString firstIdentifier() {return QStringLiteral("A");}
		static QString nextIdentifier(const QString &identifier);

		QString identifier() const {return m_identifier;}
		QString approvedBy() const {return m_approved_by;}
		QString description() const {return m_description;}
		/// Null for as long as the revision is open. Stamped by close().
		QDate date() const {return m_date;}

		bool isClosed() const {return m_closed;}
		bool isOpen() const {return !m_closed;}

		/*
			The three setters answer whether the change was applied, and a
			closed revision applies none of them. A void setter that
			quietly did nothing would be the same defect read twice: the
			caller believes it wrote, and the field it thinks it changed
			is the one somebody has already signed.
		*/
		bool setIdentifier(const QString &identifier);
		bool setApprovedBy(const QString &approved_by);
		bool setDescription(const QString &description);

		bool canBeClosed() const;
		bool close(const QDate &closing_date);
		/// Close stamping today, which is what the dialogue does.
		bool close() {return close(QDate::currentDate());}

		/// A revision with no identifier has no name, so it is not a revision.
		bool isValid() const {return !m_identifier.isEmpty();}

		bool operator==(const ProjectRevision &other) const;
		bool operator!=(const ProjectRevision &other) const;

	private:
		static QDomElement infoToXml(QDomDocument &xml_document,
					     const QString &name,
					     const QString &value);
		static QString dateToString(const QDate &date);
		static QDate dateFromString(const QString &string);

		QString m_identifier = ProjectRevision::firstIdentifier();
		bool m_closed = false;
		QString m_approved_by;
		QString m_description;
		QDate m_date;
};

#endif // PROJECTREVISION_H
