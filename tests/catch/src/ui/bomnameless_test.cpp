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

#include "../../../../sources/cli_export.h"
#include "../../../../sources/dataBase/bomquery.h"
#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/dataBase/ui/elementquerywidget.h"
#include "../../../../sources/diagram.h"
#include "../../../../sources/qetgraphicsitem/terminal.h"
#include "../../../../sources/qetproject.h"
#include "../../../../sources/ui/bomexportdialog.h"

#include <catch2/catch.hpp>

#include <QCheckBox>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector>

/*
	A linha da lista de material que não diz nada, e a que parece com ela e
	é peça comprada.

	Medido num projeto de quatorze folhas: cem das trezentas e cinquenta e
	quatro linhas da lista traziam só o título e o número da folha. Setenta e
	uma vinham da folha das réguas, e o que estava por trás delas eram tampas
	de fim de régua e postes finais — marcador desenhado, não peça.

	O perigo do conserto é maior que o defeito, e é o que este arquivo mede.
	A tampa de fim de régua é um elemento do tipo Terminal, exatamente como o
	borne ao lado dela: separar por espécie de elemento levaria o borne
	junto, e borne é peça comprada. Separar por "não tem fio ligado" leva
	disjuntor de projeto unifilar, o que foi medido nos exemplos que
	acompanham o programa. O que sobra é o que a própria linha diz — e é por
	isso que os casos abaixo afirmam sempre as duas coisas:

	1. quantas linhas a lista tem, que pega a contaminação grosseira; e
	2. quanto elas somam, que é o que pega a fina.

	Uma delas sozinha não basta, e a razão é aritmética: um filtro que tire
	uma linha a mais do que devia mantém a contagem plausível e some com uma
	peça, e um agrupamento que junte demais também. A soma da coluna de
	quantidade sobre a lista inteira é invariante — todo componente é contado
	exatamente uma vez — e é ela que diferencia os dois.
*/

namespace
{
	/// Um componente do projeto de prova, como a folha o guarda.
	struct Instance
	{
		const char *definition;
		const char *link_type;
		const char *kind_informations;
		const char *label;
		const char *designation;
		const char *part_code;
	};

	/**
		Os sete elementos desenhados, e cada um está aqui por uma pergunta.

		X1 e X2 são bornes de verdade: carregam etiqueta e nada mais, que é
		como a esmagadora maioria dos bornes de um projeto real é
		preenchida — medido, cento e cinco bornes num projeto de quatorze
		folhas, todos com etiqueta e sem designação. Eles são a prova de que
		o filtro não come peça comprada.

		A tampa de fim de régua é do mesmo tipo Terminal que eles e não
		carrega informação nenhuma. O poste final é do tipo Simple e também
		não. São os dois marcadores, e são o defeito.

		K1 é um componente comum, com código de peça.

		Os dois sinaleiros não têm etiqueta e têm a mesma designação: eles
		são uma linha só, de quantidade dois. Sem eles a soma e a contagem
		dariam o mesmo número, e um caso em que as duas coincidem não prova
		que são duas asserções.
	*/
	const Instance instances[] = {
		{"strip.elmt", "terminal",
		 "<kindInformations>"
		 "<kindInformation name=\"type\">generic</kindInformation>"
		 "<kindInformation name=\"function\">generic</kindInformation>"
		 "</kindInformations>",
		 "X1", "", ""},
		{"strip.elmt", "terminal",
		 "<kindInformations>"
		 "<kindInformation name=\"type\">generic</kindInformation>"
		 "<kindInformation name=\"function\">generic</kindInformation>"
		 "</kindInformations>",
		 "X2", "", ""},
		{"endcap.elmt", "terminal",
		 "<kindInformations>"
		 "<kindInformation name=\"type\">generic</kindInformation>"
		 "<kindInformation name=\"function\">generic</kindInformation>"
		 "</kindInformations>",
		 "", "", ""},
		{"box.elmt", "simple", "", "K1", "LC1D09", "XA-100"},
		{"lamp.elmt", "simple", "", "", "Sinaleiro 22 mm", ""},
		{"lamp.elmt", "simple", "", "", "Sinaleiro 22 mm", ""},
		{"stop.elmt", "simple", "", "", "", ""}};

