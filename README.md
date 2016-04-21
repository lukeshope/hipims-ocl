# HiPIMS OpenCL
The **high-performance integrated modelling system**, for hydraulic and hydrological simulations (HiPIMS), is a suite of tools designed to run on a range of modern computing platforms including CPUs, GPUs and distributed systems (MPI).

This software was created for use in academic research. It is now licensed under the GNU General Public Licence (GPL) allowing you to use the code and further develop it. We stress that is trivial to use any hydraulic modelling software to create a model that is in no way representative of the real world, and that you should validate any models against real-world observations.

* Three **numerical schemes** intended for different purposes
    * A **first-order finite-volume Godunov-type scheme**, which can capture transient flow conditions including phonomena such as [hydraulic jumps](https://en.wikipedia.org/wiki/Hydraulic_jump), making it appropriate for urban pluvial and fluvial flood modelling
    * A second-order **[MUSCL](https://en.wikipedia.org/wiki/MUSCL_scheme)-Hancock** extension to the above scheme, used to counteract numerical diffusion while remaining TVD
    * A simple implementation of a partial inertial scheme which does not provide the above benefits but is easier to compute and may offer small performance benefits accordingly
* An implementation of the [**HLLC**](https://en.wikipedia.org/wiki/Riemann_solver) approximate **Riemann solver**, and the MINMOD slope limiter, but it is relatively trivial to add further solvers and limiters if required.
* Support for a range of **boundary conditions**, such as
    * Time-varying spatially-uniform precipitation
    * Spatially- and time-varying precipitation, used for radar rainfall input or for volume losses such as higher infiltration in grassland
    * Directly introduced volume, to simulate surcharging sewers
    * Imposed depth and discharge across an area, normally used to simulate upstream river flows or downstream tidal flows
* A two-stage reduction algorithm to efficiently calculate the next timestep according to the [CFL condition](https://en.wikipedia.org/wiki/Courant%E2%80%93Friedrichs%E2%80%93Lewy_condition). 
* Experimental support for **domain decomposition** across multiple CPU/GPUs or multiple discrete systems using [MPI](https://en.wikipedia.org/wiki/Message_Passing_Interface)
    * Timesteps can be explicitly synchronised between the different domains, making reassembly of the results relatively easy
    * Timesteps can also be made independent in each separate domain, which uses a forecasting method to determine an appropriate point to synchronise data between domains (_this is experimental!_)
    * Overlapping domains are processed automatically to determine the constraints they place on the simulation (some overlap is required for exchanging information)

## Download
You probably want the HiPIMS [binaries](/dist/hipims-win32-master.zip) for Windows, and perhaps the [tests](/dist/hipims-test-master.zip) to give you an idea of what a configuration file looks like.

Your system will need the [Visual Studio 2013 runtime](https://www.microsoft.com/en-us/download/details.aspx?id=40784), but it is quite likely you already have this.

There is no graphical interface supplied with HiPIMS; this is a console-only application. We did start developing an interface, but as most of our applications ran on high-performance servers without a graphical interface, it never reached maturity. 

![HiPIMS console view](/docs/hipims-console-win.png?raw=true "HiPIMS Console View")
	
## Use
**You will need a system which can run [OpenCL](https://en.wikipedia.org/wiki/OpenCL)**. This includes most modern Intel CPUs and APUs, AMD GPUs and CPUs, and NVIDIA GPUs. Your graphics card drivers may already include the OpenCL runtime. If you do not already have an OpenCL runtime, you should try downloading:
* [Intel OpenCL runtime](https://software.intel.com/en-us/articles/opencl-drivers)
* [AMD APP SDK](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/) which includes a CPU runtime compatible with Intel CPUs

You may also find that some overzealous anti-virus software blocks OpenCL code from running, because some runtimes will compile code on the fly and run it from the temporary directory.

### Model configuration
Models are defined using an XML file similar to the one below, and using CSV and GIS formats to define the model domain. GDAL is used for reading and writing all spatial data, with available formats and their codes [listed here](http://www.gdal.org/formats_list.html).

All paths in the configuration are relative to the XML file.

````xml
<?xml version="1.0"?>
<!DOCTYPE configuration PUBLIC "HiPIMS Configuration Schema 1.1" "http://www.lukesmith.org.uk/research/namespace/hipims/1.1/"[]>
<configuration>
	<metadata>
		<name>A model</name>
		<description>A dead good model.</description>
	</metadata>
	<execution>
		<executor name="OpenCL">
			<parameter name="deviceFilter" value="GPU" />
		</executor>
	</execution>
	<simulation>
		<parameter name="duration" value="7200" />
		<parameter name="outputFrequency" value="600" />
		<parameter name="floatingPointPrecision" value="double" />
		<domainSet>
			<domain type="cartesian" deviceNumber="1">
				<data sourceDir="newcastle-centre/topography/" 
					  targetDir="newcastle-centre/output/">
					<dataSource type="constant" value="manningCoefficient" source="0.030" />
					<dataSource type="raster" value="structure,dem" source="NewcastleCentreDEM_2m.img" />
					<dataTarget type="raster" value="depth" format="HFA" target="depth_%t.img" />
				</data>
				<scheme name="Godunov">
					<parameter name="courantNumber" value="0.50" />
					<parameter name="groupSize" value="32x8" />
				</scheme>
				<boundaryConditions sourceDir="newcastle-centre/">
					<domainEdge edge="north" treatment="closed" />
					<domainEdge edge="south" treatment="closed" />
					<domainEdge edge="east" treatment="closed" />
					<domainEdge edge="west" treatment="closed" />
					<timeseries type="atmospheric" 
								name="Drainage" 
								value="loss-rate" 
								source="boundaries/drainage.csv" />
					<timeseries type="atmospheric" 
								name="Rainfall" 
								value="rain-intensity" 
								source="boundaries/rainfall.csv" />
				</boundaryConditions>
			</domain>
		</domainSet>
    </simulation>
</configuration>
````

### Command-line arguments
Arguments are mostly the same for the Linux and Windows builds of HiPIMS. Short form arguments have a space preceding the value, while the long form uses an equals sign.

| Short form | Long form | Description | Default |
| --- | --- | --- | --- |
| `-c` | `--config-file=`_..._ | XML file to use for the model configuration. | _Required_ |
| `-l` | `--log-file=`_..._ | Log file which mirrors the console output. | _model.log |
| `-s` | `--quiet-mode` | Disable any points where user feedback is required, such as on model completion. | false |
| `-n` | `--disable-screen` | On Linux, disables NCurses for console output. | false |
| `-m` | `--mpi-mode` | Forces only first MPI instance to output to the console. | false |
| `-x` | `--code-dir=`_..._ | On Linux, sets base directory for OpenCL code files. | Binary path |

## Building from source
HiPIMS has a number of dependencies you need to provide first. 

* [Boost](http://www.boost.org/) library
   * Regex
   * Filesystem
   * Lexical cast
   * Unordered map
   * Tokenizer
   * Date and time
* At least one of these OpenCL SDKs
    * [Intel OpenCL SDK](https://software.intel.com/en-us/intel-opencl)
    * [NVIDIA CUDA SDK](https://developer.nvidia.com/cuda-downloads)
    * [AMD APP SDK](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/)
* [GDAL](http://www.gdal.org/) library
* On Linux only, [NCurses](https://www.gnu.org/software/ncurses/)

A Visual Studio 2013 project is provided for building on Windows, and a Makefile for Linux. You should launch the VS project using the supplied `open-vs` batch file, after modifying it to provide the paths to Boost and GDAL. Other dependency paths should be determined from environment variables set during their install.

On Linux, a simple make command should suffice to build the binaries once the dependencies are installed.

## Test cases

These are a work in progress, because we do not own the copyright for the data used in many of the test cases HiPIMS was developed using. See the `tests` folder for information. We will provide new test cases using open data.

## Detailed information

Information on the underlying numerical schemes and previous applications was provided in journal publications. Where possible, these publications are provided in LaTeX format in `docs/papers`.

* Liang Q, Smith LS (2014) [A High-Performance Integrated Hydrodynamic Modelling System for Urban Flood Inundation](/docs/papers/urban-flood-jhi.pdf), _Journal of Hydroinformatics_, 17(4):518-533.
* Smith LS, Liang Q, Quinn PF (2015) [Towards a hydrodynamic modelling framework appropriate for applications in urban flood assessment and mitigation using heterogeneous computing](/docs/papers/carlisle-uwj.pdf), _Urban Water Journal_, 12(1):67-78.
* Smith LS, Liang Q (2013) [Towards a generalised GPU/CPU shallow-flow modelling tool](/docs/papers/dam-break-cf.pdf), _Computers & Fluids_, 88:334-343.
* Liang Q (2010) [Flood Simulation Using a Well-Balanced Shallow Flow Model](http://dx.doi.org/10.1061/(ASCE)HY.1943-7900.0000219), 136(9):669-675.
