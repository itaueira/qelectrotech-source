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

#include "../../../../sources/diagram.h"
#include "../../../../sources/qetpalette.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/terminalelement.h"
#include "../../../../sources/TerminalStrip/realterminal.h"
#include "../../../../sources/TerminalStrip/terminalstrip.h"
#include "../../../../sources/TerminalStrip/ui/freeterminalmodel.h"
#include "../../../../sources/TerminalStrip/ui/terminalstripmodel.h"
#include "../../../../sources/dataBase/ui/elementquerywidget.h"
#include "../../../../sources/ui/configpage/generalconfigurationpage.h"
#include "../../../../sources/ui/diagramcontextwidget.h"

#include <catch2/catch.hpp>

#include <QAbstractItemModel>
#include <QBrush>
#include <QColor>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QPalette>
#include <QRegularExpression>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QWidget>

#include <algorithm>

/*
	Fixed colors of the program that the first pass of T63 did not reach,
	measured on a dark palette.

	Each of them was picked on a light background, and the defect has two
	shapes. A light background given to a cell or a field without a text
	color - the yellow of an edited terminal, the orange of a value too
	high - leaves the text to the palette, and the text of a dark palette is
	white: 1.07:1 on the yellow, 1.97:1 on the orange. The text of those is
	black now, on both palettes, since the background is light on both. A
	colored text - the red of a refused key or query - sinks into the dark
	background instead, and goes through QET::Palette::readable(), which
	keeps it as it is on a light palette.

	The dark palette is the one Qt builds on Windows, the one the review
	measured against: Window 30, Base 45, white text. Nothing here sets the
	application palette; it is handed to the widget, so nothing of this file
	outlives its cases.
*/

namespace
{
	/**
		The dark palette Qt builds on Windows, as darkpalette_test.cpp
		describes it: Window 30, Base Window.lighter(150) - 45 - white text,
		and a faint translucent white for the alternate rows.
	*/
	QPalette windowsDark()
	{
		const QColor window(0x1e, 0x1e, 0x1e);
		QPalette palette;
		for (QPalette::ColorGroup group : {QPalette::Active,
						   QPalette::Inactive,
						   QPalette::Disabled})
		{
			palette.setColor(group, QPalette::Window, window);
			palette.setColor(group, QPalette::WindowText, Qt::white);
			palette.setColor(group, QPalette::Base, window.lighter(150));
			palette.setColor(group, QPalette::AlternateBase, QColor(255, 255, 255, 0x0f));
			palette.setColor(group, QPalette::Text, Qt::white);
			palette.setColor(group, QPalette::Button, window.lighter(200));
			palette.setColor(group, QPalette::ButtonText, Qt::white);
		}
		return palette;
	}

	/// The dark palettes the cases go through.
	QList<QPair<const char *, QPalette>> darkPalettes()
	{
		return {{"windows", windowsDark()},
			{"fusion", QET::Palette::fusionDark()}};
	}

	/// a as it shows over the opaque b.
	QColor over(const QColor &a, const QColor &b)
	{
		const int alpha = a.alpha();
		const auto mix = [alpha](int front, int back) {
			return (front * alpha + back * (255 - alpha) + 127) / 255;
		};
		return QColor(mix(a.red(), b.red()),
			      mix(a.green(), b.green()),
			      mix(a.blue(), b.blue()));
	}

	/// Where readable() promises the text reads: Window, Base, AlternateBase.
	QList<QColor> textGrounds(const QPalette &palette)
	{
		const QColor base = palette.color(QPalette::Active, QPalette::Base);
		return {palette.color(QPalette::Active, QPalette::Window),
			base,
			over(palette.color(QPalette::Active, QPalette::AlternateBase), base)};
	}

	/// The lowest contrast of the opaque color against the backgrounds.
	double worst(const QColor &color, const QList<QColor> &backgrounds)
	{
		double lowest = 21.0;
		for (const QColor &background : backgrounds) {
			lowest = std::min(lowest, QET::Palette::contrastRatio(color, background));
		}
		return lowest;
	}

