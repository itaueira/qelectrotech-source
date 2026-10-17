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
#include "diagramcommands.h"

#include "ElementProvider.h"
#include "cable/cablemanager.h"
#include "dataBase/projectdatabase.h"
#include "autoNum/elementautonumschemecommand.h"
#include "diagram.h"
#include "itemgroups.h"
#include "qetproject.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetgraphicsitem/diagramimageitem.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/elementtextitemgroup.h"
#include "qetproject.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "qetgraphicsitem/qetshapeitem.h"
#include "qetgraphicsitem/reporttags.h"
#include "qetinformation.h"
#include "qgimanager.h"
#include "undocommand/linkelementcommand.h"

#include <QHash>
#include <QPointer>
#include <QSet>

#include <utility>

namespace {
/// Is @p element among those the numberings in @p schemes are given to?
bool pasted_schemes_has(const QMap<QString, QVector<Element *>> &schemes, const Element *element)
{
	for (auto it = schemes.constBegin() ; it != schemes.constEnd() ; ++it) {
		if (it.value().contains(const_cast<Element *>(element))) {
			return true;
		}
	}
	return false;
}

/**
	What a cut keeps of each element it takes off a folio, under the uuid
	of the element, for the paste that may bring it back (see
	moved_elements()): the project it was taken from, and the group index
	each partner it was linked to had given it, by the uuid of the partner
	(Element::groupIndexForElement()). That index is the partner's, and the
	cut loses it: a PLC master forgets it as it unlinks, and any other one
	keeps it for the element taken away, which is not the element a paste
	puts back.
*/
struct CutElement
{
	QPointer<QETProject> project;
	QHash<QUuid, int> partner_groups;
};

/**
	Every element cut in this session, by uuid. Never emptied, and it need
	not be: an entry only counts while no folio of its project holds its
	uuid, and the project is watched, so a project closed and opened again
	is another one.
*/
QHash<QUuid, CutElement> &cut_elements()
{
	static QHash<QUuid, CutElement> elements;
	return elements;
}

/**
	The elements of @p pasted that are not copies: those a cut took off a
	folio of @p project (CutDiagramCommand) and that no folio of it holds
	any more. They are the elements themselves, carried from one place to
	another - in this program, cut and paste is the way to take an element
	to another folio - and they keep their identity, their information and
	their links.

	Everything else pasted is a copy, as it has always been: of an element
	still on a folio (a copy, a second paste of the same cut, a cut that was
	undone), of an element of another project, of a group of symbols read
	from a file. The uuid alone could not tell those apart: an element
	copied from another project is not in this one either, and taking it
	for a moved one would keep its label and skip its numbering.

	Where several pasted elements carry the same uuid, only the first one
	can be the element itself.
*/
QList<Element *> moved_elements(QETProject *project, const QList<Element *> &pasted)
{
	QList<Element *> moved;
	if (!project) {
		return moved;
	}

	QSet<QUuid> cut;
	for (const Element *element : pasted)
	{
		const auto known = cut_elements().constFind(element->uuid());
		if (known != cut_elements().constEnd() && known->project.data() == project) {
			cut.insert(element->uuid());
		}
	}
	if (cut.isEmpty()) {
		return moved;
	}

		//Still on a folio: what is pasted is a copy of it. The folios are
		//not built for this (lazy folios): the elements a cut took were on
		//a folio being shown, and a folio not built holds none of them.
	const QSet<const Element *> batch(pasted.cbegin(), pasted.cend());
	const QList<Diagram *> folios = project->folios();
	for (Diagram *folio : folios)
	{
		const QList<Element *> elements = folio->elements();
		for (const Element *element : elements) {
			if (!batch.contains(element)) {
				cut.remove(element->uuid());
			}
		}
	}

	for (Element *element : pasted) {
		if (cut.remove(element->uuid())) {
			moved << element;
		}
	}
	return moved;
}

/**
	Link @p element, a moved element (moved_elements()), to the partners it
	was saved with, the way a project being opened links its elements
	(Element::initLink(QETProject *)) - with one difference: only a partner
	free to take the link gets it back. One that found another partner
	since the cut keeps that one; linking it anyway would take it from the
	other without a word, and undoing the paste would not give it back.
	The partners not found, or not free, are forgotten, as a copy forgets
	those of its partners that were not pasted with it.

	The group index each partner had given the element before the cut is
	given back to it (CutElement::partner_groups).
*/
void link_moved(Element *element, QETProject *project)
{
	QList<QUuid> saved;
	const QList<std::pair<QUuid, int>> waiting = element->waitingLinks();
	for (const auto &link : waiting) {
		saved << link.first;
	}

	QList<Element *> partners;
	if (!saved.isEmpty() && project)
	{
		const QList<Element *> found = ElementProvider(project->folios()).fromUuids(saved);
		for (Element *partner : found)
		{
			if (partner != element && LinkElementCommand::isLinkable(element, partner)) {
				partners << partner;
			}
		}
	}
		//Empty, it still forgets what the element was saved linked to
	element->initLink(partners);

	const auto known = cut_elements().constFind(element->uuid());
	if (known == cut_elements().constEnd()) {
		return;
	}
	const QList<Element *> linked = element->linkedElements();
	for (Element *partner : linked)
	{
		const int group = known->partner_groups.value(partner->uuid(), -1);
		if (group >= 0 && partner->groupIndexForElement(element) < 0) {
			partner->setGroupIndexForElement(element, group);
		}
	}
}

/**
	The links of the elements a paste put on a folio, made again on each
	redo of the paste after the first. Taking the elements off the folio
	(PasteDiagramCommand::undo(), through Diagram::removeItem()) unlinks
	them, and the redo only puts them back: without this, a master and its
	slave pasted together, or an element a cut brought back, came back on
	the folio unlinked. DeleteQGraphicsItemCommand keeps the links of what
	it takes away the same way, for its undo.

	A child of the paste, so that it runs once the paste has put its
	elements on the folio, and made last, after the numbering of the
	pasted elements: on the first redo it only takes note of what the paste
	made - each link, with the group index each side gave the other, and
	what each element of a link says. Unlinking from a PLC master erases
	from its slave what the master gave it, and the redo gives it back.

	Its undo does nothing: the paste takes the elements off the folio right
	after, and that unlinks them.
*/
class PastedLinksCommand : public QUndoCommand
{
	public:
		PastedLinksCommand(const QList<Element *> &pasted, QUndoCommand *parent) :
			QUndoCommand(parent),
			m_pasted(pasted)
		{}

