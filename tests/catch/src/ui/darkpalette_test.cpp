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
#include "../../../../sources/autoNum/ui/autonumberingdockwidget.h"
#include "../../../../sources/qetpalette.h"

#include <catch2/catch.hpp>

#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPainter>
#include <QPair>
#include <QPalette>
#include <QSpinBox>
#include <QString>
#include <QStyle>
#include <QStyleFactory>
#include <QTest>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <memory>

/*
	Readable text on a dark palette (T63, steps 4 and 5).

	Two functions of QET::Palette are proved here. withReadableDisabled()
	lifts the text of disabled widgets to 3:1 when the palette the platform
	hands over is dark and its disabled grey sinks into the window; it is
	what QETApp::initStyle() applies on Windows, where the native style and
	the platform palette stay. readable() turns a color written into the
	code for an error or a warning - picked on a light background - into
	one that reads at 4.5:1 on a dark palette, same hue, and leaves it
	alone on a light one.

	The dock of the automatic numbering is the widget the dark screen was
	reported on: with no project, it is disabled whole. It is measured here
	under Fusion, named, and with a palette handed to the widget rather than
	to the application, so nothing of this file outlives its cases. What it
	proves is the palette - the text the style draws from the palette's
	disabled roles - and not the windows11 style of the reported screen:
	that style draws the frame and the fill of a disabled field from colors
	of its own (WinUI 3), which no palette reaches, and the offscreen
	platform cannot report the dark color scheme it would need anyway.
*/

namespace
{
	const QList<QPalette::ColorGroup> groups = {QPalette::Active,
						    QPalette::Inactive,
						    QPalette::Disabled};

	/// The four roles withReadableDisabled() looks after.
	const QList<QPalette::ColorRole> text_roles = {QPalette::WindowText,
						       QPalette::Text,
						       QPalette::ButtonText,
						       QPalette::PlaceholderText};

	/// Every role in every group, as a list of (group, role).
	QList<QPair<QPalette::ColorGroup, QPalette::ColorRole>> everyRole()
	{
		QList<QPair<QPalette::ColorGroup, QPalette::ColorRole>> all;
		for (QPalette::ColorGroup group : groups)
		{
			for (int role = 0 ; role < QPalette::NColorRoles ; ++role)
			{
				if (role == QPalette::NoRole) {
					continue;
				}
				all << qMakePair(group, static_cast<QPalette::ColorRole>(role));
			}
		}
		return all;
	}

	/// a as it shows over the opaque b.
	QColor over(const QColor &a, const QColor &b)
	{
		const int alpha = a.alpha();
		const auto mix = [alpha](int front, int back) {
			return (front * alpha + back * (255 - alpha) + 127) / 255;
		};
		return QColor(mix(a.red(), b.red()),
			      mix(a.green(), b.green()),
			      mix(a.blue(), b.blue()));
	}

	/// The lowest contrast of color, as it shows, against the backgrounds.
	double worst(const QColor &color, const QList<QColor> &backgrounds)
	{
		double lowest = 21.0;
		for (const QColor &background : backgrounds) {
			lowest = std::min(lowest, QET::Palette::contrastRatio(
						  over(color, background), background));
		}
		return lowest;
	}

	/// Where readable() promises the text reads: Window, Base, AlternateBase.
	QList<QColor> textGrounds(const QPalette &palette)
	{
		const QColor base = palette.color(QPalette::Active, QPalette::Base);
		return {palette.color(QPalette::Active, QPalette::Window),
			base,
			over(palette.color(QPalette::Active, QPalette::AlternateBase), base)};
	}

	/// Where withReadableDisabled() promises disabled text reads.
	QList<QColor> disabledGrounds(const QPalette &palette)
	{
		return {palette.color(QPalette::Disabled, QPalette::Window),
			palette.color(QPalette::Disabled, QPalette::Base),
			palette.color(QPalette::Disabled, QPalette::Button)};
	}

	/// Hue distance in degrees, on the circle.
	double hueDistance(const QColor &a, const QColor &b)
	{
		const double d = std::fabs(a.hslHueF() * 360.0 - b.hslHueF() * 360.0);
		return std::min(d, 360.0 - d);
	}

