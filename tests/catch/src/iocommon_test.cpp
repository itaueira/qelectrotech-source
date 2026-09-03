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
#include "../../../sources/catalog/catalogpart.h"
#include "../../../sources/plc/iocommon.h"
#include "qt_catch_tostring.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace
{
		/// One pin, of @a role, numbered @a label, in @a group.
	CatalogPin pino(const QString &label, CatalogPinRole role,
			const QString &group = QString())
	{
		CatalogPin pin(label, role);
		pin.group = group;
		return pin;
	}

		/// A part whose pins are, in pin order, the roles asked for.
	CatalogPart peca(const QList<CatalogPinRole> &roles)
	{
		CatalogPart part(QStringLiteral("6ES7-321"), 1);
		int number = 1;
		for (CatalogPinRole role : roles)
		{
			part.pins.append(pino(QString::number(number), role));
			++number;
		}
		return part;
	}

		/// One element the drawing offers as a default bar.
	IoCommon::Bus barra(const QString &id, const QString &label,
			    IoCommon::BusKind kind, int folio, bool marked,
			    int terminals, int free_terminals)
	{
		IoCommon::Bus bus;
		bus.id = id;
		bus.label = label;
		bus.kind = kind;
		bus.folio = folio;
		bus.marked = marked;
		bus.terminals = terminals;
		bus.free_terminals = free_terminals;
		return bus;
	}

		/// One connection point of a card that the catalogue calls a common.
	IoCommon::Common comum(const QString &id, const QString &label,
			       IoCommon::BusKind bus, int folio)
	{
		IoCommon::Common common;
		common.id = id;
		common.label = label;
		common.bus = bus;
		common.folio = folio;
		common.card_label = QStringLiteral("A1");
		return common;
	}
}

TEST_CASE("T11 — o papel do catálogo diz em qual barra o comum entra",
	  "[iocommon]")
{
	CHECK(IoCommon::busOfRole(CatalogPinRole::SupplyCommon)
	      == IoCommon::SupplyBus);
	CHECK(IoCommon::busOfRole(CatalogPinRole::ReturnCommon)
	      == IoCommon::ReturnBus);

	SECTION("todo papel que não é comum fica de fora")
	{
		CHECK(IoCommon::busOfRole(CatalogPinRole::Unknown)
		      == IoCommon::NoBus);
		CHECK(IoCommon::busOfRole(CatalogPinRole::Terminal)
		      == IoCommon::NoBus);
		CHECK(IoCommon::busOfRole(CatalogPinRole::Coil)
		      == IoCommon::NoBus);
		CHECK(IoCommon::busOfRole(CatalogPinRole::Input)
		      == IoCommon::NoBus);
		CHECK(IoCommon::busOfRole(CatalogPinRole::Output)
		      == IoCommon::NoBus);
		CHECK(IoCommon::busOfRole(CatalogPinRole::OutputRelay)
		      == IoCommon::NoBus);
	}

	SECTION("o que se grava no elemento volta igual")
	{
		CHECK(IoCommon::busToString(IoCommon::SupplyBus)
		      == QStringLiteral("supply"));
		CHECK(IoCommon::busToString(IoCommon::ReturnBus)
		      == QStringLiteral("return"));
		CHECK(IoCommon::busToString(IoCommon::NoBus).isEmpty());

		CHECK(IoCommon::busFromString(
			      IoCommon::busToString(IoCommon::SupplyBus))
		      == IoCommon::SupplyBus);
		CHECK(IoCommon::busFromString(
			      IoCommon::busToString(IoCommon::ReturnBus))
		      == IoCommon::ReturnBus);
	}

	SECTION("valor que não nomeia barra nenhuma não vira barra")
	{
		CHECK(IoCommon::busFromString(QString()) == IoCommon::NoBus);
		CHECK(IoCommon::busFromString(QStringLiteral("barramento"))
		      == IoCommon::NoBus);
		CHECK(IoCommon::busFromString(QStringLiteral("  RETURN  "))
		      == IoCommon::ReturnBus);
	}

	SECTION("as duas barras têm nome, e a não-barra não tem")
	{
		CHECK(!IoCommon::busName(IoCommon::SupplyBus).isEmpty());
		CHECK(!IoCommon::busName(IoCommon::ReturnBus).isEmpty());
		CHECK(IoCommon::busName(IoCommon::SupplyBus)
		      != IoCommon::busName(IoCommon::ReturnBus));
		CHECK(IoCommon::busName(IoCommon::NoBus).isEmpty());
	}
}

