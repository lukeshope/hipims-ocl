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
 *  XML dataset handling class
 * ------------------------------------------
 *
 */
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <vector>

#include "../common.h"
#include "CXMLDataset.h"
#include "CCSVDataset.h"
#include "CRasterDataset.h"
#include "../MPI/CMPIManager.h"
#include "../Domain/CDomainManager.h"
#include "../Domain/CDomainBase.h"
#include "../Domain/CDomain.h"
#include "../Domain/Cartesian/CDomainCartesian.h"
#include "../Schemes/CScheme.h"
#include "../Schemes/CSchemeGodunov.h"
#include "../Schemes/CSchemeInertial.h"
#include "../Schemes/CSchemeMUSCLHancock.h"
#include "../Boundaries/CBoundaryMap.h"
#include "../Boundaries/CBoundary.h"

/*
 *  Default constructor
 */
CXMLDataset::CXMLDataset( std::string sFilename )
{
	this->pDocument		= new tinyxml2::XMLDocument;
	this->pDocument->LoadFile( sFilename.c_str() );
	
	// Check for errors
	this->bError = this->pDocument->ErrorID() != 0;

	if ( this->bError )
	{
		model::doError(
			"Unable to load/parse XML file.",
			model::errorCodes::kLevelWarning
		);
	} else {
		pManager->log->writeLine( "Successfully opened XML file for parsing." );
	}
}

/*
*  Alternative constructor
*/
CXMLDataset::CXMLDataset(const char* cFile)
{
	this->pDocument = new tinyxml2::XMLDocument;
	this->pDocument->Parse(cFile);

	this->bError = this->pDocument->ErrorID() != 0;

	if (this->bError)
	{
		model::doError(
			"Unable to load/parse XML file.",
			model::errorCodes::kLevelWarning
			);
	}
	else {
		pManager->log->writeLine("Successfully opened XML file for parsing.");
	}
}


/*
 *  Destructor
 */
CXMLDataset::~CXMLDataset(void)
{
	delete	this->pDocument;
}

/*
*  Get the text contents of the file
*/
const char*	CXMLDataset::getFileContents(void)
{
	tinyxml2::XMLPrinter printer;
	pDocument->Print(&printer);
	return printer.CStr();
}


/*
*  Get the text length of the file
*/
unsigned int	CXMLDataset::getFileLength(void)
{
	return std::strlen( this->getFileContents() );
}

/*
 *  Parse this document as a configuration file for the model
 */
bool	CXMLDataset::parseAsConfigFile(void)
{
	if ( this->bError ) return false;

	XMLElement*		pConfiguration = this->pDocument->FirstChildElement( "configuration" );
	if ( pConfiguration == NULL ) return false;

	pManager->log->writeLine( "Reading configuration: execution settings..." );
	if ( !this->parseExecution( pConfiguration ) ) return false;

	// Invoke MPI to send device details to all nodes if necessary
#ifdef MPI_ON
	if (pManager->getMPIManager() != NULL)
		pManager->getMPIManager()->exchangeDevices();
#endif

	pManager->log->writeLine( "Reading configuration: model metadata..." );
	if ( !this->parseMetadata( pConfiguration ) ) return false;

	pManager->log->writeLine( "Reading configuration: simulation settings..." );
	if ( !this->parseSimulation( pConfiguration ) ) return false;

	// TODO: Can this be moved to CModel?
	#ifdef _WINDLL
	for( unsigned int i = 0; i < pManager->getDomainSet()->getDomainCount(); ++i )
		if (pManager->getDomainSet()->isDomainLocal(i) )
			pManager->getDomainSet()->getDomain(i)->sendAllToRenderer();
	#endif

	return true;
}

/*
 *  Parse everything under the <execution> element
 */
