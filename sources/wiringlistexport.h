#ifndef WIRINGLISTEXPORT_H
#define WIRINGLISTEXPORT_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QStringList>

class QETProject;
class QWidget;
class QDomElement;
class QDomNode;

struct ElementInfo {
    QString folio;
    QStringList links;
    QString label;
    QString name;
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
    QString folio;
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
	The header row of nine column names is written unconditionally, so a
	readable project with no cable in it returns that row and not an
	empty string. Both callers read an empty return as "the project
	could not be read": do not add an early return that gives back an
	empty string for an ordinary state. The invariant, and what it costs,
	is recorded on the definition in wiringlistexport.cpp. The price of
	saying it here too is a paragraph that has to be kept in step with
	that one.
    */
    QString toCsvString() const;

private:
    QETProject *m_project;
    QWidget *m_parent;

    QString normalizeUuid(const QString &u) const;
    QString findDiagramFolio(const QDomElement &diagramElem) const;
    QDomElement climbToDiagram(QDomNode node) const;

    QMap<QString, ElementInfo> collectElementsInfo(const QDomElement &root) const;
    QList<ConductorData> collectConductors(const QDomElement &root) const;
};

#endif // WIRINGLISTEXPORT_H
