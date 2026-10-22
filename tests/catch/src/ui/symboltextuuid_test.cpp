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

#include "../../../../sources/editor/elementscene.h"
#include "../../../../sources/editor/graphicspart/partdynamictextfield.h"
#include "../../../../sources/editor/ui/qetelementeditor.h"

#include <catch2/catch.hpp>

#include <QDomDocument>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>

/*
	Each text of a symbol has a uuid of its own (T82). The uuid is what
	links the texts of the elements placed on folios to the texts of their
	symbol; a text copied and pasted in the symbol editor used to keep the
	uuid of the one copied, and the texts sharing one swapped their contents
	on every placed element. The editor now gives a pasted text a uuid of
	its own, and mends a symbol opened with texts sharing one.
*/

namespace
{
	const QString shared_uuid = QStringLiteral("{82828282-0000-4000-8000-000000000001}");

	QString textXml(const QString &info, int y)
	{
		return QStringLiteral(
			"<dynamic_text text_from=\"ElementInfo\" x=\"-40\" y=\"%2\" z=\"3\" rotation=\"0\""
			" frame=\"false\" text_width=\"-1\" font=\"Sans Serif,5,-1,5,50,0,0,0,0,0\""
			" uuid=\"%3\" Halignment=\"AlignLeft\" Valignment=\"AlignTop\""
			" keep_visual_rotation=\"false\" rotation_point_center=\"false\">"
			"<text/><info_name>%1</info_name></dynamic_text>")
			.arg(info).arg(y).arg(shared_uuid);
	}

		/// A symbol whose function, description and label share one uuid
	QString symbolXml()
	{
		return QStringLiteral(
			"<definition type=\"element\" version=\"0.80\" width=\"20\" height=\"40\""
			" hotspot_x=\"10\" hotspot_y=\"20\" link_type=\"simple\">"
			"<names><name lang=\"en\">valve</name></names>"
			"<elementInformations/>"
			"<description>"
			"%1%2%3"
			"<rect x=\"-8\" y=\"-10\" width=\"16\" height=\"20\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
			"<terminal x=\"0\" y=\"-20\" orientation=\"n\"/>"
			"</description>"
			"</definition>")
			.arg(textXml(QStringLiteral("function"), 2),
			     textXml(QStringLiteral("description"), 10),
			     textXml(QStringLiteral("label"), -19));
	}

	QList<QUuid> textUuids(ElementScene *scene)
	{
		QList<QUuid> uuids;
		for (QGraphicsItem *item : scene->items()) {
			if (auto *text = dynamic_cast<PartDynamicTextField *>(item)) {
				uuids << text->uuid();
			}
		}
		return uuids;
	}
}

TEST_CASE("CU-82.3 — A symbol opened with texts sharing a uuid gets one per text",
	  "[t82][elementeditor]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString path = dir.filePath(QStringLiteral("valve.elmt"));
	{
		QFile file(path);
		REQUIRE(file.open(QIODevice::WriteOnly));
		file.write(symbolXml().toUtf8());
	}

	QETElementEditor editor;
	editor.fromFile(path);
	ElementScene *scene = editor.elementScene();
	REQUIRE(scene != nullptr);
	const QList<QUuid> opened = textUuids(scene);
	REQUIRE(opened.size() == 3);
	CHECK(QSet<QUuid>(opened.cbegin(), opened.cend()).size() == 3);

	SECTION("a text pasted gets a uuid no other text has")
	{
		QDomDocument copy;
		REQUIRE(copy.setContent(QStringLiteral(
			"<definition type=\"element\" version=\"0.80\" width=\"20\" height=\"20\""
			" hotspot_x=\"10\" hotspot_y=\"10\" link_type=\"simple\">"
			"<description>%1</description></definition>")
			.arg(textXml(QStringLiteral("function"), 30).replace(shared_uuid, opened.first().toString()))));
		ElementContent pasted;
		scene->fromXml(copy, QPointF(20, 20), false, &pasted);
		REQUIRE(pasted.size() == 1);
		const QList<QUuid> after = textUuids(scene);
		REQUIRE(after.size() == 4);
		CHECK(QSet<QUuid>(after.cbegin(), after.cend()).size() == 4);
	}

	SECTION("saved, each text keeps a uuid of its own")
	{
		const QDomDocument saved = scene->toXml();
		QSet<QString> uuids;
		int texts = 0;
		for (QDomElement text = saved.documentElement().firstChildElement(QStringLiteral("description"))
			 .firstChildElement(QStringLiteral("dynamic_text"));
		     !text.isNull() ; text = text.nextSiblingElement(QStringLiteral("dynamic_text")))
		{
			++texts;
			uuids.insert(text.attribute(QStringLiteral("uuid")));
		}
		CHECK(texts == 3);
		CHECK(uuids.size() == 3);
	}
}
