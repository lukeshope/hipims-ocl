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
````

Details of the test can be found in Wang Y., Liang Q., Kesserwani G., Hall J.W. (2011) [A 2D shallow flow model for practical dam-break simulations](http://dx.doi.org/10.1080/00221686.2011.566248), _Journal of Hydraulic Research_, 49(3):307-316.

## Lake at rest
Used to test the well-balanced property of the numerical scheme. No change in water level is expected.

This test is a work in progress.

## Dam-break flow against an isolated obstacle
This test is a work in progress.

## Dam break over an emerging bed
This test is a work in progress.
