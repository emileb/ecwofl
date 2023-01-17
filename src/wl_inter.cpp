// WL_INTER.C

#include "wl_def.h"
#include "wl_menu.h"
#include "wl_play.h"
#include "id_ca.h"
#include "id_sd.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_us.h"
#include "language.h"
#include "v_video.h"
#include "wl_agent.h"
#include "wl_game.h"
#include "wl_inter.h"
#include "wl_net.h"
#include "wl_text.h"
#include "g_mapinfo.h"
#include "colormatcher.h"

LRstruct LevelRatios;

static int32_t lastBreathTime = 0;

//==========================================================================

/*
==================
=
= CLearSplitVWB
=
==================
*/

void
ClearSplitVWB (void)
{
	WindowX = 0;
	WindowY = 0;
	WindowW = 320;
	WindowH = 160;
}

//==========================================================================

static void Erase (int x, int y, const char *string, bool rightAlign=false)
{
	double nx = x*8;
	double ny = y*8;

	word width, height;
	VW_MeasurePropString(IntermissionFont, string, width, height);

	if(rightAlign)
		nx -= width;

	double fw = width;
	double fh = height;
	screen->VirtualToRealCoords(nx, ny, fw, fh, 320, 200, true, true);
	VWB_DrawFill(TexMan(levelInfo->GetBorderTexture()), nx, ny, nx+fw, ny+fh);
}

static void Write (int x, int y, const char *string, bool rightAlign=false, bool bonusfont=false)
{
	FFont *font = bonusfont ? V_GetFont("BonusFont") : IntermissionFont;
	FRemapTable *remap = font->GetColorTranslation(CR_UNTRANSLATED);

	int nx = x*8;
	int ny = y*8;

	if(rightAlign)
	{
		word width, height;
		VW_MeasurePropString(font, string, width, height);
		nx -= width;
	}

	int width;
	while(*string != '\0')
	{
		if(*string != '\n')
		{
			FTexture *glyph = font->GetChar(*string, &width);
			if(glyph)
				VWB_DrawGraphic(glyph, nx, ny, MENU_NONE, remap);
			nx += width;
		}
		else
		{
			nx = x*8;
			ny += font->GetHeight();
		}
		++string;
	}
}


static const unsigned int PAR_AMOUNT = 500;
static struct IntermissionState
{
	unsigned int kr, sr, tr;
	int timeleft;
	uint32_t bonus;
	bool acked;
	bool graphical;
} InterState;
enum
{
	WI_LEVEL,
	WI_FLOOR,
	WI_FINISH,
	WI_BONUS,
	WI_TIME,
	WI_PAR,
	WI_KILLS,
	WI_TREASR,
	WI_SECRTS,
	WI_PERFCT,
	WI_RATING,

	NUM_WI
};
static const char* const GraphicalTexNames[NUM_WI] = {
	"WILEVEL", "WIFLOOR", "WIFINISH", "WIBONUS", "WITIME", "WIPAR",
	"WIKILLS", "WITREASR", "WISECRTS", "WIPERFCT", "WIRATING"
};
static FTextureID GraphicalTexID[NUM_WI];

//
// Breathe Mr. BJ!!!
//
void BJ_Breathe (bool drawOnly=false)
{
	static int which = 0, max = 10;
	static FTexture* const pics[2] = { TexMan("L_GUY1"), TexMan("L_GUY2") };
	unsigned int height = InterState.graphical ? 8 : 16;

	if(drawOnly)
	{
		VWB_DrawGraphic(pics[which], 0, height);
		return;
	}

	SDL_Delay(5);

	if ((int32_t) GetTimeCount () - lastBreathTime > max)
	{
		which ^= 1;
		VWB_DrawGraphic(pics[which], 0, height);
		VW_UpdateScreen ();
		lastBreathTime = GetTimeCount();
		max = 35;
	}
}

