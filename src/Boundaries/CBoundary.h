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

#define BOUNDARY_DEFINEDAS_DEPTH		1
#define BOUNDARY_DEFINEDAS_DISCHARGE	2
#define BOUNDARY_DEFINED_LEVEL			4
#define BOUNDARY_DEFINED_FLOWX			8
#define BOUNDARY_DEFINED_FLOWY			16
#define BOUNDARY_DEFINEDAS_NEIGHBOUR_N	32
#define BOUNDARY_DEFINEDAS_NEIGHBOUR_E	64
#define BOUNDARY_DEFINEDAS_NEIGHBOUR_S	128
#define BOUNDARY_DEFINEDAS_NEIGHBOUR_W	256
#define	BOUNDARY_DEFINEDAS_REFLECTIVE	512
#define	BOUNDARY_DEFINEDAS_FLOWSURGE	1024
#define	BOUNDARY_DEFINEDAS_CRITICALDEPTH	2048
#define BOUNDARY_DEFINEDAS_FSL			0
#define BOUNDARY_DEFINEDAS_VELOCITY		0
#define BOUNDARY_UNDEFINED_LEVEL		0
#define BOUNDARY_UNDEFINED_FLOWX		0
#define BOUNDARY_UNDEFINED_FLOWY		0

namespace model {

// Kernel configurations
namespace boundaries { 
namespace types { enum types {
	kBndyTypeCell,
	kBndyTypeAtmospheric,
	kBndyTypeCopy,
	kBndyTypeReflective,		// -- Put the gridded types after this
	kBndyTypeAtmosphericGrid
}; }

namespace depthValues { enum depthValues {
	kValueFSL						= BOUNDARY_DEFINED_LEVEL | BOUNDARY_DEFINEDAS_FSL,		// 2nd column in timeseries is the FSL
	kValueDepth						= BOUNDARY_DEFINED_LEVEL | BOUNDARY_DEFINEDAS_DEPTH,	// 2nd column in timeseries is a depth
	kValueCriticalDepth				= BOUNDARY_DEFINED_LEVEL | BOUNDARY_DEFINEDAS_DEPTH | BOUNDARY_DEFINEDAS_CRITICALDEPTH, // Force critical depth based on the discharge
	kValueIgnored					= BOUNDARY_UNDEFINED_LEVEL								// 2nd column can be ignored
}; }

namespace dischargeValues { enum dischargeValues {
	kValueTotal						= BOUNDARY_DEFINED_FLOWX | BOUNDARY_DEFINED_FLOWY | BOUNDARY_DEFINEDAS_DISCHARGE,	// Value represents the total discharge through the boundary
	kValuePerCell					= BOUNDARY_DEFINED_FLOWX | BOUNDARY_DEFINED_FLOWY | BOUNDARY_DEFINEDAS_DISCHARGE,	// Value represents the discharge per cell through the boundary
	kValueVelocity					= BOUNDARY_DEFINED_FLOWX | BOUNDARY_DEFINED_FLOWY | BOUNDARY_DEFINEDAS_VELOCITY,	// Value represents a velocity through the boundary
	kValueSurging					= BOUNDARY_DEFINED_FLOWX | BOUNDARY_DEFINED_FLOWY | BOUNDARY_DEFINEDAS_FLOWSURGE, // Value represents a depth increase in volumetric rate terms (e.g. manhole surge)
	kValueIgnored					= BOUNDARY_UNDEFINED_FLOWX | BOUNDARY_UNDEFINED_FLOWY
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
