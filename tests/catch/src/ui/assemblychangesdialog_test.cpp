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
#include "uibench.h"

#include "../qt_catch_tostring.h"

#include "../../../../sources/autoNum/assemblychanges.h"
#include "../../../../sources/autoNum/assemblystate.h"
#include "../../../../sources/autoNum/projectrenumberer.h"
#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/diagramposition.h"
#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/properties/elementdata.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/assemblychangesdialog.h"
#include "../../../../sources/ui/assemblystatedialog.h"
#include "../../../../sources/undocommand/addgraphicsobjectcommand.h"
#include "../../../../sources/undocommand/assemblystatecommand.h"
#include "../../../../sources/undocommand/deleteqgraphicsitemcommand.h"

#include <catch2/catch.hpp>

#include <QDialog>
#include <QDomDocument>
#include <QFile>
#include <QMap>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVector>

#include <algorithm>
#include <utility>

/*
	The order of service after the panel was wired, on a real project: an
	example of examples/ marked as assembled, then changed the four ways a
	modification changes a panel - a component deleted, components and a
	wire drawn, a tag retyped, a wire renumbered - and the lists read back.

	What the comparison itself owes - the order of the lines, the copies,
	the item with no identity, the quoting of the file - is proved one floor
	down, on lists built by hand, in src/assemblychanges_test.cpp, and is
	not repeated here. What that suite cannot see is everything this one is
	about: that the walk of a project reads the drawing the way the
	photograph read it. Read one field another way and every component of
	the panel comes out "relabelled"; walk another list and every terminal
	comes out "added"; walk a folio that is not built yet and everything on
	it comes out "removed". None of that is visible on a list typed by hand.

	The example is copied into a directory of its own before it is touched,
	so that the case that saves and reopens does not write into the source
	tree. Its wires carry no uuid in the file - none of the examples' wires
	do - which is the trap the plan of the task warns about, and the case
	that saves and reopens measures it.

	Labelled T29 and not CU-29.4: the case ends with the shop carrying out
	the modification from the document alone, and that is for a person to
	judge. What is proved here is the document.
*/

namespace
{
	const QString EXAMPLE = QStringLiteral("tremie_vibrante.qet");

	const QString RELABELLED = QStringLiteral("K-RELABELLED");
	const QString RENUMBERED = QStringLiteral("W-RENUMBERED");
	const QString NEW_FIRST = QStringLiteral("K-NEW1");
	const QString NEW_SECOND = QStringLiteral("K-NEW2");
	const QString NEW_WIRE = QStringLiteral("W-NEW");

	void markAsAssembled(QETProject *project)
	{
		project->undoStack()->push(new AssemblyStateCommand(
			project,
			AssemblyStateCommand::photograph(project,
							 AssemblyStage::Assembled)));
	}

	QString labelOf(Element *element)
	{
		return element ? element->elementInformations()
				 .value(QStringLiteral("label")).toString()
			       : QString();
	}

	/**
		The components the renumbering tags, in the order of their uuid.
		Two openings of the same example do not walk the scene in the same
		order; the uuid of a component is written in the file and does not
		move, so the component a case picks is the same on every run.
	*/
	QList<Element *> componentsByUuid(QETProject *project)
	{
		QList<Element *> components = ProjectRenumberer::components(project);
		std::sort(components.begin(), components.end(),
			  [](Element *a, Element *b) {
				  return a->uuid().toString() < b->uuid().toString();
			  });
		return components;
	}

	/**
		The wires of the project in an order that does not move between
		runs. Not by uuid: the wires of the examples carry none in the file,
		and the one they are given while the project is opened is new every
		time. By folio and by where the two ends are instead.
	*/
	QList<Conductor *> wiresInOrder(QETProject *project)
	{
		QList<std::pair<QString, Conductor *>> keyed;
		const QList<Diagram *> sheets = project->diagrams();
		for (Diagram *sheet : sheets)
		{
			const QList<Conductor *> wires = sheet->conductors();
			for (Conductor *wire : wires)
			{
				const QPointF one = wire->terminal1->dockConductor();
				const QPointF two = wire->terminal2->dockConductor();
				keyed << std::make_pair(
					QStringLiteral("%1|%2|%3|%4|%5")
					.arg(sheet->folioIndex(), 4, 10, QLatin1Char('0'))
					.arg(one.x(), 10, 'f', 1).arg(one.y(), 10, 'f', 1)
					.arg(two.x(), 10, 'f', 1).arg(two.y(), 10, 'f', 1),
					wire);
			}
		}
		std::stable_sort(keyed.begin(), keyed.end(),
				 [](const std::pair<QString, Conductor *> &a,
				    const std::pair<QString, Conductor *> &b) {
					 return a.first < b.first;
				 });

		QList<Conductor *> ordered;
		for (const auto &pair : keyed) {
			ordered << pair.second;
		}
		return ordered;
	}

