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
 *  Scheme class
 * ------------------------------------------
 *
 */
#include <boost/lexical_cast.hpp>

#include "../common.h"
#include "../Boundaries/CBoundaryMap.h"
#include "../Boundaries/CBoundary.h"
#include "CScheme.h"
#include "CSchemeGodunov.h"
#include "CSchemeMUSCLHancock.h"
#include "CSchemeInertial.h"
#include "../Domain/CDomain.h"
#include "../Domain/Cartesian/CDomainCartesian.h"
#include "../Datasets/CXMLDataset.h"
#include "../Datasets/CRasterDataset.h"

/*
 *  Default constructor
 */
CScheme::CScheme()
{
	pManager->log->writeLine( "Scheme base-class instantiated." );

	// Not ready by default
	this->bReady				= false;
	this->bRunning				= false;
	this->bThreadRunning		= false;
	this->bThreadTerminated		= false;

	this->bAutomaticQueue		= true;
	this->uiQueueAdditionSize	= 1;
	this->dCourantNumber		= 0.5;
	this->dTimestep				= 0.001;
	this->bDynamicTimestep		= true;
	this->bFrictionEffects		= true;
	this->dTargetTime			= 0.0;
	this->uiBatchSkipped		= 0;
	this->uiBatchSuccessful		= 0;
	this->dBatchTimesteps		= 0.0;
}

/*
 *  Destructor
 */
CScheme::~CScheme(void)
{
	pManager->log->writeLine( "The abstract scheme class was unloaded from memory." );
}

/*
 *  Read in settings from the XML configuration file for this scheme
 */
void	CScheme::setupFromConfig( XMLElement* pXScheme, bool bInheritanceChain )
{
	XMLElement		*pParameter		= pXScheme->FirstChildElement("parameter");
	char			*cParameterName = NULL, *cParameterValue = NULL;

	while ( pParameter != NULL )
	{
		Util::toLowercase( &cParameterName,  pParameter->Attribute( "name" ) );
		Util::toLowercase( &cParameterValue, pParameter->Attribute( "value" ) );

		if ( strcmp( cParameterName, "queuemode" ) == 0 )
		{ 
			unsigned char ucQueueMode = 255;
			if ( strcmp( cParameterValue, "auto" ) == 0 )
				ucQueueMode = model::queueMode::kAuto;
			if ( strcmp( cParameterValue, "fixed" ) == 0 )
				ucQueueMode = model::queueMode::kFixed;
			if ( ucQueueMode == 255 )
			{
				model::doError(
					"Invalid queue mode given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setQueueMode( ucQueueMode );
			}
		}
		else if ( strcmp( cParameterName, "queueinitialsize" )	== 0 ||
				  strcmp( cParameterName, "queuesize" )			== 0 ||
				  strcmp( cParameterName, "queuefixedsize" )	== 0 )
		{ 
			if ( !CXMLDataset::isValidUnsignedInt( cParameterValue ) )
			{
				model::doError(
					"Invalid queue size given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setQueueSize( boost::lexical_cast<unsigned int>( cParameterValue ) );
			}
		}

		pParameter = pParameter->NextSiblingElement("parameter");
	}
}

/*
 *  Ask the executor to create a type of scheme with the defined
 *  flags.
 */
CScheme* CScheme::createScheme( unsigned char ucType )
{
	switch( ucType )
	{
		case model::schemeTypes::kGodunov:
			return static_cast<CScheme*>( new CSchemeGodunov() );
		break;
		case model::schemeTypes::kMUSCLHancock:
			return static_cast<CScheme*>( new CSchemeMUSCLHancock() );
		break;
		case model::schemeTypes::kInertialSimplification:
			return static_cast<CScheme*>( new CSchemeInertial() );
		break;
	}

	return NULL;
}

/*
 *  Ask the executor to create a type of scheme with the defined
 *  flags.
 */
CScheme* CScheme::createFromConfig( XMLElement *pXScheme )
{
	char		*cSchemeName = NULL;
	CScheme		*pScheme	 = NULL;

	Util::toLowercase( &cSchemeName, pXScheme->Attribute( "name" ) );

	if ( strcmp( cSchemeName, "muscl-hancock" ) == 0 )
	{
		pManager->log->writeLine( "MUSCL-Hancock scheme specified for the domain." );
		pScheme	= CScheme::createScheme(
			model::schemeTypes::kMUSCLHancock
		);
	} else if ( strcmp( cSchemeName, "godunov" ) == 0 )
	{
		pManager->log->writeLine( "Godunov-type scheme specified for the domain." );
		pScheme	= CScheme::createScheme(
			model::schemeTypes::kGodunov
		);
	} else if ( strcmp( cSchemeName, "inertial" ) == 0 )
	{
		pManager->log->writeLine( "Partial-inertial scheme specified for the domain." );
		pScheme	= CScheme::createScheme(
			model::schemeTypes::kInertialSimplification
		);
	} else {
		model::doError(
			"Unsupported scheme specified for the domain.",
			model::errorCodes::kLevelWarning
		);
		return NULL;
	}

	return pScheme;
}

/*
 *  Simple check for if the scheme is ready to run
 */
bool	CScheme::isReady()
{
	return this->bReady;
}

/*
 *  Set the queue mode
 */
void	CScheme::setQueueMode( unsigned char ucQueueMode )
{
	this->bAutomaticQueue = ( ucQueueMode == model::queueMode::kAuto );
}

/*
 *  Get the queue mode
 */
unsigned char	CScheme::getQueueMode()
{
	return ( this->bAutomaticQueue ? model::queueMode::kAuto : model::queueMode::kFixed );
}

/*
 *  Set the queue size (or initial)
 */
void	CScheme::setQueueSize( unsigned int uiQueueSize )
{
	this->uiQueueAdditionSize = uiQueueSize;
}

/*
 *  Get the queue size (or initial)
 */
unsigned int	CScheme::getQueueSize()
{
	return this->uiQueueAdditionSize;
}

/*
 *  Set the Courant number
 */
void	CScheme::setCourantNumber( double dCourantNumber )
{
	this->dCourantNumber = dCourantNumber;
}

/*
 *  Get the Courant number
 */
double	CScheme::getCourantNumber()
{
	return this->dCourantNumber;
}

/*
 *  Set the timestep mode
 */
void	CScheme::setTimestepMode( unsigned char ucMode )
{
	this->bDynamicTimestep = ( ucMode == model::timestepMode::kCFL );
}

/*
 *  Get the timestep mode
 */
unsigned char	CScheme::getTimestepMode()
{
	return ( this->bDynamicTimestep ? model::timestepMode::kCFL : model::timestepMode::kFixed );
}

/*
 *  Set the timestep
 */
void	CScheme::setTimestep( double dTimestep )
{
	this->dTimestep = dTimestep;
}

/*
 *  Get the timestep
 */
double	CScheme::getTimestep()
{
	return fabs(this->dTimestep);
}

/*
 *  Enable/disable friction effects
 */
void	CScheme::setFrictionStatus( bool bEnabled )
{
	this->bFrictionEffects = bEnabled;
}

/*
 *  Get enabled/disabled for friction
 */
bool	CScheme::getFrictionStatus()
{
	return this->bFrictionEffects;
}

/*
 *  Set the target time
 */
void	CScheme::setTargetTime( double dTime )
{
	this->dTargetTime = dTime;
}

/*
 *  Get the target time
 */
double	CScheme::getTargetTime()
{
	return this->dTargetTime;
}

/*
 *	Are we in the middle of a batch?
 */
bool	CScheme::isRunning()
{
	return bRunning;
}