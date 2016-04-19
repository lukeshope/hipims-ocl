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
 *  Utility functions for Unix only, etc.
 * ------------------------------------------
 *
 */
#include "../common.h"
#include "../main.h"

#ifdef PLATFORM_UNIX

/*
 * Generate and print a stack trace on Unix systems 
 * if a seg fault occurs.
 */
void segFaultHandler(int sig) {
	fprintf(stderr, "Seg fault... Bugger.\n");
	
	/*
	void *array[10];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	exit(1);
	*/
}

/*
 *  Fetch an embedded resource
 */
char*	Util::getFileResource( const char * sName, const char * sType )
{
	std::string sFilename = getOCLResourceFilename( sName );

	if ( sFilename.length() <= 0 )
	{

		model::doError(
			"Requested an invalid resource.",
			model::errorCodes::kLevelWarning
		);
		return "";

	}

	try {

		std::ifstream pResourceFile( sFilename );
		std::stringstream ssResource;
		ssResource << pResourceFile.rdbuf();
		std::string sResource = ssResource.str();

		char*	cResource	= new char[ sResource.length() + 1 ];
		memcpy( cResource, sResource.c_str(), sResource.length() );
		cResource[ sResource.length() ] = 0;

		return cResource;

	} catch ( std::exception )
	{
		model::doError(
			"Error loading a resource.",
			model::errorCodes::kLevelWarning
		);
		return "";
	}
}

/*
 *  Identify the cursor location in the console, mostly so we can remove it
 *  and overwrite later.
 */
cursorCoords	Util::getCursorPosition()
{
	cursorCoords sPos;

	if ( !model::disableScreen )
	{
		int y,x;

		getyx(stdscr, y, x);

		unsigned int uiLineCount = pManager->log->getLineCount();

		sPos.sX = (int)x;
		sPos.sY = (int)uiLineCount;

		return sPos;
	}

	sPos.sX = 0;
	sPos.sY = 0;
	return sPos;
}

/*
 *  Identify the cursor location in the console, mostly so we can remove it
 *  and overwrite later.
 */
void	Util::setCursorPosition( cursorCoords pLocation )
{
	if ( !model::disableScreen )
	{
		unsigned int uiLineCount = pManager->log->getLineCount();
		int iDiff = (int)uiLineCount - pLocation.sY;
		if ( iDiff < 0 ) iDiff += 1000;

		int y,x;
		getyx(stdscr, y, x);
		wmove(stdscr, y - iDiff, (int)pLocation.sX);
	}
}

/*
 *  Get the system hostname
 */
void Util::getHostname( char* cHostname )
{
	gethostname( cHostname, 255 );
}

#endif