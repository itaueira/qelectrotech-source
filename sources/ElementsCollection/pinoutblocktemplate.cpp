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
#include "pinoutblocktemplate.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QDomElement>
#include <QSet>
#include <QSettings>

namespace
{
	const char *SETTINGS_KEY = "pinout/convention";

	const QLatin1String CONVENTION_TAG("pinout-convention");
	const QLatin1String TEMPLATE_TAG("block-template");
	const QLatin1String SIDE_TAG("side");
	const QLatin1String ROLE_ATTR("role");
	const QLatin1String ORIENTATION_ATTR("orientation");
	const QLatin1String REASON_ATTR("reason");

		//The four defaults, in one place, so that isNull() and the field
		//initialisers cannot drift apart.
	const int DEFAULT_WIDTH = 6;
	const int DEFAULT_PITCH = 1;
	const int DEFAULT_MARGIN = 1;
	const int DEFAULT_MAX_TERMINALS = 0;
}

/**
	@brief PinoutConvention::declares
	@param role
	@return true when the convention has an opinion about @a role
*/
bool PinoutConvention::declares(CatalogPinRole role) const
{
	return sides.contains(role);
}

/**
	@brief PinoutConvention::sideOf
	@param role
	@return which way a terminal of @a role points

	A role nobody placed goes to the left. That is a deliberate choice and
	not a fallback nobody thought about: the left edge is where a reader
	looks for what does not belong to the field wiring, so a role the
	convention forgot is visible rather than mixed in with the points.
*/
Qet::Orientation PinoutConvention::sideOf(CatalogPinRole role) const
{
	return sides.value(role, Qet::West);
}

/**
	@brief PinoutConvention::isValid
	@param error : when not nullptr, receives the reason on failure
	@return true when the convention can place every role it will meet
*/
bool PinoutConvention::isValid(QString *error) const
{
	if (key.trimmed().isEmpty())
	{
		if (error) {
			*error = QCoreApplication::translate("PinoutConvention",
					"Une convention doit avoir un nom.");
		}
		return false;
	}

	const QList<CatalogPinRole> roles = conventionalRoles();
	for (const CatalogPinRole role : roles)
	{
		if (!sides.contains(role))
		{
			if (error) {
				*error = QCoreApplication::translate("PinoutConvention",
						"La convention ne dit pas de quel "
						"côté va une borne "
						"« %1 ».")
					 .arg(CatalogPin::translatedRoleName(role));
			}
			return false;
		}
	}
	return true;
}

/**
	@brief PinoutConvention::toXml
	@return the convention, serialised
*/
QString PinoutConvention::toXml() const
{
	QDomDocument document;
	QDomElement root = document.createElement(CONVENTION_TAG);
	root.setAttribute(QStringLiteral("key"), key);

	for (auto it = sides.constBegin() ; it != sides.constEnd() ; ++it)
	{
		QDomElement side = document.createElement(SIDE_TAG);
		side.setAttribute(ROLE_ATTR, CatalogPin::roleToString(it.key()));
		side.setAttribute(ORIENTATION_ATTR,
				  SymbolTerminal::orientationToString(it.value()));
		root.appendChild(side);
	}

	document.appendChild(root);
	return document.toString();
}

/**
	@brief PinoutConvention::fromXml
	@param xml
	@return the convention @a xml describes, CEI when it describes nothing

	Roles are read by name and never by number, so a convention written by
	a build that knew fewer roles still reads here, and a role this build
	does not know is dropped rather than silently placed somewhere.
*/
PinoutConvention PinoutConvention::fromXml(const QString &xml)
{
	QDomDocument document;
	if (!document.setContent(xml)) {
		return iec();
	}

	const QDomElement root = document.documentElement();
	if (root.tagName() != CONVENTION_TAG) {
		return iec();
	}

	PinoutConvention convention;
	convention.key = root.attribute(QStringLiteral("key"),
					QStringLiteral("custom"));

	for (QDomElement side = root.firstChildElement(SIDE_TAG) ;
	     !side.isNull() ;
	     side = side.nextSiblingElement(SIDE_TAG))
	{
		const CatalogPinRole role =
			CatalogPin::roleFromString(side.attribute(ROLE_ATTR));
		if (role == CatalogPinRole::Unknown) {
			continue;
		}
		convention.sides.insert(role,
			SymbolTerminal::orientationFromString(
				side.attribute(ORIENTATION_ATTR)));
	}
	return convention;
}