static void InterWriteCounter(int start, int end, int step, unsigned int x, unsigned int y, const char* sound, unsigned int sndfreq, bool bonusfont=false)
{
	const unsigned int tx = x>>3, ty = y>>3;

	if(InterState.acked)
	{
		FString tempstr;
		tempstr.Format("%d", end);
		Write(tx, ty, tempstr, true, bonusfont);
		return;
	}

	bool cont = true;
	unsigned int i = 0;
	FString tempstr("0");
	do
	{
		if(start > end)
		{
			start = end;
			cont = false;
		}

		if(start) Erase (tx, ty, tempstr, true);
		tempstr.Format("%d", start);
		Write (tx, ty, tempstr, true, bonusfont);
		if (sndfreq == 0)
		{
			if(cont)
				SD_PlaySound(sound);
		}
		else if(!((i++) % sndfreq))
			SD_PlaySound (sound);
		if(!(start & 1)) VW_UpdateScreen ();
		do
		{
			BJ_Breathe ();
		}
		while (sndfreq && SD_SoundPlaying ());

		if (IN_CheckAck ())
		{
			InterState.acked = true;
			if(start != end)
			{
				start = end;
				continue;
			}
		}

		start += step;
	}
	while(cont);
}

static void InterWriteTime(unsigned int time, unsigned int x, unsigned int y, bool hours=false)
{
	unsigned int m, s;
	FString timestamp;
	if(hours)
	{
		unsigned int h = clamp<unsigned int>(time/3600, 0, 9);
		if(h < 9)
			m = (time / 60) % 60;
		else
			m = clamp<unsigned int>((time - 3600*9)/60, 0, 99);

		if(m < 99)
			s = time % 60;
		else
			s = clamp<unsigned int>(time - (3600*9 + 60*99), 0, 99);

		timestamp.Format("%u:%02u:%02u", h, m, s);
	}
	else
	{
		m = clamp<unsigned int>(time/60, 0, 99);

		if(m < 99)
			s = time % 60;
		else
			s = clamp<unsigned int>(time - (3600*9 + 60*99), 0, 99);

		timestamp.Format("%02u:%02u", m, s);
	}

	Write(x>>3, y>>3, timestamp, false);
}

static void InterAddBonus(unsigned int bonus, bool count=false)
{
	const unsigned int y = InterState.graphical ? 72 : 56;
	if(count)
	{
		InterState.bonus += bonus;
		InterWriteCounter(0, bonus, PAR_AMOUNT, 288, y, "misc/end_bonus1", PAR_AMOUNT/10, InterState.graphical);
		return;
	}

	FString bonusstr;
	bonusstr.Format("%u", InterState.bonus);
	Erase (36, y>>3, bonusstr, true);
	InterState.bonus += bonus;
	bonusstr.Format("%u", InterState.bonus);
	Write (36, y>>3, bonusstr, true, InterState.graphical);
	VW_UpdateScreen ();
}

// Divy up bonus points to all players
static void InterGiveBonus(unsigned int bonus)
{
	unsigned int commonBonus = bonus/Net::InitVars.numPlayers;
	unsigned int extraBonus = bonus%Net::InitVars.numPlayers;

	// We'll give the remainder points to the lowest scoring player because why not?
	player_t *extraRecipient = players;
	for(unsigned int i = 1;i < Net::InitVars.numPlayers;++i)
	{
		if(players[i].score < extraRecipient->score)
			extraRecipient = &players[i];
	}

	for(unsigned int i = 0;i < Net::InitVars.numPlayers;++i)
		players[i].GivePoints(commonBonus + (&players[i] == extraRecipient ? extraBonus : 0));
}

/**
 * Displays a percentage ratio, counting up to the ratio.
 * Returns true if the intermission has been acked and should be skipped.
 */
static void InterCountRatio(int ratio, unsigned int x, unsigned int y)
{
	static const unsigned int VBLWAIT = 30;
	static const unsigned int PERCENT100AMT = 10000;

	if (InterState.graphical)
		InterWriteCounter(1, ratio, 1, x, y, "misc/end_bonus1", 0);
	else
		InterWriteCounter(0, ratio, 1, x, y, "misc/end_bonus1", 10);
	if (ratio >= 100)
	{
		if(!InterState.acked)
			VW_WaitVBL (VBLWAIT);
		SD_StopSound ();
		InterAddBonus(PERCENT100AMT);
		if(InterState.acked)
			return;
		SD_PlaySound ("misc/100percent");
	}
	else if (!ratio)
	{
		if(InterState.acked)
			return;
		VW_WaitVBL (VBLWAIT);
		SD_StopSound ();
		SD_PlaySound ("misc/no_bonus");
	}
	else
	{
		if(InterState.acked)
			return;
		SD_PlaySound ("misc/end_bonus2");
	}

	VW_UpdateScreen ();
	while (SD_SoundPlaying ())
		BJ_Breathe ();
}

