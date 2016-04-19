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
 *  Domain link class
 * ------------------------------------------
 *
 */
#include <boost/lexical_cast.hpp>
#include <math.h>
#include <cmath>
#include <stdlib.h>

#include "../../common.h"
#include "../../MPI/CMPIManager.h"
#include "CDomainLink.h"
#include "../Cartesian/CDomainCartesian.h"	// TEMP: Remove me!
#include "../CDomainManager.h"				// TEMP: Remove me!

using std::min;
using std::max;

/*
 *  Constructor
 */
CDomainLink::CDomainLink( CDomainBase* pTarget, CDomainBase *pSource )
{
	this->uiTargetDomainID = pTarget->getID();
	this->uiSourceDomainID = pSource->getID();

#ifdef MPI_ON
	this->iTargetNodeID = pTarget->getSummary().uiNodeID;
#else
	this->iTargetNodeID = -1;
#endif
	
	// Start as invalid
	this->dValidityTime = -1.0;
	this->bSent 		= true;
	this->uiSmallestOverlap = 999999999;

	pManager->log->writeLine("Generating link definitions between domains #" + toString(this->uiTargetDomainID + 1) 
		+ " and #" + toString(this->uiSourceDomainID + 1));

	this->generateDefinitions( pTarget, pSource );
}

/*
 *  Destructor
 */
CDomainLink::~CDomainLink(void)
{
	for (unsigned int i = 0; i < linkDefs.size(); i++)
	{
		delete[] linkDefs[i].vStateData;
	}
}

/*
 *	Return whether or not it is possible for two domains to be linked
 *	together in a CDomainLink class.
 */
bool CDomainLink::canLink(CDomainBase* pA, CDomainBase* pB)
{
	CDomainBase::DomainSummary pSumA = pA->getSummary();
	CDomainBase::DomainSummary pSumB = pB->getSummary();

	// Cannot link two remote domains (as there's no need)
	if ( !pSumA.bAuthoritative && !pSumB.bAuthoritative )
		return false;

	// Do the two domains overlap at all?
	// N/S axis
	if ( ( pSumA.dEdgeNorth >= pSumB.dEdgeNorth && pSumA.dEdgeSouth >= pSumB.dEdgeNorth ) ||
		 ( pSumA.dEdgeNorth <= pSumB.dEdgeSouth && pSumA.dEdgeSouth <= pSumB.dEdgeSouth ) )
		return false;

	// E/W axis
	if ( pSumA.dEdgeWest >= pSumB.dEdgeEast &&
		 pSumA.dEdgeEast <= pSumB.dEdgeWest )
		return false;

	// Are the two domains identical?
	// NOTE: This would just be daft...
	if ( pSumA.dEdgeEast  == pSumB.dEdgeEast &&
		 pSumA.dEdgeWest  == pSumB.dEdgeWest &&
		 pSumA.dEdgeNorth == pSumB.dEdgeNorth &&
		 pSumA.dEdgeSouth == pSumB.dEdgeSouth )
		return false;

	// Are the two resolutions the same?
	// TODO: Add support for mixed-resolution syncing at a later date
	if ( pSumA.dResolution != pSumB.dResolution )
		return false;

	// Are the two domains aligned (or at least roughly aligned)...
	// Limit the misalignment to 1/10 of the resolution, but even this would cause problems
	// for mass conservation...
	if (fmod(fabs(pSumA.dEdgeSouth - pSumB.dEdgeSouth), pSumA.dResolution) > 0.1 * pSumA.dResolution)
		return false;
	if (fmod(fabs(pSumA.dEdgeEast - pSumB.dEdgeWest), pSumA.dResolution) > 0.1 * pSumA.dResolution)
		return false;

	return true;
}

/*
 *	Import data received through MPI
 */
void	CDomainLink::pullFromMPI(double dCurrentTime, char* pData)
{
#ifdef DEBUG_MPI
	pManager->log->writeLine("[DEBUG] Importing link data via MPI... Data time: " + Util::secondsToTime(dCurrentTime) + ", Current time: " + Util::secondsToTime(this->dValidityTime));
#endif

	if ( this->dValidityTime >= dCurrentTime )
		return;

	unsigned int uiOffset = 0;
		
	for( unsigned int i = 0; i < this->linkDefs.size(); i++ )
	{
		memcpy(
			this->linkDefs[i].vStateData,
			&pData[ uiOffset ],
			this->linkDefs[i].ulSize
		);
		uiOffset += this->linkDefs[i].ulSize;
	}
		
	this->dValidityTime = dCurrentTime;
}

/*
 *	Download data for the link from a memory buffer
 */
