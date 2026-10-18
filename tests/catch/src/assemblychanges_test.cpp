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
#include "../../../sources/autoNum/assemblychanges.h"

#include "qt_catch_tostring.h"

#include <catch2/catch.hpp>

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

#include <algorithm>

/*
	What changed since the panel was wired, computed from two lists: the
	photograph the marking took, and the items handed in as drawn now.

	No project here, and that is the point of the file being in this suite:
	the comparison is the half that can be got wrong - a removal that goes
	missing, a relabelling read as an addition, an order that moves between
	two runs - and none of that needs a folio. Walking a real project and
	the window that shows the lines are in src/ui/assemblychangesdialog_test.cpp.

	Every expectation is written out here, line by line, and never computed
	from the rule under test: a test that asked the rule what to expect
	would pass whatever the rule did.

	Labelled T29 and not CU-29.4: that case ends with the shop carrying out
	the modification from the document alone, which is for a person to
	judge. What is proved here is the list it is made of.
*/

namespace
{
	const QString C1 = QStringLiteral("{c0000000-0000-4000-8000-000000000001}");
	const QString C2 = QStringLiteral("{c0000000-0000-4000-8000-000000000002}");
	const QString C3 = QStringLiteral("{c0000000-0000-4000-8000-000000000003}");
	const QString C4 = QStringLiteral("{c0000000-0000-4000-8000-000000000004}");
	const QString W1 = QStringLiteral("{a0000000-0000-4000-8000-000000000001}");
	const QString W2 = QStringLiteral("{a0000000-0000-4000-8000-000000000002}");
	const QString W3 = QStringLiteral("{a0000000-0000-4000-8000-000000000003}");
	const QString W4 = QStringLiteral("{a0000000-0000-4000-8000-000000000004}");

	/// Three components and three wires, as the marking left them.
	AssemblyState photograph(AssemblyStage stage = AssemblyStage::Assembled)
	{
		AssemblyState state;
		state.stage = stage;
		state.components.insert(C1, QStringLiteral("K1"));
		state.components.insert(C2, QStringLiteral("K2"));
		state.components.insert(C3, QStringLiteral("K3"));
		state.conductors.insert(W1, QStringLiteral("101"));
		state.conductors.insert(W2, QStringLiteral("102"));
		state.conductors.insert(W3, QStringLiteral("103"));
		return state;
	}

	AssemblyItem item(const QString &uuid, const QString &label, int folio = 1,
			  const QString &position = QString(),
			  const QString &description = QString(),
			  const QString &part_code = QString())
	{
		AssemblyItem made;
		made.uuid = uuid;
		made.label = label;
		made.folio = folio;
		made.position = position;
		made.description = description;
		made.part_code = part_code;
		return made;
	}

	/// The drawing exactly as it was photographed.
	QList<AssemblyItem> untouchedComponents()
	{
		return {item(C1, QStringLiteral("K1"), 1, QStringLiteral("A1")),
			item(C2, QStringLiteral("K2"), 1, QStringLiteral("A2")),
			item(C3, QStringLiteral("K3"), 2, QStringLiteral("B1"))};
	}

	QList<AssemblyItem> untouchedWires()
	{
		return {item(W1, QStringLiteral("101"), 1, QStringLiteral("A1")),
			item(W2, QStringLiteral("102"), 1, QStringLiteral("A2")),
			item(W3, QStringLiteral("103"), 2, QStringLiteral("B1"))};
	}

	QString actionWord(AssemblyAction action)
	{
		switch (action)
		{
			case AssemblyAction::Add: return QStringLiteral("add");
			case AssemblyAction::Remove: return QStringLiteral("remove");
			case AssemblyAction::Relabel: return QStringLiteral("relabel");
		}
		return QStringLiteral("?");
	}

