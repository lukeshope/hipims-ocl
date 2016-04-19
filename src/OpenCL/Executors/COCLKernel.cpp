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
 *  OpenCL kernel
 * ------------------------------------------
 *
 */

// Includes
#include <boost/lexical_cast.hpp>
#include <math.h>
#include <cmath>
#include <thread>

#include "../../common.h"
#include "COCLDevice.h"
#include "COCLProgram.h"
#include "COCLBuffer.h"
#include "COCLKernel.h"

/*
 *  Constructor
 */
COCLKernel::COCLKernel(
		COCLProgram*		program,
		std::string			sKernelName
	)
{
	this->program			= program;
	this->sName				= sKernelName;
	this->bReady			= false;
	this->bGroupSizeForced	= false;
	this->clProgram			= program->clProgram;
	this->clKernel			= NULL;
	this->pDevice			= program->getDevice();
	this->uiDeviceID		= program->getDevice()->uiDeviceNo;
	this->clQueue			= program->getDevice()->clQueue;
	this->fCallback			= COCLDevice::defaultCallback;
	this->szGlobalSize[0] = 1;	 this->szGlobalSize[1] = 1;	  this->szGlobalSize[2] = 1;
	this->szGroupSize[0] = 1;	 this->szGroupSize[1] = 1;	  this->szGroupSize[2] = 1;
	this->szGlobalOffset[0] = 0; this->szGlobalOffset[1] = 0; this->szGlobalOffset[2] = 0;

	this->prepareKernel();
}


/*
 *  Destructor
 */
COCLKernel::~COCLKernel()
{
	if ( this->clKernel != NULL )
		clReleaseKernel( clKernel );

	delete [] arguments;
}

/*
 *  Schedule the kernel for execution on the device
 */
void COCLKernel::scheduleExecution()
{
	if ( !this->bReady ) return;

	cl_event		clEvent		= NULL;
	cl_int			iErrorID	= CL_SUCCESS;

	pDevice->markBusy();
	
	iErrorID = clEnqueueNDRangeKernel(
		this->clQueue,
		clKernel,
		3,
		szGlobalOffset,
		szGlobalSize,
		szGroupSize,
		NULL,
		NULL,
		( fCallback != NULL && fCallback != COCLDevice::defaultCallback ? &clEvent : NULL )
	);

	if ( iErrorID != CL_SUCCESS )
	{
		// The model cannot continue in this case
		model::doError(
			"Kernel queue failed for device #" + toString( this->uiDeviceID ) + ". Error " + toString( iErrorID ) + ".\n" 
			+ "  " + sName,
			model::errorCodes::kLevelModelStop
		);
		return;
	}

	if ( fCallback != NULL && fCallback != COCLDevice::defaultCallback )
	{
		iErrorID = clSetEventCallback(
			clEvent,
			CL_COMPLETE,
			fCallback,
			&this->uiDeviceID
		);

		if ( iErrorID != CL_SUCCESS )
		{
			// The model cannot continue in this case, odd though it is...
			// Shouldn't ever really happen, but you never know
			model::doError(
				"Attaching thread callback failed for device #" + toString( this->uiDeviceID ) + ".",
				model::errorCodes::kLevelModelStop
			);
			return;
		}
	}
}

/*
 *  Schedule the kernel for execution on the device
 */
void COCLKernel::scheduleExecutionAndFlush()
{
	cl_int			iErrorID;

	if ( !this->bReady ) return;

	scheduleExecution();

	iErrorID = clFlush(
		this->clQueue
	);

	if ( iErrorID != CL_SUCCESS )
	{
		// The model cannot continue in this case, odd though it is...
		// Shouldn't ever really happen, but you never know
		model::doError(
			"Failed flushing commands to device #" + toString( this->uiDeviceID ) + ".",
			model::errorCodes::kLevelModelStop
		);
		return;
	}
}

