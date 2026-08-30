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
#include "../../../sources/plc/iocircuit.h"
#include "../../../sources/plc/iolist.h"
#include "../../../sources/plc/iopoint.h"
#include "qt_catch_tostring.h"

#include <QStringList>

namespace
{
	const QString MACRO =
			QStringLiteral("C:/macros/entrada-digital.qetmak");
	const QString OUTRO =
			QStringLiteral("C:/macros/saida-digital.qetmak");
	const QString CARTAO = QStringLiteral("{cartao-de-entrada}");

		/// A macro that declares @a names, all of them free text.
	MacroParameterSet parametros(const QStringList &names)
	{
		MacroParameterSet set;
		for (const QString &name : names)
		{
			set.append(MacroParameter(name, name,
						  MacroParameterType::Text));
		}
		return set;
	}

		/**
			One line of the sheet, already sitting in a channel of the
			card - which is the only state from which a circuit can be
			drawn at all.
		*/
	IoPoint atribuido(const char *tag,
			  const char *description,
			  int io_index,
			  const char *channel)
	{
		IoPoint point;
		point.tag = QString::fromUtf8(tag);
		point.description = QString::fromUtf8(description);
		point.connect_to = MACRO;
		point.master_uuid = CARTAO;
		point.io_index = io_index;
		point.channel = QString::fromUtf8(channel);
		return point;
	}
}

TEST_CASE("CU-11.4 — o ponto vira circuito, e o que ele sabe entra "
	  "nas variáveis do macro",
	  "[iocircuit]")
{
	IoList list;

	IoPoint point = atribuido("BP1", "Bouton de marche", 0, "I0.0");
	point.address = QStringLiteral("I0.0");
	point.comment = QString::fromUtf8("na porta do quadro");
	const QString id = list.append(point);
	REQUIRE(!id.isEmpty());

	CircuitTable table;
	table.setParameters(MACRO,
			    parametros(QStringList{QStringLiteral("MARCACAO"),
						   QStringLiteral("DESCRICAO"),
						   QStringLiteral("ENDERECO"),
						   QStringLiteral("CANAL"),
						   QStringLiteral("CARTAO"),
						   QStringLiteral("COMENTARIO")}));

	IoCircuit::Plan plan = IoCircuit::plan(list, QStringList{id},
					       QStringLiteral("A1"));

	REQUIRE(plan.jobs.count() == 1);
	CHECK(plan.isClean());
	CHECK(plan.jobs.at(0).io_index == 0);
	CHECK(plan.jobs.at(0).macro_path == MACRO);

	QStringList problems;
	const int written = IoCircuit::fill(table, plan, &problems);

	CHECK(problems.isEmpty());
	CHECK(written == 6);
	REQUIRE(table.rowCount() == 1);

	SECTION("as seis coisas que o ponto tem a dizer")
	{
		CHECK(table.value(0, QStringLiteral("MARCACAO"))
		      == QStringLiteral("BP1"));
		CHECK(table.value(0, QStringLiteral("DESCRICAO"))
		      == QStringLiteral("Bouton de marche"));
		CHECK(table.value(0, QStringLiteral("ENDERECO"))
		      == QStringLiteral("I0.0"));
		CHECK(table.value(0, QStringLiteral("CANAL"))
		      == QStringLiteral("I0.0"));
		CHECK(table.value(0, QStringLiteral("COMENTARIO"))
		      == QString::fromUtf8("na porta do quadro"));
	}

	SECTION("o cartão se nomeia, e a coluna da planilha só responde "
		"quando ele cala")
	{
			//The card was asked how it names itself: A1 wins over
			//whatever the sheet had asked for before the assignment.
		CHECK(table.value(0, QStringLiteral("CARTAO"))
		      == QStringLiteral("A1"));

		IoList outra;
		IoPoint sem_cartao = atribuido("BP2", "Arrêt", 1, "I0.1");
		sem_cartao.card = QStringLiteral("CLP-01");
		const QString outro_id = outra.append(sem_cartao);

		IoCircuit::Plan sem = IoCircuit::plan(outra,
						      QStringList{outro_id});
		REQUIRE(sem.jobs.count() == 1);
		CHECK(sem.jobs.at(0).values.value(QStringLiteral("CARTAO"))
		      == QStringLiteral("CLP-01"));
	}

	SECTION("a linha da tabela e o trabalho do plano se reconhecem")
	{
		REQUIRE(!plan.jobs.at(0).row_id.isEmpty());
		CHECK(table.row(0).id == plan.jobs.at(0).row_id);
		CHECK(table.indexOfId(plan.jobs.at(0).row_id) == 0);
	}
}

