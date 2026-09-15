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

#include "../../../sources/properties/projectrevision.h"

#include <QDir>
#include <QDomDocument>
#include <QSet>
#include <QSettings>
#include <QTemporaryDir>

/*
	The revision a project is in, as data and nothing else.

	Nothing runs here, and that is the shape of the step: there is no
	command yet, no dialogue, and no project holding one of these. What can
	already be got wrong is every one of the things that only show
	themselves months later, on a drawing somebody has already signed:

	- an open revision coming back from the project file carrying a date,
	  which the title block then prints as if the drawing had been emitted;
	- a date that did not parse becoming today, so that two copies of the
	  same revision disagree about when it was emitted - the very defect
	  this whole feature exists to kill;
	- a closed revision reading back as open, which lets somebody edit what
	  has already been sent to a customer;
	- a project file written before any of this existed failing to open,
	  because the read of a revision that is not there was treated as an
	  error;
	- an accent or a quotation mark in the description or in the name of
	  whoever approved it coming back mangled, which is not noticed until
	  the emission is printed;
	- the letters running out at Z.

	The round trip goes through the text of the document on purpose, and
	not through the element in memory: an escaping defect in an ampersand
	or in a quotation mark does not exist until the document is written out
	and parsed again.

	Labelled T25 and not CU-25.n: this file closes no use case of that
	specification by itself. Every one of them needs a project open, a
	dialogue, or a printed sheet.
*/

namespace
{
	/*
		Accents, an em dash, guillemets, an ampersand, a lesser-than and a
		pair of quotation marks. Every one of them is a character XML has
		an opinion about, and the two fields below are the only free text
		a revision carries.
	*/
	const QString APPROVED_BY =
			QString::fromUtf8("Jean-\xC3\x89mile B\xC3\xA9ranger \"chef de projet\"");
	const QString DESCRIPTION =
			QString::fromUtf8("\xC3\x89mis pour approbation \xE2\x80\x94 "
					  "3 < 5 modifications & relecture "
					  "\xC2\xAB compl\xC3\xA8te \xC2\xBB");

	/// Write the revision, serialise the document, parse it again, read it back.
	ProjectRevision throughTheProjectFile(const ProjectRevision &revision)
	{
		QDomDocument document;
		QDomElement root = document.createElement(QStringLiteral("project"));
		document.appendChild(root);
		root.appendChild(revision.toXml(document));

		QDomDocument read_document;
		REQUIRE(read_document.setContent(document.toString()));

		ProjectRevision read;
		REQUIRE(read.fromXml(read_document.documentElement()
				     .firstChildElement(ProjectRevision::xmlTagName())));
		return read;
	}

	/// Field by field, so that a forgetful operator== cannot hide a lost field.
	void requireSameFields(const ProjectRevision &read, const ProjectRevision &written)
	{
		REQUIRE(read.identifier() == written.identifier());
		REQUIRE(read.isClosed() == written.isClosed());
		REQUIRE(read.approvedBy() == written.approvedBy());
		REQUIRE(read.description() == written.description());
		REQUIRE(read.date() == written.date());
	}

	QDomElement elementOf(QDomDocument &document, const QString &xml)
	{
		REQUIRE(document.setContent(xml));
		return document.documentElement();
	}
}

TEST_CASE("T25 — a project is born in revision A, open, and with no date", "[t25][revision]")
{
	ProjectRevision revision;

	REQUIRE(revision.isValid());
	REQUIRE(revision.identifier() == QStringLiteral("A"));
	REQUIRE(revision.isOpen());
	REQUIRE_FALSE(revision.isClosed());
	REQUIRE(revision.approvedBy().isEmpty());
	REQUIRE(revision.description().isEmpty());

		//The one that matters: the date is what closing stamps, so an
		//open revision has none, and it is null - not today
	REQUIRE(revision.date().isNull());
	REQUIRE_FALSE(revision.date().isValid());
	REQUIRE(revision.date() != QDate::currentDate());

		//An open revision may be filled in
	REQUIRE(revision.setApprovedBy(APPROVED_BY));
	REQUIRE(revision.setDescription(DESCRIPTION));
	REQUIRE(revision.setIdentifier(QStringLiteral("B")));
	REQUIRE(revision.identifier() == QStringLiteral("B"));

		//A revision with no name could not be found again in the file
	REQUIRE_FALSE(revision.setIdentifier(QStringLiteral("   ")));
	REQUIRE(revision.identifier() == QStringLiteral("B"));
}