	/**
		A plain component with a tag, whose deletion takes its wires with it
		and adds none: every terminal holds one wire at most, and it holds
		at least one in all. A terminal holding two would make the deletion
		draw a new wire between the two others to keep the potential
		(DeleteQGraphicsItemCommand::setPotentialsOfRemovedElements), and
		this case would be measuring that instead of the report.
	*/
	bool deletesCleanly(Element *element)
	{
		if (element->elementData().m_type != ElementData::Simple
		    || labelOf(element).isEmpty()) {
			return false;
		}
		int wires = 0;
		const QList<Terminal *> terminals = element->terminals();
		for (Terminal *terminal : terminals)
		{
			const int here = int(terminal->conductors().size());
			if (here > 1) {
				return false;
			}
			wires += here;
		}
		return wires > 0;
	}

	/// A component drawn on @a sheet the way a paste draws it, from XML.
	Element *drawComponent(Diagram *sheet, const QString &type,
			       const QPointF &where, const QString &label)
	{
		QDomDocument document;
		QDomElement root = document.createElement(QStringLiteral("diagram"));
		document.appendChild(root);
		QDomElement elements = document.createElement(QStringLiteral("elements"));
		root.appendChild(elements);

		QDomElement element = document.createElement(QStringLiteral("element"));
		// 'f' and not the default 'g': six significant digits would round
		// a coordinate past 99999.9, and the case compares positions.
		element.setAttribute(QStringLiteral("x"),
				     QString::number(where.x(), 'f', 3));
		element.setAttribute(QStringLiteral("y"),
				     QString::number(where.y(), 'f', 3));
		element.setAttribute(QStringLiteral("z"), QStringLiteral("10"));
		element.setAttribute(QStringLiteral("prefix"), QString());
		element.setAttribute(QStringLiteral("freezeLabel"), QStringLiteral("false"));
		element.setAttribute(QStringLiteral("orientation"), QStringLiteral("0"));
		element.setAttribute(QStringLiteral("type"), type);
		// No uuid: the component is given a fresh one, which is what makes
		// it a component the photograph never saw.
		element.appendChild(document.createElement(QStringLiteral("terminals")));
		element.appendChild(document.createElement(QStringLiteral("inputs")));
		QDomElement informations =
			document.createElement(QStringLiteral("elementInformations"));
		QDomElement information =
			document.createElement(QStringLiteral("elementInformation"));
		information.setAttribute(QStringLiteral("show"), QStringLiteral("1"));
		information.setAttribute(QStringLiteral("name"), QStringLiteral("label"));
		information.appendChild(document.createTextNode(label));
		informations.appendChild(information);
		element.appendChild(informations);
		element.appendChild(document.createElement(QStringLiteral("dynamic_texts")));
		element.appendChild(document.createElement(QStringLiteral("texts_groups")));
		elements.appendChild(element);

		root.appendChild(document.createElement(QStringLiteral("inputs")));
		root.appendChild(document.createElement(QStringLiteral("conductors")));

		DiagramContent added;
		// false: the border and the title block of the sheet are not to be
		// read again from a fragment that carries none.
		if (!sheet->fromXml(root, QPointF(), false, &added)
		    || added.m_elements.size() != 1) {
			return nullptr;
		}
		return added.m_elements.first();
	}

	/// What the modification did, written down before and as it was done.
	struct Modification
	{
		bool done = false;
		QString why;

		Diagram *sheet = nullptr;
		int folio = 0;

		QString removed_uuid;
		QString removed_label;
		QString removed_position;
			/// uuid -> number of each wire the deletion took with it
		QMap<QString, QString> removed_wires;

