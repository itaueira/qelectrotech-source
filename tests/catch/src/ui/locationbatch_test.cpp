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

#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/location/locatableelement.h"
#include "../../../../sources/location/locationtree.h"
#include "../../../../sources/location/projectlocation.h"
#include "../../../../sources/location/ui/locationreportdialog.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/undocommand/assignlocationcommand.h"

#include <catch2/catch.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QRect>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUndoStack>
#include <QWidget>

/*
	The batch assignment of a location, from the window of the components
	nobody has placed yet, without a screen.

	A hundred and twenty components on two folios, none of them located:
	eighty on the first folio, forty on the second. Eighty are picked in the
	window "Composants non localisés", "Affecter à…" is pressed, the question
	is answered with the main panel, and the list falls from 120 to 40 in one
	step of the undo stack; one undo brings it back to 120.

	What is driven is the window itself, and not the command behind it. The
	folio row is clicked with the mouse, the button is clicked with the
	mouse, and the question - a modal QInputDialog::getItem - is answered
	from inside its own exec(), by picking the entry in its list and pressing
	OK. The numbers are read off the window, from its rows and from the
	sentences a person reads there, and every component is asked which
	location it carries: a count that agrees with itself proves nothing about
	which eighty moved.

	Three things are not driven, and they are said here rather than left to
	be discovered:
	- the menu entry that opens the window, because the diagram editor that
	  owns it is not built by this bench;
	- Ctrl+Z as a key. The window has no shortcut of its own, and the one of
	  the program is the undo action of the editor, which ends in undo() on
	  the stack of the project - the call made here;
	- eighty Ctrl+clicks. When the rows are picked one by one, they are
	  selected through their items, which is what a Ctrl+click does to each
	  of them; the folio row, which is the gesture the window is made for, is
	  clicked for real.
*/

namespace
{
	/// The two locations of the project. The main panel is the second one on
	/// purpose: the question opens on the first, so a case that never touched
	/// the list of the question would send the eighty to the wrong place.
	const QString aux_code = QStringLiteral("QA");
	const QString aux_name = QStringLiteral("Quadro Auxiliar");
	const QString main_code = QStringLiteral("QP");
	const QString main_name = QStringLiteral("Quadro Principal");

	/// What the question offers for each of them, as the person reads it.
	const QString aux_entry = QString::fromUtf8("+QA — Quadro Auxiliar");
	const QString main_entry = QString::fromUtf8("+QP — Quadro Principal");

	/// The sentences of the window and of the question the case reads, with
	/// their numbers written out rather than computed.
	const QString hundred_twenty_left =
		QString::fromUtf8("120 composant(s) non localisé(s)");
	const QString forty_left = QString::fromUtf8("40 composant(s) non localisé(s)");
	const QString eighty_picked = QString::fromUtf8("80 composant(s) choisi(s).");
	const QString eighty_asked = QString::fromUtf8("Mettre 80 composant(s) dans :");
	const QString eighty_to_main =
		QString::fromUtf8("Affecter 80 composant(s) à la localisation QP");
	const QString eighty_to_aux =
		QString::fromUtf8("Affecter 80 composant(s) à la localisation QA");
	const QString eighty_done = QString::fromUtf8("80 composant(s) affecté(s) à « +QP ».");

	/// "K001": the label of the component numbered @a number.
	QString labelOf(int number)
	{
		return QStringLiteral("K%1").arg(number, 3, 10, QLatin1Char('0'));
	}

	/// The labels of the components numbered @a first to @a last, in order -
	/// which is also the sorted order, the numbers being padded.
	QStringList labels(int first, int last)
	{
		QStringList list;
		for (int number = first ; number <= last ; ++number) {
			list << labelOf(number);
		}
		return list;
	}