static void InterWaitForAck()
{
	InterState.acked = false;
	IN_StartAck (ACK_Any);
	while (!IN_CheckAck ())
		BJ_Breathe ();
	IN_ClearKeysDown();
}

static void InterDrawNormalTop()
{
	FString completedString;
	if(!levelInfo->CompletionString.IsEmpty())
	{
		if(levelInfo->CompletionString[0] == '$')
			completedString = language[levelInfo->CompletionString.Mid(1)];
		else
			completedString = levelInfo->CompletionString;

		FString formattedString;
		formattedString.Format(completedString, levelInfo->FloorNumber.GetChars());
		Write (14, 2, formattedString);
	}
	else
	{
		if(levelInfo->TitlePatch.isValid())
		{
			VWB_DrawGraphic(TexMan(levelInfo->TitlePatch), 112, 16);
		}
		else
		{
			completedString.Format("%s %s", language["STR_FLOOR"], levelInfo->FloorNumber.GetChars());
			Write (14, 2, completedString);
		}
		Write(14, 4, language["STR_COMPLETED"]);
	}
}

static void InterDrawGraphicalTop()
{
	// Handle X-Y floor numbers. If not in that format emulate the normal
	// mode by just using floor X.
	int dash = levelInfo->FloorNumber.IndexOf('-');
	if(dash != -1)
	{
		if(levelInfo->TitlePatch.isValid())
		{
			VWB_DrawGraphic(TexMan(levelInfo->TitlePatch), 104, 8);
			VWB_DrawGraphic(TexMan(GraphicalTexID[WI_LEVEL]), 104, 24);
			Write(23, 3, levelInfo->FloorNumber.Left(dash), false);
		}
		else
		{
			VWB_DrawGraphic(TexMan(GraphicalTexID[WI_LEVEL]), 104, 8);
			Write(23, 1, levelInfo->FloorNumber.Left(dash), false);
			VWB_DrawGraphic(TexMan(GraphicalTexID[WI_FLOOR]), 104, 24);
			Write(23, 3, levelInfo->FloorNumber.Mid(dash+1), false);
		}
		VWB_DrawGraphic(TexMan(GraphicalTexID[WI_FINISH]), 104, 40);
	}
	else
	{
		VWB_DrawGraphic(TexMan(GraphicalTexID[WI_FLOOR]), 104, 8);
		VWB_DrawGraphic(TexMan(GraphicalTexID[WI_FINISH]), 104, 24);
		Write(23, 1, levelInfo->FloorNumber, false);
	}
}

static void InterDoBonus()
{
	if(InterState.graphical)
		InterDrawGraphicalTop();
	else
		InterDrawNormalTop();

	FString bonusString;
	bonusString.Format("%d bonus!", levelInfo->LevelBonus);
	Write (34, 16, bonusString, true);

	VW_UpdateScreen ();
	VW_FadeIn ();

	InterGiveBonus (levelInfo->LevelBonus);
}

static void InterDoNormal()
{
	InterDrawNormalTop();

	Write (24, 7, language["STR_BONUS"], true);
	Write (24, 10, language["STR_TIME"], true);
	Write (24, 12, language["STR_PAR"], true);

	// Write the starting value based on InterState.bonus in case ForceTally is on
	FString bonusstr;
	bonusstr.Format("%u", InterState.bonus);
	Write (36, 7, bonusstr, true);

	Write (37, 14, "%");
	Write (37, 16, "%");
	Write (37, 18, "%");
	Write (29, 14, language["STR_RAT2KILL"], true);
	Write (29, 16, language["STR_RAT2SECRET"], true);
	Write (29, 18, language["STR_RAT2TREASURE"], true);

	InterWriteTime(levelInfo->Par, 26*8, 12*8);

	//
	// PRINT TIME
	//
	InterWriteTime(gamestate.TimeCount/TICRATE, 26*8, 10*8);

	VW_UpdateScreen ();
	VW_FadeIn ();

	//
	// PRINT TIME BONUS
	//
	if(InterState.timeleft)
		InterAddBonus(InterState.timeleft * PAR_AMOUNT, true);
	if (InterState.bonus)
	{
		VW_UpdateScreen ();

		SD_PlaySound ("misc/end_bonus2");
		while (SD_SoundPlaying ())
			BJ_Breathe ();
	}

	InterCountRatio(InterState.kr, 296, 112);
	InterCountRatio(InterState.sr, 296, 112+16);
	InterCountRatio(InterState.tr, 296, 112+32);

	InterGiveBonus (InterState.bonus);
}

