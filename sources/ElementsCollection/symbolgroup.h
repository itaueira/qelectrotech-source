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
#ifndef SYMBOLGROUP_H
#define SYMBOLGROUP_H

#include <QDomDocument>
#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief A piece of schematic saved in the library, insertable like a
	symbol.

	A grouping is not a symbol and not a parameterised macro. It has no
	variable: it is a start/stop command, a motor feeder, a supply, drawn
	once and inserted whole. What makes it worth having is that the
	components inside keep the catalog parts already assigned to them — the
	half hour spent choosing the contactor and the overload is spent once.

	It holds the very fragment of the sheet the copy command produces, which
	is why it comes back complete: element information, conductors, texts,
	positions. Nothing is translated into another format on the way in, so
	nothing can be lost on the way out.

	Editing what was inserted does not change what is saved: the file was
	copied into the sheet, not linked to it. Changing the saved grouping is a
	deliberate act — draw it again and save over the same name.
*/
class SymbolGroup
{
	public:
		SymbolGroup();

		QString name;
		QString description;
		/// the <diagram> fragment as the sheet produced it
		QDomDocument fragment;

		bool isNull() const;

		int elementCount() const;
		int conductorCount() const;
		/**
			@return the part codes carried by the components inside, each one
			once. Shown before inserting so the designer knows the grouping
			brings parts with it, and shown before saving so they can see the
			work they are about to file away.
		*/
		QStringList partCodes() const;

		bool save(const QString &file_path, QString *error = nullptr) const;
		static SymbolGroup load(const QString &file_path,
					QString *error = nullptr);

		/**
			@brief Build a grouping out of the fragment the sheet copied.
			@param name what to call it
			@param fragment the <diagram> fragment
		*/
		static SymbolGroup fromFragment(const QString &name,
						const QDomDocument &fragment);

		/// the file extension, without the dot
		static QString extension();
		/// a file name for @a name, safe on every platform
		static QString fileNameFor(const QString &name);
		/// the groupings filed in @a dir_path, by file path, sorted by name
		static QStringList listFolder(const QString &dir_path);

	private:
		/// the root <diagram> of the fragment, or a null element
		QDomElement root() const;
};

#endif // SYMBOLGROUP_H
