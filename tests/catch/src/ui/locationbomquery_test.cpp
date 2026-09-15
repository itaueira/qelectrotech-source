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

#include "../../../../sources/dataBase/projectdatabase.h"
#include "../../../../sources/location/ui/locationbomdialog.h"
#include "../../../../sources/qetproject.h"

#include <catch2/catch.hpp>

#include <QComboBox>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

/*
	An apostrophe in the name of a location, and the block that came out
	empty.

	"Insérer dans le folio…" does not draw a table. It writes a query and
	hands it over, and the nomenclature block that lands on the folio
	keeps that query as text inside the project and runs it again every
	time the project is opened. So the query has to be a piece of SQL that
	survives being written down - which is exactly why the value cannot be
	bound and has to be escaped.

	Interpolated raw, a location named "Painel d'Água" closed the string
	literal in the middle of the filter. What was left did not parse, and
	nothing on that path reports a failure: the block arrived on the folio
	with no row in it. A person reading that folio concludes the enclosure
	is empty.

	@par The assertion that tells before from after
	Not the text of the query - a test that only compared strings would
	pass on any escaping that looks plausible. The query is executed,
	against the database of the project it was written from. Before the
	repair it does not execute at all; after it, it executes and answers
	the rows of that enclosure.

	@par And the two questions asked of the answer
	How many rows, which is cheap and catches a filter that fell off
	entirely - a query with no filter answers every component of the
	project and would otherwise look like a success. Then which rows,
	because a filter can be present, run, and still be wrong: a
	containment written one character too wide picks up the neighbouring
	enclosure, the count changes by one, and nothing says so. The labels
	are asserted as a set for that reason.
*/

namespace
{
	/// One component of the fixture, and where it was placed.
	struct Instance
	{
		const char *label;
		const char *designation;
		const char *location_path;
	};

	/// The path of the enclosure whose name carries an apostrophe.
	QString enclosure()
	{
		return QString::fromUtf8("Painel d'Água");
	}

	/// The path of the door inside it.
	QString door()
	{
		return enclosure() + QStringLiteral("/Porta");
	}

	/**
		The components the cases are drawn on.

		Three inside the enclosure, one inside the door of that
		enclosure, and one inside a second enclosure that has nothing to
		do with either. The last is the control: it is what a filter
		that ran but matched too much brings back, and without it a case
		counting rows would call that a success.
	*/
	const Instance instances[] = {
		{"K1", "Contator 9 A",        "Painel d'Água"},
		{"K2", "Contator 9 A",        "Painel d'Água"},
		{"Q1", "Disjuntor 3P 25 A",   "Painel d'Água"},
		{"S1", "Botão de emergência", "Painel d'Água/Porta"},
		{"K9", "Contator 9 A",        "Quadro B"}};

	/// @return the whole .qet, as text
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

		QString drawn;
		int x = 100;
		int uuid = 0;
		for (const Instance &instance : instances)
		{
			QString info;
			info += information(QStringLiteral("label"),
					    QString::fromUtf8(instance.label));
			info += information(QStringLiteral("designation"),
					    QString::fromUtf8(instance.designation));
			info += information(QStringLiteral("location_path"),
					    QString::fromUtf8(instance.location_path));

			++uuid;
			drawn += QStringLiteral(
					 "<element x=\"%1\" y=\"100\" z=\"10\" prefix=\"\""
					 " freezeLabel=\"false\" orientation=\"0\""
					 " type=\"embed://bench/part.elmt\""
					 " uuid=\"{cafe0000-0000-4000-8000-%2}\">"
					 "<terminals/><inputs/>"
					 "<elementInformations>%3</elementInformations>"
					 "<dynamic_texts/><texts_groups/>"
					 "</element>")
				 .arg(x)
				 .arg(uuid, 12, 10, QLatin1Char('0'))
				 .arg(info);
			x += 100;
		}

			//The tree the scope box is filled from. The enclosure
			//holds a door, so the scope of the enclosure is written
			//as a containment and the scope of the door as an
			//equality - the two branches of the filter, and both of
			//them carry the apostrophe.
		const QString tree = QStringLiteral(
			"<location_tree>"
			"<location uuid=\"{10000000-0000-4000-8000-000000000001}\""
			" code=\"%1\" name=\"Enclosure\"/>"
			"<location uuid=\"{10000000-0000-4000-8000-000000000002}\""
			" parent=\"{10000000-0000-4000-8000-000000000001}\""
			" code=\"Porta\" name=\"Door\"/>"
			"<location uuid=\"{10000000-0000-4000-8000-000000000003}\""
			" code=\"Quadro B\" name=\"Other\"/>"
			"</location_tree>").arg(enclosure().toHtmlEscaped());

		const QString definition = QStringLiteral(
			"<element name=\"part.elmt\">"
			"<definition type=\"element\" version=\"0.80\""
			" width=\"20\" height=\"40\""
			" hotspot_x=\"10\" hotspot_y=\"20\""
			" orientation=\"dnnn\" link_type=\"simple\">"
			"<names><name lang=\"en\">Part</name></names>"
			"<description>"
			"<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\""
			" end1=\"none\" end2=\"none\" length1=\"1.5\""
			" length2=\"1.5\" antialias=\"false\""
			" style=\"line-style:normal;line-weight:normal;"
			"filling:none;color:black\"/>"
			"<terminal x=\"0\" y=\"-10\" orientation=\"n\"/>"
			"<terminal x=\"0\" y=\"10\" orientation=\"s\"/>"
			"</description>"
			"</definition>"
			"</element>");

