/*
 Copyright 2012 Mattias Holm <mattias.holm(at)openorbit.org>

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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "gl3drawable.h"
#include "shader-manager.h"
#include "physics/physics.h"
#include <OpenGL/gl3.h>
#include <openorbit/log.h>

void
sgDrawGeometry(sg_geometry_t *geo)
{
  //  SG_CHECK_ERROR;

  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);

  glBindBuffer(GL_ARRAY_BUFFER, geo->vbo);
  glBindVertexArray(geo->vba);
  glEnableVertexAttribArray(geo->vba);

  glDrawArrays(GL_TRIANGLES, 0, geo->vertexCount);

  glDisableVertexAttribArray(geo->vba);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  //SG_CHECK_ERROR;
}

void
sgDrawObject(sg_object_t *obj)
{
  // Set model matrix for current object, we transpose this
  glUseProgram(obj->shader->shaderId);

  glUniformMatrix4fv(obj->shader->uniforms.projectionId, 1, GL_TRUE,
                     (GLfloat*)obj->scene->cam->proj_matrix);

  glUniformMatrix4fv(obj->shader->uniforms.modelViewId, 1, GL_TRUE,
                     (GLfloat*)obj->modelViewMatrix);

  // Set light params for object
  for (int i = 0 ; i < obj->lightCount ; i ++) {
    glUniform3fv(obj->shader->uniforms.lightIds[i].pos, 1,
                 (GLfloat*)&obj->lights[i]->pos);
    glUniform4fv(obj->shader->uniforms.lightIds[i].ambient, 1,
                 (GLfloat*)&obj->lights[i]->ambient);
    glUniform4fv(obj->shader->uniforms.lightIds[i].specular, 1,
                 (GLfloat*)&obj->lights[i]->specular);
    glUniform4fv(obj->shader->uniforms.lightIds[i].diffuse, 1,
                 (GLfloat*)&obj->lights[i]->diffuse);
    glUniform3fv(obj->shader->uniforms.lightIds[i].dir, 1,
                 (GLfloat*)&obj->lights[i]->dir);

    glUniform1f(obj->shader->uniforms.lightIds[i].constantAttenuation,
                obj->lights[i]->constantAttenuation);
    glUniform1f(obj->shader->uniforms.lightIds[i].linearAttenuation,
                obj->lights[i]->linearAttenuation);
    glUniform1f(obj->shader->uniforms.lightIds[i].quadraticAttenuation,
                obj->lights[i]->quadraticAttenuation);

    glUniform4fv(obj->shader->uniforms.lightIds[i].globAmbient, 1,
                 (GLfloat*)&obj->lights[i]->globAmbient);
  }

  // Set texture params
  for (int i = 0 ; i < obj->texCount ; i ++) {
    glUniform1i(obj->shader->uniforms.texIds[i], obj->textures[i]);
  }
  //glUniform1iv(obj->shader->texArrayId, SG_OBJ_MAX_TEXTURES, obj->textures);
  // Set material params
  sgDrawGeometry(obj->geometry);
  glUseProgram(0);

  ARRAY_FOR_EACH(i, obj->subObjects) {
    sgDrawObject(ARRAY_ELEM(obj->subObjects, i));
  }
}

void
sgRecomputeModelViewMatrix(sg_object_t *obj)
{
  if (obj->parent) {
    mf4_cpy(obj->modelViewMatrix, obj->parent->modelViewMatrix);
  } else {
    mf4_cpy(obj->modelViewMatrix, obj->scene->cam->view_matrix);
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
sgAnimateObject(sg_object_t *obj, float dt)
{
  obj->q = q_normalise(q_vf3_rot(obj->q, obj->dr, dt));
  q_mf3_convert(obj->R, obj->q);
  obj->pos += obj->dp;

  ARRAY_FOR_EACH(i, obj->subObjects) {
    sgAnimateObject(ARRAY_ELEM(obj->subObjects, i), dt);
  }
}

void
sgUpdateObject(sg_object_t *obj)
{
  if (obj->rigidBody) {
    obj->dp = plGetVel(obj->rigidBody);
    obj->dr = plGetAngularVel(obj->rigidBody);
    obj->q = plGetQuat(obj->rigidBody);
  }

  //ARRAY_FOR_EACH(i, obj->subObjects) {
  //  sgUpdateObject(ARRAY_ELEM(obj->subObjects, i));
  //}
}



sg_object_t*
sgCreateObject(sg_scene_t *scene)
{
  sg_object_t *obj = malloc(sizeof(sg_object_t));
  memset(obj, 0, sizeof(sg_object_t));

  obj->parent = NULL;
  obj->scene = scene;

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
  sg_object_t *obj = malloc(sizeof(sg_object_t));
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
sg_geometry_create(sg_object_t *obj, size_t vertexCount,
                   float *vertices, float *normals, float *texCoords)
{
  sg_geometry_t *geo = malloc(sizeof(sg_geometry_t));
  memset(geo, 0, sizeof(sg_geometry_t));
  obj->geometry = geo;

  if (normals) geo->hasNormals = true;
  if (texCoords) geo->hasTexCoords = true;

  size_t vertexDataSize = sizeof(float) * vertexCount * 3;
  size_t normalDataSize = normals ? sizeof(float) * vertexCount * 3 : 0;
  size_t texCoordDataSize = texCoords ? sizeof(float) * vertexCount * 2 : 0;
  size_t buffSize = vertexDataSize + normalDataSize + texCoordDataSize;

  glGenVertexArrays(1, &geo->vba);
  glBindVertexArray(geo->vba);
  glGenBuffers(1, &geo->vbo);

  glBindBuffer(GL_ARRAY_BUFFER, geo->vbo);
  glBufferData(GL_ARRAY_BUFFER,
               buffSize,
               NULL, // Just allocate, will copy with subdata
               GL_STATIC_DRAW);



  glBufferSubData(GL_ARRAY_BUFFER, 0, vertexDataSize, vertices);

  sg_shader_t *shader = obj->shader;

  glVertexAttribPointer(sgGetLocationForParam(shader->shaderId, SG_VERTEX), 4, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(sgGetLocationForParam(shader->shaderId, SG_VERTEX));
  if (normals) {
    glBufferSubData(GL_ARRAY_BUFFER, vertexDataSize, normalDataSize, normals);
    glVertexAttribPointer(sgGetLocationForParam(shader->shaderId, SG_NORMAL), 3, GL_FLOAT, GL_FALSE, 0,
                          (void*)vertexDataSize);
    glEnableVertexAttribArray(sgGetLocationForParam(shader->shaderId, SG_NORMAL));
  }
  if (texCoords) {
    glBufferSubData(GL_ARRAY_BUFFER, vertexDataSize + normalDataSize,
                    texCoordDataSize, texCoords);
    glVertexAttribPointer(sgGetLocationForParamAndIndex(shader->shaderId, SG_TEX, 0), 2, GL_FLOAT, GL_FALSE, 0,
                          (void*)vertexDataSize + normalDataSize);
    glEnableVertexAttribArray(sgGetLocationForParamAndIndex(shader->shaderId, SG_TEX, 0));
  }

  glBindVertexArray(0); // Done

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
  obj->shader = sgLoadProgram(name, name, name, NULL);
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


sg_object_t*
sg_load_object(const char *file)
{
  // TODO: Fill in code
  return NULL;
}

sg_object_t*
sg_create_sphere(const char *name, GLuint shader, float radius,
               GLuint tex, GLuint nightTex, float spec,
               sg_material_t *mat)
{
  return NULL;
}

sg_object_t*
sg_create_ellipse(const char *name, float semiMajor,
                float semiMinor, float asc,
                float inc, float argOfPeriapsis,
                float dec, float ra, int segments)
{
  return NULL;
}

