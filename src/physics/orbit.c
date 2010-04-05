/*
  Copyright 2008, 2009, 2010 Mattias Holm <mattias.holm(at)openorbit.org>

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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "orbit.h"

#include "physics.h"

#include <vmath/vmath.h>
#include "sim.h"
#include "sim/simtime.h"
#include "geo/geo.h"
#include "log.h"
#include "parsers/hrml.h"
#include "res-manager.h"
#include "rendering/scenegraph.h"
#include "rendering/scenegraph-private.h"

#include "common/lwcoord.h"

/// Gravitational constant in m^3/kg/s^2
#define PL_GRAVITATIONAL_CONST 6.67428e-11
#define PL_SEC_PER_DAY (3600.0 * 24.0)
/*
 NOTE: Coordinate system are specified in the normal convention used for mission
       analysis. This means that x is positive towards you, y is positvie to the
       right and z positive going upwards. This is a right handed coordinate
       system. Positive x, in our case points towards the reference point of
       ares on the ecliptic.
 */


struct state_vectors {
  double vx, vy, vz, rx, ry, rz;
};

double
plGm(double m0, double m1)
{
  return PL_GRAVITATIONAL_CONST * (m0 + m1);
}

double
comp_orbital_period(double semimajor, double g, double m1, double m2)
{
  return 2.0 * M_PI * sqrt(pow(semimajor, 3.0)/(g*(m1 + m2)));
}

double
comp_orbital_period_for_planet(double semimajor)
{
  return sqrt(pow(semimajor, 3.0));
}


/*! Computes the orbital period when there is a dominating object in the system
  \param a Semi-major axis of orbit
  \param GM Gravitational parameter (GM) of orbited body
 */
double
plOrbitalPeriod(double a, double GM)
{
  return 2.0 * M_PI * sqrt((a*a*a) / GM);
}

double
plMeanMotionFromPeriod(double tau)
{
  return (2.0 * M_PI) / tau;
}

/*! Computes the mean motion when there is a dominating object in the system
 \param a Semi-major axis of orbit
 \param u Gravitational parameter (GM) of orbited body
 */
long double
plMeanMotion(long double u, long double a)
{
  return sqrtl(u/(a*a*a));
}

/*!
  Computes the estimate of the next eccentric anomaly
 \param E_i Eccentric anomaly of previous step, initialise to n*t.
 \param ecc Eccentricity of orbital ellipse
 \param m Mean anomaly
 */
long double
plEccAnomalityStep(long double E_i, long double ecc, long double m)
{
  return E_i - ( (E_i-ecc*sinl(E_i)-m) / (1-ecc*cosl(E_i)) );
}

/*!
  Computes the eccentric anomaly for time t, t = 0 is assumed to be when the
  object pass through its periapsis.

  The method solves this by making a few iterations with newton-rapson, for
  equations, see celestial mechanics chapter in Fortescue, Stark and Swinerd's
  Spacecraft Systems Engineering.

  The eccentric anomaly can be used to solve the position of an orbiting object.

  Note on units: n and t should be compatible, n is composed of the GM and a, GM
  in terms is defined in distance / time, and a is the orbits semi-major axis.
  Thus good units for usage are for example: time = earth days or years, and
  distance = m, km or au.

  \param ecc Eccentricity of orbit
  \param n Mean motion around object
  \param t Absolute time for which we want the eccentric anomaly.
 */
long double
plEccAnomaly(long double ecc, long double n, long double t)
{
  // 7.37 mm accuracy for an object at the distance of the dwarf-planet Pluto
#define ERR_LIMIT 0.000000000001l
  long double meanAnomaly = n * t;

  long double E_1 = plEccAnomalityStep(meanAnomaly, ecc, meanAnomaly);
  long double E_2 = plEccAnomalityStep(E_1, ecc, meanAnomaly);

  long double E_i = E_1;
  long double E_i1 = E_2;
  int i = 0;

  while (fabsl(E_i1-E_i) > ERR_LIMIT) {
    E_i = E_i1;
    E_i1 = plEccAnomalityStep(E_i, ecc, meanAnomaly);
    i ++;

    if (i > 10) {
      ooLogWarn("ecc anomaly did not converge in %d iters, err = %.16f", i, fabs(E_i1-E_i));
      break;
    }
  }

  ooLogTrace("ecc anomaly solved in %d iters", i);
  return E_i1;
#undef ERR_LIMIT
}
quaternion_t
plOrbitalQuaternion(PL_keplerian_elements *kepler)
{
  quaternion_t qasc = q_rot(0.0, 0.0, 1.0, kepler->longAsc);
  quaternion_t qinc = q_rot(0.0, 1.0, 0.0, kepler->inc);
  quaternion_t qaps = q_rot(0.0, 0.0, 1.0, kepler->argPeri);

  quaternion_t q = q_mul(qasc, qinc);
  q = q_mul(q, qaps);
  return q;
}

