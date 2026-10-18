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
#include "../../../../sources/qetpalette.h"
#include "../../../../sources/qetstyle.h"
#include "../../../../sources/ui/conductorcolortoolbutton.h"
#include "../../../../sources/ui/diagrambgcolorbutton.h"

#include <catch2/catch.hpp>

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QPair>
#include <QPalette>
#include <QPixmap>
#include <QProxyStyle>
#include <QSpinBox>
#include <QStringList>
#include <QStyleFactory>
#include <QStyleOption>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

/*
	The icons of the tool bars and menus on a dark palette, measured without
	a screen: the enabled icon, the disabled one, and the color swatches of
	the conductor and sheet background buttons. And, at the end, the frame
	of a disabled input box, which QETStyle draws for the same palette.

	How an icon is measured. Its pixels more than half opaque are composed
	over the ground it is drawn on; the fifth of them that differ the most
	from the ground are averaged, and the WCAG ratio of that average against
	the ground is the figure. A line-art icon is a few strong pixels among
	faint anti-aliasing, and the mean of all its pixels would call a crisp
	icon faint. Enabled must reach 3:1 (WCAG 1.4.11, non-text contrast);
	disabled must be weaker than enabled and still reach 1.8:1.

	The ground is the tool bar of a Windows 11 session in dark mode, with
	Qt's windows11 style: (30,30,30), measured on a screenshot. The swatches
	are also measured on the menu grounds (49,49,49) and Fusion's dark window
	(53,53,53), and on light grounds, since they are drawn once for both.

	What this does not prove, and why the cases carry "T63" and not the
	number of a use case:
	  - On that screen the base style is windows11, and here it cannot be.
	    The disabled pixmap comes from QETStyle either way: QIcon asks the
	    application style for it, and the program wraps whatever style runs
	    in QETStyle (QETApp::initStyle()). So the base style here is one
	    whose disabled icon is empty (VanishingStyle below): if QETStyle
	    left a dark palette to its base style, every disabled reading would
	    be 0. Whether windows11 paints the pixmap with an opacity of its own
	    on top is only seen on that screen.
	  - The editor window is not built: it needs a QETApp, which this suite
	    does not start (see contextmenuassign_test.cpp). So the icons are
	    taken by their theme name, which is how sources/qeticons.cpp loads
	    every icon of the tool bars, and not from the actions.

	Not measured, on purpose:
	  - Maximize, Restore, Minimize and Close of a folio window, and
	    "What's This?" of the Help menu: QMdiSubWindow's system menu and
	    QWhatsThis show the style's standard pixmaps (QStyle::standardIcon),
	    not an icon of the program. They follow the platform's style.
	  - The project file-type icon (QET::Icons::ProjectFile, "Activate this
	    project" in the elements panel): colored art of ico/breeze-icons/,
	    whose license (CC-BY-ND-3.0) allows no recolored copy. It reads at
	    3.1:1 on the Windows ground and below 3:1 on Fusion's dark window.
	  - The flags of the language menu and the color swatches of the symbol
	    editor (ico/flags/, ico/colors/): pictures, not drawn in ink, and
	    not on a tool bar.
*/

namespace
{
	/// The tool bar of Windows 11 in dark mode, with Qt's windows11 style.
	const QColor windows_dark_bar(30, 30, 30);

	struct Reading
	{
		double ratio = 0.;
		int pixels = 0;
	};

