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
 *  MPI manager
 * ------------------------------------------
 *
 */
#ifdef MPI_ON

// Includes
#include "../common.h"
#include "../Domain/CDomainManager.h"
#include "../Domain/CDomainBase.h"
#include "../Domain/Links/CDomainLink.h"
#include "../Domain/Remote/CDomainRemote.h"
#include "../Datasets/CXMLDataset.h"
#include "../OpenCL/Executors/CExecutorControlOpenCL.h"
#include "../OpenCL/Executors/COCLDevice.h"
#include "CMPIManager.h"
#include "CMPINode.h"
#include <boost/lexical_cast.hpp>

/*
 *  Constructor
 */
CMPIManager::CMPIManager()
{
	// Store the key details
	MPI_Comm_rank(MPI_COMM_WORLD, &this->iNodeID);
	MPI_Comm_size(MPI_COMM_WORLD, &this->iNodeCount);

	// Initialise array for storing each node's details
	// so we're aware of what's going on
	this->pNodes = new CMPINode*[this->iNodeCount];
	
	this->pRecvReq = NULL;
	this->pRecvBuffer = NULL;
	this->uiRecvBufferSize = 0;
	
	this->bCollectiveThreadRun = true;
	this->bCollective_Hold 		= false;
	this->bCollective_Barrier 	= false;
	this->bCollective_Reduction = false;
	
	this->dCollective_ReductionInput = 0.0;
	this->dCollective_ReductionLastTime = -1.0;

#ifdef PLATFORM_WIN
	HANDLE hThread = CreateThread(
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)CMPIManager::Threaded_processCollective_Launch,
		this,
		0,
		NULL
		);
	CloseHandle(hThread);
#endif
#ifdef PLATFORM_UNIX
	pthread_t tid;
	int result = pthread_create(&tid, 0, CMPIManager::Threaded_processCollective_Launch, this);
	if (result == 0)
		pthread_detach(tid);
#endif
}

/*
 *  Destructor
 */
CMPIManager::~CMPIManager()
{
	this->bCollectiveThreadRun = false;
	delete [] this->pNodes;
	delete [] this->pRecvBuffer;
}

/*
 *  Spit out some details
 */
void CMPIManager::logDetails()
{
	pManager->log->writeDivide();
	unsigned short	wColour = model::cli::colourInfoBlock;
	pManager->log->writeLine("MESSAGE PASSING INTERFACE", true, wColour);
	pManager->log->writeLine("  Status:           " + ( this->iNodeCount > 1 ? std::string("Enabled") : std::string("Disabled") ), true, wColour);
	pManager->log->writeLine("  Node ID:          " + toString(this->iNodeID + 1) + " of " + toString(this->iNodeCount), true, wColour);
	pManager->log->writeDivide();
}


/*
*  Spit out some extra details
*/
void CMPIManager::logCommDetails()
{
	unsigned int uiDeviceNo = 0;

	pManager->log->writeDivide();
	unsigned short	wColour = model::cli::colourInfoBlock;

	pManager->log->writeLine("NETWORK-BASED SYNCH: DEVICES IDENTIFIED", true, wColour);
	pManager->log->writeLine("  Device count:     " + toString(this->getDeviceCount()), true, wColour);
	pManager->log->writeDivide();
	for( int i = 0; i < iNodeCount; ++i )
	{
		pManager->log->writeLine( "  Node " + toString( i + 1 ) + ": " + this->pNodes[i]->getHostname() +
								  " [" + toString( this->pNodes[i]->getDeviceCount() ) + " device(s)]" );
		for( unsigned int j = 0; j < this->pNodes[i]->getDeviceCount(); j++ )
		{
			COCLDevice::sDeviceSummary pSummary = this->pNodes[i]->getDeviceInfo(j);
			pManager->log->writeLine( "       " + toString( i + 1 ) + "." + 
												   toString( j + 1 ) + "  " +
												   std::string(pSummary.cDeviceName) + " (#" +
												   toString( ++uiDeviceNo ) + " " +
												   std::string(pSummary.cDeviceType) + ")" );
		}
	}
	pManager->log->writeDivide();
}

/*
 *  Get the total number of devices
 */
unsigned int CMPIManager::getDeviceCount()
{
	unsigned int uiDeviceCount = 0;
	for( int i = 0; i < iNodeCount; ++i )
		uiDeviceCount += this->pNodes[i]->getDeviceCount();
	return uiDeviceCount;
}

