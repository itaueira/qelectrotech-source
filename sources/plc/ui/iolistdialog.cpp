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
#include "iolistdialog.h"

#include "../../diagram.h"
#include "../../elementprovider.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetinformation.h"
#include "../../qetproject.h"
#include "../../undocommand/editiopointcommand.h"
#include "../ioassignment.h"
#include "../iolist.h"
#include "../iopoint.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUndoStack>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
		/**
			The columns of the tree. The first one is the tree column, so it
			carries the names of the three levels and, on a level, its count.
		*/
	enum Column
	{
		NameColumn = 0,
		TypeColumn,
		ChannelColumn,
		TagColumn,
		DescriptionColumn,
		AddressColumn,
		CommentColumn,
		ConnectColumn,
		LinkedColumn,
		ColumnCount
	};

		/// what a row of the tree stands for
	enum Kind
	{
		UnitKind = 0,
		CardKind,
		PointKind
	};

	const int KIND_ROLE  = Qt::UserRole + 1;
	const int ID_ROLE    = Qt::UserRole + 2;
	const int UUID_ROLE  = Qt::UserRole + 3;

	const int SCOPE_KIND_ROLE = Qt::UserRole + 1;
	const int SCOPE_KEY_ROLE  = Qt::UserRole + 2;

		/// what the scope combo asks for
	enum Scope
	{
		WholeProject = 0,
		OneUnit,
		OneCard
	};

	/**
		@brief isEditableColumn
		@param column a column of the tree
		@return true when @a column is one a person may type into

		The four fields the list is corrected on. Everything else - the
		channel, the type, the count - is decided elsewhere and typing over
		it here would only lie.
	*/
	bool isEditableColumn(int column)
	{
		return column == TagColumn
			|| column == DescriptionColumn
			|| column == AddressColumn
			|| column == CommentColumn;
	}

	/**
		@brief The ColumnDelegate class

		QTreeWidgetItem carries its flags for the whole row, so the editable
		flag cannot be given to four columns out of nine. The delegate is
		where the distinction fits: it hands back no editor at all for the
		columns that are read only, and the double click on them falls
		through to the jump.
	*/
	class ColumnDelegate : public QStyledItemDelegate
	{
		public:
			explicit ColumnDelegate(QObject *parent = nullptr) :
				QStyledItemDelegate(parent)
			{}

			QWidget *createEditor(QWidget *parent,
					      const QStyleOptionViewItem &option,
					      const QModelIndex &index) const override
			{
				if (!isEditableColumn(index.column())) {
					return nullptr;
				}
				return QStyledItemDelegate::createEditor(parent, option, index);
			}
	};
}

/**
	@brief IoListDialog::IoListDialog
	@param project the project whose I/O list is shown
	@param parent
*/
IoListDialog::IoListDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	buildWidgets();

	if (m_project)
	{
		if (QUndoStack *stack = m_project->undoStack()) {
				//there is no signal for the I/O list, and adding one to
				//QETProject would be a divergence from upstream for nothing:
				//every change to the list goes through the undo stack, so
				//the stack index is the same news, and it also catches the
				//undo, the redo, and an edit made from the element properties.
			connect(stack, &QUndoStack::indexChanged,
				this, &IoListDialog::stackChanged);
		}
		connect(m_project.data(), &QObject::destroyed,
			this, &QWidget::close);
	}

	reload();
}

/**
	@brief IoListDialog::project
	@return the project this window is reading, so the editor can find
	an already open window instead of opening a second one on the same
	project.
*/
QETProject *IoListDialog::project() const
{
	return m_project.data();
}

