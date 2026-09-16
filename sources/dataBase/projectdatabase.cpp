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
#include "projectdatabase.h"

#include "bomquery.h"
#include "../diagram.h"
#include "../diagramposition.h"
#include "../elementprovider.h"
#include "../qetapp.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetinformation.h"
#include "../qetproject.h"

#include <QLocale>
#include <QSqlError>

#include <QSqlDriver>
#include <sqlite3.h>


/**
	@brief projectDataBase::projectDataBase
	Default constructor
	@param project : project from the database work
	@param parent : parent QObject
*/
projectDataBase::projectDataBase(QETProject *project, QObject *parent) :
	QObject(parent),
	m_project(project)
{
	createDataBase();
	connect(m_project, &QETProject::diagramAdded, [this](QETProject *, Diagram *diagram) {
		this->addDiagram(diagram);
	});
	connect(m_project, &QETProject::diagramRemoved, [this](QETProject *, Diagram *diagram) {
		this->removeDiagram(diagram);
	});
	connect(m_project, &QETProject::projectDiagramsOrderChanged, [this]()
	{
		for (auto diagram : m_project->diagrams())
		{
			m_diagram_order_changed.bindValue(":pos", m_project->folioIndex(diagram)+1);
			m_diagram_order_changed.bindValue(":uuid", diagram->uuid());
			m_diagram_order_changed.exec();


			m_diagram_info_order_changed.bindValue(":folio", diagram->border_and_titleblock.titleblockInformation().value("folio"));
			m_diagram_info_order_changed.bindValue(":uuid", diagram->uuid());
			m_diagram_info_order_changed.exec();

		}
		notifyUpdated();
	});
}

/**
	@brief projectDataBase::~projectDataBase
	Destructor
*/
projectDataBase::~projectDataBase()
{
	m_data_base.close();
}

/**
	@brief projectDataBase::updateDB
	Up to date the content of the data base.
	Emit the signal dataBaseUpdated
*/
void projectDataBase::updateDB()
{
	populateDiagramTable();
	populateDiagramInfoTable();
	populateElementTable();
	populateElementInfoTable();
	populateConductorTable();
	notifyUpdated();
}

/**
	@brief projectDataBase::project
	@return the project of this  database
*/
QETProject *projectDataBase::project() const
{
	return m_project;
}

/**
	@brief projectDataBase::newQuery
	@return a QSqlquery with query as query
	and the internal database of this class as database to use.
*/
QSqlQuery projectDataBase::newQuery(const QString &query) {
	return QSqlQuery(query, m_data_base);
}

/**
	@brief projectDataBase::namelessComponentCount
	See the header: the rows the parts list withholds, as a number.
*/
int projectDataBase::namelessComponentCount()
{
	const QString statement =
		QStringLiteral("SELECT COUNT(*) FROM element_label_view"
			       " WHERE (exclude_from_bom IS NOT 'true')"
			       " AND NOT ")
		+ QETBom::informationPresentCondition(
			QString(), QETInformation::elementInfoKeys());

	QSqlQuery query = newQuery(statement);
		//No alias, because the view already publishes the information
		//columns under their own names; the "ei." of the view body
		//belongs to the table underneath it and would name nothing here.
	if (!query.exec() || !query.next())
	{
			//Zero, and not a fault code. The caller reports this beside
			//a list it has already written, and a report that cannot be
			//produced must not turn a finished export into a failure.
		qDebug() << "projectDataBase::namelessComponentCount :"
			 << query.lastError();
		return 0;
	}

	return query.value(0).toInt();
}

/**
	@brief projectDataBase::populatedElementTypes
	See the header: the record of the project, wider than any list.
*/
ElementData::Types projectDataBase::populatedElementTypes()
{
	return ElementData::Simple
			| ElementData::Terminal
			| ElementData::Master
			| ElementData::Thumbnail
			| ElementData::Slave
			| ElementData::NextReport
			| ElementData::PreviousReport;
}

/**
	@brief projectDataBase::publishedElementTypes
	See the header: what a person reads, unchanged by the widening above.
*/
ElementData::Types projectDataBase::publishedElementTypes()
{
	return ElementData::Simple
			| ElementData::Terminal
			| ElementData::Master
			| ElementData::Thumbnail;
}

/**
	@brief projectDataBase::elementTypeClause
	@param types : the kinds of element the caller keeps
	@param column : the column of the element table holding the kind
	@return the SQL that keeps those kinds and nothing else, ready to be
	appended to a WHERE that already has a condition in it.

	Written out of ElementData::typeToString() rather than by hand, for the
	reason elementViewBody() records about the information keys: the strings
	in the table are the ones that function produces, so producing the
	strings of the clause the same way is what keeps a renamed kind from
	silently matching nothing. A clause of hand-written literals would go on
	parsing, go on running, and quietly answer with no row.
*/
QString projectDataBase::elementTypeClause(ElementData::Types types,
					   const QString &column)
{
		//Every kind the enumeration declares, so that a kind added later is
		//covered by whichever of the two sets names it, without a second
		//list to remember.
	static const QVector<ElementData::Type> all_types {
		ElementData::Simple,
		ElementData::NextReport,
		ElementData::PreviousReport,
		ElementData::Master,
		ElementData::Slave,
		ElementData::Terminal,
		ElementData::Thumbnail,
		ElementData::ConductorDefinition};

	QStringList names;
	for (const ElementData::Type type : all_types)
	{
		if (types.testFlag(type)) {
			names << QStringLiteral("'")
				 + ElementData::typeToString(type)
				 + QStringLiteral("'");
		}
	}

		//A set that keeps nothing would produce "IN ()", which SQLite
		//rejects: the view would not be created at all and every list would
		//come back empty. Saying so with a condition that is false is the
		//legible end of that mistake.
	if (names.isEmpty()) {
		return QStringLiteral(" AND 0");
	}

	return QStringLiteral(" AND ") + column
			+ QStringLiteral(" IN (")
			+ names.join(QLatin1Char(','))
			+ QStringLiteral(")");
}

