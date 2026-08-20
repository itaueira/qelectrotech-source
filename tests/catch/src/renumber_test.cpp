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
#include "../../../sources/autoNum/numberingformat.h"
#include "../../../sources/autoNum/renumberplan.h"
#include "qt_catch_tostring.h"

namespace
{
	RenumberInput object(const QString &uuid,
			     const QString &root,
			     int folio_index,
			     qreal x,
			     qreal y,
			     const QString &current = QString())
	{
		RenumberInput input;
		input.uuid = uuid;
		input.root = root;
		input.folio_index = folio_index;
		input.position = QPointF(x, y);
		input.current = current;
		input.folio = QString::number(folio_index + 1);
		return input;
	}

	QStringList labelsOf(const RenumberPlan &plan)
	{
		QStringList labels;
		for (const RenumberEntry &entry : plan.entries) {
			labels.append(entry.to);
		}
		return labels;
	}

	NumberingFormat formatNamed(const QString &name)
	{
		const QList<NumberingFormat> formats = NumberingFormat::builtinFormats();
		for (const NumberingFormat &format : formats)
		{
			if (format.name == name) {
				return format;
			}
		}
		return NumberingFormat();
	}
}

TEST_CASE("CU-07.1 — trois moteurs sur trois folios naissent M1, M2, M3")
{
	// The tag root comes from the class, so the motor root is what the class
	// says: MTR outside the norm, M under IEC 81346. Both are exercised, and
	// the numbering does not care which - that is the point.
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("a"), QStringLiteral("M"), 0, 100, 100)
	       << object(QStringLiteral("b"), QStringLiteral("M"), 1, 100, 100)
	       << object(QStringLiteral("c"), QStringLiteral("M"), 2, 100, 100);

	const NumberingFormat sequential = formatNamed(
		QStringLiteral("Séquentiel"));
	REQUIRE_FALSE(sequential.isNull());

	const RenumberPlan plan = Renumberer::plan(inputs, sequential);
	CHECK(labelsOf(plan) == QStringList({ QStringLiteral("M1"),
					      QStringLiteral("M2"),
					      QStringLiteral("M3") }));
	CHECK_FALSE(plan.hasDuplicates());
	CHECK(plan.changeCount() == 3);

	// Under the house standard the very same objects give MTR1, MTR2, MTR3
	// without anything else changing.
	for (RenumberInput &input : inputs) {
		input.root = QStringLiteral("MTR");
	}
	const RenumberPlan house = Renumberer::plan(inputs, sequential);
	CHECK(labelsOf(house) == QStringList({ QStringLiteral("MTR1"),
					       QStringLiteral("MTR2"),
					       QStringLiteral("MTR3") }));
}

TEST_CASE("CU-07.1 — chaque racine compte à partir de un")
{
	// Contactors and motors on the same folio: K1, K2 and M1, not K1, M2.
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("k1"), QStringLiteral("K"), 0, 100, 100)
	       << object(QStringLiteral("m1"), QStringLiteral("M"), 0, 200, 100)
	       << object(QStringLiteral("k2"), QStringLiteral("K"), 0, 300, 100);

	const RenumberPlan plan = Renumberer::plan(inputs, formatNamed(QStringLiteral("Séquentiel")));
	CHECK(plan.labelFor(QStringLiteral("k1")) == QStringLiteral("K1"));
	CHECK(plan.labelFor(QStringLiteral("m1")) == QStringLiteral("M1"));
	CHECK(plan.labelFor(QStringLiteral("k2")) == QStringLiteral("K2"));
}

