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
#ifndef ASSEMBLYSTATE_H
#define ASSEMBLYSTATE_H

#include <QDomElement>
#include <QMap>
#include <QString>

/**
	@brief How far one project has gone from the drawing board to the panel
	in service.

	Three stages and not a boolean, because the two things that change are
	not the same thing. Wired on the bench, the panel exists and its labels
	are printed and stuck on; in service, the panel is in somebody's plant
	and a change to it is a site visit. The automation is restricted the same
	way in both, so nothing here tells them apart yet - but a project that
	has recorded which of the two it is can be asked later, and one that
	recorded a boolean cannot be asked at all.
*/
enum class AssemblyStage
{
	InProject,  ///< still being drawn: the automation may touch everything
	Assembled,  ///< wired on the bench: the labels exist as printed matter
	InField     ///< installed and running
};

/**
	@brief The assembly state of one project, and the photograph taken when it
	was marked.

	@par Why a photograph, and not a flag on each component

	Freezing four hundred components one at a time is work nobody does, and
	an automation nobody uses after the panel is built is an automation that
	was not written. So marking is one click, and what that click produces is
	this: the set of uuids that existed at that moment, each with the label it
	was carrying.

	The set is read, never written back. Nothing is stored in any component,
	which is what lets the marking be taken back without having to tell apart,
	afterwards, what the machine froze from what the drawer froze by hand
	(decision P85). The `auto_num_locked` key a person ticks stays entirely
	outside this class and survives an unmarking, because no code here ever
	touches it.

	@par The labels, and what they are for

	Storing the label next to the uuid costs one string per item and answers a
	question the uuid alone cannot: *what changed since*. A component whose
	tag today differs from the tag in the photograph was re-labelled after the
	panel was wired, and that is the line the shop floor order of service is
	made of. Without the labels the photograph would only be able to say which
	items are new.

	@par The order the entries are written in

	QMap and not QHash, and that is the whole reason: QHash iterates in an
	order that is not stable between runs, so the same project saved twice
	would produce two different files. A .qet whose bytes move on every save
	breaks the backup comparison, fills the diff of anybody who versions their
	projects, and makes "did this save change anything?" unanswerable. The
	lookup cost of a QMap over a thousand entries is not measurable next to
	that.

	@par Not a PropertiesInterface, and why

	That interface asks for toSettings()/fromSettings() as well, which is a
	copy of the value in the application settings. A per project fact has no
	business there: the same workstation draws for two customers, and an
	assembly state read out of QSettings would belong to whichever project was
	opened last. All four containers QETProject already writes into the .qet -
	IecStructureSettings, CircuitTable, IoList and LocationTree - carry a plain
	toXml()/fromXml() pair for that same reason, and this follows them rather
	than inventing a fifth shape.

	@par Who marked it, and when

	The two fields exist and are never filled by anything in this fork today.
	Authorship and date of a change of state is what revision control defines
	(T25), and two sources of truth about it would drift apart without anyone
	seeing which one had gone wrong (decision P86). They are read and written
	whole, so that the day something does fill them the file format does not
	have to change.
*/
class AssemblyState
{
	public:
		AssemblyState();

		/// Which stage the project is at. In project, and nothing is frozen.
		AssemblyStage stage = AssemblyStage::InProject;

		/// uuid of a component -> the label it carried when marked
		QMap<QString, QString> components;
		/// uuid of a conductor -> the text it carried when marked
		QMap<QString, QString> conductors;

		/// Left empty on purpose until T25 says where the revision keeps them.
		QString marked_by;
		QString marked_at;

		/**
			@return true when the automation has to leave what is in the
			photograph alone.

			The stage decides it, and not whether the photograph holds
			anything: an empty project marked as assembled is assembled, and
			the next component drawn on it is a component drawn after the
			panel was wired.
		*/
		bool isFrozen() const;

		/**
			@return true when this state says nothing at all, and therefore
			nothing needs to be written to the file.

			This is what keeps a project that never heard of the assembly
			state byte for byte the file it always was - the same rule the
			IEC settings and the table of circuits already follow.
		*/
		bool isEmpty() const;

		/// How many items the photograph holds, components and conductors.
		int frozenCount() const;

		bool holdsComponent(const QString &uuid) const;
		bool holdsConductor(const QString &uuid) const;

		/// The label the component carried when marked, empty when it is not
		/// in the photograph at all.
		QString componentLabel(const QString &uuid) const;
		/// The text the conductor carried when marked, same rule.
		QString conductorLabel(const QString &uuid) const;

		bool operator==(const AssemblyState &other) const;
		bool operator!=(const AssemblyState &other) const;

		QDomElement toXml(QDomDocument &document) const;
		/**
			@param element the node QETProject found under the project root
			@return true when @a element was a state this class understands.

			Reading is tolerant and writing is strict: a project saved before
			this existed carries no such node, fromXml() is handed a null
			element, and the silence reads as *in project* - which is exactly
			what a delivered project needs in order to keep opening.
		*/
		bool fromXml(const QDomElement &element);

		/// the tag name toXml() writes and fromXml() reads
		static QString tagName();

		static QString stageToString(AssemblyStage stage);
		static AssemblyStage stageFromString(const QString &string);
		/// The name of the stage as the interface says it.
		static QString translatedStage(AssemblyStage stage);
};

#endif // ASSEMBLYSTATE_H
