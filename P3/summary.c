/*
 * File: summary.c
 *
 * Description:
 *   This is where you implement your project 3 support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <bool.h>
/* LLVM Header Files */
#include "llvm-c/Core.h"
#include "dominance.h"

/* Header file global to this project */
#include "summary.h"
//#include "uthash.h"
/*---------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------*/
typedef struct Stats_def {
  int functions;
  int globals;
  int bbs;
  int insns;
  int insns_bb_deps;
  int insns_g_deps;
  int branches;
  int loads;
  int stores;
  int calls;
  int loops; //approximated by backedges
} Stats;

void pretty_print_stats(FILE *f, Stats s, int spaces)
{
  char spc[128];
  int i;

  // insert spaces before each line
  for(i=0; i<spaces; i++)
    spc[i] = ' ';
  spc[i] = '\0';
    
  fprintf(f,"%sFunctions.......................%d\n",spc,s.functions);
  fprintf(f,"%sGlobal Vars.....................%d\n",spc,s.globals);
  fprintf(f,"%sBasic Blocks....................%d\n",spc,s.bbs);
  fprintf(f,"%sInstructions....................%d\n",spc,s.insns);
  fprintf(f,"%sInstructions (bb deps)..........%d\n",spc,s.insns_bb_deps);
  fprintf(f,"%sInstructions (g/c deps).........%d\n",spc,s.insns_g_deps);
  fprintf(f,"%sInstructions - Branches.........%d\n",spc,s.branches);
  fprintf(f,"%sInstructions - Loads............%d\n",spc,s.loads);
  fprintf(f,"%sInstructions - Stores...........%d\n",spc,s.stores);
  fprintf(f,"%sInstructions - Calls............%d\n",spc,s.calls);
  fprintf(f,"%sInstructions - Other............%d\n",spc,
	  s.insns-s.branches-s.loads-s.stores);
  fprintf(f,"%sLoops...........................%d\n",spc,s.loops);
}

void print_csv_file(const char *filename, Stats s, const char *id)
{
  FILE *f = fopen(filename,"w");
  fprintf(f,"id,%s\n",id);
  fprintf(f,"functions,%d\n",s.functions);
  fprintf(f,"globals,%d\n",s.globals);
  fprintf(f,"bbs,%d\n",s.bbs);
  fprintf(f,"insns,%d\n",s.insns);
  fprintf(f,"insns_bb_deps,%d\n",s.insns_bb_deps);
  fprintf(f,"insns_g_deps,%d\n",s.insns_g_deps);
  fprintf(f,"branches,%d\n",s.branches);
  fprintf(f,"loads,%d\n",s.loads);
  fprintf(f,"stores,%d\n",s.stores);
  fprintf(f,"calls,%d\n",s.calls);
  fprintf(f,"loops,%d\n",s.loops);
  fclose(f);
}

Stats MyStats;

