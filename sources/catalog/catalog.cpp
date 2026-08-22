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
#include "catalog.h"

#include "catalogschema.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariantList>

namespace
{
	/// Separator used to store an inline list of options in one text column.
	const QLatin1Char OPTION_SEPARATOR('\n');

	QString isoNow()
	{
		return QDateTime::currentDateTime().toString(Qt::ISODate);
	}
}

/**
	@brief Catalog::Catalog
	@param parent
*/
Catalog::Catalog(QObject *parent) :
	QObject(parent)
{}

/**
	@brief Catalog::~Catalog
*/
Catalog::~Catalog()
{
	close();
}

/**
	@brief Catalog::open
	@param file_path : the catalog file, created when it does not exist
	@param error
	@return true when the catalog is open, at the current schema version and
	has a usable class tree.
*/
bool Catalog::open(const QString &file_path, QString *error)
{
	close();

	const QFileInfo info(file_path);
	const QDir directory = info.absoluteDir();
	if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Impossible de créer le dossier du catalogue : %1")
			 .arg(directory.absolutePath()));
		return false;
	}

	const bool existed = info.exists();
	const bool directory_writable = QFileInfo(directory.absolutePath()).isWritable();

	m_connection_name = QStringLiteral("qet_catalog_")
			    + QUuid::createUuid().toString(QUuid::WithoutBraces);
	m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection_name);
	m_database.setDatabaseName(info.absoluteFilePath());

	if (!m_database.open())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Impossible d'ouvrir le catalogue %1 : %2")
			 .arg(file_path, m_database.lastError().text()));
		close();
		return false;
	}

	m_file_path = info.absoluteFilePath();

	// A catalog on a read only share, or one a colleague opened read only,
	// still has to be usable for drawing. Writing is what gets refused.
	m_writable = existed ? info.isWritable() : directory_writable;

	return finishOpen(error);
}

/**
	@brief Catalog::openInMemory
	@param error
	@return true on success. An in memory catalog is what the test suite uses
	and what a machine with no access to the shared catalog falls back to.
*/
bool Catalog::openInMemory(QString *error)
{
	close();

	m_connection_name = QStringLiteral("qet_catalog_mem_")
			    + QUuid::createUuid().toString(QUuid::WithoutBraces);
	m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection_name);
	m_database.setDatabaseName(QStringLiteral(":memory:"));

	if (!m_database.open())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Impossible d'ouvrir le catalogue en mémoire : %1")
			 .arg(m_database.lastError().text()));
		close();
		return false;
	}

	m_file_path.clear();
	m_writable = true;
	return finishOpen(error);
}

/**
	@brief Catalog::finishOpen
	@param error
	@return true on success. Shared tail of open() and openInMemory() :
	pragmas, migration, caches and first time seeding.
*/
bool Catalog::finishOpen(QString *error)
{
	CatalogSchema::applyConnectionPragmas(m_database);

	QString schema_error;
	if (!CatalogSchema::applyUpTo(m_database, CatalogSchema::currentVersion(), &schema_error))
	{
		setError(error, schema_error);
		close();
		return false;
	}

	m_schema_version = CatalogSchema::versionOf(m_database);

	reloadLists();
	reloadClasses();
	reloadProperties();

	if (m_classes.isEmpty() && m_writable)
	{
		if (!seedDefaultModel(error))
		{
			close();
			return false;
		}
	}

	return true;
}

/**
	@brief Catalog::close
*/
void Catalog::close()
{
	if (m_database.isOpen()) {
		m_database.close();
	}
	m_database = QSqlDatabase();
	if (!m_connection_name.isEmpty())
	{
		QSqlDatabase::removeDatabase(m_connection_name);
		m_connection_name.clear();
	}
	m_file_path.clear();
	m_schema_version = 0;
	m_classes.clear();
	m_properties.clear();
	m_lists.clear();
}

/**
	@brief Catalog::isOpen
	@return true when the catalog is usable
*/
bool Catalog::isOpen() const
{
	return m_database.isOpen();
}

/**
	@brief Catalog::filePath
	@return the catalog file, empty for an in memory catalog
*/
QString Catalog::filePath() const
{
	return m_file_path;
}

/**
	@brief Catalog::lastError
	@return the reason of the last failure
*/
QString Catalog::lastError() const
{
	return m_last_error;
}

/**
	@brief Catalog::schemaVersion
	@return the schema version of the open catalog, 0 when closed
*/
int Catalog::schemaVersion() const
{
	return m_schema_version;
}

/**
	@brief Catalog::isWritable
	@return true when this session may create and change parts
*/
bool Catalog::isWritable() const
{
	return m_writable;
}

/**
	@brief Catalog::setWritable
	@param writable
	Not every draughtsman creates parts. Turning this off makes the catalog
	read only for this session, whatever the file permissions say.
*/
void Catalog::setWritable(bool writable)
{
	m_writable = writable;
}

/**
	@brief Catalog::unitSystem
	@return "mm" or "inch"
*/
QString Catalog::unitSystem() const
{
	if (!isOpen()) {
		return QStringLiteral("mm");
	}
	const QString stored = CatalogSchema::meta(const_cast<QSqlDatabase &>(m_database),
						   QStringLiteral("unit_system"));
	return stored.isEmpty() ? QStringLiteral("mm") : stored;
}

/**
	@brief Catalog::setUnitSystem
	@param system : "mm" or "inch"
	@return true on success
*/
bool Catalog::setUnitSystem(const QString &system)
{
	if (!isOpen() || !m_writable) {
		return false;
	}
	if (system != QStringLiteral("mm") && system != QStringLiteral("inch")) {
		return false;
	}
	return CatalogSchema::setMeta(m_database, QStringLiteral("unit_system"), system);
}

/**
	@brief Catalog::setError
	@param error
	@param message
*/
void Catalog::setError(QString *error, const QString &message) const
{
	m_last_error = message;
	if (error) {
		*error = message;
	}
}

/**
	@brief Catalog::requireWritable
	@param error
	@return true when this session may write
*/
bool Catalog::requireWritable(QString *error) const
{
	if (!isOpen())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Le catalogue n'est pas ouvert."));
		return false;
	}
	if (!m_writable)
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Le catalogue est en lecture seule."));
		return false;
	}
	return true;
}

// -----------------------------------------------------------------------------
// Classes
// -----------------------------------------------------------------------------

/**
	@brief Catalog::reloadClasses
	Read the whole class tree into memory. The tree is small - a few dozen
	rows - and everything else asks it questions constantly, so it is read
	once instead of being queried per component.
*/
void Catalog::reloadClasses()
{
	m_classes.clear();
	if (!isOpen()) {
		return;
	}

	QSqlQuery query(m_database);
	if (!query.exec(QStringLiteral("SELECT id, parent_id, key, name, description, root, "
				       "root_iec, has_symbol, order_index, uuid, numbering_format "
				       "FROM catalog_class ORDER BY order_index, name")))
	{
		m_last_error = query.lastError().text();
		return;
	}

	while (query.next())
	{
		CatalogClass catalog_class;
		catalog_class.id          = query.value(0).toInt();
		catalog_class.parent_id   = query.value(1).toInt();
		catalog_class.key         = query.value(2).toString();
		catalog_class.name        = query.value(3).toString();
		catalog_class.description = query.value(4).toString();
		catalog_class.root        = query.value(5).toString();
		catalog_class.root_iec    = query.value(6).toString();
		catalog_class.has_symbol  = query.value(7).toInt() != 0;
		catalog_class.order_index = query.value(8).toInt();
		catalog_class.uuid        = query.value(9).toString();
		catalog_class.numbering_format = query.value(10).toString();
		m_classes.append(catalog_class);
	}
}

/**
	@brief Catalog::classes
	@return every class of the catalog
*/
QList<CatalogClass> Catalog::classes() const
{
	return m_classes;
}

/**
	@brief Catalog::classById
	@param class_id
	@return the class, a null CatalogClass when there is none
*/
CatalogClass Catalog::classById(int class_id) const
{
	for (const CatalogClass &catalog_class : m_classes)
	{
		if (catalog_class.id == class_id) {
			return catalog_class;
		}
	}
	return CatalogClass();
}

/**
	@brief Catalog::classByKey
	@param key
	@return the class, a null CatalogClass when there is none
*/
CatalogClass Catalog::classByKey(const QString &key) const
{
	for (const CatalogClass &catalog_class : m_classes)
	{
		if (catalog_class.key == key) {
			return catalog_class;
		}
	}
	return CatalogClass();
}

