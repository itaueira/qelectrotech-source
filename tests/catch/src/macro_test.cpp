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
#include "../../../sources/macro/macroparameter.h"
#include "../../../sources/macro/macroparameterset.h"
#include "../../../sources/macro/macrosubstitution.h"
#include "qt_catch_tostring.h"

#include <QDomDocument>
#include <QTextStream>

namespace
{
		/**
			One macro that declares six variables, offers two value sets and
			carries the markers where a real drawing carries them: in the
			element information, in the manufacturer reference, in the
			comment, on the conductor section and in a free text. The shapes
			below are the ones read out of the real project this fork exists
			to serve, down to the "2,5mm²" of a conductor section.
		*/
	QString macroXml()
	{
		return QString::fromUtf8(
			"<qet_macro>"
			" <parameters>"
			"  <parameter name=\"MARCACAO\" label=\"Marquage\" type=\"text\""
			"             default=\"\" required=\"true\"/>"
			"  <parameter name=\"FABRICANTE\" label=\"Fabricant\" type=\"text\""
			"             default=\"ACME\"/>"
			"  <parameter name=\"CODIGO\" label=\"Code article\" type=\"part\" default=\"\"/>"
			"  <parameter name=\"POTENCIA\" label=\"Puissance\" type=\"number\""
			"             default=\"7,5\" unit=\"cv\"/>"
			"  <parameter name=\"SECAO\" label=\"Section\" type=\"list\" default=\"2,5mm²\">"
			"   <choice>1,5mm²</choice>"
			"   <choice>2,5mm²</choice>"
			"   <choice>4mm²</choice>"
			"  </parameter>"
			"  <parameter name=\"OBSERVACAO\" label=\"Commentaire\" type=\"text\" default=\"\"/>"
			" </parameters>"
			" <valuesets>"
			"  <valueset name=\"Partida direta 7,5 cv\">"
			"   <value name=\"MARCACAO\">M1</value>"
			"   <value name=\"FABRICANTE\">ACME</value>"
			"   <value name=\"CODIGO\">MTR-75-4P</value>"
			"   <value name=\"POTENCIA\">7,5</value>"
			"   <value name=\"SECAO\">2,5mm²</value>"
			"   <value name=\"OBSERVACAO\">Ventilador do quadro</value>"
			"  </valueset>"
			"  <valueset name=\"Secao 4\">"
			"   <value name=\"SECAO\">4mm²</value>"
			"  </valueset>"
			" </valuesets>"
			" <collection>"
			"  <element path=\"import/moteur.elmt\"><definition/></element>"
			" </collection>"
			" <diagram_content>"
			"  <diagram>"
			"   <elements>"
			"    <element type=\"moteur\" x=\"10\" y=\"20\">"
			"     <elementInformations>"
			"      <elementInformation name=\"label\" show=\"1\">${MARCACAO}</elementInformation>"
			"      <elementInformation name=\"manufacturer_reference\" show=\"1\">"
			"${CODIGO}</elementInformation>"
			"      <elementInformation name=\"comment\" show=\"1\">${OBSERVACAO}</elementInformation>"
			"     </elementInformations>"
			"    </element>"
			"   </elements>"
			"   <conductors>"
			"    <conductor num=\"1\" conductor_section=\"${SECAO}\""
			"               element1_label=\"${MARCACAO}\"/>"
			"   </conductors>"
			"   <inputs>"
			"    <input x=\"0\" y=\"0\" text=\"Moteur ${POTENCIA} cv - ${FABRICANTE}\"/>"
			"    <input x=\"0\" y=\"8\" text=\"Repère littéral $${MARCACAO}\"/>"
			"   </inputs>"
			"  </diagram>"
			" </diagram_content>"
			"</qet_macro>");
	}

