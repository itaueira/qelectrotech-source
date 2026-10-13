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
#include "../../../../sources/configdialog.h"
#include "../../../../sources/ui/configpage/configpage.h"

#include <catch2/catch.hpp>

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QListWidget>
#include <QPixmap>
#include <QString>
#include <QTest>

/*
	The column of pages on the left of a settings window - the project
	properties and the application preferences share it.

	Its items used to be as wide as their own names, so a long name pushed
	its icon aside: "Autonumbering" sat left of "General" in the window the
	draughtsman sent. What is measured here is the cell of each item, which
	is where the list centres the icon and the name: the same axis for all,
	and no cell wider than the column.

	The icons are plain pixmaps and not the program's: the program's icons
	are loaded by the application, which this suite does not start, and a
	null icon takes no room - the case would then measure the names alone.
*/

namespace
{
	class Page : public ConfigPage
	{
		public:
			explicit Page(const QString &title) :
				ConfigPage(nullptr),
				m_title(title)
			{
				QPixmap pixmap(64, 64);
				pixmap.fill(Qt::darkGray);
				m_icon = QIcon(pixmap);
			}
			void applyConf() override {}
			QString title() const override {return m_title;}
			QIcon icon() const override {return m_icon;}

		private:
			QString m_title;
			QIcon m_icon;
	};
}

TEST_CASE("T52 — the pages of a settings window line up on one axis, whatever the length of their names",
	  "[t52][ui]")
{
	ConfigDialog dialog;
		//The font of the screen the window was sent from, where the longer
		//names are wider than the icon: with the small font of this bench
		//every name fits under the icon, and the column lines up by chance
	QFont font = dialog.font();
	font.setPointSizeF(font.pointSizeF() * 2);
	dialog.setFont(font);
	REQUIRE(QFontMetrics(font).horizontalAdvance(QString::fromUtf8("Autonumeração")) > 64);

		//The four of the project properties, a short name and a long one
	for (const char *title : {"Geral", "Nova folha", "Autonumeração", "Plano de bornes"})
		dialog.addPage(new Page(QString::fromUtf8(title)));
	dialog.show();
	REQUIRE(QTest::qWaitForWindowExposed(&dialog));

	auto list = dialog.findChild<QListWidget *>();
	REQUIRE(list != nullptr);
	REQUIRE(list->count() == 4);

	const int axis = list->visualItemRect(list->item(0)).center().x();
	for (int row = 0 ; row < list->count() ; ++row)
	{
		const QRect cell = list->visualItemRect(list->item(row));
		INFO("page " << list->item(row)->text().toStdString()
		     << ": cell from " << cell.left() << " to " << cell.right());
		CHECK(qAbs(cell.center().x() - axis) <= 1);
			//Nothing cut on the right of the column, and room for the icon
		CHECK(cell.right() < list->viewport()->width());
		CHECK(cell.width() >= list->iconSize().width());
	}

	SECTION("a name too long for the column wraps, and keeps the axis")
	{
		dialog.addPage(new Page(QStringLiteral("Uma página de nome bem comprido")));
		QApplication::processEvents();
		REQUIRE(list->count() == 5);
		const QRect cell = list->visualItemRect(list->item(4));
		CHECK(qAbs(cell.center().x() - axis) <= 1);
		CHECK(cell.right() < list->viewport()->width());
	}
}
