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
#include "conductornumexport.h"
#include "qetproject.h"
#include "qetapp.h"
#include "diagram.h"
#include "diagramcontent.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"

#include <QFileDialog>

/**
	@brief ConductorNumExport::ConductorNumExport
	@param project : the project to export the conductors num
	@param parent : parent widget
*/
ConductorNumExport::ConductorNumExport(QETProject *project, QWidget *parent) :
	m_project(project),
	m_parent_widget(parent)
{
	fillHash();
}

/**
	@brief ConductorNumExport::toCsv
	Export the num of conductors into a csv file.
	@return true if suceesfully exported.

	@par End of the file : nothing after the last row
	wiresNum() terminates every row with a line break, so the stream writes
	the text and stops. What stood here was
	@c {stream << wiresNum() << &Qt::endl(stream)}, which the precedence of
	the call over the unary @c & turns into @c {&(Qt::endl(stream))} : the
	manipulator runs and its @c QTextStream* return is printed by
	@c {operator<<(const void *)} as a hexadecimal address, measured as an
	empty row plus @c 0x9a033ff0e0 at the end of every exported file. The
	price of writing nothing is a file with no final newline should the
	payload ever stop terminating its rows.

	@par Replacing a file : truncate, do not delete first
	The existing file is no longer deleted before the new one is opened,
	because @c QIODevice::WriteOnly truncates on open - measured, forty
	bytes down to two with no @c QFile::remove - and the save dialog has
	already asked the user to confirm the overwrite. The delete only opened
	a window in which the old file was gone and the new one had not been
	proved writable. The price is that the separate "cannot replace" report
	disappears and its @c tr() string, translated in twenty three
	catalogues, is replaced by a new one that falls back to French until
	lupdate runs again.

	@par A failed open : say it, do not only return false
	The error branch already returned false, but said nothing, and the sole
	caller - QETDiagramEditor, where the action is wired - discards the
	return value. So the user picked a file name and got no file and no
	message. The branch now reports before returning. The price is a dialog
	that mixes the French sentence with the system reason in the language of
	the operating system, which is what distinguishes a denied folder from a
	full disk.
*/
bool ConductorNumExport::toCsv()
{
		//save in csv file in same directory as project by default
	QString dir = m_project->currentDir();
	if (dir.isEmpty()) dir = QETApp::documentDir();
	QString name = dir % "/" % QObject::tr("numero_de_fileries_") % m_project->title() % ".csv";
	//    if(!name.endsWith(".csv")) {
	//        name += ".csv";
	//    }

	QString filename = QFileDialog::getSaveFileName(m_parent_widget, QObject::tr("Enregister sous... "), name, QObject::tr("Fichiers csv (*.csv)"));
	QFile file(filename);
	if(!filename.isEmpty())
	{
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			QMessageBox::critical(m_parent_widget, QObject::tr("Erreur"),
								  QObject::tr("Le fichier « %1 » n'a pas pu être écrit.").arg(filename) %
								  "\n\n" % file.errorString());
			return false;
		}

			//wiresNum() ends its last row with a line break already, so
			//nothing is appended after it.
		QTextStream stream(&file);
		stream << wiresNum();
	}
	else {
		return false;
	}

	return true;
}

/**
	@brief ConductorNumExport::wiresNum
	@return the wire num formatted in csv
*/
QString ConductorNumExport::wiresNum() const
{
	QString csv;

	QStringList list = m_hash.keys();
	list.sort();
	for (QString key : list)
	{
		for (int i=0; i<m_hash.value(key) ; ++i) {
			csv.append(key % "\n");
		}
	}

	return csv;
}

/**
	@brief ConductorNumExport::fillHash
	make/fill of m_hash
*/
void ConductorNumExport::fillHash()
{
	//We used this rx to avoid insert num composed only withe white space.
	QRegularExpression rx("^ *$");
	for (Diagram *d : m_project->diagrams())
	{
		DiagramContent dc(d, false);
		for (Conductor *c : dc.conductors())
		{
			QString num = c->textItem()->toPlainText();
			if (num.isEmpty() || num.contains(rx)) {
				continue;
			}

			//We must define if the connected terminal is a folio report, if it is the case
			//we don't add the num to the hash because the terminal doesn't represent a real terminal.
			if(!(c->terminal1->parentElement()->linkType() & Element::AllReport))
			{
				int value = m_hash.value(num, 0);
				++value;
				m_hash.insert(num, value);
			}
			if(!(c->terminal2->parentElement()->linkType() & Element::AllReport))
			{
				int value = m_hash.value(num, 0);
				++value;
				m_hash.insert(num, value);
			}
		}
	}
}