/*!
 \param a Semi-major axis
 \param b Semi-minor axis
 \param ecc Eccentricity of orbit
 \param GM Gravitational parameter of orbited object GM
 \param t Absolute time in seconds.
 */
float3
plOrbitPosAtTime(PL_keplerian_elements *orbit, double GM, double t)
{
  long double meanMotion = plMeanMotion(GM, orbit->a);
  long double eccAnomaly = plEccAnomaly(orbit->ecc, meanMotion, t);

  /* Compute x, y from anomaly, y is pointing in the direction of the
     periapsis */
  double y = orbit->a * cos(eccAnomaly) - orbit->a * orbit->ecc; // NOTE: on the plane we usually do x = a cos t
  double x = -orbit->b * sin(eccAnomaly); // Since we use y as primary axis, x points downwards

  quaternion_t q = orbit->qOrbit;
  float3 v = vf3_set(x, y, 0.0);
  v = v_q_rot(v, q);
  return v;
}

quaternion_t
plSideralRotationAtTime(PLastrobody *ab, double t)
{
  quaternion_t q = ab->kepler->qOrbit;
  q = q_mul(q, q_rot(1.0, 0.0, 0.0, ab->obliquity));
  double rotations = t/ab->siderealPeriod;
  double rotFrac = fmod(rotations, 1.0);
  quaternion_t z_rot = q_rot(0.0f, 0.0f, 1.0f, rotFrac * 2.0 * M_PI);
  return q_mul(q, z_rot);
}

PL_keplerian_elements*
plNewKeplerElements(double ecc, double a, double inc, double longAsc,
                    double argOfPeriapsis, double meanAnomalyOfEpoch)
{
  PL_keplerian_elements *elems = malloc(sizeof(PL_keplerian_elements));
  elems->ecc = ecc;
  elems->a = a; // Semi-major
  elems->b = ooGeoComputeSemiMinor(a, ecc); // Auxillary semi-minor
  elems->inc = DEG_TO_RAD(inc);
  elems->longAsc = DEG_TO_RAD(longAsc);
  elems->argPeri = DEG_TO_RAD(argOfPeriapsis);
  elems->meanAnomalyOfEpoch = DEG_TO_RAD(meanAnomalyOfEpoch);
  elems->qOrbit = plOrbitalQuaternion(elems);
  return elems;
}

PLsystem*
plGetSystem(PLworld *world, const char *name)
{
  char str[strlen(name)+1];
  strcpy(str, name); // TODO: We do not trust the user, should probably
  //       check alloca result
  char *strp = str;
  char *strTok = strsep(&strp, "/");

  // One level name?
  if (strp == NULL) {
    if (!strcmp(strTok, world->rootSys->name)) {
      return world->rootSys;
    }
    return NULL;
  } else if (strcmp(strTok, world->rootSys->name)) {
    return NULL;
  }

  int idx = 0;
  obj_array_t *vec = &world->rootSys->orbits;

  if (vec->length == 0) {
    return NULL;
  }

  PLsystem *sys = vec->elems[idx];
  strTok = strsep(&strp, "/");

  while (sys) {
    if (!strcmp(sys->name, strTok)) {
      if (strp == NULL) {
        // At the end of the sys path
        return sys;
      }

      // If this is not the lowest level, go one level down
      strTok = strsep(&strp, "/");

      vec = &sys->orbits;
      idx = 0;
      if (vec->length <= 0) return NULL;
      sys = vec->elems[idx];
    } else {
      if (vec == NULL) return NULL;
      idx ++;
      if (vec->length <= idx) return NULL;
      sys = vec->elems[idx];
    }
  }
  return NULL;
}


PLastrobody*
plGetObject(PLworld *world, const char *name)
{
  PLsystem *sys = plGetSystem(world, name);
  if (sys) return sys->orbitalBody;
  return NULL;
}



float3
plGetPos(const PLastrobody *obj)
{
  return ooLwcGlobal(&obj->obj.p);
}

float3
plGetPosForName(const PLworld *world, const char *name)
{
  PLastrobody *obj = plGetObject((PLworld*)world, name);
  assert(obj);

  return plGetPos(obj);
}

void
plGetPosForName3f(const PLworld *world, const char *name,
                  float *x, float *y, float *z)
{
  assert(world && name && x && y && z);

  float3 p = plGetPosForName(world, name);

  // TODO: Move to clang ext vector support
  *x = vf3_get(p, 0);
  *y = vf3_get(p, 1);
  *z = vf3_get(p, 2);
}