	/// Quantos elementos a folha desenha
	int drawnCount()
	{
		return int(sizeof(instances) / sizeof(instances[0]));
	}

	/// @return true se @a instance carrega alguma informação
	bool isNamed(const Instance &instance)
	{
		return instance.label[0] != '\0'
				|| instance.designation[0] != '\0'
				|| instance.part_code[0] != '\0';
	}

	/// Quantos deles nomeiam alguma coisa, e portanto entram na lista
	int namedCount()
	{
		int named = 0;
		for (const Instance &instance : instances)
		{
			if (isNamed(instance)) {
				++named;
			}
		}
		return named;
	}

	/**
		Quantas linhas a lista agrupada tem.

		Um por conjunto distinto de valores publicados, entre os que
		nomeiam alguma coisa — que para este projeto de prova são etiqueta,
		designação e código de peça, porque as outras colunas estão vazias
		em todos. Contado daqui, e não escrito como literal, para que um
		projeto de prova que ganhe um elemento não deixe de provar isto em
		silêncio.
	*/
	int groupCount()
	{
		QStringList seen;
		for (const Instance &instance : instances)
		{
			if (!isNamed(instance)) {
				continue;
			}
				//Separador que não pode aparecer num valor, para que
				//dois campos diferentes não formem a mesma chave.
			const QChar separator = QChar(ushort(0x1f));
			const QString key = QString::fromUtf8(instance.label)
					+ separator
					+ QString::fromUtf8(instance.designation)
					+ separator
					+ QString::fromUtf8(instance.part_code);
			if (!seen.contains(key)) {
				seen << key;
			}
		}
		return int(seen.size());
	}

	/// @return o XML de um projeto que desenha os elementos acima
	QString fixtureXml()
	{
		auto information = [](const QString &name, const QString &value)
		{
			if (value.isEmpty()) {
				return QString();
			}
			return QStringLiteral(
				       "<elementInformation show=\"1\" name=\"%1\">%2"
				       "</elementInformation>")
			       .arg(name, value.toHtmlEscaped());
		};

			//O ponto de encaixe de um terminal, que é o que a instância
			//guarda.
		const qreal east_dock = 10. - Terminal::terminalSize;
		const qreal west_dock = -10. + Terminal::terminalSize;

		QStringList written_definitions;
		QString definitions;
		QString drawn;
		int index = 0;
		int x = 100;
		for (const Instance &instance : instances)
		{
				//Dois sinaleiros, uma definição: dois elementos da mesma
				//espécie é justamente o caso da linha agrupada.
			const QString definition_name =
				QString::fromLatin1(instance.definition);
			if (!written_definitions.contains(definition_name))
			{
				written_definitions << definition_name;
				definitions += QStringLiteral(
						       "<element name=\"%1\">"
						       "<definition type=\"element\" version=\"0.80\""
						       " width=\"30\" height=\"20\""
						       " hotspot_x=\"15\" hotspot_y=\"10\""
						       " orientation=\"dnnn\" link_type=\"%2\">"
						       "<names><name lang=\"en\">%1</name></names>"
						       "%3"
						       "<description>"
						       "<rect x=\"-8\" y=\"-8\" width=\"16\" height=\"16\""
						       " antialias=\"false\""
						       " style=\"line-style:normal;line-weight:normal;"
						       "filling:none;color:black\"/>"
						       "<terminal x=\"10\" y=\"0\" orientation=\"e\" name=\"1\"/>"
						       "<terminal x=\"-10\" y=\"0\" orientation=\"w\" name=\"2\"/>"
						       "</description>"
						       "</definition>"
						       "</element>")
						//Uma de cada vez, e não a forma de três
						//argumentos: %1 aparece duas vezes de
						//propósito, e cada chamada troca todas as
						//ocorrências do menor número restante.
					       .arg(QLatin1String(instance.definition))
					       .arg(QLatin1String(instance.link_type))
					       .arg(QLatin1String(instance.kind_informations));
			}

			QString info;
			info += information(QStringLiteral("label"),
					    QString::fromUtf8(instance.label));
			info += information(QStringLiteral("designation"),
					    QString::fromUtf8(instance.designation));
			info += information(QStringLiteral("part_code"),
					    QString::fromUtf8(instance.part_code));

			drawn += QStringLiteral(
					 "<element x=\"%1\" y=\"200\" z=\"10\" prefix=\"\""
					 " freezeLabel=\"false\" orientation=\"0\""
					 " type=\"embed://bench/%2\""
					 " uuid=\"{b0b00000-0000-4000-8000-00000000000%3}\">"
					 "<terminals>"
					 "<terminal x=\"%4\" y=\"0\" orientation=\"1\" id=\"%5\"/>"
					 "<terminal x=\"%6\" y=\"0\" orientation=\"3\" id=\"%7\"/>"
					 "</terminals>"
					 "<inputs/>"
					 "<elementInformations>%8</elementInformations>"
					 "<dynamic_texts/><texts_groups/>"
					 "</element>")
				 .arg(x)
				 .arg(QLatin1String(instance.definition))
				 .arg(index)
				 .arg(east_dock)
				 .arg(index * 2)
				 .arg(west_dock)
				 .arg(index * 2 + 1)
				 .arg(info);
			x += 100;
			++index;
		}

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "<collection>"
			       "<category name=\"bench\">%1</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"600\""
			       " cols=\"17\" colsize=\"50\" rows=\"8\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%2</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(definitions, drawn);
	}