/**
	@brief PinoutConvention::iec
	@return inputs on top, outputs at the bottom

	The five roles of a control symbol - a plain terminal, a coil, the three
	kinds of contact - are placed on top like an input, and not because a
	coil has an input: a pair of terminals is drawn along the flow of the
	power, first one on top and its partner opposite, and the generator asks
	opposite() for the second one. So what the convention has to say about a
	pair is where its first terminal goes.
*/
PinoutConvention PinoutConvention::iec()
{
	PinoutConvention convention;
	convention.key = QStringLiteral("iec");
	convention.sides.insert(CatalogPinRole::Terminal, Qet::North);
	convention.sides.insert(CatalogPinRole::Coil, Qet::North);
	convention.sides.insert(CatalogPinRole::ContactNo, Qet::North);
	convention.sides.insert(CatalogPinRole::ContactNc, Qet::North);
	convention.sides.insert(CatalogPinRole::PowerContactNo, Qet::North);
	convention.sides.insert(CatalogPinRole::Input, Qet::North);
	convention.sides.insert(CatalogPinRole::InputAnalog, Qet::North);
	convention.sides.insert(CatalogPinRole::SupplyCommon, Qet::North);
	convention.sides.insert(CatalogPinRole::Output, Qet::South);
	convention.sides.insert(CatalogPinRole::OutputAnalog, Qet::South);
	convention.sides.insert(CatalogPinRole::OutputRelay, Qet::South);
	convention.sides.insert(CatalogPinRole::ReturnCommon, Qet::South);
		//A communication port is not a point of the field. It goes to the
		//side so that it does not take a row away from the points, which
		//is where the reader counts.
	convention.sides.insert(CatalogPinRole::CommPort, Qet::West);
	return convention;
}

/**
	@brief PinoutConvention::horizontal
	@return inputs on the right, outputs on the left
*/
PinoutConvention PinoutConvention::horizontal()
{
	PinoutConvention convention = iec();
	convention.key = QStringLiteral("horizontal");
	for (auto it = convention.sides.begin() ; it != convention.sides.end() ; ++it)
	{
		if (it.value() == Qet::North) {
			it.value() = Qet::East;
		} else if (it.value() == Qet::South) {
			it.value() = Qet::West;
		}
	}
		//Left and right are taken, so the port moves to the bottom.
	convention.sides.insert(CatalogPinRole::CommPort, Qet::South);
	return convention;
}

/**
	@brief PinoutConvention::builtinConventions
	@return the two the specification names, ready to use
*/
QList<PinoutConvention> PinoutConvention::builtinConventions()
{
	return { iec(), horizontal() };
}

/**
	@brief PinoutConvention::translatedName
	@param key
	@return the name to show for @a key

	Worked out from the key rather than stored beside it, so that changing
	the language changes the label of a convention that was chosen years
	ago.
*/
QString PinoutConvention::translatedName(const QString &key)
{
	if (key == QLatin1String("iec")) {
		return QCoreApplication::translate("PinoutConvention",
				"CEI : entrées en haut, sorties en bas");
	}
	if (key == QLatin1String("horizontal")) {
		return QCoreApplication::translate("PinoutConvention",
				"Entrées à droite, sorties à gauche");
	}
	return QCoreApplication::translate("PinoutConvention", "Personnalisée");
}

