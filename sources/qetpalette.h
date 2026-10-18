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
#ifndef QET_PALETTE_H
#define QET_PALETTE_H

#include <QImage>
#include <QPalette>
#include <QPixmap>

class QImage;
class QStyle;

/**
	Palettes tuned for the Fusion style.

	main.cpp forces the Fusion style on macOS, but the palette still comes
	from Qt's macOS platform theme, which is built for the native style.
	That palette gives Window, Button and Base the same color, makes Dark
	lighter than Light, and in dark mode sets the Inactive ButtonText to
	black. Fusion derives its outlines, gradients and indicator colors from
	those roles, so under that palette controls lose their edges, radio
	indicators vanish, and combo box text turns black as soon as the window
	loses focus.

	This namespace provides a light and a dark palette with the role
	separation Fusion expects, and the WCAG 2 contrast helper the tests use
	to keep them readable.
*/
namespace QET {
	namespace Palette {
		/**
			@return true when the style draws with Fusion's rules,
			i.e. its object name is "fusion".
		*/
		bool styleIsFusion(const QStyle *style);

		/**
			@return true when the palette's Window color is dark
			(lightness below 128).
		*/
		bool isDark(const QPalette &palette);

		/**
			Invert the lightness of every pixel of \a image, keeping its hue
			and saturation, then stretch the result between two colors: pure
			white becomes \a sheet, pure black becomes \a ink, and a red
			line stays red, only lighter. Made for a rendering of a white
			sheet that has to read on a dark palette, with sheet = Base and
			ink = Text. The image must be opaque; an image in another format
			is converted to RGB32 first.
		*/
		void invertLightness(QImage &image, const QColor &sheet = Qt::black,
		                     const QColor &ink = Qt::white);
		/// The same colour mapping for a transparent composition layer.
		/// Retains alpha; premultiplied input is converted to straight ARGB32.
		void invertLightnessLayer(QImage &image, const QColor &sheet, const QColor &ink);

		/**
			The color of the grid dots on a sheet of color \a sheet: black,
			or white on a black sheet. With \a inverted the sheet is about to
			be shown with its lightness inverted (PaletteGraphicsView), where
			black dots would come out as bright as the ink; the dots are then
			a third of the way from the sheet color to black, which shows as
			a soft gray.
		*/
		QColor gridDotColor(const QColor &sheet, bool inverted);

		/**
			WCAG 2 contrast ratio between two opaque colors, from 1 (equal)
			to 21 (black on white). Normal text needs at least 4.5, large
			text and non-text controls at least 3.
		*/
		double contrastRatio(const QColor &a, const QColor &b);

		/**
			A light palette with the role separation Fusion expects: Window
			and Button a light grey, Base white, Text black, and Inactive
			equal to Active so nothing changes when the window loses focus.
		*/
		QPalette fusionLight();

		/**
			A dark palette with the same role separation: Window a mid
			grey, Button lighter than Window, Base darker than Window, Text
			light, and Inactive equal to Active.
		*/
		QPalette fusionDark();

		/**
			Copy the Highlight and HighlightedText colors of the platform
			palette into ours, so the user's accent color survives, but only
			when the pair reads at 4.5:1 or better. Otherwise ours is kept.
		*/
		QPalette withPlatformAccent(QPalette palette, const QPalette &platform);

		/**
			The palette QET should use under Fusion, given the palette the
			platform handed to the application: fusionDark() when the
			platform palette is dark, fusionLight() otherwise, with the
			platform's accent color kept when it is readable.
		*/
		QPalette forFusion(const QPalette &platform);

		/**
			\a palette with the text of disabled widgets made readable,
			for a dark platform palette drawn by the platform's own style
			(Windows: see QETApp::initStyle()).

			Each of the Disabled WindowText, Text, ButtonText and
			PlaceholderText is measured against the Disabled Window, Base
			and Button, the three backgrounds disabled text is drawn on; a
			translucent color is measured as it shows, over each of them.
			A role that reads below \a minimum against any of the three is
			given the same hue and saturation at the smallest HSL lightness
			that reaches it, opaque. The smallest is what keeps it dimmer
			than the enabled text, so a disabled control still reads as
			disabled; should the enabled text itself read below that, the
			disabled one is left readable rather than dimmer.

			Only the roles that failed are set, so the resolve mask of the
			palette gains a bit for those and no other: a style that draws
			from the palette only where the application set a role (Qt's
			windows11 style does that for ButtonText) keeps its own colors
			everywhere else. A light palette, and a dark one whose disabled
			text already reads, come back unchanged.

			The backgrounds are expected to be dark, which is what isDark()
			says of Window. Should a lighter one make \a minimum
			unreachable, the role gets the lightness that reads best.
		*/
		QPalette withReadableDisabled(QPalette palette, double minimum = 3.0);

		/**
			The color \a wanted, made readable as text on \a palette.

			Meant for the colors written into the code for messages and
			list rows - an error red, a warning amber - that were picked
			on a light background. On a light palette \a wanted comes back
			as it is, so the light look does not change. On a dark one its
			hue and saturation are kept and its HSL lightness is raised by
			the smallest step that makes it read at \a minimum (4.5 is
			WCAG's ratio for normal text) against every background colored
			text sits on in this program: Window (a label in a dialog or a
			status line), Base (a line edit, a list or a table) and
			AlternateBase (the other rows of a list with alternating row
			colors, measured over Base when it is translucent), all from the
			Active group. A color that already reads comes back as it is.
			\a wanted is measured as if it were opaque, and its alpha is
			put back on the result.

			The result is computed when it is asked for: a style sheet or a
			list row colored with it keeps that color until it is colored
			again. The light or dark preference applies at the next start
			(main.cpp), so that is only visible after a live change of the
			custom application color.
		*/
		QColor readable(const QColor &wanted, const QPalette &palette,
		                double minimum = 4.5);

		/**
			True when fewer than a fifth of the visible pixels are
			saturated: black or gray line art, which is what element
			previews and most of QET's own icons are.
		*/
		bool isLineArt(const QImage &image);

		/**
			The image with its lightness inverted and hue, saturation and
			alpha kept: the darkest ink becomes light gray (220), white
			becomes black. The rule misc/make_icon_themes.py applies when
			it builds the dark icon theme.
		*/
		QImage invertedLightness(const QImage &image);

		/**
			A picture drawn for a white sheet, made to read on palette:
			returned as is on a light palette, and with its lightness
			inverted on a dark one when it is line art. Colored art is
			left alone either way.
		*/
		QPixmap forPalette(const QPixmap &pixmap, const QPalette &palette);

		/**
			Make every widget that carries a style sheet take the current
			application palette. QApplication::setPalette() reaches plain
			widgets, but a widget with a style sheet keeps the palette
			QStyleSheetStyle resolved when the sheet was applied, so after a
			live light/dark switch it is drawn in the old colors (the folio
			tab bar and its buttons, the element info widgets, several
			configuration pages). Re-applying each widget's own sheet makes
			QStyleSheetStyle resolve it again. Call after setPalette().
		*/
		void refreshStyleSheets();
	}
}

#endif
