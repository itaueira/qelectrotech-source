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
#ifndef PDFPROBE_H
#define PDFPROBE_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

/**
	Reads back a PDF the program has just written.

	It exists because the two things an emitted drawing has to do cannot be
	seen from the code that writes it: whether a word of the sheet can be
	found in the file, and whether a cross-reference lands where it says it
	does. Both are properties of the bytes, and only reading the bytes back
	answers them.

	It is not a PDF library and must not grow into one. It reads exactly what
	Qt's own PDF engine writes, which is a narrow and stable dialect:
	uncompressed object dictionaries, one flate-compressed content stream per
	page, one glyph per text-showing operator, and an uncompressed /ToUnicode
	map per font. Anything outside that dialect is reported as an error rather
	than guessed at - a probe that guesses turns a defect into a green run.
*/
namespace PdfProbe
{
	/// A clickable rectangle of a page, and where it sends the reader.
	struct Link
	{
		/// 1-based page carrying the rectangle.
		int source_page = 0;
		/// 1-based page it jumps to; 0 when the action is not an internal jump.
		int target_page = 0;
		/// The clickable rectangle, in PDF points, on the source page.
		QRectF rect;
		/// The region framed on the target page; empty when the whole page is.
		QRectF destination;
	};

	/**
		A PDF file opened for reading.

		Every accessor is 1-based on pages, the way a reader numbers them and
		the way the export path numbers them in its own page map, so that a
		failure message can be compared with what a person sees on screen.
	*/
	class Document
	{
		public:
			explicit Document(const QString &file_path);

			bool isValid() const {return m_error.isEmpty();}
			/// Empty while isValid(); says what went wrong otherwise.
			QString error() const {return m_error;}

			int pageCount() const {return m_pages.size();}
			/// The page box, in PDF points. Null for an out-of-range page.
			QSizeF pageSize(int page) const;

			/**
				The text of a page, in the order the page draws it.

				Decoded through the /ToUnicode map of the font that drew each
				glyph, which is the same table a reader's search box uses. A
				page whose text was drawn as curves comes back empty, and that
				is the point: it is how "the file is searchable" is told apart
				from "the file looks right".
			*/
			QString text(int page) const;
			/// Every page's text, in page order, separated by a line break.
			QString text() const;

			/// Whether a page draws with a font that carries a /ToUnicode map.
			bool hasMappedFont(int page) const;

			QList<Link> links() const {return m_links;}

		private:
			struct Page
			{
				int object = 0;
				QRectF box;
				int contents = 0;
				int resources = 0;
				int annots = 0;
			};

			QByteArray object(int number) const;
			QByteArray streamOf(int number) const;
			QHash<QString, int> fontsOf(int resources_object) const;
			const QHash<int, uint> &toUnicodeOf(int font_object) const;
			void readObjects();
			void readPages();
			void readLinks();
			int pageIndexOfObject(int object) const;

			QByteArray m_data;
			QString m_error;
			QHash<int, int> m_offsets;
			QVector<Page> m_pages;
			QList<Link> m_links;
			// The engine names its font again before every single glyph, so
			// a page of a thousand characters would ask for the same map a
			// thousand times. It is read once and kept.
			mutable QHash<int, QHash<int, uint>> m_to_unicode;
	};

	/**
		The same text with every space, tabulation and line break removed.

		Needed because the engine does not write the spaces between words: it
		positions each glyph and lets the gap speak. What comes back is
		therefore "TABLEAUELECTRIQUE", and comparing the squeezed forms is
		what makes a search for a phrase mean the same thing on both sides.
	*/
	QString squeezed(const QString &text);
}

#endif // PDFPROBE_H
