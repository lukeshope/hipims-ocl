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
 *  Domain Class
 * ------------------------------------------
 *
 */
#include <boost/lexical_cast.hpp>

#include "../common.h"
#include "CDomain.h"
#include "Cartesian/CDomainCartesian.h"
#include "../Datasets/CXMLDataset.h"
#include "../Datasets/CRasterDataset.h"
#include "../Boundaries/CBoundaryMap.h"
#include "../Schemes/CScheme.h"
#include "../OpenCL/Executors/COCLDevice.h"

/*
 *  Constructor
 */
CDomain::CDomain(void)
{
	this->pScheme			= NULL;
	this->pDevice			= NULL;
	this->bPrepared			= false;
	this->ucFloatSize		= 0;
	this->dMinFSL			= 9999.0;
	this->dMaxFSL			= -9999.0;
	this->dMinTopo			= 9999.0;
	this->dMaxTopo			= -9999.0;
	this->dMinDepth			= 9999.0;
	this->dMaxDepth			= -9999.0;
	this->uiRollbackLimit	= 999999999;

	this->cTargetDir				= NULL;
	this->cSourceDir				= NULL;

	this->pBoundaries = new CBoundaryMap( this );
}

/*
 *  Destructor
 */
CDomain::~CDomain(void)
{
	if ( this->ucFloatSize == 4 )
	{
		delete [] this->fCellStates;
		delete [] this->fBedElevations;
		delete [] this->fManningValues;
	} else if ( this->ucFloatSize == 8 ) {
		delete [] this->dCellStates;
		delete [] this->dBedElevations;
		delete [] this->dManningValues;
	}

	if ( this->pBoundaries != NULL ) delete pBoundaries;
	if ( this->pScheme != NULL )     delete pScheme;

	delete [] this->cSourceDir;
	delete [] this->cTargetDir;

	pManager->log->writeLine("All domain memory has been released.");
}

/*
 *  Configure the domain using the XML file
 */
