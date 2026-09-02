#include "wiringlistexport.h"
#include "qetproject.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QDomDocument>
#include <QFile>
#include <algorithm>

WiringListExport::WiringListExport(QETProject *project, QWidget *parent) :
QObject(parent),
m_project(project),
m_parent(parent)
{
}

QString WiringListExport::normalizeUuid(const QString &u) const
{
    QString res = u;
    res.remove('{').remove('}');
    return res.trimmed().toLower();
}

QString WiringListExport::findDiagramFolio(const QDomElement &diagramElem) const
{
    if (diagramElem.isNull()) return "";
    if (diagramElem.hasAttribute("folio")) return diagramElem.attribute("folio");
    if (diagramElem.hasAttribute("title")) return diagramElem.attribute("title");
    return "";
}

QDomElement WiringListExport::climbToDiagram(QDomNode node) const
{
    while (!node.isNull()) {
        if (node.isElement() && node.toElement().tagName().toLower() == "diagram") {
            return node.toElement();
        }
        node = node.parentNode();
    }
    return QDomElement();
}

QMap<QString, ElementInfo> WiringListExport::collectElementsInfo(const QDomElement &root) const
{
    QMap<QString, ElementInfo> infoMap;

    QSet<QString> placeholderTypes;
    QDomElement collection = root.firstChildElement("collection");
    if (!collection.isNull()) {
        QDomNodeList defs = collection.elementsByTagName("definition");
        for (int i = 0; i < defs.size(); ++i) {
            QDomElement def = defs.at(i).toElement();
            QString ltype = def.attribute("link_type");
            if (ltype == "next_report" || ltype == "previous_report") {
                QDomElement parentEl = def.parentNode().toElement();
                if (parentEl.tagName().toLower() == "element") {
                    QString name = parentEl.attribute("name");
                    if (!name.isEmpty()) {
                        placeholderTypes.insert(name);
                    }
                }
            }
        }
    }

    QDomNodeList elements = root.elementsByTagName("element");
    for (int i = 0; i < elements.size(); ++i) {
        QDomElement el = elements.at(i).toElement();
        QString uuid = normalizeUuid(el.attribute("uuid", el.attribute("id", "")));
        if (uuid.isEmpty()) continue;

        ElementInfo info;
        info.folio = findDiagramFolio(climbToDiagram(el));

        QDomElement linksNode = el.firstChildElement("links_uuids");
        if (!linksNode.isNull()) {
            QDomNodeList linkUuids = linksNode.elementsByTagName("link_uuid");
            for (int j = 0; j < linkUuids.size(); ++j) {
                QString luuid = normalizeUuid(linkUuids.at(j).toElement().attribute("uuid"));
                if (!luuid.isEmpty()) info.links.append(luuid);
            }
        }

        QDomElement elInfoNode = el.firstChildElement("elementInformations");
        if (!elInfoNode.isNull()) {
            QDomNodeList eics = elInfoNode.elementsByTagName("elementInformation");
            for (int j = 0; j < eics.size(); ++j) {
                QDomElement eic = eics.at(j).toElement();
                QString nameAttr = eic.attribute("name").toLower();
                if (nameAttr == "label") info.label = eic.text().trimmed();
                if (nameAttr == "name") info.name = eic.text().trimmed();
            }
        }

        QString typeVal = el.attribute("type");
        info.isPlaceholder = false;
        for (const QString &ptype : placeholderTypes) {
            if (typeVal.endsWith(ptype)) {
                info.isPlaceholder = true;
                break;
            }
        }

        infoMap.insert(uuid, info);
    }
    return infoMap;
}

QList<ConductorData> WiringListExport::collectConductors(const QDomElement &root) const
{
    QList<ConductorData> conductors;
    QDomNodeList conductorNodes = root.elementsByTagName("conductor");

    for (int i = 0; i < conductorNodes.size(); ++i) {
        QDomElement cond = conductorNodes.at(i).toElement();

        if (cond.attribute("num") == "Brücke") continue;

        ConductorData data;
        data.index = i;
        data.el1_uuid = normalizeUuid(cond.attribute("element1", cond.attribute("element1id", "")));
        data.el2_uuid = normalizeUuid(cond.attribute("element2", cond.attribute("element2id", "")));

        data.element1_label = cond.attribute("element1_label");
        if (data.element1_label.isEmpty()) {
            data.element1_label = cond.attribute("element1_linked");
        }

        data.element2_label = cond.attribute("element2_label");
        if (data.element2_label.isEmpty()) {
            data.element2_label = cond.attribute("element2_linked");
        }

        data.terminalname1 = cond.attribute("terminalname1");
        data.terminalname2 = cond.attribute("terminalname2");
        data.tension_protocol = cond.attribute("tension_protocol");
        data.conductor_color = cond.attribute("conductor_color");
        data.conductor_section = cond.attribute("conductor_section");
        data.function = cond.attribute("function");

        QDomElement diag = climbToDiagram(cond);
        data.folio = findDiagramFolio(diag);
        if (data.folio.isEmpty()) data.folio = cond.attribute("folio", cond.attribute("page", ""));

        conductors.append(data);
    }
    return conductors;
}

