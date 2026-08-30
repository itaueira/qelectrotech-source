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
#include "../../../sources/macro/circuittable.h"
#include "../../../sources/macro/macroparameter.h"
#include "../../../sources/macro/macroparameterset.h"
#include "../../../sources/macro/macrouuid.h"
#include "qt_catch_tostring.h"

#include <QDomDocument>
#include <QDomElement>

namespace
{
		/**
			Two elements, one dynamic text, one conductor and two links, in the
			shapes the real .qetmak of the fork carries them: braces on every
			uuid, the conductor naming the element at each end under element1
			and element2 and the terminal at each end under terminal1 and
			terminal2, one link pointing at a master that is inside the macro
			and one pointing at a master that is not.
		*/
	QString diagramXml()
	{
		return QString::fromUtf8(
			"<diagram>"
			 "<elements>"
			  "<element type=\"import/ITR/contactor.elmt\""
			  " uuid=\"{11111111-1111-4111-8111-111111111111}\" x=\"40\" y=\"20\">"
			   "<terminals>"
			    "<terminal id=\"18\" orientation=\"3\" x=\"-9\" y=\"0\"/>"
			   "</terminals>"
			   "<dynamic_texts>"
			    "<dynamic_elmt_text text_from=\"ElementInfo\""
			    " uuid=\"{22222222-2222-4222-8222-222222222222}\" x=\"0\" y=\"-10\">"
			     "<text>${TAG}</text>"
			     "<info_name>label</info_name>"
			    "</dynamic_elmt_text>"
			   "</dynamic_texts>"
			  "</element>"
			  "<element type=\"import/ITR/auxiliary.elmt\""
			  " uuid=\"{33333333-3333-4333-8333-333333333333}\" x=\"90\" y=\"20\">"
			   "<links_uuids>"
			    "<link_uuid uuid=\"{11111111-1111-4111-8111-111111111111}\"/>"
			   "</links_uuids>"
			  "</element>"
			  "<element type=\"import/ITR/remote.elmt\""
			  " uuid=\"{44444444-4444-4444-8444-444444444444}\" x=\"140\" y=\"20\">"
			   "<links_uuids>"
			    "<link_uuid uuid=\"{99999999-9999-4999-8999-999999999999}\"/>"
			   "</links_uuids>"
			  "</element>"
			 "</elements>"
			 "<conductors>"
			  "<conductor uuid=\"{55555555-5555-4555-8555-555555555555}\""
			  " element1=\"{11111111-1111-4111-8111-111111111111}\""
			  " element2=\"{33333333-3333-4333-8333-333333333333}\""
			  " terminal1=\"{66666666-6666-4666-8666-666666666666}\""
			  " terminal2=\"{77777777-7777-4777-8777-777777777777}\""
			  " num=\"${PREFIXO_FIO}-3\"/>"
			 "</conductors>"
			"</diagram>");
	}

		/// The uuids written in diagramXml(), by the name this file calls them.
	const QString kContactor  = QStringLiteral("{11111111-1111-4111-8111-111111111111}");
	const QString kText       = QStringLiteral("{22222222-2222-4222-8222-222222222222}");
	const QString kAuxiliary  = QStringLiteral("{33333333-3333-4333-8333-333333333333}");
	const QString kRemote     = QStringLiteral("{44444444-4444-4444-8444-444444444444}");
	const QString kConductor  = QStringLiteral("{55555555-5555-4555-8555-555555555555}");
	const QString kTerminal1  = QStringLiteral("{66666666-6666-4666-8666-666666666666}");
	const QString kTerminal2  = QStringLiteral("{77777777-7777-4777-8777-777777777777}");
	const QString kOutside    = QStringLiteral("{99999999-9999-4999-8999-999999999999}");

		/**
			@brief The element of @a document whose tag is @a tag and whose
			attribute @a attribute holds @a value.
		*/
	QDomElement findByAttribute(const QDomDocument &document,
				    const QString &tag,
				    const QString &attribute,
				    const QString &value)
	{
		const QDomNodeList list = document.elementsByTagName(tag);
		for (int i = 0 ; i < list.count() ; ++ i)
		{
			const QDomElement element = list.at(i).toElement();
			if (element.attribute(attribute) == value) {
				return element;
			}
		}
		return QDomElement();
	}

