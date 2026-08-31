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
#ifndef IOPOINT_H
#define IOPOINT_H

#include "../properties/elementdata.h"

#include <QFlags>
#include <QString>

class QDomDocument;
class QDomElement;

/**
	@brief One field of an I/O point, as a bit.

	An import does not always own every column it could write. The basic
	sheet of CU-11.1 has two columns and must not be able to blank the
	address someone typed by hand afterwards; a sheet that carries only
	the wiring must not be able to rewrite the description. The set of
	fields an import is allowed to touch is therefore handed in, and what
	is outside it is not written at all.
*/
enum IoField {
	NoIoField          = 0x00,
	IoTypeField        = 0x01,
	IoTagField         = 0x02,
	IoDescriptionField = 0x04,
	IoAddressField     = 0x08,
	IoCardField        = 0x10,
	IoConnectField     = 0x20,
	IoTerminalField    = 0x40,
	IoCommentField     = 0x80,
	AllIoFields        = 0xFF
};
Q_DECLARE_FLAGS(IoFields, IoField)
Q_DECLARE_OPERATORS_FOR_FLAGS(IoFields)

/**
	@brief One input or output of a PLC, before it is on any folio.

	QElectroTech already knows what an I/O point is: ElementData::PlcIO
	holds one, and PlcMasterData holds the ones a card carries. What it
	does not have is a place to keep a point that has not been drawn yet -
	and that is the whole of the sheet a person imports, ninety-six lines
	of them, sitting in the project while the designer decides which
	card each one goes into. IoPoint is that place.

	The type is the program's own PlcIOType and not a new enumeration, so
	that assigning a point to a channel is copying a field rather than
	translating between two vocabularies that would drift apart at the
	first sync. The serialisation, on the other hand, is this class's own:
	the automation sheet writes DI, DO, AI and AO, and the file must read
	the same way the sheet does.
*/
class IoPoint
{
	public:
		IoPoint();
		explicit IoPoint(const QString &description);

		bool isNull() const;

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);

		static QString tagName();

		bool operator==(const IoPoint &other) const;
		bool operator!=(const IoPoint &other) const;

			/// @return the two letters the sheet writes, DI DO AI AO UI UO
		static QString typeToString(ElementData::PlcIOType type);
			/**
				@brief Read a type the way a person wrote it.
				@param text what the cell holds
				@param ok set to false when nothing matched, EntreeDigitale returned
				Deliberately tolerant: the same column comes in as DI, as ED, as
				"entrada digital" and as "digital input" depending on who filled
				the sheet, and refusing all but one of those would make the
				import useless on every sheet but the one it was written for.
			*/
		static ElementData::PlcIOType typeFromString(const QString &text,
							     bool *ok = nullptr);

			/**
				@brief Fold a text down to what two people typing the same thing
				share: no case, no accent, no double space.
				Used both to read a type and to recognise a description that was
				retyped rather than rewritten.
			*/
		static QString normalize(const QString &text);

	public:
			/**
				Given when the point enters a list and never afterwards. It is
				this, and not the key of the import, that the assignment, the
				drawing and the cross reference know the point by - so that
				correcting a tag halfway through a project does not lose the
				channel already drawn.
			*/
		QString id;

			/// digital, analogue or universal, in or out - the program's own
		ElementData::PlcIOType type = ElementData::EntreeDigitale;
			/// what the automation calls this point, when it names it at all
		QString tag;
			/// the sentence a person reads on the folio
		QString description;
			/// where it sits on the card, as the automation writes it
		QString address;
			/// the card this point was asked to go in, by name, before assignment
		QString card;
			/// what is on the other end - a symbol name, or a .qetmak path
		QString connect_to;
			/// true when the point has to reach the field through a terminal
		bool needs_terminal = false;
			/// anything the sheet said that no other column has room for
		QString comment;

			/// uuid of the master element this point was assigned to, empty when free
		QString master_uuid;
			/// index in that master's list of I/O, -1 when free
		int io_index = -1;
			/// the channel of the card, as the catalogue names it
		QString channel;

			/// @return true when this point is already in a card
		bool isAssigned() const;
};

#endif // IOPOINT_H