	/// How pixmap reads on ground: see the comment at the top of this file.
	Reading reading(const QPixmap &pixmap, const QColor &ground)
	{
		const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
		QList<QPair<double, QColor>> pixels;
		for (int y = 0; y < image.height(); ++y)
		{
			const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
			for (int x = 0; x < image.width(); ++x)
			{
				const int alpha = qAlpha(line[x]);
				if (alpha <= 127)
				{
					continue;
				}
				const auto mix = [alpha](int front, int back) {
					return (front * alpha + back * (255 - alpha) + 127) / 255;
				};
				const QColor shown(mix(qRed(line[x]), ground.red()),
						   mix(qGreen(line[x]), ground.green()),
						   mix(qBlue(line[x]), ground.blue()));
				pixels.append(qMakePair(QET::Palette::contrastRatio(shown, ground), shown));
			}
		}

		Reading result;
		result.pixels = int(pixels.size());
		if (pixels.isEmpty())
		{
			return result;
		}
		std::stable_sort(pixels.begin(), pixels.end(),
				 [](const QPair<double, QColor> &a, const QPair<double, QColor> &b) {
					 return a.first > b.first;
				 });
		const int top = qMax(1, int(std::ceil(pixels.size() * 0.2)));
		qint64 red = 0;
		qint64 green = 0;
		qint64 blue = 0;
		for (int i = 0; i < top; ++i)
		{
			red += pixels.at(i).second.red();
			green += pixels.at(i).second.green();
			blue += pixels.at(i).second.blue();
		}
		result.ratio = QET::Palette::contrastRatio(
					QColor(int(red / top), int(green / top), int(blue / top)), ground);
		return result;
	}

	/**
		The dark palette of Windows 11 as far as these cases are concerned:
		the window of the reported screen, white text, and disabled roles
		close to those of Qt's Windows dark palette (the disabled text
		(157,157,157), the disabled base the window). For an icon QETStyle
		only asks whether Window is dark.
	*/
	QPalette windowsDark()
	{
		QPalette palette;
		palette.setColor(QPalette::Window, windows_dark_bar);
		palette.setColor(QPalette::WindowText, Qt::white);
		palette.setColor(QPalette::Base, QColor(45, 45, 45));
		palette.setColor(QPalette::Text, Qt::white);
		palette.setColor(QPalette::Button, QColor(60, 60, 60));
		palette.setColor(QPalette::ButtonText, Qt::white);
		const QColor disabled_text(157, 157, 157);
		palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled_text);
		palette.setColor(QPalette::Disabled, QPalette::Text, disabled_text);
		palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled_text);
		palette.setColor(QPalette::Disabled, QPalette::Base, windows_dark_bar);
		return palette;
	}

	/**
		A base style whose disabled icon is empty: it stands for a style
		whose disabled icon all but vanishes on a dark window, which is
		what the reported screen showed. Fusion would prove nothing here:
		its disabled icon of the dark theme reads at 9:1 on that ground.
	*/
	class VanishingStyle : public QProxyStyle
	{
		public:
			VanishingStyle() :
				QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
			{}

			QPixmap generatedIconPixmap(QIcon::Mode mode,
						    const QPixmap &pixmap,
						    const QStyleOption *option) const override
			{
				if (mode != QIcon::Disabled)
				{
					return QProxyStyle::generatedIconPixmap(mode, pixmap, option);
				}
				QPixmap empty(pixmap.size());
				empty.fill(Qt::transparent);
				return empty;
			}
	};

	/// The icon theme of the program, put back as it was when the case ends.
	class IconTheme
	{
		public:
			explicit IconTheme(const QString &name) :
				m_name(QIcon::themeName()),
				m_paths(QIcon::themeSearchPaths())
			{
				QStringList paths = m_paths;
				if (!paths.contains(QStringLiteral(":/ico/themes")))
				{
					paths.prepend(QStringLiteral(":/ico/themes"));
				}
				QIcon::setThemeSearchPaths(paths);
				QIcon::setThemeName(name);
			}
			~IconTheme()
			{
				QIcon::setThemeName(m_name);
				QIcon::setThemeSearchPaths(m_paths);
			}

			IconTheme(const IconTheme &) = delete;
			IconTheme &operator=(const IconTheme &) = delete;

		private:
			QString m_name;
			QStringList m_paths;
	};

	/// Every icon name of the light theme; the dark theme inherits the rest.
	QStringList themeNames()
	{
		QStringList names;
		const QStringList folders = {
			QStringLiteral("16x16"), QStringLiteral("22x22"), QStringLiteral("24x24"),
			QStringLiteral("32x32"), QStringLiteral("48x48"), QStringLiteral("64x64"),
			QStringLiteral("128x128"), QStringLiteral("scalable")};
		for (const QString &folder : folders)
		{
			const QDir dir(QStringLiteral(":/ico/themes/qet/%1").arg(folder));
			const QStringList files = dir.entryList(QDir::Files);
			for (const QString &file : files)
			{
				names << file.section(QLatin1Char('.'), 0, -2);
			}
		}
		names.removeDuplicates();
		names.sort();
		return names;
	}

	QPixmap at22(const QIcon &icon, QIcon::Mode mode = QIcon::Normal)
	{
		return icon.pixmap(QSize(22, 22), 1.0, mode, QIcon::Off);
	}

	QString figure(double ratio)
	{
		return QString::number(ratio, 'f', 2);
	}

	/**
		widget drawn by style into an image of ground, the way it shows on
		a window of that color: the widget's own background is not filled,
		only what its style paints.
	*/
	QImage drawnBy(QWidget &widget, QStyle *style, const QPalette &palette, bool enabled,
		       const QColor &ground)
	{
		widget.setStyle(style);
		widget.setPalette(palette);
		widget.setEnabled(enabled);
		widget.resize(160, 28);
		widget.ensurePolished();
		QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
		image.fill(ground);
		widget.render(&image, QPoint(), QRegion(), QWidget::DrawChildren);
		return image.convertToFormat(QImage::Format_ARGB32);
	}

	/**
		How the edge of a box reads on ground: the mean color of its top
		and bottom rows, the rounded corners left out, against ground.
		Both styles measured here, and drawDisabledFrame(), draw the frame
		on the outermost pixels of the widget.
	*/
	double edge(const QImage &image, const QColor &ground)
	{
		qint64 red = 0;
		qint64 green = 0;
		qint64 blue = 0;
		int count = 0;
		for (int y : {0, image.height() - 1})
		{
			for (int x = 6; x < image.width() - 6; ++x)
			{
				const QColor color = image.pixelColor(x, y);
				red += color.red();
				green += color.green();
				blue += color.blue();
				++count;
			}
		}
		return QET::Palette::contrastRatio(
					QColor(int(red / count), int(green / count), int(blue / count)), ground);
	}
}

