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
#include "qetstyle.h"

#include <QApplication>
#include <QImage>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QStyleOption>
#include <QWidget>

#include "qetpalette.h"

namespace {
	/**
		Opacity of a disabled icon on a dark palette. The dark theme's ink
		is (220,220,220); at 0.65 it shows as about (153,153,153) on the
		Windows dark tool bar (30,30,30): 5.8:1, against 12:1 enabled,
		about the grey of disabled text there. Any icon that reads at 3:1
		enabled still reads at 1.8:1 disabled, on that tool bar and on
		Fusion's dark window (53,53,53): pure red is the worst case, at
		1.84 and 1.81, and less opacity drops it below. Above one half on
		purpose too: a fully opaque pixel stays more than half opaque
		(165), so a measure that only counts such pixels still sees it.
	*/
	const qreal kDisabledOpacity = 0.65;

	/// A disabled control on a dark palette: the case drawDisabledFrame() is for.
	bool disabledOnDark(const QStyleOption *option)
	{
		return option && !(option->state & QStyle::State_Enabled)
		       && QET::Palette::isDark(option->palette);
	}

	/**
		The outline of a disabled input box, over whatever the base style
		drew: a one pixel rounded rectangle on the outermost pixels of
		rect, where Fusion draws its own frame.
	*/
	void drawDisabledFrame(QPainter *painter, const QRect &rect, const QPalette &palette)
	{
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing, true);
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(QETStyle::disabledFrameColor(palette), 1));
		painter->drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
		painter->restore();
	}
}

/**
	@brief QETStyle::QETStyle
	@param base the style to wrap; this object takes ownership of it.
*/
QETStyle::QETStyle(QStyle *base) :
	QProxyStyle(base)
{
	setObjectName(base ? base->objectName() : QString());
}

/**
	@brief QETStyle::isLineArt
	The same rule misc/make_icon_themes.py uses to pick the icons that
	get a dark variant: an icon is line art when fewer than 20% of its
	visible pixels are saturated.
*/
bool QETStyle::isLineArt(const QImage &image)
{
	const QImage source = image.convertToFormat(QImage::Format_ARGB32);
	int visible = 0;
	int saturated = 0;
	for (int y = 0; y < source.height(); ++y)
	{
		const QRgb *line = reinterpret_cast<const QRgb *>(source.constScanLine(y));
		for (int x = 0; x < source.width(); ++x)
		{
			const QRgb pixel = line[x];
			if (qAlpha(pixel) <= 64)
				continue;
			++visible;
			const QColor color(pixel);
			if (color.hslSaturationF() > 0.25 && color.value() > 60)
				++saturated;
		}
	}
	return visible > 0 && saturated < visible * 0.20;
}

/**
	@brief QETStyle::hoverColor
	The palette's highlight color is the accent users already know from
	selections, moved away from the hovered button face until it reaches
	3:1 (WCAG 1.4.11) against the Light role: Fusion paints a hovered
	auto-raise button with a gradient that runs from Button up to about
	that color, and the icon has to read on the lightest part of it. On a
	dark face the accent is lightened, a step at a time; on a light face it
	is darkened, which keeps a pale accent (macOS's green or yellow
	selection color, which comes with black selection text) from being
	pushed to white. Should twenty steps not get there, the button text
	color serves, which reads on the face by construction.
*/
QColor QETStyle::hoverColor(const QPalette &palette)
{
	const QColor face = palette.color(QPalette::Active, QPalette::Light);
	const bool light_face = face.lightnessF() > 0.5;
	QColor ink = palette.color(QPalette::Active, QPalette::Highlight);
	// 3.5 rather than 3.0: the top of Fusion's hover gradient is a shade
	// lighter than the Light role, so the icon needs some headroom there.
	for (int step = 0; step < 20 && QET::Palette::contrastRatio(ink, face) < 3.5; ++step)
		ink = light_face ? ink.darker(110) : ink.lighter(110);
	if (QET::Palette::contrastRatio(ink, face) < 3.5)
		ink = palette.color(QPalette::Active, QPalette::ButtonText);
	return ink;
}

