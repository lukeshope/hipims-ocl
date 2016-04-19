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
 *  Application entry point. Instantiates the
 *  management class.
 * ------------------------------------------
 *
 */

// Includes
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include "main.h"
#include "CModel.h"
#include "MPI/CMPIManager.h"
#include "Datasets/CXMLDataset.h"
#include "Datasets/CRasterDataset.h"
#include "OpenCL/Executors/COCLDevice.h"
#include "Domain/CDomainManager.h"
#include "Domain/CDomain.h"
#include "Boundaries/CBoundaryMap.h"

// Globals
CModel*					model::pManager;
CXMLDataset*			pConfigFile;
char*					model::workingDir;
char*					model::logFile;
char*					model::configFile;
char*					model::codeDir;
bool					model::quietMode;
bool					model::forceAbort;
bool					model::gdalInitiated;
bool					model::disableScreen;
bool					model::disableConsole;
#ifdef _WINDLL
model::fnNotifyLog		model::fCallbackLog;
model::fnNotifyProgress	model::fCallbackProgress;
model::fnNotifyComplete	model::fCallbackComplete;
model::fnLoadTopography model::fSendTopography;
#endif

#ifdef _CONSOLE

#ifdef PLATFORM_WIN
/*
 *  Application entry-point. 
 */
int _tmain(int argc, char* argv[])
{
	// Default configurations
	model::workingDir   = NULL;
	model::configFile	= new char[50];
	model::logFile		= new char[50];
	model::codeDir		= NULL;
	model::quietMode	= false;
	model::forceAbort	= false;
	model::gdalInitiated = true;
	model::disableScreen = true;
	model::disableConsole = false;

	std::strcpy( model::configFile, "configuration.xml" );
	std::strcpy( model::logFile,    "_model.log" );

	// Nasty function calls for Windows console stuff
	system("color 17");
	SetConsoleTitle("HiPIMS Simulation Engine");

	model::storeWorkingEnv();
	model::parseArguments( argc, argv );
	CRasterDataset::registerAll();

	int iReturnCode = model::loadConfiguration();
	if ( iReturnCode != model::appReturnCodes::kAppSuccess ) return iReturnCode;
	iReturnCode = model::commenceSimulation();
	if ( iReturnCode != model::appReturnCodes::kAppSuccess ) return iReturnCode;
	iReturnCode = model::closeConfiguration();
	if ( iReturnCode != model::appReturnCodes::kAppSuccess ) return iReturnCode;

	return iReturnCode;
}
#endif
#ifdef PLATFORM_UNIX
/*
 *  Application entry-point. 
 */
int main(int argc, char* argv[])
{
	// Do this first as we might need to disable NCurses
	model::configFile	= new char[50];
	model::logFile		= new char[50];
	std::strcpy( model::configFile, "configuration.xml" );
	std::strcpy( model::logFile,    "_model.log" );
	model::quietMode	= false;
	model::forceAbort	= false;
	model::disableScreen = false;
	model::disableConsole = false;

#ifdef MPI_ON
	int iProvidedThreadSupport;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &iProvidedThreadSupport);
#endif

	model::storeWorkingEnv();
	model::parseArguments( argc, argv );

	// Seg fault handler
	//signal(SIGSEGV, segFaultHandler);  

	// NCurses
	if ( !model::disableScreen )
	{
		initscr();
		scrollok(stdscr, TRUE);
		idlok(stdscr, TRUE);

		if ( has_colors() && can_change_color() )
		{
			start_color();

			// Assign some colours for UNIX systems
			init_pair(1, COLOR_WHITE, COLOR_BLACK);
			init_pair(2, COLOR_RED, COLOR_BLACK);
			init_pair(3, COLOR_CYAN, COLOR_BLACK);
			init_pair(4, COLOR_WHITE, COLOR_BLACK);
			init_pair(5, COLOR_YELLOW, COLOR_BLACK);
		}
	}

	CRasterDataset::registerAll();

	int iReturnCode = model::loadConfiguration();
	if ( iReturnCode != model::appReturnCodes::kAppSuccess ) 
		return iReturnCode;
	iReturnCode = model::commenceSimulation();
	if ( iReturnCode != model::appReturnCodes::kAppSuccess ) 
		return iReturnCode;
	iReturnCode = model::closeConfiguration();
	if ( iReturnCode != model::appReturnCodes::kAppSuccess ) 
		return iReturnCode;

#ifdef MPI_ON
	MPI_Finalize();
#endif

	return iReturnCode;
}
#endif

#endif
#ifdef _WINDLL

/*
 *  DLL entry-point used to store the handle
 */
BOOL WINAPI DllMain(
	_In_ HINSTANCE	hinstDLL,
	_In_ DWORD		fdwReason,
	_In_ LPVOID		lpvReserved
	)
{
	model::hdlDLL = hinstDLL;
	model::gdalInitiated = false;
	return true;
}