		QString relabelled_uuid;
		QString relabelled_from;

		QString renumbered_uuid;
		QString renumbered_from;

		QPointer<Element> first;
		QPointer<Element> second;
		QPointer<Conductor> new_wire;
		QPointer<Conductor> renumbered;
	};

	/**
		The modification: one component deleted, two drawn where it stood
		and wired together, one retagged, one wire renumbered. Every choice
		is by uuid or by position, so it is the same on every run.
	*/
	Modification modify(QETProject *project)
	{
		Modification done;

		Element *doomed = nullptr;
		Element *retagged = nullptr;
		const QList<Element *> components = componentsByUuid(project);
		for (Element *element : components)
		{
			if (!doomed && deletesCleanly(element)) {
				doomed = element;
				continue;
			}
			if (!retagged && element->elementData().m_type == ElementData::Simple
			    && !labelOf(element).isEmpty()) {
				retagged = element;
			}
		}
		if (!doomed || !retagged || doomed == retagged) {
			done.why = QStringLiteral("the example has no plain tagged component "
						  "to delete cleanly and another to retag");
			return done;
		}

		Diagram *sheet = doomed->diagram();
		if (!sheet) {
			done.why = QStringLiteral("the component to delete is on no folio");
			return done;
		}
		done.sheet = sheet;
		done.folio = sheet->folioIndex() + 1;

			//The wire to renumber: not one the deletion takes away, and
			//preferably alone in its potential, so that nothing but its own
			//text can be read as having changed.
		Conductor *wire = nullptr;
		Conductor *fallback = nullptr;
		const QList<Conductor *> wires = wiresInOrder(project);
		for (Conductor *candidate : wires)
		{
			if (candidate->terminal1->parentElement() == doomed
			    || candidate->terminal2->parentElement() == doomed) {
				continue;
			}
			if (!fallback) {
				fallback = candidate;
			}
			if (candidate->relatedPotentialConductors().isEmpty()) {
				wire = candidate;
				break;
			}
		}
		if (!wire) {
			wire = fallback;
		}
		if (!wire) {
			done.why = QStringLiteral("the example has no wire to renumber");
			return done;
		}

			//Deleted the way the folio deletes a selection: what the
			//selection holds, wires attached included, as one step.
		done.removed_uuid = doomed->uuid().toString();
		done.removed_label = labelOf(doomed);
		done.removed_position = sheet->convertPosition(doomed->scenePos())
					.toString();
		const QPointF where = doomed->scenePos();
		const QString type = doomed->location().collectionPath();
		const QList<Conductor *> attached = doomed->conductors();
		for (Conductor *gone : attached) {
			done.removed_wires.insert(gone->uuid().toString(),
						  gone->properties().text);
		}
		sheet->clearSelection();
		doomed->setSelected(true);
		DiagramContent selection(sheet);
		sheet->clearSelection();
		sheet->undoStack().push(new DeleteQGraphicsItemCommand(sheet, selection));

			//Retagged by hand, the way the information panel does it: the
			//formula goes, the label is what was typed.
		done.relabelled_uuid = retagged->uuid().toString();
		done.relabelled_from = labelOf(retagged);
		DiagramContext information = retagged->elementInformations();
		information.remove(QStringLiteral("formula"));
		information.addValue(QStringLiteral("label"), RELABELLED);
		retagged->setElementInformations(information);

		done.renumbered = wire;
		done.renumbered_uuid = wire->uuid().toString();
		ConductorProperties properties = wire->properties();
		done.renumbered_from = properties.text;
		properties.m_formula.clear();
		properties.text = RENUMBERED;
		wire->setProperties(properties);

			//Two components of the same kind where the deleted one stood,
			//and a wire between them.
		done.first = drawComponent(sheet, type, where, NEW_FIRST);
		done.second = drawComponent(sheet, type, where + QPointF(100, 0),
					    NEW_SECOND);
		if (!done.first || !done.second
		    || done.first->terminals().isEmpty()
		    || done.second->terminals().isEmpty()) {
			done.why = QStringLiteral("drawing the new components failed");
			return done;
		}

		auto *added = new Conductor(done.first->terminals().first(),
					    done.second->terminals().first());
		sheet->undoStack().push(new AddGraphicsObjectCommand(added, sheet,
								     QPointF()));
		ConductorProperties fresh = added->properties();
		fresh.m_formula.clear();
		fresh.text = NEW_WIRE;
		added->setProperties(fresh);
		done.new_wire = added;

		done.done = true;
		return done;
	}

