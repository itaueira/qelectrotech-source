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
#include "wirenames.h"

#include "QPropertyUndoCommand/qpropertyundocommand.h"
#include "autoNum/assignvariables.h"
#include "autoNum/autonumschemecommand.h"
#include "autoNum/renumberplan.h"
#include "conductorautonumerotation.h"
#include "diagram.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetproject.h"
#include "undocommand/groupedupdatecommand.h"
#include "undocommand/setautonumcontextcommand.h"
#include "wiresections.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVariant>

#include <algorithm>
#include <utility>

namespace
{
	const QString unit_folio = QStringLiteral("unitfolio");

	QString undoCaption()
	{
		return QObject::tr("Convert the wire names", "undo caption");
	}

	QString renumberCaption()
	{
		return QObject::tr("Renumber the wires", "undo caption");
	}

		/**
			The title a scheme created by the conversion takes: a name that
			says what it writes, followed by a number when a scheme of
			@p project writing something else has it already
		*/
	QString newSchemeTitle(const QETProject *project)
	{
		const QString base = QObject::tr(
					"Wire name per folio, with colour and cross-section",
					"name of the conductor numbering the wire names are converted to");
		QString title = base;
		for (int i = 2 ; !AutoNumSchemeCommand::nameProblem(
				 project, AutoNumSchemeCommand::Kind::Conductor, title).isEmpty() ; ++i)
		{
			title = QStringLiteral("%1 %2").arg(base).arg(i);
		}
		return QETProject::normalizedAutoNumName(title);
	}

	bool sameContext(const NumerotationContext &a, const NumerotationContext &b)
	{
		if (a.size() != b.size()) {
			return false;
		}
		for (int i = 0 ; i < a.size() ; ++i)
		{
			if (a[i] != b[i]) {
				return false;
			}
		}
		return true;
	}

		/**
			@return @p context with the value of its folio counter at
			@p highest plus its increase: the count a folio whose highest
			number is @p highest goes on from, as Diagram::loadFolioSeqHash()
			works it out when the folio is shown
		*/
	NumerotationContext countingFrom(const NumerotationContext &context, int highest)
	{
		NumerotationContext counted = context;
		for (int i = 0 ; i < counted.size() ; ++i)
		{
			const QStringList part = counted.itemAt(i);
			if (part.value(0) == unit_folio) {
				counted.replaceValue(i, QString::number(highest + part.value(2).toInt()));
			}
		}
		return counted;
	}

		/// @return true when @p potential holds a wire of a panel already built
	bool holdsFrozen(const QList<Conductor *> &potential)
	{
		for (const Conductor *conductor : potential)
		{
			if (ConductorAutoNumerotation::isFrozen(conductor)) {
				return true;
			}
		}
		return false;
	}

		/**
			A folio made to follow the scheme of the wire names, and the
			highest number of its folio counter set: the number
			Diagram::loadCndFolioSeq() goes on from when the folio is shown
			(Diagram::m_cnd_unitfolio_max, written in the file with the
			folio).
		*/
	class FolioCountCommand : public QUndoCommand
	{
		public:
			FolioCountCommand(Diagram *diagram, const QString &title,
					  const QStringList &count, QUndoCommand *parent) :
				QUndoCommand(parent),
				m_diagram(diagram),
				m_title(title),
				m_name_before(diagram->conductorsAutonumName()),
				m_had_count(diagram->m_cnd_unitfolio_max.contains(title)),
				m_count_before(diagram->m_cnd_unitfolio_max.value(title)),
				m_count_after(count)
			{}

				/// Whether the command would change anything on @p diagram
			static bool changes(const Diagram *diagram, const QString &title,
					    const QStringList &count)
			{
				return diagram->conductorsAutonumName() != title
						|| !diagram->m_cnd_unitfolio_max.contains(title)
						|| diagram->m_cnd_unitfolio_max.value(title) != count;
			}

			void redo() override
			{
				if (!m_diagram) {
					return;
				}
				m_diagram->setConductorsAutonumName(m_title);
				m_diagram->m_cnd_unitfolio_max.insert(m_title, m_count_after);
			}

