// WL_MAIN.C

#ifdef _WIN32
//	#include <io.h>
#else
	#include <unistd.h>
#endif

#include "wl_def.h"
#include "wl_menu.h"
#include "id_ca.h"
#include "id_sd.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_us.h"
#include "wl_atmos.h"
#include "m_classes.h"
#include "m_random.h"
#include "config.h"
#include "w_wad.h"
#include "language.h"
#include "textures/textures.h"
#include "c_cvars.h"
#include "thingdef/thingdef.h"
#include "v_font.h"
#include "v_palette.h"
#include "v_video.h"
#include "r_data/colormaps.h"
#include "wl_agent.h"
#include "doomerrors.h"
#include "lumpremap.h"
#include "scanner.h"
#include "g_shared/a_keys.h"
#include "g_mapinfo.h"
#include "wl_draw.h"
#include "wl_inter.h"
#include "wl_iwad.h"
#include "wl_play.h"
#include "wl_game.h"
#include "dobject.h"

/*
=============================================================================

							WOLFENSTEIN 3-D

						An Id Software production

							by John Carmack

=============================================================================
*/

/*
=============================================================================

							LOCAL CONSTANTS

=============================================================================
*/


#define FOCALLENGTH     (0x5700l)               // in global coordinates

#define VIEWWIDTH       256                     // size of view window
#define VIEWHEIGHT      144

/*
=============================================================================

							GLOBAL VARIABLES

=============================================================================
*/

//
// proejection variables
//
fixed    focallength;
unsigned screenofs;
int      viewscreenx, viewscreeny;
int      viewwidth;
int      viewheight;
int      statusbarx;
int      statusbary;
short    centerx;
short    centerxwide;
int      shootdelta;           // pixels away from centerx a target can be
fixed    scale;
fixed    pspritexscale;
fixed    pspriteyscale;
fixed    yaspect;
int32_t  heightnumerator;


void    Quit (const char *error,...);

bool	startgame;
bool	loadedgame;
int		mouseadjustment;

//
// Command line parameter variables
//
bool param_nowait = false;
int     param_difficulty = 1;           // default is "normal"
const char* param_tedlevel = NULL;            // default is not to start a level
int     param_joystickindex = 0;

int     param_joystickhat = -1;
int     param_samplerate = 44100;
int     param_audiobuffer = 2048 / (44100 / param_samplerate);

//===========================================================================

/*
=====================
=
= NewGame
=
= Set up new game to start from the beginning
=
=====================
*/

void NewGame (int difficulty, const FString &map)
{
	memset (&gamestate,0,sizeof(gamestate));
	gamestate.difficulty = difficulty;
	strncpy(gamestate.mapname, map, 8);
	gamestate.mapname[8] = 0;
	levelInfo = &LevelInfo::Find(map);

	players[0].state = player_t::PST_ENTER;

	startgame = true;
}

//===========================================================================

/*
==========================
=
= ShutdownId
=
= Shuts down all ID_?? managers
=
==========================
*/

void ShutdownId (void)
{
	US_Shutdown ();         // This line is completely useless...
	SD_Shutdown ();
	IN_Shutdown ();
}


//===========================================================================

/*
==================
=
= BuildTables
=
= Calculates:
=
= scale                 projection constant
= sintable/costable     overlapping fractional tables
=
==================
*/

const float radtoint = (float)(FINEANGLES/2/PI);

void BuildTables (void)
{
	//
	// calculate fine tangents
	//

	int i;
	for(i=0;i<FINEANGLES/8;i++)
	{
		double tang=tan((i+0.5)/radtoint);
		finetangent[i]=(int32_t)(tang*GLOBAL1);
		finetangent[FINEANGLES/4-1-i]=(int32_t)((1/tang)*GLOBAL1);
	}

	//
	// costable overlays sintable with a quarter phase shift
	// ANGLES is assumed to be divisable by four
	//

	float angle = 0;
	float anglestep = (float)(PI/2/ANG90);
	for(i=0; i<FINEANGLES; i++)
	{
		finesine[i]=fixed(GLOBAL1*sin(angle));
		angle+=anglestep;
	}
	memcpy(&finesine[FINEANGLES], finesine, FINEANGLES*sizeof(fixed)/4);

#if defined(USE_STARSKY) || defined(USE_RAIN) || defined(USE_SNOW)
	Init3DPoints();
#endif
}

