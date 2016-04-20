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
 *  Domain Manager Class
 * ------------------------------------------
 *
 */
#include "../common.h"
#include "CDomainManager.h"
#include "CDomainBase.h"
#include "CDomain.h"
#include "Cartesian/CDomainCartesian.h"
#include "Links/CDomainLink.h"
#include "../Schemes/CScheme.h"
#include "../Datasets/CXMLDataset.h"
#include "../Datasets/CRasterDataset.h"
#include "../Boundaries/CBoundaryMap.h"
#include "../OpenCL/Executors/COCLDevice.h"
#include "../MPI/CMPIManager.h"
#include "../MPI/CMPINode.h"

/*
 *  Constructor
 */
CDomainManager::CDomainManager(void)
{
	this->ucSyncMethod = model::syncMethod::kSyncForecast;
	this->uiSyncSpareIterations = 3;
}

/*
*  Destructor
*/
CDomainManager::~CDomainManager(void)
{
	for (unsigned int uiID = 0; uiID < domains.size(); ++uiID)
		delete domains[uiID];

	pManager->log->writeLine("The domain manager has been unloaded.");
}

/*
 *  Set up the domain manager using the configuration file
 */
bool CDomainManager::setupFromConfig( XMLElement* pXNode )
{
	XMLElement*		pParameter			= pXNode->FirstChildElement( "parameter" );
	char			*cParameterName		= NULL;
	char			*cParameterValue	= NULL;
	char			*cSyncMethodName    = NULL;
	char			*cSyncSpareIterations = NULL;

	// Have we defined a synchronisation method for multi-domains?
	Util::toLowercase(&cSyncMethodName, pXNode->Attribute("syncMethod"));
	if (cSyncMethodName != NULL)
	{
		if (strcmp(cSyncMethodName, "timestep") == 0)
		{
			this->setSyncMethod(model::syncMethod::kSyncTimestep);
		}
		else if(strcmp(cSyncMethodName, "forecast") == 0)
		{
			this->setSyncMethod(model::syncMethod::kSyncForecast);
		}
		else 
		{
			model::doError(
				"Invalid synchronisation method given.",
				model::errorCodes::kLevelWarning
			);
		}
	}

	// Have we defined a number of spare iterations to aim for?
	Util::toLowercase(&cSyncSpareIterations, pXNode->Attribute("syncSpareSize"));
	if (cSyncSpareIterations != NULL)
	{
		if ( CXMLDataset::isValidUnsignedInt(cSyncSpareIterations) )
		{
			this->setSyncBatchSpares(boost::lexical_cast<unsigned int>(cSyncSpareIterations));
		}
		else
		{
			model::doError(
				"Invalid synchronisation spare buffer size given.",
				model::errorCodes::kLevelWarning
			);
		}
	}

	// TODO: Ditch <parameter> for the domainSet?
	/*
	while ( pParameter != NULL )
	{
		Util::toLowercase( &cParameterName, pParameter->Attribute( "name" ) );
		Util::toLowercase( &cParameterValue, pParameter->Attribute( "value" ) );

		if ( strcmp( cParameterName, "overlapcellcount" ) == 0 )
		{ 
			if ( !CXMLDataset::isValidFloat( cParameterValue ) )
			{
				model::doError(
					"Invalid simulation length given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				// TODO: Use this value to split domains automatically etc.
			}
		}
		else 
		{
			model::doError(
				"Unrecognised parameter: " + std::string( cParameterName ),
				model::errorCodes::kLevelWarning
			);
		}

		pParameter = pParameter->NextSiblingElement( "parameter" );
	}
	*/

	/*
	
	XMLElement*		pXData;

	// Device number(s) are now declared as part of the domain to allow for
	// split workloads etc.
	char			*cDomainDevice		= NULL;
	Util::toLowercase( &cDomainDevice, pXDomain->Attribute( "deviceNumber" ) );

	// TODO: For now we're only working with 1 device. If we need more than one device,
	// the domain manager must handle splitting...
	// TODO: Device selection/assignment should take place in the domain manager class.

	// Validate the device number
	if ( cDomainDevice == NULL )
	{
	// Select a device automatically
	model::doError(
	"Skeleton domain has no device number.",
	model::errorCodes::kLevelModelStop
	);
	} else {
	if ( CXMLDataset::isValidUnsignedInt( std::string(cDomainDevice) ) )
	{
	// TODO: Fix this...
	//pDevice = pManager->getExecutor()->getDevice( boost::lexical_cast<unsigned int>(cDomainDevice) );
	} else {
	model::doError(
	"The domain device specified is invalid.",
	model::errorCodes::kLevelWarning
	);
	}
	}
	
	*/

	// From here on we're more concerned with the actual domains
	XMLElement*		pXDomain			= pXNode->FirstChildElement( "domain" );
	char			*cDomainType		= NULL;
	char			*cDomainDevice		= NULL;

	while ( pXDomain != NULL )
	{
		Util::toLowercase( &cDomainType,   pXDomain->Attribute( "type" ) );
		Util::toLowercase( &cDomainDevice, pXDomain->Attribute( "deviceNumber" ) );

		if (strcmp(cDomainType, "cartesian") == 0)
		{
			CDomainBase* pDomainNew;

			// Do we have a valid device number?
			if (cDomainDevice == NULL)
			{
				// Select a device automatically
				model::doError(
					"Domain is missing device number.",
					model::errorCodes::kLevelModelStop
				);
				return false;
			}
			else {
				if (!CXMLDataset::isValidUnsignedInt(std::string(cDomainDevice)))
				{
					model::doError(
						"The domain device specified is invalid.",
						model::errorCodes::kLevelWarning
						);
				}
			}

			// Is the domain located on this node?
			unsigned int uiDeviceAdjust = 1;
#ifdef MPI_ON
			if ( !pManager->getMPIManager()->getNode()->isDeviceOnNode(boost::lexical_cast<unsigned int>(cDomainDevice)) )
			{
				// Domain lives somewhere else, so we only need a skeleton data structure
				pManager->log->writeLine("Creating a new skeleton domain for a remote node.");
				pDomainNew = CDomainBase::createDomain(model::domainStructureTypes::kStructureRemote);
			} else {
				// Adjust device number to be a local ID rather than across the whole MPI COMM
				uiDeviceAdjust = pManager->getMPIManager()->getNode()->getDeviceBaseID();
#endif
				// Domain resides on this node
				pManager->log->writeLine("Creating a new Cartesian-structured domain.");
				pDomainNew = CDomainBase::createDomain(model::domainStructureTypes::kStructureCartesian);
				pManager->log->writeLine("Local device IDs are relative to #" + toString(uiDeviceAdjust) + "." );
				pManager->log->writeLine("Assigning domain to device #" + toString(boost::lexical_cast<unsigned int>(cDomainDevice) - uiDeviceAdjust + 1) + "."  );
				static_cast<CDomain*>(pDomainNew)->setDevice(pManager->getExecutor()->getDevice(boost::lexical_cast<unsigned int>(cDomainDevice) - uiDeviceAdjust + 1));
#ifdef MPI_ON
			}
#endif

			if (!pDomainNew->configureDomain(pXDomain))
				return false;

			pDomainNew->setID( getDomainCount() );	// Should not be needed, but somehow is?
			domains.push_back( pDomainNew );
		}
		else 
		{
			model::doError(
				"Invalid domain type: " + std::string( cDomainType ),
				model::errorCodes::kLevelWarning
			);
			return false;
		}

		pXDomain = pXDomain->NextSiblingElement( "domain" );
	}

	// Warn the user if it's a multi-domain model, just so they know...
	if ( domains.size() <= 1 )
	{
		pManager->log->writeLine( "This is a SINGLE-DOMAIN model, limited to 1 device." );
	} else {
		pManager->log->writeLine( "This is a MULTI-DOMAIN model, and possibly multi-device." );
	}

#ifdef MPI_ON
	pManager->log->writeLine("Waiting for all nodes to finish loading domain data...");
	pManager->getMPIManager()->blockOnComm();
	pManager->log->writeLine("Proceeding to exchange domain data across MPI...");
	pManager->getMPIManager()->exchangeDomains();
#endif

	// Generate links
	this->generateLinks();

	// Spit out some details
	this->logDetails();

	// If we have more than one domain then we also need at least one link per domain
	// otherwise something is wrong...
	if (domains.size() > 1)
	{
		for (unsigned int i = 0; i < domains.size(); i++)
		{
			if (domains[i]->getLinkCount() < 1)
			{
				model::doError(
					"One or more domains are not linked.",
					model::errorCodes::kLevelModelStop
				);
				return false;
			}
		}
	}

	return true;
}

