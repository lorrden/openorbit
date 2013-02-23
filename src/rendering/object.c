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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#if __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl3.h>
#endif

#include "rendering/object.h"
#include "rendering/camera.h"
#include "rendering/shader-manager.h"
#include "physics/physics.h"
#include "res-manager.h"
#include <openorbit/log.h>
#include "palloc.h"

#include "3ds.h"
#include "ac3d.h"
#include "cmod.h"
#include "collada.h"

struct sg_geometry_t {
  sg_object_t *obj;
  int gl_primitive_type;
  int vertex_count;

  GLuint vba;
  GLuint vbo;

  bool has_normals;
  bool has_tex_coords;
  GLsizei normal_offset;
  GLsizei tex_coord_offset;

  bool has_indices;
  GLenum index_type;
  GLsizei index_count;
  GLuint ibo;
};

struct sg_object_t {
  struct sg_object_t *parent;
  sg_scene_t *scene;

  lwcoord_t lwc; // Global position
  PLobject *rigidBody;
  float3 pos; // Relative to camera
  float3 dp; // delta pos per time step
  float3 dr; // delta rot per time step
  quaternion_t q; // Quaternion

  float4x4 R;
  float4x4 modelViewMatrix;

  size_t lightCount;
  sg_light_t *lights[SG_OBJ_MAX_LIGHTS];

  size_t texCount;
  sg_texture_t *textures[SG_OBJ_MAX_TEXTURES];

  sg_material_t *material;

  sg_shader_t *shader;
  sg_geometry_t *geometry;
  obj_array_t subObjects;
};

void
sg_object_print(sg_object_t *obj)
{
  ooLogInfo("** object");

  ooLogInfo("\tlwc: [%d %d %d] + [%f %f %f]",
            obj->lwc.seg.x, obj->lwc.seg.y, obj->lwc.seg.z,
            obj->lwc.offs.x, obj->lwc.offs.y, obj->lwc.offs.z);
  ooLogInfo("\tpos: [%f %f %f]", obj->pos.x, obj->pos.y, obj->pos.z);
  ooLogInfo("\tdp:  [%f %f %f]", obj->dp.x, obj->dp.y, obj->dp.z);
}

void
sg_object_add_child(sg_object_t *obj, sg_object_t *child)
{
  obj_array_push(&obj->subObjects, child);
  child->scene = obj->scene;
}
int
sg_objects_compare_dist(sg_object_t const **o0, sg_object_t const **o1)
{
  bool gt = vf3_gt((*o0)->pos, (*o1)->pos);

  if (gt) return -1;
  else return 1;
}

float3
sg_object_get_pos(sg_object_t *obj)
{
  return obj->pos;
}

float3
sg_object_get_vel(sg_object_t *obj)
{
  return obj->dp;
}


void
sg_object_set_pos(sg_object_t *obj, float3 pos)
{
  obj->pos = pos;
}

void
sg_object_get_lwc(sg_object_t *obj, lwcoord_t *lwc)
{
  if (obj->rigidBody) {
    *lwc = obj->rigidBody->p;
    lwc_translate3fv(lwc, obj->rigidBody->p_offset);
    return;
  }

  ooLogError("lwc queiried on object without physical backing");
}


quaternion_t
sg_object_get_quat(sg_object_t *obj)
{
  return obj->q;
}
void
sg_object_set_quat(sg_object_t *obj, quaternion_t q)
{
  obj->q = q;
}

void
sg_object_set_rot(sg_object_t *obj, const float4x4 *r)
{
  memcpy(obj->R, *r, sizeof(float4x4));
}

void
sg_object_set_material(sg_object_t *obj, sg_material_t *mat)
{
  obj->material = mat;
}
void
sg_object_set_scene(sg_object_t *obj, sg_scene_t *sc)
{
  assert(obj != NULL);

  obj->scene = sc;

  ARRAY_FOR_EACH(i, obj->subObjects) {
    sg_object_set_scene(ARRAY_ELEM(obj->subObjects, i), sc);
  }
}

