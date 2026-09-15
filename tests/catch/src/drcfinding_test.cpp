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
#include "../../../sources/drc/drcfinding.h"

#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QList>
#include <QString>

/*
	What a finding of the checker is, before anything has been found.

	It lives here, in the pure suite, and that is the measurement rather
	than the convenience: the finding must be buildable and readable
	without a project, without a folio and without a component, because
	the case that matters most is precisely the one that has none of the
	three - a rule that speaks about the project as a whole.

	Four things are settled, and each of them is a way the panel could
	send somebody to the wrong place:

	1. a finding with nothing to point at says so, instead of pointing at
	   the first sheet;
	2. a kind that promises an object and a null pointer is not a target
	   either - so that a caller who asks once does not have to ask twice;
	3. a folio is 1-based and a zero is "nobody said", never "sheet zero";
	4. counting by severity goes through the one comparison that knows the
	   order of the severities.

	The pointers below are never dereferenced: what is being proved is
	which of them the finding tests, and a real Element would drag the
	whole program into a suite whose contract is that it does not link it.
*/

namespace
{
	/// A pointer that is only ever compared against nullptr.
	Element *fakeElement()
	{
		static int somewhere = 0;
		return reinterpret_cast<Element *>(&somewhere);
	}

	/// The same, for the other kind of target.
	Conductor *fakeConductor()
	{
		static int somewhere = 0;
		return reinterpret_cast<Conductor *>(&somewhere);
	}

	Diagram *fakeDiagram()
	{
		static int somewhere = 0;
		return reinterpret_cast<Diagram *>(&somewhere);
	}
}

TEST_CASE("T23 — um achado sem alvo diz que não tem para onde ir",
	  "[drc][t23]")
{
	SECTION("o achado do projeto inteiro não finge apontar a folha 1")
	{
		const DrcFinding finding = DrcFinding::onProject(
					QStringLiteral("missing_template"),
					DrcSeverity::Error,
					QStringLiteral("Le projet n'a pas de cartouche."));

		CHECK(finding.isValid());
		CHECK(finding.targetKind() == DrcTargetKind::None);
		CHECK_FALSE(finding.hasTarget());

			//A folha 1 seria mentira: o achado não fala dela. Zero é a
			//única resposta honesta, e hasFolio() é quem a lê.
		CHECK(finding.folio() == 0);
		CHECK_FALSE(finding.hasFolio());
		CHECK(finding.location().isEmpty());

			//Sem lugar, a linha inteira é a própria frase -- e não a
			//frase seguida de um parêntese vazio.
		CHECK(finding.describe() == finding.text());
	}

	SECTION("prometer um objeto e não trazê-lo também não é alvo")
	{
			//Este é o caso que engana: o tipo diz Element, e quem lesse
			//só o tipo chamaria setSelected() num ponteiro nulo. hasTarget()
			//testa os dois juntos para que ninguém precise testar o segundo.
		const DrcFinding finding = DrcFinding::onElement(
					QStringLiteral("no_label"),
					DrcSeverity::Warning,
					QStringLiteral("Composant sans repère."),
					nullptr);

		CHECK(finding.targetKind() == DrcTargetKind::Element);
		CHECK_FALSE(finding.hasTarget());
	}

	SECTION("com objeto, o alvo existe nas três formas")
	{
		const DrcFinding on_element = DrcFinding::onElement(
					QStringLiteral("r"), DrcSeverity::Warning,
					QStringLiteral("t"), fakeElement());
		const DrcFinding on_conductor = DrcFinding::onConductor(
					QStringLiteral("r"), DrcSeverity::Warning,
					QStringLiteral("t"), fakeConductor());
		const DrcFinding on_diagram = DrcFinding::onDiagram(
					QStringLiteral("r"), DrcSeverity::Warning,
					QStringLiteral("t"), fakeDiagram());

		CHECK(on_element.hasTarget());
		CHECK(on_element.element() == fakeElement());
		CHECK(on_conductor.hasTarget());
		CHECK(on_conductor.conductor() == fakeConductor());
		CHECK(on_diagram.hasTarget());
		CHECK(on_diagram.diagram() == fakeDiagram());

			//O condutor não é elemento, e o elemento não é condutor: se
			//os três ponteiros fossem um só, o painel chamaria o método
			//errado no objeto certo.
		CHECK(on_conductor.element() == nullptr);
		CHECK(on_element.conductor() == nullptr);
	}
}

