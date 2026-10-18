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
#include "terminalstriptreedockwidget.h"
#include "ui_terminalstriptreedockwidget.h"

#include "../UndoCommand/addterminaltostripcommand.h"
#include "../../elementprovider.h"
#include "../physicalterminal.h"
#include "../../qeticons.h"
#include "../../qetproject.h"
#include "../realterminal.h"
#include "../../qetgraphicsitem/terminalelement.h"
#include "../terminalstrip.h"
#include "../../qetinformation.h"
#include "../../utils/qetutils.h"
#include "freeterminalmodel.h"

#include <QApplication>
#include <QDrag>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QObject>
#include <QUuid>

namespace
{
	/**
		The label of @a real_terminal as this tree has to show it: what the
		terminal carries, or, when it carries nothing, where it is drawn.

		A terminal with no label is not a misreading. End stops and spare
		terminals go on the rail without a number, and the field is simply
		empty in the project file - measured on a delivered project of 434
		components: of its 118 terminals, 61 carry no label at all, every one
		of those 61 is an end stop, and their whole elementInformations node
		is empty. Nothing is lost on the way here either, and that is worth
		writing down because it is the first suspicion: both branches of this
		tree read the same Element::actualLabel(), through
		RealTerminal::label(), so a label that reaches one of them reaches
		the other.

		What is a defect is a tree that shows those terminals as nothing at
		all. Rows all blank, none of them tellable from the next, in a tree
		whose only job is to have one of them picked - and the button that
		moves a terminal into a strip asks for exactly that pick.

		So the stand in carries the cross reference, and it carries it
		because that is the string the free terminal table already shows in a
		column of its own: a row of this tree and a row of that table are
		then recognisably the same terminal, which is what the move needs.

		It is written between parentheses and says in words that there is no
		label. A bare "3-B4" would read as a label, be looked for on the
		sheet, and not be there - a name that the drawing does not carry
		costs more than the blank row did.

		Display only, on purpose. RealTerminal::label() is what the strip
		drawing, the sorting and the undo texts read, and it goes on
		answering empty: a stand in printed on the rail would be a lie on
		paper.
	*/
	QString listLabel(const QSharedPointer<RealTerminal> &real_terminal)
	{
		if (real_terminal.isNull()) {
			return QObject::tr("(sans repère)");
		}

		const auto label_ = real_terminal->label();
		if (!label_.isEmpty()) {
			return label_;
		}

		const auto xref_ = real_terminal->Xref();
		return xref_.isEmpty()
				? QObject::tr("(sans repère)")
				: QObject::tr("(sans repère — %1)").arg(xref_);
	}
}

TerminalStripTreeDockWidget::TerminalStripTreeDockWidget(QETProject *project, QWidget *parent) :
	QDockWidget(parent),
    ui(new Ui::TerminalStripTreeDockWidget)
{
	ui->setupUi(this);
    setProject(project);

	ui->m_tree_view->expandRecursively(ui->m_tree_view->rootIndex());

		//Free terminals are dragged from this tree or from the free terminal
		//table and dropped on a strip. The tree items themselves never move,
		//the tree is rebuilt from the project after the drop.
	ui->m_tree_view->viewport()->setAcceptDrops(true);
	ui->m_tree_view->viewport()->installEventFilter(this);
}

TerminalStripTreeDockWidget::~TerminalStripTreeDockWidget()
{
	delete ui;
}

/**
 * @brief TerminalStripTreeDockWidget::setProject
 * Set @project as project handled by this tree dock.
 * If a previous project was setted, everything is clear.
 * This function track the destruction of the project,
 * that  mean if the project pointer is deleted
 * no need to call this function with a nullptr,
 * everything is made inside this class.
 * @param project
 */
void TerminalStripTreeDockWidget::setProject(QETProject *project)
{
    if(m_project && m_project_destroy_connection) {
        disconnect(m_project_destroy_connection);
    }
    m_project = project;
    if (m_project) {
            //`this` as context: this dock can be deleted before the project
            //(with the editor window that owns it), and the connection must go with it
        m_project_destroy_connection = connect(m_project, &QObject::destroyed, this, [this](){
            this->m_current_strip.clear();
            this->reload();
        });
    }
    m_current_strip.clear();
    reload();
}

/**
 * @brief TerminalStripTreeDockWidget::reload
 */
