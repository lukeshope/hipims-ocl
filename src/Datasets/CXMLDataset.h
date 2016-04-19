/*
 * ------------------------------------------
 *
 *  HIGH-PERFORMANCE INTEGRATED MODELLING SYSTEM (HiPIMS)
 *  Luke S. Smith and Qiuhua Liang
 *  luke@smith.ac
 *
 *  School of Civil Engineering & Geosciences
 *  Newcastle University
 * 
 * ------------------------------------------
 *  This code is licensed under GPLv3. See LICENCE
 *  for more information.
 * ------------------------------------------
 *  XML dataset handling functionality
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_DATASETS_CXMLDATASET_H_
#define HIPIMS_DATASETS_CXMLDATASET_H_

#include "../common.h"

/*
 *  XML DATASET CLASS
 *  CXMLDataset
 *
 *  Provides access for loading and saving(?)
 *  XML datasets using TinyXML-2
 */
class CXMLDataset
{

	public:

		CXMLDataset( std::string );																			// Constructor
		CXMLDataset( const char* );																			// Constructor
		~CXMLDataset( void );																				// Destructor

		// Public variables
		// ...

		// Public functions
		bool					parseAsConfigFile();														// Parse the document as a config file
		static bool				isValidUnsignedInt( char* );												// Safe for lexical_cast?
		static bool				isValidUnsignedInt( std::string );											// Safe for lexical_cast?
		static bool				isValidInt( char* );														// Safe for lexical_cast?
		static bool				isValidFloat( char* );														// Safe for lexical_cast?
		unsigned int			getFileLength();															// Get the file length
		const char*				getFileContents();															// Get the file contents

	private:

		// Private functions
		bool					parseMetadata( XMLElement* );												// Read the <metadata> tag
		bool					parseExecution( XMLElement* );												// Read the <execution> tag
		bool					parseSimulation( XMLElement* );												// Read the <simulation> tag
		bool					parseDomain( XMLElement* );													// Read the <domain> tag

		// Private variables
		bool					bError;																		// An error loading/parsing the file?
		std::string				sFilename;																	// XML filename
		tinyxml2::XMLDocument*	pDocument;																	// TinyXML-2 document

		// Friendships
		// ...

};

#endif
