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
 *  OpenCL program
 * ------------------------------------------
 *
 */

// Includes
#include <boost/lexical_cast.hpp>
#include <boost/unordered_map.hpp>
#include <boost/algorithm/string.hpp>
#include "../../common.h"
#include "COCLProgram.h"
#include "COCLKernel.h"

/*
 *  Constructor
 */
COCLProgram::COCLProgram(
		CExecutorControlOpenCL*		execController,
		COCLDevice*					device
	)
{
	this->execController		= execController;
	this->device				= device;
	this->clProgram				= NULL;
	this->clContext				= device->getContext();
	this->bCompiled				= false;
	this->sCompileParameters	= "";
}

/*
 *  Destructor
 */
COCLProgram::~COCLProgram()
{
	if ( this->clProgram != NULL )
		clReleaseProgram( this->clProgram );

	clearCode();
}

/*
 *  Attempt to build the program
 */
bool COCLProgram::compileProgram(
		bool	bIncludeStandardElements
	)
{
	cl_int		iErrorID;

#ifdef USE_SIMPLE_ARCH_OPENCL
	// Use GCC argument instead
	this->sCompileParameters += " -fsingle-precision-constant";
#else
	// Is the device configured to force single-precision?
	if (this->bForceSinglePrecision)
		this->sCompileParameters += " -cl-single-precision-constant";

	// Some standard things we need
	this->sCompileParameters += " -cl-mad-enable";
#endif

	// This might not be a good idea...
	//this->sCompileParameters += " -cl-denorms-are-zero";

	// For intel debugging only!
	//this->sCompileParameters += " -g";

	pManager->log->writeLine( "Compiling a new program for device #" + toString( this->device->getDeviceID() ) + "." );

	// Should we add standard elements to the code stack first?
	if ( bIncludeStandardElements )
	{
		this->prependCodeFromResource( "CLUniversalHeader_H" );						// Standard header with things like gravitational acceleration.
		this->prependCode( this->getExtensionsHeader() );							// Required for double-precision etc.
		this->prependCode( this->getConstantsHeader() );							// Domain constant data (i.e. rows, cols etc.)
	}

	cl_uint			uiStackLength	= static_cast<cl_uint>( oclCodeStack.size() );
	OCL_RAW_CODE*	orcCode			= new char* [ uiStackLength ];
	for ( unsigned int i = 0; i < uiStackLength; i++ )
		orcCode[ i ] = oclCodeStack[ i ];

	clProgram = clCreateProgramWithSource(
		this->clContext,
		uiStackLength,
		const_cast<const char**>(orcCode),
		NULL,							// All char* must be null terminated because we don't pass any lengths!
		&iErrorID
	);

	if ( iErrorID != CL_SUCCESS )
	{
		model::doError(
			"Could not create a program to run on device #" + toString( this->device->getDeviceID() ) + ".",
			model::errorCodes::kLevelModelStop
		);
		return false;
	}

	iErrorID = clBuildProgram(
		clProgram,																		// Program
		NULL,																			// Num. devices
		NULL,																			// Device list
		sCompileParameters.c_str(),														// Options (no  -cl-finite-math-only  -cl-denorms-are-zero)
		NULL,																			// Callback
		NULL																			// Callback data
	);

	if ( iErrorID != CL_SUCCESS )
	{
		model::doError(
			"Could not build the program to run on device #" + toString( this->device->getDeviceID() ) + ".",
			model::errorCodes::kLevelModelStop
		);
		pManager->log->writeDivide();
		pManager->log->writeLine( this->getCompileLog(), false );
		pManager->log->writeDivide();
		pManager->log->writeDebugFile( orcCode, uiStackLength );
		return false;
	}

	pManager->log->writeLine( "Program successfully compiled for device #" + toString( this->device->getDeviceID() ) + "." );

	std::string sBuildLog = this->getCompileLog();
	if ( sBuildLog.length() > 0 )
	{
		model::doError( "Some messages were reported while building.", model::errorCodes::kLevelWarning );
		pManager->log->writeDivide();
		pManager->log->writeLine( sBuildLog, false );
		pManager->log->writeDivide();
	}

	// Write debug file containing the concatenated code
	// TODO: Make debug outputs configurable/optional/debug only
	pManager->log->writeDebugFile( orcCode, uiStackLength );

	delete[] orcCode;

	this->bCompiled = true;
	return true;
}

/*
 *  Append code to the program stack
 */
void COCLProgram::appendCode(
		OCL_RAW_CODE	oclCode
	)
{
	oclCodeStack.push_back( oclCode );
}

/*
 *  Prepend code to the program stack
 */
void COCLProgram::prependCode(
		OCL_RAW_CODE	oclCode
	)
{
	oclCodeStack.insert( oclCodeStack.begin(), oclCode );
}

/*
 *  Append code to the program stack from an embedded resource
 */
void COCLProgram::appendCodeFromResource(
		std::string sFilename
	)
{
	appendCode(
		device->getExecController()->getOCLCode( sFilename )
	);
}

/*
 *  Prepend code to the program stack from an embedded resource
 */
void COCLProgram::prependCodeFromResource(
		std::string sFilename
	)
{
	prependCode(
		device->getExecController()->getOCLCode( sFilename )
	);
}

/*
 *  Empty the code stack
 */
void COCLProgram::clearCode()
{
	if ( oclCodeStack.size() < 1 ) return;

	for( auto cCode =  oclCodeStack.begin();
			  cCode != oclCodeStack.end();
			  cCode++ )
	{
		delete [] (*cCode);
	}

	oclCodeStack.clear();
}

/*
 *  Fetch a kernel from within the program and create a class to handle it
 */
