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
#include "catalogclasspackage.h"

#include "catalog.h"
#include "catalogclass.h"
#include "catalogproperty.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QTextStream>

namespace
{
	const QLatin1String CLASS_TAG("class");
	const QLatin1String PROPERTY_TAG("property");
	const QLatin1String OPTION_TAG("option");
	const QLatin1String LIST_TAG("list");
	const QLatin1String VALUE_TAG("value");
	const QLatin1String FORMAT_TAG("numbering-format");

	/**
		@brief One class of the file, resolved against the target catalog.
		Kept beside the CatalogClass rather than in it because what matters
		while reading is the key of the parent, and a CatalogClass only
		knows its identifier - which is exactly what a file cannot carry.
	*/
	class FileClass
	{
		public:
			QDomElement element;
			QString key;
			QString parent_key;
			QString name;
			int id = 0;            ///< >0 once it exists in the target catalog
			bool done = false;
			/// key and type of what this class declares, for the dry run
			QList<QPair<QString, CatalogPropertyType>> declared;
	};

	QStringList childTexts(const QDomElement &element, const QLatin1String &tag_name)
	{
		QStringList values;
		for (QDomElement child = element.firstChildElement(tag_name) ;
		     !child.isNull() ;
		     child = child.nextSiblingElement(tag_name))
		{
			values << child.text();
		}
		return values;
	}

	QDomElement propertyToXml(QDomDocument &document, const CatalogProperty &property)
	{
		QDomElement element = document.createElement(PROPERTY_TAG);
		element.setAttribute(QStringLiteral("key"), property.key);
		element.setAttribute(QStringLiteral("name"), property.name);
		element.setAttribute(QStringLiteral("type"),
				     CatalogProperty::typeToString(property.type));
		element.setAttribute(QStringLiteral("order"), QString::number(property.order_index));
		if (property.list_behaviour != CatalogListBehaviour::None)
		{
			element.setAttribute(QStringLiteral("list-behaviour"),
					     CatalogProperty::listBehaviourToString(property.list_behaviour));
		}
		if (!property.list_name.isEmpty()) {
			element.setAttribute(QStringLiteral("list-name"), property.list_name);
		}
		if (!property.default_value.isEmpty()) {
			element.setAttribute(QStringLiteral("default"), property.default_value);
		}
		if (!property.unit.isEmpty()) {
			element.setAttribute(QStringLiteral("unit"), property.unit);
		}
		if (!property.description.isEmpty()) {
			element.setAttribute(QStringLiteral("description"), property.description);
		}
			//Inline values as nodes of their own, so that a value holding
			//whatever separator the database uses still arrives whole.
		if (property.list_name.isEmpty())
		{
			for (const QString &option : property.options)
			{
				QDomElement child = document.createElement(OPTION_TAG);
				child.appendChild(document.createTextNode(option));
				element.appendChild(child);
			}
		}
		return element;
	}

	CatalogProperty propertyFromXml(const QDomElement &element, bool *type_known)
	{
		CatalogProperty property;
		property.key = element.attribute(QStringLiteral("key")).trimmed();
		property.name = element.attribute(QStringLiteral("name")).trimmed();
		bool ok = false;
		property.type = CatalogProperty::typeFromString(
					element.attribute(QStringLiteral("type")), &ok);
		if (type_known) {
			*type_known = ok;
		}
		property.list_behaviour = CatalogProperty::listBehaviourFromString(
					element.attribute(QStringLiteral("list-behaviour")));
		property.list_name = element.attribute(QStringLiteral("list-name"));
		property.default_value = element.attribute(QStringLiteral("default"));
		property.unit = element.attribute(QStringLiteral("unit"));
		property.description = element.attribute(QStringLiteral("description"));
		property.order_index = element.attribute(QStringLiteral("order")).toInt();
		property.options = childTexts(element, OPTION_TAG);
		if (property.name.isEmpty()) {
			property.name = property.key;
		}
		return property;
	}

