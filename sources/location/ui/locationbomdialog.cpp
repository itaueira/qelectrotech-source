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
#include "locationbomdialog.h"

#include "../../catalog/catalog.h"
#include "../../catalog/catalogpart.h"
#include "../../dataBase/projectdatabase.h"
#include "../../qetapp.h"
#include "../../qetinformation.h"
#include "../../qetproject.h"
#include "../locationtree.h"
#include "../projectlocation.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
	enum Column { QuantityColumn = 0,
		      PartColumn,
		      DesignationColumn,
		      ManufacturerColumn,
		      ReferenceColumn,
		      LocationColumn,
		      ColumnCount };

		/// what a line of the scope box is about, in Qt::UserRole
	enum Scope { AllScope = 0, PathScope, FieldScope };
		/// the location_path a PathScope line stands for
	const int PATH_ROLE = Qt::UserRole + 1;

	/**
		@return the element type filter, written exactly the way
		ElementQueryWidget::queryStr writes it
		The spelling matters twice over. The list this window shows and the
		table it puts on a folio have to count the same things, or the
		person reads two different answers to one question; and the seeded
		query has to survive ElementQueryWidget::setQuery, which does not
		store the string but parses it back into its check boxes - a clause
		it cannot express is a clause that disappears the moment somebody
		opens the table's properties.

		What that vocabulary can say is: terminals, simple components, and
		the four kinds of master. That is the right set anyway. A cross
		reference arrow is not bought; a folio thumbnail is not bought; and
		a slave is the auxiliary contact of a master that is already on the
		list, so counting it would order the same contact block twice.
	*/
	QString typeClause()
	{
		return QStringLiteral(" WHERE ( element_type = 'terminal'"
				      " OR element_type = 'simple'"
				      " OR element_sub_type = 'commutator'"
				      " OR element_sub_type = 'coil'"
				      " OR element_sub_type = 'protection'"
				      " OR element_sub_type = 'plc')");
	}

	/**
		@param text
		@return @a text ready to sit between two single quotes in SQL
		Location codes are typed by a person. Nothing in the sanitizer
		forbids an apostrophe in a name, and a name that ends the string
		early would not fail loudly - it would quietly list the wrong
		material.
	*/
	QString quoted(const QString &text)
	{
		QString escaped = text;
		escaped.replace(QStringLiteral("'"), QStringLiteral("''"));
		return QStringLiteral("'") + escaped + QStringLiteral("'");
	}

	/// @return the part code with its revision, as it is ordered
	QString partLabel(const QString &code, int revision)
	{
		if (code.isEmpty()) {
			return QString();
		}
		if (revision > 1) {
			return code + QStringLiteral(" rev. ")
					+ QString::number(revision);
		}
		return code;
	}
}

/**
	@brief LocationBomDialog::LocationBomDialog
	@param project
	@param parent
*/
LocationBomDialog::LocationBomDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	buildWidgets();

	if (m_project)
	{
			//Assigning a component, buying a cabinet, renaming a
			//location - everything this window shows is undoable, so
			//the undo stack is the one thing worth listening to.
		if (QUndoStack *stack = m_project->undoStack()) {
			connect(stack, &QUndoStack::indexChanged,
				this, &LocationBomDialog::reload);
		}
		connect(m_project.data(), &QObject::destroyed,
			this, &QDialog::close);
	}

	reload();
}

/**
	@brief LocationBomDialog::project
	@return
*/
QETProject *LocationBomDialog::project() const
{
	return m_project.data();
}

