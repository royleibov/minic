#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>

#include <unordered_set>
#include <list>
using namespace std;

#define DEBUGGING 1

/* This function reads the given llvm file and loads the LLVM IR into
	 data-structures that we can works on for optimization phase.
*/
LLVMModuleRef createLLVMModel(char * filename){
	char *err = 0;

	LLVMMemoryBufferRef ll_f = 0;
	LLVMModuleRef m = 0;

	LLVMCreateMemoryBufferWithContentsOfFile(filename, &ll_f, &err);

	if (err != NULL) { 
		printf("%s\n", err);
		return NULL;
	}
	
	LLVMParseIRInContext(LLVMGetGlobalContext(), ll_f, &m, &err);

	if (err != NULL) {
		printf("%s\n", err);
	}

	return m;
}

void walkBBInstructions(LLVMBasicBlockRef bb){
	for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction;
  				instruction = LLVMGetNextInstruction(instruction)) {

        printf("Inst: \n");
        LLVMDumpValue(instruction);
        printf("    \n");
    }
}

void walkBasicblocks(LLVMValueRef function){
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
		
		printf("In basic block\n");
        // LLVMDumpValue(basicBlock);
        walkBBInstructions(basicBlock);
	}
}

void walkFunctions(LLVMModuleRef module){
	for (LLVMValueRef function =  LLVMGetFirstFunction(module); 
			function; 
			function = LLVMGetNextFunction(function)) {

		const char* funcName = LLVMGetValueName(function);	

		printf("Function Name: %s\n", funcName);

		walkBasicblocks(function);
 	}
}

// ---- Subexpression elimination ----

bool operandsEqual(LLVMValueRef op1, LLVMValueRef op2) {
    // Same pointer — always equal (covers SSA variables)
    if (op1 == op2)
        return true;

    // Both are integer constants — compare their values
    if (LLVMIsAConstantInt(op1) && LLVMIsAConstantInt(op2)) {
        return LLVMConstIntGetSExtValue(op1) == LLVMConstIntGetSExtValue(op2);
    }

    return false;
}

int subexprElimination(LLVMModuleRef module){
	bool changed = false;

    // Walk through functions, basic blocks, and instructions
    for (LLVMValueRef function =  LLVMGetFirstFunction(module); 
			function; 
			function = LLVMGetNextFunction(function)) {

        if (DEBUGGING) {
            const char* funcName = LLVMGetValueName(function);	

            printf("Function Name: %s\n", funcName);
        }

		// Walk through basic blocks
        for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {

            if (DEBUGGING) {
                printf("In basic block\n");
            }

            // Walk through instructions
            for (LLVMValueRef inst = LLVMGetFirstInstruction(basicBlock); inst;
  					inst = LLVMGetNextInstruction(inst)) {

				if (LLVMIsACmpInst(inst) || LLVMIsACallInst(inst) || LLVMIsAAllocaInst(inst)
					|| LLVMIsATerminatorInst(inst) || LLVMIsAStoreInst(inst)) {
					// Note all these instruction types have side effects or are control flow instructions, 
					// so we cannot eliminate them.
					// Note, cmp instructions are always followed by a branch instruction, so we cannot eliminate them either.
					continue; // Skip comparisons, calls, allocas, and terminator instructions
				}

				LLVMOpcode op = LLVMGetInstructionOpcode(inst);

				// Inner loop to check if will see the same instruction again later in the basic block
				// O(n^2) :(
				for (LLVMValueRef otherInst = LLVMGetNextInstruction(inst); otherInst;
  					otherInst = LLVMGetNextInstruction(otherInst)) {
					
					// Check if the two instructions are the same (same opcode and same operands)
					if (LLVMGetInstructionOpcode(otherInst) == op) {
						// Check if operands are the same
						unsigned numOperands = LLVMGetNumOperands(inst);
						bool sameOperands = true;
						if (numOperands != LLVMGetNumOperands(otherInst)) {
							sameOperands = false;
							continue; // Skip if different number of operands
						} else {
							for (unsigned i = 0; i < numOperands; i++) {
								LLVMValueRef thisOperand = LLVMGetOperand(inst, i);
								LLVMValueRef otherOperand = LLVMGetOperand(otherInst, i);
								if (!operandsEqual(thisOperand, otherOperand)) {
									sameOperands = false;
									break;
								}
							}

							// Edge case: commutative operations (add, mul)
							if (!sameOperands && (op == LLVMAdd || op == LLVMMul)) {
								sameOperands = true;
								for (unsigned i = 0; i < numOperands; i++) {
									// Check if operands are the same in reverse order
									LLVMValueRef thisOperand = LLVMGetOperand(inst, i);
									LLVMValueRef otherOperand = LLVMGetOperand(otherInst, numOperands - 1 - i);
									if (!operandsEqual(thisOperand, otherOperand)) {
										sameOperands = false;
										break;
									}
								}
							}
						}

						if (sameOperands) {
							changed = true;
							// Found a common subexpression
							LLVMReplaceAllUsesWith(otherInst, inst);

							if (DEBUGGING) {
								printf("Found common subexpression:\n");
								LLVMDumpValue(inst);
								printf("\n Eliminating duplicate:\n");
								LLVMDumpValue(otherInst);
								printf("\n");
								printf("New Basic Block after elimination:\n");
								LLVMDumpValue(LLVMBasicBlockAsValue(basicBlock));
							}
						}
					} else if (op == LLVMLoad && LLVMGetInstructionOpcode(otherInst) == LLVMStore) {
						// All loads after a store to the same address cannot be eliminated
						LLVMValueRef loadAddr = LLVMGetOperand(inst, 0);
						LLVMValueRef storeAddr = LLVMGetOperand(otherInst, 1); // Store
						if (operandsEqual(loadAddr, storeAddr)) {
							// Cannot eliminate any following load after store to same address
							break;
						}
					}
				}
            }
        }
 	}

    if (changed) return 1; // Indicate that we made changes
	else return 0; // No changes made
}

