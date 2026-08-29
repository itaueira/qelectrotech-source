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
#include "macroparametersdialog.h"

#include "../../catalog/catalogpart.h"
#include "../../catalog/ui/catalogbrowserdialog.h"
#include "../../qetapp.h"
#include "../macroparameter.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

/**
	@brief MacroParametersDialog::MacroParametersDialog
	@param parameters : what the macro declares
	@param values : where the fields start, usually the macro defaults with
	the marks that are already on the sheet moved on
	@param parent
*/
MacroParametersDialog::MacroParametersDialog(const MacroParameterSet &parameters,
					     const QHash<QString, QString> &values,
					     QWidget *parent) :
	QDialog(parent),
	m_parameters(parameters),
	m_values(values)
{
	setWindowTitle(tr("Insérer un macro"));
	setMinimumWidth(420);
	buildWidgets();
	fillFromValues(values);
}

/**
	@brief MacroParametersDialog::values
	@return the values as they were when the dialogue was confirmed
*/
QHash<QString, QString> MacroParametersDialog::values() const
{
	return m_values;
}

/**
	@brief MacroParametersDialog::askValues
	@param parameters
	@param values
	@param parent
	@param accepted
	@return the values chosen, @a values untouched on cancel

	A macro that declares nothing shows no dialogue at all and confirms on
	the spot: every macro written before this task inserts exactly the
	drawing it inserted yesterday, without one extra click.
*/
QHash<QString, QString> MacroParametersDialog::askValues(const MacroParameterSet &parameters,
							 const QHash<QString, QString> &values,
							 QWidget *parent,
							 bool *accepted)
{
	if (parameters.isEmpty())
	{
		if (accepted) {
			*accepted = true;
		}
		return values;
	}

	MacroParametersDialog dialog(parameters, values, parent);
	const bool confirmed = (dialog.exec() == QDialog::Accepted);
	if (accepted) {
		*accepted = confirmed;
	}
	return confirmed ? dialog.values() : values;
}

/**
	@brief MacroParametersDialog::looksLikeNumber
	@param value
	@return whether @a value spells a number, with either separator

	This fork writes 7,5 and the engine writes 7.5. Which of the two the
	person typed is not a mistake, and refusing one of them would only
	teach them that the field is broken.
*/
bool MacroParametersDialog::looksLikeNumber(const QString &value)
{
	QString normalised = value;
	normalised.replace(QLatin1Char(','), QLatin1Char('.'));

	bool ok = false;
	normalised.toDouble(&ok);
	return ok;
}

