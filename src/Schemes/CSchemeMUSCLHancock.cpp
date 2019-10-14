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
 *  The MUSCL-Hancock scheme
 *  i.e. Godunov-type
 * ------------------------------------------
 *
 */
#include <boost/lexical_cast.hpp>
#include <algorithm>

#include "../common.h"
#include "../Boundaries/CBoundaryMap.h"
#include "../Boundaries/CBoundary.h"
#include "../Domain/CDomain.h"
#include "../Domain/Cartesian/CDomainCartesian.h"
#include "../Datasets/CXMLDataset.h"
#include "CSchemeMUSCLHancock.h"

using std::min;
using std::max;

/*
 *  Constructor
 */
CSchemeMUSCLHancock::CSchemeMUSCLHancock(void)
{
	pManager->log->writeLine( "MUSCL-Hancock scheme loaded for execution on OpenCL platform." );

	this->bDebugOutput					= false;
	this->uiDebugCellX					= 100;
	this->uiDebugCellY					= 100;

	this->ucSolverType					= model::solverTypes::kHLLC;
	this->ucConfiguration				= model::schemeConfigurations::musclHancock::kCachePrediction;
	this->ucCacheConstraints			= model::cacheConstraints::musclHancock::kCacheActualSize;

	this->ulCachedWorkgroupSizeX		= 0;
	this->ulCachedWorkgroupSizeY		= 0;
	this->ulNonCachedWorkgroupSizeX		= 0;
	this->ulNonCachedWorkgroupSizeY		= 0;

	this->dBoundaryTimeSeries			= NULL;
	this->ulBoundaryRelationCells		= NULL;
	this->uiBoundaryRelationSeries		= NULL;
	this->bContiguousFaceData			= false;

	oclKernelHalfTimestep				= NULL;
	oclBufferFaceExtrapolations			= NULL;
	oclBufferFaceExtrapolationN			= NULL;
	oclBufferFaceExtrapolationE			= NULL;
	oclBufferFaceExtrapolationS			= NULL;
	oclBufferFaceExtrapolationW			= NULL;		
}

/*
 *  Destructor
 */
CSchemeMUSCLHancock::~CSchemeMUSCLHancock(void)
{
	this->releaseResources();
	pManager->log->writeLine( "The MUSCL-Hancock scheme was unloaded from memory." );
}

/*
 *  Read in settings from the XML configuration file for this scheme
 */
