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

#include "../../../../sources/autoNum/iecstructure.h"
#include "../../../../sources/autoNum/ui/iecstructuredialog.h"
#include "../../../../sources/bordertitleblock.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/titleblock/templatelocation.h"
#include "../../../../sources/titleblockproperties.h"

#include <catch2/catch.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSignalSpy>

/*
	The identification structure of the norm, turned off, on a delivered
	project.

	This is the most important case of the block, and the reason is not that
	the norm is hard: it is that the switch is optional and a project drawn
	before it existed must open exactly as it always did. "Exactly" is the
	word the roteiro uses, and it is checkable without a screen - the tag the
	sheet draws is a string, and two lists of strings are either equal or they
	are not.

	The dialog is driven for real: it is built on the open project, its own
	buttons are pressed, and Cancel goes through QDialogButtonBox::rejected
	like anybody's click. What is not done is exec() - a modal exec() under
	the offscreen platform waits for ever - and nothing is lost by that,
	because exec() only spins an event loop around the widget that is being
	driven here.

	What this case does not prove, and what stays with the human queue: that
	the preview reads legibly, that the window fits, and that the three
	controls are laid out in a way anyone understands. It proves what they do,
	not how they look.
*/

namespace {

	/// A delivered project of the current format, in the sense the roteiro means.
	const QString reference_example = QStringLiteral("industrial.qet");

	/// Every tag the project draws, sheet by sheet, in the order it draws them.
	QStringList allDisplayedLabels(QETProject *project)
	{
		QStringList labels;
		if (!project) {
			return labels;
		}
		const QList<Diagram *> sheets = project->diagrams();
		for (Diagram *sheet : sheets) {
			labels << UiBench::displayedLabels(sheet);
		}
		return labels;
	}

	/// The first place two lists of tags disagree, as "before -> after".
	QString firstDifference(const QStringList &before, const QStringList &after)
	{
		const int shared = qMin(before.count(), after.count());
		for (int index = 0 ; index < shared ; ++index)
		{
			if (before.at(index) != after.at(index)) {
				return QStringLiteral("tag %1: %2 -> %3")
				       .arg(QString::number(index),
					    before.at(index), after.at(index));
			}
		}
		if (before.count() != after.count()) {
			return QStringLiteral("count: %1 -> %2")
			       .arg(QString::number(before.count()),
				    QString::number(after.count()));
		}
		return QString();
	}

	/// The check box that turns the structure on: the first one of the dialog.
	QCheckBox *enabledBox(IecStructureDialog &dialog)
	{
		const QList<QCheckBox *> boxes = dialog.findChildren<QCheckBox *>();
		return boxes.isEmpty() ? nullptr : boxes.first();
	}

	/// Presses a button of the dialog, the way a click on it would.
	bool press(IecStructureDialog &dialog, QDialogButtonBox::StandardButton which)
	{
		QDialogButtonBox *box = dialog.findChild<QDialogButtonBox *>();
		if (!box) {
			return false;
		}
		QPushButton *pushed = box->button(which);
		if (!pushed) {
			return false;
		}
		pushed->click();
		return true;
	}

	/**
		Opens the dialog on @a project, sets the switch to @a on, and confirms.

		Through the dialog and not through QETProject::setIecSettings, because
		what the roteiro exercises is the window: a switch that only works when
		it is set from code is a switch nobody can use.
	*/
	void switchStructure(QETProject *project, bool on)
	{
		IecStructureDialog dialog(project);
		QCheckBox *box = enabledBox(dialog);
		REQUIRE(box != nullptr);
		box->setChecked(on);
		REQUIRE(press(dialog, QDialogButtonBox::Ok));
		REQUIRE(dialog.result() == QDialog::Accepted);
	}

	/// A one component project, so that the exact composed tag can be written down.
	QString fixtureXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"contactor.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Contactor</name></names>"
			       "<description>"
			       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"15\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>"
			       "<element x=\"100\" y=\"100\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/contactor.elmt\""
			       " uuid=\"{c0ffee01-0000-4000-8000-000000000001}\">"
			       "<terminals/><inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">K1"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>"
			       "</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	// The two fields the folio hands down, as the roteiro writes them.
	const QString folio_plant = QStringLiteral("CT1");
	const QString folio_location = QStringLiteral("A1");
}