		/**
			The same macro as every macro made before this task existed: no
			<parameters>, no <valuesets>, and QElectroTech's own "%{...}"
			variables in the title block text, because those are what a
			drawing really carries.
		*/
	QString legacyMacroXml()
	{
		return QString::fromUtf8(
			"<qet_macro>"
			" <collection>"
			"  <element path=\"import/moteur.elmt\"><definition/></element>"
			" </collection>"
			" <diagram_content>"
			"  <diagram>"
			"   <elements>"
			"    <element type=\"moteur\" x=\"10\" y=\"20\">"
			"     <elementInformations>"
			"      <elementInformation name=\"label\" show=\"1\">M1</elementInformation>"
			"     </elementInformations>"
			"    </element>"
			"   </elements>"
			"   <inputs>"
			"    <input x=\"0\" y=\"0\" text=\"Folio %{folio} - %{title}\"/>"
			"   </inputs>"
			"  </diagram>"
			" </diagram_content>"
			"</qet_macro>");
	}

	QDomElement diagramOf(const QDomDocument &document)
	{
		return document.documentElement()
				.firstChildElement(QStringLiteral("diagram_content"))
				.firstChildElement(QStringLiteral("diagram"));
	}

		/// The first @a tag under @a root whose name attribute is @a name.
	QDomElement findNamed(const QDomElement &root, const QString &tag, const QString &name)
	{
		if (root.tagName() == tag
		    && root.attribute(QStringLiteral("name")) == name) {
			return root;
		}
		for (QDomElement child = root.firstChildElement() ;
		     !child.isNull() ;
		     child = child.nextSiblingElement())
		{
			const QDomElement found = findNamed(child, tag, name);
			if (!found.isNull()) {
				return found;
			}
		}
		return QDomElement();
	}

		/// The first @a tag under @a root, whatever it is named.
	QDomElement findFirst(const QDomElement &root, const QString &tag)
	{
		if (root.tagName() == tag) {
			return root;
		}
		for (QDomElement child = root.firstChildElement() ;
		     !child.isNull() ;
		     child = child.nextSiblingElement())
		{
			const QDomElement found = findFirst(child, tag);
			if (!found.isNull()) {
				return found;
			}
		}
		return QDomElement();
	}

		/// The @a index'th <input> under @a root, in file order.
	QDomElement inputAt(const QDomElement &root, int index)
	{
		const QDomElement inputs = findFirst(root, QStringLiteral("inputs"));
		QDomElement child = inputs.firstChildElement(QStringLiteral("input"));
		for (int i = 0 ; i < index && !child.isNull() ; ++ i) {
			child = child.nextSiblingElement(QStringLiteral("input"));
		}
		return child;
	}

	QString serialise(const QDomNode &node)
	{
		QString out;
		QTextStream stream(&out);
		node.save(stream, 0);
		return out;
	}
}