void
plNormaliseObject__(PLastrobody *obj)
{
  ooLwcNormalise(&obj->obj.p);
}

void
plClearObject__(PLobject *obj)
{
  obj->f_ack = vf3_set(0.0f, 0.0f, 0.0f);
  obj->t_ack = vf3_set(0.0f, 0.0f, 0.0f);
}

void
plSysUpdateCurrentPos(PLsystem *sys, double dt)
{
  if (sys->parent) {
    double t = ooTimeGetJD();

    if (sys->orbitalBody->tUpdate > 0) {
      ooLwcTranslate3fv(&sys->orbitalBody->obj.p,
                        vf3_s_mul(sys->orbitalBody->obj.v, dt));
      sys->orbitalBody->obj.q = plSideralRotationAtTime(sys->orbitalBody, t);
      sys->orbitalBody->tUpdate --;
    } else {
      float3 newPos = plOrbitPosAtTime(sys->orbitalBody->kepler,
                                       sys->parent->orbitalBody->GM + sys->orbitalBody->GM,
                                       t*PL_SEC_PER_DAY);

      float3 nextPos = plOrbitPosAtTime(sys->orbitalBody->kepler,
                                        sys->parent->orbitalBody->GM + sys->orbitalBody->GM,
                                        t*PL_SEC_PER_DAY + (double)sys->orbitalBody->orbitFixationPeriod * dt);

      float3 vel = vf3_s_div(vf3_sub(nextPos, newPos), (double)sys->orbitalBody->orbitFixationPeriod * dt);

      sys->orbitalBody->obj.p = sys->parent->orbitalBody->obj.p;
      ooLwcTranslate3fv(&sys->orbitalBody->obj.p, newPos);

      sys->orbitalBody->obj.q = plSideralRotationAtTime(sys->orbitalBody, t);
      sys->orbitalBody->obj.v = vel;
      // Reset tUpdate;
      sys->orbitalBody->tUpdate = sys->orbitalBody->orbitFixationPeriod;
    }
  }
}

// Note that the position can only be changed for an object that is not the root
// root is by def not orbiting anything
void
plSysSetCurrentPos(PLsystem *sys)
{
  if (sys->parent) {
    double t = ooTimeGetJD();
    float3 newPos = plOrbitPosAtTime(sys->orbitalBody->kepler,
                                     sys->parent->orbitalBody->GM + sys->orbitalBody->GM,
                                     t*PL_SEC_PER_DAY);
    sys->orbitalBody->obj.p = sys->parent->orbitalBody->obj.p;
    ooLwcTranslate3fv(&sys->orbitalBody->obj.p, newPos);

    sys->orbitalBody->obj.q = plSideralRotationAtTime(sys->orbitalBody, t);
    sys->orbitalBody->tUpdate = 0;
  }
}

void
plDeleteSys(PLsystem *sys)
{
  for (size_t i = 0 ; i < sys->orbits.length ; i ++) {
    plDeleteSys(sys->orbits.elems[i]);
  }

  //ooGeoEllipseFree(sys->orbitalPath);
  free((char*)sys->name);
  free(sys);
}

void
plDeleteWorld(PLworld *world)
{
  plDeleteSys(world->rootSys);

  free((char*)world->name);
  free(world);
}

void
plSetDrawable(PLastrobody *obj, SGdrawable *drawable)
{
  obj->drawable = drawable;
}

/*!
  Creates a new object
  All objects, even the small ones have a GM value

  \param world The world in which the object is placed
  \param name Name of object
  \param m Mass of object in kg
  \param gm Standard gravitational parameter for object (GM), if set to NAN, the
            value will be calculated from m. This allow you to enter more exact
            values, that will not be subject to innacurate floating point
            calculations.
  \param coord The large world coordinate of the object
 */
PLastrobody*
plNewObj(PLworld*world, const char *name, double m, double gm,
         OOlwcoord * coord,
         quaternion_t q, double siderealPeriod, double obliquity,
         double radius, double flattening)
{
  PLastrobody *obj = malloc(sizeof(PLastrobody));
  
  plInitObject(&obj->obj);
  obj->obj.p = *coord;
  obj->obj.q = q;
  obj->name = strdup(name);
  obj->sys = NULL;
  obj->world = world;
  obj->lightSource = NULL;
  // TODO: Ensure quaternion is set for orbit
  //       asc * inc * obl ?

  obj->drawable = NULL;
  obj->obj.m.m = m;

  if (isnormal(gm)) {
    obj->GM = gm;
  } else {
    obj->GM = m * PL_GRAVITATIONAL_CONST;
  }
  obj->kepler = NULL;
  obj->eqRad = radius;

  // flattening = ver(angEcc) = 1 - cos(angEcc) => angEcc = acos(1 - flattening)
  obj->angEcc = acos(1.0 - flattening);
  obj->obliquity = DEG_TO_RAD(obliquity);
  obj->siderealPeriod = siderealPeriod;

  // Used for smothening the orbits, this is a rather ugly hack, but should work for now
  obj->tUpdate = 0;
  obj->orbitFixationPeriod = 100;
  return obj;
}

