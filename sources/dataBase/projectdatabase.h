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
#ifndef PROJECTDATABASE_H
#define PROJECTDATABASE_H

#include "updatecoalescer.h"

//For ElementData::Types, which is what the two type sets below are expressed
//in: they are the same flags the element provider is asked with, so the set
//that fills a table and the set the provider searches cannot be written in
//two different vocabularies.
#include "../properties/elementdata.h"

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QPointer>
#include <QFileDialog>

class Element;
class QETProject;
class Diagram;
class Conductor;
class Terminal;
class sqlite3;

/**
	@brief The projectDataBase class
	This class wraps a sqlite data base where you can find several things
	about the content of a project.
 *
	@note this class is still in development.
*/
class projectDataBase : public QObject
{
	Q_OBJECT

	public:
		projectDataBase(QETProject *project, QObject *parent = nullptr);
		virtual ~projectDataBase() override;

		void updateDB();
		QETProject *project() const;
		QSqlQuery newQuery(const QString &query = QString());

		/**
			Which drawn elements reach the element and element_info tables.

			Everything a sheet draws that stands for a piece of the circuit,
			which is wider than what any list shows on purpose: the tables
			are the record of the project, and a wire that ends on a relay
			contact has to find a row for that contact or the wire itself
			drops out of a join that nobody looks at twice.

			It is one set and not two because addElement() and the
			repopulation both ask it. They used to disagree - the live insert
			took every element and the repopulation took four types - so a
			contact drawn today was in the table until the project was saved
			and opened again, and then was not. Nothing said so; the list
			simply came back one line shorter.

			ConductorDefinition stays out, and that is a decision and not an
			omission: it is not a piece of the circuit but the element that
			carries the default look of the wires, and it is drawn on no
			sheet of any example shipped here.
		*/
		static ElementData::Types populatedElementTypes();

		/**
			Which of those the two element views publish.

			The four kinds a parts list has always shown. It is narrower than
			populatedElementTypes() so that widening the tables changes
			nothing a person reads: the bill of materials, the nomenclature,
			the label roll and the tables drawn on a folio all read a view,
			and they answer today exactly what they answered before the
			tables were widened.

			Whether a relay contact deserves a line of its own in a parts
			list is a question about the factory and not about the data base.
			The day it is answered, it is answered here, in one line.
		*/
		static ElementData::Types publishedElementTypes();

		void addElement         (Element *element);
		void removeElement      (Element *element);
		void elementInfoChanged (Element *element);
		void elementInfoChanged (QList<Element *> elements);

		void addDiagram         (Diagram *diagram);
		void removeDiagram      (Diagram *diagram);
		void diagramInfoChanged (Diagram *diagram);
		void diagramOrderChanged();

		void addConductor       (Conductor *conductor);
		void removeConductor    (Conductor *conductor);
		void updateConductor    (Conductor *conductor);

		/**
			Open a gesture. Every change made until the matching
			endOperation() is announced once, when it closes, instead of one
			announcement per row -- see UpdateCoalescer for the rule, and for
			what a nested or an unbalanced call does.

			Prefer the Operation guard below: an operation left open by an
			early return stops the folio from ever following the project
			again, and that failure is silent.
		*/
		void beginOperation();
		void endOperation();

		/**
			@brief An operation open for as long as this object lives.

			A null project -- or a null data base -- is accepted and means no
			grouping, so that a caller writes the guard without checking
			first.
		*/
		class Operation
		{
			public:
				explicit Operation(projectDataBase *data_base);
				explicit Operation(QETProject *project);
				~Operation();

				Operation(const Operation &) = delete;
				Operation &operator=(const Operation &) = delete;

			private:
				QPointer<projectDataBase> m_data_base;
		};

	private slots:
			//Refresh the sender()'s row after Conductor::setProperties().
		void conductorPropertiesChanged();

	public:

	signals:
		void dataBaseUpdated();

	private:
		bool createDataBase();
		/// @return SQL restricting @a column of the element table to @a types
		static QString elementTypeClause(ElementData::Types types,
						 const QString &column);
		static QString elementViewBody();
		void createElementNomenclatureView();
		void createElementLabelView();
		void createSummaryView();
		void populateDiagramTable();
		void populateElementTable();
		void populateElementInfoTable();
		void populateDiagramInfoTable();
		void populateConductorTable();
		static void bindElementValues(QSqlQuery &query, Element *element, Diagram *diagram);
		static void bindElementInfoValues(QSqlQuery &query, Element *element);
		void bindConductorValues(QSqlQuery &query, Conductor *conductor, Diagram *diagram);
		void watchConductor(Conductor *conductor);
		void insertTerminal(Terminal *terminal);
		void prepareQuery();
		static QHash<QString, QString> elementInfoToString(
				Element *elmt);
		void bindDiagramInfoValues(QSqlQuery &query, Diagram *diagram);
		/// Announce a change, now or when the open gesture ends.
		void notifyUpdated();

	private:
		QPointer<QETProject> m_project;
		QSqlDatabase m_data_base;
		QSqlQuery m_insert_elements_query,
				  m_insert_element_info_query,
				  m_remove_element_query,
				  m_update_element_query,
				  m_insert_diagram_query,
				  m_remove_diagram_query,
				  m_insert_diagram_info_query,
				  m_update_diagram_info_query,
				  m_diagram_order_changed,
				  m_diagram_info_order_changed,
				  m_insert_terminal_query,
				  m_insert_conductor_query,
				  m_update_conductor_query,
				  m_remove_conductor_query;

		/// Whether a change is announced now or held until the gesture ends.
		UpdateCoalescer m_coalescer;

#ifdef QET_EXPORT_PROJECT_DB
	public:
		static sqlite3 *sqliteHandle(QSqlDatabase *db);
		static void exportDb(projectDataBase *db,
				     QWidget *parent = nullptr,
				     const QString &caption = QString(),
				     const QString &dir = QString());
#endif
};

#endif // PROJECTDATABASE_H
