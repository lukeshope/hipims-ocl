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
#include <vector>
#include <boost/lexical_cast.hpp>

#include "CBoundaryMap.h"
#include "CBoundarySimplePipe.h"
#include "../Domain/Cartesian/CDomainCartesian.h"
#include "../OpenCL/Executors/COCLBuffer.h"
#include "../OpenCL/Executors/COCLKernel.h"
#include "../common.h"

using std::vector;

/* 
 *  Constructor
 */
CBoundarySimplePipe::CBoundarySimplePipe( CDomain* pDomain )
{
	this->pBufferConfiguration = NULL;
	this->pDomain = pDomain;
	this->bedElevationChecked = false;
}

/*
 *  Destructor
 */
CBoundarySimplePipe::~CBoundarySimplePipe()
{
	delete this->pBufferConfiguration;
}

/*
 *	Configure this boundary and load in any related files
 */
bool CBoundarySimplePipe::setupFromConfig(XMLElement* pElement, std::string sBoundarySourceDir)
{
	char *cBoundaryType, 
		 *cBoundaryName,
		 *cBoundaryStartX,
		 *cBoundaryStartY,
		 *cBoundaryEndX,
		 *cBoundaryEndY,
		 *cBoundaryPipeLength,
		 *cBoundaryPipeOrientation,
		 *cBoundaryRoughness,
		 *cBoundaryLossCoefficients,
		 *cBoundaryDiameter,
		 *cBoundaryInvertStart,
		 *cBoundaryInvertEnd;

	Util::toLowercase(&cBoundaryType,			pElement->Attribute("type"));
	Util::toNewString(&cBoundaryName,			pElement->Attribute("name"));
	Util::toLowercase(&cBoundaryPipeLength,		pElement->Attribute("length"));
	Util::toLowercase(&cBoundaryPipeOrientation,pElement->Attribute("orientation"));
	Util::toLowercase(&cBoundaryRoughness,		pElement->Attribute("roughness"));
	Util::toLowercase(&cBoundaryLossCoefficients, pElement->Attribute("lossCoefficients"));
	Util::toLowercase(&cBoundaryDiameter,		pElement->Attribute("diameter"));
	Util::toLowercase(&cBoundaryInvertStart,	pElement->Attribute("invertStart"));
	Util::toLowercase(&cBoundaryInvertEnd,		pElement->Attribute("invertEnd"));
	Util::toLowercase(&cBoundaryStartX,			pElement->Attribute("startX"));
	Util::toLowercase(&cBoundaryStartY,			pElement->Attribute("startY"));
	Util::toLowercase(&cBoundaryEndX,			pElement->Attribute("endX"));
	Util::toLowercase(&cBoundaryEndY,			pElement->Attribute("endY"));

	this->sName				= std::string( cBoundaryName );
	this->length			= boost::lexical_cast<double>(cBoundaryPipeLength);
	this->roughness			= boost::lexical_cast<double>(cBoundaryRoughness);
	this->lossCoefficients	= boost::lexical_cast<double>(cBoundaryLossCoefficients);
	this->diameter			= boost::lexical_cast<double>(cBoundaryDiameter);
	this->invertStart		= boost::lexical_cast<double>(cBoundaryInvertStart);
	this->invertEnd			= boost::lexical_cast<double>(cBoundaryInvertEnd);

	double offsetX = 0.0, offsetY = 0.0;
	if (cBoundaryPipeOrientation != NULL) {
		double orientation = boost::lexical_cast<double>(cBoundaryPipeOrientation);
		offsetX = sin(orientation / 180 * CL_M_PI) * this->length;
		offsetY = cos(orientation / 180 * CL_M_PI) * this->length;
	}

	CDomainCartesian* pDomain = static_cast<CDomainCartesian*>(this->pDomain);

	if (pDomain->isRemote()) {
		model::doError(
			"Attempted to setup pipe boundary on remote domain.",
			model::errorCodes::kLevelModelStop
		);
		return false;
	}

	double dCornerN, dCornerE, dCornerS, dCornerW, dResolution;
	pDomain->getCellResolution(&dResolution);
	pDomain->getRealExtent(&dCornerN, &dCornerE, &dCornerS, &dCornerW);

	double startX			= boost::lexical_cast<double>(cBoundaryStartX);
	double startY			= boost::lexical_cast<double>(cBoundaryStartY);

	this->startCellX = floor((startX - dCornerW) / dResolution);
	this->startCellY = floor((startY - dCornerS) / dResolution);

	if (cBoundaryEndX != NULL && cBoundaryEndY != NULL) {
		double endX = boost::lexical_cast<double>(cBoundaryEndX);
		double endY = boost::lexical_cast<double>(cBoundaryEndY);
		this->endCellX = floor((endX - dCornerW) / dResolution);
		this->endCellY = floor((endY - dCornerS) / dResolution);
	}
	else
	{
		this->endCellX = this->startCellX + ceil(fabs(offsetX / dResolution)) * (offsetX > 0 ? 1.0 : -1.0);
		this->endCellY = this->startCellY + ceil(fabs(offsetY / dResolution)) * (offsetY > 0 ? 1.0 : -1.0);
	}

	return true;
}

