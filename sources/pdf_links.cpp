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
#include "pdf_links.h"

#include "diagram.h"
#include "qetgraphicsitem/crossrefitem.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/elementtextitemgroup.h"

// Private Qt PDF engine for drawHyperlink() — not public API.
// Availability of Qt::GuiPrivate is verified at configure time in CMakeLists.txt.
#include <private/qpdf_p.h>

#include <QByteArray>
#include <QFile>
#include <QGraphicsTextItem>
#include <QList>
#include <QRegularExpression>
#include <QUrl>
#include <QVector>

namespace PdfLinks {

namespace {

/**
	The object number of every page of @p data, in document order.

	Read from the page tree (/Type /Pages -> /Kids [ N 0 R ... ]).  This is
	reliable; scanning raw bytes for "/Type /Page" is NOT: that marker also
	occurs inside content streams, and a forward lookahead wrongly tags
	neighbouring objects (it found 280 "pages" for a 137-page document).  Qt
	writes a single, flat /Kids array listing every page.
*/
QVector<int> collectPageObjects(const QByteArray &data)
{
	QVector<int> pageObjs;
	int pagesPos = data.indexOf("/Type /Pages");
	int kidsPos  = (pagesPos == -1) ? -1 : data.indexOf("/Kids", pagesPos);
	int lb       = (kidsPos  == -1) ? -1 : data.indexOf('[', kidsPos);
	int rb       = (lb       == -1) ? -1 : data.indexOf(']', lb);
	if (lb != -1 && rb != -1 && rb > lb) {
		const QString kids =
			QString::fromLatin1(data.mid(lb + 1, rb - lb - 1));
		QRegularExpression re(QStringLiteral("(\\d+)\\s+\\d+\\s+R"));
		auto it = re.globalMatch(kids);
		while (it.hasNext()) {
			int objNum = it.next().captured(1).toInt();
			if (objNum > 0) pageObjs.append(objNum);
		}
	}
	return pageObjs;
}

/**
	Where every object of @p body begins, by object number.

	Measured rather than read from the table that is already in the file,
	because the table is exactly what the caller is about to replace: each of
	these passes rewrites objects in place, which moves everything after them.
*/
QMap<int, int> objectOffsets(const QByteArray &body)
{
	QMap<int, int> offsets;
	const QByteArray objMarker(" 0 obj");
	int p = 0;
	while ((p = body.indexOf(objMarker, p)) != -1) {
		int numStart = p - 1;
		while (numStart > 0
			   && body[numStart - 1] != '\n' && body[numStart - 1] != '\r')
			--numStart;
		QByteArray numStr = body.mid(numStart, p - numStart).trimmed();
		bool ok = false;
		int objNum = numStr.toInt(&ok);
		if (ok && objNum > 0)
			offsets[objNum] = numStart;
		++p;
	}
	return offsets;
}

/**
	A cross-reference table covering object 1 up to the highest of @p offsets.

	A gap is written as a free entry rather than left out: the table is read
	positionally, so a missing line would shift every object after it.
*/
QByteArray buildXrefTable(const QMap<int, int> &offsets)
{
	const int maxObj = offsets.lastKey();
	QByteArray xref;
	xref += "xref\n";
	xref += "0 " + QByteArray::number(maxObj + 1) + "\n";
	xref += "0000000000 65535 f \n";
	for (int i = 1; i <= maxObj; ++i) {
		if (offsets.contains(i)) {
			xref += QByteArray::number(offsets[i]).rightJustified(10, '0')
				+ " 00000 n \n";
		} else {
			xref += "0000000000 65535 f \n";
		}
	}
	return xref;
}

/**
	@p text as a PDF text string: UTF-16BE behind a byte-order mark, written
	as a hexadecimal string.

	Hexadecimal spares the escaping of parentheses and backslashes, which a
	sheet title may well carry.  The byte-order mark is what tells a reader to
	decode UTF-16 instead of PDFDocEncoding (PDF 1.7, 7.9.2.2): without it an
	accented title comes back mangled in the bookmark panel.  A QString is
	already UTF-16, so every code unit is written as it stands and a surrogate
	pair needs no special case.
*/
QByteArray pdfTextString(const QString &text)
{
	QByteArray utf16be;
	utf16be.reserve(2 + text.size() * 2);
	utf16be.append('\xfe');
	utf16be.append('\xff');
	for (QChar c : text) {
		const ushort u = c.unicode();
		utf16be.append(static_cast<char>((u >> 8) & 0xFF));
		utf16be.append(static_cast<char>(u & 0xFF));
	}
	return "<" + utf16be.toHex() + ">";
}

} // namespace

void injectCrossRefLinks(QPdfEngine *engine, Diagram *diagram,
						 const PageGeometry &geom,
						 const QMap<Diagram *, int> &pageMap,
						 const QString &outputFileName)
{
	if (!engine || !diagram)
		return;

	const QTransform &fit       = geom.sceneToDevice;
	const QRectF      &target   = geom.target;
	const QRectF      &pageBounds = geom.pageBounds;

	// Compute, in PDF points on its OWN page, the rectangle to frame for a
	// target element (used as a /FitR destination so the link zooms onto it).
	auto destRectPdf = [&](Element *tgt) -> QRectF {
		Diagram *dg = tgt ? tgt->diagram() : nullptr;
		if (!dg) return QRectF();
		const QRectF srcT = geom.sourceRectOf(dg);
		if (srcT.width() <= 0.0 || srcT.height() <= 0.0) return QRectF();

		// The page to frame on is the TARGET's page, not the one being drawn.
		// They are the same page size in a printer-driven export and are not
		// when each page is cut to its own sheet, which is why the caller gets
		// to say; see PageGeometry::pageTargetOf.
		QRectF pageT = target;
		if (geom.pageTargetOf) {
			const QRectF own = geom.pageTargetOf(dg);
			if (own.width() > 0.0 && own.height() > 0.0)
				pageT = own;
		}

		const qreal sT = qMin(pageT.width()  / srcT.width(),
							  pageT.height() / srcT.height());
		QTransform fitT;
		fitT.translate(pageT.x(), pageT.y());
		fitT.scale(sT, sT);
		fitT.translate(-srcT.x(), -srcT.y());

		QRectF elemScene = tgt->mapRectToScene(tgt->boundingRect());
		// Frame the element with a little context, and enforce a minimum
		// framed size so tiny contacts don't zoom in extremely.
		const qreal pad = 25.0;
		elemScene.adjust(-pad, -pad, pad, pad);
		const qreal minSide = 160.0;
		if (elemScene.width()  < minSide)
			elemScene.adjust(-(minSide - elemScene.width())  / 2.0, 0,
							  (minSide - elemScene.width())  / 2.0, 0);
		if (elemScene.height() < minSide)
			elemScene.adjust(0, -(minSide - elemScene.height()) / 2.0,
							 0,  (minSide - elemScene.height()) / 2.0);

		const QRectF devT = fitT.mapRect(elemScene);
		// Same reasoning as pageT above: the Y flip is around the height of
		// the page the destination lives on.
		const QPointF a = geom.devToPdfOn ? geom.devToPdfOn(dg, devT.topLeft())
										  : geom.devToPdf(devT.topLeft());
		const QPointF b = geom.devToPdfOn
						  ? geom.devToPdfOn(dg, devT.bottomRight())
						  : geom.devToPdf(devT.bottomRight());
		return QRectF(QPointF(qMin(a.x(), b.x()), qMin(a.y(), b.y())),
					  QPointF(qMax(a.x(), b.x()), qMax(a.y(), b.y())));
	};

	auto injectLink = [&](const QRectF &sceneRect, Element *targetElmt) {
		if (!targetElmt || !targetElmt->diagram()) return;
		const int targetPage = pageMap.value(targetElmt->diagram(), -1);
		if (targetPage < 1) return;
		const QRectF devRect = fit.mapRect(sceneRect);
		if (!devRect.isValid() || !pageBounds.intersects(devRect)) return;

		QString frag = QString("page=%1").arg(targetPage);
		const QRectF d = destRectPdf(targetElmt);   // /FitR L_B_R_T
		if (d.isValid())
			frag += QString("&fitr=%1_%2_%3_%4")
				.arg(qRound(d.left())).arg(qRound(d.top()))
				.arg(qRound(d.right())).arg(qRound(d.bottom()));

		QUrl url = QUrl::fromLocalFile(outputFileName);
		url.setFragment(frag);
		engine->drawHyperlink(devRect, url);
	};

	for (auto *item : diagram->items()) {

		// --- CrossRefItem links ---
		if (auto *xref = dynamic_cast<CrossRefItem*>(item)) {
			for (auto it = xref->hoveredContactsMap().begin();
				 it != xref->hoveredContactsMap().end(); ++it)
			{
				Element *targetElmt = it.key();
				if (!targetElmt || !targetElmt->diagram()) continue;
				// it.value() is in the CrossRefItem's LOCAL coords -> scene
				injectLink(xref->mapRectToScene(it.value()), targetElmt);
			}
			continue;
		}

		// --- Folio report links (DynamicElementTextItem) ---
		if (auto *deti = dynamic_cast<DynamicElementTextItem*>(item)) {
			Element *parent = deti->parentElement();
			if (!parent) continue;

			// (a) Report element : label -> linked report on another folio
			if (parent->linkType() & Element::AllReport) {
				if (parent->linkedElements().isEmpty()) continue;

				bool showsLabel =
					(deti->textFrom() == DynamicElementTextItem::ElementInfo
					 && deti->infoName() == QLatin1String("label")) ||
					(deti->textFrom() == DynamicElementTextItem::CompositeText
					 && deti->compositeText().contains(QStringLiteral("%{label}")));
				if (!showsLabel) continue;

				Element *targetElmt = parent->linkedElements().first();
				if (!targetElmt || !targetElmt->diagram()) continue;

				injectLink(deti->mapRectToScene(deti->boundingRect()), targetElmt);
				continue;
			}

			// (b) Slave element : the "(folio-pos)" text -> master element
			if (parent->linkType() == Element::Slave) {
				QGraphicsTextItem *sx = deti->slaveXrefItem();
				Element *master = deti->masterElement();
				if (sx && master && master->diagram()) {
					injectLink(sx->mapRectToScene(sx->boundingRect()), master);
				}
				continue;
			}
			continue;
		}

		// --- Slave cross-reference carried by a grouped text ---
		if (auto *grp = dynamic_cast<ElementTextItemGroup*>(item)) {
			Element *parent = grp->parentElement();
			if (!parent || parent->linkType() != Element::Slave) continue;
			if (parent->linkedElements().isEmpty()) continue;
			QGraphicsTextItem *sx = grp->slaveXrefItem();
			if (!sx) continue;
			Element *master = parent->linkedElements().first();
			if (!master || !master->diagram()) continue;
			injectLink(sx->mapRectToScene(sx->boundingRect()), master);
			continue;
		}
	}
}

void convertUriToGoTo(const QString &pdfPath)
{
	// --- 1. Read raw bytes ---
	QFile f(pdfPath);
	if (!f.open(QIODevice::ReadOnly)) return;
	QByteArray data = f.readAll();
	f.close();

	// --- 2. Collect page object numbers in document order ---
	const QVector<int> pageObjs = collectPageObjects(data);
	if (pageObjs.isEmpty()) return;  // nothing to do

	// --- 3. Replace URI annotations with GoTo ---
	// Pattern (Qt always writes exactly this):
	//   /S /URI\n/URI (file:///...<anything>#page=N)\n
	// or (older patches without file://):
	//   /S /URI\n/URI (page=N)\n
	bool changed = false;
	{
		// We do a manual scan to handle variable-length replacements.
		QByteArray out;
		out.reserve(data.size());

		const QByteArray sUri  = "/S /URI\n/URI (";
		const QByteArray sGoTo = "/S /GoTo\n/D [";
		int pos = 0;

		while (pos < data.size()) {
			int found = data.indexOf(sUri, pos);
			if (found == -1) {
				out.append(data.mid(pos));
				break;
			}

			// Copy everything up to the match
			out.append(data.mid(pos, found - pos));

			// Find closing ')' of the URI value
			int uriStart = found + sUri.size();
			int closeParen = data.indexOf(")\n", uriStart);
			if (closeParen == -1) {
				// Malformed — copy rest verbatim
				out.append(data.mid(found));
				pos = data.size();
				break;
			}

			QByteArray uriVal = data.mid(uriStart, closeParen - uriStart);

			// Skip component-info annotations — handled by
			// convertComponentInfoAnnotations() in a separate pass.
			if (uriVal.startsWith("componentinfo://")
				|| uriVal.startsWith("http://componentinfo.local/")) {
				out.append(data.mid(found, closeParen + 1 - found));
				pos = closeParen + 1;
				continue;
			}

			// Extract page number: look for #page=N or bare page=N
			int pageNum = -1;
			int hashPos = uriVal.lastIndexOf("#page=");
			int digitStart = -1;
			if (hashPos != -1) {
				digitStart = hashPos + 6;
			} else if (uriVal.startsWith("page=")) {
				digitStart = 5;
			}
			if (digitStart != -1) {
				// Take only the leading digits: the fragment may carry extra
				// parameters after the page number (e.g. "22&fitr=15_489_..."),
				// and QByteArray::toInt() would fail on the whole remainder.
				int e = digitStart;
				while (e < uriVal.size()
					   && uriVal[e] >= '0' && uriVal[e] <= '9')
					++e;
				if (e > digitStart)
					pageNum = uriVal.mid(digitStart, e - digitStart).toInt();
			}

			if (pageNum >= 1 && pageNum <= pageObjs.size()) {
				// Valid page reference — emit GoTo action.
				int pageObjNum = pageObjs[pageNum - 1];

				// Optional precise destination: &fitr=Left_Bottom_Right_Top
				// (integer PDF points). If present -> /FitR (frame the element);
				// otherwise -> /Fit (whole page, top).
				QByteArray dest = " /Fit]";
				int fr = uriVal.indexOf("fitr=");
				if (fr != -1) {
					QByteArray rest = uriVal.mid(fr + 5);
					// stop at first char that is not part of the number list
					int end = 0;
					while (end < rest.size()
						   && ((rest[end] >= '0' && rest[end] <= '9')
							   || rest[end] == '_' || rest[end] == '-'))
						++end;
					QList<QByteArray> parts = rest.left(end).split('_');
					if (parts.size() == 4) {
						dest = " /FitR " + parts[0] + " " + parts[1] + " "
							   + parts[2] + " " + parts[3] + "]";
					}
				}

				QByteArray goTo = sGoTo
					+ QByteArray::number(pageObjNum)
					+ " 0 R" + dest;
				out.append(goTo);
				changed = true;
			} else {
				// Unknown page — keep original URI
				out.append(sUri);
				out.append(uriVal);
				out.append(')');
			}

			pos = closeParen + 1;  // skip past ')'
		}

		if (!changed) return;  // nothing was replaced
		data = out;
	}

	// --- 4. Rebuild xref table ---
	// Find start of existing xref (last occurrence)
	int xrefStart = data.lastIndexOf("\nxref\n");
	if (xrefStart == -1) xrefStart = data.lastIndexOf("\nxref ");
	if (xrefStart == -1) return;  // malformed PDF
	++xrefStart;  // skip the leading '\n'

	QByteArray body = data.left(xrefStart);

	// Collect all object offsets from the body
	QMap<int, int> offsets;  // objNum -> byte offset
	{
		const QByteArray objMarker = " 0 obj";
		int pos = 0;
		while ((pos = body.indexOf(objMarker, pos)) != -1) {
			int numStart = pos - 1;
			while (numStart > 0 && body[numStart-1] != '\n' && body[numStart-1] != '\r')
				--numStart;
			QByteArray numStr = body.mid(numStart, pos - numStart).trimmed();
			bool ok = false;
			int objNum = numStr.toInt(&ok);
			if (ok && objNum > 0)
				offsets[objNum] = numStart;
			++pos;
		}
	}

	if (offsets.isEmpty()) return;

	int maxObj = offsets.lastKey();

	// Build xref table
	QByteArray xref;
	xref += "xref\n";
	xref += "0 " + QByteArray::number(maxObj + 1) + "\n";
	xref += "0000000000 65535 f \n";
	for (int i = 1; i <= maxObj; ++i) {
		if (offsets.contains(i)) {
			xref += QByteArray::number(offsets[i]).rightJustified(10, '0')
				+ " 00000 n \n";
		} else {
			xref += "0000000000 65535 f \n";
		}
	}

	// Find trailer dict from the original xref section
	int trailerPos = data.indexOf("trailer", xrefStart);
	int trailerEnd = -1;
	if (trailerPos != -1) {
		trailerEnd = data.indexOf("%%EOF", trailerPos);
		if (trailerEnd != -1) trailerEnd += 5;
	}

	QByteArray trailer;
	if (trailerPos != -1 && trailerEnd != -1)
		trailer = data.mid(trailerPos, trailerEnd - trailerPos);
	else
		trailer = "trailer\n<<>>\n%%EOF";

	int newXrefOffset = body.size();

	QByteArray result;
	result.reserve(body.size() + xref.size() + trailer.size() + 30);
	result += body;
	result += xref;
	result += trailer;
	result += "\nstartxref\n";
	result += QByteArray::number(newXrefOffset);
	result += "\n%%EOF\n";

	// --- 5. Write back ---
	QFile out(pdfPath);
	if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
	out.write(result);
	out.close();
}

void convertComponentInfoAnnotations(const QString &pdfPath,
									const QList<ComponentInfo> &annotations)
{
	if (annotations.isEmpty()) return;

	QFile f(pdfPath);
	if (!f.open(QIODevice::ReadOnly)) return;
	QByteArray data = f.readAll();
	f.close();

	const QByteArray markerPrefix("http://componentinfo.local/");
	if (!data.contains(markerPrefix)) return;

	int xrefStart = data.lastIndexOf("\nxref\n");
	if (xrefStart == -1) xrefStart = data.lastIndexOf("\nxref ");
	if (xrefStart == -1) return;
	++xrefStart;
	QByteArray body = data.left(xrefStart);

	// Pre-scan body for highest object number
	int maxObjNum = 0;
	{
		const QByteArray objMarker(" 0 obj");
		int p = 0;
		while ((p = body.indexOf(objMarker, p)) != -1) {
			int numStart = p - 1;
			while (numStart > 0 && body[numStart - 1] != '\n' && body[numStart - 1] != '\r')
				--numStart;
			QByteArray numStr = body.mid(numStart, p - numStart).trimmed();
			bool ok = false;
			int num = numStr.toInt(&ok);
			if (ok && num > maxObjNum)
				maxObjNum = num;
			++p;
		}
	}
	// Empty Form XObject used as invisible appearance for all annotations.
	// All annotations reference this single object.
	int emptyXObjNum = maxObjNum + 1;

	QByteArray out;
	out.reserve(data.size());

	int pos = 0;
	bool anyConverted = false;

	while (pos < body.size()) {
		// Find next indexed marker: http://componentinfo.local/<N>
		int markerPos = body.indexOf(markerPrefix, pos);
		if (markerPos == -1) {
			out.append(body.mid(pos));
			break;
		}

		// Extract index from marker URL
		int idxStart = markerPos + markerPrefix.size();
		int idxEnd = idxStart;
		while (idxEnd < body.size() && body[idxEnd] >= '0' && body[idxEnd] <= '9')
			++idxEnd;
		if (idxEnd == idxStart) {
			// No index — skip malformed marker
			out.append(body.mid(pos, markerPos + markerPrefix.size() - pos));
			pos = markerPos + markerPrefix.size();
			continue;
		}
		int annotIndex = body.mid(idxStart, idxEnd - idxStart).toInt();

		int uriOpen = body.lastIndexOf("/URI (", markerPos);
		int closeParen = (uriOpen != -1) ? body.indexOf(')', markerPos) : -1;
		if (uriOpen == -1 || closeParen == -1 || uriOpen < pos) {
			out.append(body.mid(pos, idxEnd - pos));
			pos = idxEnd;
			continue;
		}

		// Find /S /URI before /URI (
		int sUriPos = body.lastIndexOf("/S /URI", uriOpen);

		// Find the /A << that opens the action dict containing /S /URI.
		// Qt always writes: /A <<\n/S /URI\n/URI (...)\n>>\n>>
		int aDictOpen = (sUriPos != -1)
			? body.lastIndexOf("/A <<", sUriPos)
			: -1;

		// If /A << is not found, leave this annotation untouched
		if (aDictOpen == -1 || aDictOpen < pos) {
			out.append(body.mid(pos, closeParen + 1 - pos));
			pos = closeParen + 1;
			continue;
		}

		// Validate index
		if (annotIndex < 0 || annotIndex >= annotations.size()) {
			// Index out of range — keep original annotation intact
			out.append(body.mid(pos, closeParen + 1 - pos));
			pos = closeParen + 1;
			continue;
		}

		// Copy annotation dict header up to /A <<
		out.append(body.mid(pos, aDictOpen - pos));

		QByteArray contents = annotations[annotIndex].contents.toUtf8();

		// Replace /Subtype /Link with /Subtype /Text, bounded to the enclosing
		// PDF object.  Anchoring to " 0 obj" prevents hitting /Subtype /Link
		// from a cross-ref annotation that sits between the previous marker
		// and the current annotation on the same page.
		int objStart = body.lastIndexOf(" 0 obj", aDictOpen);
		int subTypeInBody = (objStart != -1)
			? body.indexOf("/Subtype /Link", objStart) : -1;
		int subTypePos = (subTypeInBody != -1 && subTypeInBody < aDictOpen)
			? out.size() - (aDictOpen - subTypeInBody) : -1;
		if (subTypePos != -1)
			// Acrobat draws its own sticky-note icon for /Subtype /Text
			// whatever /AP says (verified on Reader 5.0). /Square with no
			// /C and no /IC draws nothing anywhere, and is still a markup
			// annotation, so the /Contents popup is unaffected.
			out.replace(subTypePos, 14, "/Subtype /Square");

		// Encode as UTF-16BE hex with BOM for proper Unicode support (Umlauten etc.).
		// PDF spec: hex strings starting with FE FF are interpreted as UTF-16BE.
		QByteArray utf16be;
		utf16be.append('\xfe');
		utf16be.append('\xff');
		{
			for (int i = 0; i < contents.size(); ) {
				ushort cp = 0;
				uchar c = static_cast<uchar>(contents.at(i));
				if (c < 0x80) {
					cp = c;
					++i;
				} else if ((c & 0xE0) == 0xC0 && i + 1 < contents.size()) {
					cp = ((c & 0x1F) << 6)
					   | (static_cast<uchar>(contents.at(i + 1)) & 0x3F);
					i += 2;
				} else if ((c & 0xF0) == 0xE0 && i + 2 < contents.size()) {
					cp = ((c & 0x0F) << 12)
					   | ((static_cast<uchar>(contents.at(i + 1)) & 0x3F) << 6)
					   | (static_cast<uchar>(contents.at(i + 2)) & 0x3F);
					i += 3;
				} else {
					cp = 0xFFFD; // replacement character
					++i;
				}
				utf16be.append(static_cast<char>((cp >> 8) & 0xFF));
				utf16be.append(static_cast<char>(cp & 0xFF));
			}
		}

		out += "/Contents <";
		out += utf16be.toHex();
		out += ">\n/AP << /N " + QByteArray::number(emptyXObjNum) + " 0 R >>\n";

		// Skip the entire /A << ... >> action dict.
		// After closeParen ')' we have: \n>>  (closes /A dict)  \n>>  (closes annot dict)
		int actionDictClose = body.indexOf(">>", closeParen + 1);
		if (actionDictClose != -1) {
			pos = actionDictClose + 2;  // skip past >> that closes /A dict
		} else {
			pos = closeParen + 1;
		}
		anyConverted = true;
	}

	if (!anyConverted) return;

	// Append empty Form XObject (shared by all annotations for invisible appearance)
	QByteArray emptyXObj;
	emptyXObj += QByteArray::number(emptyXObjNum) + " 0 obj\n";
	emptyXObj += "<< /Type /XObject /Subtype /Form /BBox [0 0 0 0] /Length 0 >>\n";
	emptyXObj += "stream\nendstream\n";
	emptyXObj += "endobj\n";
	out += emptyXObj;

	// Rebuild xref table
	QMap<int, int> offsets;
	{
		const QByteArray objMarker(" 0 obj");
		int p = 0;
		while ((p = out.indexOf(objMarker, p)) != -1) {
			int numStart = p - 1;
			while (numStart > 0 && out[numStart - 1] != '\n' && out[numStart - 1] != '\r')
				--numStart;
			QByteArray numStr = out.mid(numStart, p - numStart).trimmed();
			bool ok = false;
			int objNum = numStr.toInt(&ok);
			if (ok && objNum > 0)
				offsets[objNum] = numStart;
			++p;
		}
	}

	if (offsets.isEmpty()) return;

	int maxObj = offsets.lastKey();

	QByteArray xref;
	xref += "xref\n";
	xref += "0 " + QByteArray::number(maxObj + 1) + "\n";
	xref += "0000000000 65535 f \n";
	for (int i = 1; i <= maxObj; ++i) {
		if (offsets.contains(i)) {
			xref += QByteArray::number(offsets[i]).rightJustified(10, '0')
				+ " 00000 n \n";
		} else {
			xref += "0000000000 65535 f \n";
		}
	}

	// Copy trailer and bump /Size to account for the new XObject
	QByteArray trailer;
	{
		int tPos = data.indexOf("trailer", xrefStart);
		if (tPos != -1) {
			int tEnd = data.indexOf("%%EOF", tPos);
			if (tEnd != -1) tEnd += 5;
			if (tEnd != -1)
				trailer = data.mid(tPos, tEnd - tPos);
		}
	}
	if (trailer.isEmpty())
		trailer = "trailer\n<<>>\n%%EOF";

	// Bump /Size: original was maxObjNum+1, now it's emptyXObjNum+1
	{
		int sizePos = trailer.indexOf("/Size ");
		if (sizePos != -1) {
			int numStart = sizePos + 6;
			int numEnd = numStart;
			while (numEnd < trailer.size() && trailer[numEnd] >= '0' && trailer[numEnd] <= '9')
				++numEnd;
			if (numEnd > numStart) {
				trailer.replace(numStart, numEnd - numStart,
								QByteArray::number(emptyXObjNum + 1));
			}
		}
	}

	// Remove duplicate startxref if present in copied trailer
	{
		int stPos = trailer.indexOf("\nstartxref\n");
		if (stPos != -1)
			trailer = trailer.left(stPos);
		// Ensure trailer ends with %%EOF
		if (!trailer.endsWith("%%EOF\n"))
			trailer += "\n%%EOF\n";
	}

	QByteArray result;
	result.reserve(out.size() + xref.size() + trailer.size() + 64);
	result += out;
	int newXrefOffset = out.size();
	result += xref;
	result += trailer;
	result += "\nstartxref\n";
	result += QByteArray::number(newXrefOffset);
	result += "\n%%EOF\n";

	QFile outF(pdfPath);
	if (!outF.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
	outF.write(result);
	outF.close();
}

QString outlineTitleOf(Diagram *diagram, int page)
{
	if (diagram) {
		// simplified() because the panel shows one line per entry: a title
		// carrying a line break would be read as far as the break and no
		// further, which reads as a truncated sheet name.
		const QString title = diagram->title().simplified();
		if (!title.isEmpty())
			return title;
		const QString folio =
			diagram->border_and_titleblock.finalfolio().simplified();
		if (!folio.isEmpty())
			return folio;
	}
	return QString::number(page);
}

void injectOutline(const QString &pdfPath, const QList<OutlineEntry> &entries)
{
	if (entries.isEmpty()) return;

	// --- 1. Read raw bytes ---
	QFile f(pdfPath);
	if (!f.open(QIODevice::ReadOnly)) return;
	QByteArray data = f.readAll();
	f.close();

	// Never lay a second tree over a first: the catalog would end up with two
	// /Outlines keys and a reader would follow whichever it parsed last.
	if (data.contains("/Type /Outlines")) return;

	// --- 2. The pages the entries can point at ---
	const QVector<int> pageObjs = collectPageObjects(data);
	if (pageObjs.isEmpty()) return;

	// Everything up to the existing cross-reference table; the table itself is
	// rebuilt at the end, because the objects appended below are not in it and
	// the catalog above grows by a key, which moves what follows it.
	int xrefStart = data.lastIndexOf("\nxref\n");
	if (xrefStart == -1) xrefStart = data.lastIndexOf("\nxref ");
	if (xrefStart == -1) return;  // malformed PDF
	++xrefStart;                  // skip the leading '\n'
	QByteArray body = data.left(xrefStart);

	// The catalog is where a reader looks for the tree. Without the key the
	// objects written below are unreachable and the file is merely bigger.
	int catPos = body.indexOf("/Type /Catalog");
	int catLen = 14;
	if (catPos == -1) {
		catPos = body.indexOf("/Type/Catalog");
		catLen = 13;
	}
	if (catPos == -1) return;

	// Keep only what this document can honour, in the order given. A narrowed
	// export - "folios=12,13,40" - has fewer pages than the project has
	// sheets, and a bookmark onto a page that was left out opens nothing.
	QList<OutlineEntry> kept;
	for (const OutlineEntry &e : entries) {
		if (e.page >= 1 && e.page <= pageObjs.size())
			kept.append(e);
	}
	if (kept.isEmpty()) return;

	const QMap<int, int> existing = objectOffsets(body);
	if (existing.isEmpty()) return;

	const int rootObj   = existing.lastKey() + 1;
	const int firstItem = rootObj + 1;
	const int count     = kept.size();

	// --- 3. Point the catalog at the tree ---
	// Inserted immediately after the /Type key rather than at the end of the
	// dictionary: the engine does not write the closing ">>" of the catalog
	// at a place these bytes can recognise reliably, and a key may be added
	// anywhere in a dictionary.
	QByteArray catalogKeys =
		"\n/Outlines " + QByteArray::number(rootObj) + " 0 R";
	// And open the reader on that panel. Writing the tree without this leaves
	// it to the reader's own default, which is usually to show nothing - and
	// a navigation panel nobody finds is the complaint this answers.
	if (!body.contains("/PageMode"))
		catalogKeys += "\n/PageMode /UseOutlines";
	body.insert(catPos + catLen, catalogKeys);

	// --- 4. The tree itself: one flat level, one entry per emitted sheet ---
	QByteArray objects;
	objects += QByteArray::number(rootObj) + " 0 obj\n<<\n";
	objects += "/Type /Outlines\n";
	objects += "/First " + QByteArray::number(firstItem) + " 0 R\n";
	objects += "/Last "
			   + QByteArray::number(firstItem + count - 1) + " 0 R\n";
	// A positive /Count is what makes the panel come up with the sheets
	// already listed instead of folded under a single root.
	objects += "/Count " + QByteArray::number(count) + "\n";
	objects += ">>\nendobj\n";

	for (int i = 0; i < count; ++i) {
		const int self = firstItem + i;
		objects += QByteArray::number(self) + " 0 obj\n<<\n";
		objects += "/Title " + pdfTextString(kept.at(i).title) + "\n";
		objects += "/Parent " + QByteArray::number(rootObj) + " 0 R\n";
		if (i > 0)
			objects += "/Prev " + QByteArray::number(self - 1) + " 0 R\n";
		if (i < count - 1)
			objects += "/Next " + QByteArray::number(self + 1) + " 0 R\n";
		// /Fit and not /FitR: a bookmark opens a whole sheet. It is the same
		// destination convertUriToGoTo() writes for a link with no rectangle
		// to frame, so both ways of reaching a folio land on it alike.
		objects += "/Dest ["
				   + QByteArray::number(pageObjs.at(kept.at(i).page - 1))
				   + " 0 R /Fit]\n";
		objects += ">>\nendobj\n";
	}

	QByteArray out = body + objects;

	// --- 5. Rebuild the cross-reference table ---
	const QMap<int, int> offsets = objectOffsets(out);
	if (offsets.isEmpty()) return;
	const QByteArray xref = buildXrefTable(offsets);

	// --- 6. Carry the trailer over, with /Size grown by what was appended ---
	QByteArray trailer;
	{
		int tPos = data.indexOf("trailer", xrefStart);
		if (tPos != -1) {
			int tEnd = data.indexOf("%%EOF", tPos);
			if (tEnd != -1)
				trailer = data.mid(tPos, tEnd + 5 - tPos);
		}
	}
	if (trailer.isEmpty())
		trailer = "trailer\n<<>>\n%%EOF";

	{
		int sizePos = trailer.indexOf("/Size ");
		if (sizePos != -1) {
			int numStart = sizePos + 6;
			int numEnd = numStart;
			while (numEnd < trailer.size()
				   && trailer[numEnd] >= '0' && trailer[numEnd] <= '9')
				++numEnd;
			if (numEnd > numStart) {
				trailer.replace(numStart, numEnd - numStart,
								QByteArray::number(offsets.lastKey() + 1));
			}
		}
	}

	// The copied trailer still carries the offset of the table it came with,
	// which is now wrong; the one appended below is the last in the file and
	// is the one a reader takes.
	{
		int stPos = trailer.indexOf("\nstartxref\n");
		if (stPos != -1)
			trailer = trailer.left(stPos);
		if (!trailer.endsWith("%%EOF\n"))
			trailer += "\n%%EOF\n";
	}

	QByteArray result;
	result.reserve(out.size() + xref.size() + trailer.size() + 64);
	const int newXrefOffset = out.size();
	result += out;
	result += xref;
	result += trailer;
	result += "\nstartxref\n";
	result += QByteArray::number(newXrefOffset);
	result += "\n%%EOF\n";

	// --- 7. Write back ---
	QFile outF(pdfPath);
	if (!outF.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
	outF.write(result);
	outF.close();
}

} // namespace PdfLinks
