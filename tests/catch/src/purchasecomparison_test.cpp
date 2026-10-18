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
#include "../../../sources/location/locationtree.h"
#include "../../../sources/location/purchasecomparison.h"
#include "qt_catch_tostring.h"

#include <QChar>
#include <QList>
#include <QLocale>
#include <QString>
#include <QStringList>

/*
	Two purchase lists compared on their key: what went, what came, and
	what is ordered in another quantity.

	The texts are written here by hand, in the format --export-purchase
	writes - the counts, an empty row, the lines under their header, an
	empty row, the pending block under its header, CRLF between rows - and
	never by the writer: these cases are about the reader and the rule,
	and a text made by the code under test would agree with the reader on
	whatever the two of them got wrong together. The writer and the reader
	meet on a real project in ui/purchasecompare_cli_test.cpp, which is
	where the two halves of the round trip are proved together.

	Every comparison asserts the whole answer - how many differences, of
	which kind, on which key, in which order - and not that one difference
	is among them: a comparison that also reported a line nobody changed
	would pass a case that only looked for the right one.
*/

namespace
{
	/// The header of the lines block, written out and not asked of the
	/// code under test
	QString lineHeaderRow()
	{
		return QStringLiteral("part_code;part_revision;designation;"
				      "quantity;unit;tags");
	}

	/**
		@return the text of a purchase list holding @p line_rows
		@param line_rows the rows of the lines block, as written
		@param declared the count of lines the file says it holds; the
		number of rows when negative
		@param end what ends each row
	*/
	QString exportText(const QStringList &line_rows,
			   int declared = -1,
			   const QString &end = QStringLiteral("\r\n"))
	{
		QStringList rows;
		rows << QStringLiteral("pending;0")
		     << QStringLiteral("excluded;0")
		     << QStringLiteral("nameless;0")
		     << QStringLiteral("lines;%1")
			.arg(declared < 0 ? int(line_rows.size()) : declared)
		     << QStringLiteral("mounted;0")
		     << QString()
		     << lineHeaderRow();
		rows << line_rows;
		rows << QString()
		     << QStringLiteral("reason;tag;designation;where");
		return rows.join(end) + end;
	}

	/// @return the lines of a list holding @p line_rows; fails the case
	/// when the reader refuses it
	QList<LocationTree::BomLine> linesOf(const QStringList &line_rows)
	{
		QList<LocationTree::BomLine> lines;
		QString error;
		const bool read = PurchaseComparison::readLines(
			exportText(line_rows), &lines, &error);
		INFO(error.toStdString());
		REQUIRE(read);
		REQUIRE(lines.size() == line_rows.size());
		return lines;
	}

	/// @return why the reader refused @p text; empty when it accepted it,
	/// which fails the case
	QString refusalOf(const QString &text)
	{
		QList<LocationTree::BomLine> lines;
			//Filled beforehand, so that "left empty" means emptied.
		lines << LocationTree::BomLine();
		QString error;
		const bool read = PurchaseComparison::readLines(text, &lines,
								 &error);
		CHECK_FALSE(read);
		CHECK(lines.isEmpty());
		CHECK_FALSE(error.isEmpty());
		return error;
	}

	/// @return the kind and the key of @p difference, in one word a
	/// failure prints whole
	QString describe(const PurchaseComparison::Difference &difference)
	{
		const LocationTree::BomLine &key = difference.current();
		return PurchaseComparison::changeKey(difference.change)
				+ QLatin1Char(' ') + key.part_code
				+ QLatin1Char('/')
				+ QString::number(key.part_revision);
	}

	QStringList describe(const QList<PurchaseComparison::Difference> &differences)
	{
		QStringList all;
		for (const PurchaseComparison::Difference &difference : differences) {
			all << describe(difference);
		}
		return all;
	}

	/// @return the quantity @p cell reads as; fails the case when refused
	double readable(const QString &cell)
	{
		bool ok = false;
		const double value = PurchaseComparison::quantityOf(cell, &ok);
		INFO(cell.toStdString());
		CHECK(ok);
		return value;
	}

	/// @return true when @p cell is refused as a quantity
	bool refused(const QString &cell)
	{
		bool ok = true;
		PurchaseComparison::quantityOf(cell, &ok);
		return !ok;
	}