/*
 *	Import cell map data from a CSV file
 */
void CBoundarySimplePipe::importMap(CCSVDataset *pCSV)
{
	// Not applicable to this type of boundary
}

void CBoundarySimplePipe::prepareBoundary(
			COCLDevice* pDevice, 
			COCLProgram* pProgram,
			COCLBuffer* pBufferBed, 
			COCLBuffer* pBufferManning,
			COCLBuffer* pBufferTime,
			COCLBuffer* pBufferTimeHydrological,
			COCLBuffer* pBufferTimestep
	 )
{
	// Configuration for the pipe, no timeseries required
	if (pProgram->getFloatForm() == model::floatPrecision::kSingle)
	{
		sConfigurationSP pConfiguration;

		pConfiguration.diameter = this->diameter;
		pConfiguration.invertStart = this->invertStart;
		pConfiguration.invertEnd = this->invertEnd;
		pConfiguration.length = this->length;
		pConfiguration.lossCoefficients = this->lossCoefficients;
		pConfiguration.roughness = this->roughness;
		pConfiguration.uiStartCellX = this->startCellX;
		pConfiguration.uiStartCellY = this->startCellY;
		pConfiguration.uiEndCellX = this->endCellX;
		pConfiguration.uiEndCellY = this->endCellY;
		
		this->pBufferConfiguration = new COCLBuffer(
			"Bdy_" + this->sName + "_Conf",
			pProgram,
			true,
			true,
			sizeof(sConfigurationSP),
			true
		);
		std::memcpy(
			this->pBufferConfiguration->getHostBlock<void*>(),
			&pConfiguration,
			sizeof(sConfigurationSP)
		);
	}
	else {
		sConfigurationDP pConfiguration;

		pConfiguration.diameter = this->diameter;
		pConfiguration.invertStart = this->invertStart;
		pConfiguration.invertEnd = this->invertEnd;
		pConfiguration.length = this->length;
		pConfiguration.lossCoefficients = this->lossCoefficients;
		pConfiguration.roughness = this->roughness;
		pConfiguration.uiStartCellX = this->startCellX;
		pConfiguration.uiStartCellY = this->startCellY;
		pConfiguration.uiEndCellX = this->endCellX;
		pConfiguration.uiEndCellY = this->endCellY;

		this->pBufferConfiguration = new COCLBuffer(
			"Bdy_" + this->sName + "_Conf",
			pProgram,
			true,
			true,
			sizeof(sConfigurationDP),
			true
		);
		std::memcpy(
			this->pBufferConfiguration->getHostBlock<void*>(),
			&pConfiguration,
			sizeof(sConfigurationDP)
		);
	}

	this->pBufferConfiguration->createBuffer();
	this->pBufferConfiguration->queueWriteAll();

	this->oclKernel = pProgram->getKernel("bdy_SimplePipe");
	COCLBuffer* aryArgsBdy[] = {
		pBufferConfiguration,						// 0
		pBufferTime,								// 1
		pBufferTimestep,							// 2
		pBufferTimeHydrological,					// 3
		NULL,	// Cell states (added later)		   4
		pBufferBed,									// 5
		pBufferManning								// 6
	};

	this->oclKernel->assignArguments(aryArgsBdy);
	this->oclKernel->setGroupSize(1);
	this->oclKernel->setGlobalSize(1);
}

void CBoundarySimplePipe::applyBoundary(COCLBuffer* pBufferCell)
{
	if (!this->bedElevationChecked) {
		CDomainCartesian* pDomain = static_cast<CDomainCartesian*>(this->pDomain);

		double dBedStart = pDomain->getBedElevation(pDomain->getCellID(this->startCellX, this->startCellY));
		double dBedEnd = pDomain->getBedElevation(pDomain->getCellID(this->endCellX, this->endCellY));

		if (this->invertStart < dBedStart || this->invertEnd < dBedEnd) {
			model::doError(
				"Pipe '" + this->sName + "' has invert lower than bed elevation.",
				model::errorCodes::kLevelModelStop
			);
		}

		this->bedElevationChecked = true;
	}

	this->oclKernel->assignArgument( 4, pBufferCell );
	this->oclKernel->scheduleExecution();
}

void CBoundarySimplePipe::streamBoundary(double dTime)
{
	// ...
	// TODO: Should we handle all the memory buffer writing in here?...
}

void CBoundarySimplePipe::cleanBoundary()
{
	// ...
	// TODO: Is this needed? Most stuff is cleaned in the destructor
}

