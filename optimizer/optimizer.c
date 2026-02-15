#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>

#include <unordered_set>
#include <unordered_map>
#include <list>
#include <vector>
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

            // if (DEBUGGING) {
            //     printf("In basic block\n");
            // }

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

// ---- Constant folding ----

int constantFolding(LLVMModuleRef module) {
	bool changed = false;

	for (LLVMValueRef function =  LLVMGetFirstFunction(module); 
			function; 
			function = LLVMGetNextFunction(function)) {

		for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {

			for (LLVMValueRef inst = LLVMGetFirstInstruction(basicBlock); inst;
  					inst = LLVMGetNextInstruction(inst)) {

				// check if the instruction involves only constants and is a binary operator
				if (LLVMIsABinaryOperator(inst) && LLVMGetFirstUse(inst) != NULL) {
					LLVMValueRef op1 = LLVMGetOperand(inst, 0);
					LLVMValueRef op2 = LLVMGetOperand(inst, 1);

					if (LLVMIsAConstantInt(op1) && LLVMIsAConstantInt(op2)) {
						LLVMValueRef foldedConst = NULL;

						LLVMOpcode opcode = LLVMGetInstructionOpcode(inst);

						switch (opcode) {
							case LLVMAdd:
								foldedConst = LLVMConstAdd(op1, op2);
								break;
							case LLVMSub:
								foldedConst = LLVMConstSub(op1, op2);
								break;
							case LLVMMul:
								foldedConst = LLVMConstMul(op1, op2);
								break;
							default:
								// Not a supported binary operator for folding
								printf("Unsupported opcode for constant folding: %u\n", opcode);
								break;
						}

						if (foldedConst != NULL) {
							LLVMReplaceAllUsesWith(inst, foldedConst);
							changed = true;
							if (DEBUGGING) {
								printf("Folded constant expression:\n");
								LLVMDumpValue(inst);
								printf("\n into:\n");
								LLVMDumpValue(foldedConst);
								printf("\n");
								printf("New Basic Block after constant folding:\n");
								LLVMDumpValue(LLVMBasicBlockAsValue(basicBlock));
							}
						}
					}
				}		
			}
		}
 	}

	if (changed) return 1; // Indicate that we made changes
	else return 0; // No changes made
}

// ---- Global constant propagation ----

// Structure to hold all dataflow sets for a basic block
struct BBDataflow {
    unordered_set<LLVMValueRef> genSet;
    unordered_set<LLVMValueRef> killSet;
    unordered_set<LLVMValueRef> inSet;
    unordered_set<LLVMValueRef> outSet;
};

