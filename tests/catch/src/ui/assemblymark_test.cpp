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

#include "../qt_catch_tostring.h"

#include "../../../../sources/autoNum/assemblystate.h"
#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/assemblystatecommand.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QUndoStack>

/*
	Marking a project as assembled: the photograph it takes, where the command
	lands, and what the .qet ends up holding.

	The serialisation of the state on its own is proved without a project, in
	src/assemblystate_test.cpp, and is not repeated here. What that suite
	cannot see is everything this file is about: that the photograph is taken
	off the sheets that are actually drawn, that the command goes on the stack
	the rest of the program uses, and - the part a green unit suite would
	happily lie about - that QETProject writes the state to the file at all.
	Cut either of the two lines in QETProject that do the writing and the
	reading and every case in the other file still passes.
*/

namespace {

	struct Box
	{
		int x;
		int y;
		const char *label;
	};

	/**
		Three components and one wire.

		Note what the conductors of this fixture do **not** carry: a uuid.
		That is the shape of a project drawn by an older QElectroTech, and
		the trap the plan of this task warns about - a conductor with no uuid
		in the file is given a fresh one while the project is being opened.
		The fixture is written this way on purpose so that the case which
		saves and reopens measures the trap instead of assuming it.
	*/
	QString fixtureXml()
	{
		const Box boxes[] = {
			{200, 200, "K1"},
			{320, 200, "K2"},
			{200, 380, "K3"}};

		// The docking point of a terminal, which is what the instance stores.
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QString instances;
		int index = 0;
		for (const Box &box : boxes)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/box.elmt\""
					     " uuid=\"{decaf000-0000-4000-8000-00000000000%3}\">"
					     "<terminals>"
					     "<terminal x=\"%4\" y=\"0\" orientation=\"1\" id=\"%5\"/>"
					     "<terminal x=\"%6\" y=\"0\" orientation=\"3\" id=\"%7\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%8"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(box.x)
				     .arg(box.y)
				     .arg(index)
				     .arg(east_dock)
				     .arg(index * 2)
				     .arg(west_dock)
				     .arg(index * 2 + 1)
				     .arg(QLatin1String(box.label));
			++index;
		}

		const QString conductors = QStringLiteral(
			"<conductor terminal1=\"0\" terminal2=\"3\" num=\"L1\""
			" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"box.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"30\" height=\"20\""
			       " hotspot_x=\"15\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Box</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/>"
			       "<conductors>%2</conductors>"
			       "</diagram>"
			       "</project>")
		       .arg(instances, conductors);
	}

	/// The uuid of every component drawn on @a sheet, sorted.
	QStringList componentUuids(Diagram *sheet)
	{
		QStringList uuids;
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements) {
			uuids << element->uuid().toString();
		}
		uuids.sort();
		return uuids;
	}

	/// The uuid of every wire drawn on @a sheet, sorted.
	QStringList conductorUuids(Diagram *sheet)
	{
		QStringList uuids;
		const QList<Conductor *> wires = sheet->conductors();
		for (Conductor *wire : wires) {
			uuids << wire->uuid().toString();
		}
		uuids.sort();
		return uuids;
	}

	/// Which of @a uuids the photograph does not hold, said out loud.
	QStringList missingFrom(const QMap<QString, QString> &photograph,
				const QStringList &uuids)
	{
		QStringList missing;
		for (const QString &uuid : uuids)
		{
			if (!photograph.contains(uuid)) {
				missing << uuid;
			}
		}
		return missing;
	}

	Element *box(Diagram *sheet, const QString &label)
	{
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			if (element->elementInformations()
			    .value(QStringLiteral("label")).toString() == label) {
				return element;
			}
		}
		return nullptr;
	}
}