/**
	@brief Catalog::childClasses
	@param parent_id
	@return the direct children of @a parent_id
*/
QList<CatalogClass> Catalog::childClasses(int parent_id) const
{
	QList<CatalogClass> children;
	for (const CatalogClass &catalog_class : m_classes)
	{
		if (catalog_class.parent_id == parent_id) {
			children.append(catalog_class);
		}
	}
	return children;
}

/**
	@brief Catalog::descendantClassIds
	@param class_id
	@return @a class_id and every class below it
*/
QList<int> Catalog::descendantClassIds(int class_id) const
{
	QList<int> ids;
	if (class_id <= 0) {
		return ids;
	}
	ids.append(class_id);

	// Breadth first over the cached tree. The cursor walks the list it is
	// appending to, which is what makes one pass enough.
	for (int index = 0 ; index < ids.size() ; ++index)
	{
		const int current = ids.at(index);
		for (const CatalogClass &catalog_class : m_classes)
		{
			if (catalog_class.parent_id == current && !ids.contains(catalog_class.id)) {
				ids.append(catalog_class.id);
			}
		}
	}
	return ids;
}

/**
	@brief Catalog::classAncestry
	@param class_id
	@return the ancestry of @a class_id, root first, @a class_id last
*/
QList<int> Catalog::classAncestry(int class_id) const
{
	QList<int> ancestry;
	int current = class_id;
	while (current > 0 && !ancestry.contains(current))
	{
		ancestry.prepend(current);
		current = classById(current).parent_id;
	}
	return ancestry;
}

/**
	@brief Catalog::addClass
	@param catalog_class
	@param error
	@return the identifier of the new class, 0 on failure
*/
int Catalog::addClass(const CatalogClass &catalog_class, QString *error)
{
	if (!requireWritable(error)) {
		return 0;
	}

	CatalogClass to_save = catalog_class;
	if (to_save.key.isEmpty()) {
		to_save.key = CatalogClass::keyFromName(to_save.name);
	}
	if (!to_save.isValid(error))
	{
		m_last_error = error ? *error : QString();
		return 0;
	}
	if (!classByKey(to_save.key).isNull())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Une classe avec la clé « %1 » existe déjà.")
			 .arg(to_save.key));
		return 0;
	}
	if (to_save.parent_id > 0 && classById(to_save.parent_id).isNull())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "La classe parente n'existe pas."));
		return 0;
	}
	if (to_save.uuid.isEmpty()) {
		to_save.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("INSERT INTO catalog_class "
				     "(parent_id, key, name, description, root, root_iec, "
				     "has_symbol, order_index, uuid, numbering_format) "
				     "VALUES (:parent_id, :key, :name, :description, :root, "
				     ":root_iec, :has_symbol, :order_index, :uuid, "
				     ":numbering_format)"));
	query.bindValue(QStringLiteral(":parent_id"),
			to_save.parent_id > 0 ? QVariant(to_save.parent_id) : QVariant());
	query.bindValue(QStringLiteral(":key"), to_save.key);
	query.bindValue(QStringLiteral(":name"), to_save.name);
	query.bindValue(QStringLiteral(":description"), to_save.description);
	query.bindValue(QStringLiteral(":root"), to_save.root);
	query.bindValue(QStringLiteral(":root_iec"), to_save.root_iec);
	query.bindValue(QStringLiteral(":has_symbol"), to_save.has_symbol ? 1 : 0);
	query.bindValue(QStringLiteral(":order_index"), to_save.order_index);
	query.bindValue(QStringLiteral(":uuid"), to_save.uuid);
	query.bindValue(QStringLiteral(":numbering_format"), to_save.numbering_format);

	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return 0;
	}

	const int new_id = query.lastInsertId().toInt();
	reloadClasses();
	reloadProperties();
	emit classesChanged();
	return new_id;
}

/**
	@brief Catalog::updateClass
	@param catalog_class
	@param error
	@return true on success. Changing the root of a class changes the
	numbering of every object of that class, past and future alike - that is
	the point of the root living on the class.
*/
bool Catalog::updateClass(const CatalogClass &catalog_class, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}
	if (catalog_class.id <= 0)
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "La classe n'a pas encore été enregistrée."));
		return false;
	}
	if (!catalog_class.isValid(error)) {
		return false;
	}

	// Refuse a cycle: a class cannot become its own descendant.
	if (catalog_class.parent_id > 0)
	{
		const QList<int> below = descendantClassIds(catalog_class.id);
		if (below.contains(catalog_class.parent_id))
		{
			setError(error, QCoreApplication::translate("Catalog",
								    "Une classe ne peut pas devenir la fille d'une de ses propres sous-classes."));
			return false;
		}
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("UPDATE catalog_class SET parent_id = :parent_id, "
				     "key = :key, name = :name, description = :description, "
				     "root = :root, root_iec = :root_iec, "
				     "has_symbol = :has_symbol, order_index = :order_index, "
				     "numbering_format = :numbering_format "
				     "WHERE id = :id"));
	query.bindValue(QStringLiteral(":parent_id"),
			catalog_class.parent_id > 0 ? QVariant(catalog_class.parent_id) : QVariant());
	query.bindValue(QStringLiteral(":key"), catalog_class.key);
	query.bindValue(QStringLiteral(":name"), catalog_class.name);
	query.bindValue(QStringLiteral(":description"), catalog_class.description);
	query.bindValue(QStringLiteral(":root"), catalog_class.root);
	query.bindValue(QStringLiteral(":root_iec"), catalog_class.root_iec);
	query.bindValue(QStringLiteral(":has_symbol"), catalog_class.has_symbol ? 1 : 0);
	query.bindValue(QStringLiteral(":order_index"), catalog_class.order_index);
	query.bindValue(QStringLiteral(":numbering_format"), catalog_class.numbering_format);
	query.bindValue(QStringLiteral(":id"), catalog_class.id);

	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return false;
	}

	reloadClasses();
	emit classesChanged();
	return true;
}

/**
	@brief Catalog::removeClass
	@param class_id
	@param error
	@return true on success. A class holding parts or subclasses is refused
	instead of taking them down with it.
*/
bool Catalog::removeClass(int class_id, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}
	if (!childClasses(class_id).isEmpty())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Cette classe a des sous-classes : supprimez-les d'abord."));
		return false;
	}

	QSqlQuery count(m_database);
	count.prepare(QStringLiteral("SELECT COUNT(*) FROM catalog_part WHERE class_id = :id"));
	count.bindValue(QStringLiteral(":id"), class_id);
	if (count.exec() && count.next() && count.value(0).toInt() > 0)
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Cette classe contient %1 pièce(s) : supprimez-les ou déplacez-les d'abord.")
			 .arg(count.value(0).toInt()));
		return false;
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("DELETE FROM catalog_class WHERE id = :id"));
	query.bindValue(QStringLiteral(":id"), class_id);
	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return false;
	}

	reloadClasses();
	reloadProperties();
	emit classesChanged();
	return true;
}

/**
	@brief Catalog::tagRoot
	@param class_id
	@param iec
	@return the tag letter of @a class_id, inherited from the closest
	ancestor that declares one
*/
QString Catalog::tagRoot(int class_id, bool iec) const
{
	const QList<int> ancestry = classAncestry(class_id);
	// Walk from the class up to the root and keep the first root declared.
	for (int index = ancestry.size() - 1 ; index >= 0 ; --index)
	{
		const CatalogClass catalog_class = classById(ancestry.at(index));
		const QString root = iec ? catalog_class.root_iec : catalog_class.root;
		if (!root.isEmpty()) {
			return root;
		}
	}
	return QString();
}

// -----------------------------------------------------------------------------
// Properties
// -----------------------------------------------------------------------------