//===========================================================================


/*
====================
=
= CalcProjection
=
= Uses focallength
=
====================
*/

void CalcProjection (int32_t focal)
{
	int     i;
	int    intang;
	float   angle;
	double  tang;
	int     halfview;
	double  facedist;

	// 0xFD17 is a magic number to convert the player's radius 0x5800 to FOCALLENGTH (0x5700)
	focallength = FixedMul(focal, 0xFD17);
	facedist = 2*FOCALLENGTH+0x100; // Used to be MINDIST (0x5800) which was 0x100 then the FOCALLENGTH (0x5700)
	halfview = viewwidth/2;                                 // half view in pixels

	//
	// calculate scale value for vertical height calculations
	// and sprite x calculations
	//
	scale = (fixed) (halfview*facedist/(AspectCorrection[r_ratio].viewGlobal/2));

	//
	// divide heightnumerator by a posts distance to get the posts height for
	// the heightbuffer.  The pixel height is height>>2
	//
	heightnumerator = ((TILEGLOBAL*scale)>>6)*(yaspect/65536.);

	//
	// calculate the angle offset from view angle of each pixel's ray
	//

	for (i=0;i<halfview;i++)
	{
		// start 1/2 pixel over, so viewangle bisects two middle pixels
		tang = (int32_t)i*AspectCorrection[r_ratio].viewGlobal/viewwidth/facedist;
		angle = (float) atan(tang);
		intang = (int) (angle*radtoint);
		pixelangle[halfview-1-i] = intang;
		pixelangle[halfview+i] = -intang;
	}
}

//===========================================================================

/*
==========================
=
= SignonScreen
=
==========================
*/

void SignonScreen (void)                        // VGA version
{
	VL_SetVGAPlaneMode ();
	CA_CacheScreen(TexMan(gameinfo.SignonLump));
}

//===========================================================================

Menu musicMenu(CTL_X, CTL_Y-6, 280, 32);
static TArray<FString> songList;

MENU_LISTENER(ChangeMusic)
{
	StartCPMusic(songList[which]);
	for(unsigned int i = 0;i < songList.Size();++i)
		musicMenu[i]->setHighlighted(i == (unsigned)which);
	musicMenu.draw();
	return true;
}

void DoJukebox(void)
{
	IN_ClearKeysDown();
	if (!AdLibPresent && !SoundBlasterPresent)
		return;

	VW_FadeOut ();

	ClearMScreen ();
	musicMenu.setHeadText(language["ROBSJUKEBOX"], true);
	for(unsigned int i = 0;i < (unsigned)Wads.GetNumLumps();++i)
	{
		if(Wads.GetLumpNamespace(i) != ns_music)
			continue;

		FString langString;
		langString.Format("MUS_%s", Wads.GetLumpFullName(i));
		const char* trackName = language[langString];
		if(trackName == langString.GetChars())
			musicMenu.addItem(new MenuItem(Wads.GetLumpFullName(i), ChangeMusic));
		else
			musicMenu.addItem(new MenuItem(language[langString], ChangeMusic));
		songList.Push(Wads.GetLumpFullName(i));
		
	}
	musicMenu.show();
	return;
}

/*
==========================
=
= InitGame
=
= Load a few things right away
=
==========================
*/

#ifdef _WIN32
void SetupWM();
#endif

static void CollectGC()
{
	GC::FullGC();
	GC::DelSoftRootHead();
}

