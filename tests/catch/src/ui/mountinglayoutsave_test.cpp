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

#include "../../../../sources/location/mountinglayout.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QPointF>
#include <QSizeF>
#include <QString>

/*
	What is mounted on which face, from the project to the file and back.

	The arithmetic of the layout - the millimetre it keeps, what it refuses,
	what a change of enclosure costs - is proved without a project in
	src/mountinglayout_test.cpp, and none of it is repeated here. What that
	suite cannot see is the two lines of QETProject that write the layout
	into the .qet and read it back: delete either of them and every case of
	the other file stays green, while a mounting plate a person spent an
	afternoon filling disappears when the project is closed.

	That is the whole subject of this file, and the reason it is here and not
	there.
*/

namespace
{
	/// The smallest project that opens: one sheet, nothing on it.
	QString projectXml()
	{
		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"import\"/></collection>"
			       "<diagram title=\"Sheet\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements/><inputs/><conductors/>"
			       "</diagram>"
			       "</project>");
	}

	/// A breaker of a real width, mounted at a place nobody would round off.
	MountedItem breaker()
	{
		MountedItem item(QStringLiteral("-Q1"),
				 QPointF(10.5, 20.25),
				 QSizeF(22.5, 85));
		item.part_code = QStringLiteral("A9F74210");
		return item;
	}
}

TEST_CASE("T19 — o que está montado atravessa salvar e reabrir",
	  "[uibench][calepinagem]")
{
	SECTION("a placa volta do arquivo com a peça no mesmo milímetro")
	{
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("layout.qet"));
		INFO(scratch.error().toStdString());
		REQUIRE(scratch.isOpen());
		REQUIRE(scratch->mountingLayout().isEmpty());

		MountingLayout layout;
		const QString face = layout.appendSurface(
					MountingSurface(QStringLiteral("QCM1"),
							QStringLiteral("plate"),
							MountingArea(600, 800)));
		REQUIRE_FALSE(face.isEmpty());
		const QString mounted = layout.mountItem(face, breaker());
		REQUIRE_FALSE(mounted.isEmpty());

		scratch->setMountingLayout(layout);
		REQUIRE(scratch->mountingLayout().itemCount() == 1);

		REQUIRE(scratch.saveAndReopen());

		const MountingLayout reread = scratch->mountingLayout();
		REQUIRE(reread.count() == 1);
		REQUIRE(reread.itemCount() == 1);
		CHECK(reread == layout);
		CHECK(reread.surfaceOfItem(mounted) == face);
		CHECK(reread.surface(face).area.width == 600.0);
		CHECK(reread.surface(face).area.height == 800.0);
		CHECK(reread.surface(face).kind == QString("plate"));
		CHECK(reread.surface(face).location_path == QString("QCM1"));
		CHECK(reread.item(mounted).position.x() == 10.5);
		CHECK(reread.item(mounted).position.y() == 20.25);
		CHECK(reread.item(mounted).size.width() == 22.5);
		CHECK(reread.item(mounted).part_code == QString("A9F74210"));

		const QString written = UiBench::fileContent(scratch.filePath());
		REQUIRE_FALSE(written.isEmpty());
		CHECK(written.contains(MountingLayout::tagName()));
	}

	SECTION("um projeto que nunca abriu a calepinagem sai do arquivo como sempre saiu")
	{
		/*
			The rule that keeps a delivered project the file it always
			was, and the negative control of the case above: without it,
			that one would pass on a QETProject that wrote the node
			whether or not there was anything in it.
		*/
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("layout.qet"));
		REQUIRE(scratch.isOpen());
		REQUIRE(scratch.saveAndReopen());

		const QString written = UiBench::fileContent(scratch.filePath());
		REQUIRE_FALSE(written.isEmpty());
		CHECK_FALSE(written.contains(MountingLayout::tagName()));
		CHECK(scratch->mountingLayout().isEmpty());
	}

	SECTION("trocar o armário no projeto aberto sobrevive ao salvar")
	{
		UiBench::ScratchProject scratch(projectXml(),
						QStringLiteral("layout.qet"));
		REQUIRE(scratch.isOpen());

		MountingLayout layout;
		const QString face = layout.appendSurface(
					MountingSurface(QStringLiteral("QCM1"),
							QStringLiteral("plate"),
							MountingArea(600, 800)));
		const QString mounted = layout.mountItem(face, breaker());
		MountedItem wide(QStringLiteral("-T1"), QPointF(10, 400), QSizeF(450, 200));
		const QString transformer = layout.mountItem(face, wide);
		REQUIRE_FALSE(transformer.isEmpty());

		EnclosureTransferPlan plan;
		QString error;
		REQUIRE(layout.applyArea(face, MountingArea(400, 800), &plan, &error));
		CHECK(error.isEmpty());
		REQUIRE(plan.lossCount() == 1);

		scratch->setMountingLayout(layout);
		REQUIRE(scratch.saveAndReopen());

			//What did not fit is still in the project after the save:
			//the report names it, and nothing removes it.
		const MountingLayout reread = scratch->mountingLayout();
		CHECK(reread.itemCount() == 2);
		CHECK(reread.surface(face).area.width == 400.0);
		CHECK(reread.item(transformer).position.y() == 400.0);
		CHECK(reread.holdsItem(mounted));
	}
}
