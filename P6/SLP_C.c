/*
 * File: SLP_C.c
 *
 * Description:
 *   Stub for SLP in C. This is where you implement your SLP pass.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"
//#include "Type.h"
#include "dominance.h"

/* Header file global to this project */
#include "cfg.h"
#include "loop.h"
#include "worklist.h"
#include "valmap.h"


// dom:  does a dom b?
//
//       a and b can be instructions. This is different
//       from the LLVMDominates API which requires basic
//       blocks.
int sizeOneCount,sizeTwoCount,sizeThreeCount,sizeFourCount,sizeGtFiveCount,storeChain,loadChain;

static int dom(LLVMValueRef a, LLVMValueRef b)
{
  if (LLVMGetInstructionParent(a)!=LLVMGetInstructionParent(b)) {
    LLVMValueRef fun = LLVMGetBasicBlockParent(LLVMGetInstructionParent(a));
    // a dom b?
    return LLVMDominates(fun,LLVMGetInstructionParent(a),
			 LLVMGetInstructionParent(b));
  }

  // a and b must be in same block
  // which one comes first?
  LLVMValueRef t = a;
  while(t!=NULL) {
    if (t==b)
      return 1;
    t = LLVMGetNextInstruction(t);
  }
  return 0;
}



static LLVMBuilderRef Builder;

typedef struct VectorPairDef {
  LLVMValueRef pair[2];
  int insertAt0;
  struct VectorPairDef *next;
  struct VectorPairDef *prev;
} VectorPair;

typedef struct  {
  VectorPair *head;
  VectorPair *tail;
  valmap_t    visited;
  valmap_t    sliceA;
  int size;  
  int score;
} VectorList;

static VectorList* VectorList_Create() {
  VectorList *new = (VectorList*) malloc(sizeof(VectorList));
  new->head = NULL;
  new->tail = NULL;
  new->visited = valmap_create();
  new->sliceA = valmap_create();
  new->size=0;
  return new;
}

static void VectorList_Destroy(VectorList *list)
{
  valmap_destroy(list->visited);
  valmap_destroy(list->sliceA);
  VectorPair *head = list->head;
  VectorPair *tmp;
  while(head) {
    tmp = head;
    head=head->next;
    free(tmp);    
  }
  free(list);
}
valmap_t globalMap;

VectorList* globalList;// = VectorList_Create();
VectorList* List;// = VectorList_Create(); 



//
// add a and b to the current chain of vectorizable instructions
//
static VectorPair *VectorList_AppendPair(VectorList *list, LLVMValueRef a, LLVMValueRef b)
{
  VectorPair *new = (VectorPair*) malloc(sizeof(VectorPair));
  new->pair[0] = a;
  new->pair[1] = b;
  new->insertAt0 = 1;
  valmap_insert(list->visited,a,(void*)1);
  valmap_insert(list->visited,b,(void*)1);
  new->next = NULL;
  new->prev = NULL;
  // insert at head
  if (list->head==NULL) {
    list->head = new;
    list->tail = new;
  } else {
    // find right place to insert
    VectorPair *temp = list->head;
    VectorPair *prev = NULL;

    while(temp && dom(temp->pair[0],a)) {
      prev=temp;
      temp=temp->next;   
    }
    if (prev) {
      new->next = temp;
      new->prev = prev;
      prev->next = new;
      if (temp) // if not at end
	temp->prev = new;
      else
	list->tail = new;
    } else {
      list->head->prev = new;
      new->next = list->head;
      list->head = new;
    }
  }  
  list->size++;
  return new;
}

// AssembleVector: Helper function for generating vector code.
//               It figures out how to assemble a vector from two inputs under
//               the assumption that the inputs are either constants or registers.
//               If constants, it builds a constant vector.  If registers,
//               it emits the insertelement instructions.
//  
//               This is only helpful for generating vector code, not for detecting
//               vectorization opportunities