bool CDomain::configureDomain( XMLElement* pXDomain )
{
	XMLElement*		pXData;

	// Device number(s) are now declared as part of the domain to allow for
	// split workloads etc.
	char			*cDomainDevice		= NULL;
	Util::toLowercase( &cDomainDevice, pXDomain->Attribute( "deviceNumber" ) );

	// TODO: For now we're only working with 1 device. If we need more than one device,
	// the domain manager must handle splitting...
	// TODO: Device selection/assignment should take place in the domain manager class.

	/*
	// Validate the device number
	if ( cDomainDevice == NULL )
	{
		// Select a device automatically
		pDevice = pManager->getExecutor()->getDevice();
	} else {
		if ( CXMLDataset::isValidUnsignedInt( std::string(cDomainDevice) ) )
		{
			pDevice = pManager->getExecutor()->getDevice( boost::lexical_cast<unsigned int>(cDomainDevice) );
		} else {
			model::doError(
				"The domain device specified is invalid.",
				model::errorCodes::kLevelWarning
			);
		}
	}
	*/

	if ( pDevice == NULL )
	{
		model::doError(
			"No valid device was identified for the domain.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	pXData = pXDomain->FirstChildElement( "data" );
	const char*	cDataSourceDir = pXData->Attribute( "sourceDir" );
	const char*	cDataTargetDir = pXData->Attribute( "targetDir" );

	if ( cDataSourceDir != NULL )
	{
		this->cSourceDir = new char[ std::strlen( cDataSourceDir ) + 1 ];
		std::strcpy( this->cSourceDir, cDataSourceDir );
	}
	if ( cDataTargetDir != NULL )
	{
		this->cTargetDir = new char[ std::strlen( cDataTargetDir ) + 1 ];
		std::strcpy( this->cTargetDir, cDataTargetDir );
	}

	return true;
}

/*
 *  Creates an OpenCL memory buffer for the specified data store
 */
void	CDomain::createStoreBuffers(
			void**			vArrayCellStates,
			void**			vArrayBedElevations,
			void**			vArrayManningCoefs,
			unsigned char	ucFloatSize
		)
{
	if ( !bPrepared )
		prepareDomain();

	this->ucFloatSize = ucFloatSize;

	try {
		if ( ucFloatSize == sizeof( cl_float ) )
		{
			// Single precision
			this->fCellStates		= new cl_float4[ this->ulCellCount ];
			this->fBedElevations	= new cl_float[ this->ulCellCount ];
			this->fManningValues	= new cl_float[ this->ulCellCount ];
			this->dCellStates		= (cl_double4*)( this->fCellStates );
			this->dBedElevations	= (cl_double*)( this->fBedElevations );
			this->dManningValues	= (cl_double*)( this->fManningValues );

			*vArrayCellStates	 = static_cast<void*>( this->fCellStates );
			*vArrayBedElevations = static_cast<void*>( this->fBedElevations );
			*vArrayManningCoefs	 = static_cast<void*>( this->fManningValues );
		} else {
			// Double precision
			this->dCellStates		= new cl_double4[ this->ulCellCount ];
			this->dBedElevations	= new cl_double[ this->ulCellCount ];
			this->dManningValues	= new cl_double[ this->ulCellCount ];
			this->fCellStates		= (cl_float4*)( this->dCellStates );
			this->fBedElevations	= (cl_float*)( this->dBedElevations );
			this->fManningValues	= (cl_float*)( this->dManningValues );

			*vArrayCellStates	 = static_cast<void*>( this->dCellStates );
			*vArrayBedElevations = static_cast<void*>( this->dBedElevations );
			*vArrayManningCoefs	 = static_cast<void*>( this->dManningValues );
		}
	}
	catch( std::bad_alloc )
	{
		model::doError(
			"Domain memory allocation failure. Probably out of memory.",
			model::errorCodes::kLevelFatal
		);
		return;
	}
}

/*
 *  Populate all domain cells with default values
 *  NOTE: Shouldn't be required anymore, C++11 used in COCLBuffer to initialise to zero on allocate
 */
void	CDomain::initialiseMemory()
{
	pManager->log->writeLine( "Initialising heap domain data." );

	for( unsigned long i = 0; i < this->ulCellCount; i++ )
	{
		if ( this->ucFloatSize == 4 )
		{
			this->fCellStates[ i ].s[0]	= 0;	// Free-surface level
			this->fCellStates[ i ].s[1]	= 0;	// Maximum free-surface level
			this->fCellStates[ i ].s[2]	= 0;	// Discharge X
			this->fCellStates[ i ].s[3]	= 0;	// Discharge Y
			this->fBedElevations[ i ]   = 1;	// Bed elevation
			this->fManningValues[ i ]	= 0;	// Manning coefficient
		} else {
			this->dCellStates[ i ].s[0]	= 0;	// Free-surface level
			this->dCellStates[ i ].s[1]	= 0;	// Maximum free-surface level
			this->dCellStates[ i ].s[2]	= 0;	// Discharge X
			this->dCellStates[ i ].s[3]	= 0;	// Discharge Y
			this->dBedElevations[ i ]   = 1;	// Bed elevation
			this->dManningValues[ i ]	= 0;	// Manning coefficient
		}
	}
}

/*
 *  Sets the bed elevation for a given cell
 */
void	CDomain::setBedElevation( unsigned long ulCellID, double dElevation )
{
	if ( this->ucFloatSize == 4 )
	{
		this->fBedElevations[ ulCellID ] = static_cast<float>( dElevation );
	} else {
		this->dBedElevations[ ulCellID ] = dElevation;
	}
}

/*
 *  Sets the Manning coefficient for a given cell
 */
void	CDomain::setManningCoefficient( unsigned long ulCellID, double dCoefficient )
{
	if ( this->ucFloatSize == 4 )
	{
		this->fManningValues[ ulCellID ] = static_cast<float>( dCoefficient );
	} else {
		this->dManningValues[ ulCellID ] = dCoefficient;
	}
}

/*
 *  Sets a state variable for a given cell
 */
void	CDomain::setStateValue( unsigned long ulCellID, unsigned char ucIndex, double dValue )
{
	if ( this->ucFloatSize == 4 )
	{
		this->fCellStates[ ulCellID ].s[ ucIndex ] = static_cast<float>( dValue );
	} else {
		this->dCellStates[ ulCellID ].s[ ucIndex ] = dValue;
	}
}

/*
 *  Gets the bed elevation for a given cell
 */
double	CDomain::getBedElevation( unsigned long ulCellID )
{
	if ( this->ucFloatSize == 4 ) 
		return static_cast<double>( this->fBedElevations[ ulCellID ] );
	return this->dBedElevations[ ulCellID ];
}

/*
 *  Gets the Manning coefficient for a given cell
 */
double	CDomain::getManningCoefficient( unsigned long ulCellID )
{
	if ( this->ucFloatSize == 4 ) 
		return static_cast<double>( this->fManningValues[ ulCellID ] );
	return this->dManningValues[ ulCellID ];
}

/*
 *  Gets a state variable for a given cell
 */
double	CDomain::getStateValue( unsigned long ulCellID, unsigned char ucIndex )
{
	if ( this->ucFloatSize == 4 ) 
		return static_cast<double>( this->fCellStates[ ulCellID ].s[ ucIndex ] );
	return this->dCellStates[ ulCellID ].s[ ucIndex ];
}

/*
 *  Handle initial conditions input data for a cell (usually from a raster dataset)
 */
void	CDomain::handleInputData( 
			unsigned long	ulCellID, 
			double			dValue,
			unsigned char	ucValue,
			unsigned char	ucRounding
		)
{
	if ( !bPrepared )
		prepareDomain();

	switch( ucValue )
	{
	case model::rasterDatasets::dataValues::kBedElevation:
		this->setBedElevation( 
			ulCellID, 
			Util::round( dValue, ucRounding ) 
		);
		this->setStateValue( 
			ulCellID, 
			model::domainValueIndices::kValueFreeSurfaceLevel, 
			Util::round( dValue, ucRounding ) 
		);
		if ( dValue < dMinTopo && dValue != -9999.0 ) dMinTopo = dValue;
		if ( dValue > dMaxTopo && dValue != -9999.0 ) dMaxTopo = dValue;
		break;
	case model::rasterDatasets::dataValues::kFreeSurfaceLevel:
		this->setStateValue( 
			ulCellID,
			model::domainValueIndices::kValueFreeSurfaceLevel, 
			Util::round( dValue, ucRounding ) 
		);
		this->setStateValue( 
			ulCellID, 
			model::domainValueIndices::kValueMaxFreeSurfaceLevel, 
			Util::round( dValue, ucRounding ) 
		);
		if ( dValue - this->getBedElevation( ulCellID ) < dMinDepth && this->getBedElevation( ulCellID ) > -9999.0 && dValue > -9999.0 ) dMinDepth = dValue;
		if ( dValue - this->getBedElevation( ulCellID ) > dMaxDepth && this->getBedElevation( ulCellID ) > -9999.0 && dValue > -9999.0 ) dMaxDepth = dValue;
		if ( dValue < dMinFSL && this->getBedElevation( ulCellID ) > -9999.0 && dValue > -9999.0 ) dMinFSL = dValue;
		if ( dValue > dMaxFSL && this->getBedElevation( ulCellID ) > -9999.0 && dValue > -9999.0 ) dMaxFSL = dValue;
		break;
	case model::rasterDatasets::dataValues::kDepth:
		this->setStateValue( 
			ulCellID, 
			model::domainValueIndices::kValueFreeSurfaceLevel, 
			Util::round( ( this->getBedElevation( ulCellID ) + max(-1e-12, dValue) ), ucRounding ) 
		);
		this->setStateValue( 
			ulCellID, 
			model::domainValueIndices::kValueMaxFreeSurfaceLevel, 
			Util::round( ( this->getBedElevation( ulCellID ) + max(-1e-12, dValue) ), ucRounding )
		);
		if ( dValue + this->getBedElevation( ulCellID ) < dMinFSL && this->getBedElevation( ulCellID ) > -9999.0 && dValue > -9999.0 ) dMinFSL = dValue;
		if ( dValue + this->getBedElevation( ulCellID ) > dMaxFSL && this->getBedElevation( ulCellID ) > -9999.0 && dValue > -9999.0 ) dMaxFSL = dValue;
		if ( dValue < dMinDepth && this->getBedElevation( ulCellID ) > -9999.0 && dValue > -9999.0 ) dMinDepth = dValue;
		if ( dValue > dMaxDepth && this->getBedElevation( ulCellID ) > -9999.0 && dValue > -9999.0 ) dMaxDepth = dValue;
		break;
	case model::rasterDatasets::dataValues::kDisabledCells:
		// Cells are disabled using a free-surface level of -9999
		if ( dValue > 1.0 && dValue < 9999.0 )
		{
			this->setStateValue( 
				ulCellID, 
				model::domainValueIndices::kValueMaxFreeSurfaceLevel, 
				Util::round( ( -9999.0 ), ucRounding ) 
			);
		}
		break;
	case model::rasterDatasets::dataValues::kDischargeX:
		this->setStateValue( 
			ulCellID,
			model::domainValueIndices::kValueDischargeX, 
			Util::round( dValue, ucRounding ) 
		);
		break;
	case model::rasterDatasets::dataValues::kDischargeY:
		this->setStateValue( 
			ulCellID, 
			model::domainValueIndices::kValueDischargeY, 
			Util::round( dValue, ucRounding ) 
		);
		break;
	case model::rasterDatasets::dataValues::kVelocityX:
		this->setStateValue( 
			ulCellID, 
			model::domainValueIndices::kValueDischargeX, 
			Util::round( dValue * ( this->getStateValue( ulCellID, model::domainValueIndices::kValueFreeSurfaceLevel ) - this->getBedElevation( ulCellID ) ), ucRounding )
		);
		break;
	case model::rasterDatasets::dataValues::kVelocityY:
		this->setStateValue( 
			ulCellID, 
			model::domainValueIndices::kValueDischargeY, 
			Util::round( dValue * ( this->getStateValue( ulCellID, model::domainValueIndices::kValueFreeSurfaceLevel ) - this->getBedElevation( ulCellID ) ), ucRounding ) 
		);
		break;
	case model::rasterDatasets::dataValues::kManningCoefficient:
		this->setManningCoefficient( 
			ulCellID, 
			Util::round( dValue, ucRounding ) 
		);
		break;
	}
}

/*
 *  Calculate the total volume present in all of the cells
 */
double	CDomain::getVolume()
{
	double dVolume = 0.0;

	// Stub
	return dVolume;
}

/*
 *  Sets the scheme we're running on this domain
 */
void	CDomain::setScheme( CScheme* pScheme )
{
	this->pScheme = pScheme;
}

/*
 *  Gets the scheme we're running on this domain
 */
CScheme*	CDomain::getScheme()
{
	return pScheme;
}

/*
 *  Sets the device to use
 */
void	CDomain::setDevice( COCLDevice* pDevice )
{
	this->pDevice = pDevice;
}

/*
 *  Gets the scheme we're running on this domain
 */
COCLDevice*	CDomain::getDevice()
{
	return this->pDevice;
}

/*
 *  Gets the scheme we're running on this domain
 */
CDomainBase::mpiSignalDataProgress	CDomain::getDataProgress()
{
	CDomainBase::mpiSignalDataProgress pResponse;
	CScheme *pScheme = getScheme();

	pResponse.uiDomainID = this->uiID;
	pResponse.dBatchTimesteps = pScheme->getAverageTimestep();
	pResponse.dCurrentTime = pScheme->getCurrentTime();
	pResponse.dCurrentTimestep = pScheme->getCurrentTimestep();
	pResponse.uiBatchSize = pScheme->getBatchSize();
	pResponse.uiBatchSkipped = pScheme->getIterationsSkipped();
	pResponse.uiBatchSuccessful = pScheme->getIterationsSuccessful();

	return pResponse;
}

/*
 *  Fetch the code for a string descrption of an input/output
 */
unsigned char	CDomain::getDataValueCode( char* cSourceValue )
{
	if ( strstr( cSourceValue, "dem" ) != NULL )		
		return model::rasterDatasets::dataValues::kBedElevation;
	if ( strstr( cSourceValue, "maxdepth" ) != NULL )		
	{
		return model::rasterDatasets::dataValues::kMaxDepth;
	}
	else if ( strstr( cSourceValue, "depth" ) != NULL )		
	{
		return model::rasterDatasets::dataValues::kDepth;
	}
	if ( strstr( cSourceValue, "disabled" ) != NULL )		
		return model::rasterDatasets::dataValues::kDisabledCells;
	if ( strstr( cSourceValue, "dischargex" ) != NULL )		
		return model::rasterDatasets::dataValues::kDischargeX;
	if ( strstr( cSourceValue, "dischargey" ) != NULL )		
		return model::rasterDatasets::dataValues::kDischargeY;
	if ( strstr( cSourceValue, "maxfsl" ) != NULL )
	{
		return model::rasterDatasets::dataValues::kMaxFSL;
	}
	else if ( strstr( cSourceValue, "fsl" ) != NULL )
	{
		return model::rasterDatasets::dataValues::kFreeSurfaceLevel;
	}
	if ( strstr( cSourceValue, "manningcoefficient" ) != NULL )		
		return model::rasterDatasets::dataValues::kManningCoefficient;
	if ( strstr( cSourceValue, "velocityx" ) != NULL )		
		return model::rasterDatasets::dataValues::kVelocityX;
	if ( strstr( cSourceValue, "velocityy" ) != NULL )		
		return model::rasterDatasets::dataValues::kVelocityY;
	if ( strstr( cSourceValue, "froude" ) != NULL )		
		return model::rasterDatasets::dataValues::kFroudeNumber;

	return 255;
}