	/**
		Every field of a line, in one string, so that a list of lines
		compares in one assertion and a failure prints the whole of both
		lists - count, order and content - instead of the first field that
		differs.
	*/
	QStringList described(const QList<AssemblyChange> &changes)
	{
		QStringList lines;
		for (const AssemblyChange &change : changes)
		{
			lines << QStringLiteral("%1 %2 [%3]->[%4] folio=%5 pos=%6 "
						"what=%7 part=%8 source=%9 uuid=%10")
				 .arg(actionWord(change.action),
				      change.kind == AssemblyItemKind::Component
				      ? QStringLiteral("component")
				      : QStringLiteral("wire"),
				      change.old_label, change.new_label,
				      QString::number(change.folio), change.position,
				      change.description, change.part_code,
				      QString::number(change.source))
				 .arg(change.uuid);
		}
		return lines;
	}

	/**
		The modification the order of service is about, on each half:
		C1 untouched, C2 relabelled K2 -> K5, C3 deleted, C4 drawn new; W1
		untouched, W2 renumbered 102 -> 110, W3 deleted, W4 drawn new.
	*/
	QList<AssemblyItem> modifiedComponents()
	{
		return {item(C1, QStringLiteral("K1"), 1, QStringLiteral("A1"),
			     QStringLiteral("Contactor")),
			item(C2, QStringLiteral("K5"), 1, QStringLiteral("A2"),
			     QStringLiteral("Contactor"), QStringLiteral("LC1D09")),
			item(C4, QStringLiteral("KA1"), 2, QStringLiteral("B3"),
			     QStringLiteral("Auxiliary relay"), QStringLiteral("CAD32"))};
	}

	QList<AssemblyItem> modifiedWires()
	{
		return {item(W1, QStringLiteral("101"), 1, QStringLiteral("A1"),
			     QStringLiteral("K1:13 - X1:1")),
			item(W2, QStringLiteral("110"), 1, QStringLiteral("A2"),
			     QStringLiteral("K5:14 - X1:2")),
			item(W4, QStringLiteral("104"), 2, QStringLiteral("B3"),
			     QStringLiteral("KA1:A1 - X1:4"))};
	}

	/// What the reader below found in a text.
	struct Parsed
	{
		QVector<QStringList> rows;
			/// Rows ended by CR LF, outside any quoted cell.
		int crlf_ends = 0;
			/// Rows ended by anything else.
		int other_ends = 0;
	};

	/**
		RFC 4180 with ';' between the cells. Written here rather than
		borrowed, so that a defect of the writer cannot be matched by the
		same defect in the reader.
	*/
	Parsed parse(const QString &text)
	{
		Parsed parsed;
		QStringList row;
		QString cell;
		bool quoted = false;
		bool pending = false;

		for (int i = 0 ; i < text.size() ; ++i)
		{
			const QChar c = text.at(i);
			if (quoted)
			{
				if (c == QLatin1Char('"'))
				{
					if (i + 1 < text.size()
					    && text.at(i + 1) == QLatin1Char('"')) {
						cell += c;
						++i;
					} else {
						quoted = false;
					}
				} else {
					cell += c;
				}
				continue;
			}

			if (c == QLatin1Char('"')) {
				quoted = true;
				pending = true;
				continue;
			}
			if (c == QLatin1Char(';')) {
				row << cell;
				cell.clear();
				pending = true;
				continue;
			}
			if (c == QLatin1Char('\r') || c == QLatin1Char('\n'))
			{
				if (c == QLatin1Char('\r') && i + 1 < text.size()
				    && text.at(i + 1) == QLatin1Char('\n')) {
					++parsed.crlf_ends;
					++i;
				} else {
					++parsed.other_ends;
				}
				row << cell;
				parsed.rows << row;
				row.clear();
				cell.clear();
				pending = false;
				continue;
			}
			cell += c;
			pending = true;
		}
		if (pending || !cell.isEmpty()) {
			row << cell;
			parsed.rows << row;
		}
		return parsed;
	}

	/// The first row of the file, written here and not asked of the rule.
	QStringList expectedHeader()
	{
		return {QStringLiteral("action"), QStringLiteral("kind"),
			QStringLiteral("old_label"), QStringLiteral("new_label"),
			QStringLiteral("folio"), QStringLiteral("position"),
			QStringLiteral("description"), QStringLiteral("part_code")};
	}
}