TEST_CASE("T25 — closing is refused without who approved it and without what it is for",
	  "[t25][revision]")
{
	const QDate emission(2026, 9, 14);

	SECTION("nothing filled in")
	{
		ProjectRevision revision;
		REQUIRE_FALSE(revision.canBeClosed());
		REQUIRE_FALSE(revision.close(emission));
		REQUIRE(revision.isOpen());
		REQUIRE(revision.date().isNull());
	}

	SECTION("who approved it, but not what for")
	{
		ProjectRevision revision;
		revision.setApprovedBy(APPROVED_BY);
		REQUIRE_FALSE(revision.canBeClosed());
		REQUIRE_FALSE(revision.close(emission));
		REQUIRE(revision.isOpen());
		REQUIRE(revision.date().isNull());
	}

	SECTION("what for, but nobody approved it")
	{
		ProjectRevision revision;
		revision.setDescription(DESCRIPTION);
		REQUIRE_FALSE(revision.canBeClosed());
		REQUIRE_FALSE(revision.close(emission));
		REQUIRE(revision.isOpen());
	}

	SECTION("a field holding nothing but spaces is not a filled field")
	{
		ProjectRevision revision;
		revision.setApprovedBy(QStringLiteral("   "));
		revision.setDescription(QStringLiteral("\t "));
		REQUIRE_FALSE(revision.canBeClosed());
		REQUIRE_FALSE(revision.close(emission));
		REQUIRE(revision.isOpen());
	}

	SECTION("both filled in, and it closes")
	{
		ProjectRevision revision;
		revision.setApprovedBy(APPROVED_BY);
		revision.setDescription(DESCRIPTION);
		REQUIRE(revision.canBeClosed());
		REQUIRE(revision.close(emission));
		REQUIRE(revision.isClosed());
		REQUIRE(revision.date() == emission);
	}

	SECTION("a date nobody could read is refused with the rest")
	{
		ProjectRevision revision;
		revision.setApprovedBy(APPROVED_BY);
		revision.setDescription(DESCRIPTION);
		REQUIRE(revision.canBeClosed());

			//A closed revision without a readable date is the one field
			//nobody can go back and fill in
		REQUIRE_FALSE(revision.close(QDate()));
		REQUIRE(revision.isOpen());
		REQUIRE(revision.date().isNull());
	}
}

TEST_CASE("T25 — what is closed changes no more, not even its own fields", "[t25][revision]")
{
	const QDate emission(2026, 9, 14);

	ProjectRevision revision(QStringLiteral("B"));
	revision.setApprovedBy(APPROVED_BY);
	revision.setDescription(DESCRIPTION);
	REQUIRE(revision.close(emission));

		//Every setter answers that it did nothing, and the field is the
		//one that was signed
	REQUIRE_FALSE(revision.setApprovedBy(QStringLiteral("quelqu un d autre")));
	REQUIRE_FALSE(revision.setDescription(QStringLiteral("Pour construction")));
	REQUIRE_FALSE(revision.setIdentifier(QStringLiteral("C")));

	REQUIRE(revision.identifier() == QStringLiteral("B"));
	REQUIRE(revision.approvedBy() == APPROVED_BY);
	REQUIRE(revision.description() == DESCRIPTION);
	REQUIRE(revision.date() == emission);

		//And it cannot be closed a second time, with a second date
	REQUIRE_FALSE(revision.canBeClosed());
	REQUIRE_FALSE(revision.close(QDate(2026, 12, 25)));
	REQUIRE(revision.date() == emission);
}

TEST_CASE("T25 — an open revision comes back from the project file still without a date",
	  "[t25][revision]")
{
	ProjectRevision written(QStringLiteral("C"));
	written.setApprovedBy(APPROVED_BY);
	written.setDescription(DESCRIPTION);

	const ProjectRevision read = throughTheProjectFile(written);

	requireSameFields(read, written);
	REQUIRE(read == written);
	REQUIRE(read.isOpen());

		//Not today, and not an unreadable date either: no date at all
	REQUIRE(read.date().isNull());
	REQUIRE_FALSE(read.date().isValid());
	REQUIRE(read.date() != QDate::currentDate());

		//And the writer left the attribute out rather than writing an
		//empty one, which is what makes the line above true
	QDomDocument document;
	REQUIRE_FALSE(written.toXml(document).hasAttribute(QStringLiteral("date")));
}

TEST_CASE("T25 — a closed revision survives the project file field by field", "[t25][revision]")
{
	const QDate emission(2026, 9, 14);

	ProjectRevision written(QStringLiteral("B"));
	written.setApprovedBy(APPROVED_BY);
	written.setDescription(DESCRIPTION);
	REQUIRE(written.close(emission));

	const ProjectRevision read = throughTheProjectFile(written);

	requireSameFields(read, written);
	REQUIRE(read == written);
	REQUIRE(read.isClosed());
	REQUIRE(read.date() == emission);

		//Named one by one, because a comparison that passes on an empty
		//pair of strings would pass here too
	REQUIRE_FALSE(read.approvedBy().isEmpty());
	REQUIRE_FALSE(read.description().isEmpty());
	REQUIRE(read.approvedBy().contains(QString::fromUtf8("B\xC3\xA9ranger")));
	REQUIRE(read.approvedBy().contains(QLatin1Char('"')));
	REQUIRE(read.description().contains(QLatin1Char('&')));
	REQUIRE(read.description().contains(QLatin1Char('<')));
	REQUIRE(read.description().contains(QString::fromUtf8("compl\xC3\xA8te")));
}