/**
	@brief LocationBomDialog::buildWidgets
*/
void LocationBomDialog::buildWidgets()
{
	setWindowTitle(tr("Liste de matériel par localisation"));

	QVBoxLayout *layout = new QVBoxLayout(this);

	QHBoxLayout *scope_layout = new QHBoxLayout();
	QLabel *scope_label = new QLabel(tr("Localisation :"), this);
	m_scope = new QComboBox(this);
	m_scope->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	m_scope->setToolTip(tr("Ce qu'il faut sortir du magasin pour cette "
			       "localisation, sous-localisations comprises."));
	scope_layout->addWidget(scope_label);
	scope_layout->addWidget(m_scope, 1);
	layout->addLayout(scope_layout);

	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(ColumnCount);
	QStringList headers;
	headers << tr("Qté")
		<< tr("Pièce")
		<< tr("Désignation")
		<< tr("Fabricant")
		<< tr("Référence")
		<< tr("Localisation");
	m_tree->setHeaderLabels(headers);
	m_tree->setRootIsDecorated(true);
	m_tree->setAlternatingRowColors(true);
	m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_tree->header()->setStretchLastSection(false);
	m_tree->header()->setSectionResizeMode(DesignationColumn,
					       QHeaderView::Stretch);
	layout->addWidget(m_tree, 1);

	m_summary = new QLabel(this);
	layout->addWidget(m_summary);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	layout->addWidget(m_status);

	QHBoxLayout *button_layout = new QHBoxLayout();
	m_copy = new QPushButton(tr("Copier"), this);
	m_copy->setToolTip(tr("Copier la liste dans le presse-papiers, "
			      "colonnes séparées par une tabulation."));
	m_export = new QPushButton(tr("Exporter en CSV…"), this);
	m_insert = new QPushButton(tr("Insérer dans le folio…"), this);
	m_insert->setToolTip(tr("Poser sur le folio courant un tableau de "
				"nomenclature limité à cette localisation. Le "
				"tableau suit le projet : il se met à jour tout "
				"seul."));
	button_layout->addWidget(m_copy);
	button_layout->addWidget(m_export);
	button_layout->addWidget(m_insert);
	button_layout->addStretch();
	layout->addLayout(button_layout);

	QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close,
						     this);
	layout->addWidget(box);

	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_scope, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &LocationBomDialog::scopeChanged);
	connect(m_copy, &QPushButton::clicked,
		this, &LocationBomDialog::copyToClipboard);
	connect(m_export, &QPushButton::clicked,
		this, &LocationBomDialog::exportCsv);
	connect(m_insert, &QPushButton::clicked,
		this, &LocationBomDialog::insertOnFolio);

	resize(900, 560);
}

/**
	@brief LocationBomDialog::fillScopes
	Every location of the project, plus the two scopes that are not
	locations: everything, and everything that is nowhere.
*/
void LocationBomDialog::fillScopes()
{
	const bool blocked = m_scope->blockSignals(true);
	const QString previous = m_scope->count() ? scopePath() : QString();
	const int previous_scope = m_scope->count()
			? m_scope->currentData().toInt()
			: int(AllScope);

	m_scope->clear();
	m_scope->addItem(tr("(toutes les localisations)"), int(AllScope));

	if (m_project)
	{
		const LocationTree tree = m_project->locationTree();
		for (int i = 0 ; i < tree.count() ; ++i)
		{
			const ProjectLocation location = tree.at(i);
			const QString path = tree.path(location.uuid);
			if (path.isEmpty()) {
				continue;
			}

			QString text = LocationTree::iecTag(path);
			if (!location.name.isEmpty()) {
				text += QStringLiteral(" — ") + location.name;
			}

			m_scope->addItem(text, int(PathScope));
			m_scope->setItemData(m_scope->count() - 1, path,
					     PATH_ROLE);
		}
	}

	m_scope->addItem(tr("Hors localisation (chantier)"), int(FieldScope));

		//Come back where the person was, if it still exists.
	int index = 0;
	if (previous_scope == FieldScope) {
		index = m_scope->count() - 1;
	} else if (previous_scope == PathScope && !previous.isEmpty()) {
		for (int i = 0 ; i < m_scope->count() ; ++i)
		{
			if (m_scope->itemData(i, PATH_ROLE).toString() == previous) {
				index = i;
				break;
			}
		}
	}
	m_scope->setCurrentIndex(index);

	m_scope->blockSignals(blocked);
}

/**
	@brief LocationBomDialog::scopePath
	@return
*/
QString LocationBomDialog::scopePath() const
{
	if (!m_scope) {
		return QString();
	}
	return m_scope->currentData(PATH_ROLE).toString();
}

/**
	@brief LocationBomDialog::scopeIsField
	@return
*/
bool LocationBomDialog::scopeIsField() const
{
	return m_scope && m_scope->currentData().toInt() == FieldScope;
}

/**
	@brief LocationBomDialog::scopeChanged
*/
void LocationBomDialog::scopeChanged() {
	reload();
}

