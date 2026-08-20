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
#include "qetenvironment.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

QString QETEnvironment::m_default_path = QString();

namespace
{
	const char *SETTINGS_KEY = "environment/path";

	QString withoutTrailingSlash(QString path)
	{
		while (path.endsWith(QLatin1Char('/')) || path.endsWith(QLatin1Char('\\'))) {
			path.chop(1);
		}
		return path;
	}
}

/**
	@brief QETEnvironment::setDefaultPath
	@param path
	Called once by QETApp at startup with the folder the program already uses,
	so that the fallback here and QETApp::dataDir() cannot say different things.
*/
void QETEnvironment::setDefaultPath(const QString &path)
{
	m_default_path = withoutTrailingSlash(path);
}

/**
	@brief QETEnvironment::defaultPath
	@return the environment used when the user chose none
*/
QString QETEnvironment::defaultPath()
{
	if (!m_default_path.isEmpty()) {
		return m_default_path;
	}
	// Same location QETApp::dataDir() resolves to. Reached only when nobody
	// called setDefaultPath, which in practice means the test suite.
	return withoutTrailingSlash(
		QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

/**
	@brief QETEnvironment::isConfigured
	@return true when a path was chosen explicitly
*/
bool QETEnvironment::isConfigured()
{
	QSettings settings;
	return !settings.value(QLatin1String(SETTINGS_KEY)).toString().isEmpty();
}

/**
	@brief QETEnvironment::path
	@return the environment in use
*/
QString QETEnvironment::path()
{
	QSettings settings;
	const QString configured =
		withoutTrailingSlash(settings.value(QLatin1String(SETTINGS_KEY)).toString());
	if (!configured.isEmpty()) {
		return configured;
	}
	return defaultPath();
}

/**
	@brief QETEnvironment::setPath
	@param path
	@param error
	@return true when @a path is usable and was stored
*/
bool QETEnvironment::setPath(const QString &path, QString *error)
{
	const QString wanted = withoutTrailingSlash(path.trimmed());

	if (wanted.isEmpty())
	{
		// Empty means "go back to the default", which is a legitimate move.
		QSettings settings;
		settings.remove(QLatin1String(SETTINGS_KEY));
		return true;
	}

	QDir directory(wanted);
	if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
	{
		if (error) {
			*error = QCoreApplication::translate("QETEnvironment",
							     "Impossible de créer le dossier d'environnement %1.")
				 .arg(wanted);
		}
		return false;
	}

	if (!QFileInfo(wanted).isWritable())
	{
		// Read only is not refused: an environment on a share where this user
		// may only read is a real case, and drawing has to work. Saying it out
		// loud is the interface's job.
		if (error) {
			*error = QCoreApplication::translate("QETEnvironment",
							     "Le dossier %1 n'est pas accessible en écriture : "
							     "l'environnement sera utilisable en lecture seule.")
				 .arg(wanted);
		}
	}

	QString skeleton_error;
	if (!createSkeleton(wanted, &skeleton_error) && error && error->isEmpty())
	{
		*error = skeleton_error;
		return false;
	}

	QSettings settings;
	settings.setValue(QLatin1String(SETTINGS_KEY), wanted);
	return true;
}

/**
	@brief QETEnvironment::skeletonFolders
	@return the relative names of the folders an environment holds
*/
QStringList QETEnvironment::skeletonFolders()
{
	return { QStringLiteral("projects"),
		 QStringLiteral("elements"),
		 QStringLiteral("elements-company"),
		 QStringLiteral("titleblocks"),
		 QStringLiteral("titleblocks-company"),
		 QStringLiteral("macros"),
		 QStringLiteral("groupings") };
}

/**
	@brief QETEnvironment::createSkeleton
	@param root
	@param error
	@return true when every folder exists afterwards
*/
bool QETEnvironment::createSkeleton(const QString &root, QString *error)
{
	QDir directory(root);
	if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
	{
		if (error) {
			*error = QCoreApplication::translate("QETEnvironment",
							     "Impossible de créer le dossier %1.").arg(root);
		}
		return false;
	}

	const QStringList folders = skeletonFolders();
	for (const QString &folder : folders)
	{
		if (directory.exists(folder)) {
			continue;
		}
		if (!directory.mkpath(folder))
		{
			if (error) {
				*error = QCoreApplication::translate("QETEnvironment",
								     "Impossible de créer le dossier %1.")
					 .arg(directory.absoluteFilePath(folder));
			}
			return false;
		}
	}
	return true;
}

/**
	@brief QETEnvironment::looksLikeEnvironment
	@param root
	@return true when @a root holds the folders an environment holds
*/
bool QETEnvironment::looksLikeEnvironment(const QString &root)
{
	const QDir directory(root);
	if (!directory.exists()) {
		return false;
	}
	const QStringList folders = skeletonFolders();
	for (const QString &folder : folders)
	{
		if (!directory.exists(folder)) {
			return false;
		}
	}
	return true;
}

/**
	@brief QETEnvironment::subDir
	@param name
	@return the absolute path of the @a name folder of the environment,
	created when missing
*/
QString QETEnvironment::subDir(const QString &name)
{
	const QString root = path();
	QDir directory(root);
	if (!directory.exists(name)) {
		directory.mkpath(name);
	}
	return directory.absoluteFilePath(name);
}

QString QETEnvironment::projectsDir()
{
	return subDir(QStringLiteral("projects"));
}

QString QETEnvironment::elementsDir()
{
	return subDir(QStringLiteral("elements"));
}

QString QETEnvironment::companyElementsDir()
{
	return subDir(QStringLiteral("elements-company"));
}

QString QETEnvironment::titleBlocksDir()
{
	return subDir(QStringLiteral("titleblocks"));
}

QString QETEnvironment::companyTitleBlocksDir()
{
	return subDir(QStringLiteral("titleblocks-company"));
}

QString QETEnvironment::macrosDir()
{
	return subDir(QStringLiteral("macros"));
}

/**
	@brief QETEnvironment::groupingsDir
	@return where the graphic groupings are filed
*/
QString QETEnvironment::groupingsDir()
{
	return subDir(QStringLiteral("groupings"));
}

/**
	@brief QETEnvironment::catalogFile
	@return the shared catalog of this environment
*/
QString QETEnvironment::catalogFile()
{
	return path() + QStringLiteral("/catalog.sqlite");
}

/**
	@brief QETEnvironment::copyTo
	@param destination
	@param error
	@return how many files were copied, -1 on failure
*/
int QETEnvironment::copyTo(const QString &destination, QString *error)
{
	const QString source = QDir(path()).absolutePath();
	const QString target = QDir(destination).absolutePath();

	if (target == source
	    || target.startsWith(source + QLatin1Char('/'), Qt::CaseInsensitive))
	{
		// A copy inside the folder it protects is not a copy: the day the
		// share is lost, it goes with it.
		if (error) {
			*error = QCoreApplication::translate("QETEnvironment",
							     "La copie de sauvegarde doit être en dehors de "
							     "l'environnement : %1 est à l'intérieur de %2.")
				 .arg(target, source);
		}
		return -1;
	}

	QDir target_dir(target);
	if (!target_dir.exists() && !target_dir.mkpath(QStringLiteral(".")))
	{
		if (error) {
			*error = QCoreApplication::translate("QETEnvironment",
							     "Impossible de créer le dossier %1.").arg(target);
		}
		return -1;
	}

	int copied = 0;
	const QDir source_dir(source);
	QDirIterator iterator(source, QDir::Files | QDir::NoDotAndDotDot,
			      QDirIterator::Subdirectories);
	while (iterator.hasNext())
	{
		const QString file = iterator.next();
		const QString relative = source_dir.relativeFilePath(file);
		const QString destination_file = target_dir.absoluteFilePath(relative);

		const QString destination_folder = QFileInfo(destination_file).absolutePath();
		if (!QDir().mkpath(destination_folder))
		{
			if (error) {
				*error = QCoreApplication::translate("QETEnvironment",
								     "Impossible de créer le dossier %1.")
					 .arg(destination_folder);
			}
			return -1;
		}

		// Overwrite: a backup run twice must end up with the current state,
		// not fail on the second run.
		if (QFile::exists(destination_file)) {
			QFile::remove(destination_file);
		}
		if (!QFile::copy(file, destination_file))
		{
			if (error) {
				*error = QCoreApplication::translate("QETEnvironment",
								     "Impossible de copier %1 vers %2.")
					 .arg(relative, destination_file);
			}
			return -1;
		}
		++copied;
	}

	return copied;
}
