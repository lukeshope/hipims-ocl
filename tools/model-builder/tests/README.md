# Numerical tests

Ensuring the underlying mathematics of the software provide the right results is no easy task. We typically compare results against analytical test cases, or laboratory-scale studies to assess how well the software's numerical methods perform.

Open versions of some of these test cases will be provided here, providing the basis for an automated test suite. 

## Sloshing parabolic bowl
This test is implemented without friction, as the Manning coefficient cannot easily be converted into a bed friction parameter as per the analytical solution. Expect some deviation from the analytical solution as a consequence of numerical diffusion, which should be reduced using the second-order MUSCL-Hancock scheme.

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
          --constants="a=3000,h0=10,B=5"
````
Constant values are used to define the initial conditions and parabolic shape.

* **a** is used to scale the parabolic bowl, and represents the distance to shoreline
* **B** defines the initial flow velocity
* **h0** is the water depth at the domain centre and used to scale the bowl dimensions

Details of the test can be found in Wang Y., Liang Q., Kesserwani G., Hall J.W. (2011) [A 2D shallow flow model for practical dam-break simulations](http://dx.doi.org/10.1080/00221686.2011.566248), _Journal of Hydraulic Research_, 49(3):307-316.

Support files created for the test case are:
* **Validation depth** expected at each output interval, therefore showing the location of the oscillation

## Lake at rest
Used to test the well-balanced property of the numerical scheme. No change in water level is expected.

````
hipims-mb --name="Lake at rest"
          --source=analytical
          --directory="models/lake-at-rest"
          --resolution=8
          --time="10 minutes"
          --output-frequency="10 minutes"
          --manning=0.00
          --scheme=godunov
          --width=1000
          --height=1000
          --constants="a=2000,b=5000,n=0.0,i=100,s=50"
````

Constant values define the water level, and the shape and height of an island in the middle of the domain.

* **a** is used to vertically scale the island
* **b** defines the scale of the island in terms of the shape
* **n** is the water level
* **i** is the maximum height of the island in the centre
* **s** is maximum depth of the sea surrounding the island, and thus relative to the water level

This test is adapted from a one-dimensional example used in Xing Y., Zhang X., Shu C. (2010) [Positivity-preserving high order well-balanced discontinuous Galerkin methods for shallow water equations](http://dx.doi.org/10.1016/j.advwatres.2010.08.005), _Advances in Water Resources_, 33:1476-1493.

Support files created for the test case are:
* **Validation depth** at the output intervals, which should be the same as the initial depth

## Dam-break flow against an isolated obstacle
This test is a work in progress.

## Dam-break over an emerging bed
Used to test the software's ability to capture shocks and deal with a moving wet-dry front. Expect a difference between the analytically-derived front location, and the simulation, regardless of the numerical scheme used. The second-order accurate MUSCL-Hancock scheme employed in HiPIMS will fall back to providing a first-order solution at wet-dry fronts, therefore has limited effect in this test case.

![Example results for dam break over an emerging bed](/tools/model-builder/tests/dam-break-emerging-bed-example.png?raw=true "Example results for dam break over an emerging bed")

````
hipims-mb --name="Dam break over an emerging bed"
          --source=analytical
          --directory="models/dam-break-over-emerging-bed"
          --resolution=0.05
          --time="2 seconds"
          --output-frequency="0.1 seconds"
          --manning=0.00
          --scheme=muscl-hancock
          --width=30
          --height=0.35
          --constants="w=2.0,a=0.0523598,n=1.0,p=0.0"
````

Constant values define the dam location, depth of water behind the dam, height of the wall around the domain, etc.

* **w** is used to define the wall height around the domain, which will have a width of two pixels
* **a** defines the bed slope, and is by default pi divided by 60, as per the published results
* **n** is the water level behind the dam
* **p** is the location of the dam along the x-axis

This test is adapted from an example used in Xing Y., Zhang X., Shu C. (2010) [Positivity-preserving high order well-balanced discontinuous Galerkin methods for shallow water equations](http://dx.doi.org/10.1016/j.advwatres.2010.08.005), _Advances in Water Resources_, 33:1476-1493.

Support files created for the test case are:
* **Front location** at each output interval, in a raster file which has the value zero for dry areas, one for inundated areas, and two for the exact location of the front
* **Front velocity** at each output interval, at the location of the front only, and providing null values elsewhere