COCLKernel* COCLProgram::getKernel(
		const char*		cKernelName
	)
{
	if ( !this->bCompiled ) return NULL;
	
	return new COCLKernel(
		this,
		std::string( cKernelName )
	);
}

/*
 *  Get the compile log, mostly used for error messages
 */
std::string COCLProgram::getCompileLog()
{
	std::string	sLog;
	size_t		szLogLength;
	cl_int		iErrorID;

	iErrorID = clGetProgramBuildInfo(
		clProgram,
		this->device->getDevice(),
		CL_PROGRAM_BUILD_LOG,
		NULL,
		NULL,
		&szLogLength
	);

	if ( iErrorID != CL_SUCCESS )
	{
		// The model cannot continue in this case
		model::doError(
			"Could not obtain a build log for the program on device #" + toString( this->device->getDeviceID() ) + ".",
			model::errorCodes::kLevelModelStop
		);
		return "An error occured";
	}

	char* cBuildLog = new char[ szLogLength + 1 ];

	iErrorID = clGetProgramBuildInfo(
		clProgram,
		this->device->getDevice(),
		CL_PROGRAM_BUILD_LOG,
		szLogLength,
		cBuildLog,
		NULL
	);

	if ( iErrorID != CL_SUCCESS )
	{
		// The model cannot continue in this case
		model::doError(
			"Could not obtain a build log for the program on device #" + toString( this->device->getDeviceID() ) + ".",
			model::errorCodes::kLevelModelStop
		);
		return "An error occured";
	}

	cBuildLog[ szLogLength ] = 0;

	sLog = std::string( cBuildLog );
	delete[] cBuildLog;

	boost::trim(sLog);
	return sLog;
}

/*
 *  Add a compiler argument to the list
 */
void COCLProgram::addCompileParameter(
		std::string sParameter 
	)
{
	sCompileParameters += " " + sParameter;
}

/*
 *  Add a new constant to the register for inclusion in all OpenCL programs
 */
bool COCLProgram::registerConstant( std::string sName, std::string sValue )
{
	this->uomConstants[ sName ] = sValue;

	return true;
}

/*
 *  Remove a single named constant
 */
bool COCLProgram::removeConstant( std::string sName )
{
	if ( this->uomConstants.find( sName ) == this->uomConstants.end() )
		return false;

	this->uomConstants.erase( sName );

	return true;
}

/*
 *  Remove all of the registered constants
 */
void COCLProgram::clearConstants()
{
	this->uomConstants.clear();
}

/*
 *  Get OpenCL code representing the constants defined
 */
OCL_RAW_CODE COCLProgram::getConstantsHeader()
{
	std::stringstream ssHeader;
	ssHeader << std::endl;

	for( std::unordered_map< std::string, std::string >::iterator
		 uomItConstants  = this->uomConstants.begin();
		 uomItConstants != this->uomConstants.end();
		 ++uomItConstants )
	{
		ssHeader << "#define " << uomItConstants->first
				 << " " << uomItConstants->second <<
				 std::endl;
	}

	char*	cHeader = new char[ ssHeader.str().length() + 1 ];
	std::strcpy( cHeader, ssHeader.str().c_str() );
	return cHeader;
}

/*
 *  Create a header for inclusion in all code that handles the support
 *  or lack thereof for double precision, etc.
 */
OCL_RAW_CODE	COCLProgram::getExtensionsHeader()
{
	std::stringstream	ssHeader;

	// Double precision support available?
	// Check for all of the required features, assuming we will need them all...
	if ( this->device->isDoubleCompatible() && !this->bForceSinglePrecision )
	{
		// Include the pragma directive to enable double support
		// AMD GPU devices don't yet support the full Khronos-approved extension
		if( std::strcmp( this->device->getVendor(), "Advanced Micro Devices, Inc." ) == 0 &&
			this->device->getDeviceType() &	CL_DEVICE_TYPE_GPU &&											// Support introduced in new AMD drivers for the proper extension
			std::strstr( this->device->getOCLVersion(), "OpenCL 1.0" ) == NULL &&
			std::strstr( this->device->getOCLVersion(), "OpenCL 1.1" ) == NULL 
		   )
		{
			ssHeader << "#pragma OPENCL EXTENSION cl_amd_fp64 : enable" << std::endl;
		} else {
			ssHeader << "#pragma OPENCL EXTENSION cl_khr_fp64 : enable" << std::endl;
		}

		// Create an alias
		ssHeader << "typedef double      cl_double;" << std::endl;
		ssHeader << "typedef double2     cl_double2;" << std::endl;
		//ssHeader << "typedef double3     cl_double3;" << std::endl;
		ssHeader << "typedef double4     cl_double4;" << std::endl;
		ssHeader << "typedef double8     cl_double8;" << std::endl;
		//ssHeader << "typedef double16     cl_double16;" << std::endl;
	} else {
		// Send warning to log
		model::doError(
			"Double-precision will be handled as single-precision.",
			model::errorCodes::kLevelWarning
		);

		// Create an alias and treat doubles as floats
		ssHeader << "typedef float		cl_double;" << std::endl;
		ssHeader << "typedef float2		cl_double2;" << std::endl;
		//ssHeader << "typedef float3		cl_double3;" << std::endl;
		ssHeader << "typedef float4		cl_double4;" << std::endl;
		ssHeader << "typedef float8		cl_double8;" << std::endl;
		//ssHeader << "typedef float16	cl_double16;" << std::endl;
	}

	char*	cHeader = new char[ ssHeader.str().length() + 1 ];
	std::strcpy( cHeader, ssHeader.str().c_str() );
	return cHeader;
}

/*
 *  Should we force the device to use single precision?
 */
void	COCLProgram::setForcedSinglePrecision( bool bForce )
{
	this->bForceSinglePrecision	= bForce;
}

