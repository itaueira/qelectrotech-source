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
#ifndef DRCFINDING_H
#define DRCFINDING_H

#include "drcrule.h"

#include <QList>
#include <QString>

class Conductor;
class Diagram;
class Element;

/**
	@brief What kind of thing a finding points at.

	There is a None on purpose, and it is the whole reason this is an
	enumeration rather than a pointer that may be null.

	A rule that speaks of the project as a whole - a missing template, a
	setting nobody filled in - has nothing on a folio to point at. The
	tempting shortcut is to hand back folio 1, and it is a lie that costs
	more than the missing navigation: whoever double clicks lands on a
	sheet that has nothing to do with what he read, and concludes the
	checker is wrong about the finding too. None says "there is nowhere to
	go", the panel can say so, and nobody is sent anywhere.
*/
enum class DrcTargetKind
{
	/// The project as a whole. Nothing to navigate to, and that is stated.
	None,
	Element,
	Conductor,
	Diagram
};

/**
	@brief One thing a rule found wrong, and where to go and look at it.

	The finding is what a rule produces and what the panel lists, and it
	holds three separate things that are easy to run together:

	- **what is wrong**, in text() - one sentence, already translated,
	  naming the component and the measured value. It does **not** carry
	  the folio and the position, because the panel has a column for each
	  of those and a sentence that repeats them makes the table unreadable
	  sideways. describe() is what assembles the whole line, for a log or
	  for the command line, where there are no columns;
	- **which rule said so**, in ruleIdentifier() - the machine key, never
	  translated, so that switching a rule off and finding its findings
	  gone is the same key in both places;
	- **where it is**, in the target, the folio and the position.

	@par The severity is copied, not looked up
	It is the severity the rule carried at the moment of the run, exactly
	as DrcSqlResult copies it, and for the same reason: a report read after
	somebody lowered the rule stays true to the run that produced it.

	@par The target is a bare pointer, and it is a short-lived one
	The panel that shows a finding is opened on a run and closed after it;
	it does not outlive the project, and nothing here is written to a file.
	That is the same contract the missing-part report of the catalogue
	works under, and it is stated rather than assumed so that nobody stores
	a list of findings somewhere that does.

	@par The folio is 1-based, and 0 means nobody said
	1-based because that is the number printed on the sheet and the number
	QETProject::folioIndex() + 1 produces; 0 because a finding with no
	folio - the project-wide one above - must not read as the first sheet.
	This class does not go and fetch it: computing it needs the project,
	and keeping that out is what lets the finding be built and proved
	without one. Whoever creates the finding fills it in.
*/
class DrcFinding
{
	public:
		DrcFinding() {}

		static DrcFinding onElement(const QString &rule_identifier,
					    DrcSeverity severity,
					    const QString &text,
					    Element *element);
		static DrcFinding onConductor(const QString &rule_identifier,
					      DrcSeverity severity,
					      const QString &text,
					      Conductor *conductor);
		static DrcFinding onDiagram(const QString &rule_identifier,
					    DrcSeverity severity,
					    const QString &text,
					    Diagram *diagram);
		/**
			A finding about the project, with nothing on a folio to point at.

			The explicit case the decision behind DrcTargetKind::None asks
			for: it is built on purpose and reads as such, instead of being
			an onElement() that happened to be handed a null pointer.
		*/
		static DrcFinding onProject(const QString &rule_identifier,
					    DrcSeverity severity,
					    const QString &text);

		/// A finding with no rule or no sentence says nothing, so it is not one.
		bool isValid() const;

		/// The machine key of the rule that found it. Never translated.
		QString ruleIdentifier() const {return m_rule_identifier;}
		/// The severity the rule carried when the run happened.
		DrcSeverity severity() const {return m_severity;}
		/// What is wrong, in one translated sentence, without the place.
		QString text() const {return m_text;}

		DrcTargetKind targetKind() const {return m_target_kind;}
		/**
			Whether there is somewhere to go.

			False for a project-wide finding, and false as well for a
			finding whose target kind promises an object that is not there
			- so that a caller who tests this never has to test the pointer
			again.
		*/
		bool hasTarget() const;

		Element *element() const {return m_element;}
		Conductor *conductor() const {return m_conductor;}
		/**
			The sheet to open.

			For a finding on a diagram it is that diagram; for one on an
			element or a conductor it is the sheet the object was drawn on,
			when whoever built the finding filled it in. It is the one thing
			navigation needs, so it is answered once here instead of at
			every call site.
		*/
		Diagram *diagram() const {return m_diagram;}
		void setDiagram(Diagram *diagram) {m_diagram = diagram;}

		/// The number printed on the sheet, 1-based. 0 when there is none.
		int folio() const {return m_folio;}
		void setFolio(int folio);
		bool hasFolio() const {return m_folio > 0;}

		/// Where on the sheet, in the grid of the border, e.g. "B2". May be empty.
		QString position() const {return m_position;}
		void setPosition(const QString &position) {m_position = position;}

		/// Where it is, in words: empty when nothing is known.
		QString location() const;
		/// The whole finding in one line: the sentence and the place.
		QString describe() const;

		/// How many of @a findings are at least that serious.
		static int countAtLeast(const QList<DrcFinding> &findings,
					DrcSeverity threshold);
		/**
			Whether any of @a findings is at least that serious.

			Goes through DrcRule::isAtLeast like everything else that
			compares severities, because that is the single place the order
			of them is written - the exit code of the command line asks this
			same question of the other route.
		*/
		static bool hasAtLeast(const QList<DrcFinding> &findings,
				       DrcSeverity threshold);

	private:
		QString m_rule_identifier;
		QString m_text;
		DrcSeverity m_severity = DrcSeverity::Warning;
		DrcTargetKind m_target_kind = DrcTargetKind::None;
		Element *m_element = nullptr;
		Conductor *m_conductor = nullptr;
		Diagram *m_diagram = nullptr;
		int m_folio = 0;
		QString m_position;
};

#endif // DRCFINDING_H
