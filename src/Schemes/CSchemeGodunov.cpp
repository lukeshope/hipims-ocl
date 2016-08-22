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
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <algorithm>

#include "../common.h"
#include "../main.h"
#include "../Boundaries/CBoundaryMap.h"
#include "../Boundaries/CBoundary.h"
#include "../Domain/CDomainManager.h"
#include "../Domain/CDomain.h"
#include "../Domain/Links/CDomainLink.h"
#include "../Domain/Cartesian/CDomainCartesian.h"
#include "../Datasets/CXMLDataset.h"
#include "CSchemeGodunov.h"
#include "CSchemeMUSCLHancock.h"
#include "CSchemeInertial.h"

using std::min;
using std::max;
/*
 *  Default constructor
 */
CSchemeGodunov::CSchemeGodunov()
{
	// Scheme is loaded
	pManager->log->writeLine( "Godunov-type scheme loaded for execution on OpenCL platform." );

	// Default setup values
	this->bRunning						= false;
	this->bThreadRunning				= false;
	this->bThreadTerminated				= false;
	this->bDebugOutput					= false;
	this->uiDebugCellX					= 9999;
	this->uiDebugCellY					= 9999;

	this->dCurrentTime					= 0.0;
	this->dThresholdVerySmall			= 1E-10;
	this->dThresholdQuiteSmall			= this->dThresholdVerySmall * 10;
	this->bFrictionInFluxKernel			= true;
	this->bIncludeBoundaries			= false;
	this->uiTimestepReductionWavefronts = 200;

	this->ucSolverType					= model::solverTypes::kHLLC;
	this->ucConfiguration				= model::schemeConfigurations::godunovType::kCacheNone;
	this->ucCacheConstraints			= model::cacheConstraints::godunovType::kCacheActualSize;

	this->ulCachedWorkgroupSizeX		= 0;
	this->ulCachedWorkgroupSizeY		= 0;
	this->ulNonCachedWorkgroupSizeX		= 0;
	this->ulNonCachedWorkgroupSizeY		= 0;

	this->dBoundaryTimeSeries			= NULL;
	this->fBoundaryTimeSeries			= NULL;
	this->uiBoundaryParameters			= NULL;
	this->ulBoundaryRelationCells		= NULL;
	this->uiBoundaryRelationSeries		= NULL;

	// Default null values for OpenCL objects
	oclModel							= NULL;
	oclKernelFullTimestep				= NULL;
	oclKernelFriction					= NULL;
	oclKernelTimestepReduction			= NULL;
	oclKernelTimeAdvance				= NULL;
	oclKernelResetCounters				= NULL;
	oclKernelTimestepUpdate				= NULL;
	oclBufferCellStates					= NULL;
	oclBufferCellStatesAlt				= NULL;
	oclBufferCellManning				= NULL;
	oclBufferCellBed					= NULL;
	oclBufferTimestep					= NULL;
	oclBufferTimestepReduction			= NULL;
	oclBufferTime						= NULL;
	oclBufferTimeTarget					= NULL;
	oclBufferTimeHydrological			= NULL;

	if ( this->bDebugOutput )
		model::doError( "Debug mode is enabled!", model::errorCodes::kLevelWarning );

	pManager->log->writeLine( "Populated scheme with default settings." );
}

/*
 *  Destructor
 */
CSchemeGodunov::~CSchemeGodunov(void)
{
	this->releaseResources();
	pManager->log->writeLine( "The Godunov scheme class was unloaded from memory." );
}

/*
 *  Read in settings from the XML configuration file for this scheme
 */
