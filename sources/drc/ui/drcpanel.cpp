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
#include "drcpanel.h"

#include "../../diagram.h"
#include "../../qetgraphicsitem/conductor.h"
#include "../../qetgraphicsitem/element.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

/**
	@brief DrcPanel::DrcPanel
	@param parent
*/
DrcPanel::DrcPanel(QWidget *parent) :
	QWidget(parent)
{
	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);

	m_table = new QTableWidget(this);
	m_table->setColumnCount(4);
	m_table->setHorizontalHeaderLabels({ tr("Gravité"),
					     tr("Folio"),
					     tr("Repère"),
					     tr("Constat") });
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->verticalHeader()->setVisible(false);
		//The sentence is the column that says what to do, so it is the one
		//that takes the room that is left. The three before it are a word,
		//a number and a grid reference.
	m_table->horizontalHeader()->setStretchLastSection(true);

		//activated, and not doubleClicked: it fires on Enter as well, so
		//the list is worked through without a mouse. Same signal, same
		//reason, as the missing parts report.
	connect(m_table, &QTableWidget::activated, this,
		[this](const QModelIndex &index)
	{
		activateFinding(index.row());
		Q_EMIT findingActivated(index.row());
	});

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addWidget(m_summary);
	layout->addWidget(m_table);
}

/**
	@brief DrcPanel::~DrcPanel
*/
DrcPanel::~DrcPanel()
{}

/**
	@brief DrcPanel::setFindings
	@param findings : what the run produced, in the order the rules ran
*/
void DrcPanel::setFindings(const QList<DrcFinding> &findings)
{
	m_findings = findings;
	fillTable();
}

/**
	@brief DrcPanel::setSummary
	@param summary : the sentence above the table
*/
void DrcPanel::setSummary(const QString &summary)
{
	m_summary->setText(summary);
}

/**
	@brief DrcPanel::summary
	@return the sentence above the table
*/
QString DrcPanel::summary() const
{
	return m_summary->text();
}

/**
	@brief DrcPanel::fillTable
	One row per finding, in the order they were found. The order is the
	order the rules are registered in, which is stable between runs: a
	list that reshuffles reads as if the project had changed.
*/
void DrcPanel::fillTable()
{
	m_table->clearContents();
	m_table->setRowCount(m_findings.count());

	for (int row = 0 ; row < m_findings.count() ; ++row)
	{
		const DrcFinding &finding = m_findings.at(row);

			//No folio number for a finding about the project as a whole.
			//An empty cell says "nowhere in particular"; a 1 would say
			//"the first sheet", which is somewhere it never was.
		const QString folio = finding.hasFolio()
				      ? QString::number(finding.folio())
				      : QString();

		m_table->setItem(row, 0, new QTableWidgetItem(
					 DrcRule::translatedSeverity(finding.severity())));
		m_table->setItem(row, 1, new QTableWidgetItem(folio));
		m_table->setItem(row, 2, new QTableWidgetItem(finding.position()));
		m_table->setItem(row, 3, new QTableWidgetItem(finding.text()));
	}

	m_table->resizeColumnsToContents();
}

/**
	@brief DrcPanel::activateFinding
	@param row : the row of the table
	@return whether it went anywhere

	The four lines of navigation the parts report uses, and not a fifth:
	the panel stays where it is. Whoever activated a row wants to see the
	component **and** to carry on down the list.
*/
bool DrcPanel::activateFinding(int row)
{
	if (row < 0 || row >= m_findings.count()) {
		return false;
	}

	const DrcFinding &finding = m_findings.at(row);
	if (!finding.hasTarget()) {
		return false;
	}

	Diagram *sheet = finding.diagram();
	if (!sheet) {
		return false;
	}

	sheet->showMe();

	if (Element *element = finding.element())
	{
		sheet->clearSelection();
		element->setSelected(true);
		element->ensureVisible();
		return true;
	}

	if (Conductor *conductor = finding.conductor())
	{
		sheet->clearSelection();
		conductor->setSelected(true);
		conductor->ensureVisible();
		return true;
	}

		//A finding on the sheet itself - a title block with nothing in it,
		//say. The sheet is opened and nothing on it is selected, because
		//nothing on it is what the rule was talking about.
	return true;
}