		void redo() override
		{
			if (m_first_redo) {
				m_first_redo = false;
				keep();
			} else {
				restore();
			}
			QUndoCommand::redo();
		}

	private:
		struct Link
		{
			QPointer<Element> element;
			QPointer<Element> partner;
				/// element->groupIndexForElement(partner)
			int here = -1;
				/// partner->groupIndexForElement(element)
			int there = -1;
		};

		void keep()
		{
			QSet<Element *> informed;
			auto inform = [&](Element *element) {
				if (!informed.contains(element)) {
					informed.insert(element);
					m_informations.append(std::make_pair(QPointer<Element>(element),
														 element->elementInformations()));
				}
			};

			for (Element *element : std::as_const(m_pasted))
			{
				const QList<Element *> partners = element->linkedElements();
				for (Element *partner : partners)
				{
					Link link;
					link.element = element;
					link.partner = partner;
					link.here = element->groupIndexForElement(partner);
					link.there = partner->groupIndexForElement(element);
					m_links.append(link);
					inform(element);
					inform(partner);
				}
			}
		}

		void restore()
		{
			QETProject *project = nullptr;
			bool report = false;
			for (const Link &link : std::as_const(m_links))
			{
				Element *element = link.element.data();
				Element *partner = link.partner.data();
				if (!element || !partner) {
					continue;
				}
				element->linkToElement(partner);
				if (!element->linkedElements().contains(partner)) {
					continue;
				}
				if (link.here >= 0) {
					element->setGroupIndexForElement(partner, link.here);
				}
				if (link.there >= 0) {
					partner->setGroupIndexForElement(element, link.there);
				}
				if (element->linkType() & Element::AllReport) {
					report = true;
				}
				if (!project && element->diagram()) {
					project = element->diagram()->project();
				}
			}

			QList<Element *> changed;
			for (const auto &kept : std::as_const(m_informations))
			{
				Element *element = kept.first.data();
				if (element && element->elementInformations() != kept.second) {
					element->setElementInformations(kept.second);
					changed << element;
				}
			}
			if (project && !changed.isEmpty()) {
				project->dataBase()->elementInfoChanged(changed);
			}
				//Linked again, a folio report carries its cable core over
				//to the next folio once more
			if (project && report) {
				CableManager::refreshLabels(project);
			}
		}

