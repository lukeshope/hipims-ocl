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
 *  Handles cascading execution and enumeration 
 *  on OpenCL devices
 * ------------------------------------------
 *
 */

// Includes
#include "../../common.h"
#include "CExecutorControlOpenCL.h"
#include "COCLDevice.h"
#include "../../Datasets/CXMLDataset.h"
#include <vector>
#include <boost/lexical_cast.hpp>

/*
 *  Constructor
 */
CExecutorControlOpenCL::CExecutorControlOpenCL(void)
{
	this->clDeviceTotal = 0;
	this->uiSelectedDeviceID = NULL;

	if ( !this->getPlatforms() ) return;

	this->logPlatforms();
}

/*
 *  Setup the executor using parameters specified in the configuration file
 */
void CExecutorControlOpenCL::setupFromConfig( XMLElement* pXNode )
{
	XMLElement*		pParameter			= pXNode->FirstChildElement();
	char			*cParameterName		= NULL;
	char			*cParameterValue	= NULL;
	unsigned int	uiDeviceFilter		= model::filters::devices::devicesCPU | 
										  model::filters::devices::devicesGPU |
										  model::filters::devices::devicesAPU;

	while ( pParameter != NULL )
	{
		Util::toLowercase( &cParameterName, pParameter->Attribute( "name" ) );
		Util::toLowercase( &cParameterValue, pParameter->Attribute( "value" ) );

		if ( strcmp( cParameterName, "devicefilter" ) == 0 )
		{ 
			uiDeviceFilter = 0;
			if ( strstr( cParameterValue, "cpu" ) != NULL )	
				uiDeviceFilter |= model::filters::devices::devicesCPU;
			if ( strstr( cParameterValue, "gpu" ) != NULL )	
				uiDeviceFilter |= model::filters::devices::devicesGPU;
			if ( strstr( cParameterValue, "apu" ) != NULL )	
				uiDeviceFilter |= model::filters::devices::devicesAPU;
		}
		else 
		{
			model::doError(
				"Unrecognised parameter: " + std::string( cParameterName ),
				model::errorCodes::kLevelWarning
			);
		}

		pParameter = pParameter->NextSiblingElement();
	}

	this->setDeviceFilter( uiDeviceFilter );
	if ( !this->createDevices() ) return;
}

/*
 *  Destructor
 */
CExecutorControlOpenCL::~CExecutorControlOpenCL(void)
{
	for ( unsigned int iPlatformID = 0; iPlatformID < this->clPlatformCount; iPlatformID++ )
	{
		delete[] this->platformInfo[ iPlatformID ].cExtensions;
		delete[] this->platformInfo[ iPlatformID ].cName;
		delete[] this->platformInfo[ iPlatformID ].cProfile;
		delete[] this->platformInfo[ iPlatformID ].cVendor;
		delete[] this->platformInfo[ iPlatformID ].cVersion;
	}
	delete[] this->platformInfo;

	for ( unsigned int iDeviceID = 0; iDeviceID < this->clDeviceTotal; iDeviceID++ )
	{
		delete this->pDevices[ iDeviceID ];
	}

	this->pDevices.clear();
	delete [] this->clPlatforms;

	pManager->log->writeLine( "The OpenCL executor is now unloaded." );
}

/*
 *  Ascertain the number of, and store a pointer to each device
 *  available to us.
 */
