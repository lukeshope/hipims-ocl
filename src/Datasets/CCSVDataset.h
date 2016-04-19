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
 *  CSV time series handling class
 * ------------------------------------------
 *
 */

#ifndef HIPIMS_DATASETS_CCSVDATASET_H_
#define HIPIMS_DATASETS_CCSVDATASET_H_

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iterator>

/*
 *  CSV DATASET CLASS
 *  CCSVDataset
 *
 *  Provides access for reading CSV files.
 */
class CCSVDataset
{
public:
	CCSVDataset( std::string sCSVFilename );
	~CCSVDataset();

	bool													readFile();
	std::vector<std::vector<std::string>>::const_iterator	begin()				{ return vContents.begin(); }
	std::vector<std::vector<std::string>>::const_iterator	end()				{ return vContents.end(); }
	unsigned int											getLength()			{ return vContents.size() - 1; }
	bool													isReady()			{ return bReadFile; }

private:
	std::string												sFilename;
	std::ifstream											ifsFile;
	std::vector<std::vector<std::string>>					vContents;
	bool													bReadFile;
};

#endif