		QList<Element *> m_pasted;
		QList<Link> m_links;
		QList<std::pair<QPointer<Element>, DiagramContext>> m_informations;
		bool m_first_redo = true;
};
} // namespace

/**
	@brief PasteDiagramCommand::PasteDiagramCommand
	Constructor
	@param dia : diagram where we must paste
	@param c : content to paste
	@param parent : parent undo command
*/
PasteDiagramCommand::PasteDiagramCommand( Diagram *dia, const DiagramContent &c, QUndoCommand *parent) :
	QUndoCommand(parent),
	content(c),
	diagram(dia),
	filter(DiagramContent::Elements|DiagramContent::TextFields|DiagramContent::Images|DiagramContent::ConductorsToMove | DiagramContent::Shapes),
	first_redo(true)
{
	setText(QObject::tr("paste %1", "undo caption - %1 is a sentence listing the content to paste").arg(content.sentence(filter)));
	diagram -> qgiManager().manage(content.items(filter));
}

/**
	@brief PasteDiagramCommand::~PasteDiagramCommand
	Destructor
*/
PasteDiagramCommand::~PasteDiagramCommand()
{
	diagram -> qgiManager().release(content.items(filter));
}

/**
	@brief PasteDiagramCommand::undo
	Undo this command
*/
void PasteDiagramCommand::undo()
{
		//A paste is one gesture: the items land, the pasted conductors are
		//given new identities and their text is reset, and every one of those
		//is a row of the data base. One notice for the paste, not one per
		//item.
	projectDataBase::Operation operation(
			diagram ? diagram->project() : nullptr);

	diagram -> showMe();

		//The numbering of the pasted elements, if any: gives the numbers
		//back to the numberings they came from
	QUndoCommand::undo();

		//A folio report linked to another carries a cable core over to the
		//next folio: taking it away changes which wires belong to which
		//cable, as deleting it does (DeleteQGraphicsItemCommand::redo())
	bool linked_report = false;
	for (Element *e : std::as_const(content.m_elements)) {
		if ((e->linkType() & Element::AllReport) && !e->isFree()) {
			linked_report = true;
			break;
		}
	}

	foreach(QGraphicsItem *item, content.items(filter))
		diagram->removeItem(item);

	if (linked_report && diagram->project()) {
		CableManager::refreshLabels(diagram->project());
	}
}

