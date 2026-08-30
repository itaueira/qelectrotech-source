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
#include "../diagramcontent.h"
#include "../qetproject.h"
#include "../titleblockproperties.h"
#include "../ElementsCollection/elementslocation.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qet.h"
#include "../qetgraphicsitem/element.h"
#include "../undocommand/adddiagramcontentcommand.h"
#include "../undocommand/changetitleblockcommand.h"
#include "../undocommand/deleteqgraphicsitemcommand.h"

#include <QDomElement>
#include <QGraphicsItem>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QScopedPointer>
#include <QSet>
#include <QStringList>
#include <QUndoStack>
#include <QUuid>

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
		diagram->undoStack().push(new AddDiagramContentCommand(
			diagram, content, tr("générer un circuit", "undo caption")));

			//Where it landed, not where it was asked to land: fromXml snaps the
			//move to the grid. The regeneration measures the same way, and only
			//then does it put the circuit back where it was.
		report.positions.insert(table.row(row).id, anchorOf(content));
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

/**
	@brief CircuitGenerator::regenerate
	@param table
	@param rows : the rows to redraw, every generated row when empty
	@return what was redrawn, and what was not

	Regenerating is not generating again: the folios already exist, the
	circuits already sit somewhere, and the nineteen rows nobody touched
	have to come out of it with the same labels, the same wire numbers and
	the same position they had before (CU-08.5). So each row is taken on
	its own - its own drawing found, removed and redrawn where it was -
	and a row that cannot be redone safely is refused whole rather than
	half done.
*/
CircuitGenerator::Report CircuitGenerator::regenerate(const CircuitTable &table,
						      const QList<int> &rows)
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

		//The same refusals as the generation. Redrawing a row whose answers
		//stopped being valid would remove a circuit that works and put
		//nothing in its place.
	QSet<int> skipped;
	const QList<CircuitTable::Problem> problems = table.problems();
	for (const CircuitTable::Problem &problem : problems)
	{
		skipped << problem.row;
		report.problems << problem.text();
	}

		//No row named means every row that was drawn once: asking to
		//regenerate without saying which row is asking the drawing to catch
		//up with the table. A row that was never drawn has nothing to catch
		//up with, and it is the generation that draws it the first time.
	QList<int> wanted = rows;
	if (wanted.isEmpty())
	{
		for (int i = 0 ; i < table.rowCount() ; ++i) {
			if (table.wasGenerated(i)) {
				wanted << i;
			}
		}
	}

	QList<int> targets;
	for (const int row : wanted)
	{
		if (row < 0 || row >= table.rowCount() || targets.contains(row)) {
			continue;
		}
		if (skipped.contains(row)) {
			continue;
		}
		if (!table.wasGenerated(row))
		{
			report.problems << tr("Ligne %1 : ce circuit n'a jamais été généré ; "
					      "utilisez « Générer ».")
					   .arg(row + 1);
			continue;
		}
		targets << row;
	}

	if (targets.isEmpty()) {
		return report;
	}

	QUndoStack *stack = m_project->undoStack();
	bool macro_open = false;
	QSet<Diagram *> touched;

	for (const int row : targets)
	{
		const CircuitRow line = table.row(row);

			//The folio is found by the drawing, not by a folio number: folios
			//are reordered, and Diagram carries no identity of its own into
			//the file. Whatever folio still holds the items this row issued is
			//the folio this row lives on.
		QSet<QString> issued;
		for (const QString &uuid : line.issued) {
			issued << uuid;
		}

		Diagram *diagram = nullptr;
		QList<QGraphicsItem *> old_items;
		const QList<Diagram *> diagrams = m_project->diagrams();
		for (Diagram *candidate : diagrams)
		{
			const QList<QGraphicsItem *> found = itemsOf(candidate, issued);
			if (!found.isEmpty())
			{
				diagram = candidate;
				old_items = found;
				break;
			}
		}

		if (!diagram)
		{
			report.problems << tr("Ligne %1 : le circuit n'a pas été retrouvé dans "
					      "le projet ; il a sans doute été supprimé.")
					   .arg(row + 1);
			continue;
		}

		MacroFile file;
		const ElementsLocation location(line.macro_path);
		const QString file_path = location.fileSystemPath();
		if (file_path.isEmpty()) {
			file.load(line.macro_path);
		} else {
			file.load(file_path);
		}

		if (file.isNull())
		{
			report.problems << tr("Ligne %1 : %2")
					   .arg(row + 1)
					   .arg(file.errorText());
			continue;
		}

		QDomElement clone = file.clonedDiagramNode();
		if (clone.isNull())
		{
			report.problems << tr("Ligne %1 : le macro ne contient aucun schéma.")
					   .arg(row + 1);
			continue;
		}

			//What cannot be given a new identity cannot be removed by identity
			//either: the shape drawn by the first generation is not in the list
			//this row issued, so it would stay on the folio while a second copy
			//of it is drawn on top. Said before anything is removed.
		const QStringList refused = unrenewable(clone);
		if (!refused.isEmpty())
		{
			report.problems << tr("Ligne %1 : ce macro contient %2 ; il peut être "
					      "inséré, mais pas régénéré.")
					   .arg(row + 1)
					   .arg(refused.join(tr(", ")));
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

			//Measured before anything is removed, and measured the way
			//Diagram::fromXml measures what it is about to place. Measuring it
			//any other way would walk the circuit a grid step across the folio
			//at every regeneration.
		QPointF anchor = anchorOf(old_items);
		if (anchor.isNull()) {
			anchor = line.position;
		}

			//Built from the selection and not by hand: the constructor is what
			//tells a wire belonging to this circuit from a wire someone drew
			//out of it, and it is also what finds the terminals that cannot be
			//deleted.
		diagram->clearSelection();
		for (QGraphicsItem *item : old_items) {
			item->setSelected(true);
		}
		const DiagramContent old_content(diagram, true);
		diagram->clearSelection();

		if (!old_content.m_conductors_to_update.isEmpty())
		{
			report.problems << tr("Ligne %1 : %2 conducteur(s) relient ce circuit au "
					      "reste de la folio et seraient supprimés avec lui. "
					      "Débranchez-les d'abord.")
					   .arg(row + 1)
					   .arg(old_content.m_conductors_to_update.count());
			continue;
		}

		if (DeleteQGraphicsItemCommand::hasNonDeletableTerminal(old_content))
		{
			report.problems << tr("Ligne %1 : ce circuit contient une borne pontée ou "
					      "à plusieurs niveaux, qui ne peut pas être supprimée.")
					   .arg(row + 1);
			continue;
		}

			//Opened here and not before the loop: a run where every row is
			//refused must leave no undo entry behind at all.
		if (!macro_open)
		{
			stack->beginMacro(tr("régénérer %1 circuit(s)").arg(targets.count()));
			macro_open = true;
		}

			//The symbols travel again, because the macro may have gained one
			//since the circuit was first drawn.
		file.importCollection(m_project->embeddedElementCollection());

		stack->push(new DeleteQGraphicsItemCommand(diagram, old_content));

		const MacroUuid::Result renewed = MacroUuid::renew(clone);

		DiagramContent content;
		diagram->fromXml(clone, anchor, false, &content);
		diagram->refreshContents();
		stack->push(new AddDiagramContentCommand(diagram, content,
							tr("régénérer un circuit", "undo caption")));

		report.issued.insert(line.id, renewed.issued);
		report.positions.insert(line.id, anchorOf(content));
		touched << diagram;
		++report.generated;
	}

	if (macro_open) {
		stack->endMacro();
	}

	report.sheets = touched.count();
	return report;
}

