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
#include "../../../sources/utils/csvwriter.h"
#include "qt_catch_tostring.h"

#include <QChar>
#include <QString>
#include <QStringList>

/*
	The quoting every list of this program writes through.

	It is worth nailing here, and by itself, for one reason: the failure
	it prevents is silent. A designation holding the separator does not
	make the export fail, and does not make the file unreadable - it
	shifts every column of that row by one, and the row after it looks
	exactly as well formed as the rows that are right. Purchasing reads a
	manufacturer where the quantity should be, and the only way anybody
	finds out is by recognising the value.

	Four cells and one separator cover the whole rule:

	- the cell holding the separator, which is the defect as it was met -
	  "Contator 3P; 25A" in a semicoloned file;
	- the cell holding a double quote, which has to be doubled, because
	  the quote that opens the cell would otherwise close it in the middle;
	- the cell holding an end of line, which splits one row into two and
	  is the only form of the defect that changes the number of rows;
	- the plain cell, which must come back untouched. That one is not a
	  nicety: it is what says the change added quoting and nothing else,
	  so a file exported before and after this is byte for byte the same
	  whenever the material has no odd character in it. Without it, a
	  function that quoted everything would pass the other three.

	And the separator itself is a parameter because the same rows are
	written twice, with ';' to a file and with a tabulation to the
	clipboard. A rule that escaped a hard-written ';' would leave the
	tabulated form broken while looking fixed, which is the reason the
	extracted version takes the separator instead of knowing one.
*/

TEST_CASE("T16 — um campo com o separador sai entre aspas", "[csv]")
{
		//The defect as it was met: a designation typed by a person.
	REQUIRE(QETCsv::field(QStringLiteral("Contator 3P; 25A"))
		== QStringLiteral("\"Contator 3P; 25A\""));

		//Joined into a row, the reader still counts one cell per column.
	QStringList row;
	row << QStringLiteral("K1")
	    << QStringLiteral("Contator 3P; 25A")
	    << QStringLiteral("2");
	REQUIRE(QETCsv::row(row)
		== QStringLiteral("K1;\"Contator 3P; 25A\";2"));
}

TEST_CASE("T16 — uma aspa dentro do campo é duplicada", "[csv]")
{
		//A part named by its thread size is the ordinary way a quote
		//reaches a list of material.
	REQUIRE(QETCsv::field(QStringLiteral("Prensa-cabo 1/2\""))
		== QStringLiteral("\"Prensa-cabo 1/2\"\"\""));

		//Doubled, and only doubled: the text inside is otherwise
		//untouched.
	REQUIRE(QETCsv::field(QStringLiteral("a\"b\"c"))
		== QStringLiteral("\"a\"\"b\"\"c\""));
}

TEST_CASE("T16 — uma quebra de linha dentro do campo não vira outra linha",
	  "[csv]")
{
		//The only form of the defect that changes the number of rows:
		//without the quotes, the reader sees two rows of three and
		//four columns instead of one row of two.
	QStringList row;
	row << QStringLiteral("XS1")
	    << QStringLiteral("Bloco\nde bornes");
	REQUIRE(QETCsv::row(row)
		== QStringLiteral("XS1;\"Bloco\nde bornes\""));

		//The carriage return counts too: a value pasted from a
		//spreadsheet carries one, and it ends the row just as well.
	REQUIRE(QETCsv::field(QStringLiteral("Bloco\rde bornes"))
		== QStringLiteral("\"Bloco\rde bornes\""));
}

TEST_CASE("T16 — um campo limpo não ganha aspas", "[csv]")
{
		//This is the case that proves nothing else changed: what the
		//exporters wrote before the quoting existed, they still write.
	REQUIRE(QETCsv::field(QStringLiteral("Contator 3P 25A"))
		== QStringLiteral("Contator 3P 25A"));
	REQUIRE(QETCsv::field(QString()) == QString());
	REQUIRE(QETCsv::field(QString("")) == QString(""));

		//An accent, a comma and a slash are not the separator and buy
		//no quotes: a semicoloned file is not a comma-separated one.
	REQUIRE(QETCsv::field(QStringLiteral("Disjuntor 3P, curva C"))
		== QStringLiteral("Disjuntor 3P, curva C"));
	REQUIRE(QETCsv::field(QStringLiteral("Seção 2,5 mm²"))
		== QStringLiteral("Seção 2,5 mm²"));

		//A whole row of plain cells is the join it always was.
	QStringList row;
	row << QStringLiteral("Q1")
	    << QStringLiteral("Disjuntor 3P 16A")
	    << QStringLiteral("1");
	REQUIRE(QETCsv::row(row) == QStringLiteral("Q1;Disjuntor 3P 16A;1"));
	REQUIRE(QETCsv::row(row) == row.join(QStringLiteral(";")));
}

TEST_CASE("T16 — o separador é o que o chamador passa", "[csv]")
{
		//The clipboard form: tabulated, so the ';' is ordinary text and
		//must not be quoted - while the tabulation must.
	const QString tab = QStringLiteral("\t");
	REQUIRE(QETCsv::field(QStringLiteral("Contator 3P; 25A"), tab)
		== QStringLiteral("Contator 3P; 25A"));
	REQUIRE(QETCsv::field(QStringLiteral("Contator\t3P"), tab)
		== QStringLiteral("\"Contator\t3P\""));

		//And the other way round, in the semicoloned file: a tabulation
		//is ordinary text there.
	REQUIRE(QETCsv::field(QStringLiteral("Contator\t3P"))
		== QStringLiteral("Contator\t3P"));

		//The comma separator, which is what a CSV means outside this
		//program - the catalogue table is written with a delimiter the
		//caller chooses, and passes it as a QChar.
	REQUIRE(QETCsv::field(QStringLiteral("Disjuntor 3P, curva C"),
			      QChar(','))
		== QStringLiteral("\"Disjuntor 3P, curva C\""));

		//A row keeps the separator it was given, both between the cells
		//and inside the test that quotes them.
	QStringList row;
	row << QStringLiteral("K1")
	    << QStringLiteral("Contator 3P; 25A")
	    << QStringLiteral("2");
	REQUIRE(QETCsv::row(row, tab)
		== QStringLiteral("K1\tContator 3P; 25A\t2"));

		//An empty separator quotes nothing by itself. QString::contains
		//answers true for the empty string, so without the guard every
		//cell of the file would come out quoted.
	REQUIRE(QETCsv::field(QStringLiteral("Contator 3P 25A"), QString())
		== QStringLiteral("Contator 3P 25A"));
		//The rest of the rule still applies without a separator.
	REQUIRE(QETCsv::field(QStringLiteral("a\"b"), QString())
		== QStringLiteral("\"a\"\"b\""));
}

TEST_CASE("T16 — a linha vazia e a linha de uma célula", "[csv]")
{
		//No cell is an empty line, not a separator: the exporters build
		//their rows from a query, and a query can answer no column.
	REQUIRE(QETCsv::row(QStringList()) == QString());

		//One cell is that cell, with no separator added - which is what
		//the list of wire numbers writes, one column wide.
	REQUIRE(QETCsv::row(QStringList() << QStringLiteral("W12"))
		== QStringLiteral("W12"));
	REQUIRE(QETCsv::row(QStringList() << QStringLiteral("W12;13"))
		== QStringLiteral("\"W12;13\""));
}