void
sg_geometry_draw(sg_geometry_t *geo)
{
  SG_CHECK_ERROR;

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);

  glBindBuffer(GL_ARRAY_BUFFER, geo->vbo);
  glBindVertexArray(geo->vba);

  // ???
  //glEnableVertexAttribArray(geo->vba);
  //SG_CHECK_ERROR;

  if (geo->has_indices) {
    ooLogInfo("draw %d vertices", (int)geo->index_count);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo->ibo);
    glDrawElements(geo->gl_primitive_type, geo->index_count, geo->index_type, 0);
  } else {
    glDrawArrays(geo->gl_primitive_type, 0, geo->vertex_count);
    SG_CHECK_ERROR;
  }
  //???  glDisableVertexAttribArray(geo->vba);

  glBindVertexArray(0);
  //glBindBuffer(GL_ARRAY_BUFFER, 0);
  SG_CHECK_ERROR;
}

void
sg_object_draw(sg_object_t *obj)
{
  SG_CHECK_ERROR;

  // Set model matrix for current object, we transpose this
  sg_shader_bind(obj->shader);
  sg_shader_invalidate_textures(obj->shader);

  const float4x4 *pm = sg_camera_project(sg_scene_get_cam(obj->scene));
  sg_shader_set_projection(obj->shader, &(*pm)[0]);

  //const float4 *vm = sg_camera_get_view(sg_scene_get_cam(obj->scene));
  sg_shader_set_model_view(obj->shader, obj->modelViewMatrix);

  // Set light params for object
  // TODO: Global ambient light as well...
  for (int i = 0 ; i < obj->lightCount ; i ++) {
    sg_shader_bind_light(obj->shader, i, obj->lights[i]);
  }

  // Set texture params
  for (int i = 0 ; i < obj->texCount ; i ++) {
    sg_shader_bind_texture(obj->shader, obj->textures[i], i);
  }
  //glUniform1iv(obj->shader->texArrayId, SG_OBJ_MAX_TEXTURES, obj->textures);
  // Set material params

  sg_geometry_draw(obj->geometry);
  sg_shader_bind(NULL);

  ARRAY_FOR_EACH(i, obj->subObjects) {
    sg_object_draw(ARRAY_ELEM(obj->subObjects, i));
  }
  SG_CHECK_ERROR;
}

void
sgRecomputeModelViewMatrix(sg_object_t *obj)
{
  if (obj->parent) {
    mf4_cpy(obj->modelViewMatrix, obj->parent->modelViewMatrix);
  } else {
    sg_camera_t *cam = sg_scene_get_cam(obj->scene);
    mf4_cpy(obj->modelViewMatrix, *sg_camera_modelview(cam));
  }

  mf4_mul2(obj->modelViewMatrix, obj->R);
  float4x4 t;
  mf4_make_translate(t, obj->pos);
  mf4_mul2(obj->modelViewMatrix, t);

  ARRAY_FOR_EACH(i, obj->subObjects) {
    sgRecomputeModelViewMatrix(ARRAY_ELEM(obj->subObjects, i));
  }
}

void
sg_object_camera_moved(sg_object_t *obj, float3 cam_dp)
{
  obj->pos -= cam_dp;
}

void
sg_object_animate(sg_object_t *obj, float dt)
{
  obj->q = q_normalise(q_vf3_rot(obj->q, obj->dr, dt));
  q_mf3_convert(obj->R, obj->q);

  lwc_translate3fv(&obj->lwc, obj->dp * dt);
  obj->pos += obj->dp * dt;

  if (obj->parent) {
    mf4_cpy(obj->modelViewMatrix, obj->parent->modelViewMatrix);
  } else {
    mf4_cpy(obj->modelViewMatrix, *sg_camera_modelview(sg_scene_get_cam(obj->scene)));
  }

  float4x4 translate;
  mf4_make_translate(translate, obj->pos);
  mf4_mul2(obj->modelViewMatrix, obj->R);
  mf4_mul2(obj->modelViewMatrix, translate);

  ARRAY_FOR_EACH(i, obj->subObjects) {
    sg_object_animate(ARRAY_ELEM(obj->subObjects, i), dt);
  }
}