	CatalogClass classFromXml(const QDomElement &element)
	{
		CatalogClass catalog_class;
		catalog_class.key = element.attribute(QStringLiteral("key")).trimmed();
		catalog_class.name = element.attribute(QStringLiteral("name")).trimmed();
		catalog_class.description = element.attribute(QStringLiteral("description"));
		catalog_class.root = element.attribute(QStringLiteral("root"));
		catalog_class.root_iec = element.attribute(QStringLiteral("root-iec"));
		catalog_class.has_symbol =
				element.attribute(QStringLiteral("has-symbol"),
						  QStringLiteral("1")) != QStringLiteral("0");
		catalog_class.order_index = element.attribute(QStringLiteral("order")).toInt();
		const QDomElement format = element.firstChildElement(FORMAT_TAG);
		if (!format.isNull()) {
			catalog_class.numbering_format = format.text();
		}
		if (catalog_class.name.isEmpty()) {
			catalog_class.name = catalog_class.key;
		}
			//The uuid of the sender is deliberately left behind: it is a
			//trace in the file, not an identity. See the header.
		return catalog_class;
	}

	QDomElement rootOfFile(const QString &file_path, QString *error)
	{
		QFile file(file_path);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			if (error) {
				*error = QCoreApplication::translate("CatalogClassPackage",
								     "Impossible de lire %1.").arg(file_path);
			}
			return QDomElement();
		}
		QDomDocument document;
		if (!document.setContent(&file))
		{
			if (error) {
				*error = QCoreApplication::translate("CatalogClassPackage",
								     "%1 n'est pas un paquet de classes.").arg(file_path);
			}
			return QDomElement();
		}
			//A QDomElement keeps the document alive on its own, which is
			//what makes returning one from here safe.
		return document.documentElement();
	}

	/**
		@brief Read a block of classes into a catalog, or work out what
		reading it would do.
		@param root : the block
		@param catalog : always read from
		@param writable : the same catalog, or null for a dry run
		@param report
		@param error
		@return false only when @a root is not one of ours

		One function for both because the two have to answer the same
		thing: a dialog that says "two classes and nine properties" and
		then creates something else is worse than no dialog at all.
	*/
	bool applyOrPlan(const QDomElement &root,
			 const Catalog &catalog,
			 Catalog *writable,
			 CatalogClassPackage::Report *report,
			 QString *error)
	{
		if (root.isNull() || root.tagName() != CatalogClassPackage::blockTagName())
		{
			if (error) {
				*error = QCoreApplication::translate("CatalogClassPackage",
								     "Ce n'est pas un paquet de classes.");
			}
			return false;
		}

		CatalogClassPackage::Report local;

			//The controlled lists first: a mandatory list that arrives
			//after the property that reads from it is a field nobody can
			//fill, because addProperty() copies the values in.
		for (QDomElement list = root.firstChildElement(LIST_TAG) ;
		     !list.isNull() ;
		     list = list.nextSiblingElement(LIST_TAG))
		{
			const QString list_name = list.attribute(QStringLiteral("name")).trimmed();
			if (list_name.isEmpty()) {
				continue;
			}
			const QStringList values = childTexts(list, VALUE_TAG);
			if (catalog.listNames().contains(list_name))
			{
				++local.lists_found;
				if (catalog.listValues(list_name) != values)
				{
					local.refused << QCoreApplication::translate("CatalogClassPackage",
										    "La liste %1 existe déjà ici avec d'autres valeurs : elle n'a pas été modifiée.")
							 .arg(list_name);
				}
				continue;
			}
			++local.lists_created;
			if (writable)
			{
				QString local_error;
				if (!writable->setListValues(list_name, values, &local_error))
				{
					--local.lists_created;
					local.refused << local_error;
				}
			}
		}

		QList<FileClass> file_classes;
		for (QDomElement element = root.firstChildElement(CLASS_TAG) ;
		     !element.isNull() ;
		     element = element.nextSiblingElement(CLASS_TAG))
		{
			FileClass file_class;
			file_class.element = element;
			file_class.key = element.attribute(QStringLiteral("key")).trimmed();
			file_class.parent_key = element.attribute(QStringLiteral("parent-key")).trimmed();
			file_class.name = element.attribute(QStringLiteral("name")).trimmed();
			if (file_class.key.isEmpty())
			{
				local.refused << QCoreApplication::translate("CatalogClassPackage",
									    "Une classe du fichier n'a pas de clé : elle a été laissée de côté.");
				continue;
			}
			if (file_class.name.isEmpty()) {
				file_class.name = file_class.key;
			}
			file_classes << file_class;
		}

		auto indexOfKey = [&file_classes](const QString &key) -> int
		{
			for (int index = 0 ; index < file_classes.size() ; ++index)
			{
				if (file_classes.at(index).key == key) {
					return index;
				}
			}
			return -1;
		};

			//The declaration of @a key in force on the class at @a index:
			//from the catalog as soon as a class of the chain exists there,
			//from the file for the ones that do not exist yet - which is
			//the only way a dry run can know that a property arriving on a
			//subclass is already declared on the parent it brings along.
		auto effectiveType = [&file_classes, &catalog, &indexOfKey]
				     (int index, const QString &key, bool *found) -> CatalogPropertyType
		{
			*found = false;
			int cursor = index;
			int guard = 0;
			while (cursor >= 0 && guard <= file_classes.size())
			{
				++guard;
				const FileClass &current = file_classes.at(cursor);
				if (current.id > 0)
				{
					const CatalogProperty existing =
							catalog.effectiveProperty(current.id, key);
					if (!existing.isNull())
					{
						*found = true;
						return existing.type;
					}
						//The catalog answered for the whole ancestry
						//above it: there is nothing more to walk.
					return CatalogPropertyType::Text;
				}
				for (const QPair<QString, CatalogPropertyType> &declared : current.declared)
				{
					if (declared.first == key)
					{
						*found = true;
						return declared.second;
					}
				}
				cursor = current.parent_key.isEmpty() ? -1 : indexOfKey(current.parent_key);
			}
			return CatalogPropertyType::Text;
		};

		int remaining = file_classes.size();
		bool progress = true;
		while (progress && remaining > 0)
		{
			progress = false;
			for (int index = 0 ; index < file_classes.size() ; ++index)
			{
				FileClass &current = file_classes[index];
				if (current.done) {
					continue;
				}

				const CatalogClass existing = catalog.classByKey(current.key);
				if (!existing.isNull())
				{
						//Recognised by the key. Nothing is modified: this
						//is why reading the same file twice changes
						//nothing the second time.
					current.id = existing.id;
					++local.classes_found;
				}
				else
				{
					int parent_id = 0;
					bool placeable = true;
					if (!current.parent_key.isEmpty())
					{
						const CatalogClass parent = catalog.classByKey(current.parent_key);
						if (!parent.isNull())
						{
							parent_id = parent.id;
						}
						else
						{
							const int parent_index = indexOfKey(current.parent_key);
							if (parent_index < 0)
							{
								local.refused << QCoreApplication::translate("CatalogClassPackage",
													    "La classe %1 va sous %2, qui n'existe ni ici ni dans le fichier.")
										 .arg(current.name, current.parent_key);
								placeable = false;
							}
							else if (!file_classes.at(parent_index).done)
							{
									//Its parent has not been placed yet:
									//another pass will get to it.
								continue;
							}
							else if (writable)
							{
								local.refused << QCoreApplication::translate("CatalogClassPackage",
													    "La classe %1 n'a pas pu être créée : sa classe mère %2 ne l'a pas été.")
										 .arg(current.name, current.parent_key);
								placeable = false;
							}
						}
					}

					if (!placeable)
					{
						current.done = true;
						--remaining;
						progress = true;
						continue;
					}

					++local.classes_created;
					local.missing_classes << current.name;
					if (writable)
					{
						CatalogClass to_add = classFromXml(current.element);
						to_add.parent_id = parent_id;
						QString local_error;
						const int new_id = writable->addClass(to_add, &local_error);
						if (new_id <= 0)
						{
							--local.classes_created;
							local.missing_classes.removeLast();
							local.refused << local_error;
							current.done = true;
							--remaining;
							progress = true;
							continue;
						}
						current.id = new_id;
					}
				}

				for (QDomElement element = current.element.firstChildElement(PROPERTY_TAG) ;
				     !element.isNull() ;
				     element = element.nextSiblingElement(PROPERTY_TAG))
				{
					bool type_known = false;
					CatalogProperty property = propertyFromXml(element, &type_known);
					if (property.key.isEmpty()) {
						continue;
					}
					if (!type_known)
					{
						local.refused << QCoreApplication::translate("CatalogClassPackage",
											    "La propriété %1 de %2 a un type que cette version ne connaît pas.")
								 .arg(property.key, current.name);
						continue;
					}

					bool found = false;
					const CatalogPropertyType in_force =
							effectiveType(index, property.key, &found);
					if (found)
					{
						if (in_force == property.type)
						{
							++local.properties_found;
						}
						else
						{
								//Changing the type of a property
								//reinterprets the value of every part
								//that uses it. Named, not overwritten.
							local.refused << QCoreApplication::translate("CatalogClassPackage",
												    "La propriété %1 existe déjà sur %2 en %3 : elle n'a pas été remplacée par %4.")
									 .arg(property.key, current.name,
									      CatalogProperty::translatedTypeName(in_force),
									      CatalogProperty::translatedTypeName(property.type));
						}
						continue;
					}

					++local.properties_created;
					current.declared << qMakePair(property.key, property.type);
					if (writable)
					{
						property.class_id = current.id;
						QString local_error;
						if (writable->addProperty(property, &local_error) <= 0)
						{
							--local.properties_created;
							current.declared.removeLast();
							local.refused << local_error;
						}
					}
				}

				current.done = true;
				--remaining;
				progress = true;
			}
		}

		for (const FileClass &current : file_classes)
		{
			if (!current.done)
			{
				local.refused << QCoreApplication::translate("CatalogClassPackage",
									    "La classe %1 n'a pas pu être placée : la chaîne au-dessus d'elle est incomplète.")
						 .arg(current.name);
			}
		}

		if (report) {
			*report = local;
		}
		return true;
	}
}

