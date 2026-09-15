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

#include "../../../../sources/options/optiontree.h"
#include "../../../../sources/options/projectoption.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/editoptiontreecommand.h"

#include <catch2/catch.hpp>

#include <QString>
#include <QStringList>
#include <QUndoStack>

/*
	The five things a person does to an option, each one a step of the undo
	stack of the project.

	What the tree itself refuses, and what "switched on" means against
	"applies", is proved without a project in src/option_test.cpp and is not
	repeated here. What that suite cannot see is the stack: how many steps an
	operation leaves behind it, what the step is called, what the operation
	that changes nothing leaves behind - nothing - and whether walking back
	over a step gives back the tree that was there or a tree that merely
	looks like it.

	The last one is the whole point of this file. A sub-option switched on
	under a parent that is off is a choice the project is storing and the
	drawing is not showing, and it is exactly the state a command that
	replayed its edit instead of restoring its tree would flatten. Undo,
	redo, and undo of a deletion all have to give it back untouched.
*/

namespace
{
	/// The smallest project that opens: one sheet, nothing on it.
	QString projectXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"Sheet\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	/**
		@brief Push what the maker handed back, and say whether there was
		anything to push.
		@param project the project whose stack is the one that counts
		@param command what a maker returned, nullptr included
		@return true when a step was made

		Every case below goes through here rather than through the stack
		directly, so that "nothing was pushed" is measured the same way
		the panel will measure it.
	*/
	bool push(QETProject *project, EditOptionTreeCommand *command)
	{
		if (!command) {
			return false;
		}
		project->undoStack()->push(command);
		return true;
	}

	/// @return how many steps the stack of this project holds
	int steps(QETProject *project)
	{
		return project->undoStack()->index();
	}

	/// A conveyor with the option that has a sub-option under it.
	void conveyorOptions(QETProject *project,
			     QString *bidirectional,
			     QString *brake)
	{
		REQUIRE(push(project, EditOptionTreeCommand::createOption(
				     project,
				     ProjectOption(QStringLiteral("Bidirectionnel"),
						   QStringLiteral("Marche avant "
								  "et arrière")),
				     bidirectional)));

		ProjectOption with_brake(QStringLiteral("Avec frein"));
		with_brake.parent_uuid = *bidirectional;
		REQUIRE(push(project, EditOptionTreeCommand::createOption(
				     project, with_brake, brake)));
	}
}