/**
	@brief QETStyle::disabledFrameColor
	A disabled box sits on the window and holds a fill close to the
	disabled base (Fusion paints that role; the Windows 11 style a white
	at 4% of its own, about (39,39,39) on its (30,30,30) window), so the
	outline is read against the lighter of the two. 3.5 and not 3.0 for
	that fill the palette does not know of: with a window and a disabled
	base of (30,30,30) and a disabled text of (157,157,157), the outline
	comes out at (119,119,119), 3.7:1 on the window and 3.3:1 on that
	fill. It goes from the window toward the disabled text, so it stays
	dimmer than the text; should the text itself not get there, the
	window text does.
*/
QColor QETStyle::disabledFrameColor(const QPalette &palette)
{
	const QColor window = palette.color(QPalette::Active, QPalette::Window);
	const QColor base = palette.color(QPalette::Disabled, QPalette::Base);
	const QColor ground = QET::Palette::contrastRatio(base, Qt::black)
	                      > QET::Palette::contrastRatio(window, Qt::black)
	                      ? base : window;

	for (const QColor &target : {palette.color(QPalette::Disabled, QPalette::Text),
	                             palette.color(QPalette::Active, QPalette::WindowText)})
	{
		for (int step = 1; step <= 20; ++step)
		{
			const qreal t = step / 20.0;
			const QColor mix(qRound(window.red()   + (target.red()   - window.red())   * t),
			                 qRound(window.green() + (target.green() - window.green()) * t),
			                 qRound(window.blue()  + (target.blue()  - window.blue())  * t));
			if (QET::Palette::contrastRatio(mix, ground) >= 3.5)
				return mix;
		}
	}
	return palette.color(QPalette::Active, QPalette::WindowText);
}

/**
	@brief QETStyle::drawPrimitive
	PE_FrameLineEdit of a disabled line edit with a frame, on a dark
	palette: the base style's frame, then the outline of
	drawDisabledFrame(). Fusion and the Windows 11 style both draw a line
	edit's frame through proxy()->drawPrimitive(PE_FrameLineEdit) from
	their PE_PanelLineEdit, which is how this override is reached. A line
	edit without a frame is the editor inside a spin box or an editable
	combo box, whose frame drawComplexControl() outlines; without a line
	edit behind it, a frame option of zero width is left alone too (Fusion
	draws an editable combo box that way, into a pixmap it caches).
*/
void QETStyle::drawPrimitive(PrimitiveElement element,
                             const QStyleOption *option,
                             QPainter *painter,
                             const QWidget *widget) const
{
	QProxyStyle::drawPrimitive(element, option, painter, widget);
	if (element != PE_FrameLineEdit || !disabledOnDark(option))
		return;

	bool framed = false;
	if (const auto *edit = qobject_cast<const QLineEdit *>(widget))
		framed = edit->hasFrame();
	else if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option))
		framed = frame->lineWidth > 0;
	if (framed)
		drawDisabledFrame(painter, option->rect, option->palette);
}

/**
	@brief QETStyle::drawComplexControl
	CC_SpinBox and CC_ComboBox of a disabled control with a frame, on a
	dark palette: the base style's control, then the outline of
	drawDisabledFrame() around it.
*/
void QETStyle::drawComplexControl(ComplexControl control,
                                  const QStyleOptionComplex *option,
                                  QPainter *painter,
                                  const QWidget *widget) const
{
	QProxyStyle::drawComplexControl(control, option, painter, widget);
	if (!disabledOnDark(option))
		return;

	bool framed = false;
	if (control == CC_SpinBox)
	{
		if (const auto *spin = qstyleoption_cast<const QStyleOptionSpinBox *>(option))
			framed = spin->frame;
	}
	else if (control == CC_ComboBox)
	{
		if (const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option))
			framed = combo->frame;
	}
	if (framed)
		drawDisabledFrame(painter, option->rect, option->palette);
}

/**
	@brief QETStyle::tinted
	@return pixmap with every pixel set to color, alpha kept.
*/
QPixmap QETStyle::tinted(const QPixmap &pixmap, const QColor &color)
{
	QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
	QPainter painter(&image);
	painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
	painter.fillRect(image.rect(), color);
	painter.end();

	QPixmap result = QPixmap::fromImage(image);
	result.setDevicePixelRatio(pixmap.devicePixelRatio());
	return result;
}

/**
	@brief QETStyle::faded
	@return pixmap with its opacity multiplied by opacity, colors kept.
	On premultiplied pixels, scaling the four channels alike scales the
	opacity and nothing else.
*/
QPixmap QETStyle::faded(const QPixmap &pixmap, qreal opacity)
{
	QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
	const int keep = qBound(0, qRound(opacity * 256), 256);
	for (int y = 0; y < image.height(); ++y)
	{
		QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
		for (int x = 0; x < image.width(); ++x)
		{
			const QRgb pixel = line[x];
			line[x] = qRgba((qRed(pixel) * keep) >> 8,
			                (qGreen(pixel) * keep) >> 8,
			                (qBlue(pixel) * keep) >> 8,
			                (qAlpha(pixel) * keep) >> 8);
		}
	}

	QPixmap result = QPixmap::fromImage(image);
	result.setDevicePixelRatio(pixmap.devicePixelRatio());
	return result;
}