TEST_CASE("T63 — every icon of the dark theme reads at 3:1 on the dark tool bar of Windows, "
	  "and at 1.8:1 disabled, weaker than enabled",
	  "[T63][darkicons]")
{
	IconTheme theme(QStringLiteral("qet-dark"));
	REQUIRE(QIcon::hasThemeIcon(QStringLiteral("document-save")));

	QETStyle style(new VanishingStyle);
	QStyleOption option;
	option.palette = windowsDark();
	REQUIRE(QET::Palette::isDark(option.palette));

	const QStringList names = themeNames();
		//197 names on 18/10/2026; far fewer means the theme was not found
	REQUIRE(names.size() > 150);

	QStringList faint;
	QStringList faint_disabled;
	QStringList not_weaker;
	int measured = 0;
	for (const QString &name : names)
	{
		const QPixmap normal = at22(QIcon::fromTheme(name));
		const Reading enabled = reading(normal, windows_dark_bar);
			//Nothing opaque at 22 px is not "unreadable", and no tool
			//bar shows that size of it: left out of the count.
		if (enabled.pixels == 0)
		{
			continue;
		}
		++measured;

		const QPixmap disabled = style.generatedIconPixmap(QIcon::Disabled, normal, &option);
		const Reading greyed = reading(disabled, windows_dark_bar);

		if (enabled.ratio < 3.)
		{
			faint << QStringLiteral("%1 %2:1").arg(name, figure(enabled.ratio));
		}
		if (greyed.ratio < 1.8)
		{
			faint_disabled << QStringLiteral("%1 %2:1").arg(name, figure(greyed.ratio));
		}
		if (greyed.ratio >= enabled.ratio)
		{
			not_weaker << QStringLiteral("%1 %2:1 disabled, %3:1 enabled")
				      .arg(name, figure(greyed.ratio), figure(enabled.ratio));
		}
	}

	INFO("icons measured: " << measured << " of " << names.size());
	REQUIRE(measured > 150);
	INFO("enabled below 3:1: " << faint.join(QStringLiteral(", ")).toStdString());
	CHECK(faint.isEmpty());
	INFO("disabled below 1.8:1: " << faint_disabled.join(QStringLiteral(", ")).toStdString());
	CHECK(faint_disabled.isEmpty());
	INFO("disabled not weaker than enabled: " << not_weaker.join(QStringLiteral(", ")).toStdString());
	CHECK(not_weaker.isEmpty());
}

