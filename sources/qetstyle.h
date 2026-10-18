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
#ifndef QET_STYLE_H
#define QET_STYLE_H

#include <QHash>
#include <QIcon>
#include <QProxyStyle>

/**
	@brief The QETStyle class
	A proxy over the running widget style that gives icons a hover state.

	QToolButton, QTabBar and item views ask the application style for the
	QIcon::Active pixmap of an icon while the mouse is over it. Every
	built-in style returns the icon unchanged for that mode, so only the
	button frame changes on hover, and on a dark palette Fusion's frame is
	too faint to see (GitHub #870). This proxy answers QIcon::Active for
	line-art icons with the icon tinted in the palette's highlight color,
	lightened when needed to reach 3:1 on a button face, so the icon
	itself changes on both light and dark palettes. Colored icons keep
	their colors.

	QIcon::Disabled on a dark palette comes back as the icon itself at
	reduced opacity. The built-in styles grey a disabled icon out against
	the window color, and on a dark window the result can all but vanish
	(the Windows style) or barely differ from the enabled icon (Fusion).
	Faded, an icon of the dark theme stays visibly weaker than its enabled
	self and still reads on the tool bar. On a light palette the base
	style keeps the disabled state. Every other mode is left to the base
	style.

	Fusion also asks for QIcon::Active for the icon of a highlighted menu
	item and paints it on the highlight bar, where the hover tint would
	vanish. The generated pixmap is cached per icon and cannot tell a
	menu from a toolbar, so drawControl(CE_MenuItem) hands the base style
	an icon whose Active pixmap is the icon in the highlighted-text color.

	A disabled line edit, spin box or combo box on a dark palette gets an
	outline at 3.5:1 against the window, drawn over the base style's own
	frame. The Windows 11 style draws those boxes in colors of its own,
	not the palette's, and disabled they read at about 1.2:1 on its dark
	window: the box vanishes and leaves its text floating. Enabled boxes
	and light palettes are left to the base style.
*/
class QETStyle : public QProxyStyle
{
	Q_OBJECT

	public:
		explicit QETStyle(QStyle *base);
		~QETStyle() override = default;

		QPixmap generatedIconPixmap(QIcon::Mode mode,
		                            const QPixmap &pixmap,
		                            const QStyleOption *option) const override;
		void drawControl(ControlElement element,
		                 const QStyleOption *option,
		                 QPainter *painter,
		                 const QWidget *widget = nullptr) const override;
		void drawPrimitive(PrimitiveElement element,
		                   const QStyleOption *option,
		                   QPainter *painter,
		                   const QWidget *widget = nullptr) const override;
		void drawComplexControl(ComplexControl control,
		                        const QStyleOptionComplex *option,
		                        QPainter *painter,
		                        const QWidget *widget = nullptr) const override;

		/**
			The outline of a disabled input box on a dark palette: the
			window color moved toward the disabled text until it reads at
			3.5:1 against the lighter of the window and the disabled base,
			so it stays dimmer than the text it frames.
		*/
		static QColor disabledFrameColor(const QPalette &palette);

		/// True when fewer than a fifth of the visible pixels are saturated.
		static bool isLineArt(const QImage &image);
		/// The highlight color, lightened until it reads at 3:1 on a button.
		static QColor hoverColor(const QPalette &palette);
		/// pixmap with every pixel set to color, alpha kept.
		static QPixmap tinted(const QPixmap &pixmap, const QColor &color);
		/// pixmap with its opacity multiplied by opacity (0 to 1), colors kept.
		static QPixmap faded(const QPixmap &pixmap, qreal opacity);
		/**
			A square of color for a color picker. The fill is color,
			unchanged; around it, a dark ring and a translucent white
			ring, so the swatch reads on a light ground (the dark ring)
			and on a dark one (the white ring) whatever palette was in
			force when it was drawn.
		*/
		static QIcon swatchIcon(const QColor &color);

	private:
		QIcon menuIcon(const QIcon &icon, const QPalette &palette, int size, qreal dpr) const;
		mutable QHash<QString, QIcon> menu_icons_;
};

#endif