bool CExecutorControlOpenCL::getPlatforms(void)
{
	if ( this->clDeviceTotal > 0 )
		model::doError( "An attempt to obtain OpenCL platforms for a second time is invalid.", model::errorCodes::kLevelFatal );

	cl_int			iErrorID;
	unsigned int	iPlatformID;

	iErrorID = clGetPlatformIDs( NULL, NULL, &this->clPlatformCount );

	if ( iErrorID != CL_SUCCESS ) 
	{
		model::doError( 
			"Error obtaining the number of CL platforms.", 
			model::errorCodes::kLevelFatal
		);
		return false;
	}

	this->clPlatforms		  = new cl_platform_id[ this->clPlatformCount ];
	this->platformInfo		  = new sPlatformInfo[ this->clPlatformCount ];

	iErrorID = clGetPlatformIDs( this->clPlatformCount, this->clPlatforms, &this->clPlatformCount );

	if ( iErrorID != CL_SUCCESS ) 
	{
		model::doError( 
			"Error obtaining the CL platforms.", 
			model::errorCodes::kLevelFatal 
		);
		return false;
	}

	for ( iPlatformID = 0; iPlatformID < this->clPlatformCount; iPlatformID++ )
	{
		this->platformInfo[ iPlatformID ].cProfile		= this->getPlatformInfo( iPlatformID, CL_PLATFORM_PROFILE );
		this->platformInfo[ iPlatformID ].cVersion		= this->getPlatformInfo( iPlatformID, CL_PLATFORM_VERSION );
		this->platformInfo[ iPlatformID ].cName			= this->getPlatformInfo( iPlatformID, CL_PLATFORM_NAME );
		this->platformInfo[ iPlatformID ].cVendor		= this->getPlatformInfo( iPlatformID, CL_PLATFORM_VENDOR );
		this->platformInfo[ iPlatformID ].cExtensions	= this->getPlatformInfo( iPlatformID, CL_PLATFORM_EXTENSIONS );
	}

	for ( iPlatformID = 0; iPlatformID < this->clPlatformCount; iPlatformID++ )
	{
		iErrorID = clGetDeviceIDs( this->clPlatforms[ iPlatformID ], CL_DEVICE_TYPE_ALL, 0, NULL, &this->platformInfo[ iPlatformID ].uiDeviceCount );

		if ( iErrorID != CL_SUCCESS ) 
		{
			model::doError( 
				"Error obtaining the number of devices on each CL platform.", 
				model::errorCodes::kLevelFatal
			);
			return false;
		}

		this->clDeviceTotal += this->platformInfo[ iPlatformID ].uiDeviceCount;
	}

	return true;
}

/*
 *  Sends the details about the platforms to the log.
 */
void CExecutorControlOpenCL::logPlatforms(void)
{
	CLog*		pLog			= pManager->log;
	std::string	sPlatformNo;

	unsigned short	wColour		= model::cli::colourInfoBlock;

	pLog->writeDivide();
	pLog->writeLine( "OPENCL PLATFORMS", true, wColour );

	for ( unsigned int iPlatformID = 0; iPlatformID < this->clPlatformCount; iPlatformID++ )
	{
		sPlatformNo = "  " + toString( iPlatformID + 1 ) + ". ";

		pLog->writeLine( 
			sPlatformNo + this->platformInfo[ iPlatformID ].cName,
			true,
			wColour
		);
		pLog->writeLine( 
			std::string( sPlatformNo.size(), ' ' ) + std::string( this->platformInfo[ iPlatformID ].cVersion ) + " with " + toString( this->platformInfo[ iPlatformID ].uiDeviceCount ) + " device(s)" ,
			true,
			wColour
		);
	}

	pLog->writeDivide();
}

/*
 *  Create a new instance of the device class for each device
 *  we can identify on the platforms.
 */
bool CExecutorControlOpenCL::createDevices(void)
{
	cl_int				iErrorID;
	cl_device_id*		clDevice;
	unsigned int		uiDeviceCount	= 0;

	COCLDevice*					pDevice;
	std::vector<COCLDevice*>	pDevices;

	for ( unsigned int iPlatformID = 0; iPlatformID < this->clPlatformCount; iPlatformID++ )
	{
		clDevice = new cl_device_id[ this->platformInfo[ iPlatformID ].uiDeviceCount ];

		iErrorID = clGetDeviceIDs( 
			this->clPlatforms[ iPlatformID ], 
			CL_DEVICE_TYPE_ALL, 
			this->platformInfo[ iPlatformID ].uiDeviceCount, 
			clDevice, 
			NULL 
		);

		if ( iErrorID != CL_SUCCESS ) 
		{
			model::doError( 
				"Error obtaining the devices for CL platform '" + std::string( this->platformInfo[ iPlatformID ].cName ) + "'", 
				model::errorCodes::kLevelFatal
			);
			return false;
		}

		for( unsigned int iDeviceID = 0; iDeviceID < this->platformInfo[ iPlatformID ].uiDeviceCount; iDeviceID++ )
		{
			pDevice = new COCLDevice(
				clDevice[ iDeviceID ],
				iPlatformID,
				uiDeviceCount
			);

			if ( pDevice->isReady() )		
			{
				// Complies with the device filters?
				if ( 
					pDevice->clDeviceType == CL_DEVICE_TYPE_CPU			&& ( ( this->deviceFilter & model::filters::devices::devicesCPU ) == model::filters::devices::devicesCPU ) ||
					pDevice->clDeviceType == CL_DEVICE_TYPE_GPU			&& ( ( this->deviceFilter & model::filters::devices::devicesGPU ) == model::filters::devices::devicesGPU ) ||
					pDevice->clDeviceType == CL_DEVICE_TYPE_ACCELERATOR && ( ( this->deviceFilter & model::filters::devices::devicesAPU ) == model::filters::devices::devicesAPU )
				   )
				{
					pDevices.push_back( pDevice );
					uiDeviceCount++;
					pDevice->logDevice();
				} else {
					pManager->log->writeLine( "Device type is filtered." );
					delete pDevice;
				}
			} else {
				pManager->log->writeLine( "Device is not ready." );
				delete pDevice;
			}
		}
	}

	this->pDevices = pDevices;
	this->clDeviceTotal = uiDeviceCount;

	delete[] clDevice;

	pManager->log->writeLine( "The OpenCL executor is now fully loaded." );
	this->setState( model::executorStates::executorReady );

	return true;
}

