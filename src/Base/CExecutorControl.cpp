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
 *  Executor Control Base Class
 * ------------------------------------------
 *
 */

// TODO: Remove this class entirely (we only deal with OpenCL
// executors for the purposes of this model now).

// Includes
#include "../common.h"
#include "../Datasets/CXMLDataset.h"
#include "CExecutorControl.h"
#include "../OpenCL/Executors/CExecutorControlOpenCL.h"

/*
 *  Constructor
 */
CExecutorControl::CExecutorControl(void)
{
	// This stub class is not enough to be ready
	// The child classes should change this value when they're
	// done initialising.
	this->setState( model::executorStates::executorError );
}

/*
 *  Destructor
 */
CExecutorControl::~CExecutorControl(void)
{
	// ...
}

/*
 *  Create a new executor of the specified type (static func)
 */
CExecutorControl*	CExecutorControl::createExecutor( unsigned char cType )
{
	switch ( cType )
	{
		case model::executorTypes::executorTypeOpenCL:
			return new CExecutorControlOpenCL();
		break;
	}

	return NULL;
}

/*
 *  Create and configure an executor using an XML configuration node
 */
CExecutorControl*	CExecutorControl::createFromConfig( XMLElement* pXNode )
{
	CExecutorControl*	pExecutor		= NULL;
	char*				cExecutorName	= NULL;

	Util::toLowercase( &cExecutorName, pXNode->Attribute( "name" ) );
	if ( cExecutorName == NULL )
	{
		model::doError(
			"The <executor> element has no name.",
			model::errorCodes::kLevelWarning
		);
		return NULL;
	}

	if ( std::strcmp( cExecutorName, "opencl" ) == 0 )
	{
		pManager->log->writeLine( "OpenCL executor specified in configuration." );
		pExecutor = CExecutorControl::createExecutor(
			model::executorTypes::executorTypeOpenCL
		);
	} else {
		model::doError(
			"Unsupported executor specified.",
			model::errorCodes::kLevelWarning
		);
		return NULL;
	}
	
	pExecutor->setupFromConfig( pXNode );

	return pExecutor;
}

/*
 *  Is this executor ready to run models?
 */
bool CExecutorControl::isReady( void )
{
	return this->state == model::executorStates::executorReady;
}

/*
 *  Set the ready state of this executor
 */
void CExecutorControl::setState( unsigned int iState )
{
	this->state = iState;
}

/*
 *  Set the device filter to determine what types of device we
 *  can use to execute the model.
 */
void CExecutorControl::setDeviceFilter( unsigned int uiFilters )
{
	this->deviceFilter = uiFilters;
}

/*
 *  Return any current device filters
 */
unsigned int CExecutorControl::getDeviceFilter()
{
	return this->deviceFilter;
}