PLastrobody*
plNewObjInSys(PLsystem *sys, const char *name, double m, double gm,
              OOlwcoord *coord, quaternion_t q, double siderealPeriod, double obliquity,
              double radius, double flattening)
{
  PLastrobody *obj = plNewObj(sys->world, name, m, gm, coord,
                              q, siderealPeriod, obliquity,
                              radius, flattening);
  obj->sys = sys;

  obj_array_push(&sys->astroObjs, obj);
  return obj;
}


PLworld*
plNewWorld(const char *name, SGscene *sc,
           double m, double gm, double radius, double siderealPeriod,
           double obliquity, double eqRadius, double flattening)
{
  PLworld *world = malloc(sizeof(PLworld));

  world->name = strdup(name);
  world->rootSys = plNewRootSystem(world, sc, name, m, gm,
                                   obliquity, siderealPeriod,
                                   eqRadius, flattening);
  obj_array_init(&world->objs);
  obj_array_init(&world->partSys);
  return world;
}

PLsystem*
plCreateOrbitalObject(PLworld *world, SGscene *scene, const char *name,
                      double m, double gm,
                      double orbitPeriod,
                      double obliquity, double siderealPeriod,
                      double semiMaj, double semiMin,
                      double inc, double ascendingNode, double argOfPeriapsis,
                      double meanAnomaly,
                      double eqRadius, double flattening)
{
  assert(world);

  PLsystem *sys = malloc(sizeof(PLsystem));
  obj_array_init(&sys->orbits);
  obj_array_init(&sys->astroObjs);
  obj_array_init(&sys->rigidObjs);

  sys->name = strdup(name);
  sys->world = world;
  sys->scene = scene;
  sys->parent = NULL;

  sys->orbitalPeriod = orbitPeriod;

  // TODO: Stack allocation based on untrusted length should not be here
  char orbitName[strlen(name) + strlen(" Orbit") + 1];
  strcpy(orbitName, name); // safe as size is checked in allocation
  strcat(orbitName, " Orbit");

  OOlwcoord p;
  ooLwcSet(&p, 0.0, 0.0, 0.0);

  // TODO: Cleanup this quaternion
  quaternion_t q = q_rot(0.0, 0.0, 1.0, DEG_TO_RAD(ascendingNode));
  q = q_mul(q, q_rot(0.0, 1.0, 0.0, DEG_TO_RAD(inc)));
  q = q_mul(q, q_rot(0.0, 0.0, 1.0, DEG_TO_RAD(argOfPeriapsis)));
  q = q_mul(q, q_rot(1.0, 0.0, 0.0, DEG_TO_RAD(obliquity)));

  sys->orbitalBody = plNewObjInSys(sys/*world->rootSys*/, name, m, gm, &p,
                                   q, siderealPeriod, obliquity,
                                   eqRadius, flattening);
  sys->orbitalBody->kepler = plNewKeplerElements(sqrt((semiMaj*semiMaj-semiMin*semiMin)/(semiMaj*semiMaj)),
                                                 semiMaj, inc, ascendingNode,
                                                 argOfPeriapsis, meanAnomaly);

  sys->orbitDrawable = sgNewEllipsis(orbitName, semiMaj, semiMin,
                                     ascendingNode, inc, argOfPeriapsis,
                                     0.0, 0.0, 1.0,
                                     1024);
  sgSceneAddObj(sys->scene, sys->orbitDrawable);

  return sys;
}

PLsystem*
plNewRootSystem(PLworld *world, SGscene *sc, const char *name, double m, double gm, double obliquity, double siderealPeriod,
                double eqRadius, double flattening)
{
  assert(world);

  PLsystem *sys = malloc(sizeof(PLsystem));
  obj_array_init(&sys->orbits);
  obj_array_init(&sys->astroObjs);
  obj_array_init(&sys->rigidObjs);

  sys->name = strdup(name);
  sys->world = world;
  sys->parent = NULL;
  sys->scene = sc;
  OOlwcoord p;
  ooLwcSet(&p, 0.0, 0.0, 0.0);
  quaternion_t q = q_rot(1.0, 0.0, 0.0, DEG_TO_RAD(obliquity));

  sys->orbitalBody = plNewObj(world, name, m, gm, &p, q, siderealPeriod, obliquity,
                              eqRadius, flattening);
  sys->orbitalBody->kepler = NULL;
  sys->orbitDrawable = NULL;
//  ooSgSceneAddObj(sys->world->scene,
//                  sys->orbitDrawable);

  world->rootSys = sys;
  plSysSetCurrentPos(sys);
  return sys;
}



