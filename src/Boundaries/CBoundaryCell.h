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
#ifndef HIPIMS_BOUNDARIES_CBOUNDARYCELL_H_
#define HIPIMS_BOUNDARIES_CBOUNDARYCELL_H_

#include "../common.h"
#include "CBoundary.h"

class CBoundaryCell : public CBoundary
{
public:
	CBoundaryCell( CDomain* = NULL );
	~CBoundaryCell();

	virtual bool					setupFromConfig(XMLElement*, std::string);
	virtual void					prepareBoundary(COCLDevice*, COCLProgram*, COCLBuffer*, COCLBuffer*,
													COCLBuffer*, COCLBuffer*, COCLBuffer*);
	virtual void					applyBoundary(COCLBuffer*);
	virtual void					streamBoundary(double);
	virtual void					cleanBoundary();
	virtual void					importMap(CCSVDataset*);

protected:	

	struct sTimeseriesCell
	{
		cl_double		dTime;
		cl_double		dDepthComponent;		// FSL/depth
		cl_double		dDischargeComponentX;	// Discharge (X)
		cl_double		dDischargeComponentY;	// Discharge (Y)
	};
	struct sRelationCell
	{
		cl_uint			uiCellX;
		cl_uint			uiCellY;
	};

	struct sConfigurationSP
	{
		cl_ulong		TimeseriesEntries;
		cl_float		TimeseriesInterval;
		cl_float		TimeseriesLength;
		cl_ulong		RelationCount;
		cl_uint			DefinitionDepth;
		cl_uint			DefinitionDischarge;
	};
	struct sConfigurationDP
	{
		cl_ulong		TimeseriesEntries;
		cl_double		TimeseriesInterval;
		cl_double		TimeseriesLength;
		cl_ulong		RelationCount;
		cl_uint			DefinitionDepth;
		cl_uint			DefinitionDischarge;
	};

	void							setDischargeValue( unsigned char a )		{ ucDischargeValue = a; };
	void							setDepthValue( unsigned char a )			{ ucDepthValue = a; };
	void							importTimeseries( CCSVDataset* );

	unsigned char					ucDischargeValue;
	unsigned char					ucDepthValue;

	double							dTotalVolume;
	double							dTimeseriesLength; 
	double							dTimeseriesInterval;

	sTimeseriesCell*				pTimeseries;
	sRelationCell*					pRelations;
	unsigned int					uiTimeseriesLength;
	unsigned int					uiRelationCount;

	COCLBuffer*						pBufferTimeseries;
	COCLBuffer*						pBufferRelations;
	COCLBuffer*						pBufferConfiguration;
};

#endif
