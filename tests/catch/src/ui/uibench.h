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
#ifndef UIBENCH_H
#define UIBENCH_H

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

class Diagram;
class QETProject;

/**
	Helpers for the tests that need a real project open.

	A QETProject can be built without a QETApp: the command line export path
	in main.cpp does exactly that, and nothing in the loading path reaches
	QETApp::instance(). What the loading path does reach are two modal
	message boxes, and that is the trap this namespace exists to close -
	see opensWithoutDialog().
*/
namespace UiBench
{
	/**
		Absolute path of a file of the examples directory shipped with
		QElectroTech. The directory itself comes from the QET_EXAMPLES_DIR
		compile definition, so the tests do not depend on the working
		directory they happen to be run from.
	*/
	QString examplePath(const QString &file_name);

	/**
		Whether opening file_path would go through without a dialog.

		QETProject::readProjectXml() raises a modal warning in two cases: a
		project saved by a newer version, and a project made with QET 0.6 or
		lower. Both of them call exec(), and a modal exec() under the
		offscreen platform does not fail - it waits, forever, for a click
		that no one is there to give. A suite that opens such a file does not
		report an error: it hangs, and the run has to be killed.

		So the check happens before the file is handed to QETProject, using
		the very functions the program uses to decide (QetVersion), and a
		refused file becomes a legible message instead of a stuck process.

		Concretely: examples/schema_indus.qet is version 0.3 and is refused
		here. Every other example ships as 0.80 or later and passes.
	*/
	bool opensWithoutDialog(const QString &file_path, QString *reason = nullptr);

	/**
		A project open for the duration of a test case, closed when the case
		ends however it ends.
	*/
	class Project
	{
		public:
			explicit Project(const QString &example_file_name);
			~Project();

			Project(const Project &) = delete;
			Project &operator=(const Project &) = delete;

			bool isOpen() const;
			/// Empty while isOpen(); says what went wrong otherwise.
			QString error() const {return m_error;}

			QETProject *project() const {return m_project;}
			QETProject *operator->() const {return m_project;}

			QList<Diagram *> diagrams() const;
			Diagram *diagram(int index) const;
			int diagramCount() const;

			// The sheet a test should sample is not the first one, and not the
			// first one that draws either. Both were tried here, and both were
			// wrong, for two different reasons.
			//
			// The front sheets of a real project are not drawings: industrial.qet
			// has fifty sheets and its first three are a references page and two
			// folio lists. So diagram(0) samples an index page - no component, no
			// label - and reports a defect that is not there.
			//
			// "The first sheet carrying a component" does not fix it, because that
			// references page carries exactly one component and no label at all;
			// and sheet six of the same example draws eleven components with no
			// label either. A sheet can draw and still say nothing.
			//
			// What holds is the busiest sheet: the one with the most components.
			// It is the sheet the example has the most to say about (sheet 38 of
			// industrial.qet - 42 components, 42 labels), it is deterministic, and
			// it cannot land on an index page by construction. Ties go to the
			// first, so the answer does not move between runs.
			//
			// Returns nullptr when the project draws nothing at all, so a caller
			// that forgets to check fails on the spot rather than on the next line.
			Diagram *busiestSheet() const;

		private:
			QETProject *m_project = nullptr;
			QString m_error;
	};

	/// What the sheet draws for each element, already composed - not the stored value.
	QStringList displayedLabels(Diagram *diagram);

	/// The independent text fields of the sheet, sorted (they are held in a QSet).
	QStringList textFields(Diagram *diagram);

	/// One entry per element: the value it carries under key, empty string when absent.
	QStringList information(Diagram *diagram, const QString &key);

	/// The label of the command an undo would take back, empty when the stack is clean.
	QString undoTopText(QETProject *project);

	/// The sheet drawn into an image, for the appearance checks.
	QImage render(Diagram *diagram, int width = 1600);
}

#endif // UIBENCH_H