	QStringList uuidsOf(const QList<AssemblyChange> &changes)
	{
		QStringList uuids;
		for (const AssemblyChange &change : changes) {
			uuids << change.uuid;
		}
		return uuids;
	}

	/// The line of @a changes about @a uuid, or -1.
	int lineOf(const QList<AssemblyChange> &changes, const QString &uuid,
		   AssemblyAction action)
	{
		for (int index = 0 ; index < changes.size() ; ++index)
		{
			if (changes.at(index).uuid == uuid
			    && changes.at(index).action == action) {
				return index;
			}
		}
		return -1;
	}

	/**
		RFC 4180 with ';' between the cells, the reader of the bench. Written
		here, as in the suite of the rule, so that the file is read back by
		something that is not the writer.
	*/
	QVector<QStringList> parse(const QString &text)
	{
		QVector<QStringList> rows;
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
			} else if (c == QLatin1Char(';')) {
				row << cell;
				cell.clear();
				pending = true;
			} else if (c == QLatin1Char('\r') || c == QLatin1Char('\n')) {
				if (c == QLatin1Char('\r') && i + 1 < text.size()
				    && text.at(i + 1) == QLatin1Char('\n')) {
					++i;
				}
				row << cell;
				rows << row;
				row.clear();
				cell.clear();
				pending = false;
			} else {
				cell += c;
				pending = true;
			}
		}
		if (pending || !cell.isEmpty()) {
			row << cell;
			rows << row;
		}
		return rows;
	}
}

TEST_CASE("T29 — o relatório de alterações num projeto de examples/: sem "
	  "marcação não há 'desde', e marcado e intocado nada mudou",
	  "[uibench][assembly][changes]")
{
	UiBench::ScratchProject scratch(
		UiBench::fileContent(UiBench::examplePath(EXAMPLE)), EXAMPLE);
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	SECTION("sem marcação, nada é comparado")
	{
		const AssemblyChanges changes =
			AssemblyChangesDialog::compute(scratch.project());
		CHECK_FALSE(changes.isMarked());
		CHECK(changes.isEmpty());
	}

	SECTION("marcado e intocado, as seis listas ficam vazias")
	{
		/*
			The case that proves the walk reads what the photograph read.
			The photograph holds every component and every wire of the
			example; a single field read another way, or a single list
			walked another way, and this comes out in the hundreds.
		*/
		markAsAssembled(scratch.project());
		REQUIRE(scratch->assemblyState().components.size() > 0);
		REQUIRE(scratch->assemblyState().conductors.size() > 0);

		const AssemblyChanges changes =
			AssemblyChangesDialog::compute(scratch.project());
		REQUIRE(changes.isMarked());
		INFO(uuidsOf(changes.all()).join(QStringLiteral(", ")).toStdString());
		CHECK(changes.isEmpty());
		CHECK(changes.count() == 0);
	}

	SECTION("marcado, salvo e reaberto, continua sem mudança nenhuma")
	{
		/*
			The wires of the example carry no uuid in the file and are
			given a new one each time it is opened. If the photograph did
			not write out the one it named, every wire would come back
			from the reopening as one removed and one added - an order of
			service to rewire the whole panel, made of nothing.
		*/
		markAsAssembled(scratch.project());
		const int photographed = scratch->assemblyState().frozenCount();
		REQUIRE(photographed > 0);

		REQUIRE(scratch.saveAndReopen());
		REQUIRE(scratch->assemblyState().frozenCount() == photographed);

		const AssemblyChanges changes =
			AssemblyChangesDialog::compute(scratch.project());
		REQUIRE(changes.isMarked());
		INFO(uuidsOf(changes.all()).join(QStringLiteral(", ")).toStdString());
		CHECK(changes.isEmpty());
	}
}