PLsystem*
plNewOrbit(PLworld *world, SGscene *sc, const char *name,
           double m, double gm,
           double orbitPeriod, double obliquity, double siderealPeriod,
           double semiMaj, double semiMin,
           double inc, double ascendingNode, double argOfPeriapsis,
           double meanAnomaly, double eqRadius, double flattening)
{
  assert(world);
  return plNewSubOrbit(world->rootSys, sc, name,
                       m, gm, orbitPeriod, obliquity, siderealPeriod,
                       semiMaj, semiMin,
                       inc, ascendingNode, argOfPeriapsis, meanAnomaly,
                       eqRadius, flattening);
}

PLsystem*
plNewSubOrbit(PLsystem *parent, SGscene *sc, const char *name,
              double m, double gm,
              double orbitPeriod, double obliquity, double siderealPeriod,
              double semiMaj, double semiMin,
              double inc, double ascendingNode, double argOfPeriapsis,
              double meanAnomaly,
              double eqRadius, double flattening)
{
  assert(parent);
  assert(parent->world);

  PLsystem * sys = plCreateOrbitalObject(parent->world, sc,
                                         name, m, gm, orbitPeriod, obliquity, siderealPeriod,
                                         semiMaj, semiMin,
                                         inc, ascendingNode, argOfPeriapsis, meanAnomaly,
                                         eqRadius, flattening);
  sys->parent = parent;
  obj_array_push(&parent->orbits, sys);
  plSysSetCurrentPos(sys);

  return sys;
}

void
plSysClear(PLsystem *sys)
{
  for (size_t i = 0; i < sys->rigidObjs.length ; i ++) {
    plClearObject__(sys->rigidObjs.elems[i]);
  }

  for (size_t i = 0; i < sys->orbits.length ; i ++) {
    plSysClear(sys->orbits.elems[i]);
  }
}

void
plWorldClear(PLworld *world)
{
  plSysClear(world->rootSys);
}

float3
plComputeGravity(PLastrobody *a, PLobject *b)
{
  float3 dist = ooLwcDist(&b->p, &a->obj.p);
  double r12 = vf3_abs_square(dist);
  float3 f12 = vf3_s_mul(vf3_normalise(dist),
                         -a->GM * b->m.m / r12);
  return f12;
}

void
plSysStep(PLsystem *sys, double dt)
{
  // Add gravitational forces
  for (size_t i = 0; i < sys->rigidObjs.length ; i ++) {
    PLobject *obj = sys->rigidObjs.elems[i];
    float3 f12 = plComputeGravity(sys->orbitalBody, obj);
    plForce3fv(obj, f12);
    if (sys->parent) {
      f12 = plComputeGravity(sys->parent->orbitalBody, obj);
      plForce3fv(obj, f12);
    }
    float3 drag = plComputeDragForObject(obj);

    plStepObjectf(obj, dt);
  }

  plSysUpdateCurrentPos(sys, dt);
  for (size_t i = 0; i < sys->orbits.length ; i ++) {
    plSysStep(sys->orbits.elems[i], dt);
  }
}

void
plSysInit(PLsystem *sys)
{
  plSysSetCurrentPos(sys);
  for (size_t i = 0; i < sys->orbits.length ; i ++) {
    plSysInit(sys->orbits.elems[i]);
  }
}


