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
#include "uibench.h"

#include "../qt_catch_tostring.h"

#include "../../../../sources/cli_export.h"
#include "../../../../sources/location/mountinglayout.h"

#include <catch2/catch.hpp>

#include <QChar>
#include <QDomDocument>
#include <QFile>
#include <QList>
#include <QPointF>
#include <QSet>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

/*
	The purchase list, as the command line writes it and as a spreadsheet
	reads it back.

	BomCollector has a bench of its own in the pure suite, which proves the
	join of the two halves on rows built by hand. What that bench cannot
	see is the road to it: the rows the project data base really hands
	over, the enclosures a project file really declares, and the file a
	shop really opens. That is what is asserted here, and nothing is taken
	from the writer under test - the file is parsed again by a reader
	written in this file, and the numbers it should come to are worked out
	from the bench below rather than written as literals, with a literal
	probe beside each so that a bench that shrinks cannot stop proving
	anything in silence.

	Every case asserts the number of lines and the total they come to.
	The first catches an item that became a line of its own; only the
	second catches an item added into the line beside it, which changes no
	count and disappears inside a total that goes on looking plausible.
*/

namespace
{
	/// One drawn component of the bench, as the folio holds it.
	struct Instance
	{
		int folio = 1;
		QString label;
		QString designation;
		QString part_code;
		QString part_revision;
		QString quantity;
		bool excluded = false;
	};

	Instance drawn(int folio,
		       const char *label,
		       const char *designation,
		       const char *code,
		       const char *revision,
		       const char *quantity = "",
		       bool excluded = false)
	{
		Instance instance;
		instance.folio = folio;
		instance.label = QString::fromUtf8(label);
		instance.designation = QString::fromUtf8(designation);
		instance.part_code = QString::fromUtf8(code);
		instance.part_revision = QString::fromUtf8(revision);
		instance.quantity = QString::fromUtf8(quantity);
		instance.excluded = excluded;
		return instance;
	}

	/**
		The components of the bench, and each is here for a question.

		Q1 and Q2 are two different parts described with the same words:
		two lines, or the key is the description (CU-18.8).

		K1 and K2 are one part, and K3 is the same part one revision up:
		two lines that say which revision, never one line adding them up
		(CU-18.14).

		R1 stands for five pieces with one symbol: the line says five,
		which is the quantity field summed and not the rows counted.

		H1 to H5 have no part. They are the pending block, and H5 is
		alone on the second folio so that a folio of 1 on every pending
		item cannot pass for right (CU-16.6, CU-18.3).

		X1 to X7 were ticked out of the parts list, and carry a part code
		so that a filter that let them through would show as a line of
		its own (CU-18.12).

		The last one names nothing at all: a drawn marker, counted and
		never listed.
	*/
	QList<Instance> benchInstances()
	{
		QList<Instance> list;
		list << drawn(1, "Q1", "Disjuntor tripolar", "XA-201", "1")
		     << drawn(1, "Q2", "Disjuntor tripolar", "XA-202", "1")
		     << drawn(1, "K1", "Contator 9 A", "XA-300", "1")
		     << drawn(1, "K2", "Contator 9 A", "XA-300", "1")
		     << drawn(1, "K3", "Contator 9 A", "XA-300", "2")
		     << drawn(1, "R1", "Rele de interface", "XA-400", "", "5")
		     << drawn(1, "H1", "Sinaleiro 22 mm", "", "")
		     << drawn(1, "H2", "Sinaleiro 22 mm", "", "")
		     << drawn(1, "H3", "Sinaleiro 22 mm", "", "")
		     << drawn(1, "H4", "Sinaleiro 22 mm", "", "")
		     << drawn(2, "H5", "Sinaleiro 22 mm", "", "")
		     << drawn(2, "X1", "Borne de passagem", "XA-900", "1", "", true)
		     << drawn(2, "X2", "Borne de passagem", "XA-900", "1", "", true)
		     << drawn(2, "X3", "Borne de passagem", "XA-900", "1", "", true)
		     << drawn(2, "X4", "Borne de passagem", "XA-900", "1", "", true)
		     << drawn(2, "X5", "Borne de passagem", "XA-900", "1", "", true)
		     << drawn(2, "X6", "Borne de passagem", "XA-900", "1", "", true)
		     << drawn(2, "X7", "Borne de passagem", "XA-900", "1", "", true)
		     << drawn(2, "", "", "", "");
		return list;
	}

	/// @return the bench without the components tagged as @p tags
	QList<Instance> without(const QList<Instance> &instances,
				const QStringList &tags)
	{
		QList<Instance> kept;
		for (const Instance &instance : instances)
		{
			if (!tags.contains(instance.label)) {
				kept << instance;
			}
		}
		return kept;
	}

	/// @return true when @p instance carries any information at all
	bool namesSomething(const Instance &instance)
	{
		return !instance.label.isEmpty()
				|| !instance.designation.isEmpty()
				|| !instance.part_code.isEmpty()
				|| !instance.part_revision.isEmpty()
				|| !instance.quantity.isEmpty();
	}

	/// How many components should land in the pending block
	int expectedPending(const QList<Instance> &instances)
	{
		int count = 0;
		for (const Instance &instance : instances)
		{
			if (!instance.excluded && namesSomething(instance)
			    && instance.part_code.isEmpty()) {
				++count;
			}
		}
		return count;
	}

	/// How many components were ticked out of the parts list
	int expectedExcluded(const QList<Instance> &instances)
	{
		int count = 0;
		for (const Instance &instance : instances)
		{
			if (instance.excluded) {
				++count;
			}
		}
		return count;
	}

	/// How many components name nothing and are not ticked out
	int expectedNameless(const QList<Instance> &instances)
	{
		int count = 0;
		for (const Instance &instance : instances)
		{
			if (!instance.excluded && !namesSomething(instance)) {
				++count;
			}
		}
		return count;
	}

	/// @return true when @p instance reaches a line of the list
	bool reachesALine(const Instance &instance)
	{
		return !instance.excluded && !instance.part_code.isEmpty();
	}