/**
	@brief projectDataBase::addElement
	@param element

	The kinds it takes are populatedElementTypes(), which is the set the
	repopulation uses. It used to take every kind, and the disagreement was
	invisible in the one direction that matters: an element the repopulation
	would refuse was inserted here, lived in the table for as long as the
	project stayed open, and was gone the next time the file was opened.

	sub_type is written the way the repopulation writes it, and that is the
	second half of the same disagreement. This used to bind the kindInformation
	named "type", which is the master kind only for a master: for a terminal
	it is the kind of terminal, and for a relay contact the kind of contact.
	So a terminal drawn today answered 'generic' in element_sub_type and
	answered nothing at all once the project had been saved and opened again -
	the column the parts list and the material list filter on, changing under a
	list that nobody had touched.

	Both halves are closed the same way, and it is the way bindConductorValues()
	was already closed for the wires: the columns are written once, in
	bindElementValues() and bindElementInfoValues(), and both paths call them.
	The comment on bindConductorValues() said that binder existed for elements
	too. It did not - and what was missing is exactly what drifted.
*/
void projectDataBase::addElement(Element *element)
{
	if (!element || !element->diagram()) {
		qDebug() << "projectDataBase::addElement: null element or diagram";
		return;
	}

	if (!populatedElementTypes().testFlag(element->elementData().m_type)) {
		return;
	}

	bindElementValues(m_insert_elements_query, element, element->diagram());
	if (!m_insert_elements_query.exec()) {
		qDebug() << "projectDataBase::addElement insert element error : " << m_insert_elements_query.lastError();
	}

	bindElementInfoValues(m_insert_element_info_query, element);

	if (!m_insert_element_info_query.exec()) {
		qDebug() << "projectDataBase::addElement insert element info error : " << m_insert_element_info_query.lastError();
	} else {
		notifyUpdated();
	}
}

/**
	@brief projectDataBase::removeElement
	@param element
*/
void projectDataBase::removeElement(Element *element)
{
	m_remove_element_query.bindValue(":uuid", element->uuid().toString());
	if(!m_remove_element_query.exec()) {
		qDebug() << "projectDataBase::removeElement remove error : " << m_remove_element_query.lastError();
	} else {
		notifyUpdated();
	}
}

/**
	@brief projectDataBase::elementInfoChanged
	@param element
*/
void projectDataBase::elementInfoChanged(Element *element)
{
	auto hash = elementInfoToString(element);
	for (auto str : QETInformation::elementInfoKeys()) {
		m_update_element_query.bindValue(":" + str, hash.value(str));
	}
	m_update_element_query.bindValue(":uuid", element->uuid().toString());
	if (!m_update_element_query.exec()) {
		qDebug() << "projectDataBase::elementInfoChanged update error : " << m_update_element_query.lastError();
	} else {
		notifyUpdated();
	}
}

void projectDataBase::elementInfoChanged(QList<Element *> elements)
{
		//One notice for the whole list, not one per element: the single
		//element overload announces on its own, and a hundred of them would
		//make every list drawn on a folio re-run its query a hundred times.
		//
		//This used to be blockSignals(), which does the same thing here by
		//silencing every signal of this object rather than the one being
		//grouped. Going through the operation leaves one grouping mechanism
		//in this class instead of two that could disagree.
	Operation operation(this);

	m_data_base.transaction();
	for (auto elmt : elements) {
		elementInfoChanged(elmt);
	}
	m_data_base.commit();
}

void projectDataBase::addDiagram(Diagram *diagram)
{
	m_insert_diagram_query.bindValue(":uuid", diagram->uuid().toString());
	m_insert_diagram_query.bindValue(":pos", m_project->folioIndex(diagram)+1);
	if(!m_insert_diagram_query.exec()) {
		qDebug() << "projectDataBase::addDiagram insert error : " << m_insert_diagram_query.lastError();
	}

	bindDiagramInfoValues(m_insert_diagram_info_query, diagram);

	if (!m_insert_diagram_info_query.exec()) {
		qDebug() << "projectDataBase::addDiagram insert info error : " << m_insert_diagram_info_query.lastError();
	}

		//The information "folio" of other existing diagram can have the variable %total,
		//so when a new diagram is added this variable change.
		//We need to update this information in the database.
	for (auto diagram : project()->diagrams())
	{
		m_diagram_info_order_changed.bindValue(":folio", diagram->border_and_titleblock.titleblockInformation().value("folio"));
		m_diagram_info_order_changed.bindValue(":uuid", diagram->uuid());
		if (!m_diagram_info_order_changed.exec()) {
			qDebug() << "projectDataBase::addDiagram update diagram infp order error : " << m_diagram_info_order_changed.lastError();
		}
	}
	notifyUpdated();
}

void projectDataBase::removeDiagram(Diagram *diagram)
{
	m_remove_diagram_query.bindValue(":uuid", diagram->uuid().toString());
	if (!m_remove_diagram_query.exec()) {
		qDebug() << "projectDataBase::removeDiagram delete error : " << m_remove_diagram_query.lastError();
	} else {
		notifyUpdated();
	}
}

void projectDataBase::diagramInfoChanged(Diagram *diagram)
{
	bindDiagramInfoValues(m_update_diagram_info_query, diagram);

	if (!m_update_diagram_info_query.exec()) {
		qDebug() << "projectDataBase::diagramInfoChanged update error : " << m_update_diagram_info_query.lastError();
	} else {
		notifyUpdated();
	}
}

void projectDataBase::diagramOrderChanged()
{
}

/**
	@brief projectDataBase::addConductor
	@param conductor
*/
void projectDataBase::addConductor(Conductor *conductor)
{
	if (!conductor || !conductor->diagram()) {
		qDebug() << "projectDataBase::addConductor: null conductor or diagram";
		return;
	}

		//Both endpoints must belong to an element: the terminal table is keyed
		//on (terminal, element) and a terminal with no parent has no identity
		//to key on. Terminals whose *definition* predates terminal uuids are
		//fine -- Terminal::stableUuid() derives one from the terminal's local
		//position, which is what the project format itself matches on.
	if (!conductor->terminal1->parentElement()
		|| !conductor->terminal2->parentElement()) {
		return;
	}

	insertTerminal(conductor->terminal1);
	insertTerminal(conductor->terminal2);

	watchConductor(conductor);
	bindConductorValues(m_insert_conductor_query, conductor, conductor->diagram());
	if (!m_insert_conductor_query.exec()) {
		qDebug() << "projectDataBase::addConductor insert error : " << m_insert_conductor_query.lastError();
	} else {
		notifyUpdated();
	}
}

/**
	@brief projectDataBase::removeConductor
	@param conductor
*/
void projectDataBase::removeConductor(Conductor *conductor)
{
	m_remove_conductor_query.bindValue(":uuid", conductor->uuid().toString());
	if (!m_remove_conductor_query.exec()) {
		qDebug() << "projectDataBase::removeConductor delete error : " << m_remove_conductor_query.lastError();
	} else {
		notifyUpdated();
	}
}