TEST_CASE("T11 — a peça atribuída responde borne a borne, na mesma ordem",
	  "[iocommon]")
{
	const CatalogPart part = peca(QList<CatalogPinRole>{
					     CatalogPinRole::Input,
					     CatalogPinRole::ReturnCommon,
					     CatalogPinRole::Input,
					     CatalogPinRole::ReturnCommon,
					     CatalogPinRole::SupplyCommon});

	const QList<IoCommon::BusKind> buses =
			IoCommon::busesOfPart(part, QString(), 5);

	REQUIRE(buses.count() == 5);
	CHECK(buses.at(0) == IoCommon::NoBus);
	CHECK(buses.at(1) == IoCommon::ReturnBus);
	CHECK(buses.at(2) == IoCommon::NoBus);
	CHECK(buses.at(3) == IoCommon::ReturnBus);
	CHECK(buses.at(4) == IoCommon::SupplyBus);

	SECTION("borne que a peça não descreve não vira comum")
	{
		const QList<IoCommon::BusKind> mais =
				IoCommon::busesOfPart(part, QString(), 7);
		REQUIRE(mais.count() == 7);
		CHECK(mais.at(5) == IoCommon::NoBus);
		CHECK(mais.at(6) == IoCommon::NoBus);
	}

	SECTION("símbolo com menos borne só responde pelos que tem")
	{
		const QList<IoCommon::BusKind> menos =
				IoCommon::busesOfPart(part, QString(), 3);
		CHECK(menos.count() == 3);
		CHECK(IoCommon::busesOfPart(part, QString(), 0).isEmpty());
	}

	SECTION("peça sem pino nenhum não inventa comum")
	{
		const CatalogPart muda(QStringLiteral("SEM-PINO"), 1);
		const QList<IoCommon::BusKind> nada =
				IoCommon::busesOfPart(muda, QString(), 4);
		REQUIRE(nada.count() == 4);
		for (IoCommon::BusKind kind : nada) {
			CHECK(kind == IoCommon::NoBus);
		}
	}
}

TEST_CASE("T11 — o grupo escolhe o sub-símbolo, e o que não tem grupo "
	  "atende a todos",
	  "[iocommon]")
{
	CatalogPart part(QStringLiteral("3RT2-016"), 1);
	part.pins.append(pino("A1", CatalogPinRole::SupplyCommon, "bobina"));
	part.pins.append(pino("13", CatalogPinRole::ContactNo, "contato"));
	part.pins.append(pino("N", CatalogPinRole::ReturnCommon));

	SECTION("cada grupo responde pelos pinos dele")
	{
		const QList<IoCommon::BusKind> bobina =
				IoCommon::busesOfPart(part,
						      QStringLiteral("bobina"),
						      1);
		REQUIRE(bobina.count() == 1);
		CHECK(bobina.at(0) == IoCommon::SupplyBus);

		const QList<IoCommon::BusKind> contato =
				IoCommon::busesOfPart(part,
						      QStringLiteral("contato"),
						      1);
		REQUIRE(contato.count() == 1);
		CHECK(contato.at(0) == IoCommon::NoBus);
	}

	SECTION("grupo que a peça não conhece cai nos pinos sem grupo")
	{
		const QList<IoCommon::BusKind> outro =
				IoCommon::busesOfPart(part,
						      QStringLiteral("auxiliar"),
						      1);
		REQUIRE(outro.count() == 1);
		CHECK(outro.at(0) == IoCommon::ReturnBus);
	}
}

