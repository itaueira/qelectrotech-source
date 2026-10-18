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
#include "../qt_catch_tostring.h"
#include "uibench.h"

#include "../../../../sources/ElementsCollection/elementscollectionmodel.h"
#include "../../../../sources/ElementsCollection/elementslocation.h"
#include "../../../../sources/ElementsCollection/xmlelementcollection.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramevent/diagrameventaddelement.h"
#include "../../../../sources/diagramview.h"
#include "../../../../sources/factory/elementfactory.h"
#include "../../../../sources/factory/elementpicturefactory.h"
#include "../../../../sources/qetapp.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QDropEvent>
#include <QFile>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QMimeData>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPicture>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>

/*
	T61 - a symbol dropped on a folio becomes an element, whatever the
	number the project was given in the session.

	The element is built from the copy of the symbol in the project, found
	through a text of the form "project<id>+embed://import/...": the import
	writes it (ElementsLocation::projectCollectionPath()) and
	ElementsLocation::setPath() reads it back. The reading took one digit of
	id, and the id grows with every project opened, or opened again, in a
	session: from the eleventh on, the text read back as nothing, the build
	gave up without a word, and the definition stayed copied in the project
	with nothing on the folio.

	The project ids only grow in a process, and a case cannot choose the
	next one: it can only register its project again until it has the id
	it wants. So the cases are in the order of the ids they want - 0 to 9
	first, then 10 and 123 - and when an earlier case of the same run took
	an id already, a case checks the next free one and says so.

	What is not proved here: the sentence in the status bar. The bar
	belongs to QETDiagramEditor, a window this suite cannot build (see
	contextmenuassign_test.cpp). The reason it carries is checked instead
	through the function that words it and through the warning written to
	the log, which says the same reason.
*/

namespace
{
	const QString kFolder = QStringLiteral("t61");
	const QString kSettingsKey =
			QStringLiteral("elements-collections/company-collection-path");
	/// The rectangle of the symbol, around its hotspot
	const QRectF kSymbolRect(-10, -40, 50, 80);

	/// What keeps a symbol from becoming an element
	enum class Flaw {None, NoWidth, UnreadablePart};

	QString lineXml(int x1, int y1, int x2, int y2)
	{
		return QStringLiteral("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%4\""
				      " end1=\"none\" end2=\"none\" length1=\"1.5\" length2=\"1.5\""
				      " antialias=\"false\""
				      " style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>\n")
				.arg(x1).arg(y1).arg(x2).arg(y2);
	}

	QString terminalXml(const QUuid &symbol, const QString &name, int x, int y,
			    const QString &orientation)
	{
		return QStringLiteral("<terminal uuid=\"%1\" name=\"%2\" x=\"%3\" y=\"%4\""
				      " orientation=\"%5\" type=\"Generic\"/>\n")
				.arg(QUuid::createUuidV5(symbol, name).toString(), name)
				.arg(x).arg(y).arg(orientation);
	}

	/// A box, a wire from each of its two terminals to it
	QString symbolXml(const QUuid &uuid, Flaw flaw = Flaw::None)
	{
		QString parts;
		if (flaw == Flaw::UnreadablePart) {
				//An old text field without its position:
				//Element::parseInput() refuses it, and the build stops
				//there (state 7). Tagged "label", so that reading it does
				//not write into the definition
			parts += QStringLiteral("<input y=\"0\" size=\"9\" text=\"_\" tagg=\"label\"/>\n");
		}
		parts += QStringLiteral(
			"<rect x=\"-10\" y=\"-25\" width=\"40\" height=\"50\" rx=\"0\" ry=\"0\""
			" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>\n");
		parts += lineXml(0, -40, 0, -25);
		parts += lineXml(20, 25, 20, 40);
		parts += terminalXml(uuid, QStringLiteral("A1"), 0, -40, QStringLiteral("n"));
		parts += terminalXml(uuid, QStringLiteral("A2"), 20, 40, QStringLiteral("s"));

			//No width: the build stops before the drawing (state 5)
		const QString size = flaw == Flaw::NoWidth
				? QStringLiteral("height=\"80\"")
				: QStringLiteral("width=\"50\" height=\"80\"");
		return QStringLiteral(
			"<definition version=\"0.100.0\" type=\"element\" link_type=\"simple\""
			" %1 hotspot_x=\"10\" hotspot_y=\"40\">\n"
			"    <uuid uuid=\"%2\"/>\n"
			"    <names>\n"
			"        <name lang=\"en\">T61 symbol</name>\n"
			"    </names>\n"
			"    <informations>License: GPL-2.0-or-later</informations>\n"
			"    <description>\n%3    </description>\n"
			"</definition>\n").arg(size, uuid.toString(), parts);
	}