// Updatdes an object from the physics system
void
sg_object_update(sg_object_t *obj)
{
  if (obj->rigidBody) {
    obj->dp = plGetVel(obj->rigidBody);
    obj->dr = plGetAngularVel(obj->rigidBody);
    obj->q = plGetQuat(obj->rigidBody);

    // Update position, this is based on where the camera is
    sg_camera_t *cam = sg_scene_get_cam(obj->scene);
    lwcoord_t pos = sg_camera_pos(cam);

    obj->lwc = obj->rigidBody->p;
    obj->pos = lwc_dist(&obj->lwc, &pos);
  }
  q_mf3_convert(obj->R, obj->q);

  if (obj->parent) {
    mf4_cpy(obj->modelViewMatrix, obj->parent->modelViewMatrix);
  } else {
    mf4_cpy(obj->modelViewMatrix, *sg_camera_modelview(sg_scene_get_cam(obj->scene)));
  }

  float4x4 translate;
  mf4_make_translate(translate, obj->pos);
  mf4_mul2(obj->modelViewMatrix, obj->R);
  mf4_mul2(obj->modelViewMatrix, translate);

  ARRAY_FOR_EACH(i, obj->subObjects) {
    sg_object_update(ARRAY_ELEM(obj->subObjects, i));
  }
}



sg_object_t*
sgCreateObject(sg_scene_t *scene)
{
  sg_object_t *obj = smalloc(sizeof(sg_object_t));
  memset(obj, 0, sizeof(sg_object_t));

  obj->parent = NULL;
  obj->scene = scene;

  lwc_set(&obj->lwc, 0.0, 0.0, 0.0);
  obj->pos = vf3_set(0.0, 0.0, 0.0);
  obj->dp = vf3_set(0.0, 0.0, 0.0); // delta pos per time step
  obj->dr = vf3_set(0.0, 0.0, 0.0); // delta rot per time step
  obj->q = q_rot(1.0, 0.0, 0.0, 0.0); // Quaternion

  mf4_ident(obj->R);
  mf4_ident(obj->modelViewMatrix);

  for (int i = 0 ; i < SG_OBJ_MAX_LIGHTS ; i++) {
    obj->lights[i] = NULL;
  }

  obj->shader = NULL;
  obj->geometry = NULL;

  obj_array_init(&obj->subObjects);

  return obj;
}

sg_object_t*
sgCreateSubObject(sg_object_t *parent)
{
  sg_object_t *obj = smalloc(sizeof(sg_object_t));
  memset(obj, 0, sizeof(sg_object_t));

  obj->parent = parent;
  obj->scene = parent->scene;

  obj->pos = vf3_set(0.0, 0.0, 0.0);
  obj->dp = vf3_set(0.0, 0.0, 0.0); // delta pos per time step
  obj->dr = vf3_set(0.0, 0.0, 0.0); // delta rot per time step
  obj->q = q_rot(1.0, 0.0, 0.0, 0.0); // Quaternion

  mf4_ident(obj->R);
  mf4_ident(obj->modelViewMatrix);

  for (int i = 0 ; i < SG_OBJ_MAX_LIGHTS ; i++) {
    obj->lights[i] = NULL;
  }

  obj->shader = NULL;
  obj->geometry = NULL;

  obj_array_init(&obj->subObjects);

  return obj;
}

void
sgObjectSetPos(sg_object_t *obj, float4 pos)
{
  obj->pos = pos;
}

