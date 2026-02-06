#include "frontend.h"

int search_variable(string var_name, vector<unordered_set<string>> *symbol_table_stack) {
    // Search from innermost scope to outermost
    for (auto symbol_table = symbol_table_stack->rbegin(); symbol_table != symbol_table_stack->rend(); ++symbol_table) {
        int found = symbol_table->count(var_name);
        if (found > 0) {
            return found; // Found
        }
    }
    return 0; // Not found
}

int build_symbol_table(astNode* node, vector<unordered_set<string>> *symbol_table_stack, int extend) {
    if (node == NULL) {
        return 0;
    }

    if (node->type == ast_stmt) {
        astStmt *stmt = &node->stmt;

        switch(stmt->type){
            case ast_call: 
                            if (stmt->call.param != NULL) {
                                return build_symbol_table(stmt->call.param, symbol_table_stack);
                            }
                            return 0; // No parameters to process
            case ast_ret: 
                            if (stmt->ret.expr != NULL) {
                                return build_symbol_table(stmt->ret.expr, symbol_table_stack);
                            }
                            return 0; // No expression to process
            case ast_block:
                            if (stmt->block.stmt_list != NULL) {
                                // Push a new symbol table for the block scope
                                if (extend == 0)
                                    symbol_table_stack->push_back(unordered_set<string>());

                                // Traverse each statement in the block
                                int rc = 0;
                                for (auto statement : *stmt->block.stmt_list) {
                                    rc = rc || build_symbol_table(statement, symbol_table_stack);
                                    if (rc != 0) {
                                        if (extend == 0)
                                            symbol_table_stack->pop_back();
                                        return rc;
                                    }
                                }

                                // Pop the symbol table after exiting the block scope
                                if (extend == 0)
                                    symbol_table_stack->pop_back();
                            }
                            return 0; // No statements to process
            case ast_while: {
                            int rc = 0;
                            if (stmt->whilen.cond != NULL) {
                                rc = rc || build_symbol_table(stmt->whilen.cond, symbol_table_stack);
                                if (rc != 0) return rc;
                            }
                            if (stmt->whilen.body != NULL) {
                                return build_symbol_table(stmt->whilen.body, symbol_table_stack);
                            }
                            break;
            }
            case ast_if: {
                            int rc = 0;
                            if (stmt->ifn.cond != NULL) {
                                rc = rc || build_symbol_table(stmt->ifn.cond, symbol_table_stack);
                                if (rc != 0) return rc;
                            } else {
                                fprintf(stderr, "If statement missing condition\n");
                                return 1; // Error
                            }
                            if (stmt->ifn.if_body != NULL) {
                                rc = rc || build_symbol_table(stmt->ifn.if_body, symbol_table_stack);
                                if (rc != 0) return rc;
                            } else {
                                fprintf(stderr, "If statement missing if body\n");
                                return 1; // Error
                            }
                            if (stmt->ifn.else_body != NULL) {
                                return build_symbol_table(stmt->ifn.else_body, symbol_table_stack);
                            }
                            return rc;
            }
            case ast_asgn:	
                            if (stmt->asgn.lhs != NULL) {
                                int rc = build_symbol_table(stmt->asgn.lhs, symbol_table_stack);
                                if (rc != 0) return rc;
                            }
                            if (stmt->asgn.rhs != NULL) {
                                return build_symbol_table(stmt->asgn.rhs, symbol_table_stack);
                            }
                            break;
            case ast_decl:	
                            if (stmt->decl.name != NULL) {
                                string var_name = stmt->decl.name;
                                // Check for redeclaration in the current scope only
                                if (symbol_table_stack->back().count(var_name) > 0) {
                                    fprintf(stderr, "Semantic error: Redeclaration of variable '%s'\n", var_name.c_str());
                                    return 1; // Error
                                }
                                // Add variable to the current scope
                                symbol_table_stack->back().insert(var_name);
                                return 0; // Success
                            }
                            break;
            default: {
                        // No other statement types exist, so should not reach here
                        fprintf(stderr, "Incorrect statement type\n");
                        return 1; // Error
                    };
        }
        fprintf(stderr, "Unhandled statement type\n");
        return 1; // Error
    } else {
        switch(node->type){
            case ast_prog:
                            if (node->prog.func != NULL) {
                                return build_symbol_table(node->prog.func, symbol_table_stack);
                            }
            case ast_func:
                            if (node->func.param != NULL) {
                                // Push a new symbol table for the function scope
                                symbol_table_stack->push_back(unordered_set<string>());

                                int param_success = build_symbol_table(node->func.param, symbol_table_stack);

                                if (node->func.body != NULL) {
                                    int body_success = build_symbol_table(node->func.body, symbol_table_stack, 1);
                                    // Pop the symbol table after exiting the function scope
                                    symbol_table_stack->pop_back();
                                    return param_success || body_success;
                                }
                                // Pop the symbol table after exiting the function scope
                                symbol_table_stack->pop_back();
                                return param_success;
                            } else if (node->func.body != NULL) {
                                // Inner statement recursions will take care of symbol table stack management
                                return build_symbol_table(node->func.body, symbol_table_stack);
                            }
                            break;
            case ast_stmt:
                            // Should never reach here, handled above
                            fprintf(stderr, "Incorrectly reached ast_stmt in outer recursion\n");
                            return build_symbol_table(node, symbol_table_stack);
            case ast_extern:
                            return 0; // No symbol table changes needed
            case ast_var:
                            if (node->var.name != NULL) {
                                string var_name = node->var.name;
                                // Check if variable is declared in any accessible scope
                                if (search_variable(var_name, symbol_table_stack) == 0) {
                                    fprintf(stderr, "Semantic error: Undeclared variable '%s'\n", var_name.c_str());
                                    return 1; // Error
                                }
                                return 0; // Success
                            }
                            break;
            case ast_cnst:
                            return 0; // No symbol table changes needed
            case ast_rexpr: 
                            if (node->rexpr.lhs != NULL) {
                                int rc = build_symbol_table(node->rexpr.lhs, symbol_table_stack);
                                if (rc != 0) return rc;
                            }
                            if (node->rexpr.rhs != NULL) {
                                return build_symbol_table(node->rexpr.rhs, symbol_table_stack);
                            }
                            break;
            case ast_bexpr: 
                            if (node->bexpr.lhs != NULL) {
                                int rc = build_symbol_table(node->bexpr.lhs, symbol_table_stack);
                                if (rc != 0) return rc;
                            }
                            if (node->bexpr.rhs != NULL) {
                                return build_symbol_table(node->bexpr.rhs, symbol_table_stack);
                            }
                            break;
            case ast_uexpr: 
                            if (node->uexpr.expr != NULL) {
                                return build_symbol_table(node->uexpr.expr, symbol_table_stack);
                            }
                            break;
            default: {
                        // No other node types exist, so should not reach here
                        fprintf(stderr, "Incorrect AST node type\n");
                        return 1; // Error
                    }
        }
        fprintf(stderr, "Unhandled AST node type\n");
        return 1; // Error
    }
    return 0; // Success ???
}

int semantic_analysis(astNode* root) {
    vector<unordered_set<string>> symbol_table_stack;

    // Recursive semantic analysis function - DFS traversal of AST
    return build_symbol_table(root, &symbol_table_stack);
}
