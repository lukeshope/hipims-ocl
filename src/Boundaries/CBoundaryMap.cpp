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
#include <boost/algorithm/string.hpp>

#include "../common.h"
#include "CBoundaryMap.h"
#include "CBoundary.h"
#include "CBoundaryCell.h"
#include "CBoundaryUniform.h"
#include "CBoundaryGridded.h"
#include "CBoundarySimplePipe.h"
#include "../Datasets/CXMLDataset.h"
#include "../Domain/CDomainManager.h"
#include "../Domain/CDomain.h"
#include "../Domain/Cartesian/CDomainCartesian.h"

using std::vector;

/* 
 *  Constructor
 */
CBoundaryMap::CBoundaryMap( CDomain* pDomain )
{
	this->pDomain = pDomain;
}

/*
 *  Destructor
 */
CBoundaryMap::~CBoundaryMap()
{
	for (mapBoundaries_t::iterator it = mapBoundaries.begin(); it != mapBoundaries.end(); it++)
	{
		(it->second)->cleanBoundary();
		delete it->second;
		mapBoundaries.erase(it);
	}
}

/*
 *	Prepare the boundary resources required (buffers, kernels, etc.)
 */
void CBoundaryMap::prepareBoundaries(
		COCLProgram* pProgram,
		COCLBuffer* pBufferBed,
		COCLBuffer* pBufferManning,
		COCLBuffer* pBufferTime,
		COCLBuffer* pBufferTimeHydrological,
		COCLBuffer* pBufferTimestep
	)
{
	for (mapBoundaries_t::iterator it = mapBoundaries.begin(); it != mapBoundaries.end(); it++)
		(it->second)->prepareBoundary( pProgram->getDevice(), pProgram, pBufferBed, pBufferManning, pBufferTime, pBufferTimeHydrological, pBufferTimestep );
}

/*
 *	Apply the buffers (execute the relevant kernels etc.)
 */
void CBoundaryMap::applyBoundaries(COCLBuffer* pCellBuffer)
{
	for (mapBoundaries_t::iterator it = mapBoundaries.begin(); it != mapBoundaries.end(); it++)
		(it->second)->applyBoundary(pCellBuffer);
}

/*
 *	Stream the buffer (i.e. prepare resources for the current time period)
 */
void CBoundaryMap::streamBoundaries(double dTime)
{
	for (mapBoundaries_t::iterator it = mapBoundaries.begin(); it != mapBoundaries.end(); it++)
	{
		(it->second)->streamBoundary(dTime);
	}
}

/*
 *	How many boundaries do we have?
 */
unsigned int CBoundaryMap::getBoundaryCount()
{
	return mapBoundaries.size();
}

/*
 *  Parse everything under the <boundaryConditions> element
 */
