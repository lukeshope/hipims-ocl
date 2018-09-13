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
#ifndef HIPIMS_BOUNDARIES_CBOUNDARYSIMPLEPIPE_H_
#define HIPIMS_BOUNDARIES_CBOUNDARYSIMPLEPIPE_H_

#include "../common.h"
#include "CBoundary.h"

class CBoundarySimplePipe : public CBoundary
{
public:
	CBoundarySimplePipe( CDomain* = NULL );
	~CBoundarySimplePipe();

	virtual bool					setupFromConfig(XMLElement*, std::string);
	virtual void					prepareBoundary(COCLDevice*, COCLProgram*, COCLBuffer*, COCLBuffer*,
													COCLBuffer*, COCLBuffer*, COCLBuffer*);
	virtual void					applyBoundary(COCLBuffer*);
	virtual void					streamBoundary(double);
	virtual void					cleanBoundary();
	virtual void					importMap(CCSVDataset*);

protected:	
	struct sConfigurationSP
	{
		cl_uint			uiStartCellX;
		cl_uint			uiStartCellY;
		cl_uint			uiEndCellX;
		cl_uint			uiEndCellY;
		cl_ulong		ent1;
		cl_float		ent2;
		cl_float		ent3;
		cl_ulong		ent4;
		cl_uint			ent5;
		cl_uint			ent6;
	};
	struct sConfigurationDP
	{
		cl_uint			uiStartCellX;
		cl_uint			uiStartCellY;
		cl_uint			uiEndCellX;
		cl_uint			uiEndCellY;
		cl_ulong		ent1;
		cl_double		ent2;
		cl_double		ent3;
		cl_ulong		ent4;
		cl_uint			ent5;
		cl_uint			ent6;
	};

	COCLBuffer*						pBufferConfiguration;
};

#endif