	/// How many lines the drawn half comes to: one per part and revision
	int expectedDrawnLines(const QList<Instance> &instances)
	{
		QSet<QString> keys;
		for (const Instance &instance : instances)
		{
			if (reachesALine(instance))
			{
					//A separator no code can hold, so that two fields
					//cannot run together into the same key.
				keys.insert(instance.part_code + QChar(ushort(0x1f))
					    + instance.part_revision);
			}
		}
		return int(keys.size());
	}

	/// How many pieces the drawn half comes to: the quantity field, one
	/// when it is empty
	double expectedDrawnPieces(const QList<Instance> &instances)
	{
		double pieces = 0.0;
		for (const Instance &instance : instances)
		{
			if (reachesALine(instance)) {
				pieces += instance.quantity.isEmpty()
						? 1.0
						: instance.quantity.toDouble();
			}
		}
		return pieces;
	}

	/// The enclosure half of the bench: XA-100, XA-110 and XA-120
	const int enclosure_lines = 3;
	/// Two cabinets, one door, one plate
	const double enclosure_pieces = 4.0;

	/**
		@return the location tree of the bench

		Two cabinets bought as one part, and the door and the mounting
		plate of the first, each bought as a part of its own. None of the
		four is drawn on a folio, which is the whole point. The name of
		the cabinets carries a letter outside ASCII, so that the file is
		read back as UTF-8 and not merely as bytes that happen to agree.
	*/
	QString treeXml()
	{
		return QString::fromUtf8(
			"<location_tree>"
			"<location uuid=\"{10000000-0000-4000-8000-000000000001}\""
			" code=\"QCM1\" name=\"Armário 1200x800\""
			" part=\"XA-100\" revision=\"1\"/>"
			"<location uuid=\"{10000000-0000-4000-8000-000000000002}\""
			" parent=\"{10000000-0000-4000-8000-000000000001}\""
			" code=\"PORTE\" name=\"Porta 1200x800\" part=\"XA-110\"/>"
			"<location uuid=\"{10000000-0000-4000-8000-000000000003}\""
			" parent=\"{10000000-0000-4000-8000-000000000001}\""
			" code=\"PLACA\" name=\"Placa de montagem\" part=\"XA-120\"/>"
			"<location uuid=\"{10000000-0000-4000-8000-000000000004}\""
			" code=\"QCM2\" name=\"Armário 1200x800\""
			" part=\"XA-100\" revision=\"1\"/>"
			"</location_tree>");
	}

	/// The one symbol every component of the bench is drawn with.
	QString definitionXml()
	{
		return QStringLiteral(
			"<element name=\"part.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"40\""
			" hotspot_x=\"10\" hotspot_y=\"20\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">Part</name></names>"
			"<description>"
			"<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
			" end1=\"none\" end2=\"none\" length1=\"1.5\""
			" length2=\"1.5\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;"
			"filling:none;color:black\"/>"
			"<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"
			"<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"
			"</description>"
			"</definition>"
			"</element>");
	}

	/// One element information, nothing when the value is empty
	QString information(const QString &name, const QString &value)
	{
		if (value.isEmpty()) {
			return QString();
		}
		return QStringLiteral("<elementInformation show=\"0\" name=\"")
				+ name + QStringLiteral("\">")
				+ value.toHtmlEscaped()
				+ QStringLiteral("</elementInformation>");
	}

	/**
		@return the uuid the bench gives the @p n th component it draws,
		counted from 1 in the order of the list

		One spelling, for the folios and for whatever points at a
		component on them: a part on the plate names its symbol by this,
		and a second spelling of it would be a link to nothing that reads
		exactly like a link to something.
	*/
	QString elementUuid(int n)
	{
		return QStringLiteral("{b18e0000-0000-4000-8000-%1}")
				.arg(n, 12, 10, QLatin1Char('0'));
	}

	/// @return the uuid of the component tagged @p tag, empty when none is
	QString uuidOfTag(const QList<Instance> &instances, const QString &tag)
	{
		for (int i = 0 ; i < instances.size() ; ++i)
		{
			if (instances.at(i).label == tag) {
				return elementUuid(i + 1);
			}
		}
		return QString();
	}

	/**
		@return a project of two folios drawing @p instances, with the tree
		@param layout the <mounting_layout> of the project, none when empty
	*/
	QString fixtureXml(const QList<Instance> &instances,
			   const QString &layout = QString())
	{
		QString folios[2];
		int placed[2] = {0, 0};
		int uuid = 0;
		for (const Instance &instance : instances)
		{
			const int page = instance.folio - 1;

			QString info;
			info += information(QStringLiteral("label"), instance.label);
			info += information(QStringLiteral("designation"),
					    instance.designation);
			info += information(QStringLiteral("part_code"),
					    instance.part_code);
			info += information(QStringLiteral("part_revision"),
					    instance.part_revision);
			info += information(QStringLiteral("quantity"),
					    instance.quantity);
			if (instance.excluded) {
				info += information(QStringLiteral("exclude_from_bom"),
						    QStringLiteral("true"));
			}

				//Ten to a row, so that no two components share a
				//place on the folio.
			const int x = 100 + (placed[page] % 10) * 100;
			const int y = 100 + (placed[page] / 10) * 150;
			++placed[page];
			++uuid;

			folios[page] += QStringLiteral(
					"<element x=\"%1\" y=\"%2\" z=\"10\" prefix=\"\""
					" freezeLabel=\"false\" orientation=\"0\""
					" type=\"embed://bench/part.elmt\""
					" uuid=\"%3\">"
					"<terminals/><inputs/>"
					"<elementInformations>%4</elementInformations>"
					"<dynamic_texts/><texts_groups/>"
					"</element>")
				 .arg(x)
				 .arg(y)
				 .arg(elementUuid(uuid))
				 .arg(info);
		}

		auto sheet = [](int number, const QString &elements)
		{
			const QString n = QString::number(number);
			return QStringLiteral("<diagram title=\"Bench ") + n
					+ QStringLiteral("\" order=\"") + n
					+ QStringLiteral("\" folio=\"") + n
					+ QStringLiteral("\" height=\"500\""
							 " cols=\"25\" colsize=\"50\""
							 " rows=\"6\" rowsize=\"80\""
							 " displaycols=\"true\""
							 " displayrows=\"true\">"
							 "<elements>")
					+ elements
					+ QStringLiteral("</elements>"
							 "<inputs/><conductors/>"
							 "</diagram>");
		};

		return QStringLiteral("<project title=\"bench\" version=\"0.80\">")
				+ treeXml()
				+ layout
				+ QStringLiteral("<collection><category name=\"bench\">")
				+ definitionXml()
				+ QStringLiteral("</category></collection>")
				+ sheet(1, folios[0])
				+ sheet(2, folios[1])
				+ QStringLiteral("</project>");
	}

