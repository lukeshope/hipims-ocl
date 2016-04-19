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
 *  Executor Control Base Class
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_DOMAIN_CEXECUTORCONTROL_H_
#define HIPIMS_DOMAIN_CEXECUTORCONTROL_H_

// Executor states
namespace model { 
namespace executorStates { enum executorStates {
	executorReady				= 1,				// This executor can be used
	executorError				= 0					// This executor cannot be used
}; }

// Executor types
namespace executorTypes { enum executorTypes {
	executorTypeOpenCL			= 0					// OpenCL-based executor
}; }
}

// Device-type filers
namespace model {
namespace filters {
namespace devices {
	enum devices {
		devicesGPU			= 1,					// Graphics processors
		devicesCPU			= 2,					// Standard processors
		devicesAPU			= 4						// Accelerated processors
	};
};};}

/*
 *  EXECUTOR CONTROL CLASS
 *  CExecutorControl
 *
 *  Controls the model execution
 */
class CExecutorControl
{

	public:

		CExecutorControl( void );										
		virtual ~CExecutorControl( void );								

		// Public functions
		bool						isReady( void );					// Is the executor ready?
		void						setDeviceFilter( unsigned int );	// Filter to specific types of device
		unsigned int				getDeviceFilter();					// Fetch back the current device filter
		virtual void				setupFromConfig( XMLElement* ) = 0;	// Set up the executor

		// Static functions
		static CExecutorControl*	createExecutor( unsigned char );	// Create a new executor of the specified type
		static CExecutorControl*	createFromConfig( XMLElement* );	// Parse and configure the executor system
		
	protected:

		// Protected functions
		void					setState( unsigned int );				// Set the ready state
		unsigned int			deviceFilter;							// Device filter active for this executor

	private:

		// Private variables
		unsigned int			state;									// Ready state value

};

#endif
