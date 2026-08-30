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
#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSettings>
#include <QString>
#include <QStringList>

#include "qt_catch_tostring.h"

#include "../../../sources/catalog/catalogassignment.h"
#include "../../../sources/ElementsCollection/pinoutblocktemplate.h"
#include "../../../sources/ElementsCollection/pinoutgenerator.h"
#include "../../../sources/ElementsCollection/pinoutusage.h"

namespace
{
	/**
		Put the pinout convention back where it was.

		Same reason as the guard in environment_test.cpp: the test binary
		has no organisation or application name, so QSettings writes to a
		file named after this executable rather than to the settings of
		QElectroTech - but rubbish left behind is rubbish either way.
	*/
	class ConventionGuard
	{
		public:
			ConventionGuard()
			{
				QSettings settings;
				m_previous = settings.value(
					QStringLiteral("pinout/convention")).toString();
			}

			~ConventionGuard()
			{
				QSettings settings;
				if (m_previous.isEmpty()) {
					settings.remove(QStringLiteral("pinout/convention"));
				} else {
					settings.setValue(
						QStringLiteral("pinout/convention"),
						m_previous);
				}
			}

		private:
			QString m_previous;
	};
	/**
		The pin the cases below build one at a time: a label, a role, and
		the place it holds in the list the manufacturer printed.
	*/
	CatalogPin pinAt(const QString &label, CatalogPinRole role, int order)
	{
		CatalogPin pin(label, role);
		pin.order_index = order;
		return pin;
	}

	/**
		A card of @a inputs points and the two commons that return them.

		Nothing else is filled in on purpose: a label, a role and an
		order is all T14 imports out of a manufacturer's table, and a
		generator that needed more than that would be a generator no
		imported part could be drawn with.
	*/
	CatalogPart inputCard(int inputs)
	{
		CatalogPart part;
		part.id = 1;
		part.class_id = 1;
		part.code = QStringLiteral("PCI-32E");
		part.revision = 3;
		for (int index = 0 ; index < inputs ; ++index) {
			part.pins << pinAt(QStringLiteral("I%1").arg(index),
					CatalogPinRole::Input, index);
		}
		for (int index = 0 ; index < 2 ; ++index) {
			part.pins << pinAt(QStringLiteral("C%1").arg(index),
					CatalogPinRole::ReturnCommon,
					inputs + index);
		}
		return part;
	}

	/**
		A card of @a channels analogue points, each one an input and the
		common that returns it, tied together by a channel.
	*/
	CatalogPart analogCard(int channels)
	{
		CatalogPart part;
		part.id = 2;
		part.class_id = 1;
		part.code = QStringLiteral("PCI-4AI");
		part.revision = 1;
		for (int index = 0 ; index < channels ; ++index)
		{
			const QString channel = QStringLiteral("CH%1").arg(index + 1);
			CatalogPin input = pinAt(QStringLiteral("AI%1").arg(index),
					CatalogPinRole::InputAnalog, index * 2);
			input.channel = channel;
			CatalogPin common = pinAt(QStringLiteral("AC%1").arg(index),
					CatalogPinRole::ReturnCommon,
					index * 2 + 1);
			common.channel = channel;
			part.pins << input << common;
		}
		return part;
	}

		/// the generator of a workshop that changed nothing: the standard
		/// template, the standard convention and the standard grid
	PinoutGenerator standardGenerator()
	{
		return PinoutGenerator(PinoutBlockTemplate(),
				PinoutConvention::iec(), SymbolGrid(),
				QStringLiteral("cartao-entrada"));
	}
}