/*
*  Wrapper for error handling
*/
void CMPIManager::wrapError( int iErrorID )
{
	if ( iErrorID == MPI_SUCCESS ) 
		return;

	if ( iErrorID == MPI_ERR_COMM ) 
		model::doError(
				"MPI error: Invalid communicator",
				model::errorCodes::kLevelWarning
		);

	if ( iErrorID == MPI_ERR_COUNT ) 
		model::doError(
			"MPI error: Invalid count argument",
			model::errorCodes::kLevelWarning
		);

	if ( iErrorID == MPI_ERR_TYPE ) 
		model::doError(
			"MPI error: Invalid data type",
			model::errorCodes::kLevelWarning
		);

	if ( iErrorID == MPI_ERR_BUFFER ) 
		model::doError(
			"MPI error: Invalid buffer pointer",
			model::errorCodes::kLevelWarning
		);

	if ( iErrorID == MPI_ERR_ROOT ) 
		model::doError(
			"MPI error: Invalid root",
			model::errorCodes::kLevelWarning
		);
}

/*
*  Swap configuration files between the master and slaves
*/
bool CMPIManager::exchangeConfiguration( CXMLDataset *& pDataset )
{
	unsigned int uiConfigSize;
	char*		 cConfigFile;

	if ( this->isMaster() )
	{
		if ( model::configFile != NULL )
		{
			pDataset = new CXMLDataset(std::string(model::configFile));
		}
		else {
			pDataset = new CXMLDataset("");
		}

		uiConfigSize = pDataset->getFileLength();
		pManager->log->writeLine( "Broadcasting configuration size..." );
	} else {
		pManager->log->writeLine( "Waiting for configuration size..." );
	}
	
	if ( this->iNodeCount <= 1 )
		return true;

	wrapError( 
		MPI_Bcast(
			&uiConfigSize,
			sizeof(uiConfigSize),
			MPI_BYTE,
			0,
			MPI_COMM_WORLD
		)
	);

	pManager->log->writeLine( "Configuration file is " + toString( uiConfigSize ) + " bytes." );

	if ( this->isMaster() )
	{
		pManager->log->writeLine( "Broadcasting configuration file..." );
		wrapError( 
			MPI_Bcast(
				const_cast<char*>(pDataset->getFileContents()),
				uiConfigSize,
				MPI_BYTE,
				0,
				MPI_COMM_WORLD
			)
		);
	} else {
		pManager->log->writeLine( "Waiting for configuration contents..." );
		cConfigFile = new char[ uiConfigSize + 1 ];
		wrapError( 
			MPI_Bcast(
				cConfigFile,
				uiConfigSize,
				MPI_BYTE,
				0,
				MPI_COMM_WORLD
			)
		);
		cConfigFile[ uiConfigSize ] = 0; // Null terminated
		pDataset = new CXMLDataset(cConfigFile);
		pManager->log->writeLine( "Loaded configuration received from master..." );
		delete [] cConfigFile;
	}

	return true;
}

/*
 *  Swap device information among nodes
 */