/**
	@brief projectDataBase::updateConductor
	Refresh the mutable columns of an already-inserted conductor.

	Everything a conductor carries can change without the conductor being
	removed and re-added -- the wire number, its section, its colour, its
	function, its voltage and whether it is drawn as one wire or as a
	multi-conductor line. Only its endpoints are fixed for its lifetime.
	Without this, renaming a wire left the database holding the old number
	and the wiring list showed a stale value until the next full repopulate.

	The hook that brings the change here is watchConductor(), which has
	been listening to Conductor::propertiesChange the whole time: until the
	columns existed, this method was handed a full set of properties and
	wrote one of them down.
	@param conductor
*/
void projectDataBase::updateConductor(Conductor *conductor)
{
	if (!conductor) {
		return;
	}

	m_update_conductor_query.bindValue(QStringLiteral(":uuid"), conductor->uuid().toString());
	bindConductorProperties(m_update_conductor_query, conductor->properties());
	if (!m_update_conductor_query.exec()) {
		qDebug() << "projectDataBase::updateConductor update error : " << m_update_conductor_query.lastError();
		return;
	}

		//A write that matched no row is not a change: properties can be set
		//on a conductor before it is inserted -- pasting a circuit does
		//exactly that -- and announcing there would redraw every list on a
		//folio for a row that is not in the table. Only zero counts as
		//nothing happened: a driver that cannot tell answers -1, and an
		//answer of "do not know" must not silence a real change.
	if (m_update_conductor_query.numRowsAffected() == 0) {
		return;
	}

		//This announced nothing at all until T17, and the reason written here
		//rested on a wire_count column of element_nomenclature_view. There is
		//no such column: the view is spelled out in
		//createElementNomenclatureView() and carries no subquery of any kind.
		//It exists on a branch that was never merged. (Not to be confused
		//with the wire_count attribute a cable writes into the project file,
		//which has nothing to do with this table.)
		//
		//What the wrong reason hid is a real cost, and it argues for grouping
		//rather than for silence: renaming a wire renames the whole potential,
		//so one gesture is dozens of calls to this method, and announcing each
		//of them makes every list drawn on a folio re-run its query dozens of
		//times. Whoever knows a gesture has begun opens an operation, and the
		//notice is delivered once, at the end of it.
	notifyUpdated();
}

/**
	@brief projectDataBase::watchConductor
	Keep this conductor's row in step with its properties.

	Conductor::setProperties() has a dozen call sites (auto-numbering, the
	properties dialog, element moves, deletion re-links...), so listening to
	the signal it already emits is the only way to catch them all -- and the
	only way to catch the ones added later. Qt::UniqueConnection makes a
	repeated insert or a full repopulate harmless.
	@param conductor
*/
void projectDataBase::watchConductor(Conductor *conductor)
{
	connect(conductor, &Conductor::propertiesChange,
			this, &projectDataBase::conductorPropertiesChanged,
			Qt::UniqueConnection);
}

/**
	@brief projectDataBase::conductorPropertiesChanged
*/
void projectDataBase::conductorPropertiesChanged()
{
	if (auto *conductor = qobject_cast<Conductor *>(sender())) {
		updateConductor(conductor);
	}
}

/**
	@brief projectDataBase::beginOperation
	Open a gesture: what follows is announced once, when it closes.
*/
void projectDataBase::beginOperation()
{
	m_coalescer.beginOperation();
}

/**
	@brief projectDataBase::endOperation
	Close a gesture, announcing it if anything happened.
*/
void projectDataBase::endOperation()
{
	if (m_coalescer.endOperation()) {
		emit dataBaseUpdated();
	}
}

/**
	@brief projectDataBase::notifyUpdated
	Announce a change, now or when the open gesture ends.

	Every emission of dataBaseUpdated() outside endOperation() goes through
	here, and not only the conductor ones: a gesture that adds a folio and
	renames the wires on it is one gesture, and a grouping that only knew
	about conductors would announce the rest of it anyway.
*/
void projectDataBase::notifyUpdated()
{
	if (m_coalescer.notify()) {
		emit dataBaseUpdated();
	}
}

/**
	@brief projectDataBase::Operation::Operation
	@param data_base : the data base whose notices are grouped; nullptr
	means no grouping.
*/
projectDataBase::Operation::Operation(projectDataBase *data_base) :
	m_data_base(data_base)
{
	if (m_data_base) {
		m_data_base->beginOperation();
	}
}

/**
	@brief projectDataBase::Operation::Operation
	@param project : the project whose data base groups the notices;
	nullptr means no grouping.
*/
projectDataBase::Operation::Operation(QETProject *project) :
	m_data_base(project ? project->dataBase() : nullptr)
{
	if (m_data_base) {
		m_data_base->beginOperation();
	}
}

/**
	@brief projectDataBase::Operation::~Operation
	Close the gesture, whatever ended it.
*/
projectDataBase::Operation::~Operation()
{
	if (m_data_base) {
		m_data_base->endOperation();
	}
}

/**
	@brief projectDataBase::bindElementValues
	One binder for both insert paths of the element table, so an element
	added to a live diagram and one read from a file can never drift apart.

	It is the binder the comment on bindConductorValues() below claimed
	already existed. It did not, and the two paths had drifted in the only
	column they could: sub_type was the master kind on one side and the
	kindInformation named "type" on the other, which for a terminal is the
	kind of terminal and for a relay contact the kind of contact.

	@param query
	@param element
	@param diagram : the diagram the element is drawn on
*/
void projectDataBase::bindElementValues(QSqlQuery &query, Element *element, Diagram *diagram)
{
	const ElementData element_data = element->elementData();
	query.bindValue(QStringLiteral(":uuid"), element->uuid().toString());
	query.bindValue(QStringLiteral(":diagram_uuid"), diagram->uuid().toString());
	query.bindValue(QStringLiteral(":pos"), diagram->convertPosition(element->scenePos()).toString());
	query.bindValue(QStringLiteral(":type"), element_data.typeToString());
	query.bindValue(QStringLiteral(":sub_type"), element_data.masterTypeToString());
}

