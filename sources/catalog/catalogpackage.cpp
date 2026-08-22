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
#include "catalogpackage.h"

#include "catalog.h"
#include "catalogclasspackage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

/**
	@brief CatalogPackage::fileExtension
	@return the extension of a part package
*/
QString CatalogPackage::fileExtension()
{
	return QStringLiteral("qetpart");
}

/**
	@brief CatalogPackage::fileFilter
	@return the file dialog filter
*/
QString CatalogPackage::fileFilter()
{
	return QCoreApplication::translate("CatalogPackage",
					   "Pièce de catalogue QElectroTech (*.qetpart)");
}

/**
	@brief CatalogPackage::suggestedFileName
	@param part
	@return a file name derived from the part code
*/
QString CatalogPackage::suggestedFileName(const CatalogPart &part)
{
	QString name = part.code.trimmed();
	// A part code carries slashes and spaces often enough that using it raw
	// as a file name would fail on the first real export.
	static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9._-]+"));
	name.replace(unsafe, QStringLiteral("_"));
	while (name.startsWith(QLatin1Char('_'))) {
		name.remove(0, 1);
	}
	if (name.isEmpty()) {
		name = QStringLiteral("piece");
	}
	return name + QLatin1Char('.') + fileExtension();
}

/**
	@brief CatalogPackage::excludedKeys
	@return the property keys a package never carries
*/
QStringList CatalogPackage::excludedKeys()
{
	// Price and commercial terms belong to a company and to a date, not to a
	// part. Sending them along would be sending a negotiation abroad.
	return { QStringLiteral("preco"),
		 QStringLiteral("price"),
		 QStringLiteral("prix"),
		 QStringLiteral("custo"),
		 QStringLiteral("cost"),
		 QStringLiteral("prazo"),
		 QStringLiteral("lead_time"),
		 QStringLiteral("desconto"),
		 QStringLiteral("discount"),
		 QStringLiteral("supplier"),
		 QStringLiteral("fornecedor") };
}

/**
	@brief stripExcludedProperties
	Removes from a class block the fields a package must not carry, and the
	controlled lists nothing references once they are gone. Same list as the
	values: a package that refuses to send a price has no business creating the
	field that holds one - it would arrive empty and stay empty, and the
	commercial columns of one company are not a gift to another.
	@param block : the class block, modified in place
*/
static void stripExcludedProperties(QDomElement &block)
{
	const QStringList excluded = CatalogPackage::excludedKeys();
	QStringList used;
	for (QDomElement element = block.firstChildElement(QStringLiteral("class")) ;
	     !element.isNull() ;
	     element = element.nextSiblingElement(QStringLiteral("class")))
	{
		QDomElement property = element.firstChildElement(QStringLiteral("property"));
		while (!property.isNull())
		{
			QDomElement next = property.nextSiblingElement(QStringLiteral("property"));
			if (excluded.contains(property.attribute(QStringLiteral("key")))) {
				element.removeChild(property);
			}
			else
			{
				const QString list_name = property.attribute(QStringLiteral("list-name"));
				if (!list_name.isEmpty() && !used.contains(list_name)) {
					used.append(list_name);
				}
			}
			property = next;
		}
	}

	QDomElement list = block.firstChildElement(QStringLiteral("list"));
	while (!list.isNull())
	{
		QDomElement next = list.nextSiblingElement(QStringLiteral("list"));
		if (!used.contains(list.attribute(QStringLiteral("name")))) {
			block.removeChild(list);
		}
		list = next;
	}
}

