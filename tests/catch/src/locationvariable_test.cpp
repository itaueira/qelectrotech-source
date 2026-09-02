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
#include "../../../sources/autoNum/iecstructure.h"
#include "../../../sources/diagramcontext.h"
#include "../../../sources/location/locationtree.h"
#include "../../../sources/qetinformation.h"
#include "qt_catch_tostring.h"

#include <QString>
#include <QVariant>

/*
	The composite text variable %{location_path}, added by commit d14ef67a1.

	What is under test and what is not, measured before this file was written:

	AssignVariables::replaceVariable() is

	    static QString replaceVariable(const QString &formula,
	                                   const DiagramContext &dc);

	so the function itself asks for no live scene: a bag of key/value pairs is
	its whole input, and DiagramContext is on this bench already. Its
	translation unit is another matter. sources/autoNum/assignvariables.cpp
	includes diagram.h, qetapp.h, element.h, conductor.h and qetproject.h, and
	its other functions use them for real - genericXref() dereferences
	element->diagram()->border_and_titleblock, assignProjectVar() walks
	m_diagram->project()->projectProperties(), and two more reach for
	QETApp::customElementsDir(). Adding that file to the test target would
	therefore drag Diagram, Element, QETProject and QETApp in behind it, which
	is exactly what the eligibility rule of tests/catch/CMakeLists.txt refuses:
	"no Element, no Diagram, so the ordering and the inheritance can be tested
	without a project open".

	So the substitution below is a transcription of the two lines
	replaceVariable() runs for these two keys, in the order it runs them, and
	not a call into it. Everything it computes still comes from the shipped
	code - QETInformation::displayedInfoValue(), LocationTree::iecTag() and
	IecStructure::fromTag() are all compiled into this bench - so the display
	rule, the empty case and the doubled sign are checked against production.
	What a transcription cannot catch is somebody deleting the
	str.replace("%{location_path}", ...) line altogether: that would put the
	literal %{location_path} back on the folio and leave this file green. The
	day assignvariables.cpp becomes linkable here, replace the helper with the
	real call and this file starts covering that too.
*/

namespace
{
	/**
		The two lines AssignVariables::replaceVariable() runs for the two
		location keys, transcribed in the order the shipped function runs
		them. See the note at the top of this file for why this bench cannot
		call the real one.

		The order matters and is kept: %{location} is substituted first. It is
		safe only because "%{location}" is not a prefix of "%{location_path}"
		- the closing brace of the shorter key falls where the underscore of
		the longer one is - so neither key can eat the other's token.
	*/
	QString replaceLocationVariables(const QString &formula,
					 const DiagramContext &dc)
	{
		QString str = formula;
		str.replace(QStringLiteral("%{location}"),
			    dc.value(QETInformation::ELMT_LOCATION).toString());
		str.replace(QStringLiteral("%{location_path}"),
			    QETInformation::displayedInfoValue(
				    QETInformation::ELMT_LOCATION_PATH,
				    dc.value(QETInformation::ELMT_LOCATION_PATH)));
		return str;
	}

	/**
		@param path where the component is mounted, written the way the
		project file holds it: the path down the location tree, with the
		tree's own separator
		@param location the value of the other, older key
		@return the component information a placed element carries
	*/
	DiagramContext mounted(const char *path, const char *location = "")
	{
		DiagramContext dc;
		dc.addValue(QETInformation::ELMT_LOCATION_PATH,
			    QString::fromUtf8(path));
		dc.addValue(QETInformation::ELMT_LOCATION,
			    QString::fromUtf8(location));
		return dc;
	}
}

