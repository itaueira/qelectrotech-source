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
#include "monochromepaintdevice.h"

#include <QGradient>
#include <QPainter>
#include <QPixmap>
#include <QRegion>

/**
	@brief Monochrome::stroke
	@param color : colour of a line, an outline or a text
	@return pure black, with the alpha of color; pure white is kept as it is
*/
QColor Monochrome::stroke(const QColor &color)
{
	if (!color.isValid())
	{
		return color;
	}

	const QRgb rgb = color.rgb();
	if (qRed(rgb) == 255 && qGreen(rgb) == 255 && qBlue(rgb) == 255)
	{
		return color;
	}
	return QColor(0, 0, 0, color.alpha());
}

/**
	@brief Monochrome::fill
	@param color : colour of a filled area
	@return the gray of the luminance of color, with its alpha
*/
QColor Monochrome::fill(const QColor &color)
{
	if (!color.isValid())
	{
		return color;
	}

	const int gray = qGray(color.rgb());
	return QColor(gray, gray, gray, color.alpha());
}

/**
	@brief Monochrome::pen
	A pen that draws nothing is returned untouched: giving it a colour would
	make it draw. Any other pen keeps its width, style, cap and join, and
	strokes with Monochrome::stroke() of its colour - which also turns a
	gradient or textured pen into a plain one.
	@param pen
	@return
*/
QPen Monochrome::pen(const QPen &pen)
{
	if (pen.style() == Qt::NoPen || pen.brush().style() == Qt::NoBrush)
	{
		return pen;
	}

	QPen result(pen);
	result.setColor(stroke(pen.color()));
	return result;
}

/**
	@brief Monochrome::brush
	Every kind of brush keeps its kind - pattern, gradient or texture - and
	only its colours go through Monochrome::fill().
	@param brush
	@return
*/
QBrush Monochrome::brush(const QBrush &brush)
{
	switch (brush.style())
	{
		case Qt::NoBrush:
			return brush;

		case Qt::LinearGradientPattern:
		case Qt::RadialGradientPattern:
		case Qt::ConicalGradientPattern:
		{
			// QGradient is a value class: the copy keeps the type, the
			// geometry, the spread and the coordinate mode.
			QGradient gradient(*brush.gradient());
			QGradientStops stops = gradient.stops();
			for (QGradientStop &stop : stops)
			{
				stop.second = fill(stop.second);
			}
			gradient.setStops(stops);
			QBrush result(gradient);
			result.setTransform(brush.transform());
			return result;
		}

		case Qt::TexturePattern:
		{
			// A bitmap texture paints with the colour of the brush, like a
			// pattern; any other texture is a picture.
			if (brush.texture().depth() == 1)
			{
				QBrush result(brush);
				result.setColor(fill(brush.color()));
				return result;
			}
			QBrush result(image(brush.textureImage()));
			result.setTransform(brush.transform());
			return result;
		}

		default:
		{
			QBrush result(brush);
			result.setColor(fill(brush.color()));
			return result;
		}
	}
}

/**
	@brief Monochrome::image
	@param image
	@return image with every pixel replaced by the gray of its luminance,
	alpha kept. Format_ARGB32, which is not premultiplied, so that the
	luminance is computed on the real colour of a translucent pixel.
*/
QImage Monochrome::image(const QImage &image)
{
	if (image.isNull())
	{
		return image;
	}

	QImage result = image.convertToFormat(QImage::Format_ARGB32);
	for (int y = 0 ; y < result.height() ; ++y)
	{
		QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
		for (int x = 0 ; x < result.width() ; ++x)
		{
			const int gray = qGray(line[x]);
			line[x] = qRgba(gray, gray, gray, qAlpha(line[x]));
		}
	}
	return result;
}

/**
	@brief MonochromePaintEngine::MonochromePaintEngine
	@param target : the painter every call is relayed to. It must be active
	when painting begins, and stay active until it ends.
*/
MonochromePaintEngine::MonochromePaintEngine(QPainter *target) :
	QPaintEngine(QPaintEngine::AllFeatures),
	m_target(target)
{
}

