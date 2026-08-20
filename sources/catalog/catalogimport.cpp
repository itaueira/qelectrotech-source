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
#include "catalogimport.h"

#include <QHash>

#include "catalog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDomDocument>
#include <QDomElement>

// -----------------------------------------------------------------------------
// CatalogImportProfile
// -----------------------------------------------------------------------------

/**
	@brief CatalogImportProfile::isValid
	@param error
	@return true when this profile can be used
*/
bool CatalogImportProfile::isValid(QString *error) const
{
	if (code_column.trimmed().isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogImportProfile",
							     "Il faut dire quelle colonne porte la référence de la pièce.");
		}
		return false;
	}
	if (class_key.trimmed().isEmpty() && class_column.trimmed().isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("CatalogImportProfile",
							     "Il faut choisir une classe de destination, ou dire "
							     "quelle colonne la porte.");
		}
		return false;
	}
	return true;
}

/**
	@brief CatalogImportProfile::policyToString
	@param policy
	@return the stable string stored with the profile
*/
QString CatalogImportProfile::policyToString(CatalogDuplicatePolicy policy)
{
	switch (policy)
	{
		case CatalogDuplicatePolicy::Ignore:      return QStringLiteral("ignore");
		case CatalogDuplicatePolicy::Update:      return QStringLiteral("update");
		case CatalogDuplicatePolicy::NewRevision: return QStringLiteral("new_revision");
	}
	return QStringLiteral("update");
}

/**
	@brief CatalogImportProfile::policyFromString
	@param string
	@return the policy @a string names, Update when it names nothing known
*/
CatalogDuplicatePolicy CatalogImportProfile::policyFromString(const QString &string)
{
	if (string == QStringLiteral("ignore")) {
		return CatalogDuplicatePolicy::Ignore;
	}
	if (string == QStringLiteral("new_revision")) {
		return CatalogDuplicatePolicy::NewRevision;
	}
	return CatalogDuplicatePolicy::Update;
}

/**
	@brief CatalogImportProfile::translatedPolicyName
	@param policy
	@return the name of @a policy in the user language
*/
QString CatalogImportProfile::translatedPolicyName(CatalogDuplicatePolicy policy)
{
	switch (policy)
	{
		case CatalogDuplicatePolicy::Ignore:
			return QCoreApplication::translate("CatalogImportProfile",
							   "Ignorer : garder ce que le catalogue a");
		case CatalogDuplicatePolicy::Update:
			return QCoreApplication::translate("CatalogImportProfile",
							   "Mettre à jour : écraser les champs apportés");
		case CatalogDuplicatePolicy::NewRevision:
			return QCoreApplication::translate("CatalogImportProfile",
							   "Nouvelle révision : garder l'ancienne donnée");
	}
	return QString();
}

/**
	@brief CatalogImportProfile::toXml
	@return the profile as a document, for storing in the catalog
*/
QString CatalogImportProfile::toXml() const
{
	QDomDocument document;
	QDomElement root = document.createElement(QStringLiteral("import-profile"));
	root.setAttribute(QStringLiteral("name"), name);
	root.setAttribute(QStringLiteral("class-key"), class_key);
	root.setAttribute(QStringLiteral("class-column"), class_column);
	root.setAttribute(QStringLiteral("code-column"), code_column);
	root.setAttribute(QStringLiteral("policy"), policyToString(policy));
	if (!delimiter.isNull()) {
		root.setAttribute(QStringLiteral("delimiter"), QString(delimiter));
	}

	const QStringList keys = value_columns.keys();
	for (const QString &key : keys)
	{
		QDomElement column = document.createElement(QStringLiteral("column"));
		column.setAttribute(QStringLiteral("property"), key);
		column.setAttribute(QStringLiteral("header"), value_columns.value(key));
		root.appendChild(column);
	}

	document.appendChild(root);
	return document.toString();
}