static LLVMValueRef AssembleVector(LLVMValueRef a, LLVMValueRef b)
{
  LLVMTypeRef type = LLVMTypeOf(a);
  LLVMValueRef ret;

  // if they are constants...
  if (LLVMIsAConstant(a) && LLVMIsAConstant(b)) {
    // Build constant vector
    LLVMValueRef vec[2] = {a,b};
    ret = LLVMConstVector(vec,2);        
  }  else {
    // Otherwise, one of them is a register

    LLVMTypeRef vtype = LLVMVectorType(type,2);
    LLVMValueRef c = LLVMConstNull(vtype);
    
    // Insert a into null vector
    ret = LLVMBuildInsertElement(Builder,c,a,
				 LLVMConstInt(LLVMInt32Type(),0,0),"v.ie");

    // Insert b into ret
    ret = LLVMBuildInsertElement(Builder,ret,b,
				 LLVMConstInt(LLVMInt32Type(),1,0),"v.ie");    
  }

  // Return new vector as input for a new vector instruction
  return ret;
}

static int AdjacentLoadsStores(LLVMValueRef I, LLVMValueRef J, int loadStore)//loadStore = 0 for load and 1 for store
{
//	printf("AdjacentLoadsStores--%d\n",loadStore);
	LLVMValueRef gep1,gep2;
	int last_operand,i;
	
//	printf("1");
	gep1 = LLVMGetOperand(I,loadStore);
	gep2 = LLVMGetOperand(J,loadStore);
//	printf("x");
	if(!LLVMIsAInstruction(gep1) || !LLVMIsAInstruction(gep2))
		 {				return 0;}
//	printf("y");
	last_operand = (LLVMGetNumOperands(gep1)-1);
//	printf("z");
	if(!LLVMIsAGetElementPtrInst(gep1) || !LLVMIsAGetElementPtrInst(gep2) )
	{
		/*if(!LLVMIsAGetElementPtrInst(gep1) && !LLVMIsAGetElementPtrInst(gep2))
		{
			//if(!LLVMIsAGetElementPtrInst(gep1) && !LLVMIsAGetElementPtrInst(gep2))
			if(LLVMIsAStoreInst())
			AdjacentLoadsStores()
		}
		else*/
			return 0;
	}
//	printf("2");
	for(i = 0;i<(last_operand);i++)
	{
		if(LLVMGetOperand(gep1,i) != LLVMGetOperand(gep2,i))
			return 0;
	}
	if((LLVMGetOperand(gep1,0) == LLVMGetOperand(gep2,0)))// && (LLVMGetOperand(gep1,1) == LLVMGetOperand(gep2,1)))
	{
//		printf("3");
		if((LLVMIsAConstant(LLVMGetOperand(gep1,last_operand))) && (LLVMIsAConstant(LLVMGetOperand(gep2,(LLVMGetNumOperands(gep2)-1)))))
		{
//			printf("4");
			if((LLVMConstIntGetZExtValue(LLVMGetOperand(gep2,last_operand))-LLVMConstIntGetZExtValue(LLVMGetOperand(gep1,last_operand)) == 1) || (LLVMConstIntGetZExtValue(LLVMGetOperand(gep1,last_operand))-LLVMConstIntGetZExtValue(LLVMGetOperand(gep2,last_operand)) == 1))
			{
//				printf("\n----TRUE----");
//				LLVMDumpValue(I);
//				LLVMDumpValue(J);
				return 1;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}
}


static int shouldVectorize(LLVMValueRef I, LLVMValueRef J)
{
//	printf("shouldVectorize\n");
	//if(!LLVMIsAInstruction(I)||!LLVMIsAInstruction(J))
		//return 0;
	if(LLVMIsAStoreInst(I))
	{
		if( (LLVMGetTypeKind(LLVMTypeOf(I)) != LLVMVoidTypeKind) &&
			(LLVMGetTypeKind(LLVMTypeOf(I)) != LLVMIntegerTypeKind) &&
			(LLVMGetTypeKind(LLVMTypeOf(I)) != LLVMFloatTypeKind) &&
			(LLVMGetTypeKind(LLVMTypeOf(I)) != LLVMPointerTypeKind))
			return 0;
	}
	else	
	{	if(!((LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMIntegerTypeKind)||
			 (LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMFloatTypeKind)||
			 (LLVMGetTypeKind(LLVMTypeOf(I)) == LLVMPointerTypeKind)))//,float,or pointer kind)
			return 0;
	}
	if(LLVMGetInstructionParent(I) != LLVMGetInstructionParent(J))
		return 0;
	if(LLVMIsATerminatorInst(I))
		return 0;
	if((LLVMIsALoadInst(I) || LLVMIsAStoreInst(I)) && (LLVMGetVolatile(I)))
		return 0;
	if(  LLVMIsAPHINode(I) ||
    	LLVMIsACallInst(I)||
		LLVMIsAFCmpInst(I)||
		LLVMIsAICmpInst(I)||
		LLVMIsAExtractValueInst(I)||
		LLVMIsAInsertElementInst(I)||
		LLVMIsAInsertValueInst(I) ||
		 LLVMIsAExtractElementInst(I)||
		LLVMIsAAddrSpaceCastInst(I)
		
		/*( LLVMGetInstructionOpcode(I) <= 57 && LLVMGetInstructionOpcode(I) >= 55 )*///fence an atomic type instruction
		)
		return 0;
	if((LLVMIsALoadInst(I)) && (LLVMIsALoadInst(J)))
	{
		if(!AdjacentLoadsStores(I,J,0))//0 for load
		{
			return 0;
		}
	}
	if ((LLVMGetInstructionOpcode(I) == LLVMAtomicCmpXchg)
			|| LLVMGetInstructionOpcode(I) == LLVMAtomicRMW
			|| LLVMGetInstructionOpcode(I) == LLVMFence)
		return 0;

	if((LLVMIsAStoreInst(I)) && (LLVMIsAStoreInst(J)))
	{
		if(!AdjacentLoadsStores(I,J,1))//1 for stores
		{
			return 0;
		}
	}
/*	if (LLVMIsAShuffleVectorInst(I))
		return 0;
	if (LLVMIsALandingPadInst(I))
		return 0;*/
	if(LLVMIsAGetElementPtrInst(I) || LLVMIsAGetElementPtrInst(J))
	{
		//if(LLVMIsAStoreInst(I)||LLVMIsAStoreInst(J))
			return 0;
	}
	if(I==J)
		return 0;
//	printf("should vectorize TRUE\n");	
	return 1;
}

static int Isomorphic(LLVMValueRef I, LLVMValueRef J)
{	
//	printf("Isomorphic\n");
	int i;
	if(!LLVMIsAInstruction(I) && !LLVMIsAInstruction(J))
		 return 0;
	if(LLVMGetInstructionOpcode(I) != LLVMGetInstructionOpcode(J))//NOT same opcode
	{
		return 0;
	}
//	printf("Iso1\n");
	if(LLVMTypeOf(I) != LLVMTypeOf(J))//same type
	{
		return 0;
	}
//	printf("Iso2\n");
	if(LLVMGetNumOperands(I) != LLVMGetNumOperands(J))//same number of opearand	
	{
		return 0;
	}
//	printf("5");
	for(i = 0; i<LLVMGetNumOperands(I);i++)
	{
		if(LLVMTypeOf(LLVMGetOperand(I,i)) != LLVMTypeOf(LLVMGetOperand(J,i)))
		{
			return 0;
		}
	}
//	printf("TRUE\n");
	return 1;
}


static VectorList* collectIsomorphicInstructions(VectorList *L, LLVMValueRef I, LLVMValueRef J)
{
//	printf("collectIsomorphicInstructions\n");
	int i;
	if(!shouldVectorize(I,J))
	{
//		printf("not able to vectorize...\n");
		return L;
	}
	if(valmap_check(L->visited,I) || valmap_check(L->visited,J))
	{
//		printf("--already in visited list--\n");
		return L;
	}
	VectorList_AppendPair(L,I,J);
	
//	printf("6");
	for(i = 0;i< LLVMGetNumOperands(I);i++)
	{
		if(Isomorphic(LLVMGetOperand(I,i),LLVMGetOperand(J,i)))
		{	
//			printf("7");
			collectIsomorphicInstructions(L, LLVMGetOperand(I,i), LLVMGetOperand(J,i));
		}
	}
	
	return L;
}

static void updateStats(VectorList* L,int whichChain)//implement this
{
//	printf("updateStats\n");
	if(L->size == 1)
	{
		sizeOneCount++;
	}
	else if(L->size == 2)
	{
		sizeTwoCount++;
	}
	else if(L->size == 3)
	{
		sizeThreeCount++;
	}
	else if(L->size == 4)
	{
		sizeFourCount++;
	}
	else if(L->size >= 5)
	{
		sizeGtFiveCount++;
	}
	if(whichChain == 0)//load
	{
		loadChain++;
	}
	else
	{
		storeChain++;
	}
}
static int isAlreadyVectorized( LLVMValueRef I)
{
//	printf("isAlreadyVectorized\n");
	if(valmap_check(globalMap, I))
	{
//		printf("TRUE\n");
		return 1;
	}
	else
		return 0;
}

valmap_t vmap;
/*
static void vectorize(VectorList* L)
{
	vmap = valmap_create();
	LLVMValueRef I, J;
	int i;
	
	while(L->head != NULL)
	{
		LLVMValueRef ops[] = {};
		if(dom(L->head.pair[0],L->head.pair[1]))
		{
			I = L->head.pair[0];
			J = L->head.pair[1];
		}
		else
		{
			J = L->head.pair[0];
			I = L->head.pair[1];
		}
		for(i = 0; i<LLVMGetNumOperand(I); i++)
		{
			if(!valmap_check(vmap,LLVMGetOperand(I,i)))
			{
				ops[i] = AssembleVector(LLVMGetOperand(I,i),LLVMGetOperand(J,i));
				valmap_insert(vmap,LLVMGetOPerand(I,i),ops[i]);
				valmap_insert(vmap,LLVMGetOPerand(J,i),ops[i]);
			}
			else
			{
				ops[i] = valmap_find(vmap,LLVMGetOperand(I,i));
			}
		}
		LLVMPositionBuilderBefore(Builder,J);
		
	}
	
	
}*/
static void copyToGlobal(VectorList *List)
{
//	printf("\ncopyToGlobal");
			int i=0;
			VectorPair* head = List->head;
	while(head)
	{

		i++;
//		printf("%d\n",i);
		if(!valmap_check(globalMap,head->pair[0])) 
		{
//			printf("\n-*-");
//			LLVMDumpValue(head->pair[0]);
			valmap_insert(globalMap, head->pair[0], head->pair[0]);
		}
		
		if(!valmap_check(globalMap,head->pair[1])) 
		{
//			printf("\n-*-");
//			LLVMDumpValue(head->pair[1]);
			valmap_insert(globalMap, head->pair[1], head->pair[1]);
		}
			head = head->next;
	}
//	printf("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
}
static void VectorList_print(VectorList *List)
{
	int i=0;
	VectorPair* head = List->head;
	if(head)
	{
//		printf("----%s---\n",LLVMGetValueName((LLVMValueRef)LLVMGetInstructionParent(head->pair[0])));
	}
	while(head)
	{
		i++;
		printf("%d:\n",i);
		LLVMDumpValue(head->pair[0]);
		LLVMDumpValue(head->pair[1]);
		printf("\n");
/*		//printf("\n%d = %s <---> %s\n",i,LLVMGetValueName(head->pair[0]),LLVMGetValueName(head->pair[1]));
		printf("**%d**\n",i);
//		LLVMDumpValue(head->pair[0]);
		if(LLVMIsAStoreInst(head->pair[0]))
		{
//			printf("%s\n",LLVMGetValueName(LLVMGetOperand(head->pair[0],1)));
		}
		else if(LLVMIsALoadInst(head->pair[0]))
		{
//			printf("%s\n",LLVMGetValueName(LLVMGetOperand(head->pair[0],0)));
		}
		else
		{
//			printf("%s\n",LLVMGetValueName(head->pair[0]));
		}
//		LLVMDumpValue(head->pair[1]);
		if(LLVMIsAStoreInst(head->pair[1]))
		{
//			printf("%s\n",LLVMGetValueName(LLVMGetOperand(head->pair[1],1)));
		}
		else if(LLVMIsALoadInst(head->pair[1]))
		{
//			printf("%s\n",LLVMGetValueName(LLVMGetOperand(head->pair[1],0)));
		}
		else
		{
//			printf("%s\n",LLVMGetValueName(head->pair[1]));
		}*/
		head = head->next;
	}
	printf("---------------\n");
}
static void SLPOnBasicBlock(LLVMBasicBlockRef BB)
{
//	printf("SLPOnBasicBlock\n");
  LLVMValueRef I, J;
  int changed;
  int i=0;
    globalMap = valmap_create();
    for(I=LLVMGetLastInstruction(BB);I!=NULL; I=LLVMGetPreviousInstruction(I))
    {      
//     printf("--%s-I::%s-\n",LLVMGetValueName((LLVMValueRef)LLVMGetInstructionParent(I)),LLVMGetValueName(I));
          //VectorList* List = VectorList_Create(); 
          VectorList* globalList = VectorList_Create(); 
		  if(LLVMIsAStoreInst(I) && !isAlreadyVectorized(I))
		  {
		  	for(J = LLVMGetPreviousInstruction(I);J!= NULL; J=LLVMGetPreviousInstruction(J))
		  //LLVMValueRef J =   LLVMGetPreviousInstruction(I);
		  //while(J != NULL)
		  	{	
//		  		printf("J::%s\n",LLVMGetValueName(J));
		  		if(LLVMIsAStoreInst(J)&& !isAlreadyVectorized(J))
		  		{
		  			if(AdjacentLoadsStores(I,J,1) && Isomorphic(I,J))
		  			{
		  				globalList = collectIsomorphicInstructions(globalList,I,J);
		  				if(globalList->size>= 2)
						{
							//vectorize(&globalList);
						//updateStats(globalList,1);//1 for stores
							//copyToGlobal(globalList);
							break;
						}
		  			}
		  		}
		  		
			//		J = LLVMGetPreviousInstruction(J);
		  		//if(J == LLVMGetFirstInstruction(BB))
		  		//	break;
		  	 }
		  }

		  if(LLVMIsALoadInst(I)  && !isAlreadyVectorized(I))
		  {
		  	for(J = LLVMGetNextInstruction(I); J != NULL; J = LLVMGetNextInstruction(J))
		  	//LLVMValueRef J = LLVMGetNextInstruction(I);
		  	//while(J!=NULL)
		  	{	
//		  		printf("J::%s\n",LLVMGetValueName(J));
		  		if(LLVMIsALoadInst(J) && !isAlreadyVectorized(J))
		  		{
		  			if((AdjacentLoadsStores(I,J,0)) && (Isomorphic(I,J)))
		  			{
//		  				printf("I am here\n");
		  				collectIsomorphicInstructions(globalList,I,J);
		  				if(globalList->size>= 2)
							{
								//vectorize(globalList);
								//updateStats(globalList,0);// 0 for loads
								//copyToGlobal(globalList);
								break;
							}
		  			}
		  		}
//		  		printf("LIST SIZE--%d\n",globalList->size);
		  		//J = LLVMGetNextInstruction(J);
		  	//	if(J == LLVMGetLastInstruction(BB))
		  		//	break;
		  	}
		  }
		  if(globalList->size>=1)
		  {
			 //vectorize(globalList);
			 copyToGlobal(globalList);
			 VectorList_print(globalList);
			 if(LLVMIsALoadInst(I))
				 updateStats(globalList,0);
			 else
			 	 updateStats(globalList,1);
		  }
//		  printf("destroy  list2\n");
          VectorList_Destroy(globalList);
//          printf("destroy  list3\n");
	 }
	 valmap_destroy(globalMap);
}

static void SLPOnFunction(LLVMValueRef F) 
{
  LLVMBasicBlockRef BB;
  for(BB=LLVMGetFirstBasicBlock(F);
      BB!=NULL;
      BB=LLVMGetNextBasicBlock(BB))
    {
      SLPOnBasicBlock(BB);
    }
}

void SLP_C(LLVMModuleRef Module)
{
  LLVMValueRef F;
 
  for(F=LLVMGetFirstFunction(Module); 
      F!=NULL;
      F=LLVMGetNextFunction(F))
    {
      SLPOnFunction(F);
    }
	printf("\nSize\t Count");
	printf("\n1 :\t%0d\n2 :\t%0d\n3 :\t%0d\n4 :\t%0d\n>5 :\t%0d\nLoad-Chain :\t%0d\nStore-Chain :\t%0d\n",sizeOneCount,sizeTwoCount,sizeThreeCount,sizeFourCount,sizeGtFiveCount,loadChain,storeChain);
	
}