TEST_CASE("CU-07.3 — un composant effacé : renuméroter ferme le trou")
{
	// M1 and M3 are left after M2 was deleted. Renumbering makes M3 become M2,
	// and nothing else moves.
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("a"), QStringLiteral("M"), 0, 100, 100,
			 QStringLiteral("M1"))
	       << object(QStringLiteral("c"), QStringLiteral("M"), 0, 100, 300,
			 QStringLiteral("M3"));

	const RenumberPlan plan = Renumberer::plan(inputs, formatNamed(QStringLiteral("Séquentiel")));

	REQUIRE(plan.entries.size() == 2);
	CHECK(plan.entries.at(0).from == QStringLiteral("M1"));
	CHECK(plan.entries.at(0).to == QStringLiteral("M1"));
	CHECK_FALSE(plan.entries.at(0).changed);
	CHECK(plan.entries.at(1).from == QStringLiteral("M3"));
	CHECK(plan.entries.at(1).to == QStringLiteral("M2"));
	CHECK(plan.entries.at(1).changed);

	// One change, and the preview says so before anything is applied.
	CHECK(plan.changeCount() == 1);
}

TEST_CASE("CU-07.4 — l'ordre de lecture, dans les deux sens")
{
	// Four objects in a two by two grid:
	//   topleft   topright
	//   bottomleft bottomright
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("bottomright"), QStringLiteral("K"), 0, 300, 300)
	       << object(QStringLiteral("topleft"), QStringLiteral("K"), 0, 100, 100)
	       << object(QStringLiteral("bottomleft"), QStringLiteral("K"), 0, 100, 300)
	       << object(QStringLiteral("topright"), QStringLiteral("K"), 0, 300, 100);

	const NumberingFormat sequential = formatNamed(QStringLiteral("Séquentiel"));

	// Default: top to bottom, then left to right.
	const RenumberPlan rows_first = Renumberer::plan(inputs, sequential, false);
	CHECK(rows_first.labelFor(QStringLiteral("topleft")) == QStringLiteral("K1"));
	CHECK(rows_first.labelFor(QStringLiteral("topright")) == QStringLiteral("K2"));
	CHECK(rows_first.labelFor(QStringLiteral("bottomleft")) == QStringLiteral("K3"));
	CHECK(rows_first.labelFor(QStringLiteral("bottomright")) == QStringLiteral("K4"));

	// The other orientation: left to right, then top to bottom.
	const RenumberPlan columns_first = Renumberer::plan(inputs, sequential, true);
	CHECK(columns_first.labelFor(QStringLiteral("topleft")) == QStringLiteral("K1"));
	CHECK(columns_first.labelFor(QStringLiteral("bottomleft")) == QStringLiteral("K2"));
	CHECK(columns_first.labelFor(QStringLiteral("topright")) == QStringLiteral("K3"));
	CHECK(columns_first.labelFor(QStringLiteral("bottomright")) == QStringLiteral("K4"));
}

TEST_CASE("CU-07.4 — deux pixels de décalage ne changent pas la ligne")
{
	// A symbol dropped three pixels above its neighbour is on the same row as
	// far as a person reading the drawing is concerned. Comparing raw
	// coordinates would order these two by an accident of the mouse.
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("right"), QStringLiteral("K"), 0, 300, 100)
	       << object(QStringLiteral("left"), QStringLiteral("K"), 0, 100, 103);

	const RenumberPlan plan = Renumberer::plan(inputs,
						   formatNamed(QStringLiteral("Séquentiel")));
	CHECK(plan.labelFor(QStringLiteral("left")) == QStringLiteral("K1"));
	CHECK(plan.labelFor(QStringLiteral("right")) == QStringLiteral("K2"));
}

TEST_CASE("CU-07.4 — l'ordre est le même à chaque exécution")
{
	// Two objects at the very same place. Whatever order is chosen, it has to
	// be the same one on every station and every run, or two people
	// renumbering the same project disagree.
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("zebra"), QStringLiteral("K"), 0, 100, 100)
	       << object(QStringLiteral("alpha"), QStringLiteral("K"), 0, 100, 100);

	const NumberingFormat sequential = formatNamed(QStringLiteral("Séquentiel"));
	const RenumberPlan first = Renumberer::plan(inputs, sequential);

	QList<RenumberInput> reversed;
	reversed << inputs.at(1) << inputs.at(0);
	const RenumberPlan second = Renumberer::plan(reversed, sequential);

	CHECK(first.labelFor(QStringLiteral("alpha")) == QStringLiteral("K1"));
	CHECK(second.labelFor(QStringLiteral("alpha")) == QStringLiteral("K1"));
	CHECK(first.labelFor(QStringLiteral("zebra")) == second.labelFor(QStringLiteral("zebra")));
}

