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
 *  Domain Class
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_DOMAIN_CDOMAINBASE_H_
#define HIPIMS_DOMAIN_CDOMAINBASE_H_

#include <vector>

#include "../OpenCL/opencl.h"

namespace model {

// Model domain structure types
namespace domainStructureTypes{ enum domainStructureTypes {
	kStructureCartesian						= 0,	// Cartesian
	kStructureRemote						= 1,	// Remotely held domain
	kStructureInvalid						= 255	// Error state, cannot work with this type of domain
}; }

}

/*
 *  DOMAIN BASE CLASS
 *  CDomainBase
 *
 *  Core base class for domains, even ones which do not reside on this system.
 */
class CDomain;
class CDomainLink;
class CDomainBase
{

	public:

		CDomainBase(void);																			// Constructor
		~CDomainBase(void);																			// Destructor

		// Public structures
		struct DomainSummary
		{
			bool			bAuthoritative;
			unsigned int	uiDomainID;
			unsigned int	uiNodeID;
			unsigned int	uiLocalDeviceID;
			double			dEdgeNorth;
			double			dEdgeEast;
			double			dEdgeSouth;
			double			dEdgeWest;
			double			dResolution;
			unsigned long	ulRowCount;
			unsigned long	ulColCount;
			unsigned char	ucFloatPrecision;
		};

		struct mpiSignalDataProgress
		{
			unsigned int 	uiDomainID;
			double			dCurrentTimestep;
			double			dCurrentTime;
			double			dBatchTimesteps;
			cl_uint			uiBatchSkipped;
			cl_uint			uiBatchSuccessful;
			unsigned int	uiBatchSize;
		};

		// Public variables
		// ...

		// Public functions
		static		CDomainBase*	createDomain(unsigned char);									// Create a new domain of the specified type
		virtual		DomainSummary	getSummary();													// Fetch summary information for this domain
		virtual 	bool			configureDomain( XMLElement* );									// Configure a domain, loading data etc.
		virtual		bool			isRemote()				{ return true; };						// Is this domain on this node?
		virtual		unsigned char	getType()				{ return model::domainStructureTypes::kStructureInvalid; };	// Fetch a type code

		bool						isInitialised();												// X Is the domain ready to be used
		unsigned	long			getCellCount();													// X Return the total number of cells
		bool						isPrepared()			{ return bPrepared; }					// Is the domain prepared?
		unsigned int				getRollbackLimit()		{ return uiRollbackLimit; }				// How many iterations before a rollback is required?
		unsigned int				getID()					{ return uiID; }						// Get the ID number
		void						setID( unsigned int i ) { uiID = i; }							// Set the ID number
		unsigned int				getLinkCount()			{ return links.size(); }				// Get the number of links in this domain
		unsigned int				getDependentLinkCount()	{ return dependentLinks.size(); }		// Get the number of dependent links
		CDomainLink*				getLink(unsigned int i) { return links[i]; };					// Fetch a link
		CDomainLink*				getDependentLink(unsigned int i) { return dependentLinks[i]; };	// Fetch a dependent link
		CDomainLink*				getLinkFrom(unsigned int);										// Fetch a link with a specific domain
		bool						sendLinkData();													// Send link data to other nodes
		bool						isLinkSetAtTime( double );										// Are all this domain's links at the specified time?
		void						clearLinks()			{ links.clear(); dependentLinks.clear(); }	// Remove pre-existing links
		void						addLink(CDomainLink*);											// Add a new link to another domain
		void						addDependentLink(CDomainLink*);									// Add a new dependent link to another domain
		void						markLinkStatesInvalid();										// When a rollback is initiated, link data becomes invalid
		void						setRollbackLimit();												// Automatically identify a rollback limit
		void						setRollbackLimit( unsigned int i ) { uiRollbackLimit = i; }		// Set the number of iterations before a rollback is required
		virtual unsigned long		getCellID(unsigned long, unsigned long);						// Get the cell ID using an X and Y index
		virtual mpiSignalDataProgress getDataProgress()		{ return pDataProgress; };				// Fetch some data on this domain's progress
		virtual void 				setDataProgress( mpiSignalDataProgress a )	{ pDataProgress = a; };	// Set some data on this domain's progress

	protected:

		// Private variables
		bool				bPrepared;																// Is the domain prepared?
		unsigned int		uiID;																	// Domain ID
		unsigned int		uiRollbackLimit;														// Iterations after which a rollback is required
		unsigned long		ulCellCount;															// Total number of cells
		mpiSignalDataProgress pDataProgress;														// Data on this domain's progress
		std::vector<CDomainLink*>	links;															// Vector of domain links
		std::vector<CDomainLink*>	dependentLinks;													// Vector of dependent domain links

};

#endif
