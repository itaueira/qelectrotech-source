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

#include <QDate>
#include <QLocale>
#include <QObject>
#include <QHash>
#include <QRegularExpression>
#include <algorithm>
#include "diagramcontext.h"
#include "location/locationtree.h"
#include "qetinformation.h"

/**
	@brief QETInformation::titleblockInfoKeys
	@return all available key for use with a titleblock
*/
QStringList QETInformation::titleblockInfoKeys()
{
	QStringList info_list = { DIA_AUTHOR,
							  DIA_DATE,
							  DIA_TITLE,
							  DIA_FILENAME,
							  DIA_PLANT,
							  DIA_LOCMACH,
							  DIA_INDEX_REV,
							  QET_VERSION,
							  DIA_FOLIO,
							  DIA_FOLIO_ID,
							  PROJECT_FOLIO_TOTAL,
							  DIA_PREVIOUS_FOLIO_NUM,
							  DIA_NEXT_FOLIO_NUM,
							  PROJECT_TITLE,
							  PROJECT_PATH,
							  PROJECT_FILE_NAME,
							  PROJECT_SAVE_DATE,
							  PROJECT_SAVE_DATE_EU,
							  PROJECT_SAVE_DATE_US,
							  PROJECT_SAVE_TIME,
							  PROJECT_SAVED_FILE_NAME,
							  PROJECT_SAVED_FILE_PATH };
	return info_list;
}

/**
	@brief QETInformation::stripUnresolvedVariables
	@param text
	@return @a text without the variables nothing filled in
*/
QString QETInformation::stripUnresolvedVariables(const QString &text)
{
	if (!text.contains(QLatin1Char('%'))) {
		return text;
	}
	QString stripped = text;

		//The braced form goes unconditionally: the braces are what make it a
		//variable reference and nothing else, so "%{whatever}" left standing
		//can only be a variable that was never filled - including a custom
		//field somebody added to the template and never gave a value.
	static const QRegularExpression braced(QStringLiteral("%\\{[^{}]*\\}"));
	stripped.remove(braced);

		//The bare form only goes for the vocabulary the title block actually
		//has, and longest first so that "%folio-total" is not eaten as
		//"%folio" plus a stray "-total". Anything else beginning with a per
		//cent sign is left exactly as typed: a title block legitimately says
		//"100%", and a cell that lost its per cent sign would be a worse bug
		//than the one being fixed.
	QStringList keys = titleblockInfoKeys();
	std::sort(keys.begin(), keys.end(),
		  [](const QString &a, const QString &b) {
			return a.length() > b.length();
		  });
	for (const QString &key : keys) {
			//Plain +, and not the % of QStringBuilder: % needs <QStringBuilder>,
			//which Qt6 pulls in on its own and Qt5 does not, so % here builds on
			//one and not the other. remove() takes a QString anyway, so the
			//builder would buy nothing.
		stripped.remove(QLatin1Char('%') + key);
	}
	return stripped;
}

/**
	@brief QETInformation::titleblockVariablesIn
	@param text
	@return the variable names @a text refers to
*/
QStringList QETInformation::titleblockVariablesIn(const QString &text)
{
	QStringList names;
	if (!text.contains(QLatin1Char('%'))) {
		return names;
	}
	QString scratch = text;

		//Same two rules as stripUnresolvedVariables, in the same order,
		//because what the folio erases is exactly what the editor has to
		//name. The braced form goes first and is taken out of the scratch
		//copy, so the bare pass below never sees what it already claimed.
	static const QRegularExpression braced(
				QStringLiteral("%\\{([^{}]*)\\}"));
	QRegularExpressionMatchIterator iterator = braced.globalMatch(scratch);
	while (iterator.hasNext()) {
		const QString name = iterator.next().captured(1);
			//"%{}" names nothing, and the folio drops it all the same:
			//there is no name to put back in its place.
		if (!name.isEmpty() && !names.contains(name)) {
			names << name;
		}
	}
	scratch.remove(braced);

		//Sorted once and kept: the keys are constants, and this runs for
		//every cell of every repaint of the title block editor.
	static const QStringList keys = []() {
		QStringList sorted = titleblockInfoKeys();
		std::sort(sorted.begin(), sorted.end(),
			  [](const QString &a, const QString &b) {
				return a.length() > b.length();
			  });
		return sorted;
	}();
	for (const QString &key : keys) {
		const QString reference = QLatin1Char('%') + key;
		if (!scratch.contains(reference)) {
			continue;
		}
		if (!names.contains(key)) {
			names << key;
		}
			//Taken out for the same reason the longest key is tried
			//first: otherwise "folio" would claim what is left of
			//"%folio-total".
		scratch.remove(reference);
	}
	return names;
}

