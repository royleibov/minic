/* MiniC Parser Driver */
#include "frontend/frontend.h"

/* Global AST root - written by yacc grammar rules */
astNode* root = NULL;

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