bool CMPIManager::exchangeDevices()
{
	//if ( this->iNodeCount <= 1 )
	//	return true;

	struct sNodeCoreData
	{
		char			cHostname[255];
		unsigned int	uiDeviceCount;
	};

	pManager->log->writeLine( "Exchanging core data on device counts with each node." );
	for( int i = 0; i < iNodeCount; ++i )
	{
		sNodeCoreData pNodeData;

		if ( i == iNodeID )
		{
			// Broadcast details for this node
			Util::getHostname(&pNodeData.cHostname[0]);
			pNodeData.uiDeviceCount = pManager->getExecutor()->getDeviceCount();

			wrapError(
				MPI_Bcast(
					&pNodeData,
					sizeof(sNodeCoreData),
					MPI_BYTE,
					i,
					MPI_COMM_WORLD
				)
			);

		} else {
			// Receive details for another node
			wrapError(
				MPI_Bcast(
					&pNodeData,
					sizeof(sNodeCoreData),
					MPI_BYTE,
					i,
					MPI_COMM_WORLD
				)
			);
		}

		this->pNodes[i] = new CMPINode();
		this->pNodes[i]->setHostname( std::string(pNodeData.cHostname) );
		this->pNodes[i]->setDeviceCount( pNodeData.uiDeviceCount );
	}

	pManager->log->writeLine( "Exchanging summary data on each device with each node." );
	for( int i = 0; i < iNodeCount; ++i )
	{
		COCLDevice::sDeviceSummary *pDevices = new COCLDevice::sDeviceSummary[this->pNodes[i]->getDeviceCount()];

		if ( i == iNodeID )
		{
			// Send details of each device
			for( unsigned int j = 1; j <= this->pNodes[i]->getDeviceCount(); j++ )
				pManager->getExecutor()->getDevice(j)->getSummary( pDevices[j-1] );

			wrapError(
				MPI_Bcast(
					pDevices,
					sizeof(COCLDevice::sDeviceSummary) * this->pNodes[i]->getDeviceCount(),
					MPI_BYTE,
					i,
					MPI_COMM_WORLD
				)
			);

		} else {

			// Populate devices
			wrapError(
				MPI_Bcast(
					pDevices,
					sizeof(COCLDevice::sDeviceSummary) * this->pNodes[i]->getDeviceCount(),
					MPI_BYTE,
					i,
					MPI_COMM_WORLD
				)
			);
		}

		for (unsigned int j = 0; j < this->pNodes[i]->getDeviceCount(); j++)
			this->pNodes[i]->setDeviceInfo(j, pDevices[j]);

		delete[] pDevices;
	}

	// Store device ranges for each node
	unsigned int uiBase = 1;
	for( int i = 0; i < iNodeCount; ++i )
	{
		this->pNodes[i]->setDeviceRange( uiBase, uiBase + this->pNodes[i]->getDeviceCount() - 1 );
		uiBase += this->pNodes[i]->getDeviceCount();
	}

	// Log details for all nodes
	this->logCommDetails();

	return true;
}

/*
 *  Swap domain information among nodes
 */
bool CMPIManager::exchangeDomains()
{
	//if ( this->iNodeCount <= 1 )
	//	return true;

	// Prepare our list of summary information for the domains
	// we have authority over
	unsigned int*				uiDomainCounts = new unsigned int[this->iNodeCount];
	CDomainBase::DomainSummary*	pDomainLocalSummaries = new CDomainBase::DomainSummary[ pManager->getDomainSet()->getDomainCount() ];
	unsigned int				uiDomainLocalAuthCount = 0;

	// Fetch for all domains
	for (int i = 0; i < pManager->getDomainSet()->getDomainCount(); ++i)
	{
		pDomainLocalSummaries[i] = pManager->getDomainSet()->getDomainBase(i)->getSummary();
		if (pDomainLocalSummaries[i].bAuthoritative)
			uiDomainLocalAuthCount++;
	}

	// Create subset for those we have authority for
	CDomainBase::DomainSummary*	pDomainAuth = new CDomainBase::DomainSummary[uiDomainLocalAuthCount];
	uiDomainLocalAuthCount = 0;
	for (int i = 0; i < pManager->getDomainSet()->getDomainCount(); ++i)
	{
		if (pDomainLocalSummaries[i].bAuthoritative)
			pDomainAuth[uiDomainLocalAuthCount++] = pDomainLocalSummaries[i];
	}
	delete[] pDomainLocalSummaries;

	uiDomainCounts[iNodeID] = uiDomainLocalAuthCount;

	// Swap total number of authoritative domains on each node
	pManager->log->writeLine("Exchanging domain authority information with all nodes.");
	for (int i = 0; i < iNodeCount; ++i)
	{
		wrapError(
			MPI_Bcast(
				&uiDomainCounts[i],
				sizeof(unsigned int),
				MPI_BYTE,
				i,
				MPI_COMM_WORLD
			)
		);

		this->pNodes[i]->setDomainCount(uiDomainCounts[i]);
	}

	pManager->log->writeLine("Exchanging domain information to populate key details for all nodes.");
	for (int i = 0; i < iNodeCount; ++i)
	{
		if (i == iNodeID)
		{
			wrapError(
				MPI_Bcast(
					pDomainAuth,		// Note: not the array defined above...
					sizeof(CDomainBase::DomainSummary) * uiDomainCounts[i],
					MPI_BYTE,
					i,
					MPI_COMM_WORLD
				)
			);
		}
		else {
			CDomainBase::DomainSummary *pDomains = new CDomainBase::DomainSummary[uiDomainCounts[i]];

			// Populate domain summary info
			wrapError(
				MPI_Bcast(
					pDomains,
					sizeof(CDomainBase::DomainSummary) * uiDomainCounts[i],
					MPI_BYTE,
					i,
					MPI_COMM_WORLD
				)
			);

			// Set the summary details for each domain this node has authority for
			for (unsigned int j = 0; j < uiDomainCounts[i]; j++)
				static_cast<CDomainRemote*>(pManager->getDomainSet()->getDomainBase(pDomains[j].uiDomainID))->setSummary(pDomains[j]);

			delete[] pDomains;
		}
	}

	// Clean up memory
	delete[] uiDomainCounts;
	delete[] pDomainAuth;

	return true;
}