// ---- Dead code elimination ----

int deadcodeElimination(LLVMModuleRef module) {
	bool changed = false;

	// Walk through functions, basic blocks, and instructions
	for (LLVMValueRef function =  LLVMGetFirstFunction(module); 
			function; 
			function = LLVMGetNextFunction(function)) {

		// Walk through basic blocks
		for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {

			list<LLVMValueRef> toDelete; // List of instructions to delete after iteration

			// Walk through instructions
			for (LLVMValueRef inst = LLVMGetFirstInstruction(basicBlock); inst;
  					inst = LLVMGetNextInstruction(inst)) {

				// If the instruction has no uses and is not a terminator, store, call, or alloca, it is dead code
				if (LLVMGetFirstUse(inst) == NULL && !LLVMIsATerminatorInst(inst) && !LLVMIsAStoreInst(inst) 
						&& !LLVMIsACallInst(inst) && !LLVMIsAAllocaInst(inst)) {
					
					changed = true;
					toDelete.push_back(inst); 
					if (DEBUGGING) {
						printf("Found dead code:\n");
						LLVMDumpValue(inst);
						printf("\n");
					}
				}
			}
			// Delete instructions
			for (LLVMValueRef inst : toDelete) {
				LLVMInstructionEraseFromParent(inst);
			}
			if (DEBUGGING && !toDelete.empty()) {
				printf("New Basic Block after dead code elimination:\n");
				LLVMDumpValue(LLVMBasicBlockAsValue(basicBlock));
			}
		}
 	}

	if (changed) return 1; // Indicate that we made changes
	else return 0; // No changes made
}

int main(int argc, char** argv)
{
	LLVMModuleRef m;

	if (argc == 2){
		m = createLLVMModel(argv[1]);
	}
	else{
		m = NULL;
		return 1;
	}

	if (m != NULL){
        int changed = subexprElimination(m);
		printf("Subexpression elimination made changes: %s\n", changed ? "Yes" : "No");
		changed = deadcodeElimination(m);
		printf("Dead code elimination made changes: %s\n", changed ? "Yes" : "No");
    }

	return 0;
}