/**
	@brief PasteDiagramCommand::redo
	Redo this command
*/
void PasteDiagramCommand::redo()
{
		//A paste is one gesture: the items land, the pasted conductors are
		//given new identities and their text is reset, and every one of those
		//is a row of the data base. One notice for the paste, not one per
		//item.
	projectDataBase::Operation operation(
			diagram ? diagram->project() : nullptr);

	diagram -> showMe();
	QSettings settings;

	if (first_redo)
	{
		first_redo = false;

		QETProject *project = diagram->project();
		const QList <Element *> elmts_list = content.m_elements;

			//The elements a cut took off this project and that this paste
			//brings back are the elements themselves, moved, and not
			//copies of them (see moved_elements()): they keep their uuid,
			//their information and their links. What follows that makes a
			//copy of an element - an identity of its own, a label erased or
			//numbered anew - is done to the copies only.
		const QList<Element *> moved = moved_elements(project, elmts_list);
		QList<Element *> copies = elmts_list;
		for (Element *e : moved) {
			copies.removeOne(e);
		}

		//Resolve a linked master/slave pair pasted together (bugtracker
		//#607) before anything below renews their uuids: at this exact
		//moment a pasted element's tmp_uuids_link still holds its
		//source's original partner uuid, which still equals the
		//not-yet-renewed uuid of that partner's own pasted copy if it
		//was carried along in the same batch. Scoped to the copies of
		//this batch only (not a project-wide search), so a pair pasted
		//together links to each other and not to an original element
		//left elsewhere that happens to still carry that same
		//soon-to-be-replaced uuid. If only one half of a linked group was
		//pasted, its link entry simply finds no match here and is dropped
		//-- same "leave it unlinked" outcome as always. The moved
		//elements are linked further down, to the partners they had.
		for (Element *e : copies) {
			e->initLink(copies);
		}

		//The numberings the copies follow, known now: the label erasing
		//below empties their formula. Copies get the next number of their
		//numbering, as placed elements do, instead of the label of the
		//element they were copied from. A moved element keeps its own: it
		//is not a new element.
		const bool autonumber = m_autonumber && settings.value(
					"diagramcommands/autonumber-pasted-elements", true).toBool();
		QMap<QString, QVector<Element *>> pasted_schemes;
		if (project && autonumber) {
			pasted_schemes = ElementAutoNumSchemeCommand::pastedSchemes(project, copies);
		}

		//make new uuid for every pasted conductor, because old uuid are
		//the uuid of the copied conductor
		const QList <Conductor *> all_pasted_conductors = content.conductors();
		for (Conductor *c : all_pasted_conductors) {
			c -> newUuid();
		}
		for (IndependentTextItem *t : std::as_const(content.m_text_fields)) {
			t -> newUuid();
		}
		for (DiagramImageItem *i : std::as_const(content.m_images)) {
			i -> newUuid();
		}
		for (QetShapeItem *s : std::as_const(content.m_shapes)) {
			s -> newUuid();
		}

		//this is the first paste, we do some actions for the copies
		for (Element *e : std::as_const(copies))
		{
			//make new uuid, because old uuid are the uuid of the copied element
			e -> newUuid();

			// PLC slaves carry master-specific data (type, address,
			// function, cross-ref, etc.) in their elementInformations.
			// Always clear those on paste so the duplicate starts clean,
			// regardless of the user's erase-label-on-copy preference.
			// A slave pasted together with its master was relinked to the
			// master's copy by initLink() above: that data is still right,
			// and its texts show the master's label, so leave it alone.
			const bool is_slave = (e->linkType() == Element::Slave);
			if (is_slave && e->isFree()) {
				DiagramContext dc = e->elementInformations();
				dc.remove(QETInformation::ELMT_PLC_TYPE);
				dc.remove(QETInformation::ELMT_PLC_ADDRESS);
				dc.remove(QETInformation::ELMT_PLC_FUNCTION);
				dc.remove(QETInformation::ELMT_PLC_COMMENT);
				dc.remove(QETInformation::ELMT_PLC_CROSSREF);
				dc.remove(QETInformation::ELMT_LABEL);
				dc.remove(QETInformation::ELMT_PLC_TC);
				dc.remove(QETInformation::ELMT_PLC_T1);
				dc.remove(QETInformation::ELMT_PLC_T2);
				dc.remove(QETInformation::ELMT_PLC_T3);
				dc.remove(QETInformation::ELMT_PLC_T4);
				dc.remove(QStringLiteral("xref"));

				// Block alignment before setElementInformations so
				// that elementInfoChanged() resolves texts without
				// finishAlignment() shifting right/center-aligned items.
				for (DynamicElementTextItem *deti : e->dynamicTextItems())
					deti->m_block_alignment = true;
				for (auto *group : e->textGroups())
					group->blockAlignmentUpdate(true);

				e->setElementInformations(dc);

				for (DynamicElementTextItem *deti : e->dynamicTextItems())
					deti->m_block_alignment = false;
				for (auto *group : e->textGroups())
					group->blockAlignmentUpdate(false);

				// After setElementInformations, elementInfoChanged()
				// resolves composite text with cleaned dc.  Clear
				// all non-UserText items directly as a safety net.
				for (DynamicElementTextItem *deti : e->dynamicTextItems()) {
					if (deti->textFrom() != DynamicElementTextItem::UserText) {
						deti->m_block_alignment = true;
						deti->setPlainText(QString());
						deti->m_block_alignment = false;
					}
				}
				for (auto *group : e->textGroups()) {
					for (DynamicElementTextItem *deti : group->texts()) {
						if (deti->textFrom() != DynamicElementTextItem::UserText) {
							deti->m_block_alignment = true;
							deti->setPlainText(QString());
							deti->m_block_alignment = false;
						}
					}
				}
			}

			if (settings.value("diagramcommands/erase-label-on-copy", true).toBool())
			{
				//Reset the information about the label, the comment and location
				DiagramContext dc = e->elementInformations();
				dc.addValue("formula", "");
				dc.addValue("label", "");
				dc.addValue("comment", "");
				dc.addValue("location", "");

				// The texts are emptied with their realignment on, so
				// that an empty text keeps the anchor of its alignment:
				// the right edge of a right-aligned label, the centre of
				// a centred one. Whatever fills it afterwards - the
				// numbering of the copy below, the user typing a label -
				// realigns from that anchor. Emptied with the
				// realignment blocked, the text kept its top left corner
				// instead, and the label that filled it landed off by its
				// own width, or half of it, from where the original's
				// sits. Element::fromXml() blocks it for another reason:
				// there the saved position is already the aligned one.
				// A group lays its texts out from its own corner and
				// keeps no anchor, so its update stays blocked.
				if (!is_slave) {
					for (auto *group : e->textGroups())
						group->blockAlignmentUpdate(true);
				}

				e->setElementInformations(dc);

				for (auto *group : e->textGroups())
					group->blockAlignmentUpdate(false);
			}
		}

		//Reset the text of conductors, the same way the label/comment/
		//location above are reset to "" rather than to some other
		//value - "erase on copy" means erase, not "replace with the
		//project's default new-conductor text" (which happens to
		//default to a literal "_" character, unrelated to whether the
		//user wanted this copy's old label kept or cleared; see
		//issue #413).
		//This loop used to sit inside the element loop above. Through the
		//clipboard that only wasted work - it ran once per pasted element
		//over the same whole list - and never lost a reset, because a
		//clipboard content cannot hold conductors without elements:
		//Diagram::toXml() only writes a conductor when both of its
		//elements are selected, and Diagram::fromXml() only rebuilds one
		//when both terminals are among the elements it just added. What
		//the nesting did break is this command's own contract, which takes
		//a DiagramContent and says nothing about elements being in it: a
		//caller that hands over conductors alone got no reset at all. It
		//is out here so that the reset follows the contract rather than
		//the habits of today's three callers.
		if (settings.value("diagramcommands/erase-label-on-copy", true).toBool())
		{
			const QList <Conductor *> conductors_list = content.m_conductors_to_move;
			for (Conductor *c : conductors_list)
			{
				ConductorProperties cp = c -> properties();
				cp.text = "";
				c -> setProperties(cp);
			}
		}

			//What the pasted elements keep of their numbering names a
			//numbering of this project, or none (a paste from another
			//project, or from a file written before the ids)
		if (project) {
			ElementAutoNumSchemeCommand::linkPasted(project, elmts_list);
			if (autonumber)
			{
					//A formula which names no numbering of this project is
					//not kept: it would stand for a numbering which does
					//not exist here, with a label which is not its result.
					//The label is what the erase preference made of it. A
					//moved element keeps its formula, as it would have on
					//its folio.
				for (Element *e : std::as_const(copies))
				{
					const DiagramContext &info = e->elementInformations();
					if (e->linkType() == Element::Slave || (e->linkType() & Element::AllReport)
							|| info.value(QETInformation::ELMT_FORMULA).toString().isEmpty()
							|| !ElementAutoNumSchemeCommand::followedScheme(project, info).isEmpty()
							|| pasted_schemes_has(pasted_schemes, e)) {
						continue;
					}
					DiagramContext dc = info;
					dc.addValue(QETInformation::ELMT_FORMULA, QString());
					for (DynamicElementTextItem *deti : e->dynamicTextItems())
						deti->m_block_alignment = true;
					for (auto *group : e->textGroups())
						group->blockAlignmentUpdate(true);
					e->setElementInformations(dc);
					for (DynamicElementTextItem *deti : e->dynamicTextItems())
						deti->m_block_alignment = false;
					for (auto *group : e->textGroups())
						group->blockAlignmentUpdate(false);
				}
			}
			if (!pasted_schemes.isEmpty()) {
				ElementAutoNumSchemeCommand::numberPasted(project, pasted_schemes, this);
			}
		}

			//The moved elements linked again to the partners they had, only
			//now that every copy has an identity of its own: one which still
			//carried the uuid of a partner would be taken for it
		for (Element *e : moved) {
			link_moved(e, project);
		}

			//What this paste linked, linked again on every redo: taking the
			//elements off the folio on undo unlinks them. Made after the
			//numbering above, so that it runs after it (see
			//PastedLinksCommand).
		bool linked = false;
		bool linked_report = false;
		for (Element *e : elmts_list)
		{
			if (!e->isFree()) {
				linked = true;
				if (e->linkType() & Element::AllReport) {
					linked_report = true;
				}
			}
		}
		if (linked) {
			new PastedLinksCommand(elmts_list, this);
		}

			//Pasted groups become new groups: the members of one source
			//group all get the same new uuid, never the source's, or the
			//copy would join the original's group. After the elements got
			//their own uuids: the database row is found by uuid, and before
			//that it would have been the source element's row.
		QHash<QUuid, QUuid> renewed_groups;
		for (QGraphicsItem *item : content.items(DiagramContent::Elements
												 | DiagramContent::TextFields
												 | DiagramContent::Images
												 | DiagramContent::Shapes))
		{
			const QUuid source_group = ItemGroups::groupOf(item);
			if (source_group.isNull()) {
				continue;
			}
			if (!renewed_groups.contains(source_group)) {
				renewed_groups.insert(source_group, QUuid::createUuid());
			}
			diagram -> setItemGroup(item, renewed_groups.value(source_group));
		}

			//A copied folio report carries the tag of the arrow it was
			//copied from: it gets the next one. A cut arrow pasted back is
			//alone with its tag, and keeps it (T59).
		ReportTags::renewRepeated(elmts_list, project);

			//A folio report linked to another carries a cable core over to
			//the next folio: a pasted one that is linked changes which
			//wires belong to which cable
		if (linked_report && project) {
			CableManager::refreshLabels(project);
		}
	}
	else
	{
		const QList<QGraphicsItem *> qgis_list = content.items(filter);
		for (QGraphicsItem *item : qgis_list) {
			diagram->addItem(item);
		}
	}

		//The numbering of the pasted elements, if any, on the first redo
		//as well as on the next ones
	QUndoCommand::redo();

	const QList<QGraphicsItem *> qgis_list = content.items();
	for (QGraphicsItem *qgi : qgis_list)
		qgi -> setSelected(true);
}