	/// As colunas de informação que a lista de montagem publica, na ordem
	/// em que exportBom() as publica.
	QStringList bomColumns()
	{
		return QStringList {QStringLiteral("label"),
				    QStringLiteral("designation"),
				    QStringLiteral("manufacturer"),
				    QStringLiteral("manufacturer_reference"),
				    QStringLiteral("quantity"),
				    QStringLiteral("unity"),
				    QStringLiteral("location"),
				    QStringLiteral("location_path"),
				    QStringLiteral("function")};
	}

	/**
		@return o resultado de @a statement como um inteiro, ou -1

		Menos um, e não zero, quando a consulta não roda: zero é uma
		resposta legítima de toda contagem abaixo, e as duas confundidas
		fariam uma consulta quebrada passar por uma lista vazia.
	*/
	int countOf(projectDataBase *data_base, const QString &statement)
	{
		QSqlQuery query = data_base->newQuery(statement);
		if (!query.exec() || !query.next()) {
			return -1;
		}
		return query.value(0).toInt();
	}

	/**
		O leitor de csv deste arquivo, e ele é deliberadamente simples.

		Nenhum valor do projeto de prova carrega ponto e vírgula, aspas nem
		quebra de linha, então a divisão crua responde. A regra de escape
		tem bancada própria — csvwriter_test.cpp do C_unittests e
		csvexport_test.cpp aqui — e não se repete neste arquivo.
	*/
	QVector<QStringList> rowsOf(const QString &text)
	{
		QVector<QStringList> rows;
		const QStringList lines =
			text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
		for (const QString &line : lines) {
			rows << line.split(QLatin1Char(';'));
		}
		return rows;
	}

	/// @return a soma da coluna de contagem sobre as linhas de dados
	int quantitySum(const QVector<QStringList> &rows, int column)
	{
		int sum = 0;
			//A partir de 1: a primeira linha é o cabeçalho.
		for (int i = 1 ; i < rows.size() ; ++i)
		{
			if (column < rows.at(i).size()) {
				sum += rows.at(i).at(column).toInt();
			}
		}
		return sum;
	}

	/// @return a primeira linha que traz @a value em alguma célula
	QStringList rowHolding(const QVector<QStringList> &rows,
			       const QString &value)
	{
		for (const QStringList &row : rows) {
			if (row.contains(value)) {
				return row;
			}
		}
		return QStringList();
	}