TEST_CASE("T29 — sem mudança desde a marcação, as seis listas ficam vazias",
	  "[t29][assembly][changes]")
{
	SECTION("o desenho como foi fotografado")
	{
		const AssemblyChanges changes = AssemblyChanges::compare(
			photograph(), untouchedComponents(), untouchedWires());

		REQUIRE(changes.isMarked());
		REQUIRE(changes.stage == AssemblyStage::Assembled);
		CHECK(changes.isEmpty());
		CHECK(changes.count() == 0);
		CHECK(changes.all().isEmpty());
		CHECK(changes.components.to_add.isEmpty());
		CHECK(changes.components.to_remove.isEmpty());
		CHECK(changes.components.to_relabel.isEmpty());
		CHECK(changes.wires.to_add.isEmpty());
		CHECK(changes.wires.to_remove.isEmpty());
		CHECK(changes.wires.to_relabel.isEmpty());
	}

	SECTION("entregue em outra ordem, continua sem mudança")
	{
		// The walk of a project follows the order of the scene, which is
		// not the same from one opening to the next.
		QList<AssemblyItem> components = untouchedComponents();
		std::reverse(components.begin(), components.end());
		QList<AssemblyItem> wires = untouchedWires();
		wires.move(0, 2);

		const AssemblyChanges changes = AssemblyChanges::compare(
			photograph(), components, wires);
		CHECK(changes.isMarked());
		CHECK(changes.isEmpty());
	}

	SECTION("em campo é tão marcado quanto montado")
	{
		const AssemblyChanges changes = AssemblyChanges::compare(
			photograph(AssemblyStage::InField), untouchedComponents(),
			untouchedWires());
		CHECK(changes.isMarked());
		CHECK(changes.stage == AssemblyStage::InField);
		CHECK(changes.isEmpty());
	}

	SECTION("o arquivo de uma modificação vazia é a linha de chaves, só")
	{
		// A file that says "nothing to do", not a missing file.
		const AssemblyChanges changes = AssemblyChanges::compare(
			photograph(), untouchedComponents(), untouchedWires());
		CHECK(changes.csvText()
		      == QStringLiteral("action;kind;old_label;new_label;folio;"
					"position;description;part_code\r\n"));
	}
}

TEST_CASE("T29 — sem marcação não há 'desde': nada é comparado, e isso não "
	  "se confunde com 'nada mudou'",
	  "[t29][assembly][changes]")
{
	// The same drawing in all three sections: the changes of the case
	// above, which a marked project does report.
	const QList<AssemblyItem> components = modifiedComponents();
	const QList<AssemblyItem> wires = modifiedWires();

	SECTION("controle negativo — marcado, as mudanças aparecem")
	{
		// Without this the two sections below would pass on a rule that
		// had stopped comparing anything at all.
		const AssemblyChanges changes = AssemblyChanges::compare(
			photograph(), components, wires);
		REQUIRE(changes.isMarked());
		REQUIRE(changes.count() == 6);
	}

	SECTION("uma fotografia que sobrou de uma marcação desfeita não compara")
	{
		// The stage decides, as it decides the freezing: a photograph left
		// over in a project put back on the drawing board freezes nothing,
		// and has nothing to say about what changed since.
		const AssemblyChanges changes = AssemblyChanges::compare(
			photograph(AssemblyStage::InProject), components, wires);
		CHECK_FALSE(changes.isMarked());
		CHECK(changes.isEmpty());
		CHECK(changes.count() == 0);
	}

	SECTION("sem fotografia nenhuma, nada vira 'a acrescentar'")
	{
		// The wrong answer, named so that nobody writes it: comparing with
		// an empty photograph would list every component of the drawing as
		// one to mount.
		const AssemblyChanges changes = AssemblyChanges::compare(
			AssemblyState(), components, wires);
		CHECK_FALSE(changes.isMarked());
		CHECK(changes.components.to_add.isEmpty());
		CHECK(changes.wires.to_add.isEmpty());
		CHECK(changes.isEmpty());
	}
}