/**
	@brief Catalog::reloadProperties
	Read every property declaration into memory, resolving the values of the
	controlled lists as it goes so that callers never have to.
*/
void Catalog::reloadProperties()
{
	m_properties.clear();
	if (!isOpen()) {
		return;
	}

	QSqlQuery query(m_database);
	if (!query.exec(QStringLiteral("SELECT id, class_id, key, name, type, list_behaviour, "
				       "list_name, options, default_value, unit, description, "
				       "order_index FROM catalog_property "
				       "ORDER BY class_id, order_index, name")))
	{
		m_last_error = query.lastError().text();
		return;
	}

	while (query.next())
	{
		CatalogProperty property;
		property.id             = query.value(0).toInt();
		property.class_id       = query.value(1).toInt();
		property.key            = query.value(2).toString();
		property.name           = query.value(3).toString();
		property.type           = CatalogProperty::typeFromString(query.value(4).toString());
		property.list_behaviour = CatalogProperty::listBehaviourFromString(query.value(5).toString());
		property.list_name      = query.value(6).toString();
		property.default_value  = query.value(8).toString();
		property.unit           = query.value(9).toString();
		property.description    = query.value(10).toString();
		property.order_index    = query.value(11).toInt();

		const QString inline_options = query.value(7).toString();
		if (!property.list_name.isEmpty()) {
			property.options = m_lists.value(property.list_name);
		} else if (!inline_options.isEmpty()) {
			property.options = inline_options.split(OPTION_SEPARATOR, Qt::SkipEmptyParts);
		}

		m_properties[property.class_id].append(property);
	}
}

/**
	@brief Catalog::ownProperties
	@param class_id
	@return the properties declared on @a class_id itself
*/
QList<CatalogProperty> Catalog::ownProperties(int class_id) const
{
	return m_properties.value(class_id);
}

/**
	@brief Catalog::isDescendantOf
	@param class_id
	@param ancestor_key
	@return true when @a class_id is @a ancestor_key or descends from it
*/
bool Catalog::isDescendantOf(int class_id, const QString &ancestor_key) const
{
	if (ancestor_key.isEmpty()) {
		return false;
	}
		//Walked with a step limit rather than trusted: a parent_id cycle in a
		//hand edited catalog would otherwise hang the program, and a hang is
		//harder to diagnose than a wrong answer.
	int id = class_id;
	for (int step = 0 ; step < 64 && id > 0 ; ++step)
	{
		const CatalogClass current = classById(id);
		if (current.id <= 0) {
			return false;
		}
		if (current.key == ancestor_key) {
			return true;
		}
		id = current.parent_id;
	}
	return false;
}

/**
	@brief Catalog::effectiveProperties
	@param class_id
	@return the properties of @a class_id, the inherited ones first.

	This is the function that makes the model work: adding a property to a
	parent class makes it appear on every subclass and on every part that
	already exists, because nobody stores a copy of the declaration - it is
	resolved here, every time it is asked for.
*/
QList<CatalogProperty> Catalog::effectiveProperties(int class_id) const
{
	QList<CatalogProperty> properties;
	QStringList seen_keys;

	const QList<int> ancestry = classAncestry(class_id);
	for (const int ancestor : ancestry)
	{
		const QList<CatalogProperty> own = m_properties.value(ancestor);
		for (const CatalogProperty &property : own)
		{
			// A subclass declaring a key its parent already declares is
			// refused by addProperty(); should an older catalog carry
			// one anyway, the closest declaration wins.
			const int existing = seen_keys.indexOf(property.key);
			if (existing >= 0) {
				properties[existing] = property;
			} else {
				seen_keys.append(property.key);
				properties.append(property);
			}
		}
	}
	return properties;
}

/**
	@brief Catalog::effectiveProperty
	@param class_id
	@param key
	@return the property @a key of @a class_id, inherited or own, a null
	CatalogProperty when the class has no such property
*/
CatalogProperty Catalog::effectiveProperty(int class_id, const QString &key) const
{
	const QList<CatalogProperty> properties = effectiveProperties(class_id);
	for (const CatalogProperty &property : properties)
	{
		if (property.key == key) {
			return property;
		}
	}
	return CatalogProperty();
}

/**
	@brief Catalog::addProperty
	@param property
	@param error
	@return the identifier of the new property, 0 on failure
*/
int Catalog::addProperty(const CatalogProperty &property, QString *error)
{
	if (!requireWritable(error)) {
		return 0;
	}

	CatalogProperty to_save = property;
	if (to_save.key.isEmpty()) {
		to_save.key = CatalogProperty::keyFromName(to_save.name);
	}
	if (to_save.list_behaviour != CatalogListBehaviour::None
	    && !to_save.list_name.isEmpty())
	{
		to_save.options = m_lists.value(to_save.list_name);
	}
	if (!to_save.isValid(error)) {
		return 0;
	}
	if (classById(to_save.class_id).isNull())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "La classe de la propriété n'existe pas."));
		return 0;
	}
	if (!effectiveProperty(to_save.class_id, to_save.key).isNull())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "La propriété « %1 » existe déjà sur cette classe ou sur une de ses classes mères.")
			 .arg(to_save.key));
		return 0;
	}

	if (to_save.order_index == 0)
	{
		// Land after the properties already declared on this class.
		const QList<CatalogProperty> own = ownProperties(to_save.class_id);
		int highest = 0;
		for (const CatalogProperty &existing : own) {
			highest = qMax(highest, existing.order_index);
		}
		to_save.order_index = highest + 1;
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("INSERT INTO catalog_property "
				     "(class_id, key, name, type, list_behaviour, list_name, "
				     "options, default_value, unit, description, order_index) "
				     "VALUES (:class_id, :key, :name, :type, :list_behaviour, "
				     ":list_name, :options, :default_value, :unit, :description, "
				     ":order_index)"));
	query.bindValue(QStringLiteral(":class_id"), to_save.class_id);
	query.bindValue(QStringLiteral(":key"), to_save.key);
	query.bindValue(QStringLiteral(":name"), to_save.name);
	query.bindValue(QStringLiteral(":type"), CatalogProperty::typeToString(to_save.type));
	query.bindValue(QStringLiteral(":list_behaviour"),
			CatalogProperty::listBehaviourToString(to_save.list_behaviour));
	query.bindValue(QStringLiteral(":list_name"), to_save.list_name);
	query.bindValue(QStringLiteral(":options"),
			to_save.list_name.isEmpty()
			? to_save.options.join(OPTION_SEPARATOR)
			: QString());
	query.bindValue(QStringLiteral(":default_value"), to_save.default_value);
	query.bindValue(QStringLiteral(":unit"), to_save.unit);
	query.bindValue(QStringLiteral(":description"), to_save.description);
	query.bindValue(QStringLiteral(":order_index"), to_save.order_index);

	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return 0;
	}

	const int new_id = query.lastInsertId().toInt();
	reloadProperties();
	emit propertiesChanged(to_save.class_id);
	return new_id;
}

/**
	@brief Catalog::updateProperty
	@param property
	@param error
	@return true on success
*/
bool Catalog::updateProperty(const CatalogProperty &property, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}
	if (property.id <= 0)
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "La propriété n'a pas encore été enregistrée."));
		return false;
	}

	CatalogProperty to_save = property;
	if (to_save.list_behaviour != CatalogListBehaviour::None
	    && !to_save.list_name.isEmpty())
	{
		to_save.options = m_lists.value(to_save.list_name);
	}
	if (!to_save.isValid(error)) {
		return false;
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("UPDATE catalog_property SET key = :key, name = :name, "
				     "type = :type, list_behaviour = :list_behaviour, "
				     "list_name = :list_name, options = :options, "
				     "default_value = :default_value, unit = :unit, "
				     "description = :description, order_index = :order_index "
				     "WHERE id = :id"));
	query.bindValue(QStringLiteral(":key"), to_save.key);
	query.bindValue(QStringLiteral(":name"), to_save.name);
	query.bindValue(QStringLiteral(":type"), CatalogProperty::typeToString(to_save.type));
	query.bindValue(QStringLiteral(":list_behaviour"),
			CatalogProperty::listBehaviourToString(to_save.list_behaviour));
	query.bindValue(QStringLiteral(":list_name"), to_save.list_name);
	query.bindValue(QStringLiteral(":options"),
			to_save.list_name.isEmpty()
			? to_save.options.join(OPTION_SEPARATOR)
			: QString());
	query.bindValue(QStringLiteral(":default_value"), to_save.default_value);
	query.bindValue(QStringLiteral(":unit"), to_save.unit);
	query.bindValue(QStringLiteral(":description"), to_save.description);
	query.bindValue(QStringLiteral(":order_index"), to_save.order_index);
	query.bindValue(QStringLiteral(":id"), to_save.id);

	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return false;
	}

	reloadProperties();
	emit propertiesChanged(to_save.class_id);
	return true;
}