TEST_CASE("T23 — o número de folha de um achado é o que sai impresso",
	  "[drc][t23]")
{
	DrcFinding finding = DrcFinding::onElement(
				QStringLiteral("no_part"),
				DrcSeverity::Warning,
				QStringLiteral("KM1 sans pièce."),
				fakeElement());

	SECTION("folha e posição entram na linha, nessa ordem")
	{
		finding.setFolio(4);
		finding.setPosition(QStringLiteral("B2"));

		CHECK(finding.hasFolio());
		CHECK(finding.folio() == 4);

		const QString line = finding.describe();
		INFO("a linha: " << line.toStdString());
		CHECK(line.contains(finding.text()));
		CHECK(line.contains(QStringLiteral("4")));
		CHECK(line.contains(QStringLiteral("B2")));

			//A frase não repete o lugar: o painel tem coluna para cada
			//um, e frase que repete coluna deixa a tabela ilegível de
			//lado. É describe() quem junta, e só ele.
		CHECK_FALSE(finding.text().contains(QStringLiteral("B2")));
	}

	SECTION("número que não é folha vira ausência, e não folha zero")
	{
		finding.setFolio(0);
		CHECK_FALSE(finding.hasFolio());

		finding.setFolio(-3);
		CHECK(finding.folio() == 0);
		CHECK_FALSE(finding.hasFolio());

			//E voltar a ter folha volta a valer: a guarda é sobre o
			//valor, não um estado que fica.
		finding.setFolio(1);
		CHECK(finding.hasFolio());
		CHECK(finding.folio() == 1);
	}

	SECTION("só a posição também é lugar")
	{
		finding.setPosition(QStringLiteral("C7"));
		CHECK_FALSE(finding.location().isEmpty());
		CHECK(finding.describe().contains(QStringLiteral("C7")));
	}
}

TEST_CASE("T23 — contar achado por gravidade usa a ordem escrita uma vez só",
	  "[drc][t23]")
{
	QList<DrcFinding> findings;
	findings << DrcFinding::onProject(QStringLiteral("a"),
					  DrcSeverity::Information,
					  QStringLiteral("i"));
	findings << DrcFinding::onProject(QStringLiteral("b"),
					  DrcSeverity::Warning,
					  QStringLiteral("w"));
	findings << DrcFinding::onProject(QStringLiteral("c"),
					  DrcSeverity::Error,
					  QStringLiteral("e"));

		//"Pelo menos" inclui o próprio nível: era um >= escrito num lugar
		//só justamente para que dois filtros não discordassem, e é isso
		//que estas três linhas prendem.
	CHECK(DrcFinding::countAtLeast(findings, DrcSeverity::Information) == 3);
	CHECK(DrcFinding::countAtLeast(findings, DrcSeverity::Warning) == 2);
	CHECK(DrcFinding::countAtLeast(findings, DrcSeverity::Error) == 1);

	CHECK(DrcFinding::hasAtLeast(findings, DrcSeverity::Error));

	SECTION("sem erro, a pergunta do código de saída responde não")
	{
		findings.removeLast();
		CHECK(DrcFinding::countAtLeast(findings, DrcSeverity::Error) == 0);
		CHECK_FALSE(DrcFinding::hasAtLeast(findings, DrcSeverity::Error));

			//E continua havendo aviso: "nenhum erro" não é "nada achado",
			//e confundir os dois é o que faria o painel calar.
		CHECK(DrcFinding::hasAtLeast(findings, DrcSeverity::Warning));
	}

	SECTION("lista vazia não tem nada de gravidade nenhuma")
	{
		const QList<DrcFinding> none;
		CHECK(DrcFinding::countAtLeast(none, DrcSeverity::Information) == 0);
		CHECK_FALSE(DrcFinding::hasAtLeast(none, DrcSeverity::Information));
	}
}

TEST_CASE("T23 — achado sem regra ou sem frase não é achado",
	  "[drc][t23]")
{
	CHECK_FALSE(DrcFinding().isValid());

	CHECK_FALSE(DrcFinding::onProject(QString(),
					  DrcSeverity::Error,
					  QStringLiteral("texto")).isValid());

		//Frase vazia é o caso que passaria despercebido: a linha apareceria
		//no painel, com gravidade e folha, e não diria nada.
	CHECK_FALSE(DrcFinding::onProject(QStringLiteral("regra"),
					  DrcSeverity::Error,
					  QString()).isValid());

	CHECK(DrcFinding::onProject(QStringLiteral("regra"),
				    DrcSeverity::Error,
				    QStringLiteral("texto")).isValid());
}
