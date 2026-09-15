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
#ifndef APPPREFERENCES_H
#define APPPREFERENCES_H

#include <QString>

class QSettings;

/**
	Preferences of the workstation, as opposed to properties of the project.

	The question that decides whether a setting belongs here is one: if two
	people open the same project, does this have to be the same for both? If
	yes it is a property of the project and its place is the .qet file; if no
	it is a preference and its place is QSettings. Nothing declared here is
	ever written into a project file.

	Why a module instead of a settings.value() at each call site: the default
	value of an omitted key is part of the meaning of the key, and when it is
	spelled out at every call site the call sites drift apart. They did: the
	preference page read border-columns_0 with an omitted value of false while
	the four places that consume it read it with an omitted value of true, and
	because the page writes the key on every visit, merely opening the
	preferences of a fresh profile renumbered the columns of every folio. The
	default lives in one function now, and a test holds it against the value
	the consumers read with.

	Every preference comes in two shapes: one taking a QSettings, which is
	what lets the rule be exercised against a temporary file instead of
	against the settings of whoever runs the suite, and one without, which
	reads and writes the settings of the running application.
*/
namespace AppPreferences
{
		/**
			What to do about the backup copy when a project is opened.

			Three states, and not two: "do not ask again" alone does not say
			what should happen in place of the question, and a check box that
			can only mean yes would be a door that locks from the outside.
		*/
	enum class BackupPolicy
	{
		Ask,	///< show the question, which is what a fresh profile does
		Always,	///< copy without asking
		Never	///< do not copy and do not ask
	};

		/**
			Which palette the interface is drawn with.

			Light is the factory default, and deliberately not System: the
			icon collection and the title block are drawn for a light
			background, so following a desktop in dark mode makes the element
			panel and the toolbars nearly unreadable.
		*/
	enum class ColorScheme
	{
		Light,
		Dark,
		System	///< whatever the desktop says
	};

	QString backupPolicyKey();
	QString colorSchemeKey();
	QString borderColumnsFromZeroKey();

	QString backupPolicyToString(BackupPolicy policy);
	BackupPolicy backupPolicyFromString(const QString &text);
	BackupPolicy backupPolicy(const QSettings &settings);
	BackupPolicy backupPolicy();
	void setBackupPolicy(QSettings &settings, BackupPolicy policy);
	void setBackupPolicy(BackupPolicy policy);
	BackupPolicy policyForAnswer(bool accepted,
				     bool remember,
				     BackupPolicy current);

	QString colorSchemeToString(ColorScheme scheme);
	ColorScheme colorSchemeFromString(const QString &text);
	ColorScheme colorScheme(const QSettings &settings);
	ColorScheme colorScheme();
	void setColorScheme(QSettings &settings, ColorScheme scheme);
	void setColorScheme(ColorScheme scheme);

	bool borderColumnsFromZeroDefault();
	bool borderColumnsFromZero(const QSettings &settings);
	bool borderColumnsFromZero();
	void setBorderColumnsFromZero(QSettings &settings, bool from_zero);
	void setBorderColumnsFromZero(bool from_zero);
}

#endif // APPPREFERENCES_H
