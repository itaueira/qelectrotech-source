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
#include "../../diagram.h"
#include "../../qetapp.h"
#include "../../qetinformation.h"
#include "../../qetproject.h"
#include "../../qetxml.h"
#include "../../undocommand/changeelementinformationcommand.h"
#include "../../utils/qetutils.h"
#include "../element.h"

#include <QSet>
#include <QSqlError>
#include <QSqlRecord>
#include <QUndoCommand>
#include <QUndoStack>
#include <QUuid>

namespace
{
	/**
		What joins the values of a row into one lookup key.

		0x1F is a control character, and XML 1.0 admits none below 0x20
		but tab, line feed and carriage return : a value carrying one
		could not be written into a .qet at all. So no value read back
		from a project holds this character, and two different tuples
		cannot fold into the same key.
	*/
	const QChar identity_separator(QLatin1Char('\x1f'));
}

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
	An edit of a cell goes to the component the row stands for ; any other
	role is the styling of the whole table, which is carried by the cell
	(0,0) and stored nowhere else.
	@param index
	@param value
	@param role
	@return

	@par Editing never writes into the data base
	The base is derived from the project : it is emptied and filled again
	from the sheets. A value written straight into it would draw a list
	holding something the drawing does not hold, and would vanish the next
	time the base is filled - which is the one failure this whole path
	exists to make impossible. What happens instead is the ordinary round
	trip : the command writes the component, the component tells the base,
	and the base tells this model to fill itself again. Nothing is written
	into m_record here either, for the same reason : the value shown comes
	back from the project or it does not come back at all.
*/
bool ProjectDBModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if (index.isValid() &&
	    role == Qt::EditRole &&
	    flags(index).testFlag(Qt::ItemIsEditable)) {
		return writeInformation(index, value.toString());
	}

		//The styling of the table - its font, its alignment, its margins -
		//is read and written on the cell (0,0), with roles that are never
		//the edit role : the editor of the table item writes Qt::FontRole,
		//Qt::TextAlignmentRole and Qt::UserRole+1 there, and toXml() saves
		//those three. So the branch above cannot shadow this one.
	if (!index.isValid() || index.row() != 0 || index.column() != 0) {
		return false;
	}
	m_index_0_0_data.insert(role, value);
	emit dataChanged(index, index, {role});
	return true;
}