void TerminalStripTreeDockWidget::reload()
{
	auto current_ = m_current_strip;
		//Read before the clear below, which resets it
	const bool current_is_free_ = m_current_is_free_terminal;

	ui->m_tree_view->clear();
	m_item_strip_H.clear();
	m_uuid_terminal_H.clear();
	m_uuid_strip_H.clear();

	for (const auto &connection_ : std::as_const(m_strip_changed_connection)) {
		disconnect(connection_);
	}
	m_strip_changed_connection.clear();


	buildTree();

	ui->m_tree_view->expandRecursively(ui->m_tree_view->rootIndex());

		//Reselect the tree widget item of the current edited strip
   auto item = m_item_strip_H.key(current_);
   if (item) {
	   ui->m_tree_view->setCurrentItem(item);
   }
		//Or the independent terminals, when they were what was shown: moving
		//some of them into a strip reloads this tree, and the next ones are
		//usually moved right after, into the next strip. Left with no
		//current item, the window showed an empty page instead.
   else if (current_is_free_) {
	   if (auto free_item = ui->m_tree_view->topLevelItem(1)) {
		   ui->m_tree_view->setCurrentItem(free_item);
	   }
   }
}

/**
 * @brief TerminalStripTreeDockWidget::currentIsStrip
 * @return true if the current selected item is a terminal strip.
 */
bool TerminalStripTreeDockWidget::currentIsStrip() const {
	return m_item_strip_H.contains(ui->m_tree_view->currentItem());
}

/**
 * @brief TerminalStripTreeDockWidget::currentStrip
 * @return The current selected strip or nullptr if there is
 * no strip selected;
 */
TerminalStrip *TerminalStripTreeDockWidget::currentStrip() const {
	return m_current_strip;
}

/**
 * @brief TerminalStripTreeDockWidget::currentInstallation
 * @return the installation according to the current selection
 */
QString TerminalStripTreeDockWidget::currentInstallation() const
{
	if (m_current_strip) {
		return m_current_strip->installation();
	}

	if (auto item = ui->m_tree_view->currentItem())
	{
		if (item->type() == Location) {
			item = item->parent();
		}
		if (item->type() == Installation) {
			return item->data(0, Qt::DisplayRole).toString();
		}
	}

	return QString();
}

/**
 * @brief TerminalStripTreeDockWidget::currentLocation
 * @return the location according to the current selection
 */
QString TerminalStripTreeDockWidget::currentLocation() const
{
	if (m_current_strip) {
		return m_current_strip->location();
	}

	if (auto item = ui->m_tree_view->currentItem()) {
		if (item->type() == Location) {
			return item->data(0, Qt::DisplayRole).toString();
		}
	}

	return QString();
}

/**
 * @brief TerminalStripTreeDockWidget::currentIsFreeTerminals
 * @return true if the current item is the "Independent terminals" item,
 * or one of the terminals listed under it
 */
bool TerminalStripTreeDockWidget::currentIsFreeTerminals() const {
	return m_current_is_free_terminal;
}

/**
 * @brief TerminalStripTreeDockWidget::setSelectedStrip
 * @param strip
 */
void TerminalStripTreeDockWidget::setSelectedStrip(TerminalStrip *strip) {
	ui->m_tree_view->setCurrentItem(m_item_strip_H.key(strip));
}

/**
 * @brief TerminalStripTreeDockWidget::currentRealTerminal
 * @return the current real terminal or a null QSharedPointer.
 */
QSharedPointer<RealTerminal> TerminalStripTreeDockWidget::currentRealTerminal() const
{
	if (auto item = ui->m_tree_view->currentItem()) {
		if (item->type() == Terminal) {
			return m_uuid_terminal_H.value(item->data(0,UUID_USER_ROLE).toUuid());
		}
	}
	return QSharedPointer<RealTerminal>();
}

/**
 * @brief TerminalStripTreeDockWidget::on_m_tree_view_currentItemChanged
 * @param current
 * @param previous
 */
