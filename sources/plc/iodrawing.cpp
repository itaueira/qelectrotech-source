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
#include "iodrawing.h"

#include "../ElementsCollection/elementslocation.h"
#include "../TerminalStrip/UndoCommand/addterminalstripcommand.h"
#include "../TerminalStrip/UndoCommand/addterminaltostripcommand.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../elementprovider.h"
#include "../macro/circuitgenerator.h"
#include "../macro/circuittable.h"
#include "../macro/macrofile.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminalelement.h"
#include "../qetproject.h"
#include "../undocommand/linkelementcommand.h"

#include <QFileInfo>
#include <QSharedPointer>
#include <QUndoStack>
#include <QUuid>

/**
	@brief IoDrawing::Report::text
	@return what was drawn, said out loud
*/
QString IoDrawing::Report::text() const
{
	QStringList lines;

	if (drawn > 0)
	{
		lines << tr("%1 circuit(s) généré(s) sur %2 folio(s).")
			 .arg(drawn)
			 .arg(sheets);

		if (linked > 0)
		{
			lines << tr("%1 symbole(s) lié(s) à une voie de la carte.")
				 .arg(linked);
		}
		if (terminals > 0)
		{
			lines << tr("%1 borne(s) de champ ajoutée(s) au groupe "
				    "de bornes.")
				 .arg(terminals);
		}
	}
	else
	{
		lines << tr("Aucun circuit n'a été généré.");
	}

	lines << problems;

	return lines.join(QLatin1Char('\n'));
}

/**
	@brief IoDrawing::IoDrawing
	@param project
*/
IoDrawing::IoDrawing(QETProject *project) :
	m_project(project)
{}

/**
	@brief IoDrawing::stripNameFor
	The strip is named after the card, because that is how somebody looking
	for it in the strip editor will look for it: a project with four cards
	gets four strips, and a strip called "bornes" would be four strips
	called "bornes".
	@param master the card, may be null
	@return the name of its strip of field terminals
*/
QString IoDrawing::stripNameFor(Element *master)
{
	QString label;
	if (master)
	{
		label = master->actualLabel();
		if (label.isEmpty()) {
			label = master->name();
		}
	}

	return label.isEmpty() ? tr("E/S")
			       : tr("E/S %1").arg(label);
}

/**
	@brief IoDrawing::loadMacros
	Every macro the plan names is opened once, and what it declares is
	given to the table: without that, setValue would refuse every cell,
	because a table only accepts a column one of its macros asked for.
	@param plan
	@param table
	@param problems collects the macros that would not open
	@return true when at least one macro opened
*/
bool IoDrawing::loadMacros(const IoCircuit::Plan &plan, CircuitTable &table,
			   QStringList *problems) const
{
	bool one = false;

	const QStringList paths = plan.macroPaths();
	for (const QString &path : paths)
	{
			//A path of the collection and a path of the disk are both
			//accepted, and resolved the way the generator resolves them.
		MacroFile file;
		const ElementsLocation location(path);
		const QString file_path = location.fileSystemPath();
		if (file_path.isEmpty()) {
			file.load(path);
		} else {
			file.load(file_path);
		}

		if (file.isNull())
		{
			if (problems)
			{
				*problems << tr("%1 : %2")
					     .arg(QFileInfo(path).completeBaseName())
					     .arg(file.errorText());
			}
			continue;
		}

		table.setParameters(path, file.parameters());
		one = true;
	}

	return one;
}

/**
	@brief IoDrawing::elementsOf
	@param uuids what one generated circuit issued, elements and conductors
	together
	@return the elements among them
*/
QList<Element *> IoDrawing::elementsOf(const QStringList &uuids) const
{
	QList<QUuid> wanted;
	wanted.reserve(uuids.count());
	for (const QString &uuid : uuids)
	{
		const QUuid one(uuid);
		if (!one.isNull()) {
			wanted << one;
		}
	}

	if (wanted.isEmpty()) {
		return QList<Element *>();
	}

	ElementProvider provider(m_project);
	return provider.fromUuids(wanted);
}

/**
	@brief IoDrawing::link
	Tie to the channel what the macro drew for it. The filter is upstream's
	own: isLinkable refuses a slave that is not a PLC slave, so the
	auxiliary contact a "motor starter" macro carries - a slave of the
	contactor, not of the card - is left alone without this having to know
	anything about motor starters.
	@param master the card
	@param elements what the circuit issued
	@param io_index the row of the card
	@return how many were tied
*/
int IoDrawing::link(Element *master, const QList<Element *> &elements,
		    int io_index)
{
	if (!master || io_index < 0) {
		return 0;
	}

	int count = 0;
	for (Element *element : elements)
	{
		if (!element) {
			continue;
		}
		if (!LinkElementCommand::isLinkable(element, master)) {
			continue;
		}

		LinkElementCommand *command = new LinkElementCommand(element);
		command->setLink(master);
		command->setGroupIndex(io_index);
		m_project->undoStack()->push(command);
		++count;
	}

	return count;
}