TEST_CASE("CU-05.1 — as seis variáveis do macro são lidas", "[macro]")
{
	QDomDocument document;
	REQUIRE(document.setContent(macroXml()));

	MacroParameterSet set;
	REQUIRE(set.fromXml(document.documentElement()));

	SECTION("são seis, na ordem em que o arquivo as declara")
	{
			//Declaration order, not alphabetical: it is the order the
			//dialogue of the T06 will read the fields from, and someone
			//fills in the marking before the conductor section.
		CHECK(set.count() == 6);
		CHECK(set.names() == QStringList{QStringLiteral("MARCACAO"),
						 QStringLiteral("FABRICANTE"),
						 QStringLiteral("CODIGO"),
						 QStringLiteral("POTENCIA"),
						 QStringLiteral("SECAO"),
						 QStringLiteral("OBSERVACAO")});
	}

	SECTION("cada uma chega tipada, com rótulo, unidade e obrigatoriedade")
	{
		const MacroParameter marking = set.parameter(QStringLiteral("MARCACAO"));
		CHECK(marking.type == MacroParameterType::Text);
		CHECK(marking.label == QStringLiteral("Marquage"));
		CHECK(marking.required);

		CHECK(set.parameter(QStringLiteral("CODIGO")).type == MacroParameterType::Part);

		const MacroParameter power = set.parameter(QStringLiteral("POTENCIA"));
		CHECK(power.type == MacroParameterType::Number);
		CHECK(power.unit == QStringLiteral("cv"));
		CHECK_FALSE(power.required);

		const MacroParameter section = set.parameter(QStringLiteral("SECAO"));
		CHECK(section.type == MacroParameterType::List);
		CHECK(section.choices.size() == 3);
		CHECK(section.choices.contains(QString::fromUtf8("4mm²")));
	}

	SECTION("cada uma sabe dizer a que marcador responde")
	{
		CHECK(set.parameter(QStringLiteral("MARCACAO")).marker()
		      == QStringLiteral("${MARCACAO}"));
	}

	SECTION("os padrões são o estado em que o diálogo abre")
	{
		const QHash<QString, QString> defaults = set.defaults();
		CHECK(defaults.size() == 6);
		CHECK(defaults.value(QStringLiteral("MARCACAO")).isEmpty());
		CHECK(defaults.value(QStringLiteral("FABRICANTE")) == QStringLiteral("ACME"));
		CHECK(defaults.value(QStringLiteral("SECAO")) == QString::fromUtf8("2,5mm²"));
	}

	SECTION("um tipo que este build não conhece é lido como texto, e não recusado")
	{
			//A macro written by a newer version still has to come in: the
			//drawing is the point, and an unknown type still substitutes.
		QDomDocument newer;
		REQUIRE(newer.setContent(QStringLiteral(
			"<qet_macro><parameters>"
			"<parameter name=\"COR\" type=\"colour-picker\" default=\"vermelho\"/>"
			"</parameters></qet_macro>")));

		MacroParameterSet read;
		REQUIRE(read.fromXml(newer.documentElement()));
		CHECK(read.count() == 1);
		CHECK(read.parameter(QStringLiteral("COR")).type == MacroParameterType::Text);
		CHECK(read.parameter(QStringLiteral("COR")).default_value
		      == QStringLiteral("vermelho"));
	}
}

