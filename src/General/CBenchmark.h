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
 *  Benchmark class and structures for time-tracking
 * ------------------------------------------
 *
 */
#ifndef HIPIMS_GENERAL_CBENCHMARK_H_
#define HIPIMS_GENERAL_CBENCHMARK_H_

class CBenchmark
{

	public:

		CBenchmark( bool );									// Constructor
		~CBenchmark( void );								// Destructor

		// Public variables
		// ...

		// Public structures
		struct sPerformanceMetrics							// Performance return values
		{
			double dMilliseconds;
			double dSeconds;
			double dHours;
			double sTime;
		};

		// Public functions
		void					start( void );				// Start counting
		void					finish( void );				// End counting
		bool					isRunning( void ) { return bRunning; }		// Is it counting?
		sPerformanceMetrics*	getMetrics( void );			// Fetch the results

	private:

		// Private variables
		bool					bRunning;					// Is the timer currently running?
		double					dStartTime;					// Start of counting
		double					dEndTime;					// End of counting
		sPerformanceMetrics		sMetrics;					// The most recent metrics

		// Private functions
		double	getCurrentTime();							// Fetch the time in seconds from the CPU

};

#endif
