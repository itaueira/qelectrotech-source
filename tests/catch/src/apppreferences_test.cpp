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

#include <QCheckBox>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include "qt_catch_tostring.h"

#include "../../../sources/ui/backupdialog.h"
#include "../../../sources/utils/apppreferences.h"

/*
	The preferences of the workstation, and the two defects they were written
	against.

	The first is silent and it was in the tree since the option existed: the
	preference page read border-columns_0 with an omitted value of false while
	the four places that consume it read it with an omitted value of true, and
	the page writes the key on every visit. Nobody has to change anything for
	the damage to happen - opening the preferences once on a fresh profile is
	enough, and what changes is the column numbering of every folio of every
	project, which is printed on drawings that get signed. A reader and a
	writer of the same key disagreeing about what its absence means is the
	whole of it, and that is what these cases pin.

	The second is not silent at all, it is just tiring: the backup copy
	question was asked on every single open and the answer was never kept.
	What it needs is three states and not two, because "do not ask again"
	does not say what should happen in place of the question - and a way back
	from the preference page, or the check box becomes a door that locks from
	the outside.

	Nothing here touches the settings of whoever runs the suite: every case
	works on a QSettings of its own, in a temporary directory that goes away
	with it. The no-argument overloads, which read the settings of the running
	application, are two-line delegations and are deliberately not exercised
	for that reason.
*/

namespace
{
	/**
		@brief settingsPath
		@param dir
		@return the path of a settings file of its own, inside @a dir
	*/
	QString settingsPath(const QTemporaryDir &dir)
	{
		return dir.path() + QDir::separator() + QStringLiteral("prefs.ini");
	}
}

TEST_CASE("T40 — an omitted border-columns key means to the page what it means to the four that read it",
	  "[t40][preferences]")
{
	/*
		The omitted value the consumers read with, written here as the
		literal they hold:

			sources/bordertitleblock.cpp    -- twice
			sources/diagramposition.cpp
			sources/autoNum/assignvariables.cpp

		The assertion runs one way: it fails when the module drifts away from
		the consumers, which is the direction the defect came from. It cannot
		see a consumer being changed - that one is caught by this constant
		being read against the sources by whoever changes them.
	*/
	const bool CONSUMER_OMITTED_VALUE = true;

	REQUIRE(AppPreferences::borderColumnsFromZeroDefault() == CONSUMER_OMITTED_VALUE);

	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QSettings settings(settingsPath(dir), QSettings::IniFormat);

	SECTION("a fresh profile holds no key, and both sides still agree")
	{
		REQUIRE_FALSE(settings.contains(AppPreferences::borderColumnsFromZeroKey()));

		const bool as_a_consumer_reads_it =
			settings.value(AppPreferences::borderColumnsFromZeroKey(),
				       CONSUMER_OMITTED_VALUE).toBool();

		REQUIRE(AppPreferences::borderColumnsFromZero(settings) == as_a_consumer_reads_it);
	}

	SECTION("visiting the preference page once changes nothing")
	{
		/*
			What the page does, in the order it does it: it reads the key to
			show the box at construction, and applyConf() writes the box back
			whether or not anybody touched it. On a fresh profile that pair
			used to turn "number from 0" into "number from 1" for every folio
			of every project, and the only visible trace was the drawing.
		*/
		const bool shown_in_the_box = AppPreferences::borderColumnsFromZero(settings);
		AppPreferences::setBorderColumnsFromZero(settings, shown_in_the_box);

		REQUIRE(settings.contains(AppPreferences::borderColumnsFromZeroKey()));
		REQUIRE(settings.value(AppPreferences::borderColumnsFromZeroKey(),
				       CONSUMER_OMITTED_VALUE).toBool() == CONSUMER_OMITTED_VALUE);
		REQUIRE(AppPreferences::borderColumnsFromZero(settings) == CONSUMER_OMITTED_VALUE);
	}

	SECTION("and what the user does choose survives, both ways round")
	{
		AppPreferences::setBorderColumnsFromZero(settings, false);
		REQUIRE_FALSE(AppPreferences::borderColumnsFromZero(settings));
		REQUIRE_FALSE(settings.value(AppPreferences::borderColumnsFromZeroKey(),
					     CONSUMER_OMITTED_VALUE).toBool());

		AppPreferences::setBorderColumnsFromZero(settings, true);
		REQUIRE(AppPreferences::borderColumnsFromZero(settings));
		REQUIRE(settings.value(AppPreferences::borderColumnsFromZeroKey(),
				       CONSUMER_OMITTED_VALUE).toBool());
	}
}