/**
	@brief MonochromePaintEngine::begin
	Saves the target, so that end() gives it back as it was, and keeps the
	clip it already has: the relay may clip further, never wider.
	@param device
	@return false when the target painter is not active
*/
bool MonochromePaintEngine::begin(QPaintDevice *device)
{
	Q_UNUSED(device)

	if (!m_target || !m_target->isActive())
	{
		return false;
	}

	m_target->save();
	m_transform = m_target->worldTransform();

	// clipPath() is in the logical coordinates of the whole transform;
	// mapped through the world part only, it is what the same clip reads
	// under an identity world transform - which is how restoreBaseClip()
	// puts it back.
	m_has_base_clip = m_target->hasClipping();
	m_base_clip = m_has_base_clip
			? m_target->worldTransform().map(m_target->clipPath())
			: QPainterPath();

	setActive(true);
	return true;
}

/**
	@brief MonochromePaintEngine::end
	@return
*/
bool MonochromePaintEngine::end()
{
	if (isActive() && m_target && m_target->isActive())
	{
		m_target->restore();
	}
	setActive(false);
	return true;
}

/**
	@brief MonochromePaintEngine::restoreBaseClip
	Back to the clip the target had when painting began, or to none.
*/
void MonochromePaintEngine::restoreBaseClip()
{
	if (!m_has_base_clip)
	{
		m_target->setClipping(false);
		return;
	}

	m_target->setWorldTransform(QTransform());
	m_target->setClipPath(m_base_clip, Qt::ReplaceClip);
	m_target->setWorldTransform(m_transform);
}

/**
	@brief MonochromePaintEngine::updateState
	Copies what changed onto the target, pen and brush through Monochrome.

	The transform goes first: a clip is expressed in the coordinates in
	force when it was set, and QPainter sends both in the same update when
	it replays the clips of a restore().

	The background brush and mode never arrive here: QPainter keeps them
	from engines of this kind and draws an opaque background itself, with
	the brush - which does arrive, and is converted.
	@param state
*/
void MonochromePaintEngine::updateState(const QPaintEngineState &state)
{
	const QPaintEngine::DirtyFlags flags = state.state();

	if (flags & QPaintEngine::DirtyTransform)
	{
		m_transform = state.transform();
		m_target->setWorldTransform(m_transform);
	}
	if (flags & QPaintEngine::DirtyPen)
	{
		m_target->setPen(Monochrome::pen(state.pen()));
	}
	if (flags & QPaintEngine::DirtyBrush)
	{
		m_target->setBrush(Monochrome::brush(state.brush()));
	}
	if (flags & QPaintEngine::DirtyBrushOrigin)
	{
		m_target->setBrushOrigin(state.brushOrigin());
	}
	if (flags & QPaintEngine::DirtyFont)
	{
		m_target->setFont(state.font());
	}
	if (flags & QPaintEngine::DirtyBackground)
	{
		m_target->setBackground(Monochrome::brush(state.backgroundBrush()));
	}
	if (flags & QPaintEngine::DirtyBackgroundMode)
	{
		m_target->setBackgroundMode(state.backgroundMode());
	}
	if (flags & QPaintEngine::DirtyHints)
	{
		// Exactly these hints: the ones not in the set off, then these on.
		const QPainter::RenderHints hints = state.renderHints();
		m_target->setRenderHints(~hints, false);
		m_target->setRenderHints(hints, true);
	}
	if (flags & QPaintEngine::DirtyCompositionMode)
	{
		m_target->setCompositionMode(state.compositionMode());
	}
	if (flags & QPaintEngine::DirtyOpacity)
	{
		m_target->setOpacity(state.opacity());
	}

	if (flags & QPaintEngine::DirtyClipEnabled)
	{
		if (state.isClipEnabled())
		{
			m_target->setClipping(true);
		}
		else
		{
			restoreBaseClip();
		}
	}

	const bool region_dirty = flags.testFlag(QPaintEngine::DirtyClipRegion);
	const bool path_dirty = flags.testFlag(QPaintEngine::DirtyClipPath);
	if (region_dirty || path_dirty)
	{
		const Qt::ClipOperation operation = state.clipOperation();
		if (operation == Qt::NoClip)
		{
			restoreBaseClip();
		}
		else
		{
			// Both flagged in one update only when QPainter accumulates the
			// changes of a restore(); the one that carries something is the
			// one that was set.
			const bool use_region = region_dirty
					&& (!path_dirty || !state.clipRegion().isEmpty());
			const bool use_path = path_dirty
					&& (!region_dirty || !state.clipPath().isEmpty());

			// A replacing clip replaces what the relay clipped, not the clip
			// the target came with: that one stays under everything.
			Qt::ClipOperation forwarded = operation;
			if (operation == Qt::ReplaceClip)
			{
				restoreBaseClip();
				if (m_has_base_clip)
				{
					forwarded = Qt::IntersectClip;
				}
			}
			if (use_region)
			{
				m_target->setClipRegion(state.clipRegion(), forwarded);
				if (forwarded == Qt::ReplaceClip)
				{
					forwarded = Qt::IntersectClip;
				}
			}
			if (use_path)
			{
				m_target->setClipPath(state.clipPath(), forwarded);
			}
		}
	}
}

