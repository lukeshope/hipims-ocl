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
 *  Handles execution on OpenCL-capable devices
 * ------------------------------------------
 *
 */

// Includes
#include "../../common.h"
#include <boost/lexical_cast.hpp>
#include "../opencl.h"
#include "../../CModel.h"
#include "../../Base/CExecutorControl.h"
#include "CExecutorControlOpenCL.h"
#include "COCLDevice.h"

/*
 *  Constructor
 */
COCLDevice::COCLDevice( cl_device_id clDevice, unsigned int iPlatformID, unsigned int iDeviceNo )
{
	// Store the device and platform ID
	this->uiPlatformID			= iPlatformID;
	this->uiDeviceNo			= iDeviceNo + 1;
	this->clDevice				= clDevice;
	this->execController		= pManager->getExecutor();
	this->bForceSinglePrecision	= false;
	this->bErrored				= false;
	this->bBusy					= false;
	this->clMarkerEvent			= NULL;

	pManager->log->writeLine( "Querying the suitability of a discovered device." );

	this->getAllInfo();
	this->createQueue();
}

/*
 *  Destructor
 */
COCLDevice::~COCLDevice(void)
{
	clFinish( this->clQueue );
	clReleaseCommandQueue( this->clQueue );
	clReleaseContext( this->clContext );

	delete[] this->clDeviceMaxWorkItemSizes;
	delete[] this->clDeviceName;
	delete[] this->clDeviceCVersion;
	delete[] this->clDeviceProfile;
	delete[] this->clDeviceVendor;
	delete[] this->clDeviceOpenCLVersion;
	delete[] this->clDeviceOpenCLDriver;

	pManager->log->writeLine( "An OpenCL device has been released (#" + toString(this->uiDeviceNo) + ")." );
}

/*
 *  Obtain the size and value for a device info field
 */
void COCLDevice::getAllInfo()
{
	void*	vMemBlock;

	// This is messy, but at least it doesn't memory leak...
	// TODO: This should really use a template function for getDeviceInfo. That'd be sensible...
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_ADDRESS_BITS );
	this->clDeviceAddressSize			= *static_cast<cl_uint*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_AVAILABLE );
	this->clDeviceAvailable				= *static_cast<cl_bool*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_COMPILER_AVAILABLE );
	this->clDeviceCompilerAvailable		= *static_cast<cl_bool*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_ERROR_CORRECTION_SUPPORT );
	this->clDeviceErrorCorrection		= *static_cast<cl_bool*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_EXECUTION_CAPABILITIES );
	this->clDeviceExecutionCapability	= *static_cast<cl_device_exec_capabilities*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_GLOBAL_MEM_CACHE_SIZE );
	this->clDeviceGlobalCacheSize		= *static_cast<cl_ulong*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_GLOBAL_MEM_CACHE_TYPE );
	this->clDeviceGlobalCacheType		= *static_cast<cl_device_mem_cache_type*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_GLOBAL_MEM_SIZE );
	this->clDeviceGlobalSize			= *static_cast<cl_ulong*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_LOCAL_MEM_SIZE );
	this->clDeviceLocalSize				= *static_cast<cl_ulong*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_LOCAL_MEM_TYPE );
	this->clDeviceLocalType				= *static_cast<cl_device_local_mem_type*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_MAX_CLOCK_FREQUENCY );
	this->clDeviceClockFrequency		= *static_cast<cl_uint*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_MAX_COMPUTE_UNITS );
	this->clDeviceComputeUnits			= *static_cast<cl_uint*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_MAX_CONSTANT_ARGS );
	this->clDeviceMaxConstants			= *static_cast<cl_uint*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE );
	this->clDeviceMaxConstantSize		= *static_cast<cl_ulong*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_MAX_MEM_ALLOC_SIZE );
	this->clDeviceMaxMemAlloc			= *static_cast<cl_ulong*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_GLOBAL_MEM_SIZE );
	this->clDeviceGlobalMemSize			= *static_cast<cl_ulong*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_MAX_PARAMETER_SIZE );
	this->clDeviceMaxParamSize			= *static_cast<size_t*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_MAX_WORK_GROUP_SIZE );
	this->clDeviceMaxWorkGroupSize		= *static_cast<size_t*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS );
	this->clDeviceMaxWorkItemDims		= *static_cast<cl_uint*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_PROFILING_TIMER_RESOLUTION );
	this->clDeviceTimerResolution		= *static_cast<size_t*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_QUEUE_PROPERTIES );
	this->clDeviceQueueProperties		= *static_cast<cl_command_queue_properties*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_SINGLE_FP_CONFIG );
	this->clDeviceSingleFloatConfig		= *static_cast<cl_device_fp_config*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_DOUBLE_FP_CONFIG );
	this->clDeviceDoubleFloatConfig		= *static_cast<cl_device_fp_config*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_TYPE );
	this->clDeviceType					= *static_cast<cl_device_type*>( vMemBlock );
	delete[] vMemBlock;
	vMemBlock							= this->getDeviceInfo( CL_DEVICE_MEM_BASE_ADDR_ALIGN );
	this->clDeviceAlignBits				= *static_cast<cl_uint*>( vMemBlock );
	delete[] vMemBlock;

	this->clDeviceMaxWorkItemSizes		= (size_t *)this->getDeviceInfo( CL_DEVICE_MAX_WORK_ITEM_SIZES );
	this->clDeviceName					= (char *)this->getDeviceInfo( CL_DEVICE_NAME );
	this->clDeviceCVersion				= (char *)this->getDeviceInfo( CL_DEVICE_OPENCL_C_VERSION );
	this->clDeviceProfile				= (char *)this->getDeviceInfo( CL_DEVICE_PROFILE );
	this->clDeviceVendor				= (char *)this->getDeviceInfo( CL_DEVICE_VENDOR );
	this->clDeviceOpenCLVersion			= (char *)this->getDeviceInfo( CL_DEVICE_VERSION );
	this->clDeviceOpenCLDriver			= (char *)this->getDeviceInfo( CL_DRIVER_VERSION );
}

