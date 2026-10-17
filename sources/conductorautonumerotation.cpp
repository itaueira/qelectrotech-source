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
#include "conductorautonumerotation.h"
#include "qetproject.h"
#include "QPropertyUndoCommand/qpropertyundocommand.h"
#include "autoNum/assemblystate.h"
#include "autoNum/assignvariables.h"
#include "autoNum/numerotationcontextcommands.h"
#include "diagram.h"
#include "qet.h"
#include "qetdiagrameditor.h"
#include "ui/potentialselectordialog.h"
#include "undocommand/setautonumcontextcommand.h"

namespace
{
	/**
		@param diagram
		@return the assembly state of the project @a diagram belongs to;
		in project - which freezes nothing - when there is no project.
	*/
	AssemblyState assemblyStateOf(const Diagram *diagram)
	{
		QETProject *project = diagram ? diagram->project() : nullptr;
		if (!project) {
			return AssemblyState();
		}
		return project->assemblyState();
	}
}

/**
	@brief ConductorAutoNumerotation::ConductorAutoNumerotation
	Constructor of autonum, after create a class,
	call numerate to apply the autonum.
	When autonum is applyed,
	they do with an undo command added to the stack of diagram.
	If you give a parent_undo at constructor,
	the undo command create in this class have parent_undo for parent,
	and wasn't added to the stack of diagram
	(it's the responsabillty of the parent_undo)
	@param conductor   : the conductor to apply automatic numerotation
	@param diagram     : the diagram of conductor
	@param parent_undo : parent undo command
*/
ConductorAutoNumerotation::ConductorAutoNumerotation(
		Conductor *conductor,
		Diagram *diagram,
		QUndoCommand *parent_undo) :
	m_diagram      (diagram),
	m_conductor     (conductor),
	conductor_list (conductor -> relatedPotentialConductors().values()),
	m_parent_undo  (parent_undo)
{}

/**
	@brief ConductorAutoNumerotation::numerate
	execute the automatic numerotation
*/
void ConductorAutoNumerotation::numerate()
{
	if (!m_conductor) return;
		//A wire of a panel already built keeps its number, whoever asks
		//(T29). Every caller today hands in a conductor it has just
		//created, which is never in the photograph; this is what keeps it
		//that way for the next one.
	if (isFrozen(m_conductor.data(), assemblyStateOf(m_diagram))) return;
	if (conductor_list.size() >= 1 ) numeratePotential();
	else if (m_conductor -> properties().type == ConductorProperties::Multi)
		numerateNewConductor();
}

/**
	@brief ConductorAutoNumerotation::applyText
	apply the text t to conductor_
	and all conductors at the same potential
	@param t : Conductor text
*/
void ConductorAutoNumerotation::applyText(const QString& t)
{
	if (!m_conductor) return;
		//The text goes on every conductor of the potential, so a potential
		//holding one wire of a panel already built is not written at all:
		//writing the others would leave that wire with a number its own
		//potential no longer carries (T29).
	if (potentialIsFrozen()) return;

	QVariant old_value, new_value;
	ConductorProperties cp = m_conductor -> properties();
	old_value.setValue(cp);
	cp.text = t;
	new_value.setValue(cp);

	QUndoCommand *undo = nullptr;

	if (m_parent_undo)
	{
		new QPropertyUndoCommand(
					m_conductor,
					"properties",
					old_value,
					new_value,
					m_parent_undo);
		undo = m_parent_undo;
	}
	else
	{
		undo = new QUndoCommand();
		new QPropertyUndoCommand(
					m_conductor,
					"properties",
					old_value,
					new_value,
					undo);
		undo->setText(
			QObject::tr(
				"Edit conductor properties",
				"undo caption"));
	}

	if (!conductor_list.isEmpty())
	{
		if (!m_parent_undo)
			undo->setText(
				QObject::tr(
					"Edit the properties of several conductors",
					"undo caption"));

		foreach (Conductor *cond, conductor_list)
		{
			ConductorProperties cp2 = cond -> properties();
			old_value.setValue(cp2);
			cp2.text = t;
			new_value.setValue(cp2);
			new QPropertyUndoCommand(
						cond,
						"properties",
						old_value,
						new_value,
						undo);
		}
	}

	if (!m_parent_undo)
		m_diagram->undoStack().push(undo);
}