TEST_CASE("CU-05.2 — a substituição atinge rótulo, referência, comentário, seção e texto livre",
	  "[macro]")
{
	QDomDocument document;
	REQUIRE(document.setContent(macroXml()));

	MacroParameterSet set;
	REQUIRE(set.fromXml(document.documentElement()));

	QHash<QString, QString> values = set.defaults();
	values.insert(QStringLiteral("MARCACAO"), QStringLiteral("M7"));
	values.insert(QStringLiteral("CODIGO"), QStringLiteral("MTR-75-4P"));
	values.insert(QStringLiteral("OBSERVACAO"), QStringLiteral("Ventilador do quadro"));

	QDomElement diagram = diagramOf(document);
	REQUIRE_FALSE(diagram.isNull());

	const MacroSubstitution::Result result = MacroSubstitution::apply(diagram, values);

	SECTION("nada fica por resolver")
	{
		CHECK(result.ok);
		CHECK(result.orphans.isEmpty());
		CHECK(result.errorText().isEmpty());
			//label, manufacturer_reference, comment, conductor_section,
			//element1_label, and the two of the free text.
		CHECK(result.replacements == 7);
	}

	SECTION("o valor chega a cada um dos cinco lugares")
	{
		CHECK(findNamed(diagram, QStringLiteral("elementInformation"),
				QStringLiteral("label")).text()
		      == QStringLiteral("M7"));
		CHECK(findNamed(diagram, QStringLiteral("elementInformation"),
				QStringLiteral("manufacturer_reference")).text()
		      == QStringLiteral("MTR-75-4P"));
		CHECK(findNamed(diagram, QStringLiteral("elementInformation"),
				QStringLiteral("comment")).text()
		      == QStringLiteral("Ventilador do quadro"));

		const QDomElement conductor = findFirst(diagram, QStringLiteral("conductor"));
		CHECK(conductor.attribute(QStringLiteral("conductor_section"))
		      == QString::fromUtf8("2,5mm²"));
			//conductor/@element1_label mirrors the label of the element the
			//conductor is tied to, which is why the walk has no whitelist of
			//fields: a whitelist would have to know that.
		CHECK(conductor.attribute(QStringLiteral("element1_label"))
		      == QStringLiteral("M7"));

		CHECK(inputAt(diagram, 0).attribute(QStringLiteral("text"))
		      == QStringLiteral("Moteur 7,5 cv - ACME"));
	}

	SECTION("nenhum marcador sobra onde havia marcador")
	{
			//O único "${" que resta no desenho é o que o autor pediu com a
			//fuga "$${". Fora dele, não há marcador em pé em lugar nenhum —
			//e é por isso que a conta é do documento inteiro, e não da lista
			//de campos que alguém se lembrou de conferir.
		CHECK_FALSE(serialise(findFirst(diagram, QStringLiteral("elements")))
			    .contains(QStringLiteral("${")));
		CHECK_FALSE(serialise(findFirst(diagram, QStringLiteral("conductors")))
			    .contains(QStringLiteral("${")));
		CHECK_FALSE(inputAt(diagram, 0).attribute(QStringLiteral("text"))
			    .contains(QStringLiteral("${")));
		CHECK(serialise(diagram).count(QStringLiteral("${")) == 1);
	}

	SECTION("$${ escreve um ${ literal, e não é marcador nenhum")
	{
		CHECK(inputAt(diagram, 1).attribute(QStringLiteral("text"))
		      == QString::fromUtf8("Repère littéral ${MARCACAO}"));

			//E a mesma fuga dentro de um texto formatado não é confundida
			//com um marcador partido pela formatação: o reexame lê o texto
			//como ele entrou, onde a fuga é visivelmente uma fuga.
		QDomDocument rich;
		REQUIRE(rich.setContent(QString::fromUtf8(
			"<qet_macro><diagram_content><diagram><inputs>"
			"<input x=\"0\" y=\"0\" text=\"&lt;b&gt;Repère&lt;/b&gt; $${MARCACAO}\"/>"
			"</inputs></diagram></diagram_content></qet_macro>")));

		QDomElement other = diagramOf(rich);
		const MacroSubstitution::Result escaped = MacroSubstitution::apply(other, values);
		CHECK(escaped.ok);
		CHECK(escaped.orphans.isEmpty());
		CHECK(inputAt(other, 0).attribute(QStringLiteral("text"))
		      == QString::fromUtf8("<b>Repère</b> ${MARCACAO}"));
	}

	SECTION("o valor substituído é literal: não é lido de novo")
	{
			//A value that named another variable would ask for an evaluation
			//order and a cycle check, and would pay for them with a new way
			//for a macro to come out half done without saying so.
		QHash<QString, QString> tricky;
		tricky.insert(QStringLiteral("A"), QStringLiteral("${B}"));
		tricky.insert(QStringLiteral("B"), QStringLiteral("nunca"));
		CHECK(MacroSubstitution::substitute(QStringLiteral("${A}"), tricky)
		      == QStringLiteral("${B}"));
	}

	SECTION("a segunda inserção parte dos marcadores, e não do que a primeira escreveu")
	{
			//É para isto que o DiagramEventAddMacro substitui sobre um clone:
			//o documento que ele guarda em memória continua sendo o macro
			//como foi lido. Duas inserções são duas passagens sobre os mesmos
			//marcadores, e nunca uma passagem sobre o resultado da outra —
			//que substituiria o "${" que uma fuga acabara de escrever.
		QDomDocument second;
		REQUIRE(second.setContent(macroXml()));
		QDomElement other = diagramOf(second);

		QHash<QString, QString> again = values;
		again.insert(QStringLiteral("MARCACAO"), QStringLiteral("M8"));

		const MacroSubstitution::Result result_again = MacroSubstitution::apply(other, again);
		CHECK(result_again.ok);
		CHECK(result_again.replacements == 7);
		CHECK(findNamed(other, QStringLiteral("elementInformation"),
				QStringLiteral("label")).text()
		      == QStringLiteral("M8"));
		CHECK(inputAt(other, 1).attribute(QStringLiteral("text"))
		      == QString::fromUtf8("Repère littéral ${MARCACAO}"));
	}
}

