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

#include <QList>
#include <QPointF>
#include <QSettings>
#include <QString>

#include "qt_catch_tostring.h"

#include "../../../sources/ElementsCollection/pinoutblocktemplate.h"

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