/**
	@brief ConductorAutoNumerotation::newProperties
	Create a new properties according to the current autonum rule of diagram
	@param diagram : Diagram class
	@param cp : ConductorProperties
	@param seq : sequentialNumbers
*/
void ConductorAutoNumerotation::newProperties(
		Diagram *diagram,
		ConductorProperties &cp,
		autonum::sequentialNumbers &seq)
{	
	NumerotationContext context = diagram->project()->conductorAutoNum(
				diagram->conductorsAutonumName());
	if (context.isEmpty()) {
		return;
	}

	QString autoNum_name = diagram->project()->conductorCurrentAutoNum();
	QString formula = autonum::numerotationContextToFormula(context);
	cp.m_formula = formula;

	autonum::setSequential(formula, seq, context, diagram, autoNum_name);

	NumerotationContextCommands ncc (context, diagram);
	NumerotationContext new_context = ncc.next();

	QETProject *project = diagram->project();
	auto *undo = new SetAutoNumContextCommand(
			[project](const QString &k, const NumerotationContext &c) {project->addConductorAutoNum(k, c);},
			autoNum_name,
			context,
			new_context);
	undo->setText(QObject::tr("Automatically number a conductor", "undo caption"));
	diagram->undoStack().push(undo);
}

/**
	@brief ConductorAutoNumerotation::numeratePotential
	Numerate a conductor on an existing potential
*/
void ConductorAutoNumerotation::numeratePotential()
{
	ConductorProperties cp = conductor_list.first()->properties();
	bool properties_equal = true;
	foreach (const Conductor *conductor, conductor_list)
	{
		if (conductor->properties() != cp)
			properties_equal = false;
	}
	// Every properties of the potential is equal,
	// so we apply it to m_conductor
	if (properties_equal)
	{
		m_conductor->setProperties(cp);
		m_conductor->rSequenceNum() = conductor_list.first()->sequenceNum();
		return;
	}

	QStringList text_list;
	QStringList formula_list;
	foreach (const Conductor *cc, conductor_list)
	{
		ConductorProperties cp = cc->properties();
		text_list << cp.text;
		formula_list << cp.m_formula;
	}


		//the texts is identicals
	if (QET::eachStrIsEqual(text_list) && QET::eachStrIsEqual(formula_list))
	{
		QList<ConductorProperties> cp_list;
		foreach(Conductor *c, conductor_list)
			cp_list<<c->properties();

		ConductorProperties cp = m_conductor->properties();
		cp.applyForEqualAttributes(cp_list);
		m_conductor->rSequenceNum() = conductor_list.first()->sequenceNum();
		m_conductor->setProperties(cp);
	}
		//the texts isn't identicals
	else
	{
			//Whichever potential the dialog keeps, it writes its number
			//over the other one. A project marked as built is answered
			//without asking, so that the answer cannot be a number
			//printed on a wire of the panel (T29).
		if (keepFrozenPotential()) {
			return;
		}

		PotentialSelectorDialog psd(
					m_conductor,
					m_parent_undo,
					m_conductor->diagramEditor());
		psd.exec();
	}
}

/**
	@brief ConductorAutoNumerotation::numerateNewConductor
	create and apply a new numerotation to m_conductor
*/
void ConductorAutoNumerotation::numerateNewConductor()
{
	if (!m_conductor) {
		return;
	}

	if (!m_diagram->conductorsAutonumName().isEmpty())
	{
		NumerotationContext context = m_diagram->project()->conductorAutoNum(
			m_diagram -> conductorsAutonumName());
		if (context.isEmpty())
			return;

		QString autoNum_name = m_diagram->project()->conductorCurrentAutoNum();
		QString formula = autonum::numerotationContextToFormula(context);

		ConductorProperties cp = m_conductor -> properties();
		cp.m_formula = formula;
		m_conductor->setProperties(cp);

		autonum::setSequential(formula,
							   m_conductor->rSequenceNum(),
							   context,
							   m_diagram,
							   autoNum_name);

		NumerotationContextCommands ncc (context, m_diagram);
		NumerotationContext new_context = ncc.next();

		QETProject *project = m_diagram->project();
		auto setter = [project](const QString &k, const NumerotationContext &c) {project->addConductorAutoNum(k, c);};

		if (m_parent_undo)
		{
			new SetAutoNumContextCommand(setter, autoNum_name, context, new_context, m_parent_undo);
		}
		else
		{
			auto *undo = new SetAutoNumContextCommand(setter, autoNum_name, context, new_context);
			undo->setText(QObject::tr("Automatically number a conductor", "undo caption"));
			m_diagram->undoStack().push(undo);
		}
	}

	applyText(autonum::AssignVariables::formulaToLabel(
		m_conductor->properties().m_formula,
		m_conductor->rSequenceNum(),
		m_diagram));
}

/**
	@brief ConductorAutoNumerotation::keepFrozenPotential
	Settle, without asking, a new conductor that joins potentials whose
	numbers disagree, when some of their conductors are frozen (T29). The
	rule itself is settleFrozenPotential(), shared with the link of two
	folio reports.

	@return true when the potential holds a frozen conductor and was settled
	here, false when nothing is frozen and the dialog has to ask as it did.
*/
bool ConductorAutoNumerotation::keepFrozenPotential()
{
		//The conductor being numbered is not in conductor_list: it is not
		//one of its own related potential conductors. numerate() never
		//gets here with it frozen, so it lands with the ones to change.
	QList<Conductor *> potential = conductor_list;
	potential.prepend(m_conductor.data());
	return settleFrozenPotential(potential, m_diagram, m_parent_undo);
}