TEST_CASE("CU-11.4 — o macro decide o que quer: o que ele não declara "
	  "não é escrito, e o que só ele sabe fica no padrão dele",
	  "[iocircuit]")
{
	IoList list;
	IoPoint point = atribuido("BP1", "Bouton de marche", 0, "I0.0");
	const QString id = list.append(point);

	MacroParameterSet set;
	set.append(MacroParameter(QStringLiteral("MARCACAO"),
				  QStringLiteral("Repère"),
				  MacroParameterType::Text));

	MacroParameter secao(QStringLiteral("SECAO"),
			     QStringLiteral("Section"),
			     MacroParameterType::Text);
	secao.default_value = QStringLiteral("1,5");
	set.append(secao);

	CircuitTable table;
	table.setParameters(MACRO, set);

	IoCircuit::Plan plan = IoCircuit::plan(list, QStringList{id});
	QStringList problems;
	const int written = IoCircuit::fill(table, plan, &problems);

		//Two things the point knows are simply not asked for here, and
		//not being asked for is not a problem to report.
	CHECK(problems.isEmpty());
	CHECK(written == 1);
	CHECK(table.value(0, QStringLiteral("MARCACAO"))
	      == QStringLiteral("BP1"));

		//SECAO is the macro's own business: the point says nothing
		//about it, so the macro keeps the value it shipped with.
	CHECK(table.value(0, QStringLiteral("SECAO"))
	      == QStringLiteral("1,5"));

		//And a column no macro of this table declares does not exist.
	CHECK(!table.columns().contains(QStringLiteral("DESCRICAO")));
}

TEST_CASE("CU-11.4 — o nome da variável casa inteiro, e nunca por prefixo",
	  "[iocircuit]")
{
	CHECK(IoCircuit::keyForColumn(QStringLiteral("MARCACAO_DISJUNTOR"))
	      .isEmpty());
	CHECK(IoCircuit::keyForColumn(QStringLiteral("MARCACAO_RELE"))
	      .isEmpty());
	CHECK(IoCircuit::keyForColumn(QStringLiteral("DESCRICAO_RELE"))
	      .isEmpty());

	IoList list;
	IoPoint point = atribuido("BP1", "Bouton de marche", 0, "I0.0");
	const QString id = list.append(point);

	CircuitTable table;
	table.setParameters(MACRO,
			    parametros(QStringList{
					    QStringLiteral("MARCACAO_DISJUNTOR"),
					    QStringLiteral("MARCACAO_RELE")}));

	IoCircuit::Plan plan = IoCircuit::plan(list, QStringList{id});
	QStringList problems;

		//A library where both exist would get the same tag written into
		//both, and a wrong tag on the folio costs more than an empty one.
	CHECK(IoCircuit::fill(table, plan, &problems) == 0);
	CHECK(problems.isEmpty());
	CHECK(table.value(0, QStringLiteral("MARCACAO_DISJUNTOR")).isEmpty());
	CHECK(table.value(0, QStringLiteral("MARCACAO_RELE")).isEmpty());
}

TEST_CASE("CU-11.4 — a mesma variável em três idiomas, e com acento",
	  "[iocircuit]")
{
	SECTION("cada apelido devolve a sua chave")
	{
		CHECK(IoCircuit::keyForColumn(QStringLiteral("REPERE"))
		      == QStringLiteral("MARCACAO"));
		CHECK(IoCircuit::keyForColumn(QString::fromUtf8("Repère"))
		      == QStringLiteral("MARCACAO"));
		CHECK(IoCircuit::keyForColumn(QStringLiteral("tag"))
		      == QStringLiteral("MARCACAO"));
		CHECK(IoCircuit::keyForColumn(QString::fromUtf8("Descrição"))
		      == QStringLiteral("DESCRICAO"));
		CHECK(IoCircuit::keyForColumn(QStringLiteral("Fonction"))
		      == QStringLiteral("DESCRICAO"));
		CHECK(IoCircuit::keyForColumn(QStringLiteral("Adresse"))
		      == QStringLiteral("ENDERECO"));
		CHECK(IoCircuit::keyForColumn(QStringLiteral("Voie"))
		      == QStringLiteral("CANAL"));
		CHECK(IoCircuit::keyForColumn(QStringLiteral("Carte"))
		      == QStringLiteral("CARTAO"));
		CHECK(IoCircuit::keyForColumn(QStringLiteral("Commentaire"))
		      == QStringLiteral("COMENTARIO"));
	}

	SECTION("o que não é apelido de nada não é chave de nada")
	{
		CHECK(IoCircuit::keyForColumn(QStringLiteral("SECAO"))
		      .isEmpty());
		CHECK(IoCircuit::keyForColumn(QStringLiteral("TENSAO_COMANDO"))
		      .isEmpty());
		CHECK(IoCircuit::keyForColumn(QString()).isEmpty());
		CHECK(IoCircuit::keyForColumn(QStringLiteral("   ")).isEmpty());
	}

	SECTION("um macro que nomeia as variáveis em francês recebe igual")
	{
		IoList list;
		IoPoint point = atribuido("BP1", "Bouton de marche", 0, "I0.0");
		const QString id = list.append(point);

		CircuitTable table;
		table.setParameters(
				MACRO,
				parametros(QStringList{
						QStringLiteral("REPERE"),
						QStringLiteral("FONCTION"),
						QStringLiteral("VOIE")}));

		IoCircuit::Plan plan = IoCircuit::plan(list, QStringList{id});
		QStringList problems;

		CHECK(IoCircuit::fill(table, plan, &problems) == 3);
		CHECK(problems.isEmpty());
		CHECK(table.value(0, QStringLiteral("REPERE"))
		      == QStringLiteral("BP1"));
		CHECK(table.value(0, QStringLiteral("FONCTION"))
		      == QStringLiteral("Bouton de marche"));
		CHECK(table.value(0, QStringLiteral("VOIE"))
		      == QStringLiteral("I0.0"));
	}
}

