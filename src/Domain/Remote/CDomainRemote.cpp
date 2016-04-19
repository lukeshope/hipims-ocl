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
 *  Domain Class
 * ------------------------------------------
 *
 */
#include <boost/lexical_cast.hpp>

#include "../../common.h"
#include "CDomainRemote.h"

/*
 *  Constructor
 */
CDomainRemote::CDomainRemote(void)
{
	// Populate summary details to avoid errors
	pSummary.bAuthoritative = false;
	pSummary.dEdgeEast = 0.0;
	pSummary.dEdgeNorth = 0.0;
	pSummary.dEdgeSouth = 0.0;
	pSummary.dEdgeWest = 0.0;
	pSummary.dResolution = 0.0;
	pSummary.ucFloatPrecision = 0;
	pSummary.uiNodeID = 0;
	pSummary.uiDomainID = 0;
	pSummary.uiLocalDeviceID = 0;
	pSummary.ulColCount = 0;
	pSummary.ulRowCount = 0;
}

/*
 *  Destructor
 */
CDomainRemote::~CDomainRemote(void)
{
	// ...
}

/*
 *	Fetch summary information for this domain
 */
CDomainBase::DomainSummary CDomainRemote::getSummary()
{
	return this->pSummary;
}