	/// @return false quando @a content não pôde ser escrito em @a path
	bool writeFile(const QString &path, const QString &content)
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			return false;
		}
		QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		stream.setCodec("UTF-8");
#endif
		stream << content;
		file.close();
		return true;
	}

	/// @return o texto inteiro de @a path, ou uma string nula
	QString readFile(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			return QString();
		}
		QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		stream.setCodec("UTF-8");
#endif
		return stream.readAll();
	}
}

TEST_CASE("T16 — o marcador de fim de régua sai da lista e o borne fica",
	  "[bom][database][t16]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("bomnameless.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	Diagram *sheet = scratch.diagram(0);
	REQUIRE(sheet != nullptr);
		//Que a folha desenhou o que o projeto de prova manda desenhar. Sem
		//esta linha, um elemento que não carregasse deixaria as contagens
		//abaixo relatando um defeito de filtro que não existe.
	REQUIRE(sheet->elements().count() == drawnCount());

	projectDataBase *data_base = scratch.project()->dataBase();
	REQUIRE(data_base != nullptr);
	data_base->updateDB();

		//A sonda: sem os dois marcadores desenhados, tudo abaixo passaria
		//sobre um projeto que não mede nada.
	REQUIRE(drawnCount() - namedCount() == 2);

	SECTION("a lista de material perde as linhas que não nomeiam nada")
	{
			//A contagem, que pega a contaminação grosseira. Escrita como
			//o número de componentes que nomeiam alguma coisa, e não como
			//literal, para que um projeto de prova que cresça não deixe
			//de provar isto em silêncio.
		CHECK(countOf(data_base,
			      QStringLiteral("SELECT COUNT(*)"
					     " FROM element_nomenclature_view"))
		      == namedCount());

			//E o número dos que ficaram de fora, que é o que impede que
			//a perda seja silenciosa.
		CHECK(data_base->namelessComponentCount()
		      == drawnCount() - namedCount());
	}

	SECTION("a etiqueta continua marcando tudo o que a folha desenha")
	{
			//A outra visão não é a mesma pergunta, e este é o caso que
			//guarda a diferença: um item fora da lista de compra continua
			//tendo de ser marcado no trilho. Um coletor lendo a visão
			//filtrada deixaria buracos na marcação que ninguém percebe
			//até a montagem.
		CHECK(countOf(data_base,
			      QStringLiteral("SELECT COUNT(*)"
					     " FROM element_label_view"))
		      == drawnCount());
	}

	SECTION("borne de verdade é peça comprada e continua na lista")
	{
			//Os dois bornes carregam etiqueta e nada mais — que é como um
			//projeto real os preenche — e são do mesmo tipo Terminal que a
			//tampa de fim de régua que saiu. Este é o erro oposto, e é o
			//caro: uma lista de compra sem os bornes é lida sem ninguém
			//desconfiar.
		for (const QString &label : {QStringLiteral("X1"),
					     QStringLiteral("X2")})
		{
			INFO(label.toStdString());
			CHECK(countOf(data_base,
				      QStringLiteral("SELECT COUNT(*)"
						     " FROM element_nomenclature_view"
						     " WHERE label = '%1'").arg(label))
			      == 1);
		}

			//E os dois marcadores, pelo tipo: nenhuma linha de Terminal
			//sem etiqueta sobrou, e ainda assim as duas de cima estão lá.
		CHECK(countOf(data_base,
			      QStringLiteral("SELECT COUNT(*)"
					     " FROM element_nomenclature_view"
					     " WHERE COALESCE(label,'') = ''"
					     " AND COALESCE(designation,'') = ''"))
		      == 0);
	}
}

