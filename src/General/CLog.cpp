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
 *  Command line and file-based log outputs
 * ------------------------------------------
 *
 */

// Includes
#include "../common.h"
#include "../main.h"
#include <sstream>
#include <boost/algorithm/string_regex.hpp>
#include <boost/lexical_cast.hpp>

/*
 *  Constructor
 */
CLog::CLog(void)
{
	this->setDir();
	this->setPath();
	this->openFile();
	this->writeHeader();
	this->uiDebugFileID = 1;
	this->uiLineCount = 0;
	setlocale( LC_ALL, "" );

#ifdef MPI_ON
	MPI_Comm_rank(MPI_COMM_WORLD, &this->iProcID);
	MPI_Comm_size(MPI_COMM_WORLD, &this->iProcCount);
#endif

	this->writeLine( "Log component fully loaded." );
}

/*
 *  Destructor
 */
CLog::~CLog(void)
{
	this->closeFile();
	delete[] this->logDir;
	delete[] this->logPath;
}

/*
 *  Open the file for write access
 */
void CLog::openFile()
{
	if ( this->isFileAvailable() )
		return;

	try 
	{
		this->logStream.open( this->logPath );
	} 
	catch ( char * cError )
	{
		this->writeError( std::string( cError ), model::errorCodes::kLevelWarning );
	}
}

/*
 *  Is the log available for write
 */
bool CLog::isFileAvailable()
{
	// Nothing fancy, just abstracted for portability
	return this->logStream.is_open();
}

/*
 *  Close the file safelty again
 */
void CLog::closeFile()
{
	if ( !this->isFileAvailable() ) 
		return;

	this->logStream.flush();
	this->logStream.close();
}

/*
 *  Erase the contents of the log file (to start again)
 */
void CLog::clearFile()
{
	// Erase the file
	// ...
}

/*
 *  OVERLOAD FUNCTIONS
 *  Write a single line to the log with no 2nd parameter
 */
// Include timestamp by default
void CLog::writeLine( std::string sLine ) { this->writeLine( sLine, true ); }
void CLog::writeLine( std::string sLine, bool bTimestamp ) { this->writeLine( sLine, bTimestamp, model::cli::colourMain ); }

/*
 *  Write a single line to the log
 */
void CLog::writeLine(std::string sLine, bool bTimestamp, unsigned short wColour)
{
	time_t tNow;
	time(&tNow);

	char caTimeBuffer[50];
	strftime(caTimeBuffer, sizeof(caTimeBuffer), "%H:%M:%S", localtime(&tNow));

	std::stringstream ssLine;

	if (bTimestamp)
	{
		this->setColour(model::cli::colourTimestamp);
#ifdef MPI_ON
		ssLine << "[" << (this->iProcID + 1) << "/" << this->iProcCount << " " << std::string(caTimeBuffer) << "] ";
#else
		ssLine << "[" << std::string(caTimeBuffer) << "] ";
#endif
#ifdef _CONSOLE
#ifdef PLATFORM_UNIX
		if ( !model::disableScreen )
		{
			addstr( ssLine.str().c_str() );
		} else if ( !model::disableConsole ) {
			std::cout << ssLine.str();
		}
#else
		std::cout << ssLine.str();
#endif
#else
		std::string	sTimestamp = ssLine.str();
		char*		cAPIStringTimestamp = const_cast<char*>(sTimestamp.c_str());
		model::fCallbackLog(
			cAPIStringTimestamp,
			wColour
			);
#endif
	}

	ssLine << sLine << std::endl;

	this->resetColour();
	this->setColour( wColour );

#ifdef _CONSOLE
#ifdef PLATFORM_UNIX
	if ( !model::disableScreen )
	{
		addstr( sLine.c_str() );
		addstr( "\n" );
		clrtoeol();
		refresh();
	} else if ( !model::disableConsole ) {
		std::cout << sLine << std::endl;
	}
#else
	std::cout << sLine << std::endl;
#endif
	this->uiLineCount = ++this->uiLineCount % 1000;
#else
	sLine.append("\n");
	char*		cAPIStringMessage = const_cast<char*>( sLine.c_str() );
	model::fCallbackLog(
		cAPIStringMessage,
		wColour
	);
#endif
	sLine = ssLine.str();

	if ( this->isFileAvailable() )
	{
		this->logStream << sLine;
	}

	this->resetColour();
}

/*
 *  Write details of an error that's occured 
 *  Actually handling the error is conducted in the main
 *  sub-procedure.
 */
void CLog::writeError( std::string sError, unsigned char cError )
{
	std::string sErrorPrefix = "UNKNOWN";

	if ( cError & model::errorCodes::kLevelFatal ) { 
		sErrorPrefix = "FATAL ERROR"; 
	} else if ( cError & model::errorCodes::kLevelModelStop	) { 
		sErrorPrefix = "MODEL FAILURE"; 
	} else if ( cError & model::errorCodes::kLevelModelContinue	) { 
		sErrorPrefix = "MODEL WARNING"; 
	} else if ( cError & model::errorCodes::kLevelWarning ) { 
		sErrorPrefix = "WARNING"; 
	} else if ( cError & model::errorCodes::kLevelInformation ) { 
		sErrorPrefix = "INFO"; 
	}

	this->writeLine( "---------------------------------------------", false, model::cli::colourError );
	this->writeLine( sErrorPrefix + ": " + sError, true, model::cli::colourError );
	this->writeLine( "---------------------------------------------", false, model::cli::colourError );
}