			void undo() override
			{
				if (!m_diagram) {
					return;
				}
				m_diagram->setConductorsAutonumName(m_name_before);
				if (m_had_count) {
					m_diagram->m_cnd_unitfolio_max.insert(m_title, m_count_before);
				} else {
					m_diagram->m_cnd_unitfolio_max.remove(m_title);
				}
			}

		private:
			QPointer<Diagram> m_diagram;
			QString m_title;
			QString m_name_before;
			bool m_had_count;
			QStringList m_count_before;
			QStringList m_count_after;
	};

		/// What a cable formula starts with, its type after it
	QString cablePrefix()
	{
		return QStringLiteral("F%id-%sequf_1 (%wq Cabo ");
	}

		/**
			@return true for the text of an earth conductor: "PE" as a
			whole word at its start - followed by a space, a bracket or
			nothing - or after the number of a folio, as the conversion
			writes it ("F5 PE")
		*/
	bool isEarthText(const QString &text)
	{
		static const QRegularExpression earth(
					QStringLiteral("^ *(F[0-9]+ +)?PE(?=[ (]|$)"));
		return earth.match(text).hasMatch();
	}

		/**
			@return true for the text of a neutral or a phase typed without
			a number: "N" or "F" as a whole word at its start - which
			"F5-2" is not
		*/
	bool isLineText(const QString &text)
	{
		static const QRegularExpression line(QStringLiteral("^ *[NF](?=[ (]|$)"));
		return line.match(text).hasMatch();
	}

		/**
			@return true for the text of a cable, in the two ways the
			projects measured write one: starting with "Cabo " ("Cabo Flat
			(Paralelo) 26 Fios"), or with its type in brackets after what
			it goes to ("UCM (CAT6 STP)"). A short name with neither -
			"L1", "24V" - is a potential, and is numbered
		*/
	bool isCableText(const QString &text)
	{
		static const QRegularExpression cable(
					QStringLiteral("^ *Cabo |[(][^)]+[)]"),
					QRegularExpression::CaseInsensitiveOption);
		return cable.match(text).hasMatch();
	}

		/// @return true when @p text writes a cross-section, mm² or mm2
	bool carriesSection(const QString &text)
	{
		return text.contains(QStringLiteral("mm") + QChar(0x00B2), Qt::CaseInsensitive)
				|| text.contains(QStringLiteral("mm2"), Qt::CaseInsensitive);
	}

		/// @p list without its empty values
	QStringList withValues(const QStringList &list)
	{
		QStringList values;
		for (const QString &value : list)
		{
			if (!value.isEmpty()) {
				values << value;
			}
		}
		return values;
	}

		/**
			@return true when @p a and @p b count the same: a conductor read
			from a file holds one empty value in each part the file does not
			write (sequentialNumbers::fromXml() splits an empty text), and a
			project converted, saved and opened again must not be converted
			again for that
		*/
	bool sameSequence(const autonum::sequentialNumbers &a, const autonum::sequentialNumbers &b)
	{
		return withValues(a.unit) == withValues(b.unit)
				&& withValues(a.wrap) == withValues(b.wrap)
				&& withValues(a.unit_folio) == withValues(b.unit_folio)
				&& withValues(a.ten) == withValues(b.ten)
				&& withValues(a.ten_folio) == withValues(b.ten_folio)
				&& withValues(a.hundred) == withValues(b.hundred)
				&& withValues(a.hundred_folio) == withValues(b.hundred_folio)
				&& withValues(a.alpha) == withValues(b.alpha);
	}

