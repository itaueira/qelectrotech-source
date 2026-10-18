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
#include "bomcollector.h"

#include "bommeasure.h"
#include "mountinglayout.h"
#include "mountingprofile.h"
#include "undrawnparts.h"

#include "../qetinformation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace BomCollector
{

namespace {

/**
	@param value a quantity, already a number
	@return true when a line can be bought from it

	One spelling for the two halves that read a quantity - the enclosures
	arrive with theirs worked out: a symbol whose field says zero and a bar
	nobody gave a length to are refused by the same words. Zero is refused
	with the rest because it looks like an answer and is not one this list
	can act on - see quantityOf().
*/
bool isUsableQuantity(double value)
{
	return std::isfinite(value) && value > 0.0;
}

/**
	@param raw the unit a row says it is in
	@return the unit its line carries: the count unit when it says none
*/
QString lineUnitOf(const QString &raw)
{
	const QString unit = raw.trimmed();
	return unit.isEmpty() ? BomMeasure::countUnit() : unit;
}

/**
	@param a one unit
	@param b another one
	@return true when a quantity in one may be added to a quantity in the
	other

	A count is a count whatever word sits beside it, so anything that is
	not a length reads as the same thing: "un", "pc" and an empty cell all
	mean pieces. Two lengths have to be spelled the same, because adding
	millimetres to metres is how a rail order comes out a thousand times
	too small and still looks like a number somebody meant.
*/
bool readsAsSameUnit(const QString &a, const QString &b)
{
	if (!BomMeasure::isLengthUnit(a) && !BomMeasure::isLengthUnit(b)) {
		return true;
	}
	return a.trimmed().compare(b.trimmed(), Qt::CaseInsensitive) == 0;
}

/**
	@brief Record one item the list cannot answer for.
	@param result where to record it
	@param reason why
	@param name the tag of a component, the code of a location
	@param designation what it says it is
	@param where the folio it is drawn on, or the path it sits at
*/
void addPending(Result &result,
		Pendency reason,
		const QString &name,
		const QString &designation,
		const QString &where)
{
	Pending pending;
	pending.reason = reason;
	pending.name = name;
	pending.designation = designation;
	pending.where = where;
	result.pendings.append(pending);
}

/**
	@param path a path of location codes
	@return the code of the location itself, the parents left out

	What a person calls the place. The whole path answers "where", and the
	last code answers "which one", and a pendency needs both.
*/
QString lastCodeOf(const QString &path)
{
	const QStringList codes = LocationTree::splitPath(path);
	return codes.isEmpty() ? path : codes.last();
}

} // namespace

QString viewName()
{
	return QStringLiteral("element_label_view");
}

QStringList columns()
{
		//The key first, because it is what a line is, then what a person
		//reads on it, then the two that only a pendency needs, then the
		//box. Named through QETInformation rather than written out: the
		//column of the view is the element information key itself, so
		//taking the name from there is what keeps the question and the
		//answer from drifting apart when a key is renamed.
	static const QStringList list {
		QETInformation::ELMT_PART_CODE,
		QETInformation::ELMT_PART_REVISION,
		QETInformation::ELMT_DESIGNATION,
		QETInformation::ELMT_QUANTITY,
		QETInformation::ELMT_UNITY,
		QETInformation::ELMT_LABEL,
		QStringLiteral("folio"),
		QStringLiteral("exclude_from_bom")
	};
	return list;
}

QString selectStatement()
{
		//Ordered by the key of the line first, so that the rows of one
		//part arrive together and arrive in the same order twice. Where
		//the component sits breaks the tie, which makes the paths of a
		//line read in folio order.
	return QStringLiteral("SELECT ") + columns().join(QStringLiteral(", "))
			+ QStringLiteral(" FROM ") + viewName()
			+ QStringLiteral(" ORDER BY part_code, part_revision,"
					 " diagram_position, position, label");
}

/**
	@brief BomCollector::isExcluded
	See the header for why the comparison is this literal.
*/
bool isExcluded(const QString &raw)
{
		//Not even trimmed, and that is on purpose: the nomenclature view
		//compares the stored cell against 'true' with no trimming either,
		//so a cell holding a stray space is kept by the view and has to be
		//kept here. Being stricter than the view would be a silent
		//disagreement between two lists of the same project.
	return raw == QLatin1String("true");
}

/**
	@brief BomCollector::revisionOf
	@param raw the part_revision cell
	@return the revision it says, 0 when it says nothing usable
*/
int revisionOf(const QString &raw)
{
	bool parsed = false;
	const int value = raw.trimmed().toInt(&parsed);
	return (parsed && value > 0) ? value : 0;
}

/**
	@brief BomCollector::quantityOf
	See the header for why zero is refused along with the text that is not
	a number.
*/
double quantityOf(const QString &raw, bool *ok)
{
	if (ok) {
		*ok = true;
	}

	const QString text = raw.trimmed();
	if (text.isEmpty()) {
		return 1.0;
	}

	bool parsed = false;
	const double value = text.toDouble(&parsed);
	if (!parsed || !isUsableQuantity(value))
	{
		if (ok) {
			*ok = false;
		}
			//One, and not zero, for the caller that ignores ok: an
			//understated line is wrong and visible, a line that
			//vanished is wrong and is not.
		return 1.0;
	}
	return value;
}

/**
	@brief BomCollector::rowFrom
	@param values one row of selectStatement(), column name to cell
	@return the row, its cells still text
*/
ElementRow rowFrom(const QHash<QString, QString> &values)
{
	ElementRow row;
	row.part_code = values.value(QETInformation::ELMT_PART_CODE);
	row.part_revision = values.value(QETInformation::ELMT_PART_REVISION);
	row.designation = values.value(QETInformation::ELMT_DESIGNATION);
	row.quantity = values.value(QETInformation::ELMT_QUANTITY);
	row.unit = values.value(QETInformation::ELMT_UNITY);
	row.label = values.value(QETInformation::ELMT_LABEL);
	row.folio = values.value(QStringLiteral("folio"));
	row.exclude_from_bom = values.value(QStringLiteral("exclude_from_bom"));
	return row;
}

/**
	@brief BomCollector::mountedRowsOf
	See the header for which parts of the layout come out, and for the one
	input this cannot check.
*/
QList<MountedRow> mountedRowsOf(const MountingLayout &layout,
				const QSet<QUuid> &drawn_elements)
{
		//Where each face is, worked out once and not once per part. The
		//location path, which is how the half nobody draws names a place
		//too; the face's own words only for a face that belongs to no
		//location, which a file may hold and the read keeps.
	QHash<QString, QString> where_of;
	for (int index = 0 ; index < layout.count() ; ++index)
	{
		const MountingSurface &surface = layout.at(index);
		where_of.insert(surface.uuid,
				surface.location_path.isEmpty()
				? surface.designation()
				: surface.location_path);
	}

	QList<MountedRow> rows;
		//UndrawnPart decides which parts no folio draws, and nothing here
		//decides it again: a second reading of the link would be free to
		//disagree with the window that lists the same parts.
	for (const UndrawnPart &part : UndrawnPart::of(layout, drawn_elements))
	{
		const MountedItem &item = part.item;
			//A piece cut from a bar: bought by the length of the bar,
			//which is the loop below, and never by the piece.
		if (item.isCutToLength()) {
			continue;
		}

		MountedRow row;
		row.part_code = item.part_code;
		row.label = item.label;
		row.name = item.designation();
		row.where = where_of.value(part.surface_uuid);
		rows << row;
	}

		//And the bars, face by face. What a face cut is the face's own
		//answer - profileTotals() groups by the key of the profile and
		//keeps apart the pieces nobody measured - and nothing here adds
		//up a length again: a second sum would be free to disagree with
		//the layout's. Face by face rather than over the whole project so
		//that each row can say where it was cut, which is what the tags
		//of the line and a pendency both need; collect() adds the faces
		//up by the same key.
	for (int index = 0 ; index < layout.count() ; ++index)
	{
		const MountingSurface &surface = layout.at(index);
		const QList<MountingProfileTotal> totals = surface.profileTotals();
		for (const MountingProfileTotal &total : totals)
		{
			MountedRow bar;
			bar.part_code = total.profile.key();
			bar.name = total.profile.designation();
			bar.designation = bar.name;
			bar.where = where_of.value(surface.uuid);
			bar.unit = BomMeasure::defaultLengthUnit();

			const int measured = total.pieces - total.uncut;
			if (measured > 0)
			{
				bar.quantity = total.metres();
				bar.pieces = measured;
				rows << bar;
			}

				//Not a number, and not zero: the piece is still to
				//be cut, and a zero added into the line would read
				//as a length somebody measured. One row per piece,
				//so that each is a pendency of its own.
			bar.quantity = std::numeric_limits<double>::quiet_NaN();
			bar.pieces = 1;
			for (int uncut = 0 ; uncut < total.uncut ; ++uncut) {
				rows << bar;
			}
		}
	}
	return rows;
}

/**
	@brief BomCollector::collect
	See the header for the halves and for the key they are joined on.

	Every drawn row leaves this function through exactly one of three
	doors - a line, a pendency, or the exclusion count - so
	drawn + pendings + excluded is the number of rows that came in. That
	is not decoration: it is the one statement that makes "a row was lost"
	impossible to write, and it is what the bench asserts.
*/
Result collect(const QList<ElementRow> &rows,
	       const QList<LocationTree::BomLine> &enclosures,
	       const QList<MountedRow> &mounted)
{
	Result result;

		//The drawn half first, so that a part both halves use is named by
		//its designation and not by the name of a place - see the header.
	for (const ElementRow &row : rows)
	{
		if (isExcluded(row.exclude_from_bom))
		{
			++ result.excluded;
			continue;
		}

		const QString code = row.part_code.trimmed();
		if (code.isEmpty())
		{
			addPending(result, Pendency::NoPart, row.label,
				   row.designation, row.folio);
			continue;
		}

		bool readable = false;
		const double quantity = quantityOf(row.quantity, &readable);
		if (!readable)
		{
			addPending(result, Pendency::UnreadableQuantity, row.label,
				   row.designation, row.folio);
			continue;
		}

		const QString unit = lineUnitOf(row.unit);
		const int revision = revisionOf(row.part_revision);

		int found = LocationTree::indexOfBomLine(result.lines, code,
							revision);
		if (found < 0)
		{
			LocationTree::BomLine line;
			line.part_code = code;
			line.part_revision = revision;
			line.name = row.designation;
			line.unit = unit;
			result.lines.append(line);
			found = int(result.lines.count()) - 1;
		}
		else if (!readsAsSameUnit(result.lines.at(found).unit, unit))
		{
				//Announced instead of added, and instead of split
				//onto a second line: the key of a line is the part
				//and its revision, so a line per unit would be a
				//second key nobody declared. Adding them would be
				//worse - the total would stay plausible.
			addPending(result, Pendency::UnitConflict, row.label,
				   row.designation, row.folio);
			continue;
		}

		if (result.lines.at(found).name.isEmpty()) {
			result.lines[found].name = row.designation;
		}
		result.lines[found].quantity += quantity;
		if (!row.label.trimmed().isEmpty()) {
			result.lines[found].paths << row.label.trimmed();
		}
		++ result.drawn;
	}

		//And the half nobody drew. Its quantities arrive already worked
		//out - LocationTree::bomLines() counts one piece per place - so
		//there is nothing to read here, only to add.
	for (const LocationTree::BomLine &enclosure : enclosures)
	{
		const QString code = enclosure.part_code.trimmed();
		if (code.isEmpty())
		{
				//One pendency per place and not one per line.
				//bomLines() puts every location that was never
				//assigned a part on a single code-less line, so
				//reporting that line as one item would announce one
				//missing cabinet where there are nine.
			for (const QString &path : enclosure.paths)
			{
					//No designation, and that is not an
					//omission: the name on a code-less line
					//belongs to the first location that
					//landed on it, so carrying it onto the
					//others would put one cabinet's name on
					//another cabinet's pendency. The path
					//says which place, which is what the
					//tree actually knows.
				addPending(result, Pendency::NoPart,
					   lastCodeOf(path), QString(), path);
			}
			if (enclosure.paths.isEmpty())
			{
				addPending(result, Pendency::NoPart,
					   enclosure.name, QString(), QString());
			}
			continue;
		}

		int found = LocationTree::indexOfBomLine(result.lines, code,
							enclosure.part_revision);
		if (found < 0)
		{
				//Copied with the code trimmed, and not as it came:
				//the lookup above trims, so a code carrying a
				//stray space would be stored untrimmed, missed by
				//the next lookup, and answered with a second line
				//for the same part.
			LocationTree::BomLine line = enclosure;
			line.part_code = code;
			result.lines.append(line);
			++ result.enclosures;
			continue;
		}

		if (!readsAsSameUnit(result.lines.at(found).unit, enclosure.unit))
		{
			addPending(result, Pendency::UnitConflict,
				   lastCodeOf(enclosure.paths.value(0)),
				   enclosure.name,
				   enclosure.paths.value(0));
			continue;
		}

		if (result.lines.at(found).name.isEmpty()) {
			result.lines[found].name = enclosure.name;
		}
		result.lines[found].quantity += enclosure.quantity;
		result.lines[found].paths << enclosure.paths;
		++ result.enclosures;
	}

		//And the plate: the parts on it that no folio draws, and the bars
		//cut on it. Whether a part belongs here was settled by
		//mountedRowsOf(), and settled by the link to its symbol: what
		//arrives is counted, and nothing that arrives is a part some
		//symbol already counts.
	for (const MountedRow &row : mounted)
	{
			//The tag column of a pendency: the tag when the row says
			//what the product is, and otherwise the plate's own name
			//for it, which is all there is - a part with no code and
			//no tag still has to be found on the plate. A bar says
			//what it is and carries no tag, so it is named by its
			//profile and not by its profile twice.
		const QString tag = row.designation.isEmpty() ? row.name
							       : row.label;

		const QString code = row.part_code.trimmed();
		if (code.isEmpty())
		{
			addPending(result, Pendency::NoPart, tag, row.designation,
				   row.where);
			continue;
		}

			//Refused by the rule a symbol's quantity is refused by,
			//and the case it is here for is the piece of a bar nobody
			//gave a length to: as zero metres it would make the line
			//order short, and nothing on paper would say so.
		if (!isUsableQuantity(row.quantity))
		{
			addPending(result, Pendency::UnreadableQuantity, tag,
				   row.designation, row.where);
			continue;
		}

			//Revision 0: the layout keeps none, and 0 is "whatever is
			//current" - see LocationTree::BomLine.
		const QString unit = lineUnitOf(row.unit);
		int found = LocationTree::indexOfBomLine(result.lines, code, 0);
		if (found < 0)
		{
				//The name of the product when the plate knows it,
				//which is a bar's profile. A part bought as a piece
				//has none here: the plate keeps the code it was laid
				//as and not what the catalogue calls it, and another
				//half that names the same part fills it in.
			LocationTree::BomLine line;
			line.part_code = code;
			line.name = row.designation;
			line.unit = unit;
			result.lines.append(line);
			found = int(result.lines.count()) - 1;
		}
		else if (!readsAsSameUnit(result.lines.at(found).unit, unit))
		{
			addPending(result, Pendency::UnitConflict, tag,
				   row.designation, row.where);
			continue;
		}

		if (result.lines.at(found).name.isEmpty()) {
			result.lines[found].name = row.designation;
		}
			//Added as a number, under the key: two bars that come to
			//the same metres are two lines when their profiles differ,
			//and one bar cut on two faces is one line of the sum.
		result.lines[found].quantity += row.quantity;
			//The tag when the part has one, as a symbol is named by
			//its tag. A part laid from the catalogue has none, and is
			//named by the place it is screwed in - once per place: a
			//plate of twelve terminal blocks is one place, and twelve
			//copies of its path would bury the tags beside them.
		const QString label = row.label.trimmed();
		if (!label.isEmpty()) {
			result.lines[found].paths << label;
		}
		else if (!row.where.isEmpty()
			 && !result.lines.at(found).paths.contains(row.where)) {
			result.lines[found].paths << row.where;
		}
			//Pieces, never metres - see Result::mounted.
		result.mounted += row.pieces;
	}

		//Ordered by the key, so that two exports of the same project can
		//be read side by side. Without this the order would depend on
		//which half happened to name a part first, and a cabinet bought
		//as a part somebody also drew would move up and down the file for
		//reasons nobody could see.
	std::sort(result.lines.begin(), result.lines.end(),
		  [](const LocationTree::BomLine &a,
		     const LocationTree::BomLine &b)
	{
		if (a.part_code != b.part_code) {
			return a.part_code < b.part_code;
		}
		return a.part_revision < b.part_revision;
	});

	return result;
}

/**
	@brief BomCollector::countOf
	@param result what collect() answered
	@param reason the kind of pendency to count
	@return how many of them there are
*/
int countOf(const Result &result, Pendency reason)
{
	int total = 0;
	for (const Pending &pending : result.pendings)
	{
		if (pending.reason == reason) {
			++ total;
		}
	}
	return total;
}

/**
	@brief BomCollector::totalOf
	See the header for why this is never asked over the whole list.
*/
double totalOf(const QList<LocationTree::BomLine> &lines, const QString &unit)
{
	double total = 0.0;
	for (const LocationTree::BomLine &line : lines)
	{
		if (readsAsSameUnit(line.unit, unit)) {
			total += line.quantity;
		}
	}
	return total;
}

} // namespace BomCollector
