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
#include "../../../sources/autoNum/assemblystate.h"

#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QDomDocument>

/*
	The assembly state on its own: what it writes into a .qet, what it reads
	back, and what it does with a file that says nothing.

	No project here, and that is the point of the file being in this suite:
	the photograph is a set of strings, and the day it stops being one -
	the day it needs an Element to answer a question - the split will say
	so by refusing to link.

	What is not here: taking the photograph, which needs a project open, and
	pushing it as a command, which needs an undo stack. Both are in
	src/ui/assemblymark_test.cpp.
*/

namespace {

	/// A state with something in it, so that the round trip has work to do.
	AssemblyState markedState()
	{
		AssemblyState state;
		state.stage = AssemblyStage::Assembled;
		state.components.insert(QStringLiteral("{decaf000-0000-4000-8000-000000000001}"),
					QStringLiteral("K1"));
		state.components.insert(QStringLiteral("{decaf000-0000-4000-8000-000000000002}"),
					QStringLiteral("K2"));
		state.conductors.insert(QStringLiteral("{beef0000-0000-4000-8000-000000000001}"),
					QStringLiteral("L1"));
		return state;
	}

	/// The node the state writes, as text, so two of them can be compared.
	QString serialised(const AssemblyState &state)
	{
		QDomDocument document;
		QDomElement node = state.toXml(document);
		document.appendChild(node);
		return document.toString(0);
	}

	/// The state a piece of xml describes, read the way QETProject reads it.
	AssemblyState parsed(const QString &xml)
	{
		QDomDocument document;
		document.setContent(xml);
		AssemblyState state;
		state.fromXml(document.documentElement());
		return state;
	}
}

TEST_CASE("T29 — o estado de montagem sobrevive ao ida e volta pelo XML",
	  "[assembly][autonum]")
{
	SECTION("o que foi gravado é o que se lê de volta")
	{
		const AssemblyState before = markedState();
		AssemblyState after;
		QDomDocument document;
		QDomElement node = before.toXml(document);
		document.appendChild(node);

		REQUIRE(after.fromXml(document.documentElement()));
		REQUIRE(after.stage == AssemblyStage::Assembled);
		REQUIRE(after.components.size() == 2);
		REQUIRE(after.conductors.size() == 1);
		REQUIRE(after.componentLabel(
				QStringLiteral("{decaf000-0000-4000-8000-000000000001}"))
			== QStringLiteral("K1"));
		REQUIRE(after.conductorLabel(
				QStringLiteral("{beef0000-0000-4000-8000-000000000001}"))
			== QStringLiteral("L1"));
		REQUIRE(after == before);
	}

	SECTION("os três estágios se escrevem e se leem, cada um o seu")
	{
		for (const AssemblyStage stage : {AssemblyStage::InProject,
						  AssemblyStage::Assembled,
						  AssemblyStage::InField})
		{
			AssemblyState before = markedState();
			before.stage = stage;
			QDomDocument document;
			document.appendChild(before.toXml(document));

			AssemblyState after;
			REQUIRE(after.fromXml(document.documentElement()));
			REQUIRE(after.stage == stage);
		}
	}

	SECTION("quem marcou e quando, quando alguém preencher, atravessa igual")
	{
		// T25 is what will fill these; the file format does not have to
		// change on the day it does, and this is what says so.
		AssemblyState before = markedState();
		before.marked_by = QStringLiteral("someone");
		before.marked_at = QStringLiteral("2026-09-06T10:00:00");

		QDomDocument document;
		document.appendChild(before.toXml(document));
		AssemblyState after;
		REQUIRE(after.fromXml(document.documentElement()));
		REQUIRE(after.marked_by == before.marked_by);
		REQUIRE(after.marked_at == before.marked_at);
	}

	SECTION("vazios, os dois campos não deixam atributo nenhum no arquivo")
	{
		const QString written = serialised(markedState());
		REQUIRE_FALSE(written.contains(QStringLiteral("marked_by")));
		REQUIRE_FALSE(written.contains(QStringLiteral("marked_at")));
	}
}