		/// The only <conductor> of @a document.
	QDomElement conductorOf(const QDomDocument &document)
	{
		return document.elementsByTagName(QStringLiteral("conductor")).at(0).toElement();
	}

		/// A text parameter, the kind that carries a tag or a wire prefix.
	MacroParameter textParameter(const QString &name,
				     const QString &label,
				     const QString &default_value = QString(),
				     bool required = false)
	{
		MacroParameter parameter(name, label, MacroParameterType::Text);
		parameter.default_value = default_value;
		parameter.required = required;
		return parameter;
	}

		/**
			@brief The direct starter: a tag, a wire prefix and a power that
			has to be filled in.
		*/
	MacroParameterSet directStarter()
	{
		MacroParameterSet set;
		set.append(textParameter(QStringLiteral("TAG"),
					 QStringLiteral("Tag do motor"),
					 QStringLiteral("-M1")));
		set.append(textParameter(QStringLiteral("PREFIXO_FIO"),
					 QStringLiteral("Prefixo do fio"),
					 QStringLiteral("W")));
		set.append(textParameter(QStringLiteral("POTENCIA"),
					 QStringLiteral("Potência (cv)"),
					 QString(),
					 true));
		return set;
	}

		/**
			@brief The reversing starter: the three of the direct one, in the
			same order, plus the two only it has.
		*/
	MacroParameterSet reversingStarter()
	{
		MacroParameterSet set = directStarter();
		set.append(textParameter(QStringLiteral("TAG_REVERSAO"),
					 QStringLiteral("Tag do contator de reversão"),
					 QStringLiteral("-KM1R")));

		MacroParameter travamento(QStringLiteral("TRAVAMENTO"),
					  QStringLiteral("Travamento"),
					  MacroParameterType::List);
		travamento.choices = QStringList{QStringLiteral("Elétrico"),
						 QStringLiteral("Elétrico e mecânico")};
		travamento.default_value = QStringLiteral("Elétrico");
		set.append(travamento);
		return set;
	}

	const QString kDirect    = QStringLiteral("common://itr/partida-direta.qetmak");
	const QString kReversing = QStringLiteral("common://itr/partida-reversora.qetmak");
}