/**
	@brief CatalogImportProfile::fromXml
	@param xml
	@return the profile the document describes
*/
CatalogImportProfile CatalogImportProfile::fromXml(const QString &xml)
{
	CatalogImportProfile profile;
	QDomDocument document;
	if (!document.setContent(xml)) {
		return profile;
	}

	const QDomElement root = document.documentElement();
	profile.name = root.attribute(QStringLiteral("name"));
	profile.class_key = root.attribute(QStringLiteral("class-key"));
	profile.class_column = root.attribute(QStringLiteral("class-column"));
	profile.code_column = root.attribute(QStringLiteral("code-column"));
	profile.policy = policyFromString(root.attribute(QStringLiteral("policy")));

	const QString delimiter_text = root.attribute(QStringLiteral("delimiter"));
	if (!delimiter_text.isEmpty()) {
		profile.delimiter = delimiter_text.at(0);
	}

	QDomNodeList columns = root.elementsByTagName(QStringLiteral("column"));
	for (int index = 0 ; index < columns.count() ; ++index)
	{
		const QDomElement column = columns.at(index).toElement();
		const QString property = column.attribute(QStringLiteral("property"));
		if (!property.isEmpty()) {
			profile.value_columns.insert(property,
						     column.attribute(QStringLiteral("header")));
		}
	}

	return profile;
}

/**
	@brief CatalogImportProfile::guess
	@param catalog
	@param class_id
	@param table
	@return a mapping filled where the header and the property clearly agree
*/
CatalogImportProfile CatalogImportProfile::guess(const Catalog &catalog,
						 int class_id,
						 const CatalogTable &table)
{
	CatalogImportProfile profile;
	profile.class_key = catalog.classById(class_id).key;

	const QList<CatalogProperty> properties = catalog.effectiveProperties(class_id);
	for (const CatalogProperty &property : properties)
	{
		// The header may be the user visible name or the technical key: a
		// spreadsheet exported from this very catalog carries the names, and
		// one written by hand often carries neither exactly.
		int column = table.columnIndex(property.name);
		if (column < 0) {
			column = table.columnIndex(property.key);
		}
		if (column >= 0) {
			profile.value_columns.insert(property.key, table.headers.at(column));
		}
	}

	// The code column is the one thing that must be right, so it is looked for
	// under the several names a list of parts uses for it.
	const QStringList code_headers = { QStringLiteral("code"),
					   QStringLiteral("codigo"),
					   QStringLiteral("código"),
					   QStringLiteral("referencia"),
					   QStringLiteral("référence"),
					   QStringLiteral("reference"),
					   QStringLiteral("part_code"),
					   QStringLiteral("part number"),
					   QStringLiteral("sku") };
	for (const QString &candidate : code_headers)
	{
		const int column = table.columnIndex(candidate);
		if (column >= 0)
		{
			profile.code_column = table.headers.at(column);
			break;
		}
	}
	if (profile.code_column.isEmpty() && !table.headers.isEmpty())
	{
		// Nothing recognised: the first column is the usual place, and the
		// user is going to see and correct this anyway.
		profile.code_column = table.headers.first();
	}

	return profile;
}

// -----------------------------------------------------------------------------
// CatalogImportReport
// -----------------------------------------------------------------------------

/**
	@brief CatalogImportReport::rejected
	@return how many rows were refused
*/
int CatalogImportReport::rejected() const
{
	return rejections.size();
}

/**
	@brief CatalogImportReport::total
	@return how many rows were looked at
*/
int CatalogImportReport::total() const
{
	return created + updated + revised + ignored + rejected();
}

/**
	@brief CatalogImportReport::changedAnything
	@return true when the catalog is different afterwards
*/
bool CatalogImportReport::changedAnything() const
{
	return created > 0 || updated > 0 || revised > 0;
}

