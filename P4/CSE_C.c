/*
 * File: CSE_C.c
 *
 * Description:
 *   This is where you implement the C version of project 4 support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"
#include "dominance.h"
#include "transform.h"

/* Header file global to this project */
#include "cfg.h"
#include "CSE.h"

unsigned long CSE_Basic, CSE_Dead, CSE_Simplify, CSE_RLoad, CSE_RStore,CSE_Store2Load;
unsigned char imInChild, exactOperands;

static
int commonSubexpression(LLVMValueRef I, LLVMValueRef J)
{
	// return 1 if I and J are common subexpressions

	// check these things:
	//   - they have the same opcode
	//   - they have the same type
	//   - they have the same number of operands
	//   - all operands are the same (pointer equivalence) LLVMValueRef
	// if any are false, return 0

	// return 0 is always conservative, so by default return 0
	int i;
	//printf("commonSubexpression\n");
	//printf("I:%s---J:%s\n",LLVMGetValueName(I),LLVMGetValueName(J));
	//if();
	if(LLVMGetInstructionOpcode(I) == LLVMGetInstructionOpcode(J))//same opcode
	{
		if(LLVMTypeOf(I) == LLVMTypeOf(J))//same type
	  	{
			if(LLVMGetNumOperands(I) == LLVMGetNumOperands(J))//same number of opearand	
			{
				exactOperands = 0;
 				if(LLVMIsAICmpInst(I))
				{
					if(LLVMGetICmpPredicate(I) != LLVMGetICmpPredicate(J))
					return 0;
				}
				for(i = 0;i<LLVMGetNumOperands(I);i++)
				{
					//if(LLVMTypeOf(LLVMGetOperand(I,i)) == LLVMTypeOf(LLVMGetOperand(J,i)))
					{
						if(LLVMGetOperand(I,i) != LLVMGetOperand(J,i))
						{
							exactOperands = 1;
							//printf("NOT Same instructions\n");
						}
					}
				}
				if(!exactOperands)
				{
					//printf("I: %s\t\t J: %s\n",LLVMGetValueName(I),LLVMGetValueName(J));
					//printf("Same instructions\n");
					return 1;
				}
				
			}
	  	}
	}
	return 0;
}

static
int canHandle(LLVMValueRef I) 
{
	//if(I==NULL)
	//printf("CANHANDLE NULL INSTRUCTION\n");
	//printf("canHandle\t%s\n",LLVMGetValueName(I));

  return ! 
    ( LLVMIsALoadInst(I) ||
      LLVMIsAStoreInst(I) ||
      LLVMIsATerminatorInst(I) ||
      LLVMIsACallInst(I) ||
      LLVMIsAPHINode(I) ||
      LLVMIsAAllocaInst(I) || 
      LLVMIsAFCmpInst(I) ||
      LLVMIsAVAArgInst(I)||
      /*LLVMIsAICmpInst(I) ||*/
      LLVMIsAExtractValueInst(I)||
      LLVMIsAInsertValueInst(I)/*||
      LLVMIsABranchInst(I)||
      LLVMIsAGetElementPtrInst(I)||
	  LLVMIsASExtInst(I)	*/        	       
    );
}

static
int isDead(LLVMValueRef I)
{	
	//printf("\nInside is dead");
	LLVMOpcode opcode = LLVMGetInstructionOpcode(I);
	if(LLVMGetFirstUse(I) != NULL)
	return 0;
	
	switch(opcode)
	{
	  // when in doubt, keep it! add opcode here to keep:
	  case LLVMRet:
	  case LLVMBr:
	  case LLVMSwitch:
	  case LLVMIndirectBr:
	  case LLVMInvoke:     
	  case LLVMUnreachable:
	  case LLVMFence:
	  case LLVMStore:
	  case LLVMCall:
	  case LLVMAtomicCmpXchg:
	  case LLVMAtomicRMW:
	  case LLVMResume:    
	  case LLVMLandingPad: 
		return 0;

	  case LLVMLoad: if(LLVMGetVolatile(I)) return 0;
	  // all others can be removed
	  default:
		break;
     }

     // All conditions passed
    return 1;
}