/**
	@brief PinoutConvention::conventionalRoles
	@return every role a convention has to place
*/
QList<CatalogPinRole> PinoutConvention::conventionalRoles()
{
	QList<CatalogPinRole> roles = CatalogPin::allRoles();
	roles.removeAll(CatalogPinRole::Unknown);
	return roles;
}

/**
	@brief PinoutConvention::opposite
	@param side
	@return the side across the block from @a side
*/
Qet::Orientation PinoutConvention::opposite(Qet::Orientation side)
{
	switch (side) {
		case Qet::North: return Qet::South;
		case Qet::South: return Qet::North;
		case Qet::East:  return Qet::West;
		case Qet::West:  return Qet::East;
	}
	return Qet::South;
}

/**
	@brief PinoutConvention::current
	@return the convention of the environment, CEI when none was chosen
*/
PinoutConvention PinoutConvention::current()
{
	QSettings settings;
	const QString stored = settings.value(QLatin1String(SETTINGS_KEY)).toString();
	if (stored.isEmpty()) {
		return iec();
	}
	return fromXml(stored);
}

/**
	@brief PinoutConvention::isConfigured
	@return true when a convention was chosen instead of taking CEI
*/
bool PinoutConvention::isConfigured()
{
	QSettings settings;
	return !settings.value(QLatin1String(SETTINGS_KEY)).toString().isEmpty();
}

/**
	@brief PinoutConvention::setCurrent
	@param convention
*/
void PinoutConvention::setCurrent(const PinoutConvention &convention)
{
	QSettings settings;
	settings.setValue(QLatin1String(SETTINGS_KEY), convention.toXml());
}

/**
	@brief PinoutConvention::clearCurrent
	Go back to CEI.
*/
void PinoutConvention::clearCurrent()
{
	QSettings settings;
	settings.remove(QLatin1String(SETTINGS_KEY));
}

/**
	@brief PinoutBlockTemplate::isNull
	@return true when the class declared nothing of its own
*/
bool PinoutBlockTemplate::isNull() const
{
	return width_steps == DEFAULT_WIDTH
	       && pitch_steps == DEFAULT_PITCH
	       && margin_steps == DEFAULT_MARGIN
	       && max_terminals == DEFAULT_MAX_TERMINALS
	       && side_overrides.isEmpty();
}

/**
	@brief PinoutBlockTemplate::isValid
	@param error : when not nullptr, receives the reason on failure
	@return true when the template describes a block that can be drawn

	The last check is the one that carries the decision: a class may
	contradict the convention on a kind of terminal, not on every kind at
	once. A template that places all of them is a second convention, and two
	conventions in a company means nobody knows which one is the company's.
*/
bool PinoutBlockTemplate::isValid(QString *error) const
{
	if (width_steps < 2)
	{
		if (error) {
			*error = QCoreApplication::translate("PinoutBlockTemplate",
					"La largeur du bloc doit valoir au moins "
					"deux pas de grille.");
		}
		return false;
	}
	if (pitch_steps < 1)
	{
		if (error) {
			*error = QCoreApplication::translate("PinoutBlockTemplate",
					"L'espacement entre bornes doit valoir au "
					"moins un pas de grille, sinon deux bornes "
					"se posent au même endroit.");
		}
		return false;
	}
	if (margin_steps < 0 || max_terminals < 0)
	{
		if (error) {
			*error = QCoreApplication::translate("PinoutBlockTemplate",
					"La marge et le nombre maximum de bornes "
					"ne peuvent pas être négatifs.");
		}
		return false;
	}

	QSet<int> seen;
	for (const PinoutSideOverride &override_ : side_overrides)
	{
		if (override_.role == CatalogPinRole::Unknown)
		{
			if (error) {
				*error = QCoreApplication::translate("PinoutBlockTemplate",
						"Une exception doit dire de quel type "
						"de borne elle parle.");
			}
			return false;
		}
		if (seen.contains(static_cast<int>(override_.role)))
		{
			if (error) {
				*error = QCoreApplication::translate("PinoutBlockTemplate",
						"Le type « %1 » a deux "
						"exceptions.")
					 .arg(CatalogPin::translatedRoleName(override_.role));
			}
			return false;
		}
		seen.insert(static_cast<int>(override_.role));

		if (override_.reason.trimmed().isEmpty())
		{
			if (error) {
				*error = QCoreApplication::translate("PinoutBlockTemplate",
						"L'exception sur « %1 » "
						"doit dire pourquoi la classe "
						"contredit la convention.")
					 .arg(CatalogPin::translatedRoleName(override_.role));
			}
			return false;
		}
	}

	if (side_overrides.size() >= PinoutConvention::conventionalRoles().size())
	{
		if (error) {
			*error = QCoreApplication::translate("PinoutBlockTemplate",
					"Une classe peut contredire la convention "
					"sur un type de borne, pas sur tous : ceci "
					"est une seconde convention, pas une "
					"exception.");
		}
		return false;
	}
	return true;
}