/*
	T30 - the shape of the block a class is drawn as. Nothing is drawn here:
	what is measured is where each terminal falls along a side, how many
	blocks a pinout needs, and the two rules the specification insists on -
	everything in steps of the grid, and a class contradicts the convention
	on one kind of terminal, never on all of them.
*/
TEST_CASE("CU-30.5 — le modèle de bloc se mesure en pas de grille")
{
	const SymbolGrid grid;
	REQUIRE(grid.main_step == Approx(10.0));

	SECTION("o modelo em branco é a convenção do ambiente, sem mudança")
	{
		PinoutBlockTemplate model;
		QString error;
		CHECK(model.isNull());
		CHECK(model.isValid(&error));
		CHECK(error.isEmpty());
		CHECK(model.side_overrides.isEmpty());

			//And it draws by the convention, which is the whole meaning of
			//"declared nothing".
		const PinoutConvention convention = PinoutConvention::iec();
		CHECK(model.sideOf(CatalogPinRole::Input, convention) == Qet::North);
		CHECK(model.sideOf(CatalogPinRole::Output, convention) == Qet::South);
	}

	SECTION("aumentar a largura e o espaçamento muda o próximo bloco")
	{
		PinoutBlockTemplate model;
		model.width_steps = 6;
		model.pitch_steps = 4;
		CHECK_FALSE(model.isNull());
		CHECK(model.width(grid) == Approx(60.0));
		CHECK(model.pitch(grid) == Approx(40.0));
		CHECK(model.offsetOf(0, grid) == Approx(10.0));
		CHECK(model.offsetOf(3, grid) == Approx(130.0));
		CHECK(model.lengthFor(8, grid) == Approx(300.0));

			//A block that was generated carries a copy of the model, not a
			//view of it, which is what makes the second half of the use
			//case free: the cards already inserted are files on disk and
			//keep the drawing they were born with.
		const PinoutBlockTemplate as_generated = model;

		model.width_steps = 8;
		model.pitch_steps = 6;
		CHECK(model.width(grid) == Approx(80.0));
		CHECK(model.pitch(grid) == Approx(60.0));
		CHECK(model.offsetOf(3, grid) == Approx(190.0));

		CHECK(as_generated.width(grid) == Approx(60.0));
		CHECK(as_generated.pitch(grid) == Approx(40.0));
		CHECK(as_generated.offsetOf(3, grid) == Approx(130.0));
	}

	SECTION("nenhuma medida cai fora da grade")
	{
		PinoutBlockTemplate model;
		model.width_steps = 5;
		model.pitch_steps = 3;
		model.margin_steps = 2;

			//A grid of another step, because the point is not the number
			//ten: it is that every length asked of the model comes out a
			//whole number of steps, so a conductor always finds the terminal.
		const SymbolGrid coarse(25.0, 5.0);
		for (int index = 0 ; index < 12 ; ++index)
		{
			const qreal offset = model.offsetOf(index, coarse);
			CHECK(coarse.isOnMain(QPointF(0.0, offset)));
			CHECK(model.lengthFor(index + 1, coarse)
			      == Approx(2 * model.margin(coarse)
					+ index * model.pitch(coarse)));
		}
		CHECK(model.width(coarse) == Approx(125.0));
		CHECK(model.lengthFor(0, coarse) == Approx(0.0));
		CHECK(model.lengthFor(1, coarse) == Approx(100.0));
	}

	SECTION("uma placa de 32 pontos se parte em blocos que cabem na folha")
	{
		PinoutBlockTemplate model;
		CHECK(model.max_terminals == 0);
		CHECK(model.blocksFor(32) == 1);
		CHECK(model.blocksFor(0) == 0);

		model.max_terminals = 20;
		CHECK(model.blocksFor(32) == 2);
		CHECK(model.blocksFor(20) == 1);
		CHECK(model.blocksFor(21) == 2);
		CHECK(model.blocksFor(40) == 2);
		CHECK(model.blocksFor(41) == 3);
		CHECK(model.blocksFor(0) == 0);
	}

	SECTION("o modelo vai e volta pelo XML da classe")
	{
		PinoutBlockTemplate model;
		model.width_steps = 8;
		model.pitch_steps = 6;
		model.margin_steps = 2;
		model.max_terminals = 20;
		model.setOverride(CatalogPinRole::SupplyCommon, Qet::West,
				  QStringLiteral("A fonte alimenta pela lateral."));

		const PinoutBlockTemplate reread =
			PinoutBlockTemplate::fromXml(model.toXml());
		CHECK(reread.width_steps == 8);
		CHECK(reread.pitch_steps == 6);
		CHECK(reread.margin_steps == 2);
		CHECK(reread.max_terminals == 20);
		REQUIRE(reread.side_overrides.size() == 1);
		CHECK(reread.side_overrides.first().role == CatalogPinRole::SupplyCommon);
		CHECK(reread.side_overrides.first().side == Qet::West);
		CHECK(reread.side_overrides.first().reason
		      == QStringLiteral("A fonte alimenta pela lateral."));

			//A class that never declared one has an empty column, and an
			//empty column has to read as "the convention, unchanged" - not
			//as an error, because that is what every class had before this.
		CHECK(PinoutBlockTemplate::fromXml(QString()).isNull());
		CHECK(PinoutBlockTemplate::fromXml(QStringLiteral("<outra-coisa/>")).isNull());
	}

	SECTION("o modelo recusa o que não se desenha")
	{
		QString error;
		PinoutBlockTemplate model;

		model.width_steps = 1;
		CHECK_FALSE(model.isValid(&error));
		CHECK_FALSE(error.isEmpty());
		model.width_steps = 6;

		model.pitch_steps = 0;
		CHECK_FALSE(model.isValid(&error));
		model.pitch_steps = 1;

		model.margin_steps = -1;
		CHECK_FALSE(model.isValid(&error));
		model.margin_steps = 1;

		CHECK(model.isValid(&error));
	}
}

