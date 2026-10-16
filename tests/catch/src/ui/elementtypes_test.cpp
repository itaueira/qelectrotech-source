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
#include "../qt_catch_tostring.h"

#include "../../../../sources/editor/ui/elementpropertieseditorwidget.h"
#include "../../../../sources/properties/elementdata.h"

#include <catch2/catch.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QFile>
#include <QPushButton>
#include <QSpinBox>

/*
	The types the symbol editor offers, condensed (T56).

	What changes is what the editor shows, never what it writes: a terminal
	drawn on the folio of its strip is still saved as a master of the strip
	terminal kind, a ground terminal still as the ground type, and a folio
	report keeps its type. Each case below opens the dialog on some data,
	chooses as a user would, presses OK and reads what would be saved.
*/

namespace
{
	template <typename T>
	T *child(ElementPropertiesEditorWidget &dialog, const char *name)
	{
		T *widget = dialog.findChild<T *>(QLatin1String(name));
		REQUIRE(widget != nullptr);
		return widget;
	}

	/// OK pressed, and what the editor would write into the symbol
	ElementData accept(ElementPropertiesEditorWidget &dialog)
	{
		child<QDialogButtonBox>(dialog, "m_buttonBox")
				->button(QDialogButtonBox::Ok)->click();
		return dialog.editedData();
	}

	QList<int> dataOf(const QComboBox *combo)
	{
		QList<int> values;
		for (int i = 0 ; i < combo->count() ; ++i) {
			values << combo->itemData(i).toInt();
		}
		return values;
	}

	enum { Schematic = 0, Strip = 1 };
}

TEST_CASE("CU-56.1 — The editor offers six base types, and a terminal its role",
	  "[t56][elementtypes]")
{
	ElementPropertiesEditorWidget dialog{ElementData()};

	CHECK(dataOf(child<QComboBox>(dialog, "m_base_type_cb"))
	      == QList<int>{ElementData::Simple, ElementData::Master,
			    ElementData::Slave, ElementData::Terminal,
			    ElementData::Thumbnail, ElementData::ConductorDefinition});

	const QList<int> masters = dataOf(child<QComboBox>(dialog, "m_master_type_cb"));
	CHECK(masters.count() == 4);
	CHECK_FALSE(masters.contains(ElementData::StripTerminal));

		//Every kind of terminal but ground, which is offered as a function;
		//the five after the diode are the upstream's, since 2026-10
	const QList<int> types = dataOf(child<QComboBox>(dialog, "m_terminal_type_cb"));
	CHECK(types == QList<int>{ElementData::TTGeneric, ElementData::TTFuse,
				  ElementData::TTSectional, ElementData::TTDiode,
				  ElementData::TTAntiParallelDiodes, ElementData::TTResistor,
				  ElementData::TTVaristor, ElementData::TTTestSocket,
				  ElementData::TTDiodeReversed});
	CHECK_FALSE(types.contains(ElementData::TTGround));

		//Ground comes last among the functions
	CHECK(child<QComboBox>(dialog, "m_terminal_func_cb")->count() == 4);
	CHECK(child<QComboBox>(dialog, "m_terminal_role_cb")->count() == 2);
}

TEST_CASE("CU-56.2 — A strip terminal opens as a terminal on the strip, and is saved as before",
	  "[t56][elementtypes]")
{
	ElementData data;
	data.m_type = ElementData::Master;
	data.m_master_type = ElementData::StripTerminal;
	data.m_max_slaves = 2;
	ElementPropertiesEditorWidget dialog(data);

	CHECK(child<QComboBox>(dialog, "m_base_type_cb")->currentData().toInt()
	      == ElementData::Terminal);
	CHECK(child<QComboBox>(dialog, "m_terminal_role_cb")->currentIndex() == Strip);
	CHECK(child<QCheckBox>(dialog, "m_strip_limit_cb")->isChecked());
	CHECK(child<QSpinBox>(dialog, "m_strip_limit_sb")->value() == 2);
		//What belongs to a schematic terminal is not offered for it
	CHECK(child<QComboBox>(dialog, "m_terminal_type_cb")->isHidden());
	CHECK(child<QCheckBox>(dialog, "m_strip_slave_cb")->isHidden());
	CHECK_FALSE(child<QCheckBox>(dialog, "m_strip_limit_cb")->isHidden());

	const ElementData saved = accept(dialog);
	CHECK(saved.m_type == ElementData::Master);
	CHECK(saved.m_master_type == ElementData::StripTerminal);
	CHECK(saved.m_max_slaves == 2);
}

TEST_CASE("CU-56.3 — A terminal chosen for the strip is saved as a strip terminal",
	  "[t56][elementtypes]")
{
	ElementPropertiesEditorWidget dialog{ElementData()};
	auto base = child<QComboBox>(dialog, "m_base_type_cb");
	base->setCurrentIndex(base->findData(ElementData::Terminal));
	child<QComboBox>(dialog, "m_terminal_role_cb")->setCurrentIndex(Strip);

	SECTION("with a limit")
	{
		child<QCheckBox>(dialog, "m_strip_limit_cb")->setChecked(true);
		child<QSpinBox>(dialog, "m_strip_limit_sb")->setValue(1);
		const ElementData saved = accept(dialog);
		CHECK(saved.isStripTerminalMaster());
		CHECK(saved.m_max_slaves == 1);
		CHECK_FALSE(saved.m_slave_contact_groups_enabled);
	}

	SECTION("without a limit")
	{
		const ElementData saved = accept(dialog);
		CHECK(saved.isStripTerminalMaster());
		CHECK(saved.m_max_slaves == -1);
	}
}