/**
	@brief CutDiagramCommand::CutDiagramCommand
	Constructeur
	@param dia Schema dont on coupe des elements et conducteurs
	@param content Contenu coupe
	@param parent QUndoCommand parent
*/
CutDiagramCommand::CutDiagramCommand(
	Diagram *dia,
	const DiagramContent &content,
	QUndoCommand *parent
) : 
	DeleteQGraphicsItemCommand(dia, content, parent)
{
	setText(
		QString(
			QObject::tr(
				"cut %1",
				"undo caption - %1 is a sentence listing the content to cut"
			).arg(content.sentence(DiagramContent::All))
		)
	);

		//What the paste that brings these elements back needs to know them
		//by (moved_elements()), taken now: still linked, each partner still
		//holds the group index it gave the element
	QETProject *project = dia ? dia->project() : nullptr;
	for (Element *element : content.m_elements)
	{
		CutElement cut;
		cut.project = project;
		const QList<Element *> partners = element->linkedElements();
		for (Element *partner : partners)
		{
			const int group = partner->groupIndexForElement(element);
			if (group >= 0) {
				cut.partner_groups.insert(partner->uuid(), group);
			}
		}
		cut_elements().insert(element->uuid(), cut);
	}
}

/**
	@brief CutDiagramCommand::~CutDiagramCommand
	Destructeur
*/
CutDiagramCommand::~CutDiagramCommand()
{
}