static void InitGame()
{
	// initialize SDL
#if defined _WIN32
	putenv("SDL_VIDEODRIVER=directx");
#endif
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
	{
		printf("Unable to init SDL: %s\n", SDL_GetError());
		exit(1);
	}
	atterm(SDL_Quit);

	int numJoysticks = SDL_NumJoysticks();
	if(param_joystickindex && (param_joystickindex < -1 || param_joystickindex >= numJoysticks))
	{
		if(!numJoysticks)
			printf("No joysticks are available to SDL!\n");
		else
			printf("The joystick index must be between -1 and %i!\n", numJoysticks - 1);
		exit(1);
	}

	//
	// Mapinfo
	//

	V_InitFontColors();
	G_ParseMapInfo(true);

	//
	// Init texture manager
	//

	TexMan.Init();
	printf("VL_ReadPalette: Setting up the Palette...\n");
	VL_ReadPalette();
	InitPalette(gameinfo.GamePalette);
	R_InitColormaps();
	atterm(R_DeinitColormaps);
	GenerateLookupTables();

	SignonScreen ();

#if defined _WIN32
	if(!fullscreen)
		SetupWM();
#endif
	VW_UpdateScreen();

	// Parse non-gameinfo sections in MAPINFO
	G_ParseMapInfo(false);

//
// Fonts
//
	V_InitFonts();
	atterm(V_ClearFonts);

	VH_Startup ();
	IN_Startup ();
	SD_Startup ();
	printf("US_Startup: Starting the User Manager.\n");
	US_Startup ();

//
// Load Actors
//

	ClassDef::LoadActors();
	atterm(CollectGC);

//
// Load Keys
//

	P_InitKeyMessages();
	atterm(P_DeinitKeyMessages);

//
// draw intro screen stuff
//
	IntroScreen ();

//
// Finish with setting up through the config file.
//
	FinalReadConfig();

//
// load in and lock down some basic chunks
//

	BuildTables ();          // trig tables

//
// initialize variables
//
	InitRedShifts ();

//
// initialize the menus
	printf("CreateMenus: Preparing the menu system...\n");
	CreateMenus();

//
// Finish signon screen
//
	VH_UpdateScreen();

	if (!param_nowait)
		IN_UserInput(70*4);

//
// HOLDING DOWN 'M' KEY?
//
	IN_ProcessEvents();

	if (Keyboard[sc_M])
		DoJukebox();

#ifdef NOTYET
	vdisp = (byte *) (0xa0000+PAGE1START);
	vbuf = (byte *) (0xa0000+PAGE2START);
#endif
}

//===========================================================================

/*
==========================
=
= SetViewSize
=
==========================
*/

