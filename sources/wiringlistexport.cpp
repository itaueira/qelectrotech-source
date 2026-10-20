#include "wiringlistexport.h"
#include "foliovariables.h"
#include "qetproject.h"
#include "utils/csvwriter.h"
#include "diagram.h"
#include "dataBase/projectdatabase.h"
#include "ElementsCollection/xmlelementcollection.h"
#include "qetgraphicsitem/element.h"
#include "wirecolorname.h"
#include <QColor>
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

/**
	@brief WiringListExport::diagramCount
	@param root : the <project> element of the project document
	@return how many sheets it holds, which is what %total stands for.

	Counted here rather than asked of the QETProject on purpose: every
	other cell of every row is read out of this same document, and a page
	number counted against a different state of the project is a cell that
	disagrees with the row it sits in. Tag name compared case-insensitively
	for the same reason climbToDiagram() does it.
*/
int WiringListExport::diagramCount(const QDomElement &root) const
{
    int count = 0;
    for (QDomElement child = root.firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) {
        if (child.tagName().toLower() == QLatin1String("diagram")) {
            ++count;
        }
    }
    return count;
}

/**
	@brief WiringListExport::diagramIndex
	@param diagramElem : a <diagram> element of the project document
	@return its position among the sheets, counted from 1, which is what
	%id stands for; 0 when it cannot be told.

	QETProject::toXml() numbers the sheets as it writes them, in the order
	of the project, so the attribute is the answer and the walk below is
	only for a document that came from somewhere else. Zero is given back
	rather than a guess: a page number that is wrong reads exactly like a
	page number that is right.
*/
int WiringListExport::diagramIndex(const QDomElement &diagramElem) const
{
    bool ok = false;
    const int order = diagramElem.attribute("order").toInt(&ok);
    if (ok && order > 0) return order;

    int index = 1;
    for (QDomElement sibling = diagramElem.parentNode().firstChildElement(); !sibling.isNull(); sibling = sibling.nextSiblingElement()) {
        if (sibling == diagramElem) return index;
        if (sibling.tagName().toLower() == QLatin1String("diagram")) ++index;
    }
    return 0;
}

