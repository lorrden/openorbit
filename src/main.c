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

/*! \mainpage Open Orbit Source Documentation 
 *
 * \section intro_sec Introduction
 *
 * Open Orbit is an interactive space flight simulator, intended to allow the
 * user experience the wonders of space flight within the solar system. The
 * simulator works with newtonian mechanics. This differs from some other space
 * simulators, that some times use an atmospheric flight model, despite being
 * in vacuum, and microgravity.
 * 
 * \section build_sec Building
 * 
 * The build procedure is documented separatelly. See the file INSTALL.txt for
 * more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#include "error.h"
#include "settings.h"
#include "io-manager.h"
#include "plugin-handler.h"
#include "sim.h"
#include "rendering/render.h"
#include "rendering/camera.h"


#include "SDL.h"



extern settings_t SETTINGS;


static void
main_loop(void)
{
    SDL_Event event;
    int done = 0;
  
//    SDL_TimerID sim_step_timer =
    SDL_AddTimer(SIM_STEP_PERIOD, sim_step_event, NULL);
        
    while ( !done ) {
        
		/* Check for events, will do the initial io-decoding */
		while ( SDL_PollEvent (&event) ) {
//            io_event_t e;
			switch (event.type) {
            case SDL_ACTIVEEVENT:
                break;
            case SDL_MOUSEMOTION:
                if (event.motion.state) {
#if 0
                    e.kind = IO_EV_Drag;
                    e.u.drag.button = SDL_BUTTON(event.motion.state);
                    e.u.drag.dx = (float)event.motion.xrel;
                    e.u.drag.dy = (float)event.motion.yrel;
                    io_dispatch_event(&e);
#endif
                    io_handle_mouse_drag(SDL_BUTTON(event.motion.state), 
                                         (float)event.motion.xrel,
                                         (float)event.motion.yrel);                        
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
#if 0
                e.kind = IO_EV_Click;
                e.u.click.button = event.button.button;
                e.u.click.down = true;
                e.u.click.x = (float)event.button.x;
                e.u.click.y = (float)event.button.y;
                io_dispatch_event(&e);
#endif
                io_handle_mouse_down(event.button.button,
                                    (float)event.button.x,
                                    (float)event.button.y);
                break;
            case SDL_MOUSEBUTTONUP:
#if 0
                e.kind = IO_EV_Click;
                e.u.click.button = event.button.button;
                e.u.click.down = false;
                e.u.click.x = (float)event.button.x;
                e.u.click.y = (float)event.button.y;
                io_dispatch_event(&e);
#endif
                
                io_handle_mouse_up(event.button.button,
                                   (float)event.button.x,
                                   (float)event.button.y);
                break;
            case SDL_KEYDOWN:
                io_handle_key_down(event.key.keysym.sym,
                                   event.key.keysym.mod);
                break;
            case SDL_KEYUP:
                if (event.key.keysym.sym == SDLK_q) done = 1;
                else io_handle_key_up(event.key.keysym.sym,
                                      event.key.keysym.mod);
                break;
            case SDL_JOYAXISMOTION:
            case SDL_JOYBALLMOTION:
            case SDL_JOYHATMOTION:
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            case SDL_VIDEORESIZE:
            case SDL_VIDEOEXPOSE:
            case SDL_USEREVENT:
                switch (event.user.code) {
                case SIM_STEP_EVENT:
                    sim_step();
                    break;
                case SIM_DEBUG_EVENT:
                    break;
                default:
                    break;
                }
                break;
            case SDL_QUIT:
                done = 1;
                break;
            default:
                break;
			}
		}
        
        // draw as often as possible, should interpolate between steps depending
        // on passed time
        render_scene();
	}
}


void
inner_main(void *data, int argc, char *argv[])
{    
    // Setup IO-tables
    io_manager_init();
    init_cam();
    
    if (! init_plugin_manager() ) exit(1);
    
    // Load and run initialisation script
    if (init_scripting()) {
        fprintf(stderr, "Failed to init scripting system\n");
        exit(1);
    }
	
    // Initialise SDL, GL and AL
    
	// Init SDL video subsystem
	if ( SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK) < 0 ) {
        fprintf(stderr, "Couldn't initialize SDL: %s\n",
                SDL_GetError());
		exit(1);
	}
    
    atexit(SDL_Quit);
    
    // Init GL state
    init_renderer();
        
    // Draw, get events...
    main_loop();
    
	printf("Shutting down normally...");
    exit(0);
    
}

int
main(int argc, char *argv[])
{
#ifdef WITH_GUILE
    scm_boot_guile(argc, argv, inner_main, NULL); // never returns
#else
    inner_main(NULL, argc, argv); // never returns
#endif /* ! WITH_GUILE */
    
    return 0;
}