TEST_CASE("CU-08.1 — vinte inserções do mesmo macro são vinte circuitos, não vinte cópias de um",
	  "[circuit]")
{
	QDomDocument document;
	REQUIRE(document.setContent(diagramXml()));
	QDomElement subtree = document.documentElement();

	const MacroUuid::Result result = MacroUuid::renew(subtree);

	SECTION("todo definidor de uuid recebe identidade nova, e só os definidores")
	{
			//dois elementos com filho, um sem, o texto dinâmico e o condutor:
			//cinco. O terminal não define uuid nenhum — o dele nasce no .elmt.
		CHECK(result.definitions == 5);
		CHECK(result.issued.count() == 5);
		CHECK(result.map.count() == 5);

		CHECK(result.map.contains(MacroUuid::normalised(kContactor)));
		CHECK(result.map.contains(MacroUuid::normalised(kText)));
		CHECK(result.map.contains(MacroUuid::normalised(kAuxiliary)));
		CHECK(result.map.contains(MacroUuid::normalised(kRemote)));
		CHECK(result.map.contains(MacroUuid::normalised(kConductor)));

		CHECK_FALSE(result.map.contains(MacroUuid::normalised(kTerminal1)));
		CHECK_FALSE(result.map.contains(MacroUuid::normalised(kOutside)));
	}

	SECTION("nenhum uuid antigo sobra escrito em lugar nenhum")
	{
		const QString written = document.toString();
		CHECK_FALSE(written.contains(kContactor));
		CHECK_FALSE(written.contains(kText));
		CHECK_FALSE(written.contains(kAuxiliary));
		CHECK_FALSE(written.contains(kRemote));
		CHECK_FALSE(written.contains(kConductor));
	}

	SECTION("o condutor acompanha os dois elementos que ele liga")
	{
			//Este é o caso que a primeira redação do plano deixaria passar:
			//o condutor guarda a ponta em element1 e element2, e não em um
			//atributo chamado uuid.
		const QDomElement conductor = conductorOf(document);
		CHECK(conductor.attribute(QStringLiteral("element1"))
		      == result.map.value(MacroUuid::normalised(kContactor)));
		CHECK(conductor.attribute(QStringLiteral("element2"))
		      == result.map.value(MacroUuid::normalised(kAuxiliary)));
	}

	SECTION("o terminal das duas pontas não se mexe")
	{
			//Ele nasce na definição do símbolo e é o mesmo em toda instância:
			//renová-lo desligaria o condutor do borne em que ele está.
		const QDomElement conductor = conductorOf(document);
		CHECK(conductor.attribute(QStringLiteral("terminal1")) == kTerminal1);
		CHECK(conductor.attribute(QStringLiteral("terminal2")) == kTerminal2);
	}

	SECTION("a ligação mestre-escravo de dentro do macro segue a cópia")
	{
		const QDomElement auxiliary =
				findByAttribute(document,
						QStringLiteral("element"),
						QStringLiteral("uuid"),
						result.map.value(MacroUuid::normalised(kAuxiliary)));
		REQUIRE_FALSE(auxiliary.isNull());

		const QDomElement link = auxiliary.firstChildElement(QStringLiteral("links_uuids"))
						  .firstChildElement(QStringLiteral("link_uuid"));
		REQUIRE_FALSE(link.isNull());
		CHECK(link.attribute(QStringLiteral("uuid"))
		      == result.map.value(MacroUuid::normalised(kContactor)));
	}

	SECTION("a ligação que aponta para fora da subárvore fica intacta")
	{
			//É o que preserva o vínculo com o que já está desenhado na folha.
		const QDomElement remote =
				findByAttribute(document,
						QStringLiteral("element"),
						QStringLiteral("uuid"),
						result.map.value(MacroUuid::normalised(kRemote)));
		REQUIRE_FALSE(remote.isNull());

		const QDomElement link = remote.firstChildElement(QStringLiteral("links_uuids"))
					       .firstChildElement(QStringLiteral("link_uuid"));
		REQUIRE_FALSE(link.isNull());
		CHECK(link.attribute(QStringLiteral("uuid")) == kOutside);
	}

	SECTION("as referências contadas são as três que havia")
	{
			//link_uuid do escravo, element1 e element2 do condutor. O link
			//para fora e os dois terminais não entram na conta porque não
			//foram tocados.
		CHECK(result.references == 3);
	}

	SECTION("nada do que foi emitido se repete")
	{
		QStringList issued = result.issued;
		CHECK(issued.removeDuplicates() == 0);
	}
}

TEST_CASE("CU-08.1 — duas renovações da mesma origem não coincidem em nada", "[circuit]")
{
	QDomDocument first;
	QDomDocument second;
	REQUIRE(first.setContent(diagramXml()));
	REQUIRE(second.setContent(diagramXml()));

	QDomElement first_root = first.documentElement();
	QDomElement second_root = second.documentElement();
	const MacroUuid::Result first_result = MacroUuid::renew(first_root);
	const MacroUuid::Result second_result = MacroUuid::renew(second_root);

	SECTION("as dez identidades emitidas são dez")
	{
		QStringList all = first_result.issued;
		all << second_result.issued;
		REQUIRE(all.count() == 10);
		CHECK(all.removeDuplicates() == 0);
	}

	SECTION("o condutor do segundo circuito aponta para o elemento do segundo")
	{
			//Sem renew(), os dois condutores teriam o mesmo uuid de ponta, e
			//o segundo circuito acabaria ligado ao contator do primeiro.
		const QDomElement first_conductor = conductorOf(first);
		const QDomElement second_conductor = conductorOf(second);
		CHECK(first_conductor.attribute(QStringLiteral("element1"))
		      != second_conductor.attribute(QStringLiteral("element1")));
		CHECK(second_conductor.attribute(QStringLiteral("element1"))
		      == second_result.map.value(MacroUuid::normalised(kContactor)));
	}
}

