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

#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/ElementsCollection/xmlelementcollection.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontent.h"
#include "../../../../sources/macro/macrofile.h"
#include "../../../../sources/diagramevent/diagrameventaddmacro.h"
#include "../../../../sources/macro/macroparameter.h"
#include "../../../../sources/macro/macroparameterset.h"
#include "../../../../sources/macro/ui/macroparametersdialog.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <functional>

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QSignalSpy>
#include <QTimer>
#include <QUndoStack>
#include <QValidator>

/*
	Inserting a parameterised macro: the window that asks for the values, and
	the drop that writes the circuit on the folio.

	What a macro declares, what a substitution reaches and which marker is left
	orphaned are proved without a project open, in macro_test.cpp - six cases
	under the CU-05 labels of the task that wrote the file format. The
	arithmetic of the next free mark is proved there too. None of it is
	repeated here.

	What is proved here is the window and the drop: that a variable is offered
	the control of its own type, that the numeric one refuses a letter and
	takes a comma, that a list offers what it declares and nothing else, that
	a value set fills the fields without locking them, and that a macro
	declaring nothing goes in with no window at all - which is the
	non-regression case that matters most in the task, because every macro
	written before the variables existed has to keep inserting the drawing it
	inserted yesterday.

	Nothing here is drawn on a screen. The dialog is built, typed into and
	clicked inside this process under the offscreen platform, and the drop is
	the very QGraphicsSceneMouseEvent the scene would hand to the event
	interface of the sheet.
*/

namespace
{
	MacroParameter textParameter(const QString &name, const QString &label,
				     const QString &default_value = QString(),
				     bool required = false)
	{
		MacroParameter parameter(name, label, MacroParameterType::Text);
		parameter.default_value = default_value;
		parameter.required = required;
		return parameter;
	}

	/**
		A macro that declares one variable of each type, which is what the
		queued case asks to see: a text, a number, a list and a part.
	*/
	MacroParameterSet oneOfEachType()
	{
		MacroParameterSet set;
		set.append(textParameter(QStringLiteral("TAG"),
					 QStringLiteral("Repère du moteur"),
					 QStringLiteral("-M1")));

		MacroParameter power(QStringLiteral("POTENCIA"),
				     QStringLiteral("Puissance"),
				     MacroParameterType::Number);
		power.unit = QStringLiteral("cv");
		set.append(power);

		MacroParameter interlock(QStringLiteral("TRAVAMENTO"),
					 QStringLiteral("Verrouillage"),
					 MacroParameterType::List);
		interlock.choices = QStringList{QStringLiteral("Électrique"),
						QStringLiteral("Électrique et mécanique")};
		interlock.default_value = QStringLiteral("Électrique");
		set.append(interlock);

		MacroParameter part(QStringLiteral("CODIGO"),
				    QStringLiteral("Article"),
				    MacroParameterType::Part);
		set.append(part);
		return set;
	}

	/// The editor the window offers for @a label, whatever it wrapped it in.
	QWidget *editorFor(QDialog &dialog, const QString &label)
	{
		const QList<QFormLayout *> forms = dialog.findChildren<QFormLayout *>();
		for (QFormLayout *form : forms)
		{
			for (int row = 0 ; row < form->rowCount() ; ++ row)
			{
				QLayoutItem *label_item = form->itemAt(row, QFormLayout::LabelRole);
				QLabel *text = label_item
					       ? qobject_cast<QLabel *>(label_item->widget())
					       : nullptr;
				if (!text || !text->text().startsWith(label)) {
					continue;
				}

				QLayoutItem *field = form->itemAt(row, QFormLayout::FieldRole);
				QWidget *widget = field ? field->widget() : nullptr;
				if (!widget) {
					continue;
				}

					//A parameter with a unit, or one that browses the
					//catalogue, sits inside a row of its own; the editor
					//is the first thing in it that holds a value.
				if (qobject_cast<QLineEdit *>(widget)
				    || qobject_cast<QComboBox *>(widget)) {
					return widget;
				}
				if (QLineEdit *line = widget->findChild<QLineEdit *>()) {
					return line;
				}
				if (QComboBox *combo = widget->findChild<QComboBox *>()) {
					return combo;
				}
			}
		}
		return nullptr;
	}