/**
	@brief LocationBomDialog::reload
*/
void LocationBomDialog::reload()
{
	if (!m_tree) {
		return;
	}

	fillScopes();
	m_tree->clear();
	say(QString());

	const int locations = fillLocations();
	const int components = fillComponents();

	m_tree->expandAll();
	for (int i = 0 ; i < ColumnCount ; ++i) {
		if (i != DesignationColumn) {
			m_tree->resizeColumnToContents(i);
		}
	}

	if (locations + components == 0)
	{
		m_summary->setText(tr("Rien à sortir du magasin pour cette "
				      "localisation."));
	}
	else
	{
		m_summary->setText(tr("%n article(s) à sortir du magasin.", "",
				      locations + components));
	}

	//Posing a table writes on the folio, so a project opened read-only
	//can be read and exported here, but not written to.
	m_insert->setEnabled(components > 0
			     && m_project && !m_project->isReadOnly());
}

/**
	@brief LocationBomDialog::fillLocations
	@return how many enclosures the scope asks for
	The half of the list that no walk over the folios could find. An
	enclosure is not drawn: it is said, in the location tree, and the part
	it was bought as is written on it there. CU-32.4 lives in
	LocationTree::bomLines, which leaves out the locations whose part is
	marked virtual - a door and a mounting plate came with the cabinet, and
	the storeroom has nothing to hand over for them.
*/
int LocationBomDialog::fillLocations()
{
	if (!m_project) {
		return 0;
	}

	const LocationTree tree = m_project->locationTree();
	if (tree.isEmpty() || scopeIsField()) {
		return 0;
	}

		//The scope, and everything it contains: a component screwed onto
		//the mounting plate of a cabinet is inside that cabinet, and
		//assembly carries it there in the same trip.
	QSet<QString> wanted;
	const QString scope = scopePath();
	const QStringList all_paths = tree.paths();
	if (scope.isEmpty())
	{
		wanted = QSet<QString>(all_paths.begin(), all_paths.end());
	}
	else
	{
		const QString prefix = scope + ProjectLocation::separator();
		for (const QString &path : all_paths)
		{
			if (path == scope || path.startsWith(prefix)) {
				wanted.insert(path);
			}
		}
	}

	Catalog *catalog = QETApp::catalog();
	QTreeWidgetItem *group = nullptr;
	int total = 0;

	const QList<LocationTree::BomLine> lines = tree.bomLines();
	for (const LocationTree::BomLine &line : lines)
	{
			//Only the enclosures the scope asks for count, and only
			//those count towards the quantity: asking for one cabinet
			//and being handed the quantity of the whole project would
			//be a list nobody could use.
		QStringList paths;
		for (const QString &path : line.paths)
		{
			if (wanted.contains(path)) {
				paths << LocationTree::iecTag(path);
			}
		}
		if (paths.isEmpty()) {
			continue;
		}

		QString manufacturer;
		QString reference;
		if (catalog && !line.part_code.isEmpty())
		{
			const CatalogPart part =
					line.part_revision > 0
					? catalog->partByCode(line.part_code,
							      line.part_revision)
					: catalog->partByCode(line.part_code);
			if (!part.isNull())
			{
				const QHash<QString, QString> values =
						catalog->effectiveValues(part);
				manufacturer = values.value(QETInformation::ELMT_MANUFACTURER);
				reference = values.value(QETInformation::ELMT_MANUFACTURER_REF);
			}
		}

		if (!group)
		{
			group = new QTreeWidgetItem(m_tree);
			group->setText(DesignationColumn,
				       tr("Armoires et supports"));
			group->setFirstColumnSpanned(false);
			QFont font = group->font(DesignationColumn);
			font.setBold(true);
			for (int i = 0 ; i < ColumnCount ; ++i) {
				group->setFont(i, font);
			}
		}

		QTreeWidgetItem *item = new QTreeWidgetItem(group);
		item->setText(QuantityColumn, QString::number(paths.count()));
		item->setTextAlignment(QuantityColumn,
				       Qt::AlignRight | Qt::AlignVCenter);
		item->setText(PartColumn, partLabel(line.part_code,
						    line.part_revision));
		item->setText(DesignationColumn, line.name);
		item->setText(ManufacturerColumn, manufacturer);
		item->setText(ReferenceColumn, reference);
		item->setText(LocationColumn, paths.join(QStringLiteral(", ")));

		total += paths.count();
	}

	if (group) {
		group->setText(QuantityColumn, QString::number(total));
		group->setTextAlignment(QuantityColumn,
					Qt::AlignRight | Qt::AlignVCenter);
	}

	return total;
}

