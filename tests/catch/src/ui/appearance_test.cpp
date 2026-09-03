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
#include "../../../../sources/qetgraphicsitem/diagramtextitem.h"
#include "../../../../sources/qetgraphicsitem/element.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QPointF>
#include <QRectF>

/*
	What a sheet looks like, checked without a screen.

	The measurements below are geometry and ink counts, never a comparison
	against a stored image: the drawing font is read from the settings and no
	font is shipped with the program, so a reference image would differ from one
	machine to the next for a reason that has nothing to do with the code it was
	put there to guard. The long form of that argument is on the appearance
	section of uibench.h.
*/

namespace {
	/// A project of the current format, opened by every case below.
	const QString reference_example = QStringLiteral("industrial.qet");

	// How thick a band along an edge has to be to catch the line drawn on it.
	// Three scene units: the frame is drawn with a thin pen, the render below is
	// close to one image pixel per scene unit, so three units catch the line and
	// stop well short of the drawing, which starts twenty units further in.
	const qreal edge_band = 3.;
}

TEST_CASE("aparência — a moldura se encaixa: o desenho dentro da borda, a borda dentro da folha",
		  "[uibench][appearance]")
{
	UiBench::Project project(reference_example);
	INFO(project.error().toStdString());
	REQUIRE(project.isOpen());

	Diagram *sheet = project.busiestSheet();
	REQUIRE(sheet != nullptr);

	const QRectF page = UiBench::pageRect(sheet);
	const QRectF border = UiBench::borderRect(sheet);
	const QRectF drawing = UiBench::drawingRect(sheet);

	REQUIRE_FALSE(page.isEmpty());
	REQUIRE(page.contains(border));
	REQUIRE(border.contains(drawing));

	// The row and column headers have to take room from the drawing area. Were
	// these two equal, the headers would be printed over the schematic.
	REQUIRE(drawing.width() < border.width());
	REQUIRE(drawing.height() < border.height());

	SECTION("nenhuma folha do projeto sai sem moldura")
	{
		// The title block template is reached through the project and not
		// through QETApp, and this is what says so: were the default template
		// missing under a test run, the sheets would come out frameless and
		// every appearance check after this one would be measuring nothing.
		QStringList frameless;
		const QList<Diagram *> sheets = project.diagrams();
		for (Diagram *diagram : sheets)
		{
			if (UiBench::pageRect(diagram).isEmpty())
			{
				frameless << diagram->title();
			}
		}
		INFO(frameless.join(QLatin1String(", ")).toStdString());
		REQUIRE(frameless.isEmpty());
		REQUIRE(sheets.count() > 1);
	}
}

TEST_CASE("aparência — o carimbo encosta no rodapé da moldura e ocupa a largura dela",
		  "[uibench][appearance]")
{
	UiBench::Project project(reference_example);
	REQUIRE(project.isOpen());

	Diagram *sheet = project.busiestSheet();
	REQUIRE(sheet != nullptr);

	const QRectF page = UiBench::pageRect(sheet);
	const QRectF border = UiBench::borderRect(sheet);
	const QRectF drawing = UiBench::drawingRect(sheet);
	const QRectF title_block = UiBench::titleBlockRect(sheet);

	REQUIRE_FALSE(title_block.isEmpty());

	// Docked, not floating: the top of the title block is the bottom of the
	// border. A gap between them prints as a white stripe across the sheet, and
	// an overlap prints as the title block eating the last row of the drawing.
	REQUIRE(title_block.top() == Approx(border.bottom()));
	REQUIRE(title_block.width() == Approx(border.width()));
	REQUIRE(title_block.top() >= drawing.bottom());

	// And the page is the two of them together, nothing more.
	REQUIRE((border | title_block) == page);
}

TEST_CASE("aparência — a caixa de cada texto já vale antes da primeira pintura",
		  "[uibench][appearance]")
{
	/*
		Measured here rather than assumed, because every check that follows
		depends on it: a text item that only sizes itself when it is first
		painted would report an empty box to a suite that never paints, and
		every containment check written over it would pass by being vacuous.

		It does size itself. On the busiest sheet of the example all sixty-three
		texts report a non-empty box on a project that has just been read and
		never drawn, and not one of those boxes moves when the sheet is then
		rendered. QGraphicsTextItem lays its document out when the text is set,
		which is what DiagramTextItem does on construction; the paint is not
		part of the measurement.
	*/
	UiBench::Project project(reference_example);
	REQUIRE(project.isOpen());

	Diagram *sheet = project.busiestSheet();
	REQUIRE(sheet != nullptr);

	// Nothing has been rendered at this point of the case, and nothing may be
	// until the first snapshot below has been taken.
	const QList<DiagramTextItem *> texts = UiBench::texts(sheet);
	REQUIRE_FALSE(texts.isEmpty());

	QStringList empty_before_paint;
	QList<QRectF> before;
	for (DiagramTextItem *text : texts)
	{
		before << text->boundingRect();
		if (text->boundingRect().isEmpty())
		{
			empty_before_paint << text->toPlainText();
		}
	}
	INFO(empty_before_paint.join(QLatin1String(", ")).toStdString());
	REQUIRE(empty_before_paint.isEmpty());

	const UiBench::Rendering rendering(sheet);
	REQUIRE_FALSE(rendering.isNull());

	int moved = 0;
	for (int i = 0; i < texts.count(); ++i)
	{
		if (texts.at(i)->boundingRect() != before.at(i))
		{
			++moved;
		}
	}
	REQUIRE(moved == 0);
}