/**
	@brief ProjectDBModel::writeInformation
	Push the change of one information of one component onto the undo stack
	of the project.
	@param index : the cell being edited
	@param value : what the reader typed
	@return true when a command was pushed

	@par Why the command is wrapped instead of pushed on its own
	ChangeElementInformationCommand answers 1 to id() and merges with any
	other command of its kind touching the same component, which is what a
	properties dialogue wants : a dozen fields applied at once are one undo
	step. A table is edited one cell at a time, and that same merge would
	fold an edit of the designation and an edit of the comment into a single
	step - one undo, two cells back, with nothing on screen to say so. The
	wrapper is a plain QUndoCommand, whose id() is -1 and which therefore
	never merges ; the work is still done by the command of the object,
	which is its child, and QUndoCommand::redo() runs it.
*/
bool ProjectDBModel::writeInformation(const QModelIndex &index, const QString &value)
{
	Element *element_ = elementForRow(index.row());
	if (!element_ || !element_->diagram()) {
		return false;
	}

	const QString key_ = m_column_names.at(index.column());
	const DiagramContext old_information = element_->elementInformations();
	if (old_information.value(key_).toString() == value) {
			//Nothing to undo : a reader who opens a cell and closes it
			//without typing must not leave a step on the stack.
		return false;
	}

	DiagramContext new_information = old_information;
	new_information.addValue(key_, value);

	QString name_ = QETInformation::translatedInfoKey(key_);
	if (name_.isEmpty()) {
		name_ = key_;
	}

	auto *undo_ = new QUndoCommand(tr("Modifier %1 de l'élément : %2")
				       .arg(name_, element_->name()));
	new ChangeElementInformationCommand(element_,
					    old_information,
					    new_information,
					    undo_);
	element_->diagram()->undoStack().push(undo_);
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
	
		//Answered before the styling of the cell (0,0) below, and the
		//order is the point : a view opens its editor with the value of
		//the edit role, and (0,0) answers every role but the display one
		//out of the hash carrying the font and the margins of the table.
		//Left after it, the first cell of the list would open an editor
		//holding nothing, and a reader who pressed Enter without typing
		//would write that nothing over the value.
		//And it is the stored string, not the drawn one : a location path
		//is stored as the tree writes it and drawn as the norm writes it,
		//and any value a variant reads as a date is drawn in the locale of
		//the machine. Handing the drawn form to an editor makes a reader
		//who opens a cell and closes it write something else than what was
		//there.
	if (role == Qt::EditRole)
	{
		const int identity_ = m_identity_of_column.value(index.column(), -1);
		if (identity_ >= 0 &&
			index.row() < m_row_identity.count() &&
			identity_ < m_row_identity.at(index.row()).count()) {
			return m_row_identity.at(index.row()).at(identity_);
		}
	}
	
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
	@brief ProjectDBModel::readOnlyInfoKeys
	@return the element information keys a cell of this table must not
	write, and the reason each of them is on the list.

	Every other key of QETInformation::elementInfoKeys() is free text the
	reader types about a component, and a cell writes it whole. These are
	the ones a cell cannot write whole :

	- exclude_from_bom is not a sentence but a flag, and the nomenclature
	  view filters on it. A row that reaches this table is by construction
	  a row whose flag is off, so the only edit a reader could make here is
	  the one that deletes the row from under the cursor. It has a check box
	  of its own in the properties of the component, and the same reason is
	  written down where that dialogue leaves it out of its rows.

	- location_path is stored as the location tree writes it and drawn as
	  the norm writes it, so what the cell shows is not what the project
	  holds, and it belongs to the tree rather than to the component : a
	  path typed here would name no node.

	- part_code and part_revision are written by the assignment of a
	  catalogue part, together with everything else that describes the
	  part. A code typed on its own would name no part, and would leave the
	  designation, the manufacturer and the reference describing another
	  one.

	- none of the plc_ keys is free text, and they are not all on this list
	  for the same reason. plc_type, plc_address, plc_function, plc_comment
	  and plc_crossref are copied from the input or output the component is
	  linked to and written again every time that link is made or remade,
	  so anything typed here is overwritten without a word. plc_bus is a
	  mark taken from a fixed set of values, picked from a menu ; a sentence
	  typed in its place names no rail. plc_unit names the card a point
	  belongs to, and the tree of inputs and outputs is built on it.

	label is not on this list because it is not read only everywhere : it
	is read only on the rows whose component carries a formula, which is a
	question about a row and is asked in flags().
*/
QStringList ProjectDBModel::readOnlyInfoKeys()
{
	return QStringList{QStringLiteral("exclude_from_bom"),
			   QETInformation::ELMT_LOCATION_PATH,
			   QETInformation::ELMT_PART_CODE,
			   QETInformation::ELMT_PART_REVISION,
			   QETInformation::ELMT_PLC_TYPE,
			   QETInformation::ELMT_PLC_ADDRESS,
			   QETInformation::ELMT_PLC_FUNCTION,
			   QETInformation::ELMT_PLC_COMMENT,
			   QETInformation::ELMT_PLC_CROSSREF,
			   QETInformation::ELMT_PLC_UNIT,
			   QETInformation::ELMT_PLC_BUS};
}

/**
	@brief ProjectDBModel::isEditableColumn
	@param column
	@return whether writing this column means writing one information of one
	component, and nothing else.

	A column that is not an element information key answers no, and that is
	what keeps every derived column out without naming any of them : the six
	columns of the join the view adds - the sheet, its title, the position -
	are not information keys, and neither is the alias a query invents for a
	total, such as the COUNT a bill of materials groups by. There is nothing
	for a cell of those to write to.
*/
bool ProjectDBModel::isEditableColumn(int column) const
{
	if (column < 0 || column >= m_column_names.count()) {
		return false;
	}
	if (m_identity_of_column.value(column, -1) < 0) {
		return false;
	}
	return !readOnlyInfoKeys().contains(m_column_names.at(column));
}

/**
	@brief ProjectDBModel::flags
	Reimplemented from QAbstractTableModel.
	@param index
	@return

	@par A cell is editable only when it is known what it would write on
	Besides the column being one a cell may write, the row has to stand for
	exactly one component : a row that stands for none - or for several, as
	every row of a grouped list does - has no component to push a command
	against. Asked here rather than only in setData(), so that no editor
	opens on a cell that would refuse the text afterwards ; a reader who
	types into a cell and loses what was typed has no way to tell that from
	a program that lost it.

	And the label of a component is editable only while no formula drives
	it. The column carries Element::actualLabel(), which is the result of
	the formula when there is one, and setElementInformations() writes that
	result back over anything put in its place - so the edit would be
	accepted, the undo step would be on the stack, and the cell would go
	back to saying what it said before.
*/
Qt::ItemFlags ProjectDBModel::flags(const QModelIndex &index) const
{
	const Qt::ItemFlags flags_ = QAbstractTableModel::flags(index);
	if (!index.isValid() || !isEditableColumn(index.column())) {
		return flags_;
	}

	Element *element_ = elementForRow(index.row());
	if (!element_ || !element_->diagram()) {
		return flags_;
	}

	if (m_column_names.at(index.column()) == QETInformation::ELMT_LABEL &&
		!element_->elementInformations()
			.value(QETInformation::ELMT_FORMULA).toString().isEmpty()) {
		return flags_;
	}

	return flags_ | Qt::ItemIsEditable;
}

/**
	@brief ProjectDBModel::elementForRow
	@param row
	@return the one component the row stands for, or nullptr.
*/
Element *ProjectDBModel::elementForRow(int row) const
{
	if (row < 0 || row >= m_record.count()) {
		return nullptr;
	}
	if (!m_rows_resolved) {
		resolveRowElements();
	}
	return row < m_row_element.count() ? m_row_element.at(row).data()
					   : nullptr;
}

/**
	@brief ProjectDBModel::resolveRowElements
	Trace every row back to the component it stands for, once, for the whole
	table.

	@par Why by value and not by key
	element_nomenclature_view publishes what
	QETInformation::elementInfoKeys() declares plus six columns of the join,
	and the uuid of the component is in neither set : there is no key to
	carry. So the information columns the query happens to select are read
	back out of element_info - the very table the view is built from, which
	is what keeps a value and its copy from drifting apart - and a row
	belongs to a component when that component is the only one answering to
	all of them at once.

	@par What it refuses, and why refusing is the answer
	Two components written the same way in every column the list shows are
	two components a reader cannot tell apart either ; both rows answer
	nullptr and neither is editable. A grouped list falls out of the same
	rule without being named : its row stands for the whole group, the group
	holds several components, and the tuple matches all of them.
	The refusal is wider than it has to be, and knowingly : element_info
	holds every component of the project, while the query may have filtered
	the list down to a few, so a tuple shared with a component the list does
	not show refuses a row that was unambiguous on screen. Read only is the
	safe end of that mistake.
*/
void ProjectDBModel::resolveRowElements() const
{
	m_rows_resolved = true;
	m_row_element = QVector<QPointer<Element>>(m_record.count());

	if (m_identity_columns.isEmpty() || m_record.isEmpty() || !m_project) {
		return;
	}

		//Built out of names this program owns and nothing else :
		//fillValue() only keeps a column name that is in
		//QETInformation::elementInfoKeys(), so nothing a project or a
		//reader wrote reaches this statement.
	QSqlQuery query_ = m_project->dataBase()->newQuery(
				QStringLiteral("SELECT element_uuid,")
				+ m_identity_columns.join(QLatin1Char(','))
				+ QStringLiteral(" FROM element_info"));
	if (!query_.isActive()) {
		return;
	}

	QHash<QString, QUuid> uuid_of_tuple;
	QSet<QString> shared_tuples;
	while (query_.next())
	{
		QStringList tuple_;
		for (int i = 0 ; i < m_identity_columns.count() ; ++i) {
			tuple_ << query_.value(i + 1).toString();
		}

		const QString key_ = tuple_.join(identity_separator);
		if (uuid_of_tuple.contains(key_)) {
			shared_tuples.insert(key_);
			continue;
		}
		uuid_of_tuple.insert(key_, QUuid(query_.value(0).toString()));
	}

	QHash<QUuid, Element *> element_of_uuid;
	const QList<Diagram *> diagrams_ = m_project->diagrams();
	for (Diagram *diagram_ : diagrams_)
	{
		const QList<Element *> elements_ = diagram_->elements();
		for (Element *element_ : elements_) {
			element_of_uuid.insert(element_->uuid(), element_);
		}
	}

	for (int row = 0 ;
	     row < m_row_identity.count() && row < m_row_element.count() ;
	     ++row)
	{
		const QString key_ = m_row_identity.at(row).join(identity_separator);
		if (shared_tuples.contains(key_)) {
			continue;
		}
		m_row_element[row] = element_of_uuid.value(uuid_of_tuple.value(key_));
	}
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

		//fillValue() above dropped the cache of row -> component, and
		//then the record it is indexed by was swapped twice. Dropping it
		//again here is what makes that juggling invisible to it : whoever
		//asks next rebuilds it against the record that is finally in
		//place, and not against the one that was held for two statements.
	m_rows_resolved = false;
	m_row_element.clear();
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
	m_identity_columns.clear();
	m_identity_of_column.clear();
	m_row_identity.clear();
	m_row_element.clear();
	m_rows_resolved = false;
	
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
		//Which of those columns are element information, and where the
		//value of each one sits in the tuple that identifies a row.
		//Worked out here, once for the whole pass, because it is asked
		//again for every cell of every row.
	const QStringList info_keys = QETInformation::elementInfoKeys();
	for (auto i=0 ; i<fields_.count() ; ++i)
	{
		const QString field_name = fields_.fieldName(i);
		m_column_names << field_name;

		if (info_keys.contains(field_name))
		{
			m_identity_of_column << m_identity_columns.count();
			m_identity_columns << field_name;
		}
		else
		{
				//A column of the join, or an alias a query
				//invented for a total : it holds no information
				//of a component, so it identifies none and
				//writes to none.
			m_identity_of_column << -1;
		}
	}

	while (query_.next())
	{
		QStringList record_;
		QStringList identity_;
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
			const QVariant value_ = query_.value(i);
			record_ << QETInformation::displayedInfoValue(fields_.fieldName(i),
								     value_);
				//Kept as it is stored, beside the form that is
				//drawn : this is what traces the row back to a
				//component, and what an editor is opened with.
			if (m_identity_of_column.at(i) >= 0) {
				identity_ << value_.toString();
			}
		}
		m_record << record_;
		m_row_identity << identity_;
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