/*
 *  Obtain the size and value for a platform info field
 */
char* CExecutorControlOpenCL::getPlatformInfo( unsigned int uiPlatformID, cl_platform_info clInfo )
{
	cl_int		iErrorID;
	size_t		clSize;

	iErrorID = clGetPlatformInfo( 
		this->clPlatforms[ uiPlatformID ], 
		clInfo, 
		NULL, 
		NULL, 
		&clSize 
	);

	char* cValue = new char[ clSize + 1 ];

	iErrorID = clGetPlatformInfo( 
		this->clPlatforms[ uiPlatformID ], 
		clInfo, 
		clSize, 
		cValue, 
		NULL 
	);

	return cValue;
}

/*
 *  Get the code for a specific type of benchmark, to be compiled 
 *  to a kernel.
 */
OCL_RAW_CODE CExecutorControlOpenCL::getOCLCode( std::string sFilename )
{
	return Util::getFileResource( sFilename.c_str(), "OpenCLCode" );
}

/*
 *  Return the currently selected device.
 */
COCLDevice*	CExecutorControlOpenCL::getDevice()
{
	// Verify that a device has been selected, and do so
	// if otherwise.
	if ( this->uiSelectedDeviceID == NULL ) 
		this->selectDevice();

	return this->getDevice(
		this->uiSelectedDeviceID
	);
}

/*
 *  Fetch a device object, required because of some of the static member callbacks.
 */
COCLDevice*	CExecutorControlOpenCL::getDevice( unsigned int uiDeviceNo )
{
	if ( uiDeviceNo > getDeviceCount() ) 
		return NULL;
	if ( uiDeviceNo < 1 )
		return NULL;
	return static_cast<COCLDevice*>( this->pDevices[ uiDeviceNo - 1 ] );
}

/*
 *  Automatically select the best device for the model to use in this instance.
 */
void	CExecutorControlOpenCL::selectDevice()
{
	// Check the device isn't filtered from use, and is seemingly
	// ready (i.e. not already consumed by another process).
	// ...

	// Check the memory requirements of the domain, for the data
	// structure of each model, for suitability.
	// ...

	// Check double precision availability.
	// ...

	// Check the number of compute units available.
	// ...

	// TODO: Implement some system of selecting the best device
	//		 Until then, just pick the first one.
	if ( this->pDevices.size() < 1 ) 
	{
		model::doError(
			"No suitable devices could be found for running this model.",
			model::errorCodes::kLevelModelStop
		);
		return;
	}

	// Select the device
	this->selectDevice( 1 );
}

/*
 *  Select the device specified by the parameter, i.e. force the model to use
 *  a particular device as long as it is suitable. Raise a fatal error otherwise.
 */
void	CExecutorControlOpenCL::selectDevice( unsigned int uiDeviceNo )
{
	if ( uiDeviceNo > this->pDevices.size() )
	{
		model::doError(
			"An invalid device was selected for execution.",
			model::errorCodes::kLevelFatal
		);
		return;
	} else {
		pManager->log->writeLine(
			"Selected device: #" + toString( uiDeviceNo )
		);
	}

	this->uiSelectedDeviceID = uiDeviceNo;
}