void MonochromePaintEngine::drawRects(const QRect *rects, int rect_count)
{
	m_target->drawRects(rects, rect_count);
}

void MonochromePaintEngine::drawRects(const QRectF *rects, int rect_count)
{
	m_target->drawRects(rects, rect_count);
}

void MonochromePaintEngine::drawLines(const QLine *lines, int line_count)
{
	m_target->drawLines(lines, line_count);
}

void MonochromePaintEngine::drawLines(const QLineF *lines, int line_count)
{
	m_target->drawLines(lines, line_count);
}

void MonochromePaintEngine::drawEllipse(const QRectF &rect)
{
	m_target->drawEllipse(rect);
}

void MonochromePaintEngine::drawEllipse(const QRect &rect)
{
	m_target->drawEllipse(rect);
}

void MonochromePaintEngine::drawPath(const QPainterPath &path)
{
	m_target->drawPath(path);
}

void MonochromePaintEngine::drawPoints(const QPointF *points, int point_count)
{
	m_target->drawPoints(points, point_count);
}

void MonochromePaintEngine::drawPoints(const QPoint *points, int point_count)
{
	m_target->drawPoints(points, point_count);
}

void MonochromePaintEngine::drawPolygon(const QPointF *points, int point_count,
										PolygonDrawMode mode)
{
	switch (mode)
	{
		case QPaintEngine::OddEvenMode:
			m_target->drawPolygon(points, point_count, Qt::OddEvenFill);
			break;
		case QPaintEngine::WindingMode:
			m_target->drawPolygon(points, point_count, Qt::WindingFill);
			break;
		case QPaintEngine::ConvexMode:
			m_target->drawConvexPolygon(points, point_count);
			break;
		case QPaintEngine::PolylineMode:
			m_target->drawPolyline(points, point_count);
			break;
	}
}

void MonochromePaintEngine::drawPolygon(const QPoint *points, int point_count,
										PolygonDrawMode mode)
{
	switch (mode)
	{
		case QPaintEngine::OddEvenMode:
			m_target->drawPolygon(points, point_count, Qt::OddEvenFill);
			break;
		case QPaintEngine::WindingMode:
			m_target->drawPolygon(points, point_count, Qt::WindingFill);
			break;
		case QPaintEngine::ConvexMode:
			m_target->drawConvexPolygon(points, point_count);
			break;
		case QPaintEngine::PolylineMode:
			m_target->drawPolyline(points, point_count);
			break;
	}
}