static void SetViewSize (unsigned int screenWidth, unsigned int screenHeight)
{
	statusbarx = 0;
	if(AspectCorrection[r_ratio].isWide)
		statusbarx = screenWidth*(48-AspectCorrection[r_ratio].multiplier)/(48*2);

	statusbary = 200 - STATUSLINES - scaleFactor;
	if(AspectCorrection[r_ratio].tallscreen)
		statusbary = ((statusbary - 100)*screenHeight*3)/AspectCorrection[r_ratio].baseHeight + screenHeight/2
			+ (screenHeight - screenHeight*AspectCorrection[r_ratio].multiplier/48)/2;
	else
		statusbary = statusbary*screenHeight/200;

	unsigned int width;
	unsigned int height;
	if(viewsize == 21)
	{
		width = screenWidth;
		height = screenHeight;
	}
	else if(viewsize == 20)
	{
		width = screenWidth;
		height = statusbary;
	}
	else
	{
		width = screenWidth - (20-viewsize)*16*screenWidth/320;
		height = (statusbary+1) - (20-viewsize)*8*screenHeight/200;
	}

	//viewwidth = width&~15;                  // must be divisable by 16
	//viewheight = height&~1;                 // must be even
	viewwidth = width;
	viewheight = height;
	centerx = viewwidth/2-1;
	centerxwide = AspectCorrection[r_ratio].isWide ? CorrectWidthFactor(centerx) : centerx;
	shootdelta = viewwidth/10;
	if((unsigned) viewheight == screenHeight)
		viewscreenx = viewscreeny = screenofs = 0;
	else
	{
		viewscreenx = (screenWidth-viewwidth) / 2;
		viewscreeny = (statusbary-viewheight)/2;
		screenofs = viewscreeny*screenWidth+viewscreenx;
	}

	int virtheight = screenHeight;
	int virtwidth = screenWidth;
	if(AspectCorrection[r_ratio].isWide)
		virtwidth = CorrectWidthFactor(virtwidth);
	else
		virtheight = CorrectWidthFactor(virtheight);
	yaspect = FixedMul((320<<FRACBITS)/200,(virtheight<<FRACBITS)/virtwidth);

	pspritexscale = (centerxwide<<FRACBITS)/160;
	pspriteyscale = FixedMul(pspritexscale, yaspect);

	//
	// calculate trace angles and projection constants
	//
	if(players[0].mo)
		CalcProjection(players[0].mo->radius);
	else
		CalcProjection (FOCALLENGTH);
}

void NewViewSize (int width, unsigned int scrWidth, unsigned int scrHeight)
{
	if(width < 4 || width > 21)
		return;

	viewsize = width;
	SetViewSize(scrWidth, scrHeight);
}



//===========================================================================

/*
==========================
=
= Quit
=
==========================
*/

void Quit (const char *errorStr, ...)
{
#ifdef NOTYET
	byte *screen;
#endif
	char error[256];
	if(errorStr != NULL)
	{
		va_list vlist;
		va_start(vlist, errorStr);
		vsprintf(error, errorStr, vlist);
		va_end(vlist);
	}
	else error[0] = 0;

	if (error[0] == 0)
	{
#ifdef NOTYET
		#ifndef JAPAN
		CA_CacheGrChunk (ORDERSCREEN);
		screen = grsegs[ORDERSCREEN];
		#endif
#endif

		WriteConfig ();
	}
#ifdef NOTYET
	else
	{
		CA_CacheGrChunk (ERRORSCREEN);
		screen = grsegs[ERRORSCREEN];
	}
#endif

	ShutdownId ();

	if (error[0] != 0)
	{
#ifdef NOTYET
		memcpy((byte *)0xb8000,screen+7,7*160);
		SetTextCursor(9,3);
#endif
		puts(error);
#ifdef NOTYET
		SetTextCursor(0,7);
#endif
		VW_WaitVBL(200);
		exit(1);
	}
	else
	if (error[0] == 0)
	{
#ifdef NOTYET
		#ifndef JAPAN
		memcpy((byte *)0xb8000,screen+7,24*160); // 24 for SPEAR/UPLOAD compatibility
		#endif
		SetTextCursor(0,23);
#endif
	}

	exit(0);
}

//==========================================================================

/*
==================
=
= PG13
=
==================
*/

static void PG13 (void)
{
	VW_FadeOut ();
	VWB_Clear(0x82, 0, 0, screenWidth, screenHeight);
	VWB_DrawGraphic(TexMan("PG13"), 216, 110);
	VW_UpdateScreen ();

	VW_FadeIn ();
	IN_UserInput (TickBase * 7);

	VW_FadeOut ();
}

//===========================================================================

