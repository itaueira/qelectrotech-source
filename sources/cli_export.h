/*
	Copyright 2006-2025 The QElectroTech Team
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
#ifndef CLI_EXPORT_H
#define CLI_EXPORT_H

#include <QStringList>

/**
	@brief Headless command-line export.

	Implements the long-requested batch/headless export
	(qelectrotech.org bugtracker #171, GitHub #309): render a project's
	diagrams to files without opening the GUI.

	Detected and handled in main() before the GUI is created.
*/
namespace CLIExport {

	/**
		@brief True if @p args request a CLI export
		(i.e. contain one of the export options).
	*/
	bool isExportRequest(const QStringList &args);

	/**
		@brief Run the CLI export described by @p args.
		@return process exit code (0 on success).

		Usage:
		  qelectrotech --export-pdf     <project.qet> <output.pdf> [folios=N,N] [--show-terminals]
		  qelectrotech --export-png     <project.qet> <output_dir> [--show-terminals]
		  qelectrotech --export-svg     <project.qet> <output_dir> [--show-terminals] [--svg-small-pictures]
		  qelectrotech --export-dxf     <project.qet> <output_dir> [--show-terminals] [--dxf-blocks] [--dxf-attributes]
		  qelectrotech --export-cables  <project.qet> <output.csv>
		  qelectrotech --export-wires   <project.qet> <output.csv>
		  qelectrotech --export-bom     <project.qet> <output.csv> [--no-slaves] [--no-junctions]
		  qelectrotech --export-purchase <project.qet> <output.csv>
		  qelectrotech --compare-purchase <before.csv> <after.csv> [report.csv]
		  qelectrotech --export-labels  <project.qet> <output.csv> [kinds=component,terminal]
		  qelectrotech --export-wiring  <project.qet> <output.csv>
		  qelectrotech --export-nets    <project.qet> <output.json>
		  qelectrotech --export-links   <project.qet> <output.csv>
		  qelectrotech --info           <project.qet> [output.json]
		  qelectrotech --check-elements <element.elmt | directory>
		  qelectrotech --check-project  <project.qet> [report.txt]
		  qelectrotech --resave         <project.qet> <output.qet>
		  qelectrotech --recover        <damaged.qetz> <output.qet>
		  qelectrotech --set-titleblock <project.qet> <output.qet> key=value...
		  qelectrotech --set-iec        <project.qet> <output.qet> <mode> [opts]

		PDF: one multi-page document (one diagram per page).  The optional
		     folios=N[,N...] narrows it to those folios and to that order
		     (1-based, as the sheet list numbers them); a folio outside
		     the project, a repeated one or a misspelled option is
		     refused rather than quietly ignored.
		PNG/SVG/DXF: one file per diagram, named <output_dir>/<NN>_<title>.<ext>.
		DXF: the same file the export dialog writes with its default options
		      (the export settings of the preferences).
		--show-terminals: also paint terminal markers (red stroke + blue
		      docking dot) and terminal names, as the interactive editor
		      does; off by default, matching the GUI export dialog's
		      default. For DXF, draws the terminal markers. Has no effect on
		      the list export modes.
		--dxf-blocks: write each symbol as a DXF block, placed with one
		      INSERT per use, so a CAD program selects it as one object.
		--dxf-attributes: as --dxf-blocks, and a symbol's texts (its label,
		      function...) are attributes of its INSERT. LibreCAD does not
		      show attributes.
		--svg-small-pictures: in an SVG, each picture at most four times the
		      size it is drawn at instead of its full resolution: a much
		      smaller file, still sharp when printed.
		cables: the from-to wiring list, as CSV: one row per conductor,
		      with its folio, the component and the terminal at each of
		      its two ends, voltage or protocol, colour, section,
		      function, cable and a note. Despite its name, it lists
		      wires, not cables.
		wires: the wire markers to print, as CSV: the number of each
		      numbered conductor that lands on a terminal, one row per
		      conductor, so a number carried by three wires is three
		      rows, and a wire drawn across two folios is two conductors
		      and two markers. Not a list of distinct numbers: the rows of
		      a number count the markers to print for it.
		bom: bill of materials (one row per element) as CSV.
		--no-slaves: leave the contact blocks (slave elements) out of the
		      bill of materials.
		--no-junctions: leave the junctions out of the bill of materials:
		      terminal-type elements with no label, designation,
		      manufacturer or manufacturer reference.
		purchase: the list a panel is ordered from, as CSV. One line per
		      catalogue part and revision, quantities summed, the
		      enclosures of the location tree (cabinet, door, mounting
		      plate) beside the drawn components, and the parts of the
		      mounting layout that no folio draws, one piece each. A part
		      of the layout whose symbol is on a folio is counted once,
		      from the symbol; rail and duct cut to length are counted by
		      the metre, one line per profile, and never by the piece (a
		      piece with no length is pending). Three blocks, an empty row
		      between them: the
		      counts first (pending, excluded, nameless, lines, mounted -
		      mounted being the pieces counted from the layout), then the
		      lines, then every item that could not be put on a line,
		      named with its reason - a component with no part assigned is
		      one, and so is a part of the layout with no code. Exit code
		      0: complete; 3: written, with pending items; 1: nothing
		      written.
		compare-purchase: what changed between two files written by
		      --export-purchase - the one kept from before a revision
		      and the one exported after it. Opens no project. The lines
		      are matched by part code and revision, never by their
		      designation, and each one that differs is listed: removed
		      (only in before.csv), added (only in after.csv) or changed
		      (in both, with another quantity or unit), with the
		      quantity and unit before and after. A part in a new
		      revision is one line removed and one added. A designation
		      or tags that changed alone is no difference. A length
		      reads with either decimal separator, so two lists exported
		      on machines set to different languages compare. The
		      report is CSV, as the export is (';', CRLF, UTF-8 without
		      a byte order mark): change, part_code, part_revision,
		      designation, quantity_before, unit_before,
		      quantity_after, unit_after; header only when nothing
		      differs. Without report.csv the rows go to stdout; a line
		      counting them follows on stdout either way. A file that
		      is not a purchase list written whole - no lines block, no
		      pending block, or fewer or more lines than its count says
		      - is refused, never compared. Exit code, as diff: 0: no
		      difference; 1: differences; 2: bad command line, a file
		      that cannot be read or is not a purchase list, or a
		      report that cannot be written or would overwrite one of
		      the two lists.
		labels: the labels of the project, one row per label, as CSV:
		      kind, text, lines, strip, position, level, location, folio,
		      revision, excluded_from_bom. The terminals come strip after
		      strip, each in the order of its rail and never sorted, with
		      the position the strip editor shows (counted from 1); the
		      components come by sheet. The lines under a tag are one
		      cell, a line feed between them. Cells quoted where needed,
		      ';' between them, CRLF between rows, UTF-8 with a byte order
		      mark. The strips are read from the project file: no window
		      has to have opened them. Exit code 0: written; 2: bad
		      option; 1: nothing written.
		kinds=component,terminal: which labels, in that order in the
		      file; both by default, the components first. A terminal
		      symbol on a strip is both, and comes out twice when both
		      are asked for. Wire, cable and door labels have no
		      collector yet and are refused.
		wiring: from-to wiring list (one row per conductor) as CSV, read from
		        the project database. Same logical list as `cables`, which
		        reads the document XML instead; the two are meant to agree,
		        so diffing them checks that the database still describes the
		        project.
		nets: electrical nets (connected-terminal groups) as JSON.
		links: element cross-references (coil/contact) as CSV, with
		       unresolved links flagged.
		info: structural project summary as JSON (stdout, or a file) —
		      per-page element / conductor counts and unconnected terminals.
		check-elements: validate .elmt file(s) against the element schema.
		      Symbol files only: it never opens a project, and a project
		      handed to it fails as a file that is not a symbol.
		check-project: run the design rule check on the project - the
		      same rules, with the same severities, as the check command
		      of the editor - and list what it found, one finding per
		      line, five fields separated by tabs: severity (error,
		      warning or information, never translated), folio (1-based;
		      empty for a finding about the project as a whole), place on
		      the folio, rule identifier, and the sentence. On stdout,
		      followed by a line counting them; with report.txt, the
		      findings alone go to that file (UTF-8, LF) and the count to
		      stdout. Exit code 0: checked, and nothing of severity error
		      (warnings do not fail); 1: at least one finding of severity
		      error; 2: bad option, or no such project; 3: no verdict -
		      the project could not be opened, no rule is switched on,
		      the report could not be written, or a rule could not run
		      and nothing of severity error was found.
		resave: load and rewrite the project XML (round-trip integrity).
		--ignore-lock: resave and set-titleblock leave a file alone when a lock
		      file beside it (.~<name>.lock, made by a QElectroTech that has
		      the project open) says someone else has it; this writes it
		      anyway. The verbs that only read a project never look at locks,
		      and none of them takes one. A lock made on this machine by a
		      process that is gone is stale and does not count.
		set-titleblock: stamp title-block fields onto every folio, then save.
		      Keys: title, author, date (or date=today), plant, location,
		      revision, version, filename; any other key becomes a custom
		      field.  E.g. --set-titleblock in.qet out.qet revision=B date=today
		set-iec: switch the IEC 81346 identification structure, then save.
		      Modes: off, short, context, full.  Option:
		      element-location=on|off.  Takes the same code path as the
		      settings dialog, so the saved file shows the labels exactly as
		      the drawing does after the change.
	*/
	int run(const QStringList &args);

}

#endif // CLI_EXPORT_H
