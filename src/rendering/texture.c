/*
  Copyright 2006,2009 Mattias Holm <mattias.holm(at)openorbit.org>

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



#include <stdlib.h>
#include <assert.h>
#include <gencds/hashtable.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#else
#include <gl/gl.h>
#endif

#include "log.h"
#include "texture.h"
#include <imgload/image.h>

#include "res-manager.h"

#include <string.h>

static hashtable_t *gOOtexDict;

static void __attribute__((constructor))
texInit(void)
{
    gOOtexDict = hashtable_new_with_str_keys(128);
}

int
ooTexLoad(const char *key, const char *name)
{
  if (hashtable_lookup(gOOtexDict, key)) {
    ooLogWarn("Tried to load texture '%s' which is already loaded", key);
    return -1;
  }
  char *fname = ooResGetPath(name);
  if (fname == NULL) return -1;

  image_t img;

  OOtexture *tex = malloc(sizeof(OOtexture));
  if (tex == NULL) {free(fname); return -1;}

  int res = img_load(&img, fname);
  if (res != 0) {free(fname);free(tex);return -1;}
  tex->path = strdup(fname);
  switch (img.kind) {
  case IMG_BGRA:
    tex->texType = GL_BGRA;
    tex->bytesPerTex = 4;
    break;
  case IMG_BGR:
    tex->texType = GL_BGR;
    tex->bytesPerTex = 3;
    break;
  case IMG_RGB:
    tex->texType = GL_RGB;
    tex->bytesPerTex = 3;
    break;
  case IMG_RGBA:
    tex->texType = GL_RGBA;
    tex->bytesPerTex = 4;
    break;
  case IMG_GRAY8:
    tex->texType = GL_LUMINANCE;
    tex->bytesPerTex = 1;
    break;
  default:
    assert(0 && "invalid case");
  }

  tex->width = img.w;
  tex->height = img.h;
  tex->data = img.data;

  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &tex->texId);
  glBindTexture(GL_TEXTURE_2D, tex->texId);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  GLint err;
  if (err = gluBuild2DMipmaps(GL_TEXTURE_2D,
                              tex->bytesPerTex,
                              tex->width, tex->height,
                              tex->texType,
                              GL_UNSIGNED_BYTE,
                              tex->data) )
  {
    ooLogFatal("failed mipmap generation %s", gluErrorString(err));
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);

  // Do not retain texture, is is loaded into GL texture memory
  free(tex->data);
  tex->data = NULL;

  hashtable_insert(gOOtexDict, key, tex);
  free(fname);
  return 0;
}

int
ooTexBind(const char *key)
{
  OOtexture *tex = hashtable_lookup(gOOtexDict, key);
  if (tex != NULL) {
    glBindTexture(GL_TEXTURE_2D, tex->texId);
    return 0;
  }

  return -1;
}

GLuint
ooTexNum(const char *key)
{
  OOtexture *tex = hashtable_lookup(gOOtexDict, key);

  if (tex != NULL) {
    return tex->texId;
  }

  return 0;
}

int
ooTexUnload(const char *key)
{
  OOtexture *tex = hashtable_lookup(gOOtexDict, key);

  if (tex == NULL) {
      return -1;
  }

  hashtable_remove(gOOtexDict, key);
  glDeleteTextures(1, &tex->texId);

  free(tex->data);
  free(tex);

  return 0;
}

OOtexture*
ooTexGet(const char *key)
{
  return hashtable_lookup(gOOtexDict, key);
}