/**
	@brief QETInformation::titleblockAuthoringContext
	@param texts
	@return a context in which each variable of @a texts stands for its own
	name
*/
DiagramContext QETInformation::titleblockAuthoringContext(
		const QStringList &texts)
{
	DiagramContext context;
	for (const QString &text : texts) {
		const QStringList names = titleblockVariablesIn(text);
		for (const QString &name : names) {
				//The value is the name inside brackets: substitution
				//then leaves a marked name where the value would have
				//gone, and stripUnresolvedVariables finds nothing left
				//to erase. A name the context refuses is dropped here,
				//and that is deliberate - see the header.
				//
				//The brackets are what tells a field apart from fixed
				//text that happens to read the same. Without them a cell
				//holding the literal word "author" and a cell holding
				//%{author} draw identically, and the person drawing the
				//title block cannot tell which is which until the first
				//project is printed - which is too late. They exist only
				//in the editor preview: the printed sheet goes through
				//render() with the project context and never sees them.
			context.addValue(name, QStringLiteral("[") + name
					 + QStringLiteral("]"));
		}
	}
	return context;
}

/**
	@brief QETInformation::titleblockInfoKeysToVar
	@param info
	@return var in form %{my-var} corresponding to the info.
	if there is not available var for the given info, the returned var is %{void}
*/
QString QETInformation::titleblockInfoKeysToVar(const QString &info)
{
	if (titleblockInfoKeys().contains(info))
		return infoToVar(info);
	else
		return QString("%{void}");
}

/**
	@brief QETInformation::titleblockTranslatedKeyHashVar
	@return a QHash with for key, the translated information key of title block,
	and for value the corresponding var.
*/
QHash<QString, QString> QETInformation::titleblockTranslatedKeyHashVar()
{
	QHash <QString, QString> hash_;
	for (QString str : titleblockInfoKeys()) {
		hash_.insert(translatedInfoKey(str), titleblockInfoKeysToVar(str));
	}
	return hash_;
}

/**
 * @brief QETInformation::folioReportInfoKey
 * @return The info key available for dynamic element text item of a folio report
 */
QStringList QETInformation::folioReportInfoKeys()
{
	QStringList list = { ELMT_LABEL,
						 ELMT_REPORT_TAG,
						 COND_FUNCTION,
						 COND_TENSION_PROTOCOL,
						 COND_COLOR,
						 COND_SECTION };

	return list;
}
QHash<QString, QString> QETInformation::folioReportInfoKeyToVar()
{
	QHash <QString, QString> H_;
	H_.insert(ELMT_LABEL,            infoToVar(ELMT_LABEL));
	H_.insert(ELMT_REPORT_TAG,       infoToVar(ELMT_REPORT_TAG));
	H_.insert(COND_FUNCTION,         infoToVar(COND_FUNCTION));
	H_.insert(COND_TENSION_PROTOCOL, infoToVar(COND_TENSION_PROTOCOL));
	H_.insert(COND_COLOR,            infoToVar(COND_COLOR));
	H_.insert(COND_SECTION,          infoToVar(COND_SECTION));

	return H_;
}

QString QETInformation::folioReportInfoToVar(const QString &info)
{
	if (QETInformation::folioReportInfoKeys().contains(info))
		return infoToVar(info);
	else
		return (QString ("%{void}"));
}