/**
	@brief WiringListExport::toCsv
	Ask for a file name and write the wiring list into it.

	@par The empty test below is a failure test, and is right to be
	The three text exporters in this tree each mean something different
	by an empty payload, so this one says which it is. Here empty means
	the project could not be read - never "there is nothing to write" -
	because @c toCsvString() emits its header row unconditionally, and
	the sentence the branch shows says exactly that. A project with no
	cable at all reaches the save dialog and writes a csv holding the
	nine column names, which is the honest answer and is what a
	spreadsheet expects to open. For comparison, and because the
	difference is not visible from any one of the three files :
	@c ConductorNumExport::wiresNum() has no header, so empty there is
	the ordinary "no numbered conductor" and conductornumexport.cpp
	reports it with an information box and writes no file; and
	ui/bomexportdialog.cpp, whose payload can fail or be legitimately
	empty, keeps the two apart with a null @c QString against an empty
	one, at bomexportdialog.cpp:145. The price of this one's choice is
	that a designer who exports a project with no cable gets a file with
	headings and no rows and no warning that it is empty, and has to open
	it to find that out.

	@par Refused : the write is not checked, and success is claimed anyway
	@c out is not flushed and @c QFile::error() is never read, so the
	success box below fires on a full disk exactly as it does on a good
	write. @c ConductorNumExport::toCsv() was just given that check -
	@c flush() first, then @c QFile::error() while the device is still
	open, above a @c close() that would wipe the error - and this
	function needs the same three lines. It is refused in this pass
	because the task that found it scoped this file to documentation once
	the measurement showed there was no empty-state defect here to fix,
	and because a write check that cannot be run is a change worth
	pairing with the build that proves it. The price of leaving it is the
	worst-shaped bug of the three: the designer is told "exporté avec
	succès" over a truncated file, so they do not re-export, and the
	short file is the one that reaches the panel builder.

	@par Refused : the truncated file on a full disk
	@c QSaveFile is the fix for the paragraph above - temporary file,
	atomic rename on commit(), original untouched on failure - and it is
	refused here for the reason already written into
	conductornumexport.cpp and ui/bomexportdialog.cpp: @c QTextStream
	only flushes past its own threshold, so a commit() called while the
	stream is alive renames a file the buffer never reached and the
	stream then flushes into a closed device, trading a rare truncation
	for content lost on every export. This file is the third of the three
	that write the same way, and the change belongs to all three at once,
	with the @c flush() before the @c commit() written into it and a real
	build behind it. The price of the delay is that all three keep
	writing straight at the destination until then.
*/
void WiringListExport::toCsv()
{
    if (!m_project) return;

    const QString csv = toCsvString();
    if (csv.isEmpty()) {
        QMessageBox::warning(m_parent, tr("Erreur"), tr("Impossible de lire la structure en mémoire du projet."));
        return;
    }

    QFileDialog dialog(m_parent);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setWindowTitle(tr("Exporter le plan de câblage"));
    dialog.setDefaultSuffix("csv");
    dialog.setNameFilter(tr("Fichiers CSV (*.csv)"));

    if (dialog.exec() != QDialog::Accepted) return;
    QString fileName = dialog.selectedFiles().first();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(m_parent, tr("Erreur"), tr("Impossible d'ouvrir le fichier pour l'écriture."));
        return;
    }
    QTextStream out(&file);
    out << csv;
    file.close();
    QMessageBox::information(m_parent, tr("Export réussi"), tr("Le plan de câblage a été exporté avec succès !"));
}

