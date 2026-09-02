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
#include "bomexportdialog.h"

#include "../dataBase/ui/elementquerywidget.h"
#include "../qetapp.h"
#include "../qetinformation.h"
#include "../qetproject.h"
#include "ui_bomexportdialog.h"

#include <QMessageBox>
#include <QSqlError>
#include <QSqlRecord>

/**
	@brief BOMExportDialog::BOMExportDialog
	@param project
	@param parent
*/
BOMExportDialog::BOMExportDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::BOMExportDialog),
	m_project(project)
{
	ui->setupUi(this);

	m_query_widget = new ElementQueryWidget(this);
	ui->m_main_layout->insertWidget(0, m_query_widget);
		//By default format as bom is clicked
	on_m_format_as_bom_clicked(true);
}

/**
	@brief BOMExportDialog::~BOMExportDialog
*/
BOMExportDialog::~BOMExportDialog()
{
	delete ui;
}

/**
	@brief BOMExportDialog::exec
	@return

	@par End of the file : nothing after the last row
	getBom() terminates every row with a line break, so the stream writes
	the text and stops there. What stood here was
	@c {stream << getBom() << &Qt::endl(stream)}, and the call binds tighter
	than the unary @c & : that is @c {&(Qt::endl(stream))}, so the
	manipulator runs and the @c QTextStream* it returns is then printed by
	@c {operator<<(const void *)} as a hexadecimal address. Measured on a
	two row export, the file ended in an empty row followed by
	@c 0x9a033ff0e0 - every csv this dialog ever wrote carried that. Writing
	@c Qt::endl instead would drop the address but keep the empty row,
	because the content already ends in a break. The price of writing
	nothing is that a payload which did not end in a line break would leave
	the file without a final newline; getBom() is the only payload here and
	it always terminates its rows.

	@par Replacing a file : truncate, do not delete first
	The existing file is no longer deleted before the new one is opened.
	@c QIODevice::WriteOnly truncates on open - measured, forty bytes down
	to two with no @c QFile::remove anywhere - so the delete bought no
	behaviour, and the save dialog has already asked the user to confirm the
	overwrite. What it did buy was a window with no file at all: the delete
	succeeded, the open then failed, and the user was left without the new
	list and without the old one. The price is that the separate "cannot
	replace" report disappears, so a file the user holds open in a
	spreadsheet is now refused by open() and reported as a file that could
	not be written; and the wording is a new @c tr() string, so the twenty
	three catalogues that translate the old one fall back to the French
	source until lupdate runs again.

	@par A failed open : say it and stop
	The open carries an error branch that reports and returns. It had none,
	so a folder without write permission, a full disk or a network drive
	that dropped produced no file and no message - and, with the delete
	above, no old file either. The system reason is appended outside
	@c tr(), which mixes the French sentence with the language of the
	operating system: the price of telling the user that access was denied
	instead of only that something went wrong.
*/
int BOMExportDialog::exec()
{
	auto r = QDialog::exec();
	if (r == QDialog::Accepted)
	{
			//save in csv file in same directory as project by default
		QString dir = m_project->currentDir();
		if (dir.isEmpty()) dir = QETApp::documentDir();
		QString file_name = dir % "/" % tr("nomenclature_") % QString(m_project ->title() % ".csv");
		QString file_path = QFileDialog::getSaveFileName(this, tr("Enregister sous... "), file_name, tr("Fichiers csv (*.csv)"));
		QFile file(file_path);
		if (!file_path.isEmpty())
		{
			if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			{
				QMessageBox::critical(this, tr("Erreur"),
									  tr("Le fichier « %1 » n'a pas pu être écrit.").arg(file_path)+
									  "\n\n"+file.errorString());
				return r;
			}

				//getBom() ends its last row with a line break already, so
				//nothing is appended after it.
			QTextStream stream(&file);
			stream << getBom();
		}
	}
	return r;
}

QString BOMExportDialog::getBom()
{
	m_project->dataBase()->updateDB();
	auto query_ = m_project->dataBase()->newQuery(m_query_widget->queryStr());
	QString return_string;

	if (!query_.exec()) {
		qDebug() << "BOMExportDialog::getBom : query errir : " << query_.lastError();
	}
	else
	{
			//Which information each column holds. The header needs it to
			//name the column, and the rows need it because a stored form
			//and a written form are not always the same string - a
			//location path is stored as the location tree writes it and
			//read as the norm writes it.
		const auto fields_ = query_.record();

			//HEADERS
		if (ui->m_include_headers->isChecked())
		{
			QStringList header_name;
			for (auto i=0 ; i<fields_.count() ; ++i)
			{
				auto field_name = fields_.fieldName(i);

				qDebug() << "field name = " << field_name;
				if (field_name == "position") {
					header_name << tr("Position");
				} else if (field_name == "diagram_position") {
					header_name << tr("Position du folio");
				} else if (field_name == "designation_qty") {
					header_name << tr("Quantité numéro d'article", "Special field with name : designation quantity");
				} else {
						//An info key with no translation gives back an empty
						//string, and an empty header cell names no column. The
						//text is measured before it joins the list - once
						//appended, the list is never empty.
					const auto translated_name = QETInformation::translatedInfoKey(field_name);
					header_name << (translated_name.isEmpty() ? field_name
										 : translated_name);
				}

			}
			return_string = header_name.join(";") % "\n";
		}

			//ROWS
		while (query_.next())
		{
			auto i=0;
			QStringList values;
			while (query_.value(i).isValid())
			{
				values << QETInformation::displayedInfoValue(fields_.fieldName(i),
									    query_.value(i));
				++i;
			}

			return_string += values.join(";") % "\n";
			values.clear();
		}
	}

	qDebug() << return_string;
	return return_string;
}

/**
	@brief BOMExportDialog::on_m_format_as_bom_clicked
	@param checked
*/
void BOMExportDialog::on_m_format_as_bom_clicked(bool checked) {
	m_query_widget->setGroupBy("designation", checked);
	m_query_widget->setCount("COUNT(*) AS designation_qty", checked);
}