static void InterDoGraphical()
{
	InterDrawGraphicalTop();

	VWB_DrawGraphic(TexMan(GraphicalTexID[WI_BONUS]), 104, 72);
	// Write the starting value based on InterState.bonus in case ForceTally is on
	FString bonusstr;
	bonusstr.Format("%u", InterState.bonus);
	Write (36, 9, bonusstr, true, true);

	VW_UpdateScreen ();
	VW_FadeIn ();

	//
	// PRINT TIME BONUS
	//
	if(InterState.timeleft)
		InterAddBonus(InterState.timeleft * PAR_AMOUNT, true);
	if (InterState.bonus)
	{
		VW_UpdateScreen ();

		SD_PlaySound ("misc/end_bonus2");
		while (SD_SoundPlaying ())
			BJ_Breathe ();
	}

	VWB_DrawGraphic(TexMan(GraphicalTexID[WI_TIME]), 88, 128);
	VWB_DrawGraphic(TexMan(GraphicalTexID[WI_PAR]), 96, 112);

	InterWriteTime(levelInfo->Par, 19*8, 14*8);

	//
	// PRINT TIME
	//
	InterWriteTime(gamestate.TimeCount/TICRATE, 19*8, 16*8);

	double cleary = 104;
	{
		// Really all we care about here is finding the starting y
		// since we need to over clear a bit in order to account for
		// rounding errors and so we don't need to worry about fonts.
		double clearx = 0, clearw = 0, clearh = 0;
		screen->VirtualToRealCoords(clearx, cleary, clearw, clearh, 320, 200, true, true);
	}

	InterWaitForAck();

	VWB_DrawFill(TexMan(levelInfo->GetBorderTexture()), 0., cleary, (double)screenWidth, (double)statusbary2);
	VWB_DrawGraphic(TexMan(GraphicalTexID[WI_KILLS]), 80, 104);
	VWB_DrawGraphic(TexMan(GraphicalTexID[WI_TREASR]), 104, 120);
	VWB_DrawGraphic(TexMan(GraphicalTexID[WI_SECRTS]), 72, 136);
	Write (27, 13, "0%");
	Write (27, 15, "0%");
	Write (27, 17, "0%");

	InterCountRatio(InterState.kr, 232, 104);
	InterCountRatio(InterState.tr, 232, 104+16);
	InterCountRatio(InterState.sr, 232, 104+32);

	InterGiveBonus (InterState.bonus);

	if(InterState.kr == 100 && InterState.sr == 100 && InterState.tr == 100)
	{
		VWB_DrawFill(TexMan(levelInfo->GetBorderTexture()), 0., cleary, (double)screenWidth, (double)statusbary2);
		VWB_DrawGraphic(TexMan(GraphicalTexID[WI_PERFCT]), 96, 120);
		SD_PlaySound ("misc/100percent");
	}
}

static void DetermineIntermissionMode()
{
	static bool modeUndetermined = true;
	if(modeUndetermined)
	{
		modeUndetermined = false;
		InterState.graphical = true;
		for(unsigned int i = 0;i < NUM_WI;++i)
		{
			if(!(GraphicalTexID[i] = TexMan.CheckForTexture(GraphicalTexNames[i], FTexture::TEX_Any)).isValid())
			{
				InterState.graphical = false;
				break;
			}
		}
	}
}

/*
==================
=
= LevelCompleted
=
= Entered with the screen faded out
= Still in split screen mode with the status bar
=
= Exit with the screen faded out
=
==================
*/