sg_geometry_t*
sg_new_geometry(sg_object_t *obj, int gl_primitive, size_t vertexCount,
                float *vertices, float *normals, float *texCoords,
                size_t index_count, GLenum index_type, void *indices,
                uint8_t *colours)
{
  assert((indices == NULL && index_type == 0) ||
         (indices && ((index_type == GL_UNSIGNED_SHORT) ||
                      (index_type == GL_UNSIGNED_INT))));

  float3 maxvert = vf3_set(0.0, 0.0, 0.0);
  for (size_t i = 0 ; i < vertexCount ; i ++) {
    float3 vert = { vertices[i*3+0], vertices[i*3+1], vertices[i*3+2]};
    if (vf3_abs(vert) > vf3_abs(maxvert)) {
      maxvert = vert;
    }
  }

  ooLogInfo("geometry: |[%f %f %f]| = %f", maxvert.x, maxvert.y, maxvert.z,
            vf3_abs(maxvert));


  SG_CHECK_ERROR;

  sg_geometry_t *geo = smalloc(sizeof(sg_geometry_t));
  memset(geo, 0, sizeof(sg_geometry_t));
  obj->geometry = geo;
  geo->gl_primitive_type = gl_primitive;
  if (normals) geo->has_normals = true;
  if (texCoords) geo->has_tex_coords = true;

  size_t vertexDataSize = sizeof(float) * vertexCount * 3;
  size_t normalDataSize = normals ? sizeof(float) * vertexCount * 3 : 0;
  size_t texCoordDataSize = texCoords ? sizeof(float) * vertexCount * 2 : 0;
  size_t colour_size = (colours) ? vertexCount * 4 : 0;

  size_t buffSize = vertexDataSize + normalDataSize + texCoordDataSize
                  + colour_size;

  glGenVertexArrays(1, &geo->vba);
  glBindVertexArray(geo->vba);
  glGenBuffers(1, &geo->vbo);
  SG_CHECK_ERROR;

  glBindBuffer(GL_ARRAY_BUFFER, geo->vbo);
  glBufferData(GL_ARRAY_BUFFER,
               buffSize,
               NULL, // Just allocate, will copy with subdata
               GL_STATIC_DRAW);

  SG_CHECK_ERROR;


  glBufferSubData(GL_ARRAY_BUFFER, 0, vertexDataSize, vertices);

  sg_shader_t *shader = obj->shader;

  glVertexAttribPointer(SG_VERTEX, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(SG_VERTEX);
  SG_CHECK_ERROR;
  if (normals) {
    glBufferSubData(GL_ARRAY_BUFFER, vertexDataSize, normalDataSize, normals);
    SG_CHECK_ERROR;
    glVertexAttribPointer(SG_NORMAL,
                          3, GL_FLOAT, GL_FALSE, 0, (void*)vertexDataSize);
    SG_CHECK_ERROR;
    glEnableVertexAttribArray(SG_NORMAL);
    SG_CHECK_ERROR;
  }

  if (texCoords) {
    glBufferSubData(GL_ARRAY_BUFFER, vertexDataSize + normalDataSize,
                    texCoordDataSize, texCoords);
    SG_CHECK_ERROR;
    glVertexAttribPointer(SG_TEX_COORD_0,
                          2, GL_FLOAT, GL_FALSE, 0,
                          (void*)vertexDataSize + normalDataSize);
    SG_CHECK_ERROR;
    glEnableVertexAttribArray(SG_TEX_COORD_0);
    SG_CHECK_ERROR;
  }

  if (colours) {
    glBufferSubData(GL_ARRAY_BUFFER, vertexDataSize + normalDataSize
                    + texCoordDataSize,
                    colour_size, colours);
    SG_CHECK_ERROR;
    glVertexAttribPointer(SG_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0,
                          (void*)vertexDataSize + normalDataSize + texCoordDataSize);
    SG_CHECK_ERROR;
    glEnableVertexAttribArray(SG_COLOR);
    SG_CHECK_ERROR;
  }

  if (indices) {
    geo->has_indices = true;
    glGenBuffers(1, &geo->ibo);
    SG_CHECK_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo->ibo);
    SG_CHECK_ERROR;

    if (index_type == GL_UNSIGNED_SHORT) {
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   index_count*sizeof(short), indices, GL_STATIC_DRAW);

    } else {
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   index_count*sizeof(int), indices, GL_STATIC_DRAW);

    }

    geo->index_count = index_count;
    geo->index_type = index_type;
    SG_CHECK_ERROR;
  }
  glBindVertexArray(0); // Done
  SG_CHECK_ERROR;

  return geo;
}

