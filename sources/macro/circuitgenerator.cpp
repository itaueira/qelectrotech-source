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
#include "circuitgenerator.h"

#include "circuittable.h"
#include "macrofile.h"
#include "macrosubstitution.h"
#include "macrouuid.h"

#include "../borderproperties.h"
#include "../diagram.h"
#include "../diagramcommands.h"
#include "../diagramcontent.h"
#include "../qetproject.h"
#include "../titleblockproperties.h"
#include "../ElementsCollection/elementslocation.h"
#include "../undocommand/changetitleblockcommand.h"

#include <QDomElement>
#include <QList>
#include <QPointF>
#include <QScopedPointer>
#include <QSet>
#include <QUndoStack>

/**
	@brief CircuitGenerator::Report::text
	@return the whole report in one paragraph
*/
QString CircuitGenerator::Report::text() const
{
	QStringList lines;

	if (generated > 0) {
		lines << tr("%1 circuit(s) généré(s) sur %2 folio(s).")
				 .arg(generated)
				 .arg(sheets);
	} else {
		lines << tr("Aucun circuit n'a été généré.");
	}

	lines << problems;
	return lines.join(QLatin1Char('\n'));
}

/**
	@brief CircuitGenerator::CircuitGenerator
	@param project : the project the circuits are drawn into
*/
CircuitGenerator::CircuitGenerator(QETProject *project) :
	m_project(project)
{}

/**
	@brief CircuitGenerator::sheetFromProject
	@return the capacity of the folios this generation will make

	Read from the project's default border and not from an existing folio:
	that is what addNewDiagram() is about to give the folios, so it is what
	the placement has to be computed against. Reading it here also means
	nothing has to be created before knowing how many folios are needed.
*/
CircuitLayout::Sheet CircuitGenerator::sheetFromProject() const
{
	CircuitLayout::Sheet sheet;
	if (!m_project) {
		return sheet;
	}

	const BorderProperties border = m_project->defaultBorderProperties();
	sheet.columns_count = border.columns_count;
	sheet.columns_width = border.columns_width;

		//The same arithmetic as BorderTitleBlock::insideBorderRect(): the top
		//left of the drawing area, which is where column zero starts.
	qreal left = Diagram::margin;
	qreal top = Diagram::margin;
	if (border.display_rows) {
		left += border.rows_header_width;
	}
	if (border.display_columns) {
		top += border.columns_header_height;
	}
	sheet.origin = QPointF(left, top);

	return sheet;
}

/**
	@brief CircuitGenerator::measure
	@param file
	@param values : the answers of the first row that uses this macro
	@return the width the circuit takes once drawn, in scene units

	A .qetmak says where its elements sit but never how wide the drawing
	comes out: the width is the union of what the symbols occupy, and only
	the program that draws them knows that. So it is drawn - in a project
	made for the purpose and thrown away immediately, the same trick the
	insertion ghost already uses. Nothing of the real project is touched,
	and every width is known before the first circuit is placed.
*/
qreal CircuitGenerator::measure(MacroFile &file, const QHash<QString, QString> &values) const
{
	if (file.isNull()) {
		return 0.0;
	}

	QScopedPointer<QETProject> dummy_project(new QETProject());
	file.importCollection(dummy_project->embeddedElementCollection());

	Diagram *dummy_diagram = dummy_project->addNewDiagram();
	if (!dummy_diagram) {
		return 0.0;
	}

	QDomElement node = file.clonedDiagramNode();
	if (node.isNull()) {
		return 0.0;
	}

		//Measured with the answers of the first row and not with the markers:
		//a mark written -QM12 is not as wide as ${TAG}, and the difference
		//lands on the column count.
	if (!file.parameters().isEmpty()) {
		const MacroSubstitution::Result result = MacroSubstitution::apply(node, values);
		Q_UNUSED(result)
	}

	dummy_diagram->fromXml(node, QPointF(0, 0), false, nullptr);
	return dummy_diagram->itemsBoundingRect().width();
}

/**
	@brief CircuitGenerator::addSheet
	@param options
	@return the folio that was added, nullptr when it could not be
*/
Diagram *CircuitGenerator::addSheet(const Options &options)
{
	Diagram *diagram = m_project->addNewDiagram();
	if (!diagram) {
		return nullptr;
	}

	if (!options.sheet_title.isEmpty())
	{
		const TitleBlockProperties before = diagram->border_and_titleblock.exportTitleBlock();
		TitleBlockProperties after = before;
		after.title = options.sheet_title;
		m_project->undoStack()->push(new ChangeTitleBlockCommand(diagram, before, after));
	}

	return diagram;
}