////////////////////////////////////////////////////////
//
// NON-SHAREWARE NOTICE
//
////////////////////////////////////////////////////////
static void NonShareware (void)
{
	if(strlen(language["REGNOTICE_TITLE"]) == 0)
		return;

	VW_FadeOut ();

	ClearMScreen ();
	DrawStripes (10);

	PrintX = 110;
	PrintY = 15;

	pa = MENU_TOP;
	US_Print (BigFont, language["REGNOTICE_TITLE"], gameinfo.FontColors[GameInfo::MENU_HIGHLIGHTSELECTION]);
	pa = MENU_CENTER;

	WindowX = PrintX = 40;
	PrintY = 60;
	US_Print (BigFont, language["REGNOTICE_MESSAGE"], gameinfo.FontColors[GameInfo::MENU_SELECTION]);

	VW_UpdateScreen ();
	VW_FadeIn ();
	IN_Ack ();
}

//===========================================================================


/*
=====================
=
= DemoLoop
=
=====================
*/


static void DemoLoop()
{
	int LastDemo = 0;

//
// check for launch from ted
//
	if (param_tedlevel)
	{
		param_nowait = true;
		NewGame(param_difficulty,param_tedlevel);
	}


//
// main game cycle
//

	if (!param_nowait)
		NonShareware();

	StartCPMusic(gameinfo.TitleMusic);

	if (!param_nowait)
		PG13 ();

	bool reloadPalette = false;
	while (1)
	{
		while (!param_nowait)
		{
//
// title page
//
			bool useTitlePalette = !gameinfo.TitlePalette.IsEmpty();
			SDL_Color pal[256];
			if(useTitlePalette)
			{
				reloadPalette = true;
				InitPalette(gameinfo.TitlePalette);
			}

			CA_CacheScreen(TexMan(gameinfo.TitlePage));
			VW_UpdateScreen ();
			VW_FadeIn();
			if (IN_UserInput(TickBase*gameinfo.TitleTime))
				break;
			VW_FadeOut();
			if(useTitlePalette)
			{
				InitPalette(gameinfo.GamePalette);
				reloadPalette = false;
			}
//
// credits page
//
			CA_CacheScreen (TexMan(gameinfo.CreditPage));
			VW_UpdateScreen();
			VW_FadeIn ();
			if (IN_UserInput(TickBase*gameinfo.PageTime))
				break;
			VW_FadeOut ();
//
// high scores
//
			DrawHighScores ();
			VW_UpdateScreen ();
			VW_FadeIn ();

			if (IN_UserInput(TickBase*gameinfo.PageTime))
				break;
//
// demo
//

			bool demoPlayed = false;
			do // This basically loops twice at most.  If the lump exists it plays the demo if not it goes to DEMO0.
			{  // PlayDemo will actually play the demo picked if it exists otherwise it will immediately return.
				char demoName[9];
				sprintf(demoName, "DEMO%d", LastDemo);
				if(Wads.CheckNumForName(demoName) == -1)
				{
					if(LastDemo == 0)
						break;
					else
						LastDemo = 0;
					continue;
				}
				else
				{
					demoPlayed = true;
					PlayDemo(LastDemo++);
					break;
				}
			}
			while(true);

			if (playstate == ex_abort)
				break;
			VW_FadeOut();
			if(screenHeight % 200 != 0)
				VL_ClearScreen(0);
			if(demoPlayed)
				StartCPMusic(gameinfo.TitleMusic);
		}

		if(!param_tedlevel)
		{
			VW_FadeOut ();
			if(reloadPalette)
			{
				InitPalette(gameinfo.GamePalette);
				reloadPalette = false;
			}

			if (Keyboard[sc_Tab])
				RecordDemo ();
			else
				US_ControlPanel (0);
		}

		if (param_tedlevel || startgame || loadedgame)
		{
			param_tedlevel = false;
			GameLoop ();
			if(!param_nowait)
			{
				VW_FadeOut();
				StartCPMusic(gameinfo.TitleMusic);
			}
		}
	}
}


//===========================================================================

