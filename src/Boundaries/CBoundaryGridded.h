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
#ifndef HIPIMS_BOUNDARIES_CBOUNDARYGRIDDED_H_
#define HIPIMS_BOUNDARIES_CBOUNDARYGRIDDED_H_

#include "../common.h"
#include "CBoundary.h"

class CBoundaryGridded : public CBoundary
{
public:
	CBoundaryGridded(CDomain* = NULL);
	~CBoundaryGridded();

	virtual bool					setupFromConfig(XMLElement*, std::string);
	virtual void					prepareBoundary(COCLDevice*, COCLProgram*, COCLBuffer*, COCLBuffer*,
													COCLBuffer*, COCLBuffer*, COCLBuffer*);
	virtual void					applyBoundary(COCLBuffer*);
	virtual void					streamBoundary(double);
	virtual void					cleanBoundary();

	struct SBoundaryGridTransform
	{
		double			dSourceResolution;
		double			dTargetResolution;
		double			dOffsetSouth;
		double			dOffsetWest;
		unsigned int	uiRows;
		unsigned int	uiColumns;
		unsigned long	ulBaseSouth;
		unsigned long	ulBaseWest;
	};

	class CBoundaryGriddedEntry
	{
	public:
		CBoundaryGriddedEntry(double, double*);
		~CBoundaryGriddedEntry();

		double			dTime;
		double*			dValues;
		void*			getBufferData(unsigned char, SBoundaryGridTransform*);
	};

protected:

	struct sConfigurationSP
	{
		cl_float		TimeseriesInterval;
		cl_float		GridResolution;
		cl_float		GridOffsetX;
		cl_float		GridOffsetY;
		cl_ulong		TimeseriesEntries;
		cl_ulong		Definition;
		cl_ulong		GridRows;
		cl_ulong		GridCols;
	};
	struct sConfigurationDP
	{
		cl_double		TimeseriesInterval;
		cl_double		GridResolution;
		cl_double		GridOffsetX;
		cl_double		GridOffsetY;
		cl_ulong		TimeseriesEntries;
		cl_ulong		Definition;
		cl_ulong		GridRows;
		cl_ulong		GridCols;
	};

	void							setValue(unsigned char a)				{ ucValue = a; };

	unsigned char					ucValue;

	double							dTotalVolume;
	double							dTimeseriesLength;
	double							dTimeseriesInterval;

	CBoundaryGriddedEntry**			pTimeseries;
	SBoundaryGridTransform*			pTransform;
	unsigned int					uiTimeseriesLength;

	COCLBuffer*						pBufferTimeseries;
	COCLBuffer*						pBufferConfiguration;
};

#endif