/*
 *  Send status information
 */
bool CMPIManager::sendStatus()
{
	return true;
}

/*
 *  Send simulation status data
 */
bool CMPIManager::sendDataSimulation()
{
	if ( this->isMaster() )
		return true;
	
	CMPIManager::mpiSignalSimulation pHeader;
	pHeader.iSignalCode  = model::mpiSignalCodes::kSignalProgress;
	pHeader.iDomainCount = 0;
	
	for( unsigned int i = 0; i < pManager->getDomainSet()->getDomainCount(); i++ )
	{
		if ( pManager->getDomainSet()->isDomainLocal( i ) )
			pHeader.iDomainCount++;
	}
	
	// TODO: Append the domain details here and create a larger structure...
	unsigned int uiDataSize = sizeof( CMPIManager::mpiSignalSimulation ) + 
							  sizeof( CDomainBase::mpiSignalDataProgress ) *
							  pHeader.iDomainCount;
	char *pData = new char[ uiDataSize ];
	unsigned int uiDomainCountAuth = 0;
	
	std::memcpy( pData, &pHeader, sizeof( CMPIManager::mpiSignalSimulation ) );
	
	for( unsigned int i = 0; i < pManager->getDomainSet()->getDomainCount(); i++ )
	{
		if ( pManager->getDomainSet()->isDomainLocal( i ) )
		{
			CDomainBase::mpiSignalDataProgress pDomain = pManager->getDomainSet()->getDomainBase(i)->getDataProgress();
		
			std::memcpy( 
				pData + sizeof( CMPIManager::mpiSignalSimulation ) + sizeof( CDomainBase::mpiSignalDataProgress ) * uiDomainCountAuth, 
				&pDomain, 
				sizeof( CDomainBase::mpiSignalDataProgress ) 
			);
			
			uiDomainCountAuth++;
		}
	}
	
	MPI_Request pRequest;
	
	wrapError(
		MPI_Isend(
			pData,
			uiDataSize,
			MPI_BYTE,
			0,
			model::mpiSignalCodes::kSignalProgress,
			MPI_COMM_WORLD,
			&pRequest
		)
	);
	
	this->sendOps.push_back( mpiSendOp( pRequest, 0, pData ) );
	
	return true;
}

/*
 *  Receive simulation status data
 */
void CMPIManager::receiveDataSimulation( char* cData )
{
	if ( !this->isMaster() )
		return;

	CMPIManager::mpiSignalSimulation pHeader;
	std::memcpy( &pHeader, &cData[0], sizeof( CMPIManager::mpiSignalSimulation ) );
	
	CDomainBase::mpiSignalDataProgress pDomainData;
	
	for( unsigned int i = 0; i < pHeader.iDomainCount; i++ )
	{
		std::memcpy( 
			&pDomainData, 
			&cData[ sizeof( CMPIManager::mpiSignalSimulation ) + sizeof( CDomainBase::mpiSignalDataProgress ) * i ], 
			sizeof( CDomainBase::mpiSignalDataProgress ) 
		);
		
		pManager->getDomainSet()->getDomainBase( pDomainData.uiDomainID )->setDataProgress( pDomainData );
	}
}

/*
 *  Send domain link data
 */
bool CMPIManager::sendDataDomainLink( int iTargetNodeID, char* pData, unsigned int uiSize )
{
	MPI_Request pRequest;
	
	wrapError(
		MPI_Isend(
			pData,
			uiSize,
			MPI_BYTE,
			iTargetNodeID,
			model::mpiSignalCodes::kSignalDomainData,
			MPI_COMM_WORLD,
			&pRequest
		)
	);
	
	this->sendOps.push_back( mpiSendOp( pRequest, iTargetNodeID, pData ) );
	
	return true;
}