void TerminalStripTreeDockWidget::on_m_tree_view_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous)

	if (!current) {
		m_current_is_free_terminal = false;
		setCurrentStrip(nullptr);
		return;
	}

	TerminalStrip *strip_ = nullptr;
	bool current_is_free{false};
	const auto current_type{current->type()};
	if (current_type == Strip) {
		strip_ = m_item_strip_H.value(current);
	}
	else if (current_type == Terminal && current->parent())
	{
		const auto parent_type{current->parent()->type()};
		if (parent_type == Strip) {
			strip_ = m_item_strip_H.value(current->parent());
		} else if (parent_type == FreeTerminal) {
			current_is_free = true;
		}
	}
		//The "Independent terminals" item itself: the natural place to click
		//to see them, and it used to show nothing at all
	else if (current_type == FreeTerminal) {
		current_is_free = true;
	}

		//The flag must follow every selection change, or a reload of the tree
		//(e.g. after moving a free terminal) leaves it stale and the next
		//click on a free terminal shows nothing (#1306)
	if (strip_ != m_current_strip || current_is_free != m_current_is_free_terminal) {
		m_current_is_free_terminal = current_is_free;
		setCurrentStrip(strip_);
	}
}

/**
 * @brief TerminalStripTreeDockWidget::buildTree
 */
void TerminalStripTreeDockWidget::buildTree()
{
    if(!m_project) {
        return;
    }

	auto title_ = m_project->title();
	if (title_.isEmpty()) {
		title_ = tr("Untitled project");
	}

	QStringList strl{title_};
	new QTreeWidgetItem(ui->m_tree_view, strl, Root);

	QStringList ftstrl(tr("Independent terminals"));
	new QTreeWidgetItem(ui->m_tree_view, ftstrl, FreeTerminal);

	auto ts_vector = m_project->terminalStrip();
	std::sort(ts_vector.begin(), ts_vector.end(), [](TerminalStrip *a, TerminalStrip *b) {
		return QETUtils::naturalLessThan(a->name(), b->name());
	});

	for (const auto &ts : std::as_const(ts_vector)) {
		addTerminalStrip(ts);
	}
	addFreeTerminal();
}

QTreeWidgetItem* TerminalStripTreeDockWidget::addTerminalStrip(TerminalStrip *terminal_strip)
{
	if (auto item = m_item_strip_H.key(terminal_strip)) {
		return item;
	}

	auto root_item = ui->m_tree_view->topLevelItem(0);

		//Check if installation already exist
		//if not create a new one
	auto installation_str = terminal_strip->installation();
	QTreeWidgetItem *inst_qtwi = nullptr;
	for (int i = 0 ; i<root_item->childCount() ; ++i) {
		auto child_inst = root_item->child(i);
		if (child_inst->data(0, Qt::DisplayRole).toString() == installation_str) {
			inst_qtwi = child_inst;
			break;
		}
	}
	if (!inst_qtwi) {
		QStringList inst_strl{installation_str};
		inst_qtwi = new QTreeWidgetItem(root_item, inst_strl, Installation);
	}

		//Check if location already exist
		//if not create a new one
	auto location_str = terminal_strip->location();
	QTreeWidgetItem *loc_qtwi = nullptr;
	for (int i = 0 ; i<inst_qtwi->childCount() ; ++i) {
		auto child_loc = inst_qtwi->child(i);
		if (child_loc->data(0, Qt::DisplayRole).toString() == location_str) {
			loc_qtwi = child_loc;
			break;
		}
	}
	if (!loc_qtwi) {
		QStringList loc_strl{location_str};
		loc_qtwi = new QTreeWidgetItem(inst_qtwi, loc_strl, Location);
	}

		//Add the terminal strip
	QStringList name{terminal_strip->name()};
	auto strip_item = new QTreeWidgetItem(loc_qtwi, name, Strip);
	strip_item->setData(0, UUID_USER_ROLE, terminal_strip->uuid());
	strip_item->setIcon(0, QET::Icons::TerminalStrip);

		//Add child terminal of the strip
	for (auto i=0 ; i<terminal_strip->physicalTerminalCount() ; ++i)
	{
		auto phy_t = terminal_strip->physicalTerminal(i);
		if (phy_t->realTerminalCount())
		{
			QString text_;
			for (const auto &real_t : phy_t->realTerminals())
			{
				if (text_.isEmpty())
					text_ = listLabel(real_t);
				else
					text_.append(QStringLiteral(", ")).append(listLabel(real_t));
			}
			const auto real_t = phy_t->realTerminals().at(0);
			auto terminal_item = new QTreeWidgetItem(strip_item, QStringList(text_), Terminal);
			terminal_item->setData(0, UUID_USER_ROLE, phy_t->uuid());
			terminal_item->setIcon(0, QET::Icons::ElementTerminal);
		}
	}

	m_item_strip_H.insert(strip_item, terminal_strip);
	m_uuid_strip_H.insert(terminal_strip->uuid(), terminal_strip);

	m_strip_changed_connection.append(connect(terminal_strip, &TerminalStrip::orderChanged, this, &TerminalStripTreeDockWidget::reload));
	return strip_item;
}