TEST_CASE("T29 — marcar como montado tira uma fotografia, e não escreve em componente nenhum",
	  "[uibench][assembly]")
{
	UiBench::ScratchProject scratch(fixtureXml(), QStringLiteral("assembly.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 3);
	REQUIRE(sheet->conductors().count() == 1);

	SECTION("a fotografia é do que está desenhado, componente e condutor")
	{
		const AssemblyState state = AssemblyStateCommand::photograph(
			scratch.project(), AssemblyStage::Assembled);

		REQUIRE(state.stage == AssemblyStage::Assembled);
		REQUIRE(state.components.size() == 3);
		REQUIRE(state.conductors.size() == 1);
		REQUIRE(state.frozenCount() == 4);

		INFO(missingFrom(state.components, componentUuids(sheet))
		     .join(QStringLiteral(", ")).toStdString());
		REQUIRE(missingFrom(state.components, componentUuids(sheet)).isEmpty());
		INFO(missingFrom(state.conductors, conductorUuids(sheet))
		     .join(QStringLiteral(", ")).toStdString());
		REQUIRE(missingFrom(state.conductors, conductorUuids(sheet)).isEmpty());
	}

	SECTION("cada peça é guardada com a etiqueta que ela carregava")
	{
		Element *k2 = box(sheet, QStringLiteral("K2"));
		REQUIRE(k2 != nullptr);

		const AssemblyState state = AssemblyStateCommand::photograph(
			scratch.project(), AssemblyStage::Assembled);

		REQUIRE(state.componentLabel(k2->uuid().toString())
			== QStringLiteral("K2"));
		REQUIRE(state.conductorLabel(sheet->conductors().first()->uuid().toString())
			== QStringLiteral("L1"));
	}

	SECTION("nada é escrito no componente — o travamento é derivado, e não gravado")
	{
		/*
			The decision this measures is P85. Writing auto_num_locked on
			four hundred components would make the tick box of the
			information panel agree with the drawing, and would break
			unmarking in the same gesture: once written, what the machine
			froze is indistinguishable from what the drawer froze by hand.
		*/
		const QStringList before = UiBench::information(
			sheet, QStringLiteral("label"));
		const QStringList locked_before = UiBench::information(
			sheet, QStringLiteral("auto_num_locked"));

		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));

		REQUIRE(scratch->assemblyState().isFrozen());
		REQUIRE(UiBench::information(sheet, QStringLiteral("label")) == before);
		REQUIRE(UiBench::information(sheet, QStringLiteral("auto_num_locked"))
			== locked_before);
		const QStringList nothing_locked = {QString(), QString(), QString()};
		REQUIRE(locked_before == nothing_locked);
	}

	SECTION("voltar para em projeto não guarda fotografia nenhuma")
	{
		const AssemblyState state = AssemblyStateCommand::photograph(
			scratch.project(), AssemblyStage::InProject);
		REQUIRE(state.frozenCount() == 0);
		REQUIRE(state.isEmpty());
	}
}

TEST_CASE("T29 — a marcação é um comando, na pilha do projeto, e um Ctrl+Z a desfaz",
	  "[uibench][assembly]")
{
	UiBench::ScratchProject scratch(fixtureXml(), QStringLiteral("assembly.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);

	SECTION("a pilha da folha é a do projeto — há uma só, e é por projeto")
	{
		// Not a choice between two stacks: Diagram::undoStack() returns the
		// project's. Said here because the plan of this task asked for the
		// command to go on the project stack, and it would be easy to
		// believe that pushing on the sheet had put it somewhere else.
		REQUIRE(&sheet->undoStack() == scratch->undoStack());
	}

	SECTION("um comando, um passo na pilha, e o texto diz quantas peças congelou")
	{
		const int steps_before = scratch->undoStack()->index();

		auto *command = new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled));
		REQUIRE(command->changesAnything());
		REQUIRE(command->frozenCount() == 4);
		scratch->undoStack()->push(command);

		REQUIRE(scratch->undoStack()->index() == steps_before + 1);
		const QString entry = UiBench::undoTopText(scratch.project());
		INFO(entry.toStdString());
		REQUIRE(entry == QStringLiteral("Marquer le projet comme Monté : "
						"4 élément(s) figé(s)"));
	}

	SECTION("desfazer devolve o projeto ao estado de antes, e refazer o traz de volta")
	{
		REQUIRE_FALSE(scratch->assemblyState().isFrozen());

		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));
		REQUIRE(scratch->assemblyState().isFrozen());
		REQUIRE(scratch->assemblyState().frozenCount() == 4);

		scratch->undoStack()->undo();
		REQUIRE_FALSE(scratch->assemblyState().isFrozen());
		REQUIRE(scratch->assemblyState().isEmpty());

		scratch->undoStack()->redo();
		REQUIRE(scratch->assemblyState().isFrozen());
		REQUIRE(scratch->assemblyState().frozenCount() == 4);
	}

	SECTION("marcar e desmarcar são dois passos, e o segundo apaga a fotografia")
	{
		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));

		auto *release = new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::InProject));
		REQUIRE(release->changesAnything());
		scratch->undoStack()->push(release);

		REQUIRE_FALSE(scratch->assemblyState().isFrozen());
		REQUIRE(scratch->assemblyState().frozenCount() == 0);
		REQUIRE(UiBench::undoTopText(scratch.project())
			== QStringLiteral("Remettre le projet en étude"));

		// And one Ctrl+Z brings back the photograph the marking had taken,
		// entry for entry - which is the whole reason the command keeps two
		// states rather than a list of what it changed.
		scratch->undoStack()->undo();
		REQUIRE(scratch->assemblyState().isFrozen());
		REQUIRE(scratch->assemblyState().frozenCount() == 4);
	}

	SECTION("controle negativo — marcar o que já está marcado não muda nada")
	{
		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));

		AssemblyStateCommand again(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled));
		REQUIRE_FALSE(again.changesAnything());
	}
}

