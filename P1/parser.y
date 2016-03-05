%{
#include <stdio.h>
#include "llvm-c/Core.h"
#include "llvm-c/BitReader.h"
#include "llvm-c/BitWriter.h"
#include <string.h>

#include "uthash.h"

#include <errno.h>
  //#include <search.h>

extern FILE *yyin;
int yylex(void);
int yyerror(const char *);

extern char *fileNameOut;

extern LLVMModuleRef Module;
extern LLVMContextRef Context;

LLVMValueRef Function;
LLVMBasicBlockRef BasicBlock;
LLVMBuilderRef Builder;

//struct hsearch_data params;
//struct hsearch_data tmps;

int params_cnt=0;
LLVMValueRef last_TmpVal = NULL;
LLVMValueRef Result = NULL;
//Result = LLVMConstInt(LLVMInt64Type(),1,0);

struct TmpMap{
  char *key;                	  /* key */
  LLVMValueRef val;               /* data */
  UT_hash_handle hh;         	  /* makes this structure hashable */
};
 

struct TmpMap *map_tmp = NULL;       /* important! initialize to NULL */

void add_tmp(char *tmp, LLVMValueRef val) { 
  struct TmpMap *s; 
  s = malloc(sizeof(struct TmpMap)); 
  s->key = strdup(tmp); 
  s->val = val; 
  HASH_ADD_KEYPTR( hh, map_tmp, s->key, strlen(s->key), s ); 
}

LLVMValueRef get_TmpVal(char *tmp) {
  struct TmpMap *s;
  HASH_FIND_STR( map_tmp, tmp, s );  /* s: output pointer */
  if (s) 
    return s->val;
  else 
    return NULL; // returns NULL if not found
}


struct IdMap{
  char *key;                	  /* key */
  int val;                         /*position*/
  UT_hash_handle hh;         	  /* makes this structure hashable */
};
 

struct IdMap *map_id = NULL;       /* important! initialize to NULL */

void add_Id(char *Id, int val) { 
  struct IdMap *s; 
  s = malloc(sizeof(struct IdMap)); 
  s->key = strdup(Id); 
  s->val = val; 
  HASH_ADD_KEYPTR( hh, map_id, s->key, strlen(s->key), s ); 
}

int get_IdVal(char *Id) {
  struct IdMap *s;
  HASH_FIND_STR( map_id, Id, s );  /* s: output pointer */
  if (s) 
    return s->val;
  else 
    return -1; // returns NULL if not found
}

%}

%union {
  char *tmp;
  int num;
  char *id;
  LLVMValueRef val;
}

%token ASSIGN SEMI COMMA MINUS PLUS VARS MULTIPLY DIVIDE RAISE LESSTHAN /*VARS TMP ID NUM ASSIGN SEMI MINUS 									PLUS MULTIPLY DIVIDE COMMA RAISE LESSTHAN*/
%token <tmp> TMP 
%token <num> NUM 
%token <id> ID
%type <val> expr stmt stmtlist;
%left LESSTHAN
%left PLUS MINUS
%left MULTIPLY DIVIDE RAISE

%start program

%%

program: decl stmtlist 
{ 
  /* 
    IMPLEMENT: return value
  */
  
  LLVMBuildRet(Builder, last_TmpVal);//LLVMConstInt(LLVMInt64TypeInContext(Context),5,(LLVMBool)1)); //change 0 with appropriate value
}
;

decl: VARS varlist SEMI 
{  
  /* Now we know how many parameters we need.  Create a function type
     and add it to the Module */

  LLVMTypeRef Integer = LLVMInt64TypeInContext(Context);

  LLVMTypeRef *IntRefArray = malloc(sizeof(LLVMTypeRef)*params_cnt);
  int i;
  
  /* Build type for function */
  for(i=0; i<params_cnt; i++)
    IntRefArray[i] = Integer;

  LLVMBool var_arg = 0; /* false */
  LLVMTypeRef FunType = LLVMFunctionType(Integer,IntRefArray,params_cnt,var_arg);

  /* Found in LLVM-C -> Core -> Modules */
  char *tmp, *out = fileNameOut;

  if ((tmp=strchr(out,'.'))!='\0')
    {
      *tmp = 0;
    }

  /* Found in LLVM-C -> Core -> Modules */
  Function = LLVMAddFunction(Module,out,FunType);

  /* Add a new entry basic block to the function */
  BasicBlock = LLVMAppendBasicBlock(Function,"entry");

  /* Create an instruction builder class */
  Builder = LLVMCreateBuilder();

  /* Insert new instruction at the end of entry block */
  LLVMPositionBuilderAtEnd(Builder,BasicBlock);
}
;

varlist:   varlist COMMA ID 
{
  /* IMPLEMENT: remember ID and its position so that you can
     reference the parameter later
  */
  if(-1 == get_IdVal($3))
  {
     add_Id($3,params_cnt);
     params_cnt++;
  }
  else
  {
     printf("ERROR : Multiple definitions of IDENT '%s'\n",$3);
     abort();
  }
}
	| ID
{
  /* IMPLEMENT: remember ID and its position for later reference*/
  add_Id($1,params_cnt);
  params_cnt++;
}
;