/*
 *  Add a new domain to the set
 */
CDomainBase*	CDomainManager::createNewDomain( unsigned char ucType )
{
	CDomainBase*	pNewDomain = CDomainBase::createDomain(ucType);
	unsigned int	uiID		= getDomainCount() + 1;

	domains.push_back( pNewDomain );
	pNewDomain->setID( uiID );

	pManager->log->writeLine( "A new domain has been created within the model." );

	return pNewDomain;
}

/*
*  Is a domain local to this node?
*/
bool	CDomainManager::isDomainLocal(unsigned int uiID)
{
	return !( domains[uiID]->isRemote() );
}

/*
*  Fetch a specific domain by ID
*/
CDomainBase*	CDomainManager::getDomainBase(unsigned int uiID)
{
	return domains[uiID];
}

/*
 *  Fetch a specific domain by ID
 */
CDomain*	CDomainManager::getDomain(unsigned int uiID)
{
	return static_cast<CDomain*>(domains[ uiID ]);
}

/*
 *  Fetch a specific domain by a point therein
 */
CDomain*	CDomainManager::getDomain(double dX, double dY)
{
	return NULL;
}

/*
 *  How many domains in the set?
 */
unsigned int	CDomainManager::getDomainCount()
{
	return domains.size();
}

/*
 *  Fetch the total extent of all the domains
 */