	/**
		The color a style sheet, or the style attribute of a rich text,
		declares for @a property; invalid when it declares none. The
		property has to start the declaration, so that "color" does not
		find the end of "background-color".
	*/
	QColor declared(const QString &sheet, const QString &property)
	{
		const QRegularExpression declaration(
			QStringLiteral("(?:^|[;{\\s\"])%1\\s*:\\s*([^;}\"]+)")
			.arg(QRegularExpression::escape(property)));
		const QRegularExpressionMatch match = declaration.match(sheet);
		return match.hasMatch() ? QColor(match.captured(1).trimmed()) : QColor();
	}

	/**
		One folio and two terminals, 1 and 2: the first goes on a strip,
		the second stays free. The symbol is the terminal of
		stripreorder_test.cpp.
	*/
	QString fixtureXml()
	{
		QString instances;
		const char *labels[] = {"1", "2"};
		for (int index = 0 ; index < 2 ; ++index)
		{
			instances += QStringLiteral(
					     "<element x=\"%1\" y=\"150\" z=\"10\" prefix=\"\""
					     " freezeLabel=\"false\" orientation=\"0\""
					     " type=\"embed://bench/terminal.elmt\""
					     " uuid=\"{da4cf1c0-0000-4000-8000-00000000000%2}\">"
					     "<terminals>"
					     "<terminal x=\"10\" y=\"0\" orientation=\"1\" id=\"%3\"/>"
					     "</terminals>"
					     "<inputs/>"
					     "<elementInformations>"
					     "<elementInformation show=\"1\" name=\"label\">%4"
					     "</elementInformation>"
					     "</elementInformations>"
					     "<dynamic_texts/><texts_groups/>"
					     "</element>")
				     .arg(120 + index * 120)
				     .arg(index)
				     .arg(index)
				     .arg(QLatin1String(labels[index]));
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">"
			       "<element name=\"terminal.elmt\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"20\""
			       " hotspot_x=\"10\" hotspot_y=\"10\""
			       " orientation=\"dnnn\" link_type=\"terminal\">"
			       "<names><name lang=\"en\">Terminal</name></names>"
			       "<description>"
			       "<rect x=\"-4\" y=\"-4\" width=\"8\" height=\"8\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
			       "</description>"
			       "</definition>"
			       "</element>"
			       "</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " folio=\"1\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%1</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(instances);
	}

	/// The terminal element of @a diagram labelled @a label, nullptr when none.
	TerminalElement *terminalLabelled(Diagram *diagram, const QString &label)
	{
		if (!diagram) {
			return nullptr;
		}

		const auto elements = diagram->elements();
		for (const auto element : elements)
		{
			if (element->elementData().m_type != ElementData::Terminal) {
				continue;
			}

			const auto terminal_element = static_cast<TerminalElement *>(element);
			const auto real_t = terminal_element->realTerminal();
			if (!real_t.isNull() && real_t->label() == label) {
				return terminal_element;
			}
		}

		return nullptr;
	}

	/// The row of @a model whose @a column shows @a text, -1 when none.
	int rowShowing(const QAbstractItemModel &model, int column, const QString &text)
	{
		for (int row = 0 ; row < model.rowCount(QModelIndex()) ; ++row) {
			if (model.index(row, column).data().toString() == text) {
				return row;
			}
		}
		return -1;
	}

	/// Before the edit the model colors nothing: the palette draws the cell.
	void checkPlainCell(const QAbstractItemModel &model, const QModelIndex &index)
	{
		CHECK_FALSE(model.data(index, Qt::BackgroundRole).isValid());
		CHECK_FALSE(model.data(index, Qt::ForegroundRole).isValid());
	}

	/**
		The cell @a index of @a model was edited and not applied yet: the
		yellow that marks it, and a text that reads on it. The model does
		not know the palette, and does not need to: the yellow is light on
		both, so the text is black on both.
	*/
	void checkEditedCell(const QAbstractItemModel &model, const QModelIndex &index)
	{
		const QVariant background = model.data(index, Qt::BackgroundRole);
		const QVariant foreground = model.data(index, Qt::ForegroundRole);
		REQUIRE(background.isValid());
		REQUIRE(foreground.isValid());

		const QColor yellow = background.value<QBrush>().color();
		const QColor text = foreground.value<QBrush>().color();
		INFO("text " << text.name().toStdString() << " on " << yellow.name().toStdString()
		     << ": " << QET::Palette::contrastRatio(text, yellow) << ":1");
		CHECK(yellow == QColor(Qt::yellow));
		CHECK(QET::Palette::contrastRatio(text, yellow) >= 4.5);

			// What the cell showed before the text was given a color: the
			// text of the dark palette, which the view used for want of
			// one - white on yellow, 1.07:1.
		for (const auto &named : darkPalettes())
		{
			INFO(named.first);
			CHECK(QET::Palette::contrastRatio(
				      named.second.color(QPalette::Active, QPalette::Text), yellow) < 4.5);
		}

			// And on a light palette the text is the one it always had.
		CHECK(text == QET::Palette::fusionLight().color(QPalette::Active, QPalette::Text));
	}

	/// The colors a refused key is given, in the table and in the sentence under it.
	struct RefusedKey
	{
		QColor key;
		QColor sentence;
	};

	/**
		A key DiagramContextWidget refuses - an upper case letter and a
		space - typed into its table on @a palette, and the two colors the
		widget answers with.
	*/
	RefusedKey refusedKey(const QPalette &palette)
	{
		DiagramContextWidget widget;
		widget.setPalette(palette);
		auto *table = widget.findChild<QTableWidget *>(QStringLiteral("m_table"));
		auto *label = widget.findChild<QLabel *>(QStringLiteral("m_label"));
		REQUIRE(table != nullptr);
		REQUIRE(label != nullptr);
			// The palette reached the table: the color is computed from it.
		REQUIRE(table->palette().color(QPalette::Active, QPalette::Base)
			== palette.color(QPalette::Active, QPalette::Base));

		table->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Bad Key")));
		widget.refreshFormatLabel();

		REQUIRE(table->item(0, 0) != nullptr);
		INFO("sentence: " << label->text().toStdString());
		RefusedKey colors;
		colors.key = table->item(0, 0)->foreground().color();
		colors.sentence = declared(label->text(), QStringLiteral("color"));
		REQUIRE(colors.sentence.isValid());
		return colors;
	}

	/**
		The color of the warning under the query of the export window,
		built inside a widget that carries @a palette.
	*/
	QColor queryWarning(const QPalette &palette)
	{
		QWidget host;
		host.setPalette(palette);
		ElementQueryWidget widget(&host);
			// The palette reached the widget when it was built, which is
			// when the color is computed.
		REQUIRE(widget.palette().color(QPalette::Active, QPalette::Window)
			== palette.color(QPalette::Active, QPalette::Window));

		auto *warning = widget.findChild<QLabel *>(QStringLiteral("m_query_warning_label"));
		REQUIRE(warning != nullptr);
		INFO("style sheet: " << warning->styleSheet().toStdString());
		const QColor color = declared(warning->styleSheet(), QStringLiteral("color"));
		REQUIRE(color.isValid());
		return color;
	}
}

TEST_CASE("T63 — a célula editada e ainda não aplicada da régua de bornes: amarelo com texto preto, legível no escuro",
	  "[t63][darkmode][terminalstrip]")
{
	UiBench::ScratchProject bench(fixtureXml(), QStringLiteral("darkfixedcolors.qet"));
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());

	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet != nullptr);
	TerminalElement *on_strip = terminalLabelled(sheet, QStringLiteral("1"));
	REQUIRE(on_strip != nullptr);
	REQUIRE(terminalLabelled(sheet, QStringLiteral("2")) != nullptr);

	TerminalStrip *strip = bench->newTerminalStrip(QString(), QString(), QStringLiteral("X1"));
	REQUIRE(strip != nullptr);
	REQUIRE(strip->addTerminal(on_strip));

	SECTION("a régua, no gerenciador de réguas")
	{
		TerminalStripModel model(strip);
		REQUIRE(model.rowCount(QModelIndex()) == 1);
		const QModelIndex label = model.index(0, TerminalStripModel::Label);
		const QModelIndex cable = model.index(0, TerminalStripModel::Cable);

		checkPlainCell(model, label);
		REQUIRE(model.setData(label, QStringLiteral("1A"), Qt::EditRole));
		checkEditedCell(model, label);
			// Only the edited cell: the one beside it keeps the palette.
		checkPlainCell(model, cable);
	}

	SECTION("os bornes livres")
	{
		FreeTerminalModel model(bench.project());
		const int row = rowShowing(model, FreeTerminalModel::Label, QStringLiteral("2"));
		REQUIRE(row >= 0);
			// The terminal on the strip is not free.
		CHECK(rowShowing(model, FreeTerminalModel::Label, QStringLiteral("1")) < 0);
		const QModelIndex label = model.index(row, FreeTerminalModel::Label);
		const QModelIndex xref = model.index(row, FreeTerminalModel::XRef);

		checkPlainCell(model, label);
		REQUIRE(model.setData(label, QStringLiteral("2A"), Qt::EditRole));
		checkEditedCell(model, label);
		checkPlainCell(model, xref);
	}
}

