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
#ifndef MOUNTINGVIEW_H
#define MOUNTINGVIEW_H

#include <QGraphicsView>

class MountingScene;

/**
	@brief The window a mounting surface is looked at through.

	It is the third view of the program and it is built on the second: the
	element editor has a scene of its own and a view of its own, and what
	that view does - wheel zoom, trackpad pinch, zoom to fit, back to one -
	is what somebody looking at a plate wants as well. Copying it is
	cheaper and safer than inventing a third behaviour, and it means the
	two editors answer the wheel the same way.

	The one thing that is not copied is the scale it goes back to. The
	element editor resets to four, because an element is drawn in tens of
	units; here one is one, and that is not a coincidence: a scene unit is
	a millimetre, so a transform of one draws a plate of 600 mm as 600
	pixels, which fits the screen it is being laid out on.

	pixelsPerMillimetre() is that number read out loud, and it is worth
	saying what it is not. It is a property of this window - of how far
	somebody has zoomed in at this instant - and it is never written down
	anywhere. The factor that does get stored belongs to the insertion of
	the layout into a folio, where a drawing is fitted to a sheet of paper;
	a factor stored next to a length is a length nobody can compare with a
	panel any more.

	The wheel is clamped at both ends, and that is not decoration: driving a
	QGraphicsView transform far enough crashes the editor, which is the bug
	the element view was given the same two bounds for. Zooming to fit is
	clamped at the top end only, and the reason is written over clampFit: a
	floor that stops a fit from going small enough is a floor that stops the
	plate from fitting, which is the whole of what the command promises.
*/
class MountingView : public QGraphicsView
{
	Q_OBJECT

	public:
		explicit MountingView(MountingScene *scene,
				      QWidget *parent = nullptr);
		~MountingView() override;

			/// @return the surface being looked at
		MountingScene *scene() const;

		/**
			@return how many pixels one millimetre is drawn as at
			this instant.

			Read off the transform and stored nowhere, for the
			reason written above the class.
		*/
		qreal pixelsPerMillimetre() const;

	public slots:
		void zoomIn();
		void zoomOut();
		void zoomInSlowly();
		void zoomOutSlowly();

		/// @brief Fit the whole plate, and everything off it, in the window
		void zoomFit();

		/// @brief Back to one pixel per millimetre
		void zoomActualSize();

	signals:
		/**
			@brief Emitted while the mouse moves over the surface.
			@param position_mm where the pointer is, millimetre, in
			the frame the surface declares

			So that a window can say where the pointer is in the
			unit the panel is built in. A plate is laid out against
			a tape measure, and a pointer that only knows pixels is
			of no use to somebody holding one.
		*/
		void pointed(const QPointF &position_mm);

			/// @brief Emitted after the zoom changed, in pixels per millimetre
		void zoomChanged(qreal pixels_per_millimetre);

	protected:
		void wheelEvent(QWheelEvent *event) override;
		void mouseMoveEvent(QMouseEvent *event) override;

	private:
		bool gestures() const;
		void scaleClamped(qreal factor);
		void clampFit();

		/// Lowest and highest value a *step* of the zoom may leave the
		/// view transform scale (m11) at, for the reason ElementView has
		/// the same pair: driving the transform to overflow crashes the
		/// editor. A step is repeatable for as long as somebody keeps
		/// turning the wheel, which is what makes a floor necessary
		/// there - and what makes it wrong for a fit, which is absolute.
		/// Only m_max_zoom binds a fit; see clampFit.
		static constexpr qreal m_min_zoom = 0.05;
		static constexpr qreal m_max_zoom = 40.0;

		MountingScene *m_scene = nullptr;
};

#endif // MOUNTINGVIEW_H