	/**
		The components numbered @a first to @a last, as XML, ten to a row.

		The symbol has no terminal, as in the case of the live rectangle:
		nothing here is about conductors, and a symbol that can be wired is a
		symbol that can bring a conductor command onto the stack this case
		counts.
	*/
	QString instances(int first, int last)
	{
		QString xml;
		for (int number = first ; number <= last ; ++number)
		{
			const int place = number - first;
			xml += QStringLiteral(
				       "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
				       " freezeLabel=\"false\" orientation=\"0\""
				       " type=\"embed://bench/relay.elmt\""
				       " uuid=\"{c0ffee00-0000-4000-8000-%3}\">"
				       "<terminals/><inputs/>"
				       "<elementInformations>"
				       "<elementInformation show=\"1\" name=\"label\">%4"
				       "</elementInformation>"
				       "</elementInformations>"
				       "<dynamic_texts/><texts_groups/>"
				       "</element>")
			       .arg(60 + 90 * (place % 10))
			       .arg(60 + 70 * (place / 10))
			       .arg(number, 12, 10, QLatin1Char('0'))
			       .arg(labelOf(number));
		}
		return xml;
	}

	/**
		The project the case works on, as text: K001 to K080 on the first
		folio, K081 to K120 on the second, and no location anywhere.

		Built here rather than taken from examples/ because the case is about
		a number - 120, of which 80 - and a fixture written by the case is one
		whose expected numbers are not a guess about somebody else's drawing.
	*/
	QString fixtureXml()
	{
		const QString sheet = QStringLiteral(
			"<diagram title=\"%1\" order=\"%2\" height=\"640\""
			" cols=\"20\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements>%3</elements>"
			"<inputs/><conductors/>"
			"</diagram>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"relay.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Relay</name></names>"
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
			       "%1%2"
			       "</project>")
		       .arg(sheet.arg(QStringLiteral("Sheet A")).arg(1).arg(instances(1, 80)),
			    sheet.arg(QStringLiteral("Sheet B")).arg(2).arg(instances(81, 120)));
	}

	/// The component labelled @a label on any folio, nullptr when there is none.
	Element *componentLabelled(QETProject *project, const QString &label)
	{
		if (!project) {
			return nullptr;
		}

		const QList<Diagram *> sheets = project->diagrams();
		for (Diagram *sheet : sheets)
		{
			const QList<Element *> elements = sheet->elements();
			for (Element *element : elements)
			{
				if (element->elementInformations()
				    .value(QETInformation::ELMT_LABEL).toString() == label) {
					return element;
				}
			}
		}
		return nullptr;
	}

	/**
		The labels of the components carrying @a path, sorted. An empty path
		asks for the components carrying none - the ones the window lists.

		Asked of the components themselves and not of the window: the window
		saying 40 is one fact, the eighty others carrying the main panel is
		another, and only the second says the assignment went where it was
		sent.
	*/
	QStringList componentsAt(QETProject *project, const QString &path)
	{
		QStringList names;
		if (!project) {
			return names;
		}

		const QList<Diagram *> sheets = project->diagrams();
		for (Diagram *sheet : sheets)
		{
			const QList<Element *> elements = sheet->elements();
			for (Element *element : elements)
			{
				if (!isLocatableElement(element)) {
					continue;
				}
				const DiagramContext information = element->elementInformations();
				if (information.value(QETInformation::ELMT_LOCATION_PATH).toString()
				    != path) {
					continue;
				}
				names << information.value(QETInformation::ELMT_LABEL).toString();
			}
		}

		names.sort();
		return names;
	}

	/// The column of @a tree whose header reads @a title, -1 when there is none.
	int columnTitled(QTreeWidget *tree, const QString &title)
	{
		if (!tree || !tree->headerItem()) {
			return -1;
		}
		for (int column = 0 ; column < tree->columnCount() ; ++column)
		{
			if (tree->headerItem()->text(column) == title) {
				return column;
			}
		}
		return -1;
	}

	/// The component rows the window lists, folio by folio, in its own order.
	QStringList listedComponents(QTreeWidget *tree, int name_column)
	{
		QStringList names;
		if (!tree) {
			return names;
		}

		for (int f = 0 ; f < tree->topLevelItemCount() ; ++f)
		{
			QTreeWidgetItem *folio = tree->topLevelItem(f);
			for (int c = 0 ; c < folio->childCount() ; ++c)
			{
				QTreeWidgetItem *row = folio->child(c);
				if (!row->isHidden()) {
					names << row->text(name_column);
				}
			}
		}
		return names;
	}

