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
#include "iocircuit.h"

#include "../macro/circuittable.h"
#include "ioassignment.h"

#include <QFileInfo>
#include <QStringList>

/// how many macro names the paragraph says one by one before summing up
static const int MAX_SAID_MACROS = 4;
/// how many refusals the paragraph says one by one before summing up
static const int MAX_SAID_REFUSED = 6;

/// what the "connect to" column has to end with
static const char MACRO_SUFFIX[] = ".qetmak";

/**
	@brief IoCircuit::Plan::macroPaths
	@return the macros this plan needs, each once, in the order they appear
*/
QStringList IoCircuit::Plan::macroPaths() const
{
	QStringList paths;
	for (const Job &job : jobs)
	{
		if (!paths.contains(job.macro_path)) {
			paths << job.macro_path;
		}
	}
	return paths;
}

/**
	@brief IoCircuit::Plan::terminals
	@return how many of the circuits carry a field terminal
*/
int IoCircuit::Plan::terminals() const
{
	int count = 0;
	for (const Job &job : jobs)
	{
		if (job.needs_terminal) {
			++count;
		}
	}
	return count;
}

/**
	@brief IoCircuit::Plan::text
	What is going to be drawn, and what is not, in one paragraph. A long
	list is summed up rather than shown whole: a paragraph nobody reads to
	the end is a paragraph that says nothing.
	@return the plan said out loud
*/
QString IoCircuit::Plan::text() const
{
	QStringList lines;

	if (jobs.isEmpty())
	{
		lines << tr("Aucun circuit ne sera généré.");
	}
	else
	{
		QStringList names;
		const QStringList paths = macroPaths();
		for (const QString &path : paths)
		{
			if (names.count() == MAX_SAID_MACROS)
			{
				names << QStringLiteral("…");
				break;
			}
			names << QFileInfo(path).completeBaseName();
		}

		lines << tr("%1 circuit(s) à générer, à partir de : %2.")
			 .arg(int(jobs.count()))
			 .arg(names.join(QStringLiteral(", ")));

		const int with_terminal = terminals();
		if (with_terminal > 0)
		{
			lines << tr("%1 borne(s) de champ à créer.")
				 .arg(with_terminal);
		}
	}

	if (!rejected.isEmpty())
	{
		lines << tr("%1 point(s) sans circuit :")
			 .arg(int(rejected.count()));

		int said = 0;
		for (const Rejected &one : rejected)
		{
			if (said == MAX_SAID_REFUSED)
			{
				lines << tr("… et %1 autre(s).")
					 .arg(int(rejected.count()) - said);
				break;
			}
			lines << QStringLiteral("  ")
				 + refusalText(one.reason, one.label);
			++said;
		}
	}

	return lines.join(QLatin1Char('\n'));
}

/**
	@brief IoCircuit::isMacroPath
	@param connect_to what the "connect to" column of the sheet said
	@return true when it names a macro file
*/
bool IoCircuit::isMacroPath(const QString &connect_to)
{
	return connect_to.trimmed().endsWith(QLatin1String(MACRO_SUFFIX),
					     Qt::CaseInsensitive);
}

/**
	@brief IoCircuit::valueKeys
	The six things a point knows that a circuit can want. A macro asks for
	one of them by declaring a variable with that name, or with one of the
	spellings aliasesOf() accepts.
	@return the keys, in the order a person would read them
*/
QStringList IoCircuit::valueKeys()
{
	return QStringList{QStringLiteral("MARCACAO"),
			   QStringLiteral("DESCRICAO"),
			   QStringLiteral("ENDERECO"),
			   QStringLiteral("CANAL"),
			   QStringLiteral("CARTAO"),
			   QStringLiteral("COMENTARIO")};
}

/**
	@brief IoCircuit::aliasesOf
	The fork writes its macros in Portuguese, the program is French, and a
	sheet that came from the automation is as likely to be English. All
	three spellings answer, because refusing one of them would only teach
	the person to rename the variable.
	@param key one of valueKeys()
	@return the spellings it answers to, @a key included, empty when @a key
	is not one of them
*/
QStringList IoCircuit::aliasesOf(const QString &key)
{
	const QString folded = IoPoint::normalize(key);

	if (folded == QLatin1String("marcacao"))
	{
		return QStringList{QStringLiteral("MARCACAO"),
				   QStringLiteral("REPERE"),
				   QStringLiteral("TAG"),
				   QStringLiteral("ETIQUETA"),
				   QStringLiteral("LABEL")};
	}
	if (folded == QLatin1String("descricao"))
	{
		return QStringList{QStringLiteral("DESCRICAO"),
				   QStringLiteral("DESCRIPTION"),
				   QStringLiteral("FUNCAO"),
				   QStringLiteral("FONCTION"),
				   QStringLiteral("FUNCTION")};
	}
	if (folded == QLatin1String("endereco"))
	{
		return QStringList{QStringLiteral("ENDERECO"),
				   QStringLiteral("ADRESSE"),
				   QStringLiteral("ADDRESS")};
	}
	if (folded == QLatin1String("canal"))
	{
		return QStringList{QStringLiteral("CANAL"),
				   QStringLiteral("VOIE"),
				   QStringLiteral("CHANNEL")};
	}
	if (folded == QLatin1String("cartao"))
	{
		return QStringList{QStringLiteral("CARTAO"),
				   QStringLiteral("CARTE"),
				   QStringLiteral("CARD")};
	}
	if (folded == QLatin1String("comentario"))
	{
		return QStringList{QStringLiteral("COMENTARIO"),
				   QStringLiteral("COMMENTAIRE"),
				   QStringLiteral("COMMENT")};
	}

	return QStringList();
}