TEST_CASE("T40 — a fresh profile asks about the backup copy, and the answer survives the file",
	  "[t40][preferences][backup]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QSettings settings(settingsPath(dir), QSettings::IniFormat);

	REQUIRE(AppPreferences::backupPolicy(settings) == AppPreferences::BackupPolicy::Ask);

	SECTION("each of the three states goes to the file and comes back")
	{
		const auto policy = GENERATE(AppPreferences::BackupPolicy::Ask,
					     AppPreferences::BackupPolicy::Always,
					     AppPreferences::BackupPolicy::Never);

		AppPreferences::setBackupPolicy(settings, policy);
		settings.sync();

		QSettings read_again(settingsPath(dir), QSettings::IniFormat);
		REQUIRE(AppPreferences::backupPolicy(read_again) == policy);
	}

	SECTION("what is written is a word, so that the file can be read by a person")
	{
		AppPreferences::setBackupPolicy(settings, AppPreferences::BackupPolicy::Never);
		REQUIRE(settings.value(AppPreferences::backupPolicyKey()).toString()
			== QStringLiteral("never"));
	}

	SECTION("reading is tolerant, and what it cannot read it errs towards asking")
	{
			//A file written by a newer version, or edited by hand into
			//nonsense. Falling back on asking is the state that decides
			//nothing in the user's name.
		settings.setValue(AppPreferences::backupPolicyKey(), QStringLiteral("sometimes"));
		REQUIRE(AppPreferences::backupPolicy(settings) == AppPreferences::BackupPolicy::Ask);

		settings.setValue(AppPreferences::backupPolicyKey(), 2);
		REQUIRE(AppPreferences::backupPolicy(settings) == AppPreferences::BackupPolicy::Ask);

		settings.setValue(AppPreferences::backupPolicyKey(), QString());
		REQUIRE(AppPreferences::backupPolicy(settings) == AppPreferences::BackupPolicy::Ask);
	}
}

TEST_CASE("T40 — the tick and the answer are two different things", "[t40][preferences][backup]")
{
	using Policy = AppPreferences::BackupPolicy;

	SECTION("untouched box: the answer is for this project only")
	{
			//This is what keeps the box safe to leave alone. Answering yes
			//once must not make every later project copy itself in silence.
		REQUIRE(AppPreferences::policyForAnswer(true, false, Policy::Ask) == Policy::Ask);
		REQUIRE(AppPreferences::policyForAnswer(false, false, Policy::Ask) == Policy::Ask);
	}

	SECTION("ticked box: the answer being given is what gets kept")
	{
		REQUIRE(AppPreferences::policyForAnswer(true, true, Policy::Ask) == Policy::Always);
		REQUIRE(AppPreferences::policyForAnswer(false, true, Policy::Ask) == Policy::Never);
	}

	SECTION("a policy already in force is not moved by an untouched box")
	{
		REQUIRE(AppPreferences::policyForAnswer(true, false, Policy::Never) == Policy::Never);
		REQUIRE(AppPreferences::policyForAnswer(false, false, Policy::Always) == Policy::Always);
	}
}

TEST_CASE("T40 — the check box is not a door that locks from the outside",
	  "[t40][preferences][backup]")
{
	/*
		The whole point of the second half of the request: whoever ticks the
		box has to be able to undo it without editing a settings file by hand.
		What the preference page writes is this same key, so putting Ask back
		is what makes the question come back - and it is asserted here rather
		than trusted, because a page writing a key of its own would look
		exactly as correct from the page.
	*/
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QSettings settings(settingsPath(dir), QSettings::IniFormat);

		//The user ticks the box and answers no: from now on, no question and
		//no copy.
	AppPreferences::setBackupPolicy(
		settings,
		AppPreferences::policyForAnswer(false, true, AppPreferences::backupPolicy(settings)));
	REQUIRE(AppPreferences::backupPolicy(settings) == AppPreferences::BackupPolicy::Never);

		//And then he goes to the preference page and puts it back.
	AppPreferences::setBackupPolicy(settings, AppPreferences::BackupPolicy::Ask);
	REQUIRE(AppPreferences::backupPolicy(settings) == AppPreferences::BackupPolicy::Ask);
}