	/// @return a line built by hand, for the rule that compares two
	LocationTree::BomLine line(const char *code, int revision,
				   double quantity, const char *unit)
	{
		LocationTree::BomLine made;
		made.part_code = QString::fromUtf8(code);
		made.part_revision = revision;
		made.quantity = quantity;
		made.unit = QString::fromUtf8(unit);
		return made;
	}
}

TEST_CASE("T18 — o comparador acusa o que saiu, o que entrou e o que mudou, "
	  "e cala o que ficou igual", "[purchase][compare][t18]")
{
		//XA-201 keeps its quantity and changes its words and its tags,
		//which is no difference: the same is ordered. The rail is the
		//same 2.5 m written by a machine in Portuguese and then by one in
		//English. XA-300 goes, XA-500 comes, XA-400 goes from 5 to 6.
	const QList<LocationTree::BomLine> before = linesOf({
		QStringLiteral("XA-201;1;Disjuntor tripolar;2;un;Q1, Q2"),
		QStringLiteral("XA-300;1;Contator 9 A;1;un;K1"),
		QStringLiteral("XA-400;;Rele de interface;5;un;R1"),
		QStringLiteral("PERFIL-35;;Trilho 35 mm;2,5;m;QCM1/PLACA")});
	const QList<LocationTree::BomLine> after = linesOf({
		QStringLiteral("XA-201;1;Disjuntor 3P;2;un;Q1, Q3"),
		QStringLiteral("XA-400;;Rele de interface;6;un;R1, R2"),
		QStringLiteral("XA-500;;Fonte 24 V;1;un;G1"),
		QStringLiteral("PERFIL-35;;Trilho 35 mm;2.5;m;QCM1/PLACA")});

	const QList<PurchaseComparison::Difference> differences =
		PurchaseComparison::compare(before, after);
		//The whole answer, in its order: removed, added, changed.
	CHECK(describe(differences) == QStringList({
		QStringLiteral("removed XA-300/1"),
		QStringLiteral("added XA-500/0"),
		QStringLiteral("changed XA-400/0")}));
	REQUIRE(differences.size() == 3);

	const PurchaseComparison::Difference &changed = differences.at(2);
	CHECK(changed.before.quantity == 5.0);
	CHECK(changed.after.quantity == 6.0);
		//The removed line keeps what the file before said of it, and the
		//added one what the file after says.
	CHECK(differences.at(0).before.name == QStringLiteral("Contator 9 A"));
	CHECK(differences.at(0).before.quantity == 1.0);
	CHECK(differences.at(1).after.name == QStringLiteral("Fonte 24 V"));

		//A list compared with itself is no difference, the one before
		//and the one after alike.
	CHECK(PurchaseComparison::compare(before, before).isEmpty());
	CHECK(PurchaseComparison::compare(after, after).isEmpty());

		//And the other way round, the same keys swap sides: what came
		//went, what went came.
	CHECK(describe(PurchaseComparison::compare(after, before))
	      == QStringList({QStringLiteral("removed XA-500/0"),
			      QStringLiteral("added XA-300/1"),
			      QStringLiteral("changed XA-400/0")}));
}

TEST_CASE("T18 — a mesma peça em duas revisões são duas linhas para o "
	  "comparador", "[purchase][compare][t18]")
{
	SECTION("a revisão nova no lugar da antiga: uma saída e uma entrada, "
		"nunca uma mudança e nunca nada")
	{
			//Same part, same words, same quantity: only the revision
			//moved, and a revision is a part of its own to buy.
		const QList<PurchaseComparison::Difference> differences =
			PurchaseComparison::compare(
				linesOf({QStringLiteral("XA-300;1;Contator 9 A;3;un;K1")}),
				linesOf({QStringLiteral("XA-300;2;Contator 9 A;3;un;K1")}));
		CHECK(describe(differences)
		      == QStringList({QStringLiteral("removed XA-300/1"),
				      QStringLiteral("added XA-300/2")}));
	}

	SECTION("a revisão nova ao lado da antiga: só a entrada")
	{
		const QList<PurchaseComparison::Difference> differences =
			PurchaseComparison::compare(
				linesOf({QStringLiteral("XA-300;1;Contator 9 A;2;un;K1, K2")}),
				linesOf({QStringLiteral("XA-300;1;Contator 9 A;2;un;K1, K2"),
					 QStringLiteral("XA-300;2;Contator 9 A;1;un;K3")}));
		CHECK(describe(differences)
		      == QStringList({QStringLiteral("added XA-300/2")}));
	}

	SECTION("a célula vazia e o zero são a mesma revisão: a vigente")
	{
		CHECK(PurchaseComparison::compare(
			linesOf({QStringLiteral("XA-400;;Rele de interface;5;un;R1")}),
			linesOf({QStringLiteral("XA-400;0;Rele de interface;5;un;R1")}))
		      .isEmpty());
	}
}

