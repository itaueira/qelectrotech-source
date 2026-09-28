#include "../../../sources/qetapp.h"

/*
	QET::ElementsAndConductorsSentence() builds its list with
	QLocale(QETApp::interfaceLanguage()), so that "3 elements, 2 conductors
	and 1 shape" is joined the way the interface language joins a list.
	interfaceLanguage() is inline in qetapp.h and returns this static, whose
	definition lives in qetapp.cpp.

	This target deliberately does not compile qetapp.cpp: the hand-written
	source list of C_unittests is what documents that these rules compute on
	their own, and linking the whole application would erase that contract
	(see tests/catch/CMakeLists.txt). Supplying the single symbol here is the
	same answer tst_crashdumps gives for QETApp::dataDir().

	Empty on purpose: QLocale(QString()) is the C locale, which joins with a
	plain comma. No test asserts on the separator; the tests that use qet.cpp
	ask about file names and sort keys.
*/
QString QETApp::m_interface_language;