	/// @return false when @p content could not be written to @p path
	bool writeFile(const QString &path, const QString &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write(content.toUtf8()) >= 0;
	}

	/**
		@return the file as it is, a null string when there is none

		Read in binary, and not as text: the end of each row is part of
		what is asserted, and a text-mode read would turn the CRLF the
		file is written with into the LF a reader expects to see.
	*/
	QString readFile(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QString();
		}
		return QString::fromUtf8(file.readAll());
	}

	/**
		The reader of this file: ';' between cells, '"' around the cells
		that need it, CR before the LF that ends a row. Written here and
		not borrowed from the writer, so that the file is checked against
		the format rather than against the writer's own idea of it.

		An empty row comes back as a row of one empty cell, as a
		spreadsheet would show it.
	*/
	QVector<QStringList> parseCsv(const QString &text)
	{
		QVector<QStringList> rows;
		QStringList row;
		QString cell;
		bool quoted = false;
		bool cell_started = false;

		for (int i = 0 ; i < text.size() ; ++i)
		{
			const QChar c = text.at(i);

			if (quoted)
			{
				if (c == QLatin1Char('"'))
				{
					if (i + 1 < text.size()
					    && text.at(i + 1) == QLatin1Char('"'))
					{
						cell += QLatin1Char('"');
						++i;
					}
					else
					{
						quoted = false;
					}
					continue;
				}
				cell += c;
				continue;
			}

			if (c == QLatin1Char('"') && !cell_started)
			{
				quoted = true;
				cell_started = true;
				continue;
			}
			if (c == QLatin1Char(';'))
			{
				row << cell;
				cell.clear();
				cell_started = false;
				continue;
			}
			if (c == QLatin1Char('\r')) {
				continue;
			}
			if (c == QLatin1Char('\n'))
			{
				row << cell;
				cell.clear();
				cell_started = false;
				rows << row;
				row.clear();
				continue;
			}
			cell += c;
			cell_started = true;
		}
		if (cell_started || !row.isEmpty())
		{
			row << cell;
			rows << row;
		}
		return rows;
	}

	/// @return true when @p row is the empty row between two blocks
	bool isBlank(const QStringList &row)
	{
		return row.size() == 1 && row.first().isEmpty();
	}

	/// The rows, cut into the blocks the empty rows separate
	QList<QVector<QStringList>> blocksOf(const QVector<QStringList> &rows)
	{
		QList<QVector<QStringList>> blocks;
		QVector<QStringList> block;
		for (const QStringList &row : rows)
		{
			if (isBlank(row))
			{
				blocks << block;
				block.clear();
				continue;
			}
			block << row;
		}
		blocks << block;
		return blocks;
	}

	/// What one run of --export-purchase left behind.
	struct PurchaseFile
	{
		int code = -1;
		bool written = false;
		QString text;
		QVector<QStringList> rows;
		QList<QVector<QStringList>> blocks;
	};

	/**
		@return the purchase list of a project made of @p xml

		The project is written to a file of its own and never opened in
		this process: the command line opens it itself, and opening one
		file twice is a question these cases do not ask.
	*/
	PurchaseFile exportOf(const QString &xml)
	{
		PurchaseFile file;
		QTemporaryDir dir;
		if (!dir.isValid()) {
			return file;
		}
		const QString project_path = dir.filePath(QStringLiteral("bench.qet"));
		const QString csv_path = dir.filePath(QStringLiteral("purchase.csv"));
		if (!writeFile(project_path, xml)) {
			return file;
		}

		file.code = CLIExport::run(QStringList()
					   << QStringLiteral("--export-purchase")
					   << project_path << csv_path);
		file.written = QFile::exists(csv_path);
		file.text = readFile(csv_path);
		file.rows = parseCsv(file.text);
		file.blocks = blocksOf(file.rows);
		return file;
	}

	/// The first row of the lines block, written here and not taken from
	/// the code under test
	QStringList lineHeader()
	{
		return QStringList {QStringLiteral("part_code"),
				    QStringLiteral("part_revision"),
				    QStringLiteral("designation"),
				    QStringLiteral("quantity"),
				    QStringLiteral("unit"),
				    QStringLiteral("tags")};
	}

	/// The first row of the pending block, for the same reason
	QStringList pendingHeader()
	{
		return QStringList {QStringLiteral("reason"),
				    QStringLiteral("tag"),
				    QStringLiteral("designation"),
				    QStringLiteral("where")};
	}

	/**
		The shape every purchase list has, whatever the project: three
		blocks, the counts first in a fixed order, every row as wide as
		its header, CRLF between rows and no byte order mark.

		mounted is fifth and has to stay fifth: it came after the other
		four, and a script that reads the first four by their place goes
		on finding them there only as long as nothing is put among them.
	*/
	void checkShape(const PurchaseFile &file)
	{
		REQUIRE(file.blocks.size() == 3);

		const QVector<QStringList> &summary = file.blocks.at(0);
		REQUIRE(summary.size() == 5);
		CHECK(summary.at(0).value(0) == QStringLiteral("pending"));
		CHECK(summary.at(1).value(0) == QStringLiteral("excluded"));
		CHECK(summary.at(2).value(0) == QStringLiteral("nameless"));
		CHECK(summary.at(3).value(0) == QStringLiteral("lines"));
		CHECK(summary.at(4).value(0) == QStringLiteral("mounted"));
		for (const QStringList &row : summary) {
			CHECK(row.size() == 2);
		}

		const QVector<QStringList> &lines = file.blocks.at(1);
		REQUIRE_FALSE(lines.isEmpty());
		CHECK(lines.first() == lineHeader());
		for (const QStringList &row : lines) {
			CHECK(row.size() == lineHeader().size());
		}

		const QVector<QStringList> &pendings = file.blocks.at(2);
		REQUIRE_FALSE(pendings.isEmpty());
		CHECK(pendings.first() == pendingHeader());
		for (const QStringList &row : pendings) {
			CHECK(row.size() == pendingHeader().size());
		}

		CHECK_FALSE(file.text.startsWith(QChar(0xFEFF)));
		CHECK(file.text.count(QStringLiteral("\r\n"))
		      == file.text.count(QLatin1Char('\n')));
	}

	/// @return the number written beside @p key in the counts, -1 when
	/// there is no such row
	int countIn(const PurchaseFile &file, const QString &key)
	{
		if (file.blocks.isEmpty()) {
			return -1;
		}
		for (const QStringList &row : file.blocks.first())
		{
			if (row.value(0) == key)
			{
				bool ok = false;
				const int value = row.value(1).toInt(&ok);
				return ok ? value : -1;
			}
		}
		return -1;
	}

	/// @return the rows of a block, its first row left out
	QVector<QStringList> dataOf(const PurchaseFile &file, int block)
	{
		if (block >= file.blocks.size() || file.blocks.at(block).isEmpty()) {
			return QVector<QStringList>();
		}
		return file.blocks.at(block).mid(1);
	}

	/// @return the cell of @p row under the column @p name of @p header
	QString cellOf(const QStringList &header,
		       const QStringList &row,
		       const QString &name)
	{
		return row.value(int(header.indexOf(name)));
	}

	QString lineCell(const QStringList &row, const QString &name)
	{
		return cellOf(lineHeader(), row, name);
	}

	QString pendingCell(const QStringList &row, const QString &name)
	{
		return cellOf(pendingHeader(), row, name);
	}

	/**
		@return the tags of a line, sorted

		Sorted because what a line is answerable for is which tags, and
		not in what order: the drawn ones arrive in the order of the cells
		of the folio, which is text, and asserting that order would make
		these cases about the grid instead of about the list.
	*/
	QStringList tagsOf(const QStringList &row)
	{
		QStringList tags = lineCell(row, QStringLiteral("tags"))
				   .split(QStringLiteral(", "), Qt::SkipEmptyParts);
		tags.sort();
		return tags;
	}

	/// @return the lines of that part, in whatever revision
	QVector<QStringList> linesOf(const PurchaseFile &file, const QString &code)
	{
		QVector<QStringList> found;
		for (const QStringList &row : dataOf(file, 1))
		{
			if (lineCell(row, QStringLiteral("part_code")) == code) {
				found << row;
			}
		}
		return found;
	}

	/// @return the line of that part in that revision, empty when none
	QStringList lineOf(const PurchaseFile &file,
			   const QString &code,
			   const QString &revision)
	{
		for (const QStringList &row : linesOf(file, code))
		{
			if (lineCell(row, QStringLiteral("part_revision")) == revision) {
				return row;
			}
		}
		return QStringList();
	}

	/// @return what the quantity column adds up to over @p rows
	double piecesOf(const QVector<QStringList> &rows)
	{
		double total = 0.0;
		for (const QStringList &row : rows) {
			total += lineCell(row, QStringLiteral("quantity")).toDouble();
		}
		return total;
	}

	/// The face the plate cases lay their parts on: the mounting plate
	/// of the tree above, so that the place a line names is a real one
	QString platePath()
	{
		return QStringLiteral("QCM1/PLACA");
	}

	/**
		@return a part bought as a piece, as the plate holds it
		@param code the catalogue part it was laid as
		@param tag what it is called on the plate, empty for a part laid
		straight from the catalogue
		@param element the symbol it is, empty for a part with none
		@param x where it sits along the plate, millimetre
	*/
	MountedItem piece(const QString &code,
			  const QString &tag,
			  const QString &element,
			  qreal x)
	{
		MountedItem item(tag, QPointF(x, 100.0), QSizeF(22.5, 85.0));
		item.part_code = code;
		item.element = element;
		return item;
	}

	/**
		@return a piece of rail cut to 600 from a bar of @p code

		With a code on purpose: a piece with none would come back, if it
		were counted by the piece, as a pendency, and a pendency is not
		the failure this guards against. The failure is a line ordering
		rail by the piece, and only a code can make one.
	*/
	MountedItem railPiece(const QString &code)
	{
		const MountingProfile profile = MountingProfile::rail(35.0, 7.5);
		MountedItem item(QString(), QPointF(0.0, 300.0),
				 profile.sizeFor(600.0, MountingRun::Across));
		item.part_code = code;
		item.profile = profile;
		item.run = MountingRun::Across;
		return item;
	}

	/**
		@return the <mounting_layout> of a project with @p items on one
		plate, empty when the layout refused any of them

		Built through MountingLayout and written by its own toXml(), and
		not spelled out here like the tree: the file a person's plate is
		saved as is what --export-purchase reads, and a hand-written
		element that drifted from it would prove the export against a
		file nobody can make.
	*/
	QString plateXml(const QList<MountedItem> &items)
	{
		MountingLayout layout;
		const QString face = layout.appendSurface(
					MountingSurface(platePath(),
							QStringLiteral("plate"),
							MountingArea(600.0, 800.0)));
		if (face.isEmpty()) {
			return QString();
		}
		for (const MountedItem &item : items)
		{
			if (layout.mountItem(face, item).isEmpty()) {
				return QString();
			}
		}

		QDomDocument document;
		document.appendChild(layout.toXml(document));
		return document.toString(-1);
	}
}