TEST_CASE("T63 — a chave recusada das informações: vermelho a 4,5:1 na paleta escura, o mesmo vermelho na clara",
	  "[t63][darkmode][ui]")
{
	for (const auto &named : darkPalettes())
	{
		INFO(named.first);
		const QPalette &dark = named.second;
		REQUIRE(QET::Palette::isDark(dark));
		const QList<QColor> grounds = textGrounds(dark);
			// The red as it was written does not read there, which is why
			// it has to move: 3.44:1 on the Base of the Windows palette.
		REQUIRE(worst(QColor(Qt::red), grounds) < 4.5);

		const RefusedKey colors = refusedKey(dark);
		INFO("key " << colors.key.name().toStdString()
		     << " at " << worst(colors.key, grounds)
		     << ", sentence " << colors.sentence.name().toStdString()
		     << " at " << worst(colors.sentence, grounds));
		CHECK(worst(colors.key, grounds) >= 4.5);
		CHECK(worst(colors.sentence, grounds) >= 4.5);
			// Still a red: the same hue, lighter.
		CHECK(colors.key.hslHue() == QColor(Qt::red).hslHue());
	}

	const RefusedKey light = refusedKey(QET::Palette::fusionLight());
	CHECK(light.key == QColor(Qt::red));
	CHECK(light.sentence == QColor(Qt::red));
}

