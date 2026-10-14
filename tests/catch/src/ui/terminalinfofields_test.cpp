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
#include "../../../../sources/ui/elementinfopartwidget.h"
#include "../../../../sources/ui/elementinfowidget.h"

#include <catch2/catch.hpp>

#include <QSet>
#include <QUndoCommand>
#include <QUndoStack>

/*
	A terminal has the information fields of any other component (T55).

	It used to get a shorter list, without plant, location, function,
	description or quantity, on the idea that the terminal strip holds the
	plant and location of its terminals. The strip keeps them for itself and
	hands nothing to its terminals, and a terminal outside any strip - every
	terminal of the projects this was asked for - had no way at all to say
	where it is, although its symbol shows the plant.
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

	/// The keys of the fields the panel built, custom properties left out
	QSet<QString> fieldsOf(ElementInfoWidget &panel)
	{
		QSet<QString> keys;
		for (ElementInfoPartWidget *field : panel.findChildren<ElementInfoPartWidget *>()) {
			keys.insert(field->key());
		}
		return keys;
	}

	ElementInfoPartWidget *field(ElementInfoWidget &panel, const QString &key)
	{
		for (ElementInfoPartWidget *field : panel.findChildren<ElementInfoPartWidget *>()) {
			if (field->key() == key) {
				return field;
			}
		}
		return nullptr;
	}
}

TEST_CASE("CU-55.1 — A terminal has the fields of any component", "[t55][elementinfo]")
{
	UiBench::Project project(QStringLiteral("industrial.qet"));
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	Element *terminal = firstOfType(project, ElementData::Terminal);
	Element *simple = firstOfType(project, ElementData::Simple);
	REQUIRE(terminal != nullptr);
	REQUIRE(simple != nullptr);

	ElementInfoWidget terminal_panel(terminal);
	ElementInfoWidget simple_panel(simple);
	const QSet<QString> terminal_fields = fieldsOf(terminal_panel);

	CHECK(terminal_fields == fieldsOf(simple_panel));
	for (const QString &key : {QETInformation::ELMT_PLANT,
				   QETInformation::ELMT_LOCATION,
				   QETInformation::ELMT_FUNCTION,
				   QETInformation::ELMT_DESCRIPTION,
				   QETInformation::ELMT_QUANTITY})
	{
		CAPTURE(key);
		CHECK(terminal_fields.contains(key));
	}
}

TEST_CASE("CU-55.2 — What is typed in a new field of a terminal is applied, undone and saved",
	  "[t55][elementinfo]")
{
	UiBench::ScratchProject bench(
		UiBench::fileContent(UiBench::examplePath(QStringLiteral("industrial.qet"))),
		QStringLiteral("industrial.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Element *terminal = nullptr;
	for (Diagram *diagram : bench.diagrams()) {
		for (Element *element : diagram->elements()) {
			if (!terminal && element->elementData().m_type == ElementData::Terminal) {
				terminal = element;
			}
		}
	}
	REQUIRE(terminal != nullptr);
	const QUuid uuid = terminal->uuid();
	const DiagramContext before = terminal->elementInformations();

	ElementInfoWidget panel(terminal);
	ElementInfoPartWidget *plant = field(panel, QETInformation::ELMT_PLANT);
	ElementInfoPartWidget *location = field(panel, QETInformation::ELMT_LOCATION);
	REQUIRE(plant != nullptr);
	REQUIRE(location != nullptr);
	plant->setText(QStringLiteral("QCM"));
	location->setText(QStringLiteral("X10"));

	QUndoCommand *command = panel.associatedUndo();
	REQUIRE(command != nullptr);
	terminal->diagram()->undoStack().push(command);
	CHECK(terminal->elementInformations().value(QETInformation::ELMT_PLANT).toString()
	      == QLatin1String("QCM"));
	CHECK(terminal->elementInformations().value(QETInformation::ELMT_LOCATION).toString()
	      == QLatin1String("X10"));
		//What the terminal said before is all still there
	CHECK(terminal->elementInformations().value(QETInformation::ELMT_LABEL)
	      == before.value(QETInformation::ELMT_LABEL));

	SECTION("Ctrl+Z takes them back")
	{
		terminal->diagram()->undoStack().undo();
		CHECK(terminal->elementInformations().value(QETInformation::ELMT_PLANT)
		      .toString().isEmpty());
		CHECK(terminal->elementInformations().value(QETInformation::ELMT_LOCATION)
		      .toString().isEmpty());
	}

	SECTION("saved and opened again, the terminal still says them")
	{
		REQUIRE(bench.saveAndReopen());
		Element *again = nullptr;
		for (Diagram *diagram : bench.diagrams()) {
			for (Element *element : diagram->elements()) {
				if (element->uuid() == uuid) {
					again = element;
				}
			}
		}
		REQUIRE(again != nullptr);
		CHECK(again->elementInformations().value(QETInformation::ELMT_PLANT).toString()
		      == QLatin1String("QCM"));
		CHECK(again->elementInformations().value(QETInformation::ELMT_LOCATION).toString()
		      == QLatin1String("X10"));
	}
}
