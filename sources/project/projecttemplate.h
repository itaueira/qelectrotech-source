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
#ifndef PROJECTTEMPLATE_H
#define PROJECTTEMPLATE_H

#include <QString>

class QETProject;
class QObject;

/**
	@brief Starting a project out of another project.

	A workshop starts every job from the same skeleton: a cover sheet, an
	index, a schematic folio already framed with the right title block, and
	whatever else the house always draws. Until now the only way to reuse it
	was "open it and save it under another name", and the step that gets
	forgotten is the second one - the drawing goes into the skeleton, and the
	next job starts from the previous job.

	What is copied here is therefore the whole .qet file, and nothing is
	copied field by field. The file already carries the border and the title
	block of every folio, the properties of both, the project-wide defaults,
	the embedded symbols and the embedded title block templates; copying them
	one by one - the way duplicateDiagram() copies one folio into another of
	the same project - would only get out of step the day one of them gains a
	field.

	The one thing that must **not** come across is the path of the file. See
	detachFromFile(), which is the whole point of this namespace.
*/
namespace ProjectTemplate
{
	/// Where the shared project templates are filed
	QString templatesDir();

	/// The file dialog filter for a project template
	QString nameFilter();

	/**
		@brief Cut @a project loose from the file it was read from.

		A project born from a template that kept pointing at the template is
		one careless Ctrl+S away from destroying the template: the save would
		go through, silently, and the house skeleton would become somebody's
		job. So the path is dropped, which turns the first "Save" into "Save
		as" (see ProjectView::doSave), and the project is marked modified
		because what is in memory now lives in no file at all.

		Read-only is dropped with it: a template on a share where this user
		may only read still has to produce a project they can draw in.

		Does nothing to a project that has no path already, so calling it
		twice is harmless.
	*/
	void detachFromFile(QETProject *project);

	/**
		@brief Open @a template_path and hand back a project that is not
		attached to it.

		The template file is only read: no in-use lock is taken on it, it is
		not added to the recently opened files, and nothing here writes to it.

		@param template_path : the .qet to start from
		@param parent : parent of the returned project
		@param error : when not nullptr, receives the reason on failure
		@return the new project, or nullptr when the template cannot be read.
		The caller owns what comes back.
	*/
	QETProject *openAsNewProject(const QString &template_path,
				     QObject *parent = nullptr,
				     QString *error = nullptr);
}

#endif // PROJECTTEMPLATE_H