	/**
		A palette with every role of every group set, Inactive equal to
		Active, so a widget given it takes nothing from the application.
	*/
	QPalette paletteOf(const QColor &window, const QColor &base,
			   const QColor &alternate_base, const QColor &button,
			   const QColor &text, const QColor &disabled_text,
			   const QColor &placeholder, const QColor &disabled_placeholder)
	{
		QPalette p;
		for (QPalette::ColorGroup group : groups)
		{
			const bool off = group == QPalette::Disabled;
			p.setColor(group, QPalette::Window, window);
			p.setColor(group, QPalette::WindowText, off ? disabled_text : text);
			p.setColor(group, QPalette::Base, off ? window : base);
			p.setColor(group, QPalette::AlternateBase, alternate_base);
			p.setColor(group, QPalette::Text, off ? disabled_text : text);
			p.setColor(group, QPalette::Button, button);
			p.setColor(group, QPalette::ButtonText, off ? disabled_text : text);
			p.setColor(group, QPalette::BrightText, QColor(255, 90, 90));
			p.setColor(group, QPalette::Light, button.lighter(200));
			p.setColor(group, QPalette::Midlight, button.lighter(150));
			p.setColor(group, QPalette::Mid, button.darker(150));
			p.setColor(group, QPalette::Dark, button.darker(200));
			p.setColor(group, QPalette::Shadow, Qt::black);
			p.setColor(group, QPalette::Highlight, QColor(0, 90, 158));
			p.setColor(group, QPalette::HighlightedText, Qt::white);
			p.setColor(group, QPalette::Link, QColor(100, 170, 255));
			p.setColor(group, QPalette::LinkVisited, QColor(190, 130, 255));
			p.setColor(group, QPalette::ToolTipBase, button);
			p.setColor(group, QPalette::ToolTipText, text);
			p.setColor(group, QPalette::PlaceholderText,
				   off ? disabled_placeholder : placeholder);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
			p.setColor(group, QPalette::Accent, QColor(0, 90, 158));
#endif
		}
		return p;
	}

	/**
		The dark palette Qt 6.11 builds on Windows, from
		qwindowstheme.cpp (populateDarkSystemBasePalette() and the dark
		branch of systemPalette()): window 0x1E, Base window.lighter(150),
		Button window.lighter(200), white text, disabled text halfway
		between text and Button, disabled Base equal to Window. Those match
		the reported screen: window 30, the trees of the docks 45. The
		AlternateBase is a faint translucent white, as the windows11 style
		puts there; its alpha, and the half-transparent placeholder, are
		approximations.
	*/
	QPalette windowsDark()
	{
		const QColor window(0x1e, 0x1e, 0x1e);
		const QColor button = window.lighter(200);
		const QColor text(Qt::white);
		const QColor disabled((text.red() + button.red()) / 2,
				      (text.green() + button.green()) / 2,
				      (text.blue() + button.blue()) / 2);
		const QColor placeholder(255, 255, 255, 128);
		return paletteOf(window, window.lighter(150),
				 QColor(255, 255, 255, 0x0f), button,
				 text, disabled, placeholder, placeholder);
	}

	/**
		The same palette with a disabled grey that sinks into the window:
		43, the grey of the reported boxes, at 1.2:1 on the window of 30;
		and a placeholder white at an eighth of its alpha.
	*/
	QPalette failingDark()
	{
		const QColor window(0x1e, 0x1e, 0x1e);
		return paletteOf(window, window.lighter(150),
				 QColor(255, 255, 255, 0x0f), window.lighter(200),
				 Qt::white, QColor(43, 43, 43),
				 QColor(255, 255, 255, 128), QColor(255, 255, 255, 0x20));
	}

	/// A light palette whose disabled text is too pale, on purpose.
	QPalette paleLight()
	{
		return paletteOf(QColor(240, 240, 240), Qt::white, QColor(245, 245, 245),
				 QColor(225, 225, 225), Qt::black, QColor(225, 225, 225),
				 QColor(0, 0, 0, 128), QColor(0, 0, 0, 40));
	}

	/// Every color of every group, and the resolve mask, are the same.
	bool sameRoles(const QPalette &a, const QPalette &b)
	{
		if (a.resolveMask() != b.resolveMask()) {
			return false;
		}
		const auto all = everyRole();
		for (const auto &where : all)
		{
			if (a.color(where.first, where.second) != b.color(where.first, where.second)) {
				return false;
			}
		}
		return true;
	}