TEST_CASE("T11 — qual barra, e em qual folha",
	  "[iocommon]")
{
	QList<IoCommon::Bus> buses;
	buses << barra("b1", "+24V", IoCommon::SupplyBus, 3, true, 12, 12);
	buses << barra("b2", "+24V", IoCommon::SupplyBus, 5, false, 12, 12);
	buses << barra("b3", "0V", IoCommon::ReturnBus, 5, true, 12, 12);

	CHECK(IoCommon::busFor(buses, IoCommon::SupplyBus, 3).id
	      == QStringLiteral("b1"));
	CHECK(IoCommon::busFor(buses, IoCommon::SupplyBus, 5).id
	      == QStringLiteral("b2"));
	CHECK(IoCommon::busFor(buses, IoCommon::ReturnBus, 5).id
	      == QStringLiteral("b3"));

	SECTION("barra de outra folha não serve, porque o fio não atravessa "
		"folha")
	{
		CHECK(IoCommon::busFor(buses, IoCommon::ReturnBus, 3).kind
		      == IoCommon::NoBus);
		const IoCommon::Bus nenhuma =
				IoCommon::busFor(buses, IoCommon::SupplyBus,
						 42);
		CHECK(nenhuma.kind == IoCommon::NoBus);
		CHECK(nenhuma.id.isEmpty());
	}

	SECTION("pedir barra nenhuma devolve barra nenhuma")
	{
		CHECK(IoCommon::busFor(buses, IoCommon::NoBus, 3).kind
		      == IoCommon::NoBus);
	}

	SECTION("a marcada ganha da que só repete o rótulo")
	{
		QList<IoCommon::Bus> duas;
		duas << barra("solta", "+24V", IoCommon::SupplyBus, 7, false,
			      12, 8);
		duas << barra("marcada", "+24V", IoCommon::SupplyBus, 7, true,
			      4, 1);
		CHECK(IoCommon::busFor(duas, IoCommon::SupplyBus, 7).id
		      == QStringLiteral("marcada"));
	}

	SECTION("entre iguais, a que tem mais borne livre")
	{
		QList<IoCommon::Bus> duas;
		duas << barra("cheia", "+24V", IoCommon::SupplyBus, 9, false,
			      12, 1);
		duas << barra("vazia", "+24V", IoCommon::SupplyBus, 9, false,
			      12, 5);
		CHECK(IoCommon::busFor(duas, IoCommon::SupplyBus, 9).id
		      == QStringLiteral("vazia"));
	}

	SECTION("empate resolve sempre do mesmo jeito, senão duas rodadas "
		"dariam fios diferentes")
	{
		QList<IoCommon::Bus> duas;
		duas << barra("bbb", "+24V", IoCommon::SupplyBus, 11, false,
			      12, 4);
		duas << barra("aaa", "+24V", IoCommon::SupplyBus, 11, false,
			      12, 4);
		CHECK(IoCommon::busFor(duas, IoCommon::SupplyBus, 11).id
		      == QStringLiteral("aaa"));

		QList<IoCommon::Bus> invertida;
		invertida << duas.at(1) << duas.at(0);
		CHECK(IoCommon::busFor(invertida, IoCommon::SupplyBus, 11).id
		      == QStringLiteral("aaa"));
	}
}

TEST_CASE("T11 — o plano liga o que dá, e diz por que recusou o resto",
	  "[iocommon]")
{
	QList<IoCommon::Bus> buses;
	buses << barra("b1", "+24V", IoCommon::SupplyBus, 3, true, 12, 12);
	buses << barra("b2", "0V", IoCommon::ReturnBus, 3, true, 0, 0);

	QList<IoCommon::Common> commons;
	commons << comum("c1", "1M", IoCommon::SupplyBus, 3);
	commons << comum("c2", "5", IoCommon::NoBus, 3);

	IoCommon::Common ja_ligado = comum("c3", "2M", IoCommon::SupplyBus, 3);
	ja_ligado.wired = true;
	commons << ja_ligado;

	commons << comum("c4", "3M", IoCommon::SupplyBus, 8);
	commons << comum("c5", "1N", IoCommon::ReturnBus, 3);

	const IoCommon::Plan plan = IoCommon::plan(commons, buses);

	REQUIRE(plan.links.count() == 1);
	CHECK(!plan.isEmpty());
	CHECK(!plan.isClean());
	CHECK(plan.links.at(0).common_id == QStringLiteral("c1"));
	CHECK(plan.links.at(0).bus_id == QStringLiteral("b1"));
	CHECK(plan.links.at(0).bus_label == QStringLiteral("+24V"));
	CHECK(plan.links.at(0).folio == 3);

	SECTION("cada recusa com o seu motivo, na ordem em que foi pedida")
	{
		REQUIRE(plan.rejected.count() == 4);
		CHECK(plan.rejected.at(0).reason == IoCommon::NotACommon);
		CHECK(plan.rejected.at(0).common_id == QStringLiteral("c2"));
		CHECK(plan.rejected.at(1).reason == IoCommon::AlreadyWired);
		CHECK(plan.rejected.at(1).common_id == QStringLiteral("c3"));
		CHECK(plan.rejected.at(2).reason == IoCommon::NoBusOnFolio);
		CHECK(plan.rejected.at(2).common_id == QStringLiteral("c4"));
		CHECK(plan.rejected.at(3).reason == IoCommon::NoTerminalOnBus);
		CHECK(plan.rejected.at(3).common_id == QStringLiteral("c5"));
	}

	SECTION("o motivo tem texto, e o não-motivo não tem")
	{
		for (const IoCommon::Rejected &one : plan.rejected)
		{
			CHECK(!IoCommon::refusalText(one.reason, one.label)
			       .isEmpty());
			CHECK(IoCommon::refusalText(one.reason, one.label)
			      .contains(one.label));
		}
		CHECK(IoCommon::refusalText(IoCommon::NoRefusal,
					    QStringLiteral("1M")).isEmpty());
	}

	SECTION("o parágrafo diz os dois lados")
	{
		const QString text = plan.text();
		CHECK(text.contains(QStringLiteral("+24V")));
		CHECK(text.contains(QStringLiteral("1N")));
		CHECK(text.contains(QStringLiteral("3M")));
		CHECK(text.contains(QStringLiteral("2M")));
	}

	SECTION("plano vazio diz que não vai ligar nada")
	{
		const IoCommon::Plan nada =
				IoCommon::plan(QList<IoCommon::Common>(),
					       buses);
		CHECK(nada.isEmpty());
		CHECK(nada.isClean());
		CHECK(nada.text().contains(QStringLiteral("Aucun")));
	}
}