/*
  Uses the shader manager to load the shader and then binds the variables
  we have standardized on. This funciton will be simplified if OS X ever
  supports the explicit attrib loc extensions.
 */
void
sg_object_load_shader(sg_object_t *obj, const char *name)
{
  obj->shader = sg_load_shader(name, name, name, NULL);
}

void
sg_object_set_rigid_body(sg_object_t *obj, PLobject *rigidBody)
{
  if (obj->parent) {
    ooLogWarn("setting rigid body for sg object that is not root");
    return;
  }
  obj->rigidBody = rigidBody;
}
PLobject*
sg_object_get_rigid_body(const sg_object_t *obj)
{
  return obj->rigidBody;
}

sg_object_t*
sg_new_object(sg_shader_t *shader)
{
  sg_object_t *obj = smalloc(sizeof(sg_object_t));
  memset(obj, 0, sizeof(sg_object_t));
  obj->shader = shader;
  obj_array_init(&obj->subObjects);
  obj->q = q_rot(1.0, 0.0, 0.0, 0.0);
  mf4_ident(obj->R);
  mf4_ident(obj->modelViewMatrix);

  return obj;
}

void
sg_object_set_geo(sg_object_t *obj, int gl_primitive, size_t vertexCount,
                  float *vertices, float *normals, float *texCoords)
{
  obj->geometry = sg_new_geometry(obj, gl_primitive,
                                  vertexCount, vertices, normals, texCoords,
                                  0, 0, NULL, NULL);
}


void
sg_object_set_geometry(sg_object_t *obj, sg_geometry_t *geo)
{
  obj->geometry = geo;
  geo->obj = obj;
}

sg_object_t*
sg_new_object_with_geo(sg_shader_t *shader, int gl_primitive, size_t vertexCount,
                       float *vertices, float *normals, float *texCoords)
{
  sg_object_t *obj = smalloc(sizeof(sg_object_t));
  memset(obj, 0, sizeof(sg_object_t));
  obj->shader = shader;
  obj_array_init(&obj->subObjects);
  obj->q = q_rot(1.0, 0.0, 0.0, 0.0);
  mf4_ident(obj->R);
  mf4_ident(obj->modelViewMatrix);

  obj->geometry = sg_new_geometry(obj, gl_primitive,
                                  vertexCount, vertices, normals, texCoords,
                                  0, 0, NULL, NULL);
  return obj;
}

sg_object_t*
sg_load_object(const char *file, sg_shader_t *shader)
{
  assert(file && "not null");
  assert(shader && "not null");

  ooLogInfo("load object '%s'", file);

  char *fullpath = ooResGetPath(file);
  sg_object_t *model = NULL;

  char *dot = strrchr(file, '.');
  if (dot) {
    char *endptr = strchr(dot, '\0');
    assert(endptr != NULL);

    // Dispatch based on file type
    if (!strcmp(dot, ".hrml")) {

    } else if (!strcmp(dot, ".3ds")) {

    } else if (!strcmp(dot, ".dae")) {
      model = collada_load(fullpath);
    } else if (!strcmp(dot, ".ac")) {
      model = ac3d_load(fullpath, shader);
    }
  }

  if (model) sg_object_set_shader(model, shader);
  return model;
}

