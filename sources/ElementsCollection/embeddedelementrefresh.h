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
#ifndef EMBEDDEDELEMENTREFRESH_H
#define EMBEDDEDELEMENTREFRESH_H

#include <QDomDocument>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include "../qetgraphicsitem/symboltextsync.h"

class Element;
class ElementsLocation;
class QETProject;
class QUndoCommand;

/**
	@brief The EmbeddedElementRefresh namespace
	Brings into a project the symbols changed in the collections the user
	can edit, and tells whoever saves a symbol that the folios have not
	changed yet.

	A project keeps its own copy of every symbol it uses, under
	import/<the path of the symbol in its collection> (see
	QETProject::importElement()), and a placed element is drawn once, from
	that copy, when it is built. So saving a symbol of the company or user
	collection changes nothing in an open project, and saving the project's
	own copy changes nothing on the folios until their drawings are reloaded.

	Only the editable collections are looked at, company and user. The QET
	collection is never writable (ElementsLocation::isWritable()), so nothing
	anyone corrected lives there: what changes there is an update of the
	program, and bringing it in would change, in a project already issued,
	symbols nobody asked to change.

	The directories of the collections are a parameter rather than a read of
	QETApp, so that a test can hand temporary ones.

	Nothing has to be removed and inserted again. A new version that changes
	only the drawing, the size or the hotspot is applied in place, undoably:
	a terminal is placed relative to the hotspot, so no conductor moves. A
	new version that adds, removes or moves a terminal is applied by
	reloading the project the way opening it does (reopened()), from what is
	in memory and without saving: a conductor follows its terminal by uuid,
	and one whose terminal is gone is left out and listed.

	What changed is told by the content of the definitions, every uuid left
	out (ElementOrigin::contentHash()), and by the uuid only when the
	content the copy was taken with is not known: a symbol made again by a
	generator keeps its uuid on purpose, and one saved without a change gets
	a new one. A terminal renamed or turned, or another kind of element,
	reloads the project like a terminal moved: none of it can change on an
	element already built. A symbol moved to another directory of its
	collection is found by its file name and the uuids of its parts, when
	its old path is in no collection at all. The texts of the placed
	elements follow their symbol too (textPlans(), SymbolTextSync).

	The same reload puts the terminals of the project's own copies on whole
	numbers, when they are off (reopenedOnWholeNumbers(), SymbolTerminals).
*/
namespace EmbeddedElementRefresh
{
	/// What the editable collections hold for one symbol placed in a project
	enum class Outcome {
		NoSource,         ///< the path is in none of the editable collections
		Ambiguous,        ///< the path is in more than one: its origin cannot be told
		UpToDate,         ///< the same definition (same uuid) as the project's copy
		Pending,          ///< a newer definition with the terminals of every placed element
		TerminalsChanged, ///< a newer definition changing a terminal, or the kind of element
		ChangedInProject  ///< a newer definition, and the copy was saved again too
	};

	/// One symbol placed in a project, and what the collections hold for it
	struct Entry
	{
		QString embedded_path;     ///< "import/..." in the project's collection
		QStringList source_files;  ///< absolute paths of the files found for it
		QList<Element *> placed;   ///< the elements drawn from that copy, none for a copy kept only
		Outcome outcome = Outcome::NoSource;
		QDomDocument source;       ///< the collection's definition, when newer
		bool moved = false;        ///< found in another directory than the copy's path says
		bool terminals_changed = false; ///< source adds, removes, moves or changes a terminal, or the kind
	};

	/// What bringFromCollections() did, for the message shown afterwards
	struct Report
	{
		int brought_symbols = 0;       ///< definitions replaced in the project
		int brought_elements = 0;      ///< placed elements redrawn from them
		QStringList ambiguous;         ///< one line per symbol, its path
		QStringList changed_in_project;///< one line per symbol, its path
		QStringList terminals_changed; ///< one line per placed element left as it was, its folio
		QStringList moved;             ///< one line per symbol brought from another directory
		int text_elements = 0;         ///< placed elements whose texts changed
		int added_texts = 0;
		int changed_texts = 0;
		int removed_texts = 0;
	};

	QStringList editableCollectionDirs();
	QStringList sourceFilesFor(const QString &embedded_path,
				   const QStringList &collection_dirs);
	QList<Element *> placedElements(QETProject *project,
					const QString &embedded_path);
	QString collectionRelativePath(const QString &embedded_path);
	QList<Entry> survey(QETProject *project,
			    const QStringList &collection_dirs,
			    const QString &common_dir = QString(),
			    const QString &only_path = QString());
	QStringList changedInBoth(QETProject *project,
				  const QStringList &collection_dirs,
				  const QString &only_path = QString());
	QStringList needingReload(QETProject *project,
				  const QStringList &collection_dirs,
				  const QSet<QString> &taken_from_collection = QSet<QString>(),
				  const QString &only_path = QString());
	Report bringFromCollections(QETProject *project,
				    const QStringList &collection_dirs,
				    bool with_terminal_changes = false,
				    const QSet<QString> &taken_from_collection = QSet<QString>(),
				    const QString &only_path = QString());
	QUndoCommand *updateCommand(QETProject *project,
				    const QStringList &collection_dirs,
				    const QSet<QString> &taken_from_collection,
				    Report *report,
				    const QString &only_path = QString());
	QList<SymbolTextSync::Plan> textPlans(QETProject *project,
					      SymbolTextSync::Mode mode = SymbolTextSync::Mode::Merge,
					      const QList<Element *> &elements = QList<Element *>());
	int adoptBoxes(QETProject *project, const QString &only_path = QString());
	QETProject *reopened(QETProject *project, QString *error = nullptr);

	/// The copies of a project whose terminals are off whole numbers (T72)
	struct OffWholeNumbers
	{
		QStringList copies;  ///< the copies to put on whole numbers, by path
		int terminals = 0;   ///< their terminals off whole numbers
		QStringList merged;  ///< copies left as they are: two of their terminals would meet
		int placed = 0;      ///< the elements placed from the copies to put on whole numbers
	};
	OffWholeNumbers offWholeNumbers(QETProject *project);
	QETProject *reopenedOnWholeNumbers(QETProject *project, QString *error = nullptr);

	QStringList wiresLeftOut(QETProject *project);
	QString reloadQuestion(int elements);
	QString reloadReportText(const Report &report, int wires_left_out);
	QString reportText(const Report &report);
	QString textsText(const Report &report);
	QStringList reportDetails(const Report &report);
	QString describe(const Element *element);
	QString savedNotice(const ElementsLocation &saved,
			    const QList<QETProject *> &projects,
			    const QStringList &collection_dirs);
	QList<QETProject *> projectsPlacing(const ElementsLocation &saved,
					    const QList<QETProject *> &projects);

	/// What "Recharger maintenant" of the notice looks at (savedScope())
	struct Scope
	{
		QStringList collection_dirs; ///< the collections to bring a newer version from
		QString only_path;           ///< the copies to look at, "import/..."
	};
	Scope savedScope(const ElementsLocation &saved,
			 const QStringList &collection_dirs);

	QList<Element *> keptOnFolio(QETProject *project, const QString &embedded_path);
	QUndoCommand *bringKeptCommand(QETProject *project, const QList<Element *> &elements);
	bool turnsTextsByCorner(const QList<Element *> &elements);
	QString noticeFor(const QString &embedded_path,
			  const QDomElement &definition,
			  const QList<QETProject *> &projects,
			  bool from_collection,
			  const QStringList &collection_dirs);
}

#endif // EMBEDDEDELEMENTREFRESH_H
