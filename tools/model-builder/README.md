# HiPIMS Model Builder

This tool is designed to build models compatible with HiPIMS. It's a good place to start if you're building a new model from scratch, from which you can then clean up the data sources or change the boundary conditions to suit.

Requires [NodeJS 5 or later](https://nodejs.org/en/download/current/) to be installed. Depends on GDAL and a handful of other libraries to manipulate data. In a NodeJS command prompt, you should run these commands to get up and running...
````
npm install
npm link
hipims-mb --help
````

## Command-line options
Currently only pluvial (surface water flooding) and numerical test case (analytical or laboratory-scale) models are supported, and options such as resampling the data to a different resolution are not available for LiDAR data.

````
  Usage: hipims-mb [options]

  Options:

    -h, --help                                   output usage information
    -V, --version                                output the version number
    -n, --name <name>                            short name for the model
    -s, --source [pluvial|fluvial|tidal|...]     type of model to construct
    -d, --directory <dir>                        target directory for model
    -ns, --scheme [godunov|muscl-hancock]        numerical scheme to apply
    -r, --resolution <resolution>                grid resolution in metres
    -mc, --manning <coefficient>                 Manning loss coefficient
    -t, --time <duration>                        duration of simulation
    -of, --output-frequency <frequency>          raster output frequency
    -dn, --decompose <domains>                   decompose for multi-device
    -do, --decompose-overlap <rows>              rows overlapping per divide
    -dm, --decompose-method [timestep|forecast]  synchronisation method
    -dt, --decompose-forecast-target <X%>        spare buffer for forecast
    -ll, --lower-left <easting,northing>         lower left coordinates
    -ur, --upper-right <easting,northing>        upper right coordinates
    -w, --width <Xm>                             domain width
    -h, --height <Xm>                            domain height
    -ri, --rainfall-intensity <Xmm/hr>           rainfall intensity
    -rd, --rainfall-duration <Xmins>             rainfall duration
    -dr, --drainage <Xmm/hr>                     drainage rate
    -c, --constants <a=X,b=Y>                    override underlying constants
````

## Example for surface water flooding
This command will create a pluvial (surface water flooding only) model for part of the centre of Newcastle upon Tyne. The total duration of the simulation will be three hours, during which there will be 80mm/hr of rainfall for the first hour.

Coordinates for the lower left and upper right of the domain are metres in OSGB36. If you need to find coordinates easily you can use [this site](http://www.nearby.org.uk/coord.cgi?p=NE6+1TX&f=full) with postcodes or latitude and longitude. For example the postcode NE1 7RU has the coordinates 424693,565147 which you can convert into a box by adding and subtracting a suitable distance in metres.

The drainage rate of 12mm/hr is consistent with the Environment Agency's surface water flood risk analyses, and is a good approximation for the true capacity of an urban drainage network to remove water from the surface.
````
hipims-mb --name="Newcastle-upon-Tyne Centre" 
          --source=pluvial 
          --time="3 hours" 
          --output-frequency="15 minutes" 
          --directory=models/newcastle-centre 
          --lower-left=423417,564273 
          --upper-right=426189,565719 
          --rainfall-intensity=80mm/hr 
          --rainfall-duration=60mins 
          --drainage=12mm/hr
````

This will automatically download Environment Agency LiDAR files and create a single DEM file. **You would normally do further work to clean the DEM, and select a more hydrologically-sound extent**. It would be normal to remove trees, bridges and overpasses from the DEM file before running a simulation. The current version of the model builder cannot yet do this, but a future version will.

A HiPIMS configuration file will be created in the directory, along with a domain topography from LiDAR data. There may be gaps in the LiDAR data, where cells will not be computed in the model, and this could affect your results if an upstream source area is omitted. 

The simulation can then be loaded by the HiPIMS engine from a prompt, specifying the right directory and XML file. You can manually edit the XML file if required, such as specifying a device ID to use.

````
hipims --config-file=models/newcastle-centre/simulation.xml
````

![Simulation results for Newcastle](/docs/hipims-newcastle-example.jpg?raw=true "Simulation results for Newcastle")

You can load the topography and simulation results in a free GIS program such as [QGIS](http://www.qgis.org/en/site/).

The above image shows the problems caused by bridges, tunnels and culverts. On the university campus for example, flooding is exaggerated by the absence of a flow path where in reality there are paths underneath buildings, and by not representing a culverted river used as a sewer.

With a reasonably modern GPU in your system, you can expect the simulation to take less than an hour to complete. Running on a CPU only is likely to take several hours.

## Domain decomposition

If you plan to run an extremely large simulation, there's a good chance you'll need to decompose the domain over multiple processing devices, either to avoid exhausting the memory on a single device, or to achieve the performance you require.

The model builder can divide the domain automatically if you provide some additional options.

* **--decompose=_N_** specifies how many times the domain should be divided, which is normally the same as the number of processing devices available in the system
* **--decompose-overlap=_N_** defines the number of overlapping rows, which are required for synchronisation of data between the domains, but too many overlapping rows will result in redundant computation
* **--decompose-method=_timestep|forecast_** specifies how the synchronisation is performend, which can either exchange the timestep between the domain at every iteration, or attempt to forecast an appropriate point in the future to synchronise at
* **--decompose-forecast-target=_N%_** is required when using forecast synchronisation, and specifies how many of the rows should aim to be retained as 'spare' capacity, because in this mode if the synchronisation point is not reached within the number of iterations achievable with the overlap size, then the simulation must reverse itself and try again

Generally speaking, timestep synchronisation is the most reliable method but requires frequent host bus transfers, so is unlikely to deliver huge performance benefits. Forecast mode synchronisation is preferable, but you could run into problems in simulations where the timesteps frequently vary, making it difficult to forecast an appropriate point for synchronisation.

````
hipims-mb --name="Newcastle-upon-Tyne Centre"
          --source=pluvial 
          --time="3 hours" 
          --output-frequency="15 minutes" 
          --directory=models/newcastle-centre 
          --lower-left=423417,564273 
          --upper-right=426189,565719 
          --rainfall-intensity=80mm/hr 
          --rainfall-duration=60mins 
          --manning=0.02
          --drainage=12mm/hr
          --decompose=2
          --decompose-method=forecast
          --decompose-overlap=50
          --decompose-forecast-target=50%
````

As an example, if using forecast synchronisation with an overlap of 100 rows, then each domain must achieve the forecasted synchronisation point within 49 iterations, otherwise the errors could not be corrected for. Setting a 50% target will make the simulation aim to keep half of these rows spare, because future timesteps are dependent on the hydrodynamics at that point, and could vary wildly.

**All domain decomposition functionality is a work in progress**, and the methods have yet to be peer reviewed and published.

## Example for numerical tests
A suite of numerical test cases are being developed, which can be used as part of the development process for automated testing. These test cases also form the basis for some of the published work detailing HiPIMS, such as providing evidence that the domain decomposition is not adversely affecting the quality of the results.

An example for a sloshing parabolic bowl, where the analytical solution is known for comparison, would be...
````
hipims-mb --name="Sloshing Parabolic Bowl"
          --source=analytical
          --directory="models/sloshing-bowl"
          --resolution=10
          --time=3600s
          --output-frequency=300s
          --manning=0.00
          --scheme=muscl-hancock
          --width=10000
          --height=10000
````

The support directory contains the analytical solutions at each output interval for comparison. Further details are provided [here](tests/).

## Further developments

This is a rewrite of a tool used internally at Newcastle University, which built models using data from OS MasterMap, Met Office radar data, and Environment Agency LiDAR. 

The original tool was built upon the ArcGIS engine using ArcPy scripts. When complete, this version of the model builder will only use open data sources (e.g. OS OpenMap Local), and open libraries for manipulating data (e.g. GDAL).

Functions to clean topography will be added, such as to remove bridges and trees using data in vector datasets. The modelling software itself will soon be able to simulate flows at different levels, such as flooding in a subway while also simulating the flow of water on the road above.


