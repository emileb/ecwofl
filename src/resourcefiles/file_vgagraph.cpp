#include "wl_def.h"
#include "resourcefile.h"
#include "w_wad.h"
#include "lumpremap.h"
#include "zstring.h"

struct Huffnode
{
	public:
		WORD	bit0, bit1;	// 0-255 is a character, > is a pointer to a node
};

struct Dimensions
{
	public:
		WORD	width, height;
};

////////////////////////////////////////////////////////////////////////////////

struct FVGALump : public FResourceLump
{
	public:
		DWORD		position;
		DWORD		length;
		Huffnode*	huffman;

		bool		isImage;
		bool		noSkip;
		Dimensions	dimensions;

		int FillCache()
		{
			Owner->Reader->Seek(position+(noSkip ? 0 : 4), SEEK_SET);

			byte* data = new byte[length];
			byte* out = new byte[LumpSize];
			Owner->Reader->Read(data, length);
			HuffExpand(data, out);
			delete[] data;

			Cache = new char[LumpSize];
			if(!isImage)
				memcpy(Cache, out, LumpSize);
			else
			{
				memcpy(Cache, &dimensions, 4);
				memcpy(Cache+4, out, LumpSize-4);
			}
			delete[] out;

			RefCount = 1;
			return 1;
		}

		void HuffExpand(byte* source, byte* dest)
		{
			byte *end;
			Huffnode *headptr, *huffptr;
		
			if(!LumpSize || !dest)
			{
				Quit("length or dest is null!");
				return;
			}
		
			headptr = huffman+254;        // head node is always node 254
		
			int written = 0;
		
			end=dest+LumpSize;
		
			byte val = *source++;
			byte mask = 1;
			word nodeval;
			huffptr = headptr;
			while(1)
			{
				if(!(val & mask))
					nodeval = huffptr->bit0;
				else
					nodeval = huffptr->bit1;
				if(mask==0x80)
				{
					val = *source++;
					mask = 1;
				}
				else mask <<= 1;
			
				if(nodeval<256)
				{
					*dest++ = (byte) nodeval;
					written++;
					huffptr = headptr;
					if(dest>=end) break;
				}
				else
				{
					huffptr = huffman + (nodeval - 256);
				}
			}
		}
};

////////////////////////////////////////////////////////////////////////////////

class FVGAGraph : public FResourceFile
{
	public:
		FVGAGraph(const char* filename, FileReader *file) : FResourceFile(filename, file), vgagraphFile(filename), lumps(NULL)
		{
			FString path(filename);
			int lastSlash = path.LastIndexOfAny("/\\");
			extension = path.Mid(lastSlash+10);
			path = path.Left(lastSlash+1);

			vgadictFile = path + "vgadict." + extension;
			vgaheadFile = path + "vgahead." + extension;
		}
		~FVGAGraph()
		{
			if(lumps != NULL)
				delete[] lumps;
		}

		bool Open(bool quiet)
		{
			FileReader vgadictReader;
			if(!vgadictReader.Open(vgadictFile))
				return false;
			vgadictReader.Read(huffman, sizeof(huffman));

			FileReader vgaheadReader;
			if(!vgaheadReader.Open(vgaheadFile))
				return false;
			vgaheadReader.Seek(0, SEEK_END);
			NumLumps = vgaheadReader.Tell()/3;
			vgaheadReader.Seek(0, SEEK_SET);
			lumps = new FVGALump[NumLumps];
			// The vgahead has 24-bit ints.
			BYTE* data = new BYTE[NumLumps*3];
			vgaheadReader.Read(data, NumLumps*3);

			int numPictures = 0;
			Dimensions* dimensions = NULL;
			for(unsigned int i = 0;i < NumLumps;i++)
			{
				// Give the lump a temporary name.
				char lumpname[9];
				sprintf(lumpname, "VGA%05d", i);
				lumps[i].Owner = this;
				lumps[i].LumpNameSetup(lumpname);

				lumps[i].noSkip = false;
				lumps[i].isImage = (i >= 3 && i-3 < numPictures);
				lumps[i].Namespace = lumps[i].isImage ? ns_graphics : ns_global;
				lumps[i].position = READINT24(&data[i*3]);
				lumps[i].huffman = huffman;

				// The actual length isn't stored so we need to go by the position of the following lump.
				lumps[i].length = 0;
				if(i != 0)
				{
					lumps[i-1].length = lumps[i].position - lumps[i-1].position;
				}

				Reader->Seek(lumps[i].position, SEEK_SET);
				Reader->Read(&lumps[i].LumpSize, 4);
				if(i == 1) // We must do this on the second lump do to how the position is filled.
				{
					Reader->Seek(lumps[0].position+4, SEEK_SET);
					numPictures = lumps[0].LumpSize/4;

					byte* data = new byte[lumps[0].length];
					byte* out = new byte[lumps[0].LumpSize];
					Reader->Read(data, lumps[0].length);
					lumps[0].HuffExpand(data, out);
					delete[] data;

					dimensions = new Dimensions[numPictures];
					for(int j = 0;j < numPictures;j++)
					{
						dimensions[j].width = READINT16(&out[j*4]);
						dimensions[j].height = READINT16(&out[(j*4)+2]);
					}
				}
				else if(lumps[i].isImage)
				{
					lumps[i].dimensions = dimensions[i-3];
					lumps[i].LumpSize += 4;
				}
			}
			// HACK: For some reason id decided the tile8 lump will not tell
			//       its size.  So we need to assume it's right after the
			//       graphics and is 72 tiles long.
			int tile8Position = 3+numPictures;
			if(tile8Position < NumLumps && lumps[tile8Position].LumpSize > lumps[tile8Position].length)
			{
				lumps[tile8Position].noSkip = true;
				lumps[tile8Position].LumpSize = 64*72;
			}
			if(dimensions != NULL)
				delete[] dimensions;
			delete[] data;
			if(!quiet) Printf(", %d lumps\n", NumLumps);

			LumpRemaper::AddFile(extension, this, LumpRemaper::VGAGRAPH);
			return true;
		}

		FResourceLump *GetLump(int no)
		{
			return &lumps[no];
		}

	private:
		Huffnode	huffman[255];
		FVGALump*	lumps;

		FString		extension;
		FString		vgadictFile;
		FString		vgaheadFile;
		FString		vgagraphFile;
};

FResourceFile *CheckVGAGraph(const char *filename, FileReader *file, bool quiet)
{
	FString fname(filename);
	int lastSlash = fname.LastIndexOfAny("/\\");
	if(lastSlash != -1)
		fname = fname.Mid(lastSlash+1, 8);
	else
		fname = fname.Left(8);

	if(fname.Len() == 8 && fname.CompareNoCase("vgagraph") == 0) // file must be vgagraph.something
	{
		FResourceFile *rf = new FVGAGraph(filename, file);
		if(rf->Open(quiet)) return rf;
		delete rf;
	}
	return NULL;
}