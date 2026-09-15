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
#ifndef FOLIOVARIABLES_H
#define FOLIOVARIABLES_H

#include <QString>

/**
	@brief The substitution that turns a folio template into a page number.

	What a project file stores in the @c folio attribute of a sheet is not
	a page number, it is a template: the default one, written by
	TitleBlockProperties::fromSettings(), is literally "%id/%total". The
	number a reader sees on the drawing is produced from it by
	BorderTitleBlock::setFolioData(), and until this namespace existed that
	substitution had exactly one caller and lived inside the title block.

	@par Why it is a namespace of its own and not a method of the title block

	Anything that reports a page - the drawing, a wiring list, a bill of
	material - has to give the same answer, and the only way to guarantee
	that is one substitution. The title block cannot be that place: it is a
	QGraphicsItem that drags a template renderer and half of the program
	behind it, so a reader that only has the project's XML in hand cannot
	call it without opening a scene.

	This was not hypothetical. sources/wiringlistexport.cpp read the raw
	attribute and printed it, so every one of the 250 rows of a real
	project's cable list carried the literal "%id/%total" in its Page
	column - a list that goes to the bench, with the one column that says
	where to look filled with a template.

	@par What is substituted, and what is deliberately not

	Two functions rather than one, because the title block needs the
	intermediate result: it keeps the autonum-substituted template as the
	new template (so that a page number is drawn once and stays drawn) and
	only then resolves the position. A single function would have to give
	back both strings to say the same thing.

	A reader working from a saved file only ever needs resolveIndex():
	%autonum cannot survive a save. BorderTitleBlock::setFolioData()
	substitutes it into the stored template on every refresh - even when
	the numbering context is empty, in which case it substitutes nothing
	and the variable is gone - and QETProject::updateDiagramsFolioData()
	runs that refresh when the project is read, when a sheet is added and
	when the order of the sheets changes.

	No variable is invented here. Whatever is left untouched stays in the
	string rather than being blanked, so an unknown variable reaches the
	reader as itself instead of as an empty cell.
*/
namespace FolioVariables
{
		/**
			@brief folio with %autonum replaced by autonum.
			@param folio : the template, usually the stored folio of a sheet
			@param autonum : the represented value of the numbering context;
			an empty value removes the variable, which is what the title
			block has always done for a sheet with no numbering context.
		*/
	QString resolveAutonum(const QString &folio, const QString &autonum);

		/**
			@brief folio with %id and %total replaced by the position of the
			sheet and by how many sheets the project holds.
			@param folio : the template, usually the stored folio of a sheet
			@param index : position of the sheet, counted from 1
			@param total : number of sheets in the project
		*/
	QString resolveIndex(const QString &folio, int index, int total);
}

#endif // FOLIOVARIABLES_H
