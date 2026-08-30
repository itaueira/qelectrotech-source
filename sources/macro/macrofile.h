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
#ifndef MACROFILE_H
#define MACROFILE_H

#include "macroparameterset.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QDomElement>
#include <QString>

class XmlElementCollection;

/**
	@brief One .qetmak file, read into memory.

	Reading a macro is four things that always go together: open it, check
	it really is one, take the variables it declares, and rewrite the
	symbol references it carries from macro:// to embed:// so the symbols
	resolve against the project they are about to be copied into. Getting
	one of the four wrong gives a drawing that inserts and then shows
	rectangles where the symbols should be.

	They lived as private methods of DiagramEventAddMacro while the drop on
	a folio was the only way in. The generator of T08 is a second way in,
	and it must read exactly what the drop reads - a macro that generated
	differently from how it drops would be a macro nobody could check by
	dropping it once.

	Reading only: at the end of load() no project has been touched and
	nothing is undoable, which is what lets the caller still give up.
*/
class MacroFile
{
	Q_DECLARE_TR_FUNCTIONS(MacroFile)

	public:
	MacroFile() = default;
	explicit MacroFile(const QString &file_path);

		/**
			Read @a file_path, replacing whatever was read before.
			@param file_path : an absolute path to a .qetmak
			@return whether it is a macro this version can read
		*/
	bool load(const QString &file_path);

		/// @return whether nothing usable has been read
	bool isNull() const;
		/// @return why the last load() failed, ready to be shown
	QString errorText() const;
		/// @return the path the last load() was given
	QString filePath() const;

		/// @return the variables the macro declares, empty when it declares none
	const MacroParameterSet &parameters() const;

		/**
			@return the <diagram> node the macro carries, null when it has none

			The node of the document itself: substituting on it would write
			the answers of the first insertion into the second one. Callers
			that are going to change anything take clonedDiagramNode().
		*/
	QDomElement diagramNode() const;

		/// @return a deep copy of diagramNode(), safe to substitute on
	QDomElement clonedDiagramNode() const;

		/**
			Copy the symbols the macro carries into @a collection.
			@param collection : the embedded collection of a project

			Called once per project the macro is about to be drawn in -
			including the throwaway ones used to measure and to preview,
			because a preview that imported differently from the insertion
			would be showing something else than what is about to happen.
		*/
	void importCollection(XmlElementCollection *collection) const;

	private:
	QString m_file_path;
	QDomDocument m_doc;
	MacroParameterSet m_parameters;
	QString m_error;
};

#endif // MACROFILE_H