/**
	@brief projectDataBase::bindElementInfoValues
	The same, for the information table.

	Over QETInformation::elementInfoKeys() and not over the keys the hash
	happens to hold: the queries are members and are reused from one element
	to the next, so a key left unbound would keep the value the element
	before it had. elementInfoToString() fills every key, so the two are the
	same set today - binding the canonical list is what keeps them the same
	set the day it stops filling one.

	@param query
	@param element
*/
void projectDataBase::bindElementInfoValues(QSqlQuery &query, Element *element)
{
	query.bindValue(QStringLiteral(":uuid"), element->uuid().toString());

	const QHash<QString, QString> hash = elementInfoToString(element);
	for (const QString &key : QETInformation::elementInfoKeys()) {
		query.bindValue(QStringLiteral(":") + key, hash.value(key));
	}
}

/**
	@brief projectDataBase::bindConductorValues
	One binder for both insert paths, so a conductor added to a live diagram
	and one read from a file can never drift apart -- the same reason
	bindElementValues() exists for elements.
	@param query
	@param conductor
	@param diagram : the diagram the conductor belongs to
*/
void projectDataBase::bindConductorValues(QSqlQuery &query, Conductor *conductor, Diagram *diagram)
{
	query.bindValue(QStringLiteral(":uuid"), conductor->uuid().toString());
	query.bindValue(QStringLiteral(":diagram_uuid"), diagram->uuid().toString());
	query.bindValue(QStringLiteral(":terminal1_uuid"), conductor->terminal1->stableUuid().toString());
	query.bindValue(QStringLiteral(":terminal1_element_uuid"), conductor->terminal1->parentElement()->uuid().toString());
	query.bindValue(QStringLiteral(":terminal2_uuid"), conductor->terminal2->stableUuid().toString());
	query.bindValue(QStringLiteral(":terminal2_element_uuid"), conductor->terminal2->parentElement()->uuid().toString());
	bindConductorProperties(query, conductor->properties());
}

/**
	@brief projectDataBase::bindConductorProperties
	@param query : a statement holding every placeholder named below
	@param properties : the properties of the conductor being written

	@par Why the type is stored as the file stores it

	ConductorProperties::typeToString() gives back "single" or "multi",
	untranslated, and that is what goes in the column. The enumerated value
	would be a number whose meaning lives in a header, and the translated
	word would change with the interface language of whoever last opened
	the project -- neither can be compared against a project exported on
	another machine.
*/
void projectDataBase::bindConductorProperties(QSqlQuery &query, const ConductorProperties &properties)
{
	query.bindValue(QStringLiteral(":text"), properties.text);
	query.bindValue(QStringLiteral(":conductor_section"), properties.m_wire_section);
	query.bindValue(QStringLiteral(":conductor_color"), properties.m_wire_color);
	query.bindValue(QStringLiteral(":function"), properties.m_function);
	query.bindValue(QStringLiteral(":tension_protocol"), properties.m_tension_protocol);
	query.bindValue(QStringLiteral(":conductor_type"), ConductorProperties::typeToString(properties.type));
}

/**
	@brief projectDataBase::createDataBase
	Create the data base
	@return : true if the data base was successfully created.
*/
bool projectDataBase::createDataBase()
{
	m_data_base = QSqlDatabase::addDatabase("QSQLITE", "qet_project_db_" + m_project->uuid().toString());
	if(!m_data_base.open()) {
		m_data_base.close();
		return false;
	}

	QSqlQuery(m_data_base).exec("PRAGMA temp_store = MEMORY");
	QSqlQuery(m_data_base).exec("PRAGMA journal_mode = MEMORY");
	QSqlQuery(m_data_base).exec("PRAGMA synchronous = OFF");
	
	QSqlQuery query_(m_data_base);
	bool first_ = true;

	//Create diagram table
	QString diagram_table("CREATE TABLE diagram ("
						  "uuid VARCHAR(50) PRIMARY KEY NOT NULL,"
						  "pos INTEGER)");
	if (!query_.exec(diagram_table)) {
		qDebug() << "diagram_table query : "<< query_.lastError();
	}

	//Create the table element
	QString element_table("CREATE TABLE element"
						  "( "
						  "uuid VARCHAR(50) PRIMARY KEY NOT NULL, "
						  "diagram_uuid VARCHAR(50) NOT NULL,"
						  "pos VARCHAR(6) NOT NULL,"
						  "type VARCHAR(50),"
						  "sub_type VARCHAR(50),"
						  "FOREIGN KEY (diagram_uuid) REFERENCES diagram (uuid)"
						  ")");
	if (!query_.exec(element_table)) {
		qDebug() <<" element_table query : "<< query_.lastError();
	}

	//Create the diagram info table
	QString diagram_info_table("CREATE TABLE diagram_info (diagram_uuid VARCHAR(50) PRIMARY KEY NOT NULL, ");
	first_ = true;
	for (auto string : QETInformation::diagramInfoKeys())
	{
		if (first_) {
			first_ = false;
		} else {
			diagram_info_table += ", ";
		}
		diagram_info_table += string += string=="date" ? " DATE" : " VARCHAR(100)";
	}
	diagram_info_table += ", FOREIGN KEY (diagram_uuid) REFERENCES diagram (uuid))";
	if (!query_.exec(diagram_info_table)) {
		qDebug() << "diagram_info_table query : " << query_.lastError();
	}

	//Create the element info table
	QString element_info_table("CREATE TABLE element_info(element_uuid VARCHAR(50) PRIMARY KEY NOT NULL,");
	first_=true;
	for (auto string : QETInformation::elementInfoKeys())
	{
		if (first_) {
			first_ = false;
		} else {
			element_info_table += ",";
		}

		element_info_table += string += " VARCHAR(100)";
	}
	element_info_table += ", FOREIGN KEY (element_uuid) REFERENCES element (uuid));";

	if (!query_.exec(element_info_table)) {
		qDebug() << " element_info_table query : " << query_.lastError();
	}

	//Create the terminal table.
	//Terminal::uuid() is the terminal-position id baked into the catalog
	//.elmt definition (e.g. "the top terminal") -- identical across every
	//placed instance of that catalog element, not a per-instance id. A
	//terminal instance is only uniquely identified by (uuid, element_uuid)
	//together, so that pair is the primary key here, not uuid alone.
	QString terminal_table("CREATE TABLE terminal"
						  "( "
						  "uuid VARCHAR(50) NOT NULL, "
						  "element_uuid VARCHAR(50) NOT NULL,"
						  "name VARCHAR(50),"
						  "PRIMARY KEY (uuid, element_uuid),"
						  "FOREIGN KEY (element_uuid) REFERENCES element (uuid)"
						  ")");
	if (!query_.exec(terminal_table)) {
		qDebug() << "terminal_table query : "<< query_.lastError();
	}

		//Create the conductor table
		//
		//The five columns after text are what a wiring list is about, and
		//they were not here: the table carried the two endpoints and the
		//wire number, so a list built on it could say which terminals a wire
		//joins and nothing about the wire. Section, colour, function and
		//voltage are properties a designer fills in on the folio and which
		//the project file has always stored; type says whether the line
		//drawn is one conductor or a multi-conductor symbol, which is what
		//decides whether a row belongs to a wiring list or to a cable list.
		//
		//They are declared as text, including the section. A section is
		//written the way the designer writes it -- "1,5", "1.5 mm2", "AWG
		//14" -- and turning that into a number here would either refuse
		//what people type or quietly round it. Whoever groups by section
		//groups by the catalogue reference, which is the rule T17 settled
		//on, and that is a join and not a cast.
	QString conductor_table("CREATE TABLE conductor"
						  "( "
						  "uuid VARCHAR(50) PRIMARY KEY NOT NULL, "
						  "diagram_uuid VARCHAR(50) NOT NULL,"
						  "terminal1_uuid VARCHAR(50) NOT NULL,"
						  "terminal1_element_uuid VARCHAR(50) NOT NULL,"
						  "terminal2_uuid VARCHAR(50) NOT NULL,"
						  "terminal2_element_uuid VARCHAR(50) NOT NULL,"
						  "text VARCHAR(100),"
						  "conductor_section VARCHAR(50),"
						  "conductor_color VARCHAR(50),"
						  "function VARCHAR(100),"
						  "tension_protocol VARCHAR(100),"
						  "conductor_type VARCHAR(10),"
						  "FOREIGN KEY (diagram_uuid) REFERENCES diagram (uuid),"
						  "FOREIGN KEY (terminal1_uuid, terminal1_element_uuid) REFERENCES terminal (uuid, element_uuid),"
						  "FOREIGN KEY (terminal2_uuid, terminal2_element_uuid) REFERENCES terminal (uuid, element_uuid)"
						  ")");
	if (!query_.exec(conductor_table)) {
		qDebug() << "conductor_table query : "<< query_.lastError();
	}

		//No query uses these indexes yet, and the comment that stood here said
		//one did: it credited element_nomenclature_view with a correlated
		//subquery counting the wires of each element. The view is spelled out
		//in createElementNomenclatureView() and has no subquery at all -- that
		//one lives on a branch that was never merged. They are kept because
		//the wiring list joins the conductor table on exactly these three
		//columns, and are named here for what they are: paid for in advance,
		//not in use.
	for (const QString &index_ : {
			QStringLiteral("CREATE INDEX idx_conductor_terminal1_element ON conductor (terminal1_element_uuid)"),
			QStringLiteral("CREATE INDEX idx_conductor_terminal2_element ON conductor (terminal2_element_uuid)"),
			QStringLiteral("CREATE INDEX idx_conductor_diagram ON conductor (diagram_uuid)") })
	{
		if (!query_.exec(index_)) {
			qDebug() << "conductor index query : " << query_.lastError();
		}
	}

	createElementNomenclatureView();
	createElementLabelView();
	createSummaryView();
	prepareQuery();
	updateDB();
	return true;
}

