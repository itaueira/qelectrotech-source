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
#include "pdfprobe.h"

#include <QFile>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>

namespace
{
	/**
		A flate stream turned back into bytes.

		qUncompress() wants the uncompressed size in the first four bytes,
		which a PDF stream does not carry; a guess is enough, because zlib
		asks for more room when the guess is short and the call resizes the
		result down when it is long.
	*/
	QByteArray inflate(const QByteArray &deflated)
	{
		if (deflated.isEmpty()) {
			return QByteArray();
		}

		int guess = int(deflated.size()) * 8;
		if (guess < 4096) {
			guess = 4096;
		}

		QByteArray framed(4, '\0');
		framed[0] = char((guess >> 24) & 0xFF);
		framed[1] = char((guess >> 16) & 0xFF);
		framed[2] = char((guess >>  8) & 0xFF);
		framed[3] = char( guess        & 0xFF);
		framed.append(deflated);

		return qUncompress(framed);
	}

	/// The value of a /Name <n> 0 R reference of a dictionary, 0 when absent.
	int reference(const QByteArray &dictionary, const char *key)
	{
		const QRegularExpression re(
			QStringLiteral("%1\\s+(\\d+)\\s+\\d+\\s+R")
				.arg(QString::fromLatin1(key)));
		const QRegularExpressionMatch m =
			re.match(QString::fromLatin1(dictionary));
		return m.hasMatch() ? m.captured(1).toInt() : 0;
	}
}

namespace PdfProbe
{

Document::Document(const QString &file_path)
{
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly)) {
		m_error = QStringLiteral("cannot open %1").arg(file_path);
		return;
	}
	m_data = file.readAll();
	file.close();

	if (!m_data.startsWith("%PDF-")) {
		m_error = QStringLiteral("%1 is not a PDF").arg(file_path);
		return;
	}

	readObjects();
	readPages();
	if (!m_error.isEmpty()) {
		return;
	}
	readLinks();
}

/*
	Where every object begins.

	The offsets could be read from the cross-reference table, and are not: the
	table is rebuilt by hand after the link annotations are rewritten, so
	trusting it here would make the probe blind to the one thing it is meant
	to watch. Scanning the body answers from the bytes themselves.
*/
void Document::readObjects()
{
	const QByteArray marker("0 obj");
	int at = 0;
	while ((at = m_data.indexOf(marker, at)) != -1) {
		// Walk back over " <generation>" and the object number.
		int end = at - 1;
		while (end > 0 && m_data.at(end) == ' ') {
			--end;
		}
		int start = end;
		while (start >= 0 && m_data.at(start) >= '0'
		       && m_data.at(start) <= '9') {
			--start;
		}
		// A definition starts a line. Without that, the same six bytes
		// occurring by chance inside a compressed stream would claim an
		// object number and hide the real definition of it.
		const bool starts_a_line =
			start < 0
			|| m_data.at(start) == '\n'
			|| m_data.at(start) == '\r';
		if (start != end && starts_a_line) {
			const int number =
				m_data.mid(start + 1, end - start).toInt();
			if (number > 0 && !m_offsets.contains(number)) {
				m_offsets.insert(number, start + 1);
			}
		}
		at += marker.size();
	}
}

/*
	The body of one object, dictionary and stream alike.

	The end of an object is its "endobj", except that a compressed stream can
	hold those six bytes by accident; so when the object opens a stream the
	search restarts after "endstream", which the engine always writes on a
	line of its own.
*/
QByteArray Document::object(int number) const
{
	if (!m_offsets.contains(number)) {
		return QByteArray();
	}

	const int start = m_offsets.value(number);
	int from = start;
	const int stream_at = m_data.indexOf("stream", start);
	const int first_end = m_data.indexOf("endobj", start);
	if (stream_at != -1 && (first_end == -1 || stream_at < first_end)) {
		const int stream_end = m_data.indexOf("\nendstream", stream_at);
		if (stream_end != -1) {
			from = stream_end;
		}
	}

	const int end = m_data.indexOf("endobj", from);
	if (end == -1) {
		return QByteArray();
	}
	return m_data.mid(start, end - start);
}