TEST_CASE("MacroUuid — o mesmo uuid escrito duas vezes vira um só uuid novo", "[circuit]")
{
		//É o estado em que uma inserção anterior sem renew() deixou o projeto:
		//dois elementos com a mesma identidade. Renovar tem de sair daí com uma
		//identidade só, e não com meia troca.
	QDomDocument document;
	REQUIRE(document.setContent(QString::fromUtf8(
			"<diagram><elements>"
			"<element uuid=\"{11111111-1111-4111-8111-111111111111}\" x=\"0\"/>"
			"<element uuid=\"{11111111-1111-4111-8111-111111111111}\" x=\"50\"/>"
			"</elements></diagram>")));

	QDomElement subtree = document.documentElement();
	const MacroUuid::Result result = MacroUuid::renew(subtree);

	CHECK(result.map.count() == 1);
	CHECK(result.definitions == 2);

	const QDomNodeList elements = document.elementsByTagName(QStringLiteral("element"));
	REQUIRE(elements.count() == 2);
	const QString issued = result.map.value(MacroUuid::normalised(kContactor));
	CHECK(elements.at(0).toElement().attribute(QStringLiteral("uuid")) == issued);
	CHECK(elements.at(1).toElement().attribute(QStringLiteral("uuid")) == issued);
}

TEST_CASE("MacroUuid — a forma em que estava escrito é preservada", "[circuit]")
{
	SECTION("sem chaves continua sem chaves")
	{
		QDomDocument document;
		REQUIRE(document.setContent(QString::fromUtf8(
				"<diagram><elements>"
				"<element uuid=\"11111111-1111-4111-8111-111111111111\"/>"
				"</elements></diagram>")));

		QDomElement subtree = document.documentElement();
		const MacroUuid::Result result = MacroUuid::renew(subtree);
		REQUIRE(result.definitions == 1);

		const QString written = document.elementsByTagName(QStringLiteral("element"))
						.at(0).toElement()
						.attribute(QStringLiteral("uuid"));
		CHECK_FALSE(written.startsWith(QLatin1Char('{')));
		CHECK(MacroUuid::normalised(written)
		      == MacroUuid::normalised(result.map.value(
				      MacroUuid::normalised(kContactor))));
	}

	SECTION("maiúscula e minúscula encontram o mesmo uuid")
	{
			//Element escreve com chaves e WiringListExport compara sem elas,
			//e em minúscula. As duas grafias têm de se encontrar.
		QDomDocument document;
		REQUIRE(document.setContent(QString::fromUtf8(
				"<diagram><elements>"
				"<element uuid=\"{AAAAAAAA-1111-4111-8111-111111111111}\"/>"
				"</elements>"
				"<conductors>"
				"<conductor uuid=\"{55555555-5555-4555-8555-555555555555}\""
				" element1=\"aaaaaaaa-1111-4111-8111-111111111111\"/>"
				"</conductors></diagram>")));

		QDomElement subtree = document.documentElement();
		const MacroUuid::Result result = MacroUuid::renew(subtree);
		CHECK(result.references == 1);

		const QDomElement conductor = conductorOf(document);
		const QString element1 = conductor.attribute(QStringLiteral("element1"));
		CHECK_FALSE(element1.startsWith(QLatin1Char('{')));
		CHECK(MacroUuid::normalised(element1)
		      == MacroUuid::normalised(result.map.value(
				      QStringLiteral("aaaaaaaa-1111-4111-8111-111111111111"))));
	}
}

