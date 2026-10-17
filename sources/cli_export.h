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
		  qelectrotech --export-wiring  <project.qet> <output.csv>
		  qelectrotech --export-nets    <project.qet> <output.json>
		  qelectrotech --export-links   <project.qet> <output.csv>
		  qelectrotech --info           <project.qet> [output.json]
		  qelectrotech --check-elements <element.elmt | directory>
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
		cables: wiring list (one row per conductor) as CSV.
		wires: list of distinct wire numbers as CSV.
		bom: bill of materials (one row per element) as CSV.
		--no-slaves: leave the contact blocks (slave elements) out of the
		      bill of materials.
		--no-junctions: leave the junctions out of the bill of materials:
		      terminal-type elements with no label, designation,
		      manufacturer or manufacturer reference.
		purchase: the list a panel is ordered from, as CSV. One line per
		      catalogue part and revision, quantities summed, the
		      enclosures of the location tree (cabinet, door, mounting
		      plate) beside the drawn components. Three blocks, an empty
		      row between them: the counts first (pending, excluded,
		      nameless, lines), then the lines, then every item that could
		      not be put on a line, named with its reason - a component
		      with no part assigned is one. Exit code 0: complete; 3:
		      written, with pending items; 1: nothing written.
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
