/*
		Copyright 2006-2026 QElectroTech Team
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
#include "propertieseditorfactory.h"

#include "../PropertiesEditor/propertieseditorwidget.h"
#include "../qetgraphicsitem/ViewItem/projectdbmodel.h"
#include "../qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "../qetgraphicsitem/ViewItem/ui/graphicstablepropertieseditor.h"
#include "../qetgraphicsitem/ViewItem/ui/projectdbmodelpropertieswidget.h"
#include "../qetgraphicsitem/diagramimageitem.h"
#include "../qetgraphicsitem/dynamicelementtextitem.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/elementtextitemgroup.h"
#include "../qetgraphicsitem/independenttextitem.h"
#include "../qetgraphicsitem/qetshapeitem.h"
#include "../ui/dynamicelementtextitemeditor.h"
#include "../ui/elementpropertieswidget.h"
#include "../ui/imagepropertieswidget.h"
#include "../ui/inditextpropertieswidget.h"
#include "../ui/shapegraphicsitempropertieswidget.h"

#include <QGraphicsItem>

/**
	@brief PropertiesEditorFactory::propertiesEditor
	@param model : the model to be edited
	@param editor :
	if the properties editor to be created is the same class as editor,
	the this function set item as edited item of editor and return editor
	@param parent : parent widget of the returned editor
	@return an editor or nullptr

	@par Only a ProjectDBModel gets an editor
	ProjectDBModelPropertiesWidget is the only editor this overload can
	build, and it reads its model as a ProjectDBModel : identifier(),
	queryString() and setQuery() reach private members at fixed offsets,
	and setQuery() then walks m_project->dataBase().
	QetGraphicsTableItem::setModel is public and accepts any
	QAbstractItemModel, so the model handed over here is not always the one
	built by QetGraphicsTableFactory.
	The cast is therefore checked : qobject_cast answers null both for a
	foreign model and for a table that carries no model yet, and both leave
	with nullptr, which the only caller already expects and handles.
	Price : a table showing a model this factory does not know gets no
	query editor at all in the properties panel, only the position, size
	and link part of it, and its query cannot be edited from there. That is
	already what a table without a model shows, and there is no other
	editor to offer : the one that exists cannot read a model whose layout
	it does not know, and used to write into it on the first click.

	@par The class name comparison is left as it is
	className() returns a pointer into the string data of the metaobject it
	belongs to, so comparing the two const char * below compares addresses,
	not text. Two classes never share that address, so equality means
	editor->metaObject() is ProjectDBModelPropertiesWidget's own
	metaobject and the cast that follows is exact ; a derived class answers
	a different address and only loses the reuse, which costs one new
	widget.
	Price : the guard is an address comparison that reads like a text one,
	so it stays sound for a reason its own spelling does not show. Making
	it compare text, the way the item overload below does through a
	QString, would let a derived class match and the cast would be
	unchecked again.
*/
PropertiesEditorWidget *PropertiesEditorFactory::propertiesEditor(
		QAbstractItemModel *model,
		PropertiesEditorWidget *editor,
		QWidget *parent)
{
	if (auto m = qobject_cast<ProjectDBModel *>(model))
	{
		if (editor &&
			editor->metaObject()->className()
				== ProjectDBModelPropertiesWidget::staticMetaObject.className())
		{
			static_cast<ProjectDBModelPropertiesWidget *>(editor)->setModel(m);
			return editor;
		}
		return new ProjectDBModelPropertiesWidget(m, parent);
	}
	return nullptr;
}