TEST_CASE("T29 — o estado atravessa salvar e reabrir, e um projeto não marcado não ganha nó nenhum",
	  "[uibench][assembly]")
{
	/*
		The wiring, and it is the case that has to exist: the two lines of
		QETProject that write the state into the file and read it back are
		not exercised by anything else in either suite. Delete either of them
		and every other case of this task stays green.
	*/
	SECTION("marcado, salvo e reaberto, a fotografia continua apontando para o desenho")
	{
		UiBench::ScratchProject scratch(fixtureXml(),
						QStringLiteral("assembly.qet"));
		INFO(scratch.error().toStdString());
		REQUIRE(scratch.isOpen());

		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));
		REQUIRE(scratch->assemblyState().frozenCount() == 4);

		REQUIRE(scratch.saveAndReopen());

		// Every pointer taken before the save dangles; the sheet is taken
		// again from the reopened project.
		Diagram *sheet = scratch.diagram(0);
		REQUIRE(sheet != nullptr);

		const AssemblyState state = scratch->assemblyState();
		REQUIRE(state.stage == AssemblyStage::Assembled);
		REQUIRE(state.components.size() == 3);
		REQUIRE(state.conductors.size() == 1);

		/*
			And the identities still match what is drawn. This is the
			measurement the plan of the task asked for: the fixture writes
			its conductor with no uuid at all, so the one the photograph
			holds was minted while the project was being opened. What this
			says is that the save writes that minted uuid out - so the
			photograph and the drawing are consistent from the first save
			on, which is narrower than "a project from an older version
			cannot be marked".
		*/
		INFO(missingFrom(state.components, componentUuids(sheet))
		     .join(QStringLiteral(", ")).toStdString());
		REQUIRE(missingFrom(state.components, componentUuids(sheet)).isEmpty());
		INFO(missingFrom(state.conductors, conductorUuids(sheet))
		     .join(QStringLiteral(", ")).toStdString());
		REQUIRE(missingFrom(state.conductors, conductorUuids(sheet)).isEmpty());

		Element *k3 = box(sheet, QStringLiteral("K3"));
		REQUIRE(k3 != nullptr);
		REQUIRE(state.componentLabel(k3->uuid().toString())
			== QStringLiteral("K3"));
	}

	SECTION("nunca marcado, o arquivo salvo não menciona o estado de montagem")
	{
		/*
			The rule that keeps a delivered project the file it always was.
			Also the negative control of the case above: without it, that one
			would pass on a QETProject that wrote the node unconditionally.
		*/
		UiBench::ScratchProject scratch(fixtureXml(),
						QStringLiteral("assembly.qet"));
		REQUIRE(scratch.isOpen());
		REQUIRE(scratch.saveAndReopen());

		const QString written = UiBench::fileContent(scratch.filePath());
		REQUIRE_FALSE(written.isEmpty());
		REQUIRE_FALSE(written.contains(AssemblyState::tagName()));
		REQUIRE_FALSE(scratch->assemblyState().isFrozen());
	}

	SECTION("marcado e desfeito, o arquivo salvo também não menciona")
	{
		UiBench::ScratchProject scratch(fixtureXml(),
						QStringLiteral("assembly.qet"));
		REQUIRE(scratch.isOpen());

		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));
		scratch->undoStack()->undo();

		REQUIRE(scratch.saveAndReopen());
		const QString written = UiBench::fileContent(scratch.filePath());
		REQUIRE_FALSE(written.contains(AssemblyState::tagName()));
	}

	SECTION("marcado, o arquivo salvo diz o estágio e uma linha por peça")
	{
		/*
			What a marked .qet gains, written down here because it is what
			nobody will remember six months from now: one node under the
			project root, and one child per frozen item inside it. Nothing
			is added to any <element> or <conductor>.
		*/
		UiBench::ScratchProject scratch(fixtureXml(),
						QStringLiteral("assembly.qet"));
		REQUIRE(scratch.isOpen());

		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));
		REQUIRE(scratch.saveAndReopen());

		// Read back as xml and not as text: the drawing writes <conductor>
		// nodes of its own, and counting them in the raw file would count
		// those too.
		QDomDocument document;
		REQUIRE(document.setContent(UiBench::fileContent(scratch.filePath())));

		const QDomElement node = document.documentElement()
					 .firstChildElement(AssemblyState::tagName());
		REQUIRE_FALSE(node.isNull());
		REQUIRE(node.attribute(QStringLiteral("stage"))
			== QStringLiteral("assembled"));
		REQUIRE(node.elementsByTagName(QStringLiteral("component")).count() == 3);
		REQUIRE(node.elementsByTagName(QStringLiteral("conductor")).count() == 1);

		// And the peça itself gained nothing: the photograph is one node under
		// the project root, not an attribute on four hundred components.
		const QDomNodeList drawn = document.documentElement()
					   .elementsByTagName(QStringLiteral("element"));
		for (int index = 0 ; index < drawn.count() ; ++index)
		{
			const QDomElement piece = drawn.at(index).toElement();
			REQUIRE_FALSE(piece.hasAttribute(QStringLiteral("assembly_state")));
			REQUIRE_FALSE(piece.hasAttribute(QStringLiteral("assembled")));
		}
	}
}