/**
	@brief IoListDialog::buildWidgets
*/
void IoListDialog::buildWidgets()
{
	setWindowTitle(tr("Liste des entrées / sorties"));

	QLabel *scope_label = new QLabel(tr("Niveau :"), this);
	m_scope = new QComboBox(this);
	m_scope->setToolTip(tr("Montre le projet entier, un automate, ou une "
			       "seule carte."));
	m_scope->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	connect(m_scope, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &IoListDialog::scopeChanged);

	QHBoxLayout *scope_line = new QHBoxLayout;
	scope_line->addWidget(scope_label);
	scope_line->addWidget(m_scope, 1);

	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(ColumnCount);
	m_tree->setHeaderLabels(QStringList()
				<< tr("E/S")
				<< tr("Type")
				<< tr("Voie")
				<< tr("Repère")
				<< tr("Description")
				<< tr("Adresse")
				<< tr("Commentaire")
				<< tr("Connecter à")
				<< tr("Élément lié"));
	m_tree->setRootIsDecorated(true);
	m_tree->setAlternatingRowColors(true);
	m_tree->setUniformRowHeights(true);
	m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_tree->setEditTriggers(QAbstractItemView::DoubleClicked
				| QAbstractItemView::SelectedClicked
				| QAbstractItemView::EditKeyPressed);
	m_tree->setItemDelegate(new ColumnDelegate(m_tree));
	m_tree->header()->setStretchLastSection(false);
	m_tree->header()->setSectionResizeMode(NameColumn, QHeaderView::Interactive);
	m_tree->header()->setSectionResizeMode(DescriptionColumn, QHeaderView::Stretch);
	connect(m_tree, &QTreeWidget::itemChanged,
		this, &IoListDialog::itemEdited);
	connect(m_tree, &QTreeWidget::itemDoubleClicked,
		this, &IoListDialog::itemJump);
	connect(m_tree, &QTreeWidget::itemSelectionChanged,
		this, &IoListDialog::selectionChanged);

	m_counts = new QLabel(this);
	m_counts->setWordWrap(true);
	m_counts->setTextFormat(Qt::PlainText);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	m_status->setTextFormat(Qt::PlainText);

	m_show = new QPushButton(tr("Montrer sur le folio"), this);
	m_show->setToolTip(tr("Ouvre le folio de la voie choisie et l'y met en "
			      "évidence, sans fermer cette fenêtre."));
	connect(m_show, &QPushButton::clicked, this, &IoListDialog::showSelected);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	buttons->addButton(m_show, QDialogButtonBox::ActionRole);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(scope_line);
	layout->addWidget(m_tree, 1);
	layout->addWidget(m_counts);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	resize(960, 560);
}

/**
	@brief IoListDialog::reload
	Read the project again, and put the whole tree back.
*/
void IoListDialog::reload()
{
	if (!m_project) {
		return;
	}

	m_loading = true;
	reloadCards();

	const IoList list = m_project->ioList();
	const IoTree::Tree tree = IoTree::build(list, m_card_data);

	reloadScopes(tree);
	fill(tree, list);
	m_loading = false;

	selectionChanged();
}

/**
	@brief IoListDialog::reloadCards
	Find every card of the project, and the controller each one names.
*/
void IoListDialog::reloadCards()
{
	m_cards.clear();
	m_card_data.clear();

	if (!m_project) {
		return;
	}

	ElementProvider ep(m_project);
	const QVector<QPointer<Element>> masters = ep.find(ElementData::Master);
	for (Element *elmt : masters)
	{
		if (!elmt) {
			continue;
		}
		if (elmt->elementData().m_master_type != ElementData::PLC) {
			continue;
		}
		const ElementData::PlcMasterData plc = elmt->elementData().plcMasterData();
		if (plc.ios.isEmpty()) {
			continue;
		}

		IoTree::Card card;
		card.uuid = elmt->uuid().toString();
		card.label = elmt->actualLabel();
		if (card.label.isEmpty()) {
			card.label = elmt->name();
		}
		card.unit = elmt->elementInformations()
				.value(QETInformation::ELMT_PLC_UNIT).toString();
		if (elmt->diagram()) {
			card.folio = QString::number(elmt->diagram()->folioIndex() + 1);
		}
		card.channels = int(plc.ios.count());

		m_cards << QPointer<Element>(elmt);
		m_card_data << card;
	}
}