TEST_CASE("CU-05.3 — obrigatória em branco recusa e nomeia a variável", "[macro]")
{
	QDomDocument document;
	REQUIRE(document.setContent(macroXml()));

	MacroParameterSet set;
	REQUIRE(set.fromXml(document.documentElement()));

	SECTION("ausente, vazia e só com espaços contam todas como em branco")
	{
		QHash<QString, QString> absent = set.defaults();
		absent.remove(QStringLiteral("MARCACAO"));
		CHECK(set.missingRequired(absent)
		      == QStringList{QStringLiteral("MARCACAO")});

		QHash<QString, QString> empty = set.defaults();
		empty.insert(QStringLiteral("MARCACAO"), QString());
		CHECK(set.missingRequired(empty)
		      == QStringList{QStringLiteral("MARCACAO")});

		QHash<QString, QString> spaces = set.defaults();
		spaces.insert(QStringLiteral("MARCACAO"), QStringLiteral("   "));
		CHECK(set.missingRequired(spaces)
		      == QStringList{QStringLiteral("MARCACAO")});
	}

	SECTION("preenchida, não falta nada")
	{
		QHash<QString, QString> filled = set.defaults();
		filled.insert(QStringLiteral("MARCACAO"), QStringLiteral("M7"));
		CHECK(set.missingRequired(filled).isEmpty());
	}

	SECTION("a recusa traz o rótulo, que é o que o usuário vê na tela")
	{
			//The wording of the refusal lives in DiagramEventAddMacro, where
			//there is a tr() context; what has to be true here is that the
			//name and the label it is built from are both at hand.
		const QStringList missing = set.missingRequired(set.defaults());
		REQUIRE(missing.size() == 1);
		CHECK(set.parameter(missing.first()).label == QStringLiteral("Marquage"));
	}

	SECTION("faltando duas, vêm as duas, na ordem em que foram declaradas")
	{
			//Being told one missing field at a time is the slowest way to
			//fill a form, and a macro is meant to save time. TENSAO comes
			//first in the file and second in the alphabet, which is what
			//makes this a test of declaration order.
		QDomDocument two;
		REQUIRE(two.setContent(QStringLiteral(
			"<qet_macro><parameters>"
			"<parameter name=\"TENSAO\" type=\"text\" required=\"true\"/>"
			"<parameter name=\"MARCACAO\" type=\"text\" required=\"1\"/>"
			"<parameter name=\"LIVRE\" type=\"text\"/>"
			"</parameters></qet_macro>")));

		MacroParameterSet pair;
		REQUIRE(pair.fromXml(two.documentElement()));
		CHECK(pair.missingRequired(pair.defaults())
		      == QStringList{QStringLiteral("TENSAO"), QStringLiteral("MARCACAO")});
	}

	SECTION("um valor para variável que ninguém declarou é dito, não engolido")
	{
		QHash<QString, QString> typo = set.defaults();
		typo.insert(QStringLiteral("MARCACOA"), QStringLiteral("M7"));
		CHECK(set.undeclared(typo) == QStringList{QStringLiteral("MARCACOA")});
	}
}

