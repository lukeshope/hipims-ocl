# HiPIMS Model Builder

This tool is designed to build models compatible with HiPIMS. It's a good place to start if you're building a new model from scratch, from which you can then clean up the data sources or change the boundary conditions to suit.

Requires [NodeJS 5 or later](https://nodejs.org/en/download/current/) to be installed. Depends on GDAL and a handful of other libraries to manipulate data. In a NodeJS command prompt, you should run these commands to get up and running...
````
npm install
npm link
hipims-mb --help
````

## Command-line options
Currently only pluvial (surface water flooding) models are supported, and options such as resampling the data to a different resolution are not available.

````
  Usage: hipims-mb [options]

  Options:

  Options:

    -h, --help                                     output usage information
    -V, --version                                  output the version number
    -n, --name <name>                              short name for the model
    -s, --source [pluvial|fluvial|tidal|combined]  type of model to construct
    -d, --directory <dir>                          target directory for model
    -r, --resolution <resolution>                  grid resolution in metres
    -t, --time <duration>                          duration of simulation
    -of, --output-frequency <frequency>            raster output frequency
    -dn, --decompose <domains>                     decompose for multi-device
    -ll, --lower-left <easting,northing>           lower left coordinates
    -ur, --upper-right <easting,northing>          upper right coordinates
    -ri, --rainfall-intensity <Xmm/hr>             rainfall intensity
    -rd, --rainfall-duration <Xmins>               rainfall duration
    -dr, --drainage <Xmm/hr>                       drainage rate
````

## Example
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

## Further developments

This is a rewrite of a tool used internally at Newcastle University, which built models using data from OS MasterMap, Met Office radar data, and Environment Agency LiDAR. 

This was built upon the ArcGIS engine using ArcPy scripts. When complete, this version of the model builder will only use open data sources (e.g. OS OpenMap Local), and open libraries for manipulating data (e.g. GDAL).

Functions to clean topography will be added, such as to remove bridges and trees using data in vector datasets. The modelling software itself will soon be able to simulate flows at different levels, such as flooding in a subway while also simulating the flow of water on the road above.