QStringList QETInformation::conductorInfoKeys()
{
	QStringList list = 	{ COND_FORMULA,
						  COND_TEXT,
						  COND_FUNCTION,
						  COND_TENSION_PROTOCOL,
						  COND_COLOR,
						  COND_SECTION };
	return  list;
}

QStringList QETInformation::diagramInfoKeys()
{
	QStringList list = { DIA_TITLE,
						 DIA_AUTHOR,
						 DIA_FILENAME,
						 DIA_FOLIO,
						 DIA_PLANT,
						 DIA_LOCMACH,
						 DIA_INDEX_REV,
						 DIA_DATE,
						 DIA_DISPLAY_FOLIO };
	return list;
}

QStringList QETInformation::elementInfoKeys()
{
	QStringList list = { ELMT_FORMULA,
						 ELMT_LABEL,
						 ELMT_PLANT,
						 ELMT_LOCATION,
						 ELMT_LOCATION_PATH,
						 ELMT_CONNECTOR,
						 ELMT_COMMENT,
						 ELMT_FUNCTION,
						 ELMT_DESCRIPTION,
						 ELMT_DESIGNATION,
						 ELMT_MANUFACTURER,
						 ELMT_MANUFACTURER_REF,
						 ELMT_MODEL,
						 ELMT_CATEGORY,
						 ELMT_VOLTAGE_RATING,
						 ELMT_CURRENT_RATING,
						 ELMT_WIRE_SECTION,
						 ELMT_NOTES,
						 ELMT_MACHINE_MANUFACTURER_REF,
						 ELMT_SUPPLIER,
						 ELMT_QUANTITY,
						 ELMT_UNITY,
						 ELMT_WIDTH,
						 ELMT_HEIGHT,
						 ELMT_DEPTH,
						 ELMT_AUX1,
						 ELMT_DESCRIPTION_AUX1,
						 ELMT_DESIGNATION_AUX1,
						 ELMT_MANUFACTURER_AUX1,
						 ELMT_MANUFACTURER_REF_AUX1,
						 ELMT_MACHINE_MANUFACTURER_REF_AUX1,
						 ELMT_SUPPLIER_AUX1,
						 ELMT_QUANTITY_AUX1,
						 ELMT_UNITY_AUX1,
						 ELMT_AUX2,
						 ELMT_DESCRIPTION_AUX2,
						 ELMT_DESIGNATION_AUX2,
						 ELMT_MANUFACTURER_AUX2,
						 ELMT_MANUFACTURER_REF_AUX2,
						 ELMT_MACHINE_MANUFACTURER_REF_AUX2,
						 ELMT_SUPPLIER_AUX2,
						 ELMT_QUANTITY_AUX2,
						 ELMT_UNITY_AUX2,
						 ELMT_AUX3,
						 ELMT_DESCRIPTION_AUX3,
						 ELMT_DESIGNATION_AUX3,
						 ELMT_MANUFACTURER_AUX3,
						 ELMT_MANUFACTURER_REF_AUX3,
						 ELMT_MACHINE_MANUFACTURER_REF_AUX3,
						 ELMT_SUPPLIER_AUX3,
						 ELMT_QUANTITY_AUX3,
						 ELMT_UNITY_AUX3,
						 ELMT_AUX4,
						 ELMT_DESCRIPTION_AUX4,
						 ELMT_DESIGNATION_AUX4,
						 ELMT_MANUFACTURER_AUX4,
						 ELMT_MANUFACTURER_REF_AUX4,
						 ELMT_MACHINE_MANUFACTURER_REF_AUX4,
						 ELMT_SUPPLIER_AUX4,
						 ELMT_QUANTITY_AUX4,
						 ELMT_UNITY_AUX4,
						 ELMT_PLC_TYPE,
						 ELMT_PLC_ADDRESS,
						 ELMT_PLC_FUNCTION,
						 ELMT_PLC_COMMENT,
						 ELMT_PLC_CROSSREF,
						 ELMT_PLC_UNIT,
						 ELMT_PLC_BUS,
						 "exclude_from_bom",
						 ELMT_PART_CODE,
						 ELMT_PART_REVISION };
	return list;
}

