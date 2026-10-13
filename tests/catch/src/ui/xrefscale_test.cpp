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
#include "../../../../sources/properties/xrefproperties.h"
#include "../../../../sources/qetgraphicsitem/crossrefitem.h"
#include "../../../../sources/qetgraphicsitem/dynamicelementtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetgraphicsitem/masterelement.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/xrefpropertieswidget.h"

#include <catch2/catch.hpp>

#include <QComboBox>
#include <QDomDocument>
#include <QGraphicsTextItem>
#include <QImage>
#include <QPainter>
#include <QSpinBox>
#include <QString>
#include <QtMath>

/*
	The size of the cross reference, chosen per type of master in the
	properties of the project.

	The cross is drawn at its normal size by code that rebuilds its geometry
	while it paints, so the size is applied on the way out: to the drawing,
	to the box and the shape, and to the clickable contacts and the text
	rects the rest of the program reads. These cases check the four
	together, and the drawing by its ink - a box that shrinks around a
	drawing that does not would pass every other check.

	Labelled T51 and not CU-51.x: the use cases are walked on the folio of
	the draughtsman, with the settings page of the project.
*/

namespace
{
	const char *coil_uuid = "{7e510001-0000-4000-8000-000000000001}";
	const char *contact_uuids[] = {
		"{7e510001-0000-4000-8000-000000000011}",
		"{7e510001-0000-4000-8000-000000000012}"};

	QString instance(const QString &type, const QString &uuid, int x, int y,
			 int first_terminal_id, const QString &label,
			 const QStringList &links)
	{
		QString links_xml;
		for (const QString &link : links)
			links_xml += QStringLiteral("<link_uuid uuid=\"%1\"/>").arg(link);

		return QStringLiteral(
			       "<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
			       " freezeLabel=\"false\" orientation=\"0\""
			       " type=\"embed://bench/%3\" uuid=\"%4\">"
			       "<terminals>"
			       "<terminal x=\"0\" y=\"-10\" orientation=\"0\" id=\"%5\"/>"
			       "<terminal x=\"0\" y=\"10\" orientation=\"2\" id=\"%6\"/>"
			       "</terminals>"
			       "<inputs/>"
			       "<elementInformations>"
			       "<elementInformation show=\"1\" name=\"label\">%7"
			       "</elementInformation>"
			       "</elementInformations>"
			       "<links_uuids>%8</links_uuids>"
			       "<dynamic_texts/><texts_groups/>"
			       "</element>")
			.arg(x).arg(y).arg(type, uuid)
			.arg(first_terminal_id).arg(first_terminal_id + 1)
			.arg(label, links_xml);
	}

	QString definition(const QString &name, const QString &link_type,
			   const QString &kind)
	{
		return QStringLiteral(
			       "<element name=\"%1\">"
			       "<definition type=\"element\" version=\"0.80\""
			       " width=\"20\" height=\"30\""
			       " hotspot_x=\"10\" hotspot_y=\"15\""
			       " orientation=\"dnnn\" link_type=\"%2\">"
			       "<names><name lang=\"en\">%1</name></names>"
			       "<kindInformations>%3</kindInformations>"
			       "<description>"
			       "<rect x=\"-5\" y=\"-10\" width=\"10\" height=\"20\""
			       " antialias=\"false\""
			       " style=\"line-style:normal;line-weight:normal;"
			       "filling:none;color:black\"/>"
			       "<terminal x=\"0\" y=\"-10\" orientation=\"n\" name=\"1\"/>"
			       "<terminal x=\"0\" y=\"10\" orientation=\"s\" name=\"2\"/>"
			       "</description>"
			       "</definition>"
			       "</element>")
			.arg(name, link_type, kind);
	}

