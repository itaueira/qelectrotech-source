/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PROJECTDBMODEL_H
#define PROJECTDBMODEL_H

#include <QAbstractTableModel>
#include <QPointer>
#include <QDomElement>

class Element;
class QETProject;

/**
	@brief The ProjectDBModel class
	This model is intended to be use with the class projectDataBase
	and is designed to be displayed by the class QetGraphicsTableItem
	(but can be use by other view class since it inherit from QAbstractTableModel).
	This class should be sufficient to display the content of
	the project data base from a query set by the method
	void ProjectDBModel::setQuery(const QString &query).
	The indentifier method is used by widget editor to retrieve
	the good widget for edit the query.
	By default identifer returns the string 'unknow'.
	You should use setIdentfier method to set your custom identifier.
	At the time this sentence is written, there is two identifier :
	nomenclature
	summary
*/
class ProjectDBModel : public QAbstractTableModel
{
	Q_OBJECT

	public:
		explicit ProjectDBModel(QETProject *project, QObject *parent = nullptr);
		explicit ProjectDBModel (const ProjectDBModel &other_model);

		int rowCount(const QModelIndex &parent = QModelIndex()) const override;
		int columnCount(const QModelIndex &parent = QModelIndex()) const override;
		bool setHeaderData(int section,
				   Qt::Orientation orientation,
				   const QVariant &value,
				   int role = Qt::EditRole) override;
		QVariant headerData(int section,
				    Qt::Orientation orientation,
				    int role = Qt::DisplayRole) const override;
		bool setData(const QModelIndex &index,
			     const QVariant &value,
			     int role = Qt::EditRole) override;
		QVariant data(const QModelIndex &index,
			      int role = Qt::DisplayRole) const override;
		Qt::ItemFlags flags(const QModelIndex &index) const override;
		void setQuery(const QString &setQuery);
		QString queryString() const;
		QETProject *project() const;

		/**
			The name of each column the current query returns, in the
			order the query returns them. Empty when the query could
			not be run at all.
		*/
		QStringList columnNames() const {return m_column_names;}
		/**
			Empty while the current query runs. Otherwise the sentence
			to show to the reader, error of the data base included.

			This is the state that tells apart the two things that used
			to be drawn the same way : a list that ran and found
			nothing, and a query that never ran.
		*/
		QString lastError() const {return m_last_error;}

		/**
			The one component the row @a row stands for, or nullptr
			when the row stands for no component or for more than
			one.

			The row is traced back by value and not by key, because
			element_nomenclature_view publishes no key : it carries
			QETInformation::elementInfoKeys() and six columns of the
			join, and the uuid of the component is in none of them.
			So the information columns the query happens to select
			are read back out of element_info, and a row is
			identified when exactly one component answers to all of
			them at once. Two components that are written the same
			way in every column the list shows are two components
			the reader cannot tell apart either, and this answers
			nullptr for both rather than guessing one.
		*/
		Element *elementForRow(int row) const;

		QDomElement toXml(QDomDocument &document) const;
		void fromXml(const QDomElement &element);
		void setIdentifier(const QString &identifier);
		QString identifier() const {return m_identifier;}
		static QString xmlTagName() {return QString("project_data_base_model");}

	signals:
		/**
			Emitted when lastError() changes, in both directions : the
			empty string says the query runs again.
		*/
		void queryErrorChanged(const QString &error);

	private:
		void dataBaseUpdated();
		void setHeaderString();
		void fillValue();
		void setLastError(const QString &error);
		bool isEditableColumn(int column) const;
		bool writeInformation(const QModelIndex &index, const QString &value);
		void resolveRowElements() const;
		static QStringList readOnlyInfoKeys();

	private:
		QPointer<QETProject> m_project;
		QString m_query;
		QVector<QStringList> m_record;
		QStringList m_column_names;
			//The columns of the current query that are element
			//information keys, in the order the query returns them,
			//and their stored - not drawn - value on each row. This
			//is what traces a row back to the component it stands
			//for, and what an editor is opened with : the drawn form
			//and the stored form are not always the same string.
		QStringList m_identity_columns;
			//Per column of the query : where its value sits in the
			//tuples of m_row_identity, or -1 when the column is not
			//an information key.
		QVector<int> m_identity_of_column;
		QVector<QStringList> m_row_identity;
			//Worked out on demand and kept until the next fill : the
			//table drawn on a folio never asks for it, and only a
			//view that lets the reader type does.
		mutable QVector<QPointer<Element>> m_row_element;
		mutable bool m_rows_resolved = false;
		QString m_last_error;
		//First int = section, second int = Qt::role, QVariant = value
		QHash<int, QHash<int, QVariant>> m_header_data;
		QHash<int, QVariant> m_index_0_0_data;
		QString m_identifier = "unknow";
};

#endif // PROJECTDBMODEL_H
