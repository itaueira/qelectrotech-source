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
#include "embeddedelementrefresh.h"

#include "../diagram.h"
#include "../diagramcontent.h"
#include "../qet.h"
#include "../qetapp.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"
#include "../undocommand/refreshembeddedelementscommand.h"
#include "../undocommand/syncsymboltextscommand.h"
#include "elementorigin.h"
#include "elementslocation.h"
#include "xmlelementcollection.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUuid>

#include <functional>
#include <utility>

namespace {

const QString import_prefix = QStringLiteral("import/");

	//The <definition> of an element file, or a null document
QDomDocument readDefinition(const QString &file_path)
{
	QFile file(file_path);
	QDomDocument document;
	if (!file.open(QIODevice::ReadOnly) || !document.setContent(&file)) {
		return QDomDocument();
	}
	if (document.documentElement().tagName() != QLatin1String("definition")) {
		return QDomDocument();
	}
	return document;
}

	//Whether the copy at @p copy was saved again since it was taken
bool changedInProject(const ElementsLocation &copy)
{
	const QDomElement definition = copy.xml();
	return ElementOrigin::definitionUuid(definition)
			!= ElementOrigin::of(definition.parentNode().toElement());
}

	//Every copy of @p project's collection under import/, by its path
QList<QPair<QString, QDomElement>> importedCopies(QETProject *project)
{
	QList<QPair<QString, QDomElement>> copies;
	if (!project || !project->embeddedElementCollection()) {
		return copies;
	}
	std::function<void(const QDomElement &, const QString &)> walk =
			[&](const QDomElement &directory, const QString &path) {
		for (QDomElement child = directory.firstChildElement() ;
			 !child.isNull() ;
			 child = child.nextSiblingElement())
		{
			const QString child_path = path.isEmpty()
					? child.attribute(QStringLiteral("name"))
					: path + QLatin1Char('/') + child.attribute(QStringLiteral("name"));
			if (child.tagName() == QLatin1String("category")) {
				walk(child, child_path);
			}
			else if (child.tagName() == QLatin1String("element")
					 && child_path.startsWith(import_prefix)) {
				copies << qMakePair(child_path, child);
			}
		}
	};
	walk(project->embeddedElementCollection()->root(), QString());
	return copies;
}

	//The uuids of the terminals and texts of @p definition, which the symbol
	//editor and the generator of symbols keep from one save to the next
QSet<QString> partUuids(const QDomElement &definition)
{
	QSet<QString> uuids;
	const QDomElement description = definition.firstChildElement(QStringLiteral("description"));
	for (QDomElement part = description.firstChildElement() ;
		 !part.isNull() ;
		 part = part.nextSiblingElement())
	{
		if ((part.tagName() == QLatin1String("terminal")
			 || part.tagName() == QLatin1String("dynamic_text"))
				&& !part.attribute(QStringLiteral("uuid")).isEmpty()) {
			uuids.insert(QUuid(part.attribute(QStringLiteral("uuid"))).toString());
		}
	}
	return uuids;
}

	//Every element file of @p collection_dirs, by file name
QHash<QString, QStringList> filesByName(const QStringList &collection_dirs)
{
	QHash<QString, QStringList> files;
	for (const QString &dir : collection_dirs)
	{
		if (dir.isEmpty() || !QDir(dir).exists()) {
			continue;
		}
		QDirIterator it(dir, {QStringLiteral("*.elmt")}, QDir::Files,
						QDirIterator::Subdirectories);
		while (it.hasNext())
		{
			const QString file = QDir::cleanPath(it.next());
			QStringList &same_name = files[QFileInfo(file).fileName()];
			if (!same_name.contains(file)) {
				same_name << file;
			}
		}
	}
	return files;
}

	//The files of @p files that may be the symbol of the copy at
	//@p embedded_path moved to another directory: the same file name, and a
	//terminal or a text in common with the copy
QStringList movedSources(const QString &embedded_path,
						 const QDomElement &copy_definition,
						 const QHash<QString, QStringList> &files)
{
	QStringList found;
	const QSet<QString> copy_parts = partUuids(copy_definition);
	if (copy_parts.isEmpty()) {
		return found;
	}
	for (const QString &file : files.value(QFileInfo(embedded_path).fileName()))
	{
		const QDomDocument candidate = readDefinition(file);
		if (!candidate.isNull()
				&& partUuids(candidate.documentElement()).intersects(copy_parts)) {
			found << file;
		}
	}
	return found;
}

	//Every placed element of @p project, grouped by the path of its copy
QHash<QString, QList<Element *>> placedByPath(QETProject *project)
{
	QHash<QString, QList<Element *>> placed;
	if (!project) {
		return placed;
	}
	for (Diagram *diagram : project->diagrams())
	{
		const DiagramContent content(diagram, false);
		for (Element *element : content.m_elements)
		{
			const ElementsLocation location = element->location();
			if (location.project() == project) {
				placed[location.collectionPath(false)] << element;
			}
		}
	}
	return placed;
}

}