TEST_CASE("T29 — apagar, desenhar, reetiquetar e renumerar: cada mudança na "
	  "sua lista, e só ela",
	  "[uibench][assembly][changes]")
{
	UiBench::ScratchProject scratch(
		UiBench::fileContent(UiBench::examplePath(EXAMPLE)), EXAMPLE);
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	markAsAssembled(scratch.project());
	const Modification done = modify(scratch.project());
	INFO(done.why.toStdString());
	REQUIRE(done.done);
	REQUIRE_FALSE(done.removed_wires.isEmpty());

	const AssemblyChanges changes =
		AssemblyChangesDialog::compute(scratch.project());
	REQUIRE(changes.isMarked());

	SECTION("componentes: dois a acrescentar, um a remover, um a reetiquetar")
	{
		REQUIRE(changes.components.to_add.size() == 2);
		REQUIRE(changes.components.to_remove.size() == 1);
		REQUIRE(changes.components.to_relabel.size() == 1);

		// Both on the folio of the one they replace, in the order of
		// their tags; the first stands exactly where the deleted one stood.
		const AssemblyChange &first = changes.components.to_add.at(0);
		const AssemblyChange &second = changes.components.to_add.at(1);
		CHECK(first.uuid == done.first->uuid().toString());
		CHECK(first.new_label == NEW_FIRST);
		CHECK(first.old_label.isEmpty());
		CHECK(first.folio == done.folio);
		CHECK(first.position == done.removed_position);
		CHECK_FALSE(first.description.isEmpty());
		CHECK(first.source >= 0);
		CHECK(second.uuid == done.second->uuid().toString());
		CHECK(second.new_label == NEW_SECOND);
		CHECK(second.folio == done.folio);

		const AssemblyChange &removed = changes.components.to_remove.first();
		CHECK(removed.uuid == done.removed_uuid);
		CHECK(removed.old_label == done.removed_label);
		CHECK(removed.new_label.isEmpty());
		CHECK(removed.folio == 0);
		CHECK(removed.position.isEmpty());
		CHECK(removed.source == -1);

		const AssemblyChange &relabelled = changes.components.to_relabel.first();
		CHECK(relabelled.uuid == done.relabelled_uuid);
		CHECK(relabelled.old_label == done.relabelled_from);
		CHECK(relabelled.new_label == RELABELLED);
		CHECK(relabelled.folio > 0);
	}

	SECTION("fios: o novo, os que saíram com o componente, e o renumerado")
	{
		REQUIRE(changes.wires.to_add.size() == 1);
		const AssemblyChange &added = changes.wires.to_add.first();
		CHECK(added.uuid == done.new_wire->uuid().toString());
		CHECK(added.new_label == NEW_WIRE);
		CHECK(added.folio == done.folio);
		// The two ends, named by the tags of the components they reach.
		INFO(added.description.toStdString());
		CHECK(added.description.contains(NEW_FIRST));
		CHECK(added.description.contains(NEW_SECOND));

		// Every wire the deletion took, and none other, each with the
		// number it carried.
		QMap<QString, QString> removed;
		for (const AssemblyChange &change : changes.wires.to_remove)
		{
			CHECK(change.new_label.isEmpty());
			CHECK(change.folio == 0);
			removed.insert(change.uuid, change.old_label);
		}
		CHECK(removed == done.removed_wires);

		REQUIRE(changes.wires.to_relabel.size() == 1);
		const AssemblyChange &renumbered = changes.wires.to_relabel.first();
		CHECK(renumbered.uuid == done.renumbered_uuid);
		CHECK(renumbered.old_label == done.renumbered_from);
		CHECK(renumbered.new_label == RENUMBERED);
	}

	SECTION("e nada além disso")
	{
		// 2 + 1 + 1 components, 1 + the wires that left + 1 wires: a walk
		// that read anything another way would add lines here.
		CHECK(changes.count()
		      == 4 + 1 + int(done.removed_wires.size()) + 1);
	}
}

