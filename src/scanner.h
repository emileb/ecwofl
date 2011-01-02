/*
** Copyright (c) 2010, Braden "Blzut3" Obrzut
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * The names of its contributors may be used to endorse or promote
**       products derived from this software without specific prior written
**       permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT,
** INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __SCANNER_H__
#define __SCANNER_H__

#include "scanner_support.h"

enum
{
	TK_Identifier,		// Ex: SomeIdentifier
	TK_StringConst,		// Ex: "Some String"
	TK_IntConst,		// Ex: 27
	TK_FloatConst,		// Ex: 1.5
	TK_BoolConst,		// Ex: true
	TK_AndAnd,			// &&
	TK_OrOr,			// ||
	TK_EqEq,			// ==
	TK_NotEq,			// !=
	TK_GtrEq,			// >=
	TK_LessEq,			// <=
	TK_ShiftLeft,		// <<
	TK_ShiftRight,		// >>
	TK_Increment,		// ++
	TK_Decrement,		// --
	TK_PointerMember,	// ->
	TK_ScopeResolution,	// ::
	TK_MacroConcat,		// ##

	TK_NumSpecialTokens,

	TK_NoToken = -1
};

class Scanner
{
	public:
		enum MessageLevel
		{
			ERROR,
			WARNING,
			NOTICE
		};

		Scanner(const char* data, int length=-1);
		~Scanner();

		void			CheckForMeta();
		void			CheckForWhitespace();
		bool			CheckToken(char token);
		void			ExpandState();
		const char*		GetData() const { return data; }
		int				GetLine() const { return tokenLine; }
		int				GetLinePos() const { return tokenLinePosition; }
		int				GetPos() const { return logicalPosition; }
		unsigned int	GetScanPos() const { return scanPos; }
		bool			GetNextString();
		bool			GetNextToken(bool expandState=true);
		void			MustGetToken(char token);
		void			ScriptMessage(MessageLevel level, const char* error, ...) const;
		void			SetScriptIdentifier(const SCString &ident) { scriptIdentifier = ident; }
		int				SkipLine();
		bool			TokensLeft() const;

		static const SCString	&Escape(SCString &str);
		static const SCString	Escape(const char *str);
		static const SCString	&Unescape(SCString &str);

		SCString		str;
		unsigned int	number;
		double			decimal;
		bool			boolean;
		char			token;

	protected:
		void	IncrementLine();

	private:
		struct ParserState
		{
			SCString		str;
			unsigned int	number;
			double			decimal;
			bool			boolean;
			char			token;
			unsigned int	tokenLine;
			unsigned int	tokenLinePosition;
		}				nextState;

		char*			data;
		unsigned int	length;

		unsigned int	line;
		unsigned int	lineStart;
		unsigned int	logicalPosition;
		unsigned int	tokenLine;
		unsigned int	tokenLinePosition;
		unsigned int	scanPos;

		bool			needNext; // If checkToken returns false this will be false.

		SCString		scriptIdentifier;
};

#endif /* __SCANNER_H__ */