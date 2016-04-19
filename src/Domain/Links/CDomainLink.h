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
#ifndef HIPIMS_DOMAIN_CDOMAINLINK_H_
#define HIPIMS_DOMAIN_CDOMAINLINK_H_

#include "../../OpenCL/opencl.h"
#include "../../OpenCL/Executors/COCLBuffer.h"
#include "../CDomainBase.h"
#include <vector>

/*
 *  DOMAIN CLASS
 *  CDomainLink
 *
 *  Handles links between two domains, which may or may not reside on the same host system
 *  and may be of differing types.
 */
class CDomainLink
{
	public:

		CDomainLink( CDomainBase*, CDomainBase* );													// Constructor
		~CDomainLink(void);																			// Destructor

		// Public structures
		struct mpiSignalDataDomain
		{
			int				iSignalCode;
			unsigned int 	uiSourceDomainID;
			unsigned int	uiTargetDomainID;
			double			dValidityTime;
			unsigned int	uiDataSize;
		};
		
		// Public variables
		// ...

		// Public functions
		static bool			canLink(CDomainBase*, CDomainBase*);									// Can two domains be linked?
		void				pullFromBuffer(double, COCLBuffer*);									// Download data from a memory buffer
		void				pushToBuffer(COCLBuffer*);												// Push data to memory buffer
		unsigned int		getSmallestOverlap()					{ return uiSmallestOverlap;  }	// Get the smallest overlap size
		void				markInvalid()							{ dValidityTime = -1.0;  }		// Mark the data as invalid
		bool				isAtTime( double );														// Is this link at the given time?
		unsigned int		getSourceDomainID()						{ return uiSourceDomainID; }	// Fetch the source domain ID number
		unsigned int		getTargetDomainID()						{ return uiTargetDomainID; }	// Fetch the target domain ID number
		void				pullFromMPI(double, char*);												// Fetch data received via MPI
		bool				sendOverMPI();															// Send this domain data over MPI if needed

	protected:

		// None

	private:

		// Private structures
		struct LinkDefinition
		{
			unsigned long ulSourceStartCellID;
			unsigned long ulSourceEndCellID;
			unsigned long ulTargetStartCellID;
			unsigned long ulTargetEndCellID;
			unsigned long ulSize;
			unsigned long ulOffsetSource;
			unsigned long ulOffsetTarget;
			void*		  vStateData;
		};

		// Private variables
		std::vector<LinkDefinition>		linkDefs;
		unsigned int					uiSourceDomainID;
		unsigned int					uiTargetDomainID;
		int								iTargetNodeID;
		unsigned int					uiSmallestOverlap;
		double							dValidityTime;
		bool							bSent;

		// Private functions
		void				generateDefinitions(CDomainBase*, CDomainBase*);						// Identify contiguous memory areas for exchange
};

#endif
