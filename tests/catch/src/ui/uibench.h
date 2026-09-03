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
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

class Diagram;
class DiagramTextItem;
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

	/**
		The whole content of a file, as text. Handed to ScratchProject so that
		an example can be copied into a directory of its own before a test
		saves it.
	*/
	QString fileContent(const QString &path);

	/**
		A project written into a directory of its own, opened from there.

		Two things the Project above cannot do, and both are needed by a case
		that ends with "save, close, open again".

		The first is saving at all. Project opens a file of examples/, which
		is part of the source tree: writing it back would modify the tree, and
		a suite that does that is a suite nobody can run twice. Here the file
		lives in a QTemporaryDir that goes away with the test.

		The second is a project the test wrote itself. A drag of six push
		buttons in and out of a rectangle needs six push buttons in a
		rectangle, and no example ships one; the XML is built by the case,
		written here, and opened by the very same QETProject the program uses.

		The temporary directory is emptied when the object dies, whatever the
		case did with it.
	*/
	class ScratchProject
	{
		public:
			/**
				@param xml_content the whole .qet file, as text
				@param file_name the name it takes inside the temporary
				directory - only visible in a message, but a legible one
			*/
			explicit ScratchProject(const QString &xml_content,
						const QString &file_name
						= QStringLiteral("bench.qet"));
			~ScratchProject();

			ScratchProject(const ScratchProject &) = delete;
			ScratchProject &operator=(const ScratchProject &) = delete;

			bool isOpen() const;
			/// Empty while isOpen(); says what went wrong otherwise.
			QString error() const {return m_error;}
			QString filePath() const {return m_file_path;}

			QETProject *project() const {return m_project;}
			QETProject *operator->() const {return m_project;}

			QList<Diagram *> diagrams() const;
			Diagram *diagram(int index) const;
			int diagramCount() const;

			/**
				Save the project, close it, open the file again.

				Every pointer the case holds into the old project - a Diagram,
				an Element, a shape - dangles afterwards and has to be taken
				again from the reopened project. Said here because it is the
				one way to use this wrongly.

				@return false and fills error() when the save or the reopening
				failed.
			*/
			bool saveAndReopen();

		private:
			void open();

			QTemporaryDir m_dir;
			QString m_file_path;
			QString m_error;
			QETProject *m_project = nullptr;
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

	/*
		Appearance: what the sheet looks like, checked without a screen.

		Not by comparing pixels against a stored image, and the reason is worth
		writing down here rather than being rediscovered. The drawing font comes
		from QETApp::diagramTextsFont(), which reads it from the QSettings with
		"Liberation Sans" as its default, and nothing in this program registers a
		font of its own through QFontDatabase::addApplicationFont(). The text is
		therefore drawn with whatever the machine happens to have installed: the
		same sheet is a different image on two machines, and a reference image
		would go red for a reason that has nothing to do with the code it was put
		there to guard.

		What does not move when the font changes is geometry - the frame, the
		title block, the size a component takes from its own definition - and
		that is what the functions below measure. Ink is counted, never compared
		shape by shape, and only over things that are drawn as lines.
	*/

	/**
		The page: the rectangle the sheet frames, title block included, in scene
		coordinates. It is what a print or an export puts on paper - the export
		path computes the same rectangle, in diagramRect() of
		sources/cli_export.cpp, for that same reason.
	*/
	QRectF pageRect(Diagram *diagram);

	/// The border alone, row and column headers included, title block excluded.
	QRectF borderRect(Diagram *diagram);

	/// The drawing area: inside the border, headers excluded.
	QRectF drawingRect(Diagram *diagram);

	/// The title block alone. Empty when the sheet is drawn without one.
	QRectF titleBlockRect(Diagram *diagram);

	/// Every text the sheet draws: those carried by components and the free ones.
	QList<DiagramTextItem *> texts(Diagram *diagram);

	/**
		One line per component that is not entirely on the page, naming it and
		saying where it went. Empty when the sheet keeps everything on the page.

		A list and not a count, so that a failure names the component instead of
		sending whoever reads it back to the sheet to find out which one.

		Components only, deliberately: a component takes its size from its own
		definition, so the answer is the same on every machine. The text boxes
		are left out because their width comes from the font.
	*/
	QStringList elementsOffPage(Diagram *diagram);

	/// One line per component whose bounding rectangle has no surface at all.
	QStringList elementsWithoutSurface(Diagram *diagram);

	/**
		A sheet rendered at a known scale, so that a rectangle of the scene can
		be pointed at inside the image.

		render() above cannot do that: it scales the whole scene rectangle into
		an image whose proportions it does not control, and KeepAspectRatio then
		places the result somewhere inside - so where a given rectangle of the
		scene landed is not recoverable. Here the page is rendered on its own and
		the mapping is kept; map() gives it back.

		The grid and the guides are turned off for the duration, exactly as the
		export path does in renderDiagram() of sources/cli_export.cpp. They are
		on-screen comfort, not part of what the sheet says, and leaving them on
		would make "there is ink inside the title block" mean nothing more than
		"a grid dot fell there".
	*/
	class Rendering
	{
		public:
			Rendering() = default;
			explicit Rendering(Diagram *diagram, int width = 1200);

			bool isNull() const {return m_image.isNull();}
			const QImage &image() const {return m_image;}
			/// The part of the scene the image covers: pageRect() of the sheet.
			QRectF sceneRegion() const {return m_region;}

			/// Where a rectangle of the scene lands inside the image.
			QRect map(const QRectF &scene_rect) const;

			/// Pixels of the whole image that are not the background.
			int ink() const;
			/// Pixels that are not the background inside a rectangle of the scene.
			int ink(const QRectF &scene_rect) const;

		private:
			int inkOfImageRect(const QRect &image_rect) const;

			QImage m_image;
			QRectF m_region;
			qreal m_scale_x = 1.;
			qreal m_scale_y = 1.;
	};
}

#endif // UIBENCH_H
