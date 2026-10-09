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
#include "uibench.h"

#include "../qt_catch_tostring.h"

#include "../../../../sources/bordertitleblock.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/diagramcontext.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

/*
	The title block follows the project when it is renamed, on every folio
	and with no command: QETProject::setTitle() hands the project's
	variables to each folio, which rebuilds the context its title block is
	rendered with. What is read here is that context, the one the renderer
	draws %{projecttitle} from.
*/

TEST_CASE("CU-26.2 — Renaming the project changes the title block of every folio, with no command",
		  "[titleblockfollow]")
{
	UiBench::Project project(QStringLiteral("tableau_domestique.qet"));
	REQUIRE(project.isOpen());
	REQUIRE(project.diagramCount() > 1);

	const QString renamed = QStringLiteral("Quadro renomeado");
	REQUIRE(project->title() != renamed);
	for (Diagram *diagram : project.diagrams()) {
		CHECK(diagram->border_and_titleblock.titleBlockContext()
			  .value(QStringLiteral("projecttitle")).toString() != renamed);
	}

	project->setTitle(renamed);

	for (Diagram *diagram : project.diagrams())
	{
		CAPTURE(diagram->title());
		CHECK(diagram->border_and_titleblock.titleBlockContext()
			  .value(QStringLiteral("projecttitle")).toString() == renamed);
	}
}
