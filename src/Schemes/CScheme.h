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
 *  Scheme Base Class
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_SCHEMES_CSCHEME_H_
#define HIPIMS_SCHEMES_CSCHEME_H_

#include "../General/CBenchmark.h"
#include "../Domain/CDomain.h"
#include "../OpenCL/Executors/CExecutorControlOpenCL.h"
#include "../OpenCL/Executors/COCLProgram.h"
#include "../OpenCL/Executors/COCLDevice.h"
#include "../OpenCL/Executors/COCLKernel.h"
#include "../OpenCL/Executors/COCLBuffer.h"

namespace model {

// Model scheme types
namespace schemeTypes{ enum schemeTypes {
	kGodunov							= 0,	// Godunov (first-order)
	kMUSCLHancock						= 1,	// MUSCL-Hancock (second-order)
	kInertialSimplification				= 2		// Inertial simplification
}; }

// Riemann solver types
namespace solverTypes{ enum solverTypes {
	kHLLC								= 0		// HLLC approximate
}; }

// Queue mode
namespace queueMode{ enum queueMode {
	kAuto								= 0,	// Automatic
	kFixed								= 1		// Fixed
}; }

// Timestep mode
namespace timestepMode{ enum timestepMode {
	kCFL								= 0,	// CFL constrained
	kFixed								= 1		// Fixed
}; }

// Timestep mode
namespace syncMethod {
	enum syncMethod {
		kSyncTimestep = 0,						// Timestep synchronised
		kSyncForecast = 1						// Timesteps forecast
	};
}

}

/*
 *  SCHEME CLASS
 *  CScheme
 *
 *  Stores and invokes relevant methods for the actual
 *  hydraulic computations.
 */
class CScheme
{

	public:

		CScheme();																					// Default constructor
		virtual ~CScheme( void );																	// Destructor

		// Public functions
		static CScheme*		createScheme( unsigned char );											// Instantiate a scheme
		static CScheme*		createFromConfig( XMLElement* );										// Parse and configure a scheme class

		virtual void		setupFromConfig( XMLElement*, bool = false );							// Set up the scheme
		bool				isReady();																// Is the scheme ready to run?
		bool				isRunning();															// Is this scheme currently running a batch?
		virtual void		logDetails() = 0;														// Write some details about the scheme
		virtual void		prepareAll() = 0;														// Prepare absolutely everything for a model run
		virtual double		proposeSyncPoint( double ) = 0;											// Propose a synchronisation point
		virtual void		forceTimestep(double) = 0;												// Force a specific timestep
		void				setQueueMode( unsigned char );											// Set the queue mode
		unsigned char		getQueueMode();															// Get the queue mode
		void				setQueueSize( unsigned int );											// Set the queue size (or initial)
		unsigned int		getQueueSize();															// Get the queue size (or initial)
		void				setCourantNumber( double );												// Set the Courant number
		double				getCourantNumber();														// Get the Courant number
		void				setTimestepMode( unsigned char );										// Set the timestep mode
		unsigned char		getTimestepMode();														// Get the timestep mode
		void				setTimestep( double );													// Set the timestep
		double				getTimestep();															// Get the timestep
		virtual double		getAverageTimestep() = 0;												// Get batch average timestep
		void				setFrictionStatus( bool );												// Enable/disable friction effects
		bool				getFrictionStatus();													// Get enabled/disabled for friction
		virtual void		setTargetTime( double );												// Set the target sync time
		double				getTargetTime();														// Get the target sync time
		void				setDomain( CDomain *d )			{ pDomain = d; }						// Set the domain we're working on
		CDomain*			getDomain()						{ return pDomain; }						// Fetch the domain we're working on
		unsigned long long	getCellsCalculated()			{ return ulCurrentCellsCalculated; }	// Number of cells calculated so far
		double				getCurrentTimestep()			{ return dCurrentTimestep; }			// Current timestep
		bool				getCurrentSuspendedState()		{ return ( dCurrentTimestep < 0.0 ); }	// Is the simulation suspended?
		double				getCurrentTime()				{ return dCurrentTime; }				// Current progress
		unsigned int		getBatchSize()					{ return uiQueueAdditionSize; }			// Get the batch size
		unsigned int		getIterationsSuccessful()		{ return uiBatchSuccessful; }			// Get the successful iterations
		unsigned int		getIterationsSkipped()			{ return uiBatchSkipped; }				// Get the number of iterations skipped

		virtual void		readDomainAll() = 0;													// Read back all domain data
		virtual void		importLinkZoneData() = 0;												// Read back synchronisation zone data
		virtual void		prepareSimulation() = 0;												// Set everything up to start running for this domain
		virtual void		readKeyStatistics() = 0;												// Fetch the key statistics back to the right places in memory
		virtual void		runSimulation( double, double ) = 0;									// Run this simulation until the specified time
		virtual void		cleanupSimulation() = 0;												// Dispose of transient data and clean-up this domain
		virtual void		rollbackSimulation( double, double ) = 0;								// Roll back cell states to the last successful round
		virtual void		saveCurrentState() = 0;													// Save current cell states
		virtual void		forceTimeAdvance() = 0;													// Force time advance (when synced will stall)
		virtual bool		isSimulationFailure( double ) = 0;										// Check whether we successfully reached a specific time
		virtual bool		isSimulationSyncReady( double ) = 0;									// Are we ready to synchronise? i.e. have we reached the set sync time?
		virtual COCLBuffer*	getLastCellSourceBuffer() = 0;											// Get the last source cell state buffer
		virtual COCLBuffer*	getNextCellSourceBuffer() = 0;											// Get the next source cell state buffer

	protected:

		// Private functions
		// ...

		// Private variables
		bool				bRunning;																// Is this simulation currently running?
		bool				bThreadRunning;															// Is the worker thread running?
		bool				bThreadTerminated;														// Has the worker thread been terminated?
		bool				bReady;																	// Is the scheme ready?
		bool				bBatchComplete;															// Is the batch done?
		bool				bBatchError;															// Have we run out of room?
		bool				bFrictionEffects;														// Activate friction effects?
		unsigned long long	ulCurrentCellsCalculated;												// Total number of cells calculated
		double				dCurrentTime;															// Current simulation time
		double				dCurrentTimestep;														// Current simulation timestep
		double				dTargetTime;															// Target time for synchronisation
		bool				bAutomaticQueue;														// Automatic queue size detection?
		double				dTimestep;																// Constant/initial timestep
		unsigned int		uiQueueAdditionSize;													// Number of runs to queue at once
		unsigned int		uiIterationsSinceSync;													// Number of iterations since we last synchronised
		unsigned int		uiIterationsSinceProgressCheck;											// How many iterations since we downloaded progress data
		double				dCourantNumber;															// Courant number for CFL condition
		bool				bDynamicTimestep;														// Dynamic timestepping enabled?
		double				dBatchStartedTime;														// Time at which the batch was started
		cl_double			dBatchTimesteps;														// Cumulative batch timesteps
		cl_uint				uiBatchSkipped;															// Number of skipped batch iterations
		cl_uint				uiBatchSuccessful;														// Number of successful batch iterations
		cl_uint				uiBatchRate;															// Number of successful iterations per second
		CDomain*			pDomain;																// Domain which this scheme is attached to
		
};

#endif
