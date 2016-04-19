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
#ifndef HIPIMS_DOMAIN_CARTESIAN_CDOMAINCARTESIAN_H_
#define HIPIMS_DOMAIN_CARTESIAN_CDOMAINCARTESIAN_H_

#include "../CDomain.h"

/*
 *  DOMAIN CLASS
 *  CDomainCartesian
 *
 *  Stores the relevant information required for a
 *  domain based on a simple cartesian grid.
 */
class CDomainCartesian : public CDomain 
{

	public:

		CDomainCartesian( void );												// Constructor
		~CDomainCartesian( void );												// Destructor

		// Public variables
		// ...

		// Public functions
		// - Replacements for CDomain stubs
		bool			configureDomain( XMLElement* );							// Configure a domain, loading data etc.
		virtual	unsigned char	getType()										{ return model::domainStructureTypes::kStructureCartesian; };	// Fetch a type code
		virtual	CDomainBase::DomainSummary getSummary();						// Fetch summary information for this domain
		bool			validateDomain( bool );									// Verify required data is available
		bool		    loadInitialConditions( XMLElement* );					// Load the initial condition raster/constant data
		bool			loadOutputDefinitions( XMLElement* );					// Load the output file definitions
		void			prepareDomain();										// Create memory structures etc.
		void			logDetails();											// Log details about the domain
		void			writeOutputs();											// Write output files to disk
		void			syncWithDomain( CDomain* );								// Synchronise with another domain
		unsigned int	getOverlapSize( CDomain* );								// Get the size of the overlap zone
		// - Specific to cartesian grids
		void			imposeBoundaryModification(unsigned char, unsigned char); // Adjust the topography to impose boundary conditions
		void			setRealDimensions( double, double );					// Set real domain dimensions (X, Y)
		void			getRealDimensions( double*, double* );					// Fetch real domain dimensions (X, Y)
		void			setRealOffset( double, double );						// Set real domain offset (X, Y) for lower-left corner
		void			getRealOffset( double*, double* );						// Fetch real domain offset (X, Y) for lower-left corner
		void			setRealExtent( double, double, double, double );		// Set real domain extent (Clockwise: N, E, S, W)
		void			getRealExtent( double*, double*, double*, double* );	// Fetch real domain extent (Clockwise: N, E, S, W)
		void			setCellResolution( double );							// Set cell resolution
		void			getCellResolution( double* );							// Fetch cell resolution
		void			setUnits( char* );										// Set the units
		char*			getUnits();												// Get the units
		void			setProjectionCode( unsigned long );						// Set the EPSG projection code
		unsigned long	getProjectionCode();									// Get the EPSG projection code
		unsigned long	getRows();												// Get the number of rows in the domain
		unsigned long	getCols();												// Get the number of columns in the domain
		virtual unsigned long	getCellID( unsigned long, unsigned long );		// Get the cell ID using an X and Y index
		unsigned long	getCellFromCoordinates( double, double );				// Get the cell ID using real coords
		double			getVolume();											// Calculate the amount of volume in all the cells
		#ifdef _WINDLL
		virtual void	sendAllToRenderer();									// Allows the renderer to read off the bed elevations
		#endif

		enum axis
		{
			kAxisX	= 0,
			kAxisY	= 1
		};

		enum edge
		{
			kEdgeN	= 0,
			kEdgeE	= 1,
			kEdgeS	= 2,
			kEdgeW	= 3
		};

		enum boundaryTreatment
		{
			kBoundaryOpen = 0,
			kBoundaryClosed = 1
		};

	private:

		// Private structures
		struct	sDataSourceInfo {
			char*			cSourceType;
			char*			cFileValue;
			unsigned char	ucValue;
		};
		struct	sDataTargetInfo
		{
			char*			cType;
			char*			cFormat;
			unsigned char	ucValue;
			std::string		sTarget;
		};

		// Private variables
		double			dRealDimensions[2];
		double			dRealOffset[2];
		double			dRealExtent[4];
		double			dCellResolution;
		unsigned long	ulRows;
		unsigned long	ulCols;
		unsigned long	ulProjectionCode;
		char			cUnits[2];
		std::vector<sDataTargetInfo>	pOutputs;									// Structure of details about the outputs

		// Private functions
		void			addOutput( sDataTargetInfo );								// Adds a new output 
		bool			loadInitialConditionSource( sDataSourceInfo, char* );		// Load a constant/raster condition to the domain
		void			updateCellStatistics();										// Update the number of rows, cols, etc.

};

#endif
