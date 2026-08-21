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
#include "symbolpreview.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QtMath>


namespace
{
	/// room around the drawing, in widget pixels
	const int MARGIN = 14;
	/// radius of a connection point, in widget pixels
	const qreal DOT = 3.5;
	/// how close a click has to be to count as picking a point
	const qreal PICK = 10.0;

	QColor accentColor()
	{
			//The blue QElectroTech already uses for the docking point of a
			//terminal. Using the program's own vocabulary of colour means the
			//picture reads the same way the folio does.
		return QColor(30, 95, 166);
	}
}

SymbolPreview::SymbolPreview(QWidget *parent) :
	QWidget(parent)
{
	setMinimumSize(180, 150);
	setCursor(Qt::PointingHandCursor);
	setToolTip(tr("Le dessin qui va être enregistré. Cliquez un point de "
		      "raccordement pour le sélectionner dans la table."));
}

QSize SymbolPreview::sizeHint() const
{
	return QSize(240, 200);
}

void SymbolPreview::setSymbol(const SymbolDefinition &symbol)
{
	m_symbol = symbol;
	update();
}

void SymbolPreview::setHighlighted(int index)
{
	if (m_highlighted == index) {
		return;
	}
	m_highlighted = index;
	update();
}

int SymbolPreview::highlighted() const
{
	return m_highlighted;
}

QPointF SymbolPreview::widgetPositionOf(int index) const
{
	if (index < 0 || index >= m_symbol.terminals.size()) {
		return QPointF();
	}
	return viewTransform().map(m_symbol.terminals.at(index).position);
}

QTransform SymbolPreview::viewTransform() const
{
	QRectF box = m_symbol.bounds();
	if (box.isNull() || box.isEmpty()) {
		return QTransform();
	}

		//A drawing with no width or no height - a single straight line - would
		//divide by zero. Given it some so it still shows.
	if (box.width() < 1.0) {
		box.adjust(-5.0, 0.0, 5.0, 0.0);
	}
	if (box.height() < 1.0) {
		box.adjust(0.0, -5.0, 0.0, 5.0);
	}

	const qreal usable_w = width() - 2 * MARGIN;
	const qreal usable_h = height() - 2 * MARGIN;
	if (usable_w <= 0.0 || usable_h <= 0.0) {
		return QTransform();
	}

		//One scale for both axes: a symbol shown stretched is a symbol whose
		//shape you cannot judge, and judging the shape is half of why this
		//picture exists.
	const qreal scale = qMin(usable_w / box.width(), usable_h / box.height());

	QTransform transform;
	transform.translate(width() / 2.0, height() / 2.0);
	transform.scale(scale, scale);
	transform.translate(-box.center().x(), -box.center().y());
	return transform;
}