/*
 *  Assign all of the arguments, pass NULL if required
 */
bool COCLKernel::assignArguments(
		COCLBuffer*	aBuffers[]
	)
{
	if ( this->clKernel == NULL ) return false;

	pManager->log->writeLine("Assigning arguments for '" + this->sName + "':");

	for( unsigned char i = 0; i < this->uiArgumentCount; i++ )
	{
		if ( aBuffers[ i ] == NULL )
		{
			pManager->log->writeLine(" " + toString( i + 1 ) + ". NULL" );
		}
		else if ( this->assignArgument( i, aBuffers[ i ] ) == false )
		{
			model::doError(
				"Failed to assign a kernel argument for '" + this->sName + "'.",
				model::errorCodes::kLevelModelStop
			);
			return false;
		} else {
			pManager->log->writeLine(" " + toString( i + 1 ) + ". " + aBuffers[i]->getName() );
		}
	}

	this->bReady = true;

	return true;
}
	
/*
 *  Assign a single argument
 */
bool COCLKernel::assignArgument(
		unsigned char	ucArgumentIndex,
		COCLBuffer*		aBuffer
	)
{
	cl_int	iErrorID;
	cl_mem	clBuffer = aBuffer->getBuffer();

	iErrorID = clSetKernelArg(
		clKernel,
		ucArgumentIndex,
		sizeof( &clBuffer ),
		&clBuffer
	);

	if ( iErrorID != CL_SUCCESS )
		return false;

	return true;
}

/*
 *  Prepare the kernel by finding it in the program etc.
 */
void COCLKernel::prepareKernel()
{
	cl_int		iErrorID;
	size_t		szRequiredWGSize[3];

	clKernel = clCreateKernel(
		clProgram,
		sName.c_str(),
		&iErrorID
	);

	if ( iErrorID != CL_SUCCESS )
	{
		model::doError(
			"Could not prepare the kernel to run on device #" + toString( this->program->device->uiDeviceNo ) + ".",
			model::errorCodes::kLevelModelStop
		);
		return;
	}

	pManager->log->writeLine( "Kernel '" + sName + "' prepared for device #" + toString( this->program->device->uiDeviceNo ) + "." );

	// Fetch the kernel details on workgroup sizes etc.
	iErrorID = clGetKernelInfo(
		clKernel,
		CL_KERNEL_NUM_ARGS,
		sizeof( cl_uint ),
		&this->uiArgumentCount,
		NULL
	);

	if ( iErrorID != CL_SUCCESS )
	{
		model::doError(
			"Could not identify argument count for '" + sName + "' kernel.",
			model::errorCodes::kLevelModelStop
		);
		return;
	}

	iErrorID = clGetKernelWorkGroupInfo(
		clKernel,
		this->program->device->clDevice,
		CL_KERNEL_COMPILE_WORK_GROUP_SIZE,
		sizeof( size_t ) * 3,
		&szRequiredWGSize,
		NULL
	);

	if ( iErrorID != CL_SUCCESS )
	{
		model::doError(
			"Could not identify work-group constraints for '" + sName + "' kernel.",
			model::errorCodes::kLevelWarning
		);
		szGroupSize[0] = 1;	szGroupSize[1] = 1;	szGroupSize[2] = 1;
	}
	else {
		// Dimensions of (0,0,0) mean the size was not stipulated...
		if (szRequiredWGSize[0] != 0 || szRequiredWGSize[1] != 0 || szRequiredWGSize[2] != 0)
		{
			bGroupSizeForced = true;
			szGroupSize[0] = szRequiredWGSize[0];	szGroupSize[1] = szRequiredWGSize[1];	szGroupSize[2] = szRequiredWGSize[2];
		}
	}

	iErrorID = clGetKernelWorkGroupInfo(
		clKernel,
		this->program->device->clDevice,
		CL_KERNEL_PRIVATE_MEM_SIZE,
		sizeof( cl_ulong ),
		&this->ulMemPrivate,
		NULL
	);

	if ( iErrorID != CL_SUCCESS )
	{
		model::doError(
			"Could not identify private mem usage for '" + sName + "' kernel.",
			model::errorCodes::kLevelWarning
		);
		this->ulMemPrivate = 0;
	}

	iErrorID = clGetKernelWorkGroupInfo(
		clKernel,
		this->program->device->clDevice,
		CL_KERNEL_LOCAL_MEM_SIZE,
		sizeof( cl_ulong ),
		&this->ulMemLocal,
		NULL
	);

	if ( iErrorID != CL_SUCCESS )
	{
		model::doError(
			"Could not identify local mem usage for '" + sName + "' kernel.",
			model::errorCodes::kLevelWarning
		);
		this->ulMemLocal = 0;
	}

	this->arguments = new COCLBuffer*[ this->uiArgumentCount ];
	
	pManager->log->writeLine( "Kernel '" + sName + "' is defined:" ); 
	pManager->log->writeLine( "  Private memory:   " + toString( this->ulMemPrivate ) + " bytes" ); 
	pManager->log->writeLine( "  Local memory:     " + toString( this->ulMemLocal )   + " bytes" ); 
	pManager->log->writeLine( "  Arguments:        " + toString( this->uiArgumentCount ) ); 
	pManager->log->writeLine( "  Work-group size:  [ " + toString( szRequiredWGSize[0] )   + "," + 
														 toString( szRequiredWGSize[1] )   + "," +
														 toString( szRequiredWGSize[2] )   + " ]"); 

	if ( this->uiArgumentCount == 0 )
		this->bReady = true;
}

