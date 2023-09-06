
extern int main_mobile (int argc, char *argv[]);

#include "wl_def.h"
#include "wl_play.h"

#include "game_interface.h"

#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include "SDL.h"
#include "SDL_keycode.h"

#ifndef LOGI
#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"JNI", __VA_ARGS__))
#endif


extern "C"
{

extern int SDL_SendKeyboardKey(Uint8 state, SDL_Scancode scancode);
extern void androidWaitFrames(int frames);

int PortableKeyEvent(int state, int code, int unicode){
	//LOGI("PortableKeyEvent %d %d %d",state,code,unicode);

	if (state)
		SDL_SendKeyboardKey(SDL_PRESSED, (SDL_Scancode)code);
	else
		SDL_SendKeyboardKey(SDL_RELEASED, (SDL_Scancode) code);

	return 0;

}

bool my_buttonstate[NUMBUTTONS];

static bool alwaysrun = false;

void PortableAction(int state, int action)
{
	LOGI("PortableAction %d   %d",state,action);

	if (((action >= PORT_ACT_MENU_UP) && (action <= PORT_ACT_MENU_ABORT)) &&
	    (( PortableGetScreenMode() == TS_MENU ) || ( PortableGetScreenMode() == TS_Y_N )))
	{
	    SDL_Scancode scanCode = SDL_SCANCODE_UNKNOWN;

	    switch (action)
		{
		case PORT_ACT_MENU_UP:
		    scanCode = SDL_SCANCODE_UP;
		    break;
        case PORT_ACT_MENU_DOWN:
            scanCode = SDL_SCANCODE_DOWN;
            break;
        case PORT_ACT_MENU_LEFT:
            scanCode = SDL_SCANCODE_LEFT;
            break;
        case PORT_ACT_MENU_RIGHT:
            scanCode = SDL_SCANCODE_RIGHT;
            break;
        case PORT_ACT_MENU_SELECT:
            scanCode = SDL_SCANCODE_RETURN;
            break;
        case PORT_ACT_MENU_BACK:
            scanCode = SDL_SCANCODE_ESCAPE;
            break;
        case PORT_ACT_MENU_CONFIRM:
            scanCode = SDL_SCANCODE_Y;
            break;
        case PORT_ACT_MENU_ABORT:
            scanCode = SDL_SCANCODE_N;
            break;
		}

		if( scanCode != SDL_SCANCODE_UNKNOWN )
			PortableKeyEvent( state, scanCode, 0);
	}
	else {
        int key = -1;

        switch (action) {
            case PORT_ACT_LEFT:
                key = bt_turnleft;
                break;
            case PORT_ACT_RIGHT:
                key = bt_turnright;
                break;
            case PORT_ACT_FWD:
                key = bt_moveforward;
                break;
            case PORT_ACT_BACK:
                key = bt_movebackward;
                break;
            case PORT_ACT_MOVE_LEFT:
                key = bt_strafeleft;
                break;
            case PORT_ACT_MOVE_RIGHT:
                key = bt_straferight;
                break;
            case PORT_ACT_USE:
                //key = bt_use;
				PortableKeyEvent( state, SDL_SCANCODE_SPACE, 0); // Using space so we don't get stuck in IN_Ack waiting for an SDL event
                                                                 // Not ideal as this will break if the user reconfigures the use button
                break;
            case PORT_ACT_ATTACK:
                key = bt_attack;
                break;
            case PORT_ACT_MAP:
                key = bt_automap;
                break;
            case PORT_ACT_MAP_ZOOM_IN:
                PortableKeyEvent(state, SDL_SCANCODE_EQUALS, 0);
                break;
            case PORT_ACT_MAP_ZOOM_OUT:
                PortableKeyEvent(state, SDL_SCANCODE_MINUS, 0);
                break;
            case PORT_ACT_NEXT_WEP:
                key = bt_nextweapon;
                break;
            case PORT_ACT_PREV_WEP:
                key = bt_prevweapon;
                break;
            case PORT_ACT_ZOOM_IN:
                key = bt_zoom;
                break;
            case PORT_ACT_ALT_FIRE:
                key = bt_altattack;
                break;
            case PORT_ACT_QUICKSAVE:
                PortableKeyEvent(state, SDL_SCANCODE_F8, 0);
                break;
            case PORT_ACT_QUICKLOAD:
                PortableKeyEvent(state, SDL_SCANCODE_F9, 0);
                break;
            case PORT_ACT_ALWAYS_RUN:
                if (state)
                    alwaysrun = !alwaysrun;
                break;
            case PORT_ACT_WEAP1:
                key = bt_slot1;
                break;
            case PORT_ACT_WEAP2:
                key = bt_slot2;
                break;
            case PORT_ACT_WEAP3:
                key = bt_slot3;
                break;
            case PORT_ACT_WEAP4:
                key = bt_slot4;
                break;
            case PORT_ACT_WEAP5:
                key = bt_slot5;
                break;
            case PORT_ACT_WEAP6:
                key = bt_slot6;
                break;
            case PORT_ACT_WEAP7:
                key = bt_slot7;
                break;
        }

        if (key != -1)
            my_buttonstate[key] = state;
    }
}

void PortableMouseButton(int state, int button, float dx, float dy)
{

}

// =================== FORWARD and SIDE MOVMENT ==============

static float forwardmove, sidemove; //Joystick mode

void PortableMoveFwd(float fwd)
{
	if (fwd > 1)
		fwd = 1;
	else if (fwd < -1)
		fwd = -1;

	forwardmove = fwd;
}

void PortableMoveSide(float strafe)
{
	if (strafe > 1)
		strafe = 1;
	else if (strafe < -1)
		strafe = -1;

	sidemove = strafe;
}

void PortableMove(float fwd, float strafe)
{
	PortableMoveFwd(fwd);
	PortableMoveSide(strafe);
}

//======================================================================

//Look up and down
static int look_pitch_mode;
static float look_pitch_mouse,look_pitch_abs,look_pitch_joy;
void PortableLookPitch(int mode, float pitch)
{
	look_pitch_mode = mode;
	switch(mode)
	{
	case LOOK_MODE_MOUSE:
		look_pitch_mouse += pitch;
		break;
	case LOOK_MODE_ABSOLUTE:
		look_pitch_abs = pitch;
		break;
	case LOOK_MODE_JOYSTICK:
		look_pitch_joy = pitch;
		break;
	}
}

//left right
static int look_yaw_mode;
static float look_yaw_mouse,look_yaw_joy;
void PortableLookYaw(int mode, float yaw)
{
	look_yaw_mode = mode;
	switch(mode)
	{
	case LOOK_MODE_MOUSE:
		look_yaw_mouse += yaw;
		break;
	case LOOK_MODE_JOYSTICK:
		look_yaw_joy = yaw;
		break;
	}
}

void PortableInit(int argc,const char ** argv)
{
	main_mobile(argc,(char **)argv);
}

extern bool	ingame;
extern  bool menusAreFaded;
extern int inConversation;

bool inConfirm = false;

touchscreemode_t PortableGetScreenMode()
{
	//return (ingame)?0:1;
	if ((!menusAreFaded) || !ingame || inConfirm || inConversation)
		return TS_MENU;
	else
		return TS_GAME;
}

void PortableBackButton()
{
    PortableKeyEvent(1, SDL_SCANCODE_ESCAPE,0 );
    androidWaitFrames(2);
    PortableKeyEvent(0, SDL_SCANCODE_ESCAPE, 0);
}

void PortableAutomapControl(float zoom, float x, float y)
{

}

void PortableCommand(const char * cmd)
{

}

}

#define BASEMOVE                35
#define RUNMOVE                 70
#define BASETURN                35
#define RUNTURN                 70

//NOTE this is cpp
void pollAndroidControls()
{
	control[ConsolePlayer].controly  -= forwardmove     * (alwaysrun ? RUNMOVE:BASEMOVE);
	control[ConsolePlayer].controlstrafe  += sidemove   * (alwaysrun ? RUNMOVE:BASEMOVE);


	switch(look_yaw_mode)
	{
		case LOOK_MODE_MOUSE:
			control[ConsolePlayer].controlx += -look_yaw_mouse * 8000;
			look_yaw_mouse = 0;
			break;
		case LOOK_MODE_JOYSTICK:
			control[ConsolePlayer].controlx += -look_yaw_joy * 80;
			break;
	}

	for (int n=0;n<NUMBUTTONS;n++)
	{
		if (my_buttonstate[n])
			control[ConsolePlayer].buttonstate[n] = 1;
	}
}
