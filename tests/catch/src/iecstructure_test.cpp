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
#include <QDomDocument>

#include "../../../sources/autoNum/iecstructure.h"
#include "../../../sources/diagramcontext.h"
#include "qt_catch_tostring.h"

TEST_CASE("CU-10.1 — désactivée, la structure ne touche à rien", "[iec]")
{
	// This is the case that protects production, and the first one to run on
	// any change to T10: a tag written before the norm existed reads back as
	// itself, and the two other parts stay empty.
	const IecStructure plain = IecStructure::fromTag(QStringLiteral("K3"));
	CHECK(plain.product == QStringLiteral("K3"));
	CHECK(plain.plant.isEmpty());
	CHECK(plain.location.isEmpty());

	// Displayed short, it is the tag the drawing always had.
	CHECK(plain.toShortTag(false) == QStringLiteral("K3"));
	CHECK(plain.toShortTag() == QStringLiteral("-K3"));

	// A tag with a dash inside it - the second contactor of a pair - is not
	// mistaken for a structure.
	const IecStructure dashed = IecStructure::fromTag(QStringLiteral("K3-1"));
	CHECK(dashed.product == QStringLiteral("K3-1"));
	CHECK(dashed.plant.isEmpty());
	CHECK(dashed.location.isEmpty());

	// And a tag that is only a number, or empty, does not become something else.
	CHECK(IecStructure::fromTag(QStringLiteral("1")).product == QStringLiteral("1"));
	CHECK(IecStructure::fromTag(QString()).isEmpty());
}

TEST_CASE("CU-10.2 — l'héritage en cascade : personne ne tape =CT1 deux fois", "[iec]")
{
	const IecStructure project(QStringLiteral("CT1"), QString(), QString());
	const IecStructure folio(QString(), QStringLiteral("A1"), QString());
	const IecStructure component(QString(), QString(), QStringLiteral("K3"));

	const IecStructure on_folio = IecStructure::inherit(project, folio);
	CHECK(on_folio.plant == QStringLiteral("CT1"));
	CHECK(on_folio.location == QStringLiteral("A1"));

	const IecStructure resolved = IecStructure::inherit(on_folio, component);
	CHECK(resolved.plant == QStringLiteral("CT1"));
	CHECK(resolved.location == QStringLiteral("A1"));
	CHECK(resolved.product == QStringLiteral("K3"));

	// The whole point: the component shows the full tag without anybody having
	// typed =CT1 or +A1 on it. Without inheritance the norm is only extra work.
	CHECK(resolved.toFullTag() == QStringLiteral("=CT1+A1-K3"));
}

TEST_CASE("CU-10.3 — la surcharge explicite ne touche que celui qui la porte", "[iec]")
{
	const IecStructure project(QStringLiteral("CT1"), QString(), QString());
	const IecStructure folio(QString(), QStringLiteral("A1"), QString());
	const IecStructure on_folio = IecStructure::inherit(project, folio);

	// One component moved to another cabinet: it overrides its + and keeps
	// inheriting the = of the project.
	IecStructure moved(QString(), QStringLiteral("A2"), QStringLiteral("K3"));
	const IecStructure moved_resolved = IecStructure::inherit(on_folio, moved);
	CHECK(moved_resolved.toFullTag() == QStringLiteral("=CT1+A2-K3"));

	// Its neighbour on the same folio did not move.
	const IecStructure neighbour(QString(), QString(), QStringLiteral("K4"));
	CHECK(IecStructure::inherit(on_folio, neighbour).toFullTag()
	      == QStringLiteral("=CT1+A1-K4"));
}

