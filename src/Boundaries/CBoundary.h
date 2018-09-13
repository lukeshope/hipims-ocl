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
#ifndef HIPIMS_BOUNDARIES_CBOUNDARY_H_
#define HIPIMS_BOUNDARIES_CBOUNDARY_H_

#include "../common.h"
#include "../Datasets/CCSVDataset.h"

#define BOUNDARY_DEPTH_IGNORE			0
#define BOUNDARY_DEPTH_IS_FSL			1
#define BOUNDARY_DEPTH_IS_DEPTH			2
#define BOUNDARY_DEPTH_IS_CRITICAL		3

#define BOUNDARY_DISCHARGE_IGNORE		0
#define BOUNDARY_DISCHARGE_IS_DISCHARGE	1
#define BOUNDARY_DISCHARGE_IS_VELOCITY	2
#define BOUNDARY_DISCHARGE_IS_VOLUME	3

namespace model {

// Kernel configurations
namespace boundaries { 
namespace types { enum types {
	kBndyTypeCell,
	kBndyTypeAtmospheric,
	kBndyTypeCopy,
	kBndyTypeReflective,		// -- Put the gridded types after this
	kBndyTypeAtmosphericGrid,
	kBndyTypeSimplePipe
}; }

namespace depthValues { enum depthValues {
	kValueFSL = BOUNDARY_DEPTH_IS_FSL,		// 2nd column in timeseries is the FSL
	kValueDepth = BOUNDARY_DEPTH_IS_DEPTH,	// 2nd column in timeseries is a depth
	kValueCriticalDepth = BOUNDARY_DEPTH_IS_CRITICAL, // Force critical depth based on the discharge
	kValueIgnored = BOUNDARY_DEPTH_IGNORE								// 2nd column can be ignored
}; }

namespace dischargeValues { enum dischargeValues {
	kValueTotal = BOUNDARY_DISCHARGE_IS_DISCHARGE,	// Value represents the total discharge through the boundary
	kValuePerCell = BOUNDARY_DISCHARGE_IS_DISCHARGE,	// Value represents the discharge per cell through the boundary
	kValueVelocity = BOUNDARY_DISCHARGE_IS_VELOCITY,	// Value represents a velocity through the boundary
	kValueSurging = BOUNDARY_DISCHARGE_IS_VOLUME, // Value represents a depth increase in volumetric rate terms (e.g. manhole surge)
	kValueIgnored = BOUNDARY_DISCHARGE_IGNORE
}; }

namespace griddedValues { enum griddedValues {
	kValueRainIntensity				= 0,
	kValueMassFlux					= 1
}; }

namespace uniformValues { enum lossUnits {
	kValueRainIntensity				= 0,
	kValueLossRate					= 1
}; }

}
}

// Class stubs
class COCLBuffer;
class COCLDevice;
class COCLProgram;
class COCLKernel;

class CBoundary
{
public:
	CBoundary( CDomain* = NULL );
	~CBoundary();

	virtual bool					setupFromConfig(XMLElement*, std::string) = 0;
	virtual void					prepareBoundary(COCLDevice*, COCLProgram*, COCLBuffer*, COCLBuffer*,
												    COCLBuffer*, COCLBuffer*, COCLBuffer*) = 0;
	virtual void					applyBoundary(COCLBuffer*) = 0;
	virtual void					streamBoundary(double) = 0;
	virtual void					cleanBoundary() = 0;
	virtual void					importMap(CCSVDataset*)				{};
	std::string						getName()							{ return sName; };

	static int			uiInstances;

protected:	

	CDomain*			pDomain;
	COCLKernel*			oclKernel;
	std::string			sName;

	/*
	unsigned int		iType;
	unsigned int		iDepthValue;
	unsigned int		iDischargeValue;
	unsigned int		uiTimeseriesLength;
	cl_double			dTimeseriesInterval;

	double				dTotalVolume;

	friend class CBoundaryMap;
	*/
};

#endif