		/**
			Add under @p parent what gives @p wire the formula @p formula,
			the count @p sequence_after and the cross-section @p section of
			its point.
			@return true when anything changes
		*/
	bool nameWire(Conductor *wire,
		      const QString &formula,
		      const autonum::sequentialNumbers &sequence_after,
		      const QString &section,
		      QUndoCommand *parent)
	{
		const ConductorProperties before = wire->properties();
		ConductorProperties after = before;
		after.m_formula = formula;
		after.m_wire_section = section;

		const autonum::sequentialNumbers sequence_before = wire->sequenceNum();

		const bool new_sequence = !sameSequence(sequence_before, sequence_after);
		const bool new_properties = before.m_formula != after.m_formula
				|| before.m_wire_section != after.m_wire_section;
		if (!new_sequence && !new_properties) {
			return false;
		}

			//The number first: setting the properties works the text out of
			//the formula with the number the conductor holds then. Undone in
			//the reverse order, the old formula is worked out of the old
			//number, last
		if (new_sequence)
		{
			QVariant old_value, new_value;
			old_value.setValue(sequence_before);
			new_value.setValue(sequence_after);
			new QPropertyUndoCommand(wire, "sequenceNum", old_value, new_value, parent);
		}
		if (new_properties)
		{
				//The command every edit of conductor properties goes
				//through, as for the cross-sections
			QVariant old_value, new_value;
			old_value.setValue(before);
			new_value.setValue(after);
			new QPropertyUndoCommand(wire, "properties", old_value, new_value, parent);
		}
		return true;
	}

		/// What a point is named as
	enum class Naming
	{
		Numbered, ///< F<folio>-<number> (<square> <cross-section>)
		Earth,    ///< F<folio> PE (<square> <cross-section>), no number
		Cable     ///< F<folio>-<number> (<square> Cabo <type>)
	};

		/// One point of the project, as the conversion reads it
	struct Point
	{
			/**
				its conductors, on every folio, in the order of the
				folios of the project and, on a folio, the order it lists
				them
			*/
		QList<Conductor *> wires;
			/// the folios it is drawn on, in the order of the project
		QList<Diagram *> folios;
			/// the folio of its name (see the header)
		Diagram *home = nullptr;
			/// the number its text carries, -1 for none
		int number = -1;
		bool frozen = false;
		Naming naming = Naming::Numbered;
			/// the formula of a cable, with its type in it, with %id
		QString cable_formula;
	};

		/**
			Say what @p point is named as, from what its conductors carry,
			in this order:
			- a formula of the conversion already there, earth or cable,
			  is kept: a project converted twice does not change;
			- a number typed "F<n>-<n>": numbered, keeping it;
			- otherwise the first text of the point: PE is the earth; N, F,
			  a text writing a cross-section and no text at all take the
			  next free number; any other text is a cable, its type taken
			  out of the text (WireNames::cableType()).
		*/
	void settleNaming(Point &point)
	{
		QString kept;
		QString text;
		for (Conductor *wire : std::as_const(point.wires))
		{
			const ConductorProperties properties = wire->properties();
				//Pinned to a folio too: the formula of a point across
				//folios
			const QString formula = WireNames::unpinned(properties.m_formula);
			if (kept.isEmpty()
			    && (formula == WireNames::earthFormula()
				|| formula.startsWith(cablePrefix()))) {
				kept = formula;
			}
			if (text.isEmpty() && !properties.text.trimmed().isEmpty()) {
				text = properties.text;
			}
		}

		if (kept == WireNames::earthFormula()) {
			point.naming = Naming::Earth;
		} else if (!kept.isEmpty()) {
			point.naming = Naming::Cable;
			point.cable_formula = kept;
		} else if (point.number >= 0) {
			point.naming = Naming::Numbered;
		} else if (isEarthText(text)) {
			point.naming = Naming::Earth;
		} else if (text.trimmed().isEmpty() || isLineText(text) || carriesSection(text)
			   || !isCableText(text)) {
			point.naming = Naming::Numbered;
		} else {
			point.naming = Naming::Cable;
			point.cable_formula = WireNames::cableFormula(WireNames::cableType(text));
		}
	}