TEST_CASE("%{location_path} paints the designation of the norm, not the stored path",
	  "[locationvariable]")
{
	SECTION("two levels of the tree are one place with two codes")
	{
		// The file holds "QCM1/PORTE" because that is what a location is
		// renamed and moved by; the norm writes that same place "+QCM1+PORTE",
		// and the folio is what goes to the printer.
		const DiagramContext dc = mounted("QCM1/PORTE");
		CHECK(replaceLocationVariables(QStringLiteral("%{location_path}"), dc)
		      == QStringLiteral("+QCM1+PORTE"));
	}

	SECTION("a component nobody placed paints nothing, not an orphan sign")
	{
		// This is the case that protects the defect this project has already
		// shipped once: an empty footer field printing the code of the
		// variable instead of printing nothing. A lone "+" would be the same
		// defect wearing a smaller face - it claims a place that does not
		// exist. Three ways of having no place, one answer.
		const DiagramContext absent;
		CHECK(replaceLocationVariables(QStringLiteral("%{location_path}"),
					       absent)
		      == QString());

		const DiagramContext empty = mounted("");
		CHECK(replaceLocationVariables(QStringLiteral("%{location_path}"),
					       empty)
		      == QString());

		// A path made of separators and nothing else carries no code, and
		// LocationTree::iecTag() answers it the same way.
		const DiagramContext codeless = mounted("//");
		CHECK(replaceLocationVariables(QStringLiteral("%{location_path}"),
					       codeless)
		      == QString());

		// And the field around the variable survives: a formula that is a
		// label and an unplaced path keeps the label and loses nothing else.
		CHECK(replaceLocationVariables(QStringLiteral("K3 %{location_path}"),
					       absent)
		      == QStringLiteral("K3 "));
	}

	SECTION("one level is one code behind one sign")
	{
		const DiagramContext dc = mounted("QCM1");
		CHECK(replaceLocationVariables(QStringLiteral("%{location_path}"), dc)
		      == QStringLiteral("+QCM1"));
	}
}

TEST_CASE("%{location} and %{location_path} are two keys and stay two answers",
	  "[locationvariable]")
{
	// The older key is filled by hand and holds whatever was typed into it.
	// It is not the tree, it does not go through the display rule, and it
	// keeps no leading sign of its own.
	const DiagramContext dc = mounted("QCM1/PORTE", "ARMARIO 3");

	SECTION("the older key hands back its stored value, raw")
	{
		CHECK(replaceLocationVariables(QStringLiteral("%{location}"), dc)
		      == QStringLiteral("ARMARIO 3"));
	}

	SECTION("a raw value that looks like a path is still handed back raw")
	{
		// The proof that the two keys did not get merged behind one rule: the
		// same text under the other key is not converted, not prefixed, not
		// split on the separator.
		const DiagramContext confusable = mounted("", "QCM1/PORTE");
		CHECK(replaceLocationVariables(QStringLiteral("%{location}"),
					       confusable)
		      == QStringLiteral("QCM1/PORTE"));
	}

	SECTION("both keys in one formula each resolve to their own value")
	{
		CHECK(replaceLocationVariables(
			      QStringLiteral("%{location} / %{location_path}"), dc)
		      == QStringLiteral("ARMARIO 3 / +QCM1+PORTE"));
	}
}

