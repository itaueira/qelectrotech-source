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
#ifndef LOCATIONBOMDIALOG_H
#define LOCATIONBOMDIALOG_H

#include <QDialog>
#include <QPointer>
#include <QString>

class QComboBox;
class QETProject;
class QLabel;
class QPushButton;
class QTreeWidget;

/**
	@brief The LocationBomDialog class
	What has to be picked in the storeroom for one enclosure, said in one
	window: the enclosure itself and everything that goes inside it.

	The list has two halves because the project answers them in two
	different places, and pretending otherwise would make one of them
	wrong. The components come from element_nomenclature_view, which is the
	same view the printed nomenclature reads - so the two cannot disagree,
	and in particular the exclude_from_bom flag is honoured here without
	this window knowing it exists, because the view has already applied it.
	The enclosures come from the location tree, because an enclosure is not
	an element: nobody drew it on a folio, so no walk over the folios could
	ever find it. LocationTree::bomLines is what answers that half, and it
	is where CU-32.4 lives - a location whose part is marked virtual is a
	door or a mounting plate that came with the cabinet, and it is not
	bought twice.

	The scope box is the whole interface. It offers every location of the
	project, and it offers "hors localisation" as a scope of its own: an
	empty location_path is an answer, not a missing value (decision F), and
	a pushbutton on the machine frame is bought exactly like everything
	else. Leaving it out of the list would be leaving material out of the
	list.

	The window can also put what it shows onto a folio, which is CU-32.5.
	It does not draw the table itself - it hands over a query and lets the
	table factory that already exists build it, so the block that lands on
	the cover folio is an ordinary nomenclature table: it refreshes with
	the project, it prints, and the standard dialog can still edit it.
*/
class LocationBomDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit LocationBomDialog(QETProject *project,
					   QWidget *parent = nullptr);

		/// @return the project this list is drawn from
		QETProject *project() const;

	signals:
		/**
			@brief insertTable
			@param query the nomenclature query to put on a folio
			Emitted by "Insérer dans le folio…". Whoever owns the
			project view knows which folio is in front of the person;
			this window only knows what to write in it.
		*/
		void insertTable(const QString &query);

	private slots:
		void reload();
		void scopeChanged();
		void copyToClipboard();
		void exportCsv();
		void insertOnFolio();

	private:
		void buildWidgets();
		void fillScopes();
		/// @return the location_path the scope box is on, empty for the
		/// field, null for the whole project
		QString scopePath() const;
		/// @return true when the scope box is on "hors localisation"
		bool scopeIsField() const;
		int fillLocations();
		int fillComponents();
		QString asText(const QString &separator) const;
		void say(const QString &message, bool problem = false);

	private:
		QPointer<QETProject> m_project;

		QComboBox *m_scope = nullptr;
		QTreeWidget *m_tree = nullptr;
		QLabel *m_summary = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_copy = nullptr;
		QPushButton *m_export = nullptr;
		QPushButton *m_insert = nullptr;
};

#endif // LOCATIONBOMDIALOG_H