		/**
			@return the points of the folios @p diagrams, in the order of
			the folios and, on a folio, the order their first conductor
			comes in the list of the folio; each one with all its
			conductors, on every folio, and the folio of its name
		*/
	QList<Point> pointsOf(const QList<Diagram *> &diagrams)
	{
		QHash<const Diagram *, int> order;
		QHash<Conductor *, int> rank;
		for (int d = 0 ; d < diagrams.size() ; ++d)
		{
			order.insert(diagrams.at(d), d);
			const QList<Conductor *> listed = diagrams.at(d)->conductors();
			for (int i = 0 ; i < listed.size() ; ++i) {
				rank.insert(listed.at(i), i);
			}
		}
		const auto before = [&order, &rank](Conductor *a, Conductor *b)
		{
			const int folio_a = order.value(a->diagram(), order.size());
			const int folio_b = order.value(b->diagram(), order.size());
			if (folio_a != folio_b) {
				return folio_a < folio_b;
			}
			return rank.value(a) < rank.value(b);
		};

		QList<Point> points;
		QSet<Conductor *> seen;
		for (Diagram *diagram : diagrams)
		{
			const QList<Conductor *> listed = diagram->conductors();
			for (Conductor *conductor : listed)
			{
				if (seen.contains(conductor)) {
					continue;
				}
				Point point;
				point.wires = WireSections::potentialOf(conductor);
				for (Conductor *member : std::as_const(point.wires)) {
					seen.insert(member);
				}
					//A wire left out of the numbering is not named, and
					//its number is not counted in its folio (T77)
				point.wires = ConductorAutoNumerotation::counted(point.wires);
				if (point.wires.isEmpty()) {
					continue;
				}
				std::sort(point.wires.begin(), point.wires.end(), before);

				int typed_folio = -1;
				int first_typed_folio = -1;
				for (Conductor *wire : std::as_const(point.wires))
				{
					Diagram *folio = wire->diagram();
					if (folio && !point.folios.contains(folio)) {
						point.folios << folio;
					}
						//The first conductor carrying a number gives it to
						//the point, and the folio its text says with it
					const QString text = wire->properties().text;
					const int number = WireNames::typedNumber(text);
					if (number >= 0 && point.number < 0) {
						point.number = number;
						typed_folio = WireNames::typedFolio(text);
					}
					if (first_typed_folio < 0) {
						first_typed_folio = WireNames::typedFolio(text);
					}
				}
					//No number: "F5 PE", an earth already converted
				if (typed_folio < 0) {
					typed_folio = first_typed_folio;
				}
					//The folio the typed name says, when the point is drawn
					//there - "F9-3" typed on folio 5 alone is named on folio
					//5 - and the first folio of the point otherwise
				point.home = point.folios.value(0, diagram);
				for (Diagram *folio : std::as_const(point.folios))
				{
					if (folio->folioIndex() + 1 == typed_folio) {
						point.home = folio;
						break;
					}
				}
				point.frozen = holdsFrozen(point.wires);
				settleNaming(point);
				points << point;
			}
		}
		return points;
	}

		/**
			Add under @p parent what names every wire of @p points (see
			WireNames::convertProject()).
			@param highest : the highest number each folio carries, read
			from the typed names - a frozen point's too, which the next free
			number must not take; it receives the numbers given
			@return how many conductors change
		*/
	int namePoints(QList<Point> &points, QHash<Diagram *, int> &highest, QUndoCommand *parent)
	{
		int changed = 0;
		for (Point &point : points)
		{
			if (point.frozen || !point.home) {
				continue;
			}
				//The earth has no number, and takes none from the folio
			QString formula = WireNames::earthFormula();
			autonum::sequentialNumbers sequence;
			if (point.naming != Naming::Earth)
			{
				if (point.number < 0) {
					point.number = ++highest[point.home];
				}
				sequence.unit_folio << QString::number(point.number);
				formula = point.naming == Naming::Cable ? point.cable_formula
									: WireNames::formula();
			}
				//One name on every folio of the point: %id would read the
				//folio of each wire
			if (point.folios.size() > 1) {
				formula = WireNames::pinned(formula, point.home->folioIndex() + 1);
			}
				//The section of the point, and nothing when no component of
				//it says one: never what the old text said. A cable's formula
				//does not show it, and its wires still carry it, as every
				//wire of a point does
			const QString section = WireSections::sectionOf(point.wires);
			for (Conductor *wire : std::as_const(point.wires))
			{
				if (nameWire(wire, formula, sequence, section, parent)) {
					++changed;
				}
			}
		}
		return changed;
	}
}

