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
#include <vector>

namespace model {

	// Model domain structure types
	namespace mpiSignalCodes {
		enum mpiSignalCodes {
			kSignalDomainData		= 1,	// Node is ready for synchronisation
			kSignalRollbackRequired	= 2,	// A whole model rollback is required
			kSignalProgress			= 3		// Just a progress update
		};
	}
}

#ifdef MPI_ON

#ifndef HIPIMS_MPI_CMPIMANAGER_H
#define HIPIMS_MPI_CMPIMANAGER_H

class CMPINode;
class CXMLDataset;
class CMPIManager
{
public:
	CMPIManager();
	~CMPIManager();
	int						getNodeID()								{ return iNodeID; };
	int						getNodeCount()							{ return iNodeCount; };
	unsigned int			getDeviceCount();
	void					logDetails();
	void					logCommDetails();
	CMPINode*				getNode()								{ return pNodes[iNodeID]; };
	CMPINode*				getNode(int i)							{ return pNodes[i]; };
	bool					isMaster()								{ return ( iNodeID == 0 ); };
	bool					isWaitingOnTransmission()				{ return ( sendOps.size() > 0 ); };
	bool					exchangeConfiguration( CXMLDataset*& );
	bool					exchangeDevices();
	bool					exchangeDomains();
	bool					sendDataSimulation();
	void					receiveDataSimulation( char* );
	bool					sendDataDomainLink( int, char*, unsigned int );
	void					receiveDataDomainLink( char* );
	bool					sendStatus();
	bool					reduceTimeData( double, double*, double, bool = false );
	bool					isWaitingOnBlock()						{ return iNodeCount > 1 && ( bCollective_Hold | bCollective_Barrier | bCollective_Reduction ); };
	double					getLastCollectiveTime()					{ return dCollective_ReductionLastTime; };
	void					asyncBlockOnComm();
	void					blockOnComm();
	void					processIncoming( int );
	void					processQueue();
	void					Threaded_processCollective();
#ifdef PLATFORM_WIN
	static DWORD			Threaded_processCollective_Launch(LPVOID param);
#endif
#ifdef PLATFORM_UNIX
	static void*			Threaded_processCollective_Launch(void* param);
#endif
	
protected:
	// Nothing

private:

	struct mpiSignalSimulation
	{
		int		iSignalCode;
		int		iDomainCount;
	};
	
	struct mpiSendOp
	{
		MPI_Request 	pRequest;
		int				iTarget;
		char*			pData;
		mpiSendOp( MPI_Request a, int b, char* c )
		{
			pRequest = a;
			iTarget  = b;
			pData    = c;
		};
	};

	void					wrapError( int );
	int						iNodeID;
	int						iNodeCount;
	CMPINode**				pNodes;
	MPI_Request				pRecvReq;
	bool					bReceiving;
	std::vector<mpiSendOp>	sendOps;
	char*					pRecvBuffer;
	unsigned int			uiRecvBufferSize;
	bool					bCollectiveThreadRun;
	bool					bCollective_Hold;
	bool					bCollective_Reduction;
	bool					bCollective_Barrier;
	double					dCollective_ReductionInput;
	double*					dCollective_ReductionReturn;
	double					dCollective_ReductionLastTime;
	
};

#endif
#endif