TEST_CASE("CU-18.10 — armário, porta e placa saem na planilha junto com os "
	  "componentes", "[purchase][bom][t18]")
{
	const QList<Instance> instances = benchInstances();
	const PurchaseFile file = exportOf(fixtureXml(instances));
	INFO(file.text.toStdString());
	REQUIRE(file.written);
	checkShape(file);

		//The three lines nobody drew, each with its code.
	const QStringList cabinet = lineOf(file, QStringLiteral("XA-100"),
					   QStringLiteral("1"));
	REQUIRE_FALSE(cabinet.isEmpty());
	CHECK(lineCell(cabinet, QStringLiteral("designation"))
	      == QString::fromUtf8("Armário 1200x800"));
	CHECK(lineCell(cabinet, QStringLiteral("quantity")) == QStringLiteral("2"));
	CHECK(tagsOf(cabinet)
	      == (QStringList {QStringLiteral("QCM1"), QStringLiteral("QCM2")}));

		//No revision was named for the door and the plate, and the cell
		//says so by being empty rather than by a revision 0 that no
		//catalogue part has.
	const QStringList door = lineOf(file, QStringLiteral("XA-110"), QString());
	REQUIRE_FALSE(door.isEmpty());
	CHECK(lineCell(door, QStringLiteral("quantity")) == QStringLiteral("1"));
	CHECK(lineCell(door, QStringLiteral("tags"))
	      == QStringLiteral("QCM1/PORTE"));

	const QStringList plate = lineOf(file, QStringLiteral("XA-120"), QString());
	REQUIRE_FALSE(plate.isEmpty());
	CHECK(lineCell(plate, QStringLiteral("quantity")) == QStringLiteral("1"));
	CHECK(lineCell(plate, QStringLiteral("tags"))
	      == QStringLiteral("QCM1/PLACA"));

		//And the drawn half in the very same block, not in a file of
		//its own.
	CHECK_FALSE(lineOf(file, QStringLiteral("XA-201"),
			   QStringLiteral("1")).isEmpty());
	CHECK_FALSE(lineOf(file, QStringLiteral("XA-400"), QString()).isEmpty());

		//The count of lines, which catches an item that became a line of
		//its own - with the probe that keeps the bench measuring.
	const int lines = expectedDrawnLines(instances) + enclosure_lines;
	REQUIRE(lines == 8);
	CHECK(dataOf(file, 1).size() == lines);
	CHECK(countIn(file, QStringLiteral("lines")) == lines);

		//And the total, which catches an item added into its neighbour:
		//ten drawn pieces - R1 is five of them - and four enclosures.
	const double pieces = expectedDrawnPieces(instances) + enclosure_pieces;
	REQUIRE(pieces == 14.0);
	CHECK(piecesOf(dataOf(file, 1)) == pieces);
}

