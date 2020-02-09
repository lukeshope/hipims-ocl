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
#ifndef HIPIMS_BOUNDARIES_CBOUNDARYUNIFORM_H_
#define HIPIMS_BOUNDARIES_CBOUNDARYUNIFORM_H_

#include "../common.h"
#include "CBoundary.h"

class CBoundaryUniform : public CBoundary
{
public:
	CBoundaryUniform( CDomain* = NULL );
	~CBoundaryUniform();

	virtual bool					setupFromConfig(XMLElement*, std::string);
	virtual void					prepareBoundary(COCLDevice*, COCLProgram*, COCLBuffer*, COCLBuffer*,
													COCLBuffer*, COCLBuffer*, COCLBuffer*);
	virtual void					applyBoundary(COCLBuffer*);
	virtual void					streamBoundary(double);
	virtual void					cleanBoundary();

protected:

	struct sConfigurationSP
	{
		cl_uint			TimeseriesEntries;
		cl_float		TimeseriesInterval;
		cl_float		TimeseriesLength;
		cl_uint			Definition;
	};
	struct sConfigurationDP
	{
		cl_uint			TimeseriesEntries;
		cl_double		TimeseriesInterval;
		cl_double		TimeseriesLength;
		cl_uint			Definition;
	};
	struct sTimeseriesUniform
	{
		cl_double		dTime;
		cl_double		dComponent;	
	};

	void							setValue(unsigned char a)			{ ucValue = a; };
	void							importTimeseries(CCSVDataset*);

	unsigned char					ucValue;

	double							dTotalVolume;
	double							dTimeseriesLength;
	double							dTimeseriesInterval;
	double							dRatio;

	sTimeseriesUniform*				pTimeseries;
	unsigned int					uiTimeseriesLength;

	COCLBuffer*						pBufferTimeseries;
	COCLBuffer*						pBufferConfiguration;
};

#endif
