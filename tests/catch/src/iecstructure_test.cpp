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
#include "../../../sources/location/locationtree.h"
#include "../../../sources/qetinformation.h"
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

	// A tag that is only a number is a connection and not a product: the `-` of
	// the norm marks a product, and terminal 1 is a connection. What the drawing
	// shows does not change - and that is what is checked here. CU-10.10 has the
	// whole case, and the measurement that led to it.
	const IecStructure number = IecStructure::fromTag(QStringLiteral("1"));
	CHECK(number.product.isEmpty());
	CHECK(number.connection == QStringLiteral("1"));
	CHECK(number.toShortTag(false) == QStringLiteral("1"));
	CHECK(number.toShortTag() == QStringLiteral("1"));
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

	SECTION("as três exibições têm nome traduzido")
	{
		for (IecTagDisplay display : {IecTagDisplay::Short,
					      IecTagDisplay::Context,
					      IecTagDisplay::Full}) {
			CHECK_FALSE(IecStructureSettings::translatedDisplay(display).isEmpty());
			CHECK(IecStructureSettings::displayFromString(
				      IecStructureSettings::displayToString(display)) == display);
		}
	}
}

TEST_CASE("compor duas vezes dá o mesmo que compor uma", "[iec]")
{
		//Isto não é curiosidade matemática: em 21/08/2026 encontrou-se a
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

	for (IecTagDisplay display : {IecTagDisplay::Short,
				      IecTagDisplay::Context,
				      IecTagDisplay::Full})
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

			//O `true` é o CU-10.12: este trecho fala do campo Localização
			//vencendo a tag, e para isso o projeto tem de ter dito que aquele
			//campo é o `+` da norma.
		const IecStructure lido = IecStructure::fromElementInformation(
					QStringLiteral("=CT1+A1-K3"), elemento, true);
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
TEST_CASE("CU-10.8 — a folha já disse a planta e o local", "[iec]")
{
		//Em 21/08/2026 o projetista abriu o projeto convertido e disse que o esquema
		//tinha ficado poluído. A medição deu razão a ele: no modo completo o
		//projeto inteiro passou de 14295 para 17055 caracteres desenhados, +19,3%,
		//e a folha 05 — "Componentes - X1 a X4" — de 1105 para 1589, +44%. O modo por contexto
		//escreve o que o contexto ainda não disse — é a regra da própria
		//IEC 81346, que manda omitir prefixo óbvio pelo contexto e carregar a
		//designação completa na lista de material e no plano de bornes, que é
		//onde o QET já a tem.
	IecStructureSettings por_contexto;
	por_contexto.enabled = true;
	por_contexto.display = IecTagDisplay::Context;

	DiagramContext info_folio;
	info_folio.addValue(QStringLiteral("plant"),   QStringLiteral("CT1"));
	info_folio.addValue(QStringLiteral("locmach"), QStringLiteral("QCM"));
	const IecStructure folio = IecStructure::fromFolioInformation(info_folio);

	SECTION("quem herda a folha mostra só o produto")
	{
		const IecStructure herda = IecStructure::fromElementInformation(
					QStringLiteral("K3"), DiagramContext());
		CHECK(por_contexto.displayedTag(folio, herda)
		      == QStringLiteral("-K3"));
	}

	SECTION("quem está em outro armário mostra o local, e só ele")
	{
			//O `true` é o CU-10.12: neste projeto o campo Localização é lido
			//como o `+` da norma porque o projeto pediu.
		DiagramContext info;
		info.addValue(QStringLiteral("location"), QStringLiteral("QCM2"));
		const IecStructure outro = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info, true);
		CHECK(por_contexto.displayedTag(folio, outro)
		      == QStringLiteral("+QCM2-K3"));
	}

	SECTION("outra planta, mesmo local: aparece o = e não aparece o +")
	{
		DiagramContext info;
		info.addValue(QStringLiteral("plant"),    QStringLiteral("OUTRA"));
		info.addValue(QStringLiteral("location"), QStringLiteral("QCM"));
		const IecStructure fora = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info, true);
		CHECK(por_contexto.displayedTag(folio, fora)
		      == QStringLiteral("=OUTRA-K3"));
	}

	SECTION("folha que não diz onde está não esconde nada")
	{
			//Contexto vazio não é permissão para omitir: se a folha não tem
			//planta nem local, o desenho é o único lugar onde a designação
			//existe, e ela sai inteira.
		const IecStructure elemento = IecStructure::fromElementInformation(
					QStringLiteral("=CT1+QCM-K3"),
					DiagramContext());
		CHECK(por_contexto.displayedTag(IecStructure(), elemento)
		      == QStringLiteral("=CT1+QCM-K3"));
	}

	SECTION("o modo completo continua completo")
	{
			//O modo novo não é substituição: quem quiser conferir a designação
			//inteira no desenho continua podendo.
		IecStructureSettings completo;
		completo.enabled = true;
		completo.display = IecTagDisplay::Full;
		const IecStructure herda = IecStructure::fromElementInformation(
					QStringLiteral("K3"), DiagramContext());
		CHECK(completo.displayedTag(folio, herda)
		      == QStringLiteral("=CT1+QCM-K3"));
	}

	SECTION("o modo atravessa o arquivo do projeto")
	{
		QDomDocument document;
		const QDomElement written = por_contexto.toXml(document);
		IecStructureSettings read;
		read.fromXml(written);
		CHECK(read.display == IecTagDisplay::Context);
		CHECK(read == por_contexto);
	}
}