/*
 *  DLL entry point for loading a config file
 */
bool __stdcall model::SimulationLoad(
		fnNotifyLog				callbackFnLog,
		fnNotifyProgress		callbackFnProgress,
		fnNotifyComplete		callbackFnComplete,
		model::fnLoadTopography	callbackFnLoadTopography,
		LPCSTR					cWorkingDir,
		LPCSTR					cConfigFile,
		LPCSTR					cLogFile
	)
{
	// Store callback function pointers to be used
	model::fCallbackLog		 = callbackFnLog;
	model::fCallbackProgress = callbackFnProgress;
	model::fCallbackComplete = callbackFnComplete;
	model::fSendTopography   = callbackFnLoadTopography;


	// Specified configuration stuff
	if ( cWorkingDir != NULL )
	{
		model::workingDir	= new char[ std::strlen( cWorkingDir ) + 1 ];
		std::strcpy( model::workingDir, cWorkingDir );
	} else {
		model::workingDir = NULL;
	}

	if ( cConfigFile != NULL )
	{
		model::configFile	= new char[ std::strlen( cConfigFile ) + 1 ];
		std::strcpy( model::configFile, cConfigFile );
	} else {
		model::configFile = NULL;
	}

	if ( cLogFile != NULL )
	{
		model::logFile	= new char[ std::strlen( cLogFile ) + 1 ];
		std::strcpy( model::logFile, cLogFile );
	} else {
		model::logFile = NULL;
	}

	model::quietMode	= true;
	model::forceAbort	= false;
	model::disableScreen = true;
	model::disableConsole = false;

	// Do we need to get some environment data and defaults etc?
	model::storeWorkingEnv();

	if ( !gdalInitiated )
	{
		CRasterDataset::registerAll();
		gdalInitiated = true;
	}

	__int32 iReturnCode = model::loadConfiguration();

	return iReturnCode == 0;
}

/*
 *  DLL entry point for simulation
 */
bool __stdcall model::SimulationLaunch(
		void
	)
{
	__int32 iReturnCode = model::commenceSimulation();
	
	model::fCallbackComplete(
		iReturnCode
	);

	return iReturnCode == 0;
}

/*
 *  DLL entry point for closing down a configuration
 */
bool __stdcall model::SimulationClose(
		void
	)
{
	return model::closeConfiguration() == 0;
}

/*
 *  Abort an ongoing simulation
 */
bool __stdcall model::SimulationAbort( void )
{
	model::forceAbort = true;
	return true; 
}

/*
 *  Fetch the name of a device on the system
 */
BSTR __stdcall model::GetDeviceName( unsigned int uiDeviceID )
{
	std::string	 sDeviceName = "Invalid configuration";

	if ( pManager != NULL )
	{

		// Device IDs start at 1
		COCLDevice*	pDevice = pManager->getExecutor()->getDevice( uiDeviceID + 1 );

		// Get the name
		sDeviceName  = "[" + toString( uiDeviceID + 1 ) + "] " + std::string( pDevice->clDeviceName );

		// Append the type
		if( pDevice->clDeviceType & CL_DEVICE_TYPE_CPU )
			sDeviceName += " (CPU)"; 
		if( pDevice->clDeviceType & CL_DEVICE_TYPE_GPU )
			sDeviceName += " (GPU)"; 
		if( pDevice->clDeviceType & CL_DEVICE_TYPE_ACCELERATOR )
			sDeviceName += " (APU)"; 

	}

	std::wstring wDeviceName = std::wstring( sDeviceName.begin(), sDeviceName.end() );

	return ::SysAllocString( wDeviceName.c_str() );
}

/*
 *  Fetch the number of devices available
 */
unsigned int __stdcall model::GetDeviceCount( void )
{
	if ( pManager == NULL )
		return 0;
	return pManager->getExecutor()->getDeviceCount();
}

/*
 *  Fetch the current device assigned
 */
unsigned int __stdcall model::GetDeviceCurrent( void )
{
	if ( pManager == NULL )
		return 1;
	return pManager->getExecutor()->getDeviceCurrent();
}