TEST_CASE("MacroUuid — subárvore sem uuid nenhum sai como entrou", "[circuit]")
{
	QDomDocument document;
	REQUIRE(document.setContent(QString::fromUtf8(
			"<diagram><elements/><conductors/></diagram>")));
	const QString before = document.toString();

	QDomElement subtree = document.documentElement();
	const MacroUuid::Result result = MacroUuid::renew(subtree);

	CHECK(result.definitions == 0);
	CHECK(result.references == 0);
	CHECK(result.map.isEmpty());
	CHECK(document.toString() == before);

	SECTION("e um nó nulo não é motivo para parar o programa")
	{
		QDomElement null_element;
		const MacroUuid::Result nothing = MacroUuid::renew(null_element);
		CHECK(nothing.definitions == 0);
		CHECK(nothing.map.isEmpty());
	}
}

TEST_CASE("CU-08.4 — circuitos diferentes na mesma tabela", "[circuit]")
{
	CircuitTable table;
	table.setParameters(kDirect, directStarter());
	table.setParameters(kReversing, reversingStarter());

	const int direct_row = table.appendRow(kDirect);
	const int reversing_row = table.appendRow(kReversing);
	REQUIRE(direct_row == 0);
	REQUIRE(reversing_row == 1);

	SECTION("as colunas são a união das duas declarações, na ordem em que foram declaradas")
	{
		const QStringList expected{QStringLiteral("TAG"),
					   QStringLiteral("PREFIXO_FIO"),
					   QStringLiteral("POTENCIA"),
					   QStringLiteral("TAG_REVERSAO"),
					   QStringLiteral("TRAVAMENTO")};
		CHECK(table.columns() == expected);
		CHECK(table.columnCount() == 5);
	}

	SECTION("o cabeçalho traz o rótulo, que é o que o usuário lê")
	{
		CHECK(table.columnLabel(QStringLiteral("POTENCIA"))
		      == QString::fromUtf8("Potência (cv)"));
		CHECK(table.columnLabel(QStringLiteral("TRAVAMENTO"))
		      == QStringLiteral("Travamento"));
			//Coluna que macro nenhum declara responde pelo próprio nome.
		CHECK(table.columnLabel(QStringLiteral("INEXISTENTE"))
		      == QStringLiteral("INEXISTENTE"));
	}

	SECTION("a coluna que o macro da linha não declara fica inerte")
	{
		CHECK(table.isInert(direct_row, QStringLiteral("TAG_REVERSAO")));
		CHECK(table.isInert(direct_row, QStringLiteral("TRAVAMENTO")));
		CHECK_FALSE(table.isInert(direct_row, QStringLiteral("TAG")));
		CHECK_FALSE(table.isInert(reversing_row, QStringLiteral("TAG_REVERSAO")));
	}

	SECTION("célula inerte recusa ser preenchida, e diz por quê")
	{
		QString error;
		CHECK_FALSE(table.setValue(direct_row,
					   QStringLiteral("TRAVAMENTO"),
					   QStringLiteral("Elétrico"),
					   &error));
		CHECK(error.contains(QStringLiteral("TRAVAMENTO")));
		CHECK(table.value(direct_row, QStringLiteral("TRAVAMENTO")).isEmpty());
	}

	SECTION("a linha nasce com os padrões do macro dela")
	{
		CHECK(table.value(direct_row, QStringLiteral("TAG")) == QStringLiteral("-M1"));
		CHECK(table.value(direct_row, QStringLiteral("PREFIXO_FIO")) == QStringLiteral("W"));
		CHECK(table.value(direct_row, QStringLiteral("POTENCIA")).isEmpty());
		CHECK(table.value(reversing_row, QStringLiteral("TRAVAMENTO"))
		      == QString::fromUtf8("Elétrico"));
	}

	SECTION("mudar de ideia duas vezes não custa o que já foi digitado")
	{
			//O caso é o do CU-08.4: a pessoa marca a linha como reversora,
			//preenche as duas colunas que só ela tem, volta para direta e
			//volta de novo para reversora. Se o valor sumisse na ida, ela
			//digitaria tudo outra vez.
		REQUIRE(table.setMacroPath(direct_row, kReversing));
		REQUIRE(table.setValue(direct_row,
				       QStringLiteral("TAG_REVERSAO"),
				       QStringLiteral("-KM7R")));
		REQUIRE(table.setValue(direct_row,
				       QStringLiteral("TRAVAMENTO"),
				       QString::fromUtf8("Elétrico e mecânico")));

		REQUIRE(table.setMacroPath(direct_row, kDirect));
		CHECK(table.isInert(direct_row, QStringLiteral("TAG_REVERSAO")));
		CHECK(table.value(direct_row, QStringLiteral("TAG_REVERSAO"))
		      == QStringLiteral("-KM7R"));

		REQUIRE(table.setMacroPath(direct_row, kReversing));
		CHECK_FALSE(table.isInert(direct_row, QStringLiteral("TAG_REVERSAO")));
		CHECK(table.value(direct_row, QStringLiteral("TAG_REVERSAO"))
		      == QStringLiteral("-KM7R"));
		CHECK(table.value(direct_row, QStringLiteral("TRAVAMENTO"))
		      == QString::fromUtf8("Elétrico e mecânico"));
	}

	SECTION("valor fora da lista declarada é recusado")
	{
		QString error;
		CHECK_FALSE(table.setValue(reversing_row,
					   QStringLiteral("TRAVAMENTO"),
					   QStringLiteral("Pneumático"),
					   &error));
		CHECK_FALSE(error.isEmpty());
		CHECK(table.value(reversing_row, QStringLiteral("TRAVAMENTO"))
		      == QString::fromUtf8("Elétrico"));

			//Apagar, porém, é sempre permitido: é como se deixa a célula em
			//branco para preencher depois.
		CHECK(table.setValue(reversing_row, QStringLiteral("TRAVAMENTO"), QString()));
		CHECK(table.value(reversing_row, QStringLiteral("TRAVAMENTO")).isEmpty());
	}

	SECTION("os macros em uso saem na ordem em que apareceram")
	{
		CHECK(table.macroPaths() == QStringList{kDirect, kReversing});
	}
}