TEST_CASE("T29 — acrescentado, removido e reetiquetado: componente e fio, "
	  "cada um na sua lista, com o conteúdo de cada linha",
	  "[t29][assembly][changes]")
{
	const AssemblyChanges changes = AssemblyChanges::compare(
		photograph(), modifiedComponents(), modifiedWires());

	REQUIRE(changes.isMarked());
	CHECK(changes.count() == 6);
	CHECK(changes.components.count() == 3);
	CHECK(changes.wires.count() == 3);

	SECTION("componente acrescentado: a etiqueta nova, onde está e o que é")
	{
		CHECK(described(changes.components.to_add) == QStringList {
			QStringLiteral("add component []->[KA1] folio=2 pos=B3 "
				       "what=Auxiliary relay part=CAD32 source=2 uuid=")
			+ C4});
	}

	SECTION("componente removido: a etiqueta que ele carregava, e nada de folio")
	{
		// Drawn nowhere: no folio, no position, nothing to go to.
		CHECK(described(changes.components.to_remove) == QStringList {
			QStringLiteral("remove component [K3]->[] folio=0 pos= "
				       "what= part= source=-1 uuid=") + C3});
	}

	SECTION("componente reetiquetado: a etiqueta da fotografia e a de agora")
	{
		CHECK(described(changes.components.to_relabel) == QStringList {
			QStringLiteral("relabel component [K2]->[K5] folio=1 pos=A2 "
				       "what=Contactor part=LC1D09 source=1 uuid=")
			+ C2});
	}

	SECTION("fio acrescentado, removido e renumerado")
	{
		CHECK(described(changes.wires.to_add) == QStringList {
			QStringLiteral("add wire []->[104] folio=2 pos=B3 "
				       "what=KA1:A1 - X1:4 part= source=2 uuid=") + W4});
		CHECK(described(changes.wires.to_remove) == QStringList {
			QStringLiteral("remove wire [103]->[] folio=0 pos= "
				       "what= part= source=-1 uuid=") + W3});
		CHECK(described(changes.wires.to_relabel) == QStringList {
			QStringLiteral("relabel wire [102]->[110] folio=1 pos=A2 "
				       "what=K5:14 - X1:2 part= source=1 uuid=") + W2});
	}

	SECTION("o que não mudou não aparece em lista nenhuma")
	{
		// C1 and W1: present in both, same label.
		for (const AssemblyChange &change : changes.all())
		{
			INFO(change.uuid.toStdString());
			CHECK(change.uuid != C1);
			CHECK(change.uuid != W1);
		}
	}

	SECTION("all() é componentes e depois fios, cada um em acrescentar, "
		"remover, reetiquetar")
	{
		QStringList order;
		for (const AssemblyChange &change : changes.all()) {
			order << change.uuid;
		}
		CHECK(order == QStringList {C4, C3, C2, W4, W3, W2});
	}
}

