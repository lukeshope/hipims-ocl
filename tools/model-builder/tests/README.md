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

Constant values define the water level, and the shape and height of an island in the middle of the domain

* **a** is used to vertically scale the island
* **b** defines the scale of the island in terms of the shape
* **n** is the water level
* **i** is the maximum height of the island in the centre
* **s** is maximum depth of the sea surrounding the island, and thus relative to the water level

This test is adapted from a one-dimensional example used in Xing Y., Zhang X., Shu C. (2010) [Positivity-preserving high order well-balanced discontinuous Galerkin methods for shallow water equations](http://dx.doi.org/10.1016/j.advwatres.2010.08.005), _Advances in Water Resources_, 33:1476-1493.

## Dam-break flow against an isolated obstacle
This test is a work in progress.

## Dam break over an emerging bed
This test is a work in progress.
