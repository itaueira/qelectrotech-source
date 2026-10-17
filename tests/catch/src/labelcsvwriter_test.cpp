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
#include <catch2/catch.hpp>

#include "qt_catch_tostring.h"

#include "../../../sources/label/labelcsvwriter.h"

#include <QByteArray>
#include <QFile>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

/*
	The label file, written from a list built by hand.

	No project here, and on purpose: the writer takes a list and gives
	bytes, so everything it owes can be checked against entries whose
	every value is known. What it owes is not to lose anything on the way -
	a cell that moves a column, a label that changes place, an accent that
	turns into two other characters - and each case below holds one of
	those against it.

	The file is read back by a reader written in this file, and the first
	row it should find is written here too, not taken from the writer: a
	test that compared the writer with itself would pass whatever the
	writer did.

	Labelled T21 and not CU-21.x. CU-21.11 and CU-21.12 end at a file
	exported from a project and are proved where a project can be opened,
	in tests/catch/src/ui/labelexport_test.cpp; CU-21.8 ends at a printed
	label. What is proved here is the writer they all go through.
*/

namespace
{
	/// The first row of the file, written here and not asked of the writer.
	QStringList expectedHeader()
	{
		return QStringList {QStringLiteral("kind"),
				    QStringLiteral("text"),
				    QStringLiteral("lines"),
				    QStringLiteral("strip"),
				    QStringLiteral("position"),
				    QStringLiteral("level"),
				    QStringLiteral("location"),
				    QStringLiteral("folio"),
				    QStringLiteral("revision"),
				    QStringLiteral("excluded_from_bom")};
	}

	/// The columns, by name, so that a case reads a cell by what it is.
	int column(const QString &key)
	{
		return int(expectedHeader().indexOf(key));
	}

	/// What the reader below found in a text.
	struct Parsed
	{
		QVector<QStringList> rows;
			/// Rows ended by CR LF, outside any quoted cell.
		int crlf_ends = 0;
			/// Rows ended by anything else: a bare LF or a bare CR.
		int other_ends = 0;
	};

	/**
		RFC 4180, with ';' between the cells: a cell between double quotes
		may hold the separator, a doubled quote and an end of line, and an
		end of line outside quotes ends the row. Written here rather than
		borrowed, so that a defect of the writer cannot be matched by the
		same defect in the reader.
	*/
	Parsed parse(const QString &text)
	{
		Parsed parsed;
		QStringList row;
		QString cell;
		bool quoted = false;
		bool pending = false;

		for (int i = 0 ; i < text.size() ; ++i)
		{
			const QChar c = text.at(i);
			if (quoted)
			{
				if (c == QLatin1Char('"'))
				{
					if (i + 1 < text.size()
					    && text.at(i + 1) == QLatin1Char('"')) {
						cell += c;
						++i;
					} else {
						quoted = false;
					}
				} else {
					cell += c;
				}
				continue;
			}

			if (c == QLatin1Char('"')) {
				quoted = true;
				pending = true;
				continue;
			}
			if (c == QLatin1Char(';')) {
				row << cell;
				cell.clear();
				pending = true;
				continue;
			}
			if (c == QLatin1Char('\r') || c == QLatin1Char('\n'))
			{
				if (c == QLatin1Char('\r') && i + 1 < text.size()
				    && text.at(i + 1) == QLatin1Char('\n')) {
					++parsed.crlf_ends;
					++i;
				} else {
					++parsed.other_ends;
				}
				row << cell;
				parsed.rows << row;
				row.clear();
				cell.clear();
				pending = false;
				continue;
			}
			cell += c;
			pending = true;
		}
		if (pending || !cell.isEmpty()) {
			row << cell;
			parsed.rows << row;
		}
		return parsed;
	}

	LabelEntry terminal(const QString &text, int position, int level = 0,
			    const QStringList &lines = QStringList())
	{
		LabelEntry entry;
		entry.source = LabelSource::Terminal;
		entry.primary_text = text;
		entry.secondary_texts = lines;
		entry.strip = QStringLiteral("X1");
		entry.strip_position = position;
		entry.level = level;
		entry.folio = QStringLiteral("11");
		entry.folio_position = 1;
		entry.revision = QStringLiteral("A");
		return entry;
	}

	LabelEntry component(const QString &text,
			     const QStringList &lines = QStringList())
	{
		LabelEntry entry;
		entry.source = LabelSource::Component;
		entry.primary_text = text;
		entry.secondary_texts = lines;
		entry.folio = QStringLiteral("12");
		entry.folio_position = 2;
		entry.revision = QStringLiteral("B");
		return entry;
	}

	QStringList textsOf(const QVector<QStringList> &rows)
	{
		QStringList texts;
		for (int i = 1 ; i < rows.size() ; ++i) {
			texts << rows.at(i).value(column(QStringLiteral("text")));
		}
		return texts;
	}
}