	/// The label the window writes beside the editor of @a name, empty when none.
	QString labelTextFor(QDialog &dialog, const QString &beginning)
	{
		const QList<QLabel *> labels = dialog.findChildren<QLabel *>();
		for (QLabel *label : labels)
		{
			if (label->text().startsWith(beginning)) {
				return label->text();
			}
		}
		return QString();
	}

	QPushButton *okButton(QDialog &dialog)
	{
		QDialogButtonBox *box = dialog.findChild<QDialogButtonBox *>();
		return box ? box->button(QDialogButtonBox::Ok) : nullptr;
	}

	/// One symbol, the shape a macro carries its symbols in.
	QString symbolDefinition()
	{
		return QStringLiteral(
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\""
			       " orientation=\"dnnn\" link_type=\"simple\">"
			       "<names><name lang=\"en\">Contactor</name></names>"
			       "<description>"
			       "<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
			       " end1=\"none\" end2=\"none\" length1=\"1.5\" length2=\"1.5\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"
			       "<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"
			       "</description>"
			       "</definition>");
	}

	/// One drawn symbol of a macro, at @a x, marked @a label.
	QString macroInstance(int x, int uuid_tail, const QString &label)
	{
		return QStringLiteral(
			       "<element x=\"%1\" y=\"20\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"macro://import/bench/contactor.elmt\""
			       " uuid=\"{beef0000-0000-4000-8000-00000000000%2}\">"
			       "<terminals/><inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">%3"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
		       .arg(QString::number(x), QString::number(uuid_tail), label);
	}

	/**
		A whole .qetmak, as the program writes one.
		@param parameters the <parameters> block, empty for a macro that
		declares nothing - which is every macro made before this task
		@param first, second the marks of the two symbols it draws

		Two symbols and not one, because the case this fixture serves is "one
		Ctrl+Z takes the whole circuit back, not one component at a time" -
		and a circuit of one component cannot tell those two apart.
	*/
	QString macroXml(const QString &parameters, const QString &first,
			 const QString &second)
	{
		return QStringLiteral(
			       "<qet_macro>%1"
			       "<collection>"
			       "<element path=\"import/bench/contactor.elmt\">%2</element>"
			       "</collection>"
			       "<diagram_content>"
			       "<diagram>"
			       "<elements>%3%4</elements>"
			       "<conductors/><inputs/>"
			       "</diagram>"
			       "</diagram_content>"
			       "</qet_macro>")
		       .arg(parameters, symbolDefinition(),
			    macroInstance(40, 1, first), macroInstance(90, 2, second));
	}

	/**
		A macro that does declare a variable: one mark, defaulting to -Q3.

		Its symbol is deliberately not the one of macroXml() above. The
		cancelled insertion has to be able to say that the project did not
		keep the collection of the macro it refused, and it can only say that
		about a symbol no other macro of the case brought in.
	*/
	QString askedMacroXml()
	{
		return QStringLiteral(
			       "<qet_macro>"
			       "<parameters>"
			       "<parameter name=\"TAG\" label=\"Repère de l'appareil\""
			       " type=\"text\" default=\"-Q3\"/>"
			       "</parameters>"
			       "<collection>"
			       "<element path=\"import/bench/relay.elmt\">%1</element>"
			       "</collection>"
			       "<diagram_content>"
			       "<diagram>"
			       "<elements>"
			       "<element x=\"40\" y=\"20\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"macro://import/bench/relay.elmt\""
			       " uuid=\"{beef0000-0000-4000-8000-000000000002}\">"
			       "<terminals/><inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">${TAG}"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>"
			       "</elements>"
			       "<conductors/><inputs/>"
			       "</diagram>"
			       "</diagram_content>"
			       "</qet_macro>")
		       .arg(symbolDefinition());
	}

	/// An empty sheet in a project of its own, for a macro to be dropped on.
	QString emptyProjectXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	/// A macro file written into @a dir, and the path it can be opened from.
	QString writeMacro(const QTemporaryDir &dir, const QString &file_name,
			   const QString &content)
	{
		const QString path = dir.filePath(file_name);
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			return QString();
		}
			//Written as UTF-8 bytes and not through a QTextStream: the
			//stream defaults to the encoding of the machine in Qt5 and to
			//UTF-8 in Qt6, so a macro whose label carries an accent would
			//be written one way, read back another, and the label the
			//window shows would stop matching what this file asked for.
		file.write(content.toUtf8());
		file.close();
		return path;
	}