/**
	@brief EmbeddedElementRefresh::editableCollectionDirs
	@return the directories of the company and the user collections, the
	two a user can save a symbol into
*/
QStringList EmbeddedElementRefresh::editableCollectionDirs()
{
	return {QETApp::companyElementsDir(), QETApp::customElementsDir()};
}

/**
	@brief EmbeddedElementRefresh::sourceFilesFor
	The files of @p collection_dirs a project's copy may come from.

	A copy imported from a collection keeps the path the symbol had there,
	under import/, and nothing records which collection it was: so every
	directory holding that path is a candidate, and more than one means the
	origin cannot be told. A directory given twice counts once.
	@param embedded_path : path of the copy in the project, "import/..."
	@param collection_dirs : root directories of the collections
	@return the absolute paths of the files found
*/
QStringList EmbeddedElementRefresh::sourceFilesFor(
		const QString &embedded_path,
		const QStringList &collection_dirs)
{
	QStringList found;
	if (!embedded_path.startsWith(import_prefix)) {
		return found;
	}

		//A copy with import/ twice is looked for under the second import/
		//too, the way the project wrote it, and without it
	QStringList relatives = {embedded_path.mid(import_prefix.size())};
	if (!relatives.contains(collectionRelativePath(embedded_path))) {
		relatives << collectionRelativePath(embedded_path);
	}
	for (const QString &dir : collection_dirs)
	{
		if (dir.isEmpty()) {
			continue;
		}
		for (const QString &relative : std::as_const(relatives))
		{
			const QString path = QDir::cleanPath(QDir(dir).absoluteFilePath(relative));
			if (QFileInfo(path).isFile() && !found.contains(path)) {
				found << path;
			}
		}
	}
	return found;
}

/**
	@brief EmbeddedElementRefresh::collectionRelativePath
	@return the path the symbol of the copy at @p embedded_path has in its
	collection: the path of the copy without the import/ a project puts
	before it - twice or more for a symbol placed from another project's own
	collection, which imports the import/ of that project too
*/
QString EmbeddedElementRefresh::collectionRelativePath(const QString &embedded_path)
{
	QString relative = embedded_path;
	while (relative.startsWith(import_prefix)) {
		relative = relative.mid(import_prefix.size());
	}
	return relative;
}

/**
	@brief EmbeddedElementRefresh::placedElements
	@return the elements of @p project drawn from its copy at
	@p embedded_path
*/
QList<Element *> EmbeddedElementRefresh::placedElements(
		QETProject *project,
		const QString &embedded_path)
{
	return placedByPath(project).value(embedded_path);
}