/**
 * @brief TerminalStripTreeDockWidget::addFreeTerminal
 */
void TerminalStripTreeDockWidget::addFreeTerminal()
{
	ElementProvider ep(m_project);
	auto vector_ = ep.freeTerminal();

	if (vector_.isEmpty()) {
		return;
	}

		//What each of them is going to be listed as, worked out once.
		//Once because the stand in of a terminal with no label asks the
		//folio where the terminal is drawn, which is not free, and a
		//comparison function is called a good many more times than there
		//are terminals.
	QHash<TerminalElement *, QString> shown_;
	for (const auto terminal : std::as_const(vector_)) {
		shown_.insert(terminal, listLabel(terminal->realTerminal()));
	}

		//Sort the terminal element by what the tree shows of it.
		//Sorting on the stored label instead leaves every terminal that has
		//none under the same key, and std::sort is free to order equal keys
		//as it likes: the unlabelled ones would come back in a different
		//order from one opening of this dock to the next, which is no way to
		//point at one of them twice. Sorting on the displayed string keeps
		//the labelled terminals exactly where they were - for them the two
		//strings are the same - and settles the others by folio and
		//position.
		//
		//The displayed string is not enough on its own, and measuring said
		//so: the cross reference names a cell of the folio grid, and the
		//terminals of one rail are drawn closer together than a cell is
		//wide - as many as four of them under one and the same string on a
		//delivered project. So equal strings are settled the way the sheet
		//draws them, top to bottom then left to right, and what is still
		//equal after that by the uuid, which does not move between
		//sessions. The lines of such a group go on reading alike, which is
		//the cross reference talking and not the sorting; what this buys is
		//that the second line of the group is the same terminal every time
		//the dock is opened.
	std::sort(vector_.begin(), vector_.end(), [&shown_](TerminalElement *a, TerminalElement *b)
	{
		const auto label_a = shown_.value(a);
		const auto label_b = shown_.value(b);
		if (label_a != label_b) {
			return QETUtils::naturalLessThan(label_a, label_b);
		}

		const auto pos_a = a->scenePos();
		const auto pos_b = b->scenePos();
		if (pos_a.y() != pos_b.y()) {
			return pos_a.y() < pos_b.y();
		}
		if (pos_a.x() != pos_b.x()) {
			return pos_a.x() < pos_b.x();
		}

		return a->uuid() < b->uuid();
	});

	auto free_terminal_item = ui->m_tree_view->topLevelItem(1);

	for (const auto terminal : std::as_const(vector_))
	{
		QUuid uuid_ = terminal->uuid();
		QStringList strl{shown_.value(terminal)};
		auto item = new QTreeWidgetItem(free_terminal_item, strl, Terminal);
		item->setData(0, UUID_USER_ROLE, uuid_.toString());
		item->setIcon(0, QET::Icons::ElementTerminal);

		m_uuid_terminal_H.insert(uuid_, terminal->realTerminal());
	}
}

void TerminalStripTreeDockWidget::setCurrentStrip(TerminalStrip *strip)
{
	m_current_strip = strip;
	emit currentStripChanged(strip);
}

/**
 * @brief TerminalStripTreeDockWidget::setDropCheck
 * @param check : called before accepting a drop of free terminals,
 * a drop is refused when it returns false.
 */
void TerminalStripTreeDockWidget::setDropCheck(std::function<bool ()> check) {
	m_drop_check = check;
}

/**
 * @brief TerminalStripTreeDockWidget::eventFilter
 * Drag a free terminal of the tree, and drop free terminals
 * (from the tree or from the free terminal table) on a strip.
 */
