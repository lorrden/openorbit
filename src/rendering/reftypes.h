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

#ifndef SG_REFTYPES_H
#define SG_REFTYPES_H

typedef struct SGdrawable SGdrawable;
typedef struct OOscene OOscene;
typedef struct OOoverlay OOoverlay;
typedef struct OOscenegraph OOscenegraph;
typedef struct SGmaterial SGmaterial;

typedef struct SGlight SGlight;
typedef struct SGspotlight SGspotlight;
typedef struct SGpointlight SGpointlight;

typedef struct OOsphere OOsphere;
typedef struct SGcylinder SGcylinder;

typedef struct SGparticles SGparticles;

typedef void (*SGdrawfunc)(SGdrawable*);


typedef enum OOcamtype OOcamtype;
typedef struct OOfreecam OOfreecam;
typedef struct OOfixedcam OOfixedcam;
typedef struct OOorbitcam OOorbitcam;
typedef struct OOcam OOcam;

#endif /* !SG_REFTYPES_H */