	/// The fixed colors of the code that T63 step 5 goes through.
	const QList<QColor> fixed_colors = {
		QColor(0xa0, 0x00, 0x00),	// problem lines, symbol creation
		QColor(0x8a, 0x6d, 0x00),	// notes, symbol creation
		QColor(0xc0, 0x39, 0x2b),	// generic device wizard
		QColor(0xd0, 0x30, 0x30),	// macro recording
		QColor(0xc1, 0x3a, 0x2e),	// label already taken
		QColor(0x1a, 0x73, 0xe8),	// contact group question mark
		QColor(Qt::red),		// lists of the I/O, locations, macros
		QColor(Qt::darkYellow),		// mounting check
		QColor(Qt::gray),		// wired channels
		QColor(Qt::blue),
		QColor(Qt::darkGray)};

	/// Pixels per color inside the image.
	QHash<QRgb, int> histogram(const QImage &image)
	{
		QHash<QRgb, int> counts;
		for (int y = 0 ; y < image.height() ; ++y) {
			for (int x = 0 ; x < image.width() ; ++x) {
				++counts[image.pixel(x, y)];
			}
		}
		return counts;
	}

	/**
		Contrast between the background of a rendered widget (its most
		frequent color) and the ink on it (the pixel that differs most
		from the background), as tests/qttest/inkcontrast.h measures it.
	*/
	double inkContrast(const QImage &image)
	{
		const QHash<QRgb, int> counts = histogram(image);
		QRgb background = 0;
		int most = -1;
		for (auto it = counts.cbegin() ; it != counts.cend() ; ++it)
		{
			if (it.value() > most)
			{
				most = it.value();
				background = it.key();
			}
		}
		double contrast = 1.0;
		for (auto it = counts.cbegin() ; it != counts.cend() ; ++it) {
			contrast = std::max(contrast, QET::Palette::contrastRatio(
						    QColor(background), QColor(it.key())));
		}
		return contrast;
	}

	double inkContrast(QWidget *widget)
	{
		return inkContrast(widget->grab().toImage().convertToFormat(QImage::Format_RGB32));
	}

	/**
		@return whether this build draws letters at all without a screen.
		Measured on this project, not assumed: one build rasterised no
		glyph offscreen, and an ink assertion then measures the platform.
	*/
	bool rasterisesGlyphs()
	{
		QImage image(60, 20, QImage::Format_RGB32);
		image.fill(Qt::white);
		{
			QPainter painter(&image);
			painter.setPen(Qt::black);
			painter.drawText(QRect(0, 0, 60, 20), Qt::AlignLeft | Qt::AlignVCenter,
					 QStringLiteral("mmm"));
		}
		return inkContrast(image) > 1.0;
	}

	/// Give the widget and every widget inside it the style.
	void styleAll(QWidget *widget, QStyle *style)
	{
		widget->setStyle(style);
		const QList<QWidget *> children = widget->findChildren<QWidget *>();
		for (QWidget *child : children) {
			child->setStyle(style);
		}
	}
}

TEST_CASE("T63 — readable(): numa paleta clara, a cor fixa sai como entrou",
	  "[darkpalette]")
{
	for (const QPalette &light : {QET::Palette::fusionLight(), paleLight()})
	{
		REQUIRE_FALSE(QET::Palette::isDark(light));
		for (const QColor &wanted : fixed_colors)
		{
			INFO(wanted.name().toStdString());
			REQUIRE(QET::Palette::readable(wanted, light) == wanted);
		}
	}

	// The alpha is not part of the answer either way.
	const QColor translucent(0xa0, 0x00, 0x00, 100);
	REQUIRE(QET::Palette::readable(translucent, QET::Palette::fusionLight()) == translucent);
}

