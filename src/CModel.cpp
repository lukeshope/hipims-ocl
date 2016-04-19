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
 *  Main model class
 * ------------------------------------------
 *
 */

// Includes
#include <cmath>
#include <math.h>
#include "common.h"
#include "main.h"
#include "OpenCL/Executors/CExecutorControlOpenCL.h"
#include "Domain/CDomainManager.h"
#include "Domain/CDomain.h"
#include "Schemes/CScheme.h"
#include "Datasets/CXMLDataset.h"
#include "Datasets/CRasterDataset.h"
#include "MPI/CMPIManager.h"

using std::min;
using std::max;

/*
 *  Constructor
 */
CModel::CModel(void)
{
	this->log = new CLog();

	this->execController	= NULL;
	this->domains			= new CDomainManager();
#ifdef MPI_ON
	this->mpiManager		= new CMPIManager();
#else
	this->mpiManager		= NULL;
#endif

	this->dCurrentTime		= 0.0;
	this->dSimulationTime	= 60;
	this->dOutputFrequency	= 60;
	this->bDoublePrecision	= true;

	this->pProgressCoords.sX = -1;
	this->pProgressCoords.sY = -1;

	this->ulRealTimeStart = 0;
}

/*
 *  Setup the simulation using parameters specified in the configuration file
 */