/**
	@brief projectDataBase::elementViewBody
	The SELECT the two element views share: every column an element row
	carries, the join that ties an element to the sheet it is drawn on, and
	the kinds of element a list is about.

	The kinds are here and not in each view, and that is the one filtering
	this body does. What a caller states on its own is what it drops for a
	reason of its own - the bill of materials drops what the user ticked out
	of the purchase list - and that stays below. Which kinds of drawn thing
	are components at all is not a question either view answers differently,
	so writing it once is what keeps the two from drifting; written twice, a
	kind added to one of them would show up in the parts list and not on the
	label roll, or the reverse, and neither is an error anybody sees.

	It matters more since the tables underneath grew. element and
	element_info now hold every element the sheets draw - relay contacts and
	folio reference arrows included - because a wire that ends on one of them
	has to find a row for it. None of that reaches a person: the clause below
	keeps the views publishing the four kinds they have always published, so
	the parts list, the nomenclature, the label roll and the tables drawn on
	a folio answer exactly what they answered before.

	Shared rather than copied, for the reason the comment inside it already
	records: the two views below differ by one clause and by nothing else, so
	a column added to one and forgotten in the other would stay invisible
	until a field came out empty on a label, or a table came back with no
	column at all on the folio. Sharing the body makes that divergence
	unrepresentable rather than merely unlikely.

	The element information columns are generated from
	QETInformation::elementInfoKeys() rather than written out, which closes
	the same class of divergence one level up. That list already creates the
	columns of element_info and already drives the insert; this view used to
	repeat it by hand, and the repetition was forgotten twice - plc_unit and
	plc_bus shipped as keys the selector offered and the query could not
	select, and a table asking for one came back with no row and no column at
	all: empty on the folio, and silent. Both times the repair was to add the
	line that had been left out. There is no second list left to forget.
*/
QString projectDataBase::elementViewBody()
{
	QString body(QStringLiteral("SELECT "));
	for (const QString &key : QETInformation::elementInfoKeys())
	{
			//Aliased to its own name on purpose: the callers select by
			//column name, so the name an information key carries in
			//element_info is the name it has to carry here.
		body += QStringLiteral("ei.") + key
			+ QStringLiteral(" AS ") + key
			+ QStringLiteral(",");
	}

		//What is left is what no list of information keys can produce: these
		//columns are the join itself, not a property of the element, so they
		//stay written out.
	body += QStringLiteral("d.pos AS diagram_position,"
			       "e.type AS element_type,"
			       "e.sub_type AS element_sub_type,"
			       "di.title AS title,"
			       "di.folio AS folio,"
			       "e.pos AS position"
			       " FROM element_info ei, diagram_info di, element e, diagram d"
			       " WHERE ei.element_uuid = e.uuid"
			       " AND e.diagram_uuid = d.uuid"
			       " AND di.diagram_uuid = d.uuid");

		//The kinds, stated once for both views - see above.
	body += elementTypeClause(publishedElementTypes(),
				  QStringLiteral("e.type"));

	return body;
}

