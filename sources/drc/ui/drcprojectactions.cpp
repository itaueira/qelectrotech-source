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
#include "drcprojectactions.h"

#include "drcpanel.h"

#include "../../dataBase/drcsqlengine.h"
#include "../drclinkrules.h"
#include "../drcruleset.h"
#include "../drcscanengine.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
	/**
		The object name the report window is found again by.

		A function and not a literal, and passed as a QString rather than
		as a QLatin1String: setObjectName() and findChildren() gained a
		QAnyStringView overload in Qt 6.7, and a QLatin1String is one user
		conversion away from either of them. A QString matches the older
		overload exactly, which keeps the same line compiling against both
		Qt versions.
	*/
	QString reportObjectName()
	{
		return QStringLiteral("drc_report_dialog");
	}

	/**
		The property the report window remembers its project under.

		Without it, a second project checked while the report of the first
		is still up would have its findings poured into that window - and
		nothing on the screen would say the list had changed subject. The
		pointer is only ever compared, never followed.
	*/
	const char *reportProjectProperty()
	{
		return "drc_project";
	}

	/**
		A row of a query turned into something the panel can list.

		It is listed and it does **not** navigate, and that is a stated
		gap rather than an oversight. Which column of a result set holds
		the uuid of the offending component is a decision the first query
		rule makes - DrcSqlResult keeps the rows exactly as they came for
		that reason - and guessing it here would mean picking a column by
		name. There are two columns called pos in this data base, holding
		two different things, so a guess would be wrong in a way nobody
		would notice.

		Until that decision is made, a row is shown with what the query
		selected written out beside the rule that asked. Listing it
		without a way to click it is worse than clicking; swallowing it
		would be worse than both, because the panel would then report a
		clean project while a rule was finding things.
	*/
	QString rowText(const DrcSqlResult &result, int row)
	{
		const QStringList columns = result.columns();
		QStringList pairs;
		for (const QString &column : columns)
		{
			const QString value = result.value(row, column);
			pairs << QStringLiteral("%1=%2").arg(column, value);
		}
		return pairs.join(QStringLiteral(", "));
	}
}

/**
	@brief DrcProjectActions::CheckOutcome::isClean
	@return whether the project may be reported as having nothing wrong
*/
bool DrcProjectActions::CheckOutcome::isClean() const
{
	return findings.isEmpty() && failures.isEmpty() && ruleCount() > 0;
}

/**
	@brief DrcProjectActions::CheckOutcome::summary
	@return the sentence the panel shows above the table
*/
QString DrcProjectActions::CheckOutcome::summary() const
{
	if (ruleCount() == 0)
	{
		return QCoreApplication::translate(
			"DrcProjectActions",
			"Aucune règle activée : rien n'a été vérifié. "
			"Un projet sans constat n'est pas pour autant un projet "
			"sans défaut.");
	}

	const QString timing = QCoreApplication::translate(
				"DrcProjectActions",
				"%1 règle(s) exécutée(s) : parcours %2 ms, "
				"requêtes %3 ms.")
			       .arg(ruleCount()).arg(scan_msec).arg(sql_msec);

	QString head;
	if (!failures.isEmpty())
	{
		head = QCoreApplication::translate(
			       "DrcProjectActions",
			       "%1 règle(s) n'ont pas pu être exécutées : "
			       "leur résultat est inconnu, et non vide.")
		       .arg(failures.count());
	}
	else if (findings.isEmpty())
	{
		head = QCoreApplication::translate(
			"DrcProjectActions",
			"Aucun constat.");
	}

	if (!findings.isEmpty())
	{
		const int errors = DrcFinding::countAtLeast(findings,
							    DrcSeverity::Error);
		const QString found = QCoreApplication::translate(
					      "DrcProjectActions",
					      "%1 constat(s), dont %2 erreur(s). "
					      "Double-cliquez une ligne, ou "
					      "appuyez sur Entrée, pour aller au "
					      "constat.")
				      .arg(findings.count()).arg(errors);
		head = head.isEmpty()
		       ? found
		       : QStringLiteral("%1 %2").arg(head, found);
	}

	return QStringLiteral("%1 %2").arg(head, timing);
}

/**
	@brief DrcProjectActions::buildFactoryCheck
	@param rules : the register to fill
	@param scan : the engine of the walk route
	@param sql : the engine of the query route
	@return how many rules were registered
*/
int DrcProjectActions::buildFactoryCheck(DrcRuleSet &rules,
					 DrcScanEngine &scan,
					 DrcSqlEngine &sql)
{
	Q_UNUSED(sql)

		//The query route ships no rule yet: the first ones of that route
		//are written against the tables of the project data base, and they
		//register themselves here the same way, next to this line.
	return DrcLinkRules::registerRules(rules, scan);
}