/*
	T30 - the convention of sides. It belongs to the drawing office and not to
	each block, so that a company sharing one folder shares one convention.
*/
TEST_CASE("CU-30.5 — la convention de côtés appartient à l'atelier")
{
	SECTION("a CEI põe entrada em cima e saída embaixo")
	{
		const PinoutConvention convention = PinoutConvention::iec();
		QString error;
		REQUIRE(convention.isValid(&error));
		CHECK(error.isEmpty());
		CHECK(convention.key == QStringLiteral("iec"));

		CHECK(convention.sideOf(CatalogPinRole::Input) == Qet::North);
		CHECK(convention.sideOf(CatalogPinRole::InputAnalog) == Qet::North);
		CHECK(convention.sideOf(CatalogPinRole::SupplyCommon) == Qet::North);
		CHECK(convention.sideOf(CatalogPinRole::Output) == Qet::South);
		CHECK(convention.sideOf(CatalogPinRole::OutputAnalog) == Qet::South);
		CHECK(convention.sideOf(CatalogPinRole::OutputRelay) == Qet::South);
		CHECK(convention.sideOf(CatalogPinRole::ReturnCommon) == Qet::South);
			//Not a point of the field: it goes to the side rather than
			//taking a row away from the points.
		CHECK(convention.sideOf(CatalogPinRole::CommPort) == Qet::West);
	}

	SECTION("a convenção horizontal gira o bloco um quarto de volta")
	{
		const PinoutConvention convention = PinoutConvention::horizontal();
		QString error;
		REQUIRE(convention.isValid(&error));
		CHECK(convention.key == QStringLiteral("horizontal"));

		CHECK(convention.sideOf(CatalogPinRole::Input) == Qet::East);
		CHECK(convention.sideOf(CatalogPinRole::SupplyCommon) == Qet::East);
		CHECK(convention.sideOf(CatalogPinRole::Output) == Qet::West);
		CHECK(convention.sideOf(CatalogPinRole::ReturnCommon) == Qet::West);
		CHECK(convention.sideOf(CatalogPinRole::CommPort) == Qet::South);

		CHECK(PinoutConvention::builtinConventions().size() == 2);
		CHECK_FALSE(PinoutConvention::translatedName(
				QStringLiteral("iec")).isEmpty());
		CHECK_FALSE(PinoutConvention::translatedName(
				QStringLiteral("nenhuma")).isEmpty());
	}

	SECTION("uma convenção tem de colocar todo papel que vai encontrar")
	{
		const QList<CatalogPinRole> roles = PinoutConvention::conventionalRoles();
		CHECK(roles.size() == CatalogPin::allRoles().size() - 1);
		CHECK_FALSE(roles.contains(CatalogPinRole::Unknown));

		for (const PinoutConvention &convention :
		     PinoutConvention::builtinConventions())
		{
			for (const CatalogPinRole role : roles) {
				CHECK(convention.declares(role));
			}
		}

			//A convention that forgot a role is refused, and the message
			//names the role: a generator meeting an unplaced role would
			//have to invent a side, which is what a convention exists to
			//stop.
		PinoutConvention incomplete = PinoutConvention::iec();
		incomplete.sides.remove(CatalogPinRole::OutputRelay);
		QString error;
		CHECK_FALSE(incomplete.isValid(&error));
		CHECK(error.contains(
			CatalogPin::translatedRoleName(CatalogPinRole::OutputRelay)));
	}

	SECTION("a convenção vai e volta pelo XML, pelo nome do papel")
	{
		const PinoutConvention convention = PinoutConvention::horizontal();
		const PinoutConvention reread =
			PinoutConvention::fromXml(convention.toXml());
		CHECK(reread.key == convention.key);
		CHECK(reread.sides == convention.sides);
		CHECK(reread.isValid());

			//Nothing readable means CEI, which is the convention of the
			//house until somebody says otherwise.
		CHECK(PinoutConvention::fromXml(QString()).key == QStringLiteral("iec"));
		CHECK(PinoutConvention::fromXml(QStringLiteral("<lixo/>")).key
		      == QStringLiteral("iec"));
	}

	SECTION("a classe contraria a convenção num tipo, nunca em todos")
	{
		const PinoutConvention convention = PinoutConvention::iec();
		PinoutBlockTemplate model;
		QString error;

			//A power supply has no "input on top" in the sense a card of a
			//controller has, so the class is allowed to disagree - once it
			//says why.
		model.setOverride(CatalogPinRole::SupplyCommon, Qet::West,
				  QStringLiteral("A fonte alimenta pela lateral."));
		CHECK(model.overridesSide(CatalogPinRole::SupplyCommon));
		CHECK(model.sideOf(CatalogPinRole::SupplyCommon, convention) == Qet::West);
		CHECK(model.sideOf(CatalogPinRole::Input, convention) == Qet::North);
		CHECK(model.isValid(&error));

			//Setting it again replaces it: pressing the button twice must
			//not build the pair that would be refused below.
		model.setOverride(CatalogPinRole::SupplyCommon, Qet::East,
				  QStringLiteral("Corrigido: alimenta pela direita."));
		CHECK(model.side_overrides.size() == 1);
		CHECK(model.sideOf(CatalogPinRole::SupplyCommon, convention) == Qet::East);

			//An exception nobody can explain is one made by accident.
		model.setOverride(CatalogPinRole::SupplyCommon, Qet::East, QString());
		CHECK_FALSE(model.isValid(&error));
		CHECK(error.contains(
			CatalogPin::translatedRoleName(CatalogPinRole::SupplyCommon)));

		model.clearOverride(CatalogPinRole::SupplyCommon);
		CHECK_FALSE(model.overridesSide(CatalogPinRole::SupplyCommon));
		CHECK(model.sideOf(CatalogPinRole::SupplyCommon, convention) == Qet::North);
		CHECK(model.isNull());

			//And the rule that keeps an exception an exception: contradict
			//the convention on every kind of terminal and it is a second
			//convention, after which nobody knows which one is the company's.
		const QList<CatalogPinRole> roles = PinoutConvention::conventionalRoles();
		for (const CatalogPinRole role : roles)
		{
			model.setOverride(role, Qet::East,
					  QStringLiteral("porque sim"));
		}
		CHECK(model.side_overrides.size() == roles.size());
		CHECK_FALSE(model.isValid(&error));
		CHECK_FALSE(error.isEmpty());

		model.clearOverride(roles.last());
		CHECK(model.isValid(&error));
	}

	SECTION("o lado oposto é para onde o borne vai ao atravessar o bloco")
	{
		CHECK(PinoutConvention::opposite(Qet::North) == Qet::South);
		CHECK(PinoutConvention::opposite(Qet::South) == Qet::North);
		CHECK(PinoutConvention::opposite(Qet::East) == Qet::West);
		CHECK(PinoutConvention::opposite(Qet::West) == Qet::East);

		const QList<Qet::Orientation> sides =
			{ Qet::North, Qet::East, Qet::South, Qet::West };
		for (const Qet::Orientation side : sides)
		{
			CHECK(PinoutConvention::opposite(
				PinoutConvention::opposite(side)) == side);
		}
	}

	SECTION("a convenção mora no ambiente e não em cada bloco")
	{
		ConventionGuard guard;
		PinoutConvention::clearCurrent();

		CHECK_FALSE(PinoutConvention::isConfigured());
		CHECK(PinoutConvention::current().key == QStringLiteral("iec"));

		PinoutConvention::setCurrent(PinoutConvention::horizontal());
		CHECK(PinoutConvention::isConfigured());
		const PinoutConvention current = PinoutConvention::current();
		CHECK(current.key == QStringLiteral("horizontal"));
		CHECK(current.sideOf(CatalogPinRole::Input) == Qet::East);

		PinoutConvention::clearCurrent();
		CHECK_FALSE(PinoutConvention::isConfigured());
		CHECK(PinoutConvention::current().key == QStringLiteral("iec"));
	}
}

