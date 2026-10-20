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
#include "conductorautonumerotation.h"
#include "diagram.h"
#include "qetgraphicsitem/conductor.h"
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

		const bool new_sequence = sequence_before != sequence_after;
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

		/// One point of a folio, as the conversion reads it
	struct Point
	{
			/// its conductors on the folio, in the order the folio lists them
		QList<Conductor *> wires;
			/// the whole potential, the folio's conductors and the others
		QList<Conductor *> whole;
			/// the number its text carries, -1 for none
		int number = -1;
		bool frozen = false;
		Naming naming = Naming::Numbered;
			/// the formula of a cable, with its type in it
		QString cable_formula;
	};

		/**
			Say what @p point is named as, from what its conductors on the
			folio carry, in this order:
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
			if (kept.isEmpty()
			    && (properties.m_formula == WireNames::earthFormula()
				|| properties.m_formula.startsWith(cablePrefix()))) {
				kept = properties.m_formula;
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
			Add under @p parent what names every wire of @p diagram (see
			WireNames::convertProject()).
			@param highest : receives the highest number the folio carries
			afterwards
			@return how many conductors change
		*/
	int convertFolio(Diagram *diagram, const QString &title, QUndoCommand *parent, int *highest)
	{
		const QList<Conductor *> listed = diagram->conductors();
		QHash<Conductor *, int> rank;
		for (int i = 0 ; i < listed.size() ; ++i) {
			rank.insert(listed.at(i), i);
		}

			//The points, in the order their first conductor comes in the
			//list of the folio, and the highest number the folio carries -
			//a frozen point's too, which the next free number must not take
		QList<Point> points;
		QSet<Conductor *> seen;
		int folio_highest = 0;
		for (Conductor *conductor : listed)
		{
			if (seen.contains(conductor)) {
				continue;
			}
			Point point;
			point.whole = WireSections::potentialOf(conductor);
			for (Conductor *member : std::as_const(point.whole))
			{
				seen.insert(member);
				if (rank.contains(member)) {
					point.wires << member;
				}
			}
			std::sort(point.wires.begin(), point.wires.end(),
				  [&rank](Conductor *a, Conductor *b) {return rank.value(a) < rank.value(b);});
			for (Conductor *wire : std::as_const(point.wires))
			{
				const int number = WireNames::typedNumber(wire->properties().text);
				if (number < 0) {
					continue;
				}
					//The first conductor of the folio carrying a number
					//gives it to the point
				if (point.number < 0) {
					point.number = number;
				}
				folio_highest = qMax(folio_highest, number);
			}
			point.frozen = holdsFrozen(point.whole);
			settleNaming(point);
			points << point;
		}

		int changed = 0;
		for (Point &point : points)
		{
			if (point.frozen) {
				continue;
			}
				//The earth has no number, and takes none from the folio
			QString formula = WireNames::earthFormula();
			autonum::sequentialNumbers sequence;
			if (point.naming != Naming::Earth)
			{
				if (point.number < 0) {
					point.number = ++folio_highest;
				}
				sequence.unit_folio << QString::number(point.number);
				formula = point.naming == Naming::Cable ? point.cable_formula
									: WireNames::formula();
			}
				//The section of the point, and nothing when no component of
				//it says one: never what the old text said. A cable's formula
				//does not show it, and its wires still carry it, as every
				//wire of a point does
			const QString section = WireSections::sectionOf(point.whole);
			for (Conductor *wire : std::as_const(point.wires))
			{
				if (nameWire(wire, formula, sequence, section, parent)) {
					++changed;
				}
			}
		}

			//The count of the folio goes on after its highest number, and
			//never back below a number the scheme has given there already
		folio_highest = qMax(folio_highest,
				     diagram->m_cnd_unitfolio_max.value(title).value(0).toInt());
		const QStringList count {QString::number(folio_highest)};
		if (FolioCountCommand::changes(diagram, title, count)) {
			new FolioCountCommand(diagram, title, count, parent);
		}
		if (highest) {
			*highest = folio_highest;
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
int WireNames::convertProject(QETProject *project, Diagram *shown)
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
	QString title = schemeOf(project);
	NumerotationContext context;
	AutoNumSchemeCommand *scheme_command = nullptr;
	if (title.isEmpty())
	{
		title = newSchemeTitle(project);
		context = scheme();
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
	auto *conversion = new GroupedUpdateCommand(project, undoCaption());
	int changed = 0;
	int shown_highest = -1;
	for (Diagram *diagram : diagrams)
	{
		int highest = 0;
		changed += convertFolio(diagram, title, conversion, &highest);
		if (diagram == shown) {
			shown_highest = highest;
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
	stack->beginMacro(undoCaption());
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
