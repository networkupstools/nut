/* nutwriter.cpp - NUT writer

   Copyright (C)
        2012	Vaclav Krpec  <VaclavKrpec@Eaton.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "nutwriter.h"

#include <stdexcept>
#include <iostream>


namespace nut {

// End-of-Line separators (arch. dependent)

/** UNIX style EoL */
static const std::string LF("\n");

// TODO: Make a compile-time selection
#if (0)
// M$ Windows EoL
static const std::string CRLF("\r\n");

// Apple MAC EoL
static const std::string CR("\r");
#endif  // end of #if (0)


const std::string & NutWriter::eol(LF);


NutWriter::status_t NutWriter::writeEachLine(const std::string & str, const std::string & pref) {
	for (size_t pos = 0; pos < str.size(); ) {
		// Prefix every line
		status_t status = write(pref);

		if (NUTW_OK != status)
			return status;

		// Write up to the next EoL (or till the end)
		size_t eol_pos = str.find(eol, pos);

		if (str.npos == eol_pos)
			return write(str.substr(pos) + eol);

		eol_pos += eol.size();

		status = write(str.substr(pos, eol_pos));

		if (NUTW_OK != status)
			return status;

		// Update position
		pos = eol_pos;
	}

	return NUTW_OK;
}


NutWriter::status_t SectionlessConfigWriter::writeDirective(const std::string & str) {
	return write(str + eol);
}


NutWriter::status_t SectionlessConfigWriter::writeComment(const std::string & str) {
	return writeEachLine(str, "# ");
}


NutWriter::status_t SectionlessConfigWriter::writeSectionName(const std::string & name) {
	std::string e("INTERNAL ERROR: Attempt to write section name ");
	e += name + " to a section-less configuration file";

	throw std::logic_error(e);
}


NutWriter::status_t NutConfConfigWriter::writeConfig(const NutConfiguration & config) {
	// TODO

	throw std::logic_error("FIXME: Not implemented, yet");
}


NutWriter::status_t UpsmonConfigWriter::writeConfig(const UpsmonConfiguration & config) {
	// TODO

	throw std::logic_error("FIXME: Not implemented, yet");
}


NutWriter::status_t UpsdConfigWriter::writeConfig(const UpsdConfiguration & config) {
	// TODO

	throw std::logic_error("FIXME: Not implemented, yet");
}


NutWriter::status_t DefaultConfigWriter::writeComment(const std::string & str) {
	return writeEachLine(str, "# ");
}


NutWriter::status_t DefaultConfigWriter::writeSectionName(const std::string & name) {
	std::string section_line("[");
	section_line += name + "]" + eol;

	return write(section_line);
}


NutWriter::status_t DefaultConfigWriter::writeDirective(const std::string & str) {
	return write(str + eol);
}


NutWriter::status_t GenericConfigWriter::writeSection(const GenericConfigSection & section) {
	// TBD: Shouldn't this be somewhere else?
	// The parser has to use this, either...
	static const std::string value_separator(", ");

	status_t status;

	// Note that global scope definitions are in section
	// with an empty name
	// The section name won't be written and the assignments
	// won't be indented
	std::string indent;

	if (!section.name.empty()) {
		status = writeSectionName(section.name);

		if (NUTW_OK != status)
			return status;

		indent += "\t";
	}

	// Write section name/value pairs
	GenericConfigSection::EntryMap::const_iterator entry_iter = section.entries.begin();

	for (; entry_iter != section.entries.end(); ++entry_iter) {
		std::string assign(indent);
		assign += entry_iter->second.name + " = ";

		const ConfigParamList & values(entry_iter->second.values);

		ConfigParamList::const_iterator value_iter = values.begin();

		for (; value_iter != values.end(); ++value_iter) {
			if (value_iter != values.begin())
				assign += value_separator;

			assign += *value_iter;
		}

		status = writeDirective(assign);

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}


NutWriter::status_t GenericConfigWriter::writeConfig(const GenericConfiguration & config) {
	// Write sections
	// Note that lexicographic ordering places the global
	// (i.e. empty-name) section as the 1st one
	GenericConfiguration::SectionMap::const_iterator section_iter = config.sections.begin();

	for (; section_iter != config.sections.end(); ++section_iter) {
		status_t status = writeSection(section_iter->second);

		if (NUTW_OK != status)
			return status;

		// TBD: Write one empty line as section separator
		status = write(eol);

		if (NUTW_OK != status)
			return status;
	}

	return NUTW_OK;
}


}  // end of namespace nut