/**
	@brief CatalogPackage::write
	@param file_path
	@param catalog
	@param part
	@param error
	@return true on success
*/
bool CatalogPackage::write(const QString &file_path,
			   const Catalog &catalog,
			   const CatalogPart &part,
			   QString *error)
{
	if (part.isNull())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPackage",
							     "Il n'y a pas de pièce à exporter.");
		}
		return false;
	}

	QDomDocument document;
	QDomElement root = document.createElement(QStringLiteral("qet-catalog-part"));
	root.setAttribute(QStringLiteral("version"), QStringLiteral("1"));
	root.setAttribute(QStringLiteral("code"), part.code);
	root.setAttribute(QStringLiteral("revision"), QString::number(part.revision));
	root.setAttribute(QStringLiteral("class-key"), catalog.classById(part.class_id).key);
	root.setAttribute(QStringLiteral("class-name"), catalog.classById(part.class_id).name);
	root.setAttribute(QStringLiteral("exported-at"),
			  QDateTime::currentDateTime().toString(Qt::ISODate));

	// The class comes along declared, not merely named: the receiver needs the
	// type, the unit, the list and the numbering format to put the values into
	// typed fields instead of an empty node. The ancestry travels so that the
	// class lands in the right place in the tree; the subclasses the sender
	// happens to have below it do not, because a part package carries one
	// class - the one of its part. What it does not declare is the commercial
	// field: the same keys the values leave behind.
	if (part.class_id != 0)
	{
		QDomElement classes = CatalogClassPackage::toXml(document, catalog,
								part.class_id, false);
			//The same rule as the values, applied to the fields.
		stripExcludedProperties(classes);
		root.appendChild(classes);
	}

	// The values, minus what a package must not carry. Written from the
	// effective values so that a field the part never had filled travels with
	// the initial value of its class, which is what the receiver would see.
	const QStringList excluded = excludedKeys();
	const QHash<QString, QString> values = catalog.effectiveValues(part);
	const QStringList keys = values.keys();
	for (const QString &key : keys)
	{
		if (excluded.contains(key)) {
			continue;
		}
		const QString value = values.value(key);
		if (value.isEmpty()) {
			continue;
		}
		QDomElement property = document.createElement(QStringLiteral("property"));
		property.setAttribute(QStringLiteral("key"), key);
		property.appendChild(document.createTextNode(value));
		root.appendChild(property);
	}

	for (const CatalogPin &pin : part.pins)
	{
		QDomElement element = document.createElement(QStringLiteral("pin"));
		element.setAttribute(QStringLiteral("label"), pin.label);
		element.setAttribute(QStringLiteral("role"), CatalogPin::roleToString(pin.role));
		if (!pin.pair.isEmpty()) {
			element.setAttribute(QStringLiteral("pair"), pin.pair);
		}
		if (!pin.group.isEmpty()) {
			element.setAttribute(QStringLiteral("group"), pin.group);
		}
		element.setAttribute(QStringLiteral("order"), QString::number(pin.order_index));
		root.appendChild(element);
	}

	for (const CatalogAccessory &accessory : part.accessories)
	{
		QDomElement element = document.createElement(QStringLiteral("accessory"));
		element.setAttribute(QStringLiteral("code"), accessory.code);
		element.setAttribute(QStringLiteral("quantity"), QString::number(accessory.quantity));
		root.appendChild(element);
	}

	document.appendChild(root);

	QFile file(file_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPackage",
							     "Impossible d'écrire %1.").arg(file_path);
		}
		return false;
	}

	QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	stream.setEncoding(QStringConverter::Utf8);
#else
	stream.setCodec("UTF-8");
#endif
	stream << document.toString(1);
	stream.flush();
	file.close();
	return true;
}

/**
	@brief CatalogPackage::classKeyOf
	@param file_path
	@return the class key the package names
*/
QString CatalogPackage::classKeyOf(const QString &file_path)
{
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return QString();
	}
	QDomDocument document;
	if (!document.setContent(&file)) {
		return QString();
	}
	return document.documentElement().attribute(QStringLiteral("class-key"));
}