/**
	@brief propertiesEditor
	@param items : The items to be edited
	@param editor :
	If the properties editor to be created is the same class as editor,
	then this function set item as edited item of editor and return editor
	@param parent : parent widget of the returned editor
	@return : an editor or nullptr;
*/
PropertiesEditorWidget *PropertiesEditorFactory::propertiesEditor(
		QList<QGraphicsItem *> items,
		PropertiesEditorWidget *editor,
		QWidget *parent)
{
	const int count_ = items.size();
	if (count_ == 0) {
		return nullptr;
	}
	QGraphicsItem *item = items.first();
	const int type_ = item->type();

		//The editor widget can only edit one item
		//or several items of the same type
	for (auto qgi : items) {
		if (qgi->type() != type_) {
			return nullptr;
		}
	}

	QString class_name;
	if (editor) {
		class_name = editor->metaObject()->className();
	}

	switch (type_)
	{
		case Element::Type: //1000
		{
			if (count_ > 1) {
				return nullptr;
			}
			auto elmt = static_cast<Element*>(item);
			//auto created_editor = new ElementPropertiesWidget(elmt, parent);

				//We already edit an element, just update the editor with a new element
			if (class_name == ElementPropertiesWidget::staticMetaObject.className())
			{
				static_cast<ElementPropertiesWidget*>(editor)->setElement(elmt);
				return  editor;
			}
			return  new ElementPropertiesWidget(elmt, parent);
		}
		case IndependentTextItem::Type: //1005
		{
			QList<IndependentTextItem *> text_list;
			for (QGraphicsItem *qgi : items) {
				text_list.append(static_cast<IndependentTextItem*>(qgi));
			}

			if (class_name == IndiTextPropertiesWidget::staticMetaObject.className())
			{
				static_cast<IndiTextPropertiesWidget*>(editor)->setText(text_list);
				return  editor;
			}

			return new IndiTextPropertiesWidget(text_list, parent);
		}
		case DiagramImageItem::Type: //1007
		{
			if (count_ > 1) {
				return nullptr;
			}
			return new ImagePropertiesWidget(static_cast<DiagramImageItem*>(item), parent);
		}
		case QetShapeItem::Type: //1008
		{
			QList<QetShapeItem *> shapes_list;
			for (QGraphicsItem *qgi : items) {
				shapes_list.append(static_cast<QetShapeItem*>(qgi));
			}

			if (class_name == ShapeGraphicsItemPropertiesWidget::staticMetaObject.className())
			{
				static_cast<ShapeGraphicsItemPropertiesWidget*>(editor)->setItems(shapes_list);
				return editor;
			}

			return new ShapeGraphicsItemPropertiesWidget(shapes_list, parent);
		}
		case DynamicElementTextItem::Type: //1010
		{
			if (count_ > 1) {
				return nullptr;
			}

			DynamicElementTextItem *deti = static_cast<DynamicElementTextItem *>(item); 
				//For dynamic element text, we open the element editor to edit it
				//If we already edit an element, just update the editor with a new element
			if (class_name == ElementPropertiesWidget::staticMetaObject.className())
			{
				static_cast<ElementPropertiesWidget*>(editor)->setDynamicText(deti);
				return editor;
			}
			return new ElementPropertiesWidget(deti, parent);
		}
		case QGraphicsItemGroup::Type:
		{
			if (count_ > 1) {
				return nullptr;
			}

			if(ElementTextItemGroup *group = dynamic_cast<ElementTextItemGroup *>(item))
			{
					//For element text item group, we open the element editor to edit it
					//If we already edit an element, just update the editor with a new element
				if(class_name == ElementPropertiesWidget::staticMetaObject.className())
				{
					static_cast<ElementPropertiesWidget *>(editor)->setTextsGroup(group);
					return editor;
				}
				return new ElementPropertiesWidget(group, parent);
			}
			break;
		}
		case QetGraphicsTableItem::Type:
		{
			if (count_ > 1) {
				return nullptr;
			}

			auto table = static_cast<QetGraphicsTableItem*>(item);
			if (class_name == GraphicsTablePropertiesEditor::staticMetaObject.className())
			{
				static_cast<GraphicsTablePropertiesEditor*>(editor)->setTable(table);
				return editor;
			}
			return new GraphicsTablePropertiesEditor(table, parent);
		}
		default:
			return nullptr;
	}

	return nullptr;
}