TEST_CASE("CU-10.4 — court ou complet change l'affichage, pas la donnée", "[iec]")
{
	const IecStructure resolved(QStringLiteral("CT1"),
				    QStringLiteral("A1"),
				    QStringLiteral("K3"));

	CHECK(resolved.toFullTag() == QStringLiteral("=CT1+A1-K3"));
	CHECK(resolved.toShortTag() == QStringLiteral("-K3"));
	CHECK(resolved.toShortTag(false) == QStringLiteral("K3"));

	// The two forms are two readings of the same three fields: saving in one
	// and reopening in the other loses nothing.
	const IecStructure from_full = IecStructure::fromTag(resolved.toFullTag());
	CHECK(from_full == resolved);

	// A product part already carrying its dash is not given a second one.
	const IecStructure with_dash(QStringLiteral("CT1"),
				     QStringLiteral("A1"),
				     QStringLiteral("-K3"));
	CHECK(with_dash.toFullTag() == QStringLiteral("=CT1+A1-K3"));
	CHECK(with_dash.toShortTag() == QStringLiteral("-K3"));
}

TEST_CASE("CU-10.5 — allumer la structure en cours de route ne perd aucun repère", "[iec]")
{
	// Forty components already numbered, with no structure at all. Turning the
	// norm on must leave every tag where it is, with = and + empty, waiting to
	// be filled - and then inherited.
	const QStringList existing = { QStringLiteral("K1"), QStringLiteral("K2"),
				       QStringLiteral("Q1"), QStringLiteral("MTR1"),
				       QStringLiteral("X1-2") };

	for (const QString &tag : existing)
	{
		const IecStructure structure = IecStructure::fromTag(tag);
		CHECK(structure.product == tag);
		CHECK(structure.plant.isEmpty());
		CHECK(structure.location.isEmpty());
		// Nothing renumbered, nothing lost: the short tag is what the drawing
		// already showed.
		CHECK(structure.toShortTag(false) == tag);
	}

	// And once the project fills in its =, the components inherit it without
	// any of them being edited.
	const IecStructure project(QStringLiteral("CT1"), QString(), QString());
	const IecStructure first = IecStructure::inherit(
		project, IecStructure::fromTag(existing.first()));
	CHECK(first.toFullTag() == QStringLiteral("=CT1-K1"));
}

TEST_CASE("IecStructure — les champs où les trois parties vivent", "[iec]")
{
	// The specification of T10 said the `-` part was the `designation` field.
	// It is not: `designation` is what QElectroTech labels "Numéro d'article",
	// a commercial field. The `-` part is the tag itself, which is `label`.
	// Writing the structure into `designation` would have quietly destroyed
	// the article number of every component of a project that turned the
	// norm on.
	CHECK(IecStructure::plantKey() == QStringLiteral("plant"));
	CHECK(IecStructure::locationKey() == QStringLiteral("location"));
	CHECK(IecStructure::productKey() == QStringLiteral("label"));
	CHECK(IecStructure::productKey() != QStringLiteral("designation"));
	CHECK(IecStructure::folioLocationKey() == QStringLiteral("locmach"));
}

TEST_CASE("CU-10.1 — desligada, a estrutura não muda nada", "[iec]")
{
	IecStructureSettings settings;

	SECTION("o padrão é desligada, e é o que protege projeto entregue")
	{
		CHECK_FALSE(settings.enabled);
		CHECK(settings.display == IecTagDisplay::Short);
	}

	SECTION("desligada, a tag que sai é exatamente a que entrou")
	{
			//Nem "quase igual": igual. Com a estrutura desligada, o folio
			//pode carregar função e localização e nada disso aparece.
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("A1"),
					 QString());
		const IecStructure element(QString(), QString(),
					   QStringLiteral("K3"));
		CHECK(settings.displayedTag(folio, element) == QStringLiteral("K3"));

			//Inclusive quando o próprio componente declara função e
			//localização: desligado é desligado.
		const IecStructure declared(QStringLiteral("CT9"),
					    QStringLiteral("B2"),
					    QStringLiteral("K3"));
		CHECK(settings.displayedTag(folio, declared) == QStringLiteral("K3"));
	}
}

