cdef extern from "ode/ode.h":
    ctypedef struct dxBody
    ctypedef dxBody *dBodyID


cdef extern from "rendering/scenegraph.h":
  ctypedef void OOobject
  ctypedef void (*OOdrawfunc)(OOobject*)

  ctypedef struct OOscene

  ctypedef struct OOcam:
    OOscene *scene

  ctypedef struct OOscenegraph:
    OOscene *root

  ctypedef struct OOdrawable:
    OOobject *obj
    OOdrawfunc draw
  void ooSgSetSky(OOscenegraph *sg, OOdrawable *obj)
  void ooSgSetCam(OOscenegraph *sg, OOcam *cam)

  OOscenegraph* ooSgNewSceneGraph()
  void ooSgSetCam(OOscenegraph *sg, OOcam *cam)

  OOscene* ooSgNewScene(OOscene *parent, char *name)

  void ooSgSceneAddChild(OOscene *parent, OOscene *child)


  OOcam* ooSgNewFreeCam(OOscenegraph *sg, OOscene *sc,
                       float x, float y, float z,
                       float rx, float ry, float rz)
  OOcam* ooSgNewFixedCam(OOscenegraph *sg, OOscene *sc, dBodyID body,
                         float dx, float dy, float dz, 
                         float rx, float ry, float rz)

  OOcam* ooSgNewOrbitCam(OOscenegraph *sg, OOscene *sc, dBodyID body,
                         float dx, float dy, float dz)



cdef extern from "rendering/sky.h":
    ctypedef struct OOstars
    void ooSkyAddStar(OOstars *obj, double ra, double dec, double mag, double bv)
    OOstars* ooSkyInitStars(int starCount)
    OOstars *ooSkyRandomStars()
    OOdrawable *ooSkyNewDrawable(char *fileName)
    OOstars* ooSkyLoadStars(char *fileName)


cdef extern from "physics/orbit.h":
    ctypedef struct OOorbsys:
        char *name

    OOorbsys* ooOrbitLoad(OOscenegraph *scg, char *file)


cdef extern from "rendering/texture.h":
    ctypedef struct OOtexture:
        unsigned texId
        
    int ooTexLoad(char *key, char *name)
    int ooTexBind(char *key)
    int ooTexNum(char *key)
    int ooTexUnload(char *key)
    OOtexture* ooTexGet(char *key)

cdef extern from "sim.h":
    void ooSimSetSg(OOscenegraph *sg)
    void ooSimSetCam(OOcam *cam)
    void ooSimSetOrbSys(OOorbsys *osys)

    cdef struct SIMstate:
      OOorbsys *orbSys
      OOscenegraph *sg

cdef extern from "res-manager.h":
    char* ooResGetPath(char *fileName)

cdef extern from "stdlib.h":
    void free(void*)


cdef extern from "plugin-handler.h":
    int ooPluginLoadAll()
    char *ooPluginLoad(char *filename)
    void ooPluginUnload(char *key)


cdef extern from "settings.h":
  int ooConfSetBoolAsInt(char *key, int val)
  int ooConfGetBoolAsInt(char *key, int *val)    
  int ooConfSetInt(char *key, int val)
  int ooConfGetInt(char *key, int *val)
  int ooConfSetFloat(char *key, float val)
  int ooConfGetFloat(char *key, float *val)
  int ooConfSetStr(char *key, char *val)
  char* ooConfGetStr(char *key)

cdef class OdeBody:
  cdef dBodyID body
  def __cinit__(self):
    self.body = <dBodyID>0 

#def LoadAllPlugins():
#   ooPluginLoadAll()

#def LoadPlugin(name):
#    return ooPluginLoad(name)

#def UnloadPlugin(key):
#    ooPluginUnload(key)


cdef class Texture:
    cdef OOtexture *tex
    def __cinit__(self, key):
        self.tex = ooTexGet(key)

cdef class SkyDrawable:
    cdef OOdrawable *sky
    def __cinit__(self, char *fileName):
      self.sky = ooSkyNewDrawable(fileName)

def loadTexture(char *key, char *fileName):
    status = ooTexLoad(key, fileName)
    if status != 0:
      print "Texture not found"

def unloadTexture(char *key):
    ooTexUnload(key)

cdef class Scene:
   cdef OOscene *sc

cdef class Cam:
  cdef OOcam *cam
  def __cinit__(self):
    self.cam = <OOcam*>0

cdef class Scenegraph:
  cdef OOscenegraph *sg
  def __cinit__(self):
      self.sg = <OOscenegraph*>0
  
  def new(self):
      self.sg = ooSgNewSceneGraph()
      return self

  def setCam(self, Cam cam):
      ooSgSetCam(self.sg, cam.cam)
      
  def getRoot(self):
      cdef Scene sc
      sc = Scene()
      sc.sc = self.sg.root
      return sc
  
  def getScene(self, key):
      pass
  
  def setBackground(self, SkyDrawable sky):
      ooSgSetSky(self.sg, sky.sky)
  
  def setOverlay(self):
      pass

cdef class FreeCam(Cam):
  def __cinit__(self):#, node, x, y, z, rx, ry, rz):
    pass
  def setParams(self, Scenegraph sg, Scene sc, x, y, z, rx, ry, rz):
    self.cam = ooSgNewFreeCam(sg.sg, sc.sc, x, y, z, rx, ry, rz)

cdef class FixedCam(Cam):
  def __cinit__(self, Scenegraph sg, Scene sc, OdeBody body, dx, dy, dz, rx, ry, rz):
    self.cam = ooSgNewFixedCam(sg.sg, sc.sc, body.body, dx, dy, dz, rx, ry, rz)

cdef class OrbitCam(Cam):
  def __cinit__(self, Scenegraph sg, Scene sc, OdeBody body, dx, dy, dz):
    self.cam = ooSgNewOrbitCam(sg.sg, sc.sc, body.body, dx, dy, dz)



cdef class OrbitSys:
  cdef OOorbsys *osys
  def __cinit__(self):
    self.osys = <OOorbsys*>0
  def __dealloc__(self):
    # C function call to delete obj_sys object.
    pass
  def new(self, Scenegraph scg, char *fileName):
    self.osys = ooOrbitLoad(scg.sg, fileName)
    return self


def setSg(Scenegraph scg):
    ooSimSetSg(scg.sg)

def setOrbSys(OrbitSys sys):
    ooSimSetOrbSys(sys.osys)


def getResPath(str):
    cdef char *path
    path = ooResGetPath(str)
    pstr = path
    free(path)
    return pstr