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
#ifndef ADDDIAGRAMCONTENTCOMMAND_H
#define ADDDIAGRAMCONTENTCOMMAND_H

#include "../diagramcontent.h"

#include <QUndoCommand>

class Diagram;

/**
	@brief The AddDiagramContentCommand class
	Undoable insertion of a content Diagram::fromXml has just added to a
	diagram: redo puts the items back, undo takes them away, and nothing
	else happens to them.

	PasteDiagramCommand does the same job plus two things, and those two
	things are right for the clipboard and wrong here. It reissues every
	uuid on its first redo, and - when the "erase label on copy" setting
	is on, which is the default - it clears the label, the formula, the
	comment and the location of every element and the text of every
	conductor.

	A macro is inserted the other way round. Its uuids were already
	reissued on the XML, before fromXml read it, so that the report can
	name what was drawn; renewing them again would leave that report
	pointing at nothing. And its labels are not leftovers from another
	drawing but the very thing the variables were substituted into: a
	macro whose conductor is named "F (${SECAO})" arrives with "F (6mm2)"
	written in it, and erasing it erases the answer the person typed.
*/
class AddDiagramContentCommand : public QUndoCommand
{
	public:
		AddDiagramContentCommand(Diagram *diagram,
					 const DiagramContent &content,
					 const QString &text = QString(),
					 QUndoCommand *parent = nullptr);
		~AddDiagramContentCommand() override;

	private:
		AddDiagramContentCommand(const AddDiagramContentCommand &);

	public:
		void undo() override;
		void redo() override;

	private:
			/// what was added
		DiagramContent m_content;
			/// the diagram it was added to
		Diagram *m_diagram;
			/// which kinds of item this command owns
		int m_filter;
			/// fromXml has already added them, so the first redo adds nothing
		bool m_first_redo;
};

#endif // ADDDIAGRAMCONTENTCOMMAND_H
