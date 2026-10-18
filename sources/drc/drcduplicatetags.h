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
#ifndef DRCDUPLICATETAGS_H
#define DRCDUPLICATETAGS_H

#include <QList>
#include <QString>

class DiagramContext;
class IecStructureSettings;

/**
	@brief When two components carry the same tag, and the sentence the
	finding says it with.

	The half of the duplicate tag rule that needs no project: it is handed
	the tag of each component, the folio and the cell of the grid it sits
	in, and it answers from that alone. Which components are compared, and
	reading their tag off the folio, is the other half, and it lives in
	DrcTagRules, because only the scene holds the components.

	@par The tag compared is the composed one, and in its full form
	With the IEC 81346 structure off - the default, and the state of every
	project drawn before it existed - the tag compared is the tag as the
	component carries it, exactly what the folio draws.

	With the structure on, it is the full designation, =CT1+A1-K3, whatever
	form the project chose to draw. The short form -K3 leaves out what the
	folio title block already says, so two contactors in two cabinets both
	draw -K3 and are still two components; comparing what is drawn would
	report them, and the rule would accuse every project that turned the
	structure on with its default display. The full designation is the
	identity the norm gives a component, and the short and the context
	forms are ways of writing it shorter. comparedTag() is
	IecStructureSettings::composedTag() with the display forced to full,
	and nothing else: no second copy of the composition, so the guards that
	keep free text and an empty tag out of it hold here as they hold on the
	folio.

	@par Which reading was used is part of every sentence
	The same drawing is reported in one project and not in another,
	depending on a project setting. A finding that did not say which
	reading produced it would look like the checker changing its mind on
	its own. The sentence therefore says whether the tags were compared as
	typed or composed and, composed, where the + part came from: by
	default it comes from the location tree only, and a cabinet typed into
	the free location field does not tell two components apart.

	@par What counts as the same tag
	Trimmed and with the case folded: " K3", "K3" and "k3" are one tag.
	Spaces at the ends print as nothing, and a workshop that finds K3 and
	k3 on two parts of one panel reads one tag twice - that is the
	confusion this rule exists to catch, so the difference in case is a
	slip and not a second name. A space in the middle is kept, "K 3" is not
	"K3", and so are leading zeros, "K03" is not "K3": both are what
	somebody wrote and neither is spacing. It is the same rule
	Renumberer::connectorKey() holds for the name of a connector.

	@par No tag is not a tag
	A component whose tag is empty, or only spacing, is left out: two
	components with no tag are not two components sharing one. Whether a
	component with no tag is a finding of its own, and which components
	that would cover, is a separate rule, and it is not written here.

	@par One finding per tag, not per pair
	Three components on one tag are one finding naming the three places,
	and not three findings. A pair count would grow as n(n-1)/2 with
	nothing more to fix, and one finding per component would list the same
	defect as many times as it has members.
*/
namespace DrcDuplicateTags
{
	/**
		@brief One component, as far as this rule sees it.
	*/
	struct Mark
	{
		/// The tag compared: as typed, or composed - see comparedTag().
		QString tag;
		/// The number printed on its sheet, 1-based; 0 when unknown.
		int folio = 0;
		/// The cell of the grid of the border, e.g. "B2". May be empty.
		QString position;
	};

	/**
		@brief How the tags of one run were read.

		Decided by the project settings once, for every component of the
		run, and named in every sentence.
	*/
	enum class Reading
	{
		/// The IEC 81346 structure is off: the tag as the component carries it.
		AsTyped,
		/// On, with the + part taken from the location tree only.
		ComposedFromTree,
		/// On, with the location field read where there is no tree path.
		ComposedFromTreeOrField
	};

	/**
		The tag one component is compared by.

		@param settings the IEC 81346 settings of the project
		@param label the tag the component carries, the formula already
		resolved: Element::actualLabel()
		@param element_info the information of the component
		@param folio_info the information of the title block of its sheet
		@return @a label untouched when the structure is off; the full
		designation when it is on, whatever the display form of the project
	*/
	QString comparedTag(const IecStructureSettings &settings,
			    const QString &label,
			    const DiagramContext &element_info,
			    const DiagramContext &folio_info);

	/// The reading @a settings make the run use.
	Reading readingOf(const IecStructureSettings &settings);

	/**
		What makes two tags the same: trimmed, case folded.
		@return an empty string for a tag that is empty or only spacing,
		which takes part in no comparison.
	*/
	QString key(const QString &tag);

	/**
		The tags carried by more than one of @a marks.

		@return one list per such tag, holding the indexes in @a marks of
		every component that carries it, in the order of @a marks. The
		lists come in the order their first member appears, so the first
		member of each is the one the walk met first. Marks whose key() is
		empty are in none of them.
	*/
	QList<QList<int>> groups(const QList<Mark> &marks);

	/// The words a sentence says about @a reading. Never empty.
	QString readingText(Reading reading);

	/**
		The sentence of a finding about the components of @a group.

		@param group the marks of one tag, as groups() gathers them
		@param reading how the tags were read
		@return an empty string when @a group holds fewer than two marks, a
		mark with no tag, or marks whose key() differ: there is nothing to
		report then. Otherwise the tag, how many carry it, the folio and the
		cell of each of them, and the reading.

		Unlike most findings, the sentence names places: the finding points
		at one member, and the panel has one folio column, so the others
		could not be found from the panel without it.
	*/
	QString duplicateText(const QList<Mark> &group, Reading reading);
}

#endif // DRCDUPLICATETAGS_H