	/**
		The marks the sheet carries, sorted.

		Sorted because Diagram::elements() answers in the order of the scene
		and not in the order of the file, and which of the two symbols of a
		macro comes back first is not what any of these cases is about.
	*/
	QStringList marksOf(Diagram *sheet)
	{
		QStringList marks;
		const QList<Element *> elements = sheet->elements();
		for (Element *element : elements)
		{
			marks << element->elementInformations()
				 .value(QStringLiteral("label")).toString();
		}
		marks.sort();
		return marks;
	}

	/**
		Answers the one window a test may not sit and wait for.

		A modal exec() under the offscreen platform does not fail: it waits,
		for a click nobody is there to give, and the suite stops rather than
		reports. So the answer is armed before the call that raises the
		window, and it runs from inside that very event loop.

		It has a deadline for the same reason. A window that never comes must
		not leave a timer running for the rest of the suite, and a window this
		cannot recognise must not leave the process stuck: after the deadline
		every visible dialog is closed, whatever it is.
	*/
	class ModalAnswer : public QObject
	{
		public:
			explicit ModalAnswer(const std::function<void (QDialog *)> &answer,
					     int deadline_ms = 8000) :
				m_answer(answer),
				m_left(deadline_ms / 5)
			{
				m_timer.setInterval(5);
				QObject::connect(&m_timer, &QTimer::timeout,
						 this, &ModalAnswer::look);
				m_timer.start();
			}

			/// Whether a window came and was answered.
			bool answered() const {return m_answered;}
			/// What a message box said, empty when the window was not one.
			QString heard() const {return m_heard;}

		private:
			void look()
			{
				QDialog *modal = qobject_cast<QDialog *>(
						 QApplication::activeModalWidget());
				if (!modal)
				{
					if (-- m_left > 0) {
						return;
					}

						//Out of time: nothing was recognised, so nothing is
						//answered - but nothing is left waiting either.
					m_timer.stop();
					const QWidgetList windows = QApplication::topLevelWidgets();
					for (QWidget *window : windows)
					{
						if (QDialog *dialog = qobject_cast<QDialog *>(window))
						{
							if (dialog->isVisible()) {
								dialog->reject();
							}
						}
					}
					return;
				}

				m_timer.stop();
				m_answered = true;
				if (QMessageBox *box = qobject_cast<QMessageBox *>(modal)) {
					m_heard = box->text();
				}
				m_answer(modal);
			}

			std::function<void (QDialog *)> m_answer;
			QTimer m_timer;
			int m_left = 0;
			bool m_answered = false;
			QString m_heard;
	};

	/**
		Drop the macro on the sheet, the way the scene does it.

		The parameter is not called "interface", and that is not a matter of
		taste: on Windows the platform headers define interface as a macro
		for struct, and a Qt5 build here reaches them. The word compiles in
		the Qt6 build of this same file and stops the Qt5 one, which is the
		worst kind of name to choose.
	*/
	void dropMacro(DiagramEventAddMacro &adder, const QPointF &position)
	{
		QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
		release.setButton(Qt::LeftButton);
		release.setScenePos(position);
		release.setPos(position);
		adder.mouseReleaseEvent(&release);
	}
}

