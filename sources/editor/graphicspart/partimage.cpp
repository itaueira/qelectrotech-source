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
#include "partimage.h"

#include <QDomDocument>
#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QXmlStreamReader>

namespace
{
	/**
		One length of the root of an SVG, "45mm" or "120", in millimetre,
		0 when it has none or none this knows. A number with no unit is a
		CSS pixel, 96 to the inch.
	*/
	qreal svgLengthMm(const QString &length)
	{
		static const QList<QPair<QString, qreal>> units {
			{QStringLiteral("mm"), 1.0},          {QStringLiteral("cm"), 10.0},
			{QStringLiteral("in"), 25.4},         {QStringLiteral("pt"), 25.4 / 72.0},
			{QStringLiteral("pc"), 25.4 / 6.0},   {QStringLiteral("px"), 25.4 / 96.0}};
		QString text = length.trimmed();
		qreal factor = 25.4 / 96.0;
		for (const auto &unit : units) {
			if (text.endsWith(unit.first)) {
				text.chop(unit.first.size());
				factor = unit.second;
				break;
			}
		}
		bool ok = false;
		const qreal value = text.trimmed().toDouble(&ok);
		return ok && value > 0.0 ? value * factor : 0.0;
	}

	/**
		The size the root of an SVG declares, millimetre, read from its
		text: QSvgRenderer::defaultSize() answers in whole pixels, and a
		part drawn 45 mm wide would come back 44.87.
	*/
	QSizeF svgDeclaredSizeMm(const QByteArray &data)
	{
		QXmlStreamReader reader(data);
		while (!reader.atEnd()) {
			if (reader.readNext() == QXmlStreamReader::StartElement) {
				const QXmlStreamAttributes attributes = reader.attributes();
				return QSizeF(svgLengthMm(attributes.value(QLatin1String("width")).toString()),
					      svgLengthMm(attributes.value(QLatin1String("height")).toString()));
			}
		}
		return QSizeF();
	}
}

/**
	@brief PartImage::PartImage
	@param editor the QETElementEditor of this item
	@param parent parent item
*/
PartImage::PartImage(QETElementEditor *editor, QGraphicsItem *parent) :
	PartRectangle(editor, parent)
{}

/**
	@brief PartImage::~PartImage
*/
PartImage::~PartImage()
{}

/**
	@brief PartImage::toXml
	The rectangle, in millimetre, with the file inside the element: the
	bytes as they were read, in base64, and the format they are in.
	@param xml_document : Xml document to use for create the xml element.
	@return an xml element that describe this image
*/
const QDomElement PartImage::toXml(QDomDocument &xml_document) const
{
	QDomElement xml_element = xml_document.createElement(xmlName());
	const QPointF top_left(sceneTopLeft());
	auto rounded = [](qreal value) { return qRound(value * 100.0) / 100.0; };
	xml_element.setAttribute(QStringLiteral("x"), QString::number(rounded(top_left.x())));
	xml_element.setAttribute(QStringLiteral("y"), QString::number(rounded(top_left.y())));
	xml_element.setAttribute(QStringLiteral("width"),  QString::number(rounded(rect().width())));
	xml_element.setAttribute(QStringLiteral("height"), QString::number(rounded(rect().height())));
	xml_element.setAttribute(QStringLiteral("format"), m_format);
	xml_element.appendChild(xml_document.createTextNode(QString::fromLatin1(m_data.toBase64())));
	return(xml_element);
}

/**
	@brief PartImage::fromXml
	Read back what toXml() wrote. Bytes this program cannot draw are kept
	all the same, and written back as they came: an image is never lost by
	opening and saving a symbol, even one this version cannot show.
	@param qde : Xml element to read.
*/
void PartImage::fromXml(const QDomElement &qde)
{
	setPos(mapFromScene(qde.attribute(QStringLiteral("x"), QStringLiteral("0")).toDouble(),
			    qde.attribute(QStringLiteral("y"), QStringLiteral("0")).toDouble()));
	setRect(QRectF(QPointF(0, 0),
		       QSizeF(qde.attribute(QStringLiteral("width"),  QStringLiteral("0")).toDouble(),
			      qde.attribute(QStringLiteral("height"), QStringLiteral("0")).toDouble())).normalized());

	const QByteArray data = QByteArray::fromBase64(qde.text().trimmed().toLatin1());
	const QString format = qde.attribute(QStringLiteral("format"));
	if (!setFile(data, format)) {
		m_data = data;
		m_format = format;
		m_image = QImage();
		m_svg.reset();
		update();
	}
}

/**
	@brief PartImage::paint
	The file stretched over the rectangle, and where there is nothing to
	draw, a crossed box of the same size, so that the part can still be
	seen, selected and deleted.
*/
void PartImage::paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options)
	Q_UNUSED(widget)

	const QRectF target = rect().normalized();
	painter->save();
	if (isNull())
	{
		QPen pen(QColor(150, 150, 150));
		pen.setCosmetic(true);
		painter->setPen(pen);
		painter->setBrush(QColor(240, 240, 240));
		painter->drawRect(target);
		painter->drawLine(target.topLeft(), target.bottomRight());
		painter->drawLine(target.topRight(), target.bottomLeft());
	}
	else {
		paintDecoded(painter, target, m_image, m_svg.get());
	}
	painter->restore();

	if (isSelected() || m_hovered)
	{
		painter->save();
		QPen frame(isSelected() ? Qt::red : Qt::darkGray);
		frame.setCosmetic(true);
		frame.setStyle(Qt::DashLine);
		painter->setPen(frame);
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(target);
		painter->restore();
	}

	if (isSelected()) {
		drawCross(target.center(), painter);
	}
}

