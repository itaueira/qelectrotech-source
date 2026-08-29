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
#ifndef PINOUTUSAGESCANNER_H
#define PINOUTUSAGESCANNER_H

#include "pinoutusage.h"

class Diagram;
class Element;
class QETProject;

/**
	@brief Reads what a project already has drawn into a PinoutUsage.

	The bridge, and deliberately thin, the same way SheetSymbolExtractor is:
	it walks the folios and the elements of a project and turns each of them
	into an entry. Everything that could get the rule wrong — what counts as
	the same component, which name is taken, what is said about it — lives in
	PinoutUsage instead, where a bench can prove it without a project being
	open.
*/
class PinoutUsageScanner
{
	public:
		/**
			@brief Read every folio of @a project.
			@param conflicts appended with the terminals the project
			already draws twice. A project that has them is not
			refused anything: it is a drawing somebody made before
			this check existed, and what is owed to them is the list,
			not a locked door.
		*/
		static PinoutUsage scan(QETProject *project,
				QList<PinoutUsageConflict> *conflicts = nullptr);
		static PinoutUsage scan(Diagram *diagram,
				QList<PinoutUsageConflict> *conflicts = nullptr);
		/// the same, without what @a excluded holds
		static PinoutUsage scanExcluding(QETProject *project,
				const Element *excluded,
				QList<PinoutUsageConflict> *conflicts = nullptr);

		/// the component, the part and the folio of @a element
		static PinoutUsageEntry placeOf(const Element *element);
		/// the names its terminals carry on the sheet, without the empty
		static QStringList terminalsOf(const Element *element);

		/// why @a symbol cannot be inserted in @a project as @a component
		static QString refusal(QETProject *project,
				       const QString &component,
				       const SymbolDefinition &symbol);
		/**
			@return why @a element cannot stay as it is, or an empty
			string when it can. Asked of an element already on a
			folio, so what it holds itself is left out of the count.
		*/
		static QString refusal(QETProject *project,
				       const Element *element);

	private:
		static void read(PinoutUsage &usage, const Element *element,
				 QList<PinoutUsageConflict> *conflicts);
};

#endif // PINOUTUSAGESCANNER_H
