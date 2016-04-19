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
 *  Platform-specific inclusion management
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_PLATFORMS_H_
#define HIPIMS_PLATFORMS_H_

// Determine the OS we're compiling for
#ifdef __GNUC__
	#include "Platforms/unix_platform.h"
#else
	#include "Platforms/windows_platform.h"
#endif

#endif