/**
	@brief EmbeddedElementRefresh::survey
	For each copy of a collection symbol @p project holds, what
	@p collection_dirs hold for it. A copy placed on no folio is surveyed
	too: whoever places it later from the project's own collection gets it.

	Whether a file is a newer version is told by content, every uuid left
	out (ElementOrigin::contentHash()): a file with the content of the copy,
	or with the content the copy was taken with, is not newer. Only when the
	copy was changed from a folio before its content was recorded does the
	uuid tell, the way it always did.

	A copy whose path is in none of @p collection_dirs, nor in the
	collection of QElectroTech at @p common_dir, may be a symbol moved to
	another directory: a file with the same name sharing a terminal or a
	text with the copy is taken for it. The QElectroTech collection is
	excluded because a symbol saved from one of its symbols into the company
	collection shares its parts too, and is another symbol.
	@param project : the project
	@param collection_dirs : the editable collections
	@param common_dir : the QElectroTech collection, by default the program's
	@return one entry per copy, in no particular order
*/
QList<EmbeddedElementRefresh::Entry> EmbeddedElementRefresh::survey(
		QETProject *project,
		const QStringList &collection_dirs,
		const QString &common_dir)
{
	QList<Entry> entries;
	const QHash<QString, QList<Element *>> placed = placedByPath(project);
	const QString common = common_dir.isEmpty() ? QETApp::commonElementsDir() : common_dir;
	QHash<QString, QStringList> files;
	bool files_listed = false;

	for (const auto &copy : importedCopies(project))
	{
		Entry entry;
		entry.embedded_path = copy.first;
		entry.placed = placed.value(copy.first);
		entry.source_files = sourceFilesFor(entry.embedded_path, collection_dirs);

		const QDomElement copy_definition =
				copy.second.firstChildElement(QStringLiteral("definition"));
		if (copy_definition.isNull()) {
			continue;
		}

		if (entry.source_files.isEmpty()
				&& sourceFilesFor(entry.embedded_path, {common}).isEmpty())
		{
			if (!files_listed) {
				files = filesByName(collection_dirs);
				files_listed = true;
			}
			entry.source_files = movedSources(entry.embedded_path, copy_definition, files);
			entry.moved = !entry.source_files.isEmpty();
		}

		const QUuid copy_uuid = ElementOrigin::definitionUuid(copy_definition);
		const QUuid origin = ElementOrigin::of(copy.second);
		const QString copy_hash = ElementOrigin::contentHash(copy_definition);
		const QString origin_hash = ElementOrigin::hashOf(copy.second);
		const bool copy_changed = origin_hash.isEmpty() ? copy_uuid != origin
														: copy_hash != origin_hash;

		QList<QDomDocument> candidates;
		bool unchanged = false;
		for (const QString &file : std::as_const(entry.source_files))
		{
			const QDomDocument candidate = readDefinition(file);
			if (candidate.isNull()) {
				continue;
			}
			const QString hash = ElementOrigin::contentHash(candidate.documentElement());
			const QUuid uuid = ElementOrigin::definitionUuid(candidate.documentElement());
			if (hash == copy_hash
					|| (!origin_hash.isEmpty() && hash == origin_hash)
					|| (origin_hash.isEmpty() && (uuid == origin || uuid == copy_uuid))) {
				unchanged = true;
			}
			candidates << candidate;
		}

		if (candidates.isEmpty()) {
			entry.outcome = Outcome::NoSource;
		}
		else if (unchanged) {
			entry.outcome = Outcome::UpToDate;
		}
		else if (candidates.size() > 1) {
			entry.outcome = Outcome::Ambiguous;
		}
		else
		{
			entry.source = candidates.first();
			const QDomElement definition = entry.source.documentElement();
			for (const Element *element : std::as_const(entry.placed)) {
				if (!element->definitionTerminalsMatch(definition)
						|| !element->definitionKindMatches(definition)) {
					entry.terminals_changed = true;
					break;
				}
			}
			if (copy_changed) {
				entry.outcome = Outcome::ChangedInProject;
			}
			else {
				entry.outcome = entry.terminals_changed ? Outcome::TerminalsChanged
														: Outcome::Pending;
			}
		}
		entries << entry;
	}
	return entries;
}

/**
	@brief EmbeddedElementRefresh::changedInBoth
	@return the paths of the copies of @p project changed both in the
	project and in a collection: the user is the one to say which version to
	keep
*/
QStringList EmbeddedElementRefresh::changedInBoth(QETProject *project,
												  const QStringList &collection_dirs)
{
	QStringList paths;
	if (!project || project->isReadOnly()) {
		return paths;
	}
	for (const Entry &entry : survey(project, collection_dirs)) {
		if (entry.outcome == Outcome::ChangedInProject) {
			paths << entry.embedded_path;
		}
	}
	paths.sort();
	return paths;
}

namespace {

	//SymbolTextSync::consensus() for the elements of each copy placed twice
	//or more, by the path of the copy
QHash<QString, SymbolTextSync::Consensus> consensusByCopy(const QList<Element *> &elements)
{
	QHash<QString, QList<Element *>> by_copy;
	for (Element *element : elements) {
		by_copy[element->location().collectionPath(false)] << element;
	}
	QHash<QString, SymbolTextSync::Consensus> common;
	for (auto it = by_copy.cbegin() ; it != by_copy.cend() ; ++it) {
		if (it.value().size() >= 2) {
			common.insert(it.key(), SymbolTextSync::consensus(it.value()));
		}
	}
	return common;
}