void
Summarize(LLVMModuleRef Module, const char *id, const char* filename)
{
	LLVMValueRef  fn_iter; // iterator
	int i, operand; 
	char func_define,  global_dependent, bb_dependent;
	LLVMValueRef global_itr;
	/*------------------------------------------MyStats.globals--------------------------------------------------*/
	for(global_itr = LLVMGetFirstGlobal(Module);global_itr!= NULL ;global_itr = LLVMGetNextGlobal(global_itr))
	{
		if(NULL != LLVMGetInitializer(global_itr))
			MyStats.globals++;
	}
	/*------------------------------------------------Iteration through each instruction-------------------------*/
	for(fn_iter = LLVMGetFirstFunction(Module); fn_iter!=NULL; fn_iter = LLVMGetNextFunction(fn_iter))
	{
        func_define = 0;
	    // fn_iter points to a function
    	LLVMBasicBlockRef bb_iter; /* points to each basic block one at a time */
   		for(bb_iter = LLVMGetFirstBasicBlock(fn_iter);bb_iter != NULL; bb_iter = LLVMGetNextBasicBlock(bb_iter))
     	{
     		//if(LLVMIsABasicBlock(ref))
     		MyStats.bbs++;
        	LLVMValueRef inst_iter; /* points to each instruction */
        	for(inst_iter = LLVMGetFirstInstruction(bb_iter);inst_iter != NULL;inst_iter = LLVMGetNextInstruction(inst_iter)) 
        	{
              	 // get the basic block of this instruction
            	 LLVMBasicBlockRef ref = LLVMGetInstructionParent(inst_iter);
	       		
	       		 /*----------------------------MyStats.insns--------------------------------------------------*/
        		 MyStats.insns++;
        		 /*----------------------------MyStats.functions--------------------------------------------------*/
		    	 if(!func_define)
		    	 {
		    		MyStats.functions++;
		    		func_define = 1;	
		    	 }
    	         /*----------------------------MyStats.calls--------------------------------------------------*/
            	 if(LLVMIsACallInst(inst_iter))
            	 	MyStats.calls++;
    	         /*----------------------------MyStats.loads--------------------------------------------------*/
            	 if(LLVMGetInstructionOpcode(inst_iter) == LLVMLoad)
            	 	MyStats.loads++;
           	 	 /*----------------------------MyStats.stores--------------------------------------------------*/
            	 if(LLVMGetInstructionOpcode(inst_iter) == LLVMStore)
            	 	MyStats.stores++;
             	 /*----------------------------MyStats.loops--------------------------------------------------*/
             	 if(LLVMGetInstructionOpcode(inst_iter) == LLVMBr)
            	 {
            	 	if((LLVMGetNumOperands(inst_iter)>=3))
	            	{
	            	 	MyStats.branches++;
	            	 	if(LLVMDominates(fn_iter, (LLVMBasicBlockRef)LLVMGetOperand(inst_iter,1), ref))
		        	 	{
		        	 		// printf("1-->%s\n",(char *)LLVMGetOperand(inst_iter,1));
		        	 		//if(NULL == get_TmpVal((char *)LLVMGetOperand(inst_iter,1)))
        	 		        {
        	 		            MyStats.loops++;
        	 		           
        	 		           // add_tmp((char *)LLVMGetOperand(inst_iter,1), (LLVMBasicBlockRef)LLVMGetOperand(inst_iter,1)); 
        	 		        }
		        	 	}
		        	 	if(LLVMDominates(fn_iter, (LLVMBasicBlockRef)LLVMGetOperand(inst_iter,2), ref))
		        	 	{
		        	 		//MyStats.loops++;
		        	 		// printf("2-->%s\n",(char *)LLVMGetOperand(inst_iter,2));
		        	 		//if(NULL == get_TmpVal((char *)LLVMGetOperand(inst_iter,2)))
        	 		        {
        	 		            MyStats.loops++;
        	 		           // add_tmp((char *)LLVMGetOperand(inst_iter,2), (LLVMBasicBlockRef)LLVMGetOperand(inst_iter,2)); 
        	 		        }
		        	 	}
	            	}
	            	if(LLVMDominates(fn_iter, (LLVMBasicBlockRef)LLVMGetOperand(inst_iter,0), ref))
            	 	{
            	 		//MyStats.loops++;
            	 		 //printf("3-->%s\n",(char *)LLVMGetOperand(inst_iter,0));
            	 		//if(NULL == get_TmpVal((char *)LLVMGetOperand(inst_iter,0)))
        	 		    {
        	 		         MyStats.loops++;
        	 		        // add_tmp((char *)LLVMGetOperand(inst_iter,0), (LLVMBasicBlockRef)LLVMGetOperand(inst_iter,0)); 
        	 		    }
            	 	}
            	 }
           	 	 /*-----------------------------MyStats.insns_g_deps-------------------------------------------*/	 
            	 operand = LLVMGetNumOperands(inst_iter);
            	 global_dependent = 0;   	 
            	 for(i =0;i<operand;i++)
            	 {
            	 	if(LLVMIsAGlobalVariable(LLVMGetOperand(inst_iter,i))||(LLVMIsConstant(LLVMGetOperand(inst_iter,i))))
            	 	{
            	 		global_dependent = 1;
            	 	}
            	 	else
            	 	{
		        	 	global_dependent = 0;
		        	 	break;	
           	 		}
            	 }
            	 if(global_dependent)
            	 {
	            	 MyStats.insns_g_deps++;
            	 }
        		 /*-----------------------------MyStats.insns_bb_deps-----------------------------------------*/
            	 bb_dependent = 0;
            	 for(i =0;i<operand;i++)
            	 {
            	 	if(LLVMIsAInstruction(LLVMGetOperand(inst_iter,i)))
            	 	{
            	 		if(ref == LLVMGetInstructionParent(LLVMGetOperand(inst_iter,i)))
            	 			bb_dependent = 1;
            	 			//break;
            	 	}
            	 }
            	 if(bb_dependent)
            	 	MyStats.insns_bb_deps++;
            	 /*-----------------------------------End of Stats--------------------------------------------------------*/
        	}
     	}
	}
	//printf("No of loads: %d\nNo of Stores:%d\nNo of bbs: %d",MyStats.loads,MyStats.stores,MyStats.bbs);
	print_csv_file(filename, MyStats, id);
}

