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
#include "../../../sources/location/mountinglayout.h"
#include "../../../sources/location/mountingprofile.h"
#include "qt_catch_tostring.h"

#include <QDomDocument>
#include <QDomElement>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <limits>

/*
	O trilho e a canaleta como a oficina os trata: barra cortada na medida,
	sem código de peça, e o que a lista de material precisa deles é o
	comprimento somado por tipo.

	Tudo aqui é retângulo e número — nada de cena, nada de item gráfico —,
	e é de propósito: o que erra numa peça elástica é a aritmética de qual
	dos dois lados é o corte, e isso se confere por número. O que só se
	prova com desenho — a alça na ponta, o passo de desfazer que o gesto
	deixa — está em src/ui/mountedprofileitem_test.cpp, e não se repete
	aqui.
*/

namespace
{
	const qreal not_a_number = std::numeric_limits<qreal>::quiet_NaN();

	/// @return um trilho DIN de 35 × 7,5, que é o de todo painel
	MountingProfile trilho()
	{
		return MountingProfile::rail(35.0, 7.5);
	}

	/// @return uma canaleta de 40 × 60
	MountingProfile canaleta()
	{
		return MountingProfile::duct(40.0, 60.0);
	}

	/**
		@return uma identidade nova a cada chamada

		Contador e não o par posição/comprimento: duas barras iguais
		cortadas iguais são duas barras, e o modelo recusa montar duas
		vezes a mesma identidade — um gerador que repetisse faria a
		recusa parecer um defeito da soma.
	*/
	QString proxima()
	{
		static int contador = 0;
		return QStringLiteral("peca-%1").arg(++ contador);
	}

	/// @return uma peça cortada, deitada, do comprimento pedido
	MountedItem cortada(const MountingProfile &perfil,
			    qreal x, qreal y,
			    qreal comprimento,
			    MountingRun sentido = MountingRun::Across)
	{
		MountedItem item;
		item.uuid = proxima();
		item.profile = perfil;
		item.run = sentido;
		item.position = QPointF(x, y);
		item.size = perfil.sizeFor(comprimento, sentido);
		return item;
	}
}

TEST_CASE("T19 — o perfil é a barra, e o comprimento é da peça",
	  "[calepinagem]")
{
	SECTION("a barra se identifica por um token, e ele não é traduzido")
	{
		CHECK(trilho().isRail());
		CHECK_FALSE(trilho().isDuct());
		CHECK(canaleta().isDuct());
		CHECK_FALSE(canaleta().isNull());

			//A chave é o que agrupa a lista de material: dois
			//pedaços da mesma barra têm de tê-la igual, e duas
			//barras diferentes, diferente.
		CHECK(trilho().key() == MountingProfile::rail(35.0, 7.5).key());
		CHECK(trilho().key() != MountingProfile::rail(35.0, 15.0).key());
		CHECK(trilho().key() != canaleta().key());
	}

	SECTION("a altura entra na chave, porque são duas barras na prateleira")
	{
			//35 × 7,5 e 35 × 15 ocupam a mesma largura na placa e são
			//dois perfis que se compram separados. Somar os dois
			//daria um comprimento que ninguém consegue pedir.
		CHECK(MountingProfile::rail(35.0, 7.5).key()
		      != MountingProfile::rail(35.0, 15.0).key());
	}

	SECTION("um tipo que esta versão não conhece continua sendo um perfil")
	{
		const MountingProfile desconhecido(QStringLiteral("trunking"),
						   60.0, 40.0);

		CHECK_FALSE(desconhecido.isNull());
		CHECK_FALSE(desconhecido.isRail());
		CHECK_FALSE(desconhecido.isDuct());
		CHECK_FALSE(desconhecido.designation().isEmpty());
		CHECK(desconhecido.designation().contains(
			      QStringLiteral("trunking")));
	}

	SECTION("nenhum perfil fica sem nome para dizer em voz alta")
	{
		CHECK_FALSE(MountingProfile().designation().isEmpty());
		CHECK_FALSE(trilho().designation().isEmpty());
		CHECK_FALSE(MountingProfile::rail(0.0, 0.0)
			    .designation().isEmpty());
	}

	SECTION("a lista de trilhos padrão é norma, e não catálogo")
	{
		const QList<MountingProfile> trilhos =
				MountingProfile::standardRails();

		REQUIRE_FALSE(trilhos.isEmpty());
		for (const MountingProfile &perfil : trilhos)
		{
			CHECK(perfil.isRail());
			CHECK(perfil.hasSection());
		}
	}
}