/**
	@brief CatalogClassPackage::Report::changesNothing
	@return true when the target catalog already has all of it
*/
bool CatalogClassPackage::Report::changesNothing() const
{
	return classes_created == 0 && properties_created == 0 && lists_created == 0;
}

/**
	@brief CatalogClassPackage::Report::toText
	@return a human readable summary
*/
QString CatalogClassPackage::Report::toText() const
{
	QStringList lines;
	lines << QCoreApplication::translate("CatalogClassPackage",
					     "Classes : %1 nouvelle(s), %2 déjà présente(s).")
		 .arg(classes_created).arg(classes_found);
	lines << QCoreApplication::translate("CatalogClassPackage",
					     "Propriétés : %1 nouvelle(s), %2 déjà présente(s).")
		 .arg(properties_created).arg(properties_found);
	if (lists_created > 0 || lists_found > 0)
	{
		lines << QCoreApplication::translate("CatalogClassPackage",
						     "Listes contrôlées : %1 nouvelle(s), %2 déjà présente(s).")
			 .arg(lists_created).arg(lists_found);
	}
	if (!missing_classes.isEmpty())
	{
		lines << QCoreApplication::translate("CatalogClassPackage", "À créer : %1")
			 .arg(missing_classes.join(QStringLiteral(", ")));
	}
	if (!refused.isEmpty())
	{
		lines << QCoreApplication::translate("CatalogClassPackage", "Non appliqué :");
		for (const QString &line : refused) {
			lines << QStringLiteral("- ") + line;
		}
	}
	return lines.join(QLatin1Char('\n'));
}

