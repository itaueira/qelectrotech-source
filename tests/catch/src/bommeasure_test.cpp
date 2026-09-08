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
#include "../../../sources/catalog/catalog.h"
#include "../../../sources/catalog/catalogclass.h"
#include "../../../sources/location/bommeasure.h"
#include "../../../sources/location/locationtree.h"
#include "../../../sources/location/projectlocation.h"
#include "qt_catch_tostring.h"

#include <QDomDocument>
#include <QDomElement>
#include <QList>
#include <QLocale>
#include <QString>

namespace
{
		/// @return a location as the dialogue will hand it over
	ProjectLocation local(const char *codigo,
			      const char *nome = "",
			      const char *peca = "")
	{
		ProjectLocation l(QString::fromUtf8(codigo), QString::fromUtf8(nome));
		l.part_code = QString::fromUtf8(peca);
		return l;
	}

		/// The locale that writes a dot, so the expected text is fixed
		/// whatever machine runs the suite.
	QLocale ponto()
	{
		return QLocale::c();
	}

		/// The locale the program is translated into, which writes a comma.
	QLocale virgula()
	{
		return QLocale(QLocale::Portuguese, QLocale::Brazil);
	}

		/**
			@brief Add one use of a part to a list of lines.

			Written here rather than in the program because the producer of
			measured lines is the layout and does not exist yet. What it
			exercises is the part that does: the consolidation asks
			LocationTree::indexOfBomLine and never compares quantities.
		*/
	void somar(QList<LocationTree::BomLine> &linhas,
		   const char *peca,
		   int revisao,
		   double quantidade,
		   const QString &unidade)
	{
		const QString codigo = QString::fromUtf8(peca);
		int i = LocationTree::indexOfBomLine(linhas, codigo, revisao);
		if (i < 0)
		{
			LocationTree::BomLine linha;
			linha.part_code = codigo;
			linha.part_revision = revisao;
			linha.unit = unidade;
			linhas.append(linha);
			i = int(linhas.count()) - 1;
		}
		linhas[i].quantity += quantidade;
	}
}

TEST_CASE("T16 — a linha de material aceita fração e o que se conta continua inteiro",
	  "[bom]")
{
		//The default is the one already in production, and the whole
		//point of the section: making the field fractional must not
		//change what an ordinary part reads.
	const LocationTree::BomLine nova;
	CHECK(nova.quantity == 0.0);
	CHECK(nova.unit == BomMeasure::countUnit());
	CHECK(nova.unit == QString("un"));

	LocationTree arvore;
	arvore.append(local("QCM1", "Quadro 1", "GAB-600"));
	arvore.append(local("QCM2", "Quadro 2", "GAB-600"));
	arvore.append(local("QCM3", "Quadro 3", "GAB-600"));
	arvore.append(local("QCM4", "Quadro 4", "GAB-600"));
	arvore.append(local("QCM5", "Quadro 5", "GAB-600"));

	const QList<LocationTree::BomLine> linhas = arvore.bomLines();
	REQUIRE(linhas.size() == 1);

		//Exact equality on a double, on purpose and not by oversight:
		//five ones added up is representable to the last bit, and a test
		//written with a tolerance here would keep passing on the day the
		//count stopped being exact.
	CHECK(linhas.at(0).quantity == 5.0);
	CHECK(linhas.at(0).unit == QString("un"));

	SECTION("e o que se conta nunca imprime casa decimal")
	{
		CHECK(BomMeasure::formatQuantity(linhas.at(0).quantity,
						 linhas.at(0).unit, ponto())
		      == QString("5"));
		CHECK(BomMeasure::formatQuantity(14.0, QStringLiteral("un"),
						 virgula())
		      == QString("14"));
			//The unit of a count is never printed beside the number:
			//it was not there before and nobody asked for it.
		CHECK(BomMeasure::formatQuantityWithUnit(14.0,
							 QStringLiteral("un"),
							 virgula())
		      == QString("14"));
			//A fraction that reaches a counted line is a fault
			//somewhere else, and the list still may not print
			//"2,4 breakers". This is the assertion that tells the
			//two branches apart: a whole number prints the same
			//either way, so a test made only of whole numbers would
			//pass with the branch removed. Measured: with the count
			//branch disabled, of the whole section only this and the
			//thousand below fail.
		CHECK(BomMeasure::formatQuantity(2.4, QStringLiteral("un"),
						 virgula())
		      == QString("2"));
		CHECK(BomMeasure::formatQuantity(2.6, QStringLiteral("un"),
						 ponto())
		      == QString("3"));
			//Neither separator may appear in a count, whichever
			//locale is reading it.
		CHECK_FALSE(BomMeasure::formatQuantity(2.4,
						       QStringLiteral("un"),
						       virgula())
			    .contains(QLatin1Char(',')));
		CHECK_FALSE(BomMeasure::formatQuantity(2.4,
						       QStringLiteral("un"),
						       ponto())
			    .contains(QLatin1Char('.')));
			//A thousand terminals read as a thousand and not as one:
			//no group separator, which is what the lists print today.
		CHECK(BomMeasure::formatQuantity(1000.0, QStringLiteral("un"),
						 virgula())
		      == QString("1000"));
	}

	SECTION("um projeto gravado antes disto abre e conta igual")
	{
			//Nothing about a line of material is written to a file -
			//it is worked out from the tree every time - so the type
			//change migrates nothing. This is what proves it: the
			//locations go out to XML and come back, and the line the
			//tree builds from them is the same line.
		QDomDocument documento;
		documento.appendChild(arvore.toXml(documento));

		LocationTree lida;
		REQUIRE(lida.fromXml(documento.documentElement()));

		const QList<LocationTree::BomLine> depois = lida.bomLines();
		REQUIRE(depois.size() == 1);
		CHECK(depois.at(0).quantity == 5.0);
		CHECK(depois.at(0).unit == QString("un"));
		CHECK(depois.at(0).paths == linhas.at(0).paths);
	}
}