	//What bringFromCollections() and updateCommand() bring: the changes for
	//RefreshEmbeddedElementsCommand, and the report of what is left
EmbeddedElementRefresh::Report collect(
		QETProject *project,
		const QStringList &collection_dirs,
		bool with_terminal_changes,
		const QSet<QString> &taken_from_collection,
		QList<RefreshEmbeddedElementsCommand::Change> &changes)
{
	using namespace EmbeddedElementRefresh;
	Report report;
	if (!project || project->isReadOnly()) {
		return report;
	}

	for (Entry entry : survey(project, collection_dirs))
	{
		if (entry.outcome == Outcome::ChangedInProject
				&& taken_from_collection.contains(entry.embedded_path)) {
			entry.outcome = entry.terminals_changed ? Outcome::TerminalsChanged
													: Outcome::Pending;
		}

		switch (entry.outcome)
		{
			case Outcome::TerminalsChanged:
				if (!with_terminal_changes) {
					for (const Element *element : entry.placed) {
						report.terminals_changed << describe(element);
					}
					break;
				}
				Q_FALLTHROUGH();
			case Outcome::Pending:
				changes << RefreshEmbeddedElementsCommand::Change{entry.embedded_path,
																 entry.source};
				report.brought_elements += entry.placed.size();
				if (entry.moved && !entry.source_files.isEmpty()) {
					report.moved << QStringLiteral("%1 → %2")
							.arg(collectionRelativePath(entry.embedded_path),
								 QDir::toNativeSeparators(entry.source_files.first()));
				}
				break;
			case Outcome::Ambiguous:
					//A copy placed nowhere is not worth a line
				if (!entry.placed.isEmpty()) {
					report.ambiguous << QStringLiteral("%1 (%2)")
							.arg(entry.placed.first()->name(),
								 collectionRelativePath(entry.embedded_path));
				}
				break;
			case Outcome::ChangedInProject:
				if (!entry.placed.isEmpty()) {
					report.changed_in_project << QStringLiteral("%1 (%2)")
							.arg(entry.placed.first()->name(),
								 collectionRelativePath(entry.embedded_path));
				}
				break;
			case Outcome::NoSource:
			case Outcome::UpToDate:
				break;
		}
	}

	report.brought_symbols = changes.size();
	report.ambiguous.sort();
	report.changed_in_project.sort();
	report.terminals_changed.sort();
	report.moved.sort();
	return report;
}

}

/**
	@brief EmbeddedElementRefresh::bringFromCollections
	Replace, in @p project, the copies of the symbols that @p collection_dirs
	hold a newer version of, and redraw the elements placed from them.

	All of it goes on the project's undo stack as one command, which also
	marks the project as modified: saving keeps the new versions. Nothing
	is pushed when there is nothing to bring, and nothing at all is done in
	a read-only project.

	A symbol whose new version adds, removes or moves a terminal is brought
	only when @p with_terminal_changes says so, which is when the project is
	about to be reloaded (see reopened()). Otherwise it is left as it is and
	listed: the folio would stay right until the project is opened again,
	and the elements would then be built from a copy whose terminals differ
	from the ones the folio shows.

	A copy changed in the project and in a collection is brought when its
	path is in @p taken_from_collection, which is the user's answer; it is
	left and listed otherwise.
	@return what was done and what was left, for the message
*/
EmbeddedElementRefresh::Report EmbeddedElementRefresh::bringFromCollections(
		QETProject *project,
		const QStringList &collection_dirs,
		bool with_terminal_changes,
		const QSet<QString> &taken_from_collection)
{
	QList<RefreshEmbeddedElementsCommand::Change> changes;
	const Report report = collect(project, collection_dirs, with_terminal_changes,
								  taken_from_collection, changes);
	if (!changes.isEmpty()) {
		project->undoStack()->push(new RefreshEmbeddedElementsCommand(project, changes));
	}
	return report;
}

/**
	@brief EmbeddedElementRefresh::updateCommand
	Everything "Reload the drawings of the elements" does in place, as one
	command for one Ctrl+Z: the newer versions brought from the collections,
	as bringFromCollections() would bring them, and the texts of every placed
	element brought in line with its symbol (SymbolTextSync, in Merge).

	The texts are planned against the versions about to be brought, before
	anything is changed: the command brings them first and changes the
	texts after.
	@param project : the project
	@param collection_dirs : the editable collections
	@param taken_from_collection : the copies changed in the project and in
	a collection whose collection version the user chose
	@param report : filled with what is brought and what is left, and with
	the count of texts
	@return the command, to push; nullptr when there is nothing to do
*/
QUndoCommand *EmbeddedElementRefresh::updateCommand(
		QETProject *project,
		const QStringList &collection_dirs,
		const QSet<QString> &taken_from_collection,
		Report *report)
{
	QList<RefreshEmbeddedElementsCommand::Change> changes;
	const Report brought = collect(project, collection_dirs, false,
								   taken_from_collection, changes);
	if (report) {
		*report = brought;
	}
	if (!project || project->isReadOnly()) {
		return nullptr;
	}

	QHash<QString, QDomElement> brought_definitions;
	for (const auto &change : std::as_const(changes)) {
		brought_definitions.insert(change.embedded_path, change.definition.documentElement());
	}

	QList<Element *> placed;
	for (Diagram *diagram : project->diagrams()) {
		placed << DiagramContent(diagram, false).m_elements;
	}
	const QHash<QString, SymbolTextSync::Consensus> common = consensusByCopy(placed);

	QList<SymbolTextSync::Plan> plans;
	for (Element *element : std::as_const(placed))
	{
		const QString path = element->location().collectionPath(false);
		const QDomElement definition = brought_definitions.contains(path)
				? brought_definitions.value(path)
				: element->location().xml();
		if (!definition.isNull()) {
			plans << SymbolTextSync::plan(element, definition,
										  SymbolTextSync::Mode::Merge, common.value(path));
		}
	}

	auto *update = new QUndoCommand(
				QCoreApplication::translate("EmbeddedElementRefresh",
											"Mettre à jour les éléments depuis leurs symboles"));
	if (!changes.isEmpty()) {
		new RefreshEmbeddedElementsCommand(project, changes, update);
	}
	auto *texts = new SyncSymbolTextsCommand(plans, update);
	if (report) {
		report->text_elements = texts->elements();
		report->added_texts = texts->addedTexts();
		report->changed_texts = texts->changedTexts();
		report->removed_texts = texts->removedTexts();
	}
	if (changes.isEmpty() && texts->isEmpty())
	{
		delete update;
		return nullptr;
	}
	return update;
}