/*
 *  Set the global size of the work
 */
void COCLKernel::setGlobalSize(
		cl_ulong	X,
		cl_ulong	Y,
		cl_ulong	Z
	)
{
	X = static_cast<size_t>( ceil( static_cast<double>( X ) / this->szGroupSize[0] ) * this->szGroupSize[0] );
	Y = static_cast<size_t>( ceil( static_cast<double>( Y ) / this->szGroupSize[1] ) * this->szGroupSize[1] );
	Z = static_cast<size_t>( ceil( static_cast<double>( Z ) / this->szGroupSize[2] ) * this->szGroupSize[2] );

	this->szGlobalSize[0] = static_cast<size_t>(X);
	this->szGlobalSize[1] = static_cast<size_t>(Y);
	this->szGlobalSize[2] = static_cast<size_t>(Z);

	pManager->log->writeLine(
		"Global work size for '" + this->sName + "' set to [" +
		toString( this->szGlobalSize[0] ) + "," +
		toString( this->szGlobalSize[1] ) + "," +
		toString( this->szGlobalSize[2] ) + "]."
	);
}

/*
 *  Set the global offset of the work
 */
void COCLKernel::setGlobalOffset(
		cl_ulong	X,
		cl_ulong	Y,
		cl_ulong	Z
	)
{
	this->szGlobalOffset[0]	= static_cast<size_t>( X );
	this->szGlobalOffset[1]	= static_cast<size_t>( Y );
	this->szGlobalOffset[2]	= static_cast<size_t>( Z );
}

/*
 *  Set the work group size
 */
void COCLKernel::setGroupSize(
		cl_ulong	X,
		cl_ulong	Y,
		cl_ulong	Z
	)
{
	if ( this->bGroupSizeForced ) return;

	this->szGroupSize[0]	= static_cast<size_t>( X );
	this->szGroupSize[1]	= static_cast<size_t>( Y );
	this->szGroupSize[2]	= static_cast<size_t>( Z );	

	pManager->log->writeLine(
		"Work-group size for '" + this->sName + "' set to [" +
		toString( this->szGroupSize[0] ) + "," +
		toString( this->szGroupSize[1] ) + "," +
		toString( this->szGroupSize[2] ) + "]."
	);
}