/**
	@brief CatalogImportReport::toText
	@return a summary a person reads
*/
QString CatalogImportReport::toText() const
{
	QStringList lines;
	lines << QCoreApplication::translate("CatalogImportReport",
					     "%1 ligne(s) lue(s).").arg(total());
	lines << QCoreApplication::translate("CatalogImportReport",
					     "%1 créée(s), %2 mise(s) à jour, %3 nouvelle(s) "
					     "révision(s), %4 ignorée(s), %5 refusée(s).")
		 .arg(created).arg(updated).arg(revised).arg(ignored).arg(rejected());

	if (!rejections.isEmpty())
	{
		lines << QString();
		lines << QCoreApplication::translate("CatalogImportReport", "Lignes refusées :");
		for (const Rejection &rejection : rejections)
		{
			lines << QCoreApplication::translate("CatalogImportReport",
							     "  ligne %1 (%2) : %3")
				 .arg(rejection.row)
				 .arg(rejection.code.isEmpty()
				      ? QCoreApplication::translate("CatalogImportReport", "sans référence")
				      : rejection.code)
				 .arg(rejection.reason);
		}
	}

	if (!notes.isEmpty())
	{
		lines << QString();
		lines << notes;
	}

	return lines.join(QLatin1Char('\n'));
}

// -----------------------------------------------------------------------------
// CatalogImporter
// -----------------------------------------------------------------------------

/**
	@brief CatalogImporter::import
	@param catalog
	@param table
	@param profile
	@param origin
	@return what was done
*/
CatalogImportReport CatalogImporter::import(Catalog &catalog,
					    const CatalogTable &table,
					    const CatalogImportProfile &profile,
					    const QString &origin)
{
	CatalogImportReport report;

	QString error;
	if (!profile.isValid(&error))
	{
		report.notes << error;
		return report;
	}
	if (!catalog.isOpen())
	{
		report.notes << QCoreApplication::translate("CatalogImporter",
							    "Le catalogue n'est pas ouvert.");
		return report;
	}
	if (!catalog.isWritable())
	{
		report.notes << QCoreApplication::translate("CatalogImporter",
							    "Le catalogue est en lecture seule.");
		return report;
	}

	const int fixed_class_id = profile.class_key.isEmpty()
				   ? 0
				   : catalog.classByKey(profile.class_key).id;
	if (fixed_class_id == 0 && profile.class_column.isEmpty())
	{
		report.notes << QCoreApplication::translate("CatalogImporter",
							    "La classe de destination « %1 » n'existe pas.")
				.arg(profile.class_key);
		return report;
	}

	const QString stamp = QDateTime::currentDateTime().toString(Qt::ISODate);

	for (int row = 0 ; row < table.rowCount() ; ++row)
	{
		// The row number the user sees counts the header, because that is
		// what their spreadsheet shows in the margin.
		const int reported_row = row + 2;
		const QString code = table.value(row, profile.code_column);

		if (code.isEmpty())
		{
			CatalogImportReport::Rejection rejection;
			rejection.row = reported_row;
			rejection.reason = QCoreApplication::translate(
						"CatalogImporter",
						"pas de référence dans la colonne « %1 »")
					   .arg(profile.code_column);
			report.rejections.append(rejection);
			continue;
		}

		int class_id = fixed_class_id;
		if (!profile.class_column.isEmpty())
		{
			const QString class_name = table.value(row, profile.class_column);
			CatalogClass found = catalog.classByKey(class_name);
			if (found.isNull())
			{
				// The column may carry the visible name rather than the key.
				const QList<CatalogClass> classes = catalog.classes();
				for (const CatalogClass &candidate : classes)
				{
					if (candidate.name.compare(class_name, Qt::CaseInsensitive) == 0)
					{
						found = candidate;
						break;
					}
				}
			}
			if (found.isNull())
			{
				CatalogImportReport::Rejection rejection;
				rejection.row = reported_row;
				rejection.code = code;
				rejection.reason = QCoreApplication::translate(
							"CatalogImporter",
							"classe « %1 » inconnue")
						   .arg(class_name);
				report.rejections.append(rejection);
				continue;
			}
			class_id = found.id;
		}

		const CatalogPart existing = catalog.partByCode(code);
		const bool already_there = !existing.isNull();

		if (already_there && profile.policy == CatalogDuplicatePolicy::Ignore)
		{
			++report.ignored;
			continue;
		}

		// Start from what the catalog has, so that a price list updating two
		// columns does not blank the pins, the physical view and the
		// accessories the part already carried.
		CatalogPart part = already_there ? existing : CatalogPart(code, class_id);
		if (!already_there) {
			part.class_id = class_id;
		}
		part.origin = origin;
		part.origin_date = stamp;

		// The properties of the class the part is going into, so that a cell
		// can be read as what the column is for. Fetched per part because two
		// rows of the same sheet can belong to two classes.
		const QList<CatalogProperty> class_properties =
				catalog.effectiveProperties(class_id);
		QHash<QString, CatalogProperty> property_by_key;
		for (const CatalogProperty &property : class_properties) {
			property_by_key.insert(property.key, property);
		}

		const QStringList property_keys = profile.value_columns.keys();
		for (const QString &key : property_keys)
		{
			const QString header = profile.value_columns.value(key);
			if (header.isEmpty() || table.columnIndex(header) < 0) {
				continue;
			}
			QString value = table.value(row, header);
			// A date column of an .xlsx arrives as a serial number. The
			// property says it is a date, so this is the one place where
			// converting is not guessing.
			if (property_by_key.contains(key)) {
				value = property_by_key.value(key).fromSpreadsheetCell(value);
			}
			// An empty cell leaves what was there: a supplier's list rarely
			// fills every column, and treating a gap as "erase this" is how
			// an import destroys a catalog.
			if (!value.isEmpty()) {
				part.setValue(key, value);
			}
		}

		bool saved = false;
		QString save_error;
		if (already_there && profile.policy == CatalogDuplicatePolicy::NewRevision)
		{
			saved = catalog.savePartAsNewRevision(part, &save_error);
			if (saved) {
				++report.revised;
			}
		}
		else
		{
			saved = catalog.savePart(part, &save_error);
			if (saved)
			{
				if (already_there) {
					++report.updated;
				} else {
					++report.created;
				}
			}
		}

		if (!saved)
		{
			CatalogImportReport::Rejection rejection;
			rejection.row = reported_row;
			rejection.code = code;
			rejection.reason = save_error;
			report.rejections.append(rejection);
		}
	}

	return report;
}