TEST_CASE("T06 — cada variável recebe o controle do seu tipo, e o número recusa letra",
	  "[uibench][macro]")
{
	MacroParametersDialog dialog(oneOfEachType(),
				     oneOfEachType().defaults());

	QWidget *tag = editorFor(dialog, QString::fromUtf8("Repère du moteur"));
	QWidget *power = editorFor(dialog, QStringLiteral("Puissance"));
	QWidget *interlock = editorFor(dialog, QString::fromUtf8("Verrouillage"));
	QWidget *code = editorFor(dialog, QStringLiteral("Article"));
	REQUIRE(tag != nullptr);
	REQUIRE(power != nullptr);
	REQUIRE(interlock != nullptr);
	REQUIRE(code != nullptr);

	SECTION("o texto é uma linha livre, e o número é uma linha que só aceita número")
	{
		QLineEdit *tag_line = qobject_cast<QLineEdit *>(tag);
		QLineEdit *power_line = qobject_cast<QLineEdit *>(power);
		REQUIRE(tag_line != nullptr);
		REQUIRE(power_line != nullptr);

			//The two sides of the same boundary: a free line has nothing
			//deciding what may be typed into it, and the numeric one does.
		CHECK(tag_line->validator() == nullptr);
		CHECK(power_line->validator() != nullptr);

			//And it opened on what the macro declares.
		CHECK(tag_line->text() == QStringLiteral("-M1"));
	}

	SECTION("a lista oferece o que declara, e a unidade fica ao lado do campo")
	{
		QComboBox *combo = qobject_cast<QComboBox *>(interlock);
		REQUIRE(combo != nullptr);

			//Two declared choices, plus the empty entry that exists only
			//because this parameter may be left out.
		REQUIRE(combo->count() == 3);
		CHECK(combo->itemText(0).isEmpty());
		CHECK(combo->itemText(1) == QString::fromUtf8("Électrique"));
		CHECK(combo->itemText(2) == QString::fromUtf8("Électrique et mécanique"));
		CHECK(combo->currentText() == QString::fromUtf8("Électrique"));

			//The unit is written beside the field and not into it: what the
			//circuit receives is the number, not "7,5 cv".
		CHECK(labelTextFor(dialog, QStringLiteral("cv")) == QStringLiteral("cv"));
	}

	SECTION("a peça tem a linha e o botão que abre o catálogo ao lado")
	{
		QLineEdit *code_line = qobject_cast<QLineEdit *>(code);
		REQUIRE(code_line != nullptr);

			//The code stays typable: somebody who knows it is not made to
			//walk the catalogue for four characters.
		CHECK(code_line->isReadOnly() == false);

		QWidget *row = code_line->parentWidget();
		REQUIRE(row != nullptr);
		CHECK(row->findChild<QPushButton *>() != nullptr);
	}

	SECTION("a letra não entra no campo numérico, e 7,5 com vírgula entra")
	{
		QLineEdit *power_line = qobject_cast<QLineEdit *>(power);
		REQUIRE(power_line != nullptr);
		REQUIRE(power_line->text().isEmpty());

		QTest::keyClicks(power_line, QStringLiteral("abc"));
		CHECK(power_line->text().isEmpty());

			//The other side, and the one the task writes down: this fork
			//writes 7,5 and the engine writes 7.5, and refusing the comma
			//would only teach the designer that the field is broken.
		QTest::keyClicks(power_line, QStringLiteral("7,5"));
		CHECK(power_line->text() == QStringLiteral("7,5"));
		CHECK(MacroParametersDialog::looksLikeNumber(power_line->text()));

			//And the value comes back out of the window as it was typed.
		QPushButton *ok = okButton(dialog);
		REQUIRE(ok != nullptr);
		ok->click();
		CHECK(dialog.result() == QDialog::Accepted);
		CHECK(dialog.values().value(QStringLiteral("POTENCIA"))
		      == QStringLiteral("7,5"));
	}
}

