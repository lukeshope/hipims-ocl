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
 *  OpenCL memory buffer
 * ------------------------------------------
 *
 */

// Includes
#include <boost/lexical_cast.hpp>

#include "../../common.h"
#include "../opencl.h"
#include "COCLDevice.h"
#include "COCLBuffer.h"
#include "COCLProgram.h"

/*
 *  Constructor
 */
COCLBuffer::COCLBuffer(
		std::string		sName,
		COCLProgram*	pProgram,
		bool			bReadOnly,
		bool			bExistsOnHost,
		cl_ulong		ulSize,
		bool			bAllocateNow
	)
{
	this->sName				= sName;
	this->bReady			= false;
	this->bInternalBlock	= bExistsOnHost;
	this->pHostBlock		= NULL;
	this->bExistsOnHost		= bExistsOnHost;
	this->bReadOnly			= bReadOnly;
	this->ulSize			= ulSize;
	this->clContext			= pProgram->clContext;
	this->uiDeviceID		= pProgram->getDevice()->uiDeviceNo;
	this->clQueue			= pProgram->getDevice()->clQueue;
	this->pDevice			= pProgram->getDevice();
	this->clBuffer			= NULL;
	this->fCallbackRead		= COCLDevice::defaultCallback;
	this->fCallbackWrite	= COCLDevice::defaultCallback;
	this->clFlags			= 0;
	this->clFlags		   |= ( this->bReadOnly ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE ); 

	// In theory it should be possible to use USE_HOST_PTR and reduce memory consumption
	// but in a DLL environment it's a bit of a nightmare.
	if (this->bExistsOnHost)
	{
		// Use this for GPUs
		this->clFlags |= CL_MEM_COPY_HOST_PTR;
	}

	if ( bAllocateNow )
		allocateHostBlock( this->ulSize );
}

/*
 *  Destructor
 */
COCLBuffer::~COCLBuffer()
{
	if ( this->clBuffer != NULL )
		clReleaseMemObject( this->clBuffer );

	if ( bInternalBlock && pHostBlock != NULL )
		delete [] this->pHostBlock;
}

/*
 *  Create the OpenCL buffer
 */
bool COCLBuffer::createBuffer()
{
	cl_int	iErrorID;

	// If the memory block is going to reside within this class
	// and it hasn't been allocated, do so now...
	if ( this->bInternalBlock && 
		 this->bExistsOnHost &&
		 this->pHostBlock == NULL &&
		 this->ulSize != NULL )
		allocateHostBlock( this->ulSize );

	// Need to at least have a size in any case
	if ( this->ulSize < 0 || this->ulSize == NULL )
	{
		model::doError( 
			"Memory buffer '" + this->sName + "' has no size.",
			model::errorCodes::kLevelModelStop
		);
		return false;
	}

	// TODO: Look at using CL_MEM_COPY_HOST_PTR to initialise memory to zeros if required
	// TODO: Verify the buffer size is acceptable for the device

	clBuffer = clCreateBuffer(
		this->clContext,
		clFlags,
		static_cast<size_t>( ulSize ),
		pHostBlock,
		&iErrorID
	);

	if ( iErrorID != CL_SUCCESS )
	{
		// The model cannot continue in this case
		model::doError(
			"Memory buffer creation failed for '" + this->sName + "'. Error " + toString( iErrorID ) + ".",
			model::errorCodes::kLevelModelStop
		);
		return NULL;
	}

	this->bReady = true;

	pManager->log->writeLine(
		"Memory buffer created for '" + this->sName + "' with " + toString( this->ulSize ) + " bytes."
	);

	return true;
}

/*
 *  Create the OpenCL buffer but first initialise its values
 */
bool COCLBuffer::createBufferAndInitialise()
{
	this->clFlags	   |= CL_MEM_COPY_HOST_PTR;
	this->clFlags	   |= CL_MEM_ALLOC_HOST_PTR;

	allocateHostBlock( this->ulSize );

	return createBuffer();
}

/*
 *  Set the location of the host-copy of the buffer if it's not within this class instance
 */
void COCLBuffer::setPointer(
		void*		pLocation,
		cl_ulong	ulSize
	)
{
	this->pHostBlock = pLocation;
	this->ulSize	 = ulSize;
	bInternalBlock = false;
}

/*
 *  Allocate a block of memory within this class instance
 */
void COCLBuffer::allocateHostBlock(
		cl_ulong	ulSize
	)
{
	try {
		this->pHostBlock = new cl_uchar[ ulSize ]();
	} 
	catch ( std::bad_alloc )
	{
		model::doError(
			"Memory allocation failure for '" + this->sName + "'. Size is probably too large.",
			model::errorCodes::kLevelFatal
		);
		return;
	}

	this->ulSize	 = ulSize;
	bInternalBlock = true;
}

