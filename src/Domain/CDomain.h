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
#ifndef HIPIMS_DOMAIN_CDOMAIN_H_
#define HIPIMS_DOMAIN_CDOMAIN_H_

#include "../OpenCL/opencl.h"
#include "CDomainBase.h"

namespace model {

// Model domain structure types
namespace domainValueIndices{ enum domainValueIndices {
	kValueFreeSurfaceLevel					= 0,	// Free-surface level
	kValueMaxFreeSurfaceLevel				= 1,	// Max free-surface level
	kValueDischargeX						= 2,	// Discharge X
	kValueDischargeY						= 3		// Discharge Y
}; }

}

// TODO: Make a CLocation class
class CDomainCartesian;
class CBoundaryMap;
class COCLDevice;
class COCLBuffer;
class CScheme;

/*
 *  DOMAIN CLASS
 *  CDomain
 *
 *  Stores relevant details for the computational domain and
 *  common operations required.
 */
class CDomain : public CDomainBase
{

	public:

		CDomain( void );																			// Constructor
		~CDomain( void );																			// Destructor

		// Public variables
		// ...

		// Public functions
		virtual 	bool			configureDomain( XMLElement* );									// Configure a domain, loading data etc.
		virtual		bool			isRemote()				{ return false; };						// Is this domain on this node?
		virtual		bool			validateDomain( bool ) = 0;										// Verify required data is available
		virtual		void			prepareDomain() = 0;											// Create memory structures etc.
		virtual		void			logDetails() = 0;												// Log details about the domain
		virtual		void			updateCellStatistics() = 0;										// Update the total number of cells calculation
		virtual		void			writeOutputs() = 0;												// Write output files to disk
		void						createStoreBuffers( void**, void**, void**, unsigned char );	// Allocates memory and returns pointers to the three arrays
		void						initialiseMemory();												// Populate cells with default values
		void						handleInputData( unsigned long, double, unsigned char, unsigned char );	// Handle input data for varying state/static cell variables 
		void						setBedElevation( unsigned long, double );						// Sets the bed elevation for a cell
		void						setManningCoefficient( unsigned long, double );					// Sets the manning coefficient for a cell
		void						setStateValue( unsigned long, unsigned char, double );			// Sets a state variable
		bool						isDoublePrecision() { return ( ucFloatSize == 8 ); };				// Are we using double-precision?
		double						getBedElevation( unsigned long );								// Gets the bed elevation for a cell
		double						getManningCoefficient( unsigned long );							// Gets the manning coefficient for a cell
		double						getStateValue( unsigned long, unsigned char );					// Gets a state variable
		double						getMaxFSL()				{ return dMaxFSL; }						// Fetch the maximum FSL in the domain
		double						getMinFSL()				{ return dMinFSL; }						// Fetch the minimum FSL in the domain
		virtual double				getVolume();													// Calculate the total volume in all the cells
		CBoundaryMap*				getBoundaries()			{ return pBoundaries; }					// Return the boundary map class
		unsigned int				getID()					{ return uiID; }						// Get the ID number
		void						setID( unsigned int i ) { uiID = i; }							// Set the ID number
		virtual mpiSignalDataProgress getDataProgress();											// Fetch some data on this domain's progress

		void						setScheme( CScheme* );											// Set the scheme running for this domain
		CScheme*					getScheme();													// Get the scheme running for this domain
		void						setDevice( COCLDevice* );										// Set the device responsible for running this domain
		COCLDevice*					getDevice();													// Get the device responsible for running this domain

		#ifdef _WINDLL
		virtual void				sendAllToRenderer() {};											// Allows the renderer to read off the bed elevations
		#endif

	protected:

		// Private variables
		unsigned char		ucFloatSize;															// Size of floats used for cell data (bytes)
		char*				cSourceDir;																// Data source dir
		char*				cTargetDir;																// Output target dir
		cl_double4*			dCellStates;															// Heap for cell state data
		cl_double*			dBedElevations;															// Heap for bed elevations
		cl_double*			dManningValues;															// Heap for manning values
		cl_float4*			fCellStates;															// Heap for cell state date (single)
		cl_float*			fBedElevations;															// Heap for bed elevations (single)
		cl_float*			fManningValues;															// Heap for manning values (single)

		cl_double			dMinFSL;																// Min and max FSLs in the domain used for rendering
		cl_double			dMaxFSL;
		cl_double			dMinTopo;																// Min and max topographic levels above datum
		cl_double			dMaxTopo;
		cl_double			dMinDepth;																// Min and max depths 
		cl_double			dMaxDepth;

		CBoundaryMap*		pBoundaries;															// Boundary map (management)
		CScheme*			pScheme;																// Scheme we are running for this particular domain
		COCLDevice*			pDevice;																// Device responsible for running this domain

		// Private functions
		unsigned char		getDataValueCode( char* );												// Get a raster dataset code from text description
};

#endif
