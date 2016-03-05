/*
 * File: LICM_C.c
 *
 * Description:
 *   Stub for LICM in C. This is where you implement your LICM pass.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"
#include "dominance.h"

/* Header file global to this project */
#include "cfg.h"
#include "loop.h"
#include "worklist.h"
#include "valmap.h"

static worklist_t list;
valmap_t stores_map, calls_map;
static LLVMBuilderRef Builder=NULL;

unsigned int LICM_Count=0;
unsigned int LICM_NoPreheader=0;
unsigned int LICM_AfterLoop=0;
unsigned int LICM_Load=0;
unsigned int LICM_BadCall=0;
unsigned int LICM_BadStore=0;

static int canMoveOutOfLoop(LLVMLoopRef Loop, LLVMValueRef I, LLVMValueRef addr, LLVMValueRef F)
{
	//printf("entering canMoveOutOfLoop\n");
	int flag = 0, flag2=0;
	LLVMValueRef inst,  flag_inst;
	worklist_t BBs_in_loop1, exitBlocks;
	LLVMBasicBlockRef top_bb,bb;
	BBs_in_loop1 =  LLVMGetBlocksInLoop(Loop);
	// = valmap_create();	
	//LLVMBasicBlockRef top_bb;	

	if (LLVMIsAInstruction(addr) && LLVMLoopContainsInst(Loop, addr))
    	return 0;
	
	flag = 0;
	while(!worklist_empty(BBs_in_loop1))
	{
		top_bb = LLVMValueAsBasicBlock(worklist_pop(BBs_in_loop1));		
		inst = LLVMGetFirstInstruction(top_bb);	
		while(NULL!=inst)
		//for(inst = LLVMGetFirstInstruction(top_bb); inst != NULL; inst = LLVMGetNextInstruction(inst))
		{
			 if (LLVMIsACallInst(inst))		
		  	 {
				  if (!valmap_check(calls_map,LLVMGetOperand(inst,0)))
				  {
					LICM_BadCall++;
					valmap_insert(calls_map,LLVMGetOperand(inst,0),LLVMGetOperand(inst,0));
				  }
				  return 0;
		    }

			if( LLVMIsAStoreInst(inst))
			{
				flag_inst = LLVMGetOperand(inst,1);		
				flag = 1;	
				 if (LLVMIsAConstant(addr) || LLVMIsAAllocaInst(addr)) 
				 {
				 	if((addr == flag_inst)||((!LLVMIsAConstant(flag_inst)) && (!LLVMIsAAllocaInst(flag_inst))))
				 	{
				 		if(!valmap_check(stores_map,flag_inst))
						{
							valmap_insert(stores_map, flag_inst, LLVMGetOperand(inst,0));
							LICM_BadStore++;
							//return 0;
						}
						flag2 =1;
						return 0;
				 	}
				 }
			 }
		    inst = LLVMGetNextInstruction(inst);
		 }
	}
	if(!flag)
	{
		worklist_t exitBlocks = LLVMGetExitBlocks(Loop);
		while (!worklist_empty(exitBlocks))
		{
		  bb = (LLVMBasicBlockRef) worklist_pop(exitBlocks);
		  if (!LLVMDominates(F, LLVMGetInstructionParent(I), bb))	
			return 0;
		}
		return 1;
	}
	
	if(!flag2)
		return 1;
}	