TEST_CASE("CU-10.2 e CU-10.3 — ligada, a tag herda e aparece", "[iec]")
{
	IecStructureSettings settings;
	settings.enabled = true;

	const IecStructure folio(QStringLiteral("CT1"),
				 QStringLiteral("A1"),
				 QString());

	SECTION("curta mostra só o produto, com o traço da norma")
	{
		settings.display = IecTagDisplay::Short;
		const IecStructure element(QString(), QString(),
					   QStringLiteral("K3"));
		CHECK(settings.displayedTag(folio, element) == QStringLiteral("-K3"));
	}

	SECTION("completa mostra as três partes, herdadas do folio")
	{
		settings.display = IecTagDisplay::Full;
		const IecStructure element(QString(), QString(),
					   QStringLiteral("K3"));
		CHECK(settings.displayedTag(folio, element) ==
		      QStringLiteral("=CT1+A1-K3"));
	}

	SECTION("o que o componente declara vence o que o folio dá, campo a campo")
	{
		settings.display = IecTagDisplay::Full;
			//Componente movido para outro armário: sobrescreve o + e
			//continua herdando o = do folio.
		const IecStructure element(QString(), QStringLiteral("B2"),
					   QStringLiteral("K3"));
		CHECK(settings.displayedTag(folio, element) ==
		      QStringLiteral("=CT1+B2-K3"));
	}
}

TEST_CASE("o interruptor da estrutura viaja no .qet", "[iec]")
{
	IecStructureSettings settings;
	settings.enabled = true;
	settings.display = IecTagDisplay::Full;

	QDomDocument document;
	const QDomElement element = settings.toXml(document);

	SECTION("ida e volta preserva as duas decisões")
	{
		IecStructureSettings read;
		read.fromXml(element);
		CHECK(read == settings);
		CHECK(read.enabled);
		CHECK(read.display == IecTagDisplay::Full);
	}

	SECTION("projeto antigo, sem o elemento, abre com a estrutura desligada")
	{
			//É a leitura tolerante da regra do fork: campo ausente assume o
			//padrão e não gera erro.
		IecStructureSettings read;
		read.enabled = true;
		read.fromXml(QDomElement());
			//Nada foi lido, então nada mudou — e o padrão de um objeto novo
			//é desligado.
		IecStructureSettings fresh;
		fresh.fromXml(QDomElement());
		CHECK_FALSE(fresh.enabled);
	}

	SECTION("exibição desconhecida cai na curta em vez de recusar o arquivo")
	{
		QDomDocument other;
		QDomElement broken = other.createElement(
					IecStructureSettings::xmlTagName());
		broken.setAttribute(QStringLiteral("enabled"), QStringLiteral("true"));
		broken.setAttribute(QStringLiteral("display"),
				    QStringLiteral("holografica"));
		IecStructureSettings read;
		read.fromXml(broken);
		CHECK(read.enabled);
		CHECK(read.display == IecTagDisplay::Short);
	}

	SECTION("as duas exibições têm nome traduzido")
	{
		for (IecTagDisplay display : {IecTagDisplay::Short, IecTagDisplay::Full}) {
			CHECK_FALSE(IecStructureSettings::translatedDisplay(display).isEmpty());
			CHECK(IecStructureSettings::displayFromString(
				      IecStructureSettings::displayToString(display)) == display);
		}
	}
}

TEST_CASE("compor duas vezes dá o mesmo que compor uma", "[iec]")
{
		//Isto não é curiosidade matemática: em 21/08/2026 o Renan encontrou a
		//composição sendo GRAVADA no campo da tag, porque ela morava dentro do
		//actualLabel() e seis lugares do element.cpp escrevem o resultado dele
		//de volta no dado. A composição ser idempotente é o que impediu a
		//corrupção de crescer a cada passada — o campo ficou errado uma vez, e
		//não vinte. O conserto foi tirar a composição de lá; este teste guarda
		//a propriedade que limitou o estrago, para o dia em que alguém compor
		//no lugar errado de novo.
	IecStructureSettings settings;
	settings.enabled = true;

	const IecStructure folio(QStringLiteral("CT1"),
				 QStringLiteral("A1"),
				 QString());

	for (IecTagDisplay display : {IecTagDisplay::Short, IecTagDisplay::Full})
	{
		settings.display = display;
		const IecStructure element(QString(), QString(), QStringLiteral("Q1"));

		const QString uma_vez = settings.displayedTag(folio, element);
		const QString duas = settings.displayedTag(
					folio, IecStructure::fromTag(uma_vez));
		const QString tres = settings.displayedTag(
					folio, IecStructure::fromTag(duas));

		CHECK(duas == uma_vez);
		CHECK(tres == uma_vez);
	}
}