void	CSchemeGodunov::setupFromConfig( XMLElement* pXScheme, bool bInheritanceChain )
{
	// Call the base class function which handles a couple of things
	CScheme::setupFromConfig( pXScheme, true );

	// Read the parameters
	XMLElement		*pParameter		= pXScheme->FirstChildElement("parameter");
	char			*cParameterName = NULL, *cParameterValue = NULL;

	while ( pParameter != NULL )
	{
		Util::toLowercase( &cParameterName,  pParameter->Attribute( "name" ) );
		Util::toLowercase( &cParameterValue, pParameter->Attribute( "value" ) );

		// These parameter apply to Godunov and all its child classes
		if ( strcmp( cParameterName, "courantnumber" ) == 0 )
		{ 
			if ( !CXMLDataset::isValidFloat( cParameterValue ) )
			{
				model::doError(
					"Invalid Courant number given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setCourantNumber( boost::lexical_cast<double>( cParameterValue ) );
			}
		}
		else if ( strcmp( cParameterName, "drythreshold" ) == 0 )
		{ 
			if ( !CXMLDataset::isValidFloat( cParameterValue ) )
			{
				model::doError(
					"Invalid dry threshold depth given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setDryThreshold( boost::lexical_cast<double>( cParameterValue ) );
			}
		}
		else if ( strcmp( cParameterName, "timestepmode" ) == 0 )
		{ 
			unsigned char ucTimestepMode = 255;
			if ( strcmp( cParameterValue, "auto" ) == 0 ||
					strcmp( cParameterValue, "cfl" ) == 0 )
				ucTimestepMode = model::timestepMode::kCFL;
			if ( strcmp( cParameterValue, "fixed" ) == 0 )
				ucTimestepMode = model::timestepMode::kFixed;
			if ( ucTimestepMode == 255 )
			{
				model::doError(
					"Invalid timestep mode given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setTimestepMode( ucTimestepMode );
			}
		}
		else if ( strcmp( cParameterName, "timestepinitial" ) == 0 ||
					strcmp( cParameterName, "timestepfixed" ) == 0 )
		{ 
			if ( !CXMLDataset::isValidFloat( cParameterValue ) )
			{
				model::doError(
					"Invalid initial/fixed timestep given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setTimestep( boost::lexical_cast<double>( cParameterValue ) );
			}
		}
		else if ( strcmp( cParameterName, "timestepreductiondivisions" ) == 0 )
		{ 
			if ( !CXMLDataset::isValidUnsignedInt( cParameterValue ) )
			{
				model::doError(
					"Invalid reduction divisions given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setReductionWavefronts( boost::lexical_cast<unsigned int>( cParameterValue ) );
			}
		}
		else if ( strcmp( cParameterName, "frictioneffects" ) == 0 )
		{ 
			unsigned char ucFriction = 255;
			if ( strcmp( cParameterValue, "yes" ) == 0 )
				ucFriction = 1;
			if ( strcmp( cParameterValue, "no" ) == 0 )
				ucFriction = 0;
			if ( ucFriction == 255 )
			{
				model::doError(
					"Invalid friction state given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setFrictionStatus( ucFriction == 1 );
			}
		}
		else if ( strcmp( cParameterName, "riemannsolver" ) == 0 )
		{ 
			unsigned char ucSolver = 255;
			if ( strcmp( cParameterValue, "hllc" ) == 0 )
				ucSolver = model::solverTypes::kHLLC;
			if ( ucSolver == 255 )
			{
				model::doError(
					"Invalid Riemann solver given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setRiemannSolver( ucSolver );
			}
		}
		else if ( strcmp( cParameterName, "groupsize" ) == 0 )
		{
			std::string sParameterValue = std::string( cParameterValue );
			std::vector<std::string> sSizes;
			boost::split(sSizes, sParameterValue, boost::is_any_of("x"));
			if ( (   sSizes.size() == 1 &&   !CXMLDataset::isValidUnsignedInt( sSizes[0] ) ) ||
				   ( sSizes.size() == 2 && ( !CXMLDataset::isValidUnsignedInt( sSizes[0] )   || !CXMLDataset::isValidUnsignedInt( sSizes[1] ) ) ) ||
				     sSizes.size() > 2 )
			{
				model::doError(
					"Invalid group size given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				if ( sSizes.size() == 1 )
				{
					this->setCachedWorkgroupSize( boost::lexical_cast<unsigned int>( sSizes[0] ) );
					this->setNonCachedWorkgroupSize( boost::lexical_cast<unsigned int>( sSizes[0] ) );
				} else {
					this->setCachedWorkgroupSize( boost::lexical_cast<unsigned int>( sSizes[0] ), boost::lexical_cast<unsigned int>( sSizes[1] ) );
					this->setNonCachedWorkgroupSize( boost::lexical_cast<unsigned int>( sSizes[0] ), boost::lexical_cast<unsigned int>( sSizes[1] ) );
				}
			}
		}
		else if ( strcmp( cParameterName, "cachedgroupsize" ) == 0 )
		{ 
			std::string sParameterValue = std::string( cParameterValue );
			std::vector<std::string> sSizes;
			boost::split(sSizes, sParameterValue, boost::is_any_of("x"));
			if ( (   sSizes.size() == 1 &&   !CXMLDataset::isValidUnsignedInt( sSizes[0] ) ) ||
				   ( sSizes.size() == 2 && ( !CXMLDataset::isValidUnsignedInt( sSizes[0] )   || !CXMLDataset::isValidUnsignedInt( sSizes[1] ) ) ) ||
				     sSizes.size() > 2 )
			{
				model::doError(
					"Invalid cached group size given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				if ( sSizes.size() == 1 )
				{
					this->setCachedWorkgroupSize( boost::lexical_cast<unsigned int>( sSizes[0] ) );
				} else {
					this->setCachedWorkgroupSize( boost::lexical_cast<unsigned int>( sSizes[0] ), boost::lexical_cast<unsigned int>( sSizes[1] ) );
				}
			}
		}
		else if ( strcmp( cParameterName, "noncachedgroupsize" ) == 0 )
		{ 
			std::string sParameterValue = std::string( cParameterValue );
			std::vector<std::string> sSizes;
			boost::split(sSizes, sParameterValue, boost::is_any_of("x"));
			if ( (   sSizes.size() == 1 &&   !CXMLDataset::isValidUnsignedInt( sSizes[0] ) ) ||
				   ( sSizes.size() == 2 && ( !CXMLDataset::isValidUnsignedInt( sSizes[0] )   || !CXMLDataset::isValidUnsignedInt( sSizes[1] ) ) ) ||
				     sSizes.size() > 2 )
			{
				model::doError(
					"Invalid non-cached group size given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				if ( sSizes.size() == 1 )
				{
					this->setNonCachedWorkgroupSize( boost::lexical_cast<unsigned int>( sSizes[0] ) );
				} else {
					this->setNonCachedWorkgroupSize( boost::lexical_cast<unsigned int>( sSizes[0] ), boost::lexical_cast<unsigned int>( sSizes[1] ) );
				}
			}
		}

		if ( !bInheritanceChain )
		{
			if ( strcmp( cParameterName, "localcachelevel" ) == 0 )
			{ 
				unsigned char usCache = 255;
				if ( strcmp( cParameterValue, "maximum" ) == 0 || strcmp( cParameterValue, "max" ) == 0 || strcmp( cParameterValue, "enabled" ) == 0 )
					usCache = model::schemeConfigurations::godunovType::kCacheEnabled;
				if ( strcmp( cParameterValue, "none" ) == 0 || strcmp( cParameterValue, "no" ) == 0 )
					usCache = model::schemeConfigurations::godunovType::kCacheNone;
				if ( usCache == 255 )
				{
					model::doError(
						"Invalid cache level given.",
						model::errorCodes::kLevelWarning
					);
				} else {
					this->setCacheMode( usCache );
				}
			}
			else if ( strcmp( cParameterName, "localcacheconstraints" ) == 0 )
			{ 
				unsigned char ucCacheConstraints = 255;
				if ( strcmp( cParameterValue, "none" ) == 0 || strcmp( cParameterValue, "normal" ) == 0 || strcmp( cParameterValue, "actual" ) == 0 )
					ucCacheConstraints = model::cacheConstraints::godunovType::kCacheActualSize;
				if ( strcmp( cParameterValue, "larger" ) == 0 || strcmp( cParameterValue, "oversized" ) == 0 )
					ucCacheConstraints = model::cacheConstraints::godunovType::kCacheAllowOversize;
				if ( strcmp( cParameterValue, "smaller" ) == 0 || strcmp( cParameterValue, "undersized" ) == 0 )
					ucCacheConstraints = model::cacheConstraints::godunovType::kCacheAllowUndersize;
				if ( ucCacheConstraints == 255 )
				{
					model::doError(
						"Invalid cache constraints given.",
						model::errorCodes::kLevelWarning
					);
				} else {
					this->setCacheConstraints( ucCacheConstraints );
				}
			}
		}

		pParameter = pParameter->NextSiblingElement("parameter");
	}
}

/*
 *  Log the details and properties of this scheme instance.
 */
void CSchemeGodunov::logDetails()
{
	pManager->log->writeDivide();
	unsigned short wColour = model::cli::colourInfoBlock;

	std::string sSolver			= "Undefined";
	switch( this->ucSolverType )
	{
	case model::solverTypes::kHLLC:
			sSolver = "HLLC (Approximate)";
		break;
	}

	std::string sConfiguration	= "Undefined";
	switch( this->ucConfiguration )
	{
	case model::schemeConfigurations::godunovType::kCacheNone:
			sConfiguration = "No local caching";
		break;
	case model::schemeConfigurations::godunovType::kCacheEnabled:
			sConfiguration = "Original state caching";
		break;
	}

	pManager->log->writeLine( "GODUNOV-TYPE 1ST-ORDER-ACCURATE SCHEME", true, wColour );
	pManager->log->writeLine( "  Timestep mode:      " + (std::string)( this->bDynamicTimestep ? "Dynamic" : "Fixed" ), true, wColour );
	pManager->log->writeLine( "  Courant number:     " + (std::string)( this->bDynamicTimestep ? toString( this->dCourantNumber ) : "N/A" ), true, wColour );
	pManager->log->writeLine( "  Initial timestep:   " + Util::secondsToTime( this->dTimestep ), true, wColour );
	pManager->log->writeLine( "  Data reduction:     " + toString(this->uiTimestepReductionWavefronts) + " divisions", true, wColour);
	pManager->log->writeLine( "  Boundaries:         " + toString(this->pDomain->getBoundaries()->getBoundaryCount()), true, wColour);
	pManager->log->writeLine( "  Riemann solver:     " + sSolver, true, wColour );
	pManager->log->writeLine( "  Configuration:      " + sConfiguration, true, wColour );
	pManager->log->writeLine( "  Friction effects:   " + (std::string)( this->bFrictionEffects ? "Enabled" : "Disabled" ), true, wColour );
	pManager->log->writeLine( "  Kernel queue mode:  " + (std::string)( this->bAutomaticQueue ? "Automatic" : "Fixed size" ), true, wColour );
	pManager->log->writeLine( (std::string)( this->bAutomaticQueue ? "  Initial queue:      " : "  Fixed queue:        " ) + toString( this->uiQueueAdditionSize ) + " iteration(s)", true, wColour );
	pManager->log->writeLine( "  Debug output:       " + (std::string)( this->bDebugOutput ? "Enabled" : "Disabled" ), true, wColour );

	pManager->log->writeDivide();
}

/*
 *  Run all preparation steps
 */
void CSchemeGodunov::prepareAll()
{
	pManager->log->writeLine( "Starting to prepare program for Godunov-type scheme." );

	this->releaseResources();

	oclModel = new COCLProgram(
		pManager->getExecutor(),
		this->pDomain->getDevice()
	);

	// Run-time tracking values
	this->ulCurrentCellsCalculated		= 0;
	this->dCurrentTimestep				= this->dTimestep;
	this->dCurrentTime					= 0;

	// Forcing single precision?
	this->oclModel->setForcedSinglePrecision( pManager->getFloatPrecision() == model::floatPrecision::kSingle );

	// OpenCL elements
	if ( !this->prepare1OExecDimensions() ) 
	{ 
		model::doError(
			"Failed to dimension task. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}

	if ( !this->prepare1OConstants() ) 
	{ 
		model::doError(
			"Failed to allocate constants. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}

	if ( !this->prepareCode() ) 
	{ 
		model::doError(
			"Failed to prepare model codebase. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}

	if ( !this->prepare1OMemory() ) 
	{ 
		model::doError(
			"Failed to create memory buffers. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}

	if ( !this->prepareGeneralKernels() ) 
	{ 
		model::doError(
			"Failed to prepare general kernels. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}

	if ( !this->prepare1OKernels() ) 
	{ 
		model::doError(
			"Failed to prepare kernels. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}

	if (!this->prepareBoundaries())
	{
		model::doError(
			"Failed to prepare boundaries. Cannot continue.",
			model::errorCodes::kLevelModelStop
			);
		this->releaseResources();
		return;
	}

	this->logDetails();
	this->bReady = true;
}

/*
 *  Concatenate together the code for the different elements required
 */
bool CSchemeGodunov::prepareCode()
{
	bool bReturnState = true;

	oclModel->appendCodeFromResource( "CLDomainCartesian_H" );
	oclModel->appendCodeFromResource( "CLFriction_H" );
	oclModel->appendCodeFromResource( "CLSolverHLLC_H" );
	oclModel->appendCodeFromResource( "CLDynamicTimestep_H" );
	oclModel->appendCodeFromResource( "CLSchemeGodunov_H" );
	oclModel->appendCodeFromResource( "CLBoundaries_H" );

	oclModel->appendCodeFromResource( "CLDomainCartesian_C" );
	oclModel->appendCodeFromResource( "CLFriction_C" );
	oclModel->appendCodeFromResource( "CLSolverHLLC_C" );
	oclModel->appendCodeFromResource( "CLDynamicTimestep_C" );
	oclModel->appendCodeFromResource( "CLSchemeGodunov_C" );
	oclModel->appendCodeFromResource( "CLBoundaries_C" );

	bReturnState = oclModel->compileProgram();

	return bReturnState;
}

/*
 *  Create boundary data arrays etc.
 */
bool CSchemeGodunov::prepareBoundaries()
{
	CBoundaryMap*	pBoundaries = this->pDomain->getBoundaries();
	pBoundaries->prepareBoundaries( oclModel, oclBufferCellBed, oclBufferCellManning, oclBufferTime, oclBufferTimeHydrological, oclBufferTimestep );

	return true;
}

/*
 *  Set the dry cell threshold depth
 */
void	CSchemeGodunov::setDryThreshold( double dThresholdDepth )
{
	this->dThresholdVerySmall = dThresholdDepth;
	this->dThresholdQuiteSmall = dThresholdDepth * 10;
}

/*
 *  Get the dry cell threshold depth
 */
double	CSchemeGodunov::getDryThreshold()
{
	return this->dThresholdVerySmall;
}

/*
 *  Set number of wavefronts used in reductions
 */
void	CSchemeGodunov::setReductionWavefronts( unsigned int uiWavefronts )
{
	this->uiTimestepReductionWavefronts = uiWavefronts;
}

/*
 *  Get number of wavefronts used in reductions
 */
unsigned int	CSchemeGodunov::getReductionWavefronts()
{
	return this->uiTimestepReductionWavefronts;
}

/*
 *  Set the Riemann solver to use
 */
void	CSchemeGodunov::setRiemannSolver( unsigned char ucRiemannSolver )
{
	this->ucSolverType = ucRiemannSolver;
}

/*
 *  Get the Riemann solver in use
 */
unsigned char	CSchemeGodunov::getRiemannSolver()
{
	return this->ucSolverType;
}

/*
 *  Set the cache configuration to use
 */
void	CSchemeGodunov::setCacheMode( unsigned char ucCacheMode )
{
	this->ucConfiguration = ucCacheMode;
}

/*
 *  Get the cache configuration in use
 */
unsigned char	CSchemeGodunov::getCacheMode()
{
	return this->ucConfiguration;
}

/*
 *  Set the cache size
 */
void	CSchemeGodunov::setCachedWorkgroupSize( unsigned char ucSize ) 
	{ this->ulCachedWorkgroupSizeX = ucSize; this->ulCachedWorkgroupSizeY = ucSize; }
void	CSchemeGodunov::setCachedWorkgroupSize( unsigned char ucSizeX, unsigned char ucSizeY ) 
	{ this->ulCachedWorkgroupSizeX = ucSizeX; this->ulCachedWorkgroupSizeY = ucSizeY; }
void	CSchemeGodunov::setNonCachedWorkgroupSize( unsigned char ucSize ) 
	{ this->ulNonCachedWorkgroupSizeX = ucSize; this->ulNonCachedWorkgroupSizeY = ucSize; }
void	CSchemeGodunov::setNonCachedWorkgroupSize( unsigned char ucSizeX, unsigned char ucSizeY ) 
	{ this->ulNonCachedWorkgroupSizeX = ucSizeX; this->ulNonCachedWorkgroupSizeY = ucSizeY; }

/*
 *  Set the cache constraints
 */
void	CSchemeGodunov::setCacheConstraints( unsigned char ucCacheConstraints )
{
	this->ucCacheConstraints = ucCacheConstraints;
}

/*
 *  Get the cache constraints
 */
unsigned char	CSchemeGodunov::getCacheConstraints()
{
	return this->ucCacheConstraints;
}

/*
 *  Calculate the dimensions for executing the problems (e.g. reduction glob/local sizes)
 */
bool CSchemeGodunov::prepare1OExecDimensions()
{
	bool						bReturnState = true;
	CExecutorControlOpenCL*		pExecutor	 = pManager->getExecutor();
	COCLDevice*		pDevice		 = pExecutor->getDevice();
	CDomainCartesian*			pDomain		 = static_cast<CDomainCartesian*>( this->pDomain );

	// --
	// Maximum permissible work-group dimensions for this device
	// --

	cl_ulong	ulConstraintWGTotal = (cl_ulong)floor( sqrt( static_cast<double> ( pDevice->clDeviceMaxWorkGroupSize ) ) );
	cl_ulong	ulConstraintWGDim   = min( pDevice->clDeviceMaxWorkItemSizes[0], pDevice->clDeviceMaxWorkItemSizes[1] );
	cl_ulong	ulConstraintWG		= min( ulConstraintWGDim, ulConstraintWGTotal );

	// --
	// Main scheme kernels with/without caching (2D)
	// --

	if ( this->ulNonCachedWorkgroupSizeX == 0 )
		ulNonCachedWorkgroupSizeX = ulConstraintWG;
	if ( this->ulNonCachedWorkgroupSizeY == 0 )
		ulNonCachedWorkgroupSizeY = ulConstraintWG;

	ulNonCachedGlobalSizeX	= pDomain->getCols();
	ulNonCachedGlobalSizeY	= pDomain->getRows();

	if ( this->ulCachedWorkgroupSizeX == 0 )
		ulCachedWorkgroupSizeX = ulConstraintWG + 
							 ( this->ucCacheConstraints == model::cacheConstraints::musclHancock::kCacheAllowUndersize ? -1 : 0 );
	if ( this->ulCachedWorkgroupSizeY == 0 )
		ulCachedWorkgroupSizeY = ulConstraintWG;

	ulCachedGlobalSizeX	= static_cast<unsigned long>( ceil( pDomain->getCols() * 
						  ( this->ucConfiguration == model::schemeConfigurations::godunovType::kCacheEnabled ? static_cast<double>( ulCachedWorkgroupSizeX ) / static_cast<double>( ulCachedWorkgroupSizeX - 2 ) : 1.0 ) ) );
	ulCachedGlobalSizeY	= static_cast<unsigned long>( ceil( pDomain->getRows() * 
						  ( this->ucConfiguration == model::schemeConfigurations::godunovType::kCacheEnabled ? static_cast<double>( ulCachedWorkgroupSizeY ) / static_cast<double>( ulCachedWorkgroupSizeY - 2 ) : 1.0 ) ) );

	// --
	// Timestep reduction (2D)
	// --

	// TODO: May need to make this configurable?!
	ulReductionWorkgroupSize = min(static_cast<size_t>(512), pDevice->clDeviceMaxWorkGroupSize);
	//ulReductionWorkgroupSize = pDevice->clDeviceMaxWorkGroupSize / 2;
	ulReductionGlobalSize = static_cast<unsigned long>( ceil( ( static_cast<double>(pDomain->getCellCount()) / this->uiTimestepReductionWavefronts ) / ulReductionWorkgroupSize ) * ulReductionWorkgroupSize );

	return bReturnState;
}

/*
 *  Allocate constants using the settings herein
 */
bool CSchemeGodunov::prepare1OConstants()
{
	CDomainCartesian*	pDomain	= static_cast<CDomainCartesian*>( this->pDomain );

	// --
	// Dry cell threshold depths
	// --
	oclModel->registerConstant( "VERY_SMALL",			toString( this->dThresholdVerySmall ) );
	oclModel->registerConstant( "QUITE_SMALL",			toString( this->dThresholdQuiteSmall ) );

	// --
	// Debug mode 
	// --

	if ( this->bDebugOutput )
	{
		oclModel->registerConstant( "DEBUG_OUTPUT",		"1" );
		oclModel->registerConstant( "DEBUG_CELLX",		toString( this->uiDebugCellX ) );
		oclModel->registerConstant( "DEBUG_CELLY",		toString( this->uiDebugCellY ) );
	} else {
		oclModel->removeConstant( "DEBUG_OUTPUT" );
		oclModel->removeConstant( "DEBUG_CELLX" );
		oclModel->removeConstant( "DEBUG_CELLY" );
	}

	// --
	// Work-group size requirements
	// --

	if ( this->ucConfiguration == model::schemeConfigurations::godunovType::kCacheNone )
	{
		oclModel->registerConstant( 
			"REQD_WG_SIZE_FULL_TS", 
			"__attribute__((reqd_work_group_size(" + toString( this->ulNonCachedWorkgroupSizeX )  + ", " + toString( this->ulNonCachedWorkgroupSizeY )  + ", 1)))"
		);
	} 
	if ( this->ucConfiguration == model::schemeConfigurations::godunovType::kCacheEnabled )
	{
		oclModel->registerConstant( 
			"REQD_WG_SIZE_FULL_TS", 
			"__attribute__((reqd_work_group_size(" + toString( this->ulNonCachedWorkgroupSizeX )  + ", " + toString( this->ulNonCachedWorkgroupSizeY )  + ", 1)))"
		);
	} 

	oclModel->registerConstant( 
		"REQD_WG_SIZE_LINE", 
		"__attribute__((reqd_work_group_size(" + toString( this->ulReductionWorkgroupSize )  + ", 1, 1)))"
	);

	// --
	// Size of local cache arrays
	// --

	switch( this->ucCacheConstraints )
	{
		case model::cacheConstraints::godunovType::kCacheActualSize:
			oclModel->registerConstant( "GTS_DIM1", toString( this->ulCachedWorkgroupSizeX ) );
			oclModel->registerConstant( "GTS_DIM2", toString( this->ulCachedWorkgroupSizeY ) );
			break;
		case model::cacheConstraints::godunovType::kCacheAllowUndersize:
			oclModel->registerConstant( "GTS_DIM1", toString( this->ulCachedWorkgroupSizeX ) );
			oclModel->registerConstant( "GTS_DIM2", toString( this->ulCachedWorkgroupSizeY ) );
			break;
		case model::cacheConstraints::godunovType::kCacheAllowOversize:
			oclModel->registerConstant( "GTS_DIM1", toString( this->ulCachedWorkgroupSizeX ) );
			oclModel->registerConstant( "GTS_DIM2", toString( this->ulCachedWorkgroupSizeY == 16 ? 17 : ulCachedWorkgroupSizeY ) );
			break;
	}

	// --
	// CFL/fixed timestep
	// --

	if ( this->bDynamicTimestep )
	{
		oclModel->registerConstant( "TIMESTEP_DYNAMIC",	"1" );
		oclModel->removeConstant( "TIMESTEP_FIXED" );
	} else {
		oclModel->registerConstant( "TIMESTEP_FIXED",	toString(this->dTimestep) );
		oclModel->removeConstant( "TIMESTEP_DYNAMIC" );
	}

	if (this->bFrictionEffects)
	{
		oclModel->registerConstant("FRICTION_ENABLED", "1");
	}
	else {
		oclModel->removeConstant("FRICTION_ENABLED");
	}

	if ( this->bFrictionInFluxKernel )
	{
		oclModel->registerConstant( "FRICTION_IN_FLUX_KERNEL",	"1" );
	}

	// --
	// Timestep reduction and simulation parameters
	// --
	oclModel->registerConstant( "TIMESTEP_WORKERS",		toString( this->ulReductionGlobalSize ) );
	oclModel->registerConstant( "TIMESTEP_GROUPSIZE",	toString( this->ulReductionWorkgroupSize ) );
	oclModel->registerConstant( "SCHEME_ENDTIME",		toString( pManager->getSimulationLength() ) );
	oclModel->registerConstant( "SCHEME_OUTPUTTIME",	toString( pManager->getOutputFrequency() ) );
	oclModel->registerConstant( "COURANT_NUMBER",		toString( this->dCourantNumber ) );

	// --
	// Domain details (size, resolution, etc.)
	// --

	double	dResolution;
	pDomain->getCellResolution( &dResolution );

	oclModel->registerConstant( "DOMAIN_CELLCOUNT",		toString( pDomain->getCellCount() ) );
	oclModel->registerConstant( "DOMAIN_COLS",			toString( pDomain->getCols() ) );
	oclModel->registerConstant( "DOMAIN_ROWS",			toString( pDomain->getRows() ) );
	oclModel->registerConstant( "DOMAIN_DELTAX",		toString( dResolution ) );
	oclModel->registerConstant( "DOMAIN_DELTAY",		toString( dResolution ) );

	return true;
}

/*
 *  Allocate memory for everything that isn't direct domain information (i.e. temporary/scheme data)
 */
bool CSchemeGodunov::prepare1OMemory()
{
	bool						bReturnState		= true;
	CExecutorControlOpenCL*		pExecutor			= pManager->getExecutor();
	CDomain*					pDomain				= this->pDomain;
	CBoundaryMap*				pBoundaries			= pDomain->getBoundaries();
	COCLDevice*					pDevice				= pExecutor->getDevice();

	unsigned char ucFloatSize =  ( pManager->getFloatPrecision() == model::floatPrecision::kSingle ? sizeof( cl_float ) : sizeof( cl_double ) );

	// --
	// Batch tracking data
	// --

	oclBufferBatchTimesteps	 = new COCLBuffer( "Batch timesteps cumulative", oclModel, false, true, ucFloatSize, true );
	oclBufferBatchSuccessful = new COCLBuffer( "Batch successful iterations", oclModel, false, true, sizeof(cl_uint), true );
	oclBufferBatchSkipped	 = new COCLBuffer( "Batch skipped iterations", oclModel, false, true, sizeof(cl_uint), true );

	if ( pManager->getFloatPrecision() == model::floatPrecision::kSingle )
	{
		*( oclBufferBatchTimesteps->getHostBlock<float*>() )	= 0.0f;
	} else {
		*( oclBufferBatchTimesteps->getHostBlock<double*>() )	= 0.0;
	}
	*( oclBufferBatchSuccessful->getHostBlock<cl_uint*>() )		= 0;
	*( oclBufferBatchSkipped->getHostBlock<cl_uint*>() )		= 0;

	oclBufferBatchTimesteps->createBuffer();
	oclBufferBatchSuccessful->createBuffer();
	oclBufferBatchSkipped->createBuffer();

	// --
	// Domain and cell state data
	// --

	void	*pCellStates = NULL, *pBedElevations = NULL, *pManningValues = NULL;
	pDomain->createStoreBuffers(
		&pCellStates,
		&pBedElevations,
		&pManningValues,
		ucFloatSize
	);

	oclBufferCellStates		= new COCLBuffer( "Cell states",			oclModel, false, true );
	oclBufferCellStatesAlt	= new COCLBuffer( "Cell states (alternate)",oclModel, false, true );
	oclBufferCellManning	= new COCLBuffer( "Manning coefficients",	oclModel, true,	true ); 
	oclBufferCellBed		= new COCLBuffer( "Bed elevations",			oclModel, true, true );

	oclBufferCellStates->setPointer( pCellStates, ucFloatSize * 4 * pDomain->getCellCount() );
	oclBufferCellStatesAlt->setPointer( pCellStates, ucFloatSize * 4 * pDomain->getCellCount() );
	oclBufferCellManning->setPointer( pManningValues, ucFloatSize * pDomain->getCellCount() );
	oclBufferCellBed->setPointer( pBedElevations, ucFloatSize * pDomain->getCellCount() );

	oclBufferCellStates->createBuffer();
	oclBufferCellStatesAlt->createBuffer();
	oclBufferCellManning->createBuffer();
	oclBufferCellBed->createBuffer();

	// --
	// Timesteps and current simulation time
	// --

	oclBufferTimestep			= new COCLBuffer( "Timestep", oclModel, false, true, ucFloatSize, true );
	oclBufferTime				= new COCLBuffer( "Time",	  oclModel, false, true, ucFloatSize, true );
	oclBufferTimeTarget			= new COCLBuffer( "Target time (sync)",	  oclModel, false, true, ucFloatSize, true );
	oclBufferTimeHydrological	= new COCLBuffer( "Time (hydrological)", oclModel, false, true, ucFloatSize, true );

	// We duplicate the time and timestep variables if we're using single-precision so we have copies in both formats
	if ( pManager->getFloatPrecision() == model::floatPrecision::kSingle )
	{
		*( oclBufferTime->getHostBlock<float*>()     )			= static_cast<cl_float>( this->dCurrentTime );
		*( oclBufferTimestep->getHostBlock<float*>() )			= static_cast<cl_float>( this->dCurrentTimestep );
		*( oclBufferTimeHydrological->getHostBlock<float*>() )	= 0.0f;
		*( oclBufferTimeTarget->getHostBlock<float*>() )		= 0.0f;
	} else {
		*( oclBufferTime->getHostBlock<double*>()     )			= this->dCurrentTime;
		*( oclBufferTimestep->getHostBlock<double*>() )			= this->dCurrentTimestep;
		*( oclBufferTimeHydrological->getHostBlock<double*>() ) = 0.0;
		*( oclBufferTimeTarget->getHostBlock<double*>() )		= 0.0;
	}

	oclBufferTimestep->createBuffer();
	oclBufferTime->createBuffer();
	oclBufferTimeHydrological->createBuffer();
	oclBufferTimeTarget->createBuffer();

	// --
	// Timestep reduction global array
	// --

	oclBufferTimestepReduction = new COCLBuffer( "Timestep reduction scratch", oclModel, false, true, this->ulReductionGlobalSize * ucFloatSize, true );
	oclBufferTimestepReduction->createBuffer();

	// TODO: Check buffers were created successfully before returning a positive response

	// VISUALISER STUFF
	// TODO: Make this a bit better, put it somewhere else, etc.
	oclBufferCellStates->setCallbackRead( CModel::visualiserCallback );

	return bReturnState;
}

/*
 *  Create general kernels used by numerous schemes with the compiled program
 */
bool CSchemeGodunov::prepareGeneralKernels()
{
	bool						bReturnState		= true;
	CExecutorControlOpenCL*		pExecutor			= pManager->getExecutor();
	CDomain*					pDomain				= this->pDomain;
	CBoundaryMap*				pBoundaries			= pDomain->getBoundaries();
	COCLDevice*					pDevice				= pExecutor->getDevice();

	// --
	// Timestep and simulation advancing
	// --

	oclKernelTimeAdvance		= oclModel->getKernel( "tst_Advance_Normal" );
	oclKernelResetCounters		= oclModel->getKernel( "tst_ResetCounters" );
	oclKernelTimestepReduction	= oclModel->getKernel( "tst_Reduce" );
	oclKernelTimestepUpdate		= oclModel->getKernel( "tst_UpdateTimestep" );

	oclKernelTimeAdvance->setGroupSize(1, 1, 1);
	oclKernelTimeAdvance->setGlobalSize(1, 1, 1);
	oclKernelTimestepUpdate->setGroupSize(1, 1, 1);
	oclKernelTimestepUpdate->setGlobalSize(1, 1, 1);
	oclKernelResetCounters->setGroupSize(1, 1, 1);
	oclKernelResetCounters->setGlobalSize(1, 1, 1);
	oclKernelTimestepReduction->setGroupSize( this->ulReductionWorkgroupSize );
	oclKernelTimestepReduction->setGlobalSize( this->ulReductionGlobalSize );

	COCLBuffer* aryArgsTimeAdvance[]		= { oclBufferTime, oclBufferTimestep, oclBufferTimeHydrological, oclBufferTimestepReduction, oclBufferCellStates, oclBufferCellBed, oclBufferTimeTarget, oclBufferBatchTimesteps, oclBufferBatchSuccessful, oclBufferBatchSkipped };
	COCLBuffer* aryArgsTimestepUpdate[]		= { oclBufferTime, oclBufferTimestep, oclBufferTimestepReduction, oclBufferTimeTarget, oclBufferBatchTimesteps };
	COCLBuffer* aryArgsTimeReduction[]		= { oclBufferCellStates, oclBufferCellBed, oclBufferTimestepReduction };
	COCLBuffer* aryArgsResetCounters[]      = { oclBufferBatchTimesteps, oclBufferBatchSuccessful, oclBufferBatchSkipped };

	oclKernelTimeAdvance->assignArguments( aryArgsTimeAdvance );
	oclKernelResetCounters->assignArguments( aryArgsResetCounters );
	oclKernelTimestepReduction->assignArguments( aryArgsTimeReduction );
	oclKernelTimestepUpdate->assignArguments( aryArgsTimestepUpdate );

	// --
	// Boundaries and friction etc.
	// --

	oclKernelFriction			= oclModel->getKernel( "per_Friction" );
	oclKernelFriction->setGroupSize( this->ulNonCachedWorkgroupSizeX, this->ulNonCachedWorkgroupSizeY );
	oclKernelFriction->setGlobalSize( this->ulNonCachedGlobalSizeX, this->ulNonCachedGlobalSizeY );

	COCLBuffer* aryArgsFriction[] = { oclBufferTimestep, oclBufferCellStates, oclBufferCellBed, oclBufferCellManning, oclBufferTime };	
	oclKernelFriction->assignArguments( aryArgsFriction );

	return bReturnState;
}

/*
 *  Create kernels using the compiled program
 */
bool CSchemeGodunov::prepare1OKernels()
{
	bool						bReturnState		= true;
	CExecutorControlOpenCL*		pExecutor			= pManager->getExecutor();
	CDomain*					pDomain				= this->pDomain;
	COCLDevice*		pDevice				= pExecutor->getDevice();

	// --
	// Godunov-type scheme kernels
	// --

	if ( this->ucConfiguration == model::schemeConfigurations::godunovType::kCacheNone )
	{
		oclKernelFullTimestep = oclModel->getKernel( "gts_cacheDisabled" );
		oclKernelFullTimestep->setGroupSize( this->ulNonCachedWorkgroupSizeX, this->ulNonCachedWorkgroupSizeY );
		oclKernelFullTimestep->setGlobalSize( this->ulNonCachedGlobalSizeX, this->ulNonCachedGlobalSizeY );
		COCLBuffer* aryArgsFullTimestep[] = { oclBufferTimestep, oclBufferCellBed, oclBufferCellStates, oclBufferCellStatesAlt, oclBufferCellManning };	
		oclKernelFullTimestep->assignArguments( aryArgsFullTimestep );
	}
	if ( this->ucConfiguration == model::schemeConfigurations::godunovType::kCacheEnabled )
	{
		oclKernelFullTimestep = oclModel->getKernel( "gts_cacheEnabled" );
		oclKernelFullTimestep->setGroupSize( this->ulCachedWorkgroupSizeX, this->ulCachedWorkgroupSizeY );
		oclKernelFullTimestep->setGlobalSize( this->ulCachedGlobalSizeX, this->ulCachedGlobalSizeY );
		COCLBuffer* aryArgsFullTimestep[] = { oclBufferTimestep, oclBufferCellBed, oclBufferCellStates, oclBufferCellStatesAlt, oclBufferCellManning };	
		oclKernelFullTimestep->assignArguments( aryArgsFullTimestep );
	}

	return bReturnState;
}

/*
 *  Release all OpenCL resources consumed using the OpenCL methods
 */
void CSchemeGodunov::releaseResources()
{
	this->bReady = false;

	pManager->log->writeLine("Releasing scheme resources held for OpenCL.");

	this->release1OResources();
}

/*
 *  Release all OpenCL resources consumed using the OpenCL methods
 */
void CSchemeGodunov::release1OResources()
{
	this->bReady = false;

	pManager->log->writeLine("Releasing 1st-order scheme resources held for OpenCL.");

	if ( this->oclModel != NULL )							delete oclModel;
	if ( this->oclKernelFullTimestep != NULL )				delete oclKernelFullTimestep;
	if ( this->oclKernelFriction != NULL )					delete oclKernelFriction;
	if ( this->oclKernelTimestepReduction != NULL )			delete oclKernelTimestepReduction;
	if ( this->oclKernelTimeAdvance != NULL )				delete oclKernelTimeAdvance;
	if ( this->oclKernelTimestepUpdate != NULL )			delete oclKernelTimestepUpdate;
	if ( this->oclKernelResetCounters != NULL )				delete oclKernelResetCounters;
	if ( this->oclBufferCellStates != NULL )				delete oclBufferCellStates;
	if ( this->oclBufferCellStatesAlt != NULL )				delete oclBufferCellStatesAlt;
	if ( this->oclBufferCellManning != NULL )				delete oclBufferCellManning;
	if ( this->oclBufferCellBed != NULL )					delete oclBufferCellBed;
	if ( this->oclBufferTimestep != NULL )					delete oclBufferTimestep;
	if ( this->oclBufferTimestepReduction != NULL )			delete oclBufferTimestepReduction;
	if ( this->oclBufferTime != NULL )						delete oclBufferTime;
	if ( this->oclBufferTimeTarget != NULL )				delete oclBufferTimeTarget;
	if ( this->oclBufferTimeHydrological != NULL )			delete oclBufferTimeHydrological;

	oclModel						= NULL;
	oclKernelFullTimestep			= NULL;
	oclKernelFriction				= NULL;
	oclKernelTimestepReduction		= NULL;
	oclKernelTimeAdvance			= NULL;
	oclKernelResetCounters			= NULL;
	oclKernelTimestepUpdate			= NULL;
	oclBufferCellStates				= NULL;
	oclBufferCellStatesAlt			= NULL;
	oclBufferCellManning			= NULL;
	oclBufferCellBed				= NULL;
	oclBufferTimestep				= NULL;
	oclBufferTimestepReduction		= NULL;
	oclBufferTime					= NULL;
	oclBufferTimeTarget				= NULL;
	oclBufferTimeHydrological		= NULL;

	if ( this->bIncludeBoundaries )
	{
		delete [] this->dBoundaryTimeSeries;
		delete [] this->fBoundaryTimeSeries;
		delete [] this->uiBoundaryParameters;
		if ( this->ulBoundaryRelationCells != NULL )
			delete [] this->ulBoundaryRelationCells;
		if ( this->uiBoundaryRelationSeries != NULL )
			delete [] this->uiBoundaryRelationSeries;
	}
	this->dBoundaryTimeSeries		= NULL;
	this->fBoundaryTimeSeries		= NULL;
	this->ulBoundaryRelationCells	= NULL;
	this->uiBoundaryRelationSeries	= NULL;
	this->uiBoundaryParameters		= NULL;
}

/*
 *  Prepares the simulation
 */
void	CSchemeGodunov::prepareSimulation()
{
	// Adjust cell bed elevations if necessary for boundary conditions
	pManager->log->writeLine( "Adjusting domain data for boundaries..." );
	this->pDomain->getBoundaries()->applyDomainModifications();

	// Initial volume in the domain
	pManager->log->writeLine( "Initial domain volume: " + toString( abs((int)(this->pDomain->getVolume()) ) ) + "m3" );

	// Copy the initial conditions
	pManager->log->writeLine( "Copying domain data to device..." );
	oclBufferCellStates->queueWriteAll();
	oclBufferCellStatesAlt->queueWriteAll();
	oclBufferCellBed->queueWriteAll();
	oclBufferCellManning->queueWriteAll();
	oclBufferTime->queueWriteAll();
	oclBufferTimestep->queueWriteAll();
	oclBufferTimeHydrological->queueWriteAll();
	this->pDomain->getDevice()->blockUntilFinished();

	// Sort out memory alternation
	bUseAlternateKernel		= false;
	bOverrideTimestep		= false;
	bDownloadLinks			= false;
	bImportLinks			= false;
	bUseForcedTimeAdvance	= true;
	bCellStatesSynced		= true;

	// Need a timer...
	dBatchStartedTime = 0.0;

	// Zero counters
	ulCurrentCellsCalculated	= 0;
	uiIterationsSinceSync		= 0;
	uiIterationsSinceProgressCheck = 0;
	dLastSyncTime				= 0.0;

	// States
	bRunning = false;
	bThreadRunning = false;
	bThreadTerminated = false;
}

#ifdef PLATFORM_WIN
DWORD CSchemeGodunov::Threaded_runBatchLaunch(LPVOID param)
{
	CSchemeGodunov* pScheme = static_cast<CSchemeGodunov*>(param);
	pScheme->Threaded_runBatch();
	return 0;
}
#endif
#ifdef PLATFORM_UNIX
void* CSchemeGodunov::Threaded_runBatchLaunch(void* param)
{
	CSchemeGodunov* pScheme = static_cast<CSchemeGodunov*>(param);
	pScheme->Threaded_runBatch();
	return 0;
}
#endif

/*
 *	Create a new thread to run this batch using
 */
void CSchemeGodunov::runBatchThread()
{
	if (this->bThreadRunning)
		return;

	this->bThreadRunning = true;
	this->bThreadTerminated = false;

#ifdef PLATFORM_WIN
	HANDLE hThread = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)CSchemeGodunov::Threaded_runBatchLaunch,
		this,
		0,
		NULL
		);
	CloseHandle(hThread);
#endif
#ifdef PLATFORM_UNIX
	pthread_t tid;
	int result = pthread_create(&tid, 0, CSchemeGodunov::Threaded_runBatchLaunch, this);
	if (result == 0)
		pthread_detach(tid);
#endif
}

/*
 *	Schedule a batch-load of work to run on the device and block
 *	until complete. Runs in its own thread.
 */
void CSchemeGodunov::Threaded_runBatch()
{
	// Keep the thread in existence because of the overhead
	// associated with creating a thread.
	while (this->bThreadRunning)
	{
		// Are we expected to run?
		if  (!this->bRunning || this->pDomain->getDevice()->isBusy())
		{
			if ( this->pDomain->getDevice()->isBusy() )
			{
				this->pDomain->getDevice()->blockUntilFinished();
			}
			continue;
		}

		// Have we been asked to update the target time?
		if (this->bUpdateTargetTime)
		{
			this->bUpdateTargetTime = false;
#ifdef DEBUG_MPI
			pManager->log->writeLine("[DEBUG] Setting new target time of " + Util::secondsToTime(this->dTargetTime) + "...");
#endif
		
			if (pManager->getFloatPrecision() == model::floatPrecision::kSingle)
			{
				*(oclBufferTimeTarget->getHostBlock<float*>()) = static_cast<cl_float>(this->dTargetTime);
			}
			else {
				*(oclBufferTimeTarget->getHostBlock<double*>()) = this->dTargetTime;
			}
			oclBufferTimeTarget->queueWriteAll();
			pDomain->getDevice()->queueBarrier();

			this->bCellStatesSynced = false;
			this->uiIterationsSinceSync = 0;

			bUseForcedTimeAdvance = true;

			/*  Don't do this when syncing the timesteps, as it's important we have a zero timestep immediately after
			 *	output files are written otherwise the timestep wont be reduced across MPI and the domains will go out
			 *  of sync!
			 */
			if ( dCurrentTimestep <= 0.0 && pManager->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncForecast )
			{
				pDomain->getDevice()->queueBarrier();
				oclKernelTimestepReduction->scheduleExecution();
				pDomain->getDevice()->queueBarrier();
				oclKernelTimestepUpdate->scheduleExecution();
			}
			
			if ( dCurrentTime + dCurrentTimestep > dTargetTime + 1E-5 )
			{
				this->dCurrentTimestep  = dTargetTime - dCurrentTime;
				this->bOverrideTimestep = true;
			}

			pDomain->getDevice()->queueBarrier();
			//pDomain->getDevice()->blockUntilFinished();		// Shouldn't be needed
			
#ifdef DEBUG_MPI
			pManager->log->writeLine("[DEBUG] Done updating new target time to " + Util::secondsToTime(this->dTargetTime) + "...");
#endif
		}

		// Have we been asked to override the timestep at the start of this batch?
		if ( //uiIterationsSinceSync < this->pDomain->getRollbackLimit() &&
			 this->dCurrentTime < dTargetTime &&
			 this->bOverrideTimestep )
		{		
			if (pManager->getFloatPrecision() == model::floatPrecision::kSingle)
			{
				*(oclBufferTimestep->getHostBlock<float*>()) = static_cast<cl_float>(this->dCurrentTimestep);
			}
			else {
				*(oclBufferTimestep->getHostBlock<double*>()) = this->dCurrentTimestep;
			}

			oclBufferTimestep->queueWriteAll();

			// TODO: Remove me?
			pDomain->getDevice()->queueBarrier();
			//pDomain->getDevice()->blockUntilFinished();

			this->bOverrideTimestep = false;
		}

		// Have we been asked to import data for our domain links?
		if (this->bImportLinks)
		{
			// Import data from links which are 'dependent' on this domain
			for (unsigned int i = 0; i < pDomain->getLinkCount(); i++)
			{
				pDomain->getLink(i)->pushToBuffer(this->getNextCellSourceBuffer());
			}

			// Last sync time
			this->dLastSyncTime = this->dCurrentTime;
			this->uiIterationsSinceSync = 0;

			// Update the data
			oclKernelResetCounters->scheduleExecution();
			pDomain->getDevice()->queueBarrier();

			// Force timestep recalculation if necessary
			if (pManager->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncForecast)
			{
				oclKernelTimestepReduction->scheduleExecution();
				pDomain->getDevice()->queueBarrier();
				oclKernelTimestepUpdate->scheduleExecution();
				pDomain->getDevice()->queueBarrier();
			}

			this->bImportLinks = false;
		}

		// Don't schedule any work if we're already at the sync point
		// TODO: Review this...
		//if (this->dCurrentTime > dTargetTime /* + 1E-5 */)
		//{
		//	bRunning = false;
		//	continue;
		//}

		// Can only schedule one iteration before we need to sync timesteps
		// if timestep sync method is active.
		unsigned int uiQueueAmount = this->uiQueueAdditionSize;
		if (pManager->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep)
			uiQueueAmount = 1;

#ifdef DEBUG_MPI
		if ( uiQueueAmount > 0 )
			pManager->log->writeLine("[DEBUG] Starting batch of " + toString(uiQueueAmount) + " with timestep " + Util::secondsToTime(this->dCurrentTimestep) + " at " + Util::secondsToTime(this->dCurrentTime) );
#endif
			
		// Schedule a batch-load of work for the device
		// Do we need to run any work?
		if ( uiIterationsSinceSync < this->pDomain->getRollbackLimit() &&
			 this->dCurrentTime < dTargetTime )
		{
			for (unsigned int i = 0; i < uiQueueAmount; i++)
			{
#ifdef DEBUG_MPI
				pManager->log->writeLine( "Scheduling a new iteration..." );
#endif
				this->scheduleIteration(
					bUseAlternateKernel,
					pDomain->getDevice(),
					pDomain
				);
				uiIterationsSinceSync++;
				uiIterationsSinceProgressCheck++;
				ulCurrentCellsCalculated += this->pDomain->getCellCount();
				bUseAlternateKernel = !bUseAlternateKernel;
			}

			// A further download will be required...
			this->bCellStatesSynced = false;
		}

		// Schedule reading data back. We always need the timestep
		// but we might not need the other details always...
		oclBufferTimestep->queueReadAll();
		oclBufferTime->queueReadAll();
		oclBufferBatchSkipped->queueReadAll();
		oclBufferBatchSuccessful->queueReadAll();
		oclBufferBatchTimesteps->queueReadAll();
		uiIterationsSinceProgressCheck = 0;

#ifdef _WINDLL
		oclBufferCellStates->queueReadAll();
#endif

		// Download data for each of the dependent domains
		if (bDownloadLinks)
		{
			// We need to know the time...
			this->pDomain->getDevice()->blockUntilFinished();
			this->readKeyStatistics();
		
#ifdef DEBUG_MPI
			pManager->log->writeLine( "[DEBUG] Downloading link data at " + Util::secondsToTime(this->dCurrentTime) );
#endif
			for (unsigned int i = 0; i < this->pDomain->getDependentLinkCount(); i++)
			{
				this->pDomain->getDependentLink(i)->pullFromBuffer(this->dCurrentTime, this->getNextCellSourceBuffer());
			}
		}

		// Flush the command queue so we can wait for it to finish
		this->pDomain->getDevice()->flushAndSetMarker();

		// Now that we're thread-based we can actually just block
		// this thread... probably don't need the marker
		this->pDomain->getDevice()->blockUntilFinished();
		
		// Are cell states now synced?
		if (bDownloadLinks)
		{
			bDownloadLinks = false;
			bCellStatesSynced = true;
		}

		// Read from buffers back to scheme memory space
		this->readKeyStatistics();
		
#ifdef DEBUG_MPI
		if ( uiQueueAmount > 0 )
		{
			pManager->log->writeLine("[DEBUG] Finished batch of " + toString(uiQueueAmount) + " with timestep " + Util::secondsToTime(this->dCurrentTimestep) + " at " + Util::secondsToTime(this->dCurrentTime) );
			if ( this->dCurrentTimestep < 0.0 )
			{
				pManager->log->writeLine( "[DEBUG] We have a negative timestep..." );
			}
		}
#endif
		
		// Wait until further work is scheduled
		this->bRunning = false;
	}

	this->bThreadTerminated = true;
}

/*
 *  Runs the actual simulation until completion or error
 */
void	CSchemeGodunov::runSimulation( double dTargetTime, double dRealTime )
{
	// Wait for current work to finish
	if (this->bRunning || this->pDomain->getDevice()->isBusy()) 
		return;

	// Has the target time changed?
	if ( this->dTargetTime != dTargetTime )
		setTargetTime( dTargetTime );

	// No target time? Can't run anything yet then, need to calculate one
	if (dTargetTime <= 0.0)
		return;

	// If we've already hit our sync time but the other domains haven't, don't bother scheduling any work
	if ( this->dCurrentTime > dTargetTime + 1E-5 )
	{
		// TODO: Consider downloading the data at this point?
		// but need to accommodate for rollbacks... difficult...

		// This is bad. But it might happen...
		model::doError(
			"Simulation has exceeded target time",
			model::errorCodes::kLevelWarning
		);
		pManager->log->writeLine(
			"Current time:   "  + toString( dCurrentTime ) + 
			", Target time:  " + toString( dTargetTime )
		);
		pManager->log->writeLine(
			"Last sync point: "  + toString( dLastSyncTime )
		);
		return;
	}

	// If we've hit our target time, download the data we need for any dependent
	// domain links (or in timestep sync, hit the iteration limit)
	if ( pManager->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncForecast &&
		 dTargetTime - this->dCurrentTime <= 1E-5 )
		 bDownloadLinks = true;
	if ( pManager->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep &&
		 ( this->uiIterationsSinceSync >= pDomain->getRollbackLimit() ||
		   dTargetTime - this->dCurrentTime <= 1E-5 ) )
		 bDownloadLinks = true;

	// Calculate a new batch size
	if (  this->bAutomaticQueue		&&
		 !this->bDebugOutput		&&
		  dRealTime > 1E-5          &&
		  pManager->getDomainSet()->getSyncMethod() != model::syncMethod::kSyncTimestep)
	{
			// We're aiming for a seconds worth of work to be carried out
			double dBatchDuration = dRealTime - dBatchStartedTime;
			unsigned int uiOldQueueAdditionSize = this->uiQueueAdditionSize;

			if (pManager->getDomainSet()->getDomainCount() > 1) {
				this->uiQueueAdditionSize = static_cast<unsigned int>((dTargetTime - dCurrentTime) / (dBatchTimesteps / static_cast<double>(uiBatchSuccessful)) + 1.0);
			} else {
				this->uiQueueAdditionSize = static_cast<unsigned int>(max(static_cast<unsigned int>(1), min(this->uiBatchRate * 3, static_cast<unsigned int>(ceil(1.0 / (dBatchDuration / static_cast<double>(this->uiQueueAdditionSize)))))));
			}

			// Stop silly jumps in the queue addition size
			if (this->uiQueueAdditionSize > uiOldQueueAdditionSize * 2 &&
				this->uiQueueAdditionSize > 40)
				this->uiQueueAdditionSize = min(static_cast<unsigned int>(this->uiBatchRate * 3), uiOldQueueAdditionSize * 2);

			// Don't allow the batch size to exceed the work we can schedule without requiring a
			// rollback of the domain state.
			if (this->uiQueueAdditionSize > pDomain->getRollbackLimit() - this->uiIterationsSinceSync)
				this->uiQueueAdditionSize = pDomain->getRollbackLimit() - this->uiIterationsSinceSync;

			// Can't have zero queue addition size
			if (this->uiQueueAdditionSize < 1)
				this->uiQueueAdditionSize = 1;
	}

	dBatchStartedTime = dRealTime;
	this->bRunning = true;
	this->runBatchThread();
}

/*
 *  Clean-up temporary resources consumed during the simulation
 */
void	CSchemeGodunov::cleanupSimulation()
{
	// TODO: Anything to clean-up? Callbacks? Timers?
	dBatchStartedTime = 0.0;

	// Kill the worker thread
	bRunning = false;
	bThreadRunning = false;

	// Wait for the thread to terminate before returning
	while (!bThreadTerminated && bThreadRunning) {}
}

/*
 *  Rollback the simulation to the last successful round
 */
void	CSchemeGodunov::rollbackSimulation( double dCurrentTime, double dTargetTime )
{
	// Wait until any pending tasks have completed first...
	this->getDomain()->getDevice()->blockUntilFinished();

	uiIterationsSinceSync = 0;

	this->dCurrentTime = dCurrentTime;
	this->dTargetTime = dTargetTime;

	// Update the time
	if ( pManager->getFloatPrecision() == model::floatPrecision::kSingle )
	{
		*( oclBufferTime->getHostBlock<float*>() )	= static_cast<cl_float>( dCurrentTime );
		*( oclBufferTimeTarget->getHostBlock<float*>() ) = static_cast<cl_float> (dTargetTime );
	} else {
		*( oclBufferTime->getHostBlock<double*>() )	= dCurrentTime;
		*(oclBufferTimeTarget->getHostBlock<double*>()) = dTargetTime;
	}

	// Write all memory buffers...
	oclBufferTime->queueWriteAll();
	oclBufferTimeTarget->queueWriteAll();
	oclBufferCellStatesAlt->queueWriteAll();
	oclBufferCellStates->queueWriteAll();

	// Schedule timestep calculation again
	// Timestep reduction
	if ( this->bDynamicTimestep )
	{
		oclKernelTimestepReduction->scheduleExecution();
		pDomain->getDevice()->queueBarrier();
	}

	// Timestep update without simulation time update
	if (pManager->getDomainSet()->getSyncMethod() != model::syncMethod::kSyncTimestep)
		oclKernelTimestepUpdate->scheduleExecution();
	bUseForcedTimeAdvance = true;

	// Clear the failure state
	oclKernelResetCounters->scheduleExecution();

	pDomain->getDevice()->queueBarrier();
	pDomain->getDevice()->flush();
}

/*
 *  Is the simulation a failure requiring a rollback?
 */
bool	CSchemeGodunov::isSimulationFailure( double dExpectedTargetTime )
{
	if (bRunning)
		return false;

	// Can't exceed number of buffer cells in forecast mode
	if ( pManager->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncForecast &&
		 uiBatchSuccessful >= pDomain->getRollbackLimit() &&
		 dExpectedTargetTime - dCurrentTime > 1E-5)
		return true;

	// This shouldn't happen
	if ( pManager->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep &&
		 uiBatchSuccessful > pDomain->getRollbackLimit() )
		return true;

	// This also shouldn't happen... but might...
	if (this->dCurrentTime > dExpectedTargetTime + 1E-5)
	{
		model::doError(
			"Scheme has exceeded target sync time. Rolling back...",
			model::errorCodes::kLevelWarning
		);
		pManager->log->writeLine(
			"Current time: " + toString(dCurrentTime) + 
			", target time: " + toString(dExpectedTargetTime)
		);
		return true;
	}

	// Assume success
	return false;
}

/*
 *  Force the timestap to be advanced even if we're synced
 */
void	CSchemeGodunov::forceTimeAdvance()
{
	bUseForcedTimeAdvance = true;
}

/*
 *  Is the simulation ready to be synchronised?
 */
bool	CSchemeGodunov::isSimulationSyncReady( double dExpectedTargetTime )
{
	// Check whether we're still busy or failure occured
	//if ( isSimulationFailure() )
	//	return false;

	if (bRunning)
		return false;

	// Have we hit our target time?
	// TODO: Review whether this is appropriate (need fabs?) (1E-5?)
	if ( pManager->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep )
	{
		// Any criteria required for timestep-based sync?
	} else {
		if ( dExpectedTargetTime - dCurrentTime > 1E-5 )
		{
#ifdef DEBUG_MPI
			pManager->log->writeLine( "Expected target: " + toString( dExpectedTargetTime ) + " Current time: " + toString( dCurrentTime ) );
#endif
			return false;
		}
	}

	// Have we downloaded the data we need for each domain link?
	if ( !bCellStatesSynced && pManager->getDomainSet()->getDomainCount() > 1 )
		return false;

	// Are we synchronising the timesteps?
	if ( pManager->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep &&
		 uiIterationsSinceSync < this->pDomain->getRollbackLimit() - 1 &&
		 dExpectedTargetTime - dCurrentTime > 1E-5 &&
		 dCurrentTime > 0.0 )
		return false;

	//if ( uiIterationsSinceSync < this->pDomain->getRollbackLimit() )
	//	return false;

	// Assume success
#ifdef DEBUG_MPI
	//pManager->log->writeLine( "Domain is considered sync ready" );
#endif
	
	return true;
}

/*
 *  Runs the actual simulation until completion or error
 */
void	CSchemeGodunov::scheduleIteration(
				bool			bUseAlternateKernel,
				COCLDevice*		pDevice,
				CDomain*		pDomain
	)
{
	// Re-set the kernel arguments to use the correct cell state buffer
	if ( bUseAlternateKernel )
	{
		oclKernelFullTimestep->assignArgument( 2, oclBufferCellStatesAlt );
		oclKernelFullTimestep->assignArgument( 3, oclBufferCellStates );
		oclKernelFriction->assignArgument( 1, oclBufferCellStates );
		oclKernelTimestepReduction->assignArgument( 3, oclBufferCellStates );
	} else {
		oclKernelFullTimestep->assignArgument( 2, oclBufferCellStates );
		oclKernelFullTimestep->assignArgument( 3, oclBufferCellStatesAlt );
		oclKernelFriction->assignArgument( 1, oclBufferCellStatesAlt );
		oclKernelTimestepReduction->assignArgument( 3, oclBufferCellStatesAlt );
	}

	// Run the boundary kernels (each bndy has its own kernel now)
	pDomain->getBoundaries()->applyBoundaries(bUseAlternateKernel ? oclBufferCellStatesAlt : oclBufferCellStates);
	pDevice->queueBarrier();

	// Main scheme kernel
	oclKernelFullTimestep->scheduleExecution();
	pDevice->queueBarrier();

	// Friction
	if ( this->bFrictionEffects && !this->bFrictionInFluxKernel )
	{
		oclKernelFriction->scheduleExecution();
		pDevice->queueBarrier();
	}

	// Timestep reduction
	if ( this->bDynamicTimestep )
	{
		oclKernelTimestepReduction->scheduleExecution();
		pDevice->queueBarrier();
	}

	// Time advancing
	oclKernelTimeAdvance->scheduleExecution();
	pDevice->queueBarrier();

	// Only block after every iteration when testing things that need it...
	// Big performance hit...
	//pDevice->blockUntilFinished();
}

/*
 *  Read back all of the domain data
 */
void CSchemeGodunov::readDomainAll()
{
	if ( bUseAlternateKernel )
	{
		oclBufferCellStatesAlt->queueReadAll();
	} else {
		oclBufferCellStates->queueReadAll();
	}
}

/*
 *  Read back domain data for the synchronisation zones only
 */
void CSchemeGodunov::importLinkZoneData()
{
	this->bImportLinks = true;
}

/*
*	Fetch the pointer to the last cell source buffer
*/
COCLBuffer* CSchemeGodunov::getLastCellSourceBuffer()
{
	if (bUseAlternateKernel)
	{
		return oclBufferCellStates;
	}
	else {
		return oclBufferCellStatesAlt;
	}
}

/*
 *	Fetch the pointer to the next cell source buffer
 */
COCLBuffer* CSchemeGodunov::getNextCellSourceBuffer()
{
	if (bUseAlternateKernel)
	{
		return oclBufferCellStatesAlt;
	}
	else {
		return oclBufferCellStates;
	}
}

/*
 *  Save current cell states incase of need to rollback
 */
void CSchemeGodunov::saveCurrentState()
{
	// Flag is flipped after an iteration, so if it's true that means
	// the last one saved to the normal cell state buffer...
	getNextCellSourceBuffer()->queueReadAll();

	// Reset iteration tracking
	// TODO: Should this be moved into the sync function?
	uiIterationsSinceSync = 0;

	// Block until complete
	// TODO: Investigate - does this need to be a blocking command?
	// Blocking should be carried out in CModel to allow multiple domains 
	// to download at once...
	//pDomain->getDevice()->queueBarrier();
	//pDomain->getDevice()->blockUntilFinished();
}

/*
 *  Set the target sync time
 */
void CSchemeGodunov::setTargetTime( double dTime )
{
	if (dTime == this->dTargetTime)
		return;

#ifdef DEBUG_MPI
	pManager->log->writeLine("[DEBUG] Received request to set target to " + toString(dTime));
#endif
	this->dTargetTime = dTime;
	//this->dLastSyncTime = this->dCurrentTime;
	this->bUpdateTargetTime = true;
	//this->bUseForcedTimeAdvance = true;
}

/*
 *  Propose a synchronisation point based on current performance of the scheme
 */
double CSchemeGodunov::proposeSyncPoint( double dCurrentTime )
{
	// TODO: Improve this so we're using more than just the current timestep...
	double dProposal = dCurrentTime + fabs(this->dTimestep);

	// Can only use this method once we have some simulation completed, not valid
	// at the start.
	if ( dCurrentTime > 1E-5 && uiBatchSuccessful > 0 )
	{
		// Try to accommodate approximately three spare iterations
		dProposal = dCurrentTime + 
			max(fabs(this->dTimestep), pDomain->getRollbackLimit() * (dBatchTimesteps / uiBatchSuccessful) * (((double)pDomain->getRollbackLimit() - pManager->getDomainSet()->getSyncBatchSpares()) / pDomain->getRollbackLimit()));
		// Don't allow massive jumps
		//if ((dProposal - dCurrentTime) > dBatchTimesteps * 3.0)
		//	dProposal = dCurrentTime + dBatchTimesteps * 3.0;
		// If we've hit our rollback limit, use the time we reached to determine a conservative estimate for 
		// a new sync point
		if ( uiBatchSuccessful >= pDomain->getRollbackLimit() )
			dProposal = dCurrentTime + dBatchTimesteps * 0.95;
	} else {
		// Can't return a suggestion of not going anywhere..
		if ( dProposal - dCurrentTime < 1E-5 )
			dProposal = dCurrentTime + fabs(this->dTimestep);
	}

	// If using real-time visualisation, force syncs more frequently (?)
#ifdef _WINDLL
	// This line is broken... Maybe? Not sure...
	//dProposal = min( dProposal, dCurrentTime + ( dBatchTimesteps / uiBatchSuccessful ) * 2 * uiQueueAdditionSize );
#endif

	return dProposal;
}

/*
 *  Get the batch average timestep
 */
double CSchemeGodunov::getAverageTimestep()
{
	if (uiBatchSuccessful < 1) return 0.0;
	return dBatchTimesteps / uiBatchSuccessful;
}

/*
 *	Force a specific timestep (when synchronising them)
 */
void CSchemeGodunov::forceTimestep(double dTimestep)
{
	// Might not have to write anything :-)
	if (dTimestep == this->dCurrentTimestep)
		return;

	this->dCurrentTimestep = dTimestep;
	this->bOverrideTimestep = true;
}

/*
 *  Fetch key details back to the right places in memory
 */
void	CSchemeGodunov::readKeyStatistics()
{
	cl_uint uiLastBatchSuccessful = uiBatchSuccessful;

	// Pull key data back from our buffers to the scheme class
	if ( pManager->getFloatPrecision() == model::floatPrecision::kSingle )
	{
		dCurrentTimestep = static_cast<cl_double>( *( oclBufferTimestep->getHostBlock<float*>() ) );
		dCurrentTime = static_cast<cl_double>(*(oclBufferTime->getHostBlock<float*>()));
		dBatchTimesteps = static_cast<cl_double>( *( oclBufferBatchTimesteps->getHostBlock<float*>() ) );
	} else {
		dCurrentTimestep = *( oclBufferTimestep->getHostBlock<double*>() );
		dCurrentTime = *(oclBufferTime->getHostBlock<double*>());
		dBatchTimesteps = *( oclBufferBatchTimesteps->getHostBlock<double*>() );
	}
	uiBatchSuccessful = *( oclBufferBatchSuccessful->getHostBlock<cl_uint*>() );
	uiBatchSkipped	  = *( oclBufferBatchSkipped->getHostBlock<cl_uint*>() );
	uiBatchRate = uiBatchSuccessful > uiLastBatchSuccessful ? (uiBatchSuccessful - uiLastBatchSuccessful) : 1;
}
