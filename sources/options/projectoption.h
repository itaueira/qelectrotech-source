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
#ifndef PROJECTOPTION_H
#define PROJECTOPTION_H

#include <QCoreApplication>
#include <QString>

class QDomDocument;
class QDomElement;

/**
	@brief One configuration of a family of panels, named: what tells a
	one-way conveyor from a bidirectional one.

	A template project holds every configuration a family can be built in,
	and an option is the switch that picks one of them. What an option is
	worth - the difference it makes to the drawing - is not here and is not
	in this file: it belongs to the project, beside the tree, so that a
	project opened by a program that knows nothing of options still draws
	its base state.

	Two fields carry the identity, and they are not the same field:

	- uuid is what everything else points at. It is given when the option
	  enters a tree and never afterwards, so that renaming an option is one
	  operation and not a search for everything that was called
	  "bidirectionnel".
	- name is the sentence a person reads in the tree, and the only one.
	  An option has no code: nothing is built out of it the way a location
	  path is built out of codes, so a name that reads as a sentence costs
	  nothing and says more.

	switched_on is what the person clicked, and it is not the same question
	as "does this option apply" - see OptionTree::isActive, which answers
	the second one.
*/
class ProjectOption
{
	Q_DECLARE_TR_FUNCTIONS(ProjectOption)

	public:
		ProjectOption();
		explicit ProjectOption(const QString &option_name,
				       const QString &option_description
				       = QString());

			/// @return true when there is no name, so nothing can be shown
		bool isNull() const;

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();

		/**
			@brief Whether a name can be given to an option.
			@param option_name the name as typed
			@param error filled with what is wrong when it cannot
			@return true when the name is usable

			Only the empty name is refused. An option name is read and
			never parsed - no path is built out of it, unlike a location
			code - so no character has to be kept out of it.
		*/
		static bool isValidName(const QString &option_name,
					QString *error = nullptr);

			/// @return the name with the spaces a person leaves behind folded away
		static QString sanitizeName(const QString &option_name);

		bool operator==(const ProjectOption &other) const;
		bool operator!=(const ProjectOption &other) const;

	public:
			/// Given when the option enters a tree and never afterwards
		QString uuid;
			/// the option this one refines, empty when it refines none
		QString parent_uuid;

			/// the sentence the tree shows
		QString name;
			/// what the name has no room for: when to use it, what it costs
		QString description;

			/**
				Whether the person has turned this option on. Stored, because
				it is a choice and not a deduction - and kept even while an
				ancestor is off, so that turning the ancestor back on brings
				back exactly the configuration that was there before.
			*/
		bool switched_on = false;
};

#endif // PROJECTOPTION_H