TEST_CASE("T16 — a lista pela linha de comando conta o agrupamento, como a "
	  "janela", "[bom][database][t16]")
{
		//Escrito num arquivo próprio e nunca aberto neste processo: a
		//linha de comando abre o projeto sozinha, e abrir o mesmo arquivo
		//duas vezes é pergunta que este caso não faz.
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("cli.qet"));
	const QString csv_path = dir.filePath(QStringLiteral("bom.csv"));
	REQUIRE(writeFile(project_path, fixtureXml()));

	const int code = CLIExport::run(QStringList()
					<< QStringLiteral("--export-bom")
					<< project_path << csv_path);
	REQUIRE(code == 0);

	const QString csv = readFile(csv_path);
	INFO(csv.toStdString());
	REQUIRE_FALSE(csv.isEmpty());

	const QVector<QStringList> rows = rowsOf(csv);
	REQUIRE_FALSE(rows.isEmpty());

		//A coluna da contagem, achada pelo cabeçalho e não por posição
		//decorada: quem acrescentar uma coluna antes dela não deve
		//conseguir transformar este caso num falso verde.
	const int quantity_column = static_cast<int>(
		rows.first().indexOf(QStringLiteral("designation_qty")));
	REQUIRE(quantity_column >= 0);

	SECTION("a coluna de quantidade deixa de vir vazia")
	{
			//O defeito como ele era: --export-bom publicava a coluna
			//`quantity`, que é propriedade que o projetista digita, e que
			//estava vazia em trezentas e cinquenta e quatro de trezentas
			//e cinquenta e quatro linhas do projeto medido. A janela
			//contava. As duas rotas respondiam coisas diferentes para o
			//mesmo projeto, e nenhuma dizia que discordava.
		CHECK(quantitySum(rows, quantity_column) == namedCount());

			//A coluna digitada continua publicada ao lado da contagem:
			//trilho e canaleta são comprados por metro, e o metro é
			//digitado. Perder a coluna seria perder isso.
		CHECK(rows.first().contains(QStringLiteral("quantity")));
		CHECK(rows.first().contains(QStringLiteral("unity")));
	}

	SECTION("a contagem e a soma não são o mesmo número")
	{
			//Quatro linhas de dados para cinco componentes: os dois
			//sinaleiros são uma linha de quantidade dois. Um caso em que
			//os dois números coincidissem não provaria que são duas
			//asserções, e é por isso que o projeto de prova tem dois
			//iguais dentro dele.
		REQUIRE(groupCount() < namedCount());
		CHECK(int(rows.size()) - 1 == groupCount());
		CHECK(quantitySum(rows, quantity_column) == namedCount());

		const QStringList lamp =
			rowHolding(rows, QString::fromUtf8("Sinaleiro 22 mm"));
		REQUIRE(lamp.size() > quantity_column);
		CHECK(lamp.at(quantity_column) == QStringLiteral("2"));
	}

	SECTION("o marcador não chega ao arquivo e o borne chega")
	{
		CHECK_FALSE(rowHolding(rows, QStringLiteral("X1")).isEmpty());
		CHECK_FALSE(rowHolding(rows, QStringLiteral("X2")).isEmpty());

			//E nenhuma linha de dados sem nome nenhum: a folha desenha
			//dois marcadores, e o arquivo não tem linha para eles.
		for (int i = 1 ; i < rows.size() ; ++i)
		{
			INFO(rows.at(i).join(QStringLiteral(";")).toStdString());
				//The extra parentheses are not style: Catch2 decomposes
				//the expression to print both sides, and refuses a bare
				//&& inside an assertion with a static_assert.
			CHECK_FALSE((rows.at(i).at(0).isEmpty()
				     && rows.at(i).at(1).isEmpty()));
		}
	}
}