void
plSysUpdateSg(PLsystem *sys)
{
  if (sys->orbitalBody->lightSource)
    sgSetLightPosLW(sys->orbitalBody->lightSource, &sys->orbitalBody->obj.p);

  quaternion_t q = plGetQuat(&sys->orbitalBody->obj);
  sgSetObjectQuatv(sys->orbitalBody->drawable, q);
  sgSetObjectPosLW(sys->orbitalBody->drawable, &sys->orbitalBody->obj.p);

  sgSetScenePosLW(sys->scene, &sys->orbitalBody->obj.p);
  // Update orbital path base
  if (sys->parent) {
    sgSetObjectPosLW(sys->orbitDrawable, &sys->parent->orbitalBody->obj.p);
  }

  //ooSgSetObjectSpeed(OOdrawable *obj, float dx, float dy, float dz);
  //ooSgSetObjectAngularSpeed(OOdrawable *obj, float drx, float dry, float drz);
  //for (size_t i = 0; i < sys->rigidObjs.length ; i ++) {
  //  PLobject *obj = sys->rigidObjs.elems[i];
  //  if (obj->drawable) {
      //sgSetObjectPosLWAndOffset(obj->drawable, &obj->p, v_q_rot(obj->p_offset, obj->q));
      //    sgSetObjectPosLW(obj->drawable, &obj->p);
      // ooLwcDump(&obj->p);
      //sgSetObjectQuatv(obj->drawable, obj->q);
      //}
      //}

  for (size_t i = 0; i < sys->orbits.length ; i ++) {
    plSysUpdateSg(sys->orbits.elems[i]);
  }
}


void
plWorldStep(PLworld *world, double dt)
{
  plSysStep(world->rootSys, dt);
  for (size_t i = 0; i < world->objs.length ; i ++) {
    PLobject *obj = world->objs.elems[i];
    if (obj->parent && obj->drawable) {
      plStepChildObjectf(obj, dt);
    }
  }
  plSysUpdateSg(world->rootSys);

  for (size_t i = 0; i < world->objs.length ; i ++) {
    PLobject *obj = world->objs.elems[i];
    if (obj->drawable) {
      sgSetObjectPosLW(obj->drawable, &obj->p);
      sgSetObjectQuatv(obj->drawable, obj->q);
    }
  }

  for (size_t i = 0; i < world->partSys.length ; ++ i) {
    PLparticles *psys = world->partSys.elems[i];
    plStepParticleSystem(psys, dt);
  }
}
/*
    NOTE: G is defined as 6.67428 e-11 (m^3)/kg/(s^2), let's call that G_m. In AU,
      this would then be G_au = G_m / (au^3)

      This means that G_au = 1.99316734 e-44 au^3/kg/s^2
      or: G_au = 1.99316734 e-44 au^3/kg/s^2

      1 au = 149 597 870 000 m

*/

void
ooLoadMoon__(PLsystem *sys, HRMLobject *obj, SGscene *sc)
{
  assert(obj);
  assert(obj->val.typ == HRMLNode);

  HRMLvalue moonName = hrmlGetAttrForName(obj, "name");

  double mass, radius, siderealPeriod, gm = NAN, axialTilt = 0.0;
  double semiMajor, ecc, inc, longAscNode, longPerihel, meanLong;
  const char *tex = NULL;

  double flattening = 0.0;
  HRMLobject *sats = NULL;

  for (HRMLobject *child = obj->children; child != NULL ; child = child->next) {
    if (!strcmp(child->name, "physical")) {
      for (HRMLobject *phys = child->children; phys != NULL; phys = phys->next) {
        if (!strcmp(phys->name, "mass")) {
          mass = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "radius")) {
          radius = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "sidereal-rotational-period")) {
          siderealPeriod = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "axial-tilt")) {
          axialTilt = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "gm")) {
          gm = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "flattening")) {
          flattening = hrmlGetReal(phys);
        }
      }
    } else if (!strcmp(child->name, "orbit")) {
      for (HRMLobject *orbit = child->children; orbit != NULL; orbit = orbit->next) {
        if (!strcmp(orbit->name, "semimajor-axis")) {
          semiMajor = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "eccentricity")) {
          ecc = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "inclination")) {
          inc = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "longitude-ascending-node")) {
          longAscNode = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "longitude-periapsis")) {
          longPerihel = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "mean-longitude")) {
          meanLong = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "reference-date")) {

        } else {
          fprintf(stderr, "load, invalid orbit token: %s\n", orbit->name);
          assert(0);
        }
      }
    } else if (!strcmp(child->name, "atmosphere")) {

    } else if (!strcmp(child->name, "rendering")) {
      for (HRMLobject *rend = child->children; rend != NULL; rend = rend->next) {
        if (!strcmp(rend->name, "model")) {

        } else if (!strcmp(rend->name, "texture")) {
          tex = hrmlGetStr(rend);
        }
      }
    }
  }

  if (isnan(gm)) {
    gm = mass*PL_GRAVITATIONAL_CONST;
  }
  // Period will be in years assuming that semiMajor is in au
  double period = plOrbitalPeriod(semiMajor, sys->orbitalBody->GM * gm) / PL_SEC_PER_DAY;

  //  double period = 0.1;//comp_orbital_period_for_planet(semiMajor);
  SGdrawable *drawable = sgNewSphere(moonName.u.str, radius, tex);


  sgSceneAddObj(sc, drawable); // TODO: scale to radius

  PLsystem *moonSys = plNewSubOrbit(sys, sys->scene, moonName.u.str, mass, gm,
                                    period, axialTilt, siderealPeriod,
                                    semiMajor, ooGeoComputeSemiMinor(semiMajor, ecc),
                                    inc, longAscNode, longPerihel, meanLong, radius, flattening);

  quaternion_t q = q_rot(1.0, 0.0, 0.0, DEG_TO_RAD(axialTilt));
  sgSetObjectQuatv(drawable, q);

  plSetDrawable(moonSys->orbitalBody, drawable);
}