/**
	@brief EmbeddedElementRefresh::needingReload
	The placed elements of @p project that only a reload can bring up to
	date: those of a symbol @p collection_dirs hold a newer version of that
	adds, removes or moves a terminal, and those whose own copy in the
	project has other terminals than they have - a copy corrected from a
	folio, which the redraw cannot apply either. "Terminals" here also
	stands for a terminal renamed, turned or given another type, and for
	another kind of element. A symbol changed in the project and in a
	collection counts when the user took the collection's version, in
	@p taken_from_collection.
	@return one line per element, with its folio; empty when nothing needs
	the project to be reloaded, or when it is read-only
*/
QStringList EmbeddedElementRefresh::needingReload(
		QETProject *project,
		const QStringList &collection_dirs,
		const QSet<QString> &taken_from_collection)
{
	QStringList lines;
	if (!project || project->isReadOnly()) {
		return lines;
	}

	QSet<const Element *> listed;
	for (const Entry &entry : survey(project, collection_dirs))
	{
		const bool taken = entry.outcome == Outcome::ChangedInProject
				&& entry.terminals_changed
				&& taken_from_collection.contains(entry.embedded_path);
		if (entry.outcome != Outcome::TerminalsChanged && !taken) {
			continue;
		}
		for (const Element *element : entry.placed) {
			lines << describe(element);
			listed << element;
		}
	}

	for (Diagram *diagram : project->diagrams())
	{
		const DiagramContent content(diagram, false);
		for (const Element *element : content.m_elements)
		{
			if (listed.contains(element)) {
				continue;
			}
			const QDomElement definition = element->location().xml();
			if (!definition.isNull()
					&& (!element->definitionTerminalsMatch(definition)
						|| !element->definitionKindMatches(definition))) {
				lines << describe(element);
			}
		}
	}

	lines.sort();
	return lines;
}

/**
	@brief EmbeddedElementRefresh::textPlans
	What bringing the texts of the placed elements of @p project in line with
	the project's copy of their symbol does, one plan per element (see
	SymbolTextSync). Run after the copies were brought from the collections:
	each element is compared with the symbol it was built from or last
	brought in line with, and the copy as it is now.
	@param project : the project
	@param mode : follow the symbol where the element was not changed
	(Merge), or always (Reset)
	@param elements : the elements to look at; every placed element of
	@p project when empty
	@return the plans, those with nothing to do included: the element still
	has to know the symbol it is in line with
*/
QList<SymbolTextSync::Plan> EmbeddedElementRefresh::textPlans(
		QETProject *project,
		SymbolTextSync::Mode mode,
		const QList<Element *> &elements)
{
	QList<SymbolTextSync::Plan> plans;
	if (!project || project->isReadOnly()) {
		return plans;
	}

	QList<Element *> placed = elements;
	if (placed.isEmpty()) {
		for (Diagram *diagram : project->diagrams()) {
			placed << DiagramContent(diagram, false).m_elements;
		}
	}

	const QHash<QString, SymbolTextSync::Consensus> common =
			mode == SymbolTextSync::Mode::Merge ? consensusByCopy(placed)
												: QHash<QString, SymbolTextSync::Consensus>();
	for (Element *element : std::as_const(placed))
	{
		const QDomElement definition = element->location().xml();
		if (definition.isNull()) {
			continue;
		}
		plans << SymbolTextSync::plan(element, definition, mode,
									  common.value(element->location().collectionPath(false)));
	}
	return plans;
}