/**
	@brief CircuitGenerator::itemsOf
	@param diagram
	@param uuids : the uuids one row issued when it was drawn
	@return the items of that folio carrying one of those uuids

	The uuid is what survives everything a person does to a drawing:
	moving it, renaming it, changing its properties, moving the folio it
	sits on. It is the only handle a table row has on what it drew.
*/
QList<QGraphicsItem *> CircuitGenerator::itemsOf(Diagram *diagram, const QSet<QString> &uuids)
{
	QList<QGraphicsItem *> found;
	if (!diagram || uuids.isEmpty()) {
		return found;
	}

		//Compared as uuids and not as text: the same uuid is written with
		//braces in one place and without them in another, and two spellings
		//of one identity would look like two identities.
	QSet<QUuid> wanted;
	for (const QString &text : uuids)
	{
		const QUuid uuid(text);
		if (!uuid.isNull()) {
			wanted << uuid;
		}
	}
	if (wanted.isEmpty()) {
		return found;
	}

	const QList<QGraphicsItem *> items = diagram->items();
	for (QGraphicsItem *item : items)
	{
		switch (item->type())
		{
			case Element::Type:
			{
				auto *element = qgraphicsitem_cast<Element *>(item);
				if (element && wanted.contains(element->uuid())) {
					found << item;
				}
				break;
			}
			case Conductor::Type:
			{
				auto *conductor = qgraphicsitem_cast<Conductor *>(item);
				if (conductor && wanted.contains(conductor->uuid())) {
					found << item;
				}
				break;
			}
			default:
				break;
		}
	}

	return found;
}