/**
	@brief IoListDialog::reloadScopes
	@param tree the three levels, already built

	One entry for the project, one per controller, one per card. The choice
	that was made before is kept when it still exists, because the list is
	rebuilt at every change of the stack and losing the level at each undo
	would make the window unusable.
*/
void IoListDialog::reloadScopes(const IoTree::Tree &tree)
{
	int kind = WholeProject;
	QString key;
	if (m_scope->currentIndex() >= 0)
	{
		kind = m_scope->currentData(SCOPE_KIND_ROLE).toInt();
		key = m_scope->currentData(SCOPE_KEY_ROLE).toString();
	}

	const QSignalBlocker blocker(m_scope);
	m_scope->clear();

	m_scope->addItem(tr("Tout le projet"));
	m_scope->setItemData(0, int(WholeProject), SCOPE_KIND_ROLE);
	m_scope->setItemData(0, QString(), SCOPE_KEY_ROLE);

	for (const IoTree::UnitGroup &unit : tree.units)
	{
		int row = m_scope->count();
		m_scope->addItem(tr("Automate : %1").arg(unit.label));
		m_scope->setItemData(row, int(OneUnit), SCOPE_KIND_ROLE);
		m_scope->setItemData(row, unit.name.trimmed().toLower(), SCOPE_KEY_ROLE);

		for (const IoTree::CardGroup &card : unit.cards)
		{
			row = m_scope->count();
			m_scope->addItem(card.folio.isEmpty()
					 ? tr("    Carte : %1").arg(card.label)
					 : tr("    Carte : %1 (folio %2)")
						.arg(card.label, card.folio));
			m_scope->setItemData(row, int(OneCard), SCOPE_KIND_ROLE);
			m_scope->setItemData(row, card.uuid, SCOPE_KEY_ROLE);
		}
	}

	int found = 0;
	for (int i = 0; i < m_scope->count(); ++i)
	{
		if (m_scope->itemData(i, SCOPE_KIND_ROLE).toInt() == kind
		    && m_scope->itemData(i, SCOPE_KEY_ROLE).toString() == key)
		{
			found = i;
			break;
		}
	}
	m_scope->setCurrentIndex(found);
}

/**
	@brief IoListDialog::fill
	@param tree the three levels
	@param list the project's I/O list

	Puts in the tree what the scope asks for. The project shows the three
	levels, a controller shows itself and its cards, a card shows itself
	alone - that is the CU-11.9 exercise, and the counts have to add up
	across the three.
*/
void IoListDialog::fill(const IoTree::Tree &tree, const IoList &list)
{
	m_tree->clear();

	const int scope = m_scope->currentData(SCOPE_KIND_ROLE).toInt();
	const QString key = m_scope->currentData(SCOPE_KEY_ROLE).toString();

	int shown_total = 0;
	int shown_assigned = 0;

	for (const IoTree::UnitGroup &unit : tree.units)
	{
		if (scope == OneUnit && unit.name.trimmed().toLower() != key) {
			continue;
		}

		if (scope == OneCard)
		{
			for (const IoTree::CardGroup &card : unit.cards)
			{
				if (card.uuid != key) {
					continue;
				}
				addCard(nullptr, card, list);
				shown_total += card.total();
				shown_assigned += card.assigned(list);
			}
			continue;
		}

		addUnit(unit, list);
		shown_total += unit.total();
		shown_assigned += unit.assigned(list);
	}

	if (scope == WholeProject)
	{
		for (const IoTree::CardGroup &card : tree.missing)
		{
			addCard(nullptr, card, list);
			shown_total += card.total();
			shown_assigned += card.assigned(list);
		}

		if (!tree.cardless.isEmpty())
		{
			QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
			item->setData(0, KIND_ROLE, int(UnitKind));
			item->setText(NameColumn,
				      tr("Pas encore dans une carte (%1)")
					.arg(int(tree.cardless.count())));
			QFont font = item->font(NameColumn);
			font.setBold(true);
			item->setFont(NameColumn, font);
			item->setFlags(Qt::ItemIsEnabled);

			for (int index : tree.cardless) {
				addPoint(item, index, list);
			}
			item->setExpanded(true);
			shown_total += int(tree.cardless.count());
		}
	}

	m_tree->expandAll();
	for (int column = 0; column < ColumnCount; ++column)
	{
		if (column != DescriptionColumn) {
			m_tree->resizeColumnToContents(column);
		}
	}

	const int total = tree.total();
	const int assigned = tree.assigned(list);
	QString text = tr("Projet : %1 point(s), %2 dans une voie, %3 encore libre(s).")
			.arg(total).arg(assigned).arg(total - assigned);
	if (scope != WholeProject)
	{
		text += QLatin1Char(' ');
		text += tr("Affiché : %1 point(s), %2 dans une voie.")
				.arg(shown_total).arg(shown_assigned);
	}
	m_counts->setText(text);
}