/**
	@brief EmbeddedElementRefresh::adoptBoxes
	Give each placed element of @p project whose own copy has its terminals
	but another size or hotspot the box of that copy, so that the redraw
	that follows (Element::reloadPicture()) applies the drawing instead of
	refusing it.

	This is what reopening the project would do, and like the redraw it is
	only visual: the box of an element is never saved, it is read from the
	definition when the element is built. Nothing is pushed on the undo
	stack and the project is not marked as modified.
	@return the number of elements given a new box
*/
int EmbeddedElementRefresh::adoptBoxes(QETProject *project)
{
	int adopted = 0;
	if (!project) {
		return adopted;
	}
	for (Diagram *diagram : project->diagrams())
	{
		const DiagramContent content(diagram, false);
		for (Element *element : content.m_elements)
		{
			const QDomElement definition = element->location().xml();
			if (!definition.isNull()
					&& !element->definitionGeometryMatches(definition)
					&& element->definitionTerminalsMatch(definition)
					&& element->adoptDefinitionBox(definition)) {
				++adopted;
			}
		}
	}
	return adopted;
}

/**
	@brief EmbeddedElementRefresh::reopened
	@p project as opening it again would build it, from what it holds in
	memory and not from its file: whatever was not saved, the versions just
	brought from a collection included, is in it.

	The project is written to a temporary file and opened from there, the
	way QETProject opens any file, so every element is built from its copy
	again - its terminals with it. A conductor is found again by the uuid of
	its terminal, and TerminalUuids::keep() gave a moved terminal the uuid
	it had: the conductor follows it. A conductor whose terminal is gone is
	left out, as opening the project would, and wiresLeftOut() lists it.

	The project returned has the file path and the read-only state of
	@p project and is marked as modified: the file on the disk is untouched
	until it is saved. It also takes over the in-use lock and what @p project
	remembers of the file on the disk (QETProject::handOverTo()), so that
	@p project, destroyed afterwards, releases nothing it no longer owns.
	@param project : the project to reload; left as it is when this fails
	@param error : why it failed, when it does
	@return the project reloaded, owned by the caller, or nullptr
*/
QETProject *EmbeddedElementRefresh::reopened(QETProject *project, QString *error)
{
	if (!project) {
		return nullptr;
	}

	QTemporaryDir dir;
	if (!dir.isValid())
	{
		if (error) {
			*error = dir.errorString();
		}
		return nullptr;
	}

	const QString name = project->filePath().isEmpty()
			? QStringLiteral("project.qet")
			: QFileInfo(project->filePath()).fileName();
	const QString temporary = dir.filePath(name);
	QDomDocument xml = project->toXml();
	QString message;
	if (!QET::writeXmlFile(xml, temporary, &message))
	{
		if (error) {
			*error = message;
		}
		return nullptr;
	}

	QETProject *fresh = new QETProject(temporary);
	if (fresh->state() != QETProject::Ok)
	{
		delete fresh;
		if (error) {
			*error = QCoreApplication::translate(
						 "EmbeddedElementRefresh",
						 "Le projet enregistré dans un fichier temporaire n'a pas pu être rouvert.");
		}
		return nullptr;
	}

	fresh->setFilePath(project->filePath());
	fresh->setReadOnly(project->isReadOnly());
	fresh->setModified(true);
	project->handOverTo(fresh);
	return fresh;
}

/**
	@brief EmbeddedElementRefresh::wiresLeftOut
	@return the conductors that opening @p project left out because a
	terminal they join was not found, one line per conductor with its folio
	- the same list QETDiagramEditor::openAndAddProject() shows
*/
QStringList EmbeddedElementRefresh::wiresLeftOut(QETProject *project)
{
	QStringList lines;
	if (!project) {
		return lines;
	}
	for (Diagram *diagram : project->diagrams()) {
		for (const QString &wire : diagram->wiresNotReconnected()) {
			lines << QCoreApplication::translate("EmbeddedElementRefresh", "Folio %1 : %2")
					 .arg(diagram->folioIndex() + 1).arg(wire);
		}
	}
	return lines;
}

/**
	@brief EmbeddedElementRefresh::reloadQuestion
	@return the question asked before reloading the project, for
	@p elements placed elements whose terminals changed
*/
QString EmbeddedElementRefresh::reloadQuestion(int elements)
{
	return QCoreApplication::translate(
				"EmbeddedElementRefresh",
				"Les bornes ou le type de %n élément(s) ont changé. Recharger le projet"
				" pour les appliquer ?",
				nullptr, elements)
			+ QStringLiteral("\n\n")
			+ QCoreApplication::translate(
				"EmbeddedElementRefresh",
				"L'annulation sera vidée. Rien n'est enregistré sur le disque.");
}

/**
	@brief EmbeddedElementRefresh::reloadReportText
	@return what the reload did: the elements brought from a collection,
	and the conductors left out because their terminal is gone
*/
QString EmbeddedElementRefresh::reloadReportText(const Report &report, int wires_left_out)
{
	QStringList paragraphs;
	paragraphs << QCoreApplication::translate(
					  "EmbeddedElementRefresh",
					  "Projet rechargé : %n élément(s) mis à jour.",
					  nullptr, report.brought_elements);
	if (report.text_elements) {
		paragraphs << textsText(report);
	}
	if (wires_left_out) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n conducteur(s) retiré(s) : leur borne n'existe plus.",
						  nullptr, wires_left_out);
	}
	return paragraphs.join(QStringLiteral("\n\n"));
}