	QString fixtureXml()
	{
		QStringList contacts;
		for (const char *uuid : contact_uuids)
			contacts << QLatin1String(uuid);

		QString elements = instance(QStringLiteral("coil.elmt"),
					    QLatin1String(coil_uuid), 200, 200, 0,
					    QStringLiteral("K1"), contacts);
		int x = 400;
		int id = 10;
		for (const QString &uuid : contacts)
		{
			elements += instance(QStringLiteral("contact.elmt"), uuid,
					     x, 200, id, QString(),
					     {QLatin1String(coil_uuid)});
			x += 100;
			id += 2;
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection><category name=\"bench\">%1%2</category></collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%3</elements>"
			       "<inputs/>"
			       "<conductors/>"
			       "</diagram>"
			       "</project>")
			.arg(definition(QStringLiteral("coil.elmt"), QStringLiteral("master"),
					QStringLiteral("<kindInformation name=\"type\">coil"
						       "</kindInformation>")),
			     definition(QStringLiteral("contact.elmt"), QStringLiteral("slave"),
					QStringLiteral("<kindInformation name=\"type\">simple"
						       "</kindInformation>"
						       "<kindInformation name=\"state\">NO"
						       "</kindInformation>")),
			     elements);
	}

	MasterElement *coil(Diagram *sheet)
	{
		if (!sheet)
			return nullptr;
		for (Element *element : sheet->elements())
			if (element->uuid() == QUuid(QLatin1String(coil_uuid)))
				return qobject_cast<MasterElement *>(element);
		return nullptr;
	}

	CrossRefItem *xrefOf(Element *master)
	{
		if (!master)
			return nullptr;
		QList<QGraphicsItem *> todo = master->childItems();
		while (!todo.isEmpty())
		{
			QGraphicsItem *item = todo.takeFirst();
			if (auto xref = qgraphicsitem_cast<CrossRefItem *>(item))
				return xref;
			todo += item->childItems();
		}
		return nullptr;
	}

	DynamicElementTextItem *addLabelText(Element *element)
	{
		auto text = new DynamicElementTextItem(element);
		text->setTextFrom(DynamicElementTextItem::ElementInfo);
		text->setInfoName(QStringLiteral("label"));
		element->addDynamicTextItem(text);
		return text;
	}

	void setCoil(QETProject *project, int scale,
		     XRefProperties::SnapTo snap = XRefProperties::Bottom)
	{
		XRefProperties xrp = project->defaultXRefProperties(QStringLiteral("coil"));
		xrp.setSnapTo(snap);
		xrp.setScale(scale);
		project->setDefaultXRefProperties(QStringLiteral("coil"), xrp);
	}

	struct Bench
	{
		UiBench::ScratchProject project{fixtureXml(), QStringLiteral("xrefscale.qet")};
		DynamicElementTextItem *contact_label = nullptr;

		bool prepare()
		{
			if (!project.isOpen())
				return false;
			setCoil(project.project(), 100);
			for (Element *element : project.diagram(0)->elements())
				if (element->linkType() == Element::Slave && !contact_label)
					contact_label = addLabelText(element);
			return contact_label != nullptr;
		}
	};

	/// The box of what @a xref really draws, measured in its ink, in item
	/// coordinates. Drawn four times bigger so a half-pixel is not lost.
	QRectF inkOf(CrossRefItem *xref)
	{
		const QRectF box = xref->boundingRect();
		const qreal zoom = 4;
		QImage image(qCeil((box.width() + 8) * zoom), qCeil((box.height() + 8) * zoom),
			     QImage::Format_ARGB32);
		image.fill(Qt::white);
		{
			QPainter painter(&image);
			painter.scale(zoom, zoom);
			painter.translate(-box.topLeft() + QPointF(4, 4));
			xref->paintForExport(&painter);
		}

		int left = image.width(), top = image.height(), right = -1, bottom = -1;
		for (int y = 0 ; y < image.height() ; ++y)
			for (int x = 0 ; x < image.width() ; ++x)
				if (qGray(image.pixel(x, y)) < 200)
				{
					left = qMin(left, x);
					right = qMax(right, x);
					top = qMin(top, y);
					bottom = qMax(bottom, y);
				}
		if (right < 0)
			return QRectF();
		return QRectF(left / zoom, top / zoom,
			      (right - left + 1) / zoom, (bottom - top + 1) / zoom);
	}

	bool near(qreal a, qreal b, qreal tolerance) {
		return qAbs(a - b) <= tolerance;
	}
}