void CModel::setupFromConfig( XMLElement* pXNode )
{
	XMLElement*		pParameter			= pXNode->FirstChildElement( "parameter" );
	char			*cParameterName		= NULL;
	char			*cParameterValue	= NULL;
	char			*cParameterFormat   = NULL;

	while ( pParameter != NULL )
	{
		Util::toLowercase( &cParameterName, pParameter->Attribute( "name" ) );
		Util::toLowercase( &cParameterValue, pParameter->Attribute( "value" ) );
		Util::toNewString( &cParameterFormat, pParameter->Attribute( "format" ) );

		if ( strcmp( cParameterName, "duration" ) == 0 )
		{ 
			if ( !CXMLDataset::isValidFloat( cParameterValue ) )
			{
				model::doError(
					"Invalid simulation length given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setSimulationLength( boost::lexical_cast<double>( cParameterValue ) );
			}
		}
		else if ( strcmp( cParameterName, "realstart" ) == 0 )
		{ 
			this->setRealStart( cParameterValue, cParameterFormat );
		}
		else if ( strcmp( cParameterName, "outputfrequency" ) == 0 )
		{ 
			if ( !CXMLDataset::isValidFloat( cParameterValue ) )
			{
				model::doError(
					"Invalid output frequency given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setOutputFrequency( boost::lexical_cast<double>( cParameterValue ) );
			}
		}
		else if ( strcmp( cParameterName, "floatingpointprecision" ) == 0 )
		{ 
			unsigned char ucFPPrecision = 255;
			if ( strcmp( cParameterValue, "single" ) == 0 )
				ucFPPrecision = model::floatPrecision::kSingle;
			if ( strcmp( cParameterValue, "double" ) == 0 )
				ucFPPrecision = model::floatPrecision::kDouble;
			if ( ucFPPrecision == 255 )
			{
				model::doError(
					"Invalid float precision given.",
					model::errorCodes::kLevelWarning
				);
			} else {
				this->setFloatPrecision( ucFPPrecision );
			}
		}
		else 
		{
			model::doError(
				"Unrecognised parameter: " + std::string( cParameterName ),
				model::errorCodes::kLevelWarning
			);
		}

		pParameter = pParameter->NextSiblingElement( "parameter" );
	}
}

/*
 *  Destructor
 */
CModel::~CModel(void)
{
	if (this->domains != NULL)
		delete this->domains;
	if ( this->execController != NULL )
		delete this->execController;
	this->log->writeLine("The model engine is completely unloaded.");
	delete this->log;
}

/*
 *  Set the type of executor to use for the model
 */
bool CModel::setExecutor(CExecutorControl* pExecutorControl)
{
	// TODO: Has the value actually changed?

	// TODO: Delete the old executor controller

	this->execController = static_cast<CExecutorControlOpenCL*>(pExecutorControl);

	if ( !this->execController->isReady() )
	{
		this->log->writeError(
			"The executor is not ready. Model cannot continue.",
			model::errorCodes::kLevelFatal
		);
		return false;
	}

	return true;
}

/*
 *  Returns a pointer to the execution controller currently in use
 */
CExecutorControlOpenCL* CModel::getExecutor( void )
{
	return this->execController;
}

/*
 *  Returns a pointer to the domain class
 */
CDomainManager* CModel::getDomainSet( void )
{
	return this->domains;
}

/*
*  Returns a pointer to the MPI manager class
*/
CMPIManager* CModel::getMPIManager(void)
{
	// Pass back the pointer - will be NULL if not using MPI version
	return this->mpiManager;
}

/*
 *  Log the details for the whole simulation
 */
void CModel::logDetails()
{
	unsigned short wColour = model::cli::colourInfoBlock;

	this->log->writeDivide();
	this->log->writeLine( "SIMULATION CONFIGURATION", true, wColour );
	this->log->writeLine( "  Name:               " + this->sModelName, true, wColour );
	this->log->writeLine( "  Start time:         " + std::string( Util::fromTimestamp( this->ulRealTimeStart, "%d-%b-%Y %H:%M:%S" ) ), true, wColour );
	this->log->writeLine( "  End time:           " + std::string( Util::fromTimestamp( this->ulRealTimeStart + static_cast<unsigned long>( std::ceil( this->dSimulationTime ) ), "%d-%b-%Y %H:%M:%S" ) ), true, wColour );
	this->log->writeLine( "  Simulation length:  " + Util::secondsToTime( this->dSimulationTime ), true, wColour );
	this->log->writeLine( "  Output frequency:   " + Util::secondsToTime( this->dOutputFrequency ), true, wColour );
	this->log->writeLine( "  Floating-point:     " + (std::string)( this->getFloatPrecision() == model::floatPrecision::kDouble ? "Double-precision" : "Single-precision" ), true, wColour );
	this->log->writeDivide();
}

/*
 *  Execute the model
 */
bool CModel::runModel( void )
{
	this->log->writeLine( "Verifying the required data before model run..." );

	if ( !this->domains || !this->domains->isSetReady() )
	{
		model::doError(
			"The domain is not ready.",
			model::errorCodes::kLevelModelStop
		);
		return false;
	}
	if ( !this->execController || !this->execController->isReady() )
	{
		model::doError(
			"The executor is not ready.",
			model::errorCodes::kLevelModelStop
		);
		return false;
	}

	this->log->writeLine( "Verification is complete." );

	this->log->writeDivide();
	this->log->writeLine( "Starting a new simulation..." );

	this->runModelPrepare();
	this->runModelMain();

	return true;
}

/*
 *  Sets a short name for the model
 */
void	CModel::setName( std::string sName )
{
	this->sModelName = sName;
}

/*
 *  Sets a short name for the model
 */
void	CModel::setDescription( std::string sDescription )
{
	this->sModelDescription = sDescription;
}

/*
 *  Sets the total length of a simulation
 */
void	CModel::setSimulationLength( double dLength )
{
	this->dSimulationTime		= dLength;
}

/*
 *  Gets the total length of a simulation
 */
double	CModel::getSimulationLength()
{
	return this->dSimulationTime;
}

/*
 *  Set the frequency of outputs
 */
void	CModel::setOutputFrequency( double dFrequency )
{
	this->dOutputFrequency = dFrequency;
}

/*
 *  Sets the real world start time
 */
void	CModel::setRealStart( char* cTime, char* cFormat )
{
	this->ulRealTimeStart = Util::toTimestamp( cTime, cFormat );
}

/*
 *  Fetch the real world start time
 */
unsigned long CModel::getRealStart()
{
	return this->ulRealTimeStart;
}

/*
 *  Get the frequency of outputs
 */
double	CModel::getOutputFrequency()
{
	return this->dOutputFrequency;
}

/*
 *  Write periodical output files to disk
 */
void	CModel::writeOutputs()
{
	this->getDomainSet()->writeOutputs();
}

/*
 *  Set floating point precision
 */
void	CModel::setFloatPrecision( unsigned char ucPrecision )
{
	if ( !pManager->getExecutor()->getDevice()->isDoubleCompatible() )
		ucPrecision = model::floatPrecision::kSingle;

	this->bDoublePrecision = ( ucPrecision == model::floatPrecision::kDouble );
}

/*
 *  Get floating point precision
 */
unsigned char	CModel::getFloatPrecision()
{
	return ( this->bDoublePrecision ? model::floatPrecision::kDouble : model::floatPrecision::kSingle );
}

/*
 *  Write details of where model execution is currently at
 */
void	CModel::logProgress( CBenchmark::sPerformanceMetrics* sTotalMetrics )
{
	char	cTimeLine[70]      = "                                                                    X";
	char	cCellsLine[70]     = "                                                                    X";
	char	cTimeLine2[70]     = "                                                                    X";
	char	cCells[70]         = "                                                                    X";
	char	cProgressLine[70]  = "                                                                    X";
	char	cBatchSizeLine[70] = "                                                                    X";
	char	cProgress[57]      = "                                                      ";
	char	cProgessNumber[7]  = "      ";

	unsigned short wColour	= model::cli::colourInfoBlock;

	double		  dCurrentTime = ( this->dCurrentTime > this->dSimulationTime ? this->dSimulationTime : this->dCurrentTime );
	double		  dProgress = dCurrentTime / this->dSimulationTime;

	// TODO: These next bits will need modifying for when we have multiple domains
	unsigned long long	ulCurrentCellsCalculated		   = 0;
	unsigned int		uiBatchSizeMax = 0, uiBatchSizeMin = 9999;
	double				dSmallestTimestep				   = 9999.0;

	// Get the total number of cells calculated
	for( unsigned int i = 0; i < domains->getDomainCount(); ++i )
	{
		if (domains->isDomainLocal(i))
		{
			// Get the number of cells calculated (for the rate mainly)
			// TODO: Deal with this for MPI...
			ulCurrentCellsCalculated += domains->getDomain(i)->getScheme()->getCellsCalculated();
		}

		CDomainBase::mpiSignalDataProgress pProgress = domains->getDomain(i)->getDataProgress();

		if (uiBatchSizeMax < pProgress.uiBatchSize)
			uiBatchSizeMax = pProgress.uiBatchSize;
		if (uiBatchSizeMin > pProgress.uiBatchSize)
			uiBatchSizeMin = pProgress.uiBatchSize;
		if (dSmallestTimestep > pProgress.dBatchTimesteps)
			dSmallestTimestep = pProgress.dBatchTimesteps;
	}

	unsigned long ulRate = static_cast<unsigned long>(ulCurrentCellsCalculated / sTotalMetrics->dSeconds);

	// Make a progress bar
	for( unsigned char i = 0; i <= floor( 55.0f * dProgress ); i++ )
		cProgress[ i ] = ( i >= ( floor( 55.0f * dProgress ) - 1 ) ? '>' : '=' );

	// String padding stuff
	sprintf( cTimeLine,		" Simulation time:  %-15sLowest timestep: %15s", Util::secondsToTime( dCurrentTime ).c_str(), Util::secondsToTime( dSmallestTimestep ).c_str() );
#ifdef PLATFORM_WIN
	sprintf( cCells,		"%I64u", ulCurrentCellsCalculated );
#endif
#ifdef PLATFORM_UNIX
	sprintf( cCells,		"%llu", ulCurrentCellsCalculated );
#endif
	sprintf( cCellsLine,	" Cells calculated: %-24s  Rate: %13s/s", cCells, toString( ulRate ).c_str() );
	sprintf( cTimeLine2,	" Processing time:  %-16sEst. remaining: %15s", Util::secondsToTime( sTotalMetrics->dSeconds ).c_str(), Util::secondsToTime( min( ( 1.0 - dProgress ) * ( sTotalMetrics->dSeconds / dProgress ), 31536000.0 ) ).c_str() );
	sprintf( cBatchSizeLine," Batch size:       %-16s                                 ", toString( uiBatchSizeMin ).c_str() );
	sprintf( cProgessNumber,"%.1f%%", dProgress * 100 );
	sprintf( cProgressLine, " [%-55s] %7s", cProgress, cProgessNumber );

	// DLL callback goes before so the system knows when repeat outputs start
#ifdef _WINDLL
	model::fCallbackProgress(
		dProgress
	);
#endif

	pManager->log->writeDivide();																						// 1
	pManager->log->writeLine( "                                                                  ", false, wColour );	// 2
	pManager->log->writeLine( " SIMULATION PROGRESS                                              ", false, wColour );	// 3
	pManager->log->writeLine( "                                                                  ", false, wColour );	// 4
	pManager->log->writeLine( std::string( cTimeLine )											  , false, wColour );	// 5
	pManager->log->writeLine( std::string( cCellsLine )											  , false, wColour );	// 6
	pManager->log->writeLine( std::string( cTimeLine2 )											  , false, wColour );	// 7
	pManager->log->writeLine( std::string( cBatchSizeLine )										  , false, wColour );	// 8
	pManager->log->writeLine( "                                                                  ", false, wColour );	// 9
	pManager->log->writeLine( std::string( cProgressLine )										  , false, wColour );	// 10
	pManager->log->writeLine( "                                                                  ", false, wColour );	// 11

	pManager->log->writeLine( "             +----------+----------------+------------+----------+", false, wColour );	// 12
	pManager->log->writeLine( "             |  Device  |  Avg.timestep  | Iterations | Bypassed |", false, wColour );	// 12
	pManager->log->writeLine( "+------------+----------+----------------+------------+----------|", false, wColour );	// 13
	
	for( unsigned int i = 0; i < domains->getDomainCount(); i++ )
	{
		char cDomainLine[70] = "                                                                    X";
		CDomainBase::mpiSignalDataProgress pProgress = domains->getDomain(i)->getDataProgress();
		
		// TODO: Give this it's proper name...
		std::string sDeviceName = "REMOTE";

		if (domains->isDomainLocal(i))
		{
			sDeviceName = domains->getDomain(i)->getDevice()->getDeviceShortName();
		}

		sprintf(
			cDomainLine,
			"| Domain #%-2s | %8s | %14s | %10s | %8s |",
			toString(i + 1).c_str(),
			sDeviceName.c_str(),
			Util::secondsToTime(pProgress.dBatchTimesteps).c_str(),
			toString(pProgress.uiBatchSuccessful).c_str(),
			toString(pProgress.uiBatchSkipped).c_str()
		);

		pManager->log->writeLine( std::string( cDomainLine ), false, wColour );	// ++
	}

	pManager->log->writeLine( "+------------+----------+----------------+------------+----------+" , false, wColour);	// 14
	pManager->log->writeDivide();																						// 15

	this->pProgressCoords = Util::getCursorPosition();
	if (this->dCurrentTime < this->dSimulationTime) 
	{
		this->pProgressCoords.sY = max(0, this->pProgressCoords.sY - (16 + domains->getDomainCount()));
		Util::setCursorPosition(this->pProgressCoords);
	}
}

/*
 *  Update the visualisation by sending domain data over to the relevant component
 */
void CModel::visualiserUpdate()
{
	CDomain*	pDomain = pManager->domains->getDomain(0);
	COCLDevice*	pDevice	= pManager->domains->getDomain(0)->getDevice();

	#ifdef _WINDLL
	pManager->domains->getDomain(0)->sendAllToRenderer();
	#endif

	if ( this->dCurrentTime >= this->dSimulationTime - 1E-5 || model::forceAbort )
		return;

	// Request the next batch of data
	// TODO: This should read from all of the domains and ask them all to read back their
	// states...
	#ifdef _WINDLL
	//pManager->domains->getDomain(0)->getScheme()->readDomainAll();
	#endif
}

/*
 *  Memory read should have completed, so provided the simulation isn't over - read it back again
 */
void CL_CALLBACK CModel::visualiserCallback( cl_event clEvent, cl_int iStatus, void * vData )
{
	pManager->visualiserUpdate();
	clReleaseEvent( clEvent );
}

/*
*  Prepare for a new simulation, which may follow a failed simulation so states need to be reset.
*/
void	CModel::runModelPrepare()
{
	// Allow external influences to interupt the simulation (i.e. UI windows)
	model::forceAbort = false;

	// Can't have timestep sync if we've only got one domain
	if (this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep &&
		this->getDomainSet()->getDomainCount() <= 1)
		this->getDomainSet()->setSyncMethod(model::syncMethod::kSyncForecast);

	this->runModelPrepareDomains();

	bSynchronised		= true;
	bAllIdle			= true;
	dTargetTime			= 0.0;
	dLastSyncTime		= -1.0;
	dLastOutputTime		= 0.0;

	// Global block until all domains are ready
	// Don't use the global block function here as that's for async blocking during 
	// the simulation
#ifdef MPI_ON
	pManager->getMPIManager()->blockOnComm();
#endif
}

/*
*  Prepare domains for a new simulation.
*/
void	CModel::runModelPrepareDomains()
{
	for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
	{
		if (!domains->isDomainLocal(i))
			continue;

		domains->getDomain(i)->getScheme()->prepareSimulation();
		domains->getDomain(i)->setRollbackLimit();		// Auto calculate from links...

		if (domains->getDomainCount() > 1)
		{
			pManager->log->writeLine("Domain #" + toString(i + 1) + " has rollback limit of " +
				toString(domains->getDomain(i)->getRollbackLimit()) + " iterations.");
		} else {
			pManager->log->writeLine("Domain #" + toString(i + 1) + " is not constrained by " +
				"overlapping.");
		}
	}
}

/*
*  Assess the current state of each domain.
*/
void	CModel::runModelDomainAssess(
			bool *			bSyncReady,
			bool *			bIdle
		)
{
	bRollbackRequired = false;
	dEarliestTime = 0.0;
	bWaitOnLinks = false;

	// Fetch all the data we need for each domain/scheme
	for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
	{
		if (!domains->isDomainLocal(i))
		{
			// Is this domain sync ready etc?
			// TODO...
			bSyncReady[i] = true;
			bIdle[i] = true;
			continue;
		}

		// Minimum time
		if (dEarliestTime == 0.0 || dEarliestTime > domains->getDomain(i)->getScheme()->getCurrentTime())
			dEarliestTime = domains->getDomain(i)->getScheme()->getCurrentTime();

		// Check if all of the domain links have been received... especially for MPI
		// TODO: Finish this bit.

		// Either we're not ready to sync, or we were still synced from the last run
		if (!domains->getDomain(i)->getScheme()->isSimulationSyncReady(dTargetTime) || bSynchronised || dLastSyncTime == dEarliestTime )
		{
			bSyncReady[i] = false;
			// Handle rollbacks?
			if (domains->getDomain(i)->getScheme()->isSimulationFailure(dTargetTime))
			{
				bRollbackRequired = true;
			}
		}
		else {
			bSyncReady[i] = true;
		}

		// Either we're not ready to sync, or we were still synced from the last run
		if (domains->getDomain(i)->getScheme()->isRunning() || domains->getDomain(i)->getDevice()->isBusy())
		{
			bIdle[i] = false;
		}
		else {
			bIdle[i] = true;
		}
	}

	// Are all the domains synced?
	bSynchronised = true;
	bAllIdle = true;
	for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
	{
		if (!bSyncReady[i]) bSynchronised = false;
		if (!bIdle[i])		bAllIdle = false;
	}
	
	// Only allow sync if the domain links are at the right time and send data to other nodes
	// if we need to
	if ( bSynchronised && bAllIdle )
	{
		for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
		{
			if (domains->isDomainLocal(i))
			{
				if ( !domains->getDomainBase(i)->isLinkSetAtTime( dEarliestTime ) && dEarliestTime > 0.0 )
				{
#ifdef DEBUG_MPI
					pManager->log->writeLine( "[DEBUG] Earliest time: " + Util::secondsToTime( dEarliestTime ) + " - cannot sync." );
#endif
					bSynchronised = false;
					bWaitOnLinks = true;
				}
			} else {
				if ( !domains->getDomainBase(i)->sendLinkData() )
				{
					bWaitOnLinks = true;
					bAllIdle = false;
				}
			}
		}
	}

	// Force this loop to continue without scheduling work until we've sent/received
	// all our MPI messages 
#ifdef MPI_ON
	if ( pManager->getMPIManager()->isWaitingOnTransmission() ||
		 pManager->getMPIManager()->isWaitingOnBlock() )
	{
		bAllIdle = false;
	}
#endif
	
	// If we're synchronising the timesteps we need all idle
	if ( bAllIdle && !bWaitOnLinks )
	{
		double dMinTimestep = 0.0;
		bool bIsSuspended = false;
		for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
		{
			// TODO: This needs to be changed for MPI...
			if (!domains->isDomainLocal(i))
				continue;

			if ( ( dMinTimestep == 0.0 || dMinTimestep > domains->getDomain(i)->getScheme()->getCurrentTimestep() ) &&
				   domains->getDomain(i)->getScheme()->getCurrentTimestep() > 0.0 )
				dMinTimestep = domains->getDomain(i)->getScheme()->getCurrentTimestep();
				
			if ( domains->getDomain(i)->getScheme()->getCurrentSuspendedState() == true )
				bIsSuspended = true;
		}

		// Reduce across all MPI nodes if required
#ifdef MPI_ON
		if ( this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep )
		{
			// Force a reduction in cases where we've just had to write outputs as our timestep would have been zero
			if ( 
					pManager->getMPIManager()->reduceTimeData( 
						dMinTimestep, 
						&this->dGlobalTimestep, 
						this->dEarliestTime
					) 
				)
			{
#ifdef DEBUG_MPI
				pManager->log->writeLine( "[DEBUG] New reduction has cancelled all idle state..." );
#endif
				bAllIdle = false;
			}
		}
		
		if ( this->getDomainSet()->getDomainCount() <= 1 )
			dCurrentTime = dEarliestTime;
#else
		dGlobalTimestep = dMinTimestep;
		dCurrentTime = dEarliestTime;
#endif
	}
}

/*
 *  Exchange data across domains where necessary.
 */
void	CModel::runModelDomainExchange()
{
#ifdef DEBUG_MPI
	pManager->log->writeLine( "[DEBUG] Exchanging domain data NOW... (" + Util::secondsToTime( this->dEarliestTime ) + ")" );
#endif

	// Swap sync zones over
	for (unsigned int i = 0; i < domains->getDomainCount(); ++i)		// Source domain
	{
		if (domains->isDomainLocal(i))
		{
			domains->getDomain(i)->getScheme()->importLinkZoneData();
			// TODO: Above command does not actually cause import -- next line can be removed?
			domains->getDomain(i)->getDevice()->flushAndSetMarker();		
		}
	}
	
	this->runModelBlockNode();
}

/*
*  Synchronise the whole model across all domains.
*/
void	CModel::runModelUpdateTarget( double dTimeBase )
{
	// Identify the smallest batch size associated timestep
	double dEarliestSyncProposal = this->dSimulationTime;

#ifdef DEBUG_MPI
	pManager->log->writeLine( "[DEBUG] Should now be updating the target time..." );
#endif
	
	// Only bother with all this stuff if we actually need to synchronise,
	// otherwise run free, for as long as possible (i.e. until outputs needed)
	if (domains->getDomainCount() > 1 &&
		this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncForecast)
	{
		for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
		{
			// TODO: How to calculate this for remote domains etc?
			if (domains->isDomainLocal(i))
				dEarliestSyncProposal = min(dEarliestSyncProposal, domains->getDomain(i)->getScheme()->proposeSyncPoint(dCurrentTime));
		}
	}

	// Don't exceed an output interval if required
	if (floor(dEarliestSyncProposal / dOutputFrequency)  > floor(dLastSyncTime / dOutputFrequency))
	{
		dEarliestSyncProposal = (floor(dLastSyncTime / dOutputFrequency) + 1) * dOutputFrequency;
	}

	// Reduce across all MPI nodes if required
#ifdef MPI_ON
	if ( this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncForecast )
	{
		if ( pManager->getMPIManager()->reduceTimeData( dEarliestSyncProposal, &this->dTargetTime, this->dEarliestTime, true ) )
		{
#ifdef DEBUG_MPI
			pManager->log->writeLine( "Invoked MPI to reduce target time with " + toString( dEarliestSyncProposal ) );
#endif
			bAllIdle = false;
		}
	} else {
		dTargetTime = dEarliestSyncProposal;
	}
#else
	// Work scheduler within numerical schemes should identify whether this has changed
	// and update the buffer if required only...
	dTargetTime = dEarliestSyncProposal;
#endif
}

/*
*  Synchronise the whole model across all domains.
*/
void	CModel::runModelSync()
{
	if ( bRollbackRequired ||
		 !bSynchronised    ||
		 !bAllIdle )
		return;
		
	// No rollback required, thus we know the simulation time can now be increased
	// to match the target we'd defined
	this->dCurrentTime = dEarliestTime;
	dLastSyncTime = this->dCurrentTime;

	// TODO: Review if this is needed? Shouldn't earliest time get updated anyway?
	if (domains->getDomainCount() <= 1)
		this->dCurrentTime = dEarliestTime;

	// Write outputs if possible
	this->runModelOutputs();
		
	// Calculate a new target time to aim for
	this->runModelUpdateTarget( dCurrentTime );

	// ---
	// TODO: In MPI implementation, we need to invoke a reduce operation here to identify the lowest
	// sync proposal across all domains in the Comm...
	// ---

	// ---
	// TODO: All of this block of code needs to be removed ultimately. Domain links should constantly
	// be prepared to download data when a simulation reaches its target time. Domain exchange should
	// therefore need only to take data already held locally and impose it on the main domain buffer.

	// Let each domain know the goalposts have proverbially moved
	// Read back domain data incase we need to rollback
	for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
	{
		if (domains->isDomainLocal(i))
		{
			// Update with the new target time
			// Can no longer do this here - may have to wait for MPI to return with the value
			//domains->getDomain(i)->getScheme()->setTargetTime(dTargetTime);

			// Save the current state back to host memory, but only if necessary
			// for either domain sync/rollbacks or to write outputs
			if ( 
					( domains->getDomainCount() > 1 && this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncForecast ) ||
					( fmod(this->dCurrentTime, pManager->getOutputFrequency()) < 1E-5 && this->dCurrentTime > dLastOutputTime ) 
			   )
			{
#ifdef DEBUG_MPI
				pManager->log->writeLine( "[DEBUG] Saving domain state for domain #" + toString( i ) );
#endif
				domains->getDomain(i)->getScheme()->saveCurrentState();
			}
		}
	}

	// Let devices finish
	this->runModelBlockNode();
	
	// Exchange domain data
	this->runModelDomainExchange();

	// Wait for all nodes and devices
	// TODO: This should become global across all nodes 
	this->runModelBlockNode();
	//this->runModelBlockGlobal();
}

/*
*  Block execution across all domains which reside on this node only
*/
void	CModel::runModelBlockNode()
{
	for (unsigned int i = 0; i < domains->getDomainCount(); i++)
	{
		if (domains->isDomainLocal(i))
			domains->getDomain(i)->getDevice()->blockUntilFinished();
	}
}

/*
 *  Block execution across all domains until every single one is ready
 */
void	CModel::runModelBlockGlobal()
{
	this->runModelBlockNode();
#ifdef MPI_ON
	pManager->getMPIManager()->asyncBlockOnComm();
#endif
}

/*
 *  Write output files if required.
 */
void	CModel::runModelOutputs()
{
	if ( bRollbackRequired ||
		 !bSynchronised ||
		 !bAllIdle ||
		 !( fmod(this->dCurrentTime, pManager->getOutputFrequency()) < 1E-5 && 
		    this->dCurrentTime > dLastOutputTime ) )
		return;

	this->writeOutputs();
	dLastOutputTime = this->dCurrentTime;
	
	for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
	{
		if (domains->isDomainLocal(i))
			domains->getDomain(i)->getScheme()->forceTimeAdvance();
	}
	
#ifdef DEBUG_MPI
	pManager->log->writeLine( "[DEBUG] Global block until all output files have been written..." );
#endif
	this->runModelBlockGlobal();
}

/*
*  Process incoming and pending MPI messages etc.
*/
void	CModel::runModelMPI()
{
#ifdef MPI_ON
	pManager->getMPIManager()->processQueue();
#endif
}

/*
*  Schedule new work in the simulation.
*/
void	CModel::runModelSchedule(CBenchmark::sPerformanceMetrics * sTotalMetrics, bool * bIdle)
{
	// Normally we don't wait for all idle to schedule new work (only one device needs to be idle)
	// but if we're waiting on MPI activity then we can't do anything
#ifdef MPI_ON
	if ( pManager->getMPIManager()->isWaitingOnBlock() )
		return;
	
	// Synchronising timesteps so every iteration needs a collective operation
	if ( this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep &&
	     this->dEarliestTime != pManager->getMPIManager()->getLastCollectiveTime() )
		return;
#endif

	// If we're synchronising the timestep we only progress from here
	// if ALL our domains are idle
	if (this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep &&
		!bAllIdle)
		return;

	// Keep running each domain until we're ready for synchronisation
	for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
	{
		if (!domains->isDomainLocal(i))
			continue;

		// Progress until the target time is reached...
		// Either we're not ready to sync, or we were still synced from the last run
		if (!bSynchronised && bIdle[i])
		{
#ifdef MPI_ON
			// TODO: Review this - shouldn't be needed!
			if ( this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep &&
			     domains->getDomain(i)->getScheme()->getCurrentTime() != pManager->getMPIManager()->getLastCollectiveTime() )
				continue;
#endif
				
			// Set the timestep if we're synchronising them
			if (this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep &&
				dGlobalTimestep > 0.0)
				domains->getDomain(i)->getScheme()->forceTimestep(dGlobalTimestep);
			//pManager->log->writeLine( "Global timestep: " + toString( dGlobalTimestep ) + " Current time: " + toString( domains->getDomain(i)->getScheme()->getCurrentTime() ) );

			// Run a batch
			//pManager->log->writeLine("[DEBUG] Running scheme to " + toString(dTargetTime));
			domains->getDomain(i)->getScheme()->runSimulation(dTargetTime, sTotalMetrics->dSeconds);
		}
	}
	
	// Wait if we're syncing timesteps?
	//if ( this->getDomainSet()->getSyncMethod() == model::syncMethod::kSyncTimestep )
	//	this->runModelBlockGlobal();
}

/*
*  Update UI elements (progress bars etc.)
*/
void	CModel::runModelUI( CBenchmark::sPerformanceMetrics * sTotalMetrics )
{
	dProcessingTime = sTotalMetrics->dSeconds;
	if (sTotalMetrics->dSeconds - dLastProgressUpdate > 0.85)
	{
		this->logProgress(sTotalMetrics);
		dLastProgressUpdate = sTotalMetrics->dSeconds;
		
#ifdef MPI_ON
		// Send data back to root on the COMM if needed
		pManager->getMPIManager()->sendDataSimulation();
#endif
	}
}

/*
*  Rollback simulation states to a previous recorded state.
*/
void	CModel::runModelRollback()
{
	if ( !bRollbackRequired ||
		 model::forceAbort  ||
		 !bAllIdle )
		return;

	model::doError(
		"Rollback invoked - code not yet ready",
		model::errorCodes::kLevelModelStop
	);
		
	// Now sync'd again and ready to continue
	bRollbackRequired = false;
	bSynchronised = false;

	// Use the data from the last run to work out how long we can run 
	// the batch for. Same function as normal but relative to the last sync time instead.
	this->runModelUpdateTarget(dLastSyncTime);
	pManager->log->writeLine("Simulation rollback at " + Util::secondsToTime(this->dCurrentTime) + "; revised sync point is " + Util::secondsToTime(dTargetTime) + ".");

	// ---
	// TODO: Do we need to do an MPI reduce here...?
	// ---

	// ---
	// TODO: Notify all nodes of the rollback requirements...?
	// ---

	// Let each domain know the goalposts have proverbially moved
	// Write the last full set of domain data back to the GPU
	dEarliestTime = dLastSyncTime;
	dCurrentTime = dLastSyncTime;
	for (unsigned int i = 0; i < domains->getDomainCount(); i++)
	{
		if (domains->isDomainLocal(i))
			domains->getDomain(i)->getScheme()->rollbackSimulation(dLastSyncTime, dTargetTime);
	}

	// Global block across all nodes is required for rollbacks
	runModelBlockGlobal();
}


/*
 *  Clean things up after the model is complete or aborted
 */
void	CModel::runModelCleanup()
{
	// Note these will not return until their threads have terminated
	for (unsigned int i = 0; i < domains->getDomainCount(); ++i)
	{
		if (domains->isDomainLocal(i))
			domains->getDomain(i)->getScheme()->cleanupSimulation();
	}
}

/*
 *  Run the actual simulation, asking each domain and schemes therein in turn etc.
 */
void	CModel::runModelMain()
{
	bool*							bSyncReady				= new bool[ domains->getDomainCount() ];
	bool*							bIdle					= new bool[domains->getDomainCount()];
	double							dCellRate				= 0.0;
	CBenchmark::sPerformanceMetrics *sTotalMetrics;
	CBenchmark						*pBenchmarkAll;

	// Write out the simulation details
	this->logDetails();

	// Track time for the whole simulation
	pManager->log->writeLine( "Collecting time and performance data..." );
	pBenchmarkAll = new CBenchmark( true );
	sTotalMetrics = pBenchmarkAll->getMetrics();

	// Track total processing time
	dProcessingTime = sTotalMetrics->dSeconds;
	dVisualisationTime = dProcessingTime;

	// ---------
	// Run the main management loop
	// ---------
	// Even if user has forced abort, still wait until all idle state is reached
	while ( ( this->dCurrentTime < dSimulationTime - 1E-5 && !model::forceAbort ) || !bAllIdle )
	{
		// Assess the overall state of the simulation at present
		this->runModelDomainAssess(
			bSyncReady,
			bIdle
		);

		// Exchange data over MPI
#ifdef MPI_ON
		this->runModelMPI();
#endif
		
		// Perform a rollback if required
		this->runModelRollback();

		// Perform a sync if possible
		this->runModelSync();
		
		// Don't proceed beyond this point if we need to rollback and we're just waiting for 
		// devices to finish first...
		if (bRollbackRequired)
			continue;

		// Schedule new work
		this->runModelSchedule(
			sTotalMetrics,
			bIdle
		);

		// Update progress bar after each batch, not every time
		sTotalMetrics = pBenchmarkAll->getMetrics();
		this->runModelUI(
			sTotalMetrics
		);
	}

	// Update to 100% progress bar
	pBenchmarkAll->finish();
	sTotalMetrics = pBenchmarkAll->getMetrics();
	this->runModelUI(
		sTotalMetrics
	);

	// Simulation was aborted?
	if ( model::forceAbort )
	{
		model::doError(
			"Simulation has been aborted",
			model::errorCodes::kLevelModelStop
		);
	}

	// Get the total number of cells calculated
	unsigned long long	ulCurrentCellsCalculated = 0;
	double				dVolume = 0.0;
	for( unsigned int i = 0; i < domains->getDomainCount(); ++i )
	{
		if (!domains->isDomainLocal(i))
			continue;

		ulCurrentCellsCalculated += domains->getDomain(i)->getScheme()->getCellsCalculated();
		dVolume += abs( domains->getDomain(i)->getVolume() );
	}
	unsigned long ulRate = static_cast<unsigned long>(static_cast<double>(ulCurrentCellsCalculated) / sTotalMetrics->dSeconds);

	pManager->log->writeLine( "Simulation time:     " + Util::secondsToTime( sTotalMetrics->dSeconds ) );
	//pManager->log->writeLine( "Calculation rate:    " + toString( floor(dCellRate) ) + " cells/sec" );
	//pManager->log->writeLine( "Final volume:        " + toString( static_cast<int>( dVolume ) ) + "m3" );
	pManager->log->writeDivide();

	delete   pBenchmarkAll;
	delete[] bSyncReady;
	delete[] bIdle;
}