/**
	@brief Catalog::removeProperty
	@param property_id
	@param error
	@return true on success. The values the parts had for that property stay
	in the database on purpose: a property removed by mistake and declared
	again finds its values back. Nothing reads them meanwhile.
*/
bool Catalog::removeProperty(int property_id, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}

	int class_id = 0;
	const QList<int> class_ids = m_properties.keys();
	for (const int candidate : class_ids)
	{
		const QList<CatalogProperty> own = m_properties.value(candidate);
		for (const CatalogProperty &property : own)
		{
			if (property.id == property_id) {
				class_id = candidate;
			}
		}
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("DELETE FROM catalog_property WHERE id = :id"));
	query.bindValue(QStringLiteral(":id"), property_id);
	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return false;
	}

	reloadProperties();
	emit propertiesChanged(class_id);
	return true;
}

/**
	@brief Catalog::setPropertyOrder
	@param class_id
	@param property_ids : the properties of @a class_id, in the wanted order
	@param error
	@return true on success. The order of the properties is the order of the
	columns in the part lists, which is why the user can drag them.
*/
bool Catalog::setPropertyOrder(int class_id, const QList<int> &property_ids, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}

	m_database.transaction();
	int order_index = 1;
	for (const int property_id : property_ids)
	{
		QSqlQuery query(m_database);
		query.prepare(QStringLiteral("UPDATE catalog_property SET order_index = :order_index "
					     "WHERE id = :id AND class_id = :class_id"));
		query.bindValue(QStringLiteral(":order_index"), order_index);
		query.bindValue(QStringLiteral(":id"), property_id);
		query.bindValue(QStringLiteral(":class_id"), class_id);
		if (!query.exec())
		{
			setError(error, query.lastError().text());
			m_database.rollback();
			return false;
		}
		++order_index;
	}
	m_database.commit();

	reloadProperties();
	emit propertiesChanged(class_id);
	return true;
}

/**
	@brief Catalog::applyDefaultToExistingParts
	@param property_id
	@param error
	@return how many parts were touched, -1 on failure
*/
int Catalog::applyDefaultToExistingParts(int property_id, QString *error)
{
	if (!requireWritable(error)) {
		return -1;
	}

	CatalogProperty property;
	const QList<int> class_ids = m_properties.keys();
	for (const int candidate : class_ids)
	{
		const QList<CatalogProperty> own = m_properties.value(candidate);
		for (const CatalogProperty &own_property : own)
		{
			if (own_property.id == property_id) {
				property = own_property;
			}
		}
	}
	if (property.isNull())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Propriété inconnue."));
		return -1;
	}

	QStringList placeholders;
	const QList<int> class_ids_below = descendantClassIds(property.class_id);
	for (const int class_id : class_ids_below) {
		placeholders.append(QString::number(class_id));
	}
	if (placeholders.isEmpty()) {
		return 0;
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("INSERT INTO catalog_part_value (part_id, key, value) "
				     "SELECT p.id, :key, :value FROM catalog_part p "
				     "WHERE p.class_id IN (%1) AND NOT EXISTS "
				     "(SELECT 1 FROM catalog_part_value v "
				     "WHERE v.part_id = p.id AND v.key = :key2)")
		      .arg(placeholders.join(QLatin1Char(','))));
	query.bindValue(QStringLiteral(":key"), property.key);
	query.bindValue(QStringLiteral(":value"), property.default_value);
	query.bindValue(QStringLiteral(":key2"), property.key);

	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return -1;
	}

	const int touched = query.numRowsAffected();
	if (touched > 0) {
		emit partsChanged();
	}
	return touched;
}

// -----------------------------------------------------------------------------
// Controlled lists
// -----------------------------------------------------------------------------

/**
	@brief Catalog::reloadLists
*/
void Catalog::reloadLists()
{
	m_lists.clear();
	if (!isOpen()) {
		return;
	}

	QSqlQuery query(m_database);
	if (!query.exec(QStringLiteral("SELECT l.name, v.value FROM catalog_list l "
				       "LEFT JOIN catalog_list_value v ON v.list_id = l.id "
				       "ORDER BY l.name, v.order_index, v.value")))
	{
		m_last_error = query.lastError().text();
		return;
	}

	while (query.next())
	{
		const QString name = query.value(0).toString();
		if (!m_lists.contains(name)) {
			m_lists.insert(name, QStringList());
		}
		const QString value = query.value(1).toString();
		if (!value.isEmpty()) {
			m_lists[name].append(value);
		}
	}
}

/**
	@brief Catalog::listNames
	@return the name of every controlled list
*/
QStringList Catalog::listNames() const
{
	QStringList names = m_lists.keys();
	names.sort();
	return names;
}

/**
	@brief Catalog::listValues
	@param list_name
	@return the values of @a list_name, in their order
*/
QStringList Catalog::listValues(const QString &list_name) const
{
	return m_lists.value(list_name);
}

/**
	@brief Catalog::listIdForName
	@param list_name
	@return the identifier of @a list_name, 0 when there is none
*/
int Catalog::listIdForName(const QString &list_name) const
{
	QSqlQuery query(const_cast<QSqlDatabase &>(m_database));
	query.prepare(QStringLiteral("SELECT id FROM catalog_list WHERE name = :name"));
	query.bindValue(QStringLiteral(":name"), list_name);
	if (query.exec() && query.next()) {
		return query.value(0).toInt();
	}
	return 0;
}

/**
	@brief Catalog::setListValues
	@param list_name : created when it does not exist yet
	@param values
	@param error
	@return true on success. One place holds the manufacturers, so the same
	manufacturer spelled three ways inside one project stops happening.
*/
bool Catalog::setListValues(const QString &list_name,
			    const QStringList &values,
			    QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}
	if (list_name.isEmpty())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "La liste doit avoir un nom."));
		return false;
	}

	m_database.transaction();

	int list_id = listIdForName(list_name);
	if (list_id == 0)
	{
		QSqlQuery insert(m_database);
		insert.prepare(QStringLiteral("INSERT INTO catalog_list (name) VALUES (:name)"));
		insert.bindValue(QStringLiteral(":name"), list_name);
		if (!insert.exec())
		{
			setError(error, insert.lastError().text());
			m_database.rollback();
			return false;
		}
		list_id = insert.lastInsertId().toInt();
	}
	else
	{
		QSqlQuery clear(m_database);
		clear.prepare(QStringLiteral("DELETE FROM catalog_list_value WHERE list_id = :id"));
		clear.bindValue(QStringLiteral(":id"), list_id);
		if (!clear.exec())
		{
			setError(error, clear.lastError().text());
			m_database.rollback();
			return false;
		}
	}

	int order_index = 1;
	for (const QString &value : values)
	{
		if (value.isEmpty()) {
			continue;
		}
		QSqlQuery insert(m_database);
		insert.prepare(QStringLiteral("INSERT OR IGNORE INTO catalog_list_value "
					      "(list_id, value, order_index) "
					      "VALUES (:list_id, :value, :order_index)"));
		insert.bindValue(QStringLiteral(":list_id"), list_id);
		insert.bindValue(QStringLiteral(":value"), value);
		insert.bindValue(QStringLiteral(":order_index"), order_index);
		if (!insert.exec())
		{
			setError(error, insert.lastError().text());
			m_database.rollback();
			return false;
		}
		++order_index;
	}

	m_database.commit();

	reloadLists();
	reloadProperties();    // the properties bound to this list carry its values
	emit listsChanged();
	return true;
}

/**
	@brief Catalog::removeList
	@param list_name
	@param error
	@return true on success
*/
bool Catalog::removeList(const QString &list_name, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("DELETE FROM catalog_list WHERE name = :name"));
	query.bindValue(QStringLiteral(":name"), list_name);
	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return false;
	}

	reloadLists();
	reloadProperties();
	emit listsChanged();
	return true;
}

// -----------------------------------------------------------------------------
// Parts
// -----------------------------------------------------------------------------

/**
	@brief Catalog::partsFromQuery
	@param where : the WHERE clause, without the keyword
	@param bindings : positional bindings of @a where
	@return the parts the clause selects, fully read
*/
QList<CatalogPart> Catalog::partsFromQuery(const QString &where,
					   const QVariantList &bindings) const
{
	QList<CatalogPart> found;
	if (!isOpen()) {
		return found;
	}

	QString statement = QStringLiteral("SELECT id FROM catalog_part");
	if (!where.isEmpty()) {
		statement += QStringLiteral(" WHERE ") + where;
	}
	statement += QStringLiteral(" ORDER BY code, revision");

	QSqlQuery query(const_cast<QSqlDatabase &>(m_database));
	query.prepare(statement);
	for (const QVariant &binding : bindings) {
		query.addBindValue(binding);
	}
	if (!query.exec())
	{
		m_last_error = query.lastError().text();
		return found;
	}

	QList<int> ids;
	while (query.next()) {
		ids.append(query.value(0).toInt());
	}
	for (const int id : ids) {
		found.append(readPart(id));
	}
	return found;
}