/**
	@brief WireNames::formula
	See the header.
*/
QString WireNames::formula()
{
	return QStringLiteral("F%id-%sequf_1 (%wq %ws)");
}

/**
	@brief WireNames::earthFormula
	See the header.
*/
QString WireNames::earthFormula()
{
	return QStringLiteral("F%id PE (%wq %ws)");
}

/**
	@brief WireNames::cableFormula
	See the header.
*/
QString WireNames::cableFormula(const QString &type)
{
		//The type is text, and a formula has no way of writing a percent
		//sign that is not a variable: "50%id" would become "505". A
		//fullwidth percent sign looks the same and starts nothing
	QString literal = type;
	literal.replace(QLatin1Char('%'), QChar(0xFF05));
	return cablePrefix() + literal + QLatin1Char(')');
}

/**
	@brief WireNames::cableType
	See the header.
*/
QString WireNames::cableType(const QString &text)
{
	const QString trimmed = text.trimmed();
	static const QRegularExpression named(QStringLiteral("^cabo +"),
					      QRegularExpression::CaseInsensitiveOption);
	const QRegularExpressionMatch match = named.match(trimmed);
	if (match.hasMatch())
	{
		const QString rest = trimmed.mid(match.capturedLength()).trimmed();
		if (!rest.isEmpty()) {
			return rest;
		}
	}
	const int open = trimmed.indexOf(QLatin1Char('('));
	const int close = open >= 0 ? trimmed.indexOf(QLatin1Char(')'), open + 1) : -1;
	if (close > open)
	{
		const QString inside = trimmed.mid(open + 1, close - open - 1).trimmed();
		if (!inside.isEmpty()) {
			return inside;
		}
	}
	return trimmed;
}

/**
	@brief WireNames::scheme
	See the header.
*/
NumerotationContext WireNames::scheme()
{
	NumerotationContext context;
	context.addValue(QStringLiteral("string"), QStringLiteral("F"));
	context.addValue(QStringLiteral("idfolio"), QString());
	context.addValue(QStringLiteral("string"), QStringLiteral("-"));
	context.addValue(unit_folio, 1, 1, 1);
	context.addValue(QStringLiteral("string"), QStringLiteral(" (%wq %ws)"));
	return context;
}

/**
	@brief WireNames::typedNumber
	See the header.
*/
int WireNames::typedNumber(const QString &text)
{
		//Written without a backslash on purpose: the digits as a class
	static const QRegularExpression typed(QStringLiteral("^ *F[0-9]+-([0-9]+)"));
	const QRegularExpressionMatch match = typed.match(text);
	if (!match.hasMatch()) {
		return -1;
	}
	bool ok = false;
	const int number = match.captured(1).toInt(&ok);
	return ok ? number : -1;
}

/**
	@brief WireNames::typedFolio
	See the header.
*/
int WireNames::typedFolio(const QString &text)
{
	static const QRegularExpression typed(QStringLiteral("^ *F([0-9]+)(-[0-9]| +PE)"));
	const QRegularExpressionMatch match = typed.match(text);
	if (!match.hasMatch()) {
		return -1;
	}
	bool ok = false;
	const int folio = match.captured(1).toInt(&ok);
	return ok ? folio : -1;
}

/**
	@brief WireNames::pinned
	See the header.
*/
QString WireNames::pinned(const QString &formula, int folio)
{
	const QString id = QStringLiteral("F%id");
	if (folio < 1 || !formula.startsWith(id) || !isNameFormula(formula)) {
		return formula;
	}
	return QLatin1Char('F') + QString::number(folio) + formula.mid(id.size());
}

/**
	@brief WireNames::unpinned
	See the header.
*/
QString WireNames::unpinned(const QString &formula)
{
		//The number of a folio where %id is, followed by the rest of one
		//of the formulas: a formula typed "F5-..." of anything else stays
	static const QRegularExpression pinned_folio(
				QStringLiteral("^F[0-9]+(?=-%sequf_1 [(]%wq | PE [(]%wq %ws[)]$)"));
	QString plain = formula;
	plain.replace(pinned_folio, QStringLiteral("F%id"));
	return plain;
}