TEST_CASE("T25 — a project file with no revision in it still opens", "[t25][revision]")
{
	QDomDocument document;
	const QDomElement project = elementOf(
			document,
			QStringLiteral("<project version=\"0.9\"><diagram/></project>"));

	SECTION("the element is simply not there")
	{
		ProjectRevision revision;
		const QDomElement absent =
				project.firstChildElement(ProjectRevision::xmlTagName());
		REQUIRE(absent.isNull());

			//False says "there was nothing to read", not "this project
			//is broken" - and the object is the revision A it was born as
		REQUIRE_FALSE(revision.fromXml(absent));
		REQUIRE(revision.isValid());
		REQUIRE(revision.identifier() == QStringLiteral("A"));
		REQUIRE(revision.isOpen());
		REQUIRE(revision.date().isNull());
		REQUIRE(revision == ProjectRevision());
	}

	SECTION("somebody else element does not wipe the revision already held")
	{
		ProjectRevision revision(QStringLiteral("D"));
		revision.setApprovedBy(APPROVED_BY);
		revision.setDescription(DESCRIPTION);
		const ProjectRevision before = revision;

		REQUIRE_FALSE(revision.fromXml(project));
		REQUIRE_FALSE(revision.fromXml(project.firstChildElement(QStringLiteral("diagram"))));

		requireSameFields(revision, before);
	}
}

TEST_CASE("T25 — reading is tolerant, and what it cannot read it errs towards closed",
	  "[t25][revision]")
{
	SECTION("an element with nothing but its tag is the revision A, open")
	{
		QDomDocument document;
		ProjectRevision revision(QStringLiteral("F"));
		REQUIRE(revision.fromXml(elementOf(document,
						   QStringLiteral("<project_revision/>"))));

		REQUIRE(revision.identifier() == QStringLiteral("A"));
		REQUIRE(revision.isOpen());
		REQUIRE(revision.date().isNull());
		REQUIRE(revision.approvedBy().isEmpty());
	}

	SECTION("a date that does not parse is no date at all, never today")
	{
		QDomDocument document;
		ProjectRevision revision;
		REQUIRE(revision.fromXml(elementOf(
				document,
				QStringLiteral("<project_revision id=\"B\" state=\"closed\" "
					       "date=\"le 14 septembre\"/>"))));

		REQUIRE(revision.identifier() == QStringLiteral("B"));
		REQUIRE(revision.isClosed());
		REQUIRE(revision.date().isNull());
		REQUIRE(revision.date() != QDate::currentDate());
	}

	SECTION("a state nobody can read, with a date, is a closed revision")
	{
			//The date is stamped by closing and by nothing else, so a
			//revision carrying one has been closed. Erring the other way
			//would hand somebody an emitted drawing to edit.
		QDomDocument document;
		ProjectRevision revision;
		REQUIRE(revision.fromXml(elementOf(
				document,
				QStringLiteral("<project_revision id=\"B\" date=\"20260914\"/>"))));

		REQUIRE(revision.isClosed());
		REQUIRE(revision.date() == QDate(2026, 9, 14));
	}

	SECTION("a state nobody can read, with no date, is an open revision")
	{
		QDomDocument document;
		ProjectRevision revision;
		REQUIRE(revision.fromXml(elementOf(
				document,
				QStringLiteral("<project_revision id=\"B\" state=\"ouverte\"/>"))));

		REQUIRE(revision.isOpen());
		REQUIRE(revision.date().isNull());
	}

	SECTION("the fields of the element read into an object that already held some")
	{
		QDomDocument document;
		ProjectRevision revision;
		revision.setApprovedBy(APPROVED_BY);
		revision.setDescription(DESCRIPTION);

		REQUIRE(revision.fromXml(elementOf(
				document,
				QStringLiteral("<project_revision id=\"B\" state=\"open\"/>"))));

			//Nothing of the old revision survives into the new one
		REQUIRE(revision.approvedBy().isEmpty());
		REQUIRE(revision.description().isEmpty());
	}
}