/*
 *  Write the header output
 */
void CLog::writeHeader(void)
{
	std::stringstream		ssHeader;

	boost::cmatch		regmatchRevision;
	const boost::regex	regexRevision( "\\$Revision\\:\\ ([0-9]+)\\ \\$" );

	time_t tNow;
	time( &tNow );
	localtime( &tNow );

	boost::regex_match( model::appRevision.c_str(), regmatchRevision, regexRevision );

	ssHeader << "---------------------------------------------" << std::endl;
	ssHeader << " " << model::appName << std::endl;
	ssHeader << " v" << model::appVersionMajor << "." << model::appVersionMinor << "." << model::appVersionRevision;
	ssHeader << std::endl << "---------------------------------------------" << std::endl;
	ssHeader << " " << model::appAuthor << std::endl;
	ssHeader << " " << model::appUnit << std::endl;
	ssHeader << " " << model::appOrganisation << std::endl;
	ssHeader << std::endl << " Contact:     " << model::appContact << std::endl;
	ssHeader << " Source:      SVN Revision " << std::string( regmatchRevision[1] ) << std::endl;
	ssHeader << "---------------------------------------------" << std::endl;

	std::string sLogPath = this->getPath();

	if ( sLogPath.length() > 25 ) 
		sLogPath = "..." + sLogPath.substr( sLogPath.length() - 25, 25 );

	ssHeader << " Started:     " << ctime( &tNow );
	ssHeader << " Log file:    " << sLogPath << std::endl;
	ssHeader << " Platform:    " << model::env::platformName << std::endl;
	ssHeader << "---------------------------------------------";

	this->writeLine( ssHeader.str(), false, model::cli::colourHeader );
	ssHeader.clear();
}

/*
 *  Set the log path to the default
 */
void CLog::setPath()
{
	std::string sPath = std::string( model::workingDir ) + "/" + std::string( model::logFile );
	char*		cPath = new char[ sPath.length() + 1 ];

	std::strcpy( cPath, sPath.c_str() );
	this->setPath( cPath, sPath.length() );

	delete[] cPath;
}

/*
 *  Set the log path to the given value
 */
void CLog::setPath( char* sPath, size_t uiLength )
{
	this->logPath = new char[ uiLength + 1 ];
	std::strcpy( this->logPath, sPath );

	// Is it already open? Swap if so
	// TODO: Implement this
	// ...
}

/*
 *  Return the path back
 */
std::string CLog::getPath()
{
	return this->logPath;
}

/*
 *  Set the log directory to the default
 */
void CLog::setDir()
{
	std::string sDir = std::string( model::workingDir ) + "/";

	char*		cDir = new char[ sDir.length() + 1 ];
	std::strcpy( cDir, sDir.c_str() );

	this->setDir( cDir, sDir.length() );
	delete[] cDir;
}

/*
 *  Set the log directory to the given value
 */
void CLog::setDir( char* sDir, size_t stLength )
{
	this->logDir = new char[ stLength + 1 ];
	std::strcpy( this->logDir, sDir );
}

/*
 *  Return the directory back
 */
std::string CLog::getDir()
{
	return this->logDir;
}

/*
 *  Write a line to divide up the output, purely superficial
 */
void CLog::writeDivide()
{
	this->writeLine( "---------------------------------------------                           ", false );
}

/*
 *  Set colour
 */
void CLog::setColour( unsigned short wColour )
{
	// This only runs for windows!
	#ifdef PLATFORM_WIN
	HANDLE	hOut		= GetStdHandle( STD_OUTPUT_HANDLE );

	SetConsoleTextAttribute(		// Future text
		hOut, 
		wColour
	);
	#endif
	#ifdef PLATFORM_UNIX
	if ( !model::disableScreen )
	{
		if ( !has_colors() || !can_change_color() )
			return;

		attron(COLOR_PAIR( (int)wColour ));
	}
	#endif
}

/*
 *  Reset colour
 */
void CLog::resetColour()
{
	// This only runs for windows!
	#ifdef PLATFORM_WIN
	HANDLE	hOut		= GetStdHandle( STD_OUTPUT_HANDLE );

	SetConsoleTextAttribute(		// Future text
		hOut, 
		FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN
	);
	#endif
	#ifdef PLATFORM_UNIX
	if ( !model::disableScreen )
		this->setColour( model::cli::colourMain );
	#endif
}

/*
 *  Write a debug file to the log directory
 */
void CLog::writeDebugFile( char** cContents, unsigned int uiSegmentCount )
{
	std::ofstream ofsDebug;
	std::string sFilePath = this->getDir() + "_debug" + toString( this->uiDebugFileID ) + ".log";

	try 
	{
		ofsDebug.open( sFilePath.c_str() );
	} 
	catch ( char * cError )
	{
		this->writeError( std::string( cError ), model::errorCodes::kLevelWarning );
		return;
	}

	for( unsigned int i = 0; i < uiSegmentCount; ++i )
		ofsDebug << cContents[ i ];

	ofsDebug.close();

	++this->uiDebugFileID;
}