TEST_CASE("T36 — criar, renomear, aninhar, apagar e ligar são passos do desfazer",
	  "[uibench][opcao][undo]")
{
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("options.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(steps(scratch.project()) == 0);

	SECTION("criar deixa um passo, e desfazer devolve o projeto sem opções")
	{
		QString uuid;
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::createOption(
				     scratch.project(),
				     ProjectOption(QStringLiteral("Bidirectionnel")),
				     &uuid)));

		REQUIRE(steps(scratch.project()) == 1);
		REQUIRE_FALSE(uuid.isEmpty());
		CHECK(scratch->hasOptions());
		CHECK(scratch->optionTree().count() == 1);
		CHECK(UiBench::undoTopText(scratch.project())
		      == QString::fromUtf8("Créer l'option « Bidirectionnel »"));

		scratch->undoStack()->undo();
		CHECK(scratch->optionTree().isEmpty());
		CHECK_FALSE(scratch->hasOptions());

		scratch->undoStack()->redo();
		CHECK(scratch->optionTree().count() == 1);
		CHECK(scratch->optionTree().option(uuid).name
		      == QString::fromUtf8("Bidirectionnel"));
	}

	SECTION("criar dentro de outra opção diz onde, e o desfazer desfaz uma só")
	{
		QString bidirectional;
		QString brake;
		conveyorOptions(scratch.project(), &bidirectional, &brake);

		REQUIRE(steps(scratch.project()) == 2);
		CHECK(UiBench::undoTopText(scratch.project())
		      == QString::fromUtf8("Créer l'option « Avec frein » dans "
					   "« Bidirectionnel »"));

			//Two creations are two steps and not one: the person who
			//regrets the sub-option keeps the option.
		scratch->undoStack()->undo();
		CHECK(scratch->optionTree().count() == 1);
		CHECK(scratch->optionTree().indexOfUuid(bidirectional) >= 0);
	}

	SECTION("renomear diz os dois nomes")
	{
		QString uuid;
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::createOption(
				     scratch.project(),
				     ProjectOption(QStringLiteral("Bidirectionnel")),
				     &uuid)));

		ProjectOption renamed = scratch->optionTree().option(uuid);
		renamed.name = QString::fromUtf8("Deux sens de marche");
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::editOption(scratch.project(),
							       renamed)));

		CHECK(UiBench::undoTopText(scratch.project())
		      == QString::fromUtf8("Renommer l'option « Bidirectionnel »"
					   " en « Deux sens de marche »"));

		scratch->undoStack()->undo();
		CHECK(scratch->optionTree().option(uuid).name
		      == QString::fromUtf8("Bidirectionnel"));
	}

	SECTION("aninhar e desaninhar dizem para onde")
	{
		QString bidirectional;
		QString brake;
		conveyorOptions(scratch.project(), &bidirectional, &brake);

		ProjectOption moved = scratch->optionTree().option(brake);
		moved.parent_uuid.clear();
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::editOption(scratch.project(),
							       moved)));

		CHECK(UiBench::undoTopText(scratch.project())
		      == QString::fromUtf8("Déplacer l'option « Avec frein » au "
					   "premier niveau"));
		CHECK(scratch->optionTree().depth(brake) == 0);

		scratch->undoStack()->undo();
		CHECK(scratch->optionTree().depth(brake) == 1);

		moved = scratch->optionTree().option(brake);
		moved.parent_uuid = bidirectional;
		CHECK(EditOptionTreeCommand::editOption(scratch.project(), moved)
		      == nullptr);
	}

	SECTION("apagar leva a sub-opção junto, e desfazer traz as duas de volta")
	{
		QString bidirectional;
		QString brake;
		conveyorOptions(scratch.project(), &bidirectional, &brake);

		QStringList gone;
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::removeOption(
				     scratch.project(), bidirectional, &gone)));

		CHECK(gone == QStringList({bidirectional, brake}));
		CHECK(scratch->optionTree().isEmpty());
		CHECK(UiBench::undoTopText(scratch.project())
		      == QString::fromUtf8("Supprimer l'option « Bidirectionnel »"));

		scratch->undoStack()->undo();
		CHECK(scratch->optionTree().count() == 2);
		CHECK(scratch->optionTree().depth(brake) == 1);
	}

	SECTION("marcar e desmarcar são dois passos, e o conjunto ativo volta com eles")
	{
		QString bidirectional;
		QString brake;
		conveyorOptions(scratch.project(), &bidirectional, &brake);

		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::switchOption(
				     scratch.project(), bidirectional, true)));
		CHECK(UiBench::undoTopText(scratch.project())
		      == QString::fromUtf8("Cocher l'option « Bidirectionnel »"));
		CHECK(scratch->optionTree().activeUuids()
		      == QStringList({bidirectional}));

		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::switchOption(
				     scratch.project(), bidirectional, false)));
		CHECK(UiBench::undoTopText(scratch.project())
		      == QString::fromUtf8("Décocher l'option « Bidirectionnel »"));

			//Undo of the second gives back the active set of the
			//first, which is what the panel reads to know what the
			//drawing is resolved against.
		scratch->undoStack()->undo();
		CHECK(scratch->optionTree().activeUuids()
		      == QStringList({bidirectional}));
	}
}

