/*
	Copyright 2006-2025 The QElectroTech Team
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
#ifndef PDF_LINKS_H
#define PDF_LINKS_H

#include <QList>
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <functional>

class QPdfEngine;
class Diagram;

/**
	Shared helper that turns a project's cross-references and folio reports
	into clickable internal hyperlinks in a Qt-generated PDF.  Used by both the
	GUI print path (ProjectPrintWindow) and the headless CLI export, each of
	which builds its own page geometry and passes it in — this code never
	computes the scene-to-page mapping itself.
*/
namespace PdfLinks {

	/**
		Geometry mapping for one rendered PDF page.  Each caller builds this
		from its OWN page setup (printer page layout vs QPdfWriter), since the
		device-pixel and point conversions differ between them.
	*/
	struct PageGeometry {
		/// scene coordinates -> device pixels (the same "fit" render() applied)
		QTransform sceneToDevice;
		/// device paint rectangle, in pixels (the page area)
		QRectF target;
		/// links whose rectangle falls outside this are dropped
		QRectF pageBounds;
		/// device pixels -> PDF points (replicates the engine's page matrix)
		std::function<QPointF(const QPointF &)> devToPdf;
		/// a diagram -> its source rectangle in scene pixels (for /FitR framing)
		std::function<QRectF(Diagram *)> sourceRectOf;

		/**
			A diagram -> the paint rectangle, in device pixels, of the page it
			was drawn on.

			Leave it unset when every page of the document has the same
			geometry, which is the case of a printer-driven export: the page
			comes from the printer and not from the sheet, so the page being
			drawn and the page being pointed at are interchangeable.

			Set it when the pages differ in size.  A link frames a rectangle on
			its TARGET page, and computing that rectangle with the geometry of
			the page currently being drawn scales it by the wrong factor: a
			jump from a large sheet to a smaller one then frames a region that
			is not on the target sheet at all.
		*/
		std::function<QRectF(Diagram *)> pageTargetOf;

		/**
			Device pixels -> PDF points on the page a given diagram was drawn
			on.

			Unset for the same reason as pageTargetOf, and with the same
			consequence when it should have been set: the conversion flips Y
			around the page height, so a target page shorter than the one being
			drawn receives every destination shifted up by the difference.
		*/
		std::function<QPointF(Diagram *, const QPointF &)> devToPdfOn;
	};

	/**
		Inject clickable cross-reference / folio-report hyperlinks for @p diagram
		into the current page of @p engine.  Each link is emitted as a URI
		annotation encoding the target page and a /FitR rectangle;
		convertUriToGoTo() then rewrites those into native internal GoTo actions.
	*/
	void injectCrossRefLinks(QPdfEngine *engine, Diagram *diagram,
							 const PageGeometry &geom,
							 const QMap<Diagram *, int> &pageMap,
							 const QString &outputFileName);

	/**
		Post-process a Qt-generated PDF file: rewrite every "/S /URI" link
		annotation into a native internal "/S /GoTo" action (page + /FitR or
		/Fit destination) and rebuild the xref table.  No-op if the file has no
		such annotations.
	*/
	void convertUriToGoTo(const QString &pdfPath);

	struct ComponentInfo {
		QString contents;
	};

	/**
		Post-process a Qt-generated PDF file: convert component-info placeholder
		link annotations (http://componentinfo.local/<N>) into invisible text
		annotations with the actual component info as /Contents.
	*/
	void convertComponentInfoAnnotations(const QString &pdfPath,
										const QList<ComponentInfo> &annotations);

	/// One line of the bookmark panel: what it reads, and what it opens.
	struct OutlineEntry {
		/// The label shown in the reader's navigation panel.
		QString title;
		/// 1-based page the entry opens, in the emitted document.
		int page = 0;
	};

	/**
		The label a bookmark carries for @p diagram, drawn on page @p page.

		It lives here, and not in each caller, because the printed window and
		the command line have to emit the same document for the same project:
		a title decided twice would differ the day one of the two changed.

		The sheet title comes first, since that is what the person who drew
		the project wrote on it. An untitled sheet falls back to the folio
		string its own title block resolves ("1/14"), and a sheet without one
		to the page number - both language-neutral on purpose, so that the
		file does not depend on which of the two paths emitted it: the command
		line runs before any translator is installed and the window runs after.
	*/
	QString outlineTitleOf(Diagram *diagram, int page);

	/**
		Post-process a Qt-generated PDF file: give it a document outline - the
		bookmark tree a reader lists in its navigation panel - with one flat
		entry per emitted sheet, each opening its page whole.

		Written over the bytes rather than asked of the engine because there
		is nothing to ask: QPdfEngine exposes drawHyperlink() and
		addFileAttachment() and no outline call whatsoever, in Qt5 or in Qt6.
		This is the same treatment convertUriToGoTo() gives the link actions,
		for the same reason.

		Does nothing when the file already carries an outline, so that a
		second call cannot leave the catalog pointing at two trees. Entries
		naming a page the document does not have are dropped rather than
		written: a narrowed export must not offer a bookmark that opens
		nothing.
	*/
	void injectOutline(const QString &pdfPath,
					   const QList<OutlineEntry> &entries);

}

#endif // PDF_LINKS_H