TEST_CASE("T16 — trilho e canaleta somam comprimento, e o resto continua contando peça",
	  "[bom]")
{
		//The rule the answer of 05/09 wrote: cut to size, so what the
		//list needs is the metres used.
	CHECK(BomMeasure::kindForClass(BomMeasure::lengthClassKey())
	      == BomMeasure::Kind::Length);
	CHECK(BomMeasure::lengthClassKey() == QString("rail_duct"));
	CHECK(BomMeasure::defaultUnit(BomMeasure::Kind::Length)
	      == QString("m"));

		//And the other half of the rule, which is the half that keeps
		//the change from leaking into what already works.
	CHECK(BomMeasure::kindForClass(QStringLiteral("breaker"))
	      == BomMeasure::Kind::Count);
	CHECK(BomMeasure::kindForClass(QStringLiteral("component"))
	      == BomMeasure::Kind::Count);
	CHECK(BomMeasure::kindForClass(QStringLiteral("location"))
	      == BomMeasure::Kind::Count);
	CHECK(BomMeasure::kindForClass(QString())
	      == BomMeasure::Kind::Count);
		//Wire and cable are measured too, and deliberately not here: the
		//cable list already gives them a length, from its own source.
	CHECK(BomMeasure::kindForClass(QStringLiteral("wire_cable"))
	      == BomMeasure::Kind::Count);
	CHECK(BomMeasure::defaultUnit(BomMeasure::Kind::Count)
	      == QString("un"));

	SECTION("e a chave é a que o catálogo semeia, e não uma que se escreveu aqui")
	{
			//Measured: with the key spelled "rail" instead of
			//"rail_duct", the rule stays self consistent -
			//kindForClass(lengthClassKey()) still answers Length -
			//and only a check against something outside the rule
			//notices. The catalogue is that something.
		Catalog catalogo;
		QString erro;
		REQUIRE(catalogo.openInMemory(&erro));
		REQUIRE(erro.isEmpty());

		const CatalogClass trilho =
				catalogo.classByKey(BomMeasure::lengthClassKey());
		REQUIRE_FALSE(trilho.isNull());

			//And this is the question a caller holding a catalogue
			//asks, rather than comparing the key itself: it is what
			//makes a class somebody adds under Rail / Duct measured
			//too.
		CHECK(catalogo.isDescendantOf(trilho.id,
					      BomMeasure::lengthClassKey()));

		const CatalogClass disjuntor =
				catalogo.classByKey(QStringLiteral("breaker"));
		REQUIRE_FALSE(disjuntor.isNull());
		CHECK_FALSE(catalogo.isDescendantOf(disjuntor.id,
						    BomMeasure::lengthClassKey()));
	}

	SECTION("a unidade escrita na linha é o que decide como ela se lê")
	{
		CHECK(BomMeasure::kindForUnit(QStringLiteral("m"))
		      == BomMeasure::Kind::Length);
		CHECK(BomMeasure::kindForUnit(QStringLiteral("mm"))
		      == BomMeasure::Kind::Length);
			//Typed by a person into a free field, so " M " is the
			//same metre as "m".
		CHECK(BomMeasure::kindForUnit(QStringLiteral(" M "))
		      == BomMeasure::Kind::Length);
		CHECK(BomMeasure::kindForUnit(QStringLiteral("un"))
		      == BomMeasure::Kind::Count);
			//An empty unit is a line written before lines had one,
			//and it has to keep reading as a count.
		CHECK(BomMeasure::kindForUnit(QString())
		      == BomMeasure::Kind::Count);
			//And a unit nobody here knows counts, which leaves an
			//unknown line reading exactly as it reads today.
		CHECK(BomMeasure::kindForUnit(QString::fromUtf8("pç"))
		      == BomMeasure::Kind::Count);
	}

	SECTION("uma barra cortada em três entra como a metragem somada")
	{
		QList<LocationTree::BomLine> linhas;
		somar(linhas, "TRILHO-35", 0, 0.8, BomMeasure::defaultLengthUnit());
		somar(linhas, "TRILHO-35", 0, 0.6, BomMeasure::defaultLengthUnit());
		somar(linhas, "TRILHO-35", 0, 0.6, BomMeasure::defaultLengthUnit());

		REQUIRE(linhas.size() == 1);
		CHECK(linhas.at(0).quantity == Approx(2.0));
		CHECK(linhas.at(0).unit == QString("m"));
		CHECK(BomMeasure::formatQuantityWithUnit(linhas.at(0).quantity,
							 linhas.at(0).unit,
							 virgula())
		      == QString("2 m"));
	}

	SECTION("o comprimento imprime a fração, e a vírgula é a de quem lê")
	{
		CHECK(BomMeasure::formatQuantity(3.2, QStringLiteral("m"), ponto())
		      == QString("3.2"));
		CHECK(BomMeasure::formatQuantity(3.2, QStringLiteral("m"), virgula())
		      == QString("3,2"));
		CHECK(BomMeasure::formatQuantityWithUnit(3.2,
							 QStringLiteral("m"),
							 virgula())
		      == QString("3,2 m"));
			//A millimetre inside a metre is three decimals, and no
			//more than three.
		CHECK(BomMeasure::formatQuantity(1.234, QStringLiteral("m"), ponto())
		      == QString("1.234"));
			//A rail that comes out whole reads as whole: the fraction
			//is printed when there is one, not always.
		CHECK(BomMeasure::formatQuantity(2.0, QStringLiteral("m"), ponto())
		      == QString("2"));
		CHECK(BomMeasure::formatQuantity(0.0, QStringLiteral("m"), ponto())
		      == QString("0"));
		CHECK(BomMeasure::formatQuantity(0.75, QStringLiteral("m"), ponto())
		      == QString("0.75"));
	}
}

