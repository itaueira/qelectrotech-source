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

#include "../../../../sources/diagram.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QColor>
#include <QImage>

#include <algorithm>

namespace {
	/// A project of the current format, opened by every case below.
	const QString reference_example = QStringLiteral("industrial.qet");
}

TEST_CASE("bancada — o guarda recusa o exemplo que abriria uma caixa de diálogo",
		  "[uibench]")
{
	// This is the case that keeps the suite from hanging rather than
	// failing, so it is the one that must not be allowed to rot.
	QString reason;

	SECTION("um exemplo do formato corrente passa")
	{
		REQUIRE(UiBench::opensWithoutDialog(UiBench::examplePath(reference_example),
											&reason));
		REQUIRE(reason.isEmpty());
	}

	SECTION("o exemplo gravado em 0.3 é recusado, e a razão diz por quê")
	{
		const QString path = UiBench::examplePath(QStringLiteral("schema_indus.qet"));
		REQUIRE_FALSE(UiBench::opensWithoutDialog(path, &reason));
		REQUIRE(reason.contains(QLatin1String("0.6")));
	}

	SECTION("um arquivo que não existe é recusado, não aberto")
	{
		const QString path = UiBench::examplePath(QStringLiteral("no_such_file.qet"));
		REQUIRE_FALSE(UiBench::opensWithoutDialog(path, &reason));
		REQUIRE(reason.contains(QLatin1String("cannot be read")));
	}
}

TEST_CASE("bancada — um projeto abre sem QETApp e entrega as folhas montadas",
		  "[uibench]")
{
	UiBench::Project project(reference_example);

	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());
	REQUIRE(project.diagramCount() > 0);

	Diagram *first = project.diagram(0);
	REQUIRE(first != nullptr);

	// The folios are built by the time the constructor returns: readDiagramsXml
	// runs inside openFile(). Nothing further has to be called for the sheet to
	// have its items.
	REQUIRE_FALSE(first->elements().isEmpty());
}

TEST_CASE("bancada — abrir um projeto não empilha comando de desfazer",
		  "[uibench]")
{
	// A project that arrives with something to undo is a project that was
	// modified while being read - the kind of defect that only shows up as a
	// stray asterisk in the title bar, long after the cause.
	UiBench::Project project(reference_example);

	REQUIRE(project.isOpen());
	REQUIRE(UiBench::undoTopText(project.project()).isEmpty());
}

TEST_CASE("bancada — os rótulos exibidos chegam compostos, e não todos vazios",
		  "[uibench]")
{
	UiBench::Project project(reference_example);
	REQUIRE(project.isOpen());

	// busiestSheet(), and deliberately neither diagram(0) nor the first sheet
	// that draws: see the comment on the declaration, which records why both
	// of those were tried here and both were wrong.
	Diagram *sheet = project.busiestSheet();
	REQUIRE(sheet != nullptr);

	const QStringList labels = UiBench::displayedLabels(sheet);
	REQUIRE_FALSE(labels.isEmpty());

	// A sheet whose labels are all empty would satisfy any comparison a later
	// test makes against it, and prove nothing at all.
	const bool any_label = std::any_of(labels.cbegin(), labels.cend(),
									   [](const QString &label) {
										   return !label.isEmpty();
									   });
	REQUIRE(any_label);
}

TEST_CASE("bancada — a folha desenha sem tela, e o desenho não sai em branco",
		  "[uibench]")
{
	UiBench::Project project(reference_example);
	REQUIRE(project.isOpen());

	Diagram *sheet = project.busiestSheet();
	REQUIRE(sheet != nullptr);

	const QImage image = UiBench::render(sheet, 800);
	REQUIRE(image.width() == 800);
	REQUIRE(image.height() > 0);

	// An all-white image is what a render that silently drew nothing returns,
	// and it is indistinguishable from a correct render of an empty sheet
	// unless someone looks for the ink.
	bool has_ink = false;
	for (int y = 0; y < image.height() && !has_ink; ++y) {
		for (int x = 0; x < image.width(); ++x) {
			if (image.pixelColor(x, y) != QColor(Qt::white)) {
				has_ink = true;
				break;
			}
		}
	}
	REQUIRE(has_ink);
}
