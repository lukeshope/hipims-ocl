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
 *  Uses and invokves GDAL functionality
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_DATASETS_CRASTERDATASET_H_
#define HIPIMS_DATASETS_CRASTERDATASET_H_

#include <gdal_priv.h>
#include <cpl_conv.h>
#include "../Domain/CDomain.h"
#include "../Domain/Cartesian/CDomainCartesian.h"
#include "../Boundaries/CBoundaryGridded.h"

namespace model {

// Model scheme types
namespace rasterDatasets { 
namespace dataValues { enum dataValues {
	kBedElevation		= 0,		// Bed elevation
	kDepth				= 1,		// Depth
	kFreeSurfaceLevel	= 2,		// Free surface level
	kVelocityX			= 3,		// Initial velocity X
	kVelocityY			= 4,		// Initial velocity Y
	kDischargeX			= 5,		// Initial discharge X
	kDischargeY			= 6,		// Initial discharge Y
	kManningCoefficient	= 7,		// Manning coefficient
	kDisabledCells		= 8,		// Disabled cells
	kMaxDepth			= 9,		// Max depth
	kMaxFSL				= 10,		// Max FSL
	kFroudeNumber		= 11		// Froude number
}; };
};
};

/*
 *  RASTER DATASET CLASS
 *  CRasterDataset
 *
 *  Provides access for loading and saving
 *  raster datasets. Uses GDAL.
 */
class CRasterDataset
{

	public:

		CRasterDataset( void );																				// Constructor
		~CRasterDataset( void );																			// Destructor

		// Public variables
		// ...

		// Public functions
		static void		registerAll();																		// Register types for use, must be called first
		static void		cleanupAll();																		// Cleanup memory after use. Not perfect... 
		static bool		domainToRaster( const char*, std::string, CDomainCartesian*, unsigned char );		// Open a file as the dataset for writing
		bool			openFileRead( std::string );														// Open a file as the dataset for reading
		void			readMetadata();																		// Read metadata for the dataset
		void			logDetails();																		// Write details (mainly metdata) to the log
		bool			applyDimensionsToDomain( CDomainCartesian* );										// Applies the dimensions, offset and scaling to a domain
		bool			applyDataToDomain( unsigned char, CDomainCartesian* );								// Applies first band of data in the raster to a domain variable
		CBoundaryGridded::SBoundaryGridTransform* createTransformationForDomain(CDomainCartesian*);			// Create a transformation to match the domain
		double*			createArrayForBoundary(CBoundaryGridded::SBoundaryGridTransform*);					// Create an array for a boundary condition

	private:

		// Private functions
		static void		getValueDetails( unsigned char, std::string* );										// Fetch some data on a value, like its index in the CDomain array
		bool			isDomainCompatible( CDomainCartesian* );											// Is the domain compatible (i.e. row/column count, etc.) with this raster?

		// Private variables
		GDALDataset*	gdDataset;																			// Pointer to the dataset
		bool			bAvailable;																			// Raster successfully open and available?
		double			dResolutionX;																		// Cell resolution in X-direction
		double			dResolutionY;																		// Cell resolution in Y-direction
		double			dOffsetX;																			// LL corner offset X
		double			dOffsetY;																			// LL corner offset Y
		unsigned long	ulColumns;																			// Number of columns
		unsigned long	ulRows;																				// Number of rows
		unsigned int	uiEPSGCode;																			// EPSG projection code (0 if none)
		unsigned int	uiBandCount;																		// Number of bands in the file
		char*			cProjectionName;																	// String description of the projection
		char*			cUnits;																				// Units used 
		char*			cDriverDescription;																	// Description of file format driver
		char*			cDriverLongName;																	// Full name of the driver being used

		// Friendships
		// ...

};

#endif