void
ooLoadPlanet__(PLworld *world, HRMLobject *obj, SGscene *sc)
{
  assert(obj);
  assert(obj->val.typ == HRMLNode);

  HRMLvalue planetName = hrmlGetAttrForName(obj, "name");

  double mass, radius, siderealPeriod, axialTilt = 0.0, gm = NAN;
  double semiMajor, ecc, inc = NAN, longAscNode = NAN, longPerihel = NAN, meanLong;
  const char *tex = NULL;
  HRMLobject *sats = NULL;
  double flattening = 0.0;

  for (HRMLobject *child = obj->children; child != NULL ; child = child->next) {
    if (!strcmp(child->name, "physical")) {
      for (HRMLobject *phys = child->children; phys != NULL; phys = phys->next) {
        if (!strcmp(phys->name, "mass")) {
          mass = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "radius")) {
          radius = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "sidereal-rotational-period")) {
          siderealPeriod = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "axial-tilt")) {
          axialTilt = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "gm")) {
          gm = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "flattening")) {
          flattening = hrmlGetReal(phys);
        }
      }
    } else if (!strcmp(child->name, "orbit")) {
      for (HRMLobject *orbit = child->children; orbit != NULL; orbit = orbit->next) {
        if (!strcmp(orbit->name, "semimajor-axis")) {
          semiMajor = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "eccentricity")) {
          ecc = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "inclination")) {
          inc = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "longitude-ascending-node")) {
          longAscNode = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "longitude-periapsis")) {
          longPerihel = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "mean-longitude")) {
          meanLong = hrmlGetReal(orbit);
        } else if (!strcmp(orbit->name, "reference-date")) {

        } else {
          fprintf(stderr, "load, invalid orbit token: %s\n", orbit->name);
          assert(0);
        }
      }
    } else if (!strcmp(child->name, "atmosphere")) {

    } else if (!strcmp(child->name, "rendering")) {
      for (HRMLobject *rend = child->children; rend != NULL; rend = rend->next) {
        if (!strcmp(rend->name, "model")) {

        } else if (!strcmp(rend->name, "texture")) {
          tex = hrmlGetStr(rend);
        }
      }
    } else if (!strcmp(child->name, "satellites")) {
      sats = child;
    }
  }

  if (isnan(gm)) {
    gm = mass*PL_GRAVITATIONAL_CONST;
  }

  // NOTE: At present, all plantes must be specified with AUs as parameters
  double period = plOrbitalPeriod(plAuToMetres(semiMajor), world->rootSys->orbitalBody->GM+gm) / PL_SEC_PER_DAY;

  SGdrawable *drawable = sgNewSphere(planetName.u.str, radius, tex);
  sgSceneAddObj(sc, drawable); // TODO: scale to radius
  PLsystem *sys = plNewOrbit(world, sc, planetName.u.str,
                             mass, gm,
                             period, axialTilt, siderealPeriod,
                             plAuToMetres(semiMajor),
                             plAuToMetres(ooGeoComputeSemiMinor(semiMajor, ecc)),
                             inc, longAscNode, longPerihel, meanLong, radius, flattening);
  plSetDrawable(sys->orbitalBody, drawable);
  quaternion_t q = q_rot(1.0, 0.0, 0.0, DEG_TO_RAD(axialTilt));
  sgSetObjectQuatv(drawable, q);
  if (sats) {
    for (HRMLobject *sat = sats->children; sat != NULL; sat = sat->next) {
      if (!strcmp(sat->name, "moon")) {
        ooLoadMoon__(sys, sat, sc);
      }
    }
  }
}