TEST_CASE("T19 — qual dos dois lados é o corte é o sentido, e não o maior",
	  "[calepinagem]")
{
	SECTION("deitado, o comprimento é a largura; em pé, é a altura")
	{
		const QSizeF deitado = trilho().sizeFor(480.0,
							MountingRun::Across);
		const QSizeF em_pe = trilho().sizeFor(480.0, MountingRun::Down);

		CHECK(deitado.width() == Approx(480.0));
		CHECK(deitado.height() == Approx(35.0));
		CHECK(em_pe.width() == Approx(35.0));
		CHECK(em_pe.height() == Approx(480.0));
	}

	SECTION("a leitura de volta é a mesma conta ao contrário")
	{
		const QSizeF em_pe = canaleta().sizeFor(600.0,
							MountingRun::Down);

		CHECK(MountingProfile::lengthOf(em_pe, MountingRun::Down)
		      == Approx(600.0));
		CHECK(MountingProfile::sectionOf(em_pe, MountingRun::Down)
		      == Approx(40.0));
	}

	SECTION("a mesma peça lida no sentido errado daria dez vezes a compra")
	{
			//600 × 40 em pé são 600 mm de canaleta. Lendo o lado
			//maior, seriam 600 também; lendo o lado errado de uma
			//peça de 600 × 40 deitada, seriam 40. É esse o erro que
			//guardar o sentido evita.
		const MountedItem em_pe = cortada(canaleta(), 0.0, 0.0, 600.0,
						  MountingRun::Down);

		CHECK(em_pe.cutLength() == Approx(600.0));
		CHECK(em_pe.footprint().width() == Approx(40.0));
		CHECK(em_pe.footprint().height() == Approx(600.0));
	}

	SECTION("peça que ninguém cortou tem comprimento zero, e é um estado")
	{
		MountedItem sem_corte;
		sem_corte.profile = trilho();

		CHECK(sem_corte.isCutToLength());
		CHECK(sem_corte.cutLength() == Approx(0.0));
	}

	SECTION("peça comprada não é peça cortada")
	{
			//22,5 mm é o módulo de um disjuntor unipolar, e é
			//exatamente o número que sai daqui quando a pergunta
			//"isto foi cortado?" não é feita antes de ler o
			//retângulo: a largura da peça respondendo como
			//comprimento de barra. Na lista de material isso vira
			//trilho a comprar que na verdade é disjuntor.
		MountedItem disjuntor(QStringLiteral("-Q1"),
				      QPointF(10.0, 10.0),
				      QSizeF(22.5, 85.0));

		CHECK_FALSE(disjuntor.isCutToLength());
		CHECK(disjuntor.cutLength() == Approx(0.0));
	}

	SECTION("nem em pé, que é onde o lado lido seria a altura")
	{
			//O mesmo defeito pelo outro eixo: lido no sentido
			//vertical, o disjuntor responderia 85 em vez de 22,5.
			//Um caso só, no sentido padrão, deixaria essa metade
			//sem guarda.
		MountedItem disjuntor(QStringLiteral("-Q1"),
				      QPointF(10.0, 10.0),
				      QSizeF(22.5, 85.0));
		disjuntor.run = MountingRun::Down;

		CHECK(disjuntor.cutLength() == Approx(0.0));
	}
}

