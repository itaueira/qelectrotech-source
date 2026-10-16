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
#ifndef REPORTTAGS_H
#define REPORTTAGS_H

#include <QList>
#include <QString>

class DynamicElementTextItem;
class Element;
class QETProject;
class QUndoCommand;

/**
	The tag of every folio report: S1, S2... A folio report has no
	identification of its own - the text it shows is the reference to the
	other end - so it is given one when it is created, the next number
	free in the project, kept in its information
	QETInformation::ELMT_REPORT_TAG and never changed after. A reference
	to the arrow can then name it by its folio and its tag, which is read
	on a printed sheet without counting rows and columns.

	The tag is shown by a text of the arrow, an element information text,
	which is moved like any text of an element and cannot be typed into.
	It is only created when the project shows the tags, so that a project
	that never does keeps its arrows as they always were.
*/
namespace ReportTags
{
	bool isReport(const Element *element);
	QString tagOf(const Element *report);
	int numberOf(const QString &tag, const QString &prefix);
	QList<Element *> reports(const QETProject *project);
	QString nextTag(QETProject *project);

	void give(Element *report, QETProject *project);
	void renewRepeated(const QList<Element *> &pasted, QETProject *project);
	QUndoCommand *tagAllCommand(QETProject *project);
	QUndoCommand *formatCommand(QETProject *project);

	bool isTagText(const DynamicElementTextItem *text);
	DynamicElementTextItem *tagText(const Element *report);
	DynamicElementTextItem *newTagText(Element *report, const QString &tag,
									   QETProject *project);
}

#endif // REPORTTAGS_H