static void performLICM(worklist_t  BBs_in_loops, LLVMLoopRef Loop,LLVMValueRef F)
{
	LLVMValueRef inst, addr, temp,rm, clone; 
	LLVMBasicBlockRef top_bb = LLVMValueAsBasicBlock(worklist_pop(BBs_in_loops));
	LLVMBasicBlockRef PH = LLVMGetPreheader(Loop);
	//printf("entering performLICM\n");
	if(NULL == PH)
	{	
		LICM_NoPreheader++;
		return;
	}
	
	while(!worklist_empty(BBs_in_loops))
	{
		top_bb = LLVMValueAsBasicBlock(worklist_pop(BBs_in_loops));
		inst = LLVMGetFirstInstruction(top_bb);
		
		//for(inst = LLVMGetFirstInstruction(top_bb); inst != NULL; inst = LLVMGetNextInstruction(inst))
		while(NULL != inst)
		{
			temp = inst;
			if((LLVMMakeLoopInvariant(Loop,inst)))//&& (!LLVMIsALoadInst(inst) || !LLVMIsAStoreInst(inst)))
			{
				LICM_Count++;
				/*if(LLVMIsACallInst(inst))
				{
					LICM_BadCall++;
					printf("wrong assumption about call\n");
					continue;
				}*/
				//how to place instruction in preheader
				//LLVMPositionBuilderBefore(Builder,LLVMGetLastInstruction(PH));
				//clone = LLVMCloneInstruction(inst);
				//place clone at the end of PH before branch
				//LLVMInsertIntoBuilder(Builder,clone);
				//LLVMReplaceAllUsesWith(inst,clone);
				//rm = inst ;
				inst = LLVMGetNextInstruction(temp);
				//inst = LLVMGetNextInstruction(inst);//
				//printf("instruction removed 1\n");
				//printf("removed:%s\n",LLVMGetValueName(rm));
				//LLVMInstructionEraseFromParent(rm);
				//printf("instruction removed 1\n");
				continue;
			}
			else if(LLVMIsALoadInst(inst)&&(!LLVMGetVolatile(inst)))
				{
					addr = LLVMGetOperand(inst,0);
					if(canMoveOutOfLoop(Loop,inst,addr,F))
					{
						LICM_Load++;
						LICM_Count++;
						LLVMPositionBuilderBefore(Builder,LLVMGetLastInstruction(PH));
						clone = LLVMCloneInstruction(inst);
						//place clone at the end of PH before branch
						LLVMInsertIntoBuilder(Builder,clone);
						LLVMReplaceAllUsesWith(inst,clone);
						rm = inst ;
					//	printf("removed:%s\n",LLVMGetValueName(rm));
						//inst = LLVMGetNextInstruction(inst);//
						inst = LLVMGetNextInstruction(inst);
				//		printf("instruction removed 2\n");
						LLVMInstructionEraseFromParent(rm);
						continue;
					}
				}
			inst = LLVMGetNextInstruction(inst);
		}
		//top_bb = LLVMValueAsBasicBlock(worklist_pop(BBs_in_loops));
	}
} 

static int IterateOverLoops(LLVMValueRef F)
{
 //printf("entering IterateOverLoops\n");
  LLVMLoopInfoRef LI = LLVMCreateLoopInfoRef(F);
  LLVMLoopRef Loop;
  worklist_t BBs_in_loops;
  
  for(Loop=LLVMGetFirstLoop(LI);Loop!=NULL; Loop=LLVMGetNextLoop(LI,Loop))
    {
      // Use Loop to get its basic blocks   
      BBs_in_loops =  LLVMGetBlocksInLoop(Loop); 
      performLICM(BBs_in_loops,Loop,F);   
    }
  //printf("exiting IterateOverLoops\n");
  return 0;
}

static void LICMOnFunction(LLVMValueRef funs)
{
	IterateOverLoops(funs);
}


void LoopInvariantCodeMotion_C(LLVMModuleRef Module)
{
 // printf("entering LoopInvariantCodeMotion_C\n");
  LLVMValueRef funs;

  Builder = LLVMCreateBuilder();

  list = worklist_create();
  stores_map = valmap_create();
  calls_map = valmap_create();
  for(funs=LLVMGetFirstFunction(Module);funs!=NULL;funs=LLVMGetNextFunction(funs))    { 
      if (LLVMCountBasicBlocks(funs))
      	{     		
      		LICMOnFunction(funs);
      	}
    }

  LLVMDisposeBuilder(Builder);
  Builder = NULL;

  fprintf(stderr,"LICM_Count      =%d\n",LICM_Count);
  fprintf(stderr,"LICM_NoPreheader=%d\n",LICM_NoPreheader);
  fprintf(stderr,"LICM_Load       =%d\n",LICM_Load);
  fprintf(stderr,"LICM_BadCall    =%d\n",LICM_BadCall);
  fprintf(stderr,"LICM_BadStore   =%d\n",LICM_BadStore);
 // printf("exiting LoopInvariantCodeMotion_C\n");
}