TEST_CASE("T63 — on a light palette the disabled icon is still the base style's",
	  "[T63][darkicons]")
{
	IconTheme theme(QStringLiteral("qet"));
	QETStyle style(new VanishingStyle);
	QStyleOption option;
	option.palette = QET::Palette::fusionLight();
	REQUIRE_FALSE(QET::Palette::isDark(option.palette));

		//Line art, colored art, and the icon this step moved into the theme
	for (const QString &name : {QStringLiteral("document-save"), QStringLiteral("zoom-fit-best"),
				    QStringLiteral("terminal"), QStringLiteral("location-area")})
	{
		INFO(name.toStdString());
		const QPixmap normal = at22(QIcon::fromTheme(name));
		REQUIRE(reading(normal, QColor(239, 239, 239)).pixels > 0);
			//The base style's answer, untouched: empty, here
		const QPixmap disabled = style.generatedIconPixmap(QIcon::Disabled, normal, &option);
		CHECK(reading(disabled, QColor(239, 239, 239)).pixels == 0);
	}
}

TEST_CASE("T63 — the color swatches keep their color and read on dark and light grounds, "
	  "enabled and disabled",
	  "[T63][darkicons]")
{
	QETStyle style(new VanishingStyle);
	QStyleOption dark;
	dark.palette = windowsDark();

		//In menu order: sources/ui/conductorcolortoolbutton.cpp and
		//sources/ui/diagrambgcolorbutton.cpp, standardColors()
	const QList<QColor> conductor_colors = {
		QColor(0x00, 0x00, 0x00), QColor(0x7B, 0x3F, 0x00), QColor(0x80, 0x80, 0x80),
		QColor(0x00, 0x00, 0xFF), QColor(0x00, 0x80, 0x00), QColor(0xFF, 0x00, 0x00),
		QColor(0xFF, 0x80, 0x00), QColor(0x80, 0x00, 0x80), QColor(0xFF, 0xFF, 0xFF)};
	const QList<QColor> background_colors = {
		QColor(0xFF, 0xFF, 0xFF), QColor(0xFD, 0xFB, 0xF5), QColor(0xE0, 0xE0, 0xE0),
		QColor(0x80, 0x80, 0x80), QColor(0x40, 0x40, 0x40), QColor(0x00, 0x00, 0x00)};

		//No editor: the buttons only need one to recolor, not to build
		//their menu.
	ConductorColorToolButton conductor(nullptr);
	DiagramBgColorToolButton background(nullptr);

	struct Swatch
	{
		QString where;
		QIcon icon;
		QColor color;
	};
	QList<Swatch> swatches;
	const auto collect = [&swatches](QToolButton &button, const QString &where,
					 const QList<QColor> &colors) {
		REQUIRE(button.menu());
		int index = 0;
		const QList<QAction *> actions = button.menu()->actions();
		for (QAction *action : actions)
		{
				//"System color", the separators, "Other color..."
			if (action->icon().isNull())
			{
				continue;
			}
			if (index == colors.size())
			{
				break;
			}
			Swatch swatch;
			swatch.where = QStringLiteral("%1 #%2").arg(where).arg(index);
			swatch.icon = action->icon();
			swatch.color = colors.at(index);
			swatches.append(swatch);
			++index;
		}
		REQUIRE(index == colors.size());
	};
	collect(conductor, QStringLiteral("conductor"), conductor_colors);
	collect(background, QStringLiteral("background"), background_colors);
	REQUIRE(swatches.size() == 15);

	const QList<QColor> grounds = {
		windows_dark_bar, QColor(49, 49, 49), QColor(53, 53, 53),
		QColor(239, 239, 239), QColor(255, 255, 255)};

	for (const Swatch &swatch : std::as_const(swatches))
	{
		INFO(swatch.where.toStdString() << " " << swatch.color.name().toStdString());
		const QPixmap normal = swatch.icon.pixmap(QSize(16, 16), 1.0, QIcon::Normal, QIcon::Off);
		REQUIRE(normal.width() == 16);

			//The color the user picks, unchanged in the middle
		const QImage image = normal.toImage().convertToFormat(QImage::Format_ARGB32);
		CHECK(image.pixel(8, 8) == swatch.color.rgba());
		CHECK(image.pixel(2, 2) == swatch.color.rgba());

		for (const QColor &ground : grounds)
		{
			INFO("on " << ground.name().toStdString());
			CHECK(reading(normal, ground).ratio >= 3.);
		}

			//The button is disabled when no folio is open: on the dark
			//bar, the swatch fades and keeps its outline.
		const QPixmap disabled = style.generatedIconPixmap(QIcon::Disabled, normal, &dark);
		const double enabled_ratio = reading(normal, windows_dark_bar).ratio;
		const double disabled_ratio = reading(disabled, windows_dark_bar).ratio;
		INFO("disabled " << disabled_ratio << ", enabled " << enabled_ratio);
		CHECK(disabled_ratio >= 1.8);
		CHECK(disabled_ratio < enabled_ratio);
	}
}