PLworld*
ooLoadStar__(HRMLobject *obj, SGscene *sc)
{
  assert(obj);
  assert(obj->val.typ == HRMLNode);
  HRMLvalue starName = hrmlGetAttrForName(obj, "name");
  double mass = 0.0, gm = NAN;
  double radius, siderealPeriod, axialTilt;
  const char *tex = NULL;
  double flattening = 0.0;

  HRMLobject *sats = NULL;
  for (HRMLobject *child = obj->children; child != NULL ; child = child->next) {
    if (!strcmp(child->name, "satellites")) {
      sats = child;
    } else if (!strcmp(child->name, "physical")) {
      for (HRMLobject *phys = child->children; phys != NULL; phys = phys->next) {
        if (!strcmp(phys->name, "mass")) {
          mass = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "radius")) {
          radius = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "sidereal-rotational-period")) {
          siderealPeriod = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "axial-tilt")) {
          axialTilt = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "gm")) {
          gm = hrmlGetReal(phys);
        } else if (!strcmp(phys->name, "flattening")) {
          flattening = hrmlGetReal(phys);
        }
      }
    } else if (!strcmp(child->name, "rendering")) {
      for (HRMLobject *rend = child->children; rend != NULL; rend = rend->next) {
        if (!strcmp(rend->name, "model")) {

        } else if (!strcmp(rend->name, "texture")) {
          tex = hrmlGetStr(rend);
        }
      }
    }
  }

  if (isnan(gm)) {
    gm = mass*PL_GRAVITATIONAL_CONST;
  }

  sgSetSceneAmb4f(sc, 0.2, 0.2, 0.2, 1.0);
  SGdrawable *drawable = sgNewSphere(starName.u.str, radius, tex);
  SGmaterial *mat = sgSphereGetMaterial((SGsphere*)drawable);
  sgSetMaterialEmiss4f(mat, 1.0, 1.0, 1.0, 0.0);

  SGlight *starLightSource = sgNewPointlight3f(sc, 0.0f, 0.0f, 0.0f);


  sgSceneAddObj(sc, drawable); // TODO: scale to radius
  PLworld *world = plNewWorld(starName.u.str, sc, mass, gm, radius,
                              siderealPeriod, axialTilt, radius, flattening);
  world->rootSys->orbitalBody->lightSource = starLightSource;

  plSetDrawable(world->rootSys->orbitalBody, drawable);
  quaternion_t q = q_rot(1.0, 0.0, 0.0, DEG_TO_RAD(axialTilt));
  sgSetObjectQuatv(drawable, q);

  assert(sats != NULL);
  for (HRMLobject *sat = sats->children; sat != NULL; sat = sat->next) {
    if (!strcmp(sat->name, "planet")) {
      ooLoadPlanet__(world, sat, sc);
    } else if (!strcmp(sat->name, "comet")) {
    }
  }

  return world;
}
PLworld*
ooOrbitLoad(SGscenegraph *sg, const char *fileName)
{
  char *file = ooResGetPath(fileName);
  HRMLdocument *solarSys = hrmlParse(file);
  free(file);
  //HRMLschema *schema = hrmlLoadSchema(ooResGetFile("solarsystem.hrmlschema"));
  //hrmlValidate(solarSys, schema);
  if (solarSys == NULL) {
    // Parser is responsible for pestering the users with errors for now.
    return NULL;
  }

  PLworld *world = NULL;
  // Go through the document and handle each entry in the document
  SGscene *sc = sgNewScene(sg, "main");
  for (HRMLobject *node = hrmlGetRoot(solarSys); node != NULL; node = node->next) {
    if (!strcmp(node->name, "openorbit")) {
      for (HRMLobject *star = node->children; star != NULL; star = star->next) {
        if (!strcmp(star->name, "star")) {
          world = ooLoadStar__(star, sc);
        }
      }
    }
  }

  hrmlFreeDocument(solarSys);

  plSysInit(world->rootSys);
  ooLogInfo("loaded solar system");
  return world;
}

PLobject*
plObjForAstroBody(PLastrobody *abody)
{
  return &abody->obj;
}

// Ugly, but works for now...
float3
plComputeCurrentVelocity(PLastrobody *ab)
{
  double t = ooTimeGetJD();
  float3 upVec = vf3_set(0.0, 0.0, 1.0);
  upVec = v_q_rot(upVec, ab->kepler->qOrbit);
  double velocity = (2.0*M_PI*ab->kepler->a)/(ab->sys->orbitalPeriod*PL_SEC_PER_DAY);
  float3 currentPos = plOrbitPosAtTime(ab->kepler, ab->GM, t*PL_SEC_PER_DAY);
  float3 dir = vf3_normalise(vf3_cross(upVec, currentPos));

  return vf3_s_mul(dir, velocity);

  //float3 currentPos = plOrbitPosAtTime(ab->kepler, ab->GM, t*PL_SEC_PER_DAY);
  //float3 nextPos = plOrbitPosAtTime(ab->kepler, ab->GM, t*PL_SEC_PER_DAY+100.0);
  // return (nextPos - currentPos) / 100.0;
}