TEST_CASE("T06 — o conjunto de valores preenche tudo, e os campos seguem editáveis",
	  "[uibench][macro]")
{
	MacroParameterSet set = oneOfEachType();

	MacroValueSet ready(QString::fromUtf8("Partida 7,5 cv"));
	ready.values.insert(QStringLiteral("TAG"), QStringLiteral("-M7"));
	ready.values.insert(QStringLiteral("POTENCIA"), QStringLiteral("7,5"));
	ready.values.insert(QStringLiteral("TRAVAMENTO"),
			    QString::fromUtf8("Électrique et mécanique"));
	REQUIRE(set.appendValueSet(ready));

	MacroParametersDialog dialog(set, set.defaults());

	QComboBox *chooser = dialog.findChild<QComboBox *>();
	REQUIRE(chooser != nullptr);
	REQUIRE(chooser->count() == 2);
	REQUIRE(chooser->itemText(1) == QString::fromUtf8("Partida 7,5 cv"));

	QLineEdit *tag = qobject_cast<QLineEdit *>(
			 editorFor(dialog, QString::fromUtf8("Repère du moteur")));
	QLineEdit *power = qobject_cast<QLineEdit *>(
			   editorFor(dialog, QStringLiteral("Puissance")));
	QComboBox *interlock = qobject_cast<QComboBox *>(
			       editorFor(dialog, QString::fromUtf8("Verrouillage")));
	REQUIRE(tag != nullptr);
	REQUIRE(power != nullptr);
	REQUIRE(interlock != nullptr);

	SECTION("escolher o conjunto enche os três campos de uma vez")
	{
		REQUIRE(tag->text() == QStringLiteral("-M1"));
		REQUIRE(power->text().isEmpty());

		chooser->setCurrentIndex(1);

		CHECK(tag->text() == QStringLiteral("-M7"));
		CHECK(power->text() == QStringLiteral("7,5"));
		CHECK(interlock->currentText()
		      == QString::fromUtf8("Électrique et mécanique"));
	}

	SECTION("e o que ele escreveu continua podendo ser corrigido")
	{
		chooser->setCurrentIndex(1);
		REQUIRE(tag->text() == QStringLiteral("-M7"));

			//A set that is right about nine fields out of ten is still worth
			//choosing, which is only true if the tenth can be typed over.
		CHECK_FALSE(tag->isReadOnly());
		tag->setText(QStringLiteral("-M8"));

		QPushButton *ok = okButton(dialog);
		REQUIRE(ok != nullptr);
		ok->click();

		REQUIRE(dialog.result() == QDialog::Accepted);
		CHECK(dialog.values().value(QStringLiteral("TAG"))
		      == QStringLiteral("-M8"));
		CHECK(dialog.values().value(QStringLiteral("POTENCIA"))
		      == QStringLiteral("7,5"));

			//And the chooser still names the set that was chosen: going back
			//to the empty entry would tell the person the set was dropped,
			//when nine of its ten answers are still in the fields.
		CHECK(chooser->currentIndex() == 1);
	}
}