bool TerminalStripTreeDockWidget::eventFilter(QObject *watched, QEvent *event)
{
	if (watched != ui->m_tree_view->viewport()) {
		return QDockWidget::eventFilter(watched, event);
	}

	switch (event->type())
	{
		case QEvent::MouseButtonPress:
		{
			auto me = static_cast<QMouseEvent *>(event);
			m_drag_uuid = QUuid();
			auto item = ui->m_tree_view->itemAt(me->pos());
			if (me->button() == Qt::LeftButton
				&& item
				&& item->type() == Terminal
				&& item->parent()
				&& item->parent()->type() == FreeTerminal)
			{
				m_drag_start_pos = me->pos();
				m_drag_uuid = item->data(0, UUID_USER_ROLE).toUuid();
			}
			break;
		}
		case QEvent::MouseMove:
		{
			auto me = static_cast<QMouseEvent *>(event);
			if (!m_drag_uuid.isNull()
				&& (me->buttons() & Qt::LeftButton)
				&& (me->pos() - m_drag_start_pos).manhattanLength() >= QApplication::startDragDistance())
			{
				auto drag = new QDrag(ui->m_tree_view);
				drag->setMimeData(FreeTerminalModel::mimeDataForUuids({m_drag_uuid}));
				drag->setPixmap(QET::Icons::ElementTerminal.pixmap(16, 16));
				m_drag_uuid = QUuid();
				drag->exec(Qt::CopyAction);
				return true;
			}
			break;
		}
		case QEvent::DragEnter:
		{
				//Accepted wherever it enters, or no DragMove follows to reach a strip
			auto de = static_cast<QDragEnterEvent *>(event);
			if (!freeTerminals(de->mimeData()).isEmpty()) {
				de->acceptProposedAction();
			} else {
				de->ignore();
			}
			return true;
		}
		case QEvent::DragMove:
		{
			auto de = static_cast<QDragMoveEvent *>(event);
			if (dropAllowed(de->mimeData(), de->position().toPoint())) {
				de->acceptProposedAction();
			} else {
				de->ignore();
			}
			return true;
		}
		case QEvent::Drop:
		{
			auto de = static_cast<QDropEvent *>(event);
			const auto pos = de->position().toPoint();
			if (!dropAllowed(de->mimeData(), pos)) {
				de->ignore();
				return true;
			}
			auto strip = stripAt(pos);
			const auto terminals = freeTerminals(de->mimeData());
			de->acceptProposedAction();

				//The command rebuilds the tree (TerminalStrip::orderChanged)
			m_project->undoStack()->push(new AddTerminalToStripCommand(terminals, strip));
			setSelectedStrip(strip);
			return true;
		}
		default:
			break;
	}

	return QDockWidget::eventFilter(watched, event);
}

/**
 * @brief TerminalStripTreeDockWidget::stripAt
 * @param pos : position in the viewport of the tree
 * @return the strip of the item at @a pos, the item being the strip
 * or one of its terminals, or nullptr.
 */
TerminalStrip *TerminalStripTreeDockWidget::stripAt(const QPoint &pos) const
{
	auto item = ui->m_tree_view->itemAt(pos);
	if (item && item->type() == Terminal) {
		item = item->parent();
	}
	if (item && item->type() == Strip) {
		return m_item_strip_H.value(item);
	}
	return nullptr;
}

/**
 * @brief TerminalStripTreeDockWidget::freeTerminals
 * @param mime_data
 * @return the free terminals carried by @a mime_data
 * which are still free in the project.
 */
QVector<QSharedPointer<RealTerminal>> TerminalStripTreeDockWidget::freeTerminals(const QMimeData *mime_data) const
{
	QVector<QSharedPointer<RealTerminal>> terminals;
	for (const auto &uuid : FreeTerminalModel::uuidsFromMimeData(mime_data))
	{
		const auto real_t = m_uuid_terminal_H.value(uuid);
		if (real_t && !real_t->parentStrip() && !terminals.contains(real_t)) {
			terminals.append(real_t);
		}
	}
	return terminals;
}

/**
 * @brief TerminalStripTreeDockWidget::dropAllowed
 * @return true if @a mime_data carries free terminals
 * and there is a strip at @a pos to drop them in.
 */
bool TerminalStripTreeDockWidget::dropAllowed(const QMimeData *mime_data, const QPoint &pos) const
{
	return m_project
			&& stripAt(pos)
			&& !freeTerminals(mime_data).isEmpty()
			&& (!m_drop_check || m_drop_check());
}