TEST_CASE("CU-16.6 — cinco componentes sem peça saem num bloco à parte, "
	  "impossíveis de ignorar", "[purchase][bom][t18][t16]")
{
	const QList<Instance> instances = benchInstances();
	REQUIRE(expectedPending(instances) == 5);

	const PurchaseFile file = exportOf(fixtureXml(instances));
	INFO(file.text.toStdString());
	REQUIRE(file.written);
	checkShape(file);

		//Impossible to ignore, for the two readers there are: the first
		//row of the file is the count, and the exit code is the one a
		//scheduled export reads - written, and not complete.
	REQUIRE_FALSE(file.rows.isEmpty());
	CHECK(file.rows.first()
	      == (QStringList {QStringLiteral("pending"), QStringLiteral("5")}));
	CHECK(file.code == 3);

		//The block, each of the five named.
	const QVector<QStringList> pendings = dataOf(file, 2);
	REQUIRE(pendings.size() == 5);
	QStringList tags;
	for (const QStringList &row : pendings)
	{
		CHECK(pendingCell(row, QStringLiteral("reason"))
		      == QStringLiteral("no_part"));
		CHECK(pendingCell(row, QStringLiteral("designation"))
		      == QStringLiteral("Sinaleiro 22 mm"));
		tags << pendingCell(row, QStringLiteral("tag"));
	}
	tags.sort();
	CHECK(tags == (QStringList {QStringLiteral("H1"), QStringLiteral("H2"),
				    QStringLiteral("H3"), QStringLiteral("H4"),
				    QStringLiteral("H5")}));

		//And none of them among the lines, as a line with an empty code
		//or under its description.
	for (const QStringList &row : dataOf(file, 1))
	{
		INFO(row.join(QStringLiteral(";")).toStdString());
		CHECK_FALSE(lineCell(row, QStringLiteral("part_code")).isEmpty());
		CHECK(lineCell(row, QStringLiteral("designation"))
		      != QStringLiteral("Sinaleiro 22 mm"));
	}
}

TEST_CASE("CU-18.3 — o componente sem peça sai nas pendências, dizendo folha e "
	  "tag", "[purchase][bom][t18]")
{
		//One component with no part, and it is the one on the second
		//folio: the folio said has to be read, not defaulted.
	const QList<Instance> instances =
		without(benchInstances(),
			QStringList {QStringLiteral("H1"), QStringLiteral("H2"),
				     QStringLiteral("H3"), QStringLiteral("H4")});
	REQUIRE(expectedPending(instances) == 1);

	const PurchaseFile file = exportOf(fixtureXml(instances));
	INFO(file.text.toStdString());
	REQUIRE(file.written);
	checkShape(file);

		//Never a silent failure: the code says it.
	CHECK(file.code == 3);
	CHECK(countIn(file, QStringLiteral("pending")) == 1);

	const QVector<QStringList> pendings = dataOf(file, 2);
	REQUIRE(pendings.size() == 1);
	CHECK(pendings.first()
	      == (QStringList {QStringLiteral("no_part"), QStringLiteral("H5"),
			       QStringLiteral("Sinaleiro 22 mm"),
			       QStringLiteral("2")}));

		//The components that do have a part are all on the list.
	for (const Instance &instance : instances)
	{
		if (!reachesALine(instance)) {
			continue;
		}
		INFO(instance.label.toStdString());
		CHECK_FALSE(lineOf(file, instance.part_code,
				   instance.part_revision).isEmpty());
	}

		//And never a line with an empty code.
	for (const QStringList &row : dataOf(file, 1)) {
		CHECK_FALSE(lineCell(row, QStringLiteral("part_code")).isEmpty());
	}
}

