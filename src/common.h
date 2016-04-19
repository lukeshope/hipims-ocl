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
 *  Common header file
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_COMMON_H_
#define HIPIMS_COMMON_H_

// Base includes
#include "util.h"
#include "platforms.h"

//#define DEBUG_MPI 1

#define toString(s) boost::lexical_cast<std::string>(s)

// Windows-specific includes
#ifdef PLATFORM_WIN
#include <tchar.h>
#include <direct.h>
#endif
#ifdef PLATFORM_UNIX
#include <unistd.h>
#include <time.h>
#include <ncurses.h>
#include "CLCode.h"
#endif
#ifdef MPI_ON
#include <mpi.h>
#endif

#include "Datasets/TinyXML/tinyxml2.h"
#include "General/CLog.h"
#include "CModel.h"

using tinyxml2::XMLElement;

// Basic functions and variables used throughout
namespace model
{
// Application return codes
namespace appReturnCodes{ enum appReturnCodes {
	kAppSuccess							= 0,	// Success
	kAppInitFailure						= 1,	// Initialisation failure
	kAppFatal							= 2		// Fatal error
}; }

// Error type codes
namespace errorCodes { enum errorCodes {
	kLevelFatal							= 1,	// Fatal error, no continuation
	kLevelModelStop						= 2,	// Error that requires the model to abort
	kLevelModelContinue					= 4,	// Error for which the model can continue
	kLevelWarning						= 8,	// Display a warning message
	kLevelInformation					= 16	// Just provide some information
}; }

// Floating point precision
namespace floatPrecision{
	enum floatPrecision {
		kSingle = 0,	// Single-precision
		kDouble = 1		// Double-precision
	};
}

extern	CModel*			pManager;
extern  char*			configFile;
extern  char*			codeDir;
void					doError( std::string, unsigned char );
}

// Variables in use throughput
using	model::pManager;

#endif