bool	CBoundaryMap::setupFromConfig( XMLElement *pConfiguration )
{
	XMLElement					*pBoundariesElement,		// <boundaryConditions>
								*pTimeSeriesElement,		// <timeseries .../>
								*pDomainEdgeElement,		// <domainEdge .../>
								*pStructureElement;			// <structure .../>
	CCSVDataset					*pMapFile = NULL;

	pBoundariesElement = pConfiguration->FirstChildElement("boundaryConditions");
	if (pBoundariesElement == NULL)
	{
		// Boundaries aren't a requirement
		return true;
	}

	char						*cSourceDir, *cMapFile;
	std::string					sSourceDir, sMapFile;

	Util::toNewString(&cSourceDir, pBoundariesElement->Attribute("sourceDir"));
	Util::toNewString(&cMapFile, pBoundariesElement->Attribute("mapFile"));
	sSourceDir	= (cSourceDir == NULL || strcmp(cSourceDir, "") == 0 ? "./" : (std::string(cSourceDir) + "/"));
	sMapFile	= (cMapFile == NULL ? "" : (sSourceDir + std::string(cMapFile)));
	delete cSourceDir, cMapFile;

	// ---
	//  Map file
	// ---
	if (sMapFile.length() > 0)
	{
		pMapFile = new CCSVDataset(sMapFile);
		pMapFile->readFile();
	}

	// ---
	//  Timeseries boundaries
	// ---
	pTimeSeriesElement = pBoundariesElement->FirstChildElement("timeseries");

	while (pTimeSeriesElement != NULL)
	{
		char	*cBoundaryType;
		Util::toLowercase(&cBoundaryType, pTimeSeriesElement->Attribute("type"));

		if (cBoundaryType != NULL)
		{

			CBoundary *pNewBoundary = NULL;

			if (strcmp(cBoundaryType, "cell") == 0)
			{
				pNewBoundary = static_cast<CBoundary*>( new CBoundaryCell( this->pDomain ) );
			}
			else if (strcmp(cBoundaryType, "atmospheric") == 0 || strcmp(cBoundaryType, "uniform") == 0)
			{
				pNewBoundary = static_cast<CBoundary*>( new CBoundaryUniform( this->pDomain ) );
			}
			else if (strcmp(cBoundaryType, "gridded") == 0 || strcmp(cBoundaryType, "spatially-varying") == 0)
			{
				pNewBoundary = static_cast<CBoundary*>(new CBoundaryGridded(this->pDomain));
			} else {
				model::doError(
					"Ignored boundary timeseries of unrecognised type.",
					model::errorCodes::kLevelWarning
				);
			}

			// Configure the new boundary
			if ( pNewBoundary == NULL || !pNewBoundary->setupFromConfig(pTimeSeriesElement, sSourceDir))
			{
				model::doError(
					"Encountered an error loading a boundary definition.",
					model::errorCodes::kLevelWarning
				);
			}
			else {
				if ( pMapFile != NULL )
					pNewBoundary->importMap(pMapFile);
			}

			// Store the new boundary in the unordered map
			if ( pNewBoundary != NULL )
			{
				// TODO: Add unique name boundary name check

				mapBoundaries[ pNewBoundary->getName() ] = pNewBoundary;
				pManager->log->writeLine("Loaded new boundary condition '" + pNewBoundary->getName() + "'.");
			} else {
				model::doError(
					"Encountered a boundary type which is not yet available.",
					model::errorCodes::kLevelWarning
				);
			}

		} else {
			model::doError(
				"Ignored boundary timeseries with no type defined.",
				model::errorCodes::kLevelWarning
			);
		}
		pTimeSeriesElement = pTimeSeriesElement->NextSiblingElement("timeseries");

		delete cBoundaryType;
	}

	// ---
	//  Structure boundaries
	// ---
	pStructureElement = pBoundariesElement->FirstChildElement("structure");

	while (pStructureElement != NULL)
	{
		char	*cBoundaryType;
		Util::toLowercase(&cBoundaryType, pStructureElement->Attribute("type"));

		if (cBoundaryType != NULL)
		{

			CBoundary *pNewBoundary = NULL;

			if (strcmp(cBoundaryType, "simple-pipe") == 0)
			{
				pNewBoundary = static_cast<CBoundary*>(new CBoundarySimplePipe(this->pDomain));
			}
			else 
			{
				model::doError(
					"Ignored boundary structure of unrecognised type.",
					model::errorCodes::kLevelWarning
				);
			}

			// Configure the new boundary
			if (pNewBoundary == NULL || !pNewBoundary->setupFromConfig(pStructureElement, sSourceDir))
			{
				model::doError(
					"Encountered an error loading a structure definition.",
					model::errorCodes::kLevelWarning
				);
			}

			// Store the new structure in the unordered map
			if (pNewBoundary != NULL)
			{
				// TODO: Add unique name boundary name check

				mapBoundaries[pNewBoundary->getName()] = pNewBoundary;
				pManager->log->writeLine("Loaded new structure '" + pNewBoundary->getName() + "'.");
			}
			else {
				model::doError(
					"Encountered a structure type which is not yet available.",
					model::errorCodes::kLevelWarning
				);
			}

		}
		else {
			model::doError(
				"Ignored structure with no type defined.",
				model::errorCodes::kLevelWarning
			);
		}
		pStructureElement = pStructureElement->NextSiblingElement("structure");

		delete cBoundaryType;
	}

	delete pMapFile;

	return true;
}

/*
*  Adjust cell bed elevations as necessary etc. around the boundaries
*/
void	CBoundaryMap::applyDomainModifications()
{
	CDomainCartesian* pDomain = static_cast<CDomainCartesian*>(this->pDomain);

	// Populate the relation data
	// TODO: This needs to be moved into the CBoundary stuff...
	/*
	for (unsigned int i = 0;
		i < this->uiRelationCount;
		i++)
	{
		// TODO: Shouldn't need to cast this to Cartesian
		double dResolution;
		pDomain->getCellResolution(&dResolution);

		// Reflective cells must be surrounded by disabled cells in all directions bar
		// the one they are reflecting, otherwise mass will be distorted.
		//if ( this->pBoundaries[ this->pRelations[ i ].uiTimeseries ]->getType() ==
		//	 model::boundaries::types::kBndyTypeReflective )
		//{
		// Disable the boundary cell itself
		pDomain->setStateValue(
			pDomain->getCellID(this->pRelations[i].fTargetX, this->pRelations[i].fTargetY),
			model::domainValueIndices::kValueMaxFreeSurfaceLevel,
			-9998.0
			);
		//}
	}
	*/

	// Closed/open boundary conditions
	pDomain->imposeBoundaryModification(CDomainCartesian::kEdgeN, this->ucBoundaryTreatment[CDomainCartesian::kEdgeN]);
	pDomain->imposeBoundaryModification(CDomainCartesian::kEdgeE, this->ucBoundaryTreatment[CDomainCartesian::kEdgeE]);
	pDomain->imposeBoundaryModification(CDomainCartesian::kEdgeS, this->ucBoundaryTreatment[CDomainCartesian::kEdgeS]);
	pDomain->imposeBoundaryModification(CDomainCartesian::kEdgeW, this->ucBoundaryTreatment[CDomainCartesian::kEdgeW]);
}