TEST_CASE("CU-30.1 — le bloc sort de la liste des bornes", "[pinout]")
{
	const SymbolGrid grid;
	const PinoutGenerator generator = standardGenerator();
	const CatalogPart part = inputCard(32);

	const QList<SymbolDefinition> blocks = generator.generate(part);
	REQUIRE(blocks.size() == 1);
	const SymbolDefinition block = blocks.first();

	SECTION("um bloco só, com uma borna para cada pino da lista")
	{
		CHECK(block.terminals.size() == 34);
		CHECK(block.name == QStringLiteral("PCI-32E"));
		CHECK(block.class_key == QStringLiteral("cartao-entrada"));
	}

	SECTION("o corpo mede o lado mais longo, e a largura da classe é o mínimo")
	{
		REQUIRE(block.shapes.size() == 1);
		const QRectF body = block.shapes.first().bounds();
			//Trinta e duas bornas espaçadas de um passo, mais a
			//margem dos dois lados: 2x10 + 31x10.
		CHECK(body.width() == Approx(330.0));
			//Nada nas laterais, então a altura é a largura que a
			//classe pede — seis passos.
		CHECK(body.height() == Approx(60.0));
	}

	SECTION("a entrada vai para cima e o comum de retorno para baixo")
	{
		CHECK(block.terminals.at(0).orientation == Qet::North);
		CHECK(block.terminals.at(31).orientation == Qet::North);
		CHECK(block.terminals.at(32).orientation == Qet::South);
		CHECK(block.terminals.at(33).orientation == Qet::South);

		CHECK(block.terminals.at(0).position == QPointF(10.0, 0.0));
		CHECK(block.terminals.at(31).position == QPointF(320.0, 0.0));
		CHECK(block.terminals.at(32).position == QPointF(10.0, 60.0));
		CHECK(block.terminals.at(33).position == QPointF(20.0, 60.0));

		CHECK(block.terminals.at(0).label == QStringLiteral("I0"));
		CHECK(block.terminals.at(32).label == QStringLiteral("C0"));
	}

	SECTION("cada borna leva o seu próprio número, e não há texto solto")
	{
		for (const SymbolTerminal &terminal : block.terminals)
		{
			CHECK(terminal.show_name);
			CHECK_FALSE(terminal.label.isEmpty());
		}
			//Um texto, e é a etiqueta do bloco. É esta a promessa
			//da T30: as placas do projeto real levam 248 textos
			//soltos em volta de 197 bornas chamadas "".
		REQUIRE(block.texts.size() == 1);
		CHECK(block.texts.first().info_key == QStringLiteral("label"));
		CHECK(block.texts.first().position == QPointF(165.0, -5.0));
		CHECK(block.texts.first().alignment ==
				Qt::Alignment(Qt::AlignHCenter | Qt::AlignBottom));
	}

	SECTION("o número fica ao lado da borna, fora da grade de propósito")
	{
		CHECK(block.terminals.at(0).label_pos == QPointF(0.0, 2.0));
		CHECK(block.terminals.at(32).label_pos == QPointF(0.0, -2.0));
	}

	SECTION("o ponto de inserção é a primeira borna, e o bloco pode ser gravado")
	{
		CHECK(block.hotspot == QPointF(10.0, 0.0));
		CHECK(block.problems(grid).isEmpty());
		CHECK(block.canBeSaved(grid));
	}

	SECTION("o código da peça viaja como informação, senão o vínculo some ao gravar")
	{
		CHECK(block.default_part_code == QStringLiteral("PCI-32E"));
		CHECK(block.default_part_values.size() == 2);
		CHECK(block.default_part_values.value(
				CatalogAssignment::partCodeKey()) ==
				QStringLiteral("PCI-32E"));
		CHECK(block.default_part_values.value(
				CatalogAssignment::partRevisionKey()) ==
				QStringLiteral("3"));
	}

	SECTION("ida e volta pelo arquivo guarda o vínculo, o número e o espaçamento")
	{
		const QDomDocument document = block.toXml();
		const SymbolDefinition read =
				SymbolDefinition::fromXml(document.documentElement());

		CHECK(read.name == QStringLiteral("PCI-32E"));
		CHECK(read.class_key == QStringLiteral("cartao-entrada"));
		CHECK(read.default_part_code == QStringLiteral("PCI-32E"));
		CHECK(read.default_part_values.value(
				CatalogAssignment::partRevisionKey()) ==
				QStringLiteral("3"));

		REQUIRE(read.terminals.size() == 34);
		CHECK(read.terminals.at(0).show_name);
		CHECK(read.terminals.at(0).label == QStringLiteral("I0"));
		CHECK(read.terminals.at(0).orientation == Qet::North);
		CHECK(read.terminals.at(0).label_pos == QPointF(0.0, 2.0));

			//O arquivo não guarda onde na folha o desenho estava,
			//então o que tem de voltar é a distância entre dois
			//pontos de ligação, e não o lugar deles.
		const QPointF step = read.terminals.at(1).position
				- read.terminals.at(0).position;
		CHECK(step.x() == Approx(10.0));
		CHECK(step.y() == Approx(0.0));

		REQUIRE(read.texts.size() == 1);
		CHECK(read.texts.first().info_key == QStringLiteral("label"));
		CHECK(read.texts.first().alignment ==
				Qt::Alignment(Qt::AlignHCenter | Qt::AlignBottom));
	}

	SECTION("sem classe, sem grade ou sem peça não sai desenho nenhum")
	{
		QString error;

		PinoutGenerator without_class = standardGenerator();
		without_class.class_key.clear();
		CHECK_FALSE(without_class.isValid(&error));
		CHECK_FALSE(error.isEmpty());
		CHECK(without_class.generate(part).isEmpty());

		PinoutGenerator without_grid = standardGenerator();
		without_grid.grid = SymbolGrid(0.0, 1.0);
		CHECK_FALSE(without_grid.isValid());
		CHECK(without_grid.generate(part).isEmpty());

		PinoutGenerator narrow = standardGenerator();
		narrow.block_template.width_steps = 1;
		CHECK_FALSE(narrow.isValid());
		CHECK(narrow.generate(part).isEmpty());

		PinoutGenerator nameless = standardGenerator();
		nameless.convention.key.clear();
		CHECK_FALSE(nameless.isValid());
		CHECK(nameless.generate(part).isEmpty());

			//E uma peça que não existe não vira símbolo, mesmo com
			//tudo o mais em ordem.
		CHECK(generator.isValid());
		CHECK(generator.generate(CatalogPart()).isEmpty());
	}
}