/*
 *  Obtain the size and value for a device info field
 */
void* COCLDevice::getDeviceInfo( cl_device_info clInfo )
{
	cl_int		iErrorID;
	size_t		clSize;

	iErrorID = clGetDeviceInfo( 
		this->clDevice, 
		clInfo, 
		NULL, 
		NULL, 
		&clSize 
	);

	if (iErrorID != CL_SUCCESS)
		clSize = 1;

	char* cValue = new char[ clSize + 1 ];

	iErrorID = clGetDeviceInfo( 
		this->clDevice, 
		clInfo, 
		clSize, 
		cValue, 
		NULL 
	);

	if (iErrorID != CL_SUCCESS)
		cValue[0] = 0;

	return cValue;
}

/*
 *  Write details of this device to the log
 */
void COCLDevice::logDevice()
{
	CLog*		pLog			= pManager->log;
	std::string	sPlatformNo;

	pLog->writeDivide();

	unsigned short	wColour			= model::cli::colourInfoBlock;

	std::string sDeviceType = " UNKNOWN DEVICE TYPE";
	if ( this->clDeviceType & CL_DEVICE_TYPE_CPU )			sDeviceType = " CENTRAL PROCESSING UNIT";
	if ( this->clDeviceType & CL_DEVICE_TYPE_GPU )			sDeviceType = " GRAPHICS PROCESSING UNIT";
	if ( this->clDeviceType & CL_DEVICE_TYPE_ACCELERATOR )	sDeviceType += " AND ACCELERATOR";

	std::string sDoubleSupport = "Not supported";
	if ( this->isDoubleCompatible() )
		sDoubleSupport = "Available";

	std::stringstream ssGroupDimensions;
	ssGroupDimensions << "["  << this->clDeviceMaxWorkItemSizes[0] << 
						 ", " << this->clDeviceMaxWorkItemSizes[1] << 
						 ", " << this->clDeviceMaxWorkItemSizes[2] << "]";

	pLog->writeLine( "#" + toString( this->uiDeviceNo ) + sDeviceType, true, wColour );

	pLog->writeLine( "  Suitability:       " + (std::string)( this->clDeviceAvailable ? "Available" : "Unavailable" ) + ", " + (std::string)( this->clDeviceCompilerAvailable ? "Compiler found" : "No compiler available" ), true, wColour );
	pLog->writeLine( "  Processor type:    " + std::string( this->clDeviceName ), true, wColour );
	pLog->writeLine( "  Vendor:            " + std::string( this->clDeviceVendor ), true, wColour );
	pLog->writeLine( "  OpenCL driver:     " + std::string( this->clDeviceOpenCLDriver ), true, wColour );
	pLog->writeLine( "  Compute units:     " + toString( this->clDeviceComputeUnits ), true, wColour );
	pLog->writeLine( "  Profile:           " + (std::string)( std::string( this->clDeviceProfile ).compare( "FULL_PROFILE" ) == 0 ? "Full" : "Embedded" ), true, wColour );
	pLog->writeLine( "  Clock speed:       " + toString( this->clDeviceClockFrequency) + " MHz", true, wColour );
	pLog->writeLine( "  Memory:            " + toString( (unsigned int)(this->clDeviceGlobalMemSize / 1024 / 1024) ) + " Mb", true, wColour );
	pLog->writeLine( "  OpenCL C:          " + std::string( this->clDeviceOpenCLVersion ), true, wColour );
	pLog->writeLine( "  Max global size:   " + toString( this->clDeviceGlobalSize ), true, wColour );
	pLog->writeLine( "  Max group items:   " + toString( this->clDeviceMaxWorkGroupSize ), true, wColour );
	pLog->writeLine( "  Max group:         " + ssGroupDimensions.str(), true, wColour );
	pLog->writeLine( "  Max constant args: " + toString( this->clDeviceMaxConstants ), true, wColour );
	pLog->writeLine( "  Max allocation:    " + toString( this->clDeviceMaxMemAlloc / 1024 / 1024 ) + "MB", true, wColour );
	pLog->writeLine( "  Max argument size: " + toString( this->clDeviceMaxParamSize / 1024 ) + "kB", true, wColour );
	pLog->writeLine( "  Double precision:  " + sDoubleSupport, true, wColour );

	pLog->writeDivide();
}

