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
#ifndef ASSEMBLYWALK_H
#define ASSEMBLYWALK_H

#include "../autoNum/assemblychanges.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"

#include <QList>
#include <QPointer>

class QETProject;

/**
	@brief What a project draws now, read the way the photograph of the
	assembly was read (T29), and the objects it was read off.
	One walk for every reader of it - the order of service of the changes
	since the assembly and the mark of them on the folio - because two
	readings that differ list a change nobody made.
*/
struct AssemblyWalk
{
	QList<AssemblyItem> components;
	QList<QPointer<Element>> component_objects;
	QList<AssemblyItem> wires;
	QList<QPointer<Conductor>> wire_objects;

	static AssemblyWalk of(QETProject *project);
};

#endif // ASSEMBLYWALK_H