TEST_CASE("T18 — a descrição não é a chave: duas peças descritas igual, "
	  "trocadas, são uma saída e uma entrada", "[purchase][compare][t18]")
{
		//Two different breakers described with the same words, one
		//replacing the other in the same quantity. Compared by the
		//designation this is nothing at all.
	const QList<PurchaseComparison::Difference> differences =
		PurchaseComparison::compare(
			linesOf({QStringLiteral("XA-201;1;Disjuntor tripolar;1;un;Q1")}),
			linesOf({QStringLiteral("XA-202;1;Disjuntor tripolar;1;un;Q1")}));
	CHECK(describe(differences)
	      == QStringList({QStringLiteral("removed XA-201/1"),
			      QStringLiteral("added XA-202/1")}));
}

TEST_CASE("T18 — o metro lê com vírgula e com ponto, e a unidade conta",
	  "[purchase][compare][t18]")
{
	SECTION("o separador decimal de qualquer das duas máquinas")
	{
		CHECK(readable(QStringLiteral("2")) == 2.0);
		CHECK(readable(QStringLiteral("2,5")) == 2.5);
		CHECK(readable(QStringLiteral("2.5")) == 2.5);
		CHECK(readable(QStringLiteral("0,001")) == 0.001);
		CHECK(readable(QStringLiteral(" 3,25 ")) == 3.25);
	}

	SECTION("e o separador de milhar, que a lista escreve")
	{
		CHECK(readable(QStringLiteral("1.234,5")) == 1234.5);
		CHECK(readable(QStringLiteral("1,234.5")) == 1234.5);
		CHECK(readable(QStringLiteral("1 234,5")) == 1234.5);
		CHECK(readable(QStringLiteral("1") + QChar(0x00A0)
			       + QStringLiteral("234,5")) == 1234.5);
		CHECK(readable(QStringLiteral("1") + QChar(0x202F)
			       + QStringLiteral("234,5")) == 1234.5);
		CHECK(readable(QStringLiteral("1'234.5")) == 1234.5);
		CHECK(readable(QStringLiteral("1.234.567")) == 1234567.0);
		CHECK(readable(QStringLiteral("1,234,567")) == 1234567.0);
		CHECK(readable(QStringLiteral("1.234.567,25")) == 1234567.25);
	}

	SECTION("a leitura que o header avisa estar errada continua a mesma")
	{
			//A whole length of a thousand metres or more, printed with
			//its group separator and no decimals, reads as a decimal.
			//Pinned here so that changing it is a decision and not an
			//accident.
		CHECK(readable(QStringLiteral("1.234")) == 1.234);
		CHECK(readable(QStringLiteral("1,234")) == 1.234);
	}

	SECTION("o que não é quantidade é recusado")
	{
		CHECK(refused(QString()));
		CHECK(refused(QStringLiteral("  ")));
		CHECK(refused(QStringLiteral("0")));
		CHECK(refused(QStringLiteral("0,0")));
		CHECK(refused(QStringLiteral("-2,5")));
		CHECK(refused(QStringLiteral("dois")));
			//Groups that are not thousands are no number anybody wrote.
		CHECK(refused(QStringLiteral("1.2.3")));
		CHECK(refused(QStringLiteral("1,2,3")));
		CHECK(refused(QStringLiteral("1.2,3")));
		CHECK(refused(QStringLiteral("12.34,5")));
		CHECK(refused(QStringLiteral("2,5,0")));
	}

	SECTION("a mesma metragem escrita das duas formas não é diferença")
	{
		CHECK(PurchaseComparison::compare(
			linesOf({QStringLiteral("PERFIL-35;;Trilho 35 mm;2,5;m;QCM1/PLACA")}),
			linesOf({QStringLiteral("PERFIL-35;;Trilho 35 mm;2.5;m;QCM1/PLACA")}))
		      .isEmpty());
		CHECK(PurchaseComparison::compare(
			linesOf({QStringLiteral("PERFIL-35;;Trilho 35 mm;\"1.234,5\";m;")}),
			linesOf({QStringLiteral("PERFIL-35;;Trilho 35 mm;1,234.5;m;")}))
		      .isEmpty());
	}

	SECTION("e uma metragem diferente é mudança, com os dois números")
	{
		const QList<PurchaseComparison::Difference> differences =
			PurchaseComparison::compare(
				linesOf({QStringLiteral("PERFIL-35;;Trilho 35 mm;2,5;m;QCM1/PLACA")}),
				linesOf({QStringLiteral("PERFIL-35;;Trilho 35 mm;3.25;m;QCM1/PLACA")}));
		REQUIRE(differences.size() == 1);
		CHECK(differences.first().change
		      == PurchaseComparison::Change::Changed);
		CHECK(differences.first().before.quantity == 2.5);
		CHECK(differences.first().after.quantity == 3.25);
	}

	SECTION("comparada como a lista imprime: o que não chega ao milímetro "
		"não muda nada")
	{
		using PurchaseComparison::sameQuantity;
		CHECK(sameQuantity(line("P", 0, 2.5, "m"), line("P", 0, 2.5004, "m")));
		CHECK_FALSE(sameQuantity(line("P", 0, 2.5, "m"), line("P", 0, 2.501, "m")));
	}

	SECTION("a unidade: uma contagem é contagem com qualquer palavra, um "
		"comprimento só com a mesma")
	{
		using PurchaseComparison::sameQuantity;
		CHECK(sameQuantity(line("P", 0, 2.0, "un"), line("P", 0, 2.0, "pc")));
		CHECK(sameQuantity(line("P", 0, 2.0, "un"), line("P", 0, 2.0, "")));
		CHECK(sameQuantity(line("P", 0, 2.5, "m"), line("P", 0, 2.5, "M")));
		CHECK_FALSE(sameQuantity(line("P", 0, 2.5, "m"),
					 line("P", 0, 2.5, "mm")));
		CHECK_FALSE(sameQuantity(line("P", 0, 2.0, "un"),
					 line("P", 0, 2.0, "m")));

			//A unit change alone, in the same number, is a change.
		const QList<PurchaseComparison::Difference> differences =
			PurchaseComparison::compare(
				linesOf({QStringLiteral("PERFIL-35;;Trilho 35 mm;2;un;")}),
				linesOf({QStringLiteral("PERFIL-35;;Trilho 35 mm;2;m;")}));
		CHECK(describe(differences)
		      == QStringList({QStringLiteral("changed PERFIL-35/0")}));
	}
}

