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
#ifndef MONOCHROMEPAINTDEVICE_H
#define MONOCHROMEPAINTDEVICE_H

#include <QBrush>
#include <QColor>
#include <QImage>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainterPath>
#include <QPen>
#include <QTransform>

class QPainter;

/**
	The colours of a black and white output (ExportProperties::monochrome).

	The rule goes by the kind of thing drawn, never by object:
	  - a stroke - line, outline, text - becomes pure black, because a
	    light colour on a thin line is what disappears on a monochrome
	    printer. Pure white is the one exception: a white stroke on paper is
	    no ink by construction, and is used to cut a gap through a black
	    shape; turning it black would print a line nobody sees on screen;
	  - a fill becomes the gray of its own luminance, so that a white mask
	    stays white, a black fill stays black and a coloured area keeps its
	    shape instead of turning into a black blot that hides what is drawn
	    over it;
	  - a raster picture - a logo, a photo - becomes the gray of the
	    luminance of each of its pixels, for the same reason: in pure black
	    it would be a stain.

	The alpha of every colour is kept. The luminance is qGray(): one formula
	for fills and pictures, so that a coloured fill and the same colour in a
	picture print the same gray.
*/
namespace Monochrome
{
	QColor stroke(const QColor &color);
	QColor fill(const QColor &color);
	QPen pen(const QPen &pen);
	QBrush brush(const QBrush &brush);
	QImage image(const QImage &image);
}

/**
	@brief The MonochromePaintEngine class
	Relays every drawing call to another, already active QPainter, with the
	colours of Monochrome applied on the way.

	It exists so that a black and white output is one switch for every way a
	folio is drawn out of the program - a printer, a PDF, an image or an
	SVG - without any item having to know about it. They all go through
	Diagram::render(), and that is where the relay is put in, when the folio
	asks for it (Diagram::setMonochrome(), set by applyProperties()).

	What is drawn is not changed, only its colour: a text reaches the target
	as a text item, so a PDF stays searchable; a curve stays a curve; a
	picture stays a picture, in shades of gray.

	The state the painter in front of this engine holds - pen, brush,
	transform, clip... - is copied onto the target painter as it changes.
	The target is saved when painting begins and restored when it ends, so
	the caller gets its painter back as it gave it. A clip the target
	already had is kept under everything the relay clips.
*/
class MonochromePaintEngine : public QPaintEngine
{
	public:
		explicit MonochromePaintEngine(QPainter *target);

		bool begin(QPaintDevice *device) override;
		bool end() override;
		void updateState(const QPaintEngineState &state) override;

		void drawRects(const QRect *rects, int rect_count) override;
		void drawRects(const QRectF *rects, int rect_count) override;
		void drawLines(const QLine *lines, int line_count) override;
		void drawLines(const QLineF *lines, int line_count) override;
		void drawEllipse(const QRectF &rect) override;
		void drawEllipse(const QRect &rect) override;
		void drawPath(const QPainterPath &path) override;
		void drawPoints(const QPointF *points, int point_count) override;
		void drawPoints(const QPoint *points, int point_count) override;
		void drawPolygon(const QPointF *points, int point_count, PolygonDrawMode mode) override;
		void drawPolygon(const QPoint *points, int point_count, PolygonDrawMode mode) override;
		void drawPixmap(const QRectF &rect, const QPixmap &pixmap, const QRectF &source_rect) override;
		void drawTiledPixmap(const QRectF &rect, const QPixmap &pixmap, const QPointF &offset) override;
		void drawImage(const QRectF &rect, const QImage &image, const QRectF &source_rect,
					   Qt::ImageConversionFlags flags = Qt::AutoColor) override;
		void drawTextItem(const QPointF &position, const QTextItem &text_item) override;

		Type type() const override { return QPaintEngine::User; }

	private:
		void restoreBaseClip();

		QPainter *m_target;
		/// The world transform last handed to the target.
		QTransform m_transform;
		/// Whether the target was clipping when painting began.
		bool m_has_base_clip = false;
		/// That clip, in the coordinates of an identity world transform.
		QPainterPath m_base_clip;
};

/**
	@brief The MonochromePaintDevice class
	The device a QPainter is opened on to draw through MonochromePaintEngine.
	It measures like the device of the target painter, so that fonts are
	sized for it and an empty target rectangle means the same thing.
	The target painter must stay active for as long as this device is
	painted on.
*/
class MonochromePaintDevice : public QPaintDevice
{
	public:
		explicit MonochromePaintDevice(QPainter *target);
		~MonochromePaintDevice() override;

		QPaintEngine *paintEngine() const override;

	protected:
		int metric(PaintDeviceMetric metric) const override;

	private:
		Q_DISABLE_COPY(MonochromePaintDevice)

		QPainter *m_target;
		MonochromePaintEngine *m_engine;
};

#endif // MONOCHROMEPAINTDEVICE_H