// CheckRatio -- From ZDoom
//
// Tries to guess the physical dimensions of the screen based on the
// screen's pixel dimensions.
int CheckRatio (int width, int height)//, int *trueratio)
{
	int fakeratio = -1;
	Aspect ratio;

	if (vid_aspect != ASPECT_NONE)
	{
		// [SP] User wants to force aspect ratio; let them.
		fakeratio = vid_aspect;
	}
	/*if (vid_nowidescreen)
	{
		if (!vid_tft)
		{
			fakeratio = 0;
		}
		else
		{
			fakeratio = (height * 5/4 == width) ? 4 : 0;
		}
	}*/
	// If the size is approximately 16:9, consider it so.
	if (abs (height * 16/9 - width) < 10)
	{
		ratio = ASPECT_16_9;
	}
	// Consider 17:10 as well.
	else if (abs (height * 17/10 - width) < 10)
	{
		ratio = ASPECT_17_10;
	}
	// 16:10 has more variance in the pixel dimensions. Grr.
	else if (abs (height * 16/10 - width) < 60)
	{
		// 320x200 and 640x400 are always 4:3, not 16:10
		if ((width == 320 && height == 200) || (width == 640 && height == 400))
		{
			ratio = ASPECT_NONE;
		}
		else
		{
			ratio = ASPECT_16_10;
		}
	}
	// Unless vid_tft is set, 1280x1024 is 4:3, not 5:4.
	else if (height * 5/4 == width)// && vid_tft)
	{
		ratio = ASPECT_5_4;
	}
	// Assume anything else is 4:3.
	else
	{
		ratio = ASPECT_4_3;
	}

	/*if (trueratio != NULL)
	{
		*trueratio = ratio;
	}*/
	return (fakeratio >= 0) ? fakeratio : ratio;
}

#define IFARG(str) if(!strcmp(arg, (str)))