TEST_CASE("T18 — o arquivo de compra é lido de volta linha a linha",
	  "[purchase][compare][t18]")
{
	SECTION("as células como o --export-purchase as escreve")
	{
			//A designation holding the separator, quoted as QETCsv
			//quotes it, and a quote inside it doubled; the tags joined
			//by ", ". Neither moves a column.
		const QList<LocationTree::BomLine> lines = linesOf({
			QStringLiteral("XA-201;1;\"Disjuntor 3P; curva C\";2;un;Q1, Q2"),
			QStringLiteral("XA-210;;\"Botao 22 \"\"verde\"\"\";1;un;S1"),
			QStringLiteral("PERFIL-35;;Trilho 35 mm;2,5;m;QCM1/PLACA")});
		CHECK(lines.at(0).part_code == QStringLiteral("XA-201"));
		CHECK(lines.at(0).part_revision == 1);
		CHECK(lines.at(0).name == QStringLiteral("Disjuntor 3P; curva C"));
		CHECK(lines.at(0).quantity == 2.0);
		CHECK(lines.at(0).unit == QStringLiteral("un"));
		CHECK(lines.at(1).name == QStringLiteral("Botao 22 \"verde\""));
		CHECK(lines.at(1).part_revision == 0);
		CHECK(lines.at(2).quantity == 2.5);
		CHECK(lines.at(2).unit == QStringLiteral("m"));
	}

	SECTION("e como uma planilha pode devolvê-las: LF, marca de ordem de "
		"bytes e colunas vazias a mais")
	{
		const QStringList rows {
			QStringLiteral("XA-201;1;Disjuntor tripolar;2;un;Q1;;"),
			QStringLiteral("XA-400;;Rele de interface;5;un")};
		QString text = exportText(rows, -1, QStringLiteral("\n"));
		text.prepend(QChar(0xFEFF));
		text.replace(lineHeaderRow(), lineHeaderRow() + QStringLiteral(";;"));

		QList<LocationTree::BomLine> lines;
		QString error;
		const bool read = PurchaseComparison::readLines(text, &lines, &error);
		INFO(error.toStdString());
		REQUIRE(read);
		REQUIRE(lines.size() == 2);
		CHECK(lines.at(1).part_code == QStringLiteral("XA-400"));
		CHECK(lines.at(1).quantity == 5.0);
	}

	SECTION("uma lista sem linha nenhuma é uma lista, e não um erro")
	{
		QList<LocationTree::BomLine> lines;
		CHECK(PurchaseComparison::readLines(exportText({}), &lines));
		CHECK(lines.isEmpty());
	}
}