/*
 *  Receive domain link data
 */
void CMPIManager::receiveDataDomainLink( char* cData )
{
	CDomainLink::mpiSignalDataDomain pHeader;
	std::memcpy( &pHeader, &cData[0], sizeof( CDomainLink::mpiSignalDataDomain ) );

	CDomainLink*	pTargetLink = pManager->getDomainSet()->getDomainBase( pHeader.uiTargetDomainID )->getLinkFrom( pHeader.uiSourceDomainID );
	
	if ( pTargetLink == NULL )
	{
		model::doError(
			"Received domain data for a link doesn't seem to exist.",
			model::errorCodes::kLevelModelStop
		);
		return;
	}
	
	pTargetLink->pullFromMPI( pHeader.dValidityTime, &cData[ sizeof( CDomainLink::mpiSignalDataDomain ) ] );
}

/*
 *  Probe for and deal with incoming messages
 */
void CMPIManager::processQueue()
{
	MPI_Status pStatus;
	int		   iFlag = 0;
		
	bReceiving = false;
	
	if ( this->iNodeCount <= 1 )
		return;
		
	// Pending send operation has completed?
	if ( this->sendOps.size() > 0 )
	{
		for( int i = this->sendOps.size() - 1; i >= 0; i-- )
		{
			wrapError( MPI_Request_get_status(
					this->sendOps[i].pRequest,
					&iFlag,
					&pStatus
			) );
			
			if ( iFlag )
			{
			
				MPI_Test( &this->sendOps[i].pRequest, &iFlag, &pStatus );
				if ( iFlag )
				{
					delete [] this->sendOps[i].pData;
					this->sendOps.erase( this->sendOps.begin() + i );
				}
				
			} else {
				
				if ( this->sendOps[i].iTarget < this->iNodeID )
				{
					MPI_Wait(
						&this->sendOps[i].pRequest,
						&pStatus
					);
					delete [] this->sendOps[i].pData;
					this->sendOps.erase( this->sendOps.begin() + i );
				}
				
			}
		
		}
	}
	
	int iSize = 0;
	iFlag = 0;

	// Check for new messages
	wrapError( 
		MPI_Iprobe(
			MPI_ANY_SOURCE,
			MPI_ANY_TAG,
			MPI_COMM_WORLD,
			&iFlag,
			&pStatus
		)
	);

	if ( !iFlag )
	{
		return;
	}
	
	// A broadcast is likely to come in with a negative tag...
	if ( pStatus.MPI_TAG < 0 )
		return;
	
	// Some MPI implementations (incl. Microsoft) can supply the count
	// within the status struct...
	wrapError( MPI_Get_count( &pStatus, MPI_BYTE, &iSize ) );

	if ( iSize <= 0 )
		return;
		
	if ( iSize == MPI_UNDEFINED )
	{
		model::doError(
			"Got undefined size from an MPI receive...",
			model::errorCodes::kLevelWarning
		);
		return;
	}
		
	// Fetch...
	if ( this->pRecvBuffer == NULL || iSize > this->uiRecvBufferSize )
	{
		if ( this->pRecvBuffer != NULL )
			delete [] this->pRecvBuffer;
		this->pRecvBuffer = new char[ iSize ];
		this->uiRecvBufferSize = iSize;
	}

	bReceiving = true;
	
	wrapError( 
		MPI_Recv(
			this->pRecvBuffer,
			iSize,
			MPI_BYTE,
			pStatus.MPI_SOURCE,
			pStatus.MPI_TAG,
			MPI_COMM_WORLD,
			&pStatus
		)
	);
	
	this->processIncoming( pStatus.MPI_TAG );
	
	bReceiving = false;
}

/*
 *  Deal with received messages
 */
void CMPIManager::processIncoming( int iTag )
{
	int		iMessageCode = NULL;

	memcpy( &iMessageCode, &this->pRecvBuffer[0], sizeof( int ) );
	
	switch( iMessageCode )
	{
		case model::mpiSignalCodes::kSignalProgress:
			this->receiveDataSimulation( this->pRecvBuffer );
			break;
		case model::mpiSignalCodes::kSignalDomainData:
			this->receiveDataDomainLink( this->pRecvBuffer );
			break;
		default:
			pManager->log->writeLine("[DEBUG] Received a new MPI signal (="+toString( iMessageCode )+") which hasn't been processed...");
			break;
	}
}