/**
	@brief CatalogClassPackage::fileExtension
	@return the extension of a class package
*/
QString CatalogClassPackage::fileExtension()
{
	return QStringLiteral("qetclasses");
}

/**
	@brief CatalogClassPackage::fileFilter
	@return the file dialog filter
*/
QString CatalogClassPackage::fileFilter()
{
	return QCoreApplication::translate("CatalogClassPackage",
					   "Classes de catalogue QElectroTech (*.qetclasses)");
}

/**
	@brief CatalogClassPackage::blockTagName
	@return the tag name of the block
*/
QString CatalogClassPackage::blockTagName()
{
	return QStringLiteral("qet-catalog-classes");
}

/**
	@brief CatalogClassPackage::suggestedFileName
	@param class_name
	@return a file name derived from the class name
*/
QString CatalogClassPackage::suggestedFileName(const QString &class_name)
{
	QString name = class_name.trimmed();
	static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9._-]+"));
	name.replace(unsafe, QStringLiteral("_"));
	while (name.startsWith(QLatin1Char('_'))) {
		name.remove(0, 1);
	}
	if (name.isEmpty()) {
		name = QStringLiteral("classes");
	}
	return name + QLatin1Char('.') + fileExtension();
}

/**
	@brief CatalogClassPackage::toXml
	@param document
	@param catalog
	@param class_id
	@return the block that describes the branch
*/
QDomElement CatalogClassPackage::toXml(QDomDocument &document,
				       const Catalog &catalog,
				       int class_id,
				       bool include_descendants)
{
	QDomElement root = document.createElement(blockTagName());
	root.setAttribute(QStringLiteral("version"), QStringLiteral("1"));

	const CatalogClass exported = catalog.classById(class_id);
	if (exported.isNull()) {
		return root;
	}
	root.setAttribute(QStringLiteral("root-key"), exported.key);
	root.setAttribute(QStringLiteral("exported-at"),
			  QDateTime::currentDateTime().toString(Qt::ISODate));

		//The ancestry first, so that the branch can be put back where it
		//was, then the branch itself. classAncestry() ends on the class that
		//was asked for, which is why that one is written even when nothing
		//below it is. descendantClassIds() walks breadth first, which is what
		//keeps a parent ahead of its children.
	QList<int> ids = catalog.classAncestry(class_id);
	if (include_descendants)
	{
		const QList<int> below = catalog.descendantClassIds(class_id);
		for (const int id : below)
		{
			if (!ids.contains(id)) {
				ids.append(id);
			}
		}
	}

	QStringList list_names;
	for (const int id : ids)
	{
		const QList<CatalogProperty> own = catalog.ownProperties(id);
		for (const CatalogProperty &property : own)
		{
			if (!property.list_name.isEmpty() && !list_names.contains(property.list_name)) {
				list_names.append(property.list_name);
			}
		}
	}
	for (const QString &list_name : list_names)
	{
		QDomElement list = document.createElement(LIST_TAG);
		list.setAttribute(QStringLiteral("name"), list_name);
		const QStringList values = catalog.listValues(list_name);
		for (const QString &value : values)
		{
			QDomElement child = document.createElement(VALUE_TAG);
			child.appendChild(document.createTextNode(value));
			list.appendChild(child);
		}
		root.appendChild(list);
	}

	for (const int id : ids)
	{
		const CatalogClass catalog_class = catalog.classById(id);
		if (catalog_class.isNull()) {
			continue;
		}
		QDomElement element = document.createElement(CLASS_TAG);
		element.setAttribute(QStringLiteral("key"), catalog_class.key);
		element.setAttribute(QStringLiteral("name"), catalog_class.name);
		const CatalogClass parent = catalog.classById(catalog_class.parent_id);
		if (!parent.isNull()) {
			element.setAttribute(QStringLiteral("parent-key"), parent.key);
		}
		if (!catalog_class.description.isEmpty()) {
			element.setAttribute(QStringLiteral("description"), catalog_class.description);
		}
		if (!catalog_class.root.isEmpty()) {
			element.setAttribute(QStringLiteral("root"), catalog_class.root);
		}
		if (!catalog_class.root_iec.isEmpty()) {
			element.setAttribute(QStringLiteral("root-iec"), catalog_class.root_iec);
		}
		element.setAttribute(QStringLiteral("has-symbol"),
				     catalog_class.has_symbol ? QStringLiteral("1") : QStringLiteral("0"));
		element.setAttribute(QStringLiteral("order"),
				     QString::number(catalog_class.order_index));
			//A trace of where the branch came from, never read back.
		if (!catalog_class.uuid.isEmpty()) {
			element.setAttribute(QStringLiteral("uuid"), catalog_class.uuid);
		}
		if (!catalog_class.numbering_format.isEmpty())
		{
				//Its own node: it is XML of its own, and an attribute
				//holding XML is a file nobody can read.
			QDomElement format = document.createElement(FORMAT_TAG);
			format.appendChild(document.createTextNode(catalog_class.numbering_format));
			element.appendChild(format);
		}

		const QList<CatalogProperty> own = catalog.ownProperties(id);
		for (const CatalogProperty &property : own) {
			element.appendChild(propertyToXml(document, property));
		}
		root.appendChild(element);
	}

	return root;
}