TEST_CASE("T63 — o aviso da consulta da exportação: vermelho a 4,5:1 na paleta escura, o mesmo vermelho na clara",
	  "[t63][darkmode][ui]")
{
	for (const auto &named : darkPalettes())
	{
		INFO(named.first);
		const QList<QColor> grounds = textGrounds(named.second);
		const QColor warning = queryWarning(named.second);
		INFO("warning " << warning.name().toStdString() << " at " << worst(warning, grounds));
		CHECK(worst(warning, grounds) >= 4.5);
	}

	CHECK(queryWarning(QET::Palette::fusionLight()) == QColor(Qt::red));
}

TEST_CASE("T63 — o valor alto demais das preferências: laranja com texto preto, legível no escuro",
	  "[t63][darkmode][ui]")
{
	GeneralConfigurationPage page;
	auto *spin = page.findChild<QSpinBox *>(QStringLiteral("MaxPartsElementEditorList_sb"));
	REQUIRE(spin != nullptr);

	spin->setValue(200);
	CHECK(spin->styleSheet().isEmpty());

	spin->setValue(600);
	const QString sheet = spin->styleSheet();
	INFO("style sheet: " << sheet.toStdString());
	const QColor background = declared(sheet, QStringLiteral("background-color"));
	const QColor text = declared(sheet, QStringLiteral("color"));
	REQUIRE(background.isValid());
	REQUIRE(text.isValid());
	CHECK(background == QColor(QStringLiteral("orange")));
	CHECK(QET::Palette::contrastRatio(text, background) >= 4.5);
		// Without a color of its own, the text is the palette's: white on
		// orange in the dark, 1.97:1.
	for (const auto &named : darkPalettes())
	{
		INFO(named.first);
		CHECK(QET::Palette::contrastRatio(
			      named.second.color(QPalette::Active, QPalette::Text), background) < 4.5);
	}
		// On a light palette the text is the black it always was.
	CHECK(text == QET::Palette::fusionLight().color(QPalette::Active, QPalette::Text));

	spin->setValue(200);
	CHECK(spin->styleSheet().isEmpty());
}
