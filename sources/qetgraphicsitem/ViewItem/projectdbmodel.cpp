/*
				Copyright 2006-2026 QElectroTech Team
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
#include "projectdbmodel.h"

#include "../../dataBase/projectdatabase.h"
#include "../../qetapp.h"
#include "../../qetinformation.h"
#include "../../qetproject.h"
#include "../../qetxml.h"
#include "../../utils/qetutils.h"

#include <QSqlError>
#include <QSqlRecord>

/**
	@brief ProjectDBModel::ProjectDBModel
	@param project :project of this nomenclature
	@param parent : parent QObject

	@par A live project is required, and is not checked here
	m_project is a QPointer, which reads as if it could be empty, and the
	connect below dereferences it at once. It cannot be empty here : both
	callers pass Diagram::project() (QetGraphicsTableFactory::newTable and
	QetGraphicsTableItem::fromXml), and a diagram only ever exists with the
	project that built it, since QETProject is the only place that builds a
	Diagram, it passes itself, and Diagram never reassigns that member. The
	requirement is written down instead of guarded.
	Price : a caller that breaks it does not get an empty model, it gets a
	crash with nothing to read. QETProject::dataBase() returns the address
	of a member subobject, so on an empty m_project it answers a small
	non-null offset rather than null, and the fault lands inside connect,
	on a sender Qt has no reason to doubt : there is no invalid nullptr
	parameter warning to find in the log. Guarding this connect alone would
	buy the opposite trade, and a worse one : rowCount(), columnCount() and
	data() read m_record alone, and setQuery() already skips its work
	without a project, so the model would live on and draw an empty box on
	the folio for good, a nomenclature quietly missing instead of a stop at
	the mistake, while setHeaderString() and fillValue() would still
	dereference m_project unguarded.
*/
ProjectDBModel::ProjectDBModel(QETProject *project, QObject *parent) :
	QAbstractTableModel(parent),
	m_project(project)
{
	connect(m_project->dataBase(), &projectDataBase::dataBaseUpdated, this, &ProjectDBModel::dataBaseUpdated);
}

/**
	@brief ProjectDBModel::ProjectDBModel
	@param other_model

	@par The copied model always carries a live project
	This constructor takes m_project from other_model and dereferences it
	at once, under the same requirement as the constructor above and for
	its own reason : its only caller is
	QetGraphicsTableItem::setPreviousTable, which copies a model it got by
	a checked cast out of a QPointer, so a destroyed model reads back null
	there and is shared instead of copied ; and every ProjectDBModel is a
	QObject child of the project it points at, here (the copy takes the
	parent of its source) as at the two places that build one, so a model
	is destroyed with its project and never outlives it.
	Price : the same as above, plus one more reader to keep honest. Any
	future ProjectDBModel built with something other than its own project
	as parent breaks the argument without a word, and the fault shows up
	in the connect below rather than where the parent was chosen.
*/
ProjectDBModel::ProjectDBModel(const ProjectDBModel &other_model) :
	QAbstractTableModel(other_model.parent())
{
	this->setParent(other_model.parent());
	m_project = other_model.m_project;
	connect(m_project->dataBase(), &projectDataBase::dataBaseUpdated, this, &ProjectDBModel::dataBaseUpdated);
	m_index_0_0_data = other_model.m_index_0_0_data;
	setQuery(other_model.queryString());
}

/**
	@brief ProjectDBModel::rowCount
	Reimplemented for QAbstractTableModel
	@param parent
	@return
*/
int ProjectDBModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	
	return  m_record.count();
}

/**
	@brief ProjectDBModel::columnCount
	Reimplemented for QAbstractTableModel
	@param parent
	@return

	@par The columns are the ones the query asked for, not the ones the
	first row happened to bring back
	Taking the count from m_record.first() made the number of columns a
	consequence of the result : a query that found nothing and a query that
	never ran both answered 0, and the table on the folio drew the same
	empty box for both. The names now come from the record of the query
	itself, which SQLite fills as soon as the statement is valid, whether or
	not a single row matched - so a list with no item keeps its header and a
	broken query has none, and lastError() says which is which.
*/
int ProjectDBModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	
	return m_column_names.count();
}

/**
	@brief ProjectDBModel::setHeaderData
	Reimplemented from QAbstractTableModel.
	Only horizontal orientation is accepted.
	@param section
	@param orientation
	@param value
	@param role
	@return
*/
bool ProjectDBModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
	if (orientation == Qt::Vertical) {
		return false;
	}
	auto hash_ = m_header_data.value(section);
	hash_.insert(role, value);
	m_header_data.insert(section, hash_);
	emit headerDataChanged(orientation, section, section);
	return true;
}

/**
	@brief ProjectDBModel::headerData
	Reimplemented from QAbstractTableModel.
	@param section
	@param orientation
	@param role
	@return
*/
QVariant ProjectDBModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Vertical) {
		return QVariant();
	}
	
	if (m_header_data.contains(section))
	{
		auto hash_ = m_header_data.value(section);
		if (role == Qt::DisplayRole && !hash_.contains(Qt::DisplayRole)) { //special case to have the same behavior as Qt
			return hash_.value(Qt::EditRole);
		}
		return m_header_data.value(section).value(role);
	}
	return QVariant();
}

