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
#include "templatevisualcell.h"

#include "../diagramcontext.h"
#include "../qetinformation.h"
#include "../titleblocktemplate.h"

/**
	Constructor
	@param parent Parent QGraphicsItem
*/
TitleBlockTemplateVisualCell::TitleBlockTemplateVisualCell(QGraphicsItem *parent) :
	QGraphicsLayoutItem(),
	QGraphicsItem(parent),
	template_(nullptr),
	cell_(nullptr)
{
	setGraphicsItem(this);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	
}

/**
	Destructor
*/
TitleBlockTemplateVisualCell::~TitleBlockTemplateVisualCell()
{
}

/**
	Ensure geometry changes are handled for both QGraphicsObject and
	QGraphicsLayoutItem.
	@param g New geometry
*/
void TitleBlockTemplateVisualCell::setGeometry(const QRectF &g) {
	prepareGeometryChange();
	QGraphicsLayoutItem::setGeometry(g);
	setPos(g.topLeft());
}

/**
	@param which Size hint to be modified
	@param constraint New value for the size hint
	@return the size hint for \a which using the width or height of \a constraint
*/
QSizeF TitleBlockTemplateVisualCell::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
	Q_UNUSED(which);
	return constraint;
}

/**
	@return the bounding rect of this helper cell
*/
QRectF TitleBlockTemplateVisualCell::boundingRect() const
{
	return QRectF(QPointF(0,0), geometry().size());
}

/**
	Handles the helper cell visual rendering
	@param painter QPainter to be used for the rendering
	@param option Rendering options
	@param widget QWidget being painted, if any
*/
void TitleBlockTemplateVisualCell::paint(
		QPainter *painter,
		const QStyleOptionGraphicsItem *option,
		QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);
	
	QRectF drawing_rectangle(QPointF(0, 0), geometry().size() /*- QSizeF(1, 1)*/);
	
	if (template_ && cell_) {
		template_ -> renderCell(*painter, *cell_, authoringContext(), drawing_rectangle.toRect());
	}
	if (isSelected()) {
		QBrush selection_brush = QApplication::palette().highlight();
		QColor selection_color = selection_brush.color();
		selection_color.setAlpha(127);
		selection_brush.setColor(selection_color);
		painter -> setPen(Qt::NoPen);
		painter -> setBrush(selection_brush);
		painter -> drawRect(drawing_rectangle/*.adjusted(1, 1, -1, -1)*/);
	}
}

/**
	@return the context this preview renders with: every variable the cell
	names, standing for its own name.

	The folio prints nothing where nobody filled a value in, which is right,
	and the preview used to render with an empty context, which made the same
	thing happen here - so a person drawing a title block could not see which
	cells already carried an attribute and which were plain text. Before the
	folio was fixed the cell showed "%{name}", and that accident was doing the
	job.

	Nothing of this reaches the folio: the folio renders through
	TitleBlockTemplate::render(), with the context its project built.

	The context is the cell's own, not the whole template's. Only this cell is
	being rendered, so its own variables are all that can be substituted into
	it, and building it per cell keeps a repaint linear in the number of cells
	instead of quadratic - which matters in an editor where dragging a column
	repaints everything.
*/
DiagramContext TitleBlockTemplateVisualCell::authoringContext() const
{
	if (!cell_ || cell_ -> type() != TitleBlockCell::TextCell) {
		return DiagramContext();
	}

	QStringList texts;
	texts << cell_ -> value.name();
		//The label too: finalTextForCell() substitutes in both, so a
		//variable used only as a label is the half of the cell that would
		//go on being blank.
	if (cell_ -> display_label && !cell_ -> label.isEmpty()) {
		texts << cell_ -> label.name();
	}
	return QETInformation::titleblockAuthoringContext(texts);
}

/**
	Set the previewed title block cell.
	@param tbt Parent title block template of the previewed cell
	@param cell Previewed cell
*/
void TitleBlockTemplateVisualCell::setTemplateCell(
		TitleBlockTemplate *tbt, TitleBlockCell *cell)
{
	template_ = tbt;
	cell_     = cell;
}

/**
	@return the parent title block template of the previewed cell
*/
TitleBlockTemplate *TitleBlockTemplateVisualCell::titleBlockTemplate() const
{
	return(template_);
}

/**
	@return the previewed title block cell
*/
TitleBlockCell *TitleBlockTemplateVisualCell::cell() const
{
	return(cell_);
}

/**
	@return the title block cell previewed by this object, plus the cells it
	spans over, if any
*/
QSet<TitleBlockCell *> TitleBlockTemplateVisualCell::cells() const
{
	QSet<TitleBlockCell *> set;
	if (cell_) {
		if (template_) {
			set = template_ -> spannedCells(cell_);
		}
		
		// the TitleBlockCell rendered by this object
		set << cell_;
	}
	return(set);
}