/**
	@brief PartImage::shape
	@return the whole rectangle: an image is grabbed anywhere on it, where
	a rectangle is grabbed by its outline only
*/
QPainterPath PartImage::shape() const
{
	QPainterPath path;
	path.addRect(rect().normalized());
	return path;
}

QPainterPath PartImage::shadowShape() const
{
	return shape();
}

bool PartImage::setFile(const QByteArray &data, const QString &format)
{
	QImage image;
	std::unique_ptr<QSvgRenderer> svg;
	if (!decode(data, format, &image, &svg)) {
		return false;
	}
	m_data = data;
	m_format = format;
	m_image = image;
	m_svg = std::move(svg);
	update();
	return true;
}

QByteArray PartImage::fileData() const
{
	return m_data;
}

QString PartImage::fileFormat() const
{
	return m_format;
}

bool PartImage::isNull() const
{
	return m_image.isNull() && !m_svg;
}

QStringList PartImage::formats()
{
	return {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("svg")};
}

QString PartImage::formatOfFile(const QString &path)
{
	const QString suffix = QFileInfo(path).suffix().toLower();
	if (suffix == QLatin1String("jpeg")) {
		return QStringLiteral("jpg");
	}
	return formats().contains(suffix) ? suffix : QString();
}

QSizeF PartImage::fileSizeMm(const QByteArray &data, const QString &format)
{
	QImage image;
	std::unique_ptr<QSvgRenderer> svg;
	if (!decode(data, format, &image, &svg)) {
		return QSizeF();
	}
	if (svg)
	{
		const QSizeF declared = svgDeclaredSizeMm(data);
		if (!declared.isEmpty()) {
			return declared;
		}
			//No size on the root, or one in a unit this does not read: the
			//view box, in CSS pixels
		return svg->viewBoxF().size() * 25.4 / 96.0;
	}
		//A file that says nothing of its resolution is read at 96 to the
		//inch, the resolution of a screen it was most likely taken from
	const qreal per_metre_x = image.dotsPerMeterX() > 0 ? image.dotsPerMeterX() : 96.0 / 0.0254;
	const qreal per_metre_y = image.dotsPerMeterY() > 0 ? image.dotsPerMeterY() : 96.0 / 0.0254;
	return QSizeF(image.width() * 1000.0 / per_metre_x, image.height() * 1000.0 / per_metre_y);
}

bool PartImage::paintFile(QPainter *painter, const QRectF &target,
			  const QByteArray &data, const QString &format)
{
	QImage image;
	std::unique_ptr<QSvgRenderer> svg;
	if (!decode(data, format, &image, &svg)) {
		return false;
	}
	paintDecoded(painter, target, image, svg.get());
	return true;
}

/**
	@brief PartImage::mouseReleaseEvent
	A click on a rectangle cycles its handles through resizing, mirrored
	resizing and rounding the corners. An image has no corner to round,
	and keeps its handles resizing it.
*/
void PartImage::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	CustomElementGraphicPart::mouseReleaseEvent(event);
}

/**
	@brief PartImage::itemChange
	The turn the rotate command gives a part it does not know is refused:
	the image stays as it is drawn, which is what its file says. Measured
	on the screen on 2026-10-01: turned, it showed sideways while its
	fields kept the old width and height, and a save would have put it
	back upright at another place.
*/
QVariant PartImage::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == ItemRotationChange) {
		return QVariant(QGraphicsItem::rotation());
	}
	return PartRectangle::itemChange(change, value);
}

/**
	@brief PartImage::decode
	The one place that knows how a format is read, so that the editor and
	the plate draw the same file the same way.
*/
bool PartImage::decode(const QByteArray &data, const QString &format,
		       QImage *image, std::unique_ptr<QSvgRenderer> *svg)
{
	if (data.isEmpty()) {
		return false;
	}
	if (format == QLatin1String("svg"))
	{
		auto renderer = std::make_unique<QSvgRenderer>(data);
		if (!renderer->isValid()) {
			return false;
		}
			//Stretched over the rectangle like a raster image, and not
			//fitted inside it - the size of the part is the one asked.
			//That is what render() does with the bounds it is given, and
			//what Qt::IgnoreAspectRatio, the default since the setting
			//exists, keeps it doing.
		*svg = std::move(renderer);
		return true;
	}
	if (format != QLatin1String("png") && format != QLatin1String("jpg")) {
		return false;
	}
	QImage decoded;
	if (!decoded.loadFromData(data, format == QLatin1String("png") ? "PNG" : "JPG")) {
		return false;
	}
	*image = decoded;
	return true;
}

void PartImage::paintDecoded(QPainter *painter, const QRectF &target,
			     const QImage &image, QSvgRenderer *svg)
{
	painter->save();
	painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
	if (svg) {
		svg->render(painter, target);
	}
	else {
		painter->drawImage(target, image);
	}
	painter->restore();
}