static const char* CheckParameters(int argc, char *argv[], TArray<FString> &files)
{
	const char* extension = NULL;
	bool hasError = false, showHelp = false;
	bool sampleRateGiven = false, audioBufferGiven = false;
	int defaultSampleRate = param_samplerate;

	fullscreen = vid_fullscreen;

	for(int i = 1; i < argc; i++)
	{
		char *arg = argv[i];
		IFARG("--baby")
			param_difficulty = 0;
		else IFARG("--easy")
			param_difficulty = 1;
		else IFARG("--normal")
			param_difficulty = 2;
		else IFARG("--hard")
			param_difficulty = 3;
		else IFARG("--nowait")
			param_nowait = true;
		else IFARG("--tedlevel")
		{
			if(++i >= argc)
			{
				printf("The tedlevel option is missing the level argument!\n");
				hasError = true;
			}
			else param_tedlevel = argv[i];
		}
		else IFARG("--fullscreen")
			fullscreen = true;
		else IFARG("--res")
		{
			if(i + 2 >= argc)
			{
				printf("The res option needs the width and/or the height argument!\n");
				hasError = true;
			}
			else
			{
				screenWidth = atoi(argv[++i]);
				screenHeight = atoi(argv[++i]);
				if(screenWidth < 320)
					printf("Screen width must be at least 320!\n"), hasError = true;
				if(screenHeight < 200)
					printf("Screen height must be at least 200!\n"), hasError = true;
			}
		}
		else IFARG("--aspect")
		{
			const char* ratio = argv[++i];
			if(strcmp(ratio, "4:3") == 0)
				vid_aspect = ASPECT_4_3;
			else if(strcmp(ratio, "16:10") == 0)
				vid_aspect = ASPECT_16_10;
			else if(strcmp(ratio, "17:10") == 0)
				vid_aspect = ASPECT_17_10;
			else if(strcmp(ratio, "16:9") == 0)
				vid_aspect = ASPECT_16_9;
			else if(strcmp(ratio, "5:4") == 0)
				vid_aspect = ASPECT_5_4;
			else
			{
				printf("Unknown aspect ratio %s!\n", ratio);
				hasError = true;
			}
		}
		else IFARG("--bits")
		{
			if(++i >= argc)
			{
				printf("The bits option is missing the color depth argument!\n");
				hasError = true;
			}
			else
			{
				screenBits = atoi(argv[i]);
				switch(screenBits)
				{
					case 8:
					case 16:
					case 24:
					case 32:
						break;

					default:
						printf("Screen color depth must be 8, 16, 24, or 32!\n");
						hasError = true;
						break;
				}
			}
		}
		else IFARG("--noadaptive")
			noadaptive = true;
		else IFARG("--nodblbuf")
			usedoublebuffering = false;
		else IFARG("--extravbls")
		{
			if(++i >= argc)
			{
				printf("The extravbls option is missing the vbls argument!\n");
				hasError = true;
			}
			else
			{
				extravbls = atoi(argv[i]);
				if((signed)extravbls < 0)
				{
					printf("Extravbls must be positive!\n");
					hasError = true;
				}
			}
		}
		else IFARG("--joystick")
		{
			if(++i >= argc)
			{
				printf("The joystick option is missing the index argument!\n");
				hasError = true;
			}
			else param_joystickindex = atoi(argv[i]);   // index is checked in InitGame
		}
		else IFARG("--joystickhat")
		{
			if(++i >= argc)
			{
				printf("The joystickhat option is missing the index argument!\n");
				hasError = true;
			}
			else param_joystickhat = atoi(argv[i]);
		}
		else IFARG("--samplerate")
		{
			if(++i >= argc)
			{
				printf("The samplerate option is missing the rate argument!\n");
				hasError = true;
			}
			else param_samplerate = atoi(argv[i]);
			sampleRateGiven = true;
		}
		else IFARG("--audiobuffer")
		{
			if(++i >= argc)
			{
				printf("The audiobuffer option is missing the size argument!\n");
				hasError = true;
			}
			else param_audiobuffer = atoi(argv[i]);
			audioBufferGiven = true;
		}
		else IFARG("--help")
			showHelp = true;
		else IFARG("--data")
			if(++i >= argc)
			{
				printf("Expected main data extension!\n");
				hasError = true;
			}
			else
				extension = argv[i];
		else IFARG("--file")
		{
			if(++i < argc)
				files.Push(argv[++i]);
		}
		else
			files.Push(argv[i]);
	}
	if(hasError || showHelp)
	{
		if(hasError) printf("\n");
		printf(
			"ECWolf v1.0\n"
			"http://maniacsvault.net/ecwolf/\n"
			"Based on Wolf4SDL v1.7\n"
			"Ported by Chaos-Software (http://www.chaos-software.de.vu)\n"
			"Original Wolfenstein 3D by id Software\n\n"
			"Usage: ecwolf [options]\n"
			"Options:\n"
			" --help                 This help page\n"
			" --file <file>          Loads an extra data file\n"
			" --tedlevel <level>     Starts the game in the given level\n"
			" --baby                 Sets the difficulty to baby for tedlevel\n"
			" --easy                 Sets the difficulty to easy for tedlevel\n"
			" --normal               Sets the difficulty to normal for tedlevel\n"
			" --hard                 Sets the difficulty to hard for tedlevel\n"
			" --nowait               Skips intro screens\n"
			" --fullscreen           Starts the game in fullscreen mode\n"
			" --res <width> <height> Sets the screen resolution\n"
			" --aspect <aspect>      Sets the aspect ratio.\n"
			" --noadaptive           Disables adaptive tics.\n"
			" --bits <b>             Sets the screen color depth\n"
			"                        (use this when you have palette/fading problems\n"
			"                        allowed: 8, 16, 24, 32, default: \"best\" depth)\n"
			" --nodblbuf             Don't use SDL's double buffering\n"
			" --extravbls <vbls>     Sets a delay after each frame, which may help to\n"
			"                        reduce flickering (unit is currently 8 ms, default: 0)\n"
			" --joystick <index>     Use the index-th joystick if available\n"
			"                        (-1 to disable joystick, default: 0)\n"
			" --joystickhat <index>  Enables movement with the given coolie hat\n"
			" --samplerate <rate>    Sets the sound sample rate (given in Hz, default: %i)\n"
			" --audiobuffer <size>   Sets the size of the audio buffer (-> sound latency)\n"
			"                        (given in bytes, default: 2048 / (44100 / samplerate))\n"
			, defaultSampleRate
		);
		exit(1);
	}

	r_ratio = static_cast<Aspect>(CheckRatio(screenWidth, screenHeight));

	if(sampleRateGiven && !audioBufferGiven)
		param_audiobuffer = 2048 / (44100 / param_samplerate);

	return extension;
}

