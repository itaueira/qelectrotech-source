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

	@par The payload before the destination
	getBom() is called before the file is opened, not from inside the
	statement that writes the stream. @c QIODevice::WriteOnly truncates on
	open, so the old order - open first, query second - destroyed the
	nomenclature the user had exported before and only then found out
	whether the query could answer at all. A query that failed therefore
	replaced a good file with zero bytes, and the report added by the
	previous commit made that worse rather than better: the open now
	succeeds, so the empty result is written out successfully. Built first,
	a failed query costs the destination nothing. The price is that the
	whole list is held in memory before a byte is written, which is what
	getBom() already did - it hands back the entire csv as one QString.

	@par A payload that could not be produced
	getBom() returns a null QString when the query failed and an empty but
	non null one when the query ran and matched no row; only the null case
	skips the write, and getBom() has already told the user why.
	@c {QString()} is null and @c {QString("")} is not - a distinction Qt
	documents, not an implementation detail - and it is the only channel
	available, because getBom() is declared in the header as returning
	QString and the header is not this file's to change. The price is a
	contract the next reader can break without noticing: a
	@c {QString::clear()} on the success path would make it null again and
	silently turn an empty nomenclature into a skipped write. That failure
	is cheap - a zero byte file plus a dialog, which is today's behaviour
	with a message added - and a bool return or an out parameter on
	getBom() would remove the contract entirely, for the cost of a header
	change this file may not make.
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
				//Built before the destination is opened: opening truncates,
				//and a query that cannot answer must not cost the user the
				//file already sitting there. A null string means the query
				//failed, and getBom() has already reported it.
			const QString bom = getBom();
			if (bom.isNull())
			{
				return r;
			}

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
			stream << bom;
		}
	}
	return r;
}

/**
	@brief BOMExportDialog::getBom
	@return the nomenclature formatted as csv : a null QString if the query
	could not be executed, an empty but non null one if it executed and
	matched nothing. See exec(), which acts on that difference.

	@par A failed query : say it, and hand back nothing
	The branch was already here and only wrote one line to the log, with
	the word "errir" in it; the dialog then carried on and wrote the empty
	string it was holding. The user was left with a zero byte csv and no
	message at all. The report is now a @c QMessageBox::critical carrying
	the French sentence plus @c {QSqlError::text()} appended outside
	@c tr(), the same shape the write error in exec() uses, and the return
	is a null string so exec() leaves the destination untouched. The price
	is that getBom() is a public member and now raises a modal dialog, so a
	headless caller would hang on it. None exists: QETDiagramEditor is the
	only place that builds this dialog and exec() is the only place that
	calls getBom() - and the class is a QDialog, which cannot serve a
	headless caller in any case.

	@par A query that matched nothing is not a query that failed
	The two are separated by the return value of @c {QSqlQuery::exec()},
	measured rather than presumed, because @c return_string on its own
	cannot tell them apart: with the headers checkbox cleared, a project
	holding no matching component builds exactly the same empty string as a
	query that never ran. So the success path starts from an empty but non
	null string, and a project with no matching component still gets the
	file it asked for. With the headers checkbox on, which is how
	bomexportdialog.ui ships it, that file carries the header row and is
	visibly a valid empty nomenclature. The price is that with the headers
	cleared it is zero bytes and nothing says so. No dialog was added for
	it: after this change silence can only mean the query answered, and a
	second dialog would interrupt a legitimate export in a corner the .ui
	does not ship. wiringlistexport.cpp does report an empty payload, but
	that is not a precedent against this - its headers are unconditional,
	so an empty string there can only mean the project could not be read.

	@par The list is no longer dumped into the log
	@c {qDebug() << return_string} wrote the whole nomenclature on every
	export and is gone. Measured, not assumed: qDebug() here does not reach
	a console. main.cpp installs QetLogger through qInstallMessageHandler,
	so every message lands in a rotating file on disk and in the crash
	ring. QetLogger truncates each message at kMaxMessageBytes, 4096, so
	the dump was never the whole list to begin with; the ring holds
	kCapacityEntries of kEntryBytes, so the dump evicts the diagnostic
	lines a crash report exists to carry; and
	@c {QetLogger::buildDiagnosticsReport()} reads the session log file
	back for the user to attach to a public bug tracker, with redact()
	replacing the home directory and nothing else. A customer's material
	list - designations, manufacturer references, quantities - was
	travelling into a file meant to be published. The price is that whoever
	debugged the csv by reading the log loses it, and has the exported file
	instead, which is the same content and not truncated at 4096 bytes.

	@par What was left alone, on purpose
	The per column trace below stays. It is bounded - one short line per
	column, and only when headers are asked for - and it carries schema
	names, not project data, so none of the three arguments above reaches
	it; it is also the only trace over the translatedInfoKey fallback added
	just under it. Its severity stays at qDebug for the same reason, while
	the failed query moves to qWarning, which is what the rest of sources
	uses for a failure and what the log file records as one. Neither gets a
	logging category: qetlogger.h lists categories among the things
	discussion 644 deliberately left out, and the 146 qDebug calls across
	42 files in sources are all uncategorised, so introducing one here
	would be the inconsistency rather than the fix.

	@par Refused : the query is executed twice
	@c {projectDataBase::newQuery()} returns @c {QSqlQuery(query, db)}, and
	that constructor executes the query when the string is not empty, so
	the @c {query_.exec()} below is a second execution of the same select
	on every export. Reading @c {QSqlQuery::isActive()} instead would halve
	the work and would surface the constructor's own error, but it trades a
	path proved by every export that has ever worked for one that cannot be
	proved without running the program, and the build here is a single
	deferred step. Left as it stands, deliberately: detection is sound
	either way, since a statement that fails to prepare fails to execute
	too. The price is one redundant select per export.

	@par Refused : the truncated file on a full disk
	The write still goes straight at the destination, so a disk that fills
	up in the middle leaves a csv that looks complete and is not.
	@c QSaveFile is the fix - temporary file, atomic rename on commit(),
	original untouched on failure - and it would fit in the few lines of
	exec(). It is refused here for two measured reasons. The trap first :
	@c QTextStream buffers in user space and only flushes past its own
	threshold, so commit() called while the stream is still alive renames a
	file the buffer never reached, and the stream then flushes into a
	closed device. That turns a rare truncation into content lost on every
	export, and a syntax check is the only proof available today. Then
	coherence : conductornumexport.cpp and wiringlistexport.cpp write the
	same way and sit outside this file, so fixing one exporter of three
	would split the write path that the previous commit spent its whole
	argument bringing together. The three belong in one change, with the
	flush before commit written into it.
*/
QString BOMExportDialog::getBom()
{
	m_project->dataBase()->updateDB();
	auto query_ = m_project->dataBase()->newQuery(m_query_widget->queryStr());
	QString return_string;

	if (!query_.exec())
	{
		qWarning() << "BOMExportDialog::getBom : query failed :" << query_.lastError();
		QMessageBox::critical(this, tr("Erreur"),
							  tr("La nomenclature n'a pas pu être extraite du projet.")+
							  "\n\n"+query_.lastError().text());
			//return_string is left null here on purpose: that is how exec()
			//tells a query that failed from a query that answered no row.
	}
	else
	{
			//Empty, but not null : the query answered, and no matching
			//component is a legitimate answer.
		return_string = QString("");

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