TEST_CASE("CU-07.5 — un numéro mis à la main n'est jamais renuméroté")
{
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("a"), QStringLiteral("W"), 0, 100, 100,
			 QStringLiteral("W1"))
	       << object(QStringLiteral("manual"), QStringLiteral("W"), 0, 100, 200,
			 QStringLiteral("24 Vcc"))
	       << object(QStringLiteral("c"), QStringLiteral("W"), 0, 100, 300,
			 QStringLiteral("W3"));
	inputs[1].frozen = true;

	const RenumberPlan plan = Renumberer::plan(inputs,
						   formatNamed(QStringLiteral("Séquentiel")));

	// The frozen one keeps its label, and shows up in the preview marked as
	// skipped: the user has to see that it was left alone, not wonder why.
	CHECK(plan.labelFor(QStringLiteral("manual")) == QStringLiteral("24 Vcc"));
	CHECK(plan.frozenCount() == 1);
	REQUIRE(plan.entries.size() == 3);
	CHECK(plan.entries.at(1).frozen);
	CHECK_FALSE(plan.entries.at(1).changed);

	// And it does not eat a number: the two that are renumbered are W1 and W2.
	CHECK(plan.labelFor(QStringLiteral("a")) == QStringLiteral("W1"));
	CHECK(plan.labelFor(QStringLiteral("c")) == QStringLiteral("W2"));
}

TEST_CASE("CU-07.6 — collision : on sait qui a déjà ce repère")
{
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("first"), QStringLiteral("M"), 0, 100, 100,
			 QStringLiteral("M1"))
	       << object(QStringLiteral("second"), QStringLiteral("M"), 0, 100, 200,
			 QStringLiteral("M2"));

	// "Go to" needs the object, not only a refusal.
	CHECK(Renumberer::holderOf(QStringLiteral("M1"), inputs) == QStringLiteral("first"));
	CHECK(Renumberer::holderOf(QStringLiteral("M9"), inputs).isEmpty());
	// Asking about oneself is not a collision.
	CHECK(Renumberer::holderOf(QStringLiteral("M1"), inputs,
				   QStringLiteral("first")).isEmpty());

	CHECK_FALSE(Renumberer::isLabelFree(QStringLiteral("M1"), QString(), inputs));
	CHECK(Renumberer::isLabelFree(QStringLiteral("M3"), QString(), inputs));
}

TEST_CASE("CU-07.6 — le même repère dans deux localisations est légitime")
{
	// Two panels may each have their own -Q1, and the check has to know that.
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("panel-a"), QStringLiteral("Q"), 0, 100, 100,
			 QStringLiteral("Q1"))
	       << object(QStringLiteral("panel-b"), QStringLiteral("Q"), 1, 100, 100,
			 QStringLiteral("Q1"));
	inputs[0].location = QStringLiteral("+A1");
	inputs[1].location = QStringLiteral("+A2");

	CHECK(Renumberer::isLabelFree(QStringLiteral("Q1"), QStringLiteral("+A3"), inputs));
	CHECK_FALSE(Renumberer::isLabelFree(QStringLiteral("Q1"), QStringLiteral("+A1"), inputs));
}

TEST_CASE("CU-07.2 — le format par folio recommence à chaque folio")
{
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("a"), QStringLiteral("M"), 0, 100, 100)
	       << object(QStringLiteral("b"), QStringLiteral("M"), 0, 100, 200)
	       << object(QStringLiteral("c"), QStringLiteral("M"), 1, 100, 100);

	const NumberingFormat with_folio = formatNamed(
		QStringLiteral("Avec le numéro de folio"));
	REQUIRE_FALSE(with_folio.isNull());
	CHECK(with_folio.scope == NumberingScope::Folio);

	const RenumberPlan plan = Renumberer::plan(inputs, with_folio);
	CHECK(plan.labelFor(QStringLiteral("a")) == QStringLiteral("M101"));
	CHECK(plan.labelFor(QStringLiteral("b")) == QStringLiteral("M102"));
	// Folio 2: the counter started again, so it is 01 and not 03.
	CHECK(plan.labelFor(QStringLiteral("c")) == QStringLiteral("M201"));
	CHECK_FALSE(plan.hasDuplicates());
}

