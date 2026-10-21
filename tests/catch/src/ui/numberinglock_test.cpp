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

#include "../../../../sources/autoNum/assemblystate.h"
#include "../../../../sources/autoNum/numerotationcontext.h"
#include "../../../../sources/autoNum/projectrenumberer.h"
#include "../../../../sources/conductorautonumerotation.h"
#include "../../../../sources/conductorproperties.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetgraphicsitem/conductor.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/terminalnumberingdialog.h"
#include "../../../../sources/undocommand/addgraphicsobjectcommand.h"
#include "../../../../sources/undocommand/assemblystatecommand.h"
#include "../../../../sources/undocommand/linkelementcommand.h"

#include <catch2/catch.hpp>

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>

/*
	What the numbering may still write once a panel is built, and how the
	tick that locks a number by hand is read.

	Two subjects, one file, because they are the two halves of the same step
	of the plan: wire numbering had no lock at all, and the numberings that
	did have one read it two different ways.

	The wires are numbered here through ConductorAutoNumerotation, with the
	parent undo command the program hands it - the path a wire takes when it
	is drawn between two terminals and when it is created by auto-connection
	on dropping a symbol. What stays out is the gesture itself and the number
	appearing on the sheet: that is for a person to look at.
*/

namespace {

	struct Box
	{
		qreal x;
		qreal y;
		const char *label;
	};

	/**
		Eight boxes, two terminals each: east (terminal "1") and west
		(terminal "2").

		A and B carry the potential numbered 101, C and D the one numbered
		102: those are the two wires of the panel. The other four boxes are
		free, for the wires a case draws after the marking.
	*/
	const Box boxes[] = {
		{100, 100, "K1"},
		{220, 100, "K2"},
		{100, 300, "K3"},
		{220, 300, "K4"},
		{400, 100, "K5"},
		{520, 100, "K6"},
		{400, 300, "K7"},
		{520, 300, "K8"}};

	/// The two wires of the panel, by the uuid the fixture gives them.
	const char *wire_101_uuid = "{00c0ffee-0000-4000-8000-000000000101}";
	const char *wire_102_uuid = "{00c0ffee-0000-4000-8000-000000000102}";

	/**
		The first number the wire numbering of the cases hands out. Far from
		101 and 102 on purpose, so that a number a case reads back cannot be
		taken for one of the panel.
	*/
	const int first_new_number = 500;

	QString boxDefinitionXml()
	{
		return QStringLiteral(
			       "<element name=\"box.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"30\" height=\"20\""
			       " hotspot_x=\"15\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Box</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>");
	}