/**
	@brief EmbeddedElementRefresh::reportText
	@return the paragraphs that say what bringFromCollections() did, to go
	before the count of redrawn elements; empty when it did nothing and
	left nothing
*/
QString EmbeddedElementRefresh::reportText(const Report &report)
{
	QStringList paragraphs;
	if (report.brought_elements) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n élément(s) mis à jour depuis la collection.",
						  nullptr, report.brought_elements);
	}
	if (report.text_elements) {
		paragraphs << textsText(report);
	}
	if (!report.terminals_changed.isEmpty()) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n élément(s) non mis à jour : leurs bornes ont changé. Relancez"
						  " la commande et acceptez de recharger le projet.",
						  nullptr, report.terminals_changed.size());
	}
	if (!report.ambiguous.isEmpty()) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n élément(s) non mis à jour : présents dans les collections"
						  " entreprise et utilisateur.",
						  nullptr, report.ambiguous.size());
	}
	if (!report.changed_in_project.isEmpty()) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n élément(s) non mis à jour : modifiés aussi dans le projet,"
						  " dont la version est conservée.",
						  nullptr, report.changed_in_project.size());
	}
	if (!report.moved.isEmpty()) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n élément(s) retrouvé(s) dans un autre dossier de la collection.",
						  nullptr, report.moved.size());
	}
	return paragraphs.join(QStringLiteral("\n\n"));
}

/**
	@brief EmbeddedElementRefresh::textsText
	@return what the update did to the texts of the placed elements, in one
	sentence
*/
QString EmbeddedElementRefresh::textsText(const Report &report)
{
	return QCoreApplication::translate(
				"EmbeddedElementRefresh",
				"Textes de %n élément(s) mis à jour depuis leur symbole :"
				" %1 ajouté(s), %2 modifié(s), %3 retiré(s).",
				nullptr, report.text_elements)
			.arg(report.added_texts)
			.arg(report.changed_texts)
			.arg(report.removed_texts);
}

/**
	@brief EmbeddedElementRefresh::reportDetails
	@return the elements reportText() counts, one per line under a heading,
	for the details area of the message; empty when there are none
*/
QStringList EmbeddedElementRefresh::reportDetails(const Report &report)
{
	QStringList lines;
	if (!report.terminals_changed.isEmpty()) {
		lines << QCoreApplication::translate(
					 "EmbeddedElementRefresh",
					 "Bornes modifiées dans la collection :")
			  << report.terminals_changed
			  << QString();
	}
	if (!report.ambiguous.isEmpty()) {
		lines << QCoreApplication::translate(
					 "EmbeddedElementRefresh",
					 "Présents dans les deux collections :")
			  << report.ambiguous
			  << QString();
	}
	if (!report.changed_in_project.isEmpty()) {
		lines << QCoreApplication::translate(
					 "EmbeddedElementRefresh",
					 "Modifiés dans le projet et dans la collection :")
			  << report.changed_in_project
			  << QString();
	}
	if (!report.moved.isEmpty()) {
		lines << QCoreApplication::translate(
					 "EmbeddedElementRefresh",
					 "Retrouvés dans un autre dossier :")
			  << report.moved
			  << QString();
	}
	return lines;
}

/**
	@brief EmbeddedElementRefresh::describe
	@return the name of @p element and the folio it is on, as a list of the
	message names it
*/
QString EmbeddedElementRefresh::describe(const Element *element)
{
	const Diagram *diagram = element->diagram();
	if (!diagram || !diagram->project()) {
		return element->name();
	}
	return QCoreApplication::translate("EmbeddedElementRefresh", "%1 (folio %2)")
			.arg(element->name())
			.arg(diagram->project()->folioIndex(diagram) + 1);
}

/**
	@brief EmbeddedElementRefresh::savedNotice
	What to tell whoever just saved the symbol at @p saved, or an empty
	string when no open project has it placed.

	A copy of a project is placed in that project only. A symbol of the
	company or user collection is placed wherever a project holds a copy of
	it under import/ and that copy is on a folio. Either way the folios keep
	the old drawing until the drawings are reloaded, and the notice says so,
	with where the command is.

	The text itself is noticeFor()'s.
	@param saved : the location the symbol was saved to
	@param projects : the projects open in the program
	@param collection_dirs : the editable collections, see
	editableCollectionDirs()
*/
QString EmbeddedElementRefresh::savedNotice(
		const ElementsLocation &saved,
		const QList<QETProject *> &projects,
		const QStringList &collection_dirs)
{
	if (saved.isProject()) {
		return noticeFor(saved.collectionPath(false), saved.xml(),
						 {saved.project()}, false, collection_dirs);
	}
	if (saved.isFileSystem() && saved.isWritable()) {
		return noticeFor(import_prefix + saved.collectionPath(false), saved.xml(),
						 projects, true, collection_dirs);
	}
	return QString();
}

