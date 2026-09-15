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

#include "../../../../sources/diagram.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/ui/terminalstriptreedockwidget.h"

#include <catch2/catch.hpp>

#include <QString>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVector>

/*
	A árvore do gerenciador de réguas listava bornes independentes **em
	branco** — só o ícone, sem texto nenhum —, e o relato veio da
	prancheta, com um projeto real aberto.

	A primeira suspeita, e a que este arquivo existe para fechar, era que
	o rótulo estivesse se perdendo no caminho até a lista. Medido no
	arquivo do projeto: dos 118 bornes dele, 57 trazem `label` preenchido
	e 61 não trazem campo nenhum — os 61 são todos o mesmo símbolo, o
	batente de fim de régua, que ninguém numera. **O dado é que está
	vazio**, e as duas pontas da árvore leem o mesmo
	Element::actualLabel() através de RealTerminal::label(), então rótulo
	que chega a uma chega à outra.

	Mas "o dado está vazio" não absolve a lista: dezenas de linhas iguais
	a nada não deixam ninguém escolher uma delas, e escolher uma delas é
	o que o botão de deslocar pede. O recuo é mostrar **onde a borne está
	desenhada**, que é a mesma referência cruzada que a tabela de bornes
	independentes já mostra numa coluna própria — é por isso que a linha
	da árvore e a linha da tabela passam a se reconhecer.

	Os casos abaixo provam as duas metades, e mais uma terceira coisa que
	só apareceu porque esta foi a primeira bancada a construir este
	painel:

	1. o rótulo que **existe** continua aparecendo inteiro (que é a
	   resposta mecânica à suspeita do começo), e o que **não existe**
	   vira um texto identificável em vez de nada;
	2. a mesma borne, depois de entrar numa régua, continua identificável
	   — porque é exatamente para dentro de uma régua que ela vai, e o
	   ramo da régua tinha o mesmo vão;
	3. fechar o projeto **depois** de fechar o painel não derruba o
	   processo. Ver o comentário daquele caso: era uma queda de verdade,
	   e não da bancada.

	Rotulado T33 e não CU-33.x: o caso de uso é o projetista escolhendo a
	borne na tela. O que se prova aqui é que existe o que escolher.
*/