/*
*  Attempt to write all of the buffer to the device
*/
void COCLBuffer::queueReadAll()
{
	queueReadPartial(0, static_cast<size_t>(this->ulSize));
}

/*
 *  Attempt to read all of the buffer contents back from the device
 */
void COCLBuffer::queueReadPartial(cl_ulong ulOffset, size_t ulSize, void* pMemBlock)
{
	cl_event	clEvent = NULL;

	if (pMemBlock == NULL)
		pMemBlock = static_cast<char*>(this->pHostBlock) + ulOffset;
		
	pDevice->markBusy();
		
	// Add a read buffer to the queue (non-blocking)
	// Calling functions are expected to handle barriers etc.
	cl_int	iReturn = clEnqueueReadBuffer(
		this->clQueue,				// Device queue
		clBuffer,					// Buffer object
		CL_FALSE,					// Blocking?
		ulOffset,					// Offset
		ulSize,						// Size
		pMemBlock,					// Target pointer
		NULL,						// No. of events in wait list
		NULL,						// Wait list
		( fCallbackRead != NULL && fCallbackRead != COCLDevice::defaultCallback ? &clEvent : NULL )					// Event pointer
	);

	if ( iReturn != CL_SUCCESS )
	{
		pManager->log->writeLine("Error code returned from memory read is " + toString(iReturn));
		model::doError(
			"Unable to read memory buffer from device back to host  " 
			+ this->sName + " (" + toString( iReturn ) + ")",
			model::errorCodes::kLevelModelStop
		);
	}

	if ( fCallbackRead != NULL && fCallbackRead != COCLDevice::defaultCallback )
	{
		iReturn = clSetEventCallback(
			clEvent,
			CL_COMPLETE,
			fCallbackRead,
			&this->uiDeviceID
		);

		if ( iReturn != CL_SUCCESS )
		{
			model::doError(
				"Attaching thread callback failed for device #" + toString( this->uiDeviceID ) + ".",
				model::errorCodes::kLevelModelStop
			);
			return;
		}
	}	
}

/*
 *  Attempt to write all of the buffer to the device
 */
void COCLBuffer::queueWriteAll()
{
	queueWritePartial( 0, static_cast<size_t>( this->ulSize ) );
}


/*
 *  Attempt to write part of a buffer
 */
void COCLBuffer::queueWritePartial(cl_ulong ulOffset, size_t ulSize, void* pMemBlock )
{
	// Store the event returned, so the calling function
	// can use it to block etc.
	cl_event	clEvent = NULL;

	// Use the data held in this buffer object unless told otherwise
	if (pMemBlock == NULL)
		pMemBlock = static_cast<char*>(this->pHostBlock) + ulOffset;

	pDevice->markBusy();
		
	// Add a read buffer to the queue (non-blocking)
	// Calling functions are expected to handle barriers etc.
	cl_int	iReturn = clEnqueueWriteBuffer(
		this->clQueue,				// Device queue
		clBuffer,					// Buffer object
		CL_FALSE,					// Blocking?
		ulOffset,					// Offset
		ulSize,						// Size
		pMemBlock,					// Source pointer
		NULL,						// No. of events in wait list
		NULL,						// Wait list
		( fCallbackWrite != NULL && fCallbackWrite != COCLDevice::defaultCallback ? &clEvent : NULL )					// Event pointer
	);

	// Did any errors occur?
	if ( iReturn != CL_SUCCESS )
	{
		model::doError(
			"Unable to write to memory buffer for device\n  " 
			+ this->sName + " (" + toString( iReturn ) + ")\n"
			+ "  Offset: " + toString( ulOffset ) 
			+ "  Size: " + toString( ulSize ) 
			+ "  Pointer: " + toString( pMemBlock )
			+ "  Buf size: " + toString( this->ulSize ),
			model::errorCodes::kLevelModelStop
		);
	}

	// Attach a callback
	if ( fCallbackWrite != NULL && fCallbackWrite != COCLDevice::defaultCallback )
	{
		iReturn = clSetEventCallback(
			clEvent,
			CL_COMPLETE,
			fCallbackWrite,
			&this->uiDeviceID
		);

		if ( iReturn != CL_SUCCESS )
		{
			model::doError(
				"Attaching thread callback failed for device #" + toString( this->uiDeviceID ) + ".",
				model::errorCodes::kLevelModelStop
			);
			return;
		}
	}
}