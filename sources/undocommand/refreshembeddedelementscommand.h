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
#ifndef REFRESHEMBEDDEDELEMENTSCOMMAND_H
#define REFRESHEMBEDDEDELEMENTSCOMMAND_H

#include <QDomDocument>
#include <QList>
#include <QPointer>
#include <QString>
#include <QUndoCommand>

class QETProject;

/**
	@brief The RefreshEmbeddedElementsCommand class
	Replace the definitions of some symbols in a project's own collection,
	and redraw the elements placed from them, with one Ctrl+Z.

	Built by EmbeddedElementRefresh::bringFromCollections(), which decides
	what is brought: a definition whose terminals differ from the placed
	elements' only when the project is about to be reloaded. The command
	swaps definitions, gives the elements the box of the definition, and
	redraws; it does not judge them.

	The terminals of a new definition that sit where a terminal of the old
	one sat take its uuid (TerminalUuids::keep()), the way replacing a copy
	from the import dialog does. A conductor is saved against those uuids,
	so this is what keeps the wires attached when the project is opened
	again.
*/
class RefreshEmbeddedElementsCommand : public QUndoCommand
{
	public:
		/// One symbol: the path of its copy, and the <definition> to put there
		struct Change
		{
			QString embedded_path;
			QDomDocument definition;
				/// The record of the version the copy was taken from
				/// (ElementOrigin), as the attributes of its <element>: kept
				/// by the command, empty in what a caller gives it
			QString origin_uuid;
			QString origin_hash;
		};

		RefreshEmbeddedElementsCommand(QETProject *project,
					       const QList<Change> &changes,
					       QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

	private:
		void apply(const QList<Change> &definitions);

		QPointer<QETProject> m_project;
		QList<Change> m_old;
		QList<Change> m_new;
};

#endif // REFRESHEMBEDDEDELEMENTSCOMMAND_H