	/// The number the window writes on each folio row, in its own order.
	QStringList folioCounts(QTreeWidget *tree, int count_column)
	{
		QStringList counts;
		if (!tree) {
			return counts;
		}
		for (int f = 0 ; f < tree->topLevelItemCount() ; ++f) {
			counts << tree->topLevelItem(f)->text(count_column);
		}
		return counts;
	}

	QStringList sorted(QStringList list)
	{
		list.sort();
		return list;
	}

	/**
		@a expected when @a window shows it in one of its labels, and every
		sentence the window shows otherwise.

		So that a failed comparison prints what the window said, instead of
		only saying that it said something else.
	*/
	QString said(QWidget &window, const QString &expected)
	{
		QStringList sentences;
		const QList<QLabel *> labels_of_window = window.findChildren<QLabel *>();
		for (QLabel *label : labels_of_window)
		{
			if (!label->text().isEmpty()) {
				sentences << label->text();
			}
		}
		return sentences.contains(expected)
			? expected
			: sentences.join(QLatin1String(" | "));
	}

	/// The push button of @a window reading @a text, nullptr when there is none.
	QPushButton *buttonReading(QWidget &window, const QString &text)
	{
		const QList<QPushButton *> buttons = window.findChildren<QPushButton *>();
		for (QPushButton *button : buttons)
		{
			if (button->text() == text) {
				return button;
			}
		}
		return nullptr;
	}

	/**
		One click of the mouse on @a item, inside the part of its row the
		viewport shows. The middle of the row and not its left end, where the
		branch decoration of a folio row would fold the folio instead of
		picking it.
	*/
	void clickRow(QTreeWidget *tree, QTreeWidgetItem *item)
	{
		REQUIRE(tree != nullptr);
		REQUIRE(item != nullptr);

		tree->scrollToItem(item);
		const QRect row = tree->visualItemRect(item)
				  .intersected(tree->viewport()->rect());
		REQUIRE_FALSE(row.isEmpty());

		QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
				  row.center());
		REQUIRE(item->isSelected());
	}

	/**
		Pick the component rows of @a tree whose name is one of @a names,
		leaving every other row unpicked - what a Ctrl+click does to each of
		them, done through the item.

		@return how many rows were picked
	*/
	int pickRows(QTreeWidget *tree, int name_column, const QStringList &names)
	{
		int picked = 0;
		if (!tree) {
			return picked;
		}

		tree->clearSelection();
		for (int f = 0 ; f < tree->topLevelItemCount() ; ++f)
		{
			QTreeWidgetItem *folio = tree->topLevelItem(f);
			for (int c = 0 ; c < folio->childCount() ; ++c)
			{
				QTreeWidgetItem *row = folio->child(c);
				if (names.contains(row->text(name_column)))
				{
					row->setSelected(true);
					++picked;
				}
			}
		}
		return picked;
	}

	/**
		Answers the question "Affecter une localisation" the way a person
		does: the entry reading @a entry is picked in the list of the
		question, and OK is pressed. An empty @a entry presses Cancel.

		Installed before the click, because the question is a modal exec():
		under the offscreen platform an exec() nobody answers waits for ever,
		and a case that clicked without this would hang instead of failing.
		The question is looked for among the windows and not only as the
		active modal one, so that answering it does not depend on how the
		platform keeps its modal list. A modal window that is not the question
		is turned down rather than left waiting, and its title is kept, so
		that the failure names what came up instead.
	*/
	class LocationQuestion
	{
		public:
			explicit LocationQuestion(const QString &entry) :
				m_entry(entry)
			{
				m_timer.setInterval(20);
				QObject::connect(&m_timer, &QTimer::timeout, [this]()
				{
					look();
				});
				m_timer.start();
			}

			/// How many times the question came up while this lived.
			int asked() const {return m_asked;}
			/// Whether the entry asked for was there, and was picked.
			bool picked() const {return m_picked;}
			/// The sentence above the list, as the question wrote it.
			QString wording() const {return m_wording;}
			/// The entries of the list, in the order offered.
			QStringList offered() const {return m_offered;}
			/// The titles of the modal windows that were not the question.
			QStringList strangers() const {return m_strangers;}

		private:
			void look()
			{
				QInputDialog *question = nullptr;
				const QWidgetList windows = QApplication::topLevelWidgets();
				for (QWidget *window : windows)
				{
					auto *input = qobject_cast<QInputDialog *>(window);
					if (input && input->isVisible())
					{
						question = input;
						break;
					}
				}

				if (!question)
				{
					QWidget *modal = QApplication::activeModalWidget();
					if (modal && modal->isVisible())
					{
						m_strangers << modal->windowTitle();
						if (auto *dialog = qobject_cast<QDialog *>(modal)) {
							dialog->reject();
						} else {
							modal->close();
						}
					}
					return;
				}

				++m_asked;
				m_wording = question->labelText();
				m_offered = question->comboBoxItems();

				QDialogButtonBox *box = question->findChild<QDialogButtonBox *>();
				QPushButton *ok = box ? box->button(QDialogButtonBox::Ok) : nullptr;
				QPushButton *cancel = box ? box->button(QDialogButtonBox::Cancel)
							  : nullptr;
				QComboBox *list = question->findChild<QComboBox *>();
				const int row = (list && !m_entry.isEmpty())
						? list->findText(m_entry)
						: -1;

				if (row >= 0 && ok)
				{
					list->setCurrentIndex(row);
					m_picked = true;
					ok->click();
				}
				else if (cancel)
				{
					cancel->click();
				}
				else
				{
					question->reject();
				}
			}

			QString m_entry;
			QTimer m_timer;
			int m_asked = 0;
			bool m_picked = false;
			QString m_wording;
			QStringList m_offered;
			QStringList m_strangers;
	};

	/**
		The gesture of the case, as a person makes it: the row of the first
		folio clicked - eighty components -, "Affecter à…" clicked, and the
		question answered by @a question, installed by the caller before.
	*/
	void assignFirstFolio(QWidget &report, QTreeWidget *list, QPushButton *assign,
			      const LocationQuestion &question)
	{
		REQUIRE(list != nullptr);
		REQUIRE(assign != nullptr);

		clickRow(list, list->topLevelItem(0));
		REQUIRE(said(report, eighty_picked) == eighty_picked);
		REQUIRE(assign->isEnabled());

		QTest::mouseClick(assign, Qt::LeftButton);

		REQUIRE(question.strangers() == QStringList());
		REQUIRE(question.asked() == 1);
	}
}