/**
	@brief CatalogPackage::read
	@param file_path
	@param catalog
	@param error
	@return the part the package describes
*/
CatalogPart CatalogPackage::read(const QString &file_path,
				 const Catalog &catalog,
				 QString *error)
{
	CatalogPart part;

	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPackage",
							     "Impossible de lire %1.").arg(file_path);
		}
		return part;
	}

	QDomDocument document;
	if (!document.setContent(&file))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPackage",
							     "%1 n'est pas un paquet de pièce.").arg(file_path);
		}
		return part;
	}

	const QDomElement root = document.documentElement();
	if (root.tagName() != QStringLiteral("qet-catalog-part"))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPackage",
							     "%1 n'est pas un paquet de pièce.").arg(file_path);
		}
		return part;
	}

	part.code = root.attribute(QStringLiteral("code")).trimmed();
	// The revision of the sender means nothing here: what arrives is data to
	// put in this catalog, and this catalog decides its own revisions.
	part.revision = 1;
	part.origin = QStringLiteral("package:") + QFileInfo(file_path).fileName();
	part.origin_date = QDateTime::currentDateTime().toString(Qt::ISODate);

	const QString class_key = root.attribute(QStringLiteral("class-key"));
	part.class_id = catalog.classByKey(class_key).id;
	if (part.class_id == 0 && error)
	{
		// Not an error that stops the read: the caller can offer to create the
		// class or to pick another one, which is a better answer than refusing.
		*error = QCoreApplication::translate("CatalogPackage",
						     "La classe « %1 » n'existe pas dans ce catalogue.")
			 .arg(class_key.isEmpty()
			      ? root.attribute(QStringLiteral("class-name"))
			      : class_key);
	}

	QDomElement child = root.firstChildElement();
	while (!child.isNull())
	{
		if (child.tagName() == QStringLiteral("property"))
		{
			const QString key = child.attribute(QStringLiteral("key"));
			if (!key.isEmpty() && !excludedKeys().contains(key)) {
				part.setValue(key, child.text());
			}
		}
		else if (child.tagName() == QStringLiteral("pin"))
		{
			CatalogPin pin(child.attribute(QStringLiteral("label")),
				       CatalogPin::roleFromString(child.attribute(QStringLiteral("role"))));
			pin.pair = child.attribute(QStringLiteral("pair"));
			pin.group = child.attribute(QStringLiteral("group"));
			pin.order_index = child.attribute(QStringLiteral("order")).toInt();
			part.pins.append(pin);
		}
		else if (child.tagName() == QStringLiteral("accessory"))
		{
			bool ok = false;
			const double quantity =
				child.attribute(QStringLiteral("quantity")).toDouble(&ok);
			part.accessories.append(
				CatalogAccessory(child.attribute(QStringLiteral("code")),
						 ok && quantity > 0.0 ? quantity : 1.0));
		}
		child = child.nextSiblingElement();
	}

	return part;
}

/**
	@brief The class block of a part package, or a null element when there is
	none.
	@param file_path
	@param document : must outlive the element it returns
	@param error : set when the file is not a package at all
	Apart because classPlan() and applyClass() both need it, and because a
	package written before CU-14.11 has no such block: telling "no declaration"
	from "a declaration that changes nothing" is what lets the caller say the
	right thing instead of the same thing twice.
*/
static QDomElement classBlockOf(const QString &file_path,
				QDomDocument &document,
				QString *error)
{
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPackage",
							     "Impossible de lire %1.").arg(file_path);
		}
		return QDomElement();
	}

	if (!document.setContent(&file))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPackage",
							     "%1 n'est pas un paquet de pièce.").arg(file_path);
		}
		return QDomElement();
	}

	const QDomElement root = document.documentElement();
	if (root.tagName() != QStringLiteral("qet-catalog-part"))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogPackage",
							     "%1 n'est pas un paquet de pièce.").arg(file_path);
		}
		return QDomElement();
	}

	return root.firstChildElement(CatalogClassPackage::blockTagName());
}

/**
	@brief CatalogPackage::classPlan
	@param file_path
	@param catalog
	@param report
	@param error
	@return true when the package declares its class
*/
bool CatalogPackage::classPlan(const QString &file_path,
			       const Catalog &catalog,
			       CatalogClassPackage::Report *report,
			       QString *error)
{
	QDomDocument document;
	const QDomElement block = classBlockOf(file_path, document, error);
	if (block.isNull()) {
		return false;
	}
	if (report) {
		*report = CatalogClassPackage::plan(block, catalog);
	}
	return true;
}

/**
	@brief CatalogPackage::applyClass
	@param file_path
	@param catalog
	@param report
	@param error
	@return true when the declaration was applied
*/
bool CatalogPackage::applyClass(const QString &file_path,
				Catalog &catalog,
				CatalogClassPackage::Report *report,
				QString *error)
{
	QDomDocument document;
	const QDomElement block = classBlockOf(file_path, document, error);
	if (block.isNull())
	{
		if (error && error->isEmpty())
		{
			*error = QCoreApplication::translate("CatalogPackage",
							     "Ce paquet ne déclare pas sa classe.");
		}
		return false;
	}
	return CatalogClassPackage::applyXml(block, catalog, report, error);
}
