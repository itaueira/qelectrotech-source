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

/*
	The ways of a connector, numbered from one inside each connector (T34).

	Pure on purpose, for the same reason the rest of the numbering is pure:
	what goes wrong in a renumbering is the ordering and the counting, not the
	reading of the project. Everything here arrives resolved - the position,
	the connector name written on the pin, whether somebody numbered it by
	hand - so the counting can be read, argued with and measured without a
	folio open.

	Labelled T34 and not CU-34.2 or CU-34.3: both of those cases are about a
	pin inserted on a folio and a connector chosen with a keyboard modifier,
	and neither the insertion nor the modifier is written yet. What is proved
	here is the rule underneath them.
*/

namespace
{
	/**
		One pin, at @a y on the first folio. The reading order is top to
		bottom by default, so y alone decides who comes first.
	*/
	RenumberInput pin(const QString &uuid,
			  const QString &connector,
			  qreal y,
			  const QString &current = QString(),
			  const QString &root = QString())
	{
		RenumberInput input;
		input.uuid = uuid;
		input.connector = connector;
		input.root = root;
		input.folio_index = 0;
		input.position = QPointF(100, y);
		input.current = current;
		input.folio = QStringLiteral("1");
		return input;
	}

	/// The built-in format that numbers by connector, as the dialog offers it.
	NumberingFormat byConnector()
	{
		const QList<NumberingFormat> formats = NumberingFormat::builtinFormats();
		for (const NumberingFormat &format : formats)
		{
			if (format.scope == NumberingScope::Connector) {
				return format;
			}
		}
		return NumberingFormat();
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

	QStringList labelsOf(const RenumberPlan &plan)
	{
		QStringList labels;
		for (const RenumberEntry &entry : plan.entries) {
			labels.append(entry.to);
		}
		return labels;
	}

	RenumberEntry entryOf(const RenumberPlan &plan, const QString &uuid)
	{
		for (const RenumberEntry &entry : plan.entries)
		{
			if (entry.uuid == uuid) {
				return entry;
			}
		}
		return RenumberEntry();
	}
}

TEST_CASE("T34 — les broches de deux connecteurs comptent chacune à partir de un")
{
		//The two connectors are interleaved on the folio, so the answer
		//cannot come from the order of the list: a rule that just counted
		//down the list would give 1, 2, 3, 4, 5 and would still pass a
		//fixture where CN1 was drawn first and CN2 after it.
	QList<RenumberInput> inputs;
	inputs << pin(QStringLiteral("a"), QStringLiteral("CN1"), 100)
	       << pin(QStringLiteral("b"), QStringLiteral("CN2"), 140)
	       << pin(QStringLiteral("c"), QStringLiteral("CN1"), 180)
	       << pin(QStringLiteral("d"), QStringLiteral("CN2"), 220)
	       << pin(QStringLiteral("e"), QStringLiteral("CN1"), 260);

	const NumberingFormat format = byConnector();
	REQUIRE_FALSE(format.isNull());
	REQUIRE(format.scope == NumberingScope::Connector);

	const RenumberPlan plan = Renumberer::plan(inputs, format);
	CHECK(plan.labelFor(QStringLiteral("a")) == QStringLiteral("1"));
	CHECK(plan.labelFor(QStringLiteral("c")) == QStringLiteral("2"));
	CHECK(plan.labelFor(QStringLiteral("e")) == QStringLiteral("3"));
	CHECK(plan.labelFor(QStringLiteral("b")) == QStringLiteral("1"));
	CHECK(plan.labelFor(QStringLiteral("d")) == QStringLiteral("2"));

		//Two pins numbered 1 is what independent counters mean here, and it
		//is not a collision: they are the first way of two different
		//connectors. The duplicate check has to know that, because the tag of
		//a pin is its way number alone and the connector it sits in is the
		//rest of its identity - otherwise numbering two connectors at once
		//reports every way as a double and asks the user to confirm each
		//time, which is what teaches them to click through the one warning
		//that matters.
	CHECK_FALSE(plan.hasDuplicates());
	CHECK(plan.duplicates().isEmpty());
}

TEST_CASE("T34 — deux fois la même voie dans un connecteur reste un doublon")
{
		//The other half of the rule above, and the reason it is a group and
		//not a switch that turns the check off: inside one connector a
		//repeated way is exactly the mistake the warning exists for. Here a
		//way numbered by hand as 1 meets the 1 the counter is about to hand
		//out, because an object numbered by hand keeps its tag without
		//spending a number.
	QList<RenumberInput> inputs;
	RenumberInput fixed = pin(QStringLiteral("fixed"), QStringLiteral("CN1"),
				  100, QStringLiteral("1"));
	fixed.frozen = true;
	inputs << fixed
	       << pin(QStringLiteral("a"), QStringLiteral("CN1"), 140)
	       << pin(QStringLiteral("far"), QStringLiteral("CN2"), 180);

	const RenumberPlan plan = Renumberer::plan(inputs, byConnector());
	CHECK(plan.labelFor(QStringLiteral("a")) == QStringLiteral("1"));
	CHECK(plan.labelFor(QStringLiteral("far")) == QStringLiteral("1"));

		//Reported once, and only because of CN1: the way 1 of CN2 is not part
		//of it.
	CHECK(plan.hasDuplicates());
	CHECK(plan.duplicates() == QStringList({QStringLiteral("1")}));
}

TEST_CASE("T34 — trois broches effacées au milieu : renuméroter ferme le trou")
{
		//The case of the specification: a twelve way connector had ways 4, 5
		//and 6 deleted, so it carries 1, 2, 3, 7, 8, 9, 10, 11, 12 and has to
		//come back 1 to 9. A second connector is drawn between them and must
		//not move a single number.
	QList<RenumberInput> inputs;
	const char *remaining[] = {"1", "2", "3", "7", "8", "9", "10", "11", "12"};
	qreal y = 100;
	int index = 0;
	for (const char *label : remaining)
	{
		inputs << pin(QStringLiteral("cn1-%1").arg(index),
			      QStringLiteral("CN1"), y,
			      QString::fromLatin1(label));
		y += 40;
		++index;
	}
	inputs << pin(QStringLiteral("cn2-0"), QStringLiteral("CN2"), 120,
		      QStringLiteral("1"))
	       << pin(QStringLiteral("cn2-1"), QStringLiteral("CN2"), 320,
		      QStringLiteral("2"));

	const RenumberPlan plan = Renumberer::plan(inputs, byConnector());

	for (int number = 1 ; number <= 9 ; ++number)
	{
		INFO("voie " << number);
		CHECK(plan.labelFor(QStringLiteral("cn1-%1").arg(number - 1))
		      == QString::number(number));
	}

		//"sans affecter les autres connecteurs": CN2 keeps 1 and 2, and the
		//plan says so by marking them unchanged rather than by leaving them
		//out of the preview.
	CHECK(entryOf(plan, QStringLiteral("cn2-0")).to == QStringLiteral("1"));
	CHECK(entryOf(plan, QStringLiteral("cn2-1")).to == QStringLiteral("2"));
	CHECK_FALSE(entryOf(plan, QStringLiteral("cn2-0")).changed);
	CHECK_FALSE(entryOf(plan, QStringLiteral("cn2-1")).changed);

		//Six of the nine ways of CN1 move; the first three were already
		//right, and nothing of CN2 moves.
	CHECK(plan.changeCount() == 6);
	CHECK(plan.skippedCount() == 0);
}

TEST_CASE("T34 — un composant sans connecteur est passé, et le dit")
{
		//The trap this closes: the by-connector format is offered in the
		//renumbering dialog as the default for everything in the scope. A
		//counter that lumped everything with no connector into one nameless
		//group would rename every contactor of the project to a bare number,
		//and the preview would show it as an ordinary change.
	QList<RenumberInput> inputs;
	inputs << pin(QStringLiteral("k1"), QString(), 100, QStringLiteral("K1"),
		      QStringLiteral("K"))
	       << pin(QStringLiteral("k2"), QStringLiteral("   "), 140,
		      QStringLiteral("K2"), QStringLiteral("K"))
	       << pin(QStringLiteral("way"), QStringLiteral("CN1"), 180,
		      QStringLiteral("7"));

	const RenumberPlan plan = Renumberer::plan(inputs, byConnector());

	CHECK(entryOf(plan, QStringLiteral("k1")).to == QStringLiteral("K1"));
	CHECK(entryOf(plan, QStringLiteral("k1")).skipped);
	CHECK_FALSE(entryOf(plan, QStringLiteral("k1")).changed);

		//A field holding nothing but spaces is a field nobody filled in, and
		//not a connector whose name is three spaces.
	CHECK(entryOf(plan, QStringLiteral("k2")).to == QStringLiteral("K2"));
	CHECK(entryOf(plan, QStringLiteral("k2")).skipped);

		//Passed over is not the same as protected by hand, and the two are
		//counted apart because the preview says two different things about
		//them.
	CHECK(plan.skippedCount() == 2);
	CHECK(plan.frozenCount() == 0);

		//And the one pin that does belong to a connector is numbered from
		//one, undisturbed by the two that were passed over.
	CHECK(plan.labelFor(QStringLiteral("way")) == QStringLiteral("1"));
	CHECK(plan.entries.size() == 3);
}

TEST_CASE("T34 — un connecteur écrit de trois façons reste un seul connecteur")
{
		//Nothing normalises the field: the information panel writes what was
		//typed. Three pins of one connector can therefore carry three
		//different strings, and counting them apart would restart the
		//numbering halfway and hand two ways the same number - the silent
		//wrong answer this rule exists to avoid.
	QList<RenumberInput> inputs;
	inputs << pin(QStringLiteral("a"), QStringLiteral("CN1"), 100)
	       << pin(QStringLiteral("b"), QStringLiteral("cn1"), 140)
	       << pin(QStringLiteral("c"), QStringLiteral("CN1 "), 180);

	const RenumberPlan plan = Renumberer::plan(inputs, byConnector());
	CHECK(labelsOf(plan) == QStringList({QStringLiteral("1"),
					     QStringLiteral("2"),
					     QStringLiteral("3")}));

		//Counted as one, and reported as two graphies: the field was not
		//corrected, and it is the field the parts list filters on.
	CHECK(plan.inconsistentConnectors() == QStringList({QStringLiteral("CN1")}));
	CHECK(plan.connector_spellings.value(QStringLiteral("CN1"))
	      == QStringList({QStringLiteral("CN1"), QStringLiteral("cn1")}));

	SECTION("une seule graphie ne fait pas d'avertissement")
	{
		QList<RenumberInput> tidy;
		tidy << pin(QStringLiteral("a"), QStringLiteral("CN1"), 100)
		     << pin(QStringLiteral("b"), QStringLiteral("CN1"), 140);

		const RenumberPlan quiet = Renumberer::plan(tidy, byConnector());
		CHECK(quiet.inconsistentConnectors().isEmpty());
		CHECK(quiet.connector_spellings.value(QStringLiteral("CN1")).size() == 1);
	}

	SECTION("un espace au milieu du nom est un autre connecteur")
	{
			//The ends are trimmed and the case is folded; a space inside the
			//name is a name and not a slip, so CN 1 counts on its own.
		QList<RenumberInput> spaced = inputs;
		spaced << pin(QStringLiteral("d"), QStringLiteral("CN 1"), 220);

		const RenumberPlan plan_spaced = Renumberer::plan(spaced, byConnector());
		CHECK(plan_spaced.labelFor(QStringLiteral("d")) == QStringLiteral("1"));
		CHECK(plan_spaced.connector_spellings.size() == 2);
	}
}

TEST_CASE("T34 — la clé de connecteur dit quand deux noms sont le même connecteur")
{
		//The one place the answer is decided, exposed so that whatever writes
		//the field later - a connector management window - agrees with what
		//counts the ways today.
	CHECK(Renumberer::connectorKey(QStringLiteral("  CN1 "))
	      == Renumberer::connectorKey(QStringLiteral("cn1")));
	CHECK(Renumberer::connectorKey(QStringLiteral("XS1"))
	      != Renumberer::connectorKey(QStringLiteral("XS2")));
	CHECK(Renumberer::connectorKey(QStringLiteral("CN1"))
	      != Renumberer::connectorKey(QStringLiteral("CN 1")));
	CHECK(Renumberer::connectorKey(QStringLiteral("   ")).isEmpty());
	CHECK(Renumberer::connectorKey(QString()).isEmpty());
}

TEST_CASE("T34 — le symbole employé pour dessiner une voie n'ouvre pas une deuxième série")
{
		//A connector has one series of ways. Two pins of XS1 drawn with two
		//different symbols - a male one and a female one, which is what the
		//two sides of a harness look like - are still ways of XS1, and the
		//tag root of each is not what decides its number.
		//
		//The failure this guards against is a quiet one: with the root in the
		//bucket, the second symbol opens a series of its own and two ways
		//come out numbered 1, with nothing in the preview to say why.
	QList<RenumberInput> inputs;
	inputs << pin(QStringLiteral("male"), QStringLiteral("XS1"), 100,
		      QString(), QStringLiteral("XP"))
	       << pin(QStringLiteral("female"), QStringLiteral("XS1"), 140,
		      QString(), QStringLiteral("XS"));

	const RenumberPlan plan = Renumberer::plan(inputs, byConnector());
	CHECK(plan.labelFor(QStringLiteral("male")) == QStringLiteral("1"));
	CHECK(plan.labelFor(QStringLiteral("female")) == QStringLiteral("2"));
	CHECK_FALSE(plan.hasDuplicates());
}

TEST_CASE("T34 — %{connector} écrit le nom du premier, pas la graphie de chacun")
{
		//For the office that writes the connector into the pin's own tag,
		//XS1:1 the way a terminal of a strip is X1:1. The name that goes in
		//is the one of the first pin in reading order, so the tags of one
		//connector agree with each other even when the field does not.
	NumberingFormat format(QStringLiteral("essai"),
			       QStringLiteral("%{connector}:%{n}"));
	format.scope = NumberingScope::Connector;
	REQUIRE(format.isValid());

	QList<RenumberInput> inputs;
	inputs << pin(QStringLiteral("a"), QStringLiteral("XS1"), 100)
	       << pin(QStringLiteral("b"), QStringLiteral("xs1"), 140);

	const RenumberPlan plan = Renumberer::plan(inputs, format);
	CHECK(labelsOf(plan) == QStringList({QStringLiteral("XS1:1"),
					     QStringLiteral("XS1:2")}));
	CHECK_FALSE(plan.hasDuplicates());

	SECTION("et le jeton fait partie de ceux qu'un motif peut employer")
	{
			//A token missing from that list is not left in the label as
			//itself: render() strips the ones nobody filled, and it strips
			//them from that same list. A token that works but is not declared
			//would leave "%{connector}" written on the drawing.
		CHECK(NumberingFormat::tokens().contains(QStringLiteral("%{connector}")));
	}
}

TEST_CASE("T34 — une voie numérotée à la main garde son numéro et n'en consomme pas")
{
		//Same rule as everywhere else in the renumbering, said here because a
		//connector is where it is most often needed: the way that goes to a
		//particular terminal of a machine is fixed by the machine, and the
		//rest of the connector has to be numbered around it.
	QList<RenumberInput> inputs;
	RenumberInput fixed = pin(QStringLiteral("fixed"), QStringLiteral("CN1"),
				  100, QStringLiteral("7"));
	fixed.frozen = true;
	inputs << fixed
	       << pin(QStringLiteral("a"), QStringLiteral("CN1"), 140)
	       << pin(QStringLiteral("b"), QStringLiteral("CN1"), 180);

	const RenumberPlan plan = Renumberer::plan(inputs, byConnector());
	CHECK(plan.labelFor(QStringLiteral("fixed")) == QStringLiteral("7"));
	CHECK(entryOf(plan, QStringLiteral("fixed")).frozen);
	CHECK_FALSE(entryOf(plan, QStringLiteral("fixed")).skipped);
	CHECK(plan.labelFor(QStringLiteral("a")) == QStringLiteral("1"));
	CHECK(plan.labelFor(QStringLiteral("b")) == QStringLiteral("2"));
	CHECK(plan.frozenCount() == 1);
}

TEST_CASE("T34 — un format qui ne compte pas par connecteur ignore le champ")
{
		//The connector joins the counting only when the format asks for it.
		//Without that, giving every input the field would have moved the
		//numbering of every project that never heard of a connector.
	QList<RenumberInput> inputs;
	inputs << pin(QStringLiteral("k1"), QStringLiteral("CN1"), 100,
		      QString(), QStringLiteral("K"))
	       << pin(QStringLiteral("k2"), QStringLiteral("CN2"), 140,
		      QString(), QStringLiteral("K"));

	const NumberingFormat sequential = formatNamed(QStringLiteral("Séquentiel"));
	REQUIRE_FALSE(sequential.isNull());

	const RenumberPlan plan = Renumberer::plan(inputs, sequential);
	CHECK(labelsOf(plan) == QStringList({QStringLiteral("K1"),
					     QStringLiteral("K2")}));
	CHECK(plan.connector_spellings.isEmpty());
	CHECK(plan.skippedCount() == 0);
}

TEST_CASE("T34 — la portée par connecteur survit à l'écriture et à la relecture")
{
		//A format is stored on the class of the catalog, as a document. A
		//scope that goes out and comes back as something else would number
		//the ways of every connector into one series the next time the
		//project is opened.
	NumberingFormat original(QStringLiteral("Par connecteur"),
				 QStringLiteral("%{n}"));
	original.scope = NumberingScope::Connector;
	original.digits = 2;

	const NumberingFormat reread = NumberingFormat::fromXml(original.toXml());
	CHECK(reread.scope == NumberingScope::Connector);
	CHECK(reread.digits == 2);

	CHECK(NumberingFormat::scopeToString(NumberingScope::Connector)
	      == QStringLiteral("connector"));
	CHECK_FALSE(NumberingFormat::translatedScopeName(NumberingScope::Connector).isEmpty());

		//Reading stays forgiving: a scope this version does not know falls
		//back to the whole project rather than refusing the format.
	CHECK(NumberingFormat::scopeFromString(QStringLiteral("harness"))
	      == NumberingScope::Project);
}

TEST_CASE("T34 — le format proposé par la boîte de dialogue numérote la voie seule")
{
		//What the dialog offers, and the shape of it: the pin carries the way
		//number and nothing else, because the connector it belongs to sits in
		//a field of its own beside it - which is what lets the parts list ask
		//for every pin of CN1 and print a crimping guide.
	const NumberingFormat format = byConnector();
	REQUIRE_FALSE(format.isNull());
	CHECK(format.isValid());
	CHECK(format.pattern == QStringLiteral("%{n}"));

		//The tag root of the class does not reach the tag: a pin drawn from a
		//catalog part whose class says XS is way 1, and not XS1.
	QList<RenumberInput> inputs;
	inputs << pin(QStringLiteral("a"), QStringLiteral("XS1"), 100, QString(),
		      QStringLiteral("XS"));
	const RenumberPlan plan = Renumberer::plan(inputs, format);
	CHECK(plan.labelFor(QStringLiteral("a")) == QStringLiteral("1"));
}
