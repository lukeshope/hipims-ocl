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
 *  Raster dataset handling class
 * ------------------------------------------
 *
 */
#include <boost/lexical_cast.hpp>
#include <algorithm>

#include "../common.h"
#include "CRasterDataset.h"
#include "../Domain/Cartesian/CDomainCartesian.h"

using std::min;
using std::max;

/*
 *  Default constructor
 */
CRasterDataset::CRasterDataset()
{
	this->bAvailable = false;
	this->gdDataset	 = NULL;
}

/*
 *  Destructor
 */
CRasterDataset::~CRasterDataset(void)
{
	if ( this->gdDataset != NULL )
		GDALClose( this->gdDataset );
}

/*
 *  Register types before doing anything else in GDAL
 */
void	CRasterDataset::registerAll()
{
	GDALAllRegister();
}

/*
 *  Cleanup all memory allocations (except GDAL still has leaks...)
 */
void	CRasterDataset::cleanupAll()
{
	try
	{
		VSICleanupFileManager();
		CPLFinderClean();
		CPLFreeConfig();
		GDALDestroyDriverManager();
	}
	catch (std::exception e) {}
}

/*
 *  Open a file as the underlying dataset
 */
bool	CRasterDataset::openFileRead( std::string sFilename )
{
	GDALDataset		*gdDataset;

	pManager->log->writeLine( "Invoking GDAL to open dataset." );
	gdDataset = static_cast<GDALDataset*>( GDALOpen( sFilename.c_str(), GA_ReadOnly ) );
	pManager->log->writeLine( "Handle on dataset established." );

	if ( gdDataset == NULL )
	{
		model::doError(
			"Unable to open raster dataset",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	pManager->log->writeLine( "Opened GDAL raster dataset from file." );

	this->gdDataset		= gdDataset;
	this->bAvailable	= true;
	this->readMetadata();
	return true;
}

/*
 *  Open a file for output
 */
bool	CRasterDataset::domainToRaster( 
			const char*			cDriver, 
			std::string			sFilename,
			CDomainCartesian*	pDomain,
			unsigned char		ucValue
		)
{
	// Get the driver and check it's capable of writing
	GDALDriver*		pDriver;
	GDALDataset*	pDataset;
	GDALRasterBand*	pBand;
	char**			czDriverMetadata;
	char**			czOptions;
	double			adfGeoTransform[6]		= { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	double			dResolution;
	double*			dRow;
	double			dDepth;
	double			dVelocityX, dVelocityY;
	unsigned long	ulCellID;
	std::string		sValueName;

	CRasterDataset::getValueDetails( ucValue, &sValueName );
	pManager->log->writeLine( "Writing " + sValueName + " to output raster file..." );

	pDriver = GetGDALDriverManager()->GetDriverByName( cDriver );

	if ( pDriver == NULL )
	{
		model::doError(
			"Unable to obtain driver for output.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	czDriverMetadata = pDriver->GetMetadata();
	if ( CSLFetchBoolean( czDriverMetadata, GDAL_DCAP_CREATE, false ) == 0 )
	{
		model::doError(
			"GDAL format driver does not support file creation.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	// Create and set metadata
	czOptions = NULL;
	pDataset = pDriver->Create(  sFilename.c_str(),			// Filename
								 pDomain->getCols(),		// X size
								 pDomain->getRows(),		// Y size
								 1,							// Bands
								 GDT_Float64,				// Data format
								 czOptions );				// Options
								 
	if ( pDataset == NULL )
	{
		model::doError(
			"Could not create output raster file.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	pDomain->getRealOffset( &adfGeoTransform[0], &adfGeoTransform[3] );
	pDomain->getCellResolution( &dResolution );

	adfGeoTransform[3] += dResolution * pDomain->getRows();	// TL offset instead of BL
	adfGeoTransform[1]  = +dResolution;
	adfGeoTransform[5]  = -dResolution;						// Y resolution has to be negative

	pDataset->SetGeoTransform( adfGeoTransform );

	// Write data (bottom to top for rows)
	pBand	= pDataset->GetRasterBand( 1 );
	pBand->SetNoDataValue( -9999.0 );

	dRow	= new double[ pDomain->getCols() ];
	for( unsigned long iRow = 0; iRow < pDomain->getRows(); ++iRow )
	{
		for( unsigned long iCol = 0; iCol < pDomain->getCols(); ++iCol )
		{
			ulCellID = pDomain->getCellID( iCol, iRow );
			dRow[iCol] = -9999.0;

			switch( ucValue )
			{
			case model::rasterDatasets::dataValues::kMaxFSL:
				dRow[ iCol ] = pDomain->getStateValue( 
					ulCellID,
					model::domainValueIndices::kValueMaxFreeSurfaceLevel
				);
				if ( dRow[ iCol ] < pDomain->getBedElevation( ulCellID ) + 1E-8 ) 
					dRow[ iCol ] = pBand->GetNoDataValue();
				if (pDomain->getBedElevation(ulCellID) > 9999.0)
					dRow[iCol] = pBand->GetNoDataValue();
				break;
			case model::rasterDatasets::dataValues::kFreeSurfaceLevel:
				dRow[ iCol ] = pDomain->getStateValue( 
					ulCellID,
					model::domainValueIndices::kValueFreeSurfaceLevel
				);
				if ( dRow[ iCol ] < pDomain->getBedElevation( ulCellID ) + 1E-8 ) 
					dRow[ iCol ] = pBand->GetNoDataValue();
				if (pDomain->getBedElevation(ulCellID) > 9999.0)
					dRow[iCol] = pBand->GetNoDataValue();
				break;
			case model::rasterDatasets::dataValues::kMaxDepth:
				dRow[ iCol ] = max( 0.0, pDomain->getStateValue( ulCellID, 
									model::domainValueIndices::kValueMaxFreeSurfaceLevel ) - 
									pDomain->getBedElevation( ulCellID ) );
				if ( dRow[ iCol ] < 1E-8 || dRow[iCol] <= -9990.0 || dRow[iCol] >= 9999.0 ) 
					dRow[ iCol ] = pBand->GetNoDataValue();
				break;
			case model::rasterDatasets::dataValues::kDepth:
				dRow[ iCol ] = max( 0.0, pDomain->getStateValue( ulCellID, 
									model::domainValueIndices::kValueFreeSurfaceLevel ) - 
									pDomain->getBedElevation( ulCellID ) );
				if ( dRow[ iCol ] < 1E-8 ) 
					dRow[ iCol ] = pBand->GetNoDataValue();
				break;
			case model::rasterDatasets::dataValues::kDischargeX:
				dRow[ iCol ] = pDomain->getStateValue( 
					ulCellID,
					model::domainValueIndices::kValueDischargeX
				) * dResolution;
				break;
			case model::rasterDatasets::dataValues::kDischargeY:
				dRow[ iCol ] = pDomain->getStateValue( 
					ulCellID,
					model::domainValueIndices::kValueDischargeY
				) * dResolution;
				break;
			case model::rasterDatasets::dataValues::kVelocityX:
				dDepth		 = pDomain->getStateValue( ulCellID,
									model::domainValueIndices::kValueFreeSurfaceLevel ) - 
							   pDomain->getBedElevation( ulCellID );
				dRow[ iCol ] = ( dDepth > 1E-8 ?
							   ( pDomain->getStateValue( ulCellID,
									model::domainValueIndices::kValueDischargeX ) / 
									dDepth ) :
							   ( pBand->GetNoDataValue() ) );
				break;
			case model::rasterDatasets::dataValues::kVelocityY:
				dDepth		 = pDomain->getStateValue( ulCellID,
									model::domainValueIndices::kValueFreeSurfaceLevel ) - 
							   pDomain->getBedElevation( ulCellID );
				dRow[ iCol ] = ( dDepth > 1E-8 ?
							   ( pDomain->getStateValue( ulCellID,
									model::domainValueIndices::kValueDischargeY ) / 
									dDepth ) :
							   ( pBand->GetNoDataValue() ) );
				break;
			case model::rasterDatasets::dataValues::kFroudeNumber:
				dDepth		 = pDomain->getStateValue( ulCellID,
									model::domainValueIndices::kValueFreeSurfaceLevel ) - 
							   pDomain->getBedElevation( ulCellID );
				dVelocityY	 = pDomain->getStateValue( ulCellID,
									model::domainValueIndices::kValueDischargeY ) / 
									dDepth;
				dVelocityX	 = pDomain->getStateValue( ulCellID,
									model::domainValueIndices::kValueDischargeX ) / 
									dDepth;
				dRow[ iCol ] = ( dDepth > 1E-8 ?
							   ( sqrt( dVelocityX*dVelocityX + dVelocityY*dVelocityY ) / sqrt( 9.81 * dDepth ) ) :
							   ( pBand->GetNoDataValue() ) );
				break;
			}
		}

		pBand->RasterIO( GF_Write,							// Flag
						 0,									// X offset
						 pDomain->getRows() - iRow - 1,		// Y offset
						 pDomain->getCols(),				// X size
						 1,									// Y size
						 dRow,								// Memory
						 pDomain->getCols(),				// X buffer size
						 1,									// Y buffer size
						 GDT_Float64,						// Data type
						 0,									// Pixel space
						 0 );								// Line space
	}
	delete [] dRow;
	
	GDALClose( (GDALDatasetH)pDataset );
	
	return true;
}

/*
 *  Read in metadata about the layer (e.g. resolution data)
 */
void	CRasterDataset::readMetadata()
{
	double			adfGeoTransform[6];
	GDALDriver*		gdDriver;

	if ( !this->bAvailable ) return;

	gdDriver		= this->gdDataset->GetDriver();
	
	this->cDriverDescription	= const_cast<char*>( gdDriver->GetDescription() );
	this->cDriverLongName		= const_cast<char*>( gdDriver->GetMetadataItem( GDAL_DMD_LONGNAME ) );
	this->ulColumns				= this->gdDataset->GetRasterXSize();
	this->ulRows				= this->gdDataset->GetRasterYSize();
	this->uiBandCount			= this->gdDataset->GetRasterCount();

	if ( this->gdDataset->GetGeoTransform( adfGeoTransform ) == CE_None )
	{

		this->dResolutionX		= fabs( adfGeoTransform[1] );
		this->dResolutionY		= fabs( adfGeoTransform[5] );
		this->dOffsetX			= adfGeoTransform[0];
		this->dOffsetY			= adfGeoTransform[3] - this->dResolutionY * this->ulRows;

	} else {

		model::doError(
			"No georeferencing data was found in the dataset.",
			model::errorCodes::kLevelWarning
		);

		// Assumed cell resolution, not good!
		this->dOffsetX			= 0.0;
		this->dOffsetY			= 0.0;
		this->dResolutionX		= 1.0;
		this->dResolutionY		= 1.0;

	}

	// TODO: Spatial reference systems (EPSG codes etc.) once needed by CDomain
}

/*
 *  Write details to the log file
 */
void	CRasterDataset::logDetails()
{
	if ( !this->bAvailable ) return;

	pManager->log->writeDivide();
	pManager->log->writeLine( "Dataset driver:      " + std::string( this->cDriverDescription ) );
	pManager->log->writeLine( "Dataset driver name: " + std::string( this->cDriverLongName ) );
	pManager->log->writeLine( "Dataset band count:  " + toString( this->uiBandCount ) );
	pManager->log->writeLine( "Cell dimensions:     [" + toString( this->ulColumns ) + ", " + toString( this->ulRows ) + "]" );
	pManager->log->writeLine( "Cell resolution:     [" + toString( this->dResolutionX ) + ", " + toString( this->dResolutionY ) + "]" );
	pManager->log->writeLine( "Lower-left offset:   [" + toString( this->dOffsetX ) + ", " + toString( this->dOffsetY ) + "]" );
	pManager->log->writeDivide();
}

/*
 *  Apply the dimensions taken from the raster dataset to form the domain class structure
 */
bool	CRasterDataset::applyDimensionsToDomain( CDomainCartesian*	pDomain )
{
	if ( !this->bAvailable ) return false;

	pManager->log->writeLine( "Dimensioning domain from raster dataset." );

	pDomain->setProjectionCode( 0 );					// Unknown
	pDomain->setUnits( "m" );							
	pDomain->setCellResolution( this->dResolutionX );	
	pDomain->setRealDimensions( this->dResolutionX * this->ulColumns, this->dResolutionY * this->ulRows );	
	pDomain->setRealOffset( this->dOffsetX, this->dOffsetY );			
	pDomain->setRealExtent( this->dOffsetY + this->dResolutionY * this->ulRows,
						    this->dOffsetX + this->dResolutionX * this->ulColumns,
							this->dOffsetY,
							this->dOffsetX );

	return true;
}

/*
 *  Apply some data to the domain from this raster's first band
 */
bool	CRasterDataset::applyDataToDomain( unsigned char ucValue, CDomainCartesian* pDomain )
{
	GDALRasterBand*	pBand;
	std::string		sValueName	= "unknown";
	unsigned char	ucDataIndex	= 0;
	unsigned char	ucRounding	= 4;			// decimal places

	if ( !this->bAvailable ) return false;
	if ( !this->isDomainCompatible( pDomain ) ) return false;

	CRasterDataset::getValueDetails( ucValue, &sValueName );
	pManager->log->writeLine( "Loading " + sValueName + " from raster dataset." );

	pBand = this->gdDataset->GetRasterBand( 1 );

	double*			dScanLine;
	double			dValue;
	unsigned long	ulCellID;
	for( unsigned long iRow = 0; iRow < this->ulRows; iRow++ )
	{
		dScanLine = (double*) CPLMalloc(sizeof( double ) * this->ulColumns );
		pBand->RasterIO( GF_Read,				// Flag
						 0,						// X offset
						 iRow,					// Y offset
						 this->ulColumns,		// X read size
						 1,						// Y read size
						 dScanLine,				// Target heap
						 this->ulColumns,		// X buffer size
						 1,						// Y buffer size
						 GDT_Float64,			// Data type
						 0,						// Pixel space
						 0 );					// Line space

		for( unsigned long iCol = 0; iCol < this->ulColumns; iCol++ )
		{
			dValue		= dScanLine[ iCol ];
			ulCellID	= pDomain->getCellID( iCol, this->ulRows - iRow - 1 );		// Scan lines start in the top left

			pDomain->handleInputData(
				ulCellID,
				dValue,
				ucValue,
				ucRounding
			);
		}

		CPLFree( dScanLine );
	}

	return true;
}

/*
 *  Is the domain the right dimension etc. to apply data from this raster?
 */
bool	CRasterDataset::isDomainCompatible( CDomainCartesian* pDomain )
{
	if ( pDomain->getCols() != this->ulColumns ) return false;
	if ( pDomain->getRows() != this->ulRows) return false;
	
	// Assume yes for now
	// TODO: Add extra checks
	return true;
}

/*
 *	Create a transformation structure between the loaded raster dataset and
 *	a gridded boundary
 */
CBoundaryGridded::SBoundaryGridTransform* CRasterDataset::createTransformationForDomain(CDomainCartesian* pDomain)
{
	CBoundaryGridded::SBoundaryGridTransform *pReturn = new CBoundaryGridded::SBoundaryGridTransform;
	double dDomainExtent[4], dDomainResolution;

	pDomain->getCellResolution(&dDomainResolution);
	pDomain->getRealExtent( &dDomainExtent[0], &dDomainExtent[1], &dDomainExtent[2], &dDomainExtent[3] );

	pReturn->dSourceResolution = this->dResolutionX;
	pReturn->dTargetResolution = dDomainResolution;

	pReturn->dOffsetWest  = -( fmod( (dDomainExtent[3] - this->dOffsetX), this->dResolutionX ) );
	pReturn->dOffsetSouth = -( fmod( (dDomainExtent[2] - this->dOffsetY), this->dResolutionX ) );

	pReturn->uiColumns = (unsigned int)( ceil(dDomainExtent[1] / this->dResolutionX) - floor(dDomainExtent[3] / this->dResolutionX) );
	pReturn->uiRows    = (unsigned int)( ceil(dDomainExtent[0] / this->dResolutionX) - floor(dDomainExtent[2] / this->dResolutionX) );

	// TODO: Add proper check here to make sure the two domains actually align and fully overlap
	// otherwise we're likely to segfault.

	pReturn->ulBaseWest  = (unsigned long)max( 0.0, floor( ( dDomainExtent[3] - this->dOffsetX ) / this->dResolutionX ) );
	pReturn->ulBaseSouth = (unsigned long)max( 0.0, floor( ( dDomainExtent[2] - this->dOffsetY ) / this->dResolutionX ) );

	return pReturn;
}

/*
 *	Create an array for use as a boundary condition
 */
double*		CRasterDataset::createArrayForBoundary( CBoundaryGridded::SBoundaryGridTransform *sTransform )
{
	double* dReturn = new double[ sTransform->uiColumns * sTransform->uiRows ]; 
	GDALRasterBand *pBand = this->gdDataset->GetRasterBand(1);

	double*			dScanLine;
	for (unsigned long iRow = 0; iRow < sTransform->uiRows; iRow++)
	{
		dScanLine = (double*)CPLMalloc(sizeof(double)* sTransform->uiColumns);
		pBand->RasterIO(
			GF_Read,						// Flag
			sTransform->ulBaseWest,			// X offset
			( this->ulRows - sTransform->ulBaseSouth - 1 ) - iRow,	// Y offset
			sTransform->uiColumns,			// X read size
			1,								// Y read size
			dScanLine,						// Target heap
			sTransform->uiColumns,			// X buffer size
			1,								// Y buffer size
			GDT_Float64,					// Data type
			0,								// Pixel space
			0								// Line space
		);

		memcpy(
			&dReturn[ iRow * sTransform->uiColumns ],
			dScanLine,
			sizeof( double ) * sTransform->uiColumns
		);

		CPLFree(dScanLine);
	}

	return dReturn;
}

/*
 *  Get some details for a data source type, like a full name we can use in the log
 */
void	CRasterDataset::getValueDetails( unsigned char ucValue, std::string* sValueName )
{
	switch( ucValue )
	{
	case model::rasterDatasets::dataValues::kBedElevation:
		*sValueName = "bed elevation";
		break;
	case model::rasterDatasets::dataValues::kDepth:
		*sValueName  = "depth";
		break;
	case model::rasterDatasets::dataValues::kFreeSurfaceLevel:
		*sValueName  = "free-surface level";
		break;
	case model::rasterDatasets::dataValues::kVelocityX:
		*sValueName  = "velocity in X-direction";
		break;
	case model::rasterDatasets::dataValues::kVelocityY:
		*sValueName  = "velocity in Y-direction";
		break;
	case model::rasterDatasets::dataValues::kDischargeX:
		*sValueName  = "discharge in X-direction";
		break;
	case model::rasterDatasets::dataValues::kDischargeY:
		*sValueName  = "discharge in Y-direction";
		break;
	case model::rasterDatasets::dataValues::kManningCoefficient:
		*sValueName = "manning coefficients";
		break;
	case model::rasterDatasets::dataValues::kDisabledCells:
		*sValueName  = "disabled cells";
		break;
	case model::rasterDatasets::dataValues::kMaxFSL:
		*sValueName  = "maximum FSL";
		break;
	case model::rasterDatasets::dataValues::kMaxDepth:
		*sValueName  = "maximum depth";
		break;
	case model::rasterDatasets::dataValues::kFroudeNumber:
		*sValueName  = "froude number";
		break;
	default:
		*sValueName  = "unknown value";
		break;
	}
}