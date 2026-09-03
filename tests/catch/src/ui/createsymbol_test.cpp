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

#include "../../../../sources/ElementsCollection/symbolbuilder.h"
#include "../../../../sources/ElementsCollection/ui/createsymboldialog.h"
#include "../../../../sources/catalog/catalog.h"

#include <catch2/catch.hpp>

#include <QLineEdit>
#include <QPointF>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>

/*
	A connection point typed into the table, on a drawing that has no free
	end to deduce one from.

	The rule itself - a connection point lands on the main grid - is proved
	without a window in symbol_test.cpp, on a SymbolDefinition built by hand.
	What that cannot see is the table: whether the number the drawer typed and
	the number that will be written to the file are the same number. A snap
	that works perfectly and a cell that goes on showing what was typed give a
	symbol saved at a position nobody chose, and the person who typed it has no
	way of knowing. That is the sentence the queued case ends on, and it is
	what is checked here.

	So nothing here is called directly on the symbol: everything goes in
	through the widgets of the dialog, and comes back out of the widgets of
	the dialog, exactly as it does under a mouse.

	What stays out: whether any of this is readable, and the file the Save
	button writes. Save opens a file chooser and asks about overwriting an
	existing symbol, both of them windows, so the button is only asked whether
	it is enabled - which is the whole of "the dialog now lets you save".
*/

namespace
{
	/// The step a connection point has to land on, as the queued case states it.
	const qreal main_grid_step = 10.0;

	/**
		A rectangle drawn on its own, with no line coming out of it - the
		drawing of the queued case. It is named and given a class so that the
		only thing missing from it is a connection point: a dialog that
		refused for three reasons at once would not prove it refused for
		this one.
	*/
	SymbolDefinition drawnRectangle()
	{
		SymbolDefinition symbol;
		symbol.name = QStringLiteral("Rectangle sans queue");
		symbol.class_key = QStringLiteral("contactor");
		symbol.shapes << SymbolShape(SymbolShapeType::Rectangle,
					     QPolygonF() << QPointF(100.0, 100.0)
							 << QPointF(160.0, 150.0));
		symbol.hotspot = QPointF(100.0, 100.0);
		return symbol;
	}

	QPushButton *buttonNamed(QWidget &parent, const QString &text)
	{
		const QList<QPushButton *> buttons = parent.findChildren<QPushButton *>();
		for (QPushButton *button : buttons)
		{
			if (button->text() == text) {
				return button;
			}
		}
		return nullptr;
	}

	/// The name field, told from the folder field by what it was filled with.
	QLineEdit *fieldHolding(QWidget &parent, const QString &text)
	{
		const QList<QLineEdit *> fields = parent.findChildren<QLineEdit *>();
		for (QLineEdit *field : fields)
		{
			if (field->text() == text) {
				return field;
			}
		}
		return nullptr;
	}

	QString cellText(QTableWidget *table, int row, int column)
	{
		QTableWidgetItem *item = table ? table->item(row, column) : nullptr;
		return item ? item->text() : QString();
	}

	/// The way the dialog writes a position into a cell.
	QString asCell(const QPointF &position)
	{
		return QStringLiteral("%1 ; %2").arg(position.x()).arg(position.y());
	}

	/**
		Whether the table is showing the positions the symbol actually holds,
		and which row is not, said out loud. This is the whole complaint of
		the queued case: a table cannot show 43 and save 40.
	*/
	QString disagreement(QTableWidget *table, const SymbolDefinition &symbol)
	{
		if (!table) {
			return QStringLiteral("no table");
		}
		if (table->rowCount() != symbol.terminals.size()) {
			return QStringLiteral("rows: table %1, symbol %2")
			       .arg(QString::number(table->rowCount()),
				    QString::number(symbol.terminals.size()));
		}
		for (int row = 0 ; row < table->rowCount() ; ++row)
		{
			const QString shown = cellText(table, row, 0);
			const QString held = asCell(symbol.terminals.at(row).position);
			if (shown != held) {
				return QStringLiteral("row %1: table shows \"%2\", "
						      "symbol holds \"%3\"")
				       .arg(QString::number(row), shown, held);
			}
		}
		return QString();
	}

	const char *no_terminal_message =
		"Le symbole n'a aucun point de raccordement : "
		"aucun conducteur ne pourra s'y brancher.";
	const char *no_name_message = "Le symbole n'a pas de nom.";
}

