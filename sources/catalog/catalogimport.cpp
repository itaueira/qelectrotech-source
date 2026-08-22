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
#include <QSet>

/**
	@brief resolveClassNamed
	@param catalog
	@param name : what a cell of the class column says
	@return the class of that name, null when the catalog has none

	By stable key first, then by visible name, case insensitively: a sheet
	written by hand carries the name a person reads, one exported from here
	carries the key. Shared by the guess and by the import so that the two
	can never disagree about what a class name means - if they could, the
	dialog would offer a mapping the import then refuses. (CU-14.14)
*/
static CatalogClass resolveClassNamed(const Catalog &catalog, const QString &name)
{
	if (name.trimmed().isEmpty()) {
		return CatalogClass();
	}

	const CatalogClass by_key = catalog.classByKey(name);
	if (!by_key.isNull()) {
		return by_key;
	}

	const QList<CatalogClass> classes = catalog.classes();
	for (const CatalogClass &candidate : classes)
	{
		if (candidate.name.compare(name, Qt::CaseInsensitive) == 0) {
			return candidate;
		}
	}
	return CatalogClass();
}

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
	@brief CatalogImportProfile::guessClassColumn
	@param catalog
	@param table
	@return the header of the column carrying a class name, empty when none
*/
QString CatalogImportProfile::guessClassColumn(const Catalog &catalog,
					       const CatalogTable &table)
{
	for (int column = 0 ; column < table.headers.size() ; ++column)
	{
		const QString header = table.headers.at(column);
		if (header.trimmed().isEmpty()) {
			continue;
		}

		// Every cell has to name a class. Anything less and the guess would
		// be proposing rejections, which is worse than proposing nothing: the
		// person asked for help filling a form, not for rows to be thrown
		// away.
		bool all_resolve = table.rowCount() > 0;
		for (int row = 0 ; row < table.rowCount() ; ++row)
		{
			// An empty cell is refused by the import just like an unknown
			// name is, so an empty cell disqualifies the column too.
			if (resolveClassNamed(catalog, table.value(row, header)).isNull())
			{
				all_resolve = false;
				break;
			}
		}

		if (all_resolve) {
			return header;
		}
	}
	return QString();
}