/**
	@brief MacroParametersDialog::buildWidgets
	One editor per declared parameter, chosen by its type.
*/
void MacroParametersDialog::buildWidgets()
{
	QVBoxLayout *main_layout = new QVBoxLayout(this);

	QLabel *explanation = new QLabel(tr("Ces valeurs seront écrites dans le circuit inséré."), this);
	explanation->setWordWrap(true);
	main_layout->addWidget(explanation);

	const QStringList value_set_names = m_parameters.valueSetNames();
	if (!value_set_names.isEmpty())
	{
			//A value set is a whole set of answers the author of the macro
			//prepared. It fills the fields and then gets out of the way:
			//what it wrote stays editable, so a set that is right about
			//nine fields out of ten is still worth choosing.
		m_value_set = new QComboBox(this);
		m_value_set->addItem(tr("(valeurs du macro)"), QString());
		for (const QString &name : value_set_names) {
			m_value_set->addItem(name, name);
		}

		QFormLayout *set_layout = new QFormLayout();
		set_layout->addRow(tr("Jeu de valeurs :"), m_value_set);
		main_layout->addLayout(set_layout);

		connect(m_value_set, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &MacroParametersDialog::valueSetChosen);
	}

	QWidget *form_holder = new QWidget(this);
	QFormLayout *form = new QFormLayout(form_holder);
	form->setContentsMargins(0, 0, 0, 0);

	bool any_required = false;
	const QList<MacroParameter> declared = m_parameters.parameters();
	for (const MacroParameter &parameter : declared)
	{
		QWidget *editor = nullptr;
		QWidget *row = nullptr;

		switch (parameter.type)
		{
			case MacroParameterType::List:
			{
				QComboBox *combo = new QComboBox(form_holder);
					//A list holds what it declares and nothing else. The
					//empty entry is there only when the parameter may be
					//left out, so a mandatory list has no way to answer
					//nothing.
				if (!parameter.required) {
					combo->addItem(QString(), QString());
				}
				for (const QString &choice : parameter.choices) {
					combo->addItem(choice, choice);
				}
				editor = combo;
				break;
			}
			case MacroParameterType::Number:
			{
				QLineEdit *line = new QLineEdit(form_holder);
				line->setValidator(new QRegularExpressionValidator(
					QRegularExpression(QStringLiteral("^[+-]?[0-9]{0,12}([.,][0-9]{0,6})?$")),
					line));
				editor = line;
				break;
			}
			case MacroParameterType::Part:
			{
				QLineEdit *line = new QLineEdit(form_holder);

					//The code stays typable: someone who knows it should
					//not have to walk the catalogue to write four
					//characters.
				QPushButton *browse = new QPushButton(QString::fromUtf8("…"), form_holder);
				browse->setToolTip(tr("Choisir un article dans le catalogue"));
				browse->setEnabled(QETApp::catalog() != nullptr);
				browse->setMaximumWidth(32);
				connect(browse, &QPushButton::clicked, this, [this, line]() {
					browseForPart(line);
				});

				row = new QWidget(form_holder);
				QHBoxLayout *row_layout = new QHBoxLayout(row);
				row_layout->setContentsMargins(0, 0, 0, 0);
				row_layout->addWidget(line);
				row_layout->addWidget(browse);
				editor = line;
				break;
			}
			case MacroParameterType::Text:
			default:
			{
				editor = new QLineEdit(form_holder);
				break;
			}
		}

		if (!parameter.description.isEmpty()) {
			editor->setToolTip(parameter.description);
		}

		QWidget *field = row ? row : editor;
		if (!parameter.unit.isEmpty())
		{
			QWidget *with_unit = new QWidget(form_holder);
			QHBoxLayout *unit_layout = new QHBoxLayout(with_unit);
			unit_layout->setContentsMargins(0, 0, 0, 0);
			unit_layout->addWidget(field);
			unit_layout->addWidget(new QLabel(parameter.unit, with_unit));
			field = with_unit;
		}

		QString label = parameter.label.isEmpty() ? parameter.name : parameter.label;
		if (parameter.required)
		{
			any_required = true;
			label += QStringLiteral(" *");
		}
		label += QStringLiteral(" :");

		form->addRow(label, field);
		m_editors.insert(parameter.name, editor);
	}

		//Scrolled whatever the count: a macro declaring twenty variables
		//must not open a window taller than the screen, and a macro
		//declaring two must not look any different for it.
	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(form_holder);

	const int wanted = form_holder->sizeHint().height();
	scroll->setMinimumHeight(wanted < 360 ? wanted + 4 : 360);
	main_layout->addWidget(scroll);

	if (any_required) {
		main_layout->addWidget(new QLabel(tr("* obligatoire"), this));
	}

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
							 this);
	main_layout->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::accepted,
		this, &MacroParametersDialog::validateAndAccept);
	connect(buttons, &QDialogButtonBox::rejected,
		this, &MacroParametersDialog::reject);
}