/// The raw bytes between "stream" and "endstream", inflated when compressed.
QByteArray Document::streamOf(int number) const
{
	const QByteArray body = object(number);
	if (body.isEmpty()) {
		return QByteArray();
	}

	int at = body.indexOf("stream");
	if (at == -1) {
		return QByteArray();
	}
	at += 6;
	if (at < body.size() && body.at(at) == '\r') {
		++at;
	}
	if (at < body.size() && body.at(at) == '\n') {
		++at;
	}

	const int end = body.indexOf("\nendstream", at);
	if (end == -1) {
		return QByteArray();
	}

	const QByteArray raw = body.mid(at, end - at);
	const QByteArray header = body.left(body.indexOf("stream"));
	if (header.contains("/FlateDecode")) {
		return inflate(raw);
	}
	return raw;
}

void Document::readPages()
{
	const int pages_at = m_data.indexOf("/Type /Pages");
	if (pages_at == -1) {
		m_error = QStringLiteral("no page tree");
		return;
	}
	const int kids_at = m_data.indexOf("/Kids", pages_at);
	const int open    = (kids_at == -1) ? -1 : m_data.indexOf('[', kids_at);
	const int close   = (open    == -1) ? -1 : m_data.indexOf(']', open);
	if (close == -1) {
		m_error = QStringLiteral("no /Kids array");
		return;
	}

	const QString kids =
		QString::fromLatin1(m_data.mid(open + 1, close - open - 1));
	const QRegularExpression re(QStringLiteral("(\\d+)\\s+\\d+\\s+R"));
	QRegularExpressionMatchIterator it = re.globalMatch(kids);
	while (it.hasNext()) {
		Page page;
		page.object = it.next().captured(1).toInt();

		const QByteArray body = object(page.object);
		const QRegularExpression box(
			QStringLiteral("/MediaBox\\s*\\[([^\\]]*)\\]"));
		const QRegularExpressionMatch m =
			box.match(QString::fromLatin1(body));
		if (m.hasMatch()) {
			const QStringList v = m.captured(1)
				.split(QRegularExpression(QStringLiteral("\\s+")),
				       Qt::SkipEmptyParts);
			if (v.size() == 4) {
				page.box = QRectF(QPointF(v.at(0).toDouble(),
							  v.at(1).toDouble()),
						  QPointF(v.at(2).toDouble(),
							  v.at(3).toDouble()));
			}
		}
		page.contents  = reference(body, "/Contents");
		page.resources = reference(body, "/Resources");
		page.annots    = reference(body, "/Annots");
		m_pages.append(page);
	}

	if (m_pages.isEmpty()) {
		m_error = QStringLiteral("the page tree lists no page");
	}
}

int Document::pageIndexOfObject(int object_number) const
{
	for (int i = 0 ; i < m_pages.size() ; ++i) {
		if (m_pages.at(i).object == object_number) {
			return i + 1;
		}
	}
	return 0;
}

void Document::readLinks()
{
	for (int i = 0 ; i < m_pages.size() ; ++i) {
		const Page &page = m_pages.at(i);
		if (page.annots == 0) {
			continue;
		}
		const QByteArray array = object(page.annots);
		const QRegularExpression ref(QStringLiteral("(\\d+)\\s+\\d+\\s+R"));
		QRegularExpressionMatchIterator it =
			ref.globalMatch(QString::fromLatin1(array));
		while (it.hasNext()) {
			const int number = it.next().captured(1).toInt();
			const QString annot =
				QString::fromLatin1(object(number));
			if (!annot.contains(QLatin1String("/Subtype /Link"))) {
				continue;
			}

			Link link;
			link.source_page = i + 1;

			const QRegularExpression rect(
				QStringLiteral("/Rect\\s*\\[([^\\]]*)\\]"));
			const QRegularExpressionMatch mr = rect.match(annot);
			if (mr.hasMatch()) {
				const QStringList v = mr.captured(1)
					.split(QRegularExpression(
						       QStringLiteral("\\s+")),
					       Qt::SkipEmptyParts);
				if (v.size() == 4) {
					link.rect = QRectF(
						QPointF(v.at(0).toDouble(),
							v.at(1).toDouble()),
						QPointF(v.at(2).toDouble(),
							v.at(3).toDouble()));
				}
			}

			const QRegularExpression dest(
				QStringLiteral("/D\\s*\\[\\s*(\\d+)\\s+\\d+\\s+R"
					       "([^\\]]*)\\]"));
			const QRegularExpressionMatch md = dest.match(annot);
			if (md.hasMatch()) {
				link.target_page =
					pageIndexOfObject(md.captured(1).toInt());
				const QRegularExpression fitr(
					QStringLiteral("/FitR\\s+(-?\\d+)\\s+(-?\\d+)"
						       "\\s+(-?\\d+)\\s+(-?\\d+)"));
				const QRegularExpressionMatch mf =
					fitr.match(md.captured(2));
				if (mf.hasMatch()) {
					link.destination = QRectF(
						QPointF(mf.captured(1).toDouble(),
							mf.captured(2).toDouble()),
						QPointF(mf.captured(3).toDouble(),
							mf.captured(4).toDouble()));
				}
			}
			m_links.append(link);
		}
	}
}

