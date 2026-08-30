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
#include "macrofile.h"

#include "../ElementsCollection/xmlelementcollection.h"
#include "../NameList/nameslist.h"

#include <QDebug>
#include <QDomNodeList>
#include <QFile>
#include <QIODevice>
#include <QStringList>

/**
	@brief MacroFile::MacroFile
	@param file_path
*/
MacroFile::MacroFile(const QString &file_path)
{
	load(file_path);
}

/**
	@brief MacroFile::load
	@param file_path
	@return whether the file holds a macro this version can read
*/
bool MacroFile::load(const QString &file_path)
{
	m_file_path = file_path;
	m_doc = QDomDocument();
	m_parameters = MacroParameterSet();
	m_error.clear();

	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qDebug() << "Error: Macro file could not be read:" << file_path;
		m_error = tr("Le fichier %1 n'a pas pu être ouvert.").arg(file_path);
		return false;
	}

	if (!m_doc.setContent(&file)) {
		qDebug() << "Error: Invalid XML in macro.";
		m_error = tr("Le fichier %1 ne contient pas de XML valide.").arg(file_path);
		m_doc = QDomDocument();
		return false;
	}

	QDomElement root = m_doc.documentElement();
	if (root.tagName() != QLatin1String("qet_macro")) {
		m_error = tr("Le fichier %1 n'est pas une macro QElectroTech.").arg(file_path);
		m_doc = QDomDocument();
		return false;
	}

		//A macro written before this existed carries no <parameters>, and
		//reading none is not an error: that is every macro made so far.
	m_parameters.fromXml(root);

		//The symbols travel inside the macro, and they are about to live in
		//the embedded collection of whatever project this lands in. Rewriting
		//the reference here, once, is what makes the same file work dropped
		//on a folio and generated from a table.
	QDomElement diagram_node = root.firstChildElement(QStringLiteral("diagram_content"))
				       .firstChildElement(QStringLiteral("diagram"));
	if (!diagram_node.isNull())
	{
		QDomNodeList instances = diagram_node.elementsByTagName(QStringLiteral("element"));
		for (int i = 0; i < instances.count(); ++i)
		{
			QDomElement inst = instances.at(i).toElement();
			QString type = inst.attribute(QStringLiteral("type"));
			if (type.startsWith(QLatin1String("macro://"))) {
				inst.setAttribute(QStringLiteral("type"),
						  type.replace(QLatin1String("macro://"),
							       QLatin1String("embed://")));
			}
		}
	}

	return true;
}

/**
	@brief MacroFile::isNull
	@return whether nothing usable has been read
*/
bool MacroFile::isNull() const
{
	return m_doc.isNull();
}

/**
	@brief MacroFile::errorText
	@return why the last load() failed
*/
QString MacroFile::errorText() const
{
	return m_error;
}

/**
	@brief MacroFile::filePath
	@return the path the last load() was given
*/
QString MacroFile::filePath() const
{
	return m_file_path;
}

/**
	@brief MacroFile::parameters
	@return the variables the macro declares
*/
const MacroParameterSet &MacroFile::parameters() const
{
	return m_parameters;
}

/**
	@brief MacroFile::diagramNode
	@return the <diagram> node the macro carries
*/
QDomElement MacroFile::diagramNode() const
{
	if (m_doc.isNull()) {
		return QDomElement();
	}

	return m_doc.documentElement()
		    .firstChildElement(QStringLiteral("diagram_content"))
		    .firstChildElement(QStringLiteral("diagram"));
}

/**
	@brief MacroFile::clonedDiagramNode
	@return a deep copy of diagramNode()
*/
QDomElement MacroFile::clonedDiagramNode() const
{
	const QDomElement node = diagramNode();
	if (node.isNull()) {
		return QDomElement();
	}

	return node.cloneNode(true).toElement();
}

/**
	@brief MacroFile::importCollection
	@param collection
*/
void MacroFile::importCollection(XmlElementCollection *collection) const
{
	if (!collection || m_doc.isNull()) {
		return;
	}

	QDomElement root = m_doc.documentElement();
	QDomElement collection_node = root.firstChildElement(QStringLiteral("collection"));
	if (collection_node.isNull()) {
		return;
	}

	QDomNodeList elements = collection_node.elementsByTagName(QStringLiteral("element"));
	for (int i = 0; i < elements.count(); ++i)
	{
		QDomElement elmt_node = elements.at(i).toElement();
		QString path = elmt_node.attribute(QStringLiteral("path"));
		QDomElement definition = elmt_node.firstChildElement(QStringLiteral("definition"));

		if (!path.isEmpty() && !definition.isNull())
		{
			int last_slash = path.lastIndexOf(QLatin1Char('/'));
			QString dir_path = (last_slash != -1) ? path.left(last_slash) : QString();
			QString file_name = (last_slash != -1) ? path.mid(last_slash + 1) : path;

			if (!dir_path.isEmpty())
			{
				QStringList parts = dir_path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
				QString current_path;
				for (const QString &part : parts)
				{
					QString parent_path = current_path;
					if (!current_path.isEmpty()) {
						current_path += QLatin1Char('/');
					}
					current_path += part;
					if (current_path == QLatin1String("import")) {
						continue;
					}
					NamesList empty_names;
					collection->createDir(parent_path, part, empty_names);
				}
			}
			collection->addElementDefinition(dir_path, file_name, definition);
		}
	}
}
