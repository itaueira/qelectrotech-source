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
#ifndef PAPER_GRAPHICS_VIEW_H
#define PAPER_GRAPHICS_VIEW_H

#include <QGraphicsView>
#include <QImage>
#include <QPainter>

/**
	A QGraphicsView for a scene drawn as on white paper - the symbol
	editor, the title block template editor, the mounting plate - that
	shows it with its lightness inverted while the application palette is
	dark: white becomes the palette's Base color, black its Text color,
	and a colored stroke keeps its hue, only lighter. On a light palette
	the view paints exactly as QGraphicsView does.

	Only the display changes. The scene keeps the colors it was given, so
	a symbol, a title block template or a plate saved while the palette is
	dark is the one saved while it is light, and whatever renders the
	scene through another painter (printing, export) still gets black on
	white.

	The mechanism is the one PaletteGraphicsView gives the folio, and its
	header explains it: QGraphicsView::paintEvent() runs with the
	IndirectPainting flag set, the drawBackground(), drawItems() and
	drawForeground() hooks paint into an off-screen image of the viewport,
	and the exposed part of that image is inverted and blitted. The scene
	is handed the viewport when its items are drawn, so a moved item is
	erased where it was last painted, as on a light palette.

	It is not PaletteGraphicsView because of one rule the folio has and
	these views must not: whether the folio inverts, and onto what, also
	depends on the background color the user picked for the sheet, a
	setting held statically by PaletteGraphicsView for every view of that
	class - a light custom color, the grey #808080 among them, turns the
	inversion off. None of these editors has a background of its own to
	pick, so here the palette alone decides. A virtual hook for that rule
	in PaletteGraphicsView would let the two classes become one.

	A subclass that draws in drawBackground() itself paints through
	scenePainter(), and may ask paintsInverted() to soften what would
	come out too bright once inverted (a grid, for instance). The
	CacheBackground cache mode is not supported on the inverted path.
*/
class PaperGraphicsView : public QGraphicsView
{
	Q_OBJECT

	public:
		explicit PaperGraphicsView(QWidget *parent = nullptr);
		explicit PaperGraphicsView(QGraphicsScene *scene, QWidget *parent = nullptr);

		bool invertsLightness() const;

		/**
			@return true while a PaperGraphicsView is painting its scene
			for an inverted display on @a widget, i.e. when @a widget is
			the viewport of such a view and the view is inside paintEvent()
			on a dark palette. For an item's paint(), which gets the
			viewport as its widget argument: an item can then change the
			order it draws in, never its colors.
		*/
		static bool isPaintingInvertedOn(const QWidget *widget);

	protected:
		bool eventFilter(QObject *watched, QEvent *event) override;
		void paintEvent(QPaintEvent *event) override;
		void drawBackground(QPainter *painter, const QRectF &rect) override;
		void drawItems(QPainter *painter, int count, QGraphicsItem *items[],
		               const QStyleOptionGraphicsItem options[]) override;
		void drawForeground(QPainter *painter, const QRectF &rect) override;

		/// @return true while the scene is being painted for an
		/// inverted display.
		bool paintsInverted() const { return m_inverting; }
		/// @return the painter the scene really paints with: the
		/// off-screen image's while painting inverted, else \a painter.
		QPainter *scenePainter(QPainter *painter)
		{ return m_inverting ? &m_buffer_painter : painter; }

	private:
		void paintInverted(QPaintEvent *event);
		void drawRubberBand(QPainter &painter);

		/// The off-screen image the hooks paint into while m_inverting:
		/// the viewport's size, in its coordinates. Kept between paints,
		/// dropped when the view paints on a light palette again.
		QImage m_buffer;
		QPainter m_buffer_painter;
		/// True while paintEvent() paints for an inverted display.
		bool m_inverting = false;
};

#endif
