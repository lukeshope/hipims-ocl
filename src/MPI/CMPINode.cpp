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
 *  MPI node
 * ------------------------------------------
 *
 */
#ifdef MPI_ON

// Includes
#include "../common.h"
#include "CMPINode.h"

/*
 *  Constructor
 */
CMPINode::CMPINode()
{
	// To do
}

/*
 *  Destructor
 */
CMPINode::~CMPINode()
{
	delete [] this->pDeviceInfo;
}

/*
 *  Set the total number of devices and allocate memory
 */
void CMPINode::setDeviceCount( unsigned int uiCount )
{
	this->uiDeviceCount = uiCount;
	this->pDeviceInfo   = new COCLDevice::sDeviceSummary[ uiCount ];
}

/*
 *  Set the device range which resides on this node
 */
void CMPINode::setDeviceRange(unsigned int uiLow, unsigned int uiHigh)
{
	this->uiDeviceNoLow = uiLow;
	this->uiDeviceNoHigh = uiHigh;
}

/*
 *  Set the device range which resides on this node
 */
bool CMPINode::isDeviceOnNode(unsigned int uiDeviceNo)
{
	// Device numbers start at 1...
	return ( uiDeviceNo <= this->uiDeviceNoHigh && uiDeviceNo >= this->uiDeviceNoLow );
}

#endif
