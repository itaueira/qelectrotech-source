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
#ifndef SYMBOLPREVIEW_H
#define SYMBOLPREVIEW_H

#include <QWidget>

#include "../symbolbuilder.h"

/**
	@brief The drawing being turned into a symbol, with one connection point
	called out.

	This exists because of one sentence of test feedback: *"ao selecionar uma
	[linha da tabela] ele não evidencia qual ela é no desenho selecionado,
	assim não consigo atribuir o ponto à linha"*. Without knowing which row is
	which point, declaring a contact is guesswork, and declaring contacts is
	the whole reason the table has a role column.

	Drawn here, inside the dialog, rather than highlighted on the folio: the
	dialog is modal and sits over the drawing, so a highlight underneath it is
	a highlight nobody can see. And a small picture of what is about to be
	saved answers a second question the dialog could not answer before — "is
	this the shape I think it is".

	Read only. Clicking a point selects it, because a picture you can point at
	is worth more than a picture you can only look at, and because going from
	the drawing to the row is the direction the projectist actually needs.
*/
class SymbolPreview : public QWidget
{
	Q_OBJECT

	public:
		explicit SymbolPreview(QWidget *parent = nullptr);

		void setSymbol(const SymbolDefinition &symbol);
		/// which connection point is called out, -1 for none
		void setHighlighted(int index);
		int highlighted() const;

		QSize sizeHint() const override;

	signals:
		/// a connection point was clicked in the picture
		void terminalPicked(int index);

	protected:
		void paintEvent(QPaintEvent *event) override;
		void mousePressEvent(QMouseEvent *event) override;

	private:
		/// the transform that maps symbol coordinates onto the widget
		QTransform viewTransform() const;

		SymbolDefinition m_symbol;
		int m_highlighted = -1;
};

#endif // SYMBOLPREVIEW_H