/**
	@brief WireNames::isNameFormula
	See the header.
*/
bool WireNames::isNameFormula(const QString &formula)
{
	const QString plain = unpinned(formula);
	return plain == WireNames::formula()
			|| plain == earthFormula()
			|| plain.startsWith(cablePrefix());
}

/**
	@brief WireNames::isNumberingName
	See the header.
*/
bool WireNames::isNumberingName(const Conductor *wire)
{
	if (!wire || !wire->diagram()) {
		return false;
	}
	const ConductorProperties properties = wire->properties();
	if (!properties.m_formula.contains(QLatin1Char('%'))) {
		return false;
	}
	autonum::sequentialNumbers sequence = wire->sequenceNum();
	return properties.text == autonum::AssignVariables::formulaToLabel(
				properties.m_formula, sequence, wire->diagram(), nullptr, wire);
}

/**
	@brief WireNames::keepFolioOfPoint
	See the header.
*/
int WireNames::keepFolioOfPoint(Conductor *conductor, Diagram *diagram, QUndoCommand *parent_undo)
{
	if (!conductor) {
		return 0;
	}
	ConductorProperties own = conductor->properties();
	if (!own.m_formula.startsWith(QStringLiteral("F%id")) || !isNameFormula(own.m_formula)) {
		return 0;
	}
		//The wires left out of the numbering keep their formula (T77)
	if (ConductorAutoNumerotation::isIgnored(conductor)) {
		return 0;
	}
	const QList<Conductor *> point = ConductorAutoNumerotation::counted(
				WireSections::potentialOf(conductor));
	if (holdsFrozen(point)) {
		return 0;
	}

		//The folio of the name the point has: the first folio the others
		//are on. The new conductor does not move it - it takes the name
	Diagram *home = nullptr;
	bool across = false;
	for (Conductor *other : point)
	{
		if (other == conductor || !other->diagram()) {
			continue;
		}
		if (other->diagram() != conductor->diagram()) {
			across = true;
		}
		if (!home || other->diagram()->folioIndex() < home->folioIndex()) {
			home = other->diagram();
		}
	}
	if (!across || !home) {
		return 0;
	}

	const QString formula = pinned(own.m_formula, home->folioIndex() + 1);
	QList<WireSections::Change> changes;
	for (Conductor *other : point)
	{
		if (other == conductor) {
			continue;
		}
		const ConductorProperties before = other->properties();
		if (before.m_formula != own.m_formula) {
			continue;
		}
		WireSections::Change change;
		change.conductor = other;
		change.before = before;
		change.after = before;
		change.after.m_formula = formula;
		changes << change;
	}

		//The new conductor at once, as the numbering gives it its
		//properties; the others at once too, as the cross-section given
		//right after reads them, and through an undo command
	own.m_formula = formula;
	conductor->setProperties(own);
	WireSections::apply(changes, true);
	if (changes.isEmpty()) {
		return 0;
	}
	if (parent_undo) {
		return WireSections::addCommands(changes, parent_undo);
	}
	if (!diagram) {
		diagram = conductor->diagram();
	}
	if (!diagram) {
		return 0;
	}
	auto *undo = new QUndoCommand(QObject::tr("Name the wire as its point", "undo caption"));
	const int added = WireSections::addCommands(changes, undo);
	diagram->undoStack().push(undo);
	return added;
}

/**
	@brief WireNames::schemeOf
	See the header.
*/
QString WireNames::schemeOf(const QETProject *project)
{
	if (!project) {
		return QString();
	}
	const QString wanted = formula();
	const QStringList titles = AutoNumSchemeCommand::titles(
				project, AutoNumSchemeCommand::Kind::Conductor);
	for (const QString &title : titles)
	{
		if (autonum::numerotationContextToFormula(project->conductorAutoNum(title)) == wanted) {
			return title;
		}
	}
	return QString();
}