/**
 * @brief QETInformation::elementInfoToVar
 * @param info
 * @return The info to a variable or %{void} if @info
 * is not an element information
 */
QString QETInformation::elementInfoToVar(const QString &info)
{
	if (QETInformation::elementInfoKeys().contains(info))
		return infoToVar(info);
	else
		return (QString ("%{void}"));
}

/**
 * @brief QETInformation::numericInfoPattern
 * @return the pattern used to validate numeric elementInformation
 * fields (currently width/height/depth): digits with an optional "."
 * as decimal separator, requiring at least one digit overall so a
 * lone "." can never be a complete, acceptable value on its own.
 */
QRegularExpression QETInformation::numericInfoPattern()
{
	return QRegularExpression(QStringLiteral(R"(^[0-9]+$|^[0-9]*\.[0-9]{1,2}$)"));
}

/**
	@brief QETInformation::NumericInfoValidator::NumericInfoValidator
	@param parent
*/
QETInformation::NumericInfoValidator::NumericInfoValidator(QObject *parent) :
	QRegularExpressionValidator(numericInfoPattern(), parent)
{
}

/**
	@brief QETInformation::NumericInfoValidator::validate
	Rewrites any "," in @a input to "." in place, then delegates to
	the base class for the actual numericInfoPattern() check. @a pos
	is left untouched by the rewrite itself -- replacing "," with "."
	never changes the string's length, so the cursor position the
	caller already tracked stays correct.
	@param input the text being validated; may be rewritten
	@param pos the cursor position within @a input
	@return the resulting validation state
*/
QValidator::State QETInformation::NumericInfoValidator::validate(QString &input, int &pos) const
{
	if (input.contains(QLatin1Char(',')))
		input.replace(QLatin1Char(','), QLatin1Char('.'));
	return QRegularExpressionValidator::validate(input, pos);
}

/**
 * @brief QETInformation::infoToVar
 * @param info
 * @return return the string @info prepended by %{ ans appended by }
 * example : if info is label, the returned string is %{label}
 */
QString QETInformation::infoToVar(const QString &info)
{
	return QString("%{")+info+QString("}");
}

/**
 * @brief QETInformation::translatedInfoKey
 * @param info
 * @return the information translated to the current local.
 * This function translate all information : element, conductor, diagram
 * Return an empty QString if don't match
 */
