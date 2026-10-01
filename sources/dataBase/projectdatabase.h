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
#include <QHash>
#include <QSet>
#include <QUuid>

class Element;
class QETProject;
class Diagram;
class Conductor;
class ConductorProperties;
class Terminal;
class QGraphicsItem;
class QDomDocument;
class BorderTitleBlock;
class DiagramContext;
class QDate;

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
			/**
				updateDB() for a project just read from @p document: the
				diagram, element, terminal and conductor tables are filled
				from the document when it carries everything they need (see
				populateFromDocument()), from the built folios otherwise.
			*/
		void updateDB(const QDomDocument &document);
			/**
				Suppress the full rebuild performed by updateDB().

				While blocked, updateDB() returns immediately instead of
				repopulating every table. Meant for bulk operations -- notably
				loading a project, where each table model re-queries the
				database as it is built and would otherwise trigger one
				complete rebuild of it. The caller unblocks and calls
				updateDB() once when done; @see QETProject::readProjectXml().
			*/
		void setUpdateBlocked(bool blocked);
		QETProject *project() const;
		QSqlQuery newQuery(const QString &query = QString(), QString *error = nullptr);
		static bool isReadOnlySelect(const QString &query, QString *error = nullptr);

			/**
				The most rows any caller reads out of one query result.

				A SELECT is not bounded by how much data the project holds:
				SQLite produces rows lazily, so a query that never stops
				producing them makes the loop that reads them never stop
				either. A recursive CTE does exactly that in one line, and
				a <graphics_table>'s <query> is stored in the .qet and run
				on load -- so the text can arrive from a file rather than
				from the person at the keyboard, and opening that file is
				the whole attack.

				100000 is far above any real result: the largest table in
				the shipped examples is 396 rows. It is a backstop, not a
				page size -- a caller that hits it has almost certainly
				been handed something it should not run to completion, and
				says so rather than truncating quietly.
			*/
		static constexpr int MaxResultRows = 100000;
		QSqlDatabase database() const {return m_data_base;}

			/// One element of the bill of materials, as a mounting
			/// layout needs it
		struct BomElement
		{
			QString uuid;      ///< as the element table holds it
			QString label;
			QString part_code;
		};
		/**
			@return every element that enters the bill of materials, by
			the rule of element_nomenclature_view - which publishes no
			uuid, and so cannot answer which of them is mounted on a
			plate. In order of label.
		*/
		QList<BomElement> bomElements();
		int excludedConductorCount() const;

		/**
			@brief How many components the parts list withholds because
			they name nothing.
			@return the count, and 0 when the question cannot be asked

			The other half of the clause createElementNomenclatureView()
			documents. That clause drops the rows carrying nothing in any
			of their information columns, and a drop nobody can see is
			the failure this project spends most of its comments on : the
			list would simply come back shorter, and the reader would have
			no way of telling a project with fewer parts from a project
			whose parts were never filled in.

			So the number is available, and the two bills of material
			report it beside the list they wrote. It is a count and not a
			list on purpose - naming the withheld rows would mean naming
			rows that have no name, which is the sentence the count says
			in one number.

			Counted over element_label_view, which is the same body
			without the parts list filters, so the flag is applied here
			and the condition is inverted here : asking the nomenclature
			view would be asking the rows that are gone.
		*/
		int namelessComponentCount();

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
			//Shapes, independent texts and images: the folio's drawing
			//furniture. Anything else passed here is ignored.
		void addDrawingItem     (QGraphicsItem *item);
		void removeDrawingItem  (QGraphicsItem *item);
		void itemGroupChanged   (QGraphicsItem *item);

	private slots:
			//Refresh the sender()'s row after Conductor::setProperties().
		void conductorPropertiesChanged();
			//Queue the sender()'s drawing-item row for rewriting.
		void drawingItemChanged();
		void drawingItemDestroyed(QObject *object);

	public:

	signals:
		void dataBaseUpdated();

	private:
		bool createDataBase();
		/// @return SQL restricting @a column of the element table to @a types
		static QString elementTypeClause(ElementData::Types types,
						 const QString &column);
		static QString elementViewBody();
		static QString elementViewSource();
		static QString nomenclatureCondition();
		void createElementNomenclatureView();
		void createElementLabelView();
		void createSummaryView();
		void createWiringListView();
		void createDrawingItemView();
		void populateDiagramTable();
		void populateElementTable();
		void populateElementInfoTable();
		void populateDiagramInfoTable();
		void populateConductorTable();
		static void bindElementValues(QSqlQuery &query, Element *element, Diagram *diagram);
		static void bindElementInfoValues(QSqlQuery &query, Element *element);
		void populateDrawingItemTables();
		bool populateFromDocument(const QDomDocument &document, QString *why = nullptr);
		bool writeDrawingItem(QObject *object);
		void flushDrawingItems();
		void forgetDrawingItem(QObject *object);
		void bindConductorValues(QSqlQuery &query, Conductor *conductor, Diagram *diagram);
		/**
			Bind the columns of the conductor table that can change while
			the conductor stays the same one.

			Shared by the insert and by the update so that the two cannot
			describe a wire differently -- the same reason
			bindConductorValues() exists for both insert paths. Whoever
			adds a column here adds it to both prepared statements in
			prepareQuery(), or the bind is silently dropped.
		*/
		static void bindConductorProperties(QSqlQuery &query, const ConductorProperties &properties);
		void watchConductor(Conductor *conductor);
		void insertTerminal(Terminal *terminal);
		void insertTerminal(const QString &uuid, const QString &element_uuid,
							const QString &name);
		void prepareQuery();
		static QHash<QString, QString> elementInfoToString(
				Element *elmt);
		void bindDiagramInfoValues(QSqlQuery &query, Diagram *diagram);
		static void bindDiagramInfoValues(QSqlQuery &query, const QUuid &diagram_uuid,
										  const BorderTitleBlock &border);
		static void bindDiagramInfoValues(QSqlQuery &query, const QUuid &diagram_uuid,
										  const DiagramContext &infos, const QDate &date);
		static void bindElementInfoValues(QSqlQuery &query, const QString &element_uuid,
										  const DiagramContext &informations,
										  const QString &label);
		/// Announce a change, now or when the open gesture ends.
		void notifyUpdated();

	private:
		QPointer<QETProject> m_project;
		bool m_update_blocked = false;
			//Starts true : the database is empty until the first rebuild.
			//Set by every method of this class that writes rows, cleared by
			//updateDB(). Callers reach the database from outside only through
			//newQuery(), and every such call site reads.
		bool m_content_changed = true;
		QSqlDatabase m_data_base;
		QSqlQuery m_insert_elements_query,
				  m_insert_element_info_query,
				  m_remove_element_query,
				  m_remove_element_info_query,
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
				  m_remove_conductor_query,
				  m_cascade_remove_element_info_query,
				  m_cascade_remove_terminal_query,
				  m_cascade_remove_conductor_query,
				  m_cascade_remove_element_query,
				  m_insert_shape_query,
				  m_insert_independent_text_query,
				  m_insert_image_query;

			//Which uuid's row each drawing item last wrote, and which item
			//wrote each row. A pasted copy is added to the folio still
			//carrying its source's uuid and renewed only afterwards, so two
			//live items can briefly share one: the row belongs to whichever
			//wrote it, and the other waits in m_dirty_drawing_items until its
			//uuid is its own. @see writeDrawingItem().
		QHash<QObject *, QUuid> m_drawing_item_row;
		QHash<QUuid, QObject *> m_drawing_row_owner;
		QSet<QObject *> m_dirty_drawing_items;

		/// Whether a change is announced now or held until the gesture ends.
		UpdateCoalescer m_coalescer;

#ifdef QET_EXPORT_PROJECT_DB
	public:
		static void exportDb(projectDataBase *db,
				     QWidget *parent = nullptr,
				     const QString &caption = QString(),
				     const QString &dir = QString());
#endif
};

#endif // PROJECTDATABASE_H