TEST_CASE("T18 — o que não é uma lista de compra inteira é recusado, nunca "
	  "comparado", "[purchase][compare][t18]")
{
	SECTION("arquivo vazio")
	{
		refusalOf(QString());
	}

	SECTION("sem o bloco das linhas: a lista de material de outro verbo")
	{
		const QString error = refusalOf(QStringLiteral(
			"label;designation;manufacturer;manufacturer_reference\r\n"
			"Q1;Disjuntor tripolar;;\r\n"));
		CHECK(error.contains(QStringLiteral("no lines block")));
	}

	SECTION("só as contagens, sem bloco de linhas")
	{
		const QString error = refusalOf(QStringLiteral(
			"pending;0\r\nexcluded;0\r\nnameless;0\r\nlines;0\r\n"
			"mounted;0\r\n"));
		CHECK(error.contains(QStringLiteral("no lines block")));
	}

	SECTION("cortado depois das linhas: sem o bloco das pendências")
	{
		QString text = exportText({QStringLiteral("XA-201;1;Disjuntor;2;un;Q1")});
		text.truncate(text.indexOf(QStringLiteral("reason;")));
		const QString error = refusalOf(text);
		CHECK(error.contains(QStringLiteral("no pending block")));
	}

	SECTION("uma linha perdida: a contagem diz duas e o bloco tem uma")
	{
		const QString error = refusalOf(exportText(
			{QStringLiteral("XA-201;1;Disjuntor;2;un;Q1")}, 2));
		CHECK(error.contains(QStringLiteral("says it has 2")));
	}

	SECTION("sem a contagem das linhas")
	{
		QString text = exportText({QStringLiteral("XA-201;1;Disjuntor;2;un;Q1")});
		text.remove(QStringLiteral("lines;1\r\n"));
		const QString error = refusalOf(text);
		CHECK(error.contains(QStringLiteral("no count of lines")));
	}

	SECTION("um ponto e vírgula sem aspas: a linha fica mais larga que o "
		"cabeçalho")
	{
		const QString error = refusalOf(exportText(
			{QStringLiteral("XA-201;1;Disjuntor 3P; curva C;2;un;Q1")}));
		CHECK(error.contains(QStringLiteral("line 1 has 7 cells")));
	}

	SECTION("linha sem código")
	{
		const QString error = refusalOf(exportText(
			{QStringLiteral(";1;Disjuntor;2;un;Q1")}));
		CHECK(error.contains(QStringLiteral("no part code")));
	}

	SECTION("revisão que não é número")
	{
		const QString error = refusalOf(exportText(
			{QStringLiteral("XA-201;B;Disjuntor;2;un;Q1")}));
		CHECK(error.contains(QStringLiteral("\"B\" is not a number")));
	}

	SECTION("quantidade ilegível")
	{
		const QString error = refusalOf(exportText(
			{QStringLiteral("XA-201;1;Disjuntor;dois;un;Q1")}));
		CHECK(error.contains(QStringLiteral("\"dois\" is not a quantity")));
	}

	SECTION("a mesma peça e revisão em duas linhas")
	{
		const QString error = refusalOf(exportText(
			{QStringLiteral("XA-201;1;Disjuntor;2;un;Q1"),
			 QStringLiteral("XA-201;1;Disjuntor;1;un;Q2")}));
		CHECK(error.contains(QStringLiteral("lines 1 and 2")));
	}
}

