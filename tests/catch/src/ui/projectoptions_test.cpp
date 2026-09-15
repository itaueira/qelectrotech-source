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

#include <catch2/catch.hpp>

#include <QString>
#include <QStringList>

/*
	The configuration model of a project, from the project to the file and
	back.

	The arithmetic of the tree - what it refuses, what "switched on" means
	against "applies", what a damaged file becomes when it is read - is
	proved without a project in src/option_test.cpp, and none of it is
	repeated here. What that suite cannot see is the two lines of QETProject
	that write the tree into the .qet and read it back: delete either of them
	and every case of the other file stays green, while the family of panels
	a person spent a week configuring goes back to being one panel as soon as
	the project is closed.

	That is the whole subject of this file, and the reason it is here and not
	there.
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
		The same project with a configuration model already inside it,
		written by hand as a file coming from another machine holds it.

		By hand on purpose: a round trip through our own writer would
		still pass if both ends of it agreed on something wrong, and this
		is the shape that has to be readable - one option, one sub-option
		of it, one of the two switched on.
	*/
	QString projectWithOptionsXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"Sheet\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "<option_tree>"
			       "<option uuid=\"opt-1\" name=\"Bidirectionnel\""
			       " description=\"Marche avant et arrière\""
			       " on=\"true\"/>"
			       "<option uuid=\"opt-2\" parent=\"opt-1\""
			       " name=\"Avec frein\"/>"
			       "</option_tree>"
			       "</project>");
	}

	/// A configuration model as the panel will hand it over.
	OptionTree conveyorOptions(QString *bidirectional, QString *brake)
	{
		OptionTree tree;
		*bidirectional = tree.append(
				ProjectOption(QStringLiteral("Bidirectionnel"),
					      QStringLiteral("Marche avant et "
							     "marche arrière")));
		ProjectOption with_brake(QStringLiteral("Avec frein"));
		with_brake.parent_uuid = *bidirectional;
		*brake = tree.append(with_brake);
		return tree;
	}
}

TEST_CASE("T36 — o modelo de configuração atravessa salvar e reabrir",
	  "[uibench][opcao]")
{
	SECTION("a árvore e o que está ligado voltam do arquivo")
	{
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("options.qet"));
		INFO(scratch.error().toStdString());
		REQUIRE(scratch.isOpen());
		REQUIRE(scratch->optionTree().isEmpty());
		REQUIRE_FALSE(scratch->hasOptions());

		QString bidirectional;
		QString brake;
		OptionTree tree = conveyorOptions(&bidirectional, &brake);
		REQUIRE(tree.setSwitchedOn(bidirectional, true));

		scratch->setOptionTree(tree);
		REQUIRE(scratch->hasOptions());

		REQUIRE(scratch.saveAndReopen());

		const OptionTree reread = scratch->optionTree();
		REQUIRE(reread.count() == 2);
		CHECK(reread == tree);

			//The half of CU-36.8 this step closes: reopening the
			//project says exactly what is active, by the same uuids
			//it was saved with.
		CHECK(reread.activeUuids() == QStringList({bidirectional}));
		CHECK(reread.activeNames()
		      == QStringList({QString::fromUtf8("Bidirectionnel")}));
		CHECK(reread.isSwitchedOn(bidirectional));
		CHECK_FALSE(reread.isActive(brake));
		CHECK(reread.displayPath(brake)
		      == QString::fromUtf8("Bidirectionnel / Avec frein"));

		const QString written = UiBench::fileContent(scratch.filePath());
		REQUIRE_FALSE(written.isEmpty());
		CHECK(written.contains(OptionTree::tagName()));
	}

	SECTION("uma sub-opção ligada sob um pai desligado volta ligada, e sem valer")
	{
		/*
			The state that is lost the moment the two questions are
			folded into one, and the one a file round trip is most
			likely to flatten: the person chose the brake, then
			switched the whole bidirectional option off. Reopening has
			to give back the choice, not the consequence.
		*/
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("options.qet"));
		REQUIRE(scratch.isOpen());

		QString bidirectional;
		QString brake;
		OptionTree tree = conveyorOptions(&bidirectional, &brake);
		REQUIRE(tree.setSwitchedOn(brake, true));
		scratch->setOptionTree(tree);
		REQUIRE(scratch.saveAndReopen());

		OptionTree reread = scratch->optionTree();
		CHECK(reread.isSwitchedOn(brake));
		CHECK_FALSE(reread.isActive(brake));
		CHECK(reread.activeUuids().isEmpty());

		REQUIRE(reread.setSwitchedOn(bidirectional, true));
		CHECK(reread.activeUuids()
		      == QStringList({bidirectional, brake}));
	}

	SECTION("um projeto que nunca criou opção sai do arquivo como sempre saiu")
	{
		/*
			The rule that keeps a delivered project the file it always
			was, and the negative control of the case above: without
			it, that one would pass on a QETProject that wrote the node
			whether or not there was anything in it.
		*/
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("options.qet"));
		REQUIRE(scratch.isOpen());
		REQUIRE(scratch.saveAndReopen());

		const QString written = UiBench::fileContent(scratch.filePath());
		REQUIRE_FALSE(written.isEmpty());
		CHECK_FALSE(written.contains(OptionTree::tagName()));
		CHECK(scratch->optionTree().isEmpty());
		CHECK_FALSE(scratch->hasOptions());
	}
}

TEST_CASE("T36 — um arquivo escrito com opções é lido como o projeto que ele descreve",
	  "[uibench][opcao]")
{
	UiBench::ScratchProject scratch(projectWithOptionsXml(),
					QStringLiteral("family.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const OptionTree tree = scratch->optionTree();
	REQUIRE(scratch->hasOptions());
	REQUIRE(tree.count() == 2);

	CHECK(tree.option(QStringLiteral("opt-1")).name
	      == QString::fromUtf8("Bidirectionnel"));
	CHECK(tree.option(QStringLiteral("opt-1")).description
	      == QString::fromUtf8("Marche avant et arrière"));
	CHECK(tree.depth(QStringLiteral("opt-2")) == 1);
	CHECK(tree.activeUuids() == QStringList({QStringLiteral("opt-1")}));

		//Reading a configuration model is reading, and nothing else: a
		//project that arrives with something to undo is a project that
		//was modified while being read.
	CHECK(UiBench::undoTopText(scratch.project()).isEmpty());
}

TEST_CASE("T36 — um projeto de verdade, sem opção nenhuma, não muda de comportamento",
	  "[uibench][opcao]")
{
	/*
		Every project ever delivered is this one: no option element at
		all. The read has to turn that into "no option" and never into a
		refusal, and it must not leave anything behind it - which is what
		the empty undo stack says.
	*/
	UiBench::Project project(QStringLiteral("industrial.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	REQUIRE(project.diagramCount() > 0);

	CHECK(project->optionTree().isEmpty());
	CHECK_FALSE(project->hasOptions());
	CHECK(project->optionTree().activeUuids().isEmpty());
	CHECK(UiBench::undoTopText(project.project()).isEmpty());
}