TEST_CASE("T19 — esticar move a ponta puxada, e só ela",
	  "[calepinagem]")
{
	const QRectF barra(100.0, 200.0, 480.0, 35.0);

	SECTION("puxar a ponta direita muda o comprimento e não a origem")
	{
		const QRectF esticada = MountingProfile::stretch(
					barra, MountingRun::Across,
					MountingEnd::End, 700.0);

		CHECK(esticada.left() == Approx(100.0));
		CHECK(esticada.top() == Approx(200.0));
		CHECK(esticada.width() == Approx(600.0));
		CHECK(esticada.height() == Approx(35.0));
	}

	SECTION("puxar a ponta esquerda muda a origem e o comprimento")
	{
		const QRectF esticada = MountingProfile::stretch(
					barra, MountingRun::Across,
					MountingEnd::Start, 40.0);

		CHECK(esticada.left() == Approx(40.0));
		CHECK(esticada.right() == Approx(580.0));
		CHECK(esticada.width() == Approx(540.0));
		CHECK(esticada.height() == Approx(35.0));
	}

	SECTION("a seção não muda nunca, que é o que uma alça de canto faria")
	{
			//Uma canaleta de 40 puxada mais comprida continua de 40.
			//Alargá-la seria inventar uma canaleta que ninguém vende,
			//e caladamente.
		const QRectF canal(0.0, 0.0, 300.0, 40.0);
		const QRectF esticada = MountingProfile::stretch(
					canal, MountingRun::Across,
					MountingEnd::End, 1200.0);

		CHECK(esticada.height() == Approx(40.0));
	}

	SECTION("puxar uma ponta para além da outra para no comprimento mínimo")
	{
		const QRectF virada = MountingProfile::stretch(
					barra, MountingRun::Across,
					MountingEnd::End, -400.0);

		CHECK(virada.width()
		      == Approx(MountingProfile::minimumLength()));
		CHECK(virada.left() == Approx(100.0));
		CHECK(virada.width() > 0.0);
	}

	SECTION("e o mesmo pela ponta de cima, em pé")
	{
		const QRectF em_pe(50.0, 100.0, 35.0, 400.0);
		const QRectF virada = MountingProfile::stretch(
					em_pe, MountingRun::Down,
					MountingEnd::Start, 900.0);

		CHECK(virada.height()
		      == Approx(MountingProfile::minimumLength()));
		CHECK(virada.bottom() == Approx(500.0));
		CHECK(virada.width() == Approx(35.0));
	}

	SECTION("uma coordenada que não é número deixa a peça como estava")
	{
		const QRectF intacta = MountingProfile::stretch(
					barra, MountingRun::Across,
					MountingEnd::End, not_a_number);

		CHECK(intacta.left() == Approx(barra.left()));
		CHECK(intacta.width() == Approx(barra.width()));
	}
}

TEST_CASE("T19 — a barra cortada volta do arquivo como a barra que era",
	  "[calepinagem]")
{
	MountingLayout layout;
	MountingSurface placa(QStringLiteral("QCM1"),
			      QStringLiteral("plate"),
			      MountingArea(600.0, 800.0));
	const QString face = layout.appendSurface(placa);

	const QString rail = layout.mountItem(
				face, cortada(trilho(), 10.0, 60.0, 480.0));
	const QString duct = layout.mountItem(
				face, cortada(canaleta(), 10.0, 10.0, 560.0,
					      MountingRun::Down));
	REQUIRE_FALSE(rail.isEmpty());
	REQUIRE_FALSE(duct.isEmpty());

	QDomDocument documento;
	documento.appendChild(layout.toXml(documento));

	MountingLayout lido;
	REQUIRE(lido.fromXml(documento.documentElement()));

	SECTION("o tipo, a seção, a altura e o sentido atravessam o arquivo")
	{
		CHECK(lido == layout);
		CHECK(lido.item(rail).profile.isRail());
		CHECK(lido.item(rail).profile.section == Approx(35.0));
		CHECK(lido.item(rail).profile.depth == Approx(7.5));
		CHECK(lido.item(rail).run == MountingRun::Across);
		CHECK(lido.item(rail).cutLength() == Approx(480.0));

		CHECK(lido.item(duct).profile.isDuct());
		CHECK(lido.item(duct).run == MountingRun::Down);
		CHECK(lido.item(duct).cutLength() == Approx(560.0));
	}

	SECTION("um projeto sem perfil nenhum continua abrindo, e sem perfil")
	{
			//A leitura é tolerante: o arquivo escrito antes de
			//existir perfil não diz nada sobre isso, e o que ele
			//guarda é uma peça comprada.
		QDomDocument antigo;
		QDomElement face_antiga =
				antigo.createElement(MountingSurface::tagName());
		face_antiga.setAttribute(QStringLiteral("location"),
					 QStringLiteral("QCM1"));
		face_antiga.setAttribute(QStringLiteral("kind"),
					 QStringLiteral("plate"));

		QDomElement peca =
				antigo.createElement(MountingSurface::itemTagName());
		peca.setAttribute(QStringLiteral("uuid"), QStringLiteral("q1"));
		peca.setAttribute(QStringLiteral("x"), QStringLiteral("10"));
		peca.setAttribute(QStringLiteral("y"), QStringLiteral("20"));
		peca.setAttribute(QStringLiteral("width"), QStringLiteral("22.5"));
		peca.setAttribute(QStringLiteral("height"), QStringLiteral("85"));
		face_antiga.appendChild(peca);

		MountingSurface velha;
		REQUIRE(velha.fromXml(face_antiga));
		REQUIRE(velha.itemCount() == 1);

		CHECK_FALSE(velha.item(QStringLiteral("q1")).isCutToLength());
		CHECK(velha.item(QStringLiteral("q1")).run
		      == MountingRun::Across);
	}

	SECTION("um tipo que esta versão não conhece sobrevive a ida e volta")
	{
			//Se ele fosse lido como peça comprada, o próximo salvamento
			//o escreveria sem o perfil — e a barra teria desaparecido
			//do arquivo sem ninguém ver.
		MountingLayout estranho;
		const QString outra = estranho.appendSurface(
					MountingSurface(QStringLiteral("QCM2"),
							QStringLiteral("plate")));
		const QString tubo = estranho.mountItem(
					outra,
					cortada(MountingProfile(
							QStringLiteral("trunking"),
							60.0, 40.0),
						0.0, 0.0, 300.0));

		QDomDocument ida;
		ida.appendChild(estranho.toXml(ida));

		MountingLayout volta;
		REQUIRE(volta.fromXml(ida.documentElement()));

		CHECK(volta.item(tubo).isCutToLength());
		CHECK(volta.item(tubo).profile.kind
		      == QStringLiteral("trunking"));
		CHECK(volta.item(tubo).profile.section == Approx(60.0));
	}
}

TEST_CASE("T19 — a lista de material soma comprimento por barra, e conta as peças",
	  "[calepinagem]")
{
	MountingSurface placa(QStringLiteral("QCM1"),
			      QStringLiteral("plate"),
			      MountingArea(600.0, 800.0));

	placa.items << cortada(trilho(), 10.0, 100.0, 480.0)
		    << cortada(trilho(), 10.0, 300.0, 480.0)
		    << cortada(trilho(), 10.0, 500.0, 240.0)
		    << cortada(canaleta(), 10.0, 10.0, 560.0)
		    << MountedItem(QStringLiteral("-Q1"),
				   QPointF(60.0, 110.0),
				   QSizeF(22.5, 85.0));

	SECTION("três pedaços de uma barra são uma linha, com a soma")
	{
		const QList<MountingProfileTotal> linhas = placa.profileTotals();

		REQUIRE(linhas.count() == 2);

		int trilho_em = -1;
		for (int i = 0 ; i < linhas.count() ; ++ i)
		{
			if (linhas.at(i).profile.isRail()) {
				trilho_em = i;
			}
		}
		REQUIRE(trilho_em >= 0);

		CHECK(linhas.at(trilho_em).pieces == 3);
		CHECK(linhas.at(trilho_em).length == Approx(1200.0));
		CHECK(linhas.at(trilho_em).metres() == Approx(1.2));
		CHECK(linhas.at(trilho_em).uncut == 0);
	}

	SECTION("o disjuntor não entra aqui: ele se conta, não se corta")
	{
		const QList<MountingProfileTotal> linhas = placa.profileTotals();

		for (const MountingProfileTotal &linha : linhas) {
			CHECK_FALSE(linha.profile.isNull());
		}
	}

	SECTION("e não entra em milímetro nenhum, que é o que contar linhas não vê")
	{
			//Contar linhas pega o disjuntor virando linha própria;
			//não pegaria o disjuntor somando os seus 22,5 mm DENTRO
			//da linha do trilho. Por isso o total se confere em
			//milímetro: 1 200 de trilho mais 560 de canaleta, e nem
			//um milímetro de peça comprada.
		qreal total = 0.0;
		const QList<MountingProfileTotal> linhas = placa.profileTotals();

		for (const MountingProfileTotal &linha : linhas) {
			total += linha.length;
		}

		CHECK(total == Approx(1760.0));
	}

	SECTION("uma face só de peças compradas não gera linha nenhuma")
	{
		MountingSurface comprada(QStringLiteral("QCM2"),
					 QStringLiteral("plate"),
					 MountingArea(600.0, 800.0));
		comprada.items << MountedItem(QStringLiteral("-Q1"),
					      QPointF(10.0, 10.0),
					      QSizeF(22.5, 85.0))
			       << MountedItem(QStringLiteral("-KM1"),
					      QPointF(50.0, 10.0),
					      QSizeF(45.0, 85.0));

		CHECK(comprada.profileTotals().isEmpty());
	}

	SECTION("peça sem comprimento conta como peça e não soma metro nenhum")
	{
			//Ausência não é zero. Se ela somasse zero, a lista diria
			//que o painel precisa de menos trilho do que precisa — e
			//isso só aparece quando a barra acaba na bancada.
		MountedItem sem_corte;
		sem_corte.uuid = QStringLiteral("sem-corte");
		sem_corte.profile = trilho();
		placa.items << sem_corte;

		const QList<MountingProfileTotal> linhas = placa.profileTotals();

		for (const MountingProfileTotal &linha : linhas)
		{
			if (!linha.profile.isRail()) {
				continue;
			}
			CHECK(linha.pieces == 4);
			CHECK(linha.uncut == 1);
			CHECK(linha.length == Approx(1200.0));
		}
	}

	SECTION("duas barras da mesma largura e alturas diferentes são duas linhas")
	{
		placa.items << cortada(MountingProfile::rail(35.0, 15.0),
				       10.0, 700.0, 300.0);

		CHECK(placa.profileTotals().count() == 3);
	}

	SECTION("o projeto inteiro soma as faces, e a ordem não muda entre duas leituras")
	{
		MountingLayout layout;
		const QString fundo = layout.appendSurface(placa);
		REQUIRE_FALSE(fundo.isEmpty());

		MountingSurface porta(QStringLiteral("QCM1/PORTE"),
				      QStringLiteral("door"),
				      MountingArea(500.0, 700.0));
		porta.items << cortada(trilho(), 20.0, 20.0, 300.0);
		REQUIRE_FALSE(layout.appendSurface(porta).isEmpty());

		const QList<MountingProfileTotal> primeira = layout.profileTotals();
		const QList<MountingProfileTotal> segunda = layout.profileTotals();

		REQUIRE(primeira.count() == 2);
		REQUIRE(segunda.count() == primeira.count());

		for (int i = 0 ; i < primeira.count() ; ++ i) {
			CHECK(primeira.at(i).profile.key()
			      == segunda.at(i).profile.key());
		}

		for (const MountingProfileTotal &linha : primeira)
		{
			if (linha.profile.isRail())
			{
				CHECK(linha.pieces == 4);
				CHECK(linha.length == Approx(1500.0));
			}
		}
	}
}

TEST_CASE("T19 — um trilho sem rótulo se chama pelo perfil, e não pelo identificador",
	  "[calepinagem]")
{
	MountedItem barra = cortada(trilho(), 0.0, 0.0, 480.0);
	barra.uuid = QStringLiteral("8f14e45fceea167a5a36dedd4bea2543");

	SECTION("o nome dito em voz alta é o do perfil")
	{
			//Sem isto, a legenda do desfazer de um trilho que
			//ninguém rotulou seria trinta e dois caracteres
			//hexadecimais.
		CHECK(barra.designation() == trilho().designation());
		CHECK_FALSE(barra.designation().contains(barra.uuid));
	}

	SECTION("mas o rótulo de quem o rotulou continua vindo antes")
	{
		barra.label = QStringLiteral("-W1");

		CHECK(barra.designation() == QStringLiteral("-W1"));
	}
}
