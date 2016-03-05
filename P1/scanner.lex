%{ 
/* P1. Implements scanner.  No additional changes required. */

#include "llvm-c/Core.h"
#include "llvm-c/BitReader.h"
#include "llvm-c/BitWriter.h"

#include "parser.h" 
%}

%option nounput
%option noinput
 
%% 

[\t \n]+        ;

vars            { return VARS; }
\$[0-9]+	{ yylval.tmp = strdup(yytext); return TMP; } 
[a-zA-Z_]+      { yylval.id = strdup(yytext); return ID; } 

[0-9]+          { yylval.num = atoi(yytext); return NUM; }

"="		{ return ASSIGN;   } 
";"		{ return SEMI;     } 
"-"		{ return MINUS;    } 
"+"		{ return PLUS;     }  
"*"		{ return MULTIPLY; } 
"/"		{ return DIVIDE;   } 
","	        { return COMMA;    }
"^^"		{ return RAISE;    } 	
"<"	        { return LESSTHAN; }

%%
