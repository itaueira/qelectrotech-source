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
#include "uibench.h"

#include "../../../../sources/diagram.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetresult.h"

#include <catch2/catch.hpp>

#include <QUndoStack>

/*
	The crash-recovery backup, and the work it used to do for nothing.

	The timer fires every twenty minutes and used to serialise the whole
	project each time, whether or not anything had been drawn: a project
	opened and left alone all afternoon paid the same seconds of frozen
	interface as one being edited. What is proved here is the guard that
	stopped it, and above all that the guard did not go too far.

	The four cases below are the contract, and the order they are written
	in is the order they matter in:

	  1. a project nobody touched is not serialised;
	  2. a project that was touched is;
	  3. two passes with nothing in between serialise once;
	  4. touching it again after a backup serialises again.

	Case 2 and case 4 are the ones worth having. The way to get this wrong
	is not to write a backup too often - that only costs time - it is to
	stop writing one that was needed, and that costs the drawing.

	Two paths mark a project as changed and both are exercised, because
	only one of them goes through the undo stack: everything drawn on a
	sheet is a command pushed there, while a folio option, an I/O list or
	the IEC setting are applied straight and only call setModified().

	Nothing here needs a screen: the project is built as text in a
	temporary directory, opened by the very QETProject the program uses,
	and the backup is written to the recovery file the program writes to.
*/

namespace
{
	/// The smallest project that opens: one sheet, nothing on it.
	QString projectXml()
	{
		return QStringLiteral(
			       "<project title=\"backup\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"Sheet\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	/**
		The backup switch is a static shared by every project, and the
		bench turns it off in its own constructor so that a test does not
		drop a recovery file. A case about the backup has to turn it back
		on, and put it back the way it found it however the case ends.
	*/
	class BackupEnabled
	{
		public:
			BackupEnabled() {QETProject::setBackupEnabled(true);}
			~BackupEnabled() {QETProject::setBackupEnabled(false);}

			BackupEnabled(const BackupEnabled &) = delete;
			BackupEnabled &operator=(const BackupEnabled &) = delete;
	};
}

TEST_CASE("T42 — o backup automático não reserializa um projeto que não mudou",
	  "[backup][qetproject][T42]")
{
	UiBench::ScratchProject bench(projectXml(), QStringLiteral("backup.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 1);

	BackupEnabled backup_on;

	SECTION("1. um projeto recém-aberto e jamais tocado não é serializado")
	{
			//The file it was opened from already holds this state, so there
			//is nothing a recovery copy could add to it.
		REQUIRE_FALSE(bench->writeBackup());
			//And it stays that way: this is the twenty-minute tick that used
			//to cost seconds of frozen interface for nothing.
		REQUIRE_FALSE(bench->writeBackup());
	}

	SECTION("2. um projeto modificado é serializado")
	{
		SECTION("modificação empilhada no desfazer, como tudo que se desenha")
		{
			bench->addNewDiagram();
			REQUIRE(bench.diagramCount() == 2);
			REQUIRE(bench->writeBackup());
		}

		SECTION("modificação aplicada direto, sem comando de desfazer")
		{
				//The path of a folio option or of the IEC setting: the
				//project is told it changed, and nothing is pushed.
			bench->setModified(true);
			REQUIRE(bench->undoStack()->isClean());
			REQUIRE(bench->writeBackup());
		}
	}

	SECTION("3. duas passadas sem modificação entre elas serializam uma vez só")
	{
		bench->addNewDiagram();
		REQUIRE(bench->writeBackup());

			//The revision is recorded by the worker that writes the file, and
			//only when the write succeeded: waiting here is waiting for that
			//answer, not for a delay.
		bench->waitForBackup();

		REQUIRE_FALSE(bench->writeBackup());
		REQUIRE_FALSE(bench->writeBackup());
	}

	SECTION("4. modificar depois de um backup serializa de novo")
	{
		bench->addNewDiagram();
		REQUIRE(bench->writeBackup());
		bench->waitForBackup();
		REQUIRE_FALSE(bench->writeBackup());

		SECTION("segunda modificação da sessão, com o projeto já sujo")
		{
				//The trap: the modified flag is already true here, so
				//setModified(true) changes nothing and announces nothing.
				//A guard reading that flag would take the project for
				//already copied and drop this edit from the backup.
			REQUIRE(bench->projectOptionsWereModified());
			bench->setModified(true);
			REQUIRE(bench->writeBackup());
		}

		SECTION("terceira folha, empilhada no desfazer")
		{
			bench->addNewDiagram();
			REQUIRE(bench->writeBackup());
		}

		SECTION("desfazer também é uma mudança")
		{
				//Undoing takes the sheet back out, so what the backup file
				//holds is no longer what the project is.
			bench->undoStack()->undo();
			REQUIRE(bench->writeBackup());
		}
	}

	SECTION("salvar conta como cópia: o .qet no disco já tem o estado")
	{
		bench->addNewDiagram();

		const QETResult saved = bench->write();
		INFO(saved.errorMessage().toStdString());
		REQUIRE(saved.isOk());

		REQUIRE_FALSE(bench->writeBackup());

			//Until the next edit, which is not covered by that save.
		bench->addNewDiagram();
		REQUIRE(bench->writeBackup());
	}

		//Whatever the section did, no write may be left in flight when the
		//project is destroyed under the worker.
	bench->waitForBackup();
}