TEST_CASE("T18 — o relatório: o cabeçalho, uma linha por diferença, e o lado "
	  "que falta vazio", "[purchase][compare][t18]")
{
	const QList<LocationTree::BomLine> before = linesOf({
		QStringLiteral("XA-300;1;Contator 9 A;1;un;K1"),
		QStringLiteral("XA-400;;\"Rele; interface\";5;un;R1"),
		QStringLiteral("PERFIL-35;;Trilho 35 mm;2,5;m;QCM1/PLACA")});
	const QList<LocationTree::BomLine> after = linesOf({
		QStringLiteral("XA-400;;\"Rele; interface\";6;un;R1, R2"),
		QStringLiteral("XA-500;;Fonte 24 V;1;un;G1"),
		QStringLiteral("PERFIL-35;;Trilho 35 mm;3.25;m;QCM1/PLACA")});
	const QList<PurchaseComparison::Difference> differences =
		PurchaseComparison::compare(before, after);
	REQUIRE(differences.size() == 4);

	SECTION("as linhas, escritas por extenso")
	{
		const QString text = PurchaseComparison::reportText(
			differences, QLocale::c(), QStringLiteral("\r\n"));
		const QStringList rows = text.split(QStringLiteral("\r\n"));
		REQUIRE(rows.size() == 6);
		CHECK(rows.at(0) == QStringLiteral(
			"change;part_code;part_revision;designation;"
			"quantity_before;unit_before;quantity_after;unit_after"));
			//Removed: the after side is two empty cells, never a zero.
		CHECK(rows.at(1) == QStringLiteral(
			"removed;XA-300;1;Contator 9 A;1;un;;"));
		CHECK(rows.at(2) == QStringLiteral(
			"added;XA-500;;Fonte 24 V;;;1;un"));
			//The designation holding the separator is quoted, as the
			//export quotes it.
		CHECK(rows.at(3) == QStringLiteral(
			"changed;XA-400;;\"Rele; interface\";5;un;6;un"));
		CHECK(rows.at(4) == QStringLiteral(
			"changed;PERFIL-35;;Trilho 35 mm;2.5;m;3.25;m"));
			//Each row ended, the last one too.
		CHECK(rows.at(5).isEmpty());
	}

	SECTION("o metro sai com o separador decimal da máquina, e a contagem "
		"sem nenhum")
	{
		const QStringList row = PurchaseComparison::reportRow(
			differences.at(3), QLocale(QLocale::Portuguese, QLocale::Brazil));
		CHECK(row == QStringList({QStringLiteral("changed"),
					  QStringLiteral("PERFIL-35"),
					  QString(),
					  QStringLiteral("Trilho 35 mm"),
					  QStringLiteral("2,5"),
					  QStringLiteral("m"),
					  QStringLiteral("3,25"),
					  QStringLiteral("m")}));
		CHECK(PurchaseComparison::reportRow(
			differences.at(2), QLocale(QLocale::Portuguese, QLocale::Brazil))
		      .at(4) == QStringLiteral("5"));
	}

	SECTION("sem diferença, o relatório é só o cabeçalho")
	{
		CHECK(PurchaseComparison::reportText(
			PurchaseComparison::compare(before, before), QLocale::c(),
			QStringLiteral("\n"))
		      == PurchaseComparison::reportHeader().join(QLatin1Char(';'))
			 + QStringLiteral("\n"));
	}
}