stmtlist:  stmtlist stmt { $$ = $2; }
| stmt                   { $$ = $1; }
;         

stmt: TMP ASSIGN expr SEMI
{
  /* IMPLEMENT: remember temporary and associated expression $3 */
  add_tmp($1,$3);	//$3 is LLVmValueRef
  last_TmpVal =$3;	 
}
;

expr:   expr MINUS expr
{
  /* IMPLEMENT: subtraction */
  $$ = LLVMBuildSub(Builder,$1,$3,"");
} 
     | expr PLUS expr
{
  /* IMPLEMENT: addition */
  $$ = LLVMBuildAdd(Builder,$1,$3,"");
}
      | MINUS expr 
{
  /* IMPLEMENT: negation */
  $$ = LLVMBuildNeg(Builder,$2,"");
}
      | expr MULTIPLY expr
{
  /* IMPLEMENT: multiply */
   $$ = LLVMBuildMul(Builder,$1,$3,"");
}
      | expr DIVIDE expr
{
  /* IMPLEMENT: divide */
   if(LLVMIsConstant($3)&&(0 == LLVMConstIntGetSExtValue($3)))
   {
   		printf("ERROR : In rule 'expr DIVIDE expr' :: $3 is 0\nDIVIDE by ZERO\n ");
   		abort();		
   }
   $$ = LLVMBuildSDiv(Builder,$1,$3,"");
}
      | expr RAISE expr
{
  /*IMPLEMENT : raise with error if $3 is negative no*/
   int it;
   if(LLVMIsConstant($3))
   {	
   		//printf("$3 is constant\n");
 		/*printf("expr value = %llu",LLVMConstIntGetZExtValue($3));
  		printf("expr value = %lld",LLVMConstIntGetSExtValue($3));*/
   
      if(LLVMConstIntGetSExtValue($3)>=0)
      {
      	//printf("$3 is greater than equal to 0\n");
      	if(0 == LLVMConstIntGetSExtValue($3))
      	{	
      	   	//printf("$3 is 0\n");
      		if(0 == LLVMConstIntGetSExtValue($1))
      		{
      			printf("ERROR: Undefined expression :: '0^^0'\n");
      			abort();
       		}
       		else
       		{
       		  	//printf("$3 is 0 and $1 is non-zero.\n");
      			$$ = LLVMConstInt(LLVMInt64Type(),1,0); 	
      		}
      	}
      	else
      	{
      	   	//printf("$3 is NOT 0\n");
      		if(LLVMIsConstant($1))
      	   	{
		  		if(0 == LLVMConstIntGetSExtValue($1))
		  		{
		  		   	printf("$1 is 0\n");
		  			printf("Warning : '0^^constant' expression\n");
		  		}
       	 	}
       	
   			//printf("you are here\n");
   			Result = $1;
   			if(LLVMConstIntGetSExtValue($3)>1)
   			{
	  			for(it =2;it<=LLVMConstIntGetSExtValue($3);it++)
	  			{
	   	     		Result = LLVMBuildMul(Builder,Result,$1,""); 
	   	   		}
	   	   	}
	   		$$ = Result;
	   	}
   	  }
   	  else
   	  {
	  	printf("ERROR : In rule 'expr RAISE expr' :  $3 is NEGATIVE\n");
	  	abort();
   	  }
   }
   else
   {
   		printf("ERROR : In rule 'expr RAISE expr' : $3 is NOT a CONSTANT\n");
   		abort();
   }
}
      | expr LESSTHAN expr
{
  /*IMPLEMENT : compare both expressions and produce 0 if false and 1 if true*/ 
  
  $$ =  LLVMBuildZExt(Builder,LLVMBuildICmp(Builder,LLVMIntSLT,$1,$3,""),LLVMInt64Type(),"");   
} 	
      | NUM
{ 
  /* IMPLEMENT: constant */
   $$ = LLVMConstInt(LLVMInt64Type(),$1,0);
}
      | ID
{
  /* IMPLEMENT: get reference to function parameter
     Hint: LLVMGetParam(...)
   */
  if(-1 == get_IdVal($1))
  {
  		printf("ERROR : Undefined IDENT '%s'\n",$1);
  		abort();
  }
  $$ = LLVMGetParam(Function,get_IdVal($1));
}
      | TMP
{
  /* IMPLEMENT: get expression associated with TMP */
  if(NULL == get_TmpVal($1))
  {
  		printf("ERROR: Undefined TMP :: '%s'\n",$1);
  		abort();
  }  
  $$ = get_TmpVal($1); 
}
;

%%


void initialize()
{
  /* IMPLEMENT: add something here if needed */
}

int yyerror(const char *msg)
{
  printf("%s",msg);
  return 0;
}