		return QStringLiteral(
			       "<project title=\"bench\" version=\"0.80\">"
			       "%1"
			       "<collection><category name=\"bench\">%2</category>"
			       "</collection>"
			       "<diagram title=\"Bench\" order=\"1\" height=\"500\""
			       " cols=\"25\" colsize=\"50\" rows=\"6\" rowsize=\"80\""
			       " displaycols=\"true\" displayrows=\"true\">"
			       "<elements>%3</elements>"
			       "<inputs/><conductors/>"
			       "</diagram>"
			       "</project>")
		       .arg(tree, definition, drawn);
	}

	/**
		@param dialog the open window
		@param path the location to put the scope box on
		@return true when the scope box had that location to offer

		The scope box carries the path of each location in a role of its
		own, which is how the window itself finds its way back to the
		scope a person was on. The same role is read here rather than
		matching the visible text, because the visible text is the
		IEC tag plus the name and neither of those is what the filter is
		written from.
	*/
	bool putScopeOn(LocationBomDialog *dialog, const QString &path)
	{
		QComboBox *scope = dialog->findChild<QComboBox *>();
		if (!scope) {
			return false;
		}

			//Qt::UserRole holds what kind of scope the entry is, and
			//the role above it holds the path. Written out because
			//the constant is private to the window.
		const int path_role = Qt::UserRole + 1;
		for (int i = 0 ; i < scope->count() ; ++i)
		{
			if (scope->itemData(i, path_role).toString() == path)
			{
				scope->setCurrentIndex(i);
				return true;
			}
		}
		return false;
	}

	/**
		@param project the open project
		@param path the scope to ask for
		@param query filled with what the window handed over
		@return true when the window emitted a query

		The button is not clicked and its text is not matched: the slot
		is called through the meta object, which is the same call the
		click makes and does not depend on a label that a catalogue may
		translate.
	*/
	bool queryForScope(QETProject *project, const QString &path,
			   QString *query)
	{
		LocationBomDialog dialog(project);

		if (!putScopeOn(&dialog, path)) {
			return false;
		}

		bool emitted = false;
		QObject::connect(&dialog, &LocationBomDialog::insertTable,
				 [&emitted, query](const QString &built)
		{
			emitted = true;
			*query = built;
		});

		QMetaObject::invokeMethod(&dialog, "insertOnFolio");
		return emitted;
	}

	/// @return the labels the query answers, sorted
	QStringList labelsOf(QETProject *project, const QString &query,
			     QString *error)
	{
		project->dataBase()->updateDB();
		QSqlQuery run = project->dataBase()->newQuery(query);
		if (!run.exec())
		{
			*error = run.lastError().text();
			return QStringList();
		}

		QStringList labels;
			//The label is the first column the window asks for.
		while (run.next()) {
			labels << run.value(0).toString();
		}
		labels.sort();
		return labels;
	}
}

TEST_CASE("T16 — apóstrofo no nome da localização não quebra a consulta do "
	  "bloco na folha", "[bom][location][sql]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("locationbom.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

	QString query;
	REQUIRE(queryForScope(scratch.project(), enclosure(), &query));
	INFO(query.toStdString());
	REQUIRE_FALSE(query.isEmpty());

		//The apostrophe arrives doubled, which is how SQL writes one
		//inside a literal. Asserted on the text as well as through the
		//execution below, so that a repair which only happens to work
		//on this database is still told apart from the rule.
	CHECK(query.contains(QString::fromUtf8("Painel d''Água")));

	QString error;
	const QStringList labels = labelsOf(scratch.project(), query, &error);
	INFO(error.toStdString());

		//The statement runs. Before the repair it does not: what was
		//written was not SQL, and the block on the folio was empty with
		//nothing said about it.
	REQUIRE_FALSE(labels.isEmpty());

		//How many, which catches a filter that fell off - without one
		//the query answers all five components of the project.
	CHECK(labels.count() == 4);

		//And which, because a containment one character too wide runs
		//and answers the wrong list. The component of the other
		//enclosure is the one that would come along.
	CHECK(labels == QStringList({QStringLiteral("K1"),
				     QStringLiteral("K2"),
				     QStringLiteral("Q1"),
				     QStringLiteral("S1")}));
}

TEST_CASE("T16 — o apóstrofo também é escapado no filtro de igualdade",
	  "[bom][location][sql]")
{
	UiBench::ScratchProject scratch(fixtureXml(),
					QStringLiteral("locationbomdoor.qet"));
	INFO(scratch.error().toStdString());
	REQUIRE(scratch.isOpen());

		//A location with nothing inside it is filtered by equality and
		//not by containment, which is the other of the two branches;
		//this one carries the apostrophe in the middle of its path.
	QString query;
	REQUIRE(queryForScope(scratch.project(), door(), &query));
	INFO(query.toStdString());
	REQUIRE_FALSE(query.isEmpty());

	CHECK(query.contains(QString::fromUtf8("location_path='Painel d''Água/Porta'")));

	QString error;
	const QStringList labels = labelsOf(scratch.project(), query, &error);
	INFO(error.toStdString());

	REQUIRE_FALSE(labels.isEmpty());
	CHECK(labels.count() == 1);
	CHECK(labels == QStringList({QStringLiteral("S1")}));
}
