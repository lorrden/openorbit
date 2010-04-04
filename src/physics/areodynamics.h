/*
 Copyright 2010 Mattias Holm <mattias.holm(at)openorbit.org>
 
 This file is part of Open Orbit.
 
 Open Orbit is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 Open Orbit is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with Open Orbit.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PL_ATMOSPHERE_H
#define PL_ATMOSPHERE_H

#include "physics/reftypes.h"
#include <vmath/vmath.h>

typedef enum PLelement {
  PL_Elem_First = 0, PL_Elem_n = 0,
  // Period 1
  PL_Elem_H = 1, PL_Elem_He,

  // Period 2
  PL_Elem_Li, PL_Elem_Be, PL_Elem_B, PL_Elem_C,
  PL_Elem_N, PL_Elem_O, PL_Elem_F, PL_Elem_Ne,

  // Period 3
  PL_Elem_Na, PL_Elem_Mg, PL_Elem_Al, PL_Elem_Si,
  PL_Elem_P, PL_Elem_S, PL_Elem_Cl, PL_Elem_Ar,

  // Period 4
  PL_Elem_K, PL_Elem_Ca, PL_Elem_Sc, PL_Elem_Ti,
  PL_Elem_V, PL_Elem_Cr, PL_Elem_Mn, PL_Elem_Fe,
  PL_Elem_Co, PL_Elem_Ni, PL_Elem_Cu, PL_Elem_Zn,
  PL_Elem_Ga, PL_Elem_Ge, PL_Elem_As, PL_Elem_Se,
  PL_Elem_Br, PL_Elem_Kr,

  // Period 5
  PL_Elem_Rb, PL_Elem_Sr, PL_Elem_Y, PL_Elem_Zr,
  PL_Elem_Nb, PL_Elem_Mo, PL_Elem_Tc, PL_Elem_Ru,
  PL_Elem_Rh, PL_Elem_Pd, PL_Elem_Ag, PL_Elem_Cd,
  PL_Elem_In, PL_Elem_Sn, PL_Elem_Sb, PL_Elem_Te,
  PL_Elem_I, PL_Elem_Xe,

  // Period 6
  PL_Elem_Cs, PL_Elem_Ba,

  // Lanthanoids
  PL_Elem_La, PL_Elem_Ce, PL_Elem_Pr, PL_Elem_Nd,
  PL_Elem_Pm, PL_Elem_Sm, PL_Elem_Eu, PL_Elem_Gd,
  PL_Elem_Tb, PL_Elem_Dy, PL_Elem_Ho, PL_Elem_Er,
  PL_Elem_Tm, PL_Elem_Yb, PL_Elem_Lu,

  PL_Elem_Hf, PL_Elem_Ta, PL_Elem_W, PL_Elem_Re,
  PL_Elem_Os, PL_Elem_Ir, PL_Elem_Pt, PL_Elem_Au,
  PL_Elem_Hg, PL_Elem_Tl, PL_Elem_Pb, PL_Elem_Bi,
  PL_Elem_Po, PL_Elem_At, PL_Elem_Rn,

  // Period 7
  PL_Elem_Fr, PL_Elem_Ra,

  // Actinoids
  PL_Elem_Ac, PL_Elem_Th, PL_Elem_Pa, PL_Elem_U,
  PL_Elem_Np, PL_Elem_Pu, PL_Elem_Am, PL_Elem_Cm,
  PL_Elem_Bk, PL_Elem_Cf, PL_Elem_Es, PL_Elem_Fm,
  PL_Elem_Md, PL_Elem_No, PL_Elem_Lr,

  PL_Elem_Rf, PL_Elem_Db, PL_Elem_Sg, PL_Elem_Bh,
  PL_Elem_Hs, PL_Elem_Mt, PL_Elem_Ds, PL_Elem_Rg,
  PL_Elem_Cn, PL_Elem_Uut, PL_Elem_Uuq, PL_Elem_Uup,
  PL_Elem_Uuh, PL_Elem_Uus, PL_Elem_Uuo,

  PL_Elem_Last,
} PLelement;



typedef struct PLatmosphere {
  float Pb; // Standard pressure at "ground"-level
  float Tb; // Standard temperature at "ground"-level
  float M; // Molar mass (kg / mol)
  float pb; // Standard density at "ground"-level
} PLatmosphere;

double plComputeAirspeed(PLobject *obj);
double plComputeAirpressure(PLobject *obj);
double plComputeAirdensity(PLobject *obj);
float3 plComputeDragForObject(PLobject *obj);

/*!
 Computes the air preasure at a given altitude, note that for simplicitys sake,
 you can call the methods with 0 for the hb value and use standard ground level
 parameters for Pb and Tb.
 \param Pb Static preasure (reference)
 \param Tb Standard temperature
 \param g0 Gravitational accelleration
 \param M Molar mass of atmosphere
 \param h Height above sea level
 \param hb height of reference point (for Pb / Tb)
 \param Lb Lapse rate in kelvin per metre
 */
double
plPressureAtAltitudeWithLapse(double Pb, double Tb, double g0, double M,
                              double h, double hb, double Lb);

double
plPressureAtAltitude(double Pb, double Tb, double g0, double M, double h,
                     double hb);

#endif /* ! PL_ATMOSPHERE_H */