void	CDomainLink::pullFromBuffer(double dCurrentTime, COCLBuffer* pBuffer)
{
	if ( this->dValidityTime >= dCurrentTime &&
		 this->bSent )
		return;

	if ( this->dValidityTime < dCurrentTime )
	{
	
		for (unsigned int i = 0; i < this->linkDefs.size(); i++)
		{
#ifdef DEBUG_MPI
				pManager->log->writeLine("[DEBUG] Should now be downloading data from buffer at time " + Util::secondsToTime(dCurrentTime));
#endif
				pBuffer->queueReadPartial(
					this->linkDefs[i].ulOffsetSource,
					this->linkDefs[i].ulSize,
					this->linkDefs[i].vStateData
				);
		}
		
		this->dValidityTime = dCurrentTime;
		this->bSent = false;
		
	} else {
#ifdef DEBUG_MPI
		pManager->log->writeLine("[DEBUG] Not downloading data at " + Util::secondsToTime(dCurrentTime) + " as validity time is " + Util::secondsToTime(dValidityTime));
#endif
	}
}

/*
 *	Send the data over MPI if it hasn't already been sent...
 */
bool	CDomainLink::sendOverMPI()
{
	if ( this->bSent )
		return true;

#ifdef MPI_ON
	if ( this->iTargetNodeID >= 0 )
	{
		mpiSignalDataDomain pHeader;
		
		pHeader.iSignalCode = model::mpiSignalCodes::kSignalDomainData;
		pHeader.uiTargetDomainID = this->uiTargetDomainID;
		pHeader.uiSourceDomainID = this->uiSourceDomainID;
		pHeader.dValidityTime = this->dValidityTime;
		pHeader.uiDataSize = 0;
		
		for( unsigned int i = 0; i < this->linkDefs.size(); i++ )
		{
			pHeader.uiDataSize += this->linkDefs[i].ulSize;
		}
		
		unsigned int uiSize = sizeof( mpiSignalDataDomain ) + pHeader.uiDataSize;
		unsigned int uiOffset = sizeof( mpiSignalDataDomain ); 
	
		char* cSendBuffer = new char[ uiSize ];
	
		memcpy( &cSendBuffer[ 0 ], &pHeader, sizeof( mpiSignalDataDomain ) );
		
		for( unsigned int i = 0; i < this->linkDefs.size(); i++ )
		{
			memcpy(
				&cSendBuffer[ uiOffset ],
				this->linkDefs[i].vStateData,
				this->linkDefs[i].ulSize
			);
			uiOffset += this->linkDefs[i].ulSize;
		}
	
		pManager->getMPIManager()->sendDataDomainLink( this->iTargetNodeID, cSendBuffer, uiSize );
	}
#endif
	
	this->bSent = true;
	
	return false;
}

/*
 *	Push data to a memory buffer
 */
void	CDomainLink::pushToBuffer(COCLBuffer* pBuffer)
{
	// Once there is a check in the sync ready code for the current timestep of each link
	// this should become redundant...
	// TODO: Remove this later...
	if (this->dValidityTime < 0.0) return;

	for (unsigned int i = 0; i < this->linkDefs.size(); i++)
	{
#ifdef DEBUG_MPI
		pManager->log->writeLine("[DEBUG] Should now be pushing data to buffer at time " + Util::secondsToTime(dValidityTime) + " (" + toString(this->linkDefs[i].ulSize) + " bytes)");
#endif
		pBuffer->queueWritePartial(
			this->linkDefs[i].ulOffsetTarget,
			this->linkDefs[i].ulSize,
			this->linkDefs[i].vStateData
		);
	}
}

/*
 *	Is this link at the specified time yet?
 */
bool	CDomainLink::isAtTime( double dCheckTime )
{
	if ( fabs( this->dValidityTime - dCheckTime ) > 1E-5 )
		return false;
		
	return true;
}

/*
 *	Identify all the areas of contiguous memory which overlap so we know what needs exchanging
 */