/**
	@brief MacroParametersDialog::fillFromValues
	@param values
*/
void MacroParametersDialog::fillFromValues(const QHash<QString, QString> &values)
{
	const QList<MacroParameter> declared = m_parameters.parameters();
	for (const MacroParameter &parameter : declared)
	{
		QWidget *editor = m_editors.value(parameter.name);
		const QString value = values.value(parameter.name);

		if (QLineEdit *line = qobject_cast<QLineEdit *>(editor))
		{
			line->setText(value);
			continue;
		}

		if (QComboBox *combo = qobject_cast<QComboBox *>(editor))
		{
			const int index = combo->findData(value);
			if (index != -1)
			{
				combo->setCurrentIndex(index);
				continue;
			}

				//A value the list does not declare is still what the file
				//says. Dropping it here would change the circuit on the
				//way through a dialogue that was only meant to show it.
			if (!value.isEmpty())
			{
				combo->addItem(value, value);
				combo->setCurrentIndex(combo->count() - 1);
			}
		}
	}
}

/**
	@brief MacroParametersDialog::readWidgets
	@return the values as the fields hold them right now

	Starts from the values handed in, so a value carried for a marker no
	parameter declares crosses the dialogue instead of being dropped by it.
*/
QHash<QString, QString> MacroParametersDialog::readWidgets() const
{
	QHash<QString, QString> values = m_values;
	for (auto it = m_editors.constBegin() ; it != m_editors.constEnd() ; ++it)
	{
		if (const QLineEdit *line = qobject_cast<const QLineEdit *>(it.value())) {
			values.insert(it.key(), line->text());
		}
		else if (const QComboBox *combo = qobject_cast<const QComboBox *>(it.value())) {
			values.insert(it.key(), combo->currentData().toString());
		}
	}
	return values;
}

/**
	@brief MacroParametersDialog::valueSetChosen
	@param index
*/
void MacroParametersDialog::valueSetChosen(int index)
{
	if (!m_value_set || index <= 0) {
		return;
	}

	const QString name = m_value_set->itemData(index).toString();
	if (name.isEmpty()) {
		return;
	}

		//Laid over what is on screen, and not over the macro defaults: a
		//field the set says nothing about keeps the value the user just
		//typed into it.
	fillFromValues(m_parameters.applyValueSet(name, readWidgets()));
}

/**
	@brief MacroParametersDialog::browseForPart
	@param editor : the line the chosen code is written into
*/
void MacroParametersDialog::browseForPart(QLineEdit *editor)
{
	if (!editor || !QETApp::catalog()) {
		return;
	}

	const CatalogPart part = CatalogBrowserDialog::choosePart(QETApp::catalog(), this);
	if (part.isNull()) {
		return;
	}

	editor->setText(part.code);
}

/**
	@brief MacroParametersDialog::validateAndAccept
	What is refused here never reaches the sheet. Both refusals exist for
	the same reason: half a circuit looks finished, and a wrong value drawn
	on the folio is found weeks later by someone who did not draw it.
*/
void MacroParametersDialog::validateAndAccept()
{
	const QHash<QString, QString> values = readWidgets();

	const QStringList missing = m_parameters.missingRequired(values);
	if (!missing.isEmpty())
	{
		QStringList shown;
		shown.reserve(missing.size());
		for (const QString &name : missing)
		{
			const MacroParameter parameter = m_parameters.parameter(name);
			shown << (parameter.label.isEmpty() ? name : parameter.label);
		}

		QMessageBox::warning(this, tr("Valeur manquante"),
				     shown.size() == 1
				     ? tr("La variable %1 est obligatoire et n'a pas de valeur.")
					     .arg(shown.first())
				     : tr("Ces variables sont obligatoires et n'ont pas de valeur : %1.")
					     .arg(shown.join(QStringLiteral(", "))));
		return;
	}

	const QList<MacroParameter> declared = m_parameters.parameters();
	for (const MacroParameter &parameter : declared)
	{
		if (parameter.type != MacroParameterType::Number) {
			continue;
		}

		const QString value = values.value(parameter.name);
		if (value.isEmpty() || MacroParametersDialog::looksLikeNumber(value)) {
			continue;
		}

		QMessageBox::warning(this, tr("Valeur incorrecte"),
				     tr("La variable %1 attend un nombre.")
					     .arg(parameter.label.isEmpty() ? parameter.name
									    : parameter.label));
		return;
	}

	m_values = values;
	accept();
}