/**
	@brief CatalogClassPackage::applyXml
	@param element
	@param catalog
	@param report
	@param error
	@return true when the block was read
*/
bool CatalogClassPackage::applyXml(const QDomElement &element,
				   Catalog &catalog,
				   Report *report,
				   QString *error)
{
	return applyOrPlan(element, catalog, &catalog, report, error);
}

/**
	@brief CatalogClassPackage::plan
	@param element
	@param catalog
	@return what applyXml() would do
*/
CatalogClassPackage::Report CatalogClassPackage::plan(const QDomElement &element,
						      const Catalog &catalog)
{
	Report report;
	applyOrPlan(element, catalog, nullptr, &report, nullptr);
	return report;
}

/**
	@brief CatalogClassPackage::write
	@param file_path
	@param catalog
	@param class_id
	@param error
	@return true on success
*/
bool CatalogClassPackage::write(const QString &file_path,
				const Catalog &catalog,
				int class_id,
				QString *error)
{
	if (catalog.classById(class_id).isNull())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogClassPackage",
							     "Il n'y a pas de classe à exporter.");
		}
		return false;
	}

	QDomDocument document;
	document.appendChild(toXml(document, catalog, class_id));

	QFile file(file_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogClassPackage",
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
		//Indented on purpose: whoever receives a branch of classes has the
		//right to read what they are accepting before it goes in.
	stream << document.toString(1);
	stream.flush();
	file.close();
	return true;
}

/**
	@brief CatalogClassPackage::read
	@param file_path
	@param catalog
	@param report
	@param error
	@return true when the file was read
*/
bool CatalogClassPackage::read(const QString &file_path,
			       Catalog &catalog,
			       Report *report,
			       QString *error)
{
	const QDomElement root = rootOfFile(file_path, error);
	if (root.isNull()) {
		return false;
	}
	return applyXml(root, catalog, report, error);
}

/**
	@brief CatalogClassPackage::summary
	@param file_path
	@param catalog
	@param error
	@return what read() would do
*/
CatalogClassPackage::Report CatalogClassPackage::summary(const QString &file_path,
							 const Catalog &catalog,
							 QString *error)
{
	const QDomElement root = rootOfFile(file_path, error);
	if (root.isNull()) {
		return Report();
	}
	return plan(root, catalog);
}