TEST_CASE("the text beside the symbol and the table on the folio give one answer",
	  "[locationvariable]")
{
	SECTION("the substitution answers what the display rule answers")
	{
		// Two readers of the same component: the composite text variable, and
		// QETInformation::displayedInfoValue(), which is what the nomenclature
		// table drawn on the folio and the exported bill of material call. One
		// answer, or the drawing and the table disagree about where a
		// component is mounted.
		const DiagramContext two_levels = mounted("QCM1/PORTE");
		CHECK(replaceLocationVariables(QStringLiteral("%{location_path}"),
					       two_levels)
		      == QETInformation::displayedInfoValue(
				 QETInformation::ELMT_LOCATION_PATH,
				 QStringLiteral("QCM1/PORTE")));

		const DiagramContext one_level = mounted("QCM1");
		CHECK(replaceLocationVariables(QStringLiteral("%{location_path}"),
					       one_level)
		      == QETInformation::displayedInfoValue(
				 QETInformation::ELMT_LOCATION_PATH,
				 QStringLiteral("QCM1")));

		const DiagramContext unplaced = mounted("");
		CHECK(replaceLocationVariables(QStringLiteral("%{location_path}"),
					       unplaced)
		      == QETInformation::displayedInfoValue(
				 QETInformation::ELMT_LOCATION_PATH,
				 QString()));
	}

	SECTION("the display rule answers what the tree itself writes")
	{
		// The other half of the same coherence, and the half that is two
		// separate shipped functions rather than one: the centralised display
		// rule against LocationTree::iecTag(), which is where the conversion
		// lives. If one day somebody edits one of the two, this falls.
		CHECK(QETInformation::displayedInfoValue(
			      QETInformation::ELMT_LOCATION_PATH,
			      QStringLiteral("QCM1/PORTE"))
		      == LocationTree::iecTag(QStringLiteral("QCM1/PORTE")));
		CHECK(QETInformation::displayedInfoValue(
			      QETInformation::ELMT_LOCATION_PATH,
			      QStringLiteral("QCM1"))
		      == LocationTree::iecTag(QStringLiteral("QCM1")));
		CHECK(QETInformation::displayedInfoValue(
			      QETInformation::ELMT_LOCATION_PATH, QString())
		      == LocationTree::iecTag(QString()));
	}

	SECTION("the other key does not go through the display rule")
	{
		// The same coherence read from the other side: displayedInfoValue()
		// applies the conversion to ELMT_LOCATION_PATH and to nothing else, so
		// the older key comes back as it stands.
		CHECK(QETInformation::displayedInfoValue(
			      QETInformation::ELMT_LOCATION,
			      QStringLiteral("QCM1/PORTE"))
		      == QStringLiteral("QCM1/PORTE"));
	}
}

TEST_CASE("a + typed in front of the variable doubles the sign, deliberately",
	  "[locationvariable]")
{
	/*
		INTENTIONAL, AND NOT A DEFECT TO FIX HERE.

		displayedInfoValue() hands back the designation with the `+` of the
		norm already on it, so a composite text typed "+%{location_path}"
		paints "++QCM1+PORTE". Nothing on this path removes the extra sign,
		and commit d14ef67a1 decided that on purpose.

		The alternative - swallowing a `+` because the text happened to end
		with one - would sometimes eat a separator that was meant, and hand
		back a wrong designation wearing the face of a right one. A doubled
		sign is visible on the folio and the designer deletes the character on
		the spot; a silently eaten one is never noticed.

		This assertion exists so that the next person to read "++QCM1+PORTE"
		as a bug finds out here that it was a decision, and comes back to read
		this comment before "fixing" it.
	*/
	const DiagramContext dc = mounted("QCM1/PORTE");

	SECTION("the sign the designer typed is kept, and so is the sign of the norm")
	{
		CHECK(replaceLocationVariables(QStringLiteral("+%{location_path}"), dc)
		      == QStringLiteral("++QCM1+PORTE"));
	}

	SECTION("the doubling is display only: the structure still reads back")
	{
		// Why the doubled sign is allowed to reach the folio at all:
		// IecStructure::fromTag() reads "++QCM1+PORTE" back as the place
		// "QCM1+PORTE", the same place it reads out of "+QCM1+PORTE". The
		// defect is in what the eye sees and it does not corrupt the
		// structure anything downstream parses.
		const IecStructure doubled =
			IecStructure::fromTag(QStringLiteral("++QCM1+PORTE"));
		const IecStructure single =
			IecStructure::fromTag(QStringLiteral("+QCM1+PORTE"));
		CHECK(doubled.location == QStringLiteral("QCM1+PORTE"));
		CHECK(doubled.location == single.location);
		CHECK(doubled.product.isEmpty());
	}
}