void LevelCompleted (void)
{
	DetermineIntermissionMode();

	InterState.bonus = 0;
	InterState.acked = false;

	//
	// FIGURE RATIOS OUT BEFOREHAND
	//
	InterState.kr = InterState.sr = InterState.tr = 100;
	if (gamestate.killtotal)
		InterState.kr = (gamestate.killcount * 100) / gamestate.killtotal;
	if (gamestate.secrettotal)
		InterState.sr = (gamestate.secretcount * 100) / gamestate.secrettotal;
	if (gamestate.treasuretotal)
		InterState.tr = (gamestate.treasurecount * 100) / gamestate.treasuretotal;

	InterState.timeleft = 0;
	if ((unsigned)gamestate.TimeCount < levelInfo->Par * TICRATE)
		InterState.timeleft = (int) (levelInfo->Par - gamestate.TimeCount/TICRATE);

	if(levelInfo->LevelBonus == -1 || levelInfo->ForceTally)
	{
		//
		// SAVE RATIO INFORMATION FOR ENDGAME
		//
		LevelRatios.killratio += InterState.kr;
		LevelRatios.secretsratio += InterState.sr;
		LevelRatios.treasureratio += InterState.tr;
		LevelRatios.time += gamestate.TimeCount/TICRATE;
		LevelRatios.par += levelInfo->Par;
		++LevelRatios.numLevels;
	}

//
// do the intermission
//
	ClearSplitVWB ();           // set up for double buffering in split screen
	VWB_DrawFill(TexMan(levelInfo->GetBorderTexture()), 0, 0, screenWidth, screenHeight);
	DrawPlayScreen(true);

	StartCPMusic (gameinfo.IntermissionMusic);

	IN_ClearKeysDown ();
	IN_StartAck (ACK_Any);

	BJ_Breathe(true);

	if(levelInfo->LevelBonus == -1 || levelInfo->ForceTally)
	{
		if(levelInfo->LevelBonus > 0)
			InterState.bonus = levelInfo->LevelBonus;

		if(InterState.graphical)
			InterDoGraphical();
		else
			InterDoNormal();
	}
	else
	{
		InterDoBonus();
	}


	StatusBar->DrawStatusBar();
	VW_UpdateScreen ();

	InterWaitForAck();
}

//==========================================================================

/*
==================
=
= Victory
=
==================
*/

void Victory (bool fromIntermission)
{
	DetermineIntermissionMode();

	int kr = 0, sr = 0, tr = 0;

	StartCPMusic (gameinfo.VictoryMusic);
	VWB_DrawFill(TexMan(levelInfo->GetBorderTexture()), 0, 0, screenWidth, screenHeight);
	if(!fromIntermission)
		DrawPlayScreen(true);

	if(LevelRatios.numLevels)
	{
		kr = LevelRatios.killratio / LevelRatios.numLevels;
		sr = LevelRatios.secretsratio / LevelRatios.numLevels;
		tr = LevelRatios.treasureratio / LevelRatios.numLevels;
	}

	if(InterState.graphical)
	{
		VWB_DrawGraphic (TexMan("L_BJWINS"), 8, 8);
		VWB_DrawGraphic (TexMan(GraphicalTexID[WI_RATING]), 104, 32);
		VWB_DrawGraphic (TexMan(GraphicalTexID[WI_PAR]), 120, 56);
		VWB_DrawGraphic (TexMan(GraphicalTexID[WI_TIME]), 112, 72);
		VWB_DrawGraphic (TexMan(GraphicalTexID[WI_KILLS]), 104, 96);
		VWB_DrawGraphic (TexMan(GraphicalTexID[WI_TREASR]), 128, 112);
		VWB_DrawGraphic (TexMan(GraphicalTexID[WI_SECRTS]), 96, 128);

		InterWriteTime(LevelRatios.par, 184, 56, true);
		InterWriteTime(LevelRatios.time, 184, 72, true);

		FString ratioStr;
		ratioStr.Format("%u%%", kr);
		Write(35, 12, ratioStr, true);
		ratioStr.Format("%u%%", tr);
		Write(35, 14, ratioStr, true);
		ratioStr.Format("%u%%", sr);
		Write(35, 16, ratioStr, true);
	}
	else
	{
		static const unsigned int RATIOX = 22, RATIOY = 14, TIMEX = 14, TIMEY = 8;
		int min, sec;
		char tempstr[13];

		VWB_DrawGraphic (TexMan("L_BJWINS"), 8, 4);

		Write (18, 2, language["STR_YOUWIN"]);

		Write (TIMEX, TIMEY - 2, language["STR_TOTALTIME"]);

		Write (12, RATIOY - 2, language["STR_AVERAGES"]);

		Write (RATIOX, RATIOY, language["STR_RATKILL"], true);
		Write (RATIOX, RATIOY + 2, language["STR_RATSECRET"], true);
		Write (RATIOX, RATIOY + 4, language["STR_RATTREASURE"], true);
		Write (RATIOX+8, RATIOY, "%");
		Write (RATIOX+8, RATIOY + 2, "%");
		Write (RATIOX+8, RATIOY + 4, "%");

		sec = LevelRatios.time;

		min = sec / 60;
		sec %= 60;

		if (min > 99)
			min = sec = 99;

		FString timeString;
		timeString.Format("%02d:%02d", min, sec);
		Write (TIMEX, TIMEY, timeString);

		itoa (kr, tempstr, 10);
		Write (RATIOX + 8, RATIOY, tempstr, true);

		itoa (sr, tempstr, 10);
		Write (RATIOX + 8, RATIOY + 2, tempstr, true);

		itoa (tr, tempstr, 10);
		Write (RATIOX + 8, RATIOY + 4, tempstr, true);
	}

	VW_UpdateScreen ();
	VW_FadeIn ();

	IN_Ack (ACK_Any);

	EndText (levelInfo->Cluster);

	VW_FadeOut();
}

