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
 *  Domain boundary handling class
 * ------------------------------------------
 *
 */
#include <vector>
#include <boost/lexical_cast.hpp>

#include "CBoundaryMap.h"
#include "CBoundarySimplePipe.h"
#include "../Domain/Cartesian/CDomainCartesian.h"
#include "../OpenCL/Executors/COCLBuffer.h"
#include "../OpenCL/Executors/COCLKernel.h"
#include "../common.h"

using std::vector;

/* 
 *  Constructor
 */
CBoundarySimplePipe::CBoundarySimplePipe( CDomain* pDomain )
{
	this->pBufferConfiguration = NULL;
	this->pDomain = pDomain;
}

/*
 *  Destructor
 */
CBoundarySimplePipe::~CBoundarySimplePipe()
{
	delete this->pBufferConfiguration;
}

/*
 *	Configure this boundary and load in any related files
 */
bool CBoundarySimplePipe::setupFromConfig(XMLElement* pElement, std::string sBoundarySourceDir)
{
	char *cBoundaryType, *cBoundaryName, *cBoundaryPipeLength;

	Util::toLowercase(&cBoundaryType,		pElement->Attribute("type"));
	Util::toNewString(&cBoundaryName,		pElement->Attribute("name"));
	Util::toLowercase(&cBoundaryPipeLength,	pElement->Attribute("length"));

	this->sName = std::string( cBoundaryName );

	return true;
}

/*
 *	Import cell map data from a CSV file
 */
void CBoundarySimplePipe::importMap(CCSVDataset *pCSV)
{

}

void CBoundarySimplePipe::prepareBoundary(
			COCLDevice* pDevice, 
			COCLProgram* pProgram,
			COCLBuffer* pBufferBed, 
			COCLBuffer* pBufferManning,
			COCLBuffer* pBufferTime,
			COCLBuffer* pBufferTimeHydrological,
			COCLBuffer* pBufferTimestep
	 )
{
	// TODO: Create and copy for buffers
}

// TODO: Only the cell buffer should be passed here...
void CBoundarySimplePipe::applyBoundary(COCLBuffer* pBufferCell)
{
	this->oclKernel->assignArgument( 6, pBufferCell );
	this->oclKernel->scheduleExecution();
}

void CBoundarySimplePipe::streamBoundary(double dTime)
{
	// ...
	// TODO: Should we handle all the memory buffer writing in here?...
}

void CBoundarySimplePipe::cleanBoundary()
{
	// ...
	// TODO: Is this needed? Most stuff is cleaned in the destructor
}