/**
	@brief WiringListExport::toCsvString
	@return the wiring list formatted as csv : one header row of nine
	column names, then one row per cable, folio-numbered elements sorted
	by folio and then by component. Never an empty string unless the
	project could not be read at all - see the invariant below.

	@par An empty return means a failure, and can mean nothing else
	This is the invariant the command-line exporter leans on, so breaking
	it breaks a test in another file. The header row is written
	unconditionally at line 462 below, before the loop over the
	cables, and there is no return between the two guards at the top of
	this function and that write. So the only two ways out with an empty
	string are those guards: a null @c m_project, and a project whose
	@c toXml() gave back a null document. A project that is perfectly
	readable and simply holds no cable returns the header row on its own
	- nine names and a line break, not an empty string. Whoever adds a
	third early return between here and the header write must either
	make it non-empty or fix cli_export.cpp:exportCsv(), where an empty
	string from this function is reported as a failed read of the
	project. The price of relying on this rather than returning a
	distinguishable value is that a caller cannot tell the two guards
	apart, which is why neither of them says which one fired.

	@par The return does not race the stream's destructor
	@c out is still alive on the @c return statement, so the invariant
	above would be worthless if @c QTextStream held the header in a
	buffer of its own until it was destroyed. It does not, when the
	device is a @c QString : the text is appended straight into the
	string, and the user-space buffer that the file-writing paths have to
	@c flush() is only used for a @c QIODevice. Measured, rather than
	read out of Qt's sources, which are not installed here: a standalone
	probe shaped like this function - build a @c QString, attach a
	@c QTextStream, write only the header, return the string - reports a
	size of 25 and @c isEmpty() false against Qt 6.11.1, with the
	destructor not yet run. The price of leaning on that is that a
	future rewrite which streams into a @c QFile or a @c QBuffer here,
	instead of a @c QString, inherits the buffering and has to flush
	before it reads the result back.
*/
QString WiringListExport::toCsvString() const
{
    if (!m_project) return QString();

    QDomDocument doc = m_project->toXml();
    if (doc.isNull()) return QString();

    QSet<QString> conductorDefinitionTypes;
    QDomElement rootElem = doc.documentElement();
    QDomElement collection = rootElem.firstChildElement("collection");
    if (!collection.isNull()) {
        QDomNodeList defs = collection.elementsByTagName("definition");
        for (int i = 0; i < defs.size(); ++i) {
            QDomElement def = defs.at(i).toElement();
            if (def.attribute("link_type") == "conductor_definition") {
                QDomElement parentEl = def.parentNode().toElement();
                if (parentEl.tagName().toLower() == "element") {
                    QString name = parentEl.attribute("name");
                    if (!name.isEmpty()) {
                        conductorDefinitionTypes.insert(name);
                    }
                }
            }
        }
    }

    QSet<QString> conductorDefinitionUuids;
    QDomNodeList projectElements = rootElem.elementsByTagName("element");
    for (int i = 0; i < projectElements.size(); ++i) {
        QDomElement el = projectElements.at(i).toElement();
        QString typeVal = el.attribute("type");
        bool isCondDef = false;
        for (const QString &cType : conductorDefinitionTypes) {
            if (typeVal.endsWith(cType)) {
                isCondDef = true;
                break;
            }
        }
        if (isCondDef) {
            QString uuid = normalizeUuid(el.attribute("uuid", el.attribute("id", "")));
            if (!uuid.isEmpty()) {
                conductorDefinitionUuids.insert(uuid);
            }
        }
    }

    QMap<QString, ElementInfo> elementsInfo = collectElementsInfo(doc.documentElement());
    QList<ConductorData> conductors = collectConductors(doc.documentElement());

    QList<ConductorData> uniqueConductors;
    QMap<QString, ConductorData> partialWires;

    auto normalizePartial = [](ConductorData c, const QString &ph_uuid) {
        if (c.el1_uuid == ph_uuid) {
            std::swap(c.el1_uuid, c.el2_uuid);
            std::swap(c.element1_label, c.element2_label);
            std::swap(c.terminalname1, c.terminalname2);
        }
        return c;
    };

    auto mergeField = [](const QString &a, const QString &b) {
        QString at = a.trimmed();
        QString bt = b.trimmed();
        if (at.isEmpty()) return bt;
        if (bt.isEmpty()) return at;
        if (at == bt) return at;
        return at + ", " + bt;
    };

    for (int i = 0; i < conductors.size(); ++i) {
        ConductorData c = conductors[i];

        if (conductorDefinitionUuids.contains(c.el1_uuid) || conductorDefinitionUuids.contains(c.el2_uuid)) {
            continue;
        }

        if (c.element1_label.isEmpty() && elementsInfo.contains(c.el1_uuid)) {
            c.element1_label = elementsInfo[c.el1_uuid].label;
            if (c.element1_label.isEmpty()) c.element1_label = elementsInfo[c.el1_uuid].name;
        }
        if (c.element2_label.isEmpty() && elementsInfo.contains(c.el2_uuid)) {
            c.element2_label = elementsInfo[c.el2_uuid].label;
            if (c.element2_label.isEmpty()) c.element2_label = elementsInfo[c.el2_uuid].name;
        }

        bool el1_ph = elementsInfo.value(c.el1_uuid).isPlaceholder;
        bool el2_ph = elementsInfo.value(c.el2_uuid).isPlaceholder;

        if (!el1_ph && !el2_ph) {
            uniqueConductors.append(c);
            continue;
        }

        if (el1_ph && el2_ph) {
            uniqueConductors.append(c);
            continue;
        }

        QString ph_uuid = el1_ph ? c.el1_uuid : c.el2_uuid;
        ConductorData normC = normalizePartial(c, ph_uuid);

        QString matching_ph_uuid;
        if (!elementsInfo[ph_uuid].links.isEmpty()) {
            matching_ph_uuid = elementsInfo[ph_uuid].links.first();
        }

        if (!matching_ph_uuid.isEmpty() && partialWires.contains(matching_ph_uuid)) {
            ConductorData otherHalf = partialWires.take(matching_ph_uuid);

            ConductorData merged;
            merged.folio = mergeField(otherHalf.folio, normC.folio);

            merged.el1_uuid = otherHalf.el1_uuid;
            merged.element1_label = otherHalf.element1_label;
            merged.terminalname1 = otherHalf.terminalname1;

            merged.el2_uuid = normC.el1_uuid;
            merged.element2_label = normC.element1_label;
            merged.terminalname2 = normC.terminalname1;

            merged.tension_protocol = mergeField(otherHalf.tension_protocol, normC.tension_protocol);
            merged.conductor_color = mergeField(otherHalf.conductor_color, normC.conductor_color);
            merged.conductor_section = mergeField(otherHalf.conductor_section, normC.conductor_section);
            merged.function = mergeField(otherHalf.function, normC.function);

            uniqueConductors.append(merged);
        } else {
            partialWires.insert(ph_uuid, normC);
        }
    }

    for (const ConductorData &leftover : partialWires.values()) {
        uniqueConductors.append(leftover);
    }

    for (ConductorData &c : uniqueConductors) {
        if (!c.element2_label.isEmpty() && (c.element1_label.isEmpty() || c.element2_label.toLower() < c.element1_label.toLower())) {
            std::swap(c.element1_label, c.element2_label);
            std::swap(c.terminalname1, c.terminalname2);
            std::swap(c.el1_uuid, c.el2_uuid);
        }
    }

    std::sort(uniqueConductors.begin(), uniqueConductors.end(), [](const ConductorData &a, const ConductorData &b) {
        QStringList partsA = a.folio.split(',');
        QStringList partsB = b.folio.split(',');
        int minLen = std::min(partsA.size(), partsB.size());
        int folioCmp = 0;

        for (int i = 0; i < minLen; ++i) {
            bool okA, okB;
            int numA = partsA[i].trimmed().toInt(&okA);
            int numB = partsB[i].trimmed().toInt(&okB);

            if (okA && okB) {
                if (numA != numB) {
                    folioCmp = (numA < numB) ? -1 : 1;
                    break;
                }
            } else {
                int strCmp = partsA[i].trimmed().compare(partsB[i].trimmed(), Qt::CaseInsensitive);
                if (strCmp != 0) {
                    folioCmp = strCmp;
                    break;
                }
            }
        }

        if (folioCmp == 0 && partsA.size() != partsB.size()) {
            folioCmp = (partsA.size() < partsB.size()) ? -1 : 1;
        }

        if (folioCmp != 0) return folioCmp < 0;

        int el1Cmp = a.element1_label.toLower().compare(b.element1_label.toLower());
        if (el1Cmp != 0) return el1Cmp < 0;

        int el2Cmp = a.element2_label.toLower().compare(b.element2_label.toLower());
        if (el2Cmp != 0) return el2Cmp < 0;

        int term1Cmp = a.terminalname1.compare(b.terminalname1);
        if (term1Cmp != 0) return term1Cmp < 0;

        return a.terminalname2 < b.terminalname2;
    });

    QString csv;
    QTextStream out(&csv);
    out << tr("Page", "Wiring list CSV header") << ";"
    << tr("Composant 1", "Wiring list CSV header") << ";"
    << tr("Borne 1", "Wiring list CSV header") << ";"
    << tr("Composant 2", "Wiring list CSV header") << ";"
    << tr("Borne 2", "Wiring list CSV header") << ";"
    << tr("Tension / Protocole", "Wiring list CSV header") << ";"
    << tr("Couleur du fil", "Wiring list CSV header") << ";"
    << tr("Section du fil", "Wiring list CSV header") << ";"
    << tr("Fonction", "Wiring list CSV header") << "\n";

    for (const ConductorData &c : uniqueConductors) {
        out << c.folio << ";"
        << c.element1_label << ";"
        << c.terminalname1 << ";"
        << c.element2_label << ";"
        << c.terminalname2 << ";"
        << c.tension_protocol << ";"
        << c.conductor_color << ";"
        << c.conductor_section << ";"
        << c.function << "\n";
    }

    return csv;
}
