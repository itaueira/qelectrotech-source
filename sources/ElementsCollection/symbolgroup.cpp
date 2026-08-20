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
#include "symbolgroup.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "../catalog/catalogassignment.h"

SymbolGroup::SymbolGroup()
{
}

bool SymbolGroup::isNull() const
{
	return root().isNull();
}

QDomElement SymbolGroup::root() const
{
	const QDomElement element = fragment.documentElement();
	if (element.tagName() == QLatin1String("qet-group")) {
		return element.firstChildElement(QStringLiteral("diagram"));
	}
	if (element.tagName() == QLatin1String("diagram")) {
		return element;
	}
	return QDomElement();
}

int SymbolGroup::elementCount() const
{
	const QDomElement diagram = root();
	if (diagram.isNull()) {
		return 0;
	}
	return diagram.elementsByTagName(QStringLiteral("element")).count();
}

int SymbolGroup::conductorCount() const
{
	const QDomElement diagram = root();
	if (diagram.isNull()) {
		return 0;
	}
	return diagram.elementsByTagName(QStringLiteral("conductor")).count();
}

QStringList SymbolGroup::partCodes() const
{
	QStringList codes;
	const QDomElement diagram = root();
	if (diagram.isNull()) {
		return codes;
	}

	const QDomNodeList elements =
			diagram.elementsByTagName(QStringLiteral("element"));
	for (int i = 0 ; i < elements.count() ; ++i) {
		const QDomElement element = elements.at(i).toElement();
		const QDomElement informations = element.firstChildElement(
					QStringLiteral("elementInformations"));
		if (informations.isNull()) {
			continue;
		}
		QDomElement info = informations.firstChildElement(
					QStringLiteral("elementInformation"));
		while (!info.isNull()) {
			if (info.attribute(QStringLiteral("name")) ==
					CatalogAssignment::partCodeKey()) {
				const QString code = info.text().trimmed();
				if (!code.isEmpty() && !codes.contains(code)) {
					codes << code;
				}
			}
			info = info.nextSiblingElement(
						QStringLiteral("elementInformation"));
		}
	}
	codes.sort();
	return codes;
}

SymbolGroup SymbolGroup::fromFragment(const QString &name,
				      const QDomDocument &fragment)
{
	SymbolGroup group;
	group.name = name.trimmed();
	group.fragment = fragment;
	return group;
}

QString SymbolGroup::extension()
{
	return QStringLiteral("qetgroup");
}

QString SymbolGroup::fileNameFor(const QString &name)
{
	QString file_name;
	for (const QChar &character : name.trimmed()) {
		if (character.isLetterOrNumber()) {
			file_name += character.toLower();
		} else if (character == QLatin1Char('-') ||
			   character == QLatin1Char('_') ||
			   character.isSpace()) {
			if (!file_name.endsWith(QLatin1Char('_'))) {
				file_name += QLatin1Char('_');
			}
		}
	}
	while (file_name.endsWith(QLatin1Char('_'))) {
		file_name.chop(1);
	}
	return file_name;
}

bool SymbolGroup::save(const QString &file_path, QString *error) const
{
	if (isNull()) {
		if (error) {
			*error = QCoreApplication::translate("SymbolGroup",
				"Le groupement est vide.");
		}
		return false;
	}

		//Wrapped in a root of its own so the name and the description have
		//somewhere to live without touching the fragment, which must stay
		//exactly what the sheet produced.
	QDomDocument document;
	QDomElement root = document.createElement(QStringLiteral("qet-group"));
	root.setAttribute(QStringLiteral("version"), QStringLiteral("1"));
	root.setAttribute(QStringLiteral("name"), name.trimmed());
	if (!description.trimmed().isEmpty()) {
		root.setAttribute(QStringLiteral("description"),
				  description.trimmed());
	}
	root.appendChild(document.importNode(this->root(), true));
	document.appendChild(root);

	QFile file(file_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		if (error) {
			*error = QCoreApplication::translate("SymbolGroup",
				"Impossible d'écrire « %1 » : %2")
					.arg(QFileInfo(file_path).fileName(),
					     file.errorString());
		}
		return false;
	}
	QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	stream.setCodec("UTF-8");
#endif
	stream << document.toString(4);
	file.close();
	return true;
}

SymbolGroup SymbolGroup::load(const QString &file_path, QString *error)
{
	SymbolGroup group;
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		if (error) {
			*error = QCoreApplication::translate("SymbolGroup",
				"Impossible de lire « %1 » : %2")
					.arg(QFileInfo(file_path).fileName(),
					     file.errorString());
		}
		return group;
	}

	QString message;
	int line = 0;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
	const QDomDocument::ParseResult result = group.fragment.setContent(&file);
	const bool parsed = bool(result);
	if (!parsed) {
		message = result.errorMessage;
		line = int(result.errorLine);
	}
#else
	const bool parsed = group.fragment.setContent(&file, &message, &line);
#endif
	if (!parsed) {
		if (error) {
			*error = QCoreApplication::translate("SymbolGroup",
				"« %1 » n'est pas un groupement lisible "
				"(ligne %2 : %3).")
					.arg(QFileInfo(file_path).fileName())
					.arg(line)
					.arg(message);
		}
		group.fragment = QDomDocument();
		file.close();
		return group;
	}
	file.close();

	const QDomElement root = group.fragment.documentElement();
	group.name = root.attribute(QStringLiteral("name"));
	group.description = root.attribute(QStringLiteral("description"));
	if (group.name.isEmpty()) {
			//A file written by hand, or renamed: the file name is the only
			//name left, and it is better than showing nothing.
		group.name = QFileInfo(file_path).completeBaseName();
	}

	if (group.isNull() && error) {
		*error = QCoreApplication::translate("SymbolGroup",
			"« %1 » ne contient pas de morceau de folio.")
				.arg(QFileInfo(file_path).fileName());
	}
	return group;
}

QStringList SymbolGroup::listFolder(const QString &dir_path)
{
	QStringList paths;
	QDir dir(dir_path);
	if (!dir.exists()) {
		return paths;
	}
	const QFileInfoList entries = dir.entryInfoList(
				QStringList{QStringLiteral("*.%1").arg(extension())},
				QDir::Files, QDir::Name);
	for (const QFileInfo &entry : entries) {
		paths << entry.absoluteFilePath();
	}
	return paths;
}
