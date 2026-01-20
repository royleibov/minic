%{
/* Parser for a MiniC program. Inspired by Tom Niemann Lex & Yacc tutorial calculator */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "ast.h"

extern int yylex();
extern int yylex_destroy();
extern int yywrap();
extern FILE *yyin;

int semantic_analysis(astNode* root);

int yyerror(char*);
extern int yylineno;
astNode* root;
%}
%union{
    int iVal;
    char *sVal;
    astNode *nPtr;
    vector<astNode*> *nPtrList;
}

/* Tokens */
%token <iVal> INTEGER
%token <sVal> IDENT
%token WHILE IF PRINT INT RETURN VOID READ EXTERN
%nonassoc IFX
%nonassoc ELSE

%left GE LE EQ NE '>' '<'
%left '+' '-'
%left '*' '/'
%nonassoc UMINUS

/* Non-terminal types */
%type <nPtr> program extern_decl func_def param_decl block stmt expr
%type <nPtrList> decl_list stmt_list

%%
program:
      extern_decl extern_decl func_def      { $$ = createProg($1, $2, $3);
                                              root = $$; }
    ;

extern_decl:
      EXTERN VOID PRINT '(' INT ')' ';'     { $$ = createExtern("print"); }
    | EXTERN INT READ '(' ')' ';'           { $$ = createExtern("read"); }
    ;

func_def:
      INT IDENT '(' param_decl ')' block    { $$ = createFunc($2, $4, $6);
                                              free($2); }
    ;

param_decl:
      /* empty */                           { $$ = NULL; }
    | INT IDENT                             { $$ = createVar($2); free($2); }
    ;

block:
      '{' decl_list stmt_list '}'           { /* merge decl_list and stmt_list */
                                              for (auto statement : *$3) {
                                                  $2->push_back(statement);
                                              }
                                              delete $3;
                                              $$ = createBlock($2); }
    ;

decl_list:
      /* empty */                           { $$ = new vector<astNode*>(); }
    | decl_list INT IDENT ';'               { $1->push_back(createDecl($3));
                                              free($3);
                                              $$ = $1; }
    ;

stmt_list:
      /* empty */                           { $$ = new vector<astNode*>(); }
    | stmt_list stmt                        { if ($2 != NULL) $1->push_back($2);
                                              $$ = $1; }
    ;

stmt:
      expr ';'                              { $$ = $1; }
    | PRINT '(' expr ')' ';'                { $$ = createCall("print", $3); }
    | RETURN expr ';'                       { $$ = createRet($2); }
    | IDENT '=' expr ';'                    { astNode *lhs = createVar($1);
                                              $$ = createAsgn(lhs, $3);
                                              free($1); }
    | WHILE '(' expr ')' stmt               { $$ = createWhile($3, $5); }
    | IF '(' expr ')' stmt %prec IFX        { $$ = createIf($3, $5); }
    | IF '(' expr ')' stmt ELSE stmt        { $$ = createIf($3, $5, $7); }
    | block                                 { $$ = $1; }
    ;

expr:
      INTEGER                               { $$ = createCnst($1); }
    | IDENT                                 { $$ = createVar($1); free($1); }
    | READ '(' ')'                          { $$ = createCall("read", NULL); }
    | '-' expr %prec UMINUS                 { $$ = createUExpr($2, uminus); }
    | expr '+' expr                         { $$ = createBExpr($1, $3, add); }
    | expr '-' expr                         { $$ = createBExpr($1, $3, sub); }
    | expr '*' expr                         { $$ = createBExpr($1, $3, mul); }
    | expr '/' expr                         { $$ = createBExpr($1, $3, divide); }
    | expr '<' expr                         { $$ = createRExpr($1, $3, lt); }
    | expr '>' expr                         { $$ = createRExpr($1, $3, gt); }
    | expr GE expr                          { $$ = createRExpr($1, $3, ge); }
    | expr LE expr                          { $$ = createRExpr($1, $3, le); }
    | expr NE expr                          { $$ = createRExpr($1, $3, neq); }
    | expr EQ expr                          { $$ = createRExpr($1, $3, eq); }
    | '(' expr ')'                          { $$ = $2; }
    ;

%%
int yyerror(char *s) {
    fprintf(stderr, "line %d: %s\n", yylineno, s);
    return 0;
}

int semantic_analysis(astNode* root) {
    // Placeholder for semantic analysis logic
    // Return 0 if successful, non-zero otherwise
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        yyin = fopen(argv[1], "r");
        if (yyin == NULL) {
            fprintf(stderr, "Could not open file %s\n", argv[1]);
            return 1;
        }
    }

    if (yyparse() == 0 && root != NULL) {
        if (semantic_analysis(root) == 0) {
            printf("Parsing and semantic analysis successful!\n");
            printNode(root);
        } else {
            fprintf(stderr, "Semantic analysis failed.\n");
        }
        freeNode(root);
    } else {
        fprintf(stderr, "Parsing failed.\n");
        fclose(yyin);
        yylex_destroy();
        return 1;
    }

    fclose(yyin);
    yylex_destroy();
    return 0;
}