/*
*  Fetch information about a domain
*/
model::DomainData __stdcall model::GetDomainInfo(unsigned int uiDomainID)
{
	model::DomainData info;

	if (pManager != NULL &&
		pManager->getDomainSet() != NULL &&
		uiDomainID > 0 &&
		pManager->getDomainSet()->getDomainCount() >= uiDomainID)
	{
		CDomainCartesian* pDomain = static_cast<CDomainCartesian*>(pManager->getDomainSet()->getDomain(uiDomainID - 1));

		pDomain->getCellResolution(&info.dResolution);
		pDomain->getRealOffset(&info.dCornerSouth, &info.dCornerWest);
		pDomain->getRealDimensions(&info.dWidth, &info.dHeight);

		info.ulCellCount = pDomain->getCellCount();
		info.ulRows = pDomain->getRows();;
		info.ulCols = pDomain->getCols();

		// TODO: Populate these...
		info.ulBoundaryCells = NULL;
		info.ulBoundaryOthers = NULL;
	}
	else {
		info.dResolution = -1.0;
		info.ulCellCount = 0;
		info.dCornerSouth = 0.0;
		info.dCornerWest = 0.0;
		info.ulRows = 0;
		info.ulCols = 0;
		info.dWidth = 0.0;
		info.dHeight = 0.0;
		info.ulBoundaryCells = 0;
		info.ulBoundaryOthers = 0;
	}

	return info;
}


// --- End of DLL functions
#endif

/*
 *  Load the specified model config file and probe for devices etc.
 */
int model::loadConfiguration()
{
	pManager	= new CModel();

	// ---
	//  MPI
	// ---
#ifdef MPI_ON
	if (pManager->getMPIManager() != NULL)
	{
		pManager->getMPIManager()->logDetails();
		pManager->getMPIManager()->exchangeConfiguration( pConfigFile );
	}
#endif

	// ---
	//  CONFIG FILE
	// ---
	if (model::configFile != NULL && pManager->getMPIManager() == NULL)
	{
		boost::filesystem::path pConfigPath(model::configFile);
		boost::filesystem::path pConfigDir = pConfigPath.parent_path();
		pConfigFile = new CXMLDataset( std::string( model::configFile ) );
		chdir(boost::filesystem::canonical(pConfigDir).string().c_str());
	}
	else if (model::configFile == NULL) {
		pConfigFile = new CXMLDataset( "" );
	}

	if ( !pConfigFile->parseAsConfigFile() )
	{
		model::doError(
			"Cannot load model configuration.",
			model::errorCodes::kLevelModelStop
		);
		return model::doClose(
			model::appReturnCodes::kAppInitFailure
		);
	}

	pManager->log->writeLine("The computational engine is now ready.");

	return model::appReturnCodes::kAppSuccess;
}

/*
 *  Read in configuration file and launch a new simulation
 */
int model::commenceSimulation()
{
	if ( !pManager ) 
		return model::doClose(
			model::appReturnCodes::kAppInitFailure	
		);

	// ---
	//  MODEL RUN
	// ---
	if ( !pManager->runModel() )
	{
		model::doError(
			"Simulation start failed.",
			model::errorCodes::kLevelModelStop
		);
		return model::doClose( 
			model::appReturnCodes::kAppFatal 
		);
	}

	// Allow safe deletion 
	pManager->runModelCleanup();

	return model::appReturnCodes::kAppSuccess;
}

/*
 *  Close down the simulation
 */
int model::closeConfiguration()
{
	return model::doClose( 
		model::appReturnCodes::kAppSuccess 
	);
}

/*
 *  Parse command-line arguments
 */
void model::parseArguments( int iArgCount, char* cArgEntities[] )
{
	// Arguments to check for
	unsigned int	argOptionCount = 6;
	modelArgument	argOptions[]   = {
		{	
			"-c",
			"--config-file\0",
			"XML-based configuration file defining the model\0"
		},
		{	
			"-l",
			"--log-file\0",
			"File for model execution log\0"
		},
		{
			"-s",
			"--quiet-mode\0",
			"Disable all requirements for user feedback\0"
		},
		{
			"-n",
			"--disable-screen\0",
			"Disable using a screen for output with NCurses\0"
		},
		{
			"-m",
			"--mpi-mode\0",
			"Adapt output so only one process provides console output\0"
		},
		{
			"-x",
			"--code-dir\0",
			"Directory containing the OpenCL code structure\0"
		}
	};

	// First argument is the invokation command
	for( int i = 1; i < iArgCount; i++ )
	{
		for( unsigned int j = 0; j < argOptionCount; j++ )
		{
			// Check for short (unusually single char) argument option
			if ( std::strcmp( argOptions[ j ].cShort, const_cast<const char*>( cArgEntities[ i ] ) ) == 0 )
			{
				handleArgument( argOptions[ j ].cLong, static_cast<char*>( cArgEntities[ i + 1 ] ) );
				++i;
			}

			// Check for long (followed by equal sign) argument option
			if ( std::strstr( cArgEntities[ i ], const_cast<const char*>( argOptions[ j ].cLong ) ) != NULL )
			{
				std::string sArgument	= cArgEntities[ i ];
				std::string sValue		= "";
				if( strlen( cArgEntities[i] ) > strlen( argOptions[ j ].cLong ) + 1 )
							sValue      = sArgument.substr( strlen( argOptions[ j ].cLong ) + 1, 255 );
				char*		cValue		= new char[ sValue.length() + 1 ];
				cValue = const_cast<char*>( sValue.c_str() );
				handleArgument( argOptions[ j ].cLong, cValue );
			}
		}
	}
}