/**
	@brief MonochromePaintEngine::drawPixmap
	A bitmap paints with the colour of the pen, which is already converted;
	any other pixmap is a picture and goes to the target in shades of gray.
*/
void MonochromePaintEngine::drawPixmap(const QRectF &rect, const QPixmap &pixmap,
									   const QRectF &source_rect)
{
	if (pixmap.depth() == 1)
	{
		m_target->drawPixmap(rect, pixmap, source_rect);
		return;
	}
	m_target->drawImage(rect, Monochrome::image(pixmap.toImage()), source_rect);
}

void MonochromePaintEngine::drawTiledPixmap(const QRectF &rect, const QPixmap &pixmap,
											const QPointF &offset)
{
	if (pixmap.depth() == 1)
	{
		m_target->drawTiledPixmap(rect, pixmap, offset);
		return;
	}
	m_target->drawTiledPixmap(rect,
							  QPixmap::fromImage(Monochrome::image(pixmap.toImage())),
							  offset);
}

void MonochromePaintEngine::drawImage(const QRectF &rect, const QImage &image,
									  const QRectF &source_rect,
									  Qt::ImageConversionFlags flags)
{
	m_target->drawImage(rect, Monochrome::image(image), source_rect, flags);
}

/**
	@brief MonochromePaintEngine::drawTextItem
	The text item goes to the target whole - glyphs, font and characters -
	so that a PDF target writes it as text, searchable, and not as curves.
	Its colour is the pen's, already converted.

	An underline or a strike-out is drawn twice, once by the painter in
	front of this engine and once by the target, at the same place and in
	the same black: no visible difference, and not worth reaching into
	QTextItem's private side to avoid.
*/
void MonochromePaintEngine::drawTextItem(const QPointF &position, const QTextItem &text_item)
{
	m_target->drawTextItem(position, text_item);
}

/**
	@brief MonochromePaintDevice::MonochromePaintDevice
	@param target : the active painter the drawing is relayed to
*/
MonochromePaintDevice::MonochromePaintDevice(QPainter *target) :
	QPaintDevice(),
	m_target(target),
	m_engine(new MonochromePaintEngine(target))
{
}

MonochromePaintDevice::~MonochromePaintDevice()
{
	delete m_engine;
}

QPaintEngine *MonochromePaintDevice::paintEngine() const
{
	return m_engine;
}

/**
	@brief MonochromePaintDevice::metric
	The metrics of the target painter's device, so that a font is sized for
	the resolution it will be drawn at, and a null target rectangle of
	QGraphicsScene::render() means the same page.

	The device pixel ratio is the exception, and is 1: everything drawn
	here reaches the target painter in its logical coordinates, and that
	painter applies its own device's ratio. Reporting it here too would
	apply it twice.
	@param metric
	@return
*/
int MonochromePaintDevice::metric(PaintDeviceMetric metric) const
{
	const QPaintDevice *device = m_target ? m_target->device() : nullptr;
	if (!device)
	{
		return QPaintDevice::metric(metric);
	}

	switch (metric)
	{
		case PdmWidth:
			return device->width();
		case PdmHeight:
			return device->height();
		case PdmWidthMM:
			return device->widthMM();
		case PdmHeightMM:
			return device->heightMM();
		case PdmNumColors:
			return device->colorCount();
		case PdmDepth:
			return device->depth();
		case PdmDpiX:
			return device->logicalDpiX();
		case PdmDpiY:
			return device->logicalDpiY();
		case PdmPhysicalDpiX:
			return device->physicalDpiX();
		case PdmPhysicalDpiY:
			return device->physicalDpiY();
		case PdmDevicePixelRatio:
			return 1;
		case PdmDevicePixelRatioScaled:
			return int(1 * QPaintDevice::devicePixelRatioFScale());
		default:
			return QPaintDevice::metric(metric);
	}
}