/**
	@brief QETStyle::swatchIcon
	The swatches of the color pickers (conductor color, sheet background)
	used to have one (64,64,64) outline, which vanishes on a dark menu:
	a black, purple or dark grey swatch read at less than 2:1 there. The
	two rings below read on any ground, so the swatch does not depend on
	the palette in force when the menu was built, and the color the user
	picked fills the middle unchanged.

	White at 200 of 255 shows as about (206,206,206) on a (30,30,30) tool
	bar and all but disappears on a light one (252 on 239); black at 200
	shows as about (55,55,55) on white, close to the former outline. The
	swatch reads at 8:1 or more on the dark grounds (30 to 53) and on the
	light ones (239, 255), and at 4.3:1 on mid grey (128), whatever its
	color. Both rings stay more than half opaque once a disabled button
	fades them (129 of 255, see generatedIconPixmap()), so the disabled
	swatch keeps its outline: 4.4:1 or more on a dark ground.
*/
QIcon QETStyle::swatchIcon(const QColor &color)
{
	QImage image(16, 16, QImage::Format_ARGB32);
	image.fill(QColor(255, 255, 255, 200));
	QPainter painter(&image);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.fillRect(QRect(1, 1, 14, 14), QColor(0, 0, 0, 200));
	painter.fillRect(QRect(2, 2, 12, 12), color);
	painter.end();
	return QIcon(QPixmap::fromImage(image));
}

/**
	@brief QETStyle::menuIcon
	An icon for a highlighted menu item: Normal as the original, Active
	as the original in the HighlightedText color when it is line art.
	Cached per source icon, size and device pixel ratio, since menus
	repaint on every pointer move.
*/
QIcon QETStyle::menuIcon(const QIcon &icon, const QPalette &palette, int size, qreal dpr) const
{
	const QString key = QString("%1/%2/%3/%4")
	                    .arg(icon.cacheKey()).arg(size).arg(dpr)
	                    .arg(palette.color(QPalette::Active, QPalette::HighlightedText).name());
	auto it = menu_icons_.constFind(key);
	if (it != menu_icons_.constEnd())
		return *it;

	const QPixmap normal = icon.pixmap(QSize(size, size), dpr, QIcon::Normal);
	QIcon result;
	result.addPixmap(normal, QIcon::Normal);
	result.addPixmap(isLineArt(normal.toImage())
	                 ? tinted(normal, palette.color(QPalette::Active, QPalette::HighlightedText))
	                 : normal,
	                 QIcon::Active);
	result.addPixmap(icon.pixmap(QSize(size, size), dpr, QIcon::Disabled), QIcon::Disabled);
	menu_icons_.insert(key, result);
	return result;
}

/**
	@brief QETStyle::drawControl
	CE_MenuItem with an icon and a highlight: swap the icon for menuIcon()
	so the base style's QIcon::Active request gets an icon that reads on
	the highlight bar. Everything else goes to the base style.
*/
void QETStyle::drawControl(ControlElement element,
                           const QStyleOption *option,
                           QPainter *painter,
                           const QWidget *widget) const
{
	if (element == CE_MenuItem)
	{
		const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
		if (item && !item->icon.isNull()
		    && (item->state & State_Selected) && (item->state & State_Enabled))
		{
			QStyleOptionMenuItem copy = *item;
			const int size = pixelMetric(PM_SmallIconSize, option, widget);
			const qreal dpr = widget ? widget->devicePixelRatio()
			                         : QApplication::instance() ? qApp->devicePixelRatio() : 1.0;
			copy.icon = menuIcon(item->icon, item->palette, size, dpr);
			QProxyStyle::drawControl(element, &copy, painter, widget);
			return;
		}
	}
	QProxyStyle::drawControl(element, option, painter, widget);
}

/**
	@brief QETStyle::generatedIconPixmap
	QIcon::Active: a line-art icon comes back tinted in hoverColor(),
	alpha kept, so the hovered icon differs from the normal one on both
	light and dark palettes. Colored icons go to the base style.

	QIcon::Disabled on a dark palette: the icon faded to kDisabledOpacity,
	colored or not. On a light palette, the base style's disabled icon.

	Qt's icon engines keep the generated pixmap in QPixmapCache under the
	application palette's cache key and not under the style, so what is
	returned here must depend on the pixmap and on option->palette only.
	Both answers above do.

	Every other mode goes to the base style.
*/
QPixmap QETStyle::generatedIconPixmap(QIcon::Mode mode,
                                      const QPixmap &pixmap,
                                      const QStyleOption *option) const
{
	if (pixmap.isNull())
		return QProxyStyle::generatedIconPixmap(mode, pixmap, option);

	const QPalette palette = option ? option->palette : QApplication::palette();
	if (mode == QIcon::Disabled)
	{
		if (QET::Palette::isDark(palette))
			return faded(pixmap, kDisabledOpacity);
		return QProxyStyle::generatedIconPixmap(mode, pixmap, option);
	}

	if (mode != QIcon::Active || !isLineArt(pixmap.toImage()))
		return QProxyStyle::generatedIconPixmap(mode, pixmap, option);

	return tinted(pixmap, hoverColor(palette));
}
