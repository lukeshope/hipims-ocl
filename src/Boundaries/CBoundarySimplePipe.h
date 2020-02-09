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
		cl_float		length;
		cl_float		roughness;
		cl_float		lossCoefficients;
		cl_float		diameter;
		cl_float		invertStart;
		cl_float		invertEnd;
	};
	struct sConfigurationDP
	{
		cl_uint			uiStartCellX;
		cl_uint			uiStartCellY;
		cl_uint			uiEndCellX;
		cl_uint			uiEndCellY;
		cl_double		length;
		cl_double		roughness;
		cl_double		lossCoefficients;
		cl_double		diameter;
		cl_double		invertStart;
		cl_double		invertEnd;
	};

	COCLBuffer*		pBufferConfiguration;
	unsigned int	startCellX;
	unsigned int	startCellY;
	unsigned int	endCellX;
	unsigned int	endCellY;
	double			length;
	double			roughness;
	double			lossCoefficients;
	double			diameter;
	double			invertStart;
	double			invertEnd;

	bool			bedElevationChecked;
};

#endif