/**
	@brief IoCircuit::keyForColumn
	The whole name has to match, and never a prefix: a library where
	MARCACAO_DISJUNTOR and MARCACAO_RELE both exist would get the same tag
	written into both, and a wrong tag on the folio costs more than an
	empty one.
	@param column the name the macro declared
	@return which of valueKeys() it is asking for, empty when the macro
	means something the point knows nothing about
*/
QString IoCircuit::keyForColumn(const QString &column)
{
	const QString folded = IoPoint::normalize(column);
	if (folded.isEmpty()) {
		return QString();
	}

	const QStringList keys = valueKeys();
	for (const QString &key : keys)
	{
		const QStringList aliases = aliasesOf(key);
		for (const QString &alias : aliases)
		{
			if (IoPoint::normalize(alias) == folded) {
				return key;
			}
		}
	}
	return QString();
}

/**
	@brief IoCircuit::valuesOf
	@param point
	@param card_label how the card names itself on the folio
	@return what @a point has to say, by key; a key it has nothing to say
	about is absent rather than empty, so that the macro keeps its own
	default instead of being blanked
*/
QHash<QString, QString> IoCircuit::valuesOf(const IoPoint &point,
					    const QString &card_label)
{
	QHash<QString, QString> values;

	values.insert(QStringLiteral("MARCACAO"), point.tag);
	values.insert(QStringLiteral("DESCRICAO"), point.description);
	values.insert(QStringLiteral("ENDERECO"), point.address);
	values.insert(QStringLiteral("CANAL"), point.channel);
	values.insert(QStringLiteral("COMENTARIO"), point.comment);

		//The card is asked how it names itself; the column the sheet
		//carried is what somebody wanted before the assignment, and it
		//only answers when the card says nothing.
	values.insert(QStringLiteral("CARTAO"),
		      card_label.trimmed().isEmpty() ? point.card : card_label);

	for (auto it = values.begin() ; it != values.end() ; )
	{
		if (it.value().trimmed().isEmpty()) {
			it = values.erase(it);
		} else {
			++it;
		}
	}

	return values;
}

/**
	@brief IoCircuit::plan
	@param list the list of the project
	@param point_ids the points asked for
	@param card_label how the card names itself
	@return the circuits, and the points that stay undrawn with the reason
*/
IoCircuit::Plan IoCircuit::plan(const IoList &list,
				const QStringList &point_ids,
				const QString &card_label)
{
	Plan plan;

	for (const QString &id : point_ids)
	{
		const int index = list.indexOfId(id);
		if (index < 0)
		{
			plan.rejected << Rejected(id, id, PointNotFound);
			continue;
		}

		const IoPoint point = list.at(index);
		const QString label = IoAssignment::pointLabel(point);

			//A circuit is drawn on a channel. Without the channel there
			//is nothing to connect it to, and a stretch of schematic
			//left hanging in the folio is worse than one not drawn.
		if (!point.isAssigned())
		{
			plan.rejected << Rejected(id, label, NotAssigned);
			continue;
		}
		if (point.connect_to.trimmed().isEmpty())
		{
			plan.rejected << Rejected(id, label, NothingToDraw);
			continue;
		}
		if (!isMacroPath(point.connect_to))
		{
			plan.rejected << Rejected(id, label, NotAMacro);
			continue;
		}

		Job job;
		job.point_id = point.id;
		job.label = label;
		job.macro_path = point.connect_to.trimmed();
		job.io_index = point.io_index;
		job.needs_terminal = point.needs_terminal;
		job.values = valuesOf(point, card_label);

		plan.jobs << job;
	}

	return plan;
}

/**
	@brief IoCircuit::fill
	@param table where the rows go
	@param plan its jobs come back with their row_id written in
	@param problems collects the cells the table would not take
	@return how many cells were written
*/
int IoCircuit::fill(CircuitTable &table, Plan &plan, QStringList *problems)
{
	QList<int> rows;
	rows.reserve(plan.jobs.count());

	for (Job &job : plan.jobs)
	{
		const int row = table.appendRow(job.macro_path);
		job.row_id = table.row(row).id;
		rows << row;
	}

		//The columns of a table are the union of what its macros declare,
		//so they are only all known once every row is in.
	const QStringList columns = table.columns();

	int written = 0;
	for (int i = 0 ; i < int(rows.count()) ; ++i)
	{
		const int row = rows.at(i);
		const Job &job = plan.jobs.at(i);

		for (const QString &column : columns)
		{
				//A column another row's macro declared and this one
				//does not is not this row's business.
			if (table.isInert(row, column)) {
				continue;
			}

			const QString key = keyForColumn(column);
			if (key.isEmpty() || !job.values.contains(key)) {
				continue;
			}

			QString error;
			if (table.setValue(row, column,
					   job.values.value(key), &error))
			{
				++written;
			}
			else if (problems && !error.isEmpty())
			{
				*problems << tr("%1 : %2")
					     .arg(job.label)
					     .arg(error);
			}
		}
	}

	return written;
}

/**
	@brief IoCircuit::refusalText
	@param reason
	@param label how the sheet names the point
	@return the reason said out loud
*/
QString IoCircuit::refusalText(Refusal reason, const QString &label)
{
	switch (reason)
	{
		case PointNotFound:
			return tr("%1 : ce point n'est plus dans la liste.")
			       .arg(label);
		case NotAssigned:
			return tr("%1 : ce point n'est dans aucune carte.")
			       .arg(label);
		case NothingToDraw:
			return tr("%1 : la colonne Connecter à est vide.")
			       .arg(label);
		case NotAMacro:
			return tr("%1 : ce qu'il faut connecter n'est pas un "
				  "fichier .qetmak.")
			       .arg(label);
		case NoRefusal:
			break;
	}
	return QString();
}