TEST_CASE("T29 — a ordem das linhas é a do folio e da etiqueta do painel, e "
	  "não a de quem entregou os itens",
	  "[t29][assembly][changes]")
{
	SECTION("acrescentar: por folio, depois por etiqueta; folio desconhecido "
		"por último")
	{
		const QList<AssemblyItem> now {
			item(QStringLiteral("{n3}"), QStringLiteral("KB2"), 3),
			item(QStringLiteral("{n0}"), QStringLiteral("KA0"), 0),
			item(QStringLiteral("{n9}"), QStringLiteral("KA9"), 1),
			item(QStringLiteral("{n1}"), QStringLiteral("KA1"), 1)};

		const AssemblyChangeLists lists = AssemblyChanges::compareItems(
			QMap<QString, QString>(), now, AssemblyItemKind::Component);

		QStringList labels;
		QList<int> sources;
		for (const AssemblyChange &change : lists.to_add)
		{
			labels << change.new_label;
			sources << change.source;
		}
		// 0 is "not said", and not the folio before the first.
		CHECK(labels == QStringList {QStringLiteral("KA1"),
					     QStringLiteral("KA9"),
					     QStringLiteral("KB2"),
					     QStringLiteral("KA0")});
		// And each line still points at the item it came from, wherever
		// the sort put it.
		CHECK(sources == QList<int> {3, 2, 0, 1});
	}

	SECTION("remover: pela etiqueta que carregava, sem diferença de caixa")
	{
		QMap<QString, QString> photographed;
		photographed.insert(QStringLiteral("{r1}"), QStringLiteral("Q2"));
		photographed.insert(QStringLiteral("{r2}"), QStringLiteral("k7"));
		photographed.insert(QStringLiteral("{r3}"), QStringLiteral("A1"));
		photographed.insert(QStringLiteral("{r4}"), QStringLiteral("K7"));

		const AssemblyChangeLists lists = AssemblyChanges::compareItems(
			photographed, QList<AssemblyItem>(),
			AssemblyItemKind::Component);

		QStringList labels;
		for (const AssemblyChange &change : lists.to_remove) {
			labels << change.old_label;
		}
		// k7 and K7 are the same case aside; case then decides, upper
		// first, so the order is the same on every run.
		CHECK(labels == QStringList {QStringLiteral("A1"),
					     QStringLiteral("K7"),
					     QStringLiteral("k7"),
					     QStringLiteral("Q2")});
	}

	SECTION("reetiquetar: pela etiqueta que o painel mostra hoje, e não pela "
		"nova")
	{
		// The shop finds the part by the tag printed on it. Sorting by the
		// new label would put K9 -> A1 first, and the reader would look
		// for an A1 on the rail.
		QMap<QString, QString> photographed;
		photographed.insert(QStringLiteral("{p1}"), QStringLiteral("K9"));
		photographed.insert(QStringLiteral("{p2}"), QStringLiteral("K1"));

		const QList<AssemblyItem> now {
			item(QStringLiteral("{p1}"), QStringLiteral("A1"), 1),
			item(QStringLiteral("{p2}"), QStringLiteral("Z9"), 1)};

		const AssemblyChangeLists lists = AssemblyChanges::compareItems(
			photographed, now, AssemblyItemKind::Component);

		QStringList lines;
		for (const AssemblyChange &change : lists.to_relabel) {
			lines << change.old_label + QStringLiteral("->")
				 + change.new_label;
		}
		CHECK(lines == QStringList {QStringLiteral("K1->Z9"),
					    QStringLiteral("K9->A1")});
	}
}

TEST_CASE("T29 — nada se perde: identidade repetida, item sem identidade e "
	  "etiqueta que só mudou de caixa",
	  "[t29][assembly][changes]")
{
	SECTION("a mesma identidade duas vezes: a primeira é a da fotografia, a "
		"segunda é peça a acrescentar")
	{
		// A copy carries the uuid of its original for the moment a paste
		// takes to give it one of its own. It is still a second piece on
		// the panel.
		QMap<QString, QString> photographed;
		photographed.insert(C1, QStringLiteral("K1"));

		const QList<AssemblyItem> now {
			item(C1, QStringLiteral("K1"), 1),
			item(C1, QStringLiteral("K1"), 2)};

		const AssemblyChangeLists lists = AssemblyChanges::compareItems(
			photographed, now, AssemblyItemKind::Component);

		REQUIRE(lists.to_add.size() == 1);
		CHECK(lists.to_add.first().source == 1);
		CHECK(lists.to_add.first().folio == 2);
		CHECK(lists.to_remove.isEmpty());
		CHECK(lists.to_relabel.isEmpty());
	}

	SECTION("um item sem identidade é acrescentado, e não esquecido")
	{
		QMap<QString, QString> photographed;
		photographed.insert(C1, QStringLiteral("K1"));

		const QList<AssemblyItem> now {
			item(C1, QStringLiteral("K1"), 1),
			item(QString(), QStringLiteral("KX"), 1)};

		const AssemblyChangeLists lists = AssemblyChanges::compareItems(
			photographed, now, AssemblyItemKind::Component);

		REQUIRE(lists.to_add.size() == 1);
		CHECK(lists.to_add.first().new_label == QStringLiteral("KX"));
		CHECK(lists.to_add.first().source == 1);
		CHECK(lists.count() == 1);
	}

	SECTION("uma chave vazia na fotografia não vira peça a remover")
	{
		// AssemblyState never stores one; a hand-made map might. A line
		// for it would send the shop looking for a part that never had an
		// identity to lose.
		QMap<QString, QString> photographed;
		photographed.insert(QString(), QStringLiteral("K0"));
		photographed.insert(C1, QStringLiteral("K1"));

		const AssemblyChangeLists lists = AssemblyChanges::compareItems(
			photographed, QList<AssemblyItem>(),
			AssemblyItemKind::Component);

		REQUIRE(lists.to_remove.size() == 1);
		CHECK(lists.to_remove.first().uuid == C1);
	}

	SECTION("'k1' não é 'K1': é outra etiqueta a imprimir")
	{
		QMap<QString, QString> photographed;
		photographed.insert(C1, QStringLiteral("K1"));

		const AssemblyChangeLists lists = AssemblyChanges::compareItems(
			photographed, {item(C1, QStringLiteral("k1"), 1)},
			AssemblyItemKind::Component);

		REQUIRE(lists.to_relabel.size() == 1);
		CHECK(lists.to_relabel.first().old_label == QStringLiteral("K1"));
		CHECK(lists.to_relabel.first().new_label == QStringLiteral("k1"));
	}
}

