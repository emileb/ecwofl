#include "lumpremap.h"
#include "w_wad.h"
#include "zstring.h"
#include "scanner.h"

TMap<FName, LumpRemaper> LumpRemaper::remaps;
TMap<int, FName> LumpRemaper::musicReverseMap;
TMap<int, FName> LumpRemaper::vgaReverseMap;

LumpRemaper::LumpRemaper(const char* extension) : mapLumpName(extension)
{
	mapLumpName.ToUpper();
	mapLumpName += "MAP";
}

void LumpRemaper::AddFile(const char* extension, FResourceFile *file, Type type)
{
	LumpRemaper *iter = remaps.CheckKey(extension);
	if(iter == NULL)
	{
		LumpRemaper remaper(extension);
		remaper.AddFile(file, type);
		remaps.Insert(extension, remaper);
		return;
	}
	iter->AddFile(file, type);
}

void LumpRemaper::AddFile(FResourceFile *file, Type type)
{
	RemapFile rFile;
	rFile.file = file;
	rFile.type = type;
	files.Push(rFile);
}

void LumpRemaper::DoRemap()
{
	if(!LoadMap())
		return;

	for(unsigned int i = 0;i < files.Size();i++)
	{
		RemapFile &file = files[i];
		int temp = 0; // Use to count something
		int temp2 = 0;
		for(unsigned int i = 0;i < file.file->LumpCount();i++)
		{
			FResourceLump *lump = file.file->GetLump(i);
			switch(file.type)
			{
				case AUDIOT:
					if(lump->Namespace == ns_sounds)
					{
						if(i < sounds.Size())
							lump->LumpNameSetup(sounds[i]);
						temp++;
					}
					else if(lump->Namespace == ns_music && i-temp < music.Size())
						lump->LumpNameSetup(music[i-temp]);
					break;
				case VGAGRAPH:
					if(i < graphics.Size())
						lump->LumpNameSetup(graphics[i]);
					break;
				case VSWAP:
					if(lump->Namespace == ns_newtextures)
					{
						if(i < textures.Size())
							lump->LumpNameSetup(textures[i]);
						temp++;
						temp2++;
					}
					else if(lump->Namespace == ns_sprites)
					{
						if(i-temp < sprites.Size())
							lump->LumpNameSetup(sprites[i-temp]);
						temp2++;
					}
					else if(lump->Namespace == ns_sounds && i-temp2 < digitalsounds.Size())
					{
						lump->LumpNameSetup(digitalsounds[i-temp2]);
					}
					break;
				default:
					break;
			}
		}
	}
	Wads.InitHashChains();
}

bool LumpRemaper::LoadMap()
{
	int lump = Wads.GetNumForName(mapLumpName);
	if(lump == -1)
	{
		printf("\n");
		return false;
	}
	FWadLump mapLump = Wads.OpenLumpNum(lump);

	char* mapData = new char[Wads.LumpLength(lump)];
	mapLump.Read(mapData, Wads.LumpLength(lump));
	Scanner sc(mapData, Wads.LumpLength(lump));

	while(sc.TokensLeft() > 0)
	{
		if(!sc.CheckToken(TK_Identifier))
			sc.ScriptMessage(Scanner::ERROR, "Expected identifier in map.\n");

		TMap<int, FName> *reverse = NULL;
		TArray<FName> *map = NULL;
		if(sc.str.Compare("graphics") == 0)
		{
			reverse = &vgaReverseMap;
			map = &graphics;
		}
		else if(sc.str.Compare("sprites") == 0)
			map = &sprites;
		else if(sc.str.Compare("sounds") == 0)
			map = &sounds;
		else if(sc.str.Compare("digitalsounds") == 0)
			map = &digitalsounds;
		else if(sc.str.Compare("music") == 0)
		{
			reverse = &musicReverseMap;
			map = &music;
		}
		else if(sc.str.Compare("textures") == 0)
			map = &textures;
		else
			sc.ScriptMessage(Scanner::ERROR, "Unknown map section '%s'.\n", sc.str.GetChars());

		if(!sc.CheckToken('{'))
			sc.ScriptMessage(Scanner::ERROR, "Expected '{'.");
		if(!sc.CheckToken('}'))
		{
			int i = 0;
			while(true)
			{
				if(!sc.CheckToken(TK_StringConst))
					sc.ScriptMessage(Scanner::ERROR, "Expected string constant.\n");
				if(reverse != NULL)
					(*reverse)[i++] = sc.str;
				map->Push(sc.str);
				if(sc.CheckToken('}'))
					break;
				if(!sc.CheckToken(','))
					sc.ScriptMessage(Scanner::ERROR, "Expected ','.\n");
			}
		}
	}
	return true;
}

void LumpRemaper::RemapAll()
{
	TMap<FName, LumpRemaper>::Pair *pair;
	for(TMap<FName, LumpRemaper>::Iterator iter(remaps);iter.NextPair(pair);)
	{
		pair->Value.DoRemap();
	}
}