TEST_CASE("CU-11.4 — o ponto que não desenha é dito pelo nome e pelo motivo",
	  "[iocircuit]")
{
	IoList list;

		//Assigned, with a macro: this one draws.
	const QString bom = list.append(
			atribuido("BP1", "Bouton de marche", 0, "I0.0"));

		//In no card at all.
	IoPoint solto;
	solto.tag = QStringLiteral("BP2");
	solto.connect_to = MACRO;
	const QString sem_canal = list.append(solto);

		//In a card, but nobody said what to connect.
	IoPoint mudo = atribuido("BP3", "Arrêt", 1, "I0.1");
	mudo.connect_to.clear();
	const QString sem_coluna = list.append(mudo);

		//In a card, and what it names has no insertion point.
	IoPoint elemento = atribuido("BP4", "Sécurité", 2, "I0.2");
	elemento.connect_to = QStringLiteral("contact_no.elmt");
	const QString sem_macro = list.append(elemento);

	IoCircuit::Plan plan = IoCircuit::plan(
			list,
			QStringList{bom, sem_canal, sem_coluna, sem_macro,
				    QStringLiteral("{nao-existe}")});

	REQUIRE(plan.jobs.count() == 1);
	CHECK(plan.jobs.at(0).point_id == bom);
	CHECK(!plan.isClean());
	REQUIRE(plan.rejected.count() == 4);

	SECTION("cada recusa com o seu motivo, na ordem em que foi pedida")
	{
		CHECK(plan.rejected.at(0).reason == IoCircuit::NotAssigned);
		CHECK(plan.rejected.at(0).point_id == sem_canal);
		CHECK(plan.rejected.at(1).reason == IoCircuit::NothingToDraw);
		CHECK(plan.rejected.at(1).point_id == sem_coluna);
		CHECK(plan.rejected.at(2).reason == IoCircuit::NotAMacro);
		CHECK(plan.rejected.at(2).point_id == sem_macro);
		CHECK(plan.rejected.at(3).reason == IoCircuit::PointNotFound);
	}

	SECTION("e o parágrafo diz os dois lados")
	{
		const QString text = plan.text();
		CHECK(text.contains(QStringLiteral("1")));
		CHECK(text.contains(QStringLiteral("BP2")));
		CHECK(text.contains(QStringLiteral("BP3")));
		CHECK(text.contains(QStringLiteral("BP4")));
		CHECK(text.contains(QStringLiteral("entrada-digital")));
	}

	SECTION("o motivo tem texto, e o não-motivo não tem")
	{
		for (const IoCircuit::Rejected &one : plan.rejected)
		{
			CHECK(!IoCircuit::refusalText(one.reason, one.label)
			       .isEmpty());
		}
		CHECK(IoCircuit::refusalText(IoCircuit::NoRefusal,
					     QStringLiteral("BP1")).isEmpty());
	}

	SECTION("um plano vazio diz que não vai gerar nada")
	{
		const IoCircuit::Plan nada = IoCircuit::plan(list,
							    QStringList());
		CHECK(nada.isEmpty());
		CHECK(nada.isClean());
		CHECK(nada.text().contains(QStringLiteral("Aucun")));
	}
}