TEST_CASE("CU-05.4 — o conjunto preenche seis e a troca posterior preserva cinco", "[macro]")
{
	QDomDocument document;
	REQUIRE(document.setContent(macroXml()));

	MacroParameterSet set;
	REQUIRE(set.fromXml(document.documentElement()));

	SECTION("os dois conjuntos são lidos, na ordem do arquivo")
	{
		CHECK(set.valueSetNames() == QStringList{QStringLiteral("Partida direta 7,5 cv"),
							 QStringLiteral("Secao 4")});
		CHECK(set.hasValueSet(QStringLiteral("Secao 4")));
		CHECK_FALSE(set.hasValueSet(QStringLiteral("Nao existe")));
	}

	SECTION("o conjunto preenche as seis de uma vez")
	{
		const QHash<QString, QString> filled =
			set.applyValueSet(QStringLiteral("Partida direta 7,5 cv"), set.defaults());

		CHECK(filled.size() == 6);
		CHECK(set.missingRequired(filled).isEmpty());
		CHECK(filled.value(QStringLiteral("MARCACAO")) == QStringLiteral("M1"));
		CHECK(filled.value(QStringLiteral("CODIGO")) == QStringLiteral("MTR-75-4P"));
		CHECK(filled.value(QStringLiteral("OBSERVACAO"))
		      == QStringLiteral("Ventilador do quadro"));
	}

	SECTION("a troca posterior muda uma e preserva as outras cinco")
	{
		const QHash<QString, QString> filled =
			set.applyValueSet(QStringLiteral("Partida direta 7,5 cv"), set.defaults());
		const QHash<QString, QString> after =
			set.applyValueSet(QStringLiteral("Secao 4"), filled);

		CHECK(after.value(QStringLiteral("SECAO")) == QString::fromUtf8("4mm²"));
		CHECK(after.value(QStringLiteral("MARCACAO")) == QStringLiteral("M1"));
		CHECK(after.value(QStringLiteral("FABRICANTE")) == QStringLiteral("ACME"));
		CHECK(after.value(QStringLiteral("CODIGO")) == QStringLiteral("MTR-75-4P"));
		CHECK(after.value(QStringLiteral("POTENCIA")) == QStringLiteral("7,5"));
		CHECK(after.value(QStringLiteral("OBSERVACAO"))
		      == QStringLiteral("Ventilador do quadro"));
	}

	SECTION("um conjunto que não existe deixa tudo como estava")
	{
		const QHash<QString, QString> filled =
			set.applyValueSet(QStringLiteral("Partida direta 7,5 cv"), set.defaults());
		CHECK(set.applyValueSet(QStringLiteral("Nao existe"), filled) == filled);
	}

	SECTION("gravado de volta, o arquivo sai na ordem das variáveis e não na do QHash")
	{
			//A file that comes out shuffled at every save cannot be diffed,
			//which is the first thing anyone does when a macro misbehaves.
		QDomDocument written;
		QDomElement root = written.createElement(QStringLiteral("qet_macro"));
		written.appendChild(root);
		set.appendToXml(written, root);

		const QString text = serialise(root);
		CHECK(text.indexOf(QStringLiteral("name=\"MARCACAO\">M1")) > 0);
		CHECK(text.indexOf(QStringLiteral("name=\"MARCACAO\">M1"))
		      < text.indexOf(QStringLiteral("name=\"OBSERVACAO\">Ventilador")));

		MacroParameterSet reread;
		REQUIRE(reread.fromXml(root));
		CHECK(reread.names() == set.names());
		CHECK(reread.valueSetNames() == set.valueSetNames());
		CHECK(reread.valueSet(QStringLiteral("Secao 4")).values
		      == set.valueSet(QStringLiteral("Secao 4")).values);
	}
}

TEST_CASE("CU-05.5 — macro sem <parameters> gera XML idêntico ao de hoje", "[macro]")
{
	QDomDocument document;
	REQUIRE(document.setContent(legacyMacroXml()));

	const QString before = serialise(document.documentElement());

	MacroParameterSet set;

	SECTION("ler nenhuma variável não é erro: é todo macro feito até hoje")
	{
		CHECK(set.fromXml(document.documentElement()));
		CHECK(set.isEmpty());
		CHECK(set.count() == 0);
		CHECK(set.valueSetNames().isEmpty());
		CHECK(set.defaults().isEmpty());
		CHECK(set.missingRequired(QHash<QString, QString>()).isEmpty());
	}

	SECTION("gravar de volta não acrescenta um único nó")
	{
		REQUIRE(set.fromXml(document.documentElement()));
		QDomElement root = document.documentElement();
		set.appendToXml(document, root);
		CHECK(serialise(document.documentElement()) == before);
	}

	SECTION("as variáveis do próprio QElectroTech continuam intactas")
	{
			//"%{...}" is QElectroTech's own syntax, and
			//QETInformation::stripUnresolvedVariables() erases every one it
			//meets. A macro marker written that way would be deleted by the
			//program itself, which is the whole reason this task reads "${".
		QDomElement diagram = diagramOf(document);
		REQUIRE_FALSE(diagram.isNull());

		QHash<QString, QString> values;
		values.insert(QStringLiteral("folio"), QStringLiteral("nao deve entrar"));
		values.insert(QStringLiteral("title"), QStringLiteral("nem esta"));

		const MacroSubstitution::Result result = MacroSubstitution::apply(diagram, values);
		CHECK(result.ok);
		CHECK(result.replacements == 0);
		CHECK(inputAt(diagram, 0).attribute(QStringLiteral("text"))
		      == QStringLiteral("Folio %{folio} - %{title}"));
	}
}