TEST_CASE("CU-10.9 — borne se endereça pela régua, com dois-pontos", "[iec]")
{
		//O projeto real de 14 folhas tem doze réguas desenhadas e 47 notas
		//digitadas à mão dizendo qual borne pertence a qual régua. No modo
		//completo, dois bornes numerados 7 — um na X10, outro na X12 — mostravam
		//os dois o mesmo "=CT1+QCM-7". Isso não é poluição, é designação
		//ambígua: informação errada. A norma reserva o ':' para conexão, e a
		//forma dela para esse borne é "-X10:7".
	SECTION("a tag digitada com dois-pontos é lida em duas partes")
	{
		const IecStructure borne = IecStructure::fromTag(
					QStringLiteral("-X10:10"));
		CHECK(borne.product    == QStringLiteral("X10"));
		CHECK(borne.connection == QStringLiteral("10"));
		CHECK(borne.toFullTag() == QStringLiteral("-X10:10"));
	}

	SECTION("dentro da régua, onde a cabeça já disse -X10, o borne é só o número")
	{
		const IecStructure regua(QString(), QString(), QStringLiteral("X10"));
		const IecStructure borne = IecStructure::fromTag(
					QStringLiteral("-X10:7"));
		CHECK(borne.toContextTag(regua) == QStringLiteral("7"));
	}

	SECTION("fora da régua, o borne diz de qual régua é")
	{
		DiagramContext info_folio;
		info_folio.addValue(QStringLiteral("plant"),   QStringLiteral("CT1"));
		info_folio.addValue(QStringLiteral("locmach"), QStringLiteral("QCM"));
		const IecStructure folio =
				IecStructure::fromFolioInformation(info_folio);
		const IecStructure borne = IecStructure::fromElementInformation(
					QStringLiteral("-X10:7"), DiagramContext());

		IecStructureSettings por_contexto;
		por_contexto.enabled = true;
		por_contexto.display = IecTagDisplay::Context;
		CHECK(por_contexto.displayedTag(folio, borne)
		      == QStringLiteral("-X10:7"));
	}

	SECTION("os dois bornes 7 da folha 13 deixam de ter o mesmo nome")
	{
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		const IecStructure na_x10 = IecStructure::fromTag(
					QStringLiteral("-X10:7"));
		const IecStructure na_x12 = IecStructure::fromTag(
					QStringLiteral("-X12:7"));

		CHECK_FALSE(na_x10 == na_x12);
		CHECK(na_x10.toContextTag(folio) == QStringLiteral("-X10:7"));
		CHECK(na_x12.toContextTag(folio) == QStringLiteral("-X12:7"));
		CHECK(na_x10.toContextTag(folio) != na_x12.toContextTag(folio));
	}

	SECTION("a lista de material carrega a designação inteira")
	{
			//É a outra metade da regra da norma: o desenho omite o que o
			//contexto diz, e a lista não omite nada.
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		const IecStructure borne = IecStructure::fromTag(
					QStringLiteral("-X10:10"));
		CHECK(IecStructure::inherit(folio, borne).toFullTag()
		      == QStringLiteral("=CT1+QCM-X10:10"));
	}

	SECTION("borne sem régua ainda mostra o número")
	{
			//A guarda do CU-10.7 pergunta se o elemento tem produto. Um borne
			//avulso não tem, e não pode desaparecer por causa dela.
		IecStructure avulso;
		avulso.connection = QStringLiteral("7");

		IecStructureSettings por_contexto;
		por_contexto.enabled = true;
		por_contexto.display = IecTagDisplay::Context;
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		CHECK(por_contexto.displayedTag(folio, avulso)
		      == QStringLiteral("7"));
	}
}
TEST_CASE("CU-10.10 — número puro é conexão, não produto", "[iec]")
{
		//Medido no projeto de 14 folhas em 21/08/2026, no modo por contexto: dos
		//230 rótulos compostos, 173 eram bornes cujo rótulo é só um número. O
		//programa colava um traço em cada um — "-7" — e o traço da norma marca um
		//produto. Um número é conexão, o ':' da norma. Eram 173 traços a mais que
		//continuavam ambíguos: o borne 7 da X10 e o borne 7 da X12 leem igual.
	SECTION("o número vai para a conexão, e o produto fica vazio")
	{
		for (const QString &tag : { QStringLiteral("7"), QStringLiteral("-7"),
					    QStringLiteral("10"), QStringLiteral("1") })
		{
			const IecStructure borne = IecStructure::fromTag(tag);
			CHECK(borne.product.isEmpty());
			CHECK(borne.connection == QString(tag).remove(QLatin1Char('-')));
		}
	}

	SECTION("uma designação de verdade continua sendo produto")
	{
			//A régua se chama X10 e é um produto: o traço dela está certo, e é
			//o que a IEC 61082 desenha.
		for (const QString &tag : { QStringLiteral("X10"), QStringLiteral("DJ0"),
					    QStringLiteral("RL2"), QStringLiteral("K3") })
		{
			const IecStructure produto = IecStructure::fromTag(tag);
			CHECK(produto.product == tag);
			CHECK(produto.connection.isEmpty());
		}
	}

	SECTION("no desenho, o borne numerado continua sendo o número")
	{
			//Os dois modos curtos: nenhum deles põe traço em número. É a
			//diferença entre 173 rótulos com um traço a mais e o desenho que o
			//projetista tinha antes da conversão.
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		const IecStructure borne = IecStructure::fromElementInformation(
					QStringLiteral("7"), DiagramContext());

		IecStructureSettings settings;
		settings.enabled = true;
		for (IecTagDisplay display : {IecTagDisplay::Short,
					      IecTagDisplay::Context})
		{
			settings.display = display;
			CHECK(settings.displayedTag(folio, borne) == QStringLiteral("7"));
		}
	}

	SECTION("o modo completo diz o que sabe, e não inventa produto")
	{
			//Quem pede a designação inteira recebe a inteira, e o que se sabe
			//deste borne é a planta, o local e a conexão. A régua dele vem da
			//T33; até lá o desenho não mente dizendo que 7 é um produto.
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		const IecStructure borne = IecStructure::fromTag(QStringLiteral("7"));

		IecStructureSettings completo;
		completo.enabled = true;
		completo.display = IecTagDisplay::Full;
		CHECK(completo.displayedTag(folio, borne)
		      == QStringLiteral("=CT1+QCM:7"));
	}

	SECTION("o borne numerado não desaparece")
	{
			//A guarda do CU-10.7 pergunta se há produto. Um borne numerado não
			//tem, e não pode sumir do desenho por causa dela.
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Short;
		CHECK_FALSE(settings.displayedTag(
				    IecStructure(),
				    IecStructure::fromTag(QStringLiteral("7"))).isEmpty());
	}
}

