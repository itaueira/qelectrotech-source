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
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetinformation.h"
#include "../../../../sources/ui/customelementinfopartwidget.h"
#include "../../../../sources/ui/elementinfopartwidget.h"
#include "../../../../sources/ui/elementinfowidget.h"

#include <catch2/catch.hpp>

#include <QLineEdit>

/*
	Assigning a catalogue part writes on the element the physical view of
	the part - clearances, insertion point, outline flag -, its data sheet
	and its picture. The information panel has no field for those keys, so
	it showed each one as an anonymous editable row, inviting a number typed
	by hand over the one that belongs to the part code.

	They are not shown any more, and the panel keeps them as the element has
	them: the panel builds the information it applies from its rows alone,
	so a key it does not show would otherwise be dropped by the first edit
	of anything else.
*/

namespace
{
	/// The first element of @a project whose type is @a type, or nullptr
	Element *firstOfType(const UiBench::Project &project, ElementData::Type type)
	{
		for (Diagram *diagram : project.diagrams())
		{
			for (Element *element : diagram->elements())
			{
				if (element->elementData().m_type == type) {
					return element;
				}
			}
		}
		return nullptr;
	}

	/// The keys of the custom rows the panel built
	QStringList customRows(ElementInfoWidget &panel)
	{
		QStringList keys;
		for (auto row : panel.findChildren<CustomElementInfoPartWidget *>()) {
			keys << row->key();
		}
		return keys;
	}

	/// What a catalogue part writes, as the assignment writes it
	DiagramContext withPart(DiagramContext info)
	{
		info.addValue(QStringLiteral("clearance_top"),    QStringLiteral("10"));
		info.addValue(QStringLiteral("clearance_bottom"), QStringLiteral("10"));
		info.addValue(QStringLiteral("clearance_left"),   QStringLiteral("2.5"));
		info.addValue(QStringLiteral("clearance_right"),  QStringLiteral("2.5"));
		info.addValue(QStringLiteral("insertion_x"),      QStringLiteral("22.5"));
		info.addValue(QStringLiteral("insertion_y"),      QStringLiteral("40"));
		info.addValue(QStringLiteral("draw_outline"),     QStringLiteral("1"));
		info.addValue(QStringLiteral("datasheet"),        QStringLiteral("https://example.com/k1.pdf"));
		info.addValue(QStringLiteral("image"),            QStringLiteral("k1.png"));
			//a property the office declared and typed: a custom row is
			//exactly what it is for
		info.addValue(QStringLiteral("curve"),            QStringLiteral("C"));
		return info;
	}

	const QStringList catalog_keys {
		QStringLiteral("clearance_top"),
		QStringLiteral("clearance_bottom"),
		QStringLiteral("clearance_left"),
		QStringLiteral("clearance_right"),
		QStringLiteral("insertion_x"),
		QStringLiteral("insertion_y"),
		QStringLiteral("draw_outline"),
		QStringLiteral("datasheet"),
		QStringLiteral("image") };
}

TEST_CASE("T20 — the panel of an element offers no row for what the catalogue part wrote, "
	  "and keeps it when something else is edited",
	  "[t20][elementinfo]")
{
	UiBench::Project project(QStringLiteral("industrial.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	Element *element = firstOfType(project, ElementData::Simple);
	REQUIRE(element != nullptr);
	const DiagramContext before = withPart(element->elementInformations());
	element->setElementInformations(before);

	ElementInfoWidget panel(element);

		//No anonymous row for the part's own keys, and still a row for
		//a property somebody declared in the catalogue and can type
	const QStringList rows = customRows(panel);
	for (const QString &key : catalog_keys)
	{
		CAPTURE(key);
		CHECK_FALSE(rows.contains(key));
	}
	CHECK(rows.contains(QStringLiteral("curve")));

		//Edit the label, the way somebody does every day, and read what
		//the panel would apply
	QLineEdit *label = nullptr;
	for (auto part : panel.findChildren<ElementInfoPartWidget *>())
	{
		if (part->key() == QETInformation::ELMT_LABEL) {
			label = part->findChild<QLineEdit *>();
		}
	}
	REQUIRE(label != nullptr);
	label->setText(QStringLiteral("-K99"));

	const DiagramContext applied = panel.currentInfo();
	CHECK(applied.value(QETInformation::ELMT_LABEL).toString() == QStringLiteral("-K99"));
	for (const QString &key : catalog_keys)
	{
		CAPTURE(key);
		CHECK(applied.contains(key));
		CHECK(applied.value(key) == before.value(key));
	}
	CHECK(applied.value(QStringLiteral("curve")) == before.value(QStringLiteral("curve")));
}

TEST_CASE("T20 — on a terminal the size of the part is hidden too, because a terminal "
	  "has no field for it",
	  "[t20][elementinfo]")
{
	UiBench::Project project(QStringLiteral("industrial.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	Element *terminal = firstOfType(project, ElementData::Terminal);
	REQUIRE(terminal != nullptr);
		//The panel of a terminal shows a shorter list, without the size
	REQUIRE_FALSE(QETInformation::terminalElementInfoKeys().contains(QStringLiteral("width")));

	DiagramContext before = withPart(terminal->elementInformations());
	before.addValue(QStringLiteral("width"),  QStringLiteral("5.2"));
	before.addValue(QStringLiteral("height"), QStringLiteral("47"));
	terminal->setElementInformations(before);

	ElementInfoWidget panel(terminal);
	const QStringList rows = customRows(panel);
	CHECK_FALSE(rows.contains(QStringLiteral("width")));
	CHECK_FALSE(rows.contains(QStringLiteral("height")));
	CHECK_FALSE(rows.contains(QStringLiteral("insertion_x")));

	const DiagramContext applied = panel.currentInfo();
	CHECK(applied.value(QStringLiteral("width")) == before.value(QStringLiteral("width")));
	CHECK(applied.value(QStringLiteral("height")) == before.value(QStringLiteral("height")));
	CHECK(applied.value(QStringLiteral("insertion_x")) == before.value(QStringLiteral("insertion_x")));
}