void SymbolPreview::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	painter.fillRect(rect(), palette().base());
	painter.setPen(QPen(palette().mid().color()));
	painter.drawRect(rect().adjusted(0, 0, -1, -1));

	if (m_symbol.shapes.isEmpty() && m_symbol.terminals.isEmpty()) {
		painter.setPen(QPen(palette().mid().color()));
		painter.drawText(rect(), Qt::AlignCenter,
				 tr("rien à montrer"));
		return;
	}

	const QTransform view = viewTransform();

		//The drawing, with the pen the style string asks for. The width is
		//left to the pen but drawn cosmetic: at this scale a 4 unit line would
		//swallow the symbol.
	for (const SymbolShape &shape : m_symbol.shapes)
	{
		if (!shape.isValid()) {
			continue;
		}
		QPen pen = SymbolShape::penFor(shape.style);
		pen.setCosmetic(true);
		pen.setWidthF(qMax(1.0, pen.widthF()));
		painter.setPen(pen);
		painter.setBrush(SymbolShape::brushFor(shape.style));

		QPolygonF points;
		for (const QPointF &point : shape.points) {
			points << view.map(point);
		}

		switch (shape.type) {
			case SymbolShapeType::Line:
				painter.drawLine(points.value(0), points.value(1));
				break;
			case SymbolShapeType::Rectangle:
				painter.drawRect(QRectF(points.value(0),
							points.value(1)).normalized());
				break;
			case SymbolShapeType::Ellipse:
				painter.drawEllipse(QRectF(points.value(0),
							   points.value(1)).normalized());
				break;
			case SymbolShapeType::Polygon:
				if (shape.closed) {
					painter.drawPolygon(points);
				} else {
					painter.drawPolyline(points);
				}
				break;
		}
	}

		//A declared pair, drawn as the thin line that joins its two halves.
		//This is what turns "two rows in a table" into "one contact" on
		//screen, which is the thing the table alone could not say.
	painter.setBrush(Qt::NoBrush);
	const QStringList pairs = m_symbol.pairNames();
	for (const QString &pair : pairs)
	{
		QList<QPointF> members;
		for (const SymbolTerminal &terminal : m_symbol.terminals) {
			if (terminal.pair == pair) {
				members << view.map(terminal.position);
			}
		}
		if (members.size() != 2) {
			continue;
		}
		QPen pen(accentColor(), 1.0, Qt::DashLine);
		pen.setCosmetic(true);
		painter.setPen(pen);
		painter.drawLine(members.at(0), members.at(1));
	}

		//The insertion point: where the cursor holds the symbol. A cross and
		//not a dot, so it is never mistaken for a connection point.
	if (!m_symbol.terminals.isEmpty())
	{
		const QPointF hotspot = view.map(m_symbol.hotspot);
		QPen pen(palette().mid().color(), 1.0);
		pen.setCosmetic(true);
		painter.setPen(pen);
		painter.drawLine(hotspot + QPointF(-6, 0), hotspot + QPointF(6, 0));
		painter.drawLine(hotspot + QPointF(0, -6), hotspot + QPointF(0, 6));
	}

		//The connection points, and the one being edited called out by size,
		//by a ring and by its label. Three signals and not one, because a
		//colour alone fails whoever cannot tell the two colours apart.
	for (int i = 0 ; i < m_symbol.terminals.size() ; ++i)
	{
		const SymbolTerminal &terminal = m_symbol.terminals.at(i);
		const QPointF centre = view.map(terminal.position);
		const bool called_out = (i == m_highlighted);

		painter.setPen(Qt::NoPen);
		painter.setBrush(called_out ? accentColor()
					    : QColor(193, 58, 46));
		const qreal radius = called_out ? DOT * 1.9 : DOT;
		painter.drawEllipse(centre, radius, radius);

		if (called_out)
		{
			QPen ring(accentColor(), 1.5);
			ring.setCosmetic(true);
			painter.setPen(ring);
			painter.setBrush(Qt::NoBrush);
			painter.drawEllipse(centre, radius + 4.0, radius + 4.0);

			const QString label = terminal.label.isEmpty()
					? tr("sans repère")
					: terminal.label;
			painter.setPen(QPen(palette().text().color()));
			painter.drawText(centre + QPointF(radius + 7.0, -radius - 3.0),
					 label);
		}
	}
}

void SymbolPreview::mousePressEvent(QMouseEvent *event)
{
	const QTransform view = viewTransform();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	const QPointF where = event->position();
#else
	const QPointF where = event->localPos();
#endif

	int nearest = -1;
	qreal best = PICK;
	for (int i = 0 ; i < m_symbol.terminals.size() ; ++i)
	{
		const qreal distance = QLineF(
					view.map(m_symbol.terminals.at(i).position),
					where).length();
		if (distance <= best) {
			best = distance;
			nearest = i;
		}
	}

	if (nearest >= 0) {
		setHighlighted(nearest);
		emit terminalPicked(nearest);
	}
	QWidget::mousePressEvent(event);
}
