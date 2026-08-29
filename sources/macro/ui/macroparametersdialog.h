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
#ifndef MACROPARAMETERSDIALOG_H
#define MACROPARAMETERSDIALOG_H

#include "../macroparameterset.h"

#include <QDialog>
#include <QHash>
#include <QString>

class QComboBox;
class QLineEdit;

/**
	@brief The MacroParametersDialog class
	Asks, once and before anything is drawn, for the values the macro about
	to be inserted declares.

	One editor per declared parameter, chosen by its type: a line for a
	Text, a line that only takes a number for a Number, the declared list
	for a List, and a line beside a browse button for a Part. The type is
	what the macro author wrote, so the person inserting the circuit is
	offered exactly the freedom the author meant to give.

	Two things are refused here rather than after the fact. A required
	parameter left blank does not reach the sheet, because half a circuit
	looks finished; and a Number that is not a number does not either. The
	decimal separator is free: this fork writes 7,5 and the engine writes
	7.5, and neither is worth an error message.

	The dialogue never sees a Diagram, a QETProject or an ElementsLocation.
	It is handed a MacroParameterSet and a set of values, and it hands
	values back - which is what lets the caller decide, on its own, that a
	cancelled dialogue means nothing at all was touched.
*/
class MacroParametersDialog : public QDialog
{
	Q_OBJECT

	public:
		MacroParametersDialog(const MacroParameterSet &parameters,
				      const QHash<QString, QString> &values,
				      QWidget *parent = nullptr);

		QHash<QString, QString> values() const;

		/**
			@brief Ask for the values of @a parameters, starting from @a values.
			@param accepted : when not nullptr, receives whether the user
			confirmed. A macro declaring nothing confirms without a dialogue.
			@return the values chosen, @a values untouched on cancel
		*/
		static QHash<QString, QString> askValues(const MacroParameterSet &parameters,
							 const QHash<QString, QString> &values,
							 QWidget *parent = nullptr,
							 bool *accepted = nullptr);

		static bool looksLikeNumber(const QString &value);

	private slots:
		void valueSetChosen(int index);
		void validateAndAccept();

	private:
		void buildWidgets();
		void fillFromValues(const QHash<QString, QString> &values);
		QHash<QString, QString> readWidgets() const;
		void browseForPart(QLineEdit *editor);

	private:
		MacroParameterSet m_parameters;
		QHash<QString, QString> m_values;
		QComboBox *m_value_set = nullptr;
			/// parameter name to the widget that holds its value
		QHash<QString, QWidget *> m_editors;
};

#endif // MACROPARAMETERSDIALOG_H
