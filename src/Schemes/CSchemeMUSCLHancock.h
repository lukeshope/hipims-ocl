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
#ifndef HIPIMS_SCHEMES_CSCHEMEMUSCLHANCOCK_H_
#define HIPIMS_SCHEMES_CSCHEMEMUSCLHANCOCK_H_

#include "CSchemeGodunov.h"

namespace model {

// Kernel configurations
namespace schemeConfigurations{ 
namespace musclHancock { enum musclHancock {
	kCacheNone						= 10,		// Option B in dissertation: No local memory used
	kCachePrediction				= 11,		// Option C in dissertation: Only the prediction step uses caching
	kCacheMaximum					= 12		// Option D in dissertation: All stages use cache memory
}; }  }

namespace cacheConstraints{ 
namespace musclHancock { enum musclHancock {
	kCacheActualSize				= 10,		// LDS of actual size
	kCacheAllowOversize				= 11,		// Allow LDS oversizing to avoid bank conflicts
	kCacheAllowUndersize			= 12		// Allow LDS undersizing to avoid bank conflicts
}; }  }

}

/*
 *  SCHEME CLASS
 *  CSchemeMUSCLHancock
 *
 *  Stores and invokes relevant methods for the actual
 *  hydraulic computations.
 */
class CSchemeMUSCLHancock : public CSchemeGodunov
{

	public:

		CSchemeMUSCLHancock( void );										// Constructor
		virtual ~CSchemeMUSCLHancock( void );								// Destructor

		// Public functions
		virtual void		setupFromConfig( XMLElement*, bool = false );	// Set up the scheme
		virtual void		logDetails();									// Write some details about the scheme
		virtual void		prepareAll();									// Prepare absolutely everything for a model run
		virtual void		scheduleIteration( bool,						// Schedule an iteration of the scheme
											   COCLDevice*,
											   CDomain* );	
		void				setCacheMode( unsigned char );					// Set the cache configuration
		unsigned char		getCacheMode();									// Get the cache configuration
		void				setCacheConstraints( unsigned char );			// Set LDS cache size constraints
		unsigned char		getCacheConstraints();							// Get LDS cache size constraints
		void				setExtrapolatedContiguity( bool );				// Store extrapolated data contiguously?
		bool				getExtrapolatedContiguity();					// Is extrapolated data stored contiguously?
		void				readDomainAll();								// Fetch back all the domain data
		COCLBuffer*			getLastCellSourceBuffer();						// Get the last source cell state buffer
		COCLBuffer*			getNextCellSourceBuffer();						// Get the next source cell state buffer

	protected:

		// Private functions
		virtual bool		prepareCode();									// Prepare the code required
		virtual void		releaseResources();								// Release OpenCL resources consumed
		bool				prepare2OKernels();								// Prepare the kernels required
		bool				prepare2OConstants();							// Assign constants to the executor
		bool				prepare2OMemory();								// Prepare memory buffers required
		bool				prepare2OExecDimensions();						// Size the problem for execution
		void				release2OResources();							// Release 2nd-order OpenCL resources consumed

		// Private variables
		bool				bContiguousFaceData;							// Store all face state data contiguously?

		// OpenCL elements
		COCLKernel*			oclKernelHalfTimestep;
		COCLBuffer*			oclBufferFaceExtrapolations;
		COCLBuffer*			oclBufferFaceExtrapolationN;
		COCLBuffer*			oclBufferFaceExtrapolationE;
		COCLBuffer*			oclBufferFaceExtrapolationS;
		COCLBuffer*			oclBufferFaceExtrapolationW;

};

#endif
