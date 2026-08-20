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
#include "catalogrepository.h"

#include "../environment/qetenvironment.h"
#include "catalog.h"
#include "catalogpackage.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QXmlStreamReader>

namespace
{
	const char *SETTINGS_KEY = "catalog/repository-path";
}

/**
	@brief CatalogRepositoryEntry::isNull
	@return true when this entry carries nothing
*/
bool CatalogRepositoryEntry::isNull() const
{
	return file_path.isEmpty() && code.isEmpty();
}

/**
	@brief CatalogRepositoryEntry::matches
	@param text
	@return true when @a text appears in any searchable field
*/
bool CatalogRepositoryEntry::matches(const QString &text) const
{
	if (text.isEmpty()) {
		return true;
	}
	return code.contains(text, Qt::CaseInsensitive)
	       || designation.contains(text, Qt::CaseInsensitive)
	       || manufacturer.contains(text, Qt::CaseInsensitive)
	       || class_name.contains(text, Qt::CaseInsensitive)
	       || class_key.contains(text, Qt::CaseInsensitive);
}

/**
	@brief CatalogRepository::defaultFolderName
	@return the folder of the environment the repository starts in
*/
QString CatalogRepository::defaultFolderName()
{
	return QStringLiteral("repositorio-de-pecas");
}

/**
	@brief CatalogRepository::path
	@return the repository in use
*/
QString CatalogRepository::path()
{
	QSettings settings;
	const QString configured = settings.value(QLatin1String(SETTINGS_KEY)).toString();
	if (!configured.isEmpty()) {
		return configured;
	}
	return QETEnvironment::path() + QLatin1Char('/') + defaultFolderName();
}

/**
	@brief CatalogRepository::setPath
	@param path : empty to go back to the folder of the environment
	@param error
	@return true when the path was stored
*/
bool CatalogRepository::setPath(const QString &path, QString *error)
{
	if (path.trimmed().isEmpty())
	{
		QSettings settings;
		settings.remove(QLatin1String(SETTINGS_KEY));
		return true;
	}

	QDir directory(path.trimmed());
	if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogRepository",
							     "Impossible de créer le dossier %1.")
				 .arg(path.trimmed());
		}
		return false;
	}

	QSettings settings;
	settings.setValue(QLatin1String(SETTINGS_KEY), directory.absolutePath());
	return true;
}

/**
	@brief CatalogRepository::readEntry
	@param file_path
	@return the head of the package
*/
CatalogRepositoryEntry CatalogRepository::readEntry(const QString &file_path)
{
	CatalogRepositoryEntry entry;

	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return entry;
	}

	QXmlStreamReader reader(&file);
	if (!reader.readNextStartElement()) {
		return entry;
	}
	if (reader.name() != QLatin1String("qet-catalog-part")) {
		return entry;
	}

	entry.file_path = file_path;
	entry.code = reader.attributes().value(QStringLiteral("code")).toString();
	entry.class_key = reader.attributes().value(QStringLiteral("class-key")).toString();
	entry.class_name = reader.attributes().value(QStringLiteral("class-name")).toString();

	// Only the three properties the list shows, and then stop: parsing the
	// whole package to draw a row is what makes a repository of a thousand
	// parts feel broken.
	while (!reader.atEnd())
	{
		reader.readNext();
		if (!reader.isStartElement()) {
			continue;
		}
		if (reader.name() != QLatin1String("property"))
		{
			reader.skipCurrentElement();
			continue;
		}
		const QString key = reader.attributes().value(QStringLiteral("key")).toString();
		const QString value = reader.readElementText();
		if (key == QLatin1String("designation")) {
			entry.designation = value;
		} else if (key == QLatin1String("manufacturer")) {
			entry.manufacturer = value;
		} else if (key == QLatin1String("image")) {
			entry.image = value;
		}

		if (!entry.designation.isEmpty()
		    && !entry.manufacturer.isEmpty()
		    && !entry.image.isEmpty()) {
			break;
		}
	}

	return entry;
}

/**
	@brief CatalogRepository::entries
	@param root
	@return every package in @a root
*/
QList<CatalogRepositoryEntry> CatalogRepository::entries(const QString &root)
{
	QList<CatalogRepositoryEntry> found;
	if (root.isEmpty() || !QFileInfo::exists(root)) {
		return found;
	}

	const QStringList filter = { QStringLiteral("*.") + CatalogPackage::fileExtension() };
	QDirIterator iterator(root, filter, QDir::Files | QDir::NoDotAndDotDot,
			      QDirIterator::Subdirectories);
	while (iterator.hasNext())
	{
		const CatalogRepositoryEntry entry = readEntry(iterator.next());
		if (!entry.isNull()) {
			found.append(entry);
		}
	}
	return found;
}

/**
	@brief CatalogRepository::search
	@param entries
	@param text
	@param class_key
	@param manufacturer
	@return the entries matching every given criterion
*/
QList<CatalogRepositoryEntry> CatalogRepository::search(
		const QList<CatalogRepositoryEntry> &entries,
		const QString &text,
		const QString &class_key,
		const QString &manufacturer)
{
	QList<CatalogRepositoryEntry> found;
	const QString wanted_text = text.trimmed();

	for (const CatalogRepositoryEntry &entry : entries)
	{
		if (!entry.matches(wanted_text)) {
			continue;
		}
		if (!class_key.isEmpty() && entry.class_key != class_key) {
			continue;
		}
		if (!manufacturer.isEmpty()
		    && entry.manufacturer.compare(manufacturer, Qt::CaseInsensitive) != 0) {
			continue;
		}
		found.append(entry);
	}
	return found;
}

/**
	@brief CatalogRepository::manufacturersOf
	@param entries
	@return the manufacturers present, sorted
*/
QStringList CatalogRepository::manufacturersOf(const QList<CatalogRepositoryEntry> &entries)
{
	QStringList manufacturers;
	for (const CatalogRepositoryEntry &entry : entries)
	{
		if (!entry.manufacturer.isEmpty() && !manufacturers.contains(entry.manufacturer)) {
			manufacturers.append(entry.manufacturer);
		}
	}
	manufacturers.sort();
	return manufacturers;
}

/**
	@brief CatalogRepository::classKeysOf
	@param entries
	@return the class keys present, sorted
*/
QStringList CatalogRepository::classKeysOf(const QList<CatalogRepositoryEntry> &entries)
{
	QStringList keys;
	for (const CatalogRepositoryEntry &entry : entries)
	{
		if (!entry.class_key.isEmpty() && !keys.contains(entry.class_key)) {
			keys.append(entry.class_key);
		}
	}
	keys.sort();
	return keys;
}

/**
	@brief CatalogRepository::contribute
	@param root
	@param catalog
	@param part
	@param error
	@return true when the package was written
*/
bool CatalogRepository::contribute(const QString &root,
				   const Catalog &catalog,
				   const CatalogPart &part,
				   QString *error)
{
	QDir directory(root);
	if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogRepository",
							     "Impossible de créer le dossier %1.").arg(root);
		}
		return false;
	}

	const QString file =
		directory.absoluteFilePath(CatalogPackage::suggestedFileName(part));
	return CatalogPackage::write(file, catalog, part, error);
}
