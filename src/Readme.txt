--------------------------------------------
HYDRAULIC FRAMEWORK FOR PARALLEL EXECUTION
--------------------------------------------
Project name:	HiPIMS
--------------------------------------------
Luke S. Smith
School of Civil Engineering & Geosciences
Newcastle University
--------------------------------------------

This project requires libaries to compile successfully.

 1. OpenCL using any of the below installations:

    NVIDIA CUDA Toolkit
    AMD Accelerated Parallel Processing Toolkit

    This must be installed and referenced in the project libraries and includes.

 2. Boost
    
    Required for regular expressions, lexical casts, and other utility functionality.

 3. GDAL
 
    Only 32-bit makefiles are supplied. Library must be built and the 'gcore' and
	'port' directories added to the include directories.
	
	Compiler flags need to be modified from "/MD" to "/MDd" for a debug build, and 
	"DEBUG=1" appended to the make invocation command to create a library compatible
	with debug builds of the model.
	
Luke S. Smith
April 2012	
