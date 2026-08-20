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
#ifndef CONDUCTORTEXTCOMMAND_H
#define CONDUCTORTEXTCOMMAND_H

#include <QList>
#include <QPointer>
#include <QUndoCommand>

#include "../conductorproperties.h"

class Conductor;

/**
	@brief Shows or hides the number of the selected conductors.

	Renumbering fills a folio with text. The 24 Vdc that reads once is
	information; the same 24 Vdc on twenty stretches of the same potential is
	twenty things in the way of the drawing. This turns the text off on the
	stretches that do not need it.

	**It does not touch the number.** The conductor keeps the number it had,
	the wiring list keeps it, the terminal keeps it — only the drawing stops
	repeating it. Anything that changed the number instead would be a cleanup
	tool that silently edits the schematic, which is a different and much
	worse thing.
*/
class ConductorTextCommand : public QUndoCommand
{
	public:
		ConductorTextCommand(const QList<Conductor *> &conductors,
				     bool visible,
				     QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

		/// how many conductors the command touches
		int conductorCount() const;
		bool isEmpty() const;

	private:
		struct Change
		{
			QPointer<Conductor> conductor;
			bool old_visible = true;
		};

		void apply(bool forward);

		QList<Change> m_changes;
		bool m_visible = true;
};

#endif // CONDUCTORTEXTCOMMAND_H