TEST_CASE("CU-08.6 — a linha inválida é relatada, e as boas não são reféns dela", "[circuit]")
{
	CircuitTable table;
	table.setParameters(kDirect, directStarter());
	table.setParameters(kReversing, reversingStarter());

	for (int i = 0 ; i < 3 ; ++ i)
	{
		const int row = table.appendRow(kDirect);
		REQUIRE(table.setValue(row,
				       QStringLiteral("TAG"),
				       QStringLiteral("-M%1").arg(row + 1)));
		if (row != 1) {
			REQUIRE(table.setValue(row, QStringLiteral("POTENCIA"), QStringLiteral("5")));
		}
	}

	SECTION("uma linha problemática, e é a que está em branco")
	{
		const QList<CircuitTable::Problem> problems = table.problems();
		REQUIRE(problems.count() == 1);
		CHECK(problems.first().row == 1);
		CHECK(problems.first().missing == QStringList{QStringLiteral("POTENCIA")});
		CHECK(problems.first().refused.isEmpty());
		CHECK_FALSE(problems.first().no_macro);
	}

	SECTION("a queixa nomeia a linha pelo número que está na tela")
	{
		const QString text = table.problems().first().text();
		CHECK(text.contains(QStringLiteral("2")));
		CHECK(text.contains(QStringLiteral("POTENCIA")));
	}

	SECTION("a linha problemática guarda o id, que sobrevive a uma reordenação")
	{
		const QString id = table.problems().first().id;
		CHECK_FALSE(id.isEmpty());
		CHECK(table.indexOfId(id) == 1);

		REQUIRE(table.removeRow(0));
		CHECK(table.indexOfId(id) == 0);
	}

	SECTION("só espaço em branco conta como em branco")
	{
		REQUIRE(table.setValue(1, QStringLiteral("POTENCIA"), QStringLiteral("   ")));
		REQUIRE(table.problems().count() == 1);
		REQUIRE(table.setValue(1, QStringLiteral("POTENCIA"), QStringLiteral("7,5")));
		CHECK(table.problems().isEmpty());
	}

	SECTION("linha sem macro nenhum é dita, e é dita como tal")
	{
		const int row = table.appendRow(QString());
		REQUIRE(table.setValue(1, QStringLiteral("POTENCIA"), QStringLiteral("5")));

		const QList<CircuitTable::Problem> problems = table.problems();
		REQUIRE(problems.count() == 1);
		CHECK(problems.first().row == row);
		CHECK(problems.first().no_macro);
		CHECK_FALSE(problems.first().text().isEmpty());
	}

	SECTION("valor que sobrou inerte não é cobrado da linha")
	{
			//A linha foi reversora, ganhou TRAVAMENTO, e voltou a ser direta.
			//O valor fica guardado, mas não é um valor que esta linha passa,
			//e portanto não é julgado pela declaração da reversora.
		REQUIRE(table.setMacroPath(1, kReversing));
		REQUIRE(table.setValue(1, QStringLiteral("POTENCIA"), QStringLiteral("5")));
		REQUIRE(table.setMacroPath(1, kDirect));
		CHECK(table.problems().isEmpty());
	}

	SECTION("macro que ninguém apresentou à tabela não é julgado por ela")
	{
			//A tabela não abre arquivo: se ninguém lhe disse o que aquele
			//.qetmak declara, ela não tem como saber o que falta. Quem abriu
			//o arquivo é que relata a falha de leitura.
		const int row = table.appendRow(QStringLiteral("common://itr/desconhecido.qetmak"));
		REQUIRE(row == 3);
		REQUIRE(table.setValue(1, QStringLiteral("POTENCIA"), QStringLiteral("5")));
		CHECK(table.problems().isEmpty());
	}
}

