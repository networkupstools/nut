/*
    nutwriter.hpp - NUT writer

    Copyright (C) 2012 Eaton

        Author: Vaclav Krpec  <VaclavKrpec@Eaton.com>

    Copyright (C) 2024-2025 NUT Community

        Author: Jim Klimov  <jimklimov+nut@gmail.com>

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

#ifndef nut_nutwriter_h
#define nut_nutwriter_h

#ifdef __cplusplus

#include "nutstream.hpp"
#include "nutconf.hpp"

#include <stdexcept>


namespace nut
{

/**
 *  \brief  NUT stream writer
 */
class NutWriter {
	public:

	/** NUT writer status */
	typedef enum {
		NUTW_OK = 0,  /** Writing successful */
		NUTW_ERROR,   /** Writing failed     */
	} status_t;  // end of typedef enum */

	protected:

	/** EoL separator */
	static const std::string & eol;

	/** Output stream (by reference) */
	NutStream & m_output_stream;

	public:

	/**
	 *  \brief  Constructor
	 *
	 *  Creates the writer.
	 *  The \c ostream parameter provides the writer reference
	 *  to an existing output stream; note that the stream
	 *  must exist throughout whole the writer's life.
	 *
	 *  TBD:
	 *  The stream might actually be passed either by value
	 *  (\c NutStream implementations would have to support
	 *  copying, though, which is not implemented at the moment)
	 *  or using reference counting mechanism (smart pointers etc).
	 *  The latter is perhaps a better choice (if the stream existence
	 *  dependency is a considerable issue).
	 *
	 *  \param  ostream  Output stream
	 */
	NutWriter(NutStream & ostream): m_output_stream(ostream) {}

	/**
	 *  \brief  Write to output stream
	 *
	 *  The method writes the provided string to the output stream.
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	inline status_t write(const std::string & str) {
		NutStream::status_t status = m_output_stream.putString(str);

		return NutStream::NUTS_OK == status ? NUTW_OK : NUTW_ERROR;
	}

	/**
	 *  \brief  Write to output stream
	 *
	 *  The method writes the provided string to the output stream.
	 *  An exception is thrown on error.
	 */
	inline void writex(const std::string & str) {
		NutStream::status_t status = m_output_stream.putString(str);

		if (NutStream::NUTS_OK != status) {
			std::stringstream e;
			e << "Failed to write to output stream: " << status;

			throw std::runtime_error(e.str());
		}
	}

	protected:

	/**
	 *  \brief  Write (prefixed) lines
	 *
	 *  The method splits string to lines (by EoL) and prefix them
	 *  with specified string upon writing.
	 *
	 *  \param  str   String (multi-line)
	 *  \param  pref  Prefix
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	status_t writeEachLine(const std::string & str, const std::string & pref);

};  // end of class NutWriter


/**
 *  \brief  NUT configuration writer interface (generic)
 */
class NutConfigWriter: public NutWriter {
	protected:

	/** Formal constructor */
	NutConfigWriter(NutStream & ostream): NutWriter(ostream) {}

	public:

	/**
	 *  \brief  Write comment
	 *
	 *  \param  str  Comment string
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	virtual status_t writeComment(const std::string & str) = 0;

	/**
	 *  \brief  Write section name
	 *
	 *  \param  name  Section name
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	virtual status_t writeSectionName(const std::string & name) = 0;

	/**
	 *  \brief  Write directive
	 *
	 *  \param  str  Directive string
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	virtual status_t writeDirective(const std::string & str) = 0;

	/** Virtual destructor */
	virtual ~NutConfigWriter();

};  // end of class NutConfigWriter


/**
 *  \brief  NUT section-less configuration writer specialization
 *
 *  Partial implementation of \ref NutConfigWriter for section-less
 *  configuration files.
 */
class SectionlessConfigWriter: public NutConfigWriter {
	protected:

	/**
	 *  \brief  Constructor
	 *
	 *  \param  ostream  Output stream
	 */
	SectionlessConfigWriter(NutStream & ostream): NutConfigWriter(ostream) {}

	public:

	// Partial \ref NutConfigWriter interface implementation
	status_t writeDirective(const std::string & str) override;
	status_t writeComment(const std::string & str) override;

	private:

	// Section name writing is forbidden (no sections)
	status_t writeSectionName(const std::string & name) override;

};  // end of class SectionlessConfigWriter


/**
 *  \brief  \c nut.conf configuration file serializer
 */
class NutConfConfigWriter: public SectionlessConfigWriter {
	public:

	/**
	 *  \brief  Constructor
	 *
	 *  \param  ostream  Output stream
	 */
	NutConfConfigWriter(NutStream & ostream): SectionlessConfigWriter(ostream) {}