TEST_CASE("CU-30.2 — une pinoute trop longue se coupe en blocs", "[pinout]")
{
	const SymbolGrid grid;
	PinoutGenerator generator = standardGenerator();

	SECTION("onze pinos soltos com teto de seis saem em dois blocos numerados")
	{
		generator.block_template.max_terminals = 6;

		CatalogPart part;
		part.id = 3;
		part.code = QStringLiteral("INV-7K5");
		part.revision = 1;
		for (int index = 0 ; index < 11 ; ++index) {
			part.pins << pinAt(QStringLiteral("X%1").arg(index),
					CatalogPinRole::Input, index);
		}

		const QList<SymbolDefinition> blocks = generator.generate(part);
		REQUIRE(blocks.size() == 2);
		CHECK(blocks.at(0).terminals.size() == 6);
		CHECK(blocks.at(1).terminals.size() == 5);
			//O número do bloco só aparece quando há um segundo: o
			//número de uma coisa única é ruído.
		CHECK(blocks.at(0).name == QStringLiteral("INV-7K5 1/2"));
		CHECK(blocks.at(1).name == QStringLiteral("INV-7K5 2/2"));
		CHECK(PinoutGenerator::blockName(part, 0, 1) ==
				QStringLiteral("INV-7K5"));
	}

	SECTION("um canal não se parte: quatro canais com teto de três dão quatro blocos")
	{
		generator.block_template.max_terminals = 3;

		const QList<SymbolDefinition> blocks =
				generator.generate(analogCard(4));
		REQUIRE(blocks.size() == 4);
		for (const SymbolDefinition &block : blocks)
		{
				//Dois, e não três: a entrada e o comum que a
				//devolve são um ponto do campo, e um ponto
				//partido em dois blocos é um ponto que nenhuma
				//lista junta de volta.
			CHECK(block.terminals.size() == 2);
		}
	}

	SECTION("um par não se parte: metade de um contato é um símbolo que não grava")
	{
		generator.block_template.max_terminals = 3;

		CatalogPart part;
		part.id = 4;
		part.code = QStringLiteral("CT-3NA");
		part.revision = 1;
		for (int index = 0 ; index < 3 ; ++index)
		{
			const QString pair = QStringLiteral("%1").arg(index + 1);
				//13/14, 23/24, 33/34 — a numeração que a norma
				//dá a um contato aberto.
			CatalogPin in = pinAt(
					QString::number((index + 1) * 10 + 3),
					CatalogPinRole::ContactNo, index * 2);
			in.pair = pair;
			CatalogPin out = pinAt(
					QString::number((index + 1) * 10 + 4),
					CatalogPinRole::ContactNo,
					index * 2 + 1);
			out.pair = pair;
			part.pins << in << out;
		}

		const QList<SymbolDefinition> blocks = generator.generate(part);
		REQUIRE(blocks.size() == 3);
		for (const SymbolDefinition &block : blocks)
		{
			CHECK(block.terminals.size() == 2);
			CHECK(block.problems(grid).isEmpty());
			CHECK(block.canBeSaved(grid));
		}
	}

	SECTION("uma unidade maior que o teto ganha um bloco só para ela")
	{
		generator.block_template.max_terminals = 3;

		CatalogPart part;
		part.id = 5;
		part.code = QStringLiteral("PCI-1AI");
		part.revision = 1;
		for (int index = 0 ; index < 4 ; ++index)
		{
			CatalogPin pin = pinAt(QStringLiteral("A%1").arg(index),
					CatalogPinRole::InputAnalog, index);
			pin.channel = QStringLiteral("CH1");
			part.pins << pin;
		}
		part.pins << pinAt(QStringLiteral("V+"),
				CatalogPinRole::Terminal, 4);
		part.pins << pinAt(QStringLiteral("V-"),
				CatalogPinRole::Terminal, 5);

		const QList<SymbolDefinition> blocks = generator.generate(part);
		REQUIRE(blocks.size() == 2);
			//O teto é uma preferência, e a unidade é um fato.
		CHECK(blocks.at(0).terminals.size() == 4);
		CHECK(blocks.at(1).terminals.size() == 2);
	}

	SECTION("sem teto o cartão inteiro sai num bloco só")
	{
		CHECK(generator.block_template.max_terminals == 0);
		const QList<SymbolDefinition> blocks =
				generator.generate(inputCard(32));
		REQUIRE(blocks.size() == 1);
		CHECK(blocks.first().terminals.size() == 34);
	}
}