TEST_CASE("CU-18.8 — duas peças com a mesma descrição dão duas linhas",
	  "[purchase][bom][t18]")
{
	const PurchaseFile file = exportOf(fixtureXml(benchInstances()));
	INFO(file.text.toStdString());
	REQUIRE(file.written);
	checkShape(file);

	QVector<QStringList> breakers;
	for (const QStringList &row : dataOf(file, 1))
	{
		if (lineCell(row, QStringLiteral("designation"))
		    == QStringLiteral("Disjuntor tripolar")) {
			breakers << row;
		}
	}

		//Two lines, one per code, and the two together come to the two
		//components drawn - neither line swallowed the other.
	REQUIRE(breakers.size() == 2);
	CHECK(piecesOf(breakers) == 2.0);

	const QStringList first = lineOf(file, QStringLiteral("XA-201"),
					 QStringLiteral("1"));
	const QStringList second = lineOf(file, QStringLiteral("XA-202"),
					  QStringLiteral("1"));
	REQUIRE_FALSE(first.isEmpty());
	REQUIRE_FALSE(second.isEmpty());
	CHECK(lineCell(first, QStringLiteral("quantity")) == QStringLiteral("1"));
	CHECK(lineCell(first, QStringLiteral("tags")) == QStringLiteral("Q1"));
	CHECK(lineCell(second, QStringLiteral("quantity")) == QStringLiteral("1"));
	CHECK(lineCell(second, QStringLiteral("tags")) == QStringLiteral("Q2"));
}

TEST_CASE("CU-18.12 — os sete itens tirados da nomenclatura saem do arquivo e "
	  "são anunciados", "[purchase][bom][t18]")
{
	const QList<Instance> instances = benchInstances();
	REQUIRE(expectedExcluded(instances) == 7);

	const PurchaseFile file = exportOf(fixtureXml(instances));
	INFO(file.text.toStdString());
	REQUIRE(file.written);
	checkShape(file);

		//Announced: the number, in the counts at the front.
	CHECK(countIn(file, QStringLiteral("excluded")) == 7);

		//And out of the file: no line of their part, no tag of theirs on
		//any line, and not in the pending block either - they are not
		//pending, somebody decided.
	CHECK(linesOf(file, QStringLiteral("XA-900")).isEmpty());
	QSet<QString> excluded_tags;
	for (const Instance &instance : instances)
	{
		if (instance.excluded) {
			excluded_tags.insert(instance.label);
		}
	}
	for (const QStringList &row : dataOf(file, 1))
	{
		for (const QString &tag : tagsOf(row))
		{
			INFO(tag.toStdString());
			CHECK_FALSE(excluded_tags.contains(tag));
		}
	}
	for (const QStringList &row : dataOf(file, 2)) {
		CHECK_FALSE(excluded_tags.contains(
				    pendingCell(row, QStringLiteral("tag"))));
	}
}

TEST_CASE("CU-18.14 — a revisão nova de uma peça vira linha própria, dizendo "
	  "qual comprar", "[purchase][bom][t18]")
{
		//Before: K1, K2 and K3 all in revision 1.
	QList<Instance> before = benchInstances();
	for (Instance &instance : before)
	{
		if (instance.label == QStringLiteral("K3")) {
			instance.part_revision = QStringLiteral("1");
		}
	}
	const PurchaseFile old_file = exportOf(fixtureXml(before));
	INFO(old_file.text.toStdString());
	REQUIRE(old_file.written);
	checkShape(old_file);

	REQUIRE(linesOf(old_file, QStringLiteral("XA-300")).size() == 1);
	const QStringList old_line = lineOf(old_file, QStringLiteral("XA-300"),
					    QStringLiteral("1"));
	REQUIRE_FALSE(old_line.isEmpty());
	CHECK(lineCell(old_line, QStringLiteral("quantity")) == QStringLiteral("3"));

		//After: K3 went up one revision, and exported again.
	const QList<Instance> after = benchInstances();
	const PurchaseFile new_file = exportOf(fixtureXml(after));
	INFO(new_file.text.toStdString());
	REQUIRE(new_file.written);
	checkShape(new_file);

		//Two lines, each saying its revision and its own quantity - not
		//one line with the two added and the revision of one of them.
	const QVector<QStringList> contactors =
		linesOf(new_file, QStringLiteral("XA-300"));
	REQUIRE(contactors.size() == 2);

	const QStringList current = lineOf(new_file, QStringLiteral("XA-300"),
					   QStringLiteral("1"));
	const QStringList raised = lineOf(new_file, QStringLiteral("XA-300"),
					  QStringLiteral("2"));
	REQUIRE_FALSE(current.isEmpty());
	REQUIRE_FALSE(raised.isEmpty());
	CHECK(lineCell(current, QStringLiteral("quantity")) == QStringLiteral("2"));
	CHECK(tagsOf(current)
	      == (QStringList {QStringLiteral("K1"), QStringLiteral("K2")}));
	CHECK(lineCell(raised, QStringLiteral("quantity")) == QStringLiteral("1"));
	CHECK(lineCell(raised, QStringLiteral("tags")) == QStringLiteral("K3"));

		//The total of the part did not move, and neither did the whole
		//list: the revision split a line, it did not add a piece.
	CHECK(piecesOf(contactors) == 3.0);
	CHECK(piecesOf(dataOf(new_file, 1)) == piecesOf(dataOf(old_file, 1)));
	CHECK(dataOf(new_file, 1).size() == dataOf(old_file, 1).size() + 1);
}