TEST_CASE("F1 E.14 (fila) — ponto de ligação onde o desenho não tem ponta",
	  "[uibench][symbol]")
{
	Catalog catalog;
	QString catalog_error;
	const bool catalog_open = catalog.openInMemory(&catalog_error);
	INFO(catalog_error.toStdString());
	REQUIRE(catalog_open);

	const SymbolGrid grid;
	REQUIRE(grid.main_step == main_grid_step);

	SymbolDefinition drawn = drawnRectangle();
	// The dialog is handed what the sheet deduced, not a list written here:
	// "the table arrives empty" is only worth checking if the emptiness came
	// from the drawing.
	drawn.terminals = drawn.suggestedTerminals(grid);

	CreateSymbolDialog dialog(drawn, &catalog);

	QTableWidget *table = dialog.findChild<QTableWidget *>();
	QPushButton *add_point = buttonNamed(dialog, QStringLiteral("Ajouter un point"));
	QPushButton *save = buttonNamed(dialog, QStringLiteral("Enregistrer le symbole"));
	QLineEdit *name = fieldHolding(dialog, QStringLiteral("Rectangle sans queue"));
	REQUIRE(table != nullptr);
	REQUIRE(add_point != nullptr);
	REQUIRE(save != nullptr);
	REQUIRE(name != nullptr);

	SECTION("o retângulo sozinho não sugere ponta nenhuma, e a tabela chega vazia")
	{
		REQUIRE(drawn.suggestedTerminals(grid).isEmpty());
		REQUIRE(table->rowCount() == 0);
		REQUIRE(dialog.symbol().terminals.isEmpty());
	}

	SECTION("sem ponto de ligação o diálogo recusa gravar, e é por isso")
	{
		const QStringList messages = dialog.symbol().problemMessages(grid);
		INFO(messages.join(QStringLiteral(" | ")).toStdString());
		REQUIRE(messages == QStringList{QString::fromUtf8(no_terminal_message)});
		REQUIRE_FALSE(save->isEnabled());
	}

	SECTION("acrescentar um ponto passa a deixar gravar")
	{
		add_point->click();

		REQUIRE(table->rowCount() == 1);
		REQUIRE(dialog.symbol().terminals.size() == 1);
		const QStringList messages = dialog.symbol().problemMessages(grid);
		INFO(messages.join(QStringLiteral(" | ")).toStdString());
		REQUIRE(messages.isEmpty());
		REQUIRE(save->isEnabled());
	}

	SECTION("controle negativo — tirado o nome, o gravar fecha outra vez e diz a falta")
	{
		// Without this the check above would pass on a dialog whose Save
		// button is simply always enabled once a point exists.
		add_point->click();
		REQUIRE(save->isEnabled());

		name->setText(QString());

		const QStringList messages = dialog.symbol().problemMessages(grid);
		INFO(messages.join(QStringLiteral(" | ")).toStdString());
		REQUIRE(messages == QStringList{QString::fromUtf8(no_name_message)});
		REQUIRE_FALSE(save->isEnabled());

		name->setText(QStringLiteral("Rectangle sans queue"));
		REQUIRE(dialog.symbol().problemMessages(grid).isEmpty());
		REQUIRE(save->isEnabled());
	}

	SECTION("a coordenada digitada volta mostrada já grudada na grade principal")
	{
		add_point->click();
		REQUIRE(cellText(table, 0, 0) == QStringLiteral("100 ; 100"));

		table->item(0, 0)->setText(QStringLiteral("43 ; 27"));

		// Both numbers move, and in opposite directions: 43 down to 40 and
		// 27 up to 30. A snap that only ever rounded one way would pass on
		// half of this.
		REQUIRE(cellText(table, 0, 0) == QStringLiteral("40 ; 30"));
		REQUIRE(dialog.symbol().terminals.at(0).position == QPointF(40.0, 30.0));
		INFO(disagreement(table, dialog.symbol()).toStdString());
		REQUIRE(disagreement(table, dialog.symbol()).isEmpty());
		REQUIRE(save->isEnabled());
	}

	SECTION("a coordenada que já está na grade volta como foi digitada")
	{
		// The other side of the same boundary: a point that needs no move
		// must not be moved. A cell that rewrote every value would be caught
		// here and nowhere else.
		add_point->click();
		table->item(0, 0)->setText(QStringLiteral("40 ; 30"));

		REQUIRE(cellText(table, 0, 0) == QStringLiteral("40 ; 30"));
		REQUIRE(dialog.symbol().terminals.at(0).position == QPointF(40.0, 30.0));
	}

	SECTION("controle negativo — a tabela que não passa pelo encaixe mostra 43 e guarda 40")
	{
		add_point->click();
		table->item(0, 0)->setText(QStringLiteral("43 ; 27"));
		REQUIRE(disagreement(table, dialog.symbol()).isEmpty());

		// The condition the queued case forbids, produced on purpose: the
		// cell is written without the dialog being told, which is what a
		// table that displayed the typed text would do.
		const bool blocked = table->blockSignals(true);
		table->item(0, 0)->setText(QStringLiteral("43 ; 27"));
		table->blockSignals(blocked);

		const QString culprit = disagreement(table, dialog.symbol());
		INFO(culprit.toStdString());
		REQUIRE(culprit == QStringLiteral("row 0: table shows \"43 ; 27\", "
						  "symbol holds \"40 ; 30\""));

		// Restored: typed again with the dialog listening, the two agree.
		table->item(0, 0)->setText(QStringLiteral("43 ; 27 "));
		REQUIRE(cellText(table, 0, 0) == QStringLiteral("40 ; 30"));
		REQUIRE(disagreement(table, dialog.symbol()).isEmpty());
	}
}