TEST_CASE("T25 — the letters run A, B, Z, AA, and never twice the same", "[t25][revision]")
{
	REQUIRE(ProjectRevision::firstIdentifier() == QStringLiteral("A"));

	REQUIRE(ProjectRevision::nextIdentifier(QStringLiteral("A")) == QStringLiteral("B"));
	REQUIRE(ProjectRevision::nextIdentifier(QStringLiteral("B")) == QStringLiteral("C"));

		//The wrap nobody sees until the one project that gets there
	REQUIRE(ProjectRevision::nextIdentifier(QStringLiteral("Z")) == QStringLiteral("AA"));
	REQUIRE(ProjectRevision::nextIdentifier(QStringLiteral("AA")) == QStringLiteral("AB"));
	REQUIRE(ProjectRevision::nextIdentifier(QStringLiteral("AZ")) == QStringLiteral("BA"));
	REQUIRE(ProjectRevision::nextIdentifier(QStringLiteral("ZZ")) == QStringLiteral("AAA"));

		//What is not a run of letters gets the only answer that cannot
		//make two revisions share a name
	REQUIRE(ProjectRevision::nextIdentifier(QString()) == QStringLiteral("A"));
	REQUIRE(ProjectRevision::nextIdentifier(QStringLiteral("  ")) == QStringLiteral("A"));
	REQUIRE(ProjectRevision::nextIdentifier(QStringLiteral("12")) == QStringLiteral("A"));
	REQUIRE(ProjectRevision::nextIdentifier(QStringLiteral("B2")) == QStringLiteral("A"));

		//Walked from A to well past Z, checking that no letter comes back
		//twice - which a hand-written carry gets wrong silently
	QSet<QString> seen;
	QString identifier = ProjectRevision::firstIdentifier();
	for (int step = 0 ; step < 60 ; ++step)
	{
		REQUIRE_FALSE(seen.contains(identifier));
		seen.insert(identifier);
		identifier = ProjectRevision::nextIdentifier(identifier);
	}
	REQUIRE(seen.size() == 60);
	REQUIRE(seen.contains(QStringLiteral("AA")));
	REQUIRE(seen.contains(QStringLiteral("BH")));
}

TEST_CASE("T25 — two revisions are the same revision only when all five fields are",
	  "[t25][revision]")
{
	const QDate emission(2026, 9, 14);

	ProjectRevision reference(QStringLiteral("B"));
	reference.setApprovedBy(APPROVED_BY);
	reference.setDescription(DESCRIPTION);

	ProjectRevision other = reference;
	REQUIRE(other == reference);
	REQUIRE_FALSE(other != reference);

	SECTION("the identifier")
	{
		other.setIdentifier(QStringLiteral("C"));
		REQUIRE(other != reference);
	}

	SECTION("who approved it")
	{
		other.setApprovedBy(APPROVED_BY + QStringLiteral("."));
		REQUIRE(other != reference);
	}

	SECTION("what it is for")
	{
		other.setDescription(QStringLiteral("Pour construction"));
		REQUIRE(other != reference);
	}

	SECTION("the state, and the date that comes with it")
	{
		REQUIRE(other.close(emission));
		REQUIRE(other != reference);
	}

	SECTION("the date alone")
	{
		ProjectRevision closed_early = reference;
		ProjectRevision closed_late = reference;
		REQUIRE(closed_early.close(emission));
		REQUIRE(closed_late.close(emission.addDays(1)));

		REQUIRE(closed_early.isClosed());
		REQUIRE(closed_late.isClosed());
		REQUIRE(closed_early != closed_late);
	}
}

TEST_CASE("T25 — the settings carry the same five fields, and read back as tolerantly",
	  "[t25][revision]")
{
	QTemporaryDir directory;
	REQUIRE(directory.isValid());
	const QString path = QDir(directory.path()).filePath(QStringLiteral("revision.ini"));
	const QString prefix = QStringLiteral("project/");

	ProjectRevision written(QStringLiteral("B"));
	written.setApprovedBy(APPROVED_BY);
	written.setDescription(DESCRIPTION);
	REQUIRE(written.close(QDate(2026, 9, 14)));

	{
		QSettings settings(path, QSettings::IniFormat);
		written.toSettings(settings, prefix);
		settings.sync();
	}

	{
		QSettings settings(path, QSettings::IniFormat);
		ProjectRevision read;
		read.fromSettings(settings, prefix);
		requireSameFields(read, written);
		REQUIRE(read == written);
	}

	SECTION("a file that holds no revision leaves the object as it was born")
	{
		QSettings settings(path, QSettings::IniFormat);
		ProjectRevision read;
		read.fromSettings(settings, QStringLiteral("somewhere_else/"));

		REQUIRE(read.isValid());
		REQUIRE(read.identifier() == QStringLiteral("A"));
		REQUIRE(read.isOpen());
		REQUIRE(read.date().isNull());
	}
}
