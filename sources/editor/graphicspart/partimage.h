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
#ifndef PART_IMAGE_H
#define PART_IMAGE_H

#include "partrectangle.h"

#include <QByteArray>
#include <QImage>

#include <memory>

class QSvgRenderer;

/**
	@brief An image in the 2D drawing of a part: the photo of the part, or
	the drawing of its datasheet, at the size the part really is.

	The file is kept whole, inside the element. The symbol travels into
	every project that uses it and the project to other machines, and a
	path on a disk does not travel with them.

	The rectangle is in millimetre, like everything on the 2D tab, and it
	is free: a picture taken from a datasheet is seldom at the right scale,
	whoever brings it knows the real measures of the part, and so width and
	height are set apart and nothing holds the proportion of the file.

	As far as editing goes it is a rectangle - its handles, its undo, its
	scaling inside a selection of several parts - painting the file where
	the rectangle draws its outline. It has no style and no rounded corner,
	and it does not turn: the commands that rotate, flip and mirror pick
	the parts they know by type, and leave this one where it is rather
	than stretch it across.
*/
class PartImage : public PartRectangle
{
		Q_OBJECT

	public:
		PartImage(QETElementEditor *editor, QGraphicsItem *parent = nullptr);
		~PartImage() override;

	private:
		PartImage(const PartImage &);

	public:
		enum { Type = UserType + 1115 };
		int type() const override { return Type; }
		QString name() const override { return(QObject::tr("image", "element part name")); }
		QString xmlName() const override { return(QStringLiteral("image")); }

		const QDomElement toXml(QDomDocument &) const override;
		void fromXml(const QDomElement &) override;
		void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;
		QPainterPath shape() const override;
		QPainterPath shadowShape() const override;

		/**
			@brief Take the file this image shows.
			@param data the bytes of the file, as read from the disk
			@param format one of formats()
			@return false, and nothing changed, when the bytes are not a
			file of that format this program can draw
		*/
		bool setFile(const QByteArray &data, const QString &format);
		QByteArray fileData() const;
		QString fileFormat() const;
			/// @return true when there is nothing this program can draw
		bool isNull() const;

			/// @return the formats an image is kept in: png, jpg, svg
		static QStringList formats();
			/// @return the format of the file at @a path, by its suffix,
			/// empty when it is none of formats()
		static QString formatOfFile(const QString &path);
		/**
			@return the size the file says it has, millimetre, empty when
			it cannot be drawn.

			Read from the resolution written in a raster file and from
			the units of an SVG, which a datasheet image seldom has
			right: it is where the question about the real size starts,
			for its proportion, and not an answer.
		*/
		static QSizeF fileSizeMm(const QByteArray &data, const QString &format);
		/**
			@brief Paint the file @a data of @a format stretched over
			@a target, the way a part on the 2D tab shows it.
			@return false when there is nothing to paint
		*/
		static bool paintFile(QPainter *painter, const QRectF &target,
				      const QByteArray &data, const QString &format);

	protected:
		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

	private:
		static bool decode(const QByteArray &data, const QString &format,
				   QImage *image, std::unique_ptr<QSvgRenderer> *svg);
		static void paintDecoded(QPainter *painter, const QRectF &target,
					 const QImage &image, QSvgRenderer *svg);

		QByteArray m_data;
		QString m_format;
		QImage m_image;
		std::unique_ptr<QSvgRenderer> m_svg;
};

#endif