TEST_CASE("CU-10.11 — texto livre não recebe prefixo", "[iec]")
{
		//Dos 230 rótulos compostos, 12 eram texto que alguém digitou no campo do
		//rótulo e não é designação nenhuma: "Nota 1", "10A / 3P / F", "1 (PE)",
		//"PCI1 - UCM". O programa escrevia "-Nota 1". Não é norma, é enfeite.
	SECTION("o que é designação e o que não é")
	{
			//Designação da norma é código de letra e número, sem espaço.
		for (const QString &designacao : { QStringLiteral("K3"),
						   QStringLiteral("X10"),
						   QStringLiteral("DJ0"),
						   QStringLiteral("RL2"),
						   QStringLiteral("PS2"),
						   QStringLiteral("Q1.1"),
						   QStringLiteral("K3-1") })
		{
			CHECK(IecStructure::isDesignation(designacao));
		}

		for (const QString &livre : { QStringLiteral("Nota 1"),
					      QStringLiteral("10A / 3P / F"),
					      QStringLiteral("1 (PE)"),
					      QStringLiteral("PCI1 - UCM"),
					      QStringLiteral("Tomada Industrial"),
					      QStringLiteral("7"),
					      QStringLiteral("Reserva"),
					      QString() })
		{
			CHECK_FALSE(IecStructure::isDesignation(livre));
		}
	}

	SECTION("no desenho, texto livre sai como foi digitado")
	{
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		IecStructureSettings settings;
		settings.enabled = true;

		for (IecTagDisplay display : {IecTagDisplay::Short,
					      IecTagDisplay::Context,
					      IecTagDisplay::Full})
		{
			settings.display = display;
			for (const QString &livre : { QStringLiteral("Nota 1"),
						      QStringLiteral("10A / 3P / F"),
						      QStringLiteral("Tomada Industrial") })
			{
				const IecStructure elemento =
						IecStructure::fromElementInformation(
							livre, DiagramContext());
				CHECK(settings.displayedTag(folio, elemento) == livre);
			}
		}
	}

	SECTION("e a designação de verdade recebe o traço")
	{
			//45 dos 230 eram designação real. Nessas o traço está certo, é o que
			//a IEC 61082 desenha, e é a única coisa que o modo por contexto
			//passa a acrescentar ao desenho deste projeto.
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		IecStructureSettings por_contexto;
		por_contexto.enabled = true;
		por_contexto.display = IecTagDisplay::Context;

		for (const QString &designacao : { QStringLiteral("DJ0"),
						   QStringLiteral("RL2"),
						   QStringLiteral("X10") })
		{
			const IecStructure elemento =
					IecStructure::fromElementInformation(
						designacao, DiagramContext());
			CHECK(por_contexto.displayedTag(folio, elemento)
			      == QLatin1Char('-') + designacao);
		}
	}

	SECTION("quem digitou a estrutura à mão continua sendo obedecido")
	{
			//Texto livre é o que ninguém estruturou. Se a pessoa escreveu os
			//separadores, ela sabe o que quer, e o programa não apaga.
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		IecStructureSettings por_contexto;
		por_contexto.enabled = true;
		por_contexto.display = IecTagDisplay::Context;
		const IecStructure mao = IecStructure::fromElementInformation(
					QStringLiteral("+QCM2-Nota 1"), DiagramContext());
		CHECK(por_contexto.displayedTag(folio, mao)
		      == QStringLiteral("+QCM2-Nota 1"));
	}
}