TEST_CASE("T21 — the label file opens with a fixed row of keys",
	  "[t21][label][csv]")
{
	REQUIRE(LabelCsvWriter::header() == expectedHeader());
	CHECK(LabelCsvWriter::separator() == QStringLiteral(";"));

		//An empty list is a file holding the first row and nothing else:
		//the answer for a project with nothing to label, not a missing
		//file.
	const QString text = LabelCsvWriter::text(LabelEntryList());
	CHECK(text == expectedHeader().join(QStringLiteral(";"))
		      + QStringLiteral("\r\n"));

	const Parsed parsed = parse(text);
	REQUIRE(parsed.rows.size() == 1);
	CHECK(parsed.rows.first() == expectedHeader());
}

TEST_CASE("T21 — one row per label, as wide as the first, in the order of the list",
	  "[t21][label][csv]")
{
		//2, 10, 1: neither sorted as text nor as numbers, so a writer that
		//sorted by any rule would not give this order back.
	LabelEntryList entries;
	entries << terminal(QStringLiteral("2"), 0)
		<< terminal(QStringLiteral("10"), 1)
		<< terminal(QStringLiteral("1"), 2, 0,
			    {QStringLiteral("W7"), QStringLiteral("W100")})
		<< terminal(QStringLiteral("1b"), 2, 1)
		<< component(QStringLiteral("K1"),
			     {QStringLiteral("Contactor")});

	const QString text = LabelCsvWriter::text(entries);
	INFO(text.toStdString());
	const Parsed parsed = parse(text);

		//One row per entry, behind the first row: the line feed inside the
		//cell of 1 did not make a row of its own.
	REQUIRE(parsed.rows.size() == entries.size() + 1);
	CHECK(parsed.crlf_ends == parsed.rows.size());
	CHECK(parsed.other_ends == 0);

	for (const QStringList &row : parsed.rows)
	{
		INFO("row " << row.join(QStringLiteral("|")).toStdString());
		CHECK(row.size() == expectedHeader().size());
	}

	CHECK(textsOf(parsed.rows)
	      == QStringList({QStringLiteral("2"), QStringLiteral("10"),
			      QStringLiteral("1"), QStringLiteral("1b"),
			      QStringLiteral("K1")}));

		//Every cell of every row is the value of its entry, column by
		//column: the writer adds nothing and turns nothing into another
		//form.
	for (int i = 0 ; i < entries.size() ; ++i)
	{
		INFO("entry " << i);
		CHECK(parsed.rows.at(i + 1) == LabelCsvWriter::cells(entries.at(i)));
	}

	const QStringList one = parsed.rows.at(3);
	CHECK(one.at(column(QStringLiteral("kind"))) == QStringLiteral("terminal"));
	CHECK(one.at(column(QStringLiteral("strip"))) == QStringLiteral("X1"));
	CHECK(one.at(column(QStringLiteral("position"))) == QStringLiteral("2"));
	CHECK(one.at(column(QStringLiteral("level"))) == QStringLiteral("0"));
	CHECK(one.at(column(QStringLiteral("folio"))) == QStringLiteral("11"));
	CHECK(one.at(column(QStringLiteral("revision"))) == QStringLiteral("A"));

		//The second level of the same terminal: the same place on the
		//rail, one level up.
	const QStringList one_b = parsed.rows.at(4);
	CHECK(one_b.at(column(QStringLiteral("position"))) == QStringLiteral("2"));
	CHECK(one_b.at(column(QStringLiteral("level"))) == QStringLiteral("1"));

		//A component is on no rail: no strip, and empty cells where a
		//terminal has numbers - never the -1 the entry holds.
	const QStringList k1 = parsed.rows.at(5);
	CHECK(k1.at(column(QStringLiteral("kind"))) == QStringLiteral("component"));
	CHECK(k1.at(column(QStringLiteral("strip"))).isEmpty());
	CHECK(k1.at(column(QStringLiteral("position"))).isEmpty());
	CHECK(k1.at(column(QStringLiteral("level"))).isEmpty());
	CHECK_FALSE(text.contains(QStringLiteral("-1")));
}

TEST_CASE("T21 — a separator, a quote or a line in a cell does not move a column",
	  "[t21][label][csv]")
{
	LabelEntryList entries;
	entries << component(QStringLiteral("Q1;Q2"),
			     {QStringLiteral("Q1;Q2"),
			      QStringLiteral("Gland 1/2\"")})
		<< terminal(QStringLiteral("10"), 0, 0,
			    {QStringLiteral("W1;W2"), QStringLiteral("BK")});

	const QString text = LabelCsvWriter::text(entries);
	INFO(text.toStdString());
	const Parsed parsed = parse(text);

	REQUIRE(parsed.rows.size() == 3);
	for (const QStringList &row : parsed.rows) {
		CHECK(row.size() == expectedHeader().size());
	}

		//Read back whole, one cell each.
	const QStringList q = parsed.rows.at(1);
	CHECK(q.at(column(QStringLiteral("text"))) == QStringLiteral("Q1;Q2"));
	CHECK(q.at(column(QStringLiteral("lines")))
	      == QStringLiteral("Q1;Q2\nGland 1/2\""));

	const QStringList ten = parsed.rows.at(2);
	CHECK(ten.at(column(QStringLiteral("lines")))
	      == QStringLiteral("W1;W2\nBK"));

		//And quoted in the text itself, so that a reader more forgiving
		//than a spreadsheet cannot hide a missing quote: the separator in
		//quotes, the quote doubled.
	CHECK(text.contains(QStringLiteral("component;\"Q1;Q2\";")));
	CHECK(text.contains(QStringLiteral("\"Q1;Q2\nGland 1/2\"\"\"")));

		//The plain cells did not gain quotes they do not need: a file whose
		//cells are all plain is the same bytes with or without the rule.
	CHECK(text.contains(QStringLiteral("terminal;10;")));
	CHECK(text.contains(QStringLiteral(";X1;0;0;")));
}