/**
	@brief MoveConductorsTextsCommand::MoveConductorsTextsCommand
	Constructeur
	@param diagram Schema sur lequel on deplace des champs de texte
	@param parent QUndoCommand parent
*/
MoveConductorsTextsCommand::MoveConductorsTextsCommand(
	Diagram *diagram,
	QUndoCommand *parent
) :
	QUndoCommand(parent),
	diagram(diagram),
	first_redo(true)
{
}

/**
	@brief MoveConductorsTextsCommand::~MoveConductorsTextsCommand
	Destructeur
*/
MoveConductorsTextsCommand::~MoveConductorsTextsCommand()
{
}

/**
	@brief MoveConductorsTextsCommand::undo
	annule le deplacement
*/
void MoveConductorsTextsCommand::undo()
{
	diagram -> showMe();
	foreach(ConductorTextItem *cti, texts_to_move_.keys()) {
		QPointF movement = texts_to_move_[cti].first;
		bool was_already_moved = texts_to_move_[cti].second;
		
		cti -> forceMovedByUser(was_already_moved);
		if (was_already_moved) {
			cti -> setPos(cti -> pos() - movement);
		}
	}
}

/**
	@brief MoveConductorsTextsCommand::redo
	refait le deplacement
*/
void MoveConductorsTextsCommand::redo()
{
	diagram -> showMe();
	if (first_redo) {
		first_redo = false;
	} else {
		foreach(ConductorTextItem *cti, texts_to_move_.keys()) {
			QPointF movement = texts_to_move_[cti].first;
			
			cti -> forceMovedByUser(true);
			cti -> setPos(cti -> pos() + movement);
		}
	}
}