	QUuid symbolUuid(const QString &name)
	{
		return QUuid::createUuidV5(QUuid(QStringLiteral("{4f0c5e2a-61d1-4c39-8b7e-2e61a1c0f061}")),
					   name);
	}

	bool writeFile(const QString &path, const QString &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			return false;
		}
		return file.write(content.toUtf8()) > 0;
	}

	/// The company collection in a temporary directory, for one case
	class CompanyCollection
	{
		public:
			CompanyCollection()
			{
				QSettings settings;
				m_had_value = settings.contains(kSettingsKey);
				m_previous = settings.value(kSettingsKey);
				settings.setValue(kSettingsKey, m_dir.path());
				settings.sync();
				QETApp::resetCollectionsPath();
				QDir(m_dir.path()).mkpath(kFolder);
				writeFile(m_dir.path() + QStringLiteral("/") + kFolder
					  + QStringLiteral("/qet_directory"),
					  QStringLiteral("<qet-directory>\n    <names>\n"
							 "        <name lang=\"en\">T61</name>\n"
							 "    </names>\n</qet-directory>\n"));
			}
			~CompanyCollection()
			{
				QSettings settings;
				if (m_had_value) {
					settings.setValue(kSettingsKey, m_previous);
				} else {
					settings.remove(kSettingsKey);
				}
				settings.sync();
				QETApp::resetCollectionsPath();
			}

			CompanyCollection(const CompanyCollection &) = delete;
			CompanyCollection &operator=(const CompanyCollection &) = delete;

			bool isValid() const
			{
				return m_dir.isValid()
						&& QETApp::companyElementsDirN() == m_dir.path();
			}

			/// Writes the file, and gives the text the collection panel
			/// puts in the mime data of a drag ("company://t61/<file>")
			QString write(const QString &file_name, const QString &content) const
			{
				if (!writeFile(m_dir.path() + QStringLiteral("/") + kFolder
					       + QStringLiteral("/") + file_name, content)) {
					return QString();
				}
				return QStringLiteral("company://") + kFolder + QStringLiteral("/") + file_name;
			}

		private:
			QTemporaryDir m_dir;
			bool m_had_value = false;
			QVariant m_previous;
	};

	/// What QETDiagramEditor::addProject() does first: QETApp::registerProject()
	class Registration
	{
		public:
			explicit Registration(QETProject *project) : m_project(project)
			{
				QETApp::registerProject(m_project);
			}
			~Registration()
			{
				QETApp::unregisterProject(m_project);
			}

			Registration(const Registration &) = delete;
			Registration &operator=(const Registration &) = delete;

			int id() const {return QETApp::projectId(m_project);}

			/**
				Registers the project again until it has the id @p wanted,
				as closing it and opening it again in a session does. When
				an earlier case of the run took @p wanted already, the
				project stays with the next free id.
				@return the id the project has
			*/
			int reach(int wanted)
			{
				while (id() >= 0 && id() < wanted)
				{
					QETApp::unregisterProject(m_project);
					QETApp::registerProject(m_project);
				}
				return id();
			}

		private:
			QETProject *m_project;
	};

	QString emptyProjectXml()
	{
		return QStringLiteral(
			"<project title=\"bench\" version=\"0.80\">"
			"<collection><category name=\"import\"/></collection>"
			"<diagram title=\"Bench\" order=\"1\" height=\"600\""
			" cols=\"17\" colsize=\"60\" rows=\"8\" rowsize=\"80\""
			" displaycols=\"true\" displayrows=\"true\">"
			"<elements/><inputs/><conductors/>"
			"</diagram>"
			"</project>");
	}

	/**
		What the "reload collections" of the panel calls
		(ElementsCollectionWidget::reload()), without the widget: a new model
		loading the company collection and the project, waited for.
	*/
	bool reloadCollections(QETProject *project)
	{
		ElementsCollectionModel model;
		QSignalSpy finished(&model, &ElementsCollectionModel::loadingFinished);
		model.loadCollections(false, true, false, QList<QETProject *>{project});
		return finished.count() > 0 || finished.wait(30000);
	}

	/// A location in @p project, written as text, and what reading it back gives
	struct RoundTrip
	{
		QString text;
		QString read_back;
		bool same = false;
	};

	RoundTrip roundTrip(QETProject *project)
	{
		const ElementsLocation location(QStringLiteral("import/t61/round_trip.elmt"), project);
		RoundTrip r;
		r.text = location.projectCollectionPath();
		const ElementsLocation read_back(r.text);
		r.read_back = read_back.collectionPath();
		r.same = read_back == location;
		return r;
	}

	/// The embedded collection of a project, as text, to compare before and after
	QString collectionText(const XmlElementCollection *collection)
	{
		QString text;
		QTextStream stream(&text);
		collection->root().save(stream, 1);
		return text;
	}

	/// The DiagramEventAddElement a drop left installed on @p sheet
	int placementsOn(Diagram *sheet)
	{
		return sheet->findChildren<DiagramEventAddElement *>().size();
	}

	/// The drag of the collection panel entering the view and dropped on it
	void dropOnView(DiagramView &view, const QString &location_text, const QPoint &viewport_pos)
	{
		QMimeData mime;
		mime.setText(location_text);
		mime.setData(QStringLiteral("application/x-qet-element-uri"), location_text.toLatin1());
			//The drop only reaches the widget that accepted the drag entering
			//(QApplication::notify() hands it to QDragManager's current
			//target): the three events a drag is made of, in order
		QDragEnterEvent enter(viewport_pos, Qt::CopyAction, &mime,
				      Qt::LeftButton, Qt::NoModifier);
		QCoreApplication::sendEvent(view.viewport(), &enter);
		QDragMoveEvent move(viewport_pos, Qt::CopyAction, &mime,
				    Qt::LeftButton, Qt::NoModifier);
		QCoreApplication::sendEvent(view.viewport(), &move);
		QDropEvent drop(QPointF(viewport_pos), Qt::CopyAction, &mime,
				Qt::LeftButton, Qt::NoModifier);
		QCoreApplication::sendEvent(view.viewport(), &drop);
	}

	void moveOnScene(Diagram *sheet, const QPointF &scene_pos)
	{
		QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
		move.setScenePos(scene_pos);
		move.setPos(scene_pos);
		QCoreApplication::sendEvent(sheet, &move);
	}

	/// The cursor comes to @p scene_pos and clicks there
	void clickOnScene(Diagram *sheet, const QPointF &scene_pos)
	{
		moveOnScene(sheet, scene_pos);
		QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
		release.setButton(Qt::LeftButton);
		release.setScenePos(scene_pos);
		release.setPos(scene_pos);
		QCoreApplication::sendEvent(sheet, &release);
	}

	QImage blankImage()
	{
		QImage image(kSymbolRect.size().toSize() * 4 + QSize(8, 8), QImage::Format_ARGB32);
		image.fill(Qt::white);
		return image;
	}

	void placePainter(QPainter &painter)
	{
		painter.scale(4, 4);
		painter.translate(-kSymbolRect.topLeft() + QPointF(1, 1));
	}

	int inkOf(const QImage &image)
	{
		int ink = 0;
		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				if (image.pixel(x, y) != qRgb(255, 255, 255)) {
					++ink;
				}
			}
		}
		return ink;
	}

	/// The drawing the element paints itself; its terminals and texts are
	/// child items, left out
	QImage elementImage(Element *element)
	{
		QImage image = blankImage();
		QPainter painter(&image);
		placePainter(painter);
		element->setSelected(false);
		element->paint(&painter, nullptr, nullptr);
		painter.end();
		return image;
	}

	/// The drawing of a definition, built apart from any cache
	QImage referenceImage(const QString &xml)
	{
		QDomDocument document;
		document.setContent(xml);
		const QPicture picture = ElementPictureFactory::instance()->pictureFromDefinition(
					document.documentElement());
		QImage image = blankImage();
		QPainter painter(&image);
		placePainter(painter);
		painter.setPen(QPen());
		painter.setBrush(QBrush());
		if (!picture.isNull()) {
			painter.drawPicture(0, 0, picture);
		}
		painter.end();
		return image;
	}

	QMutex caught_mutex;
	QStringList caught_warnings;
	QtMessageHandler previous_handler = nullptr;

	void catchWarning(QtMsgType type, const QMessageLogContext &context, const QString &message)
	{
		if (type == QtWarningMsg)
		{
			QMutexLocker lock(&caught_mutex);
			caught_warnings << message;
		}
		if (previous_handler) {
			previous_handler(type, context, message);
		}
	}

	/// Every warning written while it lives, still handed on to the
	/// handler that was there before
	class WarningCatcher
	{
		public:
			WarningCatcher()
			{
				{
					QMutexLocker lock(&caught_mutex);
					caught_warnings.clear();
				}
				previous_handler = qInstallMessageHandler(catchWarning);
			}
			~WarningCatcher()
			{
				qInstallMessageHandler(previous_handler);
			}

			WarningCatcher(const WarningCatcher &) = delete;
			WarningCatcher &operator=(const WarningCatcher &) = delete;

			QStringList warnings() const
			{
				QMutexLocker lock(&caught_mutex);
				return caught_warnings;
			}
	};

	/// The warning that says no element was made of @p file_name, and @p reason
	bool saysNotCreated(const QStringList &warnings, const QString &file_name,
			    const QString &reason)
	{
		for (const QString &warning : warnings)
		{
			if (warning.contains(QStringLiteral("not created"))
				&& warning.contains(file_name)
				&& warning.contains(reason)) {
				return true;
			}
		}
		return false;
	}
}

	//First: the ids 0 to 9 are only free at the start of a run