/**
	@brief PinoutBlockTemplate::overridesSide
	@param role
	@return true when the class contradicts the convention about @a role
*/
bool PinoutBlockTemplate::overridesSide(CatalogPinRole role) const
{
	for (const PinoutSideOverride &override_ : side_overrides)
	{
		if (override_.role == role) {
			return true;
		}
	}
	return false;
}

/**
	@brief PinoutBlockTemplate::setOverride
	@param role
	@param side
	@param reason : why the class contradicts the convention

	Replaces the exception on @a role when there is already one, so that
	pressing the button twice cannot build the pair isValid() refuses.
*/
void PinoutBlockTemplate::setOverride(CatalogPinRole role,
				      Qet::Orientation side,
				      const QString &reason)
{
	for (int i = 0 ; i < side_overrides.size() ; ++i)
	{
		if (side_overrides.at(i).role == role)
		{
			side_overrides[i].side = side;
			side_overrides[i].reason = reason;
			return;
		}
	}
	side_overrides.append(PinoutSideOverride(role, side, reason));
}

/**
	@brief PinoutBlockTemplate::clearOverride
	@param role : goes back to the convention
*/
void PinoutBlockTemplate::clearOverride(CatalogPinRole role)
{
	for (int i = side_overrides.size() - 1 ; i >= 0 ; --i)
	{
		if (side_overrides.at(i).role == role) {
			side_overrides.removeAt(i);
		}
	}
}

/**
	@brief PinoutBlockTemplate::sideOf
	@param role
	@param convention
	@return the exception of the class, or the convention of the house
*/
Qet::Orientation PinoutBlockTemplate::sideOf(CatalogPinRole role,
					     const PinoutConvention &convention) const
{
	for (const PinoutSideOverride &override_ : side_overrides)
	{
		if (override_.role == role) {
			return override_.side;
		}
	}
	return convention.sideOf(role);
}

/**
	@brief PinoutBlockTemplate::width
	@param grid
	@return the width of the block, in the units of the drawing
*/
qreal PinoutBlockTemplate::width(const SymbolGrid &grid) const
{
	return width_steps * grid.main_step;
}

/**
	@brief PinoutBlockTemplate::pitch
	@param grid
	@return the distance between two consecutive terminals
*/
qreal PinoutBlockTemplate::pitch(const SymbolGrid &grid) const
{
	return pitch_steps * grid.main_step;
}

/**
	@brief PinoutBlockTemplate::margin
	@param grid
	@return the empty space before the first terminal of a side
*/
qreal PinoutBlockTemplate::margin(const SymbolGrid &grid) const
{
	return margin_steps * grid.main_step;
}

/**
	@brief PinoutBlockTemplate::offsetOf
	@param index : which terminal of the side, counted from zero
	@param grid
	@return how far along the side that terminal sits
*/
qreal PinoutBlockTemplate::offsetOf(int index, const SymbolGrid &grid) const
{
	if (index < 0) {
		return margin(grid);
	}
	return margin(grid) + index * pitch(grid);
}