TEST_CASE("CU-05.6 — marcador órfão é nomeado, e nada é inserido", "[macro]")
{
	SECTION("um nome que ninguém declarou é dito, e a passagem não passa")
	{
		QDomDocument document;
		REQUIRE(document.setContent(macroXml()));

		MacroParameterSet set;
		REQUIRE(set.fromXml(document.documentElement()));

		QHash<QString, QString> values = set.defaults();
		values.remove(QStringLiteral("OBSERVACAO"));

		QDomElement diagram = diagramOf(document);
		const MacroSubstitution::Result result = MacroSubstitution::apply(diagram, values);

		CHECK_FALSE(result.ok);
		CHECK(result.orphans == QStringList{QStringLiteral("OBSERVACAO")});
		CHECK(result.errorText().contains(QStringLiteral("${OBSERVACAO}")));
	}

	SECTION("os órfãos vêm distintos, e na ordem em que apareceram")
	{
		QStringList orphans;
		const QString text = QStringLiteral("${B} ${A} ${B} ${C}");
		CHECK(MacroSubstitution::substitute(text, QHash<QString, QString>(), nullptr, &orphans)
		      == text);
		CHECK(orphans == QStringList{QStringLiteral("B"),
					     QStringLiteral("A"),
					     QStringLiteral("C")});
	}

	SECTION("um marcador partido pela formatação é nomeado pelo nome certo")
	{
			//An <input text="..."> holds rich text, so a marker someone
			//formatted halfway through is stored split across style runs and
			//no scanner can put it back together. Being told which one it
			//was beats finding ${TAG} drawn on the sheet.
		QDomDocument document;
		REQUIRE(document.setContent(QString::fromUtf8(
			"<qet_macro><diagram_content><diagram><inputs>"
			"<input x=\"0\" y=\"0\" text=\"Moteur ${FA&lt;span style=&quot;"
			"font-weight:600;&quot;&gt;BRICANTE}\"/>"
			"</inputs></diagram></diagram_content></qet_macro>")));

		QHash<QString, QString> values;
		values.insert(QStringLiteral("FABRICANTE"), QStringLiteral("ACME"));

		QDomElement diagram = diagramOf(document);
		const MacroSubstitution::Result result = MacroSubstitution::apply(diagram, values);

		CHECK_FALSE(result.ok);
		CHECK(result.orphans == QStringList{QStringLiteral("FABRICANTE")});
		CHECK(result.errorText().contains(QStringLiteral("${FABRICANTE}")));
			//And the text was left exactly as the author wrote it: refusing
			//is not an excuse to half rewrite the drawing.
		CHECK(inputAt(diagram, 0).attribute(QStringLiteral("text"))
		      .contains(QStringLiteral("${FA<span")));
	}

	SECTION("um ${ sem fecho, ou com chave dentro, não é marcador e fica como está")
	{
		QStringList orphans;
		const QString unclosed = QStringLiteral("total ${MARCACAO sem fecho");
		CHECK(MacroSubstitution::substitute(unclosed, QHash<QString, QString>(),
						    nullptr, &orphans)
		      == unclosed);
		CHECK(orphans.isEmpty());

		const QString braced = QStringLiteral("css ${a{b}} do usuario");
		CHECK(MacroSubstitution::substitute(braced, QHash<QString, QString>(),
						    nullptr, &orphans)
		      == braced);
		CHECK(orphans.isEmpty());
	}

	SECTION("um macro sem desenho nenhum recusa dizendo isso")
	{
		QDomElement nothing;
		const MacroSubstitution::Result result =
			MacroSubstitution::apply(nothing, QHash<QString, QString>());
		CHECK_FALSE(result.ok);
		CHECK_FALSE(result.errorText().isEmpty());
	}
}
