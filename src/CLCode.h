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
 *  OpenCL Code Resources
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_OPENCL_RESOURCES_H_
#define HIPIMS_OPENCL_RESOURCES_H_

#include <string.h>
#include <stdio.h>
#include <fstream>

namespace model {
	extern char* codeDir;
	extern char* workingDir;
}

inline std::string getOCLResourceFilename( std::string sID )
{
	const char* cID = sID.c_str();
	std::string sBaseDir = "";
	
	if ( model::codeDir != NULL )
	{
		sBaseDir = std::string( model::codeDir ) + "/";
	} else {
		sBaseDir = std::string( model::workingDir ) + "/";
	}

	if ( strcmp( cID, "CLUniversalHeader_H" ) == 0 )
		return sBaseDir + "OpenCL/Executors/CLUniversalHeader.clh";

	if ( strcmp( cID, "CLFriction_H" ) == 0 )
		return sBaseDir + "Schemes/CLFriction.clh";

	if ( strcmp( cID, "CLSchemeGodunov_H" ) == 0 )
		return sBaseDir + "Schemes/CLSchemeGodunov.clh";

	if ( strcmp( cID, "CLSchemeMUSCLHancock_H" ) == 0 )
		return sBaseDir + "Schemes/CLSchemeMUSCLHancock.clh";

	if ( strcmp( cID, "CLSchemeInertial_H" ) == 0 )
		return sBaseDir + "Schemes/CLSchemeInertial.clh";

	if ( strcmp( cID, "CLSolverHLLC_H" ) == 0 )
		return sBaseDir + "Solvers/CLSolverHLLC.clh";

	if ( strcmp( cID, "CLDynamicTimestep_H" ) == 0 )
		return sBaseDir + "Schemes/CLDynamicTimestep.clh";

	if ( strcmp( cID, "CLDomainCartesian_H" ) == 0 )
		return sBaseDir + "Domain/Cartesian/CLDomainCartesian.clh";

	if ( strcmp( cID, "CLSlopeLimiterMINMOD_H" ) == 0 )
		return sBaseDir + "Schemes/Limiters/CLSlopeLimiterMINMOD.clh";

	if ( strcmp( cID, "CLBoundaries_H" ) == 0 )
		return sBaseDir + "Boundaries/CLBoundaries.clh";

	if ( strcmp( cID, "CLVerifyDataStructure_C" ) == 0 )
		return sBaseDir + "OpenCL/Executors/CLVerifyDataStructure.clc";

	if ( strcmp( cID, "CLFriction_C" ) == 0 )
		return sBaseDir + "Schemes/CLFriction.clc";

	if ( strcmp( cID, "CLSchemeGodunov_C" ) == 0 )
		return sBaseDir + "Schemes/CLSchemeGodunov.clc";

	if ( strcmp( cID, "CLSchemeMUSCLHancock_C" ) == 0 )
		return sBaseDir + "Schemes/CLSchemeMUSCLHancock.clc";

	if ( strcmp( cID, "CLSchemeInertial_C" ) == 0 )
		return sBaseDir + "Schemes/CLSchemeInertial.clc";

	if ( strcmp( cID, "CLSolverHLLC_C" ) == 0 )
		return sBaseDir + "Solvers/CLSolverHLLC.clc";

	if ( strcmp( cID, "CLDynamicTimestep_C" ) == 0 )
		return sBaseDir + "Schemes/CLDynamicTimestep.clc";

	if ( strcmp( cID, "CLDomainCartesian_C" ) == 0 )
		return sBaseDir + "Domain/Cartesian/CLDomainCartesian.clc";

	if ( strcmp( cID, "CLSlopeLimiterMINMOD_C" ) == 0 )
		return sBaseDir + "Schemes/Limiters/CLSlopeLimiterMINMOD.clc";

	if ( strcmp( cID, "CLBoundaries_C" ) == 0 )
		return sBaseDir + "Boundaries/CLBoundaries.clc";
	
	return "";
}

#endif
