#define CATCH_CONFIG_RUNNER
#include <QtWidgets/QApplication>
#include <catch2/catch.hpp>

int main(int argc, char** argv)
{
	// Without an organisation and an application name, QSettings has no file
	// to write to: setValue() is accepted and then discarded, so a test that
	// stores a setting and reads it back silently sees nothing - which is how
	// the first settings-dependent test in this suite looked like a bug in the
	// code under test.
	//
	// The application name is what names the file, and it is deliberately not
	// "QElectroTech": the suite must not write into the settings of the
	// program it tests.
	QCoreApplication::setOrganizationName(QStringLiteral("QElectroTech"));
	QCoreApplication::setApplicationName(QStringLiteral("C_unittests"));

	// A widget test needs a QApplication, and a QApplication needs a platform.
	// "offscreen" is the one that draws nowhere: the dialog is built, clicked
	// and asserted on entirely inside this process, and nothing appears on the
	// screen of whoever is running the suite.
	//
	// That distinction is the whole reason widget tests are allowed here at
	// all. Synthesising clicks at the level of the operating system would send
	// them into the live session of whoever is at the machine - which happened
	// once on this project, and must not happen again. QTest::mouseClick posts
	// the event to the widget, not to the desktop.
	//
	// Not forced when already set, so the suite can be run against a real
	// platform on purpose.
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
		qputenv("QT_QPA_PLATFORM", "offscreen");
	}

	QApplication app(argc, argv);
	return Catch::Session().run(argc, argv);
}