TEST_CASE("T16 — as duas rotas da lista de material respondem o mesmo",
	  "[bom][database][t16]")
{
	/*
		A paridade, medida e não suposta.

		Os dois caminhos até a lista de material são a janela e
		--export-bom, e eles discordavam: a janela agrupa e conta, a linha
		de comando publicava uma propriedade digitada. Depois deste passo
		os dois leem a mesma regra de agrupamento, de
		sources/dataBase/bomquery.h, e o que se afirma aqui é o resultado
		dela — o mesmo número de linhas e a mesma soma, para o mesmo
		conjunto de colunas.
	*/
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("bomparity.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	const QStringList columns = bomColumns();

	BOMExportDialog dialog(scratch.project());
	ElementQueryWidget *widget = dialog.findChild<ElementQueryWidget *>();
	REQUIRE(widget != nullptr);

		//A linha de cabeçalho, porque as duas contagens abaixo são
		//escritas sabendo que existe uma de cada lado. Uma janela que
		//deixasse de escrevê-la deslocaria só um dos dois números e este
		//caso relataria uma diferença de agrupamento que não existe.
	QCheckBox *headers =
		dialog.findChild<QCheckBox *>(QStringLiteral("m_include_headers"));
	REQUIRE(headers != nullptr);
	REQUIRE(headers->isChecked());

	widget->setQuery(QStringLiteral("SELECT ")
			 + columns.join(QStringLiteral(", "))
			 + QStringLiteral(" FROM element_nomenclature_view"));
	widget->setCount(QStringLiteral("COUNT(*) AS designation_qty"));
	widget->setGroupBy(QETBom::groupByColumns(columns)
			   .join(QStringLiteral(", ")));

	const QString built = widget->queryStr();
	INFO(built.toStdString());
	REQUIRE_FALSE(built.isEmpty());

		//A sonda que desarma a caixa modal de dentro de getBom(): se a
		//consulta não roda, o caso para nesta linha em vez de esperar por
		//um clique que ninguém vai dar.
	scratch.project()->dataBase()->updateDB();
	QSqlQuery probe = scratch.project()->dataBase()->newQuery(built);
	const bool probe_ran = probe.exec();
	INFO(probe.lastError().text().toStdString());
	REQUIRE(probe_ran);

	const QVector<QStringList> window_rows = rowsOf(dialog.getBom());
	REQUIRE_FALSE(window_rows.isEmpty());

		//E a mesma lista pela linha de comando.
	QTemporaryDir dir;
	REQUIRE(dir.isValid());
	const QString project_path = dir.filePath(QStringLiteral("parity.qet"));
	const QString csv_path = dir.filePath(QStringLiteral("parity.csv"));
	REQUIRE(writeFile(project_path, fixtureXml()));
	REQUIRE(CLIExport::run(QStringList()
			       << QStringLiteral("--export-bom")
			       << project_path << csv_path) == 0);

	const QVector<QStringList> cli_rows = rowsOf(readFile(csv_path));
	REQUIRE_FALSE(cli_rows.isEmpty());

		//A contagem de linhas, que pega a contaminação grosseira.
	CHECK(window_rows.size() == cli_rows.size());

		//E a soma, que pega a fina.

		//A coluna da contagem da janela é achada pela posição, e não pelo
		//nome: o cabeçalho que ela escreve é traduzido para a língua da
		//interface — "Quantité numéro d'article" —, enquanto a linha de
		//comando escreve a chave crua. Procurar pelo nome ali acharia -1 e
		//o caso passaria a somar a coluna errada. A posição é a que
		//ElementQueryWidget monta: as colunas de informação primeiro, a
		//contagem logo depois delas.
	const int window_column = int(bomColumns().size());
	const int cli_column = static_cast<int>(
		cli_rows.first().indexOf(QStringLiteral("designation_qty")));
	INFO("janela em " << window_column << ", linha de comando em "
	     << cli_column);
	REQUIRE(cli_column >= 0);

		//Que as duas escrevam a contagem na mesma posição é o que se
		//afirma, e não o que se supõe: uma coluna acrescentada de um lado
		//só desloca uma das duas e este caso reprova.
	CHECK(window_column == cli_column);

		//E a sonda da linha acima, que é o que impede um falso verde: se
		//a posição escolhida não fosse a da contagem, a soma daria zero em
		//vez do número de componentes.
	CHECK(quantitySum(window_rows, window_column) == namedCount());
	CHECK(quantitySum(cli_rows, cli_column) == namedCount());
	CHECK(quantitySum(window_rows, window_column)
	      == quantitySum(cli_rows, cli_column));
}