TEST_CASE("T51 — the size shrinks the drawing of the cross, its box and its clickable contacts together",
	  "[t51][xref]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	QETProject *project = bench.project.project();
	MasterElement *k1 = coil(bench.project.diagram(0));
	REQUIRE(k1 != nullptr);
	CrossRefItem *xref = xrefOf(k1);
	REQUIRE(xref != nullptr);
	REQUIRE(xref->isVisible());

	const QRectF box_at_100 = xref->boundingRect();
	const QRectF ink_at_100 = inkOf(xref);
	const QMultiMap<Element *, QRectF> contacts_at_100 = xref->hoveredContactsMap();
	REQUIRE_FALSE(box_at_100.isEmpty());
	REQUIRE_FALSE(ink_at_100.isEmpty());
	REQUIRE_FALSE(contacts_at_100.isEmpty());

	setCoil(project, 50);

	SECTION("the box and the shape are half")
	{
		const QRectF box = xref->boundingRect();
		CHECK(near(box.width(), box_at_100.width() / 2, 0.01));
		CHECK(near(box.height(), box_at_100.height() / 2, 0.01));
		CHECK(near(xref->shape().boundingRect().width(),
			   box_at_100.width() / 2, 0.5));
	}

	SECTION("the drawing itself is half, and not only its box")
	{
		const QRectF ink = inkOf(xref);
		REQUIRE_FALSE(ink.isEmpty());
		INFO("ink 100: " << ink_at_100.width() << " x " << ink_at_100.height()
		     << " at " << ink_at_100.left() << "," << ink_at_100.top()
		     << " | ink 50: " << ink.width() << " x " << ink.height()
		     << " at " << ink.left() << "," << ink.top()
		     << " | box 100: " << box_at_100.width() << " x " << box_at_100.height()
		     << " | box 50: " << xref->boundingRect().width() << " x " << xref->boundingRect().height());
			//The pen is shrunk too, so the ratio is close to a half but
			//not exactly: a pixel of the bigger image either way
		CHECK(near(ink.width() / ink_at_100.width(), 0.5, 0.06));
		CHECK(near(ink.height() / ink_at_100.height(), 0.5, 0.06));
	}

	SECTION("the clickable contacts are half, for the mouse and the PDF links")
	{
		const QMultiMap<Element *, QRectF> contacts = xref->hoveredContactsMap();
		REQUIRE(contacts.size() == contacts_at_100.size());
			//Same contacts, same order: each rect is the old one, halved
		auto now = contacts.cbegin();
		for (auto before = contacts_at_100.cbegin() ;
		     before != contacts_at_100.cend() ; ++before, ++now)
		{
			CHECK(now.key() == before.key());
			CHECK(near(now.value().left(), before.value().left() / 2, 0.01));
			CHECK(near(now.value().top(), before.value().top() / 2, 0.01));
			CHECK(near(now.value().width(), before.value().width() / 2, 0.01));
			CHECK(near(now.value().height(), before.value().height() / 2, 0.01));
		}
	}

	SECTION("still centred under its element, at the bottom of the folio")
	{
		CHECK(near(xref->sceneBoundingRect().center().x(),
			   k1->sceneBoundingRect().center().x(), 0.5));
	}

	SECTION("and under the label, with the offset the user gave it (T50)")
	{
		setCoil(project, 50, XRefProperties::Label);
		DynamicElementTextItem *label = addLabelText(k1);
		CrossRefItem *under_label = xrefOf(k1);
		REQUIRE(under_label != nullptr);
		REQUIRE(under_label->parentItem() == label);
		CHECK(near(under_label->boundingRect().width(), box_at_100.width() / 2, 1));

		const QPointF automatic = under_label->pos();
		k1->setXrefOffset(QPointF(30, 10));
		CHECK(under_label->pos() == automatic + QPointF(30, 10));
	}

	SECTION("back to 100%, back to the normal size")
	{
		setCoil(project, 100);
		CHECK(xref->boundingRect() == box_at_100);
		CHECK(xref->hoveredContactsMap().value(contacts_at_100.firstKey())
		      == contacts_at_100.value(contacts_at_100.firstKey()));
	}
}

