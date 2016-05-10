# HiPIMS Model Builder

Requires NodeJS >=5 to be installed. Depends on GDAL and a handful of other libraries to manipulate data. In a NodeJS command prompt, you should run these commands to get up and running...
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

    -h, --help                                     output usage information
    -V, --version                                  output the version number
    -n, --name <name>                              short name for the model
    -s, --source [pluvial|fluvial|tidal|combined]  type of model to construct
    -d, --directory <dir>                          target directory for model
    -r, --resolution <resolution>                  grid resolution in metres
    -t, --time <duration>                          duration of simulation
    -dn, --decompose <domains>                     decompose for multi-device
    -ll, --lower-left <easting,northing>           lower left coordinates
    -ur, --upper-right <easting,northing>          upper right coordinates
    -ri, --rainfall-intensity <Xmm/hr>             rainfall intensity
    -rd, --rainfall-duration <Xmins>               rainfall duration
    -dr, --drainage <Xmm/hr>                       drainage rate
````

## Example
This command will create a pluvial (surface water flooding only) model for Canvey Island near the Thames. The total duration of the simualtion will be three hours, during which there will be 80mm/hr of rainfall for the first hour.

Coordinates for the lower left and upper right of the domain are metres in OSGB36. If you need to find coordinates easily you can use [this site](http://www.nearby.org.uk/coord.cgi?p=NE6+1TX&f=full) with postcodes or latitude and longitude. For example the postcode NE1 7RU has the coordinates 424693,565147 which you can convert into a box by adding and subtracting a suitable distance in metres.

The drainage rate of 12mm/hr is consistent with the Environment Agency's surface water flood risk analyses, and is a good approximation for the true capacity of an urban drainage network to remove water from the surface.
````
hipims-mb --name="Newcastle-upon-Tyne Centre" ^
    --source=pluvial ^
    --time="3 hours" ^
    --directory=models/newcastle-centre ^
    --lower-left=423417,562837 ^
    --upper-right=426189,565719 ^
    --rainfall-intensity=80mm/hr ^
    --rainfall-duration=60mins ^
    --drainage=12mm/hr
````

A HiPIMS configuration file will be created in the directory, along with a domain topography from LiDAR data. There may be gaps in the LiDAR data, where cells will not be computed in the model, and this could affect your results if an upstream source area is omitted. 

The simulation can then be loaded by the HiPIMS engine from a prompt, specifying the right directory and XML file. You can manually edit the XML file if required, such as specifying a device ID to use.

````
hipims --config-file=models/canvey-island/simulation.xml
````