/**
	@brief ConductorAutoNumerotation::settleFrozenPotential
	Settle, without asking, two potentials being joined into one whose
	numbers disagree, when some of their conductors are frozen (T29).

	Every conductor the photograph holds keeps its text: that is the whole
	rule. What is left to decide is what the others get.
	- The frozen conductors all carry one number: the potential is the one
	  the panel already has, so every conductor drawn after the marking
	  takes that number, exactly as it would have had that potential been
	  chosen in the dialog.
	- The frozen conductors carry two numbers or more: what joins them
	  joins two potentials that are both on the panel. Either answer would
	  print a new sleeve on a wire already in the duct, so nothing is
	  written at all, and every other conductor keeps the text it had -
	  visible on the sheet as a wire nobody numbered, which is what it is.

	@param potential : every conductor of the joined potential
	@param diagram : a sheet of the project
	@param parent_undo : the command the change goes under, or null
	@return true when the potential holds a frozen conductor and was settled
	here, false when nothing is frozen and the caller has to ask as it did.
*/
bool ConductorAutoNumerotation::settleFrozenPotential(
		const QList<Conductor *> &potential,
		Diagram *diagram,
		QUndoCommand *parent_undo)
{
	const AssemblyState state = assemblyStateOf(diagram);
	if (!state.isFrozen()) {
		return false;
	}

	QList<Conductor *> frozen;
	QList<Conductor *> to_change;
	for (Conductor *conductor : potential)
	{
		if (!conductor
		    || frozen.contains(conductor)
		    || to_change.contains(conductor)) {
			continue;
		}
		if (isFrozen(conductor, state)) {
			frozen.append(conductor);
		} else {
			to_change.append(conductor);
		}
	}
	if (frozen.isEmpty()) {
		return false;
	}

	QStringList frozen_texts;
	QList<ConductorProperties> frozen_properties;
	for (const Conductor *conductor : std::as_const(frozen))
	{
		frozen_properties.append(conductor->properties());
		frozen_texts.append(frozen_properties.last().text);
	}
	if (!QET::eachStrIsEqual(frozen_texts)) {
		return true;
	}

	QUndoCommand *undo = parent_undo
			? parent_undo
			: new QUndoCommand(QObject::tr(
						   "Edit the properties of several conductors",
						   "undo caption"));

	const autonum::sequentialNumbers sequence = frozen.first()->sequenceNum();
	for (Conductor *conductor : std::as_const(to_change))
	{
		ConductorProperties new_properties = conductor->properties();
		new_properties.applyForEqualAttributes(frozen_properties);

		if (conductor->sequenceNum() != sequence)
		{
			QVariant old_sequence, new_sequence;
			old_sequence.setValue(conductor->sequenceNum());
			new_sequence.setValue(sequence);
			new QPropertyUndoCommand(conductor,
						 "sequenceNum",
						 old_sequence,
						 new_sequence,
						 undo);
		}
		if (conductor->properties() != new_properties)
		{
			QVariant old_value, new_value;
			old_value.setValue(conductor->properties());
			new_value.setValue(new_properties);
			new QPropertyUndoCommand(conductor,
						 "properties",
						 old_value,
						 new_value,
						 undo);
		}
	}

	if (!parent_undo)
	{
			//A frozen state was read through diagram, so it is not null.
		if (undo->childCount() && diagram) {
			diagram->undoStack().push(undo);
		} else {
			delete undo;
		}
	}
	return true;
}

/**
	@brief ConductorAutoNumerotation::potentialIsFrozen
	@return true when the conductor being numbered, or any conductor of its
	potential, is a wire of a panel already built.
*/
bool ConductorAutoNumerotation::potentialIsFrozen() const
{
	const AssemblyState state = assemblyStateOf(m_diagram);
	if (!state.isFrozen()) {
		return false;
	}
	if (isFrozen(m_conductor.data(), state)) {
		return true;
	}
	for (const Conductor *conductor : conductor_list)
	{
		if (isFrozen(conductor, state)) {
			return true;
		}
	}
	return false;
}

/**
	@brief ConductorAutoNumerotation::isFrozen
	@param conductor
	@return whether the numbering has to leave the text of @a conductor alone
*/
bool ConductorAutoNumerotation::isFrozen(const Conductor *conductor)
{
	if (!conductor) {
		return false;
	}
	return isFrozen(conductor, assemblyStateOf(conductor->diagram()));
}

/**
	@brief ConductorAutoNumerotation::isFrozen
	@param conductor
	@param state
	@return whether the numbering has to leave the text of @a conductor alone
*/
bool ConductorAutoNumerotation::isFrozen(const Conductor *conductor,
					 const AssemblyState &state)
{
	if (!conductor) {
		return false;
	}
	return state.isFrozen()
	       && state.holdsConductor(conductor->uuid().toString());
}