/*
 *  Create the context and command queue for this device
 */
void COCLDevice::createQueue()
{
	cl_int	iErrorID;

	if ( !this->isSuitable() )
	{
		model::doError(
			"Unsuitable device discovered. May be in use already.",
			model::errorCodes::kLevelWarning
		);
		return;
	}

	pManager->log->writeLine( "Creating an OpenCL device context and command queue." );

	this->clContext = clCreateContext( 
		NULL,						// Properties (nothing special required) 
		1,							// Number of devices
		&this->clDevice,			// Device
		NULL,						// Error handling callback
		NULL,						// User data argument for the error handling callback
		&iErrorID					// Error ID pointer
	);

	if ( iErrorID != CL_SUCCESS ) 
	{
		model::doError( 
			"Error creating device context.", 
			model::errorCodes::kLevelWarning
		);
		return;
	}

	this->clQueue = clCreateCommandQueue(
		this->clContext,
		this->clDevice,
		CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
		&iErrorID
	);

	if ( iErrorID != CL_SUCCESS ) 
	{
		model::doError( 
			"Error creating device command queue.", 
			model::errorCodes::kLevelWarning
		);
		return;
	}

	pManager->log->writeLine( "Command queue created for device successfully." );
}

/*
 *  Is this device suitable for use?
 */
bool COCLDevice::isSuitable()
{
	if ( !this->clDeviceAvailable )
	{
		pManager->log->writeLine( "Device is not available." );
		return false;
	}

	if ( !this->clDeviceCompilerAvailable )
	{
		pManager->log->writeLine( "No compiler is available." );
		return false;
	}

	// Other restrictions, like the memory and work group sizes?
	// ...

	return true;
}

/*
 *  Is this device ready for use?
 */
bool COCLDevice::isReady()
{
	if ( !this->isSuitable() )
	{
		pManager->log->writeLine( "Device is not considered suitable." );
		return false;
	}

	if ( !this->clContext ||
		 !this->clQueue ||
		 this->bErrored == true )
	{
		pManager->log->writeLine( "No context, queue or an error occured on device." );
		if ( !this->clContext )
			pManager->log->writeLine( " - No context" );
		if ( !this->clQueue )
			pManager->log->writeLine( " - No command queue" );
		if ( !this->bErrored )
			pManager->log->writeLine( " - Device error" );
		return false;
	}

	return true;
}

/*
 *  Is this device filtered, and therefore should be ignored for execution steps?
 */
bool COCLDevice::isFiltered()
{
	if ( !( pManager->getExecutor()->getDeviceFilter() & model::filters::devices::devicesGPU ) &&
			this->clDeviceType == CL_DEVICE_TYPE_GPU )
			return true;
	if ( !( pManager->getExecutor()->getDeviceFilter() & model::filters::devices::devicesCPU ) &&
			this->clDeviceType == CL_DEVICE_TYPE_CPU )
			return true;
	if ( !( pManager->getExecutor()->getDeviceFilter() & model::filters::devices::devicesAPU ) &&
			this->clDeviceType == CL_DEVICE_TYPE_ACCELERATOR )
			return true;

	return false;
}

/*
 *  Enqueue a buffer command to synchronise threads
 */
void	COCLDevice::queueBarrier()
{
#ifdef USE_SIMPLE_ARCH_OPENCL
	// Causes crashes... for some reason... Review later.
	return;
#endif
	clEnqueueBarrier( this->clQueue );
}

/*
 *  Block program execution until all commands in the queue are
 *  completed.
 */
void	COCLDevice::blockUntilFinished()
{
	this->bBusy = true;
	clFlush( this->clQueue );
	clFinish( this->clQueue );
	this->bBusy = false;
}