TEST_CASE("CU-32.1 — atribuicao em lote: 80 de 120 numa acao so, a lista de nao localizados cai a 40 e um desfazer a devolve a 120",
	  "[uibench][location]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("location-batch.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());
	REQUIRE(scratch.diagramCount() == 2);
	REQUIRE(scratch.diagram(0)->elements().count() == 80);
	REQUIRE(scratch.diagram(1)->elements().count() == 40);

	// The two locations exist before the window opens, which is how a person
	// gets there: the manager first, the list of what is left afterwards.
	LocationTree locations;
	QString locations_error;
	const QString aux_uuid = locations.append(ProjectLocation(aux_code, aux_name),
						  &locations_error);
	const QString main_uuid = locations.append(ProjectLocation(main_code, main_name),
						   &locations_error);
	INFO(locations_error.toStdString());
	REQUIRE_FALSE(aux_uuid.isEmpty());
	REQUIRE_FALSE(main_uuid.isEmpty());
	scratch->setLocationTree(locations);

	QUndoStack *stack = scratch->undoStack();
	REQUIRE(stack != nullptr);
	REQUIRE(stack->count() == 0);

	LocationReportDialog report(scratch.project());
	report.show();
	REQUIRE(QTest::qWaitForWindowExposed(&report));

	QTreeWidget *list = report.findChild<QTreeWidget *>();
	REQUIRE(list != nullptr);
	const int name_column = columnTitled(list,
					     LocationReportDialog::tr("Folio / composant"));
	const int count_column = columnTitled(list, LocationReportDialog::tr("Position"));
	REQUIRE(name_column >= 0);
	REQUIRE(count_column >= 0);

	QPushButton *assign = buttonReading(report, LocationReportDialog::tr("Affecter à…"));
	REQUIRE(assign != nullptr);

	// The list as it stands before anything is assigned, kept to hold the
	// undo against - and required to be the 120, not merely to exist: an undo
	// compared with an empty list would pass on a window that lists nothing.
	const QStringList listed_before = listedComponents(list, name_column);
	REQUIRE(listed_before.count() == 120);
	REQUIRE(sorted(listed_before) == labels(1, 120));
	REQUIRE(folioCounts(list, count_column)
		== QStringList({QStringLiteral("80"), QStringLiteral("40")}));
	REQUIRE(said(report, hundred_twenty_left) == hundred_twenty_left);
	REQUIRE_FALSE(assign->isEnabled());

	SECTION("a linha da folha escolhe 80, e a pergunta diz 80 e oferece as duas localizacoes")
	{
		LocationQuestion question(main_entry);
		assignFirstFolio(report, list, assign, question);

		REQUIRE(question.wording() == eighty_asked);
		REQUIRE(question.offered() == QStringList({aux_entry, main_entry}));
		REQUIRE(question.picked());
	}

	SECTION("os 80 entram numa acao so: a pilha de desfazer ganha uma entrada, e ela diz 80")
	{
		LocationQuestion question(main_entry);
		assignFirstFolio(report, list, assign, question);

		REQUIRE(stack->count() == 1);
		REQUIRE(stack->index() == 1);
		REQUIRE(UiBench::undoTopText(scratch.project()) == eighty_to_main);
	}

	SECTION("a lista de nao localizados cai de 120 para 40, e a folha 1 sai dela")
	{
		LocationQuestion question(main_entry);
		assignFirstFolio(report, list, assign, question);

		// The forty of the second folio, and the first folio gone from the
		// list altogether: a folio with nothing left on it is not listed.
		const QStringList listed_after = listedComponents(list, name_column);
		REQUIRE(listed_after.count() == 40);
		REQUIRE(sorted(listed_after) == labels(81, 120));
		REQUIRE(folioCounts(list, count_column)
			== QStringList({QStringLiteral("40")}));
		REQUIRE(said(report, forty_left) == forty_left);
		REQUIRE(said(report, eighty_done) == eighty_done);
	}

	SECTION("os 80 escolhidos levam o Quadro Principal, e os 40 de fora nao levam localizacao nenhuma")
	{
		LocationQuestion question(main_entry);
		assignFirstFolio(report, list, assign, question);

		// Which eighty, and where - the two sides of the selection. The
		// eighty of the first folio carry the main panel, the auxiliary one
		// received nothing, and the forty left out carry no location at all.
		REQUIRE(componentsAt(scratch.project(), main_code) == labels(1, 80));
		REQUIRE(componentsAt(scratch.project(), aux_code).isEmpty());
		REQUIRE(componentsAt(scratch.project(), QString()) == labels(81, 120));
	}

	SECTION("um desfazer devolve a lista a 120, linha por linha, e um refazer a leva de novo a 40")
	{
		LocationQuestion question(main_entry);
		assignFirstFolio(report, list, assign, question);
		REQUIRE(listedComponents(list, name_column).count() == 40);

		// Ctrl+Z: the undo action of the editor, which is this call.
		stack->undo();
		REQUIRE(stack->index() == 0);
		REQUIRE(listedComponents(list, name_column) == listed_before);
		REQUIRE(folioCounts(list, count_column)
			== QStringList({QStringLiteral("80"), QStringLiteral("40")}));
		REQUIRE(said(report, hundred_twenty_left) == hundred_twenty_left);
		REQUIRE(componentsAt(scratch.project(), main_code).isEmpty());
		REQUIRE(componentsAt(scratch.project(), QString()) == labels(1, 120));

		// And the step comes back whole, as it went.
		stack->redo();
		REQUIRE(stack->index() == 1);
		REQUIRE(sorted(listedComponents(list, name_column)) == labels(81, 120));
		REQUIRE(componentsAt(scratch.project(), main_code) == labels(1, 80));
	}

	SECTION("oitenta linhas escolhidas uma a uma, em duas folhas, tambem vao numa acao so")
	{
		// The last forty of the first folio and the forty of the second: a
		// selection no folio row can make, and one that crosses a folio.
		const QStringList chosen = labels(41, 120);
		REQUIRE(chosen.count() == 80);
		REQUIRE(pickRows(list, name_column, chosen) == 80);
		REQUIRE(said(report, eighty_picked) == eighty_picked);

		LocationQuestion question(main_entry);
		QTest::mouseClick(assign, Qt::LeftButton);

		REQUIRE(question.strangers() == QStringList());
		REQUIRE(question.asked() == 1);
		REQUIRE(question.wording() == eighty_asked);
		REQUIRE(question.picked());

		REQUIRE(stack->count() == 1);
		REQUIRE(UiBench::undoTopText(scratch.project()) == eighty_to_main);

		// What is left is the first forty, all of them on the first folio;
		// the second folio leaves the list.
		REQUIRE(sorted(listedComponents(list, name_column)) == labels(1, 40));
		REQUIRE(folioCounts(list, count_column)
			== QStringList({QStringLiteral("40")}));
		REQUIRE(said(report, forty_left) == forty_left);
		REQUIRE(componentsAt(scratch.project(), main_code) == chosen);
		REQUIRE(componentsAt(scratch.project(), QString()) == labels(1, 40));

		stack->undo();
		REQUIRE(stack->index() == 0);
		REQUIRE(listedComponents(list, name_column) == listed_before);
		REQUIRE(componentsAt(scratch.project(), QString()) == labels(1, 120));
	}

	SECTION("controle negativo — cancelar a pergunta nao empilha nada, e a lista fica em 120")
	{
		// The one entry on the stack above has to be able to be none, or it
		// proves nothing: the same gesture, answered with Cancel.
		// Braces, not parentheses: "question(QString())" declares a
		// function returning a LocationQuestion.
		LocationQuestion question{QString()};
		assignFirstFolio(report, list, assign, question);
		REQUIRE_FALSE(question.picked());

		REQUIRE(stack->count() == 0);
		REQUIRE(listedComponents(list, name_column) == listed_before);
		REQUIRE(said(report, hundred_twenty_left) == hundred_twenty_left);
		REQUIRE(componentsAt(scratch.project(), main_code).isEmpty());
		REQUIRE(componentsAt(scratch.project(), aux_code).isEmpty());
	}

	SECTION("controle negativo — sem mexer na lista da pergunta, os 80 vao para o Quadro Auxiliar")
	{
		// The other side of the choice. The question opens on the first
		// entry, and accepting it as it opens sends the eighty there: so the
		// main panel above is the answer given, and not what any answer
		// would have produced.
		LocationQuestion question(aux_entry);
		assignFirstFolio(report, list, assign, question);
		REQUIRE(question.picked());

		REQUIRE(stack->count() == 1);
		REQUIRE(UiBench::undoTopText(scratch.project()) == eighty_to_aux);
		REQUIRE(componentsAt(scratch.project(), aux_code) == labels(1, 80));
		REQUIRE(componentsAt(scratch.project(), main_code).isEmpty());
		REQUIRE(listedComponents(list, name_column).count() == 40);
	}

	SECTION("controle negativo — oitenta comandos chegam aos mesmos 40, mas um desfazer devolve um so")
	{
		/*
			The forty alone do not prove one action: eighty assignments of
			one component each reach exactly the same list. What tells them
			apart is the stack, and this is the measure of the stack being
			able to tell - eighty entries instead of one, and an undo that
			gives back one component instead of eighty.
		*/
		QList<Element *> first_folio;
		const QStringList first_labels = labels(1, 80);
		for (const QString &label : first_labels)
		{
			if (Element *component = componentLabelled(scratch.project(), label)) {
				first_folio << component;
			}
		}
		REQUIRE(first_folio.count() == 80);

		for (Element *component : first_folio)
		{
			stack->push(new AssignLocationCommand(QList<Element *>({component}),
							      main_code));
		}

		REQUIRE(stack->count() == 80);
		REQUIRE(sorted(listedComponents(list, name_column)) == labels(81, 120));
		REQUIRE(said(report, forty_left) == forty_left);

		stack->undo();
		REQUIRE(stack->index() == 79);
		const QStringList after_one_undo = listedComponents(list, name_column);
		REQUIRE(after_one_undo.count() == 41);
		REQUIRE(after_one_undo.contains(labelOf(80)));
		REQUIRE(componentsAt(scratch.project(), main_code) == labels(1, 79));
	}
}