int constantPropagation(LLVMModuleRef module) {
	bool changed = false;

	for (LLVMValueRef function =  LLVMGetFirstFunction(module); 
			function; 
			function = LLVMGetNextFunction(function)) {

		// set of all store instructions in the function
		unordered_set<LLVMValueRef> allStores;
		for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function);
			bb;
			bb = LLVMGetNextBasicBlock(bb)) {
			
			for (LLVMValueRef inst = LLVMGetFirstInstruction(bb); 
				inst;
				inst = LLVMGetNextInstruction(inst)) {
				
				if (LLVMIsAStoreInst(inst)) {
					allStores.insert(inst);
				}
			}
		}

		unordered_map<LLVMBasicBlockRef, BBDataflow> bbDataflows; // map from basic block to its dataflow sets

		// Create GEN and KILL sets for each basic block
		for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {

			BBDataflow& dataflow = bbDataflows[basicBlock];
			
			for (LLVMValueRef inst = LLVMGetFirstInstruction(basicBlock); inst;
  					inst = LLVMGetNextInstruction(inst)) {

				if (LLVMIsAStoreInst(inst)) {
					LLVMValueRef storeAddr = LLVMGetOperand(inst, 1); // Store

					// Check if this store kills any previous stores in the gen set, if so, remove them from gen set
					for (auto it = dataflow.genSet.begin(); it != dataflow.genSet.end(); ) {
						LLVMValueRef genStore = *it;
						LLVMValueRef genStoreAddr = LLVMGetOperand(genStore, 1);
						if (operandsEqual(storeAddr, genStoreAddr)) {
							it = dataflow.genSet.erase(it);  // erase returns iterator to next element
						} else {
							++it;  // only increment if we didn't erase
						}
					}

					// Check if this store kills any previous stores to the same address
					for (LLVMValueRef prevStore : allStores) {
						LLVMValueRef prevStoreAddr = LLVMGetOperand(prevStore, 1);
						if (prevStore != inst && operandsEqual(storeAddr, prevStoreAddr)) {
							dataflow.killSet.insert(prevStore);
						}
					}

					dataflow.genSet.insert(inst);
					allStores.insert(inst);
				}
			}
		}

		// Iteratively compute IN and OUT sets until convergence
		bool setsChanged = true;
		while (setsChanged) {
			setsChanged = false;

			for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {

				BBDataflow& dataflow = bbDataflows[basicBlock];

				unordered_set<LLVMValueRef> oldOutSet = dataflow.outSet;

				// OUT[B] = GEN[B] U (IN[B] - KILL[B])
				unordered_set<LLVMValueRef> newOutSet = dataflow.genSet;
				for (LLVMValueRef inStore : dataflow.inSet) {
					if (dataflow.killSet.find(inStore) == dataflow.killSet.end()) {
						newOutSet.insert(inStore);
					}
				}
				dataflow.outSet = newOutSet;

				if (dataflow.outSet != oldOutSet) {
					setsChanged = true;

					// Push change to successors: IN[S] = U OUT[P] for all predecessors P of successor S
					LLVMValueRef terminator = LLVMGetBasicBlockTerminator(basicBlock);
					unsigned numSuccessors = LLVMGetNumSuccessors(terminator);
					for (unsigned i = 0; i < numSuccessors; i++) {
						LLVMBasicBlockRef succ = LLVMGetSuccessor(terminator, i);

						BBDataflow& succDataflow = bbDataflows[succ];
						succDataflow.inSet.insert(dataflow.outSet.begin(), dataflow.outSet.end());
					}
				}
			}
		}

		// Replace loads with constants
		for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {

			BBDataflow& dataflow = bbDataflows[basicBlock];
			unordered_set<LLVMValueRef> R = dataflow.inSet;

			list<LLVMValueRef> toDelete; // List of instructions to delete after iteration

			for (LLVMValueRef inst = LLVMGetFirstInstruction(basicBlock); inst;
  					inst = LLVMGetNextInstruction(inst)) {

				if (LLVMIsAStoreInst(inst)) {
					LLVMValueRef storeAddr = LLVMGetOperand(inst, 1); // Store

					// Remove any store to the same address from R
					for (auto it = R.begin(); it != R.end(); ) {
						LLVMValueRef store = *it;
						LLVMValueRef addr = LLVMGetOperand(store, 1);
						if (operandsEqual(storeAddr, addr)) {
							it = R.erase(it);  // erase returns iterator to next element
						} else {
							++it;  // only increment if we didn't erase
						}
					}

					// Add it to R
					R.insert(inst);
				} else if (LLVMIsALoadInst(inst)) {
					LLVMValueRef loadAddr = LLVMGetOperand(inst, 0); // Load

					// Find all stores in R that store to the same address
					vector<LLVMValueRef> matchingStores;
					for (LLVMValueRef store : R) {
						LLVMValueRef storeAddr = LLVMGetOperand(store, 1);
						if (operandsEqual(loadAddr, storeAddr)) {
							matchingStores.push_back(store);
						}
					}

					if (matchingStores.size() > 0) {
						// check that all stores are a constant
						bool allConstant = true;
						vector<LLVMValueRef> constValues;
						for (LLVMValueRef store : matchingStores) {
							LLVMValueRef value = LLVMGetOperand(store, 0);
							if (LLVMIsAConstantInt(value)) {
								constValues.push_back(value);
							} else {
								allConstant = false;
								break;
							}
						}

						// If all matching stores are constants and have the same value, replace load with that constant
						if (allConstant) {
							bool allSameValue = true;
							LLVMValueRef firstValue = constValues[0]; // Guaranteed to exist since matchingStores is not empty
							for (LLVMValueRef value : constValues) {
								if (!operandsEqual(value, firstValue)) {
									allSameValue = false;
									break;
								}
							}

							if (allSameValue) {
								changed = true;
								toDelete.push_back(inst); // Mark instruction for deletion
								LLVMReplaceAllUsesWith(inst, firstValue);
								if (DEBUGGING) {
									printf("Propagated constant value:\n");
									LLVMDumpValue(firstValue);
									printf("\n into:\n");
									LLVMDumpValue(inst);
									printf("\n");
								}
							}
						}
					}
				}
			}

			// Delete instructions
			for (LLVMValueRef inst : toDelete) {
				LLVMInstructionEraseFromParent(inst);
			}
			if (DEBUGGING && !toDelete.empty()) {
				printf("New Basic Block after constant propagation:\n");
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
		// Loop until no more changes
		int changed = 1;
		while (changed) {
			changed = 0;
			printf("Starting optimization iteration...\n");
			int subexprChanged = subexprElimination(m);
			printf("Subexpression elimination made changes: %s\n", subexprChanged ? "Yes" : "No");
			int deadcodeChanged = deadcodeElimination(m);
			printf("Dead code elimination made changes: %s\n", deadcodeChanged ? "Yes" : "No");
			int constantFoldingChanged = 1;
			int constantPropagationChanged = 1;
			while (constantFoldingChanged || constantPropagationChanged) {
				constantFoldingChanged = constantFolding(m);
				printf("Constant folding made changes: %s\n", constantFoldingChanged ? "Yes" : "No");
				constantPropagationChanged = constantPropagation(m);
				printf("Constant propagation made changes: %s\n", constantPropagationChanged ? "Yes" : "No");
				if (constantFoldingChanged || constantPropagationChanged) {
					changed = 1; // If either made changes, we need to check again for more opportunities
				}
			}
			changed = changed || subexprChanged || deadcodeChanged;
		}
		LLVMDumpModule(m);
    }

	return 0;
}