/**
	@brief IoListDialog::addUnit
	@param unit the controller
	@param list the project's I/O list
*/
void IoListDialog::addUnit(const IoTree::UnitGroup &unit, const IoList &list)
{
	QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
	item->setData(0, KIND_ROLE, int(UnitKind));
	item->setText(NameColumn, tr("%1 (%2/%3)")
			.arg(unit.label)
			.arg(unit.assigned(list))
			.arg(unit.total()));
	QFont font = item->font(NameColumn);
	font.setBold(true);
	item->setFont(NameColumn, font);
	item->setFlags(Qt::ItemIsEnabled);

	for (const IoTree::CardGroup &card : unit.cards) {
		addCard(item, card, list);
	}
	item->setExpanded(true);
}

/**
	@brief IoListDialog::addCard
	@param parent the controller row, or nullptr to put the card at the top
	@param card the card
	@param list the project's I/O list
*/
void IoListDialog::addCard(QTreeWidgetItem *parent, const IoTree::CardGroup &card,
			   const IoList &list)
{
	QTreeWidgetItem *item = parent ? new QTreeWidgetItem(parent)
				       : new QTreeWidgetItem(m_tree);
	item->setData(0, KIND_ROLE, int(CardKind));
	item->setData(0, UUID_ROLE, card.uuid);

	QString label = card.label;
	if (!card.folio.isEmpty()) {
		label = tr("%1 (folio %2)").arg(label, card.folio);
	}
	item->setText(NameColumn, tr("%1 (%2/%3)")
			.arg(label)
			.arg(card.assigned(list))
			.arg(card.total()));

	QFont font = item->font(NameColumn);
	font.setBold(true);
	item->setFont(NameColumn, font);

	if (card.missing)
	{
		item->setForeground(NameColumn, QBrush(QColor(Qt::red)));
		item->setToolTip(NameColumn,
				 tr("Aucune carte de ce projet ne répond à cet "
				    "identifiant. Ses points sont montrés tout de "
				    "même, pour qu'ils ne disparaissent pas du "
				    "compte."));
		item->setFlags(Qt::ItemIsEnabled);
	}
	else
	{
		item->setText(ChannelColumn, tr("%n voie(s)", "", card.channels));
		item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
	}

	for (int index : card.points) {
		addPoint(item, index, list);
	}
	item->setExpanded(true);
}

/**
	@brief IoListDialog::addPoint
	@param parent the card row, or the row of the points with no card
	@param index the index of the point in @a list
	@param list the project's I/O list
*/
void IoListDialog::addPoint(QTreeWidgetItem *parent, int index, const IoList &list)
{
	if (index < 0 || index >= list.count()) {
		return;
	}
	const IoPoint point = list.at(index);

	QTreeWidgetItem *item = new QTreeWidgetItem(parent);
	item->setData(0, KIND_ROLE, int(PointKind));
	item->setData(0, ID_ROLE, point.id);

	item->setText(NameColumn, IoAssignment::pointLabel(point));
	item->setText(TypeColumn, ElementData::translatedPlcIOType(point.type));

		//the channel of the card wins over the one written in the point:
		//the card table can be renamed after the point took its voie, and
		//it is the card that the folio shows.
	QString channel = point.channel;
	if (point.isAssigned())
	{
		if (Element *master = cardByUuid(point.master_uuid))
		{
			const QString fresh = IoAssignment::channelName(
				master->elementData().plcMasterData().ios, point.io_index);
			if (!fresh.isEmpty()) {
				channel = fresh;
			}
		}
	}
	item->setText(ChannelColumn, channel);

	item->setText(TagColumn, point.tag);
	item->setText(DescriptionColumn, point.description);
	item->setText(AddressColumn, point.address);
	item->setText(CommentColumn, point.comment);
	item->setText(ConnectColumn, point.connect_to);

		//the last column is not what the sheet asked for, it is what is
		//there: the symbol wired to that voie of the card. Empty means the
		//voie is taken but nothing has been drawn in it yet, which is the
		//state a project is in between the assignment and the drawing.
	if (point.isAssigned())
	{
		if (Element *master = cardByUuid(point.master_uuid))
		{
			if (Element *slave = slaveOf(master, point.io_index))
			{
				QString label = slave->actualLabel();
				if (label.isEmpty()) {
					label = slave->name();
				}
				item->setText(LinkedColumn, label);
			}
		}
	}

	item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
}

/**
	@brief IoListDialog::scopeChanged
	Only the tree is put back: the cards and the levels have not moved.
*/
void IoListDialog::scopeChanged()
{
	if (m_loading || !m_project) {
		return;
	}

	const IoList list = m_project->ioList();
	const IoTree::Tree tree = IoTree::build(list, m_card_data);

	m_loading = true;
	fill(tree, list);
	m_loading = false;

	selectionChanged();
}