/**
	@brief WireNames::convertProject
	See the header.
*/
namespace
{
		/// What a renumbering asks of the conversion (T78)
	struct Renumbering
	{
		QSet<Diagram *> folios;
		bool columns_first = false;
	};

		/**
			Give the points of @p points named on a folio of @p renumbering
			their numbers again, from their place, before namePoints()
			names them.
		*/
	void renumberPoints(QList<Point> &points, const QList<Diagram *> &diagrams,
			    const Renumbering &renumbering)
	{
		QHash<const Diagram *, int> index;
		for (int i = 0 ; i < diagrams.size() ; ++i) {
			index.insert(diagrams.at(i), i);
		}

			//The numbers of a panel built stay, and the others skip them
		QHash<const Diagram *, QSet<int>> taken;
		QList<std::pair<RenumberInput, int>> places;
		for (int i = 0 ; i < points.size() ; ++i)
		{
			Point &point = points[i];
				//Named on a folio out of the renumbering: left alone, as
				//a point of a panel built is
			if (point.home && !renumbering.folios.contains(point.home)) {
				point.frozen = true;
				continue;
			}
			if (!point.home || point.naming == Naming::Earth) {
				continue;
			}
			if (point.frozen)
			{
				if (point.number >= 0) {
					taken[point.home].insert(point.number);
				}
				continue;
			}
				//The place of its name: the first of the names of its
				//wires on the folio it is named on
			bool found = false;
			RenumberInput first;
			for (Conductor *wire : std::as_const(point.wires))
			{
				if (wire->diagram() != point.home) {
					continue;
				}
				RenumberInput place;
				place.uuid = wire->uuid().toString();
				place.folio_index = index.value(point.home);
				place.position = wire->textItem()
						? wire->textItem()->sceneBoundingRect().center()
						: wire->sceneBoundingRect().center();
				if (!found || Renumberer::readingOrderLessThan(place, first,
										 renumbering.columns_first)) {
					first = place;
					found = true;
				}
			}
			if (found) {
				places.append({first, i});
			}
		}

		const bool columns_first = renumbering.columns_first;
		std::sort(places.begin(), places.end(),
			  [columns_first](const std::pair<RenumberInput, int> &a,
					  const std::pair<RenumberInput, int> &b) {
			return Renumberer::readingOrderLessThan(a.first, b.first, columns_first);
		});

		QHash<const Diagram *, int> next;
		for (const auto &place : std::as_const(places))
		{
			Point &point = points[place.second];
			int number = next.value(point.home, 0) + 1;
			while (taken.value(point.home).contains(number)) {
				++number;
			}
			next[point.home] = number;
			point.number = number;
		}
	}

		/**
			Add under @p parent what takes from the wires left out of the
			numbering on a folio of @p renumbering the name the numbering
			gave them, the text and the formula (T78, step 3). A name typed
			by hand stays (WireNames::isNumberingName()).
			@return how many conductors change
		*/
	int clearLeftOutNames(const Renumbering &renumbering, QUndoCommand *parent)
	{
		int changed = 0;
		for (Diagram *folio : renumbering.folios)
		{
			const QList<Conductor *> wires = folio->conductors();
			for (Conductor *wire : wires)
			{
				if (!ConductorAutoNumerotation::isIgnored(wire)
				    || !WireNames::isNumberingName(wire)) {
					continue;
				}
				const ConductorProperties before = wire->properties();
				ConductorProperties after = before;
				after.text.clear();
				after.m_formula.clear();
				QVariant old_value, new_value;
				old_value.setValue(before);
				new_value.setValue(after);
				new QPropertyUndoCommand(wire, "properties", old_value, new_value, parent);
				++changed;
			}
		}
		return changed;
	}
}