/**
	@brief projectDataBase::createElementNomenclatureView
	The bill of materials view: the shared body, minus the rows the user
	asked to keep out of the parts list, and minus the rows that name
	nothing.

	@par The second clause, and the two opposite mistakes it sits between
	Measured on a project of fourteen folios : a hundred of its three
	hundred and fifty four lines carried nothing but the title and the
	number of the sheet they were drawn on. Seventy one of them came from
	the folio of the terminal strips, and the kinds of drawn thing behind
	them were end caps and rail stops - a drawn marker that says where a
	strip finishes, not a thing anybody buys.

	Neither the kind of element nor the shape of the drawing tells those
	apart from a component, and that was measured before this clause was
	written rather than assumed :

	- an end cap is a Terminal, exactly like the terminal block beside it,
	  so elementTypeClause() cannot separate them and must not be asked to.
	  A terminal block is bought, and a list that lost it would be wrong in
	  the one place a cabinet shop reads first;

	- "no wire ends on it" separates them on that project and destroys
	  another one : in the examples shipped with the program it withholds
	  nine circuit breakers of photovoltaique.qet and ten of
	  tableau_domestique.qet, drawn without conductors and bought all the
	  same.

	What is left is what the row itself says. A row with nothing in any of
	its information columns has no label to find it by, no designation, no
	manufacturer, no reference and no part code : there is nothing on it to
	order and nothing to mark. The rule is stated in
	QETBom::informationPresentCondition(), with the reason
	@c exclude_from_bom is not one of those columns.

	@par The price, measured and not estimated
	It is paid by the project where nobody ever filled a component in.
	perceuse.qet, shipped with the program, draws five hundred and fifty
	two components of which five hundred and forty carry no information at
	all, so its parts list goes from five hundred and fifty two nameless
	lines to twelve named ones. Neither list can be ordered from; this one
	does not pretend. What keeps the loss from being silent is
	namelessComponentCount(), which the two bills of material report beside
	the list they wrote - and typing one character into any field of a
	component brings its line straight back.

	@par Here, and not in the shared body
	element_label_view keeps every drawn thing, and that is the whole
	reason it exists : an item kept out of the purchase list still has to
	be marked on the rail, and a collector reading a filtered view would
	leave holes in the marking that nobody notices until assembly. The body
	says what an element row is; each view says what it drops, and this is
	a drop the bill of materials makes for a reason of its own.
*/
void projectDataBase::createElementNomenclatureView()
{
	const QString create_view = QStringLiteral("CREATE VIEW element_nomenclature_view AS ")
				+ elementViewBody()
				+ QStringLiteral(" AND (ei.exclude_from_bom IS NOT 'true')")
				+ QStringLiteral(" AND ")
				+ QETBom::informationPresentCondition(
					QStringLiteral("ei."),
					QETInformation::elementInfoKeys());

	QSqlQuery query(m_data_base);
	if (!query.exec(create_view)) {
		qDebug() << query.lastError();
	}

	QSqlQuery query_version{m_data_base};
	query_version.exec("select sqlite_version();");
	query_version.next();
	QString version = query_version.value("sqlite_version()").toString();
	query_version.finish();
	
	qInfo() << "SQLite version: " << version;
}

/**
	@brief projectDataBase::createElementLabelView
	The same rows as the nomenclature view, without the bill of materials
	filter.

	The two are not the same question. "Exclude from the bill of materials"
	is a box the user ticks to keep an item out of the purchase list; it does
	not say the item is absent from the cabinet, and an item kept out of the
	purchase list still has to be marked on the rail. A collector reading the
	filtered view would leave holes in the marking that nobody notices until
	assembly, which is the whole reason this second view exists.

	The clause is the only difference, and it is written where it is because
	of that: the body above says what an element row is, and each view says
	what it drops.
*/
void projectDataBase::createElementLabelView()
{
	const QString create_view = QStringLiteral("CREATE VIEW element_label_view AS ")
				+ elementViewBody();

	QSqlQuery query(m_data_base);
	if (!query.exec(create_view)) {
		qDebug() << query.lastError();
	}
}

/**
	@brief projectDataBase::createSummaryView
	One row per sheet: everything the title block of that sheet holds, plus
	the place the sheet occupies in the project.

	The information columns are generated from
	QETInformation::diagramInfoKeys() rather than written out, for the reason
	elementViewBody() records above. That list already creates the columns of
	diagram_info and already drives the insert; a view that repeats it by hand
	is a second list, and a second list has to be remembered.

	It was not. The hand-written form published seven of the nine keys,
	leaving out filename and display_folio, and it had been that way since
	the commit that first wrote the view. That same commit taught the column
	picker to skip those two keys, which is why the gap never showed as an
	error: the picker compensated for it instead of it being closed, and a
	column the data base held could not be asked for from anywhere.

	It is the divergence the element views carried, arriving from the other
	side. There the picker offered a key the query could not answer, and the
	table came back empty on the folio; here the picker offered less than the
	data base held, and nothing came back at all because nothing was asked.
	The first form is silent, the second is invisible, and both were one list
	written twice.

	There is no second list left to forget. What the picker offers is its own
	decision and is stated there, not here: this view answers for every key,
	and which of them are worth showing is a question about the window.
*/
void projectDataBase::createSummaryView()
{
	QString create_view(QStringLiteral("CREATE VIEW project_summary_view AS SELECT "));
	for (const QString &key : QETInformation::diagramInfoKeys())
	{
			//Aliased to its own name on purpose: the callers select by
			//column name, so the name an information key carries in
			//diagram_info is the name it has to carry here.
		create_view += QStringLiteral("di.") + key
			+ QStringLiteral(" AS ") + key
			+ QStringLiteral(",");
	}

		//What no list of information keys can produce: the rank of the sheet
		//is a property of the project and not of the title block, so it
		//stays written out, and so does the join it arrives through.
	create_view += QStringLiteral("d.pos AS pos"
			      " FROM diagram_info di, diagram d"
			      " WHERE di.diagram_uuid = d.uuid");

	QSqlQuery query(m_data_base);
	if (!query.exec(create_view)) {
		qDebug() << query.lastError();
	}
}

void projectDataBase::populateDiagramTable()
{
	QSqlQuery query_(m_data_base);
	query_.exec("DELETE FROM diagram");

	for (auto diagram : m_project->diagrams())
	{
		m_insert_diagram_query.bindValue(":uuid", diagram->uuid().toString());
		m_insert_diagram_query.bindValue(":pos", m_project->folioIndex(diagram)+1);
		if(!m_insert_diagram_query.exec()) {
			qDebug() << "projectDataBase::populateDiagramTable insert error : " << m_insert_diagram_query.lastError();
		}
	}
}