/**
	@brief Catalog::readPart
	@param part_id
	@return the part with its values, pins and accessories
*/
CatalogPart Catalog::readPart(int part_id) const
{
	CatalogPart part;
	if (!isOpen() || part_id <= 0) {
		return part;
	}

	QSqlDatabase &database = const_cast<QSqlDatabase &>(m_database);

	QSqlQuery head(database);
	head.prepare(QStringLiteral("SELECT id, class_id, code, revision, is_current, "
				    "origin, origin_date FROM catalog_part WHERE id = :id"));
	head.bindValue(QStringLiteral(":id"), part_id);
	if (!head.exec() || !head.next()) {
		return part;
	}

	part.id          = head.value(0).toInt();
	part.class_id    = head.value(1).toInt();
	part.code        = head.value(2).toString();
	part.revision    = head.value(3).toInt();
	part.is_current  = head.value(4).toInt() != 0;
	part.origin      = head.value(5).toString();
	part.origin_date = head.value(6).toString();

	QSqlQuery values(database);
	values.prepare(QStringLiteral("SELECT key, value FROM catalog_part_value "
				      "WHERE part_id = :id"));
	values.bindValue(QStringLiteral(":id"), part_id);
	if (values.exec())
	{
		while (values.next()) {
			part.values.insert(values.value(0).toString(), values.value(1).toString());
		}
	}

	QSqlQuery pins(database);
	pins.prepare(QStringLiteral("SELECT label, role, pair, group_name, order_index "
				    "FROM catalog_part_pin WHERE part_id = :id "
				    "ORDER BY order_index, id"));
	pins.bindValue(QStringLiteral(":id"), part_id);
	if (pins.exec())
	{
		while (pins.next())
		{
			CatalogPin pin;
			pin.label       = pins.value(0).toString();
			pin.role        = CatalogPin::roleFromString(pins.value(1).toString());
			pin.pair        = pins.value(2).toString();
			pin.group       = pins.value(3).toString();
			pin.order_index = pins.value(4).toInt();
			part.pins.append(pin);
		}
	}

	QSqlQuery accessories(database);
	accessories.prepare(QStringLiteral("SELECT accessory_code, quantity "
					   "FROM catalog_part_accessory WHERE part_id = :id "
					   "ORDER BY accessory_code"));
	accessories.bindValue(QStringLiteral(":id"), part_id);
	if (accessories.exec())
	{
		while (accessories.next())
		{
			part.accessories.append(
				CatalogAccessory(accessories.value(0).toString(),
						 accessories.value(1).toDouble()));
		}
	}

	return part;
}

/**
	@brief Catalog::parts
	@param class_id : 0 for every class
	@param with_subclasses
	@return the current revision of every part of the asked classes
*/
QList<CatalogPart> Catalog::parts(int class_id, bool with_subclasses) const
{
	if (class_id <= 0) {
		return partsFromQuery(QStringLiteral("is_current = 1"), QVariantList());
	}

	QStringList ids;
	if (with_subclasses)
	{
		const QList<int> class_ids = descendantClassIds(class_id);
		for (const int id : class_ids) {
			ids.append(QString::number(id));
		}
	}
	else
	{
		ids.append(QString::number(class_id));
	}

	return partsFromQuery(QStringLiteral("is_current = 1 AND class_id IN (%1)")
			      .arg(ids.join(QLatin1Char(','))),
			      QVariantList());
}

/**
	@brief Catalog::part
	@param part_id
	@return the part, a null CatalogPart when there is none
*/
CatalogPart Catalog::part(int part_id) const
{
	return readPart(part_id);
}

/**
	@brief Catalog::partByCode
	@param code
	@return the current revision of @a code
*/
CatalogPart Catalog::partByCode(const QString &code) const
{
	const QList<CatalogPart> found =
		partsFromQuery(QStringLiteral("code = ? AND is_current = 1"),
			       QVariantList() << code);
	return found.isEmpty() ? CatalogPart() : found.first();
}

/**
	@brief Catalog::partByCode
	@param code
	@param revision
	@return exactly revision @a revision of @a code. A project that pinned a
	revision keeps reading that one, which is what protects a delivered
	project from a part that changed afterwards.
*/
CatalogPart Catalog::partByCode(const QString &code, int revision) const
{
	const QList<CatalogPart> found =
		partsFromQuery(QStringLiteral("code = ? AND revision = ?"),
			       QVariantList() << code << revision);
	return found.isEmpty() ? CatalogPart() : found.first();
}

/**
	@brief Catalog::partRevisions
	@param code
	@return every revision of @a code, ascending
*/
QList<int> Catalog::partRevisions(const QString &code) const
{
	QList<int> revisions;
	if (!isOpen()) {
		return revisions;
	}

	QSqlQuery query(const_cast<QSqlDatabase &>(m_database));
	query.prepare(QStringLiteral("SELECT revision FROM catalog_part WHERE code = :code "
				     "ORDER BY revision"));
	query.bindValue(QStringLiteral(":code"), code);
	if (query.exec())
	{
		while (query.next()) {
			revisions.append(query.value(0).toInt());
		}
	}
	return revisions;
}

/**
	@brief Catalog::partCount
	@return how many parts the catalog holds, counting the current revision
	of each only
*/
int Catalog::partCount() const
{
	if (!isOpen()) {
		return 0;
	}
	QSqlQuery query(const_cast<QSqlDatabase &>(m_database));
	if (query.exec(QStringLiteral("SELECT COUNT(*) FROM catalog_part WHERE is_current = 1"))
	    && query.next())
	{
		return query.value(0).toInt();
	}
	return 0;
}

/**
	@brief Catalog::validatePartValues
	@param part
	@param error
	@return true when every value of @a part is allowed by the property that
	declares it. Only a mandatory list refuses; a suggested one lets the
	value through, and the interface highlights it.
*/
bool Catalog::validatePartValues(const CatalogPart &part, QString *error) const
{
	const QList<CatalogProperty> properties = effectiveProperties(part.class_id);
	for (const CatalogProperty &property : properties)
	{
		if (!part.hasValue(property.key)) {
			continue;
		}
		const QString value = part.value(property.key);
		if (!property.acceptsValue(value))
		{
			setError(error, QCoreApplication::translate("Catalog",
								    "« %1 » n'est pas une valeur autorisée pour « %2 ».")
				 .arg(value, property.name));
			return false;
		}
	}
	return true;
}

/**
	@brief Catalog::writePartRows
	@param part : has to carry a valid id
	@param error
	@return true on success. Rewrites the values, the pins and the
	accessories of a part wholesale, which is both simpler and safer than
	working out what changed.
*/
bool Catalog::writePartRows(const CatalogPart &part, QString *error)
{
	const QStringList tables = { QStringLiteral("catalog_part_value"),
				     QStringLiteral("catalog_part_pin"),
				     QStringLiteral("catalog_part_accessory") };
	for (const QString &table : tables)
	{
		QSqlQuery clear(m_database);
		clear.prepare(QStringLiteral("DELETE FROM %1 WHERE part_id = :id").arg(table));
		clear.bindValue(QStringLiteral(":id"), part.id);
		if (!clear.exec())
		{
			setError(error, clear.lastError().text());
			return false;
		}
	}

	const QStringList keys = part.values.keys();
	for (const QString &key : keys)
	{
		QSqlQuery insert(m_database);
		insert.prepare(QStringLiteral("INSERT INTO catalog_part_value (part_id, key, value) "
					      "VALUES (:id, :key, :value)"));
		insert.bindValue(QStringLiteral(":id"), part.id);
		insert.bindValue(QStringLiteral(":key"), key);
		insert.bindValue(QStringLiteral(":value"), part.values.value(key));
		if (!insert.exec())
		{
			setError(error, insert.lastError().text());
			return false;
		}
	}

	int pin_order = 1;
	for (const CatalogPin &pin : part.pins)
	{
		QSqlQuery insert(m_database);
		insert.prepare(QStringLiteral("INSERT INTO catalog_part_pin "
					      "(part_id, label, role, pair, group_name, order_index) "
					      "VALUES (:id, :label, :role, :pair, :group_name, :order_index)"));
		insert.bindValue(QStringLiteral(":id"), part.id);
		insert.bindValue(QStringLiteral(":label"), pin.label);
		insert.bindValue(QStringLiteral(":role"), CatalogPin::roleToString(pin.role));
		insert.bindValue(QStringLiteral(":pair"), pin.pair);
		insert.bindValue(QStringLiteral(":group_name"), pin.group);
		insert.bindValue(QStringLiteral(":order_index"),
				 pin.order_index > 0 ? pin.order_index : pin_order);
		if (!insert.exec())
		{
			setError(error, insert.lastError().text());
			return false;
		}
		++pin_order;
	}

	for (const CatalogAccessory &accessory : part.accessories)
	{
		QSqlQuery insert(m_database);
		insert.prepare(QStringLiteral("INSERT OR REPLACE INTO catalog_part_accessory "
					      "(part_id, accessory_code, quantity) "
					      "VALUES (:id, :code, :quantity)"));
		insert.bindValue(QStringLiteral(":id"), part.id);
		insert.bindValue(QStringLiteral(":code"), accessory.code);
		insert.bindValue(QStringLiteral(":quantity"), accessory.quantity);
		if (!insert.exec())
		{
			setError(error, insert.lastError().text());
			return false;
		}
	}

	return true;
}

