/*
 Copyright 2012,2013 Mattias Holm <lorrden(at)openorbit.org>

 This file is part of Open Orbit.

 Open Orbit is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Open Orbit is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Open Orbit.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef orbit_scenegraph2_h
#define orbit_scenegraph2_h

#include <OpenGL/gl3.h>
#include <gencds/array.h>
#include <vmath/vmath.h>
#include "rendering/types.h"
#include "rendering/material.h"
#include "rendering/light.h"
#include "rendering/scenegraph.h"
#include "physics/reftypes.h"

int sg_objects_compare_dist(sg_object_t const **o0, sg_object_t const **o1);

void sg_object_add_child(sg_object_t *obj, sg_object_t *child);

void sg_object_set_pos(sg_object_t *obj, float3 pos);
float3 sg_object_get_pos(sg_object_t *obj);
void sg_object_get_lwc(sg_object_t *obj, lwcoord_t *lwc);
float3 sg_object_get_vel(sg_object_t *obj);
void sg_object_camera_moved(sg_object_t *obj, float3 cam_dp);

void sg_object_print(sg_object_t *obj);

quaternion_t sg_object_get_quat(sg_object_t *obj);
void sg_object_set_quat(sg_object_t *obj, quaternion_t q);

void sg_object_set_rot(sg_object_t *obj, const float4x4 *r);
void sg_object_set_scene(sg_object_t *obj, sg_scene_t *sc);

void sg_object_set_material(sg_object_t *obj, sg_material_t *mat);
void sg_object_set_geo(sg_object_t *obj, int gl_primitive, size_t vertexCount,
                       float *vertices, float *normals, float *texCoords);

sg_geometry_t*
sg_new_geometry(sg_object_t *obj, int gl_primitive, size_t vertexCount,
                float *vertices, float *normals, float *texCoords,
                size_t index_count, GLenum index_type, void *indices,
                uint8_t *colours);

sg_object_t* sg_new_ellipse(const char *name, sg_shader_t *shader,
                            float semiMajor,
                            float semiMinor, float asc,
                            float inc, float argOfPeriapsis,
                            float dec, float ra, int segments);

sg_object_t* sg_new_sphere(const char *name, sg_shader_t *shader, float radius,
                           sg_texture_t *tex, sg_texture_t *nightTex,
                           sg_texture_t *spec,
                           sg_material_t *mat);
sg_object_t* sg_new_cube(const char *name, sg_shader_t *shader, float side);

sg_object_t* sg_new_object_with_geo(sg_shader_t *shader,
                                    int gl_primitive, size_t vertexCount,
                                    float *vertices, float *normals, float *texCoords);
sg_object_t* sg_new_object(sg_shader_t *shader);

void sg_object_animate(sg_object_t *obj, float dt); // Linear interpolation between frames
void sg_object_update(sg_object_t *obj); // Pull from physis system

void sg_object_recompute_modelviewmatrix(sg_object_t *obj);
void sg_object_draw(sg_object_t *obj);
void sg_geometry_draw(sg_geometry_t *geo);

void sg_object_set_rigid_body(sg_object_t *obj, PLobject *rigidBody);
PLobject* sg_object_get_rigid_body(const sg_object_t *obj);


sg_object_t* sg_load_object(const char *file, sg_shader_t *shader);

void sg_object_set_shader(sg_object_t *obj, sg_shader_t *shader);
void sg_object_set_shader_by_name(sg_object_t *obj, const char *shname);

void sg_object_add_light(sg_object_t *obj, sg_light_t *light);
#endif