/**
	@brief LocationBomDialog::fillComponents
	@return how many components the scope asks for
	The other half, and it is read from element_nomenclature_view rather
	than from the folios on purpose. That view is what the printed
	nomenclature reads, so the two cannot disagree; and it is the view
	itself that leaves out what is flagged out of the bill of materials, so
	this window honours that flag without ever mentioning it. A walk over
	the folios would have to reimplement the rule, and the day the two
	implementations drifted apart, the panel would be built from a list
	that the nomenclature contradicts.
*/
int LocationBomDialog::fillComponents()
{
	if (!m_project || !m_project->dataBase()) {
		return 0;
	}

	QString filter;
	if (scopeIsField())
	{
			//Decision F: an empty location_path is an answer. A push
			//button on the machine frame is bought like everything
			//else, and a list that hid it would be short.
		filter = QStringLiteral(" AND (location_path IS NULL"
					" OR location_path = '')");
	}
	else if (!scopePath().isEmpty())
	{
		const LocationTree tree = m_project->locationTree();
		const QString scope = scopePath();
		const QString prefix = scope + ProjectLocation::separator();

		QStringList quoted_paths;
		const QStringList all_paths = tree.paths();
		for (const QString &path : all_paths)
		{
			if (path == scope || path.startsWith(prefix)) {
				quoted_paths << quoted(path);
			}
		}
		if (quoted_paths.isEmpty()) {
			quoted_paths << quoted(scope);
		}

		filter = QStringLiteral(" AND location_path IN (")
				+ quoted_paths.join(QStringLiteral(", "))
				+ QStringLiteral(")");
	}

	const QString columns = QStringLiteral("part_code, part_revision,"
					       " designation, manufacturer,"
					       " manufacturer_reference,"
					       " location_path");

	const QString query_str = QStringLiteral("SELECT COUNT(*), ")
			+ columns
			+ QStringLiteral(" FROM element_nomenclature_view")
			+ typeClause()
			+ filter
			+ QStringLiteral(" GROUP BY ") + columns
			+ QStringLiteral(" ORDER BY location_path, designation,"
					 " part_code");

	m_project->dataBase()->updateDB();
	QSqlQuery query = m_project->dataBase()->newQuery(query_str);
	if (!query.exec())
	{
		say(tr("La liste des composants n'a pas pu être établie : %1")
		    .arg(query.lastError().text()), true);
		return 0;
	}

	QTreeWidgetItem *group = nullptr;
	int total = 0;

	while (query.next())
	{
		const int quantity = query.value(0).toInt();
		const QString part_code = query.value(1).toString();
		const int revision = query.value(2).toInt();
		const QString designation = query.value(3).toString();
		const QString manufacturer = query.value(4).toString();
		const QString reference = query.value(5).toString();
		const QString path = query.value(6).toString();

		if (!group)
		{
			group = new QTreeWidgetItem(m_tree);
			group->setText(DesignationColumn, tr("Composants"));
			QFont font = group->font(DesignationColumn);
			font.setBold(true);
			for (int i = 0 ; i < ColumnCount ; ++i) {
				group->setFont(i, font);
			}
		}

		QTreeWidgetItem *item = new QTreeWidgetItem(group);
		item->setText(QuantityColumn, QString::number(quantity));
		item->setTextAlignment(QuantityColumn,
				       Qt::AlignRight | Qt::AlignVCenter);
		item->setText(PartColumn, partLabel(part_code, revision));
		item->setText(DesignationColumn, designation);
		item->setText(ManufacturerColumn, manufacturer);
		item->setText(ReferenceColumn, reference);
		item->setText(LocationColumn,
			      path.isEmpty() ? tr("Chantier")
					     : LocationTree::iecTag(path));

		total += quantity;
	}

	if (group) {
		group->setText(QuantityColumn, QString::number(total));
		group->setTextAlignment(QuantityColumn,
					Qt::AlignRight | Qt::AlignVCenter);
	}

	return total;
}

/**
	@brief LocationBomDialog::asText
	@param separator
	@return the list as text, one line per row, group name included
*/
QString LocationBomDialog::asText(const QString &separator) const
{
	QStringList lines;

	QStringList header;
	header << tr("Groupe")
	       << tr("Qté")
	       << tr("Pièce")
	       << tr("Désignation")
	       << tr("Fabricant")
	       << tr("Référence")
	       << tr("Localisation");
	lines << header.join(separator);

	for (int g = 0 ; g < m_tree->topLevelItemCount() ; ++g)
	{
		QTreeWidgetItem *group = m_tree->topLevelItem(g);
		for (int i = 0 ; i < group->childCount() ; ++i)
		{
			QTreeWidgetItem *item = group->child(i);
			QStringList values;
			values << group->text(DesignationColumn);
			for (int c = 0 ; c < ColumnCount ; ++c) {
				values << item->text(c);
			}
			lines << values.join(separator);
		}
	}

	return lines.join(QStringLiteral("\n"));
}

/**
	@brief LocationBomDialog::copyToClipboard
*/
void LocationBomDialog::copyToClipboard()
{
	QApplication::clipboard()->setText(asText(QStringLiteral("\t")));
	say(tr("Liste copiée dans le presse-papiers."));
}

/**
	@brief LocationBomDialog::exportCsv
*/
void LocationBomDialog::exportCsv()
{
	const QString path = QFileDialog::getSaveFileName(
				this,
				tr("Exporter la liste de matériel"),
				QString(),
				tr("Fichier CSV (*.csv)"));
	if (path.isEmpty()) {
		return;
	}

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		say(tr("Le fichier « %1 » n'a pas pu être écrit.").arg(path),
		    true);
		return;
	}

	QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	stream.setCodec("UTF-8");
#endif
	stream << asText(QStringLiteral(";")) << "\n";
	file.close();

	say(tr("Liste exportée dans « %1 ».").arg(path));
}

/**
	@brief LocationBomDialog::insertOnFolio
	CU-32.5. What goes onto the folio is not a picture of this window: it is
	a nomenclature table, the same kind the program already knows how to
	build, restricted to this location. That matters because the table then
	behaves like the rest of the project - it follows the components as they
	change, it prints, and the standard properties dialog can still edit it.
	A frozen copy of what this window shows today would be wrong by the end
	of the week.
*/
void LocationBomDialog::insertOnFolio()
{
	if (!m_project) {
		return;
	}

		//Written the way ElementQueryWidget writes it, because that widget
		//parses the query back into its controls rather than storing it.
	const QString columns = QStringLiteral("label, designation, manufacturer,"
					       " manufacturer_reference,"
					       " part_code, location_path");

	QString filter;
	if (scopeIsField())
	{
		filter = QStringLiteral(" AND (location_path IS NULL"
					" OR location_path = '')");
	}
	else if (!scopePath().isEmpty())
	{
		const QString scope = scopePath();
		const QString prefix = scope + ProjectLocation::separator();

		bool has_children = false;
		const QStringList all_paths = m_project->locationTree().paths();
		for (const QString &path : all_paths)
		{
			if (path.startsWith(prefix)) {
				has_children = true;
				break;
			}
		}

			//The filter vocabulary of the query widget holds one
			//condition per column, so a location that contains others
			//cannot be written as a list. It is written as a
			//containment instead: leaving the sub-locations out would
			//leave material out of the list, which is the worse of the
			//two mistakes.
		filter = has_children
			 ? QStringLiteral(" AND location_path LIKE'%")
					+ scope + QStringLiteral("%'")
			 : QStringLiteral(" AND location_path='")
					+ scope + QStringLiteral("'");
	}

	const QString query = QStringLiteral("SELECT ") + columns
			+ QStringLiteral(" FROM element_nomenclature_view")
			+ typeClause()
			+ filter
			+ QStringLiteral(" ORDER BY ") + columns;

	emit insertTable(query);
	say(tr("Tableau proposé pour le folio courant."));
}

/**
	@brief LocationBomDialog::say
	@param message
	@param problem
*/
void LocationBomDialog::say(const QString &message, bool problem)
{
	if (!m_status) {
		return;
	}

	QPalette palette = m_status->palette();
	palette.setColor(QPalette::WindowText,
			 problem ? QColor(Qt::red)
				 : qApp->palette().color(QPalette::WindowText));
	m_status->setPalette(palette);
	m_status->setText(message);
}
