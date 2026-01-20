digit     [0-9]
letter    [A-Za-z]
%{
    /* Parser for a MiniC program. Inspired by Tom Niemann Lex & Yacc tutorial calculator */
    #include<stdio.h>
    #include "ast.h"
    #include "y.tab.h"
    extern int yyerror(const char *s);
%}
%%
    /* reserved words */
">="            return GE;
"<="            return LE;
"=="            return EQ;
"!="            return NE;
"while"         return WHILE;
"if"            return IF;
"else"          return ELSE;
"print"         return PRINT;
"int"           return INT;
"return"        return RETURN;
"void"          return VOID;
"read"          return READ;
"extern"        return EXTERN;
    /* variables */
{letter}({letter}|{digit})*   {
            yylval.sVal = strdup(yytext);
            return IDENT;
        }
    /* integers */
{digit}+  {
            yylval.iVal = atoi(yytext);
            return INTEGER;
        }
    /* operators */
[-()<>=+*/;{}.] {
                return *yytext;
             }
    /* skip whitespace */
[ \t]   ;
\n     { yylineno++; }
    /* anything else is an error */
.   ;
%%
int yywrap(void) {
    return 1;
}