TEST_CASE("T51 — the folio position text of a contact shrinks with the size of its master's type",
	  "[t51][xref]")
{
	Bench bench;
	INFO(bench.project.error().toStdString());
	REQUIRE(bench.prepare());

	QGraphicsTextItem *position = bench.contact_label->slaveXrefItem();
	REQUIRE(position != nullptr);
	REQUIRE(near(position->font().pointSizeF(), 5, 0.01));
	const qreal width_at_100 = position->boundingRect().width();

	setCoil(bench.project.project(), 60);
	CHECK(near(position->font().pointSizeF(), 3, 0.01));
	CHECK(position->boundingRect().width() < width_at_100);

		//Another type does not touch it
	XRefProperties protection = bench.project->defaultXRefProperties(QStringLiteral("protection"));
	protection.setScale(150);
	bench.project->setDefaultXRefProperties(QStringLiteral("protection"), protection);
	CHECK(near(position->font().pointSizeF(), 3, 0.01));
}

TEST_CASE("T51 — the size is kept per type, saved only when it is not the normal one, and bounded",
	  "[t51][xref][save]")
{
	SECTION("the normal size writes nothing, another one writes itself, and reads back")
	{
		QDomDocument document;
		XRefProperties normal;
		CHECK(normal.scale() == 100);
		CHECK_FALSE(normal.toXml(document).hasAttribute(QStringLiteral("scale")));

		XRefProperties smaller;
		smaller.setScale(60);
		const QDomElement xml = smaller.toXml(document);
		CHECK(xml.attribute(QStringLiteral("scale")) == QStringLiteral("60"));

		XRefProperties read;
		read.fromXml(xml);
		CHECK(read.scale() == 60);

			//A project written before the size existed reads the normal one
		XRefProperties old;
		old.fromXml(normal.toXml(document));
		CHECK(old.scale() == 100);
			//Read back the same way, the two differ by the size only: the
			//comparison sees it, or changing only the size would not reach
			//the cross references. (Compared with a property set never
			//read, they differ by the prefixes too, which fromXml() fills
			//with empty texts.)
		CHECK(read != old);
		XRefProperties old_resized = old;
		old_resized.setScale(60);
		CHECK(read == old_resized);
	}

	SECTION("bounded to the range the page offers")
	{
		XRefProperties xrp;
		xrp.setScale(10);
		CHECK(xrp.scale() == XRefProperties::minScale());
		xrp.setScale(500);
		CHECK(xrp.scale() == XRefProperties::maxScale());
		xrp.setScale(0);
		CHECK(xrp.scale() == 100);
	}

	SECTION("saved with the project")
	{
		Bench bench;
		INFO(bench.project.error().toStdString());
		REQUIRE(bench.prepare());
		setCoil(bench.project.project(), 60);
		const QRectF box = xrefOf(coil(bench.project.diagram(0)))->boundingRect();

		REQUIRE(bench.project.saveAndReopen());
		INFO(bench.project.error().toStdString());
		CHECK(bench.project->defaultXRefProperties(QStringLiteral("coil")).scale() == 60);
		CrossRefItem *xref = xrefOf(coil(bench.project.diagram(0)));
		REQUIRE(xref != nullptr);
		CHECK(near(xref->boundingRect().width(), box.width(), 0.01));
	}
}

TEST_CASE("T51 — the settings page offers the size per type, and none for the PLC",
	  "[t51][xref]")
{
	XRefPropertiesWidget page(XRefProperties::defaultProperties());
	auto type = page.findChild<QComboBox *>(QStringLiteral("m_type_cb"));
	auto size = page.findChild<QSpinBox *>(QStringLiteral("m_scale_sb"));
	REQUIRE(type != nullptr);
	REQUIRE(size != nullptr);
	CHECK(size->minimum() == XRefProperties::minScale());
	CHECK(size->maximum() == XRefProperties::maxScale());

	type->setCurrentIndex(type->findData(QStringLiteral("coil")));
	CHECK(size->isEnabled());
	size->setValue(60);
	CHECK(page.properties().value(QStringLiteral("coil")).scale() == 60);

		//Another type keeps its own
	type->setCurrentIndex(type->findData(QStringLiteral("protection")));
	CHECK(size->value() == page.properties().value(QStringLiteral("protection")).scale());
	type->setCurrentIndex(type->findData(QStringLiteral("coil")));
	CHECK(size->value() == 60);

		//A PLC table is drawn by the element and keeps its size
	type->setCurrentIndex(type->findData(QStringLiteral("plc")));
	CHECK_FALSE(size->isEnabled());
}
