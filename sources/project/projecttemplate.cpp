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
#include "projecttemplate.h"

#include "../environment/qetenvironment.h"
#include "../qetproject.h"

#include <QCoreApplication>
#include <QFileInfo>

/**
	@brief ProjectTemplate::templatesDir
	@return the folder the shared project templates are filed in

	Inside the environment, so that pointing the program at the shared folder
	brings the templates along with the symbols and the title blocks - which
	is the whole promise of having one folder.

	Deliberately **not** part of QETEnvironment::skeletonFolders(): adding a
	folder there would make every environment created before today stop
	looking like an environment, and the dialog would start asking whether
	this really is one. The folder is made the first time it is asked for,
	which is the first time somebody opens this dialog.
*/
QString ProjectTemplate::templatesDir()
{
	return QETEnvironment::projectTemplatesDir();
}

/**
	@brief ProjectTemplate::nameFilter
	@return the file dialog filter for a project template
*/
QString ProjectTemplate::nameFilter()
{
	return QCoreApplication::translate(
		"ProjectTemplate",
		"Modèles de projet QElectroTech (*.qet);;Tous les fichiers (*)");
}

/**
	@brief ProjectTemplate::detachFromFile
	@param project
*/
void ProjectTemplate::detachFromFile(QETProject *project)
{
	if (!project || project->filePath().isEmpty()) {
		return;
	}

		//Order matters. Dropping the path first is what makes the next save
		//ask where to go; it also rewrites the "savedfilename" and
		//"savedfilepath" properties, so a title block that prints the file
		//name stops printing the template's.
	project->setFilePath(QString());

		//A template may well sit on a read-only share. The project read out
		//of it must not inherit that: it is going to be saved somewhere
		//else entirely.
	project->setReadOnly(false);

		//What was remembered of the template file on disk describes a file
		//this project no longer has anything to do with.
	project->captureDiskState();

		//Nothing on any disk holds what is in memory now, so it is modified
		//by definition - and closing it has to ask.
	project->setModified(true);
}

/**
	@brief ProjectTemplate::openAsNewProject
	@param template_path
	@param parent
	@param error
	@return the new project, nullptr on failure
*/
QETProject *ProjectTemplate::openAsNewProject(const QString &template_path,
					      QObject *parent,
					      QString *error)
{
	if (error) {
		error->clear();
	}

	const QFileInfo info(template_path);
	if (template_path.isEmpty() || !info.exists() || !info.isFile())
	{
		if (error) {
			*error = QCoreApplication::translate(
					 "ProjectTemplate",
					 "Le modèle %1 est introuvable.")
				 .arg(template_path);
		}
		return nullptr;
	}

	if (!info.isReadable())
	{
		if (error) {
			*error = QCoreApplication::translate(
					 "ProjectTemplate",
					 "Le modèle %1 n'est pas accessible en lecture.")
				 .arg(info.absoluteFilePath());
		}
		return nullptr;
	}

	auto *project = new QETProject(info.absoluteFilePath(), parent);
	if (project->state() != QETProject::Ok)
	{
		if (error) {
			*error = QCoreApplication::translate(
					 "ProjectTemplate",
					 "Le fichier %1 n'est pas un projet QElectroTech : "
					 "il ne peut pas servir de modèle.")
				 .arg(info.absoluteFilePath());
		}
		delete project;
		return nullptr;
	}

	detachFromFile(project);
	return project;
}