//==========================================================================


/*
=================
=
= PreloadGraphics
=
= Fill the cache up
=
=================
*/

bool PreloadUpdate (unsigned current, unsigned total)
{
	static const PalEntry colors[2] = {
		ColorMatcher.Pick(RPART(gameinfo.PsychedColors[0]), GPART(gameinfo.PsychedColors[0]), BPART(gameinfo.PsychedColors[0])),
		ColorMatcher.Pick(RPART(gameinfo.PsychedColors[1]), GPART(gameinfo.PsychedColors[1]), BPART(gameinfo.PsychedColors[1]))
	};

	double x = 53;
	double y = 101 + gameinfo.PsychedOffset;
	double w = 214.0*current/total;
	double h = 2;
	double ow = w - 1;
	double oh = h - 1;
	double ox = x, oy = y;
	screen->VirtualToRealCoords(x, y, w, h, 320, 200, true, true);
	screen->VirtualToRealCoords(ox, oy, ow, oh, 320, 200, true, true);

	if (current)
	{
		VWB_Clear(colors[0], x, y, x+w, y+h);
		VWB_Clear(colors[1], ox, oy, ox+ow, oy+oh);

	}
	VW_UpdateScreen ();
	return (false);
}

void PreloadGraphics (bool showPsych)
{
	if(showPsych)
	{
		ClearSplitVWB ();           // set up for double buffering in split screen

		VWB_DrawFill(TexMan(levelInfo->GetBorderTexture()), 0, 0, screenWidth, screenHeight);

		const bool oldingame = ingame;
		ingame = false;
		DrawPlayScreen(true);
		ingame = oldingame;

		VWB_DrawGraphic(TexMan("GETPSYCH"), 48, 56);

		WindowX = (screenWidth - scaleFactorX*224)/2;
		WindowY = (screenHeight - scaleFactorY*(StatusBar->GetHeight(false)+48))/2;
		WindowW = scaleFactorX * 28 * 8;
		WindowH = scaleFactorY * 48;

		VW_UpdateScreen ();
		VW_FadeIn ();

		PreloadUpdate (5, 10);
	}

	TexMan.PrecacheLevel();

	if(showPsych)
	{
		PreloadUpdate (10, 10);
		IN_UserInput (70, ACK_Any);
		VW_FadeOut ();

		DrawPlayScreen ();
		VW_UpdateScreen ();
	}
}


//==========================================================================

/*
==================
=
= DrawHighScores
=
==================
*/