/**
	@brief IoListDialog::stackChanged
	Something was done, undone or redone. The list may have moved with it.
*/
void IoListDialog::stackChanged()
{
	if (m_loading) {
		return;
	}
	reload();
}

/**
	@brief IoListDialog::selectionChanged
*/
void IoListDialog::selectionChanged()
{
	QTreeWidgetItem *item = m_tree->currentItem();
	bool showable = false;
	if (item)
	{
		const int kind = item->data(0, KIND_ROLE).toInt();
		showable = (kind == CardKind)
			|| (kind == PointKind && !item->text(ChannelColumn).isEmpty());
	}
	m_show->setEnabled(showable);
}

/**
	@brief IoListDialog::itemEdited
	@param item the row that was typed into
	@param column the column that was typed into

	The point of the project list and the row of the card say the same thing
	twice, so both are moved, in one entry of the undo. Only the cell that
	was typed into is carried across: a comment written here does not
	overwrite a function text somebody wrote in the element properties.
*/
void IoListDialog::itemEdited(QTreeWidgetItem *item, int column)
{
	if (m_loading || !item || !m_project) {
		return;
	}
	if (item->data(0, KIND_ROLE).toInt() != PointKind) {
		return;
	}
	if (!isEditableColumn(column)) {
		return;
	}

	IoList list = m_project->ioList();
	const int index = list.indexOfId(item->data(0, ID_ROLE).toString());
	if (index < 0) {
		say(tr("Ce point n'est plus dans la liste."), true);
		return;
	}

	IoPoint point = list.point(index);
	const QString typed = item->text(column).trimmed();

	QString *cell = nullptr;
	switch (column)
	{
		case TagColumn:         cell = &point.tag; break;
		case DescriptionColumn: cell = &point.description; break;
		case AddressColumn:     cell = &point.address; break;
		case CommentColumn:     cell = &point.comment; break;
		default: return;
	}
	if (*cell == typed) {
			//the editor was opened and closed without a change, or only the
			//spaces around the text moved. Nothing goes on the stack for that.
		return;
	}
	*cell = typed;
	list.setPoint(index, point);

		//the card half
	Element *master = point.isAssigned() ? cardByUuid(point.master_uuid) : nullptr;
	ElementData data;
	if (master)
	{
		data = master->elementData();
		ElementData::PlcMasterData plc = data.plcMasterData();
		if (point.io_index >= 0 && point.io_index < plc.ios.count())
		{
			ElementData::PlcIO &row = plc.ios[point.io_index];
			switch (column)
			{
				case TagColumn:
				case DescriptionColumn:
					row.functionText = IoAssignment::functionTextOf(point);
					break;
				case AddressColumn:
					row.address = point.address;
					break;
				case CommentColumn:
					row.comment = point.comment;
					break;
				default: break;
			}
			data.setPlcMasterData(plc);
		}
		else
		{
				//the point names a voie the card no longer has
			master = nullptr;
		}
	}

	QUndoStack *stack = m_project->undoStack();
	if (!stack) {
		say(tr("Ce projet n'a pas de pile d'annulation."), true);
		return;
	}

	m_loading = true;
	stack->push(new EditIoPointCommand(m_project, master, list, data,
					   IoAssignment::pointLabel(point)));
	m_loading = false;

	say(master
	    ? tr("« %1 » modifié, sur la carte comme dans la liste.")
			.arg(IoAssignment::pointLabel(point))
	    : tr("« %1 » modifié dans la liste.")
			.arg(IoAssignment::pointLabel(point)));

		//the rebuild waits for the event loop: taking the rows away from
		//under a QTreeWidget while it is still finishing its own itemChanged
		//is how a list like this crashes.
	QTimer::singleShot(0, this, &IoListDialog::reload);
}

/**
	@brief IoListDialog::itemJump
	@param item the row that was double clicked
	@param column the column that was double clicked
*/
void IoListDialog::itemJump(QTreeWidgetItem *item, int column)
{
	if (!item) {
		return;
	}
	if (item->data(0, KIND_ROLE).toInt() == PointKind
	    && isEditableColumn(column))
	{
			//that double click is opening an editor, not asking for a folio
		return;
	}
	showItem(item);
}

/**
	@brief IoListDialog::showSelected
*/
void IoListDialog::showSelected()
{
	showItem(m_tree->currentItem());
}

