Steady-State One-Dimensional Flow with Flux and Head Boundary Conditions
========================================================================

Capabilties Tested
------------------
This test illustrates the capability to model one-dimensional,
steady-state flow through a saturated porous medium with constant properties.
Capabilties tested include,

  * one-dimensional reprsentation
  * steady-state
  * saturated flow
  * heterogenous porous medium
  * flow and head boundaries

For details on this test, see :ref:`about_linear_flux_head_1d`.


Background
----------
For one-dimensional, steady-state, flow through a saturated porous medium with constant properties, 
the general governing differential equation expressing mass conservation and Darcy's law becomes simply

	.. math:: \frac{d^2h}{dx^2} = 0
		:label: ode_linear_flux_head

where the total head (:math:`h`, [L]) is the sum of pressure head (:math:`P/\rho g`, [L]) 
and elevation (:math:`z`, [L])

	.. math:: h = \frac{P}{\rho g}+z

:math:`\rho` = density [M/L\ :sup:`3`\ ], :math:`g` = gravitational acceleration [L/T\ :sup:`2`\ ], 
and :math:`x` = horizontal distance [L]. The ordinary differential equation :eq:`ode_linear_flux_head` is easily solved by 
direct integration as

	.. math:: h = C_1 x + C_2
		:label: generalSoln_linear_flux_head

where the integration constants :math:`C_1` and :math:`C_2` depend on the boundary conditions.


Model
-----
The analytic solution for prescribed inlet flow and outlet pressure is given below.

When Darcy velocity is prescribed at the inlet boundary :math:`x = 0` and 
hydraulic head at the outlet :math:`x = L` as

	.. math:: 
		U(0) &= U_0\\
		h(L) &= h_L
		:label: bc_linear_flux_head

the analytic solution :eq:`generalSoln_linear_flux_head` for hydraulic head becomes

	.. math:: h = \frac{U_0L}{K} (1 - \frac{x}{L})  + h_L
		:label: specificSoln_linear_flux_head

where :math:`L` = domain length [L]. For these boundary conditions the volumetric flowrate per unit area, 
or Darcy velocity (:math:`U`, [L/T]), is constant and defined by Darcy's law as

	.. math:: U = -\frac{k}{\mu}\rho g \frac{dh}{dx} = -K\frac{dh}{dx} = -K\frac{-U_0}{K} = U_0
		:label: DarcyVel_linear_flux_head

where :math:`k` = intrinsic permeability [L\ :sup:`2`\ ],
:math:`\mu` = viscosity [M/LT], and 
:math:`K` = hydraulic conductivity [L/T]. 


Problem Specification
---------------------
The analytic solutions for hydraulic head and Darcy velocity can be used to test Amanzi
implementation of prescribed hydraulic head boundary conditions, Darcy's law, and mass conservation
on an elementary problem. 


Schematic
~~~~~~~~~
The domain is shown in the following schematic.

.. figure:: schematic/schematic.png 
    :figclass: align-center
    :width: 400 px

    **One-dimensional, steady-state flow through a saturated porous medium with prescribed hydraulic head boundary conditions.**


Mesh
~~~~
A steady-flow mesh is applied.


Variables
~~~~~~~~~
To generate numerical results the following specifications are considered:

* Domain

	* :math:`x_{min} = y_{min} = z_{min} = 0`
	* :math:`x_{max} = 100 m, y_{max} = 2 m, z_{max} = 10 m`

* Horizontal flow in the x-coordinate direction

	* no-flow prescribed at the :math:`y_{min}, y_{max}, z_{min}, z_{max}` boundaries
	* prescribed Darcy velocity at the x-coordinate inlet: :math:`U(0) = 0.01 \text{[m/d]}`
	* prescribed hydraulic head at the x-coordinate outlet: :math:`h(L) = 19 \text{[m]}`

* Material properties:

	* :math:`\rho = 998.2 \: \text{[kg/m}^3\text{]}`,
          :math:`\mu = 1.002e-3 \: \text{[Pa}\cdot \text{s]}`, 
          :math:`g = 9.807 \: \text{[m/s}^2\text{]}` 
	* :math:`K = 1.0 \text{[m/d]}` :math:`(k = 1.1847E-12 \text{[m}^2\text{]})`

* Model discretization

	* :math:`\Delta x = 5 \text{[m]}, \Delta y = 2 \text{[m]}, \Delta z = 10 \text{[m]}`

For these input specifications, Amanzi simulation output is expected to closely match

	.. math:: h = 20 -\frac{x}{100m} \text{[m]}
		:label: expectedH_linear_flux_head

and

	.. math:: U = 1.0 \text{[m/d]}
		:label: expectedU_linear_flux_head

following Equations :eq:`specificSoln_linear_flux_head` and :eq:`DarcyVel_linear_flux_head`.


Results and Comparison
----------------------
The discretization is exact for linear solutions, and it is clear in the figure that
Amanzi has reproduced the exact solution.

.. plot:: amanzi_linear_flux_head_1d.py
   :align: center

This is also visible in the following table:

.. include:: table_values.txt


References
----------
Add a reference in here.


.. _about_linear_flux_head_1d:

About
-----

* Directory: testing/verification/flow/saturated/steady-state/linear_flux_head_1d

* Authors:  Greg Flach, SRNL and Marc Day, LBNL

* Maintainer(s): David Moulton, moulton@lanl.gov

* Input Files:

  * amanzi_linear_flux_head_1d-s.xlm 
    * Spec Version 1.2.2, structured mesh framework
    * mesh:  steady-flow_mesh.h5
    * runs
  * amanzi_linear_flue_head_1d-u.xml
    * Spec Version 2.3.0, unstructured
    * runs

* Mesh Files:
  * steady-flow_mesh.h5

* Analytic solution computed with golden output
  * Subdirectory: golden_output

  * Input Files: 
    * steady-flow_data.h5


Status
~~~~~~
.. todo:: 

  * Documentation:
    * keb: find a schematic for this test
    * keb: amanzi_linear_flux_head_1d-s.xlm seems like an older format
    * keb: there is an extra xml in folder, is that used? amanzi_linear_flux_head_1d-s-isv2.xml