/**
	@brief CatalogImportProfile::mappableProperties
	@param catalog
	@param class_id
	@param table
	@param class_column
	@return the properties a mapping may target, deduped by key
*/
QList<CatalogProperty> CatalogImportProfile::mappableProperties(const Catalog &catalog,
								int class_id,
								const CatalogTable &table,
								const QString &class_column)
{
	QList<CatalogProperty> properties = catalog.effectiveProperties(class_id);
	if (class_column.trimmed().isEmpty()) {
		return properties;
	}

	QSet<QString> keys;
	for (const CatalogProperty &property : std::as_const(properties)) {
		keys.insert(property.key);
	}

	// The order is the order of the file, so that reading the mapping table
	// next to the spreadsheet is reading the same thing twice.
	QSet<int> seen_classes;
	for (int row = 0 ; row < table.rowCount() ; ++row)
	{
		const CatalogClass declared =
			resolveClassNamed(catalog, table.value(row, class_column));
		if (declared.isNull() || seen_classes.contains(declared.id)) {
			continue;
		}
		seen_classes.insert(declared.id);

		const QList<CatalogProperty> declared_properties =
			catalog.effectiveProperties(declared.id);
		for (const CatalogProperty &property : declared_properties)
		{
			// Same key on two sibling classes is the same field: the file
			// has one column for it, so the table gets one row for it.
			if (keys.contains(property.key)) {
				continue;
			}
			keys.insert(property.key);
			properties << property;
		}
	}
	return properties;
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

	// Found before the properties are, because it is what decides which
	// properties may be mapped at all. (CU-14.14)
	profile.class_column = guessClassColumn(catalog, table);

	const QList<CatalogProperty> properties =
		mappableProperties(catalog, class_id, table, profile.class_column);
	for (const CatalogProperty &property : properties)
	{
		// The header may be the user visible name or the technical key: a
		// spreadsheet exported from this very catalog carries the names, and
		// one written by hand often carries neither exactly.
		int column = table.columnIndex(property.name);
		if (column < 0) {
			column = table.columnIndex(property.key);
		}
		if (column >= 0 && table.headers.at(column) != profile.class_column) {
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

/**
	@brief CatalogImportProfile::unmappedColumns
	@param table
	@return the headers of @a table this profile reads nothing from
*/
QStringList CatalogImportProfile::unmappedColumns(const CatalogTable &table) const
{
	QSet<int> used;
	const auto use = [&used, &table](const QString &header)
	{
		const int column = table.columnIndex(header);
		if (column >= 0) {
			used.insert(column);
		}
	};

	use(code_column);
	use(class_column);
	const QStringList mapped = value_columns.values();
	for (const QString &header : mapped) {
		use(header);
	}

	QStringList leftover;
	for (int column = 0 ; column < table.headers.size() ; ++column)
	{
		// The header spelled as the file spells it, because that is what the
		// person has to find again in their spreadsheet. A nameless column -
		// a trailing delimiter - is not worth naming.
		const QString header = table.headers.at(column);
		if (!used.contains(column) && !header.trimmed().isEmpty()) {
			leftover << header;
		}
	}
	return leftover;
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

	if (!class_moves.isEmpty())
	{
		lines << QString();
		lines << QCoreApplication::translate("CatalogImportReport",
						     "%1 pièce(s) changée(s) de classe :")
			 .arg(class_moves.size());
		for (const ClassMove &move : class_moves)
		{
			lines << QCoreApplication::translate("CatalogImportReport",
							     "  %1 : de %2 vers %3")
				 .arg(move.code, move.from, move.to);
		}
	}

	if (!undeclared_values.isEmpty())
	{
		lines << QString();
		lines << QCoreApplication::translate("CatalogImportReport",
						     "Valeurs hors de la classe :");
		for (const UndeclaredValue &undeclared : undeclared_values)
		{
			lines << (undeclared.from_sheet
				  ? QCoreApplication::translate(
					    "CatalogImportReport",
					    "  ligne %1 (%2) : « %3 » refusée, la classe "
					    "« %4 » ne la déclare pas")
				  : QCoreApplication::translate(
					    "CatalogImportReport",
					    "  ligne %1 (%2) : « %3 » gardée mais invisible, la "
					    "classe « %4 » ne la déclare pas"))
				 .arg(undeclared.row)
				 .arg(undeclared.code, undeclared.key, undeclared.class_name);
		}
	}

	if (!unmapped_columns.isEmpty())
	{
		lines << QString();
		lines << QCoreApplication::translate("CatalogImportReport",
						     "Colonnes que rien ne lit (%1) : %2")
			 .arg(unmapped_columns.size())
			 .arg(unmapped_columns.join(QStringLiteral(", ")));
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

	// Which columns nothing reads, worked out before a single row is written.
	// The mapping table has one row per property, so a column with no property
	// has nowhere to appear on screen: this is the only place the person ever
	// hears of it. (CU-14.12)
	report.unmapped_columns = profile.unmappedColumns(table);

	// Class names for the report, each looked up once. A person reads names,
	// not identifiers, and a report is read by a person.
	QHash<int, QString> class_names;
	const auto name_of_class = [&catalog, &class_names](int id) -> QString
	{
		if (!class_names.contains(id)) {
			class_names.insert(id, catalog.classById(id).name);
		}
		return class_names.value(id);
	};

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
			const CatalogClass found = resolveClassNamed(catalog, class_name);
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

		// The class the sheet declares wins, on an update as much as on a new
		// part: running a list again precisely to classify what a first run
		// brought in generic is the ordinary reason to import twice. Keeping
		// the stored class let the report say "11 updated" while nothing had
		// moved, which is the one thing a report must never do. (CU-14.13)
		if (already_there && part.class_id != class_id)
		{
			CatalogImportReport::ClassMove move;
			move.code = code;
			move.from = name_of_class(part.class_id);
			move.to = name_of_class(class_id);
			report.class_moves.append(move);
		}
		part.class_id = class_id;
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

		// Sorted, so that two runs of the same file report the same way: a
		// hash hands its keys back in whatever order it pleases.
		QStringList property_keys = profile.value_columns.keys();
		property_keys.sort();
		QSet<QString> refused_keys;
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
			if (value.isEmpty()) {
				continue;
			}
			// A cell the destination class has no field for is refused, and
			// said out loud. Storing it would work - the value rows are not
			// filtered by class - and that is exactly the trap: the part
			// dialog only shows the class, so the value would sit there
			// invisible, impossible to correct, and counted as imported.
			// (CU-14.13)
			if (!property_by_key.contains(key))
			{
				CatalogImportReport::UndeclaredValue undeclared;
				undeclared.row = reported_row;
				undeclared.code = code;
				undeclared.key = key;
				undeclared.class_name = name_of_class(class_id);
				report.undeclared_values.append(undeclared);
				refused_keys.insert(key);
				continue;
			}
			part.setValue(key, value);
		}

		// And what the part already carried that its class does not declare.
		// It is kept - moving a part between classes is no reason to delete
		// data, and the right fix is often to add the field to the class -
		// but it is named, because until somebody does that nobody can see it.
		QStringList stored_keys = part.values.keys();
		stored_keys.sort();
		for (const QString &key : stored_keys)
		{
			if (property_by_key.contains(key) || refused_keys.contains(key)
			    || part.values.value(key).isEmpty()) {
				continue;
			}
			CatalogImportReport::UndeclaredValue undeclared;
			undeclared.row = reported_row;
			undeclared.code = code;
			undeclared.key = key;
			undeclared.class_name = name_of_class(class_id);
			undeclared.from_sheet = false;
			report.undeclared_values.append(undeclared);
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
