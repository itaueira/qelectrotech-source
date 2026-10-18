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
#include "drcduplicatetags.h"

#include "../autoNum/iecstructure.h"
#include "../diagramcontext.h"

#include <QCoreApplication>
#include <QHash>
#include <QStringList>

#include <utility>

/*
	No element.h and no diagram.h here: this file is handed tags, folio
	numbers and the information of a component and of its title block,
	never the objects they came from. That is what lets it be proved in the
	pure suite, which does not link the program - iecstructure.cpp and
	diagramcontext.cpp are already there for the same reason.
*/

/**
	@brief DrcDuplicateTags::comparedTag
	@param settings : the IEC 81346 settings of the project
	@param label : the tag the component carries
	@param element_info : the information of the component
	@param folio_info : the information of the title block of its sheet
	@return the tag the component is compared by
*/
QString DrcDuplicateTags::comparedTag(const IecStructureSettings &settings,
				      const QString &label,
				      const DiagramContext &element_info,
				      const DiagramContext &folio_info)
{
		//Only the display form changes, and only here: switched off, the
		//structure gives the label back untouched whatever this says, and
		//switched on, the full form is the identity the short and the
		//context forms abbreviate. The project keeps its own setting.
	IecStructureSettings full = settings;
	full.display = IecTagDisplay::Full;
	return full.composedTag(label, element_info, folio_info);
}

/**
	@brief DrcDuplicateTags::readingOf
	@param settings : the IEC 81346 settings of the project
	@return the reading a run over that project uses
*/
DrcDuplicateTags::Reading DrcDuplicateTags::readingOf(
		const IecStructureSettings &settings)
{
	if (!settings.enabled) {
		return Reading::AsTyped;
	}
	return settings.location_from_element ? Reading::ComposedFromTreeOrField
					      : Reading::ComposedFromTree;
}

/**
	@brief DrcDuplicateTags::key
	@param tag : a tag as compared
	@return what two tags must share to be the same tag
*/
QString DrcDuplicateTags::key(const QString &tag)
{
	return tag.trimmed().toCaseFolded();
}

/**
	@brief DrcDuplicateTags::groups
	@param marks : one per component compared
	@return the indexes of the components of each tag carried more than once
*/
QList<QList<int>> DrcDuplicateTags::groups(const QList<Mark> &marks)
{
		//Every tag, in the order its first component is met, so that the
		//findings come out in the order of the walk and not in the order
		//a hash hands its keys out.
	QList<QList<int>> every_tag;
	QHash<QString, int> index_of;

	for (int index = 0 ; index < marks.count() ; ++index)
	{
		const QString tag_key = key(marks.at(index).tag);
		if (tag_key.isEmpty()) {
			continue;
		}

		const auto known = index_of.constFind(tag_key);
		if (known == index_of.constEnd())
		{
			index_of.insert(tag_key, int(every_tag.count()));
			every_tag.append(QList<int>({index}));
		}
		else
		{
			every_tag[known.value()].append(index);
		}
	}

	QList<QList<int>> repeated;
	for (const QList<int> &members : std::as_const(every_tag))
	{
		if (members.count() > 1) {
			repeated.append(members);
		}
	}
	return repeated;
}

/**
	@brief DrcDuplicateTags::readingText
	@param reading : how the tags were read
	@return the words a sentence says about it
*/
QString DrcDuplicateTags::readingText(Reading reading)
{
	switch (reading)
	{
		case Reading::ComposedFromTree:
			return QCoreApplication::translate(
				"DrcDuplicateTags",
				"Compared as composed by the IEC 81346 structure, the + "
				"part taken from the location tree only: the location "
				"field is not read.");
		case Reading::ComposedFromTreeOrField:
			return QCoreApplication::translate(
				"DrcDuplicateTags",
				"Compared as composed by the IEC 81346 structure, the + "
				"part taken from the location tree, or from the location "
				"field where there is no tree path.");
		case Reading::AsTyped:
			break;
	}
	return QCoreApplication::translate(
		"DrcDuplicateTags",
		"Compared as typed: the IEC 81346 structure is off.");
}

namespace
{
	/// Where one component is, in words: "folio 3, B2".
	QString placeOf(const DrcDuplicateTags::Mark &mark)
	{
		if (mark.folio > 0 && !mark.position.isEmpty())
		{
			return QCoreApplication::translate("DrcDuplicateTags",
							   "folio %1, %2")
			       .arg(QString::number(mark.folio), mark.position);
		}
		if (mark.folio > 0)
		{
			return QCoreApplication::translate("DrcDuplicateTags",
							   "folio %1")
			       .arg(QString::number(mark.folio));
		}
		if (!mark.position.isEmpty()) {
			return mark.position;
		}
		return QCoreApplication::translate("DrcDuplicateTags",
						   "place unknown");
	}
}

/**
	@brief DrcDuplicateTags::duplicateText
	@param group : the marks of one tag
	@param reading : how the tags were read
	@return the sentence of the finding, or an empty string when there is
	nothing to report
*/
QString DrcDuplicateTags::duplicateText(const QList<Mark> &group,
					Reading reading)
{
	if (group.count() < 2) {
		return QString();
	}

	const QString first_key = key(group.first().tag);
	if (first_key.isEmpty()) {
		return QString();
	}

	const QString first_tag = group.first().tag.trimmed();
	bool spelled_alike = true;
	for (const Mark &mark : group)
	{
		if (key(mark.tag) != first_key) {
			return QString();
		}
		if (mark.tag.trimmed() != first_tag) {
			spelled_alike = false;
		}
	}

		//When the members are not spelled alike - K3 and k3 - each place
		//says which spelling is there, or the reader sent to folio 7 to
		//look for K3 finds k3 and wonders whether he is in the right place.
	QStringList places;
	for (const Mark &mark : group)
	{
		const QString place = placeOf(mark);
		if (spelled_alike)
		{
			places << place;
		}
		else
		{
				//Both values in one call: a tag is text somebody typed,
				//and one holding a "%2" would otherwise be rewritten by
				//the second of two chained arg().
			places << QCoreApplication::translate("DrcDuplicateTags",
							      "%1 on %2")
				  .arg(mark.tag.trimmed(), place);
		}
	}

		//"; " and not ", ": a place is "folio 3, B2", so the comma is
		//already taken inside it. Not a translated joiner, like the space
		//between the two sentences: neither carries a word.
	return QCoreApplication::translate("DrcDuplicateTags",
					   "Tag %1 on %n components: %2.",
					   nullptr,
					   int(group.count()))
	       .arg(first_tag, places.join(QStringLiteral("; ")))
	       + QLatin1Char(' ')
	       + readingText(reading);
}