	/**
	 *  \brief  Serialize configuration container
	 *
	 *  \param  config  Configuration
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	status_t writeConfig(const NutConfiguration & config);

	/* Ensure an out-of-line method to avoid "weak-vtables" warning */
	virtual ~NutConfConfigWriter() override;
};  // end of class NutConfConfigWriter


/**
 *  \brief  \c upsmon.conf configuration file serializer
 */
class UpsmonConfigWriter: public SectionlessConfigWriter {
	public:

	/**
	 *  \brief  Constructor
	 *
	 *  \param  ostream  Output stream
	 */
	UpsmonConfigWriter(NutStream & ostream): SectionlessConfigWriter(ostream) {}

	/**
	 *  \brief  Serialize configuration container
	 *
	 *  \param  config  Configuration
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	status_t writeConfig(const UpsmonConfiguration & config);

	/* Ensure an out-of-line method to avoid "weak-vtables" warning */
	virtual ~UpsmonConfigWriter() override;
};  // end of class UpsmonConfigWriter


/**
 *  \brief  \c upsd.conf configuration file serializer
 */
class UpsdConfigWriter: public SectionlessConfigWriter {
	public:

	/**
	 *  \brief  Constructor
	 *
	 *  \param  ostream  Output stream
	 */
	UpsdConfigWriter(NutStream & ostream): SectionlessConfigWriter(ostream) {}

	/**
	 *  \brief  Serialize configuration container
	 *
	 *  \param  config  Configuration
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	status_t writeConfig(const UpsdConfiguration & config);

	/* Ensure an out-of-line method to avoid "weak-vtables" warning */
	virtual ~UpsdConfigWriter() override;
};  // end of class UpsdConfigWriter


/**
 *  \brief  NUT default configuration writer ancestor
 *
 *  Implements the \ref NutConfigWriter interface
 *  and adds \c writeSection prototype to be implemented
 *  by descendants.
 */
class DefaultConfigWriter: public NutConfigWriter {
	protected:

	/**
	 *  \brief  Constructor
	 *
	 *  \param  ostream  Output stream
	 */
	DefaultConfigWriter(NutStream & ostream): NutConfigWriter(ostream) {}

	public:

	// \ref NutConfigWriter interface implementation
	status_t writeComment(const std::string & str) override;
	status_t writeSectionName(const std::string & name) override;
	status_t writeDirective(const std::string & str) override;

	/**
	 *  \brief  Write configuration section
	 *
	 *  Serialize generic configuration section.
	 *
	 *  \param  section  Configuration section
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	virtual status_t writeSection(const GenericConfigSection & section) = 0;

};  // end of class DefaultConfigWriter


/**
 *  \brief  NUT generic configuration writer
 *
 *  Base configuration file serializer.
 *  Implements the \ref DefaultConfigWriter \c writeSection method
 *  and adds \c writeConfig routine for configuration file serialization.
 */
class GenericConfigWriter: public DefaultConfigWriter {
	protected:

	/** Default indentation of the key/value pair in section entry */
	static const std::string s_default_section_entry_indent;

	/** Default separator of the key/value pair in section entry */
	static const std::string s_default_section_entry_separator;

	/**
	 *  \brief  Section entry serializer
	 *
	 *  \param  entry   Section entry
	 *  \param  indent  Indentation
	 *  \param  kv_sep  Key/value separator
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	status_t writeSectionEntry(
		const GenericConfigSectionEntry & entry,
		const std::string & indent = s_default_section_entry_indent,
		const std::string & kv_sep = s_default_section_entry_separator);

	public:

	/**
	 *  \brief  Constructor
	 *
	 *  \param  ostream  Output stream
	 */
	GenericConfigWriter(NutStream & ostream): DefaultConfigWriter(ostream) {}

	// Section serializer implementation
	status_t writeSection(const GenericConfigSection & section) override;

	/**
	 *  \brief  Base configuration serializer
	 *
	 *  \param  config  Base configuration
	 *
	 *  \retval NUTW_OK    on success
	 *  \retval NUTW_ERROR otherwise
	 */
	status_t writeConfig(const GenericConfiguration & config);

};  // end of class GenericConfigWriter


/**
 *  \brief  NUT upsd.users configuration file writer
 *
 *  upsd.users configuration file serializer.
 *  Overloads the generic section serializer because of the upsmon section,
 *  which contains an anomalous upsmon (master|slave) directive.
 */
class UpsdUsersConfigWriter: public GenericConfigWriter {
	public:

	/**
	 *  \brief  Constructor
	 *
	 *  \param  ostream  Output stream
	 */
	UpsdUsersConfigWriter(NutStream & ostream): GenericConfigWriter(ostream) {}

	// Section serializer overload
	status_t writeSection(const GenericConfigSection & section) override;

};  // end of class UpsdUsersConfigWriter

}  // end of namespace nut

#endif /* __cplusplus */

#endif /* end of #ifndef nut_nutwriter_h */