TEST_CASE("a tag digitada com separador não é lida duas vezes", "[iec]")
{
		//Projeto onde alguém escreveu "=CT1+A1-Q1" à mão no campo. Lido como
		//produto inteiro, a composição sairia "=CT1+A1-=CT1+A1-Q1".
	const IecStructure lida = IecStructure::fromTag(
				QStringLiteral("=CT1+A1-Q1"));
	CHECK(lida.plant == QStringLiteral("CT1"));
	CHECK(lida.location == QStringLiteral("A1"));
	CHECK(lida.product == QStringLiteral("Q1"));
	CHECK(lida.toFullTag() == QStringLiteral("=CT1+A1-Q1"));
}

TEST_CASE("a leitura da estrutura mora num lugar só", "[iec]")
{
	// Por que este caso existe: a mesma leitura estava escrita duas vezes —
	// em Element::composedLabel, que compõe a tag DESENHADA, e no diálogo das
	// configurações, cuja previsão promete mostrar o que o desenho vai fazer.
	// Idênticas na hora em que foram escritas, e livres para divergir depois.
	// Uma previsão que divergiu é pior que previsão nenhuma: ela é lida como
	// garantia. Agora os dois chamam estas duas funções, e este caso as prova
	// contra a DiagramContext de verdade.

	SECTION("o folio guarda o + em locmach, e não na chave do componente")
	{
		// A assimetria que o cabeçalho de IecStructure marca como o único
		// ponto que vale ter num lugar só. Um folio que trouxesse o + na
		// chave do componente não daria nada para herdar.
		DiagramContext folio;
		folio.addValue(QStringLiteral("plant"),   QStringLiteral("CT1"));
		folio.addValue(QStringLiteral("locmach"), QStringLiteral("QCM"));
		folio.addValue(QStringLiteral("location"), QStringLiteral("NAO-E-DAQUI"));

		const IecStructure lido = IecStructure::fromFolioInformation(folio);
		CHECK(lido.plant    == QStringLiteral("CT1"));
		CHECK(lido.location == QStringLiteral("QCM"));
		CHECK(lido.product.isEmpty());
	}

	SECTION("folio vazio não inventa nada")
	{
		const IecStructure lido = IecStructure::fromFolioInformation(DiagramContext());
		CHECK(lido.isEmpty());
	}

	SECTION("o componente sem = nem + é a tag e nada mais")
	{
		DiagramContext elemento;
		const IecStructure lido =
				IecStructure::fromElementInformation(QStringLiteral("K3"), elemento);
		CHECK(lido.product == QStringLiteral("K3"));
		CHECK(lido.plant.isEmpty());
		CHECK(lido.location.isEmpty());
	}

	SECTION("o que o componente declara entra por cima da tag, campo a campo")
	{
		DiagramContext elemento;
		elemento.addValue(QStringLiteral("location"), QStringLiteral("A2"));

		const IecStructure lido =
				IecStructure::fromElementInformation(QStringLiteral("=CT1+A1-K3"), elemento);
		// O + do componente vence...
		CHECK(lido.location == QStringLiteral("A2"));
		// ...e o = que ele não declarou continua sendo o que a tag dizia.
		CHECK(lido.plant   == QStringLiteral("CT1"));
		CHECK(lido.product == QStringLiteral("K3"));
	}

	SECTION("campo vazio no componente não apaga o que a tag trazia")
	{
		DiagramContext elemento;
		elemento.addValue(QStringLiteral("plant"),    QString());
		elemento.addValue(QStringLiteral("location"), QString());

		const IecStructure lido =
				IecStructure::fromElementInformation(QStringLiteral("=CT1+A1-K3"), elemento);
		CHECK(lido.plant    == QStringLiteral("CT1"));
		CHECK(lido.location == QStringLiteral("A1"));
	}

	SECTION("a cadeia inteira, a mesma que o desenho e a previsão percorrem")
	{
		DiagramContext folio;
		folio.addValue(QStringLiteral("plant"),   QStringLiteral("CT1"));
		folio.addValue(QStringLiteral("locmach"), QStringLiteral("QCM"));

		DiagramContext elemento;   // herda tudo do folio

		IecStructureSettings ligada;
		ligada.enabled = true;
		ligada.display = IecTagDisplay::Full;

		CHECK(ligada.displayedTag(
				  IecStructure::fromFolioInformation(folio),
				  IecStructure::fromElementInformation(QStringLiteral("K3"), elemento))
		      == QStringLiteral("=CT1+QCM-K3"));

		// E desligada, a mesma cadeia devolve a tag intocada — é o que faz o
		// interruptor ser reversível.
		IecStructureSettings desligada;
		desligada.enabled = false;
		CHECK(desligada.displayedTag(
				  IecStructure::fromFolioInformation(folio),
				  IecStructure::fromElementInformation(QStringLiteral("K3"), elemento))
		      == QStringLiteral("K3"));
	}
}