TEST_CASE("CU-11.5 — o borne de campo é contado antes de qualquer desenho",
	  "[iocircuit]")
{
	IoList list;
	QStringList ids;

	for (int i = 0 ; i < 20 ; ++i)
	{
		IoPoint point = atribuido("BP", "Bouton", i, "I0.0");
		point.tag = QStringLiteral("BP") + QString::number(i + 1);
			//Twelve of the twenty reach the field through a terminal,
			//which is the number CU-11.5 asks about.
		point.needs_terminal = (i < 12);
		ids << list.append(point);
	}

	IoCircuit::Plan plan = IoCircuit::plan(list, ids);

	REQUIRE(plan.jobs.count() == 20);
	CHECK(plan.isClean());
	CHECK(plan.terminals() == 12);

	for (int i = 0 ; i < 20 ; ++i)
	{
		CHECK(plan.jobs.at(i).needs_terminal == (i < 12));
	}

	CHECK(plan.text().contains(QStringLiteral("12")));
}

TEST_CASE("CU-11.4 — oito pontos, oito linhas, e o macro de um não invade "
	  "a linha do outro",
	  "[iocircuit]")
{
	IoList list;
	QStringList ids;

	for (int i = 0 ; i < 8 ; ++i)
	{
		IoPoint point = atribuido("BP", "Bouton", i, "I0.0");
		point.tag = QStringLiteral("BP") + QString::number(i + 1);
		point.channel = QStringLiteral("I0.") + QString::number(i);
		point.address = point.channel;
			//Alternating, so that the first-seen order of the macros
			//is the interesting one and not the trivial one.
		if (i % 2)
		{
			point.connect_to = OUTRO;
		}
		ids << list.append(point);
	}

	CircuitTable table;
	table.setParameters(MACRO,
			    parametros(QStringList{QStringLiteral("MARCACAO"),
						   QStringLiteral("DESCRICAO")}));
	table.setParameters(OUTRO,
			    parametros(QStringList{QStringLiteral("MARCACAO"),
						   QStringLiteral("ENDERECO")}));

	IoCircuit::Plan plan = IoCircuit::plan(list, ids);
	REQUIRE(plan.jobs.count() == 8);

	CHECK(plan.macroPaths()
	      == QStringList{MACRO, OUTRO});

	QStringList problems;
	const int written = IoCircuit::fill(table, plan, &problems);

	CHECK(problems.isEmpty());
	REQUIRE(table.rowCount() == 8);

		//Three columns exist in the table, because the two macros
		//together declare three; each row only takes its own two.
	CHECK(table.columns().count() == 3);
	CHECK(written == 16);

	SECTION("a linha de um macro não recebe a coluna do outro")
	{
		CHECK(table.value(0, QStringLiteral("DESCRICAO"))
		      == QStringLiteral("Bouton"));
		CHECK(table.value(0, QStringLiteral("ENDERECO")).isEmpty());
		CHECK(table.isInert(0, QStringLiteral("ENDERECO")));

		CHECK(table.value(1, QStringLiteral("ENDERECO"))
		      == QStringLiteral("I0.1"));
		CHECK(table.value(1, QStringLiteral("DESCRICAO")).isEmpty());
		CHECK(table.isInert(1, QStringLiteral("DESCRICAO")));
	}

	SECTION("cada trabalho conhece a linha que ficou sendo dele")
	{
		for (int i = 0 ; i < 8 ; ++i)
		{
			const QString row_id = plan.jobs.at(i).row_id;
			REQUIRE(!row_id.isEmpty());
			CHECK(table.indexOfId(row_id) == i);
			CHECK(table.value(i, QStringLiteral("MARCACAO"))
			      == QStringLiteral("BP")
				 + QString::number(i + 1));
		}
	}
}

TEST_CASE("CU-11.4 — o que a coluna Connecter à tem de nomear",
	  "[iocircuit]")
{
	CHECK(IoCircuit::isMacroPath(QStringLiteral("partida.qetmak")));
	CHECK(IoCircuit::isMacroPath(QStringLiteral("C:/l/partida.QETMAK")));
	CHECK(IoCircuit::isMacroPath(QStringLiteral(" partida.qetmak  ")));

	CHECK(!IoCircuit::isMacroPath(QStringLiteral("contact_no.elmt")));
	CHECK(!IoCircuit::isMacroPath(QStringLiteral("partida.qetmak.bak")));
	CHECK(!IoCircuit::isMacroPath(QStringLiteral("partida")));
	CHECK(!IoCircuit::isMacroPath(QString()));
}