// Perform CSE on I for BB and all dominator-tree children
static
void processInst(LLVMBasicBlockRef BB, LLVMValueRef I) 
{
	//printf("processInst\t BB--%s\tI-- %s\n",LLVMGetValueName((LLVMValueRef)BB),LLVMGetValueName(I));
	LLVMBasicBlockRef child;
	LLVMValueRef rm,next_inst;
	// do nothing if trivially dead
	// bale out if not an optimizable instruction
  
	if(!canHandle(I)) return;
	// CSE w.r.t. to I on BB
	if(imInChild)
	{	
		//printf("1a\n");
		next_inst = LLVMGetFirstInstruction(BB);	
	}
	else
	{
		//printf("1b\n");
		next_inst = LLVMGetNextInstruction(I);
	}
	while(next_inst!=NULL)
	{
		if(commonSubexpression(I,next_inst))
		{
			//DO_CSE
			//printf("2.DO_CSE\n");
			LLVMReplaceAllUsesWith(next_inst,I);
			//printf("3.after replace\n");
			rm = next_inst;//inst_iter;
			next_inst  = LLVMGetNextInstruction(next_inst);
			LLVMInstructionEraseFromParent(rm);
			//printf("4.after erase\n");
			CSE_Basic++;
			continue;
			
		}
		//printf("5\n");
		next_inst = LLVMGetNextInstruction(next_inst);		
	}
	//printf("6\n");

	// for each dom-tree child of BB:
	//    processInst(child)
  
	child = LLVMFirstDomChild(BB);
	while(child!=NULL)
	{	
		//printf("7\n");
		//printf("in child\n");
		imInChild = 1;
		processInst(child,I);
		//printf("7b\n");
		child = LLVMNextDomChild(BB,child);
		//printf("8\n");
	}
	//printf("9\n");
	return;  
}