TEST_CASE("aparência — todo componente cai dentro da folha, e o que sai é apontado pelo nome",
		  "[uibench][appearance]")
{
	UiBench::Project project(reference_example);
	REQUIRE(project.isOpen());

	Diagram *sheet = project.busiestSheet();
	REQUIRE(sheet != nullptr);

	SECTION("a folha mais cheia mantém tudo dentro, e nada sem tamanho")
	{
		/*
			One sheet, not the whole project, and the reason is in the example
			itself: its terminal board sheet carries a "Coming arrow" that
			starts fourteen units to the left of the page. That is how the
			example was drawn, upstream, and a check written over every sheet
			would report it as a defect of this suite forever.
		*/
		const QStringList off_page = UiBench::elementsOffPage(sheet);
		INFO(off_page.join(QLatin1String("\n")).toStdString());
		REQUIRE(off_page.isEmpty());

		// A component whose bounding rectangle has no surface is drawn as
		// nothing at all: it is selectable, it is saved, and it is invisible.
		const QStringList no_surface = UiBench::elementsWithoutSurface(sheet);
		INFO(no_surface.join(QLatin1String("\n")).toStdString());
		REQUIRE(no_surface.isEmpty());
	}

	SECTION("um componente empurrado para fora é encontrado e nomeado")
	{
		// The check above is only worth its green if it can go red, so here it
		// is made to. The move stays in memory: the project is closed at the
		// end of the case and never written back.
		Element *element = sheet->elements().first();
		QString name = element->displayedLabel();
		if (name.isEmpty())
		{
			name = element->name();
		}

		const QPointF was = element->pos();
		element->setPos(was + QPointF(5000., 0.));

		const QStringList off_page = UiBench::elementsOffPage(sheet);
		REQUIRE(off_page.count() == 1);
		// Named, not counted: whoever reads the failure knows which component
		// to look for without going back to the sheet to find out.
		REQUIRE(off_page.first().contains(name));

		element->setPos(was);
		REQUIRE(UiBench::elementsOffPage(sheet).isEmpty());
	}
}

TEST_CASE("aparência — a folha desenhada tem moldura fechada, carimbo e desenho",
		  "[uibench][appearance]")
{
	UiBench::Project project(reference_example);
	REQUIRE(project.isOpen());

	Diagram *sheet = project.busiestSheet();
	REQUIRE(sheet != nullptr);

	const UiBench::Rendering rendering(sheet, 1200);
	REQUIRE_FALSE(rendering.isNull());
	REQUIRE(rendering.image().width() == 1200);

	const QRectF page = UiBench::pageRect(sheet);
	const QRectF border = UiBench::borderRect(sheet);
	const QRectF drawing = UiBench::drawingRect(sheet);
	const QRectF title_block = UiBench::titleBlockRect(sheet);

	// The page fills the image, which is what makes every rectangle below
	// findable inside it.
	REQUIRE(rendering.map(page) == rendering.image().rect());

	SECTION("a moldura fecha dos quatro lados")
	{
		/*
			A line along an edge inks about one pixel per unit of that edge, and
			half of it is what is asked for here: enough that a stray dot or a
			corner mark cannot answer for a missing side, loose enough that the
			pen width and the antialiasing do not decide the outcome.
		*/
		const QRectF top(border.left(), border.top(), border.width(), edge_band);
		const QRectF bottom(border.left(), border.bottom() - edge_band, border.width(), edge_band);
		const QRectF left(border.left(), border.top(), edge_band, border.height());
		const QRectF right(border.right() - edge_band, border.top(), edge_band, border.height());

		REQUIRE(rendering.ink(top) > rendering.map(top).width() / 2);
		REQUIRE(rendering.ink(bottom) > rendering.map(bottom).width() / 2);
		REQUIRE(rendering.ink(left) > rendering.map(left).height() / 2);
		REQUIRE(rendering.ink(right) > rendering.map(right).height() / 2);
	}

	SECTION("o carimbo e o desenho saem impressos, e o fundo continua aparecendo")
	{
		REQUIRE(rendering.ink(title_block) > 0);
		REQUIRE(rendering.ink(drawing) > 0);

		// Not a smear: the sheet has to leave background showing inside the
		// drawing area. A render that came out solid - a black fill, a picture
		// pasted over the page - would satisfy "there is ink" and say nothing.
		//
		// The margin here is narrower than it looks, and on purpose it is not
		// tightened: the busiest sheet of this example is a panel front view,
		// whose components are drawn as bodies rather than as symbols, and it
		// inks about six pixels out of seven of its drawing area.
		const QRect drawing_pixels = rendering.map(drawing);
		REQUIRE(rendering.ink(drawing) < drawing_pixels.width() * drawing_pixels.height());
	}
}

TEST_CASE("aparência — desenhar a mesma folha duas vezes dá a mesma imagem",
		  "[uibench][appearance]")
{
	/*
		The one comparison of images this suite can afford, because both sides
		of it are drawn here and now, on the same machine and with the same
		font. It catches what a stored reference would catch and a geometry
		check would not: a paint that depends on the order a hash was walked in,
		or on a field that was never initialised.
	*/
	UiBench::Project project(reference_example);
	REQUIRE(project.isOpen());

	Diagram *sheet = project.busiestSheet();
	REQUIRE(sheet != nullptr);

	const UiBench::Rendering first(sheet, 600);
	const UiBench::Rendering second(sheet, 600);

	REQUIRE_FALSE(first.isNull());
	REQUIRE(first.image().size() == second.image().size());
	REQUIRE(first.ink() > 0);
	REQUIRE(first.image() == second.image());
}