TEST_CASE("F1 G.1 — estrutura CEI desligada: cancelar o dialogo nao muda nada",
	  "[uibench][iec]")
{
	UiBench::Project project(reference_example);
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	const QStringList before = allDisplayedLabels(project.project());
	// A project that draws no tag at all would make every comparison below
	// true for the wrong reason.
	REQUIRE_FALSE(before.isEmpty());

	SECTION("um projeto entregue abre com a estrutura desligada")
	{
		const IecStructureSettings settings = project->iecSettings();
		REQUIRE(settings.enabled == false);
		REQUIRE(settings.location_from_element == false);
		REQUIRE(settings.display == IecTagDisplay::Short);
	}

	SECTION("desligada, a composicao e a identidade: K1 continua K1")
	{
		Diagram *sheet = project.busiestSheet();
		REQUIRE(sheet != nullptr);
		const QList<Element *> elements = sheet->elements();
		REQUIRE_FALSE(elements.isEmpty());
		REQUIRE(elements.first()->composedLabel(QStringLiteral("K1"))
			== QStringLiteral("K1"));
	}

	SECTION("cancelar o dialogo deixa a configuracao como estava")
	{
		IecStructureDialog dialog(project.project());
		QCheckBox *box = enabledBox(dialog);
		REQUIRE(box != nullptr);

		// Ticked, then cancelled: the case the roteiro describes is closing
		// without marking anything, and this is the harder version of it -
		// marked, and thrown away.
		box->setChecked(true);
		REQUIRE(press(dialog, QDialogButtonBox::Cancel));
		REQUIRE(dialog.result() == QDialog::Rejected);
		REQUIRE(project->iecSettings().enabled == false);
	}

	SECTION("cancelar o dialogo deixa todas as etiquetas exatamente como estavam")
	{
		IecStructureDialog dialog(project.project());
		QCheckBox *box = enabledBox(dialog);
		REQUIRE(box != nullptr);
		box->setChecked(true);
		REQUIRE(press(dialog, QDialogButtonBox::Cancel));

		const QStringList after = allDisplayedLabels(project.project());
		INFO(firstDifference(before, after).toStdString());
		REQUIRE(after == before);
	}

	SECTION("cancelar o dialogo nao marca o projeto como modificado")
	{
		QSignalSpy spy(project.project(), &QETProject::projectModified);
		REQUIRE(spy.isValid());

		IecStructureDialog dialog(project.project());
		QCheckBox *box = enabledBox(dialog);
		REQUIRE(box != nullptr);
		box->setChecked(true);
		REQUIRE(press(dialog, QDialogButtonBox::Cancel));

		REQUIRE(spy.count() == 0);
	}

	SECTION("controle negativo — confirmada com a caixa marcada, a mesma lista muda")
	{
		// Everything above says a list did not change. None of it is worth
		// anything until the same comparison is shown changing, on the same
		// project, through the same window.
		switchStructure(project.project(), true);
		REQUIRE(project->iecSettings().enabled == true);

		const QStringList after = allDisplayedLabels(project.project());
		REQUIRE(after.count() == before.count());
		INFO(firstDifference(before, after).toStdString());
		REQUIRE_FALSE(after == before);
		// And the difference is named, not merely counted.
		REQUIRE_FALSE(firstDifference(before, after).isEmpty());
	}

	SECTION("controle negativo — e desligar volta tudo, sem desfazer nada")
	{
		switchStructure(project.project(), true);
		REQUIRE_FALSE(allDisplayedLabels(project.project()) == before);

		switchStructure(project.project(), false);
		const QStringList after = allDisplayedLabels(project.project());
		INFO(firstDifference(before, after).toStdString());
		REQUIRE(after == before);
	}
}

TEST_CASE("F1 G.1 (controle negativo) — ligada, K1 vira -K1 e =CT1+A1-K1",
	  "[uibench][iec]")
{
	/*
		The exact strings of the roteiro, on a folio that carries the two
		fields it names: G.2 asks for -K1, G.3 for =CT1+A1-K1, and G.5 for K1
		back again with nothing undone. Written on a project of one component
		because that is what makes the three of them exact rather than "the
		list changed".
	*/
	UiBench::ScratchProject scratch(fixtureXml(), QStringLiteral("iec.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().count() == 1);
	Element *contactor = sheet->elements().first();

	TitleBlockProperties folio = sheet->border_and_titleblock.exportTitleBlock();
	folio.plant = folio_plant;
	folio.locmach = folio_location;
	sheet->border_and_titleblock.importTitleBlock(folio);

	SECTION("desligada, o desenho mostra K1")
	{
		REQUIRE(scratch.project()->iecSettings().enabled == false);
		REQUIRE(contactor->displayedLabel() == QStringLiteral("K1"));
	}

	SECTION("curta, o desenho mostra -K1")
	{
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Short;
		scratch.project()->setIecSettings(settings);

		REQUIRE(contactor->displayedLabel() == QStringLiteral("-K1"));
	}

	SECTION("completa, o desenho mostra =CT1+A1-K1, herdado do folio")
	{
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Full;
		scratch.project()->setIecSettings(settings);

		REQUIRE(contactor->displayedLabel() == QStringLiteral("=CT1+A1-K1"));
	}

	SECTION("o campo do componente continua sendo K1 nas tres situacoes")
	{
		// The promise the dialog makes out loud, and the one that makes the
		// switch safe: the composition is drawn, never written back.
		IecStructureSettings settings;
		settings.enabled = true;
		settings.display = IecTagDisplay::Full;
		scratch.project()->setIecSettings(settings);

		REQUIRE(contactor->elementInformations()
			.value(IecStructure::productKey()).toString()
			== QStringLiteral("K1"));
	}

	SECTION("desligar volta a K1, sem desfazer nada")
	{
		IecStructureSettings on;
		on.enabled = true;
		on.display = IecTagDisplay::Full;
		scratch.project()->setIecSettings(on);
		REQUIRE(contactor->displayedLabel() == QStringLiteral("=CT1+A1-K1"));

		IecStructureSettings off;
		off.enabled = false;
		scratch.project()->setIecSettings(off);
		REQUIRE(contactor->displayedLabel() == QStringLiteral("K1"));
	}
}