static
void FunctionCSE(LLVMValueRef Function) 
{
  LLVMBasicBlockRef bb_iter;
  LLVMValueRef I,rm, inst_iter,next_inst;
  int isRStore;
  //  for each bb: 
  //  for each isntruction
  //  process instructions
  //  
  //   process memory instructions
  for(bb_iter= LLVMGetFirstBasicBlock(Function);bb_iter!=NULL; bb_iter = LLVMGetNextBasicBlock(bb_iter))
  {
   	inst_iter = LLVMGetFirstInstruction(bb_iter);
   	
  	while(inst_iter!= NULL)
  	{	
  		/*------------------------------------Dead------------------------------------------*/
  		if(isDead(inst_iter))
		{
		  	CSE_Dead++;
		  	//printf("Found Dead\n");
		  	rm = inst_iter;
		  	inst_iter = LLVMGetNextInstruction(inst_iter);
		  	LLVMInstructionEraseFromParent(rm);
			continue;
		}
		/*------------------------------------Simplify--------------------------------------*/
		if(NULL!=InstructionSimplify(inst_iter))
		{
			//printf("Able to simplify\n");
			CSE_Simplify++;
			inst_iter = LLVMGetNextInstruction(inst_iter);
			continue;
		}
		/*-------------------------------------RLoad----------------------------------------*/
		if(LLVMGetInstructionOpcode(inst_iter) == LLVMLoad) 
		{
			next_inst = LLVMGetNextInstruction(inst_iter);
			//isRLoad = 0;
			while(next_inst!=NULL)
			{
				if(LLVMGetInstructionOpcode(next_inst) == LLVMStore)
				{
					if(LLVMGetOperand(next_inst,1) == LLVMGetOperand(inst_iter,0))
					{
						break;
					}
				}
				if((LLVMGetInstructionOpcode(next_inst) == LLVMLoad) && (!LLVMGetVolatile(next_inst)))
				{
					if((LLVMGetOperand(inst_iter,0) == LLVMGetOperand(next_inst,0)) && (LLVMTypeOf(inst_iter) == LLVMTypeOf(next_inst)))
					{
						LLVMReplaceAllUsesWith(next_inst,inst_iter);
						//LLVMInstructionEraseFromParent(next_inst);
						rm = next_inst;//inst_iter;
		  				next_inst  = LLVMGetNextInstruction(next_inst);
		  				LLVMInstructionEraseFromParent(rm);
						CSE_RLoad++;
						continue;
					}
				}
				next_inst = LLVMGetNextInstruction(next_inst);
			}
		 }
		
		/*-----------------------------------------RStore-------------------------------------*/
		isRStore  = 0;
		if(LLVMGetInstructionOpcode(inst_iter) == LLVMStore) //S
		{
			next_inst = LLVMGetNextInstruction(inst_iter);//R
			while(next_inst!=NULL)
			{
			//	isStore2load = 0;
				//printf("inside while for RStore\n");
				if((LLVMGetInstructionOpcode(next_inst) == LLVMLoad) && (!LLVMGetVolatile(next_inst)))
				{
					if((LLVMGetOperand(next_inst,0) == LLVMGetOperand(inst_iter,1))&&(LLVMTypeOf(next_inst)==LLVMTypeOf(LLVMGetOperand(inst_iter,0))))
					{
						LLVMReplaceAllUsesWith(next_inst,LLVMGetOperand(inst_iter,0));
						rm = next_inst;//inst_iter;
						//printf("inside while 1\n");
		  				next_inst  = LLVMGetNextInstruction(next_inst);
		  				LLVMInstructionEraseFromParent(rm);
						//isRLoad = 1;
						CSE_Store2Load++;
						continue;
					}
				}
				if((LLVMGetInstructionOpcode(next_inst) == LLVMStore) && (!LLVMGetVolatile(inst_iter)))
				{
					if(LLVMGetOperand(next_inst,1) == LLVMGetOperand(inst_iter,1))
					{
						if(LLVMTypeOf(LLVMGetOperand(next_inst,0))==LLVMTypeOf(LLVMGetOperand(inst_iter,0)))
						{
							rm = inst_iter;
		  					inst_iter  = LLVMGetNextInstruction(inst_iter);
		  					LLVMInstructionEraseFromParent(rm);
		  					//printf("inside while for 2\n");
		  					CSE_RStore++;
		  					isRStore = 1;
		  					break;							
						}
					}
				}
				if((LLVMGetInstructionOpcode(next_inst) == LLVMStore)||((LLVMGetInstructionOpcode(next_inst) == LLVMLoad)))
					break;
				next_inst  = LLVMGetNextInstruction(next_inst);
			}
		}
		
  		if(isRStore)
	  		continue;
		/*-----------------------------------CSE_Basic----------------------------------------*/
		imInChild = 0;
		//printf("%s\n",LLVMGetValueName((LLVMValueRef)bb_iter));
		//printf("0\n");
		processInst(bb_iter,inst_iter);
		
		/*-----------------------------End of all optimization--------------------------------*/
  		inst_iter = LLVMGetNextInstruction(inst_iter);
 	}
  }
}


void LLVMCommonSubexpressionElimination(LLVMModuleRef Module)
{
  // Loop over all functions
  LLVMValueRef Function;
  for (Function=LLVMGetFirstFunction(Module);Function!=NULL;Function=LLVMGetNextFunction(Function))
  {
      FunctionCSE(Function);
  }

  // print out summary of results
  printf("\nCSE_Basic = %lu\nCSE_Dead = %lu\nCSE_Simplify = %lu\nCSE_RLoad = %lu\nCSE_RStore = %lu\nCSE_Store2Load = %lu\n",CSE_Basic,CSE_Dead,CSE_Simplify,CSE_RLoad,CSE_RStore,CSE_Store2Load);
}

