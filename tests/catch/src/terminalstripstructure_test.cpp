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
#include "../../../sources/TerminalStrip/terminalstripstructure.h"
#include "qt_catch_tostring.h"

#include <QString>
#include <QVector>

/*
	A terminal strip as structure, and the rule that decides which of its
	terminals the program may bridge on its own.

	Both live away from Element and RealTerminal on purpose, so that what
	can be got wrong here - a fuse in the middle of a run, a wire number
	nobody filled in, a comb asked to step over a terminal - is settled by
	counting instead of by drawing a strip and looking at it.
*/

namespace
{
		/// @return one level of a terminal, carrying the given wire number
	StripLevel level(const char *label,
			 const char *wire,
			 ElementData::TerminalType type = ElementData::TTGeneric,
			 int bridge = StripLevel::NoBridge)
	{
		StripLevel level_;
		level_.label = QString::fromUtf8(label);
		level_.wire  = QString::fromUtf8(wire);
		level_.type  = type;
		level_.bridge = bridge;

		return level_;
	}

		/// @return a strip of single level terminals, in mounting order
	TerminalStripStructure strip(const QVector<StripLevel> &levels)
	{
		TerminalStripStructure structure;
		for (const auto &level_ : levels)
		{
			StripTerminal terminal;
			terminal.levels.append(level_);
			structure.terminals.append(terminal);
		}

		return structure;
	}

		/// @return the positions of the proposal at @a index, or an empty vector
	QVector<int> positionsOf(const QVector<StripBridgeProposal> &proposals, int index)
	{
		return index < proposals.size() ? proposals.at(index).positions
					        : QVector<int>{};
	}
}

TEST_CASE("T33 — o rótulo do nível conta a partir de T1, e o índice continua base 0",
	  "[terminalstrip]")
{
	SECTION("o que a oficina lê começa em T1")
	{
		CHECK(terminalLevelLabel(0) == QStringLiteral("T1"));
		CHECK(terminalLevelLabel(1) == QStringLiteral("T2"));
		CHECK(terminalLevelLabel(2) == QStringLiteral("T3"));
		CHECK(terminalLevelLabel(3) == QStringLiteral("T4"));
	}

	SECTION("índice que não existe não recebe rótulo inventado")
	{
		CHECK(terminalLevelLabel(-1).isEmpty());
	}

	SECTION("a altura da régua é a do borne mais alto")
	{
		TerminalStripStructure structure;
		CHECK(maximumLevelCount(structure) == 0);

		structure = strip({level("1", "L1"), level("2", "L2")});
		CHECK(maximumLevelCount(structure) == 1);

		structure.terminals[1].levels.append(level("2", "L3"));
		structure.terminals[1].levels.append(level("2", "L4"));
		CHECK(maximumLevelCount(structure) == 3);
	}
}

TEST_CASE("T33 — o que interrompe o borne por dentro decide se ele é passante",
	  "[terminalstrip]")
{
	SECTION("o borne comum e o de terra levam o mesmo fio dos dois lados")
	{
		CHECK(isThroughTerminalType(ElementData::TTGeneric));
		CHECK(isThroughTerminalType(ElementData::TTGround));
	}

	SECTION("o fusível, o seccionável e o de diodo têm um lado e outro")
	{
		CHECK_FALSE(isThroughTerminalType(ElementData::TTFuse));
		CHECK_FALSE(isThroughTerminalType(ElementData::TTSectional));
		CHECK_FALSE(isThroughTerminalType(ElementData::TTDiode));
	}
}

TEST_CASE("T33 — bornes vizinhos no mesmo fio viram uma ponte só",
	  "[terminalstrip]")
{
	const auto proposals = proposeBridgesByWire(
				strip({level("1", "L1"),
				       level("2", "L1"),
				       level("3", "L1")}));

	REQUIRE(proposals.size() == 1);
	CHECK(proposals.first().level == 0);
	CHECK(proposals.first().wire == QStringLiteral("L1"));
	CHECK(proposals.first().positions == QVector<int>{0, 1, 2});
}

TEST_CASE("T33 — borne sozinho no fio dele não faz ponte nenhuma",
	  "[terminalstrip]")
{
	const auto proposals = proposeBridgesByWire(
				strip({level("1", "L1"),
				       level("2", "L2"),
				       level("3", "L3")}));

	CHECK(proposals.isEmpty());
}

