#ifndef WIRINGLISTEXPORT_H
#define WIRINGLISTEXPORT_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QStringList>
#include <QDomDocument>

class QETProject;
class QWidget;
class QDomElement;
class QDomNode;

struct ElementInfo {
    QString folio;
    QStringList links;
    QString label;
    QString name;
    QString location_path; // the location it is mounted in (T32)
    bool isPlaceholder = false;
};

struct ConductorData {
    int index = 0;
    QString el1_uuid;
    QString el2_uuid;
    QString element1_label;
    QString element2_label;
    QString terminalname1;
    QString terminalname2;
    QString tension_protocol;
    QString conductor_color;
    QString conductor_section;
    QString function;
    QString cable;
    QString folio;
    int folio_index = 0; // position of the folio in the project, from 1
    bool bridge = false; // its number names a bridge (WiringListExport::isBridgeNumber())
        // One half of a wire drawn across a folio report, left unjoined
        // because the other half declares another section
    bool section_mismatch = false;
    QString section_mismatch_folio; // the page of that other half
};

/**
 * @brief The WiringListExport class
 * Exports the wiring diagram from QElectroTech as a CSV file.
 */
class WiringListExport : public QObject
{
    Q_OBJECT
public:
    explicit WiringListExport(QETProject *project, QWidget *parent = nullptr);
    void toCsv();
    /**
	Build the wiring-list CSV and return it as a string (no GUI).
	Used by toCsv() and by the headless command-line export.

	@par An empty return is a failure, not an empty project
	The header row of eleven column names is written unconditionally, so a
	readable project with no cable in it returns that row and not an
	empty string. Both callers read an empty return as "the project
	could not be read": do not add an early return that gives back an
	empty string for an ordinary state. The invariant, and what it costs,
	is recorded on the definition in wiringlistexport.cpp. The price of
	saying it here too is a paragraph that has to be kept in step with
	that one.
    */
    QString toCsvString() const;
    /**
	Whether a conductor numbered @p number is a bridge between two
	terminals. Such a conductor is listed, with "Bridge" in the Note
	column, and never left out. The rule, and the names it knows, are
	on the definition in wiringlistexport.cpp.
    */
    static bool isBridgeNumber(const QString &number);

private:
    QETProject *m_project;
    QWidget *m_parent;

    QString normalizeUuid(const QString &u) const;
    /**
	The page number of a sheet, as the title block of that sheet draws it.

	@param diagramElem : the <diagram> element of the project document
	@param total_diagrams : how many sheets the document holds, which is
	the caller's job to count once rather than this one's to count per
	conductor. Zero or less means "not known", and the folio is then given
	back as it is stored.
    */
    QString findDiagramFolio(const QDomElement &diagramElem, int total_diagrams) const;
    int diagramCount(const QDomElement &root) const;
    int diagramIndex(const QDomElement &diagramElem) const;
    QDomElement climbToDiagram(QDomNode node) const;
    QDomDocument projectDocument() const;

    QMap<QString, ElementInfo> collectElementsInfo(const QDomElement &root) const;
    QList<ConductorData> collectConductors(const QDomElement &root) const;
};

#endif // WIRINGLISTEXPORT_H