/*
 *  Send domain link data
 */
bool CMPIManager::reduceTimeData( double dMinimum, double* dOutput, double dTime, bool bForce )
{
	if ( this->iNodeCount <= 1 )
	{
		*dOutput = dMinimum;
		this->dCollective_ReductionLastTime = dTime;
		return false;
	}
	
	if ( *dOutput <= 0.0 && dMinimum > 0.0 )
		this->dCollective_ReductionLastTime = 0.0;

	if (  dTime == this->dCollective_ReductionLastTime &&
		 !bForce )
		return false;
		
	if ( this->bCollective_Reduction )
	{
		if ( this->dCollective_ReductionInput != dMinimum )
		{
			model::doError(
				"Attempted to start new reduction before current has finished: C" + 
				toString( this->dCollective_ReductionInput ) + ", N" + 
				toString( dMinimum ),
				model::errorCodes::kLevelWarning
			);
		}
		return true;
	}
		
	this->dCollective_ReductionLastTime = dTime;
	this->dCollective_ReductionInput 	= dMinimum;
	this->dCollective_ReductionReturn 	= dOutput;
	this->bCollective_Hold				= true;
	this->bCollective_Reduction			= true;
	
	return true;
}

/*
 *	Block across all nodes 
 */
void CMPIManager::asyncBlockOnComm()
{
	if ( this->iNodeCount <= 1 )
		return;
	
	if ( this->bCollective_Reduction )
	{
		return;
	}
	
	this->bCollective_Hold		= true;
	this->bCollective_Barrier	= true;
}

/*
 *	Block across all nodes in this thread
 */
void CMPIManager::blockOnComm()
{
	if ( this->iNodeCount <= 1 )
		return;

	MPI_Barrier(
		MPI_COMM_WORLD
	);
}

/*
 *	Handle the launching of our collective operation handling thread
 *	(different function signatures on different platforms...)
 */
#ifdef PLATFORM_WIN
DWORD CMPIManager::Threaded_processCollective_Launch(LPVOID param)
{
	CMPIManager* pMPI = static_cast<CMPIManager*>(param);
	pMPI->Threaded_processCollective();
	return 0;
}
#endif
#ifdef PLATFORM_UNIX
void* CMPIManager::Threaded_processCollective_Launch(void* param)
{
	CMPIManager* pMPI = static_cast<CMPIManager*>(param);
	pMPI->Threaded_processCollective();
	return 0;
}
#endif

/*
 *	Handle barriers and reductions in a separate thread with flags so we
 *	can continue execution on our main threads and process MPI sends/receive
 *	on those too...
 */
void	CMPIManager::Threaded_processCollective()
{
	double *dReturn = new double;

	while ( this->bCollectiveThreadRun )
	{
		if ( this->bCollective_Reduction )
		{
			wrapError(
				MPI_Allreduce(
					&this->dCollective_ReductionInput,
					dReturn,
					1,
					MPI_DOUBLE,
					MPI_MIN,
					MPI_COMM_WORLD
				)
			);

			if ( *this->dCollective_ReductionReturn < 0.0 )
			{
#ifdef DEBUG_MPI
				pManager->log->writeLine( "[DEBUG] Repeating reduction as a global block interfered." );
				this->bCollective_Barrier 	= false;
#endif
			} else {
#ifdef DEBUG_MPI
				pManager->log->writeLine( "[DEBUG] Finished an all reduction: " + toString( *this->dCollective_ReductionReturn ) );
#endif
				this->dCollective_ReductionInput = *this->dCollective_ReductionReturn;
				*this->dCollective_ReductionReturn = *dReturn;
				this->bCollective_Reduction = false;
			}
		}
	
		// Wait until all on the COMM are sync'd then set the flag back
		if ( this->bCollective_Barrier )
		{
			double dInput = -9999.9;
			wrapError(
				MPI_Allreduce(
					&dInput,
					&dReturn,
					1,
					MPI_DOUBLE,
					MPI_MIN,
					MPI_COMM_WORLD
				)
			);
			this->bCollective_Barrier = *dReturn >= 0.0;
		}
		
		// Allow simulation to progress
		if ( !this->bCollective_Barrier &&
		     !this->bCollective_Reduction )
		{
			this->bCollective_Hold = false;
		}
	}

	delete dReturn;
}

#endif