/**
	@brief DrcProjectActions::runCheck
	@param project : the project to check
	@return what the factory profile found
*/
DrcProjectActions::CheckOutcome DrcProjectActions::runCheck(
		QETProject *project)
{
	DrcRuleSet rules;
	DrcScanEngine scan;
	DrcSqlEngine sql;
	buildFactoryCheck(rules, scan, sql);

	return runCheck(project, rules, scan, sql);
}

/**
	@brief DrcProjectActions::runCheck
	@param project : the project to check
	@param rules : the register, with the current severity of each rule
	@param scan : the engine of the walk route
	@param sql : the engine of the query route
	@return what the two routes found, with the time of each one apart
*/
DrcProjectActions::CheckOutcome DrcProjectActions::runCheck(
		QETProject *project,
		const DrcRuleSet &rules,
		const DrcScanEngine &scan,
		const DrcSqlEngine &sql)
{
	CheckOutcome outcome;
	if (!project) {
		return outcome;
	}

		//The walk first, because it is the one that has rules today and
		//because it does not touch the data base: timing it after an
		//updateDB() would time a machine that had just been made busy.
	QElapsedTimer timer;
	timer.start();
	const DrcScanReport scan_report = scan.run(project, rules);
	outcome.scan_msec = timer.elapsed();
	outcome.scan_rules_run = scan_report.rulesRun();
	outcome.findings = scan_report.findings();

	timer.restart();
	const DrcSqlReport sql_report = sql.run(project, rules);
	outcome.sql_msec = timer.elapsed();
	outcome.sql_rules_run = sql_report.ruleCount();
	outcome.failures = sql_report.failures();

	const QList<DrcSqlResult> results = sql_report.results();
	for (const DrcSqlResult &result : results)
	{
		if (!result.hasFindings()) {
			continue;
		}

		const DrcRule rule = rules.rule(result.identifier());
		for (int row = 0 ; row < result.findingCount() ; ++row)
		{
			const QString text =
					rule.description().isEmpty()
					? rowText(result, row)
					: QStringLiteral("%1 : %2")
					  .arg(rule.description(),
					       rowText(result, row));

			outcome.findings.append(
				DrcFinding::onProject(result.identifier(),
						      result.severity(),
						      text));
		}
	}

	return outcome;
}

/**
	@brief DrcProjectActions::showCheckReport
	@param project : the project to check
	@param parent : the window the report belongs to
*/
void DrcProjectActions::showCheckReport(QETProject *project, QWidget *parent)
{
	if (!project) {
		return;
	}

	const CheckOutcome outcome = runCheck(project);

		//A report already open on this project is refilled rather than
		//covered by a second one. Two windows showing two runs of the same
		//check, with no way of telling which is which, is a worse answer
		//than one that is up to date.
	if (parent)
	{
		const QList<QDialog *> opened =
				parent->findChildren<QDialog *>(
					reportObjectName());
		for (QDialog *dialog : opened)
		{
			if (dialog->property(reportProjectProperty())
			    .value<quintptr>()
			    != reinterpret_cast<quintptr>(project)) {
				continue;
			}

			DrcPanel *panel = dialog->findChild<DrcPanel *>();
			if (!panel) {
				continue;
			}
			panel->setFindings(outcome.findings);
			panel->setSummary(outcome.summary());
			dialog->show();
			dialog->raise();
			dialog->activateWindow();
			return;
		}
	}

	QDialog *dialog = new QDialog(parent);
	dialog->setObjectName(reportObjectName());
		//Which project this list is about, so that checking a second one
		//opens a second window instead of quietly replacing the contents
		//of the first.
	dialog->setProperty(reportProjectProperty(),
			    QVariant::fromValue(
				    reinterpret_cast<quintptr>(project)));
	dialog->setWindowTitle(QCoreApplication::translate(
				       "DrcProjectActions",
				       "Contrôle du projet"));
		//Not modal, and it is the panel that requires it: the reader has
		//to work down the list while the folio behind it stays usable. A
		//modal window would leave the list up and the drawing frozen,
		//which is the worst of the two.
	dialog->setModal(false);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->resize(760, 460);

	DrcPanel *panel = new DrcPanel(dialog);
	panel->setFindings(outcome.findings);
	panel->setSummary(outcome.summary());

	QDialogButtonBox *buttons =
			new QDialogButtonBox(QDialogButtonBox::Close, dialog);
	QObject::connect(buttons, &QDialogButtonBox::rejected,
			 dialog, &QDialog::close);

	QVBoxLayout *layout = new QVBoxLayout(dialog);
	layout->addWidget(panel);
	layout->addWidget(buttons);

	dialog->show();
}