TEST_CASE("T18 — o marcador que não nomeia nada é contado, e não vira "
	  "pendência", "[purchase][bom][t18]")
{
		//Not a use case: the decision underneath CU-16.6. The collector
		//reads the view that keeps every drawn thing, and a drawn marker
		//with nothing on it would come back as a pending item with no
		//tag and no designation - a hundred of them on a project of
		//fourteen folios, burying the five that need a part.
	const QList<Instance> instances = benchInstances();
	REQUIRE(expectedNameless(instances) == 1);

	const PurchaseFile file = exportOf(fixtureXml(instances));
	INFO(file.text.toStdString());
	REQUIRE(file.written);
	checkShape(file);

	CHECK(countIn(file, QStringLiteral("nameless")) == 1);
	CHECK(countIn(file, QStringLiteral("pending"))
	      == expectedPending(instances));
	CHECK(dataOf(file, 2).size() == expectedPending(instances));
	for (const QStringList &row : dataOf(file, 2))
	{
		INFO(row.join(QStringLiteral(";")).toStdString());
		CHECK_FALSE((pendingCell(row, QStringLiteral("tag")).isEmpty()
			     && pendingCell(row, QStringLiteral("designation"))
				.isEmpty()));
	}
}

TEST_CASE("T18 — o código de saída distingue lista inteira, lista com "
	  "pendência e falha", "[purchase][bom][t18]")
{
	SECTION("inteira: 0, mesmo com itens excluídos e marcadores contados")
	{
			//The bench without its five components with no part. The
			//seven excluded and the marker stay: they are announced,
			//and they do not make the list less whole.
		const QList<Instance> instances =
			without(benchInstances(),
				QStringList {QStringLiteral("H1"),
					     QStringLiteral("H2"),
					     QStringLiteral("H3"),
					     QStringLiteral("H4"),
					     QStringLiteral("H5")});
		REQUIRE(expectedPending(instances) == 0);
		REQUIRE(expectedExcluded(instances) == 7);
		REQUIRE(expectedNameless(instances) == 1);

		const PurchaseFile file = exportOf(fixtureXml(instances));
		INFO(file.text.toStdString());
		REQUIRE(file.written);
		checkShape(file);

		CHECK(file.code == 0);
		CHECK(countIn(file, QStringLiteral("pending")) == 0);
		CHECK(countIn(file, QStringLiteral("excluded")) == 7);
		CHECK(countIn(file, QStringLiteral("nameless")) == 1);
			//The block is there with nothing under its header, so a
			//reader finds the same three blocks in every file.
		CHECK(dataOf(file, 2).isEmpty());
	}

	SECTION("com pendência: 3, e o arquivo está lá")
	{
		const PurchaseFile file = exportOf(fixtureXml(benchInstances()));
		INFO(file.text.toStdString());
		CHECK(file.code == 3);
		CHECK(file.written);
	}

	SECTION("falha ao escrever: 1, e nenhum arquivo")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		const QString project_path =
			dir.filePath(QStringLiteral("bench.qet"));
		REQUIRE(writeFile(project_path, fixtureXml(benchInstances())));

			//A folder that does not exist: nothing can be written
			//there, and nothing must be.
		const QString csv_path =
			dir.filePath(QStringLiteral("missing/purchase.csv"));
		const int code = CLIExport::run(QStringList()
						<< QStringLiteral("--export-purchase")
						<< project_path << csv_path);
		CHECK(code == 1);
		CHECK_FALSE(QFile::exists(csv_path));
	}

	SECTION("projeto que não existe: 2, como os outros verbos")
	{
		QTemporaryDir dir;
		REQUIRE(dir.isValid());
		const QString csv_path =
			dir.filePath(QStringLiteral("purchase.csv"));
		const int code = CLIExport::run(
			QStringList() << QStringLiteral("--export-purchase")
				      << dir.filePath(QStringLiteral("none.qet"))
				      << csv_path);
		CHECK(code == 2);
		CHECK_FALSE(QFile::exists(csv_path));
	}
}

/*
	The plate, the third half of the list (T19, step 10).

	Labelled T19 and not CU-19.11, and the difference is the rule about what
	a label promises: that case lays the plate out through the window, from
	the catalogue, and prints the list from the program, and none of the
	three happens here - the plate is built by MountingLayout and the list
	is the command line's. What these two cases prove is the half CU-19.11
	stands on and that nothing above it reached: the file the shop orders
	from counts the plate, and counts each part of it once.
*/

