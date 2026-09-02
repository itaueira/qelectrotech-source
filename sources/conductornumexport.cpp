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
#include <QMessageBox>

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
	@return true only when a file was written and its content reached the
	device; false when the export was cancelled, when there was nothing to
	export, and when the write failed.

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

	@par Nothing to export : measured before the file name is asked for
	@c wiresNum() went straight into the stream, so a project whose
	conductors carry no number opened the file, wrote zero bytes and
	returned @c true - an export that reported success and left an empty
	csv. The payload is now built and measured first, and an empty one
	reports and returns without ever reaching @c open(). Measuring before
	the save dialog rather than after also closes the hole in the decision
	above: @c QIODevice::WriteOnly truncates on open, and the confirmed
	overwrite that justifies it was destroying the user's previous export
	to make room for nothing. The price is that the string is built even
	when the user then cancels the dialog; the hash it reads was already
	filled by the constructor, so that costs one walk over a hash in
	memory, not a second walk over the project.

	@par An empty result is not a failure, so it does not speak like one
	The empty case reports with @c QMessageBox::information, not with
	@c critical, and the three outcomes now leave by three different doors:
	silence when the user closed the save dialog, because they know what
	they did; an information box when there is nothing to write; an error
	box, with the system reason, when a write was attempted and failed.
	The price is that the two states a designer cannot tell apart from that
	one information box - no number typed anywhere, versus numbers that sit
	only on folio report links, which @c fillHash() drops on purpose -
	carry the same sentence. Naming the second in the dialog would cost a
	longer string in twenty three catalogues for the rarer case, so it is
	named here instead.

	@par A write that does not land : flush, then read the error
	@c QTextStream buffers in user space, so the error of a write that
	could not land is not in the file until the buffer is pushed into it.
	@c flush() therefore comes first and @c QFile::error() is read straight
	after, while the device is still open. The next reader can break this
	without noticing by moving the test below a tidy @c {file.close()} :
	close() unsets the error once its own flush and close succeed, and the
	explicit flush above has already emptied the buffer, so that close does
	succeed and wipes the error the failed write left behind. The price is
	that the report arrives after the fact - the short file stays on disk,
	see the refusal below.

	@par Refused : the truncated file on a full disk
	The write still goes straight at the destination, so a disk that fills
	up in the middle leaves a csv that looks complete and is not. The
	branch above says so, but cannot undo it. @c QSaveFile is the fix -
	temporary file, atomic rename on commit(), original untouched on
	failure - and it is refused here for two measured reasons. The trap
	first : @c QTextStream only flushes past its own threshold, so commit()
	called while the stream is still alive renames a file the buffer never
	reached, and the stream then flushes into a closed device, trading a
	rare truncation for content lost on every export, with a syntax check
	as the only proof available today. Then coherence :
	wiringlistexport.cpp and ui/bomexportdialog.cpp write the same way, and
	the same refusal is already written into the second of them. The three
	belong in one change, with the flush before the commit written into it.

	@par Refused : the return value still cannot say why
	Four things can happen here - written, nothing to export, cancelled,
	failed - and @c bool has two seats. The header could grow an enum,
	because nothing outside this file needs @c toCsv() to stay a @c bool :
	its one caller in qetdiagrameditor.cpp discards the value, and
	cli_export.cpp never calls it, it calls @c wiresNum() and writes the
	file itself. The enum is refused because no caller would read it today,
	while it costs a recompile of both including translation units in a
	tree whose build is a single deferred step. What @c true means is
	narrowed instead, from "the file was opened" to "the file was written",
	which is the question the name asks. The price is that a future
	non-interactive caller cannot tell a project with no numbers from a
	full disk, and will have to add the enum then: the three dialogs are
	the only place that difference lives.
*/
bool ConductorNumExport::toCsv()
{
		//Measured before a file name is asked for: an export with nothing
		//in it must not reach open(), which truncates what is already
		//there.
	const QString csv = wiresNum();
	if (csv.isEmpty())
	{
		QMessageBox::information(m_parent_widget, QObject::tr("Rien à exporter"),
								 QObject::tr("Aucun conducteur numéroté n'a été trouvé dans ce projet. Aucun fichier n'a été écrit."));
		return false;
	}

		//save in csv file in same directory as project by default
	QString dir = m_project->currentDir();
	if (dir.isEmpty()) dir = QETApp::documentDir();
	QString name = dir % "/" % QObject::tr("numero_de_fileries_") % m_project->title() % ".csv";
	//    if(!name.endsWith(".csv")) {
	//        name += ".csv";
	//    }

	QString filename = QFileDialog::getSaveFileName(m_parent_widget, QObject::tr("Enregister sous... "), name, QObject::tr("Fichiers csv (*.csv)"));
	if (filename.isEmpty())
	{
			//The user closed the save dialog. Nothing was written and
			//nothing failed, so nothing is reported.
		return false;
	}

	QFile file(filename);
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
	stream << csv;

		//The buffer is pushed into the device first, and the device is
		//asked for its error while it is still open. Do not move this
		//below a close().
	stream.flush();
	if (file.error() != QFileDevice::NoError)
	{
		QMessageBox::critical(m_parent_widget, QObject::tr("Erreur"),
							  QObject::tr("L'écriture du fichier « %1 » a échoué.").arg(filename) %
							  "\n\n" % file.errorString());
		return false;
	}

	return true;
}

/**
	@brief ConductorNumExport::wiresNum
	@return the wire num formatted in csv : one row per numbered conductor
	end that is not a folio report, so a wire numbered at both ends is
	printed twice, once per marker. An empty string when the project holds
	no such end.

	@par An empty string needs no null twin here
	ui/bomexportdialog.cpp had to tell a query that failed from a query
	that answered no row, and did it with a null QString against an empty
	one. Nothing of that kind belongs here, because there is no failure to
	tell apart : @c fillHash() has no error path at all - it only skips, at
	the blank number test and at the two folio report tests - and the two
	pointers it walks through cannot be null. @c Conductor::m_text_item is
	built in the body of the only Conductor constructor, at
	conductor.cpp:164, and never set back to null; the only
	@c {new Terminal(...)} in the tree, at element.cpp:668, passes the
	owning element as the parent. So an empty return carries exactly one
	meaning, and toCsv() can report it as the ordinary answer it is. The
	price is that this leans on two invariants held elsewhere : a Conductor
	built one day without its text item, or a Terminal built with no
	element, turns the two unguarded dereferences in @c fillHash() into a
	crash instead of a skipped row. Guards are left out rather than added
	because a dead guard hides which invariant is actually being relied on,
	and this paragraph is the record of both.
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
