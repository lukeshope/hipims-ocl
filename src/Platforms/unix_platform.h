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
 *  Unix-specific code and utility functions
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_PLATFORMS_UNIX_PLATFORM_H_
#define HIPIMS_PLATFORMS_UNIX_PLATFORM_H_

#include "../common.h"
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// Segmentation handler
void segFaultHandler(int sig);

// Platform constant
namespace model {
namespace env {
const std::string	platformCode	= "LINUX";
const std::string	platformName	= "Linux";
}
}

// Forward conditionals
#define PLATFORM_UNIX
#define _CONSOLE

// We wont be using __stdcall in unix environments...
#define __stdcall

// For Epiphany/ARM architectures we need to simplify
#ifdef __ARM_ARCH
#define USE_SIMPLE_ARCH_OPENCL
#endif

// Unix-specific includes
// ...

/*
 *  OS PORTABILITY CONSTANTS
 */
namespace model {
namespace cli {
	const unsigned short		colourTimestamp		= 1;
	const unsigned short		colourError			= 2;
	const unsigned short		colourHeader		= 3;
	const unsigned short		colourMain			= 4;
	const unsigned short		colourInfoBlock		= 5;	
}
}

#endif