bool	CXMLDataset::parseExecution( XMLElement *pConfiguration )
{
	XMLElement					*pTopElement, *pExecutorElement;
	char						*cExecutorName = NULL;
	CExecutorControl			*pExecutor;

	pTopElement			= pConfiguration->FirstChildElement( "execution" );
	if ( pTopElement == NULL )
	{
		model::doError(
			"Could not find execution configuration.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	pExecutorElement	= pTopElement->FirstChildElement( "executor" );
	if ( pExecutorElement == NULL )
	{
		model::doError(
			"The <executor> element is missing.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	pExecutor = CExecutorControl::createFromConfig( pExecutorElement );

	if (pExecutor == NULL)
		return false;

	pManager->setExecutor( pExecutor );

	return true;
}

/*
 *  Parse the core aspects under the <domain> element to dimension the domain
 */
bool	CXMLDataset::parseDomain( XMLElement *pSimulation )
{
	XMLElement					*pDomainSetElement;
	char						*cDomainTypeName = NULL;
	char						*cDomainDataDir = NULL;
	std::string					sDomainDataDir;

	pDomainSetElement			= pSimulation->FirstChildElement( "domainSet" );
	if ( pDomainSetElement == NULL )
	{
		model::doError(
			"Could not find domain set configuration.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	if ( !pManager->getDomainSet()->setupFromConfig( pDomainSetElement ) )
		return false;

	return true;
}

/*
 *  Parse everything under the <simulation> element
 */
bool	CXMLDataset::parseSimulation( XMLElement *pConfiguration )
{
	XMLElement					*pSimElement;
	char						*cSchemeName = NULL;
	CScheme						*pScheme	 = NULL;

	pSimElement			= pConfiguration->FirstChildElement( "simulation" );
	if ( pSimElement == NULL )
	{
		model::doError(
			"Could not find simulation configuration.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	if (pSimElement == NULL) return false;
	pManager->setupFromConfig( pSimElement );

	pManager->log->writeLine( "Reading configuration: domain data..." );

	if ( !this->parseDomain( pSimElement ) ) return false;

	return true;
}

/*
 *  Parse everything under the <metadata> element
 */
bool	CXMLDataset::parseMetadata( XMLElement *pConfiguration )
{
	XMLElement					*pTopElement, *pNameElement, *pDescriptionElement;

	pTopElement			= pConfiguration->FirstChildElement( "metadata" );
	if ( pTopElement == NULL )
		return false;

	pNameElement	= pTopElement->FirstChildElement( "name" );
	if ( pNameElement != NULL )
	{
		pManager->setName( std::string( pNameElement->GetText() ) );
	}

	pDescriptionElement	= pTopElement->FirstChildElement( "description" );
	if ( pDescriptionElement != NULL )
	{
		pManager->setDescription( std::string( pDescriptionElement->GetText() ) );
	}

	return true;
}

/*
 *  Is a string representing an unsigned int valid for attempted casting?
 */
bool	CXMLDataset::isValidUnsignedInt( char *cValue )
{
	for( unsigned int i = 0; i < std::strlen( cValue ); ++i )
	{
		if ( !std::isdigit( cValue[i] ) )
			return false;
	}
	return true;
}

/*
 *  Is a string representing an unsigned int valid for attempted casting?
 */
bool	CXMLDataset::isValidUnsignedInt( std::string sValue )
{
	// TODO: Improve regex to catch more errors
	return boost::regex_match( 
		sValue,
		boost::regex( "([0-9]+)" )
	);
}

/*
 *  Is a string representing an int valid for attempted casting?
 */
bool	CXMLDataset::isValidInt( char *cValue )
{
	// TODO: Improve regex to catch more errors
	return boost::regex_match( 
		std::string( cValue ),
		boost::regex( "([\\-0-9]+)" )
	);
}

/*
 *  Is a string representing a float valid for attempted casting?
 */
bool	CXMLDataset::isValidFloat( char *cValue )
{
	// TODO: Improve regex to catch more errors
	return boost::regex_match( 
		std::string( cValue ),
		boost::regex( "([\\.0-9-]+)" )
	);
}