TEST_CASE("CU-56.4 — Ground is a function of the terminal, saved as the ground type",
	  "[t56][elementtypes]")
{
	SECTION("a ground terminal opens with the function ground and a generic type")
	{
		ElementData data;
		data.m_type = ElementData::Terminal;
		data.m_terminal_type = ElementData::TTGround;
		ElementPropertiesEditorWidget dialog(data);

		auto type = child<QComboBox>(dialog, "m_terminal_type_cb");
		auto function = child<QComboBox>(dialog, "m_terminal_func_cb");
		CHECK(function->currentIndex() == function->count() - 1);
		CHECK(type->currentData().toInt() == ElementData::TTGeneric);
		CHECK_FALSE(type->isEnabled());

		const ElementData saved = accept(dialog);
		CHECK(saved.m_terminal_type == ElementData::TTGround);
		CHECK(saved.m_terminal_function == ElementData::TFGeneric);
	}

	SECTION("a fuse terminal turned into ground becomes a generic one")
	{
		ElementData data;
		data.m_type = ElementData::Terminal;
		data.m_terminal_type = ElementData::TTFuse;
		data.m_terminal_function = ElementData::TFPhase;
		ElementPropertiesEditorWidget dialog(data);

		auto function = child<QComboBox>(dialog, "m_terminal_func_cb");
		function->setCurrentIndex(function->count() - 1);
		CHECK(child<QComboBox>(dialog, "m_terminal_type_cb")->currentData().toInt()
		      == ElementData::TTGeneric);

		const ElementData saved = accept(dialog);
		CHECK(saved.m_terminal_type == ElementData::TTGround);
		CHECK(saved.m_terminal_function == ElementData::TFGeneric);
	}

	SECTION("a ground terminal given the function neutral is no ground any more")
	{
		ElementData data;
		data.m_type = ElementData::Terminal;
		data.m_terminal_type = ElementData::TTGround;
		ElementPropertiesEditorWidget dialog(data);

		auto function = child<QComboBox>(dialog, "m_terminal_func_cb");
		function->setCurrentIndex(function->findData(ElementData::TFNeutral));
		CHECK(child<QComboBox>(dialog, "m_terminal_type_cb")->isEnabled());

		const ElementData saved = accept(dialog);
		CHECK(saved.m_terminal_type == ElementData::TTGeneric);
		CHECK(saved.m_terminal_function == ElementData::TFNeutral);
	}
}

TEST_CASE("CU-56.5 — A folio report keeps its type, which is not offered for anything else",
	  "[t56][elementtypes]")
{
	for (const ElementData::Type report : {ElementData::NextReport,
					       ElementData::PreviousReport})
	{
		CAPTURE(int(report));
		ElementData data;
		data.m_type = report;
		ElementPropertiesEditorWidget dialog(data);

		auto base = child<QComboBox>(dialog, "m_base_type_cb");
		CHECK(base->currentData().toInt() == report);
		CHECK_FALSE(base->isEnabled());
		CHECK(base->count() == 7);

		CHECK(accept(dialog).m_type == report);
	}
}

TEST_CASE("CU-56.6 — A schematic terminal keeps its type, its function and its link to the strip",
	  "[t56][elementtypes]")
{
	ElementData data;
	data.m_type = ElementData::Terminal;
	data.m_terminal_type = ElementData::TTDiode;
	data.m_terminal_function = ElementData::TFNeutral;
	data.m_terminal_strip_slave = true;
	ElementPropertiesEditorWidget dialog(data);

	CHECK(child<QComboBox>(dialog, "m_terminal_role_cb")->currentIndex() == Schematic);
	CHECK(child<QCheckBox>(dialog, "m_strip_limit_cb")->isHidden());

	const ElementData saved = accept(dialog);
	CHECK(saved.m_type == ElementData::Terminal);
	CHECK(saved.m_terminal_type == ElementData::TTDiode);
	CHECK(saved.m_terminal_function == ElementData::TFNeutral);
	CHECK(saved.isStripTerminalSlave());
}

TEST_CASE("CU-56.7 — Opened and saved untouched, no symbol of the collection changes its kind",
	  "[t56][elementtypes]")
{
		//Every symbol of the collection shipped with the program that is not
		//a simple one or a thumbnail: masters, slaves, terminals, folio
		//reports. What the editor would write for each, without anything
		//chosen, has to be what the symbol says now.
	const QDir elements(QStringLiteral(QET_EXAMPLES_DIR "/../elements"));
	REQUIRE(elements.exists());

	auto kindOf = [](ElementData data) {
		QDomDocument document;
		document.appendChild(data.kindInfoToXml(document));
		return QString::number(int(data.m_type)) + document.toString(-1);
	};

	int checked = 0;
	QStringList changed;
	QDirIterator it(elements.absolutePath(), {QStringLiteral("*.elmt")},
			QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext())
	{
		const QString path = it.next();
		QFile file(path);
		QDomDocument document;
		if (!file.open(QIODevice::ReadOnly) || !document.setContent(&file)) {
			continue;
		}
		ElementData data;
		if (!data.fromXml(document.documentElement())
		    || data.m_type == ElementData::Simple
		    || data.m_type == ElementData::Thumbnail) {
			continue;
		}

		ElementPropertiesEditorWidget dialog(data);
		const ElementData saved = accept(dialog);
		if (kindOf(saved) != kindOf(data)) {
			changed << elements.relativeFilePath(path);
		}
		++checked;
	}

	INFO("checked " << checked << ", changed: " << changed.join(QStringLiteral(", ")).toStdString());
	CHECK(checked > 600);
	CHECK(changed.isEmpty());
}