TEST_CASE("T63 — readable(): numa paleta escura, a cor fixa sobe até 4,5:1, com o mesmo matiz e pelo menor passo",
	  "[darkpalette]")
{
	const QList<QPair<const char *, QPalette>> palettes = {
		{"windows", windowsDark()},
		{"fusion", QET::Palette::fusionDark()}};

	for (const auto &named : palettes)
	{
		const QPalette &dark = named.second;
		REQUIRE(QET::Palette::isDark(dark));
		const QList<QColor> grounds = textGrounds(dark);

		for (const QColor &wanted : fixed_colors)
		{
			INFO(named.first << " " << wanted.name().toStdString());
			const QColor got = QET::Palette::readable(wanted, dark);
			INFO("got " << got.name().toStdString()
			     << " at " << worst(got, grounds)
			     << ", was " << worst(wanted, grounds));

			REQUIRE(worst(got, grounds) >= 4.5);
			REQUIRE(got.alpha() == wanted.alpha());
			// Lighter or the same, never darker.
			REQUIRE(got.lightnessF() >= wanted.lightnessF() - 0.002);

			if (wanted.hslSaturationF() < 0.02) {
				REQUIRE(got.hslSaturationF() < 0.02);
			} else {
				REQUIRE(hueDistance(got, wanted) <= 2.0);
			}

			if (worst(wanted, grounds) >= 4.5)
			{
				// What already reads is left as it was.
				REQUIRE(got == wanted);
			}
			else
			{
				// The smallest step: a hundredth of lightness less no
				// longer reads, so the color did not overshoot to white.
				const QColor dimmer = QColor::fromHslF(
							std::max(got.hslHueF(), 0.0f),
							got.hslSaturationF(),
							std::max(got.lightnessF() - 0.01f, 0.0f));
				REQUIRE(worst(dimmer, grounds) < 4.5);
			}
		}
	}

	// The case the specification names: dark red, dark yellow, blue and
	// grey all end above 4.5 on the palette of the reported screen, and
	// each of them had to move.
	const QPalette dark = windowsDark();
	for (const QColor &wanted : {QColor(0xa0, 0x00, 0x00), QColor(Qt::darkYellow),
				     QColor(Qt::blue), QColor(Qt::darkGray)})
	{
		INFO(wanted.name().toStdString());
		REQUIRE(worst(wanted, textGrounds(dark)) < 4.5);
		REQUIRE(QET::Palette::readable(wanted, dark) != wanted);
	}
}

TEST_CASE("T63 — withReadableDisabled(): a paleta clara não muda, nem a que já se lê",
	  "[darkpalette]")
{
	for (const QPalette &light : {QET::Palette::fusionLight(), paleLight()})
	{
		REQUIRE(sameRoles(QET::Palette::withReadableDisabled(light), light));
	}

	// The palette Windows hands over in dark mode already reads: its
	// disabled grey is halfway between white and the button, 4.1:1 on the
	// button and 6.1:1 on the window. The correction leaves it as it came,
	// so on the reported screen it changes nothing; what was faint there
	// is drawn by the style, not taken from the palette.
	const QPalette windows = windowsDark();
	for (QPalette::ColorRole role : text_roles)
	{
		INFO(int(role));
		REQUIRE(worst(windows.color(QPalette::Disabled, role), disabledGrounds(windows)) >= 3.0);
	}
	REQUIRE(sameRoles(QET::Palette::withReadableDisabled(windows), windows));

	// The palette of the macOS path has its disabled grey tuned already.
	const QPalette fusion = QET::Palette::fusionDark();
	REQUIRE(sameRoles(QET::Palette::withReadableDisabled(fusion), fusion));
}

TEST_CASE("T63 — withReadableDisabled(): o texto desabilitado sobe a 3:1 e fica mais fraco que o habilitado",
	  "[darkpalette]")
{
	QPalette failing = failingDark();
	// Start from nothing set, as the palette Qt builds for the platform is
	// (QApplicationPrivate::basePalette() clears the resolve mask), so the
	// bits the correction sets can be seen.
	failing.setResolveMask(0);
	const QPalette fixed = QET::Palette::withReadableDisabled(failing);
	const QList<QColor> grounds = disabledGrounds(fixed);

	for (QPalette::ColorRole role : text_roles)
	{
		INFO(int(role));
		const QColor before = failing.color(QPalette::Disabled, role);
		const QColor after = fixed.color(QPalette::Disabled, role);
		INFO("before " << before.name(QColor::HexArgb).toStdString()
		     << " after " << after.name(QColor::HexArgb).toStdString());

		REQUIRE(worst(before, grounds) < 3.0);
		REQUIRE(worst(after, grounds) >= 3.0);
		REQUIRE(after.alpha() == 255);
		// The bit of a corrected role is set, so a style that honours the
		// application's choice for a role draws this one.
		REQUIRE(fixed.isBrushSet(QPalette::Disabled, role));
	}

	// Still dimmer than the enabled text, so a disabled control reads as
	// disabled.
	const QColor window = fixed.color(QPalette::Disabled, QPalette::Window);
	for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
	{
		INFO(int(role));
		REQUIRE(QET::Palette::contrastRatio(fixed.color(QPalette::Disabled, role), window)
			< QET::Palette::contrastRatio(fixed.color(QPalette::Active, role), window));
	}

	// Nothing but the disabled text moved: the other groups, and the other
	// roles of the disabled one, are color for color what they were, and
	// none of them gained a bit.
	const auto all = everyRole();
	for (const auto &where : all)
	{
		if (where.first == QPalette::Disabled && text_roles.contains(where.second)) {
			continue;
		}
		INFO(int(where.first) << " " << int(where.second));
		REQUIRE(fixed.color(where.first, where.second)
			== failing.color(where.first, where.second));
		REQUIRE_FALSE(fixed.isBrushSet(where.first, where.second));
	}

	// A tinted grey keeps its tint.
	QPalette tinted = failingDark();
	tinted.setColor(QPalette::Disabled, QPalette::Text, QColor(40, 44, 64));
	const QColor lifted = QET::Palette::withReadableDisabled(tinted)
			.color(QPalette::Disabled, QPalette::Text);
	REQUIRE(worst(lifted, disabledGrounds(tinted)) >= 3.0);
	REQUIRE(hueDistance(lifted, QColor(40, 44, 64)) <= 2.0);
}