TEST_CASE("CU-30.4 — la liste compte un point, la feuille montre deux bornes",
	  "[pinout]")
{
	const PinoutGenerator generator = standardGenerator();
	const CatalogPart part = analogCard(4);

	SECTION("a lista conta quatro canais, e cada um vale dois pinos")
	{
		CHECK(part.channelKeys().size() == 4);
		CHECK(part.pinsInChannel(QStringLiteral("CH1")).size() == 2);
		CHECK(part.pins.size() == 8);
	}

	SECTION("a folha mostra as oito bornas, e o canal decide o lado de cada uma")
	{
		const QList<SymbolDefinition> blocks = generator.generate(part);
		REQUIRE(blocks.size() == 1);
		const SymbolDefinition block = blocks.first();

		CHECK(block.terminals.size() == 8);

		int north = 0, south = 0;
		for (const SymbolTerminal &terminal : block.terminals)
		{
			if (terminal.orientation == Qet::North) {
				north++;
			} else if (terminal.orientation == Qet::South) {
				south++;
			}
		}
		CHECK(north == 4);
		CHECK(south == 4);

		REQUIRE(block.shapes.size() == 1);
		const QRectF body = block.shapes.first().bounds();
			//Quatro bornas de cada lado cabem em cinco passos, e a
			//classe pede seis: o corpo é o que a classe pede.
		CHECK(body.width() == Approx(60.0));
		CHECK(body.height() == Approx(60.0));
	}
}
TEST_CASE("CU-30.3 — une même borne ne se dessine pas deux fois", "[pinout]")
{
	PinoutGenerator generator = standardGenerator();

		//Onde o primeiro bloco foi parar. É o que a recusa vai ter de
		//dizer de volta: não basta recusar, tem de apontar o original.
	PinoutUsageEntry place;
	place.component = QStringLiteral("-U1");
	place.part_code = QStringLiteral("PCI-32E");
	place.sheet = QStringLiteral("Commande");
	place.sheet_number = 3;

	SECTION("as bornas do primeiro bloco ficam com dono, uma a uma")
	{
		const QList<SymbolDefinition> blocks =
				generator.generate(inputCard(4));
		REQUIRE(blocks.size() == 1);

		PinoutUsage usage;
		CHECK(usage.addSymbol(place, blocks.first()) == 6);
		CHECK(usage.count() == 6);
		CHECK(usage.holds(QStringLiteral("-U1"), QStringLiteral("I0")));
		CHECK(usage.holds(QStringLiteral("-U1"), QStringLiteral("C1")));
		CHECK_FALSE(usage.holds(QStringLiteral("-U1"),
				QStringLiteral("I9")));
		CHECK(usage.components().size() == 1);
		CHECK(usage.entriesOf(QStringLiteral("-U1")).size() == 6);
		CHECK(usage.entryOf(QStringLiteral("-U1"),
				QStringLiteral("I0")).sheet_number == 3);
	}

	SECTION("o mesmo bloco desenhado de novo é recusado, e diz onde está o original")
	{
		const QList<SymbolDefinition> blocks =
				generator.generate(inputCard(4));
		REQUIRE(blocks.size() == 1);
		const SymbolDefinition block = blocks.first();

		PinoutUsage usage;
		usage.addSymbol(place, block);

		const QList<PinoutUsageConflict> found =
				usage.conflicts(QStringLiteral("-U1"), block);
		REQUIRE(found.size() == 6);

		QStringList named;
		for (const PinoutUsageConflict &conflict : found) {
			named << conflict.terminal;
		}
		CHECK(named.contains(QStringLiteral("I0")));
		CHECK(named.contains(QStringLiteral("C1")));

			//A frase tem de nomear as três coisas que a pessoa precisa
			//para resolver: qual borna, de qual componente, e em que
			//folha está a que já existe.
		const PinoutUsageConflict conflict = found.first();
		CHECK(conflict.existing.component == QStringLiteral("-U1"));
		CHECK(conflict.existing.whereItIs()
				== QStringLiteral("folio 3 (Commande)"));
		CHECK(conflict.message() == QStringLiteral("La borne %1 de -U1 "
				"est deja dessinee sur le folio 3 (Commande).")
				.arg(conflict.terminal));

		CHECK_FALSE(usage.refusal(QStringLiteral("-U1"), block).isEmpty());
			//E nada entrou: quem chega depois não desenha metade.
		CHECK(usage.count() == 6);
	}

	SECTION("o acionamento partido em dois blocos entra inteiro, porque as bornas são outras")
	{
		generator.block_template.max_terminals = 20;
		const QList<SymbolDefinition> blocks =
				generator.generate(inputCard(32));
		REQUIRE(blocks.size() == 2);

		PinoutUsage usage;
		QList<PinoutUsageConflict> conflicts;
		int added = 0;
		for (const SymbolDefinition &block : blocks) {
			added += usage.addSymbol(place, block, &conflicts);
		}
			//As duas metades são um componente só, e é a etiqueta que
			//diz isso. O que não pode é a borna se repetir entre elas.
		CHECK(added == 34);
		CHECK(conflicts.isEmpty());
		CHECK(usage.count() == 34);
		CHECK(usage.components().size() == 1);
	}

	SECTION("a mesma borna em outro componente não é conflito nenhum")
	{
		const QList<SymbolDefinition> blocks =
				generator.generate(inputCard(4));
		REQUIRE(blocks.size() == 1);
		const SymbolDefinition block = blocks.first();

		PinoutUsage usage;
		usage.addSymbol(place, block);

			//-U1 e -U2 são dois acionamentos, e cada um tem a borna I0
			//dele sem nada de errado nisso.
		PinoutUsageEntry other = place;
		other.component = QStringLiteral("-U2");
		other.sheet = QStringLiteral("Puissance");
		other.sheet_number = 4;

		CHECK(usage.refusal(QStringLiteral("-U2"), block).isEmpty());

		QList<PinoutUsageConflict> conflicts;
		CHECK(usage.addSymbol(other, block, &conflicts) == 6);
		CHECK(conflicts.isEmpty());
		CHECK(usage.count() == 12);
		CHECK(usage.components().size() == 2);
	}

	SECTION("bloco sem etiqueta não é verificado")
	{
		const QList<SymbolDefinition> blocks =
				generator.generate(inputCard(4));
		REQUIRE(blocks.size() == 1);
		const SymbolDefinition block = blocks.first();

		PinoutUsage usage;
		usage.addSymbol(place, block);

		PinoutUsageEntry unnamed = place;
		unnamed.component.clear();

			//Nada entra e nada é recusado: bloco que ainda não foi
			//nomeado não é atribuível a componente nenhum, e recusar
			//por isso recusaria a maioria dos desenhos pela metade.
		QList<PinoutUsageConflict> conflicts;
		CHECK(usage.addSymbol(unnamed, block, &conflicts) == 0);
		CHECK(conflicts.isEmpty());
		CHECK(usage.count() == 6);
		CHECK(usage.refusal(QString(), block).isEmpty());
	}

	SECTION("borna sem nome não conta, que é o caso das placas do projeto real")
	{
		SymbolDefinition drawn;
		drawn.name = QStringLiteral("placa_ucm");
		for (int index = 0 ; index < 8 ; ++index) {
			drawn.terminals << SymbolTerminal(
					QPointF(index * 10.0, 0.0), Qet::North);
		}

			//As três placas do projeto real levam 197 bornas assim, e
			//verificá-las faria cada placa desenhada à mão colidir com
			//todas as outras.
		CHECK(drawn.terminals.size() == 8);
		CHECK(PinoutUsage::terminalsOf(drawn).isEmpty());

		PinoutUsage usage;
		CHECK(usage.addSymbol(place, drawn) == 0);
		CHECK(usage.isEmpty());
		CHECK(usage.refusal(QStringLiteral("-U1"), drawn).isEmpty());
	}

	SECTION("o bloco que repete uma borna dentro de si é recusado sem projeto nenhum")
	{
		SymbolDefinition drawn;
		drawn.name = QStringLiteral("bloco-a-mao");
		SymbolTerminal first(QPointF(0.0, 0.0), Qet::North);
		first.label = QStringLiteral("13");
		SymbolTerminal second(QPointF(10.0, 0.0), Qet::North);
		second.label = QStringLiteral("13");
		drawn.terminals << first << second;

		const QList<PinoutUsageConflict> found =
				PinoutUsage::selfConflicts(drawn);
		REQUIRE(found.size() == 1);
		CHECK(found.first().terminal == QStringLiteral("13"));
			//Sem componente e sem folha: o erro é do desenho, e está
			//errado em qualquer lugar que ele seja posto.
		CHECK(found.first().existing.component.isEmpty());
		CHECK(found.first().message() == QStringLiteral("La borne 13 est "
				"dessinee deux fois dans ce bloc."));

		const PinoutUsage usage;
		CHECK_FALSE(usage.refusal(QStringLiteral("-U9"), drawn).isEmpty());
	}

	SECTION("a mensagem para de contar em cinco, e diz quantas sobraram")
	{
		const QList<SymbolDefinition> blocks =
				generator.generate(inputCard(32));
		REQUIRE(blocks.size() == 1);
		const SymbolDefinition block = blocks.first();

		PinoutUsage usage;
		usage.addSymbol(place, block);

		const QString message =
				usage.refusal(QStringLiteral("-U1"), block);
		const QStringList lines = message.split(QStringLiteral("\n"));
			//Cinco frases e a conta do que sobrou. Trinta e quatro
			//frases iguais dizem menos que cinco.
		REQUIRE(lines.size() == 6);
		CHECK(lines.last()
				== QStringLiteral("... et 29 autre(s) borne(s)."));
		CHECK(PinoutUsage::messageFor(
				QList<PinoutUsageConflict>()).isEmpty());
	}
}