/**
	@brief IoDrawing::terminalsOf
	@param elements what the circuit issued
	@return the terminals among them that are not already in a strip
*/
QList<Element *> IoDrawing::terminalsOf(const QList<Element *> &elements)
{
	QList<Element *> terminals;

	for (Element *element : elements)
	{
		if (!element) {
			continue;
		}
		if (element->elementData().m_type != ElementData::Terminal) {
			continue;
		}

		TerminalElement *terminal = static_cast<TerminalElement *>(element);
		const QSharedPointer<RealTerminal> real = terminal->realTerminal();
		if (real.isNull() || real->parentStrip()) {
			continue;
		}

		terminals << element;
	}

	return terminals;
}

/**
	@brief IoDrawing::bind
	@param terminals what terminalsOf() found
	@param strip
	@return how many went in
*/
int IoDrawing::bind(const QList<Element *> &terminals, TerminalStrip *strip)
{
	if (!strip) {
		return 0;
	}

	int count = 0;
	for (Element *element : terminals)
	{
		TerminalElement *terminal = static_cast<TerminalElement *>(element);
		const QSharedPointer<RealTerminal> real = terminal->realTerminal();
		if (real.isNull()) {
			continue;
		}

		m_project->undoStack()->push(
			new AddTerminalToStripCommand(real, strip));
		++count;
	}

	return count;
}

/**
	@brief IoDrawing::stripFor
	@param master the card
	@return the strip the field terminals of @a master go into, made and
	stacked if the project has none by that name
*/
TerminalStrip *IoDrawing::stripFor(Element *master)
{
	if (!m_project) {
		return nullptr;
	}

	const QString wanted = stripNameFor(master);

	const QVector<TerminalStrip *> strips = m_project->terminalStrip();
	for (TerminalStrip *strip : strips)
	{
		if (strip && strip->name() == wanted) {
			return strip;
		}
	}

		//newTerminalStrip already puts it in the project; the command is
		//what makes the undo take it back out again.
	TerminalStrip *strip = m_project->newTerminalStrip(QString(), QString(),
							   wanted);
	m_project->undoStack()->push(new AddTerminalStripCommand(strip, m_project));

	return strip;
}

/**
	@brief IoDrawing::draw
	@param plan
	@param master
	@param sheet_title
	@return what was drawn
*/
IoDrawing::Report IoDrawing::draw(const IoCircuit::Plan &plan, Element *master,
				  const QString &sheet_title)
{
	Report report;

	if (!m_project)
	{
		report.problems << tr("Aucun projet ouvert.");
		return report;
	}
	if (plan.jobs.isEmpty()) {
		return report;
	}

	CircuitTable table;
	if (!loadMacros(plan, table, &report.problems)) {
		return report;
	}

		//fill() writes into the jobs the id of the row it made for each of
		//them, which is the only way back from the report of the generator
		//to the point that asked for the circuit.
	IoCircuit::Plan drawn = plan;
	QStringList filling;
	IoCircuit::fill(table, drawn, &filling);
	report.problems << filling;

	if (table.rowCount() == 0) {
		return report;
	}

	QUndoStack *stack = m_project->undoStack();
	stack->beginMacro(tr("dessiner %1 circuit(s) d'E/S")
			  .arg(int(drawn.jobs.count())));

	CircuitGenerator generator(m_project);
	CircuitGenerator::Options options;
	options.sheet_title = sheet_title;

	const CircuitGenerator::Report generated = generator.generate(table, options);
	report.drawn = generated.generated;
	report.sheets = generated.sheets;
	report.problems << generated.problems;

	TerminalStrip *strip = nullptr;

	for (const IoCircuit::Job &job : drawn.jobs)
	{
		const QStringList uuids = generated.issued.value(job.row_id);
		if (uuids.isEmpty()) {
				//The row was skipped, and the generator has already said
				//why: saying it twice in the same paragraph helps nobody.
			continue;
		}

		const QList<Element *> elements = elementsOf(uuids);
		report.linked += link(master, elements, job.io_index);

		if (job.needs_terminal)
		{
			const QList<Element *> terminals = terminalsOf(elements);
			if (terminals.isEmpty())
			{
				report.problems << tr("%1 : le circuit ne contient "
						      "aucune borne à mettre dans "
						      "le groupe de bornes.")
						   .arg(job.label);
				continue;
			}

			if (!strip) {
				strip = stripFor(master);
			}
			report.terminals += bind(terminals, strip);
		}
	}

	stack->endMacro();

	return report;
}