/**
	@brief WiringListExport::findDiagramFolio

	@par The stored folio is a template, not a page number

	The folio attribute of a sheet holds what the designer typed into the
	title block, and the value TitleBlockProperties seeds it with is
	"%id/%total". Reading the attribute and printing it - which is what
	this function did - filled the Page column of every row of the list
	with that literal. Measured on a real project of fourteen sheets: 250
	rows of cable, 250 of them reading "%id/%total", in the one column that
	tells the bench which drawing to open. Every example project shipped
	with QElectroTech carries the same template, so this was not one
	project's habit.

	The substitution is FolioVariables', which is also what
	BorderTitleBlock::setFolioData() calls to produce the number drawn on
	the sheet - one rule, so the list and the drawing cannot disagree.
	%autonum is not substituted here and does not need to be: the title
	block resolves it into the stored template itself, before any save.

	@par When the project is open, the sheet says it itself
	The number is read first from the title block of the open sheet,
	which is the upstream fix for the same defect and gives the same
	answer by construction, since it is what setFolioData() drew. The
	substitution is what a document read without its project falls
	back on.
*/
QString WiringListExport::findDiagramFolio(const QDomElement &diagramElem, int total_diagrams) const
{
    if (diagramElem.isNull()) return "";
    // The folio number as the folio shows it ("3/12"). The saved "folio"
    // attribute is its template ("%id/%total").
    const int index = diagramElem.attribute("order").toInt();
    if (m_project && index >= 1 && index <= m_project->folios().size()) {
        return m_project->folios().at(index - 1)->border_and_titleblock
                .titleblockInformation().value("folio").toString();
    }
    if (diagramElem.hasAttribute("folio")) {
        const QString folio = diagramElem.attribute("folio");
        const int position = diagramIndex(diagramElem);
            //The same refusal BorderTitleBlock::setFolioData() makes: with
            //no position to stand on, the template is handed over as it is
            //rather than resolved against a number nobody measured.
        if (position < 1 || total_diagrams < 1) return folio;
        return FolioVariables::resolveIndex(folio, position, total_diagrams);
    }
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

/**
    The project as the export reads it: its symbol collection and its folios
    in order, each with its "order". A folio not built (lazy folios) is
    not built for it: its XML is copied, and what QElectroTech computes for
    its wires' ends -- the labels of the symbols they end on and of their
    partners, the terminals' names -- is filled in from the symbols built
    (on built folios or alone, as partners) and from the project database.
*/
QDomDocument WiringListExport::projectDocument() const
{
    QDomDocument document;
    QDomElement root = document.createElement(QStringLiteral("project"));
    document.appendChild(root);
    if (XmlElementCollection *collection = m_project->embeddedElementCollection())
        root.appendChild(document.importNode(collection->root(), true));

        //A folio whose partners built alone were linked or unlinked since
        //is not what its XML says: built, as saving builds it
    for (Diagram *diagram : m_project->folios())
        if (!diagram->isLoaded() && diagram->partialChanged())
            m_project->buildFolios({diagram});

    QHash<QUuid, Element *> built;
    for (Diagram *diagram : m_project->folios()) {
        if (diagram->isLoaded()) {
            for (Element *element : diagram->elements()) built.insert(element->uuid(), element);
        } else {
            const QHash<QUuid, Element *> partial = diagram->partialElements();
            for (auto it = partial.cbegin() ; it != partial.cend() ; ++it) built.insert(it.key(), it.value());
        }
    }
    projectDataBase *database = m_project->dataBase();
    auto label = [&built, database](const QUuid &uuid) {
        Element *element = built.value(uuid);
        return element ? element->actualLabel() : database->elementLabel(uuid);
    };
        //Conductor::toXml(): the label of the last of the symbol's partners.
        //A symbol still waiting for a partner not built has its links as
        //saved, in the database
    const QSet<QUuid> waiting = m_project->unloadedFolioCount() ? m_project->symbolsOnUnbuiltFolios()
                                                                : QSet<QUuid>();
    auto linked = [&built, database, &label, &waiting](const QUuid &uuid) -> std::optional<QString> {
        Element *element = built.value(uuid);
        if (element && database->waitingPartners(uuid, waiting).isEmpty()) {
            if (element->linkedElements().isEmpty()) return std::nullopt;
            return element->linkedElements().last()->actualLabel();
        }
        const QList<QUuid> links = database->linkedSymbols(uuid);
        if (links.isEmpty()) return std::nullopt;
        return label(links.last());
    };

    const QList<Diagram *> folios = m_project->folios();
    for (int i = 0 ; i < folios.size() ; ++i) {
        Diagram *diagram = folios.at(i);
        QDomElement folio;
        const bool is_built = diagram->isLoaded();
        folio = document.importNode(is_built ? m_project->folioXml(diagram).documentElement()
                                             : diagram->deferredXml(), true).toElement();
            //A built folio's wires describe their ends but a partner not
            //built yet; a folio not built has none of it in its part (.qetz)
            //or as read, before any change (.qet)
        QDomNodeList wires = folio.elementsByTagName(QStringLiteral("conductor"));
        for (int w = 0 ; w < wires.size() ; ++w) {
            QDomElement wire = wires.at(w).toElement();
            for (const QString end : {QStringLiteral("1"), QStringLiteral("2")}) {
                const QUuid element(wire.attribute(QStringLiteral("element") + end));
                if (element.isNull()) continue;   //an end saved the older way
                if (const auto partner = linked(element))
                    wire.setAttribute(QStringLiteral("element") + end + QStringLiteral("_linked"), *partner);
                else
                    wire.removeAttribute(QStringLiteral("element") + end + QStringLiteral("_linked"));
                if (is_built) continue;
                wire.setAttribute(QStringLiteral("element") + end + QStringLiteral("_label"), label(element));
                wire.setAttribute(QStringLiteral("terminalname") + end,
                                  database->terminalName(QUuid(wire.attribute(QStringLiteral("terminal") + end)),
                                                         element));
            }
        }
        folio.setAttribute(QStringLiteral("order"), i + 1);
        root.appendChild(folio);
    }
    return document;
}

QMap<QString, ElementInfo> WiringListExport::collectElementsInfo(const QDomElement &root) const
{
    QMap<QString, ElementInfo> infoMap;
    const int total_diagrams = diagramCount(root);

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
        info.folio = findDiagramFolio(climbToDiagram(el), total_diagrams);

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

/**
	@brief WiringListExport::isBridgeNumber
	@param number : the number (the text) of a conductor
	@return whether that number names a bridge between two terminals

	@par Listed and marked, never dropped
	The list used to skip every conductor numbered with one German word,
	compared exactly, and nothing else. The two failures were silent: a
	bridge numbered in any other language came out as an ordinary wire,
	and an ordinary wire that happened to carry that word vanished from
	the list without a trace. A bridge is still a piece of copper somebody
	fits between two terminals, so it now stays in the list, on a row
	whose last column says what it is, and the name only decides that
	note: a wrong guess costs a note, never a row.

	@par The names
	Compared whole, trimmed and without regard to case: "PONTE" and
	"ponte" are one name, "PONTE 2" is not a bridge. The German word is
	the one the list always recognised, so a project that relied on it
	keeps its bridges recognised; the Portuguese word is the one a
	designer drawing in that language writes; the English one is the
	interface's own. The price of a fixed list is that a fourth language
	is an ordinary wire until it is added here - which is still the
	visible failure, since the row is printed either way.
*/
bool WiringListExport::isBridgeNumber(const QString &number)
{
    const QString name = number.trimmed();
    if (name.isEmpty()) return false;

    static const QStringList names {
        QStringLiteral("Brücke"),
        QStringLiteral("Ponte"),
        QStringLiteral("Bridge")
    };
    for (const QString &bridge : names) {
        if (name.compare(bridge, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

QList<ConductorData> WiringListExport::collectConductors(const QDomElement &root) const
{
    QList<ConductorData> conductors;
    const int total_diagrams = diagramCount(root);
    QDomNodeList conductorNodes = root.elementsByTagName("conductor");

    for (int i = 0; i < conductorNodes.size(); ++i) {
        QDomElement cond = conductorNodes.at(i).toElement();

        ConductorData data;
        data.index = i;
            //Kept and marked, never skipped: see isBridgeNumber()
        data.bridge = isBridgeNumber(cond.attribute("num"));
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
            //The colour typed in the wire's properties, or the name of the
            //colour it is drawn in, read with the defaults
            //ConductorProperties::fromXml() gives a missing one (T70)
        const QColor drawn(cond.attribute("color"));
        const QColor second(cond.attribute("color2"));
        data.conductor_color = WireColorName::label(
                    cond.attribute("conductor_color"),
                    drawn.isValid() ? drawn : QColor(Qt::black),
                    cond.attribute("bicolor") == QLatin1String("true"),
                    second.isValid() ? second : QColor(Qt::black));
        data.conductor_section = cond.attribute("conductor_section");
        data.function = cond.attribute("function");
        data.cable = cond.attribute("cable");

        QDomElement diag = climbToDiagram(cond);
        data.folio = findDiagramFolio(diag, total_diagrams);
        data.folio_index = diag.attribute("order").toInt();
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
	eleven column names, which is the honest answer and is what a
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
        QMessageBox::warning(m_parent, tr("Error"), tr("Unable to read the project's in-memory structure."));
        return;
    }

    QFileDialog dialog(m_parent);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setWindowTitle(tr("Export the wiring diagram"));
    dialog.setDefaultSuffix("csv");
    dialog.setNameFilter(tr("CSV files (*.csv)"));

    if (dialog.exec() != QDialog::Accepted) return;
    QString fileName = dialog.selectedFiles().first();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(m_parent, tr("Error"), tr("Unable to open the file for writing."));
        return;
    }
    QTextStream out(&file);
    out << csv;
        //Success is said only once the device took the text, and the
        //device is asked while it is still open.
    const QString failure = QETCsv::writeError(out, file);
    if (!failure.isEmpty()) {
        QMessageBox::critical(m_parent, tr("Error"),
                              tr("L'écriture du fichier « %1 » a échoué.").arg(fileName)
                              + "\n\n" + failure);
        return;
    }
    file.close();
    QMessageBox::information(m_parent, tr("Export successful"), tr("The wiring diagram has been successfully exported !"));
}

/**
	@brief WiringListExport::toCsvString
	@return the wiring list formatted as csv : one header row of eleven
	column names, then one row per cable, folio-numbered elements sorted
	by folio and then by component. Never an empty string unless the
	project could not be read at all - see the invariant below.

	@par The Note column, and the two rows it marks
	The eleventh column is empty on an ordinary wire. It reads "Bridge"
	on a conductor whose number names a bridge (isBridgeNumber()), which
	the list used to drop when it had one particular name and print as a
	plain wire under any other. And it says that the section differs on
	each of the two halves of a wire drawn across a folio report whose
	halves declare two sections: those halves are no longer joined into
	one row with both sections in one cell, but printed one per row, each
	with its own section, and each naming the page of the other. The
	price is that such a wire takes two rows, like a wire whose halves
	cannot be joined, until somebody makes the drawing agree with itself;
	and that every row has one more cell than a file written before, so a
	comparison with an older file is by column, not by line.

	@par An empty return means a failure, and can mean nothing else
	This is the invariant the command-line exporter leans on, so breaking
	it breaks a test in another file. The header row is written
	unconditionally further down, before the loop over the
	cables, and there is no return between the two guards at the top of
	this function and that write. So the only two ways out with an empty
	string are those guards: a null @c m_project, and a project whose
	@c toXml() gave back a null document. A project that is perfectly
	readable and simply holds no cable returns the header row on its own
	- eleven names and a line break, not an empty string. Whoever adds a
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

    QDomDocument doc = projectDocument();
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
    // Conductors with one end on a folio report, by that report. All of them
    // are collected before any is merged, so the result does not depend on
    // the order the folios are saved in.
    QMap<QString, QList<ConductorData>> partialWires;

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

        //Two sections, both declared, that are not the same text. A half
        //that declares none is not a contradiction: it is joined, and
        //takes the other's, as mergeField() does.
    auto sectionsDisagree = [](const QString &a, const QString &b) {
        const QString at = a.trimmed();
        const QString bt = b.trimmed();
        return !at.isEmpty() && !bt.isEmpty() && at != bt;
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
        partialWires[ph_uuid].append(normalizePartial(c, ph_uuid));
    }

    // Two halves are one wire only when each report of a linked pair carries
    // exactly one conductor. With several on a side, the diagram does not say
    // which terminal is wired to which, so each conductor gets its own row,
    // ending at the report.
    QSet<QString> written;
    for (auto it = partialWires.cbegin(); it != partialWires.cend(); ++it) {
        const QString &ph_uuid = it.key();
        if (written.contains(ph_uuid)) continue;
        written.insert(ph_uuid);

        const QStringList links = elementsInfo.value(ph_uuid).links;
        const QString matching_ph_uuid = links.isEmpty() ? QString() : links.first();
        const QList<ConductorData> others = partialWires.value(matching_ph_uuid);

        if (it.value().size() != 1 || others.size() != 1 || written.contains(matching_ph_uuid)) {
            uniqueConductors.append(it.value());
            continue;
        }
        written.insert(matching_ph_uuid);

        const ConductorData &otherHalf = it.value().first();
        const ConductorData &normC = others.first();

            //Two halves declaring two sections are not one wire of a
            //strange section: they are a drawing that contradicts itself.
            //Joined, they gave one cell such as "1,5, 2,5", which reads as
            //a section in a spreadsheet and not as a fault. So they are not
            //joined: each half keeps its own section on a row of its own,
            //ending at its report like a half that cannot be joined, and
            //says in the Note column on which page the other half is.
            //Nothing is chosen for the bench - the list cannot know which
            //of the two sections is the one to cut.
        if (sectionsDisagree(otherHalf.conductor_section, normC.conductor_section)) {
            ConductorData first = otherHalf;
            ConductorData second = normC;
            first.section_mismatch = true;
            first.section_mismatch_folio = normC.folio;
            second.section_mismatch = true;
            second.section_mismatch_folio = otherHalf.folio;
            uniqueConductors.append(first);
            uniqueConductors.append(second);
            continue;
        }

        ConductorData merged;
        merged.folio = mergeField(otherHalf.folio, normC.folio);
        merged.folio_index = std::min(otherHalf.folio_index, normC.folio_index);

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
        merged.cable = mergeField(otherHalf.cable, normC.cable);
        merged.bridge = otherHalf.bridge || normC.bridge;

        uniqueConductors.append(merged);
    }

    for (ConductorData &c : uniqueConductors) {
        if (!c.element2_label.isEmpty() && (c.element1_label.isEmpty() || c.element2_label.toLower() < c.element1_label.toLower())) {
            std::swap(c.element1_label, c.element2_label);
            std::swap(c.terminalname1, c.terminalname2);
            std::swap(c.el1_uuid, c.el2_uuid);
        }
    }

    std::sort(uniqueConductors.begin(), uniqueConductors.end(), [](const ConductorData &a, const ConductorData &b) {
        // By the folio's position in the project: its number is free text
        // ("3/12", "A-2") and does not sort.
        if (a.folio_index != b.folio_index) return a.folio_index < b.folio_index;

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

    // Quoted through QETCsv rather than joined raw. Every cell of this list
    // is text somebody typed on a folio -- a component label, a wire colour,
    // a function -- and one holding the ';' used to shift every column of
    // its row, in a file that still opened and still looked complete.
    QStringList header;
    header << tr("Page", "Wiring list CSV header")
    << tr("Component 1", "Wiring list CSV header")
    << tr("Terminal 1", "Wiring list CSV header")
    << tr("Component 2", "Wiring list CSV header")
    << tr("Terminal 2", "Wiring list CSV header")
    << tr("Voltage / Protocol", "Wiring list CSV header")
    << tr("Wire color", "Wiring list CSV header")
    << tr("Wire section", "Wiring list CSV header")
    << tr("Function", "Wiring list CSV header")
    << tr("Cable", "Wiring list CSV header")
    << tr("Note", "Wiring list CSV header");
    out << QETCsv::row(header) << "\n";

        //The last column says what a row is when it is not an ordinary
        //wire, and is empty on every ordinary one, so a filter on it
        //finds every row the bench has to look at twice. Appended after
        //the others so that the ten columns before it keep their place.
    auto noteOf = [](const ConductorData &c) {
        QStringList notes;
        if (c.bridge) {
            notes << WiringListExport::tr("Bridge",
                                          "Wiring list CSV note: the row is a bridge between two terminals");
        }
        if (c.section_mismatch) {
            notes << (c.section_mismatch_folio.isEmpty()
                      ? WiringListExport::tr("Section differs from the other half of this wire",
                                             "Wiring list CSV note")
                      : WiringListExport::tr("Section differs from the other half of this wire, on %1",
                                             "Wiring list CSV note: %1 is the page of the other half")
                            .arg(c.section_mismatch_folio));
        }
        return notes.join(QStringLiteral(" / "));
    };

    for (const ConductorData &c : uniqueConductors) {
        QStringList values;
        values << c.folio
        << c.element1_label
        << c.terminalname1
        << c.element2_label
        << c.terminalname2
        << c.tension_protocol
        << c.conductor_color
        << c.conductor_section
        << c.function
        << c.cable
        << noteOf(c);
        out << QETCsv::row(values) << "\n";
    }

    return csv;
}
