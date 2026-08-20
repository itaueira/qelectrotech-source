#define CATCH_CONFIG_RUNNER
#include <QtGui/QGuiApplication>
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

	QGuiApplication app(argc, argv);
	return Catch::Session().run(argc, argv);
}