TEST_CASE("CU-10.7 — elemento sem tag não ganha uma feita de prefixo", "[iec]")
{
	// Achado medindo o projeto de 14 folhas, e não no papel: ligada a norma
	// no modo completo, 99 textos do desenho passaram a dizer "=CT1+QCM"
	// sozinho — a instalação e o local de algo que nunca foi nomeado. Na tela
	// isso se lê como designação de verdade, e são 99 pontos onde o
	// projetista procuraria um componente que não existe.

	IecStructureSettings ligada;
	ligada.enabled = true;

	DiagramContext folio;
	folio.addValue(QStringLiteral("plant"),   QStringLiteral("CT1"));
	folio.addValue(QStringLiteral("locmach"), QStringLiteral("QCM"));
	const IecStructure do_folio = IecStructure::fromFolioInformation(folio);

	SECTION("no modo completo, elemento de tag vazia não mostra nada")
	{
		ligada.display = IecTagDisplay::Full;
		const IecStructure sem_tag = IecStructure::fromElementInformation(
					QString(), DiagramContext());
		CHECK(ligada.displayedTag(do_folio, sem_tag).isEmpty());
	}

	SECTION("no modo curto também — aqui já estava certo, e fica preso")
	{
		ligada.display = IecTagDisplay::Short;
		const IecStructure sem_tag = IecStructure::fromElementInformation(
					QString(), DiagramContext());
		CHECK(ligada.displayedTag(do_folio, sem_tag).isEmpty());
	}

	SECTION("quem tem tag continua compondo — a guarda não engoliu o recurso")
	{
		ligada.display = IecTagDisplay::Full;
		const IecStructure com_tag = IecStructure::fromElementInformation(
					QStringLiteral("K3"), DiagramContext());
		CHECK(ligada.displayedTag(do_folio, com_tag)
		      == QStringLiteral("=CT1+QCM-K3"));
	}

	SECTION("a guarda está no displayedTag, não no toFullTag")
	{
		// Um folio tem = e + e nenhum produto, e a tag dele é legítima:
		// "=CT1+QCM" é o nome do quadro. Se a guarda estivesse dentro de
		// toFullTag(), o carimbo perderia isso junto.
		CHECK(do_folio.toFullTag() == QStringLiteral("=CT1+QCM"));
		CHECK(do_folio.product.isEmpty());
	}
}