QString QETInformation::translatedInfoKey(const QString &info)
{
	if      (info == ELMT_MODEL)                       return QObject::tr("Model");
	else if (info == ELMT_CATEGORY)                    return QObject::tr("Category");
	else if (info == ELMT_VOLTAGE_RATING)              return QObject::tr("Rated voltage");
	else if (info == ELMT_CURRENT_RATING)              return QObject::tr("Rated current");
	else if (info == ELMT_WIRE_SECTION)                return QObject::tr("Wire cross-section");
	else if (info == ELMT_NOTES)                       return QObject::tr("Notes");
	else if (info == DIA_AUTHOR)                       return QObject::tr("Author");
	else if (info == DIA_DATE)                         return QObject::tr("Date");
	else if (info == DIA_TITLE)                        return QObject::tr("Title");
	else if (info == DIA_FILENAME)                     return QObject::tr("File");
	else if (info == DIA_PLANT)                        return QObject::tr("Plant (=)");
	else if (info == DIA_LOCMACH)                      return QObject::tr("Location (+)");
	else if (info == DIA_INDEX_REV)                    return QObject::tr("Revision index");
	else if (info == DIA_POS)                          return QObject::tr("Position");
	else if (info == QET_VERSION)                      return QObject::tr("Version of QElectroTech");
	else if (info == DIA_FOLIO)                        return QObject::tr("Sheet number");
	else if (info == DIA_FOLIO_ID)                     return QObject::tr("Sheet position");
	else if (info == PROJECT_FOLIO_TOTAL)              return QObject::tr("Number of sheets");
	else if (info == DIA_PREVIOUS_FOLIO_NUM)           return QObject::tr("Number of the previous sheet");
	else if (info == DIA_NEXT_FOLIO_NUM)               return QObject::tr("Number of the following sheet");
	else if (info == PROJECT_TITLE)                    return QObject::tr("Project title");
	else if (info == PROJECT_PATH)                     return QObject::tr("Project file path");
	else if (info == PROJECT_FILE_NAME)                return QObject::tr("File name");
	else if (info == PROJECT_SAVE_DATE)                return QObject::tr("Date of saving of the file in local date format");
	else if (info == PROJECT_SAVE_DATE_EU)             return QObject::tr("Date of saving of the file in the format DD-MM-YYYY");
	else if (info == PROJECT_SAVE_DATE_US)             return QObject::tr("Date of saving of the file in the format YYYY-MM-DD");
	else if (info == PROJECT_SAVE_TIME)                return QObject::tr("File saving time");
	else if (info == PROJECT_SAVED_FILE_NAME)          return QObject::tr("Name of the saved file");
	else if (info == PROJECT_SAVED_FILE_PATH)          return QObject::tr("Saved file path");
	else if (info == ELMT_FORMULA)                     return QObject::tr("Label formula");
	else if (info == ELMT_LABEL)                       return QObject::tr("Label");
	else if (info == ELMT_REPORT_TAG)                  return QObject::tr("Repère du renvoi");
	else if (info == ELMT_COMMENT)                     return QObject::tr("Annotation");
	else if (info == ELMT_FUNCTION)                    return QObject::tr("Function");
	else if (info == ELMT_DESCRIPTION)                 return QObject::tr("Textual description");
	else if (info == ELMT_DESIGNATION)                 return QObject::tr("Article number");
	else if (info == ELMT_MANUFACTURER)                return QObject::tr("Manufacturer");
	else if (info == ELMT_MANUFACTURER_REF)            return QObject::tr("Order number");
	else if (info == ELMT_MACHINE_MANUFACTURER_REF)    return QObject::tr("Internal number");
	else if (info == ELMT_SUPPLIER)                    return QObject::tr("Supplier");
	else if (info == ELMT_QUANTITY)                    return QObject::tr("Quantity");
	else if (info == ELMT_UNITY)                       return QObject::tr("Unity");
	else if (info == ELMT_PART_CODE)                   return QObject::tr("Référence de la pièce");
	else if (info == ELMT_PART_REVISION)               return QObject::tr("Révision de la pièce");
	else if (info == ELMT_WIDTH)					   return QObject::tr("Width [mm]");
	else if (info == ELMT_HEIGHT)                      return QObject::tr("Height [mm]");
	else if (info == ELMT_DEPTH)                       return QObject::tr("Depth [mm]");
	else if (info == ELMT_LOCATION)                    return QObject::tr("Location (+)");
	else if (info == ELMT_LOCATION_PATH)               return QObject::tr("Chemin de localisation");
	else if (info == ELMT_CONNECTOR)                   return QObject::tr("Connecteur");
	else if (info == COND_FUNCTION)                    return QObject::tr("Function");
	else if (info == COND_TENSION_PROTOCOL)            return QObject::tr("Voltage / Protocol");
	else if (info == COND_COLOR)                       return QObject::tr("Wire color");
	else if (info == COND_SECTION)                     return QObject::tr("Wire section");
	else if (info == COND_TEXT)                        return QObject::tr("Text");
	else if (info == COND_FORMULA)                     return QObject::tr("Text Formula");
	else if (info == ELMT_AUX1)                        return QObject::tr("Auxiliary block 1");
	else if (info == ELMT_DESCRIPTION_AUX1)                 return QObject::tr("Auxiliary 1 text description");
	else if (info == ELMT_DESIGNATION_AUX1)                 return QObject::tr("Auxiliary 1 article number");
	else if (info == ELMT_MANUFACTURER_AUX1)                return QObject::tr("Manufacturer of auxiliary 1");
	else if (info == ELMT_MANUFACTURER_REF_AUX1)            return QObject::tr("Order number auxiliary 1");
	else if (info == ELMT_MACHINE_MANUFACTURER_REF_AUX1)    return QObject::tr("Internal number of auxiliary 1");
	else if (info == ELMT_SUPPLIER_AUX1)                    return QObject::tr("supplier of auxiliary 1");
	else if (info == ELMT_QUANTITY_AUX1)                    return QObject::tr("Quantity of auxiliary 1");
	else if (info == ELMT_UNITY_AUX1)                       return QObject::tr("Unity of auxiliary 1");
	else if (info == ELMT_AUX2)                        return QObject::tr("Auxiliary block 2");
	else if (info == ELMT_DESCRIPTION_AUX2)                 return QObject::tr("Auxiliary 2 text description");
	else if (info == ELMT_DESIGNATION_AUX2)                 return QObject::tr("Auxiliary 2 article number");
	else if (info == ELMT_MANUFACTURER_AUX2)                return QObject::tr("Manufacturer of auxiliary 2");
	else if (info == ELMT_MANUFACTURER_REF_AUX2)            return QObject::tr("Order number auxiliary 2");
	else if (info == ELMT_MACHINE_MANUFACTURER_REF_AUX2)    return QObject::tr("Internal number of auxiliary 2");
	else if (info == ELMT_SUPPLIER_AUX2)                    return QObject::tr("supplier of auxiliary 2");
	else if (info == ELMT_QUANTITY_AUX2)                    return QObject::tr("Quantity of auxiliary 2");
	else if (info == ELMT_UNITY_AUX2)                       return QObject::tr("Unity of auxiliary 2");
	else if (info == ELMT_AUX3)                        return QObject::tr("Auxiliary block 3");
	else if (info == ELMT_DESCRIPTION_AUX3)                 return QObject::tr("Auxiliary 3 text description");
	else if (info == ELMT_DESIGNATION_AUX3)                 return QObject::tr("Auxiliary 3 article number");
	else if (info == ELMT_MANUFACTURER_AUX3)                return QObject::tr("Manufacturer of auxiliary 3");
	else if (info == ELMT_MANUFACTURER_REF_AUX3)            return QObject::tr("Order number auxiliary 3");
	else if (info == ELMT_MACHINE_MANUFACTURER_REF_AUX3)    return QObject::tr("Internal number of auxiliary 3");
	else if (info == ELMT_SUPPLIER_AUX3)                    return QObject::tr("supplier of auxiliary 3");
	else if (info == ELMT_QUANTITY_AUX3)                    return QObject::tr("Quantity of auxiliary 3");
	else if (info == ELMT_UNITY_AUX3)                       return QObject::tr("Unity of auxiliary 3");
	else if (info == ELMT_AUX4)                        return QObject::tr("Auxiliary block 4");
	else if (info == ELMT_DESCRIPTION_AUX4)                 return QObject::tr("Auxiliary 4 text description");
	else if (info == ELMT_DESIGNATION_AUX4)                 return QObject::tr("Auxiliary 4 article number");
	else if (info == ELMT_MANUFACTURER_AUX4)                return QObject::tr("Manufacturer of auxiliary 4");
	else if (info == ELMT_MANUFACTURER_REF_AUX4)            return QObject::tr("Order number auxiliary 4");
	else if (info == ELMT_MACHINE_MANUFACTURER_REF_AUX4)    return QObject::tr("Internal number of auxiliary 4");
	else if (info == ELMT_SUPPLIER_AUX4)                    return QObject::tr("supplier of auxiliary 4");
	else if (info == ELMT_QUANTITY_AUX4)                    return QObject::tr("Quantity of auxiliary 4");
	else if (info == ELMT_UNITY_AUX4)                       return QObject::tr("Unity of auxiliary 4");
	else if (info == ELMT_PLC_TYPE)                          return QObject::tr("Type PLC");
	else if (info == ELMT_PLC_ADDRESS)                       return QObject::tr("PLC address");
	else if (info == ELMT_PLC_FUNCTION)                      return QObject::tr("PLC function");
	else if (info == ELMT_PLC_COMMENT)                       return QObject::tr("PLC comment");
	else if (info == ELMT_PLC_CROSSREF)                      return QObject::tr("PLC cross-reference");
	else if (info == ELMT_PLC_UNIT)                          return QObject::tr("Automate");
	else if (info == ELMT_PLC_BUS)                           return QObject::tr("Barre PLC");
	else if (info == ELMT_XREF)                          return QObject::tr("Cross-reference");
	else return QString();
}