/*
 *  Handle an argument that's been passed
 */
void model::handleArgument( const char * cLongName, char * cValue )
{
	if ( strcmp( cLongName, "--config-file" ) == 0 )
	{
		configFile = new char[ strlen( cValue ) + 1 ];
		strcpy( configFile, cValue );
	}

	else if ( strcmp( cLongName, "--log-file" ) == 0 )
	{
		logFile = new char[ strlen( cValue ) + 1 ];
		strcpy( logFile, cValue );
	}
	
	else if ( strcmp( cLongName, "--code-dir" ) == 0 )
	{
		codeDir = new char[ strlen( cValue ) + 1 ];
		strcpy( codeDir, cValue );
	}

	else if ( strcmp( cLongName, "--quiet-mode" ) == 0 )
	{
		model::quietMode = true;
	}

	else if (strcmp(cLongName, "--disable-screen") == 0)
	{
		model::disableScreen = true;
	}

	else if (strcmp(cLongName, "--mpi-mode") == 0)
	{
#ifdef MPI_ON
		int iProcCount, iProcID;

		MPI_Comm_rank(MPI_COMM_WORLD, &iProcID);
		MPI_Comm_size(MPI_COMM_WORLD, &iProcCount);

		// Disable outputs for all except the master process
		model::quietMode = (iProcID != 0);
		model::disableScreen = (iProcID != 0);
		model::disableConsole = (iProcID != 0);
#else
		model::quietMode = true;
		model::disableScreen = true;
		model::disableConsole = true;
#endif
	}
}

/*
 *  Model is complete.
 */
int model::doClose( int iCode )
{
	CRasterDataset::cleanupAll();
	delete pConfigFile;
	delete pManager;
	delete [] model::workingDir;
	delete [] model::logFile;			// TODO: Fix me...
	delete [] model::configFile;
	delete [] model::codeDir;
	model::doPause();

	pManager			= NULL;
	pConfigFile			= NULL;
	model::workingDir	= NULL;
	model::logFile		= NULL;
	model::configFile	= NULL;

	return iCode;
}

/*
 *  Suspend the application temporarily pending the user
 *  pressing return to continue.
 */
void model::doPause()
{
#ifndef _WINDLL
	if ( model::quietMode ) return;
#ifndef PLATFORM_UNIX
	std::cout << std::endl << "Press any key to close." << std::endl;
#else
	if ( !model::disableScreen )
	{
		addstr( "Press any key to close.\n" );
		refresh();
	}
	else {
		std::cout << std::endl << "Press any key to close." << std::endl;
	}
#endif
	std::getchar();
#endif
}

/*
 *  Raise an error message and deal with it accordingly.
 */
void model::doError( std::string sError, unsigned char cError )
{
	pManager->log->writeError( sError, cError );
	if ( cError & model::errorCodes::kLevelModelStop )
		model::forceAbort = true;
	if ( cError & model::errorCodes::kLevelFatal )
	{
		model::doPause();
#ifdef _CONSOLE
#ifdef MPI_ON
		MPI_Finalize();
#endif
		exit( model::appReturnCodes::kAppFatal );
#else
		model::fCallbackComplete(
			model::appReturnCodes::kAppFatal
		);
		ExitThread(model::appReturnCodes::kAppFatal);
#endif
		
	}
}

/*
 *  Discovers the full path of the current working directory.
 */
void model::storeWorkingEnv()
{
#ifdef PLATFORM_UNIX

	// THIS IS TEMPORARY ONLY!
	char cPath[ FILENAME_MAX ];

	// Unix version of getcwd
	getcwd( cPath, FILENAME_MAX );

	// Verify that we've managed to obtain a path
	if ( strcmp( cPath, "" ) == 0 )
	{
		// Fallback is temp directory
		strcpy( cPath, "/tmp/" );
	}
	model::workingDir = new char[ FILENAME_MAX ];
	std::strcpy( model::workingDir, cPath );
#endif
#ifdef PLATFORM_WIN

	if ( model::workingDir != NULL )
		return;

	char cPath[ _MAX_PATH ];

	// ISO compliant version of getcwd
	_getcwd( cPath, _MAX_PATH );

	// Verify that we've managed to obtain a path
	if ( strcmp( cPath, "" ) == 0 )
	{
		// Fallback is temp directory
		strcpy_s( cPath, "%TEMP%" );
	}
	model::workingDir = new char[ _MAX_PATH ];
	std::strcpy( model::workingDir, cPath );
#endif
}
