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
#include "iecstructuredialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "../../bordertitleblock.h"
#include "../../diagram.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetproject.h"

IecStructureDialog::IecStructureDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Structure d'identification (CEI 81346)"));
	setMinimumWidth(560);
	if (m_project) {
		m_settings = m_project->iecSettings();
	}
	setUpWidget();
	refreshPreview();
}

IecStructureSettings IecStructureDialog::settings() const
{
	return m_settings;
}

void IecStructureDialog::setUpWidget()
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	QLabel *explanation = new QLabel(
		tr("La norme identifie un composant en trois parties : la fonction "
		   "(=), la localisation (+) et le produit (-), qui est le repère "
		   "lui-même. La fonction et la localisation sont héritées du folio, "
		   "sauf si le composant dit autre chose.\n\n"
		   "Le repère composé est calculé à l'affichage : le champ du "
		   "composant continue de contenir ce que quelqu'un a tapé. "
		   "Décocher la case remet le projet exactement comme il était — il "
		   "n'y a rien à annuler."), this);
	explanation->setWordWrap(true);
	layout->addWidget(explanation);

	m_enabled = new QCheckBox(
				tr("Utiliser la structure d'identification dans ce projet"),
				this);
	m_enabled->setChecked(m_settings.enabled);
	layout->addWidget(m_enabled);

	QFormLayout *form = new QFormLayout();
	m_display = new QComboBox(this);
	for (IecTagDisplay display : {IecTagDisplay::Short, IecTagDisplay::Full}) {
		m_display->addItem(IecStructureSettings::translatedDisplay(display),
				   int(display));
	}
	m_display->setCurrentIndex(m_display->findData(int(m_settings.display)));
	form->addRow(tr("Sur le dessin, écrire :"), m_display);
	layout->addLayout(form);

	m_folio_note = new QLabel(this);
	m_folio_note->setWordWrap(true);
	layout->addWidget(m_folio_note);

	m_preview = new QLabel(this);
	m_preview->setWordWrap(true);
	m_preview->setTextFormat(Qt::PlainText);
	m_preview->setStyleSheet(QStringLiteral(
		"QLabel { background-color : palette(base); "
		"border : 1px solid palette(mid); padding : 8px; }"));
	layout->addWidget(m_preview);

	QDialogButtonBox *box = new QDialogButtonBox(
				QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	layout->addWidget(box);

	connect(m_enabled, &QCheckBox::toggled,
		this, &IecStructureDialog::refreshPreview);
	connect(m_display, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &IecStructureDialog::refreshPreview);
	connect(box, &QDialogButtonBox::accepted,
		this, &IecStructureDialog::apply);
	connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void IecStructureDialog::refreshPreview()
{
	m_settings.enabled = m_enabled->isChecked();
	m_settings.display = IecTagDisplay(m_display->currentData().toInt());
	m_display->setEnabled(m_settings.enabled);

	if (!m_project) {
		m_preview->clear();
		m_folio_note->clear();
		return;
	}

		//A real component of the open project, not an invented example: what
		//the projectist needs to know is what happens to their drawing.
	Element *sample = nullptr;
	Diagram *sample_diagram = nullptr;
	const QList<Diagram *> diagram_list = m_project->diagrams();
	for (Diagram *diagram : diagram_list) {
		const QList<Element *> elements = diagram->elements();
		for (Element *element : elements) {
			const QString label = element->elementInformations()
					.value(QStringLiteral("label")).toString();
			if (!label.isEmpty()) {
				sample = element;
				sample_diagram = diagram;
				break;
			}
		}
		if (sample) {
			break;
		}
	}

	if (!sample) {
		m_folio_note->clear();
		m_preview->setText(
			tr("Le projet n'a encore aucun composant repéré, alors il n'y a "
			   "rien à montrer ici. L'exemple apparaîtra dès qu'un composant "
			   "aura un repère."));
		return;
	}

	const DiagramContext folio_info =
			sample_diagram->border_and_titleblock.titleblockInformation();
	const QString folio_plant =
			folio_info.value(IecStructure::plantKey()).toString();
	const QString folio_location =
			folio_info.value(IecStructure::folioLocationKey()).toString();

	if (folio_plant.isEmpty() && folio_location.isEmpty()) {
		m_folio_note->setText(
			tr("Attention : ce folio ne porte ni fonction (=) ni "
			   "localisation (+). Sans elles, il n'y a rien à hériter et le "
			   "repère composé n'apporte rien — remplissez-les dans les "
			   "propriétés du folio."));
	} else {
		m_folio_note->setText(
			tr("Ce folio porte : fonction « %1 », localisation « %2 ». "
			   "C'est de là que vient ce qui est hérité.")
				.arg(folio_plant.isEmpty() ? tr("(vide)") : folio_plant,
				     folio_location.isEmpty() ? tr("(vide)") : folio_location));
	}

	const QString stored = sample->elementInformations()
			.value(QStringLiteral("label")).toString();

	IecStructure element_structure = IecStructure::fromTag(stored);
	const QString own_plant = sample->elementInformations()
			.value(IecStructure::plantKey()).toString();
	const QString own_location = sample->elementInformations()
			.value(IecStructure::locationKey()).toString();
	if (!own_plant.isEmpty()) {
		element_structure.plant = own_plant;
	}
	if (!own_location.isEmpty()) {
		element_structure.location = own_location;
	}
	const IecStructure folio_structure(folio_plant, folio_location, QString());

	IecStructureSettings off;
	off.enabled = false;

	m_preview->setText(
		tr("Exemple, sur le composant « %1 » :\n"
		   "  aujourd'hui  →  %2\n"
		   "  après        →  %3\n\n"
		   "Le champ du composant reste « %1 » dans les deux cas.")
			.arg(stored,
			     off.displayedTag(folio_structure, element_structure),
			     m_settings.displayedTag(folio_structure, element_structure)));
}

void IecStructureDialog::apply()
{
	refreshPreview();
	if (m_project) {
		m_project->setIecSettings(m_settings);
	}
	accept();
}