#ifndef _WIN32
// I_MakeRNGSeed is from ZDoom
#include <time.h>

// Return a random seed, preferably one with lots of entropy.
unsigned int I_MakeRNGSeed()
{
	unsigned int seed;
	int file;

	// Try reading from /dev/urandom first, then /dev/random, then
	// if all else fails, use a crappy seed from time().
	seed = time(NULL);
	file = open("/dev/urandom", O_RDONLY);
	if (file < 0)
	{
		file = open("/dev/random", O_RDONLY);
	}
	if (file >= 0)
	{
		read(file, &seed, sizeof(seed));
		close(file);
	}
	return seed;
}
#else
unsigned int I_MakeRNGSeed();
#endif

/*
==========================
=
= main
=
==========================
*/

void InitThinkerList();
void ScannerMessageHandler(Scanner::MessageLevel level, const char *error, va_list list)
{
	FString errorMessage;
	errorMessage.VFormat(error, list);

	if(level == Scanner::ERROR)
		throw CRecoverableError(errorMessage);
	else
		printf("%s", errorMessage.GetChars());
}

// Basically from ZDoom
// We are definting an atterm function so that we can control the exit behavior.
static const unsigned int MAX_TERMS = 32;
static void (*TermFuncs[MAX_TERMS])(void);
static unsigned int NumTerms;
void atterm(void (*func)(void))
{
	for(unsigned int i = 0;i < NumTerms;++i)
	{
		if(TermFuncs[i] == func)
			return;
	}

	if(NumTerms < MAX_TERMS)
		TermFuncs[NumTerms++] = func;
	else
		fprintf(stderr, "Failed to register atterm function!\n");
}
void CallTerminateFunctions()
{
	while(NumTerms > 0)
		TermFuncs[--NumTerms]();
}

#ifndef NO_GTK
#include <gtk/gtk.h>
bool GtkAvailable;
#endif
int main (int argc, char *argv[])
{
#ifndef NO_GTK
	GtkAvailable = gtk_init_check(&argc, &argv);
#endif

	Scanner::SetMessageHandler(ScannerMessageHandler);
	atexit(CallTerminateFunctions);

	try
	{
		printf("ReadConfig: Reading the Configuration.\n");
		config->LocateConfigFile(argc, argv);
		ReadConfig();

		{
			TArray<FString> wadfiles, files;

			const char* extension = CheckParameters(argc, argv, wadfiles);
			IWad::SelectGame(files, extension, "ecwolf.pk3");

			for(unsigned int i = 0;i < wadfiles.Size();++i)
				files.Push(wadfiles[i]);

			printf("W_Init: Init WADfiles.\n");
			Wads.InitMultipleFiles(files);
			language.SetupStrings();
			LumpRemapper::RemapAll();
		}

		InitThinkerList();

		printf("InitGame: Setting up the game...\n");
		InitGame();

		rngseed = I_MakeRNGSeed();
		FRandom::StaticClearRandom();

		printf("DemoLoop: Starting the game loop...\n");
		DemoLoop();

		Quit("Demo loop exited???");
	}
	catch(class CDoomError &error)
	{
		if(error.GetMessage())
			fprintf(stderr, "%s\n", error.GetMessage());
		exit(-1);
	}
	return 1;
}