TEST_CASE("T16 — consolidar linha compara a chave, e nunca o número",
	  "[bom]")
{
		//The trap, stated first so the test is not read as arithmetic
		//pedantry: these two are the same three tenths and are not the
		//same double.
	const double somado = 0.1 + 0.2;
	const double escrito = 0.3;
	CHECK_FALSE(somado == escrito);

	QList<LocationTree::BomLine> linhas;
	somar(linhas, "CANALETA-40", 0, somado, QStringLiteral("m"));
	somar(linhas, "CANALETA-40", 0, escrito, QStringLiteral("m"));

		//One line, because the key matched. A merge that had compared
		//the numbers would have produced two lines of the same duct, and
		//nobody reading the list could have seen why.
	REQUIRE(linhas.size() == 1);
	CHECK(linhas.at(0).quantity == Approx(0.6));

	SECTION("a revisão faz parte da chave, e separa duas compras")
	{
		somar(linhas, "CANALETA-40", 2, 1.5, QStringLiteral("m"));
		REQUIRE(linhas.size() == 2);
		CHECK(linhas.at(1).part_revision == 2);
		CHECK(linhas.at(1).quantity == Approx(1.5));
	}

	SECTION("e a busca da chave responde o mesmo que a lista")
	{
		CHECK(LocationTree::indexOfBomLine(linhas,
						   QStringLiteral("CANALETA-40"),
						   0) == 0);
		CHECK(LocationTree::indexOfBomLine(linhas,
						   QStringLiteral("CANALETA-40"),
						   2) == -1);
		CHECK(LocationTree::indexOfBomLine(linhas,
						   QStringLiteral("TRILHO-35"),
						   0) == -1);
		CHECK(LocationTree::indexOfBomLine(QList<LocationTree::BomLine>(),
						   QStringLiteral("CANALETA-40"),
						   0) == -1);
	}
}