QSizeF Document::pageSize(int page) const
{
	if (page < 1 || page > m_pages.size()) {
		return QSizeF();
	}
	return m_pages.at(page - 1).box.size();
}

/// The /F<n> names a page's resources bind to a font object.
QHash<QString, int> Document::fontsOf(int resources_object) const
{
	QHash<QString, int> fonts;
	if (resources_object == 0) {
		return fonts;
	}

	const QString body = QString::fromLatin1(object(resources_object));
	const QRegularExpression re(
		QStringLiteral("/(F\\d+)\\s+(\\d+)\\s+\\d+\\s+R"));
	QRegularExpressionMatchIterator it = re.globalMatch(body);
	while (it.hasNext()) {
		const QRegularExpressionMatch m = it.next();
		fonts.insert(m.captured(1), m.captured(2).toInt());
	}
	return fonts;
}

/*
	Glyph number -> code point, read from the font's /ToUnicode map.

	Two shapes appear there and both are read: a run of glyphs listed one by
	one, "<lo> <hi> [ <u> <u> ... ]", and a run mapped to consecutive code
	points, "<lo> <hi> <u>". Reading only the first would lose whole fonts
	without saying so.

	They are read in a single pass, entry by entry, and that is not a matter
	of taste. Reading the map twice - once for the arrays, once for the
	contiguous runs - makes the second pass read the insides of the first:
	three code points sitting next to each other inside an array are also a
	valid contiguous entry, and the later pass overwrote what the earlier
	one had got right. It was measured: a page saying FOLIOGAMMA came back
	as FwxIwGAyyA, and the frame's own words as "Fitheiro" and "Dutu".
	Worse than the garbling is that whether it bites depends on the layout
	of the map - the same sheets read correctly in one document and wrongly
	in another, only because they were emitted in a different order. That is
	a probe turning an intact file into a defect, and it is the one thing
	this file must not do.
*/
const QHash<int, uint> &Document::toUnicodeOf(int font_object) const
{
	auto known = m_to_unicode.constFind(font_object);
	if (known != m_to_unicode.constEnd()) {
		return known.value();
	}

	QHash<int, uint> &map = m_to_unicode[font_object];
	if (font_object == 0) {
		return map;
	}

	const int to_unicode = reference(object(font_object), "/ToUnicode");
	if (to_unicode == 0) {
		return map;
	}

	const QString cmap = QString::fromLatin1(streamOf(to_unicode));

	// Only what stands between beginbfrange and endbfrange is a mapping;
	// the codespace range above it has the same shape and means something
	// else entirely.
	const QRegularExpression block(
		QStringLiteral("beginbfrange(.*?)endbfrange"),
		QRegularExpression::DotMatchesEverythingOption);
	// One entry: two glyph numbers, then either an array of code points or
	// a single one. The alternation is what keeps an array whole - the scan
	// resumes after its closing bracket and cannot re-read what is inside.
	const QRegularExpression entry(
		QStringLiteral("<([0-9A-Fa-f]+)>\\s*<([0-9A-Fa-f]+)>\\s*"
					   "(?:\\[([^\\]]*)\\]|<([0-9A-Fa-f]+)>)"));
	const QRegularExpression one(QStringLiteral("<([0-9A-Fa-f]+)>"));

	QRegularExpressionMatchIterator bt = block.globalMatch(cmap);
	while (bt.hasNext()) {
		const QString body = bt.next().captured(1);
		QRegularExpressionMatchIterator it = entry.globalMatch(body);
		while (it.hasNext()) {
			const QRegularExpressionMatch m = it.next();
			const int low  = m.captured(1).toInt(nullptr, 16);
			const int high = m.captured(2).toInt(nullptr, 16);

			if (m.captured(3).isNull()) {
				const uint dest =
					m.captured(4).toUInt(nullptr, 16);
				if (high < low || high - low > 0xFFFF) {
					continue;
				}
				for (int g = low ; g <= high ; ++g) {
					map.insert(g, dest + uint(g - low));
				}
				continue;
			}

			QRegularExpressionMatchIterator vt =
				one.globalMatch(m.captured(3));
			int offset = 0;
			while (vt.hasNext()) {
				map.insert(low + offset,
						   vt.next().captured(1)
						   .toUInt(nullptr, 16));
				++offset;
			}
		}
	}
	return map;
}