/**
	@brief projectDataBase::populateElementTable
	Populate the element table

	With populatedElementTypes(), which is the same set addElement() takes:
	what the file holds and what the table holds are then the same thing,
	rather than two answers that agree until the project is saved.
*/
void projectDataBase::populateElementTable()
{
	QSqlQuery query_(m_data_base);
	query_.exec("DELETE FROM element");

	for (auto diagram : m_project->diagrams())
	{
		const ElementProvider ep(diagram);
		const auto elmt_vector = ep.find(populatedElementTypes());
			//Insert all values into the database
		for (const auto &elmt : elmt_vector)
		{
			bindElementValues(m_insert_elements_query, elmt, diagram);
			if (!m_insert_elements_query.exec()) {
				qDebug() << "projectDataBase::populateElementTable insert error : " << m_insert_elements_query.lastError();
			}
		}
	}
}

/**
	@brief projectDataBase::populateElementInfoTable
	Populate the element info table

	The same set as the table above, and it has to be the same one: a row in
	element with no row in element_info is a component the views cannot
	reach, because the body of both of them joins the two.
*/
void projectDataBase::populateElementInfoTable()
{
	QSqlQuery query(m_data_base);
	query.exec(QStringLiteral("DELETE FROM element_info"));

	for (const auto &diagram : m_project->diagrams())
	{
		const ElementProvider ep(diagram);
		const auto elmt_vector = ep.find(populatedElementTypes());

			//Insert all values into the database
		for (const auto &elmt : elmt_vector)
		{
			bindElementInfoValues(m_insert_element_info_query, elmt);

			if (!m_insert_element_info_query.exec()) {
				qDebug() << "projectDataBase::populateElementInfoTable insert error : " << m_insert_element_info_query.lastError();
			}
		}
	}
}

void projectDataBase::populateDiagramInfoTable()
{
	QSqlQuery query(m_data_base);
	query.exec("DELETE FROM diagram_info");

	for (auto *diagram : m_project->diagrams())
	{
		bindDiagramInfoValues(m_insert_diagram_info_query, diagram);

		if (!m_insert_diagram_info_query.exec()) {
			qDebug() << "projectDataBase::populateDiagramInfoTable insert error : " << m_insert_diagram_info_query.lastError();
		}
	}
}

/**
	@brief projectDataBase::populateConductorTable
	Populate the terminal and conductor tables. Terminals only matter here
	in the context of a conductor referencing them, so their population is
	folded into this method rather than tracked independently.
*/
void projectDataBase::populateConductorTable()
{
	QSqlQuery query(m_data_base);
	query.exec(QStringLiteral("DELETE FROM conductor"));
	query.exec(QStringLiteral("DELETE FROM terminal"));

	for (auto *diagram : m_project->diagrams())
	{
		const auto conductor_list = diagram->conductors();
		for (auto *conductor : conductor_list)
		{
				//See addConductor(): only a terminal with no parent element is
				//skipped. A missing terminal uuid is handled by stableUuid().
			if (!conductor->terminal1->parentElement()
				|| !conductor->terminal2->parentElement()) {
				continue;
			}

			insertTerminal(conductor->terminal1);
			insertTerminal(conductor->terminal2);

			watchConductor(conductor);
			bindConductorValues(m_insert_conductor_query, conductor, diagram);
			if (!m_insert_conductor_query.exec()) {
				qDebug() << "projectDataBase::populateConductorTable insert error : " << m_insert_conductor_query.lastError();
			}
		}
	}
}

/**
	@brief projectDataBase::insertTerminal
	Insert (or, if already present -- e.g. a junction shared by several
	conductors -- silently keep) @terminal in the terminal table.
	@param terminal
*/
void projectDataBase::insertTerminal(Terminal *terminal)
{
	m_insert_terminal_query.bindValue(":uuid", terminal->stableUuid().toString());
	m_insert_terminal_query.bindValue(":element_uuid", terminal->parentElement()->uuid().toString());
	m_insert_terminal_query.bindValue(":name", terminal->name());
	if (!m_insert_terminal_query.exec()) {
		qDebug() << "projectDataBase::insertTerminal insert error : " << m_insert_terminal_query.lastError();
	}
}

