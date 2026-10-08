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
#include "elementorigin.h"
#include "elementslocation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUuid>

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

	const QString relative = embedded_path.mid(import_prefix.size());
	for (const QString &dir : collection_dirs)
	{
		if (dir.isEmpty()) {
			continue;
		}
		const QString path = QDir::cleanPath(QDir(dir).absoluteFilePath(relative));
		if (QFileInfo(path).isFile() && !found.contains(path)) {
			found << path;
		}
	}
	return found;
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
	For each symbol placed in @p project whose copy came from a collection,
	what @p collection_dirs hold for it. A copy kept by the project but placed
	on no folio is left out: there is nothing on screen to bring it to.
	@return one entry per symbol, in no particular order
*/
QList<EmbeddedElementRefresh::Entry> EmbeddedElementRefresh::survey(
		QETProject *project,
		const QStringList &collection_dirs)
{
	QList<Entry> entries;
	const QHash<QString, QList<Element *>> placed = placedByPath(project);

	for (auto it = placed.cbegin() ; it != placed.cend() ; ++it)
	{
		if (!it.key().startsWith(import_prefix)) {
			continue;
		}

		Entry entry;
		entry.embedded_path = it.key();
		entry.placed = it.value();
		entry.source_files = sourceFilesFor(entry.embedded_path, collection_dirs);

		const ElementsLocation copy(entry.embedded_path, project);
		const QDomElement copy_definition = copy.xml();
		const QUuid copy_uuid = ElementOrigin::definitionUuid(copy_definition);
		const QUuid origin = ElementOrigin::of(copy_definition.parentNode().toElement());

			//A file still carrying the origin, or the copy's own uuid, is
			//the version the project already has: that collection did not
			//change, and it is also the answer to "which one did it come
			//from" when the path is in both. A file without uuid, taken by a
			//copy without one, matches the null origin: it cannot say which
			//version it is, so it is never brought
		QList<QDomDocument> candidates;
		bool unchanged = false;
		for (const QString &file : std::as_const(entry.source_files))
		{
			const QDomDocument candidate = readDefinition(file);
			if (candidate.isNull()) {
				continue;
			}
			const QUuid uuid = ElementOrigin::definitionUuid(candidate.documentElement());
			if (uuid == origin || uuid == copy_uuid) {
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
		else if (copy_uuid != origin) {
			entry.outcome = Outcome::ChangedInProject;
		}
		else
		{
				//Only the terminals decide: a new size or hotspot is given
				//to the placed elements with the drawing (see
				//RefreshEmbeddedElementsCommand), and moves no conductor
			entry.source = candidates.first();
			const QDomElement definition = entry.source.documentElement();
			entry.outcome = Outcome::Pending;
			for (const Element *element : std::as_const(entry.placed)) {
				if (!element->definitionTerminalsMatch(definition)) {
					entry.outcome = Outcome::TerminalsChanged;
					break;
				}
			}
		}
		entries << entry;
	}
	return entries;
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
	@return what was done and what was left, for the message
*/
EmbeddedElementRefresh::Report EmbeddedElementRefresh::bringFromCollections(
		QETProject *project,
		const QStringList &collection_dirs,
		bool with_terminal_changes)
{
	Report report;
	if (!project || project->isReadOnly()) {
		return report;
	}

	QList<RefreshEmbeddedElementsCommand::Change> changes;
	for (const Entry &entry : survey(project, collection_dirs))
	{
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
				break;
			case Outcome::Ambiguous:
				report.ambiguous << QStringLiteral("%1 (%2)")
						.arg(entry.placed.first()->name(),
							 entry.embedded_path.mid(import_prefix.size()));
				break;
			case Outcome::ChangedInProject:
				report.changed_in_project << QStringLiteral("%1 (%2)")
						.arg(entry.placed.first()->name(),
							 entry.embedded_path.mid(import_prefix.size()));
				break;
			case Outcome::NoSource:
			case Outcome::UpToDate:
				break;
		}
	}

	report.brought_symbols = changes.size();
	if (!changes.isEmpty()) {
		project->undoStack()->push(new RefreshEmbeddedElementsCommand(project, changes));
	}

	report.ambiguous.sort();
	report.changed_in_project.sort();
	report.terminals_changed.sort();
	return report;
}

/**
	@brief EmbeddedElementRefresh::needingReload
	The placed elements of @p project that only a reload can bring up to
	date: those of a symbol @p collection_dirs hold a newer version of that
	adds, removes or moves a terminal, and those whose own copy in the
	project has other terminals than they have - a copy corrected from a
	folio, which the redraw cannot apply either.
	@return one line per element, with its folio; empty when nothing needs
	the project to be reloaded, or when it is read-only
*/
QStringList EmbeddedElementRefresh::needingReload(
		QETProject *project,
		const QStringList &collection_dirs)
{
	QStringList lines;
	if (!project || project->isReadOnly()) {
		return lines;
	}

	QSet<const Element *> listed;
	for (const Entry &entry : survey(project, collection_dirs))
	{
		if (entry.outcome != Outcome::TerminalsChanged) {
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
			if (!definition.isNull() && !element->definitionTerminalsMatch(definition)) {
				lines << describe(element);
			}
		}
	}

	lines.sort();
	return lines;
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
				"Les bornes de %n élément(s) posé(s) ont changé (borne ajoutée,"
				" supprimée ou déplacée).",
				nullptr, elements)
			+ QStringLiteral("\n\n")
			+ QCoreApplication::translate(
				"EmbeddedElementRefresh",
				"Pour les appliquer sans rien supprimer ni réinsérer, le projet va être"
				" rechargé comme s'il était rouvert : un conducteur suit sa borne"
				" déplacée, et le conducteur d'une borne qui n'existe plus est retiré.")
			+ QStringLiteral("\n\n")
			+ QCoreApplication::translate(
				"EmbeddedElementRefresh",
				"L'historique d'annulation sera vidé. Il n'est pas nécessaire"
				" d'enregistrer avant : le fichier sur le disque ne change que lorsque"
				" vous enregistrez, et fermer sans enregistrer reste possible.")
			+ QStringLiteral("\n\n")
			+ QCoreApplication::translate("EmbeddedElementRefresh", "Recharger le projet maintenant ?");
}

/**
	@brief EmbeddedElementRefresh::reloadReportText
	@return what the reload did: the elements brought from a collection,
	and the conductors left out because their terminal is gone
*/
QString EmbeddedElementRefresh::reloadReportText(const Report &report, int wires_left_out)
{
	QStringList paragraphs;
	paragraphs << QCoreApplication::translate("EmbeddedElementRefresh",
											  "Le projet a été rechargé.");
	if (report.brought_elements) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n élément(s) mis à jour depuis la collection entreprise"
						  " ou utilisateur.",
						  nullptr, report.brought_elements);
	}
	if (wires_left_out) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n conducteur(s) retiré(s) : la borne à laquelle ils étaient"
						  " reliés n'existe plus. Si vous enregistrez, ils disparaîtront"
						  " du fichier ; fermez sans enregistrer pour garder le fichier"
						  " tel quel.",
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
						  "%n élément(s) mis à jour depuis la collection entreprise"
						  " ou utilisateur.",
						  nullptr, report.brought_elements);
	}
	if (!report.terminals_changed.isEmpty()) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n élément(s) non mis à jour depuis la collection : dans la"
						  " nouvelle version, leurs bornes ont changé (borne ajoutée,"
						  " supprimée ou déplacée). Pour l'appliquer, relancez la commande"
						  " et acceptez de recharger le projet.",
						  nullptr, report.terminals_changed.size());
	}
	if (!report.ambiguous.isEmpty()) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n élément(s) non mis à jour : le même chemin existe dans la"
						  " collection entreprise et dans la collection utilisateur, et"
						  " le projet ne dit pas de laquelle vient sa copie.",
						  nullptr, report.ambiguous.size());
	}
	if (!report.changed_in_project.isEmpty()) {
		paragraphs << QCoreApplication::translate(
						  "EmbeddedElementRefresh",
						  "%n élément(s) non mis à jour : ils ont été modifiés dans le"
						  " projet et aussi dans la collection. La version du projet est"
						  " conservée ; pour prendre celle de la collection, faites"
						  " glisser l'élément de la collection sur un folio et"
						  " choisissez de remplacer l'élément du projet.",
						  nullptr, report.changed_in_project.size());
	}
	return paragraphs.join(QStringLiteral("\n\n"));
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

	QString notice = QCoreApplication::translate(
				"EmbeddedElementRefresh",
				"Cet élément est posé %n fois dans les projets ouverts :",
				nullptr, total)
			+ QLatin1Char('\n') + lines.join(QLatin1Char('\n'))
			+ QStringLiteral("\n\n")
			+ QCoreApplication::translate(
				"EmbeddedElementRefresh",
				"Les éléments déjà posés gardent l'ancien dessin tant que vous"
				" n'avez pas cliqué sur « Recharger les dessins des éléments »,"
				" dans la barre d'outils Schéma ou dans le menu Projet.");

	if (terminals_changed) {
		notice += QStringLiteral("\n\n")
				+ QCoreApplication::translate(
					"EmbeddedElementRefresh",
					"Ses bornes ont changé (borne ajoutée, supprimée ou déplacée) :"
					" la commande proposera de recharger le projet comme s'il était"
					" rouvert. Rien n'est à supprimer ni à réinsérer : un conducteur"
					" suit sa borne déplacée, et le conducteur d'une borne qui"
					" n'existe plus est retiré.");
	}

	if (copy_changed) {
		notice += QStringLiteral("\n\n")
				+ QCoreApplication::translate(
					"EmbeddedElementRefresh",
					"La copie de cet élément a aussi été modifiée dans un projet :"
					" la version de ce projet est conservée, et elle ne sera pas"
					" remplacée par celle-ci.");
	}

	if (from_collection && sourceFilesFor(embedded_path, collection_dirs).size() > 1) {
		notice += QStringLiteral("\n\n")
				+ QCoreApplication::translate(
					"EmbeddedElementRefresh",
					"Le même chemin existe dans la collection entreprise et dans la"
					" collection utilisateur : les projets ne disent pas de laquelle"
					" vient leur copie, et elle ne sera pas mise à jour.");
	}

	return notice;
}
