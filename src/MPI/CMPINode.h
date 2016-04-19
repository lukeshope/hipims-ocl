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

#ifndef HIPIMS_MPI_CMPINODE_H
#define HIPIMS_MPI_CMPINODE_H

#include "../OpenCL/Executors/COCLDevice.h"
#include <string>

class CMPINode
{
public:
	CMPINode();
	~CMPINode();

	void						setHostname( std::string s )					{ sHostname = s; };
	std::string					getHostname()									{ return sHostname; };
	void						setDeviceCount( unsigned int i );
	unsigned int				getDeviceCount()								{ return uiDeviceCount; };
	void						setDomainCount( unsigned int i )				{ uiDomainCount = i; };
	unsigned int				getDomainCount()								{ return uiDomainCount; };
	void						setDeviceInfo( unsigned int i, COCLDevice::sDeviceSummary a )	
																				{ pDeviceInfo[i] = a; };
	COCLDevice::sDeviceSummary	getDeviceInfo( unsigned int i )					{ return pDeviceInfo[i]; };
	void						setDeviceRange( unsigned int, unsigned int );
	unsigned int				getDeviceBaseID()								{ return uiDeviceNoLow; };
	bool						isDeviceOnNode( unsigned int i );
	
protected:
	// Nothing

private:
	std::string					sHostname;
	unsigned int				uiDeviceCount;
	unsigned int				uiDomainCount;
	COCLDevice::sDeviceSummary*	pDeviceInfo;
	unsigned int				uiDeviceNoLow;
	unsigned int				uiDeviceNoHigh;
};

#endif
#endif