TEST_CASE("CU-07.2 — le format par localisation recommence dans chaque localisation")
{
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("a"), QStringLiteral("Q"), 0, 100, 100)
	       << object(QStringLiteral("b"), QStringLiteral("Q"), 0, 100, 200)
	       << object(QStringLiteral("c"), QStringLiteral("Q"), 0, 100, 300);
	inputs[0].location = QStringLiteral("+A1");
	inputs[1].location = QStringLiteral("+A2");
	inputs[2].location = QStringLiteral("+A1");

	const RenumberPlan plan = Renumberer::plan(inputs,
						   formatNamed(QStringLiteral("Avec la localisation")));
	CHECK(plan.labelFor(QStringLiteral("a")) == QStringLiteral("+A1Q1"));
	CHECK(plan.labelFor(QStringLiteral("b")) == QStringLiteral("+A2Q1"));
	CHECK(plan.labelFor(QStringLiteral("c")) == QStringLiteral("+A1Q2"));
	CHECK_FALSE(plan.hasDuplicates());
}

TEST_CASE("NumberingFormat — le motif, le pas, le remplissage et ce qui est refusé")
{
	NumberingFormat format(QStringLiteral("Teste"), QStringLiteral("%{root}%{n}"));
	CHECK(format.isValid());
	CHECK(format.render(QStringLiteral("K"), 0) == QStringLiteral("K1"));
	CHECK(format.render(QStringLiteral("K"), 4) == QStringLiteral("K5"));

	format.start = 10;
	format.step = 10;
	CHECK(format.render(QStringLiteral("K"), 0) == QStringLiteral("K10"));
	CHECK(format.render(QStringLiteral("K"), 2) == QStringLiteral("K30"));

	format.start = 1;
	format.step = 1;
	format.digits = 3;
	CHECK(format.render(QStringLiteral("K"), 0) == QStringLiteral("K001"));

	// A token nobody filled leaves nothing behind: "M%{rung}1" on a folio with
	// no line numbering would be worse than "M1".
	NumberingFormat rung(QStringLiteral("Linha"), QStringLiteral("%{root}%{rung}%{n}"));
	CHECK(rung.render(QStringLiteral("M"), 0) == QStringLiteral("M1"));
	QHash<QString, QString> context;
	context.insert(QStringLiteral("rung"), QStringLiteral("12"));
	CHECK(rung.render(QStringLiteral("M"), 0, context) == QStringLiteral("M121"));

	// A pattern without the counter is refused: every object of the class
	// would get the same label, which is the one thing numbering must not do.
	QString error;
	NumberingFormat broken(QStringLiteral("Errado"), QStringLiteral("%{root}"));
	CHECK_FALSE(broken.isValid(&error));
	CHECK_FALSE(error.isEmpty());

	NumberingFormat nameless(QString(), QStringLiteral("%{root}%{n}"));
	error.clear();
	CHECK_FALSE(nameless.isValid(&error));
	CHECK_FALSE(error.isEmpty());

	NumberingFormat no_step(QStringLiteral("Zero"), QStringLiteral("%{root}%{n}"));
	no_step.step = 0;
	error.clear();
	CHECK_FALSE(no_step.isValid(&error));
	CHECK_FALSE(error.isEmpty());

	// And it survives being written down and read back.
	NumberingFormat original(QStringLiteral("ACME"), QStringLiteral("%{root}%{folio}%{n}"));
	original.start = 5;
	original.step = 5;
	original.digits = 2;
	original.scope = NumberingScope::Folio;
	const NumberingFormat reread = NumberingFormat::fromXml(original.toXml());
	CHECK(reread.name == original.name);
	CHECK(reread.pattern == original.pattern);
	CHECK(reread.start == 5);
	CHECK(reread.step == 5);
	CHECK(reread.digits == 2);
	CHECK(reread.scope == NumberingScope::Folio);
}