/*
 *  Does this device fully support the required aspects of double precision
 */
bool COCLDevice::isDoubleCompatible()
{
	return ( this->clDeviceDoubleFloatConfig & 
		 ( CL_FP_FMA | CL_FP_ROUND_TO_NEAREST | CL_FP_ROUND_TO_ZERO | CL_FP_ROUND_TO_INF | CL_FP_INF_NAN | CL_FP_DENORM ) ) == 
		 ( CL_FP_FMA | CL_FP_ROUND_TO_NEAREST | CL_FP_ROUND_TO_ZERO | CL_FP_ROUND_TO_INF | CL_FP_INF_NAN | CL_FP_DENORM );
}

/*
 *  Release the event otherwise the 500 limit will be hit
 */
void CL_CALLBACK COCLDevice::defaultCallback( cl_event clEvent, cl_int iStatus, void * vData )
{
	//unsigned int uiDeviceNo = *(unsigned int*)vData; // Unused
	clReleaseEvent( clEvent );
}

/*
 * Flush the work to the device and use a marker we can wait on
 */
void COCLDevice::flushAndSetMarker()
{
	this->bBusy	= true;

#ifdef USE_SIMPLE_ARCH_OPENCL
	this->blockUntilFinished();
	return;
#endif
	
	if ( clMarkerEvent != NULL )
	{
		clReleaseEvent( clMarkerEvent );
	}

	// NOTE: OpenCL 1.2 uses clEnqueueMarkerWithWaitList() instead
	clEnqueueMarker(
		 clQueue,
		 &clMarkerEvent 
	);

	clSetEventCallback(
		clMarkerEvent,
		CL_COMPLETE,
		COCLDevice::markerCallback,
		static_cast<void*>( &this->uiDeviceNo )
	);

	clFlush(clQueue);
}

/*
 *	Flush the work to the device
 */
void	COCLDevice::flush()
{
	clFlush(clQueue);
}

/*
 *  Mark this device as no longer being busy so we can queue some more work
 */
void CL_CALLBACK COCLDevice::markerCallback( cl_event clEvent, cl_int iStatus, void * vData )
{
	unsigned int uiDeviceNo = *(unsigned int*)vData;
	clReleaseEvent( clEvent );

	COCLDevice* pDevice = pManager->getExecutor()->getDevice( uiDeviceNo );
	pDevice->markerCompletion();
}

/*
 *  Triggered once the marker callback has been, but this is a non-static function
 */
void COCLDevice::markerCompletion()
{
	this->clMarkerEvent = NULL;
	this->bBusy			= false;
}

/*
*  Is this device currently busy?
*/
bool COCLDevice::isBusy()
{
	// To use the callback mechanism...
	return this->bBusy;

	if (clMarkerEvent == NULL)
		return false;

	cl_int iStatus;
	size_t szStatusSize;
	cl_int iQueryStatus = clGetEventInfo(
		clMarkerEvent,
		CL_EVENT_COMMAND_EXECUTION_STATUS,
		sizeof(cl_int),
		&iStatus,
		&szStatusSize
	);

	if (iQueryStatus != CL_SUCCESS)
		return true;

	pManager->log->writeLine("Exec status for device #" + toString(uiDeviceNo)+" is " + toString(iStatus));
	if (iStatus == CL_COMPLETE)
	{
		return false;
	}
	else {
		return true;
	}
}


/*
 *  Get a short name for the device
 */
std::string		COCLDevice::getDeviceShortName( void )
{
	std::string sName = "";

	if ( this->clDeviceType & CL_DEVICE_TYPE_CPU )			sName = "CPU ";
	if ( this->clDeviceType & CL_DEVICE_TYPE_GPU )			sName = "GPU ";
	if ( this->clDeviceType & CL_DEVICE_TYPE_ACCELERATOR )	sName = "APU ";

	sName += toString( this->uiDeviceNo );

	return sName;
}

/*
 *  Fetch a struct with summary information
 */
void COCLDevice::getSummary(  COCLDevice::sDeviceSummary & pSummary )
{
	std::string sType = "Unknown";

	if (this->clDeviceType & CL_DEVICE_TYPE_CPU)			sType = "CPU";
	if (this->clDeviceType & CL_DEVICE_TYPE_GPU)			sType = "GPU";
	if (this->clDeviceType & CL_DEVICE_TYPE_ACCELERATOR)	sType = "APU";

	std::strncpy(pSummary.cDeviceName, this->clDeviceName, 99 );
	std::strncpy(pSummary.cDeviceType, sType.c_str(), 9 );
	pSummary.uiDeviceID = this->uiDeviceNo;
	pSummary.uiDeviceNumber = this->uiDeviceNo + 1;
}