/**
	@brief EmbeddedElementRefresh::projectsPlacing
	@return the projects of @p projects that have the symbol just saved at
	@p saved placed, the ones savedNotice() counts: the project itself for
	a copy saved in a project, every project placing the copy of a symbol
	saved in a collection. What "Recharger maintenant" of the notice
	reloads (T62).
*/
QList<QETProject *> EmbeddedElementRefresh::projectsPlacing(
		const ElementsLocation &saved,
		const QList<QETProject *> &projects)
{
	QString embedded_path;
	QList<QETProject *> looked_in;
	if (saved.isProject()) {
		embedded_path = saved.collectionPath(false);
		looked_in = {saved.project()};
	} else if (saved.isFileSystem() && saved.isWritable()) {
		embedded_path = import_prefix + saved.collectionPath(false);
		looked_in = projects;
	}
	QList<QETProject *> placing;
	for (QETProject *project : looked_in) {
		if (project && !placedElements(project, embedded_path).isEmpty()) {
			placing << project;
		}
	}
	return placing;
}

/**
	@brief EmbeddedElementRefresh::noticeFor
	The notice of savedNotice(), for the copies at @p embedded_path of
	@p projects, once @p definition has been saved.

	Besides the count and where the command is, it says what that command
	will do beyond a redraw: terminals added, removed or moved make it
	reload the project; and, for a symbol saved in a collection, what makes
	it fall short - a copy that was also changed in a project, which it
	keeps, and a path that exists in both editable collections, which it
	will not guess between. A new size or hotspot is not mentioned: it is
	applied with the drawing.
	@param embedded_path : path of the copies, "import/..." for a symbol of
	a collection
	@param definition : the <definition> just saved
	@param projects : the projects to look in
	@param from_collection : whether the symbol was saved in a collection
	rather than in a project's copy
	@param collection_dirs : the editable collections
	@return the notice, empty when none of @p projects has the copy placed
*/
QString EmbeddedElementRefresh::noticeFor(
		const QString &embedded_path,
		const QDomElement &definition,
		const QList<QETProject *> &projects,
		bool from_collection,
		const QStringList &collection_dirs)
{
	int total = 0;
	bool terminals_changed = false;
	bool copy_changed = false;
	QStringList lines;
	for (QETProject *project : projects)
	{
		const QList<Element *> placed = placedElements(project, embedded_path);
		if (placed.isEmpty()) {
			continue;
		}
		total += placed.size();
		lines << QStringLiteral("  • %1 : %2")
				 .arg(project->title().isEmpty() ? project->filePath() : project->title(),
					  QCoreApplication::translate("EmbeddedElementRefresh", "%n fois",
												  nullptr, placed.size()));
		for (const Element *element : placed) {
			if (!element->definitionTerminalsMatch(definition)) {
				terminals_changed = true;
			}
		}
		if (from_collection
				&& changedInProject(ElementsLocation(embedded_path, project))) {
			copy_changed = true;
		}
	}

	if (!total) {
		return QString();
	}

		//The list by project only says something when there is more than one
	QString notice = QCoreApplication::translate(
				"EmbeddedElementRefresh",
				"Cet élément est posé %n fois dans les projets ouverts.",
				nullptr, total);
	if (lines.size() > 1) {
		notice += QLatin1Char('\n') + lines.join(QLatin1Char('\n'));
	}
	notice += QStringLiteral("\n\n")
			+ QCoreApplication::translate(
				"EmbeddedElementRefresh",
				"Pour mettre à jour le dessin : « Recharger les dessins des éléments »"
				" (barre Schéma).");

	if (terminals_changed) {
		notice += QStringLiteral("\n\n")
				+ QCoreApplication::translate(
					"EmbeddedElementRefresh",
					"Ses bornes ont changé : la commande proposera de recharger le projet.");
	}

	if (copy_changed) {
		notice += QStringLiteral("\n\n")
				+ QCoreApplication::translate(
					"EmbeddedElementRefresh",
					"Il a aussi été modifié dans un projet, dont la version est conservée.");
	}

	if (from_collection && sourceFilesFor(embedded_path, collection_dirs).size() > 1) {
		notice += QStringLiteral("\n\n")
				+ QCoreApplication::translate(
					"EmbeddedElementRefresh",
					"Le même chemin existe dans les collections entreprise et"
					" utilisateur : il ne sera pas mis à jour.");
	}

	return notice;
}