bool Document::hasMappedFont(int page) const
{
	if (page < 1 || page > m_pages.size()) {
		return false;
	}

	const QHash<QString, int> fonts = fontsOf(m_pages.at(page - 1).resources);
	for (auto it = fonts.constBegin() ; it != fonts.constEnd() ; ++it) {
		if (!toUnicodeOf(it.value()).isEmpty()) {
			return true;
		}
	}
	return false;
}

/*
	The page's content stream, read as text.

	The engine shows one glyph per operator, each placed by its own Td, so
	stream order is drawing order and a word arrives whole - without its
	spaces, which are never written. squeezed() is what makes the two sides
	comparable again.
*/
QString Document::text(int page) const
{
	if (page < 1 || page > m_pages.size()) {
		return QString();
	}

	const Page &p = m_pages.at(page - 1);
	const QString content = QString::fromLatin1(streamOf(p.contents));
	if (content.isEmpty()) {
		return QString();
	}

	const QHash<QString, int> fonts = fontsOf(p.resources);
	// Held by value, not by reference into the cache: reading the next font
	// can rehash it, and a reference taken before that would dangle. The
	// copy is a reference count, which is what makes this safe and free.
	QHash<int, uint> current;
	QString out;

	const QRegularExpression token(
		QStringLiteral("/(F\\d+)\\s+[\\d.]+\\s+Tf"
			       "|<([0-9A-Fa-f]+)>\\s*Tj"
			       "|\\[([^\\]]*)\\]\\s*TJ"));
	QRegularExpressionMatchIterator it = token.globalMatch(content);
	while (it.hasNext()) {
		const QRegularExpressionMatch m = it.next();

		if (!m.captured(1).isEmpty()) {
			current = toUnicodeOf(fonts.value(m.captured(1), 0));
			continue;
		}

		QStringList shown;
		if (!m.captured(2).isEmpty()) {
			shown << m.captured(2);
		} else {
			const QRegularExpression one(
				QStringLiteral("<([0-9A-Fa-f]+)>"));
			QRegularExpressionMatchIterator st =
				one.globalMatch(m.captured(3));
			while (st.hasNext()) {
				shown << st.next().captured(1);
			}
		}

		for (const QString &hex : shown) {
			for (int i = 0 ; i + 4 <= hex.size() ; i += 4) {
				const int glyph =
					hex.mid(i, 4).toInt(nullptr, 16);
				if (current.contains(glyph)) {
					out.append(QChar(current.value(glyph)));
				}
			}
		}
	}
	return out;
}

QString Document::text() const
{
	QStringList all;
	for (int i = 1 ; i <= m_pages.size() ; ++i) {
		all << text(i);
	}
	return all.join(QLatin1Char('\n'));
}

QString squeezed(const QString &text)
{
	QString out;
	out.reserve(text.size());
	for (const QChar c : text) {
		if (!c.isSpace()) {
			out.append(c);
		}
	}
	return out;
}

}