TEST_CASE("CU-10.12 — o campo Localização só é local IEC se o projeto disser",
	  "[iec]")
{
		//O projetista preenche o campo Localização dos componentes com a régua de
		//bornes onde a fiação daquele componente chega: X1, X5, X10. As folhas
		//05, 07, 11 e 13 se chamam "Componentes - X1 a X4", "X5 e X6", "X7 e X8",
		//"X10 a X12". Não é local da IEC 81346, e o programa lia como se fosse:
		//23 componentes ganharam "+X1-" no desenho, 92 caracteres de informação
		//errada. O campo é anterior à norma e guarda texto livre; ler como local
		//é decisão do projeto, e por isso é uma caixa de seleção.
	SECTION("desligado é o padrão, e desligado o campo não vai para o desenho")
	{
		IecStructureSettings settings;
		CHECK_FALSE(settings.location_from_element);

		settings.enabled = true;
		settings.display = IecTagDisplay::Context;

		DiagramContext info;
		info.addValue(QStringLiteral("location"), QStringLiteral("X1"));
		const IecStructure elemento = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info,
					settings.location_from_element);
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		CHECK(settings.displayedTag(folio, elemento) == QStringLiteral("-K3"));
	}

	SECTION("ligado, o campo é o + da norma")
	{
			//Quem usa o campo como local de verdade — outro armário, outra sala
			//— marca a caixa e passa a ver o local no desenho.
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Context;
		settings.location_from_element = true;

		DiagramContext info;
		info.addValue(QStringLiteral("location"), QStringLiteral("QCM2"));
		const IecStructure elemento = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info,
					settings.location_from_element);
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		CHECK(settings.displayedTag(folio, elemento)
		      == QStringLiteral("+QCM2-K3"));
	}

	SECTION("com a caixa desmarcada, o local escrito no rótulo continua valendo")
	{
			//A caixa fala do campo, não da norma: quem escreveu "+QCM2-K3" no
			//rótulo disse o local explicitamente, e isso não depende de caixa.
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Context;

		const IecStructure elemento = IecStructure::fromElementInformation(
					QStringLiteral("+QCM2-K3"), DiagramContext(),
					settings.location_from_element);
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());
		CHECK(settings.displayedTag(folio, elemento)
		      == QStringLiteral("+QCM2-K3"));
	}

	SECTION("a escolha atravessa o arquivo do projeto")
	{
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Context;
		settings.location_from_element = true;

		QDomDocument document;
		IecStructureSettings lida;
		lida.fromXml(settings.toXml(document));
		CHECK(lida.location_from_element);
		CHECK(lida == settings);
	}

	SECTION("projeto antigo, sem o atributo, lê desligado")
	{
			//O silêncio de um arquivo escrito antes desta caixa não pode
			//significar "ligado": seria pôr 23 informações erradas no desenho
			//de quem só abriu o projeto.
		QDomDocument document;
		QDomElement antigo = document.createElement(
					IecStructureSettings::xmlTagName());
		antigo.setAttribute(QStringLiteral("enabled"), QStringLiteral("true"));
		antigo.setAttribute(QStringLiteral("display"), QStringLiteral("context"));

		IecStructureSettings lida;
		lida.fromXml(antigo);
		CHECK(lida.enabled);
		CHECK(lida.display == IecTagDisplay::Context);
		CHECK_FALSE(lida.location_from_element);
	}

	SECTION("a caixa é do projeto: mudar uma não iguala à outra")
	{
		IecStructureSettings com;
		com.enabled = true;
		com.location_from_element = true;
		IecStructureSettings sem;
		sem.enabled = true;
		CHECK(com != sem);
	}
}