/**
	@brief Catalog::savePart
	@param part : gets its id and revision filled in on success
	@param error
	@return true on success. Saving in place is the answer to "the record was
	wrong": every project that uses this part sees the correction next time
	it is opened.
*/
bool Catalog::savePart(CatalogPart &part, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}
	if (!part.isValid(error)) {
		return false;
	}
	if (classById(part.class_id).isNull())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "La classe de la pièce n'existe pas."));
		return false;
	}
	if (!validatePartValues(part, error)) {
		return false;
	}

	part.code = part.code.trimmed();
	m_database.transaction();

	if (part.id == 0)
	{
		// An existing code without an id means "save this revision again":
		// find it instead of colliding with the unique constraint.
		const CatalogPart existing = partByCode(part.code, part.revision);
		if (!existing.isNull()) {
			part.id = existing.id;
		}
	}

	if (part.id == 0)
	{
		QSqlQuery insert(m_database);
		insert.prepare(QStringLiteral("INSERT INTO catalog_part "
					      "(class_id, code, revision, is_current, origin, "
					      "origin_date, created_at, updated_at) "
					      "VALUES (:class_id, :code, :revision, 1, :origin, "
					      ":origin_date, :created_at, :updated_at)"));
		insert.bindValue(QStringLiteral(":class_id"), part.class_id);
		insert.bindValue(QStringLiteral(":code"), part.code);
		insert.bindValue(QStringLiteral(":revision"), part.revision);
		insert.bindValue(QStringLiteral(":origin"), part.origin);
		insert.bindValue(QStringLiteral(":origin_date"), part.origin_date);
		insert.bindValue(QStringLiteral(":created_at"), isoNow());
		insert.bindValue(QStringLiteral(":updated_at"), isoNow());
		if (!insert.exec())
		{
			setError(error, insert.lastError().text());
			m_database.rollback();
			return false;
		}
		part.id = insert.lastInsertId().toInt();
		part.is_current = true;
	}
	else
	{
		QSqlQuery update(m_database);
		update.prepare(QStringLiteral("UPDATE catalog_part SET class_id = :class_id, "
					      "code = :code, origin = :origin, "
					      "origin_date = :origin_date, updated_at = :updated_at "
					      "WHERE id = :id"));
		update.bindValue(QStringLiteral(":class_id"), part.class_id);
		update.bindValue(QStringLiteral(":code"), part.code);
		update.bindValue(QStringLiteral(":origin"), part.origin);
		update.bindValue(QStringLiteral(":origin_date"), part.origin_date);
		update.bindValue(QStringLiteral(":updated_at"), isoNow());
		update.bindValue(QStringLiteral(":id"), part.id);
		if (!update.exec())
		{
			setError(error, update.lastError().text());
			m_database.rollback();
			return false;
		}
	}

	if (!writePartRows(part, error))
	{
		m_database.rollback();
		return false;
	}

	m_database.commit();
	emit partsChanged();
	return true;
}

/**
	@brief Catalog::savePartAsNewRevision
	@param part : gets its id and revision filled in on success
	@param error
	@return true on success
*/
bool Catalog::savePartAsNewRevision(CatalogPart &part, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}
	if (!part.isValid(error)) {
		return false;
	}
	if (!validatePartValues(part, error)) {
		return false;
	}

	part.code = part.code.trimmed();
	const QList<int> revisions = partRevisions(part.code);
	if (revisions.isEmpty())
	{
		// Nothing to supersede: this is simply the first revision.
		part.id = 0;
		part.revision = 1;
		return savePart(part, error);
	}

	m_database.transaction();

	QSqlQuery clear(m_database);
	clear.prepare(QStringLiteral("UPDATE catalog_part SET is_current = 0 WHERE code = :code"));
	clear.bindValue(QStringLiteral(":code"), part.code);
	if (!clear.exec())
	{
		setError(error, clear.lastError().text());
		m_database.rollback();
		return false;
	}

	part.revision = revisions.last() + 1;
	part.is_current = true;

	QSqlQuery insert(m_database);
	insert.prepare(QStringLiteral("INSERT INTO catalog_part "
				      "(class_id, code, revision, is_current, origin, "
				      "origin_date, created_at, updated_at) "
				      "VALUES (:class_id, :code, :revision, 1, :origin, "
				      ":origin_date, :created_at, :updated_at)"));
	insert.bindValue(QStringLiteral(":class_id"), part.class_id);
	insert.bindValue(QStringLiteral(":code"), part.code);
	insert.bindValue(QStringLiteral(":revision"), part.revision);
	insert.bindValue(QStringLiteral(":origin"), part.origin);
	insert.bindValue(QStringLiteral(":origin_date"), part.origin_date);
	insert.bindValue(QStringLiteral(":created_at"), isoNow());
	insert.bindValue(QStringLiteral(":updated_at"), isoNow());
	if (!insert.exec())
	{
		setError(error, insert.lastError().text());
		m_database.rollback();
		return false;
	}
	part.id = insert.lastInsertId().toInt();

	if (!writePartRows(part, error))
	{
		m_database.rollback();
		return false;
	}

	m_database.commit();
	emit partsChanged();
	return true;
}

/**
	@brief Catalog::removePart
	@param part_id
	@param error
	@return true on success
*/
bool Catalog::removePart(int part_id, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}

	const CatalogPart doomed = readPart(part_id);
	if (doomed.isNull())
	{
		setError(error, QCoreApplication::translate("Catalog", "Pièce inconnue."));
		return false;
	}

	m_database.transaction();

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("DELETE FROM catalog_part WHERE id = :id"));
	query.bindValue(QStringLiteral(":id"), part_id);
	if (!query.exec())
	{
		setError(error, query.lastError().text());
		m_database.rollback();
		return false;
	}

	// Removing the current revision promotes the newest one that is left,
	// so that a code never ends up with revisions and no current one.
	if (doomed.is_current)
	{
		const QList<int> left = partRevisions(doomed.code);
		if (!left.isEmpty())
		{
			QSqlQuery promote(m_database);
			promote.prepare(QStringLiteral("UPDATE catalog_part SET is_current = 1 "
						       "WHERE code = :code AND revision = :revision"));
			promote.bindValue(QStringLiteral(":code"), doomed.code);
			promote.bindValue(QStringLiteral(":revision"), left.last());
			if (!promote.exec())
			{
				setError(error, promote.lastError().text());
				m_database.rollback();
				return false;
			}
		}
	}

	m_database.commit();
	emit partsChanged();
	return true;
}