void projectDataBase::prepareQuery()
{
		//INSERT DIAGRAM
	m_insert_diagram_query = QSqlQuery(m_data_base);
	m_insert_diagram_query.prepare("INSERT INTO diagram (uuid, pos) VALUES (:uuid, :pos)");

		//REMOVE DIAGRAM
	m_remove_diagram_query = QSqlQuery(m_data_base);
	m_remove_diagram_query.prepare("DELETE FROM diagram WHERE uuid=:uuid");

		//INSERT DIAGRAM INFO
	m_insert_diagram_info_query = QSqlQuery(m_data_base);
	QStringList bind_diag_info_values;
	for (auto key : QETInformation::diagramInfoKeys()) {
		bind_diag_info_values << key.prepend(":");
	}
	QString insert_diag_info("INSERT INTO diagram_info (diagram_uuid, " +
				   QETInformation::diagramInfoKeys().join(", ") +
				   ") VALUES (:uuid, " +
				   bind_diag_info_values.join(", ") +
				   ")");
	m_insert_diagram_info_query.prepare(insert_diag_info);

		//UPDATE DIAGRAM INFO
	QString update_diagram_str("UPDATE diagram_info SET ");
	for (auto str : QETInformation::diagramInfoKeys()) {
		update_diagram_str.append(str % " = :" % str % ", ");
	}
	update_diagram_str.remove(update_diagram_str.length()-2, 2); //Remove the last ", "
	update_diagram_str.append(" WHERE diagram_uuid = :uuid");
	m_update_diagram_info_query = QSqlQuery(m_data_base);
	m_update_diagram_info_query.prepare(update_diagram_str);

		//UPDATE DIAGRAM ORDER
	m_diagram_order_changed = QSqlQuery(m_data_base);
	m_diagram_order_changed.prepare("UPDATE diagram SET pos = :pos WHERE uuid = :uuid");
	m_diagram_info_order_changed = QSqlQuery(m_data_base);
	m_diagram_info_order_changed.prepare("UPDATE diagram_info SET folio = :folio WHERE diagram_uuid = :uuid");

		//INSERT ELEMENT
	QString insert_element_query("INSERT INTO element (uuid, diagram_uuid, pos, type, sub_type) VALUES (:uuid, :diagram_uuid, :pos, :type, :sub_type)");
	m_insert_elements_query = QSqlQuery(m_data_base);
	m_insert_elements_query.prepare(insert_element_query);


		//INSERT ELEMENT INFO
	QStringList bind_values;
	for (auto key : QETInformation::elementInfoKeys()) {
		bind_values << key.prepend(":");
	}
	QString insert_element_info("INSERT INTO element_info (element_uuid," +
				   QETInformation::elementInfoKeys().join(", ") +
				   ") VALUES (:uuid," +
				   bind_values.join(", ") +
				   ")");
	m_insert_element_info_query = QSqlQuery(m_data_base);
	m_insert_element_info_query.prepare(insert_element_info);

		//REMOVE ELEMENT
	QString remove_element("DELETE FROM element WHERE uuid=:uuid");
	m_remove_element_query = QSqlQuery(m_data_base);
	m_remove_element_query.prepare(remove_element);

		//UPDATE ELEMENT INFO
	QString update_str("UPDATE element_info SET ");
	for (auto string : QETInformation::elementInfoKeys()) {
		update_str.append(string % " = :" % string % ", ");
	}
	update_str.remove(update_str.length()-2, 2); //Remove the last ", "
	update_str.append(" WHERE element_uuid = :uuid");
	m_update_element_query = QSqlQuery(m_data_base);
	m_update_element_query.prepare(update_str);

		//INSERT TERMINAL
	m_insert_terminal_query = QSqlQuery(m_data_base);
	m_insert_terminal_query.prepare("INSERT OR IGNORE INTO terminal (uuid, element_uuid, name) VALUES (:uuid, :element_uuid, :name)");

		//INSERT CONDUCTOR
	m_insert_conductor_query = QSqlQuery(m_data_base);
	m_insert_conductor_query.prepare("INSERT INTO conductor (uuid, diagram_uuid, terminal1_uuid, terminal1_element_uuid, terminal2_uuid, terminal2_element_uuid, "
					  "text, conductor_section, conductor_color, function, tension_protocol, conductor_type) "
					  "VALUES (:uuid, :diagram_uuid, :terminal1_uuid, :terminal1_element_uuid, :terminal2_uuid, :terminal2_element_uuid, "
					  ":text, :conductor_section, :conductor_color, :function, :tension_protocol, :conductor_type)");

		//UPDATE CONDUCTOR
		//
		//The same five columns as the insert, and they have to be the same
		//five: the endpoints of a conductor are fixed for its lifetime, so
		//everything else about it can only reach the table through here.
		//A column present in the insert and missing from this statement is
		//the worst shape of the bug -- the row is right when the project is
		//opened and goes stale the first time somebody edits the wire, with
		//nothing to say so.
	m_update_conductor_query = QSqlQuery(m_data_base);
	m_update_conductor_query.prepare(QStringLiteral(
					  "UPDATE conductor SET text = :text, conductor_section = :conductor_section, "
					  "conductor_color = :conductor_color, function = :function, "
					  "tension_protocol = :tension_protocol, conductor_type = :conductor_type "
					  "WHERE uuid = :uuid"));

		//REMOVE CONDUCTOR
	m_remove_conductor_query = QSqlQuery(m_data_base);
	m_remove_conductor_query.prepare("DELETE FROM conductor WHERE uuid=:uuid");
}

/**
	@brief projectDataBase::elementInfoToString
	@param elmt
	@return the element information in hash as key for the info name and value as the information value.
*/
QHash<QString, QString> projectDataBase::elementInfoToString(Element *elmt)
{
	QHash<QString, QString> hash; //Store the value for each columns
	for (auto key : QETInformation::elementInfoKeys())
	{
		if (key == "label") {
			hash.insert(key, elmt->actualLabel());
		}
		else {
			hash.insert(key, elmt->elementInformations()[key].toString());
		}
	}

	return hash;
}

void projectDataBase::bindDiagramInfoValues(QSqlQuery &query, Diagram *diagram)
{
	query.bindValue(":uuid", diagram->uuid());

	auto infos = diagram->border_and_titleblock.titleblockInformation();
	for (auto key : QETInformation::diagramInfoKeys())
	{
		if (key == "date") {
			query.bindValue( ":date",
							 QLocale::system().toDate(infos.value("date").toString(),
													  QLocale::ShortFormat));
		} else {
			auto value = infos.value(key);
			auto bind = key.prepend(":");
			query.bindValue(bind, value);
		}
	}
}

#ifdef QET_EXPORT_PROJECT_DB
/**
	@brief projectDataBase::sqliteHandle
	@param db
	@return the sqlite3 handler class used internally by db
*/
sqlite3 *projectDataBase::sqliteHandle(QSqlDatabase *db)
{
	sqlite3 *handle = nullptr;

	QVariant v = db->driver()->handle();
	if (v.isValid() && qstrcmp(v.typeName(), "sqlite3*") == 0) {
		handle = *static_cast<sqlite3 **>(v.data());
	}

	return handle;
}


/**
 * @brief projectDataBase::exportDb
 * Export the db, to a file.
 * @param db : database to export
 * @param parent : parent widget of a QDialog used in this function
 * @param caption : Title of the QDialog used in this function
 * @param dir : Default directory where the database must be saved.
 */
void projectDataBase::exportDb(projectDataBase *db,
			       QWidget *parent,
			       const QString &caption,
			       const QString &dir)
{
	auto caption_ = caption;
	if (caption_.isEmpty()) {
		caption_ = tr("Exporter la base de données interne du projet");
	}

	auto dir_ = dir;
	if(dir_.isEmpty()) {
		dir_ = db->project()->filePath();
		if (dir_.isEmpty()) {
			dir_ = QETApp::documentDir() % "/" % tr("sans_nom") % ".sqlite";
		} else {
			dir_.remove(".qet");
			dir_.append(".sqlite");
		}
	}

	auto path_ = QFileDialog::getSaveFileName(parent, caption_, dir_, "*.sqlite");
	if (path_.isNull()) {
		return;
	}

	QString connection_name("export_project_db_" % db->project()->uuid().toString());

	if (true) //Enter in a scope only to nicely use QSqlDatabase::removeDatabase just after the end of the scope
	{
		auto file_db = QSqlDatabase::addDatabase("QSQLITE", connection_name);
		file_db.setDatabaseName(path_);
		if (!file_db.open()) {
			return;
		}

		auto memory_db_handle = sqliteHandle(&db->m_data_base);
		auto file_db_handle = sqliteHandle(&file_db);

		auto sqlite_backup = sqlite3_backup_init(file_db_handle, "main", memory_db_handle, "main");
		if (sqlite_backup)
		{
			sqlite3_backup_step(sqlite_backup, -1);
			sqlite3_backup_finish(sqlite_backup);
		}
		file_db.close();
	}
	QSqlDatabase::removeDatabase(connection_name);
}
#endif
