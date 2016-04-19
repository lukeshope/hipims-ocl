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
 *  Domain Class: Cartesian grid
 * ------------------------------------------
 *
 */
#include <limits>
#include <stdio.h>
#include <cstring>
#include <boost/lexical_cast.hpp>

#include "../../common.h"
#include "../../main.h"
#include "../../CModel.h"
#include "../CDomainManager.h"
#include "../CDomain.h"
#include "../../Schemes/CScheme.h"
#include "../../Datasets/CXMLDataset.h"
#include "../../Datasets/CRasterDataset.h"
#include "../../OpenCL/Executors/CExecutorControlOpenCL.h"
#include "../../Boundaries/CBoundaryMap.h"
#include "../../MPI/CMPIManager.h"
#include "CDomainCartesian.h"

/*
 *  Constructor
 */
CDomainCartesian::CDomainCartesian(void)
{
	// Default values will trigger errors on validation
	this->dCellResolution			= std::numeric_limits<double>::quiet_NaN();
	this->dRealDimensions[kAxisX]	= std::numeric_limits<double>::quiet_NaN();
	this->dRealDimensions[kAxisY]	= std::numeric_limits<double>::quiet_NaN();
	this->dRealExtent[kEdgeN]		= std::numeric_limits<double>::quiet_NaN();
	this->dRealExtent[kEdgeE]		= std::numeric_limits<double>::quiet_NaN();
	this->dRealExtent[kEdgeS]		= std::numeric_limits<double>::quiet_NaN();
	this->dRealExtent[kEdgeW]		= std::numeric_limits<double>::quiet_NaN();
	this->dRealOffset[kAxisX]		= std::numeric_limits<double>::quiet_NaN();
	this->dRealOffset[kAxisY]		= std::numeric_limits<double>::quiet_NaN();
	this->cUnits[0]					= 0;
	this->ulProjectionCode			= 0;
	this->cTargetDir				= NULL;
	this->cSourceDir				= NULL;
}

/*
 *  Destructor
 */
CDomainCartesian::~CDomainCartesian(void)
{
	// ...
}

/*
 *  Configure the domain using the XML file
 */
bool CDomainCartesian::configureDomain( XMLElement* pXDomain )
{
	XMLElement* pXData;
	XMLElement* pXScheme;
	XMLElement* pXDataSource;

	char	*cSourceType = NULL, 
			*cSourceValue = NULL,
			*cSourceFile = NULL;

	// Call the base-class configuration loading stuff first
	// which will address the device ID and the source/target
	// dirs.
	if ( !CDomain::configureDomain( pXDomain ) )
		return false;

	pXData = pXDomain->FirstChildElement( "data" );
	pXDataSource	= pXData->FirstChildElement("dataSource");

	while ( pXDataSource != NULL )
	{
		Util::toLowercase( &cSourceType,  pXDataSource->Attribute( "type" ) );
		Util::toLowercase( &cSourceValue, pXDataSource->Attribute( "value" ) );
		Util::toNewString( &cSourceFile,  pXDataSource->Attribute( "source" ) );

		// TODO: What if there is no structure definition? Should try the DEM instead?
		if ( strstr( cSourceValue, "structure" ) != NULL )
		{
			if ( strcmp( cSourceType, "raster" ) != 0 )
			{
				model::doError(
					"Domain structure can only be loaded from a raster.",
					model::errorCodes::kLevelWarning
				);
				return false;
			}

			CRasterDataset	pDataset;
			pManager->log->writeLine( "Attempting to read domain structure data." );
			pDataset.openFileRead( std::string( cSourceDir ) + std::string( cSourceFile ) );
			pManager->log->writeLine( "Successfully opened domain dataset for structure data." );
			pDataset.logDetails();
			
			pDataset.applyDimensionsToDomain( this );
		}

		pXDataSource = pXDataSource->NextSiblingElement("dataSource");
	}

	pManager->log->writeLine( "Progressing to load boundary conditions." );
	if ( !this->getBoundaries()->setupFromConfig( pXDomain ) )
		return false;

	pXScheme = pXDomain->FirstChildElement( "scheme" );
	if ( pXScheme == NULL )
	{
		model::doError(
			"The <scheme> element is missing.",
			model::errorCodes::kLevelWarning
		);
		return false;
	} else {
		pScheme = CScheme::createFromConfig( pXScheme );
		pScheme->setupFromConfig( pXScheme );
		pScheme->setDomain( this );
		pScheme->prepareAll();
		setScheme( pScheme );

		if ( !pScheme->isReady() )
		{
			model::doError(
				"Numerical scheme is not ready. Check errors.",
				model::errorCodes::kLevelWarning
			);
			return false;
		} else {
			pManager->log->writeLine( "Numerical scheme reports it is ready." );
		}
	}

	pManager->log->writeLine( "Progressing to load initial conditions." );
	if ( !this->loadInitialConditions( pXData ) )
		return false;

	pManager->log->writeLine( "Progressing to load output file definitions." );
	if ( !this->loadOutputDefinitions( pXData ) )
		return false;

	return true;
}

