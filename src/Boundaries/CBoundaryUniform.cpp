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
#include "CBoundaryUniform.h"
#include "../Domain/Cartesian/CDomainCartesian.h"
#include "../OpenCL/Executors/COCLBuffer.h"
#include "../OpenCL/Executors/COCLKernel.h"
#include "../common.h"

using std::vector;

/*
 *  Constructor
 */
CBoundaryUniform::CBoundaryUniform( CDomain* pDomain )
{
	this->ucValue = model::boundaries::uniformValues::kValueLossRate;

	this->pDomain = pDomain;
	this->dRatio = 1.0;
}

/*
*  Destructor
*/
CBoundaryUniform::~CBoundaryUniform()
{
	delete[] this->pTimeseries;

	delete this->pBufferConfiguration;
	delete this->pBufferTimeseries;
}

/*
*	Configure this boundary and load in any related files
*/
bool CBoundaryUniform::setupFromConfig(XMLElement* pElement, std::string sBoundarySourceDir)
{
	char *cBoundaryType, *cBoundaryName, *cBoundarySource, *cBoundaryValue, *cBoundaryRatio;

	Util::toLowercase(&cBoundaryType, pElement->Attribute("type"));
	Util::toNewString(&cBoundaryName, pElement->Attribute("name"));
	Util::toLowercase(&cBoundarySource, pElement->Attribute("source"));
	Util::toLowercase(&cBoundaryValue, pElement->Attribute("value"));
	Util::toLowercase(&cBoundaryRatio, pElement->Attribute("effectiveRunoffPercentage"));

	// Must have unique name for each boundary (will get autoname by default)
	this->sName = std::string(cBoundaryName);

	// Allow an effective runoff ratio to be specified
	if (cBoundaryRatio != NULL) {
		this->dRatio = boost::lexical_cast<double>(cBoundaryRatio) / 100.0;
		pManager->log->writeLine("Effective runoff ratio is set at " + toString(this->dRatio));
	}

	// Volume increase column represents...
	if (cBoundaryValue == NULL || strcmp(cBoundaryValue, "rain-intensity") == 0) {
		this->setValue(model::boundaries::uniformValues::kValueRainIntensity);
	} else if ( strcmp(cBoundaryValue, "loss-rate") == 0) {
		this->setValue(model::boundaries::uniformValues::kValueLossRate);
	} else {
		model::doError(
			"Unrecognised value for uniform timeseries file.",
			model::errorCodes::kLevelWarning
		);
	}

	// Timeseries file...
	CCSVDataset* pCSVFile = new CCSVDataset(
		sBoundarySourceDir + std::string(cBoundarySource)
		);
	if (pCSVFile->readFile())
	{
		if (pCSVFile->isReady())
			this->importTimeseries(pCSVFile);
	}
	else {
		model::doError(
			"Could not read a uniform boundary timeseries file.",
			model::errorCodes::kLevelWarning
			);
		delete pCSVFile;
		return false;
	}
	delete pCSVFile;

	return true;
}

/*
*	Import timeseries data from a CSV file
*/
void CBoundaryUniform::importTimeseries(CCSVDataset *pCSV)
{
	unsigned int uiIndex = 0;
	bool bInvalidEntries = false;
	bool bProcessedHeaders = false;

	// Is file ready?
	if (!pCSV->isReady())
		return;

	// Allocate memory
	this->pTimeseries = new sTimeseriesUniform[pCSV->getLength()];

	// Iterate over the items in the list
	for (vector<vector<std::string>>::const_iterator it = pCSV->begin();
		it != pCSV->end();
		it++)
	{
		// TODO: Check if there are headers first... first time should be zero
		if (uiIndex == 0 && !bProcessedHeaders)
		{
			bProcessedHeaders = true;
			continue;
		}

		if (it->size() == 2)
		{
			try
			{
				this->pTimeseries[uiIndex].dTime = boost::lexical_cast<cl_double>((*it)[0]);
				this->pTimeseries[uiIndex].dComponent = boost::lexical_cast<cl_double>((*it)[1]) * this->dRatio;
			}
			catch (boost::bad_lexical_cast)
			{
				bInvalidEntries = true;
			}
		}
		else {
			bInvalidEntries = true;
		}

		uiIndex++;
	}

	if (bInvalidEntries)
	{
		model::doError(
			"Some CSV entries were not valid for a boundary timeseries.",
			model::errorCodes::kLevelWarning
			);
	}

	// Need at least two entries
	if (uiIndex < 2)
	{
		model::doError(
			"A boundary timeseries is too short.",
			model::errorCodes::kLevelWarning
			);
		return;
	}

	// Calculate the interval
	this->dTimeseriesInterval = pTimeseries[1].dTime - pTimeseries[0].dTime;

	// Store the length of the timeseries
	this->uiTimeseriesLength = uiIndex;
	this->dTimeseriesLength = pTimeseries[uiIndex - 1].dTime;

	// Calculate the amount of mass in the timeseries
	this->dTotalVolume = 0.0;

	// TODO: Fix me... need to calculate volume but need to know number of cells in the domain
	/*
	for (unsigned int i = 0; i < this->uiTimeseriesLength - 1; ++i)
	{
		this->dTotalVolume += (pTimeseries[i + 1].dTime - pTimeseries[i].dTime) *
			( (pTimeseries[i + 1].dVolumeComponent + pTimeseries[i].dVolumeComponent) / 3600 / 1000 * NUMBER_OF_CELLS ) / 2;
		this->dTotalVolume += (pTimeseries[i + 1].dTime - pTimeseries[i].dTime) *
			(pTimeseries[i + 1].dDischargeComponentY + pTimeseries[i].dDischargeComponentY) / 2;
	}
	*/
}