TEST_CASE("T36 — a operação que não muda nada não deixa passo nenhum",
	  "[uibench][opcao][undo]")
{
	/*
		A Ctrl+Z that undoes nothing is worse than no entry at all: the
		person counts the steps back and lands one short of where they
		meant to. Every way of asking for the state that is already
		there has to come back empty-handed, and the two reasons for
		coming back empty-handed have to stay apart - refused says why,
		unchanged says nothing.
	*/
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("options.qet"));
	REQUIRE(scratch.isOpen());

	QString bidirectional;
	QString brake;
	conveyorOptions(scratch.project(), &bidirectional, &brake);
	const int before = steps(scratch.project());
	REQUIRE(before == 2);

	SECTION("ligar o que já está ligado não empilha, e não é erro")
	{
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::switchOption(
				     scratch.project(), bidirectional, true)));

		QString error = QStringLiteral("não tocada");
		CHECK(EditOptionTreeCommand::switchOption(
			      scratch.project(), bidirectional, true, &error)
		      == nullptr);
		CHECK(error.isEmpty());
		CHECK(steps(scratch.project()) == before + 1);

			//And desligar o que já está desligado, which is the same
			//question from the other side.
		CHECK(EditOptionTreeCommand::switchOption(
			      scratch.project(), brake, false, &error)
		      == nullptr);
		CHECK(error.isEmpty());
		CHECK(steps(scratch.project()) == before + 1);
	}

	SECTION("devolver a opção como ela está não empilha, e não é erro")
	{
		QString error = QStringLiteral("não tocada");
		CHECK(EditOptionTreeCommand::editOption(
			      scratch.project(),
			      scratch->optionTree().option(bidirectional),
			      &error)
		      == nullptr);
		CHECK(error.isEmpty());
		CHECK(steps(scratch.project()) == before);
	}

	SECTION("um nome que só ganhou espaços em volta é o mesmo nome")
	{
		/*
			The trap this guard exists for: the panel hands back the
			text of a field a person clicked into and left, and a
			comparison made before the tree folds the spaces would
			call it a change.
		*/
		ProjectOption padded = scratch->optionTree().option(bidirectional);
		padded.name = QStringLiteral("  Bidirectionnel  ");

		QString error = QStringLiteral("não tocada");
		CHECK(EditOptionTreeCommand::editOption(scratch.project(),
							padded, &error)
		      == nullptr);
		CHECK(error.isEmpty());
		CHECK(steps(scratch.project()) == before);
	}

	SECTION("o comando construído à mão sabe dizer que não muda nada")
	{
		/*
			The makers are the usual door and none of them ever builds
			one of these. The constructor is public, though, and the
			panel of a later step may want to hand over a tree it
			composed itself - so the question has to be answerable
			before the push and not only after it.
		*/
		EditOptionTreeCommand nothing(scratch.project(),
					      scratch->optionTree(),
					      QString());
		CHECK(nothing.isNull());
		CHECK(nothing.text()
		      == QString::fromUtf8("Modifier les options du projet"));

		OptionTree tree = scratch->optionTree();
		REQUIRE(tree.setSwitchedOn(bidirectional, true));
		EditOptionTreeCommand something(scratch.project(), tree,
						QString());
		CHECK_FALSE(something.isNull());

			//Neither of them was pushed, and neither of them touched
			//the project: building a command is not doing it.
		CHECK(steps(scratch.project()) == before);
		CHECK(scratch->optionTree().activeUuids().isEmpty());
	}

	SECTION("a operação recusada não empilha, e diz por quê")
	{
		QString error;
		CHECK(EditOptionTreeCommand::createOption(
			      scratch.project(),
			      ProjectOption(QStringLiteral("bidirectionnel")),
			      nullptr, &error)
		      == nullptr);
		CHECK_FALSE(error.isEmpty());

		error.clear();
		ProjectOption loop = scratch->optionTree().option(bidirectional);
		loop.parent_uuid = brake;
		CHECK(EditOptionTreeCommand::editOption(scratch.project(), loop,
							&error)
		      == nullptr);
		CHECK_FALSE(error.isEmpty());

		error.clear();
		CHECK(EditOptionTreeCommand::removeOption(
			      scratch.project(), QStringLiteral("jamais-vu"),
			      nullptr, &error)
		      == nullptr);
		CHECK_FALSE(error.isEmpty());

		error.clear();
		CHECK(EditOptionTreeCommand::switchOption(
			      scratch.project(), QStringLiteral("jamais-vu"),
			      true, &error)
		      == nullptr);
		CHECK_FALSE(error.isEmpty());

		CHECK(steps(scratch.project()) == before);
		CHECK(scratch->optionTree().count() == 2);
	}
}