/*
 *  Load all the initial conditions and data from rasters or constant definitions
 */
bool	CDomainCartesian::loadInitialConditions( XMLElement* pXData )
{
	bool							bSourceVelX			= false, 
									bSourceVelY			= false, 
									bSourceManning		= false;
	sDataSourceInfo					pDataDEM;
	sDataSourceInfo					pDataDepth;
	std::vector<sDataSourceInfo>	pDataOther;
	XMLElement*						pDataSource			= pXData->FirstChildElement( "dataSource" );

	pDataDEM.ucValue		= 255;
	pDataDepth.ucValue		= 255;

	// Read data source information but don't process it, because
	// a specific order is required...
	while ( pDataSource != NULL )
	{
		char			*cSourceType = NULL, *cSourceValue = NULL, *cSourceFile = NULL;
		unsigned char	ucValueType;
		Util::toLowercase( &cSourceType, pDataSource->Attribute( "type" ) );
		Util::toLowercase( &cSourceValue, pDataSource->Attribute( "value" ) );
		Util::toNewString( &cSourceFile, pDataSource->Attribute( "source" ) );

		ucValueType  = this->getDataValueCode( cSourceValue );

		sDataSourceInfo pDataInfo;
		pDataInfo.cSourceType	= cSourceType;
		pDataInfo.cFileValue	= cSourceFile;
		pDataInfo.ucValue		= ucValueType;

		switch( ucValueType )
		{
			case model::rasterDatasets::dataValues::kBedElevation:
				pDataDEM	= pDataInfo;
				break;
			case model::rasterDatasets::dataValues::kDepth:
			case model::rasterDatasets::dataValues::kFreeSurfaceLevel:
				pDataDepth	= pDataInfo;
				break;
			case model::rasterDatasets::dataValues::kDischargeX:
			case model::rasterDatasets::dataValues::kVelocityX:
				pDataOther.push_back( pDataInfo );
				bSourceVelX = true;
				break;
			case model::rasterDatasets::dataValues::kDischargeY:
			case model::rasterDatasets::dataValues::kVelocityY:
				pDataOther.push_back( pDataInfo );
				bSourceVelY = true;
				break;
			case model::rasterDatasets::dataValues::kManningCoefficient:
				pDataOther.push_back( pDataInfo );
				bSourceManning = true;
				break;
			default:
				pDataOther.push_back( pDataInfo );
				break;
		}

		pDataSource = pDataSource->NextSiblingElement( "dataSource" );
	}

	// Need a DEM and a depth data source as a minimum
	if ( pDataDEM.ucValue == 255 || pDataDepth.ucValue == 255 )
	{
		model::doError(
			"Missing DEM or depth data source.",
			model::errorCodes::kLevelWarning
		);
	}

	// Present warnings if some things aren't defined
	if ( !bSourceVelX )
		model::doError(
			"No source defined for X-velocity - assuming zero.",
			model::errorCodes::kLevelWarning
		);
	if ( !bSourceVelY )
		model::doError(
			"No source defined for Y-velocity - assuming zero.",
			model::errorCodes::kLevelWarning
		);
	if ( !bSourceManning )
		model::doError(
			"No source defined for Manning coefficient - assuming zero.",
			model::errorCodes::kLevelWarning
		);

	// Process the initial conditions in the order:
	// 1. DEM
	// 2. Depth/FSL
	// 3. All others
	if ( !this->loadInitialConditionSource( pDataDEM,	cSourceDir ) )
	{
		model::doError(
			"Could not load DEM data.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}
	if ( !this->loadInitialConditionSource( pDataDepth,	cSourceDir ) )
	{
		model::doError(
			"Could not load depth/FSL data.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}
	for ( unsigned int i = 0; i < pDataOther.size(); ++i )
	{
		if ( !this->loadInitialConditionSource( pDataOther[i], cSourceDir ) ) 
		{
			model::doError(
				"Could not load initial conditions.",
				model::errorCodes::kLevelWarning
			);
			return false;
		}
	}

	return true;
}

/*
 *  Load the output definitions for what should be written to disk
 */
bool	CDomainCartesian::loadOutputDefinitions( XMLElement* pXData )
{
	XMLElement*		pDataTarget		= pXData->FirstChildElement("dataTarget");
	char			*cOutputType    = NULL, 
					*cOutputValue   = NULL,
					*cOutputFormat  = NULL,
					*cOutputFile    = NULL;

	while ( pDataTarget != NULL )
	{
		Util::toLowercase( &cOutputType,   pDataTarget->Attribute( "type" ) );
		Util::toLowercase( &cOutputValue,  pDataTarget->Attribute( "value" ) );
		Util::toNewString( &cOutputFormat, pDataTarget->Attribute( "format" ) );
		Util::toNewString( &cOutputFile,   pDataTarget->Attribute( "target" ) );

		if ( cOutputType   == NULL || 
			 cOutputValue  == NULL || 
			 cOutputFormat == NULL || 
			 cOutputFile   == NULL )
		{
			model::doError(
				"Output definition is missing data.",
				model::errorCodes::kLevelWarning
			);
			return false;
		}

		if ( strcmp( cOutputType, "raster" ) == 0 )
		{
			sDataTargetInfo	pOutput;

			pOutput.cFormat	= cOutputFormat;
			pOutput.cType   = cOutputType;
			pOutput.sTarget = std::string( cTargetDir ) + std::string( cOutputFile );
			pOutput.ucValue = this->getDataValueCode( cOutputValue );

			addOutput( pOutput );
		} else {
			// TODO: Allow for timeseries outputs in specific cells etc.
			model::doError(
				"An invalid output format type was given.",
				model::errorCodes::kLevelWarning
			);
		}

		pDataTarget = pDataTarget->NextSiblingElement("dataTarget");
	}

	pManager->log->writeLine( "Identified " + toString( this->pOutputs.size() ) + " output file definition(s)." );

	return true;
}

/*
 *  Read a data source raster or constant using the pre-parsed data held in the structure
 */
bool	CDomainCartesian::loadInitialConditionSource( sDataSourceInfo pDataSource, char* cDataDir )
{
	if ( strcmp( pDataSource.cSourceType, "raster" ) == 0 )
	{
		CRasterDataset	pDataset;
		pDataset.openFileRead( 
			std::string( cDataDir ) + std::string( pDataSource.cFileValue ) 
		);
		return pDataset.applyDataToDomain( pDataSource.ucValue, this );
	} 
	else if ( strcmp( pDataSource.cSourceType, "constant" ) == 0 )
	{
		if ( !CXMLDataset::isValidFloat( pDataSource.cFileValue ) )
		{
			model::doError(
				"Invalid source constant given.",
				model::errorCodes::kLevelWarning
			);
			return false;
		}
		double	dValue = boost::lexical_cast<double>( pDataSource.cFileValue );

		// NOTE: The outer layer of cells are exempt here, as they are not computed
		// but there may be some circumstances where we want a value here?
		for( unsigned long i = 0; i < this->getCols(); i++ )
		{
			for( unsigned long j = 0; j < this->getRows(); j++ )
			{
				unsigned long ulCellID = this->getCellID( i, j );
				if (i <= 0 || 
					j <= 0 ||
					i >= this->getCols() - 1 || 
					j >= this->getRows() - 1)
				{
					double dEdgeValue = 0.0;
					if (pDataSource.ucValue == model::rasterDatasets::dataValues::kFreeSurfaceLevel)
						dEdgeValue = this->getBedElevation( ulCellID );
					this->handleInputData(
						ulCellID,
						dEdgeValue,
						pDataSource.ucValue,
						4	// TODO: Allow rounding to be configured for source constants
					);
				}
				else 
				{
					this->handleInputData(
						ulCellID,
						dValue,
						pDataSource.ucValue,
						4	// TODO: Allow rounding to be configured for source constants
					);
				}
			}
		}
	} 
	else 
	{
		model::doError(
			"Unrecognised data source type.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	return true;
}

/*
 *  Does the domain contain all of the required data yet?
 */
bool	CDomainCartesian::validateDomain( bool bQuiet )
{
	// Got a resolution?
	if ( this->dCellResolution == NAN )
	{
		if ( !bQuiet ) model::doError(
			"Domain cell resolution not defined",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	// Got a size?
	if ( ( isnan( this->dRealDimensions[ kAxisX ] ) || 
		   isnan( this->dRealDimensions[ kAxisY ] ) ) &&
		 ( isnan( this->dRealExtent[ kEdgeN ] ) || 
		   isnan( this->dRealExtent[ kEdgeE ] ) || 
		   isnan( this->dRealExtent[ kEdgeS ] ) || 
		   isnan( this->dRealExtent[ kEdgeW ] ) ) )
	{
		if ( !bQuiet ) model::doError(
			"Domain extent not defined",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	// Got an offset?
	if ( isnan( this->dRealOffset[ kAxisX ] ) ||
		 isnan( this->dRealOffset[ kAxisY ] ) )
	{
		if ( !bQuiet ) model::doError(
			"Domain offset not defined",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	// Valid extent?
	if ( this->dRealExtent[ kEdgeE ] <= this->dRealExtent[ kEdgeW ] ||
		 this->dRealExtent[ kEdgeN ] <= this->dRealExtent[ kEdgeS ] )
	{
		if ( !bQuiet ) model::doError(
			"Domain extent is not valid",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	return true;
}

/*
 *  Create the required data structures etc.
 */
void	CDomainCartesian::prepareDomain()
{
	if ( !this->validateDomain( true ) ) 
	{
		model::doError(
			"Cannot prepare the domain. Invalid specification.",
			model::errorCodes::kLevelModelStop
		);
		return;
	}

	this->bPrepared = true;
	this->logDetails();
}

/*
 *  Write useful information about this domain to the log file
 */
void	CDomainCartesian::logDetails()
{
	pManager->log->writeDivide();
	unsigned short	wColour			= model::cli::colourInfoBlock;

	pManager->log->writeLine( "REGULAR CARTESIAN GRID DOMAIN", true, wColour );
	if ( this->ulProjectionCode > 0 ) 
	{
		pManager->log->writeLine( "  Projection:        EPSG:" + toString( this->ulProjectionCode ), true, wColour );
	} else {
		pManager->log->writeLine( "  Projection:        Unknown", true, wColour );
	}
	pManager->log->writeLine( "  Device number:     " + toString( this->pDevice->uiDeviceNo ), true, wColour );
	pManager->log->writeLine( "  Cell count:        " + toString( this->ulCellCount ), true, wColour );
	pManager->log->writeLine( "  Cell resolution:   " + toString( this->dCellResolution ) + this->cUnits, true, wColour );
	pManager->log->writeLine( "  Cell dimensions:   [" + toString( this->ulCols ) + ", " + 
														 toString( this->ulRows ) + "]", true, wColour );
	pManager->log->writeLine( "  Real dimensions:   [" + toString( this->dRealDimensions[ kAxisX ] ) + this->cUnits + ", " + 
														 toString( this->dRealDimensions[ kAxisY ] ) + this->cUnits + "]", true, wColour );

	pManager->log->writeDivide();
}

/*
 *  Set real domain dimensions (X, Y)
 */
void	CDomainCartesian::setRealDimensions( double dSizeX, double dSizeY )
{
	this->dRealDimensions[ kAxisX ]	= dSizeX;
	this->dRealDimensions[ kAxisY ]	= dSizeY;
	this->updateCellStatistics();
}

/*	
 *  Fetch real domain dimensions (X, Y)
 */
void	CDomainCartesian::getRealDimensions( double* dSizeX, double* dSizeY )
{
	*dSizeX = this->dRealDimensions[ kAxisX ];
	*dSizeY = this->dRealDimensions[ kAxisY ];
}

/*
 *  Set real domain offset (X, Y) for lower-left corner
 */
void	CDomainCartesian::setRealOffset( double dOffsetX, double dOffsetY )
{
	this->dRealOffset[ kAxisX ]	= dOffsetX;
	this->dRealOffset[ kAxisY ]	= dOffsetY;
}

/*
 *  Fetch real domain offset (X, Y) for lower-left corner
 */
void	CDomainCartesian::getRealOffset( double* dOffsetX, double* dOffsetY )
{
	*dOffsetX = this->dRealOffset[ kAxisX ];
	*dOffsetY = this->dRealOffset[ kAxisY ];
}

/*
 *  Set real domain extent (Clockwise: N, E, S, W)
 */
void	CDomainCartesian::setRealExtent( double dEdgeN, double dEdgeE, double dEdgeS, double dEdgeW )
{
	this->dRealExtent[ kEdgeN ]	= dEdgeN;
	this->dRealExtent[ kEdgeE ]	= dEdgeE;
	this->dRealExtent[ kEdgeS ]	= dEdgeS;
	this->dRealExtent[ kEdgeW ]	= dEdgeW;
	//this->updateCellStatistics();
}

/*
 *  Fetch real domain extent (Clockwise: N, E, S, W)
 */
void	CDomainCartesian::getRealExtent( double* dEdgeN, double* dEdgeE, double* dEdgeS, double* dEdgeW ) 
{
	*dEdgeN = this->dRealExtent[kEdgeN];
	*dEdgeE = this->dRealExtent[kEdgeE];
	*dEdgeS = this->dRealExtent[kEdgeS];
	*dEdgeW = this->dRealExtent[kEdgeW];
}

/*
 *  Set cell resolution
 */
void	CDomainCartesian::setCellResolution( double dResolution )
{
	this->dCellResolution = dResolution;
	this->updateCellStatistics();
}

/*
 *   Fetch cell resolution
 */
void	CDomainCartesian::getCellResolution( double* dResolution )
{
	*dResolution = this->dCellResolution;
}

/*
 *  Set domain units
 */
void	CDomainCartesian::setUnits( char* cUnits )
{
	if ( std::strlen( cUnits ) > 2 ) 
	{
		model::doError(
			"Domain units can only be two characters",
			model::errorCodes::kLevelWarning
		);
		return;
	}

	// Store the units (2 char max!)
	std::strcpy( &this->cUnits[0], cUnits );
}

/*
 *  Return a couple of characters representing the units in use
 */
char*	CDomainCartesian::getUnits()
{
	return &this->cUnits[0];
}

/*
 *  Set the EPSG projection code currently in use
 *  0 = Not defined
 */
void	CDomainCartesian::setProjectionCode( unsigned long ulProjectionCode )
{
	this->ulProjectionCode = ulProjectionCode;
}

/*
 *  Return the EPSG projection code currently in use
 */
unsigned long	CDomainCartesian::getProjectionCode()
{
	return this->ulProjectionCode;
}

/*
 *  Update basic statistics on the number of cells etc.
 */
void	CDomainCartesian::updateCellStatistics()
{
	// Do we have enough information to proceed?...

	// Got a resolution?
	if ( this->dCellResolution == NAN )
	{
		return;
	}

	// Got a size?
	if ( ( isnan( this->dRealDimensions[ kAxisX ] ) || 
		   isnan( this->dRealDimensions[ kAxisY ] ) ) &&
		 ( isnan( this->dRealExtent[ kEdgeN ] ) || 
		   isnan( this->dRealExtent[ kEdgeE ] ) || 
		   isnan( this->dRealExtent[ kEdgeS ] ) || 
		   isnan( this->dRealExtent[ kEdgeW ] ) ) )
	{
		return;
	}

	// We've got everything we need...
	this->ulRows	= static_cast<unsigned long>(
		this->dRealDimensions[ kAxisY ] / this->dCellResolution
	);
	this->ulCols	= static_cast<unsigned long>(
		this->dRealDimensions[ kAxisX ] / this->dCellResolution
	);
	this->ulCellCount = this->ulRows * this->ulCols;
}

/*
 *  Return the total number of rows in the domain
 */
unsigned long	CDomainCartesian::getRows()
{
	return this->ulRows;
}

/*
 *  Return the total number of columns in the domain
 */
unsigned long	CDomainCartesian::getCols()
{
	return this->ulCols;
}

/*
 *  Get a cell ID from an X and Y index
 */
unsigned long	CDomainCartesian::getCellID( unsigned long ulX, unsigned long ulY )
{
	return ( ulY * this->getCols() ) + ulX;
}

/*
 *  Get a cell ID from an X and Y coordinate
 */
unsigned long	CDomainCartesian::getCellFromCoordinates( double dX, double dY )
{
	unsigned long ulX	= floor( ( dX - dRealOffset[ 0 ] ) / dCellResolution );
	unsigned long ulY	= floor( ( dY - dRealOffset[ 2 ] ) / dCellResolution );
	return getCellID( ulX, ulY );
}

#ifdef _WINDLL
/*
 *  Send the topography to the renderer for visualisation purposes
 */
void	CDomainCartesian::sendAllToRenderer()
{
	// TODO: This needs fixing and changing to account for multi-domain stuff...
	if ( pManager->getDomainSet()->getDomainCount() > 1 )
		return;

	if ( !this->isInitialised() ||
		 ( this->ucFloatSize == 8 && this->dBedElevations == NULL ) ||
		 ( this->ucFloatSize == 4 && this->fBedElevations == NULL ) ||
		 ( this->ucFloatSize == 8 && this->dCellStates == NULL ) ||
		 ( this->ucFloatSize == 4 && this->fCellStates == NULL ) ) 
		return;

#ifdef _WINDLL
	if ( model::fSendTopography != NULL )
		model::fSendTopography(
			this->ucFloatSize == 8 ? 
				static_cast<void*>( this->dBedElevations ) :
				static_cast<void*>( this->fBedElevations ),
			this->ucFloatSize == 8 ? 
				static_cast<void*>( this->dCellStates ) : 
				static_cast<void*>( this->fCellStates ),
			this->ucFloatSize,
			this->ulCols,
			this->ulRows,
			this->dCellResolution,
			this->dCellResolution,
			this->dMinDepth,
			this->dMaxDepth,
			this->dMinTopo,
			this->dMaxTopo
		);
#endif
}
#endif


/*
 *  Calculate the total volume present in all of the cells
 */
double	CDomainCartesian::getVolume()
{
	double dVolume = 0.0;

	for( unsigned int i = 0; i < this->ulCellCount; ++i )
	{
		if ( this->isDoublePrecision() )
		{
			dVolume += ( this->dCellStates[i].s[0] - this->dBedElevations[i] ) *
					   this->dCellResolution * this->dCellResolution;
		} else {
			dVolume += ( this->fCellStates[i].s[0] - this->fBedElevations[i] ) *
					   this->dCellResolution * this->dCellResolution;
		}
	}

	return dVolume;
}

/*
 *  Add a new output
 */
void	CDomainCartesian::addOutput( sDataTargetInfo pOutput )
{
	this->pOutputs.push_back( pOutput );
}

/*
 *  Manipulate the topography to impose boundary conditions
 */
void	CDomainCartesian::imposeBoundaryModification(unsigned char ucDirection, unsigned char ucType)
{
	unsigned long ulMinX, ulMaxX, ulMinY, ulMaxY;

	if (ucDirection == edge::kEdgeE) 
		{ ulMinY = 0; ulMaxY = this->ulRows - 1; ulMinX = this->ulCols - 1; ulMaxX = this->ulCols - 1; };
	if (ucDirection == edge::kEdgeW)
		{ ulMinY = 0; ulMaxY = this->ulRows - 1; ulMinX = 0; ulMaxX = 0; };
	if (ucDirection == edge::kEdgeN)
		{ ulMinY = this->ulRows - 1; ulMaxY = this->ulRows - 1; ulMinX = 0; ulMaxX = this->ulCols - 1; };
	if (ucDirection == edge::kEdgeS)
		{ ulMinY = 0; ulMaxY = 0; ulMinX = 0; ulMaxX = this->ulCols - 1; };

	for (unsigned long x = ulMinX; x <= ulMaxX; x++)
	{
		for (unsigned long y = ulMinY; y <= ulMaxY; y++)
		{
			if (ucType == CDomainCartesian::boundaryTreatment::kBoundaryClosed)
			{
				this->setBedElevation(
					this->getCellID( x, y ),
					9999.9
				);
			}
		}
	}
}

/*
 *  Write output files to disk
 */
void	CDomainCartesian::writeOutputs()
{
	// Read the data back first...
	// TODO: Review whether this is necessary, isn't it a sync point anyway?
	pDevice->blockUntilFinished();
	pScheme->readDomainAll();
	pDevice->blockUntilFinished();

	for( unsigned int i = 0; i < this->pOutputs.size(); ++i )
	{
		// Replaces %t with the time in the filename, if required
		// TODO: Allow for decimal output filenames
		std::string sFilename		= this->pOutputs[i].sTarget;
		std::string sTime			= toString( floor( pScheme->getCurrentTime() * 100.0 ) / 100.0 );
		unsigned int uiTimeLocation = sFilename.find( "%t" );
		if ( uiTimeLocation != std::string::npos )
			sFilename.replace( uiTimeLocation, 2, sTime );

		CRasterDataset::domainToRaster(
			this->pOutputs[i].cFormat,
			sFilename,
			this,
			this->pOutputs[i].ucValue
		);
	}
}

/*
 *	Fetch summary information for this domain
 */
CDomainBase::DomainSummary CDomainCartesian::getSummary()
{
	CDomainBase::DomainSummary pSummary;
	
	pSummary.bAuthoritative = true;

	pSummary.uiDomainID		= this->uiID;
#ifdef MPI_ON
	pSummary.uiNodeID = pManager->getMPIManager()->getNodeID();
#else
	pSummary.uiNodeID = 0;
#endif
	pSummary.uiLocalDeviceID = this->getDevice()->getDeviceID();
	pSummary.dEdgeNorth		= this->dRealExtent[kEdgeN];
	pSummary.dEdgeEast		= this->dRealExtent[kEdgeE];
	pSummary.dEdgeSouth		= this->dRealExtent[kEdgeS];
	pSummary.dEdgeWest		= this->dRealExtent[kEdgeW];
	pSummary.ulColCount		= this->ulCols;
	pSummary.ulRowCount		= this->ulRows;
	pSummary.ucFloatPrecision = ( this->isDoublePrecision() ? model::floatPrecision::kDouble : model::floatPrecision::kSingle );
	pSummary.dResolution	= this->dCellResolution;

	return pSummary;
}