/**
	@brief ProjectDBModel::setData
	Only store the data for the index 0.0
	@param index
	@param value
	@param role
	@return
*/
bool ProjectDBModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if (!index.isValid() || index.row() != 0 || index.column() != 0) {
		return false;
	}
	m_index_0_0_data.insert(role, value);
	emit dataChanged(index, index, {role});
	return true;
}

/**
	@brief ProjectDBModel::data
	Reimplemented for QAbstractTableModel
	@param index
	@param role
	@return
*/
QVariant ProjectDBModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();
	
	if (index.row() == 0 &&
		index.column() == 0 &&
		role != Qt::DisplayRole) {
		return m_index_0_0_data.value(role);
	}
	
	if (role == Qt::DisplayRole)
	{
			//columnCount() no longer comes from the row that is being
			//read, so a caller can legitimately ask for a cell of a row
			//that is shorter than the table is wide. QList::at() on a
			//bad index is undefined behaviour, and an empty cell is the
			//right answer here.
		if (index.row() >= m_record.count() ||
			index.column() >= m_record.at(index.row()).count()) {
			return QVariant();
		}
		QVariant v(m_record.at(index.row()).at(index.column()));
		return v;
	}
	
	return QVariant();
}

/**
	@brief ProjectDBModel::setQuery
	Query the internal bd with query.
	@param query
*/
void ProjectDBModel::setQuery(const QString &query)
{
	auto rm_ = m_query != query;
	if (rm_) {
		emit beginResetModel();
	}

	m_query = query;
	
	if (m_project)
	{
		if (rm_) {
			disconnect(m_project->dataBase(),
				   &projectDataBase::dataBaseUpdated,
				   this,
				   &ProjectDBModel::dataBaseUpdated);
		}
		m_project->dataBase()->updateDB();
		if (rm_) {
			setHeaderString();
			fillValue();
			connect(m_project->dataBase(),
				&projectDataBase::dataBaseUpdated,
				this,
				&ProjectDBModel::dataBaseUpdated);
		}
	}
	
	if (rm_) {
		emit endResetModel();
	}
}

/**
	@brief ProjectDBModel::queryString
	@return the current query used by this model
*/
QString ProjectDBModel::queryString() const
{
	return m_query;
}

QETProject *ProjectDBModel::project() const
{
	return m_project.data();
}

/**
	@brief ProjectDBModel::toXml
	Save the model to xml,since model can have unlimited data we only save few data (only these used by qelectrotech).
	The query, all header data. and some data of index::(0,0). All other data are not saved.
	@param document
	@return
*/
QDomElement ProjectDBModel::toXml(QDomDocument &document) const
{
	auto dom_element = document.createElement(xmlTagName());
	
	//Identifier
	auto dom_identifier = document.createElement("identifier");
	auto dom_identifier_text = document.createTextNode(m_identifier);
	dom_identifier.appendChild(dom_identifier_text);
	dom_element.appendChild(dom_identifier);
	
	//query
	auto dom_query = document.createElement("query");
	auto dom_query_text = document.createTextNode(m_query);
	dom_query.appendChild(dom_query_text);
	dom_element.appendChild(dom_query);
	
	//Add index 0,0 data
	auto index_00 = document.createElement("index00");
	index_00.setAttribute("font", QETUtils::fontToString(m_index_0_0_data.value(Qt::FontRole).value<QFont>()));
	auto me = QMetaEnum::fromType<Qt::Alignment>();
	index_00.setAttribute("alignment", me.valueToKey(m_index_0_0_data.value(Qt::TextAlignmentRole).toInt()));
	dom_element.appendChild(index_00);
	index_00.setAttribute("margins", m_index_0_0_data.value(Qt::UserRole+1).toString());
	
	//header data
	QHash<int, QList<int>> horizontal_;
	for (auto key : m_header_data.keys())
	{
		//We save all data except the display role, because he was generated in the fly
		auto list = m_header_data.value(key).keys();
		list.removeAll(Qt::DisplayRole);
		
		horizontal_.insert(key, list);
	}
	
	dom_element.appendChild(QETXML::modelHeaderDataToXml(document, this, horizontal_, QHash<int, QList<int>>()));
	
	return dom_element;
}

/**
	@brief ProjectDBModel::fromXml
	Restore the model from xml
	@param element
*/
void ProjectDBModel::fromXml(const QDomElement &element)
{
	if (element.tagName() != xmlTagName())
		return;
	
	setIdentifier(element.firstChildElement("identifier").text());
	setQuery(element.firstChildElement("query").text());
	
	//Index 0,0
	auto index_00 = element.firstChildElement("index00");
	QFont font_;
	QETUtils::fontFromString(font_, index_00.attribute("font"));
	m_index_0_0_data.insert(Qt::FontRole, font_);
	auto me = QMetaEnum::fromType<Qt::Alignment>();
	m_index_0_0_data.insert(Qt::TextAlignmentRole, me.keyToValue(index_00.attribute("alignment").toStdString().data()));
	m_index_0_0_data.insert(Qt::UserRole+1, index_00.attribute("margins"));
	
	QETXML::modelHeaderDataFromXml(element.firstChildElement("header_data"), this);
}

