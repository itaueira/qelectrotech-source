/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.
*/
#include "projectdbmodelpropertieswidget.h"

#include "../../../dataBase/ui/elementquerywidget.h"
#include "../../../dataBase/ui/summaryquerywidget.h"
#include "../../../qetproject.h"
#include "../projectdbmodel.h"
#include "ui_projectdbmodelpropertieswidget.h"

#include <QDialogButtonBox>

/**
	@brief projectDBModelPropertiesWidget::projectDBModelPropertiesWidget
	@param model
	@param parent
*/
ProjectDBModelPropertiesWidget::ProjectDBModelPropertiesWidget(
		ProjectDBModel *model,
		QWidget *parent) :
	PropertiesEditorWidget(parent),
	ui(new Ui::ProjectDBModelPropertiesWidget)
{
	ui->setupUi(this);
	setModel(model);
}

/**
	@brief projectDBModelPropertiesWidget::~projectDBModelPropertiesWidget
*/
ProjectDBModelPropertiesWidget::~ProjectDBModelPropertiesWidget()
{
	delete ui;
}

/**
	@brief projectDBModelPropertiesWidget::setModel
	@param model

	@par m_model is a raw pointer and stays one
	It cannot dangle. Every ProjectDBModel is built with the QETProject
	it reads as its QObject parent, at all three construction sites :
	QetGraphicsTableFactory passes the project twice, and the two in
	QetGraphicsTableItem do the same, the copy constructor keeping the
	parent of the model it copies. Nothing in the tree ever deletes a
	ProjectDBModel, not even QetGraphicsTableItem::setModel, whose
	documentation hands that duty to its caller and no caller takes it.
	So a model is destroyed only with its project, and this widget is
	already gone by then : QETDiagramEditor::projectWasClosed empties the
	properties dock, which deletes this widget, before it schedules the
	project for deletion, and ~QETProject deletes every Diagram from its
	own body, each emptying the dock again through its destroyed()
	signal, all of it before ~QObject reaches the models parented to the
	project.
	A QPointer was refused on that ground : a guard that cannot fire
	reads as a case that does happen, and it would call for a test at the
	six dereferences of on_m_edit_query_pb_clicked() below, which say by
	their silence that the model is there. It would also not be enough on
	its own, since the two buttons take their enabled state from m_model
	once, here, and a dead model would leave them clickable. Connecting
	to the model destroyed() signal was refused for the same reason, and
	because it would answer too late anyway : a model destroyed as a
	child of its project emits destroyed() after the dock holding this
	widget has already been emptied.
	Price : the invariant lives in this comment and not in the type, so
	giving a ProjectDBModel any other parent, or deleting one by hand,
	breaks this widget with nothing said at compile time. The m_model
	test in on_m_refresh_pb_clicked() is left as it is rather than made
	to match, so the file still reads as if the two cases differed.
*/
void ProjectDBModelPropertiesWidget::setModel(ProjectDBModel *model)
{
	m_model = model;
	ui->m_edit_query_pb->setEnabled(m_model);
	ui->m_refresh_pb->setEnabled(m_model);
}

/**
	@brief projectDBModelPropertiesWidget::on_m_edit_query_pb_clicked
*/
void ProjectDBModelPropertiesWidget::on_m_edit_query_pb_clicked()
{
	QDialog d(this);
	auto l = new QVBoxLayout;
	d.setLayout(l);

	ElementQueryWidget *nom_w = nullptr;
	SummaryQueryWidget *sum_w = nullptr;
	if (m_model->identifier() == "nomenclature")
	{
		nom_w = new ElementQueryWidget(&d);
		nom_w->setQuery(m_model->queryString());
		l->addWidget(nom_w);
	}
	else if (m_model->identifier() == "summary")
	{
		sum_w = new SummaryQueryWidget(&d);
		sum_w->setQuery(m_model->queryString());
		l->addWidget(sum_w);
	}

	auto button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	l->addWidget(button_box);
	connect(button_box, &QDialogButtonBox::accepted, &d, &QDialog::accept);
	connect(button_box, &QDialogButtonBox::rejected, &d, &QDialog::reject);

	if (d.exec())
	{
		if (nom_w) {
			m_model->setQuery(nom_w->queryStr());
		} else if (sum_w) {
			m_model->setQuery(sum_w->queryStr());
		}
	}
}

void ProjectDBModelPropertiesWidget::on_m_refresh_pb_clicked()
{
	if (m_model && m_model->project()) {
		m_model->project()->dataBase()->updateDB();
	}
}