/**
	@brief Catalog::searchParts
	@param text : matched against the code and against every value
	@param class_id : 0 for every class, subclasses included otherwise
	@param manufacturer : exact match on the manufacturer value
	@return the parts that match every given criterion
*/
QList<CatalogPart> Catalog::searchParts(const QString &text,
					int class_id,
					const QString &manufacturer) const
{
	QStringList clauses;
	QVariantList bindings;

	clauses.append(QStringLiteral("is_current = 1"));

	if (!text.isEmpty())
	{
		clauses.append(QStringLiteral("(code LIKE ? OR id IN "
					      "(SELECT part_id FROM catalog_part_value WHERE value LIKE ?))"));
		const QString pattern = QStringLiteral("%") + text + QStringLiteral("%");
		bindings << pattern << pattern;
	}

	if (class_id > 0)
	{
		QStringList ids;
		const QList<int> class_ids = descendantClassIds(class_id);
		for (const int id : class_ids) {
			ids.append(QString::number(id));
		}
		clauses.append(QStringLiteral("class_id IN (%1)").arg(ids.join(QLatin1Char(','))));
	}

	if (!manufacturer.isEmpty())
	{
		clauses.append(QStringLiteral("id IN (SELECT part_id FROM catalog_part_value "
					      "WHERE key = 'manufacturer' AND value = ?)"));
		bindings << manufacturer;
	}

	return partsFromQuery(clauses.join(QStringLiteral(" AND ")), bindings);
}

/**
	@brief Catalog::effectiveValues
	@param part
	@return every value of @a part, the initial value of the properties it
	never had filled included
*/
QHash<QString, QString> Catalog::effectiveValues(const CatalogPart &part) const
{
	QHash<QString, QString> values;

	const QList<CatalogProperty> properties = effectiveProperties(part.class_id);
	for (const CatalogProperty &property : properties)
	{
		values.insert(property.key,
			      part.hasValue(property.key) ? part.value(property.key)
							  : property.default_value);
	}

	// Values left behind by a property that was removed are kept visible
	// instead of disappearing without a trace.
	const QStringList own_keys = part.values.keys();
	for (const QString &key : own_keys)
	{
		if (!values.contains(key)) {
			values.insert(key, part.values.value(key));
		}
	}

	return values;
}

// -----------------------------------------------------------------------------
// Spreadsheet import profiles
// -----------------------------------------------------------------------------

/**
	@brief Catalog::importProfileNames
	@return the name of every saved profile, sorted
*/
QStringList Catalog::importProfileNames() const
{
	QStringList names;
	if (!isOpen()) {
		return names;
	}

	QSqlQuery query(const_cast<QSqlDatabase &>(m_database));
	if (query.exec(QStringLiteral("SELECT name FROM catalog_import_profile ORDER BY name")))
	{
		while (query.next()) {
			names.append(query.value(0).toString());
		}
	}
	return names;
}

/**
	@brief Catalog::importProfile
	@param name
	@return the payload of @a name, empty when there is no such profile
*/
QString Catalog::importProfile(const QString &name) const
{
	if (!isOpen()) {
		return QString();
	}

	QSqlQuery query(const_cast<QSqlDatabase &>(m_database));
	query.prepare(QStringLiteral("SELECT payload FROM catalog_import_profile "
				     "WHERE name = :name"));
	query.bindValue(QStringLiteral(":name"), name);
	if (query.exec() && query.next()) {
		return query.value(0).toString();
	}
	return QString();
}

/**
	@brief Catalog::saveImportProfile
	@param name
	@param payload
	@param error
	@return true on success
*/
bool Catalog::saveImportProfile(const QString &name, const QString &payload, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}
	if (name.trimmed().isEmpty())
	{
		setError(error, QCoreApplication::translate("Catalog",
							    "Le profil doit avoir un nom."));
		return false;
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("INSERT INTO catalog_import_profile (name, payload, updated_at) "
				     "VALUES (:name, :payload, :updated_at) "
				     "ON CONFLICT(name) DO UPDATE SET payload = :payload2, "
				     "updated_at = :updated_at2"));
	query.bindValue(QStringLiteral(":name"), name.trimmed());
	query.bindValue(QStringLiteral(":payload"), payload);
	query.bindValue(QStringLiteral(":updated_at"), isoNow());
	query.bindValue(QStringLiteral(":payload2"), payload);
	query.bindValue(QStringLiteral(":updated_at2"), isoNow());

	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return false;
	}

	emit importProfilesChanged();
	return true;
}

/**
	@brief Catalog::removeImportProfile
	@param name
	@param error
	@return true on success
*/
bool Catalog::removeImportProfile(const QString &name, QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}

	QSqlQuery query(m_database);
	query.prepare(QStringLiteral("DELETE FROM catalog_import_profile WHERE name = :name"));
	query.bindValue(QStringLiteral(":name"), name);
	if (!query.exec())
	{
		setError(error, query.lastError().text());
		return false;
	}

	emit importProfilesChanged();
	return true;
}

// -----------------------------------------------------------------------------
// Model bootstrap
// -----------------------------------------------------------------------------

/**
	@brief Catalog::seededComponentPropertyKeys
	@return the keys of the properties seeded on the Component class.

	They are on purpose the very keys QElectroTech already uses for the
	fixed fields of an element (QETInformation::elementInfoKeys()): the
	fields that exist today become properties of the Component class and
	keep working, and assigning a part writes them without any mapping
	table in between.
*/
QStringList Catalog::seededComponentPropertyKeys()
{
	return { QStringLiteral("designation"),
		 QStringLiteral("description"),
		 QStringLiteral("comment"),
		 QStringLiteral("function"),
		 QStringLiteral("manufacturer"),
		 QStringLiteral("manufacturer_reference"),
		 QStringLiteral("machine_manufacturer_reference"),
		 QStringLiteral("supplier"),
		 QStringLiteral("quantity"),
		 QStringLiteral("unity"),
		 QStringLiteral("datasheet"),
		 QStringLiteral("image"),
		 QStringLiteral("width"),
		 QStringLiteral("height"),
		 QStringLiteral("depth") };
}