/**
	@brief PinoutBlockTemplate::lengthFor
	@param count : how many terminals the side carries
	@param grid
	@return the length that side needs, margin included at both ends
*/
qreal PinoutBlockTemplate::lengthFor(int count, const SymbolGrid &grid) const
{
	if (count <= 0) {
		return 0.0;
	}
	return 2 * margin(grid) + (count - 1) * pitch(grid);
}

/**
	@brief PinoutBlockTemplate::blocksFor
	@param count : how many terminals the pinout has
	@return how many blocks they are split over

	This is what turns a card of 32 points into two blocks on two folios
	instead of one block taller than the sheet. Zero as a maximum means the
	class never wanted the pinout split, so everything goes in one block.
*/
int PinoutBlockTemplate::blocksFor(int count) const
{
	if (count <= 0) {
		return 0;
	}
	if (max_terminals <= 0) {
		return 1;
	}
	return (count + max_terminals - 1) / max_terminals;
}

/**
	@brief PinoutBlockTemplate::toXml
	@return the template, serialised for the column of the class
*/
QString PinoutBlockTemplate::toXml() const
{
	QDomDocument document;
	QDomElement root = document.createElement(TEMPLATE_TAG);
	root.setAttribute(QStringLiteral("width"), QString::number(width_steps));
	root.setAttribute(QStringLiteral("pitch"), QString::number(pitch_steps));
	root.setAttribute(QStringLiteral("margin"), QString::number(margin_steps));
	root.setAttribute(QStringLiteral("max-terminals"),
			  QString::number(max_terminals));

	for (const PinoutSideOverride &override_ : side_overrides)
	{
		QDomElement side = document.createElement(SIDE_TAG);
		side.setAttribute(ROLE_ATTR, CatalogPin::roleToString(override_.role));
		side.setAttribute(ORIENTATION_ATTR,
				  SymbolTerminal::orientationToString(override_.side));
		side.setAttribute(REASON_ATTR, override_.reason);
		root.appendChild(side);
	}

	document.appendChild(root);
	return document.toString();
}

/**
	@brief PinoutBlockTemplate::fromXml
	@param xml
	@return the template @a xml describes

	An empty column, or one this build cannot read, gives the template with
	nothing declared - which draws by the convention of the environment, and
	is what a class that never had an opinion has always done.
*/
PinoutBlockTemplate PinoutBlockTemplate::fromXml(const QString &xml)
{
	PinoutBlockTemplate model;
	QDomDocument document;
	if (!document.setContent(xml)) {
		return model;
	}

	const QDomElement root = document.documentElement();
	if (root.tagName() != TEMPLATE_TAG) {
		return model;
	}

	bool ok = false;
	const int width = root.attribute(QStringLiteral("width")).toInt(&ok);
	if (ok) { model.width_steps = width; }
	const int pitch = root.attribute(QStringLiteral("pitch")).toInt(&ok);
	if (ok) { model.pitch_steps = pitch; }
	const int margin = root.attribute(QStringLiteral("margin")).toInt(&ok);
	if (ok) { model.margin_steps = margin; }
	const int maximum = root.attribute(QStringLiteral("max-terminals")).toInt(&ok);
	if (ok) { model.max_terminals = maximum; }

	for (QDomElement side = root.firstChildElement(SIDE_TAG) ;
	     !side.isNull() ;
	     side = side.nextSiblingElement(SIDE_TAG))
	{
		const CatalogPinRole role =
			CatalogPin::roleFromString(side.attribute(ROLE_ATTR));
		if (role == CatalogPinRole::Unknown) {
			continue;
		}
		model.setOverride(role,
				  SymbolTerminal::orientationFromString(
					  side.attribute(ORIENTATION_ATTR)),
				  side.attribute(REASON_ATTR));
	}
	return model;
}