namespace
{
	/**
		Um projeto de uma folha com quatro bornes: dois numerados, dois sem
		rótulo nenhum.

		Os dois sem rótulo trazem `<elementInformations/>` vazio, e não um
		`label` vazio, porque é assim que o projeto real os traz: o batente
		de fim de régua é inserido e nunca recebe campo algum. Bancada que
		escreve um campo vazio estaria provando outro caso.

		As quatro posições são afastadas de propósito, cada uma numa coluna
		diferente da grade, para que a referência cruzada de uma não seja a
		da outra — é justamente o que faz uma linha ser distinguível da
		seguinte.

		A folha leva número (`folio="3"`) pelo mesmo motivo: sem ele a
		referência cruzada sai `-B4`, com o hífen solto na frente, e a
		bancada deixaria de parecer com o que aparece na tela, que é
		`3-B4`. O caso não depende do formato — ele mede a referência
		antes de comparar —, mas quem for ler a saída dele merece ver o
		que o projetista vê.
	*/
	QString fixtureXml()
	{
		struct Terminal {
			int x;
			int y;
			const char *label;
		};

		const Terminal terminals[] = {
			{120, 150, "7"},
			{300, 150, ""},
			{480, 150, "8"},
			{660, 150, ""}};

		QString instances;
		int index = 0;
		for (const Terminal &terminal : terminals)
		{
			const QString informations =
					*terminal.label == '\0'
					? QStringLiteral("<elementInformations/>")
					: QStringLiteral("<elementInformations>"
							 "<elementInformation show=\"1\" name=\"label\">%1"
							 "</elementInformation>"
							 "</elementInformations>")
					  .arg(QLatin1String(terminal.label));

			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/terminal.elmt\""
					     " uuid=\"{c0ffee02-0000-4000-8000-00000000000%3}\">"
					     "<terminals>"
					     "<terminal x=\"10\" y=\"0\" orientation=\"1\" id=\"%4\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "%5"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(terminal.x)
				     .arg(terminal.y)
				     .arg(index)
				     .arg(index)
				     .arg(informations);
			++index;
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"terminal.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"terminal\">"
			       "<names><name lang=\"en\">Terminal</name></names>"
			       "<description>"
			       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " folio=\"3\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(instances);
	}

	/// The terminal elements @a diagram draws, in the order it holds them.
	QVector<TerminalElement *> terminalsOf(Diagram *diagram)
	{
		QVector<TerminalElement *> vector_;
		if (!diagram) {
			return vector_;
		}

		const auto elements = diagram->elements();
		for (const auto element : elements)
		{
			if (element->elementData().m_type == ElementData::Terminal) {
				vector_.append(static_cast<TerminalElement *>(element));
			}
		}

		return vector_;
	}

	/// The text of the first column of each child of @a item.
	QStringList childTexts(QTreeWidgetItem *item)
	{
		QStringList list_;
		if (!item) {
			return list_;
		}

		for (int i = 0 ; i < item->childCount() ; ++i) {
			list_ << item->child(i)->text(0);
		}

		return list_;
	}

	/**
		The tree of @a dock.

		Asked of the widget by the name the form gives it, because the form
		is what the program builds: a case that read a tree of its own would
		pass while the dock went on showing blank rows.
	*/
	QTreeWidget *treeOf(TerminalStripTreeDockWidget &dock)
	{
		return dock.findChild<QTreeWidget *>(QStringLiteral("m_tree_view"));
	}
}

TEST_CASE("T33 — nenhuma borne independente é listada em branco",
	  "[terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	REQUIRE(bench.diagramCount() == 1);

	const auto terminals = terminalsOf(bench.diagram(0));
	REQUIRE(terminals.size() == 4);

		//O que o arquivo diz, medido antes de a árvore ser montada:
		//dois bornes com rótulo, dois sem nenhum.
	QStringList unlabelled_xref;
	int labelled{0};
	for (const auto terminal : terminals)
	{
		const auto real_t = terminal->realTerminal();
		REQUIRE_FALSE(real_t.isNull());

		if (real_t->label().isEmpty()) {
			unlabelled_xref << real_t->Xref();
		} else {
			++labelled;
		}
	}
	CHECK(labelled == 2);
	REQUIRE(unlabelled_xref.size() == 2);

		//Se a bancada não souber dizer onde a borne está, o caso não tem
		//como medir o recuo -- e cai dizendo isso, em vez de passar à toa
		//porque "conter uma string vazia" é sempre verdadeiro.
	INFO("referências cruzadas: "
	     << unlabelled_xref.join(QStringLiteral(" | ")).toStdString());
	REQUIRE_FALSE(unlabelled_xref.at(0).isEmpty());
	REQUIRE_FALSE(unlabelled_xref.at(1).isEmpty());
	REQUIRE(unlabelled_xref.at(0) != unlabelled_xref.at(1));

	TerminalStripTreeDockWidget dock(bench.project());
	auto tree = treeOf(dock);
	REQUIRE(tree);
	REQUIRE(tree->topLevelItemCount() == 2);

		//topLevelItem(1) é o nó "Bornes indépendante", montado em
		//buildTree() logo depois da raiz do projeto.
	auto free_item = tree->topLevelItem(1);
	REQUIRE(free_item);
	REQUIRE(free_item->childCount() == 4);

	const auto texts = childTexts(free_item);
	INFO("a árvore mostra: " << texts.join(QStringLiteral(" | ")).toStdString());

		//O defeito relatado: linha nenhuma pode estar em branco.
	for (const auto &text : texts) {
		CHECK_FALSE(text.isEmpty());
	}

		//E a resposta à primeira suspeita: rótulo que existe chega inteiro
		//à lista. Se o caminho até aqui perdesse rótulo, estes dois
		//cairiam -- e a lista seria defeito de leitura, não de escrita.
	CHECK(texts.contains(QStringLiteral("7")));
	CHECK(texts.contains(QStringLiteral("8")));

		//Quem não tem rótulo é identificado por onde está desenhado, e
		//marcado como recuo: entre parênteses, para que ninguém saia
		//procurando aquilo no desenho como se fosse número de borne.
	for (const auto &xref : unlabelled_xref)
	{
		INFO("referência cruzada procurada: " << xref.toStdString());
		const auto matching = texts.filter(xref);
		REQUIRE(matching.size() == 1);
		CHECK(matching.first().startsWith(QLatin1Char('(')));
		CHECK(matching.first() != xref);
	}

		//O recuo é de exibição, e só. O dado continua vazio: é
		//RealTerminal::label() que o desenho da régua, a ordenação e os
		//textos de desfazer leem, e um recuo impresso no trilho seria
		//mentira no papel.
	for (const auto terminal : terminals)
	{
		const auto real_t = terminal->realTerminal();
		if (unlabelled_xref.contains(real_t->Xref())) {
			CHECK(real_t->label().isEmpty());
		}
	}
}

TEST_CASE("T33 — a borne sem rótulo continua identificável dentro da régua",
	  "[terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	const auto terminals = terminalsOf(bench.diagram(0));
	REQUIRE(terminals.size() == 4);

	TerminalElement *unlabelled{nullptr};
	for (const auto terminal : terminals)
	{
		const auto real_t = terminal->realTerminal();
		if (!real_t.isNull() && real_t->label().isEmpty()) {
			unlabelled = terminal;
			break;
		}
	}
	REQUIRE(unlabelled);

	const auto xref = unlabelled->realTerminal()->Xref();
	INFO("referência cruzada da borne: " << xref.toStdString());
	REQUIRE_FALSE(xref.isEmpty());

		//É para dentro de uma régua que ela vai -- é isso que o botão de
		//deslocar faz, e é onde o mesmo vão existia.
	auto strip = bench->newTerminalStrip(QStringLiteral("="),
					     QStringLiteral("+"),
					     QStringLiteral("X1"));
	REQUIRE(strip);
	REQUIRE(strip->addTerminal(unlabelled));
	REQUIRE(strip->physicalTerminalCount() == 1);

	TerminalStripTreeDockWidget dock(bench.project());
	auto tree = treeOf(dock);
	REQUIRE(tree);
	REQUIRE(tree->topLevelItemCount() == 2);

		//Ela saiu das independentes...
	auto free_item = tree->topLevelItem(1);
	REQUIRE(free_item);
	CHECK(free_item->childCount() == 3);

		//...e entrou na régua, em projeto > instalação > localização >
		//régua > borne.
	auto item = tree->topLevelItem(0);
	REQUIRE(item);
	for (int depth = 0 ; depth < 4 ; ++depth)
	{
		INFO("profundidade " << depth << " em " << item->text(0).toStdString());
		REQUIRE(item->childCount() == 1);
		item = item->child(0);
		REQUIRE(item);
	}

	INFO("a régua mostra: " << item->text(0).toStdString());
	CHECK_FALSE(item->text(0).isEmpty());
	CHECK(item->text(0).contains(xref));
	CHECK(item->text(0).startsWith(QLatin1Char('(')));
}

/*
	Este caso não mede texto nenhum: ele mede que o processo continua de
	pé, e existe porque a primeira versão desta bancada derrubou a suíte
	inteira com SIGSEGV.

	A queda não era da bancada. TerminalStripTreeDockWidget::setProject()
	ligava o destroyed() do projeto a uma lambda **sem objeto de
	contexto**, e conexão assim pertence só a quem emite: ela sobrevive ao
	painel. Painel destruído antes do projeto — fechar a janela do
	gerenciador e depois fechar o projeto faz exatamente isso — deixava a
	lambda com um `this` pendurado, e o destroyed() chamava reload() sobre
	memória liberada.

	Os dois casos acima já faziam essa sequência sem querer, pela ordem em
	que as variáveis morrem, e por isso morriam sem dizer de quê. Aqui ela
	é feita de propósito e tem nome: quem tirar o objeto de contexto de
	volta vê este caso na última linha antes da queda, em vez de procurar
	num caso que fala de rótulo.
*/
TEST_CASE("T33 — fechar o projeto depois do painel não derruba o processo",
	  "[terminalstrip]")
{
	{
		UiBench::ScratchProject bench(fixtureXml());
		INFO(bench.error().toStdString());
		REQUIRE(bench.isOpen());

			//Cru e apagado à mão de propósito: é a ordem que interessa,
			//e ela é o contrário da ordem natural da pilha. A conferência
			//fica depois do delete para que uma reprovação não deixe o
			//painel vivo -- que seria justamente a ordem que este caso
			//não quer medir.
		auto dock = new TerminalStripTreeDockWidget(bench.project());
		const bool built = treeOf(*dock) != nullptr;
		delete dock;
		REQUIRE(built);

			//O projeto morre aqui, já sem painel nenhum para avisar.
	}

	SUCCEED("o projeto fechou depois do painel, e o processo seguiu vivo");
}