void	CSchemeMUSCLHancock::setupFromConfig( XMLElement* pXScheme, bool bInheritanceChain )
{
	// Call the base class function which handles most of the settings
	CSchemeGodunov::setupFromConfig( pXScheme, true );

	XMLElement		*pParameter		= pXScheme->FirstChildElement("parameter");
	char			*cParameterName = NULL, *cParameterValue = NULL;

	while ( pParameter != NULL )
	{
		Util::toLowercase( &cParameterName,  pParameter->Attribute( "name" ) );
		Util::toLowercase( &cParameterValue, pParameter->Attribute( "value" ) );

		if ( strcmp( cParameterName, "localcachelevel" ) == 0 )
		{ 
			unsigned char usCache = 255;
			if ( strcmp( cParameterValue, "maximum" ) == 0 || strcmp( cParameterValue, "max" ) == 0 )
				usCache = model::schemeConfigurations::musclHancock::kCacheMaximum;
			if ( strcmp( cParameterValue, "prediction" ) == 0 || strcmp( cParameterValue, "slopes" ) == 0 )
				usCache = model::schemeConfigurations::musclHancock::kCachePrediction;
			if ( strcmp( cParameterValue, "none" ) == 0 || strcmp( cParameterValue, "no" ) == 0 )
				usCache = model::schemeConfigurations::musclHancock::kCacheNone;
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
				ucCacheConstraints = model::cacheConstraints::musclHancock::kCacheActualSize;
			if ( strcmp( cParameterValue, "larger" ) == 0 || strcmp( cParameterValue, "oversized" ) == 0 )
				ucCacheConstraints = model::cacheConstraints::musclHancock::kCacheAllowOversize;
			if ( strcmp( cParameterValue, "smaller" ) == 0 || strcmp( cParameterValue, "undersized" ) == 0 )
				ucCacheConstraints = model::cacheConstraints::musclHancock::kCacheAllowUndersize;
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
		else if ( strcmp( cParameterName, "contiguousextrapolationdata" ) == 0 )
		{ 
			unsigned char ucExtrapolationDataState = 255;
			if ( strcmp( cParameterValue, "yes" ) == 0 )
				ucExtrapolationDataState = 1;
			if ( strcmp( cParameterValue, "no" ) == 0 )
				ucExtrapolationDataState = 0;
			if ( ucExtrapolationDataState == 255 )
			{
				model::doError(
					"Invalid value for contiguous extrapolation data.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setExtrapolatedContiguity( ucExtrapolationDataState == 1 );
			}
		}

		pParameter = pParameter->NextSiblingElement("parameter");
	}
}

/*
 *  Run all preparation steps
 */
void CSchemeMUSCLHancock::prepareAll()
{
	this->releaseResources();

	oclModel = new COCLProgram(
		pManager->getExecutor(),
		this->pDomain->getDevice()
	);

	this->ulCurrentCellsCalculated		= 0;
	this->dCurrentTimestep				= this->dTimestep;
	this->dCurrentTime					= 0;

	// Forcing single precision?
	this->oclModel->setForcedSinglePrecision( pManager->getFloatPrecision() == model::floatPrecision::kSingle );
	unsigned char ucFloatSize =  ( pManager->getFloatPrecision() == model::floatPrecision::kDouble ? sizeof( cl_double ) : sizeof( cl_float ) );

	// OpenCL elements
	if ( !this->prepare1OExecDimensions() ) 
	{ 
		model::doError(
			"Failed to dimension 1st-order task elements. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}
	if ( !this->prepare2OExecDimensions() ) 
	{ 
		model::doError(
			"Failed to dimension 2nd-order task elements. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}

	if ( !this->prepare1OConstants() ) 
	{ 
		model::doError(
			"Failed to allocate 1st-order constants. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}
	if ( !this->prepare2OConstants() ) 
	{ 
		model::doError(
			"Failed to allocate 2nd-order constants. Cannot continue.",
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
			"Failed to create 1st-order memory buffers. Cannot continue.",
			model::errorCodes::kLevelModelStop
		);
		this->releaseResources();
		return;
	}
	if ( !this->prepare2OMemory() ) 
	{ 
		model::doError(
			"Failed to create 2nd-order memory buffers. Cannot continue.",
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
	if ( !this->prepare2OKernels() ) 
	{ 
		model::doError(
			"Failed to prepare 2nd-order kernels. Cannot continue.",
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
 *  Log the details and properties of this scheme instance.
 */
void CSchemeMUSCLHancock::logDetails()
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
	case model::schemeConfigurations::musclHancock::kCacheNone:
			sConfiguration = "No local caching";
		break;
	case model::schemeConfigurations::musclHancock::kCachePrediction:
			sConfiguration = "Prediction-step caching";
		break;
	case model::schemeConfigurations::musclHancock::kCacheMaximum:
			sConfiguration = "Maximum local caching";
		break;
	}

	pManager->log->writeLine( "MUSCL-HANCOCK 2ND-ORDER-ACCURATE SCHEME", true, wColour );
	pManager->log->writeLine( "  Timestep mode:      " + (std::string)( this->bDynamicTimestep ? "Dynamic" : "Fixed" ), true, wColour );
	pManager->log->writeLine( "  Courant number:     " + (std::string)( this->bDynamicTimestep ? toString( this->dCourantNumber ) : "N/A" ), true, wColour );
	pManager->log->writeLine( "  Initial timestep:   " + Util::secondsToTime( this->dTimestep ), true, wColour );
	pManager->log->writeLine( "  Data reduction:     " + toString( this->uiTimestepReductionWavefronts ) + " divisions", true, wColour );
	pManager->log->writeLine( "  Boundaries:         " + toString( this->pDomain->getBoundaries()->getBoundaryCount() ), true, wColour );
	pManager->log->writeLine( "  Riemann solver:     " + sSolver, true, wColour );
	pManager->log->writeLine( "  Configuration:      " + sConfiguration, true, wColour );
	pManager->log->writeLine( "  Friction effects:   " + (std::string)( this->bFrictionEffects ? "Enabled" : "Disabled" ), true, wColour );
	pManager->log->writeLine( "  Kernel queue mode:  " + (std::string)( this->bAutomaticQueue ? "Automatic" : "Fixed size" ), true, wColour );
	pManager->log->writeLine( (std::string)( this->bAutomaticQueue ? "  Initial queue:      " : "  Fixed queue:        " ) + toString( this->uiQueueAdditionSize ) + " iteration(s)", true, wColour );
	pManager->log->writeLine( "  Debug output:       " + (std::string)( this->bDebugOutput ? "Enabled" : "Disabled" ), true, wColour );
	
	pManager->log->writeDivide();
}

/*
 *  Concatenate together the code for the different elements required
 */
bool CSchemeMUSCLHancock::prepareCode()
{
	bool bReturnState = true;

	oclModel->appendCodeFromResource( "CLDomainCartesian_H" );
	oclModel->appendCodeFromResource( "CLFriction_H" );
	oclModel->appendCodeFromResource( "CLSlopeLimiterMINMOD_H" );
	oclModel->appendCodeFromResource( "CLSolverHLLC_H" );
	oclModel->appendCodeFromResource( "CLDynamicTimestep_H" );
	oclModel->appendCodeFromResource( "CLSchemeMUSCLHancock_H" );
	oclModel->appendCodeFromResource( "CLBoundaries_H" );

	oclModel->appendCodeFromResource( "CLDomainCartesian_C" );
	oclModel->appendCodeFromResource( "CLFriction_C" );
	oclModel->appendCodeFromResource( "CLSlopeLimiterMINMOD_C" );
	oclModel->appendCodeFromResource( "CLSolverHLLC_C" );
	oclModel->appendCodeFromResource( "CLDynamicTimestep_C" );
	oclModel->appendCodeFromResource( "CLSchemeMUSCLHancock_C" );
	oclModel->appendCodeFromResource( "CLBoundaries_C" );

	bReturnState = oclModel->compileProgram();

	return bReturnState;
}

/*
 *  Calculate the dimensions for executing the problems (e.g. reduction glob/local sizes)
 */
bool CSchemeMUSCLHancock::prepare2OExecDimensions()
{
	bool						bReturnState = true;
	CExecutorControlOpenCL*		pExecutor	 = pManager->getExecutor();
	COCLDevice*		pDevice		 = pExecutor->getDevice();
	CDomainCartesian*			pDomain		 = static_cast<CDomainCartesian*>( this->pDomain );

	// --
	// Maximum permissible work-group dimensions for this device
	// --

	cl_ulong	ulConstraintWGTotal = static_cast<cl_ulong>( floor( sqrt( static_cast<double> ( pDevice->clDeviceMaxWorkGroupSize ) ) ) );
	cl_ulong	ulConstraintWGDim   = min( pDevice->clDeviceMaxWorkItemSizes[0], pDevice->clDeviceMaxWorkItemSizes[1] );
	cl_ulong	ulConstraintWG		= min( ulConstraintWGDim, ulConstraintWGTotal );

	// --
	// Main scheme kernels with/without caching (2D)
	// --

	if ( this->ulCachedWorkgroupSizeX == 0 )
		ulCachedWorkgroupSizeX = ulConstraintWG + 
							 ( this->ucCacheConstraints == model::cacheConstraints::musclHancock::kCacheAllowUndersize ? -1 : 0 );
	if ( this->ulCachedWorkgroupSizeY == 0 )
		ulCachedWorkgroupSizeY = ulConstraintWG;

	ulCachedGlobalSizeX	= static_cast<unsigned long>( ceil( pDomain->getCols() * 
						  ( this->ucConfiguration == model::schemeConfigurations::musclHancock::kCachePrediction ? static_cast<double>( ulCachedWorkgroupSizeX ) / static_cast<double>( ulCachedWorkgroupSizeX - 2 ) : 1.0 ) *
						  ( this->ucConfiguration == model::schemeConfigurations::musclHancock::kCacheMaximum    ? static_cast<double>( ulCachedWorkgroupSizeX ) / static_cast<double>( ulCachedWorkgroupSizeX - 4 ) : 1.0 ) ) );
	ulCachedGlobalSizeY	= static_cast<unsigned long>( ceil( pDomain->getRows() * 
						  ( this->ucConfiguration == model::schemeConfigurations::musclHancock::kCachePrediction ? static_cast<double>( ulCachedWorkgroupSizeY ) / static_cast<double>( ulCachedWorkgroupSizeY - 2 ) : 1.0 ) *
						  ( this->ucConfiguration == model::schemeConfigurations::musclHancock::kCacheMaximum    ? static_cast<double>( ulCachedWorkgroupSizeY ) / static_cast<double>( ulCachedWorkgroupSizeY - 4 ) : 1.0 ) ) );

	return bReturnState;
}

/*
 *  Allocate constants using the settings herein
 */
bool CSchemeMUSCLHancock::prepare2OConstants()
{
	CDomainCartesian*	pDomain	= static_cast<CDomainCartesian*>( this->pDomain );

	// --
	// Work-group size requirements
	// --

	if ( this->ucConfiguration == model::schemeConfigurations::musclHancock::kCacheNone )
	{
		oclModel->registerConstant( 
			"REQD_WG_SIZE_HALF_TS", 
			"__attribute__((reqd_work_group_size(" + toString( this->ulNonCachedWorkgroupSizeX )  + ", " + toString( this->ulNonCachedWorkgroupSizeY )  + ", 1)))"
		);
		oclModel->registerConstant( 
			"REQD_WG_SIZE_FULL_TS", 
			"__attribute__((reqd_work_group_size(" + toString( this->ulNonCachedWorkgroupSizeX )  + ", " + toString( this->ulNonCachedWorkgroupSizeY )  + ", 1)))"
		);
	} 
	if ( this->ucConfiguration == model::schemeConfigurations::musclHancock::kCachePrediction )
	{
		oclModel->registerConstant( 
			"REQD_WG_SIZE_HALF_TS", 
			"__attribute__((reqd_work_group_size(" + toString( this->ulCachedWorkgroupSizeX )  + ", " + toString( this->ulCachedWorkgroupSizeY )  + ", 1)))"
		);
		oclModel->registerConstant( 
			"REQD_WG_SIZE_FULL_TS", 
			"__attribute__((reqd_work_group_size(" + toString( this->ulNonCachedWorkgroupSizeX )  + ", " + toString( this->ulNonCachedWorkgroupSizeY )  + ", 1)))"
		);
	} 
	if ( this->ucConfiguration == model::schemeConfigurations::musclHancock::kCacheMaximum )
	{
		oclModel->registerConstant( 
			"REQD_WG_SIZE_HALF_TS", 
			"__attribute__((reqd_work_group_size(" + toString( this->ulCachedWorkgroupSizeX )  + ", " + toString( this->ulCachedWorkgroupSizeY )  + ", 1)))"
		);
		oclModel->registerConstant( 
			"REQD_WG_SIZE_FULL_TS", 
			"__attribute__((reqd_work_group_size(" + toString( this->ulCachedWorkgroupSizeX )  + ", " + toString( this->ulCachedWorkgroupSizeY )  + ", 1)))"
		);
	} 

	// --
	// Storage of face-extrapolated cell data for second-order accuracy
	// --

	if ( this->bContiguousFaceData )
	{
		oclModel->registerConstant( "MEM_CONTIGUOUS_FACES",	"1" );
		oclModel->removeConstant( "MEM_SEPARATE_FACES" );
	} else {
		oclModel->registerConstant( "MEM_SEPARATE_FACES",	"1" );
		oclModel->removeConstant( "MEM_CONTIGUOUS_FACES" );
	}

	// --
	// Size of local cache arrays
	// --

	switch( this->ucCacheConstraints )
	{
		case model::cacheConstraints::musclHancock::kCacheActualSize:
			oclModel->registerConstant( "MCH_STG1_DIM1", toString( this->ulCachedWorkgroupSizeX ) );
			oclModel->registerConstant( "MCH_STG1_DIM2", toString( this->ulCachedWorkgroupSizeY ) );
			break;
		case model::cacheConstraints::musclHancock::kCacheAllowUndersize:
			oclModel->registerConstant( "MCH_STG1_DIM1", toString( this->ulCachedWorkgroupSizeX ) );
			oclModel->registerConstant( "MCH_STG1_DIM2", toString( this->ulCachedWorkgroupSizeY ) );
			break;
		case model::cacheConstraints::musclHancock::kCacheAllowOversize:
			oclModel->registerConstant( "MCH_STG1_DIM1", toString( this->ulCachedWorkgroupSizeX ) );
			oclModel->registerConstant( "MCH_STG1_DIM2", toString( this->ulCachedWorkgroupSizeY == 16 ? 17 : ulCachedWorkgroupSizeY ) );
			break;
	}

	return true;
}

/*
 *  Allocate memory for everything that isn't direct domain information (i.e. temporary/scheme data)
 */
bool CSchemeMUSCLHancock::prepare2OMemory()
{
	bool						bReturnState		= true;
	CDomain*					pDomain				= this->pDomain;

	unsigned char ucFloatSize = (pManager->getFloatPrecision() == model::floatPrecision::kDouble ? sizeof(cl_double) : sizeof(cl_float));

	// --
	// Face extrapolated half-timestep data
	// --

	if ( this->bContiguousFaceData )
	{
		oclBufferFaceExtrapolations = new COCLBuffer( "Face extrapolations", oclModel, false, true, ucFloatSize * 4 * 4 * pDomain->getCellCount(), true );
		oclBufferFaceExtrapolations->createBuffer();
	} else {
		oclBufferFaceExtrapolationN = new COCLBuffer( "Face extrapolations N", oclModel, false, true, ucFloatSize * 4 * pDomain->getCellCount(), true );
		oclBufferFaceExtrapolationE = new COCLBuffer( "Face extrapolations E", oclModel, false, true, ucFloatSize * 4 * pDomain->getCellCount(), true );
		oclBufferFaceExtrapolationS = new COCLBuffer( "Face extrapolations S", oclModel, false, true, ucFloatSize * 4 * pDomain->getCellCount(), true );
		oclBufferFaceExtrapolationW = new COCLBuffer( "Face extrapolations W", oclModel, false, true, ucFloatSize * 4 * pDomain->getCellCount(), true );
		oclBufferFaceExtrapolationN->createBuffer();
		oclBufferFaceExtrapolationE->createBuffer();
		oclBufferFaceExtrapolationS->createBuffer();
		oclBufferFaceExtrapolationW->createBuffer();
	}

	return bReturnState;
}

/*
 *  Create kernels using the compiled program
 */
bool CSchemeMUSCLHancock::prepare2OKernels()
{
	bool						bReturnState		= true;
	CExecutorControlOpenCL*		pExecutor			= pManager->getExecutor();
	CDomain*					pDomain				= this->pDomain;
	COCLDevice*					pDevice				= pExecutor->getDevice();

	// --
	// MUSCL-Hancock main scheme kernels
	// --

	if ( this->ucConfiguration == model::schemeConfigurations::musclHancock::kCacheMaximum )
	{

			oclKernelFullTimestep = oclModel->getKernel( "mch_cacheMaximum" );
			oclKernelFullTimestep->setGroupSize( this->ulCachedWorkgroupSizeX, this->ulCachedWorkgroupSizeY );
			oclKernelFullTimestep->setGlobalSize( this->ulCachedGlobalSizeX, this->ulCachedGlobalSizeY );

			COCLBuffer* aryArgsFullTimestep[] = { oclBufferTimestep, oclBufferCellStates, oclBufferCellBed, oclBufferCellManning };	
			oclKernelFullTimestep->assignArguments( aryArgsFullTimestep );

	}
	else if ( this->ucConfiguration == model::schemeConfigurations::musclHancock::kCachePrediction )
	{

		oclKernelHalfTimestep = oclModel->getKernel( "mch_1st_cachePrediction" );
		oclKernelHalfTimestep->setGroupSize( this->ulCachedWorkgroupSizeX, this->ulCachedWorkgroupSizeY );
		oclKernelHalfTimestep->setGlobalSize( this->ulCachedGlobalSizeX, this->ulCachedGlobalSizeY );
		oclKernelFullTimestep = oclModel->getKernel( "mch_2nd_cacheNone" );
		oclKernelFullTimestep->setGroupSize( this->ulNonCachedWorkgroupSizeX, this->ulNonCachedWorkgroupSizeY );
		oclKernelFullTimestep->setGlobalSize( this->ulNonCachedGlobalSizeX, this->ulNonCachedGlobalSizeY );

		if ( this->bContiguousFaceData )
		{
			COCLBuffer* aryArgsHalfTimestep[] = { oclBufferTimestep, oclBufferCellBed, oclBufferCellStates, oclBufferFaceExtrapolations };	
			COCLBuffer* aryArgsFullTimestep[] = { oclBufferTimestep, oclBufferCellStates, oclBufferCellBed, oclBufferCellManning, oclBufferFaceExtrapolations };	
			oclKernelHalfTimestep->assignArguments( aryArgsHalfTimestep );
			oclKernelFullTimestep->assignArguments( aryArgsFullTimestep );
		} else {
			COCLBuffer* aryArgsHalfTimestep[] = { oclBufferTimestep, oclBufferCellBed, oclBufferCellStates, oclBufferFaceExtrapolationN, oclBufferFaceExtrapolationE, oclBufferFaceExtrapolationS, oclBufferFaceExtrapolationW };	
			COCLBuffer* aryArgsFullTimestep[] = { oclBufferTimestep, oclBufferCellStates, oclBufferCellBed, oclBufferCellManning, oclBufferFaceExtrapolationN, oclBufferFaceExtrapolationE, oclBufferFaceExtrapolationS, oclBufferFaceExtrapolationW };	
			oclKernelHalfTimestep->assignArguments( aryArgsHalfTimestep );
			oclKernelFullTimestep->assignArguments( aryArgsFullTimestep );
		}
	} 
	else 
	{

		oclKernelHalfTimestep = oclModel->getKernel( "mch_1st_cacheNone" );
		oclKernelHalfTimestep->setGroupSize( this->ulNonCachedWorkgroupSizeX, this->ulNonCachedWorkgroupSizeY );
		oclKernelHalfTimestep->setGlobalSize( this->ulNonCachedGlobalSizeX, this->ulNonCachedGlobalSizeY );
		oclKernelFullTimestep = oclModel->getKernel( "mch_2nd_cacheNone" );
		oclKernelFullTimestep->setGroupSize( this->ulNonCachedWorkgroupSizeX, this->ulNonCachedWorkgroupSizeY );
		oclKernelFullTimestep->setGlobalSize( this->ulNonCachedGlobalSizeX, this->ulNonCachedGlobalSizeY );

		if ( this->bContiguousFaceData )
		{
			COCLBuffer* aryArgsHalfTimestep[] = { oclBufferTimestep, oclBufferCellBed, oclBufferCellStates, oclBufferFaceExtrapolations };	
			COCLBuffer* aryArgsFullTimestep[] = { oclBufferTimestep, oclBufferCellStates, oclBufferCellBed, oclBufferCellManning, oclBufferFaceExtrapolations };	
			oclKernelHalfTimestep->assignArguments( aryArgsHalfTimestep );
			oclKernelFullTimestep->assignArguments( aryArgsFullTimestep );
		} else {
			COCLBuffer* aryArgsHalfTimestep[] = { oclBufferTimestep, oclBufferCellBed, oclBufferCellStates, oclBufferFaceExtrapolationN, oclBufferFaceExtrapolationE, oclBufferFaceExtrapolationS, oclBufferFaceExtrapolationW };	
			COCLBuffer* aryArgsFullTimestep[] = { oclBufferTimestep, oclBufferCellStates, oclBufferCellBed, oclBufferCellManning, oclBufferFaceExtrapolationN, oclBufferFaceExtrapolationE, oclBufferFaceExtrapolationS, oclBufferFaceExtrapolationW };	
			oclKernelHalfTimestep->assignArguments( aryArgsHalfTimestep );
			oclKernelFullTimestep->assignArguments( aryArgsFullTimestep );
		}

	}

	return bReturnState;
}

/*
 *  Release all OpenCL resources consumed using the OpenCL methods
 */
void CSchemeMUSCLHancock::releaseResources()
{
	this->bReady = false;

	pManager->log->writeLine("Releasing scheme resources held for OpenCL.");

	this->release2OResources();
	this->release1OResources();
}

/*
 *  Release all OpenCL resources consumed using the OpenCL methods
 */
void CSchemeMUSCLHancock::release2OResources()
{
	this->bReady = false;

	pManager->log->writeLine("Releasing 2nd-order scheme resources held for OpenCL.");

	if ( this->oclKernelHalfTimestep != NULL )				delete oclKernelHalfTimestep;
	if ( this->oclBufferFaceExtrapolations != NULL )		delete oclBufferFaceExtrapolations;
	if ( this->oclBufferFaceExtrapolationN != NULL )		delete oclBufferFaceExtrapolationN;
	if ( this->oclBufferFaceExtrapolationE != NULL )		delete oclBufferFaceExtrapolationE;
	if ( this->oclBufferFaceExtrapolationS != NULL )		delete oclBufferFaceExtrapolationS;
	if ( this->oclBufferFaceExtrapolationW != NULL )		delete oclBufferFaceExtrapolationW;

	oclKernelHalfTimestep			= NULL;
	oclBufferFaceExtrapolations		= NULL;
	oclBufferFaceExtrapolationN		= NULL;
	oclBufferFaceExtrapolationE		= NULL;
	oclBufferFaceExtrapolationS		= NULL;
	oclBufferFaceExtrapolationW		= NULL;
}

/*
 *  Set the cache configuration to use
 */
void	CSchemeMUSCLHancock::setCacheMode( unsigned char ucCacheMode )
{
	this->ucConfiguration = ucCacheMode;
}

/*
 *  Get the cache configuration in use
 */
unsigned char	CSchemeMUSCLHancock::getCacheMode()
{
	return this->ucConfiguration;
}

/*
 *  Set the cache constraints
 */
void	CSchemeMUSCLHancock::setCacheConstraints( unsigned char ucCacheConstraints )
{
	this->ucCacheConstraints = ucCacheConstraints;
}

/*
 *  Get the cache constraints
 */
unsigned char	CSchemeMUSCLHancock::getCacheConstraints()
{
	return this->ucCacheConstraints;
}

/*
 *  Runs the actual simulation until completion or error
 */
void	CSchemeMUSCLHancock::scheduleIteration(
					bool						bUseAlternateKernel,
					COCLDevice*					pDevice,
					CDomain*					pDomain
		)
{
	// Half-timestep and full-timestep kernels
	if ( this->ucConfiguration != model::schemeConfigurations::musclHancock::kCacheMaximum )
	{
		oclKernelHalfTimestep->scheduleExecution();
		pDevice->queueBarrier();
		oclKernelFullTimestep->scheduleExecution();	
	} else {
		oclKernelFullTimestep->scheduleExecution();	
	}
	pDevice->queueBarrier();

	// Friction
	if ( this->bFrictionEffects && !this->bFrictionInFluxKernel )
	{
		oclKernelFriction->scheduleExecution();
		pDevice->queueBarrier();
	}

	// Run the boundary kernels (each bndy has its own kernel now)
	pDomain->getBoundaries()->applyBoundaries(oclBufferCellStates);
	pDevice->queueBarrier();

	// Timestep reduction
	if ( this->bDynamicTimestep )
	{
		oclKernelTimestepReduction->scheduleExecution();
		pDevice->queueBarrier();
	}

	// Time advancing
	oclKernelTimeAdvance->scheduleExecution();
	pDevice->queueBarrier();
}


/*
 *  Set contiguous face data enabled state
 */
void	CSchemeMUSCLHancock::setExtrapolatedContiguity( bool bContiguous )
{
	this->bContiguousFaceData = bContiguous;
}

/*
 *  Get contiguous face data enabled state
 */
bool	CSchemeMUSCLHancock::getExtrapolatedContiguity()
{
	return this->bContiguousFaceData;
}

/*
*	Fetch the pointer to the last cell source buffer
*/
COCLBuffer* CSchemeMUSCLHancock::getLastCellSourceBuffer()
{
	// TODO: Max caching should return alternating like 
	// the 1st order scheme
	return oclBufferCellStates;
}

/*
*	Fetch the pointer to the next cell source buffer
*/
COCLBuffer* CSchemeMUSCLHancock::getNextCellSourceBuffer()
{
	// TODO: Max caching should return alternating like 
	// the 1st order scheme
	return oclBufferCellStates;
}

/*
*  Read back all of the domain data
*/
void CSchemeMUSCLHancock::readDomainAll()
{
	// There's only one cell state buffer in the MUSCL-Hancock scheme
	// unless we're using the maximum caching version
	switch (this->ucConfiguration)
	{
		default:
			oclBufferCellStates->queueReadAll();
		break;
		case model::schemeConfigurations::musclHancock::kCacheMaximum:
			if (bUseAlternateKernel)
			{
				oclBufferCellStatesAlt->queueReadAll();
			}
			else {
				oclBufferCellStates->queueReadAll();
			}
		break;
	}
}