TEST_CASE("T63 — the location area, the element and the macro record icons read on both palettes",
	  "[T63][darkicons]")
{
	IconTheme theme(QStringLiteral("qet-dark"));
	const QList<QColor> dark_grounds = {windows_dark_bar, QColor(49, 49, 49)};
	const QColor light_ground(239, 239, 239);

	SECTION("the location area: a light-ink copy in the dark theme")
	{
			//It used to be loaded from its file, outside the theme: the
			//same grey (77,77,77) on both palettes, 1.95:1 on the dark bar.
		REQUIRE(QFile::exists(QStringLiteral(":/ico/themes/qet-dark/22x22/location-area.png")));
		const QPixmap dark = at22(QIcon::fromTheme(QStringLiteral("location-area")));
		for (const QColor &ground : dark_grounds)
		{
			INFO("on " << ground.name().toStdString());
			CHECK(reading(dark, ground).ratio >= 3.);
		}
		QIcon::setThemeName(QStringLiteral("qet"));
		CHECK(reading(at22(QIcon::fromTheme(QStringLiteral("location-area"))), light_ground).ratio >= 3.);
	}

	SECTION("the element: the theme's drawing, which has a light-ink copy")
	{
			//QET::Icons::Element, behind "select the elements of this
			//sheet" and the recent elements, was the file-type PNG: 1.91:1.
		const QPixmap dark = at22(QIcon::fromTheme(QStringLiteral("element")));
		for (const QColor &ground : dark_grounds)
		{
			INFO("on " << ground.name().toStdString());
			CHECK(reading(dark, ground).ratio >= 3.);
		}
		QIcon::setThemeName(QStringLiteral("qet"));
		CHECK(reading(at22(QIcon::fromTheme(QStringLiteral("element"))), light_ground).ratio >= 3.);
	}

	SECTION("the macro record: one red for both palettes")
	{
			//Drawn in code before, in a red that read at 2.57:1 on the
			//dark menu. Colored art has no copy in the dark theme: the
			//one file has to read on both.
		REQUIRE(QIcon::hasThemeIcon(QStringLiteral("media-record")));
		CHECK_FALSE(QFile::exists(QStringLiteral(":/ico/themes/qet-dark/scalable/media-record.svg")));
		const QPixmap dark = at22(QIcon::fromTheme(QStringLiteral("media-record")));
		for (const QColor &ground : dark_grounds + QList<QColor>{QColor(53, 53, 53)})
		{
			INFO("on " << ground.name().toStdString());
			CHECK(reading(dark, ground).ratio >= 3.);
		}
		QIcon::setThemeName(QStringLiteral("qet"));
		const QPixmap light = at22(QIcon::fromTheme(QStringLiteral("media-record")));
		for (const QColor &ground : {light_ground, QColor(255, 255, 255)})
		{
			INFO("on " << ground.name().toStdString());
			CHECK(reading(light, ground).ratio >= 3.);
		}
	}
}

