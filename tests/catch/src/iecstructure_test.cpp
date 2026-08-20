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
#include "../../../sources/autoNum/iecstructure.h"
#include "qt_catch_tostring.h"

TEST_CASE("CU-10.1 — désactivée, la structure ne touche à rien")
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

TEST_CASE("CU-10.2 — l'héritage en cascade : personne ne tape =CT1 deux fois")
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

TEST_CASE("CU-10.3 — la surcharge explicite ne touche que celui qui la porte")
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

TEST_CASE("CU-10.4 — court ou complet change l'affichage, pas la donnée")
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

TEST_CASE("CU-10.5 — allumer la structure en cours de route ne perd aucun repère")
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

TEST_CASE("IecStructure — les champs où les trois parties vivent")
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