TEST_CASE("T29 — ler é tolerante: projeto que nada diz abre em projeto",
	  "[assembly][autonum]")
{
	SECTION("nó ausente — o que todo projeto salvo antes disto existir tem")
	{
		AssemblyState state;
		const QDomElement absent;    // what firstChildElement() gives back

		REQUIRE_FALSE(state.fromXml(absent));
		REQUIRE(state.stage == AssemblyStage::InProject);
		REQUIRE(state.isEmpty());
		REQUIRE_FALSE(state.isFrozen());
	}

	SECTION("nó de outra coisa não é lido como se fosse deste")
	{
		AssemblyState state;
		QDomDocument document;
		document.setContent(QStringLiteral("<iec_structure enabled=\"true\"/>"));

		REQUIRE_FALSE(state.fromXml(document.documentElement()));
		REQUIRE(state.stage == AssemblyStage::InProject);
		REQUIRE(state.isEmpty());
	}

	SECTION("estágio que esta versão não conhece lê como em projeto")
	{
		// A file written by a later version that invents a fourth stage has
		// to open, and has to open without silently refusing to renumber
		// what the drawer asked for.
		const AssemblyState state = parsed(
			QStringLiteral("<assembly_state stage=\"decommissioned\"/>"));
		REQUIRE(state.stage == AssemblyStage::InProject);
		REQUIRE_FALSE(state.isFrozen());
	}

	SECTION("estágio ausente lê como em projeto, e não como montado")
	{
		const AssemblyState state = parsed(QStringLiteral("<assembly_state/>"));
		REQUIRE(state.stage == AssemblyStage::InProject);
	}

	SECTION("entrada sem uuid é descartada, e não guardada sob chave vazia")
	{
		/*
			A null identity would match every component that has none, and
			the freezing would spread to whatever the next reading finds.
			Better to lose the entry than to freeze the wrong thing.
		*/
		const AssemblyState state = parsed(QStringLiteral(
			"<assembly_state stage=\"assembled\">"
			"<component uuid=\"{decaf000-0000-4000-8000-000000000001}\" label=\"K1\"/>"
			"<component label=\"K9\"/>"
			"<component uuid=\"\" label=\"K8\"/>"
			"</assembly_state>"));

		REQUIRE(state.components.size() == 1);
		REQUIRE_FALSE(state.holdsComponent(QString()));
	}

	SECTION("entrada sem etiqueta é guardada — ela existia, só não tinha nome")
	{
		const AssemblyState state = parsed(QStringLiteral(
			"<assembly_state stage=\"assembled\">"
			"<component uuid=\"{decaf000-0000-4000-8000-000000000001}\"/>"
			"</assembly_state>"));

		REQUIRE(state.components.size() == 1);
		REQUIRE(state.holdsComponent(
				QStringLiteral("{decaf000-0000-4000-8000-000000000001}")));
		REQUIRE(state.componentLabel(
				QStringLiteral("{decaf000-0000-4000-8000-000000000001}")).isEmpty());
	}
}

TEST_CASE("T29 — o mesmo projeto salvo duas vezes escreve os mesmos bytes",
	  "[assembly][autonum]")
{
	/*
		The reason this is a case of its own: a QHash iterates in an order
		that is not stable, so a photograph kept in one would write a
		different file every time the project was saved. Nobody would see it
		as a defect - the project would open fine - but the backup
		comparison, the diff of whoever versions their projects and the
		question "did this save change anything?" would all stop working.
	*/
	AssemblyState first;
	first.stage = AssemblyStage::Assembled;
	AssemblyState second;
	second.stage = AssemblyStage::Assembled;

	const QStringList uuids = {
		QStringLiteral("{decaf000-0000-4000-8000-00000000000a}"),
		QStringLiteral("{decaf000-0000-4000-8000-000000000003}"),
		QStringLiteral("{decaf000-0000-4000-8000-00000000000f}"),
		QStringLiteral("{decaf000-0000-4000-8000-000000000001}"),
		QStringLiteral("{decaf000-0000-4000-8000-000000000007}")};

	for (int index = 0 ; index < uuids.count() ; ++index) {
		first.components.insert(uuids.at(index), QStringLiteral("K%1").arg(index));
	}
	// The same set, inserted the other way round: two runs of the program
	// meet the components of a sheet in whatever order the scene hands them
	// over, and the file must not depend on that.
	for (int index = uuids.count() - 1 ; index >= 0 ; --index) {
		second.components.insert(uuids.at(index), QStringLiteral("K%1").arg(index));
	}

	REQUIRE(first == second);
	REQUIRE(serialised(first) == serialised(second));
	REQUIRE(serialised(first) == serialised(first));

	SECTION("controle negativo — um componente a mais e os bytes mudam")
	{
		// Without this, the check above would pass on a toXml() that wrote
		// nothing at all.
		second.components.insert(QStringLiteral("{decaf000-0000-4000-8000-000000000099}"),
					 QStringLiteral("K99"));
		REQUIRE_FALSE(serialised(first) == serialised(second));
	}
}

TEST_CASE("T29 — congelado é o estágio, e vazio é o que não precisa ser gravado",
	  "[assembly][autonum]")
{
	SECTION("recém-construído não diz nada e não congela nada")
	{
		const AssemblyState state;
		REQUIRE(state.isEmpty());
		REQUIRE_FALSE(state.isFrozen());
		REQUIRE(state.frozenCount() == 0);
	}

	SECTION("marcado sem nada desenhado ainda é marcado")
	{
		// An empty project marked as assembled is assembled, and the next
		// component drawn on it is a component drawn after the panel was
		// wired. What decides is the stage, not whether the photograph
		// holds anything.
		AssemblyState state;
		state.stage = AssemblyStage::Assembled;
		REQUIRE(state.isFrozen());
		REQUIRE(state.frozenCount() == 0);
		REQUIRE_FALSE(state.isEmpty());
	}

	SECTION("a conta do que foi congelado soma componentes e condutores")
	{
		const AssemblyState state = markedState();
		REQUIRE(state.frozenCount() == 3);
		REQUIRE(state.components.size() == 2);
		REQUIRE(state.conductors.size() == 1);
	}

	SECTION("componente e condutor são dois conjuntos, e não se confundem")
	{
		const AssemblyState state = markedState();
		const QString conductor_uuid =
			QStringLiteral("{beef0000-0000-4000-8000-000000000001}");
		REQUIRE(state.holdsConductor(conductor_uuid));
		REQUIRE_FALSE(state.holdsComponent(conductor_uuid));
	}
}