TEST_CASE("Le type du bloc — un seul genre de point, ou aucun", "[pinout]")
{
	PinoutGenerator generator = standardGenerator();

	SECTION("um cartão de entradas digitais tem um tipo só")
	{
		const QList<SymbolDefinition> blocks =
				generator.generate(inputCard(32));
		REQUIRE(blocks.size() == 1);

			//As duas bornas de comum de retorno estão lá e não
			//contam: comum de retorno há em todo cartão, e contá-lo
			//faria todo cartão ser misto.
		CHECK(inputCard(32).pins.size() == 34);
		CHECK(PinoutGenerator::ioRoleOf(inputCard(32).pins)
				== CatalogPinRole::Input);
	}

	SECTION("entradas e saídas no mesmo bloco não têm tipo nenhum")
	{
		CatalogPart mixed = inputCard(4);
		CatalogPin relay(QStringLiteral("Q0"),
				CatalogPinRole::OutputRelay);
		relay.order_index = 90;
		mixed.pins << relay;

			//Meia verdade sobre um bloco que tem os dois é pior que
			//o bloco não dizer nada de si.
		CHECK(PinoutGenerator::ioRoleOf(mixed.pins)
				== CatalogPinRole::Unknown);
	}

	SECTION("bloco sem ponto de campo nenhum não tem tipo")
	{
		CatalogPart supply;
		supply.id = 9;
		supply.code = QStringLiteral("PS-24");
		supply.pins << CatalogPin(QStringLiteral("L+"),
				CatalogPinRole::SupplyCommon)
				<< CatalogPin(QStringLiteral("M"),
				CatalogPinRole::ReturnCommon);

		CHECK(PinoutGenerator::ioRoleOf(supply.pins)
				== CatalogPinRole::Unknown);
		CHECK(PinoutGenerator::ioRoleOf(QList<CatalogPin>())
				== CatalogPinRole::Unknown);
	}

	SECTION("o cartão analógico responde pelo ponto, não pelo comum")
	{
		CHECK(PinoutGenerator::ioRoleOf(analogCard(4).pins)
				== CatalogPinRole::InputAnalog);
	}

	SECTION("cada bloco parcial responde por si, e não pela peça inteira")
	{
		CatalogPart mixed = inputCard(4);
		for (int index = 0 ; index < 4 ; ++index)
		{
			CatalogPin relay(QStringLiteral("Q%1").arg(index),
					CatalogPinRole::OutputRelay);
			relay.order_index = 90 + index;
			mixed.pins << relay;
		}

			//A peça inteira é mista; recortada em só as entradas,
			//o bloco volta a ter um tipo. É por bloco que a
			//pergunta se faz, porque é o bloco que vira componente.
		CHECK(PinoutGenerator::ioRoleOf(mixed.pins)
				== CatalogPinRole::Unknown);

		const QList<CatalogPin> only_inputs = generator.selectedPins(
				mixed, QStringList()
				<< QStringLiteral("I0")
				<< QStringLiteral("I1"));
		REQUIRE(only_inputs.size() == 2);
		CHECK(PinoutGenerator::ioRoleOf(only_inputs)
				== CatalogPinRole::Input);
	}
}