/*
	The boxes of the "automatic numbering selection" dock are disabled
	until a numbering is chosen, and on the dark palette of Windows they
	all but vanished: the Windows 11 style fills and frames a disabled box
	in colors of its own, white at 4% and 7% (about 39 and 46 on the
	window of 30), whatever the palette says - about 1.2:1.

	Fusion is the style available without a screen, and it draws from the
	palette, so the same palette gives other pixels here than on that
	screen. What this proves is QETStyle's part: on a dark palette a
	disabled box gets an outline at 3:1 or more, drawn over whatever the
	base style drew; an enabled box and a light palette are drawn exactly
	as the base style draws them. What it cannot prove is where the
	Windows 11 style puts its own frame: the outline is drawn on the
	outermost pixels of the widget, where Fusion draws its frame, and if
	that style insets its frame, the two show as a box inside a box. Only
	that screen tells.
*/
TEST_CASE("T63 — a disabled line edit, spin box and combo box keep an outline at 3:1 on a dark "
	  "palette; enabled boxes and light palettes are drawn as before",
	  "[T63][darkicons]")
{
	QETStyle qet_style(QStyleFactory::create(QStringLiteral("Fusion")));
	const std::unique_ptr<QStyle> fusion(QStyleFactory::create(QStringLiteral("Fusion")));
	REQUIRE(fusion);
	const QPalette dark = windowsDark();
	const QPalette light = QET::Palette::fusionLight();
	const QColor light_ground = light.color(QPalette::Active, QPalette::Window);

		//The outline stays dimmer than the text it frames: a disabled box
		//must not look more alive than its own content.
	const QColor outline = QETStyle::disabledFrameColor(dark);
	INFO("outline " << outline.name().toStdString());
	CHECK(outline.lightness() < dark.color(QPalette::Disabled, QPalette::Text).lightness());

	const QStringList kinds = {
		QStringLiteral("line edit"), QStringLiteral("spin box"), QStringLiteral("combo box")};
	for (int kind = 0; kind < kinds.size(); ++kind)
	{
		INFO(kinds.at(kind).toStdString());
			//A fresh widget for every image: a widget keeps state from one
			//style to the next (polish, cached size hints).
		const auto drawn = [kind](QStyle *style, const QPalette &palette, bool enabled,
					  const QColor &ground) {
			std::unique_ptr<QWidget> widget;
			if (kind == 0)
			{
				auto *edit = new QLineEdit;
				edit->setText(QStringLiteral("K1"));
				widget.reset(edit);
			}
			else if (kind == 1)
			{
				auto *spin = new QSpinBox;
				spin->setValue(12);
				widget.reset(spin);
			}
			else
			{
				auto *combo = new QComboBox;
				combo->addItem(QStringLiteral("-K1"));
				widget.reset(combo);
			}
			return drawnBy(*widget, style, palette, enabled, ground);
		};

		const QImage outlined = drawn(&qet_style, dark, false, windows_dark_bar);
		const QImage base_only = drawn(fusion.get(), dark, false, windows_dark_bar);
		INFO("edge with QETStyle " << edge(outlined, windows_dark_bar)
		     << ", with the base style alone " << edge(base_only, windows_dark_bar));
		CHECK(edge(outlined, windows_dark_bar) >= 3.);

			//Enabled, on the same dark palette: the base style's drawing
		CHECK(drawn(&qet_style, dark, true, windows_dark_bar)
		      == drawn(fusion.get(), dark, true, windows_dark_bar));
			//Disabled, on a light palette: the base style's drawing
		CHECK(drawn(&qet_style, light, false, light_ground)
		      == drawn(fusion.get(), light, false, light_ground));
	}
}