TEST_CASE("CU-08.7 — a tabela sobrevive a salvar, fechar e reabrir", "[circuit]")
{
	CircuitTable table;
	table.setParameters(kDirect, directStarter());
	table.setParameters(kReversing, reversingStarter());

	const int first = table.appendRow(kDirect);
	REQUIRE(table.setValue(first, QStringLiteral("TAG"), QStringLiteral("-M1")));
	REQUIRE(table.setValue(first, QStringLiteral("POTENCIA"), QStringLiteral("7,5")));

	const int second = table.appendRow(kReversing);
	REQUIRE(table.setValue(second, QStringLiteral("TAG"), QStringLiteral("-M2")));
	REQUIRE(table.setValue(second, QStringLiteral("POTENCIA"), QStringLiteral("15")));
	REQUIRE(table.setValue(second,
			       QStringLiteral("TRAVAMENTO"),
			       QString::fromUtf8("Elétrico e mecânico")));
		//e um valor que ficou inerte, que também tem de voltar
	REQUIRE(table.setMacroPath(second, kDirect));

	QDomDocument document;
	document.appendChild(table.toXml(document));

	CircuitTable read;
	read.setParameters(kDirect, directStarter());
	read.setParameters(kReversing, reversingStarter());
	REQUIRE(read.fromXml(document.documentElement()));

	SECTION("as duas linhas voltam, na ordem, com macro e valores")
	{
		REQUIRE(read.rowCount() == 2);
		CHECK(read.macroPath(0) == kDirect);
		CHECK(read.value(0, QStringLiteral("TAG")) == QStringLiteral("-M1"));
		CHECK(read.value(0, QStringLiteral("POTENCIA")) == QStringLiteral("7,5"));
		CHECK(read.value(1, QStringLiteral("TAG")) == QStringLiteral("-M2"));
	}

	SECTION("o valor que ficou inerte volta inerte, e com o que tinha")
	{
		CHECK(read.isInert(1, QStringLiteral("TRAVAMENTO")));
		CHECK(read.value(1, QStringLiteral("TRAVAMENTO"))
		      == QString::fromUtf8("Elétrico e mecânico"));
	}

	SECTION("o id de cada linha é o mesmo de antes, senão regerar uma linha só é impossível")
	{
		CHECK(read.row(0).id == table.row(0).id);
		CHECK(read.row(1).id == table.row(1).id);
		CHECK(read.indexOfId(table.row(1).id) == 1);
	}

	SECTION("salvar duas vezes sem editar dá o mesmo arquivo")
	{
			//Senão todo salvamento aparece como alteração no controle de
			//versão do projeto, e ninguém consegue ver o que mudou de fato.
		QDomDocument again;
		again.appendChild(read.toXml(again));
		CHECK(again.toString() == document.toString());
	}

	SECTION("linha antiga sem id ganha um ao ser lida")
	{
		QDomDocument old_file;
		REQUIRE(old_file.setContent(QString::fromUtf8(
				"<circuit_table>"
				"<circuit macro=\"common://itr/partida-direta.qetmak\">"
				"<value name=\"TAG\">-M9</value>"
				"</circuit>"
				"</circuit_table>")));

		CircuitTable older;
		older.setParameters(kDirect, directStarter());
		REQUIRE(older.fromXml(old_file.documentElement()));
		REQUIRE(older.rowCount() == 1);
		CHECK_FALSE(older.row(0).id.isEmpty());
		CHECK(older.value(0, QStringLiteral("TAG")) == QStringLiteral("-M9"));
	}

	SECTION("outra etiqueta não é uma tabela de circuitos, e é recusada")
	{
		QDomDocument other;
		REQUIRE(other.setContent(QStringLiteral("<valuesets/>")));
		CircuitTable refused;
		CHECK_FALSE(refused.fromXml(other.documentElement()));
	}
}