/**
	@brief CatalogImporter::exportToTable
	@param catalog
	@param class_id
	@return the catalog as a table
*/
CatalogTable CatalogImporter::exportToTable(const Catalog &catalog, int class_id)
{
	CatalogTable table;
	if (!catalog.isOpen()) {
		return table;
	}

	const QList<CatalogPart> parts = catalog.parts(class_id);

	// The columns: the fixed ones, then the union of the properties of every
	// class present. A single class exports a tidy table; several export a
	// wider one with gaps, which is still what purchasing asked for.
	table.headers << QStringLiteral("code")
		      << QStringLiteral("revision")
		      << QStringLiteral("class");

	QStringList property_keys;
	QHash<int, QList<CatalogProperty>> per_class;
	for (const CatalogPart &part : parts)
	{
		if (!per_class.contains(part.class_id)) {
			per_class.insert(part.class_id, catalog.effectiveProperties(part.class_id));
		}
		const QList<CatalogProperty> properties = per_class.value(part.class_id);
		for (const CatalogProperty &property : properties)
		{
			if (!property_keys.contains(property.key)) {
				property_keys.append(property.key);
			}
		}
	}
	table.headers << property_keys;

	for (const CatalogPart &part : parts)
	{
		QStringList row;
		row << part.code
		    << QString::number(part.revision)
		    << catalog.classById(part.class_id).key;

		const QHash<QString, QString> values = catalog.effectiveValues(part);
		for (const QString &key : property_keys) {
			row << values.value(key);
		}
		table.rows.append(row);
	}

	return table;
}