TEST_CASE("T36 — desfazer e refazer não apagam a escolha guardada sob uma opção desligada",
	  "[uibench][opcao][undo]")
{
	/*
		The state the whole feature is built to keep: the person chose
		the brake, then switched the bidirectional option off. The brake
		is no longer drawn and the choice is still there. Walking the
		stack back and forth over that must give back the choice, and not
		the consequence.
	*/
	UiBench::ScratchProject scratch(projectXml(),
					QStringLiteral("options.qet"));
	REQUIRE(scratch.isOpen());

	QString bidirectional;
	QString brake;
	conveyorOptions(scratch.project(), &bidirectional, &brake);

	REQUIRE(push(scratch.project(),
		     EditOptionTreeCommand::switchOption(scratch.project(),
							 bidirectional, true)));
	REQUIRE(push(scratch.project(),
		     EditOptionTreeCommand::switchOption(scratch.project(),
							 brake, true)));
	REQUIRE(scratch->optionTree().activeUuids()
		== QStringList({bidirectional, brake}));

	SECTION("desligar a mãe, desfazer e refazer deixam a escolha onde estava")
	{
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::switchOption(
				     scratch.project(), bidirectional, false)));

		OptionTree tree = scratch->optionTree();
		CHECK(tree.isSwitchedOn(brake));
		CHECK_FALSE(tree.isActive(brake));
		CHECK(tree.activeUuids().isEmpty());

		scratch->undoStack()->undo();
		tree = scratch->optionTree();
		CHECK(tree.isSwitchedOn(brake));
		CHECK(tree.isActive(brake));

		scratch->undoStack()->redo();
		tree = scratch->optionTree();
		CHECK(tree.isSwitchedOn(brake));
		CHECK_FALSE(tree.isActive(brake));

			//And the choice is still a choice: turning the parent
			//back on brings the brake back with nothing regenerated,
			//which is the third line of the task.
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::switchOption(
				     scratch.project(), bidirectional, true)));
		CHECK(scratch->optionTree().activeUuids()
		      == QStringList({bidirectional, brake}));
	}

	SECTION("desfazer uma exclusão traz o galho de volta com as escolhas dele")
	{
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::switchOption(
				     scratch.project(), bidirectional, false)));
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::removeOption(
				     scratch.project(), bidirectional)));
		REQUIRE(scratch->optionTree().isEmpty());

		scratch->undoStack()->undo();

		const OptionTree tree = scratch->optionTree();
		REQUIRE(tree.count() == 2);
		CHECK(tree.isSwitchedOn(brake));
		CHECK_FALSE(tree.isSwitchedOn(bidirectional));
		CHECK_FALSE(tree.isActive(brake));
		CHECK(tree.displayPath(brake)
		      == QString::fromUtf8("Bidirectionnel / Avec frein"));
	}

	SECTION("a escolha guardada atravessa o desfazer e o arquivo")
	{
		/*
			Undo writes the tree on the project; saving writes the
			project on the file. Neither half is worth much without
			the other, and this is the only case that crosses both.
		*/
		REQUIRE(push(scratch.project(),
			     EditOptionTreeCommand::switchOption(
				     scratch.project(), brake, false)));
		scratch->undoStack()->undo();

		REQUIRE(scratch.saveAndReopen());
		const OptionTree reread = scratch->optionTree();
		CHECK(reread.isSwitchedOn(brake));
		CHECK(reread.activeUuids()
		      == QStringList({bidirectional, brake}));
	}
}
