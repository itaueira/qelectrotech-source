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
#include "../../../sources/options/optiontree.h"
#include "../../../sources/options/projectoption.h"
#include "qt_catch_tostring.h"

#include <QDomDocument>
#include <QDomElement>
#include <QString>
#include <QStringList>

namespace
{
		/// @return an option as the panel will hand it over
	ProjectOption opcao(const char *nome, const char *descricao = "")
	{
		return ProjectOption(QString::fromUtf8(nome),
				     QString::fromUtf8(descricao));
	}

		/// @return an option element written by hand, as a file holds it
	QDomElement elemento(QDomDocument &documento,
			     const char *uuid,
			     const char *nome,
			     const char *pai = nullptr,
			     bool ligada = false)
	{
		QDomElement e = documento.createElement(ProjectOption::tagName());
		e.setAttribute(QStringLiteral("uuid"), QString::fromUtf8(uuid));
		e.setAttribute(QStringLiteral("name"), QString::fromUtf8(nome));
		if (pai) {
			e.setAttribute(QStringLiteral("parent"),
				       QString::fromUtf8(pai));
		}
		if (ligada) {
			e.setAttribute(QStringLiteral("on"),
				       QStringLiteral("true"));
		}
		return e;
	}
}

TEST_CASE("T36 — a árvore guarda as configurações da família e quem refina quem",
	  "[opcao]")
{
	OptionTree arvore;
	CHECK(arvore.isEmpty());
	CHECK(arvore.count() == 0);

	const QString bidirecional = arvore.append(
				opcao("Bidirectionnel",
				      "Marche avant et marche arrière"));
	REQUIRE_FALSE(bidirecional.isEmpty());
	CHECK_FALSE(arvore.isEmpty());

	ProjectOption freio = opcao("Avec frein");
	freio.parent_uuid = bidirecional;
	const QString com_freio = arvore.append(freio);
	REQUIRE_FALSE(com_freio.isEmpty());

	const QString sinaleiro = arvore.append(opcao("Voyant de marche"));
	REQUIRE_FALSE(sinaleiro.isEmpty());

	SECTION("a árvore responde por níveis, e o caminho se lê como frase")
	{
		CHECK(arvore.count() == 3);
		CHECK(arvore.rootUuids()
		      == QStringList({bidirecional, sinaleiro}));
		CHECK(arvore.childUuids(bidirecional)
		      == QStringList({com_freio}));
		CHECK(arvore.depth(bidirecional) == 0);
		CHECK(arvore.depth(com_freio) == 1);
		CHECK(arvore.ancestorUuids(com_freio)
		      == QStringList({bidirecional}));
		CHECK(arvore.displayPath(com_freio)
		      == QString::fromUtf8("Bidirectionnel / Avec frein"));
	}

	SECTION("a descrição fica guardada ao lado do nome")
	{
		CHECK(arvore.option(bidirecional).description
		      == QString::fromUtf8("Marche avant et marche arrière"));
	}

	SECTION("duas opções de mesmo nível não podem ter o mesmo nome")
	{
		QString erro;
		CHECK(arvore.append(opcao("bidirectionnel"), &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
		CHECK(arvore.count() == 3);

			//The same name one level down is another switch, and is
			//accepted: it is read inside its parent.
		ProjectOption homonima = opcao("Voyant de marche");
		homonima.parent_uuid = bidirecional;
		CHECK_FALSE(arvore.append(homonima).isEmpty());
	}

	SECTION("uma opção sem nome é recusada, porque ninguém a desligaria")
	{
		QString erro;
		CHECK(arvore.append(opcao("   "), &erro).isEmpty());
		CHECK_FALSE(erro.isEmpty());
	}

	SECTION("apagar uma opção leva embora o que ela refina")
	{
		QStringList foram;
		REQUIRE(arvore.remove(bidirecional, &foram));
		CHECK(foram.count() == 2);
		CHECK(foram.contains(com_freio));
		CHECK(arvore.count() == 1);
		CHECK(arvore.indexOfUuid(com_freio) < 0);
	}

	SECTION("renomear e aninhar passam pela mesma porta")
	{
		ProjectOption mudada = arvore.option(sinaleiro);
		mudada.name = QString::fromUtf8("Voyant de défaut");
		mudada.parent_uuid = bidirecional;
		REQUIRE(arvore.update(mudada));
		CHECK(arvore.depth(sinaleiro) == 1);
		CHECK(arvore.displayPath(sinaleiro)
		      == QString::fromUtf8("Bidirectionnel / Voyant de défaut"));

			//Handing back what is already there is not a failure and
			//not a change: it is what keeps a dialogue opened and
			//closed from marking the project modified.
		QString erro;
		CHECK_FALSE(arvore.update(arvore.option(sinaleiro), &erro));
		CHECK(erro.isEmpty());
	}

	SECTION("uma opção não pode ser colocada dentro do que ela contém")
	{
		ProjectOption laco = arvore.option(bidirecional);
		laco.parent_uuid = com_freio;
		QString erro;
		CHECK_FALSE(arvore.update(laco, &erro));
		CHECK_FALSE(erro.isEmpty());
		CHECK(arvore.depth(bidirecional) == 0);
	}
}

TEST_CASE("T36 — ligar não é o mesmo que valer, e é a sub-opção que mostra a diferença",
	  "[opcao]")
{
	OptionTree arvore;
	const QString bidirecional = arvore.append(opcao("Bidirectionnel"));
	ProjectOption freio = opcao("Avec frein");
	freio.parent_uuid = bidirecional;
	const QString com_freio = arvore.append(freio);
	const QString sinaleiro = arvore.append(opcao("Voyant de marche"));

	CHECK(arvore.activeUuids().isEmpty());

	REQUIRE(arvore.setSwitchedOn(bidirecional, true));
	REQUIRE(arvore.setSwitchedOn(com_freio, true));
	CHECK(arvore.activeUuids() == QStringList({bidirecional, com_freio}));
	CHECK(arvore.activeNames()
	      == QStringList({QString::fromUtf8("Bidirectionnel"),
			      QString::fromUtf8("Avec frein")}));

	SECTION("desligar o pai tira a sub-opção do desenho e não do projeto")
	{
		REQUIRE(arvore.setSwitchedOn(bidirecional, false));

			//This is the whole point of storing the two answers. The
			//brake is no longer drawn, and yet the person's choice is
			//still there: turning the parent back on brings it back
			//without anything being regenerated, which is what the
			//task asks for in its third line.
		CHECK_FALSE(arvore.isActive(com_freio));
		CHECK(arvore.isSwitchedOn(com_freio));
		CHECK(arvore.activeUuids().isEmpty());

		REQUIRE(arvore.setSwitchedOn(bidirecional, true));
		CHECK(arvore.isActive(com_freio));
	}

	SECTION("o conjunto ativo sai na ordem da árvore, pais antes de filhos")
	{
		REQUIRE(arvore.setSwitchedOn(sinaleiro, true));
		CHECK(arvore.activeUuids()
		      == QStringList({bidirecional, com_freio, sinaleiro}));
	}

	SECTION("ligar o que já está ligado não muda nada")
	{
		CHECK_FALSE(arvore.setSwitchedOn(bidirecional, true));
		CHECK_FALSE(arvore.setSwitchedOn(QStringLiteral("inexistente"),
						 true));
	}
}

TEST_CASE("T36 — a árvore volta do arquivo igual ao que entrou, ligadas inclusive",
	  "[opcao]")
{
	OptionTree arvore;
	const QString bidirecional = arvore.append(
				opcao("Bidirectionnel",
				      "Marche avant et marche arrière"));
	ProjectOption freio = opcao("Avec frein", "Frein sur l'arbre moteur");
	freio.parent_uuid = bidirecional;
	const QString com_freio = arvore.append(freio);
	arvore.append(opcao("Voyant de marche"));
	REQUIRE(arvore.setSwitchedOn(bidirecional, true));
	REQUIRE(arvore.setSwitchedOn(com_freio, true));

	QDomDocument documento;
	const QDomElement escrita = arvore.toXml(documento);
	documento.appendChild(escrita);

	OptionTree lida;
	REQUIRE(lida.fromXml(documento.documentElement()));
	CHECK(lida == arvore);

		//The half of CU-36.8 this step closes: what is switched on comes
		//back as it was left, and by the same uuids - so a difference
		//recorded against a set of options still finds its set.
	CHECK(lida.activeUuids() == arvore.activeUuids());
	CHECK(lida.isSwitchedOn(com_freio));
	CHECK(lida.option(com_freio).description
	      == QString::fromUtf8("Frein sur l'arbre moteur"));

	SECTION("um elemento que não é nosso não é lido como se fosse")
	{
		QDomDocument outro;
		QDomElement raiz = outro.createElement(
					QStringLiteral("location_tree"));
		outro.appendChild(raiz);
		OptionTree vazia;
		CHECK_FALSE(vazia.fromXml(outro.documentElement()));
		CHECK(vazia.isEmpty());
	}

	SECTION("um elemento nulo é o projeto sem opção nenhuma")
	{
			//This is what every project written before this feature
			//existed hands over, and it has to mean "no option" and
			//never "broken file".
		OptionTree vazia;
		CHECK_FALSE(vazia.fromXml(QDomElement()));
		CHECK(vazia.isEmpty());
		CHECK(vazia.activeUuids().isEmpty());
	}

	SECTION("o que está desligado não deixa vestígio no arquivo")
	{
		OptionTree desligada;
		desligada.append(opcao("Bidirectionnel"));
		QDomDocument doc;
		QDomElement raiz = desligada.toXml(doc);
		doc.appendChild(raiz);

			//Asked of the attribute and not of the serialized text:
			//"description=" holds "on=" inside it, and a search over
			//the string would answer about the wrong attribute.
		CHECK_FALSE(raiz.firstChildElement(ProjectOption::tagName())
			    .hasAttribute(QStringLiteral("on")));
	}
}

TEST_CASE("T36 — o arquivo pode guardar o que a árvore não aceita, e a leitura conserta",
	  "[opcao]")
{
	QDomDocument documento;
	QDomElement raiz = documento.createElement(OptionTree::tagName());
	documento.appendChild(raiz);

	SECTION("um pai que não está no arquivo vira opção de topo")
	{
		raiz.appendChild(elemento(documento, "A", "Bidirectionnel"));
		raiz.appendChild(elemento(documento, "B", "Avec frein",
					  "nao-existe"));

		OptionTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 2);
		CHECK(arvore.depth(QString("B")) == 0);
	}

	SECTION("uma opção que é o próprio pai vira opção de topo")
	{
		raiz.appendChild(elemento(documento, "A", "Bidirectionnel", "A"));

		OptionTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 1);
		CHECK(arvore.depth(QString("A")) == 0);
	}

	SECTION("duas opções irmãs com o mesmo nome são numeradas à parte")
	{
		raiz.appendChild(elemento(documento, "A", "Bidirectionnel"));
		raiz.appendChild(elemento(documento, "B", "Bidirectionnel"));

		OptionTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 2);
		CHECK(arvore.option(QString("A")).name
		      == QString::fromUtf8("Bidirectionnel"));
		CHECK(arvore.option(QString("B")).name
		      == QString::fromUtf8("Bidirectionnel (2)"));
	}

	SECTION("uma opção sem nome recebe um, para poder ser desligada")
	{
		raiz.appendChild(elemento(documento, "A", ""));

		OptionTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 1);
		CHECK_FALSE(arvore.option(QString("A")).name.isEmpty());
	}

	SECTION("duas opções com o mesmo uuid não viram uma só")
	{
		raiz.appendChild(elemento(documento, "A", "Bidirectionnel"));
		raiz.appendChild(elemento(documento, "A", "Voyant de marche"));

		OptionTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 2);
	}

	SECTION("um laço entre duas opções é cortado, e as duas sobrevivem")
	{
		raiz.appendChild(elemento(documento, "A", "Bidirectionnel", "B"));
		raiz.appendChild(elemento(documento, "B", "Avec frein", "A"));

		OptionTree arvore;
		REQUIRE(arvore.fromXml(documento.documentElement()));
		CHECK(arvore.count() == 2);

			//Whichever link was cut, no walk of the tree may loop -
			//and the two options are still there to be read.
		CHECK(arvore.rootUuids().count() >= 1);
		CHECK(arvore.depth(QString("A")) >= 0);
		CHECK(arvore.depth(QString("B")) >= 0);
	}
}