TEST_CASE("T29 — a janela do relatório: o que ela mostra, o caminho até a "
	  "peça, e o arquivo da oficina escrito e relido",
	  "[uibench][assembly][changes]")
{
	UiBench::ScratchProject scratch(
		UiBench::fileContent(UiBench::examplePath(EXAMPLE)), EXAMPLE);
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	markAsAssembled(scratch.project());
	const Modification done = modify(scratch.project());
	INFO(done.why.toStdString());
	REQUIRE(done.done);

	AssemblyChangesDialog dialog(scratch.project());
	const QList<AssemblyChange> lines = dialog.changes().all();
	REQUIRE(lines.size() == AssemblyChangesDialog::compute(scratch.project())
				.count());

	SECTION("seis grupos, sempre, cada um com as suas linhas")
	{
		QTreeWidget *tree = dialog.findChild<QTreeWidget *>();
		REQUIRE(tree != nullptr);
		REQUIRE(tree->topLevelItemCount() == 6);

		const QList<int> expected {
			int(dialog.changes().components.to_add.size()),
			int(dialog.changes().components.to_remove.size()),
			int(dialog.changes().components.to_relabel.size()),
			int(dialog.changes().wires.to_add.size()),
			int(dialog.changes().wires.to_remove.size()),
			int(dialog.changes().wires.to_relabel.size())};
		QList<int> shown;
		for (int group = 0 ; group < tree->topLevelItemCount() ; ++group) {
			shown << tree->topLevelItem(group)->childCount();
		}
		CHECK(shown == expected);
	}

	SECTION("a linha leva ao componente, selecionado na sua folha")
	{
		const int line = lineOf(lines, done.first->uuid().toString(),
					AssemblyAction::Add);
		REQUIRE(line >= 0);

		// Something else selected first, so that "the selection was
		// cleared" is measured rather than assumed.
		done.sheet->clearSelection();
		done.second->setSelected(true);
		REQUIRE(done.second->isSelected());

		CHECK(dialog.goTo(line));
		CHECK(done.first->isSelected());
		CHECK_FALSE(done.second->isSelected());
	}

	SECTION("e ao fio renumerado")
	{
		const int line = lineOf(lines, done.renumbered_uuid,
					AssemblyAction::Relabel);
		REQUIRE(line >= 0);
		REQUIRE(done.renumbered);

		CHECK(dialog.goTo(line));
		CHECK(done.renumbered->isSelected());
	}

	SECTION("ativar a linha na lista faz o mesmo que goTo")
	{
		QTreeWidget *tree = dialog.findChild<QTreeWidget *>();
		REQUIRE(tree != nullptr);
		// The first group is the components to add, and its first row is
		// the first line of all().
		QTreeWidgetItem *row = tree->topLevelItem(0)->child(0);
		REQUIRE(row != nullptr);
		REQUIRE(lines.first().uuid == done.first->uuid().toString());

		done.sheet->clearSelection();
		emit tree->itemActivated(row, 0);
		CHECK(done.first->isSelected());

		// A heading leads nowhere, and does not take the selection away.
		emit tree->itemActivated(tree->topLevelItem(0), 0);
		CHECK(done.first->isSelected());
	}

	SECTION("uma linha de remoção não leva a lugar nenhum")
	{
		const int line = lineOf(lines, done.removed_uuid,
					AssemblyAction::Remove);
		REQUIRE(line >= 0);
		CHECK_FALSE(dialog.goTo(line));
		CHECK_FALSE(dialog.goTo(-1));
		CHECK_FALSE(dialog.goTo(int(lines.size())));
	}

	SECTION("o arquivo da oficina: escrito, relido, e linha por linha o da janela")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		const QString path = dir.filePath(QStringLiteral("changes.csv"));

		REQUIRE(dialog.exportTo(path).isEmpty());

		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		const QByteArray bytes = file.readAll();
		CHECK(bytes == dialog.changes().csvBytes());
		REQUIRE(bytes.left(3) == QByteArray("\xEF\xBB\xBF", 3));

		const QVector<QStringList> rows =
			parse(QString::fromUtf8(bytes.mid(3)));
		REQUIRE(rows.size() == lines.size() + 1);
		CHECK(rows.first() == QStringList {
			QStringLiteral("action"), QStringLiteral("kind"),
			QStringLiteral("old_label"), QStringLiteral("new_label"),
			QStringLiteral("folio"), QStringLiteral("position"),
			QStringLiteral("description"), QStringLiteral("part_code")});

		for (int index = 0 ; index < lines.size() ; ++index)
		{
			const QStringList &row = rows.at(index + 1);
			const AssemblyChange &line = lines.at(index);
			INFO("line " << index << ": " << line.uuid.toStdString());
			REQUIRE(row.size() == 8);
			CHECK(row.at(0) == AssemblyChanges::actionKey(line.action));
			CHECK(row.at(1) == AssemblyChanges::kindKey(line.kind));
			CHECK(row.at(2) == line.old_label);
			CHECK(row.at(3) == line.new_label);
			CHECK(row.at(6) == line.description);
		}

		// The line the shop reads for the retagged component.
		const int retagged = lineOf(lines, done.relabelled_uuid,
					    AssemblyAction::Relabel);
		REQUIRE(retagged >= 0);
		CHECK(rows.at(retagged + 1).at(0) == QStringLiteral("relabel"));
		CHECK(rows.at(retagged + 1).at(1) == QStringLiteral("component"));
		CHECK(rows.at(retagged + 1).at(2) == done.relabelled_from);
		CHECK(rows.at(retagged + 1).at(3) == RELABELLED);
	}

	SECTION("atualizar segue o desenho: desfeita a última mudança, a linha some")
	{
		// The last step on the stack is the new wire. Undone, the window
		// still shows it until it is asked again.
		const int wires_to_add = int(dialog.changes().wires.to_add.size());
		REQUIRE(wires_to_add == 1);

		scratch->undoStack()->undo();
		CHECK(dialog.changes().wires.to_add.size() == 1);

		dialog.refresh();
		CHECK(dialog.changes().wires.to_add.isEmpty());
	}
}