/**
	@brief MoveConductorsTextsCommand::addTextMovement
	Ajout un mouvement de champ de texte a cet objet
	@param text_item Champ de texte deplace ;
	si celui-ci est deja connu de l'objet d'annulation,
	il sera ignore
	@param old_pos Position du champ de texte avant le mouvement
	@param new_pos Position du champ de texte apres le mouvement
	@param already_moved true si le champ de texte etait deja a une position
	personnalisee par l'utilisateur, false sinon
*/
void MoveConductorsTextsCommand::addTextMovement(ConductorTextItem *text_item,
						 const QPointF &old_pos,
						 const QPointF &new_pos,
						 bool already_moved) {
	// si le champ de texte est deja connu de l'objet d'annulation, il sera ignore
	if (texts_to_move_.contains(text_item)) return;
	
	// on memorise le champ de texte,
	//en l'associant au mouvement effectue et a son etat avant le deplacement
	texts_to_move_.insert(text_item, qMakePair(new_pos - old_pos, already_moved));
	
	// met a jour la description de l'objet d'annulation
	regenerateTextLabel();
}

/**
	@brief MoveConductorsTextsCommand::regenerateTextLabel
	Genere la description de l'objet d'annulation
*/
void MoveConductorsTextsCommand::regenerateTextLabel()
{
	QString moved_content_sentence = QET::ElementsAndConductorsSentence(0, 0, texts_to_move_.count());
	
	setText(
		QString(
			QObject::tr(
				"move %1",
				"undo caption - %1 is a sentence listing the moved content"
			).arg(moved_content_sentence)
		)
	);
}

/**
	@brief ChangeDiagramTextCommand::ChangeDiagramTextCommand
	Constructeur
	@param dti Champ de texte modifie
	@param before texte avant
	@param after texte apres
	@param parent QUndoCommand parent
*/
ChangeDiagramTextCommand::ChangeDiagramTextCommand(
	DiagramTextItem *dti,
	const QString &before,
	const QString &after,
	QUndoCommand *parent
) :
	QUndoCommand(QObject::tr("modify text", "undo caption"), parent),
	text_item(dti),
	text_before(before),
	text_after(after),
	first_redo(true),
	diagram(dti->diagram())
{
}

/**
	@brief ChangeDiagramTextCommand::~ChangeDiagramTextCommand
	destructeur
*/
ChangeDiagramTextCommand::~ChangeDiagramTextCommand()
{
}

/**
	@brief ChangeDiagramTextCommand::undo
	annule la modification de texte
*/
void ChangeDiagramTextCommand::undo()
{
	diagram -> showMe();
	text_item -> setHtml(text_before);
}

/**
	@brief ChangeDiagramTextCommand::redo
*/
void ChangeDiagramTextCommand::redo()
{
	diagram -> showMe();
	text_item->setHtml(text_after);
}

/**
	@brief ChangeConductorCommand::ChangeConductorCommand
	Constructeur
	@param c Conducteur modifie
	@param old_p ancien profil du conducteur
	@param new_p nouveau profil du conducteur
	@param path_t Trajectoire du trajet modifie
	@param parent QUndoCommand parent
*/
ChangeConductorCommand::ChangeConductorCommand(
	Conductor *c,
	const ConductorProfile &old_p,
	const ConductorProfile &new_p,
	Qt::Corner path_t,
	QUndoCommand *parent
) :
	QUndoCommand(QObject::tr("modify a conductor", "undo caption"), parent),
	conductor(c),
	old_profile(old_p),
	new_profile(new_p),
	path_type(path_t),
	first_redo(true),
	diagram (c->diagram())
{
}

