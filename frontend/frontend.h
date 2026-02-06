#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Yacc/Lex externals */
extern int yyparse();
extern int yylex();
extern int yylex_destroy();
extern int yywrap();
extern FILE *yyin;
extern int yylineno;

#include <vector>
#include <unordered_set>
#include <string>
using namespace std;

#include "ast.h"

/* Global AST root - defined in minic_parser.c, used by parser.y grammar rules */
extern astNode* root;

/* Frontend functions (frontend.c) */
int yyerror(const char *s);
int search_variable(string var_name, vector<unordered_set<string>> *symbol_table_stack);
int build_symbol_table(astNode* node, vector<unordered_set<string>> *symbol_table_stack, int extend = 0);
int semantic_analysis(astNode* root);

#endif
