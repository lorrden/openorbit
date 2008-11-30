/* 
    The contents of this file are subject to the Mozilla Public License
    Version 1.1 (the "License"); you may not use this file except in compliance
    with the License. You may obtain a copy of the License at
    http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS IS" basis,
    WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
    for the specific language governing rights and limitations under the
    License.

    The Original Code is the Open Orbit space flight simulator.

    The Initial Developer of the Original Code is Mattias Holm. Portions
    created by the Initial Developer are Copyright (C) 2006 the
    Initial Developer. All Rights Reserved.

    Contributor(s):
        Mattias Holm <mattias.holm(at)contra.nu>.

    Alternatively, the contents of this file may be used under the terms of
    either the GNU General Public License Version 2 or later (the "GPL"), or
    the GNU Lesser General Public License Version 2.1 or later (the "LGPL"), in
    which case the provisions of GPL or the LGPL License are applicable instead
    of those above. If you wish to allow use of your version of this file only
    under the terms of the GPL or the LGPL and not to allow others to use your
    version of this file under the MPL, indicate your decision by deleting the
    provisions above and replace  them with the notice and other provisions
    required by the GPL or the LGPL.  If you do not delete the provisions
    above, a recipient may use your version of this file under either the MPL,
    the GPL or the LGPL."
 */

#include <stdarg.h>
#include <stdint.h>

#include "res-manager.h"
 

#include <Python.h>

extern void initio(void);
extern void initconfig(void);
extern void initres(void);
extern void initenvironment(void);
extern void inittexture(void);
extern void initsg(void);
extern void initsim(void);

 
#include "scripting.h"

bool
ooScriptingInit(void)
{
//    Py_InitializeEx(0);
    Py_Initialize();

    initio();
    initconfig();
    initres();
    inittexture();
    initenvironment();
    initsg();
    initsim();
    
    
    
    oo_error_t err = ooScriptingRunInit();
    
    if (err) {
        oo_print_err_msg(err);
        return true;
    }
    
    return false;
}

void
ooScriptingFinalise(void)
{
    Py_Finalize();
}

oo_error_t
ooScriptingRunInit(void)
{    
    FILE *fp = ooResGetFile(SCR_INIT_SCRIPT_NAME);
    
    if (! fp) {
        fprintf(stderr, "could not open %s\n", SCR_INIT_SCRIPT_NAME);
        return ERR_file_not_found;
    }
    if (PyRun_SimpleFile(fp, SCR_INIT_SCRIPT_NAME)) {
        fclose(fp);
        return ERR_script;
    }
    
    fclose(fp);
    return ERR_none;
}

bool
ooScriptingRunPostInit(void)
{
    FILE *fp = ooResGetFile(SCR_POST_INIT_SCRIPT_NAME);
    
    if (! fp) {
        fprintf(stderr, "could not open %s\n", SCR_POST_INIT_SCRIPT_NAME);
        return false;
    }
    if (PyRun_SimpleFile(fp, SCR_POST_INIT_SCRIPT_NAME)) {
        fprintf(stderr, "execution of script %s failed\n", SCR_POST_INIT_SCRIPT_NAME);
        fclose(fp);
        return false;
    }
    
    fclose(fp);
    return true;
}

bool
ooScriptingRunFile(const char *fname)
{
  FILE *fp = ooResGetFile(fname);
  
  if (! fp) {
      fprintf(stderr, "could not open %s\n", fname);
      return false;
  }
  if (PyRun_SimpleFile(fp, fname)) {
      fprintf(stderr, "execution of script %s failed\n", fname);
      fclose(fp);
      return false;
  }
  
  fclose(fp);
  return true;  
}