/**
	@brief CircuitGenerator::generate
	@param table
	@param options
	@return what was drawn, and what was not
*/
CircuitGenerator::Report CircuitGenerator::generate(const CircuitTable &table,
						   const Options &options)
{
	Report report;

	if (!m_project) {
		report.problems << tr("Aucun projet ouvert.");
		return report;
	}
	if (m_project->isReadOnly()) {
		report.problems << tr("Le projet est en lecture seule.");
		return report;
	}

		//What the table itself already knows it cannot generate: the row with
		//no macro, the required variable left empty, the value the type
		//refuses. Said once, here, and the row is out of the run.
	QSet<int> skipped;
	const QList<CircuitTable::Problem> problems = table.problems();
	for (const CircuitTable::Problem &problem : problems)
	{
		skipped << problem.row;
		report.problems << problem.text();
	}

		//Every distinct macro is opened once, and the row order is kept: the
		//generated folios read in the order the table was written, which is
		//the order the person will look for them in.
	QHash<QString, MacroFile> files;
	QHash<QString, QHash<QString, QString> > first_values;
	QList<int> rows;

	for (int i = 0; i < table.rowCount(); ++i)
	{
		if (skipped.contains(i)) {
			continue;
		}

		const QString macro_path = table.macroPath(i);
		if (!files.contains(macro_path))
		{
			MacroFile file;
			const ElementsLocation location(macro_path);
			const QString file_path = location.fileSystemPath();
			if (file_path.isEmpty()) {
				file.load(macro_path);
			} else {
				file.load(file_path);
			}
			files.insert(macro_path, file);
			first_values.insert(macro_path, table.values(i));
		}

		if (files.value(macro_path).isNull())
		{
			report.problems << tr("Ligne %1 : %2")
					   .arg(i + 1)
					   .arg(files.value(macro_path).errorText());
			continue;
		}

		rows << i;
	}

	if (rows.isEmpty()) {
		return report;
	}

	const CircuitLayout::Sheet sheet = sheetFromProject();
	if (!sheet.isValid()) {
		report.problems << tr("Le cadre des folios de ce projet ne déclare "
				      "aucune colonne : impossible de placer les circuits.");
		return report;
	}

		//The symbols the macros carry travel into the project before anything
		//is drawn, exactly as the drop on a folio does it. This is not
		//undoable there either: what a Ctrl+Z takes back is the drawing, not
		//the symbols it made available.
	QHash<QString, int> widths;
	for (auto it = files.begin() ; it != files.end() ; ++it)
	{
		if (it.value().isNull()) {
			continue;
		}
		it.value().importCollection(m_project->embeddedElementCollection());
		widths.insert(it.key(),
			      CircuitLayout::columnsFor(measure(it.value(),
								first_values.value(it.key())),
							sheet));
	}

	QList<int> row_widths;
	row_widths.reserve(rows.count());
	for (const int row : rows) {
		row_widths << widths.value(table.macroPath(row), 1);
	}

	const QList<CircuitLayout::Placement> placements =
			CircuitLayout::place(row_widths, sheet, options.circuits_per_sheet);
	if (placements.isEmpty()) {
		return report;
	}

	const int sheet_count = CircuitLayout::sheetsUsed(placements);

	QUndoStack *stack = m_project->undoStack();
	stack->beginMacro(tr("générer %1 circuit(s)").arg(placements.count()));

	QList<Diagram *> sheets;
	sheets.reserve(sheet_count);
	for (int i = 0 ; i < sheet_count ; ++i) {
		sheets << addSheet(options);
	}

	for (int i = 0 ; i < placements.count() ; ++i)
	{
		const int row = rows.at(i);
		const CircuitLayout::Placement &placement = placements.at(i);

		Diagram *diagram = sheets.value(placement.sheet, nullptr);
		if (!diagram) {
			report.problems << tr("Ligne %1 : la folio n'a pas pu être créée.")
					   .arg(row + 1);
			continue;
		}

		MacroFile &file = files[table.macroPath(row)];
		QDomElement clone = file.clonedDiagramNode();
		if (clone.isNull()) {
			report.problems << tr("Ligne %1 : le macro ne contient aucun schéma.")
					   .arg(row + 1);
			continue;
		}

		if (!file.parameters().isEmpty())
		{
			const MacroSubstitution::Result result =
					MacroSubstitution::apply(clone, table.values(row));
			if (!result.ok)
			{
				report.problems << tr("Ligne %1 : %2")
						   .arg(row + 1)
						   .arg(result.errorText());
				continue;
			}
		}

			//Without this the twenty circuits would share one identity each,
			//and the cross references of the second one would point at the
			//first. Done on the clone, so the macro in memory keeps the
			//uuids it was written with.
		const MacroUuid::Result renewed = MacroUuid::renew(clone);
		report.issued.insert(table.row(row).id, renewed.issued);

		DiagramContent content;
		diagram->fromXml(clone, placement.pos, false, &content);
		diagram->refreshContents();
		diagram->undoStack().push(new PasteDiagramCommand(diagram, content));
		++report.generated;

		if (placement.oversized) {
			report.problems << tr("Ligne %1 : le circuit est plus large que la "
					      "folio et déborde du cadre.")
					   .arg(row + 1);
		}
	}

	stack->endMacro();
	report.sheets = sheet_count;

	return report;
}
