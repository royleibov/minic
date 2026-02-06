%{
/* Parser for a MiniC program. Inspired by Tom Niemann Lex & Yacc tutorial calculator */
#include "frontend.h"
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

%left GE LE EQ NEQ '>' '<'
%left '+' '-'
%left '*' '/'
%nonassoc UMINUS

/* Non-terminal types */
%type <nPtr> program extern_decl func_def func_param_decl block stmt expr term
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
      INT IDENT '(' func_param_decl ')' block    { $$ = createFunc($2, $4, $6);
                                              free($2); }
    ;

func_param_decl:
      /* empty */                           { $$ = NULL; }
    | INT IDENT                             { $$ = createDecl($2); free($2); }
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
    | RETURN '(' expr ')' ';'               { $$ = createRet($3); }
    | IDENT '=' expr ';'                    { astNode *lhs = createVar($1);
                                              $$ = createAsgn(lhs, $3);
                                              free($1); }
    | WHILE '(' expr ')' stmt               { $$ = createWhile($3, $5); }
    | IF '(' expr ')' stmt %prec IFX        { $$ = createIf($3, $5); }
    | IF '(' expr ')' stmt ELSE stmt        { $$ = createIf($3, $5, $7); }
    | block                                 { $$ = $1; }
    ;

expr:
      term                                  { $$ = $1; }
    | READ '(' ')'                          { $$ = createCall("read", NULL); }
    | '-' term %prec UMINUS                 { $$ = createUExpr($2, uminus); }
    | term '+' term                         { $$ = createBExpr($1, $3, add); }
    | term '-' term                         { $$ = createBExpr($1, $3, sub); }
    | term '*' term                         { $$ = createBExpr($1, $3, mul); }
    | term '/' term                         { $$ = createBExpr($1, $3, divide); }
    | term '<' term                         { $$ = createRExpr($1, $3, lt); }
    | term '>' term                         { $$ = createRExpr($1, $3, gt); }
    | term GE term                          { $$ = createRExpr($1, $3, ge); }
    | term LE term                          { $$ = createRExpr($1, $3, le); }
    | term NEQ term                         { $$ = createRExpr($1, $3, neq); }
    | term EQ term                          { $$ = createRExpr($1, $3, eq); }
    ;

term:
      INTEGER                               { $$ = createCnst($1); }
    | IDENT                                 { $$ = createVar($1); free($1); }
    ;

%%

int yyerror(const char *s) {
    fprintf(stderr, "line %d: %s\n", yylineno, s);
    return 0;
}