void	CDomainLink::generateDefinitions(CDomainBase* pTarget, CDomainBase *pSource)
{
	CDomainBase::DomainSummary pSumTgt = pTarget->getSummary();
	CDomainBase::DomainSummary pSumSrc = pSource->getSummary();

	// Get the size of our cell state vector
	unsigned char ucStateVectorSize = (pSumTgt.ucFloatPrecision == model::floatPrecision::kSingle ?
		sizeof(cl_float4) : sizeof(cl_double4)
	);

	// Fetch the high and low for the overlap
	double dSyncZoneSouth = max( pSumTgt.dEdgeSouth, pSumSrc.dEdgeSouth );
	double dSyncZoneNorth = min( pSumTgt.dEdgeNorth, pSumSrc.dEdgeNorth );

	// Calculate how large in rows the overlap is
	unsigned int uiOverlapRowCount = static_cast<unsigned int>(
		floor( ( dSyncZoneNorth - dSyncZoneSouth )/2.0 / pSumTgt.dResolution ) - 1.0 
	);
	unsigned int uiOverlapRowOffset = static_cast<unsigned int>(
		ceil( (dSyncZoneNorth - dSyncZoneSouth) / pSumTgt.dResolution)
	);

	// Smallest overlap is the size of our overlap, which is used to constrain
	// how many iterations can be run before sync.
	uiSmallestOverlap = uiOverlapRowCount;

	// Calculate the start and end rows in the source and targets
	unsigned long ulRowBaseTgt, ulRowHighTgt;
	unsigned long ulRowBaseSrc, ulRowHighSrc;
	if ( dSyncZoneSouth == pSumSrc.dEdgeSouth )
	{
		// The source domain is above this domain
		ulRowHighTgt = pSumTgt.ulRowCount - 1;
		ulRowBaseTgt = ulRowHighTgt - uiOverlapRowCount + 1;
		ulRowBaseSrc = uiOverlapRowOffset - uiOverlapRowCount;
		ulRowHighSrc = ulRowBaseSrc + uiOverlapRowCount - 1;
	} else {
		// The source domain is below this one
		ulRowBaseTgt = 0;
		ulRowHighTgt = uiOverlapRowCount - 1;
		ulRowHighSrc = pSumSrc.ulRowCount - 1 - uiOverlapRowOffset + uiOverlapRowCount;
		ulRowBaseSrc = ulRowHighSrc - uiOverlapRowCount + 1;
	}

	// Do not proceed if there is no overlap identified that we can work with
	if (ulRowHighTgt - ulRowBaseTgt < 1)
		return;

	// Calculate the low and high columns to use in sync
	// TODO: For now assuming perfect overlaps...

	// Create definitions; preferably contiguous...
	LinkDefinition pDefinition;
	pDefinition.ulSize = 0;

	for (unsigned int i = 0; i <= ulRowHighTgt - ulRowBaseTgt; i++)
	{
		// Can we extend the last?
		if ( pDefinition.ulSize > 0 && pTarget->getCellID(0, ulRowBaseTgt + i) == pDefinition.ulTargetEndCellID + 1 )
		{
			pDefinition.ulSourceEndCellID = pSource->getCellID(pSumSrc.ulColCount - 1, ulRowBaseSrc + i);
			pDefinition.ulTargetEndCellID = pTarget->getCellID(pSumTgt.ulColCount - 1, ulRowBaseTgt + i);
			
		} else {
			// Need a new definition... deal with the last one
			if (pDefinition.ulSize > 0)
				linkDefs.push_back(pDefinition);

			pDefinition.ulSourceStartCellID  = pSource->getCellID( 0, ulRowBaseSrc + i );
			pDefinition.ulSourceEndCellID	 = pSource->getCellID( pSumSrc.ulColCount - 1, ulRowBaseSrc + i );
			pDefinition.ulTargetStartCellID  = pTarget->getCellID( 0, ulRowBaseTgt + i );
			pDefinition.ulTargetEndCellID	 = pTarget->getCellID( pSumTgt.ulColCount - 1, ulRowBaseTgt + i );

			pDefinition.vStateData = NULL;
		}

		pDefinition.ulSize = (pDefinition.ulSourceEndCellID - pDefinition.ulSourceStartCellID + 1) * ucStateVectorSize;
	}

	// Store and allocate
	if ( pDefinition.ulSize > 0 )
		linkDefs.push_back( pDefinition );

	for (unsigned int i = 0; i < linkDefs.size(); i++)
	{
		if ( pSumTgt.ucFloatPrecision == model::floatPrecision::kSingle )
		{
			linkDefs[i].vStateData  = new cl_float4[ linkDefs[i].ulSourceEndCellID - linkDefs[i].ulSourceStartCellID + 1 ];
			linkDefs[i].ulOffsetSource = linkDefs[i].ulSourceStartCellID * sizeof(cl_float4);
			linkDefs[i].ulOffsetTarget = linkDefs[i].ulTargetStartCellID * sizeof(cl_float4);
		} else {
			linkDefs[i].vStateData = new cl_double4[linkDefs[i].ulSourceEndCellID - linkDefs[i].ulSourceStartCellID + 1 ];
			linkDefs[i].ulOffsetSource = linkDefs[i].ulSourceStartCellID * sizeof(cl_double4);
			linkDefs[i].ulOffsetTarget = linkDefs[i].ulTargetStartCellID * sizeof(cl_double4);
		}
	}
}