TEST_CASE("the assigned location, and not the free text, is the plus part",
	  "[iec]")
{
		//The `+` used to come from one field only, and from it only when the
		//project opted in. That opt in exists to protect the drawing from
		//what people put in that field - the terminal strip the wiring lands
		//on, a note, a panel name - because it is free text older than the
		//norm here. A path down the location tree of the project is not free
		//text: it names a place the project has, it was assigned from a list,
		//and its designation is the conversion of the path rather than
		//anybody's typing. So it is read first, and it is read whether or not
		//the project opted in to the field. The switch is then what it should
		//always have been: the answer of a project that keeps no locations.
	const IecStructure folio(QStringLiteral("CT1"),
				 QStringLiteral("QCM"),
				 QString());

	SECTION("one level: the plus sign is written once, by the composition")
	{
		DiagramContext info;
		info.addValue(IecStructure::locationPathKey(),
			      QStringLiteral("QCP1"));

		const IecStructure element = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info);

			//The member holds the middle of the designation and not the
			//designation: the leading `+` is written by toFullTag(), so
			//storing it here as well would draw `++QCP1`.
		CHECK(element.location == QStringLiteral("QCP1"));
		CHECK(element.product  == QStringLiteral("K3"));
		CHECK(element.toFullTag() == QStringLiteral("+QCP1-K3"));
		CHECK_FALSE(element.toFullTag().contains(QStringLiteral("++")));
	}

	SECTION("several levels: the prefix repeats, and no separator is lost")
	{
		DiagramContext info;
		info.addValue(IecStructure::locationPathKey(),
			      QStringLiteral("PORTA/QCP1"));

		const IecStructure element = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info);

			//The norm writes a lower level by repeating the prefix, so
			//the separator between two levels has to survive the trip
			//into the structure. Gluing them - "PORTAQCP1" - would name a
			//place no project has.
		CHECK(element.location == QStringLiteral("PORTA+QCP1"));
		CHECK(element.toFullTag() == QStringLiteral("+PORTA+QCP1-K3"));
		CHECK_FALSE(element.toFullTag().contains(QStringLiteral("++")));
	}

	SECTION("a deep path keeps exactly one separator per level")
	{
			//Counting instead of comparing one literal: this is the case
			//that fails if the conversion ever drops or doubles a
			//separator halfway down.
		const QString path = QStringLiteral("QCM1/PLACA/PORTA");
		DiagramContext info;
		info.addValue(IecStructure::locationPathKey(), path);

		const IecStructure element = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info);

		CHECK(element.location == QStringLiteral("QCM1+PLACA+PORTA"));
		CHECK(element.location.count(QLatin1Char('+')) == 2);
		CHECK(element.toFullTag().count(QLatin1Char('+')) == 3);
	}

	SECTION("the composed tag is what the tree itself writes for that path")
	{
			//The strongest form of the previous two checks, and the one
			//that cannot rot: the drawing must carry the designation the
			//location manager shows for the same path, plus the product.
			//Written against the conversion and not against a literal, so
			//that a change to either side is caught here.
		const QString path = QStringLiteral("QCM1/PLACA/PORTA");
		DiagramContext info;
		info.addValue(IecStructure::locationPathKey(), path);

		const IecStructure element = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info);

		CHECK(element.toFullTag()
		      == LocationTree::iecTag(path) + QStringLiteral("-K3"));
	}

	SECTION("with the free text switch off, the assigned place still shows")
	{
			//This is the case the change is for. The switch is off by
			//default, and it stays off by default: what it guards is the
			//free text field, which has nothing to do with a place the
			//project assigned. Before, an assigned location produced no
			//`+` at all unless the project had also opted in to a field
			//it may well not use - the location was correct in the data
			//and absent from the drawing.
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Context;
		CHECK_FALSE(settings.location_from_element);

		DiagramContext info;
		info.addValue(IecStructure::locationPathKey(),
			      QStringLiteral("PORTA/QCP1"));

		const IecStructure element = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info,
					settings.location_from_element);

		CHECK(element.location == QStringLiteral("PORTA+QCP1"));
		CHECK(settings.displayedTag(folio, element)
		      == QStringLiteral("+PORTA+QCP1-K3"));
			//And explicitly: not the tag of a component whose place the
			//drawing does not say.
		CHECK(settings.displayedTag(folio, element)
		      != QStringLiteral("-K3"));

		settings.display = IecTagDisplay::Full;
		CHECK(settings.displayedTag(folio, element)
		      == QStringLiteral("=CT1+PORTA+QCP1-K3"));
	}

	SECTION("the path wins over the field, switch on or off")
	{
			//Both filled, and they disagree - which is the normal state of
			//a project that used the field for something else before the
			//locations existed. The place that was assigned is the answer
			//in both settings; the field is a fallback and never an
			//override.
		DiagramContext info;
		info.addValue(IecStructure::locationPathKey(),
			      QStringLiteral("PORTA/QCP1"));
		info.addValue(IecStructure::locationKey(), QStringLiteral("X1"));

		const IecStructure with_switch = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info, true);
		CHECK(with_switch.location == QStringLiteral("PORTA+QCP1"));

		const IecStructure without = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info, false);
		CHECK(without.location == QStringLiteral("PORTA+QCP1"));
	}

	SECTION("no path, switch on: the field is the plus part, as before")
	{
			//The old behaviour, unchanged. A project that keeps no
			//locations and did opt in to the field reads exactly as it
			//read before.
		DiagramContext info;
		info.addValue(IecStructure::locationKey(), QStringLiteral("QCM2"));

		const IecStructure element = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info, true);

		CHECK(element.location == QStringLiteral("QCM2"));

		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Context;
		settings.location_from_element = true;
		CHECK(settings.displayedTag(folio, element)
		      == QStringLiteral("+QCM2-K3"));
	}

	SECTION("no path, switch off: no plus part at all, as before")
	{
			//The other half of the old behaviour, and the one that
			//protects a delivered drawing: the field holding a terminal
			//strip does not become a place because the norm was turned
			//on.
		DiagramContext info;
		info.addValue(IecStructure::locationKey(), QStringLiteral("X1"));

		const IecStructure element = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info, false);

		CHECK(element.location.isEmpty());

		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Context;
		CHECK(settings.displayedTag(folio, element)
		      == QStringLiteral("-K3"));
	}

	SECTION("a path naming no code at all falls back to the old rule")
	{
			//A path of separators and spaces names nothing, so it is not
			//an assigned place and must not silence the field. Reading it
			//as an empty `+` would take the fallback away from a project
			//that asked for it.
		DiagramContext info;
		info.addValue(IecStructure::locationPathKey(), QStringLiteral("/"));
		info.addValue(IecStructure::locationKey(), QStringLiteral("QCM2"));

		CHECK(IecStructure::fromElementInformation(
			      QStringLiteral("K3"), info, true).location
		      == QStringLiteral("QCM2"));
		CHECK(IecStructure::fromElementInformation(
			      QStringLiteral("K3"), info, false)
		      .location.isEmpty());
	}

	SECTION("the key is the one the element information vocabulary uses")
	{
			//The literal is written twice - once in the vocabulary of
			//element information, once here, so that this class keeps
			//depending on Qt alone. This is what the second copy costs:
			//a check that the two never drift, because a drifted key
			//would read every component as having no location.
		CHECK(IecStructure::locationPathKey()
		      == QStringLiteral("location_path"));
		CHECK(IecStructure::locationPathKey()
		      == QETInformation::ELMT_LOCATION_PATH);
		CHECK(IecStructure::locationPathKey()
		      != IecStructure::locationKey());
	}
}

TEST_CASE("a repeated prefix is one location, read back as it was written",
	  "[iec]")
{
		//The composition can now put a `+` inside the location part, because
		//that is how the norm writes a lower level inside a higher one. The
		//function that reads a tag has to agree, or the module would be able
		//to write a designation it cannot read - and the reading is what the
		//tag typed by hand goes through.
	SECTION("two levels typed by hand name one place")
	{
		const IecStructure typed =
				IecStructure::fromTag(QStringLiteral("+QCM1+PORTA-K3"));
		CHECK(typed.location == QStringLiteral("QCM1+PORTA"));
		CHECK(typed.product  == QStringLiteral("K3"));
		CHECK(typed.toFullTag() == QStringLiteral("+QCM1+PORTA-K3"));
	}

	SECTION("a separator with nothing after it separates nothing")
	{
		CHECK(IecStructure::fromTag(QStringLiteral("+A1+")).location
		      == QStringLiteral("A1"));
		CHECK(IecStructure::fromTag(QStringLiteral("++A1")).location
		      == QStringLiteral("A1"));
	}

	SECTION("composing twice gives the same as composing once")
	{
			//The property the whole file already guards for a single level,
			//and the reason the reading above had to be fixed with the
			//change: a location of two levels fed back through the reading
			//used to come out glued, so a second pass drew a different
			//tag from the first.
		DiagramContext info;
		info.addValue(IecStructure::locationPathKey(),
			      QStringLiteral("PORTA/QCP1"));
		const IecStructure element = IecStructure::fromElementInformation(
					QStringLiteral("K3"), info);
		const IecStructure folio(QStringLiteral("CT1"),
					 QStringLiteral("QCM"),
					 QString());

		IecStructureSettings settings;
		settings.enabled = true;

		for (IecTagDisplay display : {IecTagDisplay::Short,
					      IecTagDisplay::Context,
					      IecTagDisplay::Full})
		{
			settings.display = display;
			const QString once = settings.displayedTag(folio, element);
			const QString twice = settings.displayedTag(
						folio, IecStructure::fromTag(once));
			const QString third = settings.displayedTag(
						folio, IecStructure::fromTag(twice));
			CHECK(twice == once);
			CHECK(third == once);
		}
	}
}