TEST_CASE("T29 — o arquivo da oficina abre com uma linha fixa de chaves, em "
	  "inglês, e as palavras de ação e de tipo são chaves",
	  "[t29][assembly][changes][csv]")
{
	CHECK(AssemblyChanges::csvHeader() == expectedHeader());
	CHECK(AssemblyChanges::separator() == QStringLiteral(";"));

	CHECK(AssemblyChanges::actionKey(AssemblyAction::Add)
	      == QStringLiteral("add"));
	CHECK(AssemblyChanges::actionKey(AssemblyAction::Remove)
	      == QStringLiteral("remove"));
	CHECK(AssemblyChanges::actionKey(AssemblyAction::Relabel)
	      == QStringLiteral("relabel"));
	// The words the label export already writes for the same things.
	CHECK(AssemblyChanges::kindKey(AssemblyItemKind::Component)
	      == QStringLiteral("component"));
	CHECK(AssemblyChanges::kindKey(AssemblyItemKind::Wire)
	      == QStringLiteral("wire"));
}

TEST_CASE("T29 — o arquivo da oficina tem uma linha por mudança, na ordem da "
	  "janela, e cada célula na sua coluna",
	  "[t29][assembly][changes][csv]")
{
	const AssemblyChanges changes = AssemblyChanges::compare(
		photograph(), modifiedComponents(), modifiedWires());
	REQUIRE(changes.count() == 6);

	const Parsed parsed = parse(changes.csvText());

	// One row of keys and six of changes, every one of them ended by CRLF
	// and none by anything else.
	REQUIRE(parsed.rows.size() == 7);
	CHECK(parsed.crlf_ends == 7);
	CHECK(parsed.other_ends == 0);
	CHECK(parsed.rows.first() == expectedHeader());

	for (int row = 0 ; row < parsed.rows.size() ; ++row)
	{
		INFO("row " << row);
		CHECK(parsed.rows.at(row).size() == 8);
	}

	CHECK(parsed.rows.at(1) == QStringList {
		QStringLiteral("add"), QStringLiteral("component"), QString(),
		QStringLiteral("KA1"), QStringLiteral("2"), QStringLiteral("B3"),
		QStringLiteral("Auxiliary relay"), QStringLiteral("CAD32")});
	// A removal has no folio: the cell is empty, never 0.
	CHECK(parsed.rows.at(2) == QStringList {
		QStringLiteral("remove"), QStringLiteral("component"),
		QStringLiteral("K3"), QString(), QString(), QString(), QString(),
		QString()});
	CHECK(parsed.rows.at(3) == QStringList {
		QStringLiteral("relabel"), QStringLiteral("component"),
		QStringLiteral("K2"), QStringLiteral("K5"), QStringLiteral("1"),
		QStringLiteral("A2"), QStringLiteral("Contactor"),
		QStringLiteral("LC1D09")});
	CHECK(parsed.rows.at(4) == QStringList {
		QStringLiteral("add"), QStringLiteral("wire"), QString(),
		QStringLiteral("104"), QStringLiteral("2"), QStringLiteral("B3"),
		QStringLiteral("KA1:A1 - X1:4"), QString()});
	CHECK(parsed.rows.at(5) == QStringList {
		QStringLiteral("remove"), QStringLiteral("wire"),
		QStringLiteral("103"), QString(), QString(), QString(), QString(),
		QString()});
	CHECK(parsed.rows.at(6) == QStringList {
		QStringLiteral("relabel"), QStringLiteral("wire"),
		QStringLiteral("102"), QStringLiteral("110"), QStringLiteral("1"),
		QStringLiteral("A2"), QStringLiteral("K5:14 - X1:2"), QString()});
}