/**
	@brief IoListDialog::showItem
	@param item the row to show on a folio

	A card shows itself. A point shows the symbol drawn on its voie, and
	falls back to the card when nothing has been drawn there yet - a point
	that has taken a voie without a symbol is the ordinary state of a list
	that has just been imported.
*/
void IoListDialog::showItem(QTreeWidgetItem *item)
{
	if (!item || !m_project) {
		return;
	}

	const int kind = item->data(0, KIND_ROLE).toInt();
	if (kind == CardKind)
	{
		Element *card = cardByUuid(item->data(0, UUID_ROLE).toString());
		if (!card) {
			say(tr("Cette carte n'est pas dans le projet."), true);
			return;
		}
		showElement(card);
		say(tr("Carte %1.").arg(item->text(NameColumn)));
		return;
	}
	if (kind != PointKind) {
		return;
	}

	const IoList list = m_project->ioList();
	const int index = list.indexOfId(item->data(0, ID_ROLE).toString());
	if (index < 0) {
		say(tr("Ce point n'est plus dans la liste."), true);
		return;
	}
	const IoPoint point = list.at(index);
	const QString label = IoAssignment::pointLabel(point);

	if (!point.isAssigned()) {
		say(tr("« %1 » n'est encore dans aucune carte : il n'y a rien à "
		       "montrer sur un folio.").arg(label), true);
		return;
	}

	Element *master = cardByUuid(point.master_uuid);
	if (!master) {
		say(tr("La carte de « %1 » n'est pas dans ce projet.").arg(label), true);
		return;
	}

	Element *slave = slaveOf(master, point.io_index);
	showElement(slave ? slave : master);

	const QString channel = item->text(ChannelColumn);
	if (slave) {
		say(tr("« %1 » : voie %2.").arg(label, channel));
	} else {
		say(tr("« %1 » occupe la voie %2, mais rien n'y est encore "
		       "dessiné : c'est la carte qui est montrée.")
			.arg(label, channel));
	}
}

/**
	@brief IoListDialog::showElement
	@param element the element to bring up and light up
*/
void IoListDialog::showElement(Element *element)
{
		//setHighlighted stays on until somebody puts it out, so the jump
		//before this one has to be cleared here or the folio ends up with
		//every voie ever visited lit at once.
	if (m_showed && m_showed.data() != element) {
		m_showed.data()->setHighlighted(false);
	}
	m_showed = element;

	if (!element || !element->diagram()) {
		return;
	}

	element->diagram()->showMe();
	element->setHighlighted(true);

		//showMe brings the folio to the front but leaves it scrolled where it
		//was, and a voie is rarely at the top left of its folio.
	const QList<QGraphicsView *> views = element->diagram()->views();
	for (QGraphicsView *view : views) {
		view->centerOn(element);
	}
}

/**
	@brief IoListDialog::cardByUuid
	@param uuid the uuid a point carries in master_uuid
	@return the card element that answers to it, nullptr when none does
*/
Element *IoListDialog::cardByUuid(const QString &uuid) const
{
	for (int i = 0; i < int(m_card_data.count()); ++i)
	{
		if (m_card_data.at(i).uuid == uuid) {
			return m_cards.at(i).data();
		}
	}
	return nullptr;
}

/**
	@brief IoListDialog::slaveOf
	@param master a card
	@param io_index the row of its table
	@return the element drawn on that row, nullptr when nothing is drawn

	The card owns the map, so the question is asked of it and not of the
	slaves: Element::groupIndexForElement gives back the row a linked
	element was put on.
*/
Element *IoListDialog::slaveOf(Element *master, int io_index) const
{
	if (!master || io_index < 0) {
		return nullptr;
	}

	const QList<Element *> linked = master->linkedElements();
	for (Element *elmt : linked)
	{
		if (!elmt) {
			continue;
		}
		if (master->groupIndexForElement(elmt) == io_index) {
			return elmt;
		}
	}
	return nullptr;
}

/**
	@brief IoListDialog::say
	@param message what to write under the tree
	@param problem true to write it in red
*/
void IoListDialog::say(const QString &message, bool problem)
{
	if (!m_status) {
		return;
	}

	m_status->setText(message);

	QPalette status_palette = m_status->palette();
	status_palette.setColor(QPalette::WindowText,
				problem ? QColor(Qt::red)
					: palette().color(QPalette::WindowText));
	m_status->setPalette(status_palette);
}