/**
	@brief ProjectDBModel::setIdentifier
	Set the identifier of this model to identifier
	@param identifier
*/
void ProjectDBModel::setIdentifier(const QString &identifier) {
	m_identifier = identifier;
}

/**
	@brief ProjectDBModel::dataBaseUpdated
	slot called when the project database is updated
*/
void ProjectDBModel::dataBaseUpdated()
{
	auto original_record = m_record;
	fillValue();
	auto new_record = m_record;
	m_record = original_record;
	
	if (new_record.size() != m_record.size())
	{
		emit beginResetModel();
		m_record = new_record;
		emit endResetModel();
	}
	else
	{
		m_record = new_record;
		auto row = m_record.size();
		auto col = row ? m_record.first().count() : 1;
		
		emit dataChanged(this->index(0,0), this->index(row-1, col-1), {Qt::DisplayRole});
	}
}

void ProjectDBModel::setHeaderString()
{
	auto q = m_project->dataBase()->newQuery(m_query);
	auto record = q.record();
	
	for (auto i=0 ; i<record.count() ; ++i)
	{
		auto field_name = record.fieldName(i);
		QString header_name;
		
		if (field_name == "position") {
			header_name = tr("Position");
		} else if (field_name == "diagram_position") {
			header_name = tr("Position du folio");
		} else {
			header_name = QETInformation::translatedInfoKey(field_name);
			if (header_name.isEmpty()) {
				header_name = field_name;
			}
		}
		this->setHeaderData(i, Qt::Horizontal, header_name, Qt::DisplayRole);
	}
}

/**
	@brief ProjectDBModel::fillValue
	Run the current query and keep what it returned : the name of each
	column, the value of each cell, and - when it did not run - the reason.

	@par A failure stops here instead of going on with an empty record
	It used to be written to qDebug() and left behind, so the model went on
	filling nothing and the table drew a box with no column and no row : on
	the folio, a query that could not be run looked exactly like a list with
	nothing in it. The error is now state of the model, lastError() says it
	and queryErrorChanged() announces it.
*/
void ProjectDBModel::fillValue()
{
	m_record.clear();
	m_column_names.clear();
	
	if (m_query.trimmed().isEmpty())
	{
			//No column chosen : the query widgets refuse to build a
			//SELECT with no column rather than handing over a broken
			//one, and this is where that refusal becomes a sentence.
		setLastError(tr("Aucune colonne n'a été choisie : il n'y a rien à afficher."));
		return;
	}

		//newQuery() returns QSqlQuery(query, db), and that constructor
		//executes the query : what comes back is already a result set,
		//which is why setHeaderString() above reads record() off it
		//without executing anything. Asking it to exec() a second time
		//did more than double the work - on a statement that failed to
		//prepare it also threw the reason away, and lastError() then
		//read "No query Unable to fetch row" instead of the "no such
		//column" the data base had answered. Measured, not supposed :
		//it is what the test of this behaviour reported first.
	auto query_ = m_project->dataBase()->newQuery(m_query);
	if (!query_.isActive())
	{
		const auto error_ = query_.lastError().text().trimmed();
		setLastError(error_.isEmpty()
			     ? tr("La liste n'a pas pu être établie.")
			     : tr("La liste n'a pas pu être établie : %1").arg(error_));
		return;
	}
	setLastError(QString());
	
		//Which information each column holds, and not only the value it
		//holds: a stored form and a drawn form are not always the same
		//string, and the column name is what tells them apart. Taken once,
		//outside the row loop: the query is fixed for the whole pass, only
		//the row moves.
	const auto fields_ = query_.record();
	for (auto i=0 ; i<fields_.count() ; ++i) {
		m_column_names << fields_.fieldName(i);
	}

	while (query_.next())
	{
		QStringList record_;
			//One value per column of the query, so that a row can
			//never come back shorter than the table is wide - which
			//is what columnCount() now answers. The former form
			//stopped at the first value the driver reads as invalid,
			//and a NULL in the middle of a row was taken to be one
			//of those. Measured, and it is not : SQLite hands a null
			//value back as a null variant that is still valid, so
			//planting the old loop again leaves every row exactly as
			//it is. What changed here is the guarantee, not the
			//rows - and the guarantee is what columnCount() rests on.
		for (auto i=0 ; i<fields_.count() ; ++i)
		{
			record_ << QETInformation::displayedInfoValue(fields_.fieldName(i),
								     query_.value(i));
		}
		m_record << record_;
	}
}

/**
	@brief ProjectDBModel::setLastError
	Set the error state of the model to @a error, and tell the views when it
	changed. An empty string means the query runs.
	@param error
*/
void ProjectDBModel::setLastError(const QString &error)
{
	if (m_last_error == error) {
		return;
	}
	m_last_error = error;
	emit queryErrorChanged(m_last_error);
}