/**
	@brief QETInformation::displayedInfoValue
	@param info
	@param value
	@return @a value written the way a person has to read it
*/
QString QETInformation::displayedInfoValue(const QString &info, const QVariant &value)
{
	if (info == ELMT_LOCATION_PATH)
	{
			//The stored path is the tree's own writing, and the conversion
			//lives with the tree so that the drawing and the location
			//dialogues cannot disagree about what the norm writes. A
			//component nobody placed carries no path and converts to
			//nothing, which is the empty cell a reader expects.
		return LocationTree::iecTag(value.toString());
	}

		//The rule the drawn table and the exported list each carried on
		//their own: a value the variant reads as a date is written in the
		//locale of the machine, anything else is written as it stands.
	const QDate date = value.toDate();
	if (!date.isNull()) {
		return QLocale::system().toString(date, QLocale::ShortFormat);
	}

	return value.toString();
}

QStringList QETInformation::elementEditorElementInfoKeys()
{
	QStringList list = { ELMT_LABEL,
						 ELMT_PLANT,
						 ELMT_LOCATION,
						 ELMT_COMMENT,
						 ELMT_FUNCTION,
						 ELMT_DESCRIPTION,
						 ELMT_DESIGNATION,
						 ELMT_MANUFACTURER,
						 ELMT_MANUFACTURER_REF,
						 ELMT_MODEL,
						 ELMT_CATEGORY,
						 ELMT_VOLTAGE_RATING,
						 ELMT_CURRENT_RATING,
						 ELMT_NOTES,
						 ELMT_MACHINE_MANUFACTURER_REF,
						 ELMT_SUPPLIER,
						 ELMT_QUANTITY,
						 ELMT_UNITY,
						 ELMT_WIDTH,
						 ELMT_HEIGHT,
						 ELMT_DEPTH,
						 ELMT_AUX1,
						 ELMT_DESCRIPTION_AUX1,
						 ELMT_DESIGNATION_AUX1,
						 ELMT_MANUFACTURER_AUX1,
						 ELMT_MANUFACTURER_REF_AUX1,
						 ELMT_MACHINE_MANUFACTURER_REF_AUX1,
						 ELMT_SUPPLIER_AUX1,
						 ELMT_QUANTITY_AUX1,
						 ELMT_UNITY_AUX1,
						 ELMT_AUX2,
						 ELMT_DESCRIPTION_AUX2,
						 ELMT_DESIGNATION_AUX2,
						 ELMT_MANUFACTURER_AUX2,
						 ELMT_MANUFACTURER_REF_AUX2,
						 ELMT_MACHINE_MANUFACTURER_REF_AUX2,
						 ELMT_SUPPLIER_AUX2,
						 ELMT_QUANTITY_AUX2,
						 ELMT_UNITY_AUX2,
						 ELMT_AUX3,
						 ELMT_DESCRIPTION_AUX3,
						 ELMT_DESIGNATION_AUX3,
						 ELMT_MANUFACTURER_AUX3,
						 ELMT_MANUFACTURER_REF_AUX3,
						 ELMT_MACHINE_MANUFACTURER_REF_AUX3,
						 ELMT_SUPPLIER_AUX3,
						 ELMT_QUANTITY_AUX3,
						 ELMT_UNITY_AUX3,
						 ELMT_AUX4,
						 ELMT_DESCRIPTION_AUX4,
						 ELMT_DESIGNATION_AUX4,
						 ELMT_MANUFACTURER_AUX4,
						 ELMT_MANUFACTURER_REF_AUX4,
						 ELMT_MACHINE_MANUFACTURER_REF_AUX4,
						 ELMT_SUPPLIER_AUX4,
						 ELMT_QUANTITY_AUX4,
						 ELMT_UNITY_AUX4 };
	return list;
}