void DrawHighScores (void)
{
	FString buffer;

	word i, w, h;
	HighScore *s;

	FFont *font = V_GetFont(gameinfo.HighScoresFont);

	ClearMScreen ();

	FTexture *highscores = TexMan("HGHSCORE");
	DrawStripes (10);
	if(highscores->GetScaledWidth() < 320)
		VWB_DrawGraphic(highscores, 160-highscores->GetScaledWidth()/2, 0, MENU_TOP);
	else
		VWB_DrawGraphic(highscores, 0, 0, MENU_TOP);

	static FTextureID texName = TexMan.CheckForTexture("M_NAME", FTexture::TEX_Any);
	static FTextureID texLevel = TexMan.CheckForTexture("M_LEVEL", FTexture::TEX_Any);
	static FTextureID texScore = TexMan.CheckForTexture("M_SCORE", FTexture::TEX_Any);
	if(texName.isValid())
		VWB_DrawGraphic(TexMan(texName), 16, 68);
	if(texLevel.isValid())
		VWB_DrawGraphic(TexMan(texLevel), 194 - TexMan(texLevel)->GetScaledWidth()/2, 68);
	if(texScore.isValid())
		VWB_DrawGraphic(TexMan(texScore), 240, 68);

	for (i = 0, s = Scores; i < MaxScores; i++, s++)
	{
		PrintY = 76 + ((font->GetHeight() + 3) * i);

		//
		// name
		//
		PrintX = 16;
		US_Print (font, s->name, gameinfo.FontColors[GameInfo::HIGHSCORES]);

		//
		// level
		//
		buffer.Format("%s", s->completed.GetChars());
		VW_MeasurePropString (font, buffer, w, h);
		PrintX = 194 - w;

		bool drawNumber = true;
		if (s->graphic[0])
		{
			FTextureID graphic = TexMan.CheckForTexture(s->graphic, FTexture::TEX_Any);
			if(graphic.isValid())
			{
				FTexture *tex = TexMan(graphic);

				drawNumber = false;
				VWB_DrawGraphic (tex, 194 - tex->GetScaledWidth(), PrintY - 1, MENU_CENTER);
			}
		}

		if(drawNumber)
			US_Print (font, buffer, gameinfo.FontColors[GameInfo::HIGHSCORES]);

		//
		// score
		//
		buffer.Format("%d", s->score);
		VW_MeasurePropString (font, buffer, w, h);
		PrintX = 292 - w;
		US_Print (font, buffer, gameinfo.FontColors[GameInfo::HIGHSCORES]);
	}

	VW_UpdateScreen ();
}

//===========================================================================


/*
=======================
=
= CheckHighScore
=
=======================
*/

void CheckHighScore (int32_t score, const LevelInfo *levelInfo)
{
	if (!gameinfo.TrackHighScores || Net::InitVars.mode != Net::MODE_SinglePlayer)
		return;

	word i, j;
	int n;
	HighScore myscore;

	strcpy (myscore.name, "");
	myscore.score = score;
	myscore.completed = levelInfo->FloorNumber;
	if(levelInfo->HighScoresGraphic.isValid())
	{
		strncpy(myscore.graphic, TexMan[levelInfo->HighScoresGraphic]->Name, 8);
		myscore.graphic[8] = 0;
	}
	else
		myscore.graphic[0] = 0;

	for (i = 0, n = -1; i < MaxScores; i++)
	{
		if ((myscore.score > Scores[i].score)
			|| ((myscore.score == Scores[i].score) && (myscore.completed.Compare(Scores[i].completed) > 0)))
		{
			for (j = MaxScores; --j > i;)
				Scores[j] = Scores[j - 1];
			Scores[i] = myscore;
			n = i;
			break;
		}
	}

	StartCPMusic (gameinfo.ScoresMusic);
	DrawHighScores ();

	VW_FadeIn ();

	if (n != -1)
	{
		FFont *font = V_GetFont(gameinfo.HighScoresFont);

		//
		// got a high score
		//
		PrintY = 76 + ((font->GetHeight() + 3) * n);
		PrintX = 16;
		US_LineInput (font,PrintX, PrintY, Scores[n].name, 0, true, MaxHighName, 130, BKGDCOLOR, CR_WHITE);
	}
	else
	{
		IN_ClearKeysDown ();
		IN_UserInput (500, ACK_Local);
	}

	VW_FadeOut();
}