TEST_CASE("T11 — oito cartões numa ação só, e rodar de novo não duplica "
	  "fio",
	  "[iocommon]")
{
	// Four folios, two cards each, and the same two rails drawn again on
	// every folio under the same name - which is how a folio drawn to IEC
	// repeats a potential.
	QList<IoCommon::Bus> buses;
	QList<IoCommon::Common> commons;
	for (int folio = 1 ; folio <= 4 ; ++folio)
	{
		buses << barra(QStringLiteral("supply-%1").arg(folio),
			       "+24V", IoCommon::SupplyBus, folio, true,
			       24, 24);
		buses << barra(QStringLiteral("return-%1").arg(folio),
			       "0V", IoCommon::ReturnBus, folio, true,
			       24, 24);

		for (int card = 0 ; card < 2 ; ++card)
		{
			const QString stem = QStringLiteral("f%1c%2")
					     .arg(folio).arg(card);
			commons << comum(stem + QStringLiteral("-m"),
					 "1M", IoCommon::SupplyBus, folio);
			commons << comum(stem + QStringLiteral("-n"),
					 "1N", IoCommon::ReturnBus, folio);
		}
	}

	REQUIRE(commons.count() == 16);

	const IoCommon::Plan plan = IoCommon::plan(commons, buses);

	CHECK(plan.isClean());
	CHECK(plan.links.count() == 16);
	CHECK(plan.wires(IoCommon::SupplyBus) == 8);
	CHECK(plan.wires(IoCommon::ReturnBus) == 8);

	SECTION("o mesmo potencial repetido em quatro folhas continua sendo "
		"duas barras no relatório")
	{
		const QStringList labels = plan.busLabels();
		CHECK(labels.count() == 2);
		CHECK(labels.contains(QStringLiteral("+24V")));
		CHECK(labels.contains(QStringLiteral("0V")));
	}

	SECTION("rodar de novo não religa nada")
	{
		QList<IoCommon::Common> de_novo;
		for (IoCommon::Common one : commons)
		{
			one.wired = true;
			de_novo << one;
		}
		const IoCommon::Plan segunda =
				IoCommon::plan(de_novo, buses);
		CHECK(segunda.isEmpty());
		CHECK(segunda.rejected.count() == 16);
		for (const IoCommon::Rejected &one : segunda.rejected) {
			CHECK(one.reason == IoCommon::AlreadyWired);
		}
	}

	SECTION("marcar a barra que faltava e rodar de novo liga só o que "
		"sobrou")
	{
		// The folio 4 has no bar yet, so its four commons are refused
		// by name, and the twelve others are drawn.
		QList<IoCommon::Bus> tres;
		for (const IoCommon::Bus &bus : buses)
		{
			if (bus.folio != 4) {
				tres << bus;
			}
		}
		const IoCommon::Plan primeira =
				IoCommon::plan(commons, tres);
		CHECK(primeira.links.count() == 12);
		REQUIRE(primeira.rejected.count() == 4);
		for (const IoCommon::Rejected &one : primeira.rejected) {
			CHECK(one.reason == IoCommon::NoBusOnFolio);
		}

		// The bar is marked, and what was already drawn says so.
		QList<IoCommon::Common> depois;
		for (IoCommon::Common one : commons)
		{
			one.wired = (one.folio != 4);
			depois << one;
		}
		const IoCommon::Plan segunda =
				IoCommon::plan(depois, buses);
		CHECK(segunda.links.count() == 4);
		CHECK(segunda.rejected.count() == 12);
		for (const IoCommon::Link &link : segunda.links) {
			CHECK(link.folio == 4);
		}
	}
}