TEST_CASE("T61 — A location in a project, written as text, reads back as itself: project ids 0 to 9",
	  "[t61][elementslocation]")
{
	UiBench::ScratchProject bench(emptyProjectXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	Registration registration(bench.project());
	const int first = registration.id();
	REQUIRE(first >= 0);
	if (first > 0) {
		WARN("ids below " << first << " were taken by an earlier case of this run: "
		     "checked from " << first);
	}

	for (int wanted = first; wanted <= qMax(9, first); ++wanted)
	{
		REQUIRE(registration.reach(wanted) == wanted);
		const RoundTrip r = roundTrip(bench.project());
		INFO("project id " << wanted << ": \"" << r.text.toStdString()
		     << "\" reads back as \"" << r.read_back.toStdString() << "\"");
		CHECK(r.same);
	}
}

TEST_CASE("CU-61.1 — A symbol just made in the company collection, collections reloaded, dropped on a "
	  "folio of a project numbered 10, then 123, is placed where it was dropped",
	  "[t61]")
{
	CompanyCollection company;
	REQUIRE(company.isValid());
	UiBench::ScratchProject bench(emptyProjectXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	Registration registration(project);
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet);
	DiagramView view(sheet);
	view.resize(1000, 800);

	int column = 0;
	for (const int wanted : {10, 123})
	{
		const int id = registration.reach(wanted);
		INFO("project id " << id);
		REQUIRE(id >= wanted);
		if (id != wanted) {
			WARN("id " << wanted << " was taken by an earlier case of this run: checked with " << id);
		}

			//A symbol never seen before, written now, the collections reloaded
		const QString file_name = QStringLiteral("new_%1.elmt").arg(wanted);
		const QString xml = symbolXml(symbolUuid(file_name));
		const QString location = company.write(file_name, xml);
		REQUIRE_FALSE(location.isEmpty());
		REQUIRE(ElementsLocation(location).exist());
		REQUIRE(reloadCollections(project));

			//The text the import writes for the copy in the project, read back
		const ElementsLocation in_project(QStringLiteral("import/t61/") + file_name, project);
		const QString text = in_project.projectCollectionPath();
		const ElementsLocation read_back(text);
		INFO("\"" << text.toStdString() << "\" reads back as \""
		     << read_back.collectionPath().toStdString() << "\"");
		CHECK(read_back == in_project);

		const QList<Element *> before = sheet->elements();
		const QPoint viewport_pos(300 + 300 * column++, 300);
		dropOnView(view, location, viewport_pos);

			//The symbol follows the cursor: a click places it
		REQUIRE(sheet->eventInterfaceIsRunning());
		const QPointF scene_pos = view.mapToScene(viewport_pos);
		clickOnScene(sheet, scene_pos);
		sheet->clearEventInterface();
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

		const QList<Element *> after = sheet->elements();
		REQUIRE(after.size() == before.size() + 1);
		Element *placed = nullptr;
		for (Element *element : after) {
			if (!before.contains(element)) {
				placed = element;
			}
		}
		REQUIRE(placed);

		const QPointF expected = Diagram::snapToGrid(scene_pos);
		INFO("placed at " << placed->pos().x() << ", " << placed->pos().y()
		     << "; dropped at " << expected.x() << ", " << expected.y());
		CHECK(placed->pos() == expected);
		CHECK(placed->location() == in_project);

		int state = -1;
		delete ElementFactory::Instance()->createElement(ElementsLocation(text), nullptr, &state);
		CHECK(state == 0);

		CHECK(placed->terminals().size() == 2);
		const QImage image = elementImage(placed);
		CHECK(inkOf(image) > 0);
		CHECK(image == referenceImage(xml));
	}
}

TEST_CASE("T61 — Every way a symbol can fail to become an element has its own reason",
	  "[t61]")
{
	const QList<int> states {DiagramEventAddElement::ImportFailed, 1, 4, 5, 6, 7, 8};
	QStringList reasons;
	for (const int state : states)
	{
		const QString reason = DiagramEventAddElement::failureReason(state);
		INFO("state " << state << ": \"" << reason.toStdString() << "\"");
		CHECK_FALSE(reason.trimmed().isEmpty());
		CHECK_FALSE(reasons.contains(reason));
		reasons << reason;
	}

		//A state nobody foresaw is still named, by its number
	CHECK(DiagramEventAddElement::failureReason(42).contains(QStringLiteral("42")));
}

namespace
{
	struct Unbuildable
	{
		const char *file_name;
		Flaw flaw;
		bool xml;
		int state;
	};
}

TEST_CASE("T61 — A symbol that cannot become an element, dropped on a folio: nothing is placed, "
	  "the log says why, and the project is left as it was",
	  "[t61]")
{
	const Unbuildable symbol = GENERATE(
		Unbuildable{"no_width.elmt", Flaw::NoWidth, true, 5},
		Unbuildable{"unreadable_part.elmt", Flaw::UnreadablePart, true, 7},
		Unbuildable{"not_xml.elmt", Flaw::None, false, DiagramEventAddElement::ImportFailed});
	const QString file_name = QString::fromLatin1(symbol.file_name);
	INFO(symbol.file_name);

	CompanyCollection company;
	REQUIRE(company.isValid());
	UiBench::ScratchProject bench(emptyProjectXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	Registration registration(project);
	REQUIRE(registration.id() >= 0);
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet);
	DiagramView view(sheet);
	view.resize(1000, 800);

	const QString content = symbol.xml
			? symbolXml(symbolUuid(file_name), symbol.flaw)
			: QStringLiteral("not a symbol\n");
	const QString location = company.write(file_name, content);
	REQUIRE_FALSE(location.isEmpty());
	REQUIRE(ElementsLocation(location).exist());
	REQUIRE(reloadCollections(project));

	XmlElementCollection *collection = project->embeddedElementCollection();
	const QString collection_before = collectionText(collection);
	const int elements_before = sheet->elements().size();

	QStringList warnings;
	{
		WarningCatcher catcher;
		dropOnView(view, location, QPoint(300, 300));
		warnings = catcher.warnings();
	}

		//Nothing follows the cursor, nothing is on the folio
	CHECK_FALSE(sheet->eventInterfaceIsRunning());
	CHECK(sheet->elements().size() == elements_before);

		//Nothing half done in the project: not the definition, not its folder
	CHECK(collectionText(collection) == collection_before);
	CHECK_FALSE(collection->exist(QStringLiteral("import/t61/") + file_name));
	CHECK_FALSE(collection->exist(QStringLiteral("import/t61")));

		//The log says that no element was made, of which symbol, and why
	const QString reason = DiagramEventAddElement::failureReason(symbol.state);
	INFO("warnings: " << warnings.join(QStringLiteral(" | ")).toStdString());
	CHECK(saysNotCreated(warnings, file_name, reason));

		//And the folio is not held: the next mouse event ends what the
		//drop left installed, and goes on to the folio
	CHECK(placementsOn(sheet) == 1);
	moveOnScene(sheet, view.mapToScene(QPoint(320, 320)));
	CHECK(placementsOn(sheet) == 0);
}

TEST_CASE("T61 — A symbol the project already holds, that cannot become an element: its definition "
	  "stays in the project",
	  "[t61]")
{
	CompanyCollection company;
	REQUIRE(company.isValid());
	UiBench::ScratchProject bench(emptyProjectXml());
	INFO(bench.error().toStdString());
	REQUIRE(bench.isOpen());
	QETProject *project = bench.project();
	Registration registration(project);
	REQUIRE(registration.id() >= 0);
	Diagram *sheet = bench.diagram(0);
	REQUIRE(sheet);
	DiagramView view(sheet);
	view.resize(1000, 800);

	const QString file_name = QStringLiteral("held_no_width.elmt");
	const QString location = company.write(file_name,
					       symbolXml(symbolUuid(file_name), Flaw::NoWidth));
	REQUIRE_FALSE(location.isEmpty());

		//In the project before the drop: the drop only finds it there
	ElementsLocation source(location);
	REQUIRE(project->importElement(source).exist());
	XmlElementCollection *collection = project->embeddedElementCollection();
	const QString embedded = QStringLiteral("import/t61/") + file_name;
	REQUIRE(collection->exist(embedded));
	const QString collection_before = collectionText(collection);

	QStringList warnings;
	{
		WarningCatcher catcher;
		dropOnView(view, location, QPoint(300, 300));
		warnings = catcher.warnings();
	}

	CHECK_FALSE(sheet->eventInterfaceIsRunning());
	CHECK(sheet->elements().isEmpty());
	CHECK(collection->exist(embedded));
	CHECK(collectionText(collection) == collection_before);
	INFO("warnings: " << warnings.join(QStringLiteral(" | ")).toStdString());
	CHECK(saysNotCreated(warnings, file_name, DiagramEventAddElement::failureReason(5)));

	moveOnScene(sheet, view.mapToScene(QPoint(320, 320)));
	CHECK(placementsOn(sheet) == 0);
}