/**
	@brief Catalog::seedDefaultModel
	@param error
	@return true on success

	The tree a fresh catalog starts with. Every node here can be renamed,
	moved, deleted or joined by others from the interface: this is a starting
	point, not a fixed structure.

	The tag roots deserve a word. The IEC 81346 column is the norm and is
	filled in accordingly. The house column starts as a copy of it, except
	for the motor, where the plan records that the house writes MTR where the
	norm writes M. The remaining house letters are for the office to confirm,
	and changing one is an edit in the interface - no code, no migration.
*/
bool Catalog::seedDefaultModel(QString *error)
{
	if (!requireWritable(error)) {
		return false;
	}
	if (!m_classes.isEmpty()) {
		return true;
	}

	struct SeedClass
	{
		const char *key;
		const char *name;
		const char *parent_key;
		const char *root;
		const char *root_iec;
		bool has_symbol;
	};

	// Root, then the object families, then the component subclasses.
	// QT_TRANSLATE_NOOP marks the name for lupdate while leaving a plain
	// literal in the array; the translation happens below, at insert time.
	static const SeedClass seed_classes[] = {
		{ "project_object",         QT_TRANSLATE_NOOP("Catalog", "Objets du projet"),   nullptr,          "",    "",  true  },
		{ "component",              QT_TRANSLATE_NOOP("Catalog", "Composant"),          "project_object", "",    "",  true  },
		{ "project",                QT_TRANSLATE_NOOP("Catalog", "Projet"),             "project_object", "",    "",  false },
		{ "location",               QT_TRANSLATE_NOOP("Catalog", "Localisation"),       "project_object", "",    "",  false },
		{ "terminal_strip",         QT_TRANSLATE_NOOP("Catalog", "Bornier"),            "project_object", "X",   "X", false },
		{ "terminal_strip_element", QT_TRANSLATE_NOOP("Catalog", "Élément de bornier"), "terminal_strip", "X",   "X", true  },
		{ "connector",              QT_TRANSLATE_NOOP("Catalog", "Connecteur"),         "project_object", "X",   "X", true  },
		{ "accessory",              QT_TRANSLATE_NOOP("Catalog", "Accessoire"),         "project_object", "",    "",  true  },
		{ "wire_cable",             QT_TRANSLATE_NOOP("Catalog", "Fil / Câble"),        "project_object", "W",   "W", false },
		{ "rail_duct",              QT_TRANSLATE_NOOP("Catalog", "Rail / Goulotte"),    "project_object", "",    "",  false },
		{ "system",                 QT_TRANSLATE_NOOP("Catalog", "Système"),            "project_object", "",    "",  false },
		{ "contactor",              QT_TRANSLATE_NOOP("Catalog", "Contacteur"),         "component",      "K",   "K", true  },
		{ "breaker",                QT_TRANSLATE_NOOP("Catalog", "Disjoncteur"),        "component",      "Q",   "Q", true  },
		{ "motor",                  QT_TRANSLATE_NOOP("Catalog", "Moteur"),             "component",      "MTR", "M", true  },
		{ "push_button",            QT_TRANSLATE_NOOP("Catalog", "Bouton"),             "component",      "S",   "S", true  },
		{ "indicator",              QT_TRANSLATE_NOOP("Catalog", "Voyant"),             "component",      "H",   "H", true  },
		{ "plc",                    QT_TRANSLATE_NOOP("Catalog", "Automate"),           "component",      "A",   "A", true  }
	};

	m_database.transaction();

	QHash<QString, int> class_ids;
	int order_index = 1;
	for (const SeedClass &seed : seed_classes)
	{
		CatalogClass catalog_class(QString::fromLatin1(seed.key),
					   QCoreApplication::translate("Catalog", seed.name));
		catalog_class.parent_id = seed.parent_key
					  ? class_ids.value(QString::fromLatin1(seed.parent_key))
					  : 0;
		catalog_class.root        = QString::fromLatin1(seed.root);
		catalog_class.root_iec    = QString::fromLatin1(seed.root_iec);
		catalog_class.has_symbol  = seed.has_symbol;
		catalog_class.order_index = order_index++;
		catalog_class.uuid        = QUuid::createUuid().toString(QUuid::WithoutBraces);

		QSqlQuery insert(m_database);
		insert.prepare(QStringLiteral("INSERT INTO catalog_class "
					      "(parent_id, key, name, description, root, root_iec, "
					      "has_symbol, order_index, uuid) "
					      "VALUES (:parent_id, :key, :name, '', :root, :root_iec, "
					      ":has_symbol, :order_index, :uuid)"));
		insert.bindValue(QStringLiteral(":parent_id"),
				 catalog_class.parent_id > 0 ? QVariant(catalog_class.parent_id)
							     : QVariant());
		insert.bindValue(QStringLiteral(":key"), catalog_class.key);
		insert.bindValue(QStringLiteral(":name"), catalog_class.name);
		insert.bindValue(QStringLiteral(":root"), catalog_class.root);
		insert.bindValue(QStringLiteral(":root_iec"), catalog_class.root_iec);
		insert.bindValue(QStringLiteral(":has_symbol"), catalog_class.has_symbol ? 1 : 0);
		insert.bindValue(QStringLiteral(":order_index"), catalog_class.order_index);
		insert.bindValue(QStringLiteral(":uuid"), catalog_class.uuid);
		if (!insert.exec())
		{
			setError(error, insert.lastError().text());
			m_database.rollback();
			return false;
		}
		class_ids.insert(catalog_class.key, insert.lastInsertId().toInt());
	}

	// The controlled lists start empty on purpose: what belongs in them is a
	// purchasing decision of the office, not something the software knows.
	const QStringList seeded_lists = { QCoreApplication::translate("Catalog", "Fabricants"),
					   QCoreApplication::translate("Catalog", "Fournisseurs"),
					   QCoreApplication::translate("Catalog", "Unités") };
	for (const QString &list_name : seeded_lists)
	{
		QSqlQuery insert(m_database);
		insert.prepare(QStringLiteral("INSERT OR IGNORE INTO catalog_list (name) VALUES (:name)"));
		insert.bindValue(QStringLiteral(":name"), list_name);
		if (!insert.exec())
		{
			setError(error, insert.lastError().text());
			m_database.rollback();
			return false;
		}
	}

	struct SeedProperty
	{
		const char *key;
		const char *name;
		CatalogPropertyType type;
		int list_index;             ///< 0 for a free field, else index in seeded_lists
		CatalogListBehaviour behaviour;
		const char *unit;
	};

	// The keys are on purpose the ones QElectroTech already uses for the
	// fixed fields of an element, so that assigning a part needs no mapping
	// table. The names are on purpose the ones the program already shows for
	// those keys: one field with two names, depending on which dialog is open,
	// is how a user learns to distrust both. The three measures are what the
	// physical view of the part will read, and what the revision use case of
	// the specification exercises.
	static const SeedProperty seed_properties[] = {
		{ "designation",                    QT_TRANSLATE_NOOP("Catalog", "Numéro d'article"),            CatalogPropertyType::Text,    0, CatalogListBehaviour::None,      "" },
		{ "description",                    QT_TRANSLATE_NOOP("Catalog", "Description textuelle"),            CatalogPropertyType::Text,    0, CatalogListBehaviour::None,      "" },
		{ "comment",                        QT_TRANSLATE_NOOP("Catalog", "Commentaire"),            CatalogPropertyType::Text,    0, CatalogListBehaviour::None,      "" },
		{ "function",                       QT_TRANSLATE_NOOP("Catalog", "Fonction"),               CatalogPropertyType::Text,    0, CatalogListBehaviour::None,      "" },
		{ "manufacturer",                   QT_TRANSLATE_NOOP("Catalog", "Fabricant"),              CatalogPropertyType::Text,    1, CatalogListBehaviour::Suggested, "" },
		{ "manufacturer_reference",         QT_TRANSLATE_NOOP("Catalog", "Numéro de commande"),    CatalogPropertyType::Text,    0, CatalogListBehaviour::None,      "" },
		{ "machine_manufacturer_reference", QT_TRANSLATE_NOOP("Catalog", "Numéro interne"), CatalogPropertyType::Text,    0, CatalogListBehaviour::None,      "" },
		{ "supplier",                       QT_TRANSLATE_NOOP("Catalog", "Fournisseur"),            CatalogPropertyType::Text,    2, CatalogListBehaviour::Suggested, "" },
		{ "quantity",                       QT_TRANSLATE_NOOP("Catalog", "Quantité"),               CatalogPropertyType::Decimal, 0, CatalogListBehaviour::None,      "" },
		{ "unity",                          QT_TRANSLATE_NOOP("Catalog", "Unité"),                  CatalogPropertyType::Text,    3, CatalogListBehaviour::Suggested, "" },
		{ "datasheet",                      QT_TRANSLATE_NOOP("Catalog", "Fiche technique"),        CatalogPropertyType::Link,    0, CatalogListBehaviour::None,      "" },
		{ "image",                          QT_TRANSLATE_NOOP("Catalog", "Image"),                  CatalogPropertyType::Image,   0, CatalogListBehaviour::None,      "" },
		{ "width",                          QT_TRANSLATE_NOOP("Catalog", "Largeur"),                CatalogPropertyType::Measure, 0, CatalogListBehaviour::None,      "mm" },
		{ "height",                         QT_TRANSLATE_NOOP("Catalog", "Hauteur"),                CatalogPropertyType::Measure, 0, CatalogListBehaviour::None,      "mm" },
		{ "depth",                          QT_TRANSLATE_NOOP("Catalog", "Profondeur"),             CatalogPropertyType::Measure, 0, CatalogListBehaviour::None,      "mm" }
	};

	const int component_id = class_ids.value(QStringLiteral("component"));
	int property_order = 1;
	for (const SeedProperty &seed : seed_properties)
	{
		const QString list_name = seed.list_index > 0
					  ? seeded_lists.at(seed.list_index - 1)
					  : QString();

		QSqlQuery insert(m_database);
		insert.prepare(QStringLiteral("INSERT INTO catalog_property "
					      "(class_id, key, name, type, list_behaviour, list_name, "
					      "options, default_value, unit, description, order_index) "
					      "VALUES (:class_id, :key, :name, :type, :list_behaviour, "
					      ":list_name, '', '', :unit, '', :order_index)"));
		insert.bindValue(QStringLiteral(":class_id"), component_id);
		insert.bindValue(QStringLiteral(":key"), QString::fromLatin1(seed.key));
		insert.bindValue(QStringLiteral(":name"),
				 QCoreApplication::translate("Catalog", seed.name));
		insert.bindValue(QStringLiteral(":type"), CatalogProperty::typeToString(seed.type));
		insert.bindValue(QStringLiteral(":list_behaviour"),
				 CatalogProperty::listBehaviourToString(seed.behaviour));
		insert.bindValue(QStringLiteral(":list_name"), list_name);
		insert.bindValue(QStringLiteral(":unit"), QString::fromLatin1(seed.unit));
		insert.bindValue(QStringLiteral(":order_index"), property_order++);
		if (!insert.exec())
		{
			setError(error, insert.lastError().text());
			m_database.rollback();
			return false;
		}
	}

	m_database.commit();

	reloadLists();
	reloadClasses();
	reloadProperties();
	emit classesChanged();
	emit listsChanged();
	return true;
}