sg_object_t*
sg_new_sphere(const char *name, sg_shader_t *shader, float radius,
              sg_texture_t *tex, sg_texture_t *nightTex, sg_texture_t *spec,
              sg_material_t *mat)
{
  ooLogInfo("sphere '%s' radius: %f", name, radius);
  // NOTE: Z points upwards
  sg_object_t *sphere = sg_new_object(shader);
  float_array_t verts, texc, normals;
  float_array_init(&verts);
  float_array_init(&normals);
  float_array_init(&texc);
  // 10.0 degree blocks, note,
  // Outer pass for the stacks, as in latitude, inner pass for the longitude

  // Push north pole first
  
  // Texture coordinates for vertex http://en.wikipedia.org/wiki/UV_mapping
  // u = 0.5 - atan2(dz, dx)/2pi  (note assumption is poles on y axis)
  // v = 0.5 - 2 * asin(dy)/2pi
  // Allthouhg, we would like to use a single pole vertex, this is not
  // practical with respect to uv mapping of textures. Therefore we
  // Generate multiple pole coordinates

  int vert_count = 0;
  for (int i = 90 ; i >= -90 ; i -= 10) {
    for (int j = -180 ; j <= 180 ; j += 10) {
      float3 p;
      p.x = radius * sin(DEG_TO_RAD((float)i)) * cos(DEG_TO_RAD((float)j));
      p.y = radius * sin(DEG_TO_RAD((float)i)) * sin(DEG_TO_RAD((float)j));
      p.z = radius * cos(DEG_TO_RAD((float)i));
      float_array_push(&verts, p.x);
      float_array_push(&verts, p.y);
      float_array_push(&verts, p.z);

      float3 n = vf3_normalise(p);
      float_array_push(&normals, n.x);
      float_array_push(&normals, n.y);
      float_array_push(&normals, n.z);

      float u = 0.5 - atan2(-n.x, -n.y) / (2.0 * M_PI);
      float v = 0.5 - 2.0 * asin(-n.z) / (2.0 * M_PI);
      float_array_push(&texc, u); // X
      float_array_push(&texc, v); // Y
      vert_count ++;
    }
  }

  int_array_t indices;
  int_array_init(&indices);
  // For every stack and every slice, we bouild one big triangle strip with
  // degenerate triangles.
  for (int i = 0 ; i < 18; i ++) {
    int_array_push(&indices, ((i + 1)* 18) + 0);
    for (int j = 0 ; j < 36 ; j ++) {
      int_array_push(&indices, ((i + 1)* 18) + j);
      int_array_push(&indices, (i * 18) + j);
    }
    int_array_push(&indices, (i * 18) + 35);
  }

  sg_geometry_t *geo = sg_new_geometry(sphere, GL_TRIANGLE_STRIP, vert_count,
                                       verts.elems, normals.elems, texc.elems,
                                       indices.length, GL_UNSIGNED_INT,
                                       indices.elems, NULL);
  sphere->geometry = geo;

  float_array_dispose(&verts);
  float_array_dispose(&normals);
  float_array_dispose(&texc);
  int_array_dispose(&indices);

  sphere->textures[0] = tex;
  sphere->textures[1] = nightTex;
  sphere->textures[2] = spec;
  sphere->textures[3] = NULL;
  sphere->material = mat;

  sphere->texCount = 0;
  if (tex) sphere->texCount ++;
  if (nightTex) sphere->texCount ++;
  if (spec) sphere->texCount ++;

  return sphere;
}