	/**
		The sheet of the wire cases.

		The terminals of each instance are written at the docking point,
		which is what an instance stores: box i has its east terminal as
		identifier 2i and its west one as 2i+1, and the two wires of the panel
		are written against those identifiers, the way the program writes
		them.
	*/
	QString wiresFixtureXml()
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QString instances;
		int index = 0;
		for (const Box &box : boxes)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/box.elmt\""
					     " uuid=\"{decaf000-0000-4000-8000-00000000000%3}\">"
					     "<terminals>"
					     "<terminal x=\"%4\" y=\"0\" orientation=\"1\" id=\"%5\"/>"
					     "<terminal x=\"%6\" y=\"0\" orientation=\"3\" id=\"%7\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%8"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(box.x)
				     .arg(box.y)
				     .arg(index)
				     .arg(east_dock)
				     .arg(index * 2)
				     .arg(west_dock)
				     .arg(index * 2 + 1)
				     .arg(QLatin1String(box.label));
			++index;
		}

			//K1 east (0) to K2 west (3), and K3 east (4) to K4 west (7).
		const QString conductors = QStringLiteral(
			"<conductor terminal1=\"0\" terminal2=\"3\" num=\"101\""
			" uuid=\"%1\" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>"
			"<conductor terminal1=\"4\" terminal2=\"7\" num=\"102\""
			" uuid=\"%2\" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>")
			.arg(QLatin1String(wire_101_uuid), QLatin1String(wire_102_uuid));

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/>"
			       "<conductors>%3</conductors>"
			       "</diagram>"
			       "</project>")
		       .arg(boxDefinitionXml(), instances, conductors);
	}

	/**
		Give @a sheet a wire numbering: one sequential part, starting at
		first_new_number. Set here rather than in the file, so that the
		fixture says what the panel looks like and this says only how the
		next wire is numbered.
	*/
	void useWireNumbering(QETProject *project, Diagram *sheet)
	{
		NumerotationContext context;
		context.addValue(QStringLiteral("unit"), first_new_number, 1,
				 first_new_number);
		project->addConductorAutoNum(QStringLiteral("bench"), context);
		project->setCurrentConductorAutoNum(QStringLiteral("bench"));
		sheet->setConductorsAutonumName(QStringLiteral("bench"));
	}

	Element *box(Diagram *sheet, const QString &label)
	{
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			if (element->elementInformations()
			    .value(QStringLiteral("label")).toString() == label) {
				return element;
			}
		}
		return nullptr;
	}

	Terminal *side(Element *element, Qet::Orientation orientation)
	{
		if (!element) {
			return nullptr;
		}
		const QList<Terminal *> terminals = element->terminals();
		for (Terminal *terminal : terminals)
		{
			if (terminal->orientation() == orientation) {
				return terminal;
			}
		}
		return nullptr;
	}

	Conductor *wireWithUuid(Diagram *sheet, const char *uuid)
	{
		const QList<Conductor *> wires = sheet->conductors();
		for (Conductor *wire : wires)
		{
			if (wire->uuid() == QUuid(QLatin1String(uuid))) {
				return wire;
			}
		}
		return nullptr;
	}

	QString textOf(const Conductor *wire)
	{
		return wire ? wire->properties().text : QString();
	}

	/**
		A wire drawn between @a from and @a to, numbered and pushed as one
		step of the undo stack - the shape the program gives it: the wire is
		added and numbered under one parent command, and that parent is what
		goes on the stack.
	*/
	Conductor *drawWire(Diagram *sheet, Terminal *from, Terminal *to)
	{
		auto *wire = new Conductor(from, to);
		auto *undo = new QUndoCommand(QStringLiteral("draw a wire"));
		new AddGraphicsObjectCommand(wire, sheet, QPointF(), undo);
		ConductorAutoNumerotation numbering(wire, sheet, undo);
		numbering.numerate();
		sheet->undoStack().push(undo);
		return wire;
	}

	void markAsAssembled(UiBench::ScratchProject &scratch)
	{
		scratch->undoStack()->push(new AssemblyStateCommand(
			scratch.project(),
			AssemblyStateCommand::photograph(scratch.project(),
							 AssemblyStage::Assembled)));
	}

	/**
		Answers the dialog that asks which of two potentials to keep, and
		counts how many times it came up.

		Two uses. Where the dialog is expected, it presses @a button, the
		way a person would. Where it is not, it is still installed: the
		dialog is a plain exec(), and under the offscreen platform an exec()
		nobody answers waits for ever - so a regression that brought the
		question back would hang the suite instead of failing it. With this
		in place it fails, on the count.
	*/
	class PotentialQuestion
	{
		public:
			explicit PotentialQuestion(QDialogButtonBox::StandardButton button) :
				m_button(button)
			{
				m_timer.setInterval(20);
				QObject::connect(&m_timer, &QTimer::timeout, [this]() {
					auto *dialog = qobject_cast<QDialog *>(
							       QApplication::activeModalWidget());
					if (!dialog || !dialog->isVisible()) {
						return;
					}
					++m_asked;
					QDialogButtonBox *box = dialog->findChild<QDialogButtonBox *>();
					QPushButton *pressed = box ? box->button(m_button) : nullptr;
					if (pressed) {
						pressed->click();
					} else {
						dialog->reject();
					}
				});
				m_timer.start();
			}

			/// How many times a modal dialog came up while this lived.
			int asked() const {return m_asked;}

		private:
			QTimer m_timer;
			QDialogButtonBox::StandardButton m_button;
			int m_asked = 0;
	};
}