/*
	CU-33.2. O rótulo do caso não sobe para o nome do teste: o caso está na
	fila de tela do inventário, e o que está provado aqui é a regra, não o
	caso - inserir três bornes fusível numa régua e renumerar o projeto ainda
	pede uma folha aberta.
*/
TEST_CASE("T33 — o borne fusível não entra em ponte automática, e parte a fila em duas",
	  "[terminalstrip]")
{
	SECTION("três fusíveis no mesmo fio continuam três bornes soltos")
	{
		const auto proposals = proposeBridgesByWire(
					strip({level("1", "L1", ElementData::TTFuse),
					       level("2", "L1", ElementData::TTFuse),
					       level("3", "L1", ElementData::TTFuse)}));

		CHECK(proposals.isEmpty());
	}

	SECTION("o fusível no meio corta a fila, e cada metade ganha a sua ponte")
	{
		const auto proposals = proposeBridgesByWire(
					strip({level("1", "L1"),
					       level("2", "L1"),
					       level("3", "L1", ElementData::TTFuse),
					       level("4", "L1"),
					       level("5", "L1")}));

		REQUIRE(proposals.size() == 2);
		CHECK(positionsOf(proposals, 0) == QVector<int>{0, 1});
		CHECK(positionsOf(proposals, 1) == QVector<int>{3, 4});
	}

	SECTION("cortada a fila, a metade de um borne só não vira ponte")
	{
		const auto proposals = proposeBridgesByWire(
					strip({level("1", "L1"),
					       level("2", "L1", ElementData::TTSectional),
					       level("3", "L1")}));

		CHECK(proposals.isEmpty());
	}

	SECTION("o borne de terra é passante, e ponteia como qualquer outro")
	{
		const auto proposals = proposeBridgesByWire(
					strip({level("1", "PE", ElementData::TTGround),
					       level("2", "PE", ElementData::TTGround)}));

		REQUIRE(proposals.size() == 1);
		CHECK(positionsOf(proposals, 0) == QVector<int>{0, 1});
	}
}

TEST_CASE("T33 — fio que ninguém preencheu não é potencial nenhum",
	  "[terminalstrip]")
{
	SECTION("dois bornes sem número de fio não são dois bornes no mesmo fio")
	{
		const auto proposals = proposeBridgesByWire(
					strip({level("1", ""),
					       level("2", "")}));

		CHECK(proposals.isEmpty());
	}

	SECTION("o borne sem número corta a fila como o fusível corta")
	{
		const auto proposals = proposeBridgesByWire(
					strip({level("1", "L1"),
					       level("2", "L1"),
					       level("3", ""),
					       level("4", "L1"),
					       level("5", "L1")}));

		REQUIRE(proposals.size() == 2);
		CHECK(positionsOf(proposals, 0) == QVector<int>{0, 1});
		CHECK(positionsOf(proposals, 1) == QVector<int>{3, 4});
	}
}

TEST_CASE("T33 — a ponte é de um nível só, e não pula o borne que não tem aquele nível",
	  "[terminalstrip]")
{
	TerminalStripStructure structure;

		// Três bornes de dois níveis, o do meio com só um: o pente de baixo
		// atravessa os três, e o de cima não tem por onde passar no meio.
	StripTerminal two_levels_a;
	two_levels_a.levels.append(level("1", "L1"));
	two_levels_a.levels.append(level("1", "L2"));

	StripTerminal one_level;
	one_level.levels.append(level("2", "L1"));

	StripTerminal two_levels_b;
	two_levels_b.levels.append(level("3", "L1"));
	two_levels_b.levels.append(level("3", "L2"));

	structure.terminals.append(two_levels_a);
	structure.terminals.append(one_level);
	structure.terminals.append(two_levels_b);

	const auto proposals = proposeBridgesByWire(structure);

	REQUIRE(proposals.size() == 1);
	CHECK(proposals.first().level == 0);
	CHECK(proposals.first().wire == QStringLiteral("L1"));
	CHECK(positionsOf(proposals, 0) == QVector<int>{0, 1, 2});
}

TEST_CASE("T33 — dois níveis no mesmo fio dão dois pentes, um por nível",
	  "[terminalstrip]")
{
	TerminalStripStructure structure;
	for (int position = 0 ; position < 2 ; ++position)
	{
		StripTerminal terminal;
		terminal.levels.append(level("x", "L1"));
		terminal.levels.append(level("x", "L2"));
		structure.terminals.append(terminal);
	}

	const auto proposals = proposeBridgesByWire(structure);

	REQUIRE(proposals.size() == 2);
	CHECK(proposals.at(0).level == 0);
	CHECK(proposals.at(0).wire == QStringLiteral("L1"));
	CHECK(proposals.at(1).level == 1);
	CHECK(proposals.at(1).wire == QStringLiteral("L2"));
}

TEST_CASE("T33 — a ponte que já existe não é proposta de novo",
	  "[terminalstrip]")
{
	SECTION("fila inteira já ponteada não tem o que propor")
	{
		const auto proposals = proposeBridgesByWire(
					strip({level("1", "L1", ElementData::TTGeneric, 7),
					       level("2", "L1", ElementData::TTGeneric, 7)}));

		CHECK(proposals.isEmpty());
	}

	SECTION("o borne livre ao lado da ponte existente entra nela")
	{
		const auto proposals = proposeBridgesByWire(
					strip({level("1", "L1", ElementData::TTGeneric, 7),
					       level("2", "L1", ElementData::TTGeneric, 7),
					       level("3", "L1")}));

		REQUIRE(proposals.size() == 1);
		CHECK(positionsOf(proposals, 0) == QVector<int>{0, 1, 2});
	}

	SECTION("duas pontes diferentes não viram uma, mesmo no mesmo fio")
	{
		const auto proposals = proposeBridgesByWire(
					strip({level("1", "L1", ElementData::TTGeneric, 7),
					       level("2", "L1", ElementData::TTGeneric, 8),
					       level("3", "L1")}));

		REQUIRE(proposals.size() == 1);
		CHECK(positionsOf(proposals, 0) == QVector<int>{1, 2});
	}
}