TEST_CASE("T19 — a peça da placa sem símbolo entra uma vez na lista de compra, "
	  "e a que tem símbolo não soma de novo", "[purchase][bom][t19]")
{
		//Two contactors of one part, and no revision on either: a part
		//of the plate is revision 0, so a plate that counted K1 again
		//would add it into this very line - the same number of lines,
		//and a quantity of three. That is the defect only a total sees,
		//and the bench is built so that it would be the one to happen.
	QList<Instance> instances;
	instances << drawn(1, "K1", "Contator 9 A", "XA-300", "")
		  << drawn(1, "K2", "Contator 9 A", "XA-300", "");
	const QString k1 = uuidOfTag(instances, QStringLiteral("K1"));
	REQUIRE_FALSE(k1.isEmpty());

		//(a) a block laid from the catalogue, with no symbol behind it;
		//(b) the plate part of K1, whose symbol is on the first folio;
		//(c) a piece of rail cut from a bar that has a code;
		//(d) a part whose symbol was deleted after it was mounted - it
		//    names an element of the bench that no folio holds. Without
		//    it a set built from the names alone, never asking the
		//    folios, would pass every assertion below.
	const QString plate = plateXml(
		QList<MountedItem>()
		<< piece(QStringLiteral("XA-500"), QString(), QString(), 10.0)
		<< piece(QStringLiteral("XA-300"), QStringLiteral("K1"), k1, 40.0)
		<< railPiece(QStringLiteral("XA-700"))
		<< piece(QStringLiteral("XA-600"), QStringLiteral("-F9"),
			 elementUuid(99), 70.0));
	REQUIRE_FALSE(plate.isEmpty());
	REQUIRE(plate.count(QStringLiteral("<mounted_item")) == 4);

	const PurchaseFile file = exportOf(fixtureXml(instances, plate));
	INFO(file.text.toStdString());
	REQUIRE(file.written);
	checkShape(file);

		//Every part of the plate has a code, so the list is whole.
	CHECK(file.code == 0);
	CHECK(countIn(file, QStringLiteral("pending")) == 0);
	CHECK(dataOf(file, 2).isEmpty());

		//(a): on the list, one piece, named by where it is screwed -
		//it has no tag to be named by.
	const QStringList block = lineOf(file, QStringLiteral("XA-500"), QString());
	REQUIRE_FALSE(block.isEmpty());
	CHECK(lineCell(block, QStringLiteral("quantity")) == QStringLiteral("1"));
	CHECK(lineCell(block, QStringLiteral("tags")) == platePath());

		//(b): one line, the two symbols and nothing from the plate - not
		//three, and K1 once among the tags and not twice.
	REQUIRE(linesOf(file, QStringLiteral("XA-300")).size() == 1);
	const QStringList contactor = lineOf(file, QStringLiteral("XA-300"),
					     QString());
	REQUIRE_FALSE(contactor.isEmpty());
	CHECK(lineCell(contactor, QStringLiteral("quantity")) == QStringLiteral("2"));
	CHECK(tagsOf(contactor)
	      == (QStringList {QStringLiteral("K1"), QStringLiteral("K2")}));
	CHECK(lineCell(contactor, QStringLiteral("designation"))
	      == QStringLiteral("Contator 9 A"));

		//(c): no line ordering rail by the piece.
	CHECK(linesOf(file, QStringLiteral("XA-700")).isEmpty());

		//(d): the symbol is gone, the part is still on the plate the shop
		//builds, so it is bought - one piece, under its tag.
	const QStringList orphan = lineOf(file, QStringLiteral("XA-600"), QString());
	REQUIRE_FALSE(orphan.isEmpty());
	CHECK(lineCell(orphan, QStringLiteral("quantity")) == QStringLiteral("1"));
	CHECK(lineCell(orphan, QStringLiteral("tags")) == QStringLiteral("-F9"));

		//The count of lines: the drawn half, the enclosures, and the two
		//parts of the plate no folio draws - (a) and (d).
	const int lines = expectedDrawnLines(instances) + enclosure_lines + 2;
	REQUIRE(lines == 6);
	CHECK(dataOf(file, 1).size() == lines);
	CHECK(countIn(file, QStringLiteral("lines")) == lines);

		//And the total, which is what catches (b): counted twice, it
		//changes no count of lines and makes this nine.
	const double pieces = expectedDrawnPieces(instances) + enclosure_pieces
			      + 2.0;
	REQUIRE(pieces == 8.0);
	CHECK(piecesOf(dataOf(file, 1)) == pieces);

		//And the count in front says how many came from the plate.
	CHECK(countIn(file, QStringLiteral("mounted")) == 2);
}

TEST_CASE("T19 — sem nenhum símbolo desenhado, a lista de compra sai da placa",
	  "[purchase][bom][t19]")
{
		//The layout before the schematic: two folios, nothing on them.
	const QList<Instance> instances;
	REQUIRE(expectedDrawnLines(instances) == 0);

		//Two blocks of one part laid from the catalogue, a piece of
		//rail, and a part nobody gave a code to.
	const QString plate = plateXml(
		QList<MountedItem>()
		<< piece(QStringLiteral("XA-500"), QString(), QString(), 10.0)
		<< piece(QStringLiteral("XA-500"), QString(), QString(), 40.0)
		<< railPiece(QStringLiteral("XA-700"))
		<< piece(QString(), QStringLiteral("-X9"), QString(), 70.0));
	REQUIRE_FALSE(plate.isEmpty());
	REQUIRE(plate.count(QStringLiteral("<mounted_item")) == 4);

	const PurchaseFile file = exportOf(fixtureXml(instances, plate));
	INFO(file.text.toStdString());
	REQUIRE(file.written);
	checkShape(file);

		//The two blocks are one line of two, and the place is said once:
		//one plate, not one copy of its path per block.
	const QStringList blocks = lineOf(file, QStringLiteral("XA-500"), QString());
	REQUIRE_FALSE(blocks.isEmpty());
	CHECK(lineCell(blocks, QStringLiteral("quantity")) == QStringLiteral("2"));
	CHECK(lineCell(blocks, QStringLiteral("tags")) == platePath());
	CHECK(linesOf(file, QStringLiteral("XA-700")).isEmpty());

	const int lines = expectedDrawnLines(instances) + enclosure_lines + 1;
	REQUIRE(lines == 4);
	CHECK(dataOf(file, 1).size() == lines);
	CHECK(countIn(file, QStringLiteral("lines")) == lines);

	const double pieces = expectedDrawnPieces(instances) + enclosure_pieces
			      + 2.0;
	REQUIRE(pieces == 6.0);
	CHECK(piecesOf(dataOf(file, 1)) == pieces);
	CHECK(countIn(file, QStringLiteral("mounted")) == 2);

		//The part with no code is not dropped and not a line with an
		//empty code: it is pending, named as the plate names it and
		//where it is screwed - and the list says it is not whole.
	CHECK(file.code == 3);
	CHECK(countIn(file, QStringLiteral("pending")) == 1);
	const QVector<QStringList> pendings = dataOf(file, 2);
	REQUIRE(pendings.size() == 1);
	CHECK(pendings.first()
	      == (QStringList {QStringLiteral("no_part"), QStringLiteral("-X9"),
			       QString(), platePath()}));
	for (const QStringList &row : dataOf(file, 1)) {
		CHECK_FALSE(lineCell(row, QStringLiteral("part_code")).isEmpty());
	}
}