TEST_CASE("T06 — o macro sem variável nenhuma entra sem diálogo, e um Ctrl+Z tira o circuito",
	  "[uibench][macro]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());

	const QString path = writeMacro(dir, QStringLiteral("plain.qetmak"),
					macroXml(QString(), QStringLiteral("-Q3"),
						  QStringLiteral("-K3")));
	REQUIRE_FALSE(path.isEmpty());

	UiBench::ScratchProject bench(emptyProjectXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	REQUIRE(sheet->elements().isEmpty());

	ElementsLocation location(path);
	REQUIRE(location.fileSystemPath() == path);

	SECTION("o arquivo do teste é um macro de verdade, e sem variável nenhuma")
	{
			//The fixture guarded, because everything below would also pass on
			//a file the program refused to read: a macro it cannot open asks
			//no questions either, and would look exactly like a macro that
			//declares nothing.
		MacroFile file;
		INFO(file.errorText().toStdString());
		REQUIRE(file.load(path));
		REQUIRE_FALSE(file.diagramNode().isNull());
		REQUIRE(file.parameters().isEmpty());

		file.importCollection(bench->embeddedElementCollection());
		CHECK(bench->embeddedElementCollection()
		      ->exist(QStringLiteral("import/bench/contactor.elmt")));
	}

	QScopedPointer<DiagramEventAddMacro> adder(
		new DiagramEventAddMacro(location, sheet, QPointF(200, 200)));

	SECTION("nenhuma janela se abre, e a inserção acontece no clique")
	{
			//The whole of the non-regression case: a macro declaring nothing
			//is not asked about. If it were, the constructor above would be
			//sitting in a modal exec() and this line would never run.
		CHECK(QApplication::activeModalWidget() == nullptr);

		dropMacro(*adder, QPointF(200, 200));

		REQUIRE(sheet->elements().count() == 2);
		CHECK(marksOf(sheet) == QStringList{QStringLiteral("-K3"),
						    QStringLiteral("-Q3")});
	}

	SECTION("um Ctrl+Z tira o circuito inteiro")
	{
		dropMacro(*adder, QPointF(200, 200));
		REQUIRE(sheet->elements().count() == 2);

		CHECK(UiBench::undoTopText(bench.project())
		      == QString::fromUtf8("insérer un macro"));
		REQUIRE(bench->undoStack()->count() == 1);

		bench->undoStack()->undo();
		CHECK(sheet->elements().isEmpty());

			//And back, because a circuit that could not be redone would be
			//half an entry of the stack.
		bench->undoStack()->redo();
		CHECK(sheet->elements().count() == 2);
	}
}

TEST_CASE("T06 — obrigatória em branco não passa, e o diálogo diz qual é",
	  "[uibench][macro]")
{
	MacroParameterSet set;
	set.append(textParameter(QStringLiteral("TAG"),
				 QString::fromUtf8("Repère du moteur"),
				 QStringLiteral("-M1")));

	MacroParameter power(QStringLiteral("POTENCIA"),
			     QStringLiteral("Puissance"),
			     MacroParameterType::Number);
	power.required = true;
	set.append(power);

	MacroParametersDialog dialog(set, set.defaults());
	QLineEdit *field = qobject_cast<QLineEdit *>(
			   editorFor(dialog, QStringLiteral("Puissance")));
	QPushButton *ok = okButton(dialog);
	REQUIRE(field != nullptr);
	REQUIRE(ok != nullptr);
	REQUIRE(field->text().isEmpty());

	SECTION("a obrigatória se anuncia como obrigatória antes de qualquer clique")
	{
		CHECK(labelTextFor(dialog, QStringLiteral("Puissance"))
		      == QStringLiteral("Puissance * :"));
		CHECK(labelTextFor(dialog, QStringLiteral("*"))
		      == QStringLiteral("* obligatoire"));
	}

	SECTION("em branco, o diálogo não fecha e nomeia a variável que falta")
	{
		QSignalSpy accepted(&dialog, &QDialog::accepted);
		REQUIRE(accepted.isValid());

		ModalAnswer answer([](QDialog *box) {box->accept();});
		ok->click();

			//Half a circuit looks finished, so the refusal happens here and
			//not after the drawing: the window stays open and says which one.
		REQUIRE(answer.answered());
		CHECK(answer.heard()
		      == QString::fromUtf8("La variable Puissance est obligatoire "
					   "et n'a pas de valeur."));
		CHECK(accepted.count() == 0);
	}

	SECTION("preenchida, o mesmo clique passa")
	{
			//The other side of the boundary. Without it the check above would
			//pass on a window that refuses whatever it is given.
		QSignalSpy accepted(&dialog, &QDialog::accepted);
		REQUIRE(accepted.isValid());

		field->setText(QStringLiteral("7,5"));
		ok->click();

		CHECK(accepted.count() == 1);
		CHECK(dialog.values().value(QStringLiteral("POTENCIA"))
		      == QStringLiteral("7,5"));
	}
}

TEST_CASE("T06 — cancelar não insere, não empilha desfazer e não deixa coleção",
	  "[uibench][macro]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());

	const QString plain = writeMacro(dir, QStringLiteral("plain.qetmak"),
					 macroXml(QString(), QStringLiteral("-Q3"),
						  QStringLiteral("-K3")));
	const QString asked = writeMacro(dir, QStringLiteral("asked.qetmak"),
					 askedMacroXml());
	REQUIRE_FALSE(plain.isEmpty());
	REQUIRE_FALSE(asked.isEmpty());

	UiBench::ScratchProject bench(emptyProjectXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

		//Something on the stack before the cancelled insertion, because the
		//case is not only "nothing was inserted": it is that a Ctrl+Z after
		//cancelling takes back the previous action and not the cancelling.
	{
		ElementsLocation location(plain);
		QScopedPointer<DiagramEventAddMacro> first(
			new DiagramEventAddMacro(location, sheet, QPointF(200, 200)));
		dropMacro(*first, QPointF(200, 200));
	}
	REQUIRE(sheet->elements().count() == 2);
	REQUIRE(bench->undoStack()->count() == 1);

	ElementsLocation location(asked);
	ModalAnswer answer([](QDialog *window) {window->reject();});

	{
		QScopedPointer<DiagramEventAddMacro> cancelled(
			new DiagramEventAddMacro(location, sheet, QPointF(300, 300)));
		REQUIRE(answer.answered());

			//A cancelled insertion has no ghost to drop, and the click that
			//would place it does nothing.
		dropMacro(*cancelled, QPointF(300, 300));
	}

	SECTION("nada é inserido e nada entra na pilha")
	{
		CHECK(sheet->elements().count() == 2);
		CHECK(marksOf(sheet) == QStringList{QStringLiteral("-K3"),
						    QStringLiteral("-Q3")});
		CHECK(bench->undoStack()->count() == 1);

			//And the entry that is there is the earlier one: a Ctrl+Z now
			//undoes the insertion of before, not the cancelling.
		CHECK(UiBench::undoTopText(bench.project())
		      == QString::fromUtf8("insérer un macro"));
		bench->undoStack()->undo();
		CHECK(sheet->elements().isEmpty());
	}

	SECTION("e o projeto não fica com a coleção do macro recusado")
	{
			//The values are asked for before the collection is imported, so a
			//window closed on Cancel leaves the project exactly as it was
			//found - the symbol of the refused macro included.
		CHECK_FALSE(bench->embeddedElementCollection()
			    ->exist(QStringLiteral("import/bench/relay.elmt")));
	}
}

TEST_CASE("T06 — a segunda inserção do mesmo macro abre já com o número seguinte",
	  "[uibench][macro]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());

	const QString path = writeMacro(dir, QStringLiteral("asked.qetmak"),
					askedMacroXml());
	REQUIRE_FALSE(path.isEmpty());

	UiBench::ScratchProject bench(emptyProjectXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);

	ElementsLocation location(path);

		//The first insertion, confirmed as the window opens it: the macro
		//declares -Q3 and nothing on the sheet has taken it yet.
	QString first_offered;
	{
		ModalAnswer answer([&first_offered](QDialog *window) {
			QLineEdit *tag = qobject_cast<QLineEdit *>(
					 editorFor(*window,
						   QString::fromUtf8("Repère")));
			if (tag) {
				first_offered = tag->text();
			}
			window->accept();
		});

		QScopedPointer<DiagramEventAddMacro> adder(
			new DiagramEventAddMacro(location, sheet, QPointF(200, 200)));
		REQUIRE(answer.answered());
		dropMacro(*adder, QPointF(200, 200));
	}

	REQUIRE(sheet->elements().count() == 1);
	REQUIRE(marksOf(sheet) == QStringList{QStringLiteral("-Q3")});
	CHECK(first_offered == QStringLiteral("-Q3"));

	QString second_offered;
	{
		ModalAnswer answer([&second_offered](QDialog *window) {
			QLineEdit *tag = qobject_cast<QLineEdit *>(
					 editorFor(*window,
						   QString::fromUtf8("Repère")));
			if (tag) {
				second_offered = tag->text();
			}
			window->reject();
		});

		QScopedPointer<DiagramEventAddMacro> adder(
			new DiagramEventAddMacro(location, sheet, QPointF(400, 200)));
		REQUIRE(answer.answered());
	}

	SECTION("a segunda janela abre no -Q4, e a colisão nem chega a ser oferecida")
	{
			//The mark the project already carries is proposed moved on, so
			//the collision is never put in front of the person: the window
			//opens on a mark that is already free.
		CHECK(second_offered == QStringLiteral("-Q4"));
	}
}