TEST_CASE("CircuitTable — as operações de linha e o que elas recusam", "[circuit]")
{
	CircuitTable table;
	table.setParameters(kDirect, directStarter());

	SECTION("uma tabela recém-criada não tem linha nem coluna")
	{
		CHECK(table.isEmpty());
		CHECK(table.rowCount() == 0);
		CHECK(table.columns().isEmpty());
		CHECK(table.problems().isEmpty());
	}

	SECTION("cada linha nasce com um id que nenhuma outra tem")
	{
		const int a = table.appendRow(kDirect);
		const int b = table.appendRow(kDirect);
		CHECK(table.row(a).id != table.row(b).id);
		CHECK(table.indexOfId(table.row(b).id) == b);
		CHECK(table.indexOfId(QStringLiteral("nao-existe")) == -1);
		CHECK(table.indexOfId(QString()) == -1);
	}

	SECTION("inserir no meio funciona, fora da tabela não")
	{
		table.appendRow(kDirect);
		table.appendRow(kDirect);
		CHECK(table.insertRow(1, CircuitRow(kDirect)));
		CHECK(table.rowCount() == 3);
		CHECK_FALSE(table.insertRow(-1, CircuitRow(kDirect)));
		CHECK_FALSE(table.insertRow(9, CircuitRow(kDirect)));
		CHECK(table.rowCount() == 3);
	}

	SECTION("índice fora da tabela devolve linha nula, e não estoura")
	{
		CHECK(table.row(0).isNull());
		CHECK(table.row(-1).isNull());
		CHECK(table.macroPath(4).isEmpty());
		CHECK(table.value(4, QStringLiteral("TAG")).isEmpty());
		CHECK_FALSE(table.removeRow(0));
		CHECK_FALSE(table.setMacroPath(0, kDirect));

		QString error;
		CHECK_FALSE(table.setValue(0, QStringLiteral("TAG"), QStringLiteral("-M1"), &error));
		CHECK_FALSE(error.isEmpty());
	}

	SECTION("esvaziar apaga as linhas e conserva o que os macros declaram")
	{
		table.appendRow(kDirect);
		table.clear();
		CHECK(table.isEmpty());
		CHECK(table.hasParameters(kDirect));
		CHECK(table.parameters(kDirect).count() == 3);
	}

	SECTION("um macro sem caminho não é registrado")
	{
		table.setParameters(QString(), directStarter());
		CHECK_FALSE(table.hasParameters(QString()));
	}
}