TEST_CASE("T29 — o botão do estado de montagem: desligado sem marcação, e com "
	  "ela abre o relatório sem aplicar estágio nenhum",
	  "[uibench][assembly][changes]")
{
	UiBench::ScratchProject scratch(
		UiBench::fileContent(UiBench::examplePath(EXAMPLE)), EXAMPLE);
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	auto button = [](AssemblyStateDialog &dialog) {
		return dialog.findChild<QPushButton *>(
			QStringLiteral("changes_since_assembly"));
	};

	SECTION("sem marcação o botão está desligado, e não abre nada")
	{
		AssemblyStateDialog dialog(scratch.project());
		dialog.show();
		REQUIRE(button(dialog) != nullptr);
		CHECK_FALSE(button(dialog)->isEnabled());
		CHECK_FALSE(dialog.changesReportAvailable());
		CHECK(dialog.showChangesReport() == nullptr);
		// And the window stays: there was nothing to step aside for.
		CHECK(dialog.isVisible());
	}

	SECTION("oferecer 'montado' na lista não liga o botão: o que conta é o projeto")
	{
		// The list compares with the photograph already taken, and the one
		// on offer is not taken until Ok is pressed.
		AssemblyStateDialog dialog(scratch.project());
		dialog.setChosenStage(AssemblyStage::Assembled);
		REQUIRE(button(dialog) != nullptr);
		CHECK_FALSE(button(dialog)->isEnabled());
	}

	SECTION("marcado, o botão abre o relatório e a janela fecha sem aplicar nada")
	{
		markAsAssembled(scratch.project());
		const int steps = scratch->undoStack()->count();

		AssemblyStateDialog dialog(scratch.project());
		dialog.show();
		REQUIRE(button(dialog) != nullptr);
		CHECK(button(dialog)->isEnabled());

		// Offering to put the project back on the drawing board, and then
		// looking at the changes instead: the stage on offer is dropped,
		// as Cancel drops it.
		dialog.setChosenStage(AssemblyStage::InProject);
		AssemblyChangesDialog *report = dialog.showChangesReport();
		REQUIRE(report != nullptr);

		CHECK(report->isVisible());
		CHECK_FALSE(report->isModal());
		CHECK(report->testAttribute(Qt::WA_DeleteOnClose));
		CHECK(report->changes().isMarked());
		CHECK(report->changes().isEmpty());

		// Out of the way - and result() alone would not say so, since a
		// window never closed answers Rejected too.
		CHECK_FALSE(dialog.isVisible());
		CHECK(dialog.result() == QDialog::Rejected);
		CHECK(scratch->undoStack()->count() == steps);
		CHECK(scratch->assemblyState().isFrozen());

		delete report;
	}
}
