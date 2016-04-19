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
 *  Domain stump for remote domains
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_DOMAIN_CDOMAINREMOTE_H_
#define HIPIMS_DOMAIN_CDOMAINREMOTE_H_

#include "../../OpenCL/opencl.h"
#include "../CDomainBase.h"

/*
 *  DOMAIN CLASS
 *  CDomainRemote
 *
 *  Stores relevant details for a domain which resides elsewhere in the MPI comm.
 */
class CDomainRemote : public CDomainBase
{
	public:

		CDomainRemote(void);																// Constructor
		~CDomainRemote(void);																// Destructor

		// Public variables
		// ...

		// Public functions
		virtual	unsigned char				getType()				{ return model::domainStructureTypes::kStructureRemote; };	// Fetch a type code
		virtual	bool						isRemote()				{ return true; };		// Is this domain on this node?
		virtual	CDomainBase::DomainSummary	getSummary();									// Fetch summary information for this domain
		void								setSummary(CDomainBase::DomainSummary a) { pSummary = a; };

	protected:

		// None

	private:

		CDomainBase::DomainSummary			pSummary;										// Domain summary info received from the remote node
};

#endif