CDomainManager::Bounds		CDomainManager::getTotalExtent()
{
	CDomainManager::Bounds	b;
	// TODO: Calculate the total extent
	b.N = NULL;
	b.E = NULL;
	b.S = NULL;
	b.W = NULL;
	return b;
}

/*
 *  Write all of the domain data to disk
 */
void	CDomainManager::writeOutputs()
{
	for( unsigned int i = 0; i < domains.size(); i++ )
	{
		if (!domains[i]->isRemote())
		{
			getDomain(i)->writeOutputs();
		}
	}
}

/*
*	Fetch the current sync method being employed
*/
unsigned char CDomainManager::getSyncMethod()
{
	return this->ucSyncMethod;
}

/*
*	Set the sync method to being employed
*/
void CDomainManager::setSyncMethod(unsigned char ucMethod)
{
	this->ucSyncMethod = ucMethod;
}

/*
*	Fetch the number of spare batch iterations to aim for when forecast syncing
*/
unsigned int CDomainManager::getSyncBatchSpares()
{
	return this->uiSyncSpareIterations;
}

/*
*	Set the number of spare batch iterations to aim for when forecast syncing
*/
void CDomainManager::setSyncBatchSpares(unsigned int uiSpare)
{
	this->uiSyncSpareIterations = uiSpare;
}

/*
 *  Are all the domains contiguous?
 */
bool	CDomainManager::isSetContiguous()
{
	// TODO: Calculate this
	return true;
}

/*
 *  Are all the domains ready?
 */
bool	CDomainManager::isSetReady()
{
	// Is the domain ready?

	// Is the domain's scheme ready?

	// Is the domain's device ready?

	// TODO: Check this
	return true;
}

/*
 *	Generate links between domains where possible
 */
void	CDomainManager::generateLinks()
{
	pManager->log->writeLine("Generating link data for each domain");

	for (unsigned int i = 0; i < domains.size(); i++)
	{
		// Remove any pre-existing links
		domains[i]->clearLinks();
	}

	for (unsigned int i = 0; i < domains.size(); i++)
	{
		for (unsigned int j = 0; j < domains.size(); j++)
		{
			// Must overlap and meet our various constraints
			if (i != j && CDomainLink::canLink(domains[i], domains[j]))
			{
				// Make a new link...
				CDomainLink* pNewLink = new CDomainLink(domains[i], domains[j]);
				domains[i]->addLink(pNewLink);
				domains[j]->addDependentLink(pNewLink);
			}
		}
	}
}

/*
 *  Write some details to the console about our domain set
 */
void	CDomainManager::logDetails()
{
	pManager->log->writeDivide();
	unsigned short	wColour = model::cli::colourInfoBlock;

	pManager->log->writeLine("MODEL DOMAIN SET", true, wColour);
	pManager->log->writeLine("  Domain count:      " + toString(this->getDomainCount()), true, wColour);
	if (this->getDomainCount() <= 1)
	{
		pManager->log->writeLine("  Synchronisation:   Not required", true, wColour);
	}
	else {
		if (this->getSyncMethod() == model::syncMethod::kSyncForecast)
		{
			pManager->log->writeLine("  Synchronisation:   Domain-independent forecast", true, wColour);
			pManager->log->writeLine("    Forecast method: Aiming for " + toString(this->uiSyncSpareIterations) + " spare row(s)", true, wColour);
		}
		if (this->getSyncMethod() == model::syncMethod::kSyncTimestep)
			pManager->log->writeLine("  Synchronisation:   Explicit timestep exchange", true, wColour);
	}

	pManager->log->writeLine("", false, wColour);

	pManager->log->writeLine("+--------+------+--------+--------+--------+-------+-------+-------+", false, wColour);	
	pManager->log->writeLine("| Domain | Node | Device |  Rows  |  Cols  | Maths | Links | Resol |", false, wColour);	
	pManager->log->writeLine("+--------+------+--------+--------+--------+-------+-------+-------+", false, wColour);

	for (unsigned int i = 0; i < this->getDomainCount(); i++)
	{
		char cDomainLine[70] = "                                                                    X";

		CDomainBase::DomainSummary pSummary = this->getDomainBase(i)->getSummary();

		sprintf(
			cDomainLine,
			"| %6s | %4s | %6s | %6s | %6s | %5s | %5s | %5s |",
			toString(pSummary.uiDomainID + 1).c_str(),
#ifdef MPI_ON
			toString(pSummary.uiNodeID).c_str(),
#else
			"N/A",
#endif
			toString(pSummary.uiLocalDeviceID).c_str(),
			toString(pSummary.ulRowCount).c_str(),
			toString(pSummary.ulColCount).c_str(),
			(pSummary.ucFloatPrecision == model::floatPrecision::kSingle ? std::string("32bit") : std::string("64bit")).c_str(),
			toString(this->getDomainBase(i)->getLinkCount()).c_str(),
			toString(pSummary.dResolution).c_str()
		);

		pManager->log->writeLine(std::string(cDomainLine), false, wColour);	// 13
	}

	pManager->log->writeLine("+--------+------+--------+--------+--------+-------+-------+-------+", false, wColour);

	pManager->log->writeDivide();
}


