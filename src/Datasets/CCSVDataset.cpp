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
 *  CSV handling class
 * ------------------------------------------
 *
 */
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

#include "../common.h"
#include "CCSVDataset.h"

/* 
 *  Constructor
 */
CCSVDataset::CCSVDataset(
		std::string		sCSVFilename
	)
{
	sFilename	= sCSVFilename;
	bReadFile	= false;
}

/*
 *  Destructor
 */
CCSVDataset::~CCSVDataset()
{
	// ...
}

/*
 *  Read the CSV file using Boost's tokenizer stuff
 */
bool CCSVDataset::readFile()
{
	typedef boost::tokenizer<boost::escaped_list_separator<char>>	Tokenizer;

	std::string sRow;
	std::ifstream ifsCSV( this->sFilename, std::ios::in );

	if ( !ifsCSV.is_open() )
	{
		model::doError(
			"Could not open a CSV file.",
			model::errorCodes::kLevelWarning
		);
		return false;
	}

	vContents.clear();

	while ( ifsCSV.good() )
	{
		getline( ifsCSV, sRow );

		Tokenizer				tRow(sRow);
		std::vector<std::string>		vTokens;

		vTokens.assign( tRow.begin(), tRow.end() );

		for( unsigned int i = 0; i < vTokens.size(); i++ )
			vTokens[i] = boost::algorithm::trim_copy( vTokens[i] );

		if ( strcmp( sRow.c_str(), "" ) != 0 )
			vContents.push_back( vTokens );
	}

	ifsCSV.close();
	bReadFile = true;

	return true;
}
