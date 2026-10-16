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

#include "apppreferences.h"

#include <QSettings>
#include <QVariant>

namespace AppPreferences
{
	/**
		@brief colorSchemeKey
		@return the settings key holding the colour scheme.
		Read by main.cpp before the first window exists, which is why it has
		to be readable without anything of QElectroTech being constructed.
	*/
	QString colorSchemeKey()
	{
		return QStringLiteral("appearance/color-scheme");
	}

	/**
		@brief borderColumnsFromZeroKey
		@return the settings key saying whether the title block columns are
		numbered from 0 or from 1
	*/
	QString borderColumnsFromZeroKey()
	{
		return QStringLiteral("border-columns_0");
	}

	/**
		@brief colorSchemeToString
		@param scheme
		@return the text written in the settings file for @a scheme
	*/
	QString colorSchemeToString(ColorScheme scheme)
	{
		switch (scheme)
		{
			case ColorScheme::Dark:
				return QStringLiteral("dark");
			case ColorScheme::System:
				return QStringLiteral("system");
			case ColorScheme::Light:
				break;
		}
		return QStringLiteral("light");
	}

	/**
		@brief colorSchemeFromString
		@param text
		@return the scheme named by @a text, Light for anything else
	*/
	ColorScheme colorSchemeFromString(const QString &text)
	{
		if (text == QLatin1String("dark")) {
			return ColorScheme::Dark;
		}
		if (text == QLatin1String("system")) {
			return ColorScheme::System;
		}
		return ColorScheme::Light;
	}

	/**
		@brief colorScheme
		@param settings
		@return the colour scheme stored in @a settings
	*/
	ColorScheme colorScheme(const QSettings &settings)
	{
		return colorSchemeFromString(
			settings.value(colorSchemeKey(),
				       colorSchemeToString(ColorScheme::Light))
			.toString());
	}

	/**
		@brief colorScheme
		@return the colour scheme of the running application
	*/
	ColorScheme colorScheme()
	{
		QSettings settings;
		return colorScheme(settings);
	}

	/**
		@brief setColorScheme
		@param settings
		@param scheme
	*/
	void setColorScheme(QSettings &settings, ColorScheme scheme)
	{
		settings.setValue(colorSchemeKey(), colorSchemeToString(scheme));
	}

	/**
		@brief setColorScheme
		@param scheme
	*/
	void setColorScheme(ColorScheme scheme)
	{
		QSettings settings;
		setColorScheme(settings, scheme);
	}

	/**
		@brief borderColumnsFromZeroDefault
		@return what an omitted border-columns_0 key means.

		This is the value the four consumers of the key read with -
		BorderTitleBlock twice, DiagramPosition and assignVariables - and it
		is a function rather than a literal so that the preference page and
		the consumers cannot drift apart again without a test saying so.
	*/
	bool borderColumnsFromZeroDefault()
	{
		return true;
	}

	/**
		@brief borderColumnsFromZero
		@param settings
		@return true when the title block columns are numbered from 0
	*/
	bool borderColumnsFromZero(const QSettings &settings)
	{
		return settings.value(borderColumnsFromZeroKey(),
				      borderColumnsFromZeroDefault()).toBool();
	}

	/**
		@brief borderColumnsFromZero
		@return the value in force in the running application
	*/
	bool borderColumnsFromZero()
	{
		QSettings settings;
		return borderColumnsFromZero(settings);
	}

	/**
		@brief setBorderColumnsFromZero
		@param settings
		@param from_zero
	*/
	void setBorderColumnsFromZero(QSettings &settings, bool from_zero)
	{
		settings.setValue(borderColumnsFromZeroKey(), from_zero);
	}

	/**
		@brief setBorderColumnsFromZero
		@param from_zero
	*/
	void setBorderColumnsFromZero(bool from_zero)
	{
		QSettings settings;
		setBorderColumnsFromZero(settings, from_zero);
	}
}