/**
	The conversion, and the renumbering when @p renumbering is given: see
	WireNames::convertProject() and WireNames::renumberProject().
*/
static int convertOrRenumber(QETProject *project, Diagram *shown,
			     const Renumbering *renumbering)
{
	if (!project || project->isReadOnly()) {
		return 0;
	}
	const auto kind = AutoNumSchemeCommand::Kind::Conductor;

		//diagrams() builds the folios still waiting (lazy folios): a point
		//crosses folio reports, and its far end has to be there
	const QList<Diagram *> diagrams = project->diagrams();

		//The scheme, made the one new conductors take: the project's own
		//when it has one writing the formula, a new one otherwise
	QString title = WireNames::schemeOf(project);
	NumerotationContext context;
	AutoNumSchemeCommand *scheme_command = nullptr;
	if (title.isEmpty())
	{
		title = newSchemeTitle(project);
		context = WireNames::scheme();
		scheme_command = AutoNumSchemeCommand::create(project, kind, title, context, true);
		if (!scheme_command) {
			return 0;
		}
	}
	else
	{
		context = project->conductorAutoNum(title);
		if (project->conductorCurrentAutoNum() != title) {
			scheme_command = AutoNumSchemeCommand::edit(project, kind, title, title,
								    context, true);
		}
	}

		//One gesture for the data base, every folio in it
	const QString caption = renumbering ? renumberCaption() : undoCaption();
	auto *conversion = new GroupedUpdateCommand(project, caption);
	QList<Point> points = pointsOf(diagrams);
	if (renumbering) {
		renumberPoints(points, diagrams, *renumbering);
	}
		//The highest number the typed names carry on each folio: the
		//numbers of the points named on it
	QHash<Diagram *, int> highest;
	for (const Point &point : std::as_const(points))
	{
		if (point.home && point.number >= 0) {
			highest[point.home] = qMax(highest.value(point.home), point.number);
		}
	}
	int changed = namePoints(points, highest, conversion);
	if (renumbering) {
		changed += clearLeftOutNames(*renumbering, conversion);
	}

		//The count of each folio goes on after its highest number, and
		//never back below a number the scheme has given there already -
		//unless the folio was numbered again, from 1: its count is then
		//its highest number
	int shown_highest = -1;
	for (Diagram *diagram : diagrams)
	{
			//A folio out of the renumbering keeps its count
		if (renumbering && !renumbering->folios.contains(diagram)) {
			continue;
		}
		const int folio_highest = renumbering
				? highest.value(diagram)
				: qMax(highest.value(diagram),
				       diagram->m_cnd_unitfolio_max.value(title).value(0).toInt());
		const QStringList count {QString::number(folio_highest)};
		if (FolioCountCommand::changes(diagram, title, count)) {
			new FolioCountCommand(diagram, title, count, conversion);
		}
		if (diagram == shown) {
			shown_highest = folio_highest;
		}
	}

		//The count new conductors take is the one of the folio shown last,
		//put there when a folio is shown: the folio on screen is not shown
		//again, so its count is put there now, in the same step - and a
		//redo puts it back
	if (shown_highest >= 0)
	{
		const NumerotationContext counted = countingFrom(context, shown_highest);
		if (!sameContext(context, counted))
		{
			new SetAutoNumContextCommand(
					[project](const QString &key, const NumerotationContext &c) {
						project->addConductorAutoNum(key, c);
					},
					title, context, counted, conversion);
		}
	}

	if (!scheme_command && !conversion->childCount())
	{
		delete conversion;
		return 0;
	}

		//One undo step: the scheme, which cannot be a child of another
		//command, and the rest
	QUndoStack *stack = project->undoStack();
	stack->beginMacro(caption);
	if (scheme_command) {
		stack->push(scheme_command);
	}
	if (conversion->childCount()) {
		stack->push(conversion);
	} else {
		delete conversion;
	}
	stack->endMacro();
	return changed;
}

int WireNames::convertProject(QETProject *project, Diagram *shown)
{
	return convertOrRenumber(project, shown, nullptr);
}

int WireNames::renumberProject(QETProject *project, const QList<Diagram *> &folios,
			       bool columns_first, Diagram *shown)
{
	Renumbering renumbering;
	for (Diagram *folio : folios) {
		if (folio) {
			renumbering.folios.insert(folio);
		}
	}
	renumbering.columns_first = columns_first;
	return convertOrRenumber(project, shown, &renumbering);
}