TEST_CASE("T29 — ponto e vírgula, aspas e quebra de linha numa célula não "
	  "mudam a coluna, e o acento chega como UTF-8",
	  "[t29][assembly][changes][csv]")
{
	const QString awkward = QStringLiteral("Contacteur 3P; 25A \"principal\"\n"
					       "bobine 24 V");
	const QString accented = QStringLiteral("Équipement à câbler");

	QMap<QString, QString> nothing;
	AssemblyChanges changes;
	changes.stage = AssemblyStage::Assembled;
	changes.components = AssemblyChanges::compareItems(
		nothing,
		// K;1 before Q2 in the order of the rule, so that the rows below
		// are where this case reads them.
		{item(C1, QStringLiteral("K;1"), 1, QStringLiteral("A1"), awkward),
		 item(C2, QStringLiteral("Q2"), 1, QStringLiteral("A2"), accented)},
		AssemblyItemKind::Component);
	REQUIRE(changes.count() == 2);

	SECTION("a célula volta inteira, e a linha continua com oito colunas")
	{
		const Parsed parsed = parse(changes.csvText());
		REQUIRE(parsed.rows.size() == 3);
		REQUIRE(parsed.rows.at(1).size() == 8);
		CHECK(parsed.rows.at(1).at(3) == QStringLiteral("K;1"));
		CHECK(parsed.rows.at(1).at(6) == awkward);
		CHECK(parsed.rows.at(1).at(7) == QString());
	}

	SECTION("os bytes: marca de ordem de bytes, e depois UTF-8")
	{
		const QByteArray bytes = changes.csvBytes();
		REQUIRE(bytes.size() > 3);
		CHECK(bytes.left(3) == QByteArray("\xEF\xBB\xBF", 3));

		const QString text = QString::fromUtf8(bytes.mid(3));
		CHECK(text == changes.csvText());
		CHECK(parse(text).rows.at(2).at(6) == accented);
	}
}

TEST_CASE("T29 — o arquivo da oficina é escrito inteiro, ou não é escrito",
	  "[t29][assembly][changes][csv]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());

	const AssemblyChanges changes = AssemblyChanges::compare(
		photograph(), modifiedComponents(), modifiedWires());

	SECTION("escrito: o arquivo guarda os bytes, e nada é dito")
	{
		const QString path = dir.filePath(QStringLiteral("changes.csv"));
		CHECK(changes.writeCsv(path).isEmpty());

		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		CHECK(file.readAll() == changes.csvBytes());
	}

	SECTION("escrito de novo: a lista nova substitui a velha")
	{
		const QString path = dir.filePath(QStringLiteral("changes.csv"));
		REQUIRE(changes.writeCsv(path).isEmpty());

		const AssemblyChanges nothing = AssemblyChanges::compare(
			photograph(), untouchedComponents(), untouchedWires());
		REQUIRE(nothing.writeCsv(path).isEmpty());

		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		CHECK(file.readAll() == nothing.csvBytes());
	}

	SECTION("uma pasta que não existe: o motivo, e arquivo nenhum")
	{
		const QString path =
			dir.filePath(QStringLiteral("missing/changes.csv"));
		CHECK_FALSE(changes.writeCsv(path).isEmpty());
		CHECK_FALSE(QFile::exists(path));
	}
}