TEST_CASE("RenumberPlan — le doublon est vu avant d'être appliqué")
{
	// A format that ignores the folio, on objects of the same root spread over
	// two folios, cannot produce a duplicate. One that numbers by rung, with
	// two symbols on the same rung, can - and the preview has to say so before
	// anything is written.
	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("a"), QStringLiteral("M"), 0, 100, 100)
	       << object(QStringLiteral("b"), QStringLiteral("M"), 0, 300, 100);
	inputs[0].rung = QStringLiteral("12");
	inputs[1].rung = QStringLiteral("12");

	NumberingFormat rung(QStringLiteral("Só a linha"), QStringLiteral("%{root}%{rung}"));
	// This pattern has no %{n}, so it is refused as a format...
	CHECK_FALSE(rung.isValid());
	// ...and with the counter it does not collide.
	rung.pattern = QStringLiteral("%{root}%{rung}%{n}");
	rung.scope = NumberingScope::Rung;
	const RenumberPlan plan = Renumberer::plan(inputs, rung);
	CHECK(labelsOf(plan) == QStringList({ QStringLiteral("M121"),
					      QStringLiteral("M122") }));
	CHECK_FALSE(plan.hasDuplicates());
}

TEST_CASE("CU-07.1 — duas classes, duas regras, no mesmo comando")
{
	// The registered decision of T07 is that the rule lives on the class. So
	// contactors numbered sequentially and wires numbered per folio have to
	// come out right in the same run - which a single format per command
	// cannot do.
	const NumberingFormat sequential = formatNamed(QStringLiteral("Séquentiel"));
	const NumberingFormat with_folio = formatNamed(
		QStringLiteral("Avec le numéro de folio"));

	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("k1"), QStringLiteral("K"), 0, 100, 100)
	       << object(QStringLiteral("k2"), QStringLiteral("K"), 1, 100, 100)
	       << object(QStringLiteral("w1"), QStringLiteral("W"), 0, 100, 200)
	       << object(QStringLiteral("w2"), QStringLiteral("W"), 1, 100, 200);
	inputs[0].format = sequential;
	inputs[1].format = sequential;
	inputs[2].format = with_folio;
	inputs[3].format = with_folio;

	const RenumberPlan plan = Renumberer::plan(inputs);

	// The contactors ignore the folio and keep counting.
	CHECK(plan.labelFor(QStringLiteral("k1")) == QStringLiteral("K1"));
	CHECK(plan.labelFor(QStringLiteral("k2")) == QStringLiteral("K2"));
	// The wires restart on each folio and carry its number.
	CHECK(plan.labelFor(QStringLiteral("w1")) == QStringLiteral("W101"));
	CHECK(plan.labelFor(QStringLiteral("w2")) == QStringLiteral("W201"));
	CHECK_FALSE(plan.hasDuplicates());
}

TEST_CASE("CU-07.1 — dois formatos na mesma raiz não dividem contador")
{
	// Two classes that happen to share a tag root but are numbered by
	// different rules must not share a counter, or the second one starts at
	// the number the first one stopped at and the tags collide.
	NumberingFormat first(QStringLiteral("Primeiro"), QStringLiteral("%{root}%{n}"));
	NumberingFormat second(QStringLiteral("Segundo"), QStringLiteral("%{root}%{n}00"));

	QList<RenumberInput> inputs;
	inputs << object(QStringLiteral("a"), QStringLiteral("X"), 0, 100, 100)
	       << object(QStringLiteral("b"), QStringLiteral("X"), 0, 100, 200);
	inputs[0].format = first;
	inputs[1].format = second;

	const RenumberPlan plan = Renumberer::plan(inputs);
	CHECK(plan.labelFor(QStringLiteral("a")) == QStringLiteral("X1"));
	// Its own counter, so it is 1 and not 2.
	CHECK(plan.labelFor(QStringLiteral("b")) == QStringLiteral("X100"));
	CHECK_FALSE(plan.hasDuplicates());
}