sg_object_t*
sg_new_ellipse(const char *name, sg_shader_t *shader, float semiMajor,
               float semiMinor, float asc,
               float inc, float argOfPeriapsis,
               float dec, float ra, int segments)
{
#define ITERSTOP 100000
#define STEPSIZE 20.0

  int segs = 30;

  float_array_t verts;
  float_array_init(&verts);

  float ecc = sqrt(1.0-(semiMinor/semiMajor)*(semiMinor/semiMajor));;
  double area = M_PI * semiMajor * semiMinor;

  double sweep = area / (double)segs;

  float_array_push(&verts, semiMajor * cos(0.0) - ecc * semiMajor);
  float_array_push(&verts, semiMinor * sin(0.0));
  float_array_push(&verts, 0.0f);

  double segArea = 0.0, tol = 0.0;
  double prevAngle = 0.0f;
  double newAngle = prevAngle + 1.0*DEG_TO_RAD(360.0/(double)segs);
  double delta;

  for (size_t i = 1 ; i < segs ; i ++) {
    int count = 0;
    segArea = el_segment_area(semiMajor, ecc, prevAngle, newAngle);
    delta = (newAngle-prevAngle);
    do {
      if (segArea > sweep) {
        delta -= delta/STEPSIZE;
      } else {
        delta += delta/STEPSIZE;
      }
      newAngle = prevAngle + delta;
      segArea = el_segment_area(semiMajor, ecc, prevAngle, newAngle);
      tol = fabs(1.0 - segArea/sweep);
      count ++;
    } while (tol > 0.00001 && count < ITERSTOP);

    if (count >= ITERSTOP) {
      ooLogWarn("ellipse segment did not converge in %d iterations", ITERSTOP);
    }
    //segArea = segmentArea(prevAngle, newAngle, semimajor, ecc);

    // Insert vec in array, note that center is in foci
    float_array_push(&verts, semiMajor * cos(newAngle) - ecc * semiMajor);
    float_array_push(&verts, semiMinor * sin(newAngle));
    float_array_push(&verts, 0.0f);

    double nextNewAngle = newAngle + (newAngle-prevAngle);
    prevAngle = newAngle;
    newAngle = nextNewAngle;
  }

  sg_object_t *obj = sg_new_object_with_geo(shader, GL_LINE, verts.length/3,
                                            verts.elems, NULL, NULL);


  float_array_dispose(&verts);


  return obj;
#undef ITERSTOP
#undef STEPSIZE

}

sg_object_t*
sg_new_axises(const char *name, sg_shader_t *shader, float length)
{
  float axis[] = {0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f};
  sg_object_t *obj = sg_new_object_with_geo(shader, GL_LINE, 6, axis, NULL, NULL);
  return obj;
}

sg_object_t*
sg_new_cube(const char *name, sg_shader_t *shader, float side)
{
  GLfloat cube_vertices[] = {
    // front
    -1.0, -1.0,  1.0,
    1.0, -1.0,  1.0,
    1.0,  1.0,  1.0,
    -1.0,  1.0,  1.0,
    // back
    -1.0, -1.0, -1.0,
    1.0, -1.0, -1.0,
    1.0,  1.0, -1.0,
    -1.0,  1.0, -1.0,
  };

  // TODO
  GLubyte cube_colors[] = {
    // front colors
    255,   0,   0, 255,
      0, 255,   0, 255,
      0,   0, 255, 255,
    255, 255, 255, 255,
    // back colors
    255,   0,   0, 255,
      0, 255,   0, 255,
      0,   0, 255, 255,
    255, 255, 255, 255,
  };
  GLshort cube_elements[] = {
    // front
    0, 1, 2,
    2, 3, 0,
    // top
    1, 5, 6,
    6, 2, 1,
    // back
    7, 6, 5,
    5, 4, 7,
    // bottom
    4, 0, 3,
    3, 7, 4,
    // left
    4, 5, 1,
    1, 0, 4,
    // right
    3, 2, 6,
    6, 7, 3,
  };

  for (int i = 0 ; i < sizeof(cube_vertices)/sizeof(float) ; i ++) {
    cube_vertices[i] *= side/2.0;
  }

  sg_object_t *obj = sg_new_object(sg_get_shader("flat"));
  sg_geometry_t *geo = sg_new_geometry(obj, GL_TRIANGLES,
                                       sizeof(cube_vertices)/(sizeof(float)*3),
                                       cube_vertices, NULL, NULL,
                                       sizeof(cube_elements)/sizeof(GLshort),
                                       GL_UNSIGNED_SHORT,
                                       cube_elements,
                                       cube_colors);
  sg_object_set_geometry(obj, geo);

  return obj;
}

void
sg_object_set_shader(sg_object_t *obj, sg_shader_t *shader)
{
  obj->shader = shader;
}

void
sg_object_set_shader_by_name(sg_object_t *obj, const char *shname)
{
  obj->shader = sg_get_shader(shname);
}