void CBoundaryUniform::prepareBoundary(
		COCLDevice* pDevice,
		COCLProgram* pProgram,
		COCLBuffer* pBufferBed,
		COCLBuffer* pBufferManning,
		COCLBuffer* pBufferTime,
		COCLBuffer* pBufferTimeHydrological,
		COCLBuffer* pBufferTimestep
	)
{
	// Configuration for the boundary and timeseries data
	if (pProgram->getFloatForm() == model::floatPrecision::kSingle)
	{
		sConfigurationSP pConfiguration;

		pConfiguration.TimeseriesEntries = this->uiTimeseriesLength;
		pConfiguration.TimeseriesInterval = this->dTimeseriesInterval;
		pConfiguration.TimeseriesLength = this->dTimeseriesLength;
		pConfiguration.Definition = (cl_uint)this->ucValue;

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

		this->pBufferTimeseries = new COCLBuffer(
			"Bdy_" + this->sName + "_Series",
			pProgram,
			true,
			true,
			sizeof(cl_float2)* this->uiTimeseriesLength,
			true
		);
		cl_float2 *pTimeseries = this->pBufferTimeseries->getHostBlock<cl_float2*>();
		for (unsigned int i = 0; i < this->uiTimeseriesLength; ++i)
		{
			pTimeseries[i].s[0] = this->pTimeseries[i].dTime;
			pTimeseries[i].s[1] = this->pTimeseries[i].dComponent;
		}
	} else {
		sConfigurationDP pConfiguration;

		pConfiguration.TimeseriesEntries = this->uiTimeseriesLength;
		pConfiguration.TimeseriesInterval = this->dTimeseriesInterval;
		pConfiguration.TimeseriesLength = this->dTimeseriesLength;
		pConfiguration.Definition = (cl_uint)this->ucValue;

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

		this->pBufferTimeseries = new COCLBuffer(
			"Bdy_" + this->sName + "_Series",
			pProgram,
			true,
			true,
			sizeof(cl_double2)* this->uiTimeseriesLength,
			true
		);
		cl_double2 *pTimeseries = this->pBufferTimeseries->getHostBlock<cl_double2*>();
		for (unsigned int i = 0; i < this->uiTimeseriesLength; ++i)
		{
			pTimeseries[i].s[0] = this->pTimeseries[i].dTime;
			pTimeseries[i].s[1] = this->pTimeseries[i].dComponent;
		}
	}

	this->pBufferConfiguration->createBuffer();
	this->pBufferConfiguration->queueWriteAll();
	this->pBufferTimeseries->createBuffer();
	this->pBufferTimeseries->queueWriteAll();

	this->oclKernel = pProgram->getKernel("bdy_Uniform");
	COCLBuffer* aryArgsBdy[] = {
		pBufferConfiguration,
		pBufferTimeseries,
		pBufferTime,
		pBufferTimestep,
		pBufferTimeHydrological,
		NULL,	// Cell states
		pBufferBed,
		pBufferManning
	};
	this->oclKernel->assignArguments(aryArgsBdy);

	// TODO: Need a more sensible group size!
	CDomainCartesian* pDomain = static_cast<CDomainCartesian*>(this->pDomain);
	this->oclKernel->setGlobalSize(ceil(pDomain->getCols() / 8) * 8, ceil(pDomain->getRows() / 8) * 8);
	this->oclKernel->setGroupSize(8, 8);
}

void CBoundaryUniform::applyBoundary(COCLBuffer* pBufferCell)
{
	this->oclKernel->assignArgument(5, pBufferCell);
	this->oclKernel->scheduleExecution();
}

void CBoundaryUniform::streamBoundary(double dTime)
{
	// ...
}

void CBoundaryUniform::cleanBoundary()
{
	// ...
}