TEST_CASE("T40 — the dialogue reports the tick and decides nothing", "[t40][preferences][backup]")
{
	BackupDialog dialog;
		//The layout is never activated on a dialogue nobody shows, and the
		//click below needs the box to have the geometry it will have on
		//screen rather than the default one it is born with.
	dialog.adjustSize();

	auto *box = dialog.findChild<QCheckBox *>();
	REQUIRE(box != nullptr);

	SECTION("it is born unticked, which is what keeps a fresh profile asking")
	{
		REQUIRE_FALSE(box->isChecked());
		REQUIRE_FALSE(dialog.rememberChoice());
	}

	SECTION("a click on it is what the caller sees")
	{
			//click() and not setChecked(): what has to be proved is that the
			//box the user reaches is the one the accessor reads, so the run
			//has to go through QAbstractButton - nextCheckState(), toggled(),
			//clicked() - and not straight to the property.
			//
			//It is click() and not QTest::mouseClick(box, ..., rect().center())
			//because a synthetic click carries a POSITION, and a dialogue that
			//was never shown gives its children no usable one: measured here,
			//the first version of this case failed on exactly that, with the
			//box left unticked and the code under test never reached. The
			//assertion below records the geometry so the next reader does not
			//have to rediscover it - and so the day somebody shows the dialogue
			//and the rect stops being empty, the reason this line exists is
			//still written down.
		CHECK(box->rect().isEmpty() == (box->width() == 0));
		box->click();
		REQUIRE(box->isChecked());
		REQUIRE(dialog.rememberChoice());
	}
}

TEST_CASE("T40 — the factory colour scheme is light, and not the one of the desktop",
	  "[t40][preferences][appearance]")
{
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	QSettings settings(settingsPath(dir), QSettings::IniFormat);

	/*
		Light and not System, because the icon collection and the title block
		are drawn for a light background: a workstation whose desktop is dark
		would otherwise open on a panel nobody can read. It is the one default
		of this module that is a decision rather than a compatibility.
	*/
	REQUIRE(AppPreferences::colorScheme(settings) == AppPreferences::ColorScheme::Light);

	SECTION("the key is the one main.cpp reads before the first window exists")
	{
			//Written as a literal on purpose. main.cpp reads this key before
			//anything of QElectroTech is constructed, and a settings file
			//written by an earlier build holds this same name: renaming it
			//would leave the preference stored and never honoured, which
			//looks from the screen like the selector not working.
		REQUIRE(AppPreferences::colorSchemeKey()
			== QStringLiteral("appearance/color-scheme"));
	}

	SECTION("each of the three goes to the file and comes back")
	{
		const auto scheme = GENERATE(AppPreferences::ColorScheme::Light,
					     AppPreferences::ColorScheme::Dark,
					     AppPreferences::ColorScheme::System);

		AppPreferences::setColorScheme(settings, scheme);
		settings.sync();

		QSettings read_again(settingsPath(dir), QSettings::IniFormat);
		REQUIRE(AppPreferences::colorScheme(read_again) == scheme);
	}

	SECTION("the words are the ones the setting file already holds")
	{
		AppPreferences::setColorScheme(settings, AppPreferences::ColorScheme::Dark);
		REQUIRE(settings.value(AppPreferences::colorSchemeKey()).toString()
			== QStringLiteral("dark"));

		AppPreferences::setColorScheme(settings, AppPreferences::ColorScheme::System);
		REQUIRE(settings.value(AppPreferences::colorSchemeKey()).toString()
			== QStringLiteral("system"));
	}

	SECTION("reading is tolerant, and what it cannot read it errs towards light")
	{
		settings.setValue(AppPreferences::colorSchemeKey(), QStringLiteral("solarized"));
		REQUIRE(AppPreferences::colorScheme(settings) == AppPreferences::ColorScheme::Light);

		settings.setValue(AppPreferences::colorSchemeKey(), QStringLiteral("Dark"));
		REQUIRE(AppPreferences::colorScheme(settings) == AppPreferences::ColorScheme::Light);
	}
}
