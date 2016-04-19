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
 *  Main model container
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_CMODEL_H_
#define HIPIMS_CMODEL_H_

#include "OpenCL/opencl.h"
#include "General/CBenchmark.h"
#include "Datasets/TinyXML/tinyxml2.h"
#include <vector>

// Some classes we need to know about...
class CExecutorControl;
class CExecutorControlOpenCL;
class CDomainManager;
class CScheme;
class CLog;
class CMPIManager;

using tinyxml2::XMLElement;

/*
 *  APPLICATION CLASS
 *  CModel
 *
 *  Is a singleton class in reality, but need not be enforced.
 */
class CModel
{
	public:

		// Public functions
		CModel(void);															// Constructor
		~CModel(void);															// Destructor

		void					setupFromConfig( XMLElement* );					// Setup the simulation
		bool					setExecutor(CExecutorControl*);					// Sets the type of executor to use for the model
		CExecutorControlOpenCL*	getExecutor(void);								// Gets the executor object currently in use
		CDomainManager*			getDomainSet(void);								// Gets the domain set
		CMPIManager*			getMPIManager(void);							// Gets the MPI manager

		bool					runModel(void);									// Execute the model
		void					runModelPrepare(void);							// Prepare for model run
		void					runModelPrepareDomains(void);					// Prepare domains and domain links
		void					runModelMain(void);								// Main model run loop
		void					runModelDomainAssess( bool*, bool* );			// Assess domain states
		void					runModelDomainExchange(void);					// Exchange domain data
		void					runModelUpdateTarget(double);					// Calculate a new target time
		void					runModelSync(void);								// Synchronise domain and timestep data
		void					runModelOutputs(void);							// Process outputs
		void					runModelMPI(void);								// Process MPI queue etc.
		void					runModelSchedule( CBenchmark::sPerformanceMetrics *, bool * );	// Schedule work
		void					runModelUI( CBenchmark::sPerformanceMetrics * );// Update progress data etc.
		void					runModelRollback(void);							// Rollback simulation
		void					runModelBlockGlobal(void);						// Block all domains until all are done
		void					runModelBlockNode(void);						// Block further processing on this node only
		void					runModelCleanup(void);							// Clean up after a simulation completes/aborts

		void					logDetails();									// Spit some info out to the log
		double					getSimulationLength();							// Get total length of simulation
		void					setSimulationLength( double );					// Set total length of simulation
		unsigned long			getRealStart();									// Get the real world start time (relative to epoch)
		void					setRealStart( char*, char* = NULL );			// Set the real world start time
		double					getOutputFrequency();							// Get the output frequency
		void					setOutputFrequency( double );					// Set the output frequency
		void					setFloatPrecision( unsigned char );				// Set floating point precision
		unsigned char			getFloatPrecision();							// Get floating point precision
		void					setName( std::string );							// Sets the name
		void					setDescription( std::string );					// Sets the description
		void					writeOutputs();									// Produce output files
		void					logProgress( CBenchmark::sPerformanceMetrics* );// Write the progress bar etc.
		static void CL_CALLBACK	visualiserCallback( cl_event, cl_int, void * );	// Callback event used when memory reads complete, for visualisation updates

		// Public variables
		CLog*					log;											// Handle for the log singular class

	private:

		// Private functions
		void					visualiserUpdate();								// Update 3D stuff 

		// Private variables
		CExecutorControlOpenCL*	execController;									// Handle for the executor controlling class
		CDomainManager*			domains;										// Handle for the domain management class
		CMPIManager*			mpiManager;										// Handle for the MPI manager class
		std::string				sModelName;										// Short name for the model
		std::string				sModelDescription;								// Short description of the model
		bool					bDoublePrecision;								// Double precision enabled?
		double					dSimulationTime;								// Total length of simulations
		double					dCurrentTime;									// Current simulation time
		double					dVisualisationTime;								// Current visualisation time
		double					dProcessingTime;								// Total processing time
		double					dOutputFrequency;								// Frequency of outputs
		double					dLastSyncTime;									//
		double					dLastOutputTime;								//
		double					dLastProgressUpdate;							//
		double					dTargetTime;									// 
		double					dEarliestTime;									//
		double					dGlobalTimestep;								//
		unsigned long			ulRealTimeStart;
		bool					bRollbackRequired;								// 
		bool					bAllIdle;										//
		bool					bWaitOnLinks;									//
		bool					bSynchronised;									//
		unsigned char			ucFloatSize;									// Size of single/double precision floats used
		cursorCoords			pProgressCoords;								// Buffer coords of the progress output

};

#endif