/**
	@brief CircuitGenerator::anchorOf
	@param items
	@return the top left of what those items occupy in the folio

	The same arithmetic as Diagram::fromXml, on purpose: fromXml unites
	the scene rectangles of everything it added but the conductors - they
	are loaded afterwards - and moves the lot by the difference between
	that corner and the position it was given. A circuit is redrawn in the
	same place only if it is measured with the same ruler.
*/
QPointF CircuitGenerator::anchorOf(const QList<QGraphicsItem *> &items)
{
	QRectF rect;
	for (QGraphicsItem *item : items)
	{
		if (item->type() == Conductor::Type) {
			continue;
		}
		rect = rect.united(item->mapToScene(item->boundingRect()).boundingRect());
	}

	return rect.topLeft();
}

/**
	@brief CircuitGenerator::anchorOf
	@param content : what Diagram::fromXml has just added
	@return where it actually landed

	Not where it was asked to land: fromXml snaps the move to the grid,
	so the corner that comes out is not the corner that went in. What the
	table has to remember is the one that came out.
*/
QPointF CircuitGenerator::anchorOf(const DiagramContent &content)
{
	return anchorOf(content.items(DiagramContent::Elements
				      | DiagramContent::TextFields
				      | DiagramContent::Images
				      | DiagramContent::Shapes
				      | DiagramContent::Tables
				      | DiagramContent::TerminalStrip));
}

/**
	@brief CircuitGenerator::unrenewable
	@param node : the schema node of a macro
	@return the kinds of item it carries that cannot be reissued, named
	for the report

	MacroUuid::renew() knows three tags: the element, its dynamic texts
	and the conductor. Everything else a folio can hold is copied as it
	is, with no identity of its own. Inserting such a macro is fine, and
	generating it once is fine; regenerating it is not, because the row
	has no way to find the copy it drew last time and would leave it on
	the folio under the new one.
*/
QStringList CircuitGenerator::unrenewable(const QDomElement &node)
{
	QStringList kinds;
	if (node.isNull()) {
		return kinds;
	}

	if (!QET::findInDomElement(node, QStringLiteral("shapes"),
				   QStringLiteral("shape")).isEmpty()) {
		kinds << tr("des formes");
	}
	if (!QET::findInDomElement(node, QStringLiteral("inputs"),
				   QStringLiteral("input")).isEmpty()) {
		kinds << tr("des champs de texte");
	}
	if (!QET::findInDomElement(node, QStringLiteral("images"),
				   QStringLiteral("image")).isEmpty()) {
		kinds << tr("des images");
	}
	if (!QET::findInDomElement(node, QStringLiteral("tables"),
				   QStringLiteral("graphics_table")).isEmpty()) {
		kinds << tr("des tableaux");
	}
	if (!QET::findInDomElement(node, QStringLiteral("terminal_strip_items"),
				   QStringLiteral("terminal_strip_item")).isEmpty()) {
		kinds << tr("des borniers");
	}

	return kinds;
}