/**
	@brief ChangeConductorCommand::~ChangeConductorCommand
	Destructeur
*/
ChangeConductorCommand::~ChangeConductorCommand()
{
}

/**
	@brief ChangeConductorCommand::undo
	Annule la modification du conducteur
*/
void ChangeConductorCommand::undo()
{
	diagram -> showMe();
	conductor -> setProfile(old_profile, path_type);
	conductor -> textItem() -> setPos(text_pos_before_mov_);
}

/**
	@brief ChangeConductorCommand::redo
	Refait la modification du conducteur
*/
void ChangeConductorCommand::redo()
{
	diagram -> showMe();
	if (first_redo) {
		first_redo = false;
	} else {
		conductor -> setProfile(new_profile, path_type);
		conductor -> textItem() -> setPos(text_pos_after_mov_);
	}
}

/**
	@brief ChangeConductorCommand::setConductorTextItemMove
	Integre dans cet objet d'annulation le repositionnement du champ de texte
	du conducteur
	@param pos_before Position du texte avant la modification du conducteur
	@param pos_after  Position du texte apres la modification du conducteur
*/
void ChangeConductorCommand::setConductorTextItemMove(const QPointF &pos_before, const QPointF &pos_after) {
	text_pos_before_mov_ = pos_before;
	text_pos_after_mov_  = pos_after;
}

/**
	@brief ResetConductorCommand::ResetConductorCommand
	@param cp
	@param parent
*/
ResetConductorCommand::ResetConductorCommand(
	const QHash<Conductor *, ConductorProfilesGroup> &cp,
	QUndoCommand *parent
) :
	QUndoCommand(parent),
	conductors_profiles(cp),
	diagram(cp.keys().first()->diagram())
{
	setText(
		QObject::tr(
			"Reset %1",
			"undo caption - %1 is a sentence listing the reset content"
		).arg(QET::ElementsAndConductorsSentence(0, cp.count()))
	);
}

/**
	@brief ResetConductorCommand::~ResetConductorCommand
*/
ResetConductorCommand::~ResetConductorCommand()
{
}

/**
	@brief ResetConductorCommand::undo
*/
void ResetConductorCommand::undo()
{
	diagram -> showMe();
	foreach(Conductor *c, conductors_profiles.keys()) {
		c -> setProfiles(conductors_profiles[c]);
	}
}

/**
	@brief ResetConductorCommand::redo
*/
void ResetConductorCommand::redo()
{
	diagram -> showMe();
	foreach(Conductor *c, conductors_profiles.keys()) {
		c -> textItem() -> forceMovedByUser  (false);
		c -> textItem() -> forceRotateByUser (false);
		c -> setProfiles(ConductorProfilesGroup());
	}
}

/**
	@brief ChangeBorderCommand::ChangeBorderCommand
	Constructeur
	@param dia Schema modifie
	@param old_bp Anciennes proprietes du cadre du schema
	@param new_bp Nouvelles proprietes du cadre du schema
	@param parent QUndoCommand parent
*/
ChangeBorderCommand::ChangeBorderCommand(Diagram *dia,
					 const BorderProperties &old_bp,
					 const BorderProperties &new_bp,
					 QUndoCommand *parent) :
	QUndoCommand(
		QObject::tr("change the sheet size", "undo caption"),
		parent),
	diagram(dia),
	old_properties(old_bp),
	new_properties(new_bp)
{
}

/**
	@brief ChangeBorderCommand::~ChangeBorderCommand
	Destructeur
*/
ChangeBorderCommand::~ChangeBorderCommand()
{
}

/**
	@brief ChangeBorderCommand::undo
	Annule les changements apportes au schema
*/
void ChangeBorderCommand::undo()
{
	diagram -> showMe();
	diagram -> border_and_titleblock.importBorder(old_properties);
}

/**
	@brief ChangeBorderCommand::redo
	Refait les changements apportes au schema
*/
void ChangeBorderCommand::redo()
{
	diagram -> showMe();
	diagram -> border_and_titleblock.importBorder(new_properties);
}
