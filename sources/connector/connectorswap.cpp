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
#include "connectorswap.h"

#include "../autoNum/renumberplan.h"

/**
	@brief ConnectorSwap::Pin::Pin
	@param pin_id
	@param connector_name
	@param way_label
*/
ConnectorSwap::Pin::Pin(const QString &pin_id, const QString &connector_name,
			const QString &way_label) :
	id(pin_id),
	connector(connector_name),
	label(way_label)
{}

/**
	@brief ConnectorSwap::Pin::operator==
	@param other
	@return true when the two are the same reading
*/
bool ConnectorSwap::Pin::operator==(const Pin &other) const
{
	return id == other.id
			&& connector == other.connector
			&& label == other.label;
}

/**
	@brief ConnectorSwap::Pin::operator!=
	@param other
	@return the opposite of operator==
*/
bool ConnectorSwap::Pin::operator!=(const Pin &other) const
{
	return !(*this == other);
}

/**
	@brief ConnectorSwap::Plan::isValid
	@return true when two labels are to be written
*/
bool ConnectorSwap::Plan::isValid() const
{
	return refusal == Refusal::None;
}

/**
	@brief ConnectorSwap::Plan::describe
	@return why nothing is to be written, in one sentence
*/
QString ConnectorSwap::Plan::describe() const
{
	return describe(refusal);
}

/**
	@brief ConnectorSwap::Plan::describe
	@param refusal
	@return why @a refusal writes nothing, in one sentence
*/
QString ConnectorSwap::Plan::describe(Refusal refusal)
{
	switch (refusal)
	{
		case Refusal::None:
			return QString();
		case Refusal::NoSuchPin:
			return tr("aucune broche à cette place");
		case Refusal::SamePin:
			return tr("une broche ne s'échange pas avec elle-même");
		case Refusal::NoConnector:
			return tr("une des deux broches n'appartient à aucun connecteur");
		case Refusal::OtherConnector:
				//Said in full, because refusing looks arbitrary until
				//the reason is read: the move exists next door under
				//another name, and doing it here would put one number
				//on two ways.
			return tr("les deux broches sont de connecteurs différents : "
				  "déplacer une broche vers un autre connecteur est "
				  "une autre opération");
		case Refusal::SameLabel:
				//"numéro de voie" and not "repère", which this
				//program already spends on the tag of a component:
				//one word for two things is how a glossary starts
				//lying.
			return tr("les deux broches portent déjà le même numéro de voie");
	}
	return QString();
}

/**
	@brief ConnectorSwap::plan
	@param pins
	@param first_index
	@param second_index
	@return what would be written, or why nothing would be
*/
ConnectorSwap::Plan ConnectorSwap::plan(const QList<Pin> &pins,
					int first_index, int second_index)
{
	Plan answer;
	answer.first_index = first_index;
	answer.second_index = second_index;

	if (first_index < 0 || first_index >= pins.size()
			|| second_index < 0 || second_index >= pins.size())
	{
		answer.refusal = Refusal::NoSuchPin;
		return answer;
	}

	const Pin &first = pins.at(first_index);
	const Pin &second = pins.at(second_index);

		//By place and by identifier both. The second is what answers a
		//caller holding one pin twice - the project side builds a list of
		//two out of the two components it was given, and the two places
		//are then 0 and 1 whether or not the components are one. Without
		//it, that caller would be told the labels are the same, which is
		//true and is not what it asked.
	if (first_index == second_index
			|| (!first.id.isEmpty() && first.id == second.id))
	{
		answer.refusal = Refusal::SamePin;
		return answer;
	}

		//The key of step 3, and not a comparison of the raw field: two
		//pins of one connector written "CN1" and "cn1" are of one
		//connector, and refusing to swap them would be this rule
		//disagreeing with the renumbering about how many connectors the
		//project has.
	const QString first_key = Renumberer::connectorKey(first.connector);
	const QString second_key = Renumberer::connectorKey(second.connector);

	if (first_key.isEmpty() || second_key.isEmpty())
	{
		answer.refusal = Refusal::NoConnector;
		return answer;
	}

	if (first_key != second_key)
	{
		answer.refusal = Refusal::OtherConnector;
		return answer;
	}

		//Compared exactly, like everywhere a pin label is read: "9" and
		//"9 " are two labels, and exchanging them is a change the folio
		//shows.
	if (first.label == second.label)
	{
		answer.refusal = Refusal::SameLabel;
		return answer;
	}

	answer.first_label = second.label;
	answer.second_label = first.label;
	answer.refusal = Refusal::None;
	return answer;
}

/**
	@brief ConnectorSwap::applied
	@param pins
	@param swap
	@return @a pins with the two labels exchanged
*/
QList<ConnectorSwap::Pin> ConnectorSwap::applied(const QList<Pin> &pins,
						 const Plan &swap)
{
	if (!swap.isValid()) {
		return pins;
	}
	if (swap.first_index < 0 || swap.first_index >= pins.size()
			|| swap.second_index < 0 || swap.second_index >= pins.size()) {
		return pins;
	}

		//A copy written in place at two indexes, and never a removal
		//followed by an insertion: the second would move every pin after
		//them by one and give back a list that reads right and is not the
		//list that was handed in.
	QList<Pin> after = pins;
	after[swap.first_index].label = swap.first_label;
	after[swap.second_index].label = swap.second_label;
	return after;
}