TEST_CASE("T63 — o dock de numeração automática desabilitado: rótulos e números a 3:1 com a paleta corrigida",
	  "[darkpalette]")
{
	if (!rasterisesGlyphs())
	{
		SUCCEED("this build rasterises no glyph without a screen; the ink "
			"measured would be the platform's and not the palette's");
		return;
	}

	// Declared before the dock so it outlives every widget that uses it.
	const std::unique_ptr<QStyle> fusion(QStyleFactory::create(QStringLiteral("Fusion")));
	REQUIRE(fusion);

	const QPalette failing = failingDark();
	const QPalette fixed = QET::Palette::withReadableDisabled(failing);

	// No project: the dock is disabled whole, as it is on the reported
	// screen before a numbering is chosen.
	AutoNumberingDockWidget dock;
	REQUIRE_FALSE(dock.isEnabled());

	// Large and bold, so the core of a stroke covers whole pixels and the
	// ink measured is the color of the palette and not an antialiased edge.
	QFont font = dock.font();
	font.setPixelSize(20);
	font.setBold(true);
	dock.setFont(font);
	styleAll(&dock, fusion.get());
	dock.setPalette(failing);
	dock.resize(700, 260);
	dock.show();
	REQUIRE(QTest::qWaitForWindowExposed(&dock));

	QList<QLabel *> labels;
	const QList<QLabel *> found = dock.findChildren<QLabel *>();
	for (QLabel *label : found) {
		if (!label->text().isEmpty() && label->isVisible()) {
			labels << label;
		}
	}
	QList<QLineEdit *> numbers;
	const QList<QSpinBox *> spins = dock.findChildren<QSpinBox *>();
	for (QSpinBox *spin : spins)
	{
		if (QLineEdit *edit = spin->findChild<QLineEdit *>()) {
			numbers << edit;
		}
	}
	REQUIRE(labels.size() >= 5);
	REQUIRE(numbers.size() == 3);

	QList<QWidget *> measured;
	for (QLabel *label : labels) {
		measured << label;
	}
	for (QLineEdit *edit : numbers) {
		measured << edit;
	}

	// The measurement sees the failure: with the grey of the reported
	// boxes, no label and no number reaches 3:1.
	for (QWidget *widget : measured)
	{
		INFO(widget->objectName().toStdString());
		REQUIRE_FALSE(widget->isEnabled());
		REQUIRE(inkContrast(widget) < 3.0);
	}

	dock.setPalette(fixed);
	QCoreApplication::processEvents();

	double strongest_disabled = 1.0;
	for (QWidget *widget : measured)
	{
		const double contrast = inkContrast(widget);
		INFO(widget->objectName().toStdString() << " at " << contrast);
		REQUIRE(contrast >= 3.0);
		strongest_disabled = std::max(strongest_disabled, contrast);
	}

	// And dimmer than the same text enabled.
	QLabel enabled(QStringLiteral("Value"));
	enabled.setFont(font);
	enabled.setStyle(fusion.get());
	enabled.setPalette(fixed);
	enabled.resize(200, 40);
	enabled.show();
	REQUIRE(QTest::qWaitForWindowExposed(&enabled));
	REQUIRE(enabled.isEnabled());
	const double enabled_contrast = inkContrast(&enabled);
	INFO("enabled " << enabled_contrast << ", strongest disabled " << strongest_disabled);
	REQUIRE(enabled_contrast > strongest_disabled);
}