TEST_CASE("CU-29.2 — fio novo em quadro montado: o potencial novo ganha número, "
	  "e nenhum número de fio existente muda",
	  "[uibench][assembly][wire]")
{
	/*
		The wires of a built panel carry printed sleeves, and the numbering
		of wires read no lock of any kind: neither the photograph of the
		marking nor anything else.

		The place a wire of the panel was actually written over is the
		question of which potential to keep. A new wire that joins two
		potentials whose numbers differ makes the program ask, and whichever
		answer is given, the number of the other potential is written over
		every one of its conductors. On a built panel that is a sleeve to
		reprint on a wire already in the duct - the sections below measure it
		without the marking, and then show that with the marking the question
		is answered without asking and nothing of the panel moves.
	*/
	UiBench::ScratchProject scratch(wiresFixtureXml(), QStringLiteral("wires.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 8);
	REQUIRE(sheet->conductors().count() == 2);
	useWireNumbering(scratch.project(), sheet);

	Conductor *wire_101 = wireWithUuid(sheet, wire_101_uuid);
	Conductor *wire_102 = wireWithUuid(sheet, wire_102_uuid);
	REQUIRE(wire_101 != nullptr);
	REQUIRE(wire_102 != nullptr);

	// What the panel wears, wire by wire. The whole subject: not one of these
	// may move once the panel is marked.
	auto panel = [&]() {
		return QStringList{textOf(wire_101), textOf(wire_102)};
	};
	const QStringList before = {QStringLiteral("101"), QStringLiteral("102")};
	REQUIRE(panel() == before);

	Element *k1 = box(sheet, QStringLiteral("K1"));
	Element *k2 = box(sheet, QStringLiteral("K2"));
	Element *k3 = box(sheet, QStringLiteral("K3"));
	Element *k5 = box(sheet, QStringLiteral("K5"));
	Element *k6 = box(sheet, QStringLiteral("K6"));
	Element *k7 = box(sheet, QStringLiteral("K7"));
	REQUIRE(k1 != nullptr);
	REQUIRE(k2 != nullptr);
	REQUIRE(k3 != nullptr);
	REQUIRE(k5 != nullptr);
	REQUIRE(k6 != nullptr);
	REQUIRE(k7 != nullptr);

	// The two ends the wires of the panel hang off, read off the sheet: a
	// new wire landing on one of these joins that potential.
	REQUIRE(side(k2, Qet::West) != nullptr);
	REQUIRE(side(k2, Qet::West)->conductors().contains(wire_101));
	REQUIRE(side(k3, Qet::East) != nullptr);
	REQUIRE(side(k3, Qet::East)->conductors().contains(wire_102));

	SECTION("controle negativo — sem a marcação, ligar dois potenciais reescreve um número que já existia")
	{
		/*
			Without this, every section below would pass on a program that
			had simply stopped numbering wires, and "nothing of the panel
			moved" would be the report of a broken numbering rather than of
			a working freeze. The dialog is answered the way a person answers
			it without reading: Ok, on what it offers.
		*/
		PotentialQuestion question(QDialogButtonBox::Ok);
		Conductor *bridge = drawWire(sheet, side(k2, Qet::West), side(k3, Qet::East));

		REQUIRE(question.asked() == 1);
		INFO(panel().join(QStringLiteral(", ")).toStdString());
		CHECK_FALSE(panel() == before);
		// One potential now, so one number on all three: one of the two the
		// panel wore has been written over.
		CHECK(textOf(wire_101) == textOf(wire_102));
		CHECK(textOf(bridge) == textOf(wire_101));
	}

	SECTION("marcado, um fio entre dois bornes livres é um potencial novo, e ganha número")
	{
		markAsAssembled(scratch);
		REQUIRE(scratch->assemblyState().conductors.size() == 2);
		PotentialQuestion question(QDialogButtonBox::Cancel);

		Conductor *fresh = drawWire(sheet, side(k5, Qet::East), side(k6, Qet::West));

		CHECK(textOf(fresh) == QString::number(first_new_number));
		CHECK_FALSE(ConductorAutoNumerotation::isFrozen(fresh));
		CHECK(question.asked() == 0);
		INFO(panel().join(QStringLiteral(", ")).toStdString());
		CHECK(panel() == before);

		// And the numbering goes on from there: the next new potential is
		// the next number, not the same one again.
		Conductor *second = drawWire(sheet, side(k7, Qet::East), side(k6, Qet::East));
		CHECK(textOf(second) == QString::number(first_new_number + 1));
		CHECK(panel() == before);
	}

	SECTION("marcado, um fio que chega a um potencial do quadro leva o número dele")
	{
		/*
			The half that already worked, kept in sight: a wire landing on a
			potential whose conductors all agree takes their number, which
			writes nothing over anything. It is here so that the section
			below cannot be read as "the panel freezes by refusing every
			wire that touches it".
		*/
		markAsAssembled(scratch);
		PotentialQuestion question(QDialogButtonBox::Cancel);

		Conductor *extension = drawWire(sheet, side(k2, Qet::West), side(k7, Qet::East));

		CHECK(textOf(extension) == QStringLiteral("101"));
		CHECK(question.asked() == 0);
		CHECK(panel() == before);
	}

	SECTION("marcado, um fio que liga o quadro a um potencial novo: o novo leva o número do quadro, sem perguntar")
	{
		markAsAssembled(scratch);
		PotentialQuestion question(QDialogButtonBox::Cancel);

		Conductor *fresh = drawWire(sheet, side(k5, Qet::East), side(k6, Qet::West));
		REQUIRE(textOf(fresh) == QString::number(first_new_number));
		const int steps_before = scratch->undoStack()->index();

		// K6 west is on the new potential, K1 east on the one numbered 101.
		Conductor *bridge = drawWire(sheet, side(k6, Qet::West), side(k1, Qet::East));

		// Asked nothing: of the two numbers, only one is on the panel, so
		// there is nothing to ask.
		CHECK(question.asked() == 0);
		INFO(panel().join(QStringLiteral(", ")).toStdString());
		CHECK(panel() == before);
		CHECK(textOf(bridge) == QStringLiteral("101"));
		CHECK(textOf(fresh) == QStringLiteral("101"));
		CHECK_FALSE(ConductorAutoNumerotation::isFrozen(fresh));

		// One step, and one Ctrl+Z gives the new potential its number back.
		REQUIRE(scratch->undoStack()->index() == steps_before + 1);
		scratch->undoStack()->undo();
		CHECK(textOf(fresh) == QString::number(first_new_number));
		CHECK(panel() == before);
	}

	SECTION("marcado, um fio que liga dois potenciais do quadro não reescreve nenhum dos dois")
	{
		/*
			The case the negative control above measures, on a marked panel.
			Both numbers are on sleeves, so either answer of the dialog would
			reprint one: nothing is written, and the new wire is left with
			the text it was drawn with - a wire nobody numbered, which is
			exactly what the sheet then shows.
		*/
		markAsAssembled(scratch);
		PotentialQuestion question(QDialogButtonBox::Cancel);

		Conductor *bridge = drawWire(sheet, side(k2, Qet::West), side(k3, Qet::East));

		CHECK(question.asked() == 0);
		INFO(panel().join(QStringLiteral(", ")).toStdString());
		CHECK(panel() == before);
		// The text a new wire of this sheet starts with, whatever the
		// settings of the machine running this make it.
		CHECK(textOf(bridge) == sheet->defaultConductorProperties.text);
		CHECK(textOf(bridge) != QStringLiteral("101"));
		CHECK(textOf(bridge) != QStringLiteral("102"));
	}

	SECTION("desfeita a marcação, a pergunta volta")
	{
		/*
			The freezing is read from the stage and the photograph, and never
			written on the conductor: take the marking back and the wires of
			the panel are wires like any other again. Answered with Cancel,
			so that what is measured is only that the question came back.
		*/
		markAsAssembled(scratch);
		REQUIRE(ConductorAutoNumerotation::isFrozen(wire_101));
		scratch->undoStack()->undo();
		REQUIRE_FALSE(scratch->assemblyState().isFrozen());
		CHECK_FALSE(ConductorAutoNumerotation::isFrozen(wire_101));
		CHECK_FALSE(ConductorAutoNumerotation::isFrozen(wire_102));

		PotentialQuestion question(QDialogButtonBox::Cancel);
		drawWire(sheet, side(k2, Qet::West), side(k3, Qet::East));
		CHECK(question.asked() == 1);
		CHECK(panel() == before);
	}
}

TEST_CASE("T29 — o fio congelado é o da fotografia, e só enquanto o projeto está marcado",
	  "[uibench][assembly][wire]")
{
	UiBench::ScratchProject scratch(wiresFixtureXml(), QStringLiteral("wires.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	Conductor *wire_101 = wireWithUuid(sheet, wire_101_uuid);
	REQUIRE(wire_101 != nullptr);

	SECTION("sem marcação, nenhum fio é congelado")
	{
		CHECK_FALSE(ConductorAutoNumerotation::isFrozen(wire_101));
	}

	SECTION("marcado, o fio da fotografia é congelado, e nada é gravado nele")
	{
		const ConductorProperties properties_before = wire_101->properties();
		markAsAssembled(scratch);
		CHECK(ConductorAutoNumerotation::isFrozen(wire_101));
		CHECK(wire_101->properties() == properties_before);
	}

	SECTION("uma fotografia sem o estágio não congela nada")
	{
		/*
			The half a reader drops first: the photograph alone reads like
			the whole answer, and it is not one. A state that holds the wire
			and says "in project" is what an unmarking leaves behind if it
			ever kept the photograph.
		*/
		AssemblyState state = AssemblyStateCommand::photograph(
			scratch.project(), AssemblyStage::Assembled);
		REQUIRE(state.holdsConductor(wire_101->uuid().toString()));
		CHECK(ConductorAutoNumerotation::isFrozen(wire_101, state));

		state.stage = AssemblyStage::InProject;
		CHECK_FALSE(ConductorAutoNumerotation::isFrozen(wire_101, state));
	}

	SECTION("um condutor fora de folha nunca é congelado")
	{
		CHECK_FALSE(ConductorAutoNumerotation::isFrozen(nullptr));
	}
}

namespace {

	/// One way the lock may have been written, and what it has to mean.
	struct Spelling
	{
		const char *value;
		bool locked;
	};

	/**
		Every row where the two readings that existed disagreed is here, and
		so are the plain ones on both sides of them.

		The renumbering of components read the value with QVariant::toBool(),
		which takes any text that is not empty, "0" or "false" as a yes; the
		numbering of terminals and the tick box of the information panel read
		it with QET::infoFlagIsTrue(), which takes only "true", "1", "yes" and
		"on", in any case and with the spaces around trimmed. The two agree
		on the first nine rows, and disagree on the last four - "no", "off",
		" false" and a word nobody planned for - which the renumbering kept
		locked while the tick box showed them unticked. That disagreement is
		read off the documented rule of QVariant::toBool(), and is not
		asserted below: what is asserted is the one reading that is left.
	*/
	const Spelling spellings[] = {
		{"true", true},
		{"1", true},
		{"TRUE", true},
		{" true ", true},
		{"yes", true},
		{"on", true},
		{"false", false},
		{"0", false},
		{"", false},
		{"no", false},
		{"off", false},
		{" false", false},
		{"maybe", false}};

	const int spelling_count = int(sizeof(spellings) / sizeof(spellings[0]));

	QString lockDefinitionXml(const QString &file_name, const QString &link_type)
	{
		return QStringLiteral(
			       "<element name=\"%1\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"40\""
			       " hotspot_x=\"10\" hotspot_y=\"20\""
			       " orientation=\"dnnn\" link_type=\"%2\">"
			       "<names><name lang=\"en\">%1</name></names>"
			       "<description>"
			       "<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
			       " end1=\"none\" end2=\"none\" length1=\"1.5\""
			       " length2=\"1.5\" antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"
			       "<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"
			       "</description>"
			       "</definition>"
			       "</element>")
		       .arg(file_name, link_type);
	}

	/// A contactor and a terminal of strip X1 per spelling, none locked yet.
	QString lockFixtureXml()
	{
		QString drawn;
		int uuid = 0;
		for (int row = 0 ; row < spelling_count ; ++row)
		{
			auto instance = [&](const QString &symbol, qreal y, const QString &label) {
				++uuid;
				return QStringLiteral(
					       "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					       " freezeLabel=\"false\" orientation=\"0\""
					       " type=\"embed://bench/%3\""
					       " uuid=\"{beef0000-0000-4000-8000-%4}\">"
					       "<terminals/><inputs/>"
					       "<elementInformations>"
					       "<elementInformation show=\"1\" name=\"label\">%5"
					       "</elementInformation>"
					       "</elementInformations>"
					       "<dynamic_texts/><texts_groups/>"
					       "</element>")
				       .arg(QString::number(100 + row * 50), QString::number(y),
					    symbol,
					    QStringLiteral("%1").arg(uuid, 12, 10, QLatin1Char('0')),
					    label);
			};
			drawn += instance(QStringLiteral("contactor.elmt"), 100,
					  QStringLiteral("K%1").arg(row + 1));
			drawn += instance(QStringLiteral("terminalblock.elmt"), 300,
					  QStringLiteral("X1:%1").arg(90 + row));
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1%2</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%3</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(lockDefinitionXml(QStringLiteral("contactor.elmt"),
					      QStringLiteral("simple")),
			    lockDefinitionXml(QStringLiteral("terminalblock.elmt"),
					      QStringLiteral("terminal")),
			    drawn);
	}

	QString labelOf(Element *element)
	{
		return element ? element->elementInformations()
				 .value(QStringLiteral("label")).toString()
			       : QString();
	}

	/// Write @a value under the lock key of @a element, as the panel does.
	void lock(Element *element, const char *value)
	{
		DiagramContext information = element->elementInformations();
		information.addValue(QStringLiteral("auto_num_locked"),
				     QString::fromLatin1(value));
		element->setElementInformations(information);
	}
}

TEST_CASE("T29 — a trava de mão se lê de um jeito só: a renumeração de componentes "
	  "e a numeração de bornes concordam",
	  "[uibench][renumber][terminal]")
{
	int expected_locked = 0;
	for (const Spelling &spelling : spellings) {
		expected_locked += spelling.locked ? 1 : 0;
	}
	// Both sides of the line have rows, said here so that a table edited
	// into one side only cannot pass unnoticed.
	REQUIRE(expected_locked == 6);
	REQUIRE(spelling_count - expected_locked == 7);

	SECTION("a leitura, grafia por grafia")
	{
		for (const Spelling &spelling : spellings)
		{
			DiagramContext information;
			information.addValue(QStringLiteral("auto_num_locked"),
					     QString::fromLatin1(spelling.value));
			INFO("\"" << spelling.value << "\"");
			CHECK(ProjectRenumberer::isLockedByHand(information) == spelling.locked);
		}
	}

	UiBench::ScratchProject scratch(lockFixtureXml(), QStringLiteral("locks.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == spelling_count * 2);

	QList<Element *> contactors;
	QList<Element *> terminals;
	for (int row = 0 ; row < spelling_count ; ++row)
	{
		Element *contactor = box(sheet, QStringLiteral("K%1").arg(row + 1));
		Element *terminal = box(sheet, QStringLiteral("X1:%1").arg(90 + row));
		REQUIRE(contactor != nullptr);
		REQUIRE(terminal != nullptr);
		REQUIRE(terminal->elementData().m_type == ElementData::Terminal);
		lock(contactor, spellings[row].value);
		lock(terminal, spellings[row].value);
		contactors << contactor;
		terminals << terminal;
	}

	SECTION("a renumeração de componentes lê assim")
	{
		// The project is not marked, so the tick is the only reason left.
		REQUIRE_FALSE(scratch->assemblyState().isFrozen());
		for (int row = 0 ; row < spelling_count ; ++row)
		{
			INFO("\"" << spellings[row].value << "\"");
			CHECK(ProjectRenumberer::isFrozen(contactors.at(row))
			      == spellings[row].locked);
		}
	}

	SECTION("a numeração de bornes lê igual")
	{
		/*
			The numbering of terminals renumbers every terminal of the strip
			it does not skip, from X1:1 on; every label of the fixture is
			X1:90 or more, so a terminal it numbered has changed and one it
			skipped has not. Counted both ways, and the sum said, so that a
			terminal neither numbered nor skipped has nowhere to hide.
		*/
		TerminalNumberingDialog dialog(nullptr, scratch.project());
		QUndoCommand *numbering = dialog.getUndoCommand(scratch.project());
		REQUIRE(numbering != nullptr);
		scratch->undoStack()->push(numbering);

		int kept = 0;
		int renumbered = 0;
		for (int row = 0 ; row < spelling_count ; ++row)
		{
			const bool unchanged =
				labelOf(terminals.at(row)) == QStringLiteral("X1:%1").arg(90 + row);
			INFO("\"" << spellings[row].value << "\" -> "
			     << labelOf(terminals.at(row)).toStdString());
			CHECK(unchanged == spellings[row].locked);
			if (unchanged) {
				++kept;
			} else {
				++renumbered;
			}
		}
		CHECK(kept == expected_locked);
		CHECK(renumbered == spelling_count - expected_locked);
		CHECK(kept + renumbered == spelling_count);
	}
}

namespace {

	/// The three wires of the panel in the report cases.
	const char *report_wire_101_uuid = "{00c0ffee-0000-4000-8000-000000002101}";
	const char *report_wire_102_uuid = "{00c0ffee-0000-4000-8000-000000002102}";
	const char *report_wire_103_uuid = "{00c0ffee-0000-4000-8000-000000002103}";

	/// Two pairs of arrows: A on both sides wired, B wired on folio 1 only.
	const char *going_a_uuid = "{decaf000-0000-4000-8000-0000000002a1}";
	const char *coming_a_uuid = "{decaf000-0000-4000-8000-0000000002a2}";
	const char *going_b_uuid = "{decaf000-0000-4000-8000-0000000002b1}";
	const char *coming_b_uuid = "{decaf000-0000-4000-8000-0000000002b2}";

	const char *report_box_1_uuid = "{decaf000-0000-4000-8000-0000000002c1}";
	const char *report_box_2_uuid = "{decaf000-0000-4000-8000-0000000002c2}";
	const char *report_box_3_uuid = "{decaf000-0000-4000-8000-0000000002c3}";
	/// The box a case wires the free arrow of pair B to, after the marking.
	const char *loose_box_uuid = "{decaf000-0000-4000-8000-0000000002c4}";

	/// A folio report arrow with one terminal, on top.
	QString arrowDefinitionXml(const QString &file_name, const QString &link_type)
	{
		return QStringLiteral(
			       "<element name=\"%1\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"40\""
			       " hotspot_x=\"10\" hotspot_y=\"20\""
			       " orientation=\"dnnn\" link_type=\"%2\">"
			       "<names><name lang=\"en\">%1</name></names>"
			       "<description>"
			       "<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
			       " end1=\"none\" end2=\"none\" length1=\"1.5\""
			       " length2=\"1.5\" antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"
			       "</description>"
			       "</definition>"
			       "</element>")
		       .arg(file_name, link_type);
	}

	QString reportCaseInstance(const QString &definition,
				   const char *uuid,
				   qreal x,
				   qreal y,
				   const QString &terminals)
	{
		return QStringLiteral(
			       "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/%3\" uuid=\"%4\">"
			       "<terminals>%5</terminals>"
			       "<inputs/>"
			       "<elementInformations/>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
		       .arg(QString::number(x), QString::number(y), definition,
			    QString::fromLatin1(uuid), terminals);
	}

	/**
		A box of the wire cases, its terminals written at the docking point:
		east as @a east_id, west as @a east_id + 1.
	*/
	QString boxInstance(const char *uuid, qreal x, qreal y, int east_id)
	{
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;
		return reportCaseInstance(
			QStringLiteral("box.elmt"), uuid, x, y,
			QStringLiteral(
				"<terminal x=\"%1\" y=\"0\" orientation=\"1\" id=\"%2\"/>"
				"<terminal x=\"%3\" y=\"0\" orientation=\"3\" id=\"%4\"/>")
			.arg(QString::number(east_dock), QString::number(east_id),
			     QString::number(west_dock), QString::number(east_id + 1)));
	}

	/// An arrow of @a definition, its one terminal as @a id.
	QString arrowInstance(const QString &definition,
			      const char *uuid,
			      qreal x,
			      qreal y,
			      int id)
	{
		const qreal north_dock = -10. + Terminal::terminalSize;
		return reportCaseInstance(
			definition, uuid, x, y,
			QStringLiteral("<terminal x=\"0\" y=\"%1\" orientation=\"0\" id=\"%2\"/>")
			.arg(QString::number(north_dock), QString::number(id)));
	}

	QString panelWire(int from, int to, const char *num, const char *uuid)
	{
		return QStringLiteral(
			       "<conductor terminal1=\"%1\" terminal2=\"%2\" num=\"%3\""
			       " uuid=\"%4\" displaytext=\"1\" type=\"multi\" condsize=\"1\"/>")
		       .arg(QString::number(from), QString::number(to),
			    QString::fromLatin1(num), QString::fromLatin1(uuid));
	}

	/**
		Two folios and two pairs of arrows, none of them linked in the file:
		the cases link them.

		Folio 1: the 101 leaves box 1 by arrow A, the 103 leaves box 3 by
		arrow B. Folio 2: arrow A arrives at box 2 as the 102; arrow B
		arrives on nothing yet, and the loose box waits next to it for the
		wire a case draws.
	*/
	QString reportsFixtureXml()
	{
		const QString going = QStringLiteral("going.elmt");
		const QString coming = QStringLiteral("coming.elmt");

		const QString elements_1 =
				boxInstance(report_box_1_uuid, 100, 100, 0)
				+ arrowInstance(going, going_a_uuid, 100, 300, 2)
				+ boxInstance(report_box_3_uuid, 300, 100, 3)
				+ arrowInstance(going, going_b_uuid, 300, 300, 5);
		const QString wires_1 =
				panelWire(0, 2, "101", report_wire_101_uuid)
				+ panelWire(3, 5, "103", report_wire_103_uuid);

		const QString elements_2 =
				arrowInstance(coming, coming_a_uuid, 100, 100, 6)
				+ boxInstance(report_box_2_uuid, 100, 300, 7)
				+ arrowInstance(coming, coming_b_uuid, 300, 100, 9)
				+ boxInstance(loose_box_uuid, 300, 300, 10);
			//arrow A (6) to the west terminal of box 2 (8)
		const QString wires_2 = panelWire(6, 8, "102", report_wire_102_uuid);

		const QString folio = QStringLiteral(
			"<diagram title=\"Folio %1\" order=\"%1\" height=\"600\""
			" cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%2</elements>"
			"<inputs/>"
			"<conductors>%3</conductors>"
			"</diagram>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1</category></collection>"
			       "%2%3"
			       "</project>")
		       .arg(boxDefinitionXml()
			    + arrowDefinitionXml(going, QStringLiteral("next_report"))
			    + arrowDefinitionXml(coming, QStringLiteral("previous_report")),
			    folio.arg(QStringLiteral("1"), elements_1, wires_1),
			    folio.arg(QStringLiteral("2"), elements_2, wires_2));
	}

	Element *elementWithUuid(Diagram *sheet, const char *uuid)
	{
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			if (element->uuid() == QUuid(QLatin1String(uuid))) {
				return element;
			}
		}
		return nullptr;
	}

	/// Link @a from to @a to the way the linking widget does it: one
	/// LinkElementCommand, pushed on the stack of the project.
	void linkArrows(QETProject *project, Element *from, Element *to)
	{
		auto *link = new LinkElementCommand(from);
		link->setLink(to);
		project->undoStack()->push(link);
	}
}

TEST_CASE("T29 — ligar duas setas de folha em quadro montado: o fio do quadro vence "
	  "sem perguntar, e dois fios do quadro não se reescrevem",
	  "[uibench][assembly][wire][report]")
{
	/*
		The other way two potentials become one. Linking a folio report to
		its pair joins the potential at one arrow to the potential at the
		other, and when their numbers differ the program asks which one to
		keep - the same question, with the same consequence, as a new wire
		between two potentials: whichever answer is given, the number of the
		other side is written over every one of its conductors. The link
		went straight to the dialog, and the rule of a built panel did not
		reach it.

		Pair A joins two wires of the panel, the 101 on folio 1 and the 102
		on folio 2. Pair B joins the 103 of the panel to an arrow that
		nothing is wired to in the file: the cases wire it after the
		marking, so that one side of the link is a potential the panel does
		not have.

		Labelled T29 and not CU-29.2: the use case is a new wire and the
		processing of wires, which the CU-29.2 case at the top of this file
		proves; this is the other path into the same rule.
	*/
	UiBench::ScratchProject scratch(reportsFixtureXml(), QStringLiteral("reports.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagramCount() == 2);

	Diagram *first = scratch.diagram(0);
	Diagram *second = scratch.diagram(1);
	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	// The probe: a terminal that does not match its symbol drops its wires
	// without a word, and everything below would be measured on nothing.
	REQUIRE(first->conductors().count() == 2);
	REQUIRE(second->conductors().count() == 1);
	useWireNumbering(scratch.project(), second);

	Conductor *wire_101 = wireWithUuid(first, report_wire_101_uuid);
	Conductor *wire_103 = wireWithUuid(first, report_wire_103_uuid);
	Conductor *wire_102 = wireWithUuid(second, report_wire_102_uuid);
	REQUIRE(wire_101 != nullptr);
	REQUIRE(wire_102 != nullptr);
	REQUIRE(wire_103 != nullptr);

	auto panel = [&]() {
		return QStringList{textOf(wire_101), textOf(wire_102), textOf(wire_103)};
	};
	const QStringList before = {QStringLiteral("101"),
				    QStringLiteral("102"),
				    QStringLiteral("103")};
	REQUIRE(panel() == before);

	Element *going_a = elementWithUuid(first, going_a_uuid);
	Element *coming_a = elementWithUuid(second, coming_a_uuid);
	Element *going_b = elementWithUuid(first, going_b_uuid);
	Element *coming_b = elementWithUuid(second, coming_b_uuid);
	Element *loose = elementWithUuid(second, loose_box_uuid);
	REQUIRE(going_a != nullptr);
	REQUIRE(coming_a != nullptr);
	REQUIRE(going_b != nullptr);
	REQUIRE(coming_b != nullptr);
	REQUIRE(loose != nullptr);

	// The fixture is what it says: four free arrows, each where its wire is.
	REQUIRE(going_a->linkType() == Element::NextReport);
	REQUIRE(going_b->linkType() == Element::NextReport);
	REQUIRE(coming_a->linkType() == Element::PreviousReport);
	REQUIRE(coming_b->linkType() == Element::PreviousReport);
	REQUIRE(going_a->isFree());
	REQUIRE(coming_a->isFree());
	REQUIRE(going_b->isFree());
	REQUIRE(coming_b->isFree());
	REQUIRE(going_a->conductors().size() == 1);
	REQUIRE(going_a->conductors().first() == wire_101);
	REQUIRE(coming_a->conductors().size() == 1);
	REQUIRE(coming_a->conductors().first() == wire_102);
	REQUIRE(going_b->conductors().size() == 1);
	REQUIRE(going_b->conductors().first() == wire_103);
	REQUIRE(coming_b->conductors().isEmpty());
	REQUIRE(coming_b->terminals().size() == 1);
	REQUIRE(side(loose, Qet::West) != nullptr);

	// A wire on folio 2, from the free arrow of pair B to the loose box: a
	// potential of its own while the arrows are not linked.
	auto drawFresh = [&]() {
		return drawWire(second, coming_b->terminals().first(), side(loose, Qet::West));
	};

	SECTION("controle negativo — sem a marcação, ligar as setas pergunta, e a resposta reescreve um número que já existia")
	{
		/*
			Without this, the sections below would pass on a program that
			had stopped asking when two arrows are linked, and "nothing of
			the panel moved" would say nothing about the marking. The
			question is answered Ok, on what it offers.
		*/
		PotentialQuestion question(QDialogButtonBox::Ok);
		Conductor *fresh = drawFresh();
		REQUIRE(question.asked() == 0);
		REQUIRE(textOf(fresh) == QString::number(first_new_number));

		// Both links would ask, unmarked: the two sides carry two numbers.
		CHECK(LinkElementCommand::reportLinkNeedsPotentialChoice(going_a, coming_a));
		CHECK(LinkElementCommand::reportLinkNeedsPotentialChoice(going_b, coming_b));

		linkArrows(scratch.project(), going_a, coming_a);

		REQUIRE(question.asked() == 1);
		CHECK(going_a->linkedElements().contains(coming_a));
		INFO(panel().join(QStringLiteral(", ")).toStdString());
		CHECK_FALSE(panel() == before);
		// One potential now, so one number on both sides: one of the two the
		// panel wore has been written over.
		CHECK(textOf(wire_101) == textOf(wire_102));
	}

	SECTION("só duas setas perguntam: um elemento que não é seta não, com fio e tudo (P130)")
	{
		/*
			The question asked of a script before a link is the one the
			link asks, and the link asks it of two reports only. Answered
			for any pair, it refused a contact and its coil because both
			carried wires that disagreed - a link that opens no dialog.
		*/
		Conductor *fresh = drawFresh();
		REQUIRE(fresh != nullptr);
		REQUIRE_FALSE(loose->conductors().isEmpty());
		REQUIRE_FALSE(going_a->conductors().isEmpty());
		CHECK_FALSE(LinkElementCommand::reportLinkNeedsPotentialChoice(loose, going_a));
			//And the two arrows still ask: the guard did not quiet the case
			//it is for
		CHECK(LinkElementCommand::reportLinkNeedsPotentialChoice(going_b, coming_b));
	}

	SECTION("marcado, a seta que liga um fio do quadro a um fio novo: o novo leva o número do quadro, sem perguntar")
	{
		markAsAssembled(scratch);
		REQUIRE(scratch->assemblyState().conductors.size() == 3);
		PotentialQuestion question(QDialogButtonBox::Cancel);

		Conductor *fresh = drawFresh();
		REQUIRE(textOf(fresh) == QString::number(first_new_number));
		REQUIRE_FALSE(ConductorAutoNumerotation::isFrozen(fresh));
		REQUIRE(ConductorAutoNumerotation::isFrozen(wire_103));

		// A caller with nobody there to answer is not refused the link
		// either: it would not ask.
		CHECK_FALSE(LinkElementCommand::reportLinkNeedsPotentialChoice(going_b, coming_b));
		const int steps_before = scratch->undoStack()->index();

		linkArrows(scratch.project(), going_b, coming_b);

		CHECK(question.asked() == 0);
		CHECK(going_b->linkedElements().contains(coming_b));
		INFO(panel().join(QStringLiteral(", ")).toStdString());
		CHECK(panel() == before);
		CHECK(textOf(fresh) == QStringLiteral("103"));
		CHECK_FALSE(ConductorAutoNumerotation::isFrozen(fresh));

		// One step, and one Ctrl+Z takes back the link and the number
		// together.
		REQUIRE(scratch->undoStack()->index() == steps_before + 1);
		scratch->undoStack()->undo();
		CHECK(going_b->isFree());
		CHECK(textOf(fresh) == QString::number(first_new_number));
		CHECK(panel() == before);

		// Made from the other arrow, the link answers the same: which side
		// it starts from is not what decides.
		linkArrows(scratch.project(), coming_b, going_b);
		CHECK(question.asked() == 0);
		CHECK(coming_b->linkedElements().contains(going_b));
		CHECK(textOf(fresh) == QStringLiteral("103"));
		CHECK(panel() == before);
	}

	SECTION("marcado, a seta que liga dois fios do quadro não reescreve nenhum dos dois")
	{
		/*
			Both numbers are on sleeves, so either answer of the question
			would reprint one: nothing is written. The link itself is made -
			what is refused is the number, not the arrow - and the potential
			left with two numbers is what the verification of T24 reports.
		*/
		markAsAssembled(scratch);
		PotentialQuestion question(QDialogButtonBox::Cancel);
		CHECK_FALSE(LinkElementCommand::reportLinkNeedsPotentialChoice(going_a, coming_a));
		const int steps_before = scratch->undoStack()->index();

		linkArrows(scratch.project(), going_a, coming_a);

		CHECK(question.asked() == 0);
		CHECK(going_a->linkedElements().contains(coming_a));
		CHECK(scratch->undoStack()->index() == steps_before + 1);
		INFO(panel().join(QStringLiteral(", ")).toStdString());
		CHECK(panel() == before);
	}

	SECTION("desfeita a marcação, a pergunta volta")
	{
		/*
			Nothing was written on the wires by the marking: take it back and
			the arrows ask again. Answered Cancel, so that what is measured
			is only that the question came back.
		*/
		markAsAssembled(scratch);
		REQUIRE(ConductorAutoNumerotation::isFrozen(wire_101));
		scratch->undoStack()->undo();
		REQUIRE_FALSE(scratch->assemblyState().isFrozen());
		CHECK(LinkElementCommand::reportLinkNeedsPotentialChoice(going_a, coming_a));

		PotentialQuestion question(QDialogButtonBox::Cancel);
		linkArrows(scratch.project(), going_a, coming_a);

		CHECK(question.asked() == 1);
		CHECK(going_a->linkedElements().contains(coming_a));
		CHECK(panel() == before);
	}
}