TEST_CASE("T21 — the excluded mark is written, and the row is there all the same",
	  "[t21][label][csv]")
{
	LabelEntry kept = terminal(QStringLiteral("5"), 0);
	LabelEntry excluded = terminal(QStringLiteral("6"), 1);
	excluded.excluded_from_bom = true;

	const Parsed parsed =
			parse(LabelCsvWriter::text(LabelEntryList() << kept << excluded));
	REQUIRE(parsed.rows.size() == 3);

	const int mark = column(QStringLiteral("excluded_from_bom"));
	CHECK(parsed.rows.at(1).at(mark) == QStringLiteral("false"));
	CHECK(parsed.rows.at(2).at(mark) == QStringLiteral("true"));
}

TEST_CASE("T21 — every kind of label is written with a key of its own",
	  "[t21][label][csv]")
{
	CHECK(LabelCsvWriter::kindKey(LabelSource::Component)
	      == QStringLiteral("component"));
	CHECK(LabelCsvWriter::kindKey(LabelSource::Terminal)
	      == QStringLiteral("terminal"));

		//The three with no collector yet still have a word, and five kinds
		//give five words: two kinds reading alike in the column would be
		//two kinds a reader cannot tell apart.
	QSet<QString> keys;
	for (LabelSource source : {LabelSource::Component,
				   LabelSource::Terminal,
				   LabelSource::Wire,
				   LabelSource::Cable,
				   LabelSource::CabinetDoor})
	{
		const QString key = LabelCsvWriter::kindKey(source);
		CHECK_FALSE(key.isEmpty());
		CHECK(key != QStringLiteral("unknown"));
		keys.insert(key);
	}
	CHECK(keys.size() == 5);
}

TEST_CASE("T21 — accents reach the file as UTF-8, behind a byte order mark",
	  "[t21][label][csv]")
{
	LabelEntryList entries;
	entries << component(QString::fromUtf8("Válvula"),
			     {QString::fromUtf8("Pressão"),
			      QString::fromUtf8("Emergência")});

	const QByteArray data = LabelCsvWriter::bytes(entries);

		//The mark, once, at the start: what tells a spreadsheet on Windows
		//that the file is UTF-8 rather than the local code page.
	REQUIRE(data.startsWith(QByteArray("\xEF\xBB\xBF", 3)));
	CHECK(data.indexOf(QByteArray("\xEF\xBB\xBF", 3), 1) < 0);

		//The accented letters as UTF-8 bytes, written out byte by byte so
		//that the check does not depend on how this file was saved: a
		//writer in Latin-1 would give one byte for each of them.
	CHECK(data.contains(QByteArray("V\xC3\xA1lvula")));
	CHECK(data.contains(QByteArray("Press\xC3\xA3o")));
	CHECK(data.contains(QByteArray("Emerg\xC3\xAAncia")));

		//And behind the mark, the text and nothing else.
	CHECK(QString::fromUtf8(data.mid(3)) == LabelCsvWriter::text(entries));
}

TEST_CASE("T21 — the label file is written whole, or not at all",
	  "[t21][label][csv]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());

	LabelEntryList entries;
	entries << terminal(QStringLiteral("2"), 0)
		<< terminal(QStringLiteral("10"), 1);

	SECTION("written: the file holds the bytes, and nothing is said")
	{
		const QString path = dir.filePath(QStringLiteral("labels.csv"));
		CHECK(LabelCsvWriter::write(entries, path).isEmpty());

		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		CHECK(file.readAll() == LabelCsvWriter::bytes(entries));
	}

	SECTION("written again: the new list replaces the old one")
	{
		const QString path = dir.filePath(QStringLiteral("labels.csv"));
		REQUIRE(LabelCsvWriter::write(entries, path).isEmpty());

		const LabelEntryList shorter {terminal(QStringLiteral("7"), 0)};
		REQUIRE(LabelCsvWriter::write(shorter, path).isEmpty());

		QFile file(path);
		REQUIRE(file.open(QIODevice::ReadOnly));
		CHECK(file.readAll() == LabelCsvWriter::bytes(shorter));
	}

	SECTION("a folder that does not exist: the reason, and no file")
	{
		const QString path =
				dir.filePath(QStringLiteral("missing/labels.csv"));
		CHECK_FALSE(LabelCsvWriter::write(entries, path).isEmpty());
		CHECK_FALSE(QFile::exists(path));
	}
}
