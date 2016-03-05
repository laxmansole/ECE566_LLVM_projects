%{
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "llvm-c/Core.h"
#include "llvm-c/BitReader.h"
#include "llvm-c/BitWriter.h"

#include "list.h"
#include "symbol.h"

int num_errors;

extern int yylex();   /* lexical analyzer generated from lex.l */

int yyerror();
int parser_error(const char*);

void minic_abort();
char *get_filename();
int get_lineno();

int loops_found=0;

extern LLVMModuleRef Module;
extern LLVMContextRef Context;
 LLVMBuilderRef Builder;

LLVMValueRef Function=NULL;
LLVMValueRef BuildFunction(LLVMTypeRef RetType, const char *name, 
			   paramlist_t *params);

%}


/* Data structure for tree nodes*/

%union {
  int num;
  char * id;
  LLVMTypeRef  type;
  LLVMValueRef value;
  LLVMBasicBlockRef bb;
  paramlist_t *params;
}

/* these tokens are simply their corresponding int values, more terminals*/

%token SEMICOLON COMMA COLON
%token LBRACE RBRACE LPAREN RPAREN LBRACKET RBRACKET
%token ASSIGN PLUS MINUS STAR DIV MOD 
%token LT GT LTE GTE EQ NEQ NOT
%token LOGICAL_AND LOGICAL_OR
%token BITWISE_OR BITWISE_XOR LSHIFT RSHIFT BITWISE_INVERT

%token DOT ARROW AMPERSAND QUESTION_MARK

%token FOR WHILE IF ELSE DO STRUCT SIZEOF RETURN 
%token BREAK CONTINUE
%token INT VOID

/* no meaning, just placeholders */
%token STATIC AUTO EXTERN TYPEDEF CONST VOLATILE ENUM UNION REGISTER
/* NUMBER and ID have values associated with them returned from lex*/

%token <num> NUMBER /*data type of NUMBER is num union*/
%token <id>  ID

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

/* values created by parser*/

%type <id> declarator
%type <params> param_list param_list_opt
%type <value> expression
%type <value> assignment_expression
%type <value> conditional_expression
%type <value> constant_expression
%type <value> logical_OR_expression
%type <value> logical_AND_expression
%type <value> inclusive_OR_expression
%type <value> exclusive_OR_expression
%type <value> AND_expression
%type <value> equality_expression
%type <value> relational_expression
%type <value> shift_expression
%type <value> additive_expression
%type <value> multiplicative_expression
%type <value> cast_expression
%type <value> unary_expression
%type <value> lhs_expression
%type <value> postfix_expression
%type <value> primary_expression
%type <value> constant
%type <value> expr_opt 
%type <type>  type_specifier
/* 
   The grammar used here is largely borrowed from Kernighan and Ritchie's "The C
   Programming Language," 2nd Edition, Prentice Hall, 1988. 

   But, some modifications have been made specifically for MiniC!
 */

%%

/* 
   Beginning of grammar: Rules
*/

translation_unit:	  external_declaration
			| translation_unit external_declaration
;

external_declaration:	  function_definition
{
  /* finish compiling function */
  if(num_errors>100)
    {
      minic_abort();
    }
  else if(num_errors==0)
    {
      
    }
}
                        | declaration 
{ 
  /* nothing to be done here */
}
;

function_definition:	  type_specifier ID LPAREN param_list_opt RPAREN 
{
  symbol_push_scope();
  /* This is a mid-rule action */
  BuildFunction($1,$2,$4);  
} 
                          compound_stmt 
{ 
  /* This is the rule completion */
  LLVMBasicBlockRef BB = LLVMGetInsertBlock(Builder);
  if(!LLVMGetBasicBlockTerminator(BB))
    {
      LLVMBuildRet(Builder,LLVMConstInt(LLVMInt32TypeInContext(Context),
					0,(LLVMBool)1));
    }

  symbol_pop_scope();
  /* make sure basic block has a terminator (a return statement) */
}
                        | type_specifier STAR ID LPAREN param_list_opt RPAREN 
{
  symbol_push_scope();
  BuildFunction(LLVMPointerType($1,0),$3,$5);
} 
                          compound_stmt 
{ 
  /* This is the rule completion */


  /* make sure basic block has a terminator (a return statement) */

  LLVMBasicBlockRef BB = LLVMGetInsertBlock(Builder);
  if(!LLVMGetBasicBlockTerminator(BB))
    {
      LLVMBuildRet(Builder,LLVMConstPointerNull(LLVMPointerType(LLVMInt32TypeInContext(Context),0)));
    }

  symbol_pop_scope();
}
;

declaration:    type_specifier STAR declarator SEMICOLON
{
  if (is_global_scope())
    {
      LLVMAddGlobal(Module,LLVMPointerType($1,0),$3);
    } 
  else
    {
    	printf("STAR declarator SEMICOLON\n");
      symbol_insert($3,  /* map name to alloca */
		    LLVMBuildAlloca(Builder,LLVMPointerType($1,0),$3), /* build alloca */
		    0);  /* not an arg */
    }

} 
              | type_specifier declarator SEMICOLON
{
  if (is_global_scope())
    {
      LLVMAddGlobal(Module,$1,$2);
    }
  else
    {
    printf("declarator SEMICOLON\n");
      symbol_insert($2,  /* map name to alloca */
		    LLVMBuildAlloca(Builder,$1,$2), /* build alloca */
		    0);  /* not an arg */
    }
} 
;

declaration_list:	   declaration
{

}
                         | declaration_list declaration  
{

}
;


type_specifier:		  INT 
{
  $$ = LLVMInt32TypeInContext(Context);
}
;


declarator:		  ID
{
  $$ = $1;
}
;

param_list_opt:           
{ 
  $$ = NULL;
}
                        | param_list
{ 
  $$ = $1;
}
;

param_list:	
			  param_list COMMA type_specifier declarator
{
  $$ = push_param($1,$4,$3);
}
			| param_list COMMA type_specifier STAR declarator
{
  $$ = push_param($1,$5,LLVMPointerType($3,0));
}
                        | param_list COMMA type_specifier
{
  $$ = push_param($1,NULL,$3);
}
			|  type_specifier declarator
{
  /* create a parameter list with this as the first entry */
  $$ = push_param(NULL, $2, $1);
}
			| type_specifier STAR declarator
{
  /* create a parameter list with this as the first entry */
  $$ = push_param(NULL, $3, LLVMPointerType($1,0));
}
                        | type_specifier
{
  /* create a parameter list with this as the first entry */
  $$ = push_param(NULL, NULL, $1);
}
;


statement:		  expr_stmt            
			| compound_stmt        
			| selection_stmt       
			| iteration_stmt       
			| jump_stmt            
                 	| break_stmt
                  	| continue_stmt
;

expr_stmt:	           SEMICOLON            
{ 

}
			|  expression SEMICOLON       
{ 

}
;

compound_stmt:		  LBRACE declaration_list_opt statement_list_opt RBRACE 
{

}
;

declaration_list_opt:	
{

}
			| declaration_list
{

}
;

statement_list_opt:	
{

}
			| statement_list
{

}
;

statement_list:		statement
{

}
			| statement_list statement
{

}
;

break_stmt:               BREAK SEMICOLON
{
	loop_info_t info = get_loop();
	LLVMBasicBlockRef join =info.exit;
	if(join == NULL)
	{
		printf("ERROR: There is no loop for the break statement\n");
	}
	else
	{
		LLVMBuildBr(Builder,join);
		LLVMBasicBlockRef break1 = LLVMAppendBasicBlock(Function,"break");
		LLVMPositionBuilderAtEnd(Builder,break1);
	}
};

continue_stmt:            CONTINUE SEMICOLON
{
	loop_info_t info = get_loop();
	LLVMBasicBlockRef iter =info.exit;
	if(iter == NULL)
	{
		printf("ERROR: There is no loop for the Continue statement\n");
	}
	else
	{
		LLVMBuildBr(Builder,iter);
		LLVMBasicBlockRef cont = LLVMAppendBasicBlock(Function,"cont");
		LLVMPositionBuilderAtEnd(Builder,cont);
	}
};

selection_stmt:		  
		          IF LPAREN expression 
{
	LLVMBasicBlockRef then = LLVMAppendBasicBlock(Function,"if.then");
	LLVMBasicBlockRef els  = LLVMAppendBasicBlock(Function,"if.else");
	
	LLVMValueRef zero = LLVMConstInt(LLVMTypeOf($3),0,1); 
	LLVMValueRef cond = LLVMBuildICmp(Builder, LLVMIntNE, $3,zero,"cond");
	LLVMValueRef br   = LLVMBuildCondBr(Builder,cond, then, els);
	LLVMPositionBuilderAtEnd(Builder, then);
	 $<bb>$ = els;
	
}
				  RPAREN statement
{
	LLVMBasicBlockRef join  = LLVMAppendBasicBlock(Function,"if.join");
	LLVMValueRef Br = LLVMBuildBr(Builder, join);
	LLVMPositionBuilderAtEnd(Builder, $<bb>4);
	$<bb>$ = join;
}
				  ELSE statement 
{ 
	LLVMValueRef Br1 = LLVMBuildBr(Builder,$<bb>7);
	LLVMPositionBuilderAtEnd(Builder, $<bb>7);
}
;

iteration_stmt:		  WHILE LPAREN { 
	/* set up header basic block
    make it the new insertion point */
	printf("inside while-1\n");
	LLVMBasicBlockRef cond = LLVMAppendBasicBlock(Function,"while.cond");
	LLVMBuildBr(Builder,cond);
	LLVMPositionBuilderAtEnd(Builder, cond);
	 $<bb>$ = cond;

} expression RPAREN { 
	/* set up loop body */
	printf("inside while-2\n");
	LLVMBasicBlockRef body = LLVMAppendBasicBlock(Function,"while.body");
	LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function,"while.join");

	/* create new body and exit blocks */
	LLVMValueRef zero = LLVMConstInt(LLVMTypeOf($4),0,1); 
	LLVMValueRef cond = LLVMBuildICmp(Builder, LLVMIntNE, $4,zero,"cond");
	LLVMValueRef br = LLVMBuildCondBr(Builder,cond,body,join);

	/* to supcondport nesting: */
    LLVMPositionBuilderAtEnd(Builder,body);
	$<bb>$ = join;

  	push_loop($<bb>3,body,body,join);
  
} 
			  statement
{
	/* finish loop */
	printf("inside while-3\n");  
	LLVMBuildBr(Builder,$<bb>3);
	LLVMPositionBuilderAtEnd(Builder, $<bb>6);
	printf("end of while loop\n");     
	loop_info_t info = get_loop();
	pop_loop();
}
			| FOR LPAREN expr_opt 
{
	LLVMBasicBlockRef cond = LLVMAppendBasicBlock(Function,"for.cond");
	LLVMBuildBr(Builder,cond);
	LLVMPositionBuilderAtEnd(Builder,cond);
	$<bb>$ = cond;//$<bb>4
	printf("inside for-1\n");
 } 
			SEMICOLON 
{
	LLVMBasicBlockRef body = LLVMAppendBasicBlock(Function,"for.body");
	$<bb>$ = body;//$<bb>6	
	printf("inside for-2\n");
}
			expr_opt 
{
	LLVMBasicBlockRef join = LLVMAppendBasicBlock(Function,"for.join");
	printf("inside beg for-3\n");
	LLVMValueRef zero = LLVMConstInt(LLVMTypeOf($7),0,1); 
	LLVMValueRef condition = LLVMBuildICmp(Builder, LLVMIntNE, $7,zero,"cond");
	LLVMBuildCondBr(Builder,condition,$<bb>6	,join);
	$<bb>$ = join;//$<bb>8
	printf("inside for-3\n");
 
} 
			SEMICOLON
{
	 LLVMBasicBlockRef inc  = LLVMAppendBasicBlock(Function,"for.inc");
	 //LLVMBuildBr(Builder,$<bb>3);	
 	 LLVMPositionBuilderAtEnd(Builder,inc);
     $<bb>$ = inc;//$<bb>10
     printf("inside for-4\n");
}
			 expr_opt 
{

	LLVMBuildBr(Builder,$<bb>4);
	LLVMPositionBuilderAtEnd(Builder,$<bb>6);//body block
	printf("inside for-5\n");
}
			RPAREN statement
{
	push_loop($<bb>4,$<bb>6,$<bb>10,$<bb>8);
	printf("inside b for-6\n");
	LLVMBuildBr(Builder,$<bb>10);//inc
	printf("inside m for-6\n");
	LLVMPositionBuilderAtEnd(Builder,$<bb>8);//cond
	//LLVMBuildBr(Builder,$<bb>3);
	//$<bb>$ = join;
	printf("inside for-6\n");
 	loop_info_t info = get_loop();
	pop_loop();   
}
;

expr_opt:		
{ 
  //$$ = LLVMConstInt(LLVMInt32Type(),0,0);
}
			| expression
{ 
  //$$ = $1;
}
;

jump_stmt:		  RETURN SEMICOLON
{ 
      LLVMBuildRetVoid(Builder);

}
			| RETURN expression SEMICOLON
{
	LLVMBuildRet(Builder,$2);
}
;

expression:               assignment_expression
{ 
  $$=$1;
}
;

assignment_expression:    conditional_expression
{
  $$=$1;
}
                        | lhs_expression ASSIGN assignment_expression
{
	if(LLVMGetElementType(LLVMTypeOf($1))== LLVMTypeOf($3))
	{
		printf("ASSIGN express match\n"); 
		$1 = LLVMBuildStore(Builder,$3,$1); 
	}
	
}
;


conditional_expression:   logical_OR_expression
{
  $$=$1;
}
                        | logical_OR_expression QUESTION_MARK expression COLON conditional_expression
{
	$$ = LLVMBuildSelect(Builder,$1,$3,$5,"SelectStatement");
}
;

constant_expression:       conditional_expression
{ $$ = $1; }
;

logical_OR_expression:    logical_AND_expression
{
  $$ = $1;
}
                        | logical_OR_expression LOGICAL_OR logical_AND_expression
{
  //Logical OR
    $$ = LLVMBuildZExt(Builder,
  			LLVMBuildOr(Builder,
			LLVMBuildICmp(Builder,LLVMIntNE,$1,LLVMConstInt(LLVMInt32Type(),0,0),""),
			LLVMBuildICmp(Builder,LLVMIntNE,$3,LLVMConstInt(LLVMInt32Type(),0,0),""),
			""),
			LLVMInt32Type(),
			""); 

};

logical_AND_expression:   inclusive_OR_expression
{
  $$ = $1;
}
                        | logical_AND_expression LOGICAL_AND inclusive_OR_expression
{
  //Logical AND 
  $$ = LLVMBuildZExt(Builder,
  			LLVMBuildAnd(Builder,
			LLVMBuildICmp(Builder,LLVMIntNE,$1,LLVMConstInt(LLVMInt32Type(),0,0),""),
			LLVMBuildICmp(Builder,LLVMIntNE,$3,LLVMConstInt(LLVMInt32Type(),0,0),""),
			""),
			LLVMInt32Type(),
			""); 
}
;

inclusive_OR_expression:  exclusive_OR_expression
{
  $$=$1;
}
                        | inclusive_OR_expression BITWISE_OR exclusive_OR_expression
{
  $$ = LLVMBuildOr(Builder,$1,$3,"");//bitwise OR
}
;

exclusive_OR_expression:  AND_expression
{
  $$ = $1;
}
                        | exclusive_OR_expression BITWISE_XOR AND_expression
{
  $$ = LLVMBuildXor(Builder,$1,$3,"");//bitwise XOR
}
;

AND_expression:           equality_expression
{
  $$ = $1;
}
                        | AND_expression AMPERSAND equality_expression
{
  $$ = LLVMBuildAnd(Builder,$1,$3,"");//Bitwise AND
}
;

equality_expression:      relational_expression
{
    $$ = $1;
}
                        | equality_expression EQ relational_expression
{
    $$ =  LLVMBuildZExt(Builder,LLVMBuildICmp(Builder,LLVMIntEQ,$1,$3,""),LLVMInt32Type(),""); 
}

                        | equality_expression NEQ relational_expression
{
    $$ =  LLVMBuildZExt(Builder,LLVMBuildICmp(Builder,LLVMIntNE,$1,$3,""),LLVMInt32Type(),""); 
}
;

relational_expression:    shift_expression
{
   $$=$1;
}
                        | relational_expression LT shift_expression
{
  
	if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) && (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )  
	{
		printf("ERROR:LT-->Both operands are of pointer type.\n");
		//abort();
	}
	$$ =  LLVMBuildZExt(Builder,LLVMBuildICmp(Builder,LLVMIntSLT,$1,$3,""),LLVMInt32Type(),""); 
}
                        | relational_expression GT shift_expression
{
	if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) && (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )  
	{
		printf("ERROR:GT-->Both operands are of pointer type.\n");
		//abort();
	}
	$$ =  LLVMBuildZExt(Builder,LLVMBuildICmp(Builder,LLVMIntSGT,$1,$3,""),LLVMInt32Type(),""); 
}
                        | relational_expression LTE shift_expression
{
	if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) && (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )  
	{
		printf("ERROR:LTE-->Both operands are of pointer type.\n");
		//abort();
	}
    $$ =  LLVMBuildZExt(Builder,LLVMBuildICmp(Builder,LLVMIntSLE,$1,$3,""),LLVMInt32Type(),""); 
}
                        | relational_expression GTE shift_expression
{
	if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) && (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )  
	{
		printf("ERROR:GTE-->Both operands are of pointer type.\n");
		//abort();
	}
    $$ =  LLVMBuildZExt(Builder,LLVMBuildICmp(Builder,LLVMIntSGE,$1,$3,""),LLVMInt32Type(),""); 
}
;

shift_expression:         additive_expression
{
  $$=$1;
}
                        | shift_expression LSHIFT additive_expression
{
  	if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) && (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )  
	{
		printf("ERROR:LSHIFT-->Both operands are of pointer type.\n");
		abort();
	}
	else if((LLVMTypeOf($1) == LLVMInt32Type()) && (LLVMTypeOf($3) == LLVMInt32Type()) )
	{
		 $$ = LLVMBuildShl(Builder,$1,$3,"");
	}
}
                        | shift_expression RSHIFT additive_expression
{
   if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) && (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )  
	{
		printf("ERROR:RSHIFT-->Both operands are of pointer type.\n");
		abort();
	}
	else if((LLVMTypeOf($1) == LLVMInt32Type()) && (LLVMTypeOf($3) == LLVMInt32Type()) )
	{
		 $$ = LLVMBuildAShr(Builder,$1,$3,"");
	}
}
;

additive_expression:      multiplicative_expression
{
  $$ = $1;
}
                        | additive_expression PLUS multiplicative_expression
{
	if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) && (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )  
	{
		printf("ERROR:ADDITION-->Both operands are of pointer type.\n");
		abort();
	}
	else if((LLVMTypeOf($1) == LLVMInt32Type()) && (LLVMTypeOf($3) == LLVMInt32Type()) )
	{
		 $$ = LLVMBuildAdd(Builder,$1,$3,"");
	}
	else if( (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )
	{
		LLVMValueRef indices[1] = {$1};
		LLVMValueRef GEP = LLVMBuildGEP(Builder,$3,indices,1,"GEP");
		$$ = GEP;
	}
	else if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) )
	{
		LLVMValueRef indices[1] = {$3};
		LLVMValueRef GEP = LLVMBuildGEP(Builder,$1,indices,1,"GEP");
		$$ = GEP;
	}
	
}
                        | additive_expression MINUS multiplicative_expression
{
  	if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) && (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )  
	{
		$$ = LLVMBuildSub(Builder,$1,$3,"");
	}
	else if((LLVMTypeOf($1) == LLVMInt32Type()) && (LLVMTypeOf($3) == LLVMInt32Type()) )
	{
		 $$ = LLVMBuildSub(Builder,$1,$3,"");
	}
	else if( (LLVMTypeOf($1) == LLVMInt32Type()) && (LLVMTypeOf($3)==LLVMPointerType(LLVMInt32Type(),0)) )
	{
		printf("ERROR:Subtraction--> Pointer cannot be subtracted from integer\n");
	}
	else if( (LLVMTypeOf($1)==LLVMPointerType(LLVMInt32Type(),0)) && (LLVMTypeOf($3) == LLVMInt32Type()) )
	{
		LLVMValueRef indices[1] = {$3};
		LLVMValueRef GEP = LLVMBuildGEP(Builder,$1,indices,1,"GEP");
		$$ = GEP;
	}
}
;

multiplicative_expression:  cast_expression
{
  $$ = $1;
}
                        | multiplicative_expression STAR cast_expression
{
  $$ = LLVMBuildMul(Builder,$1,$3,"");// multiplication
}
                        | multiplicative_expression DIV cast_expression
{
	  if(LLVMIsConstant($3)&&(0 == LLVMConstIntGetSExtValue($3)))
	  {
	  	printf("ERROR :multiplicative_expression DIV cast_expression :: $3 is 0\nDIVIDE by ZERO\n ");
	   	abort();		
	  }
	  $$ = LLVMBuildSDiv(Builder,$1,$3,"");
}
                        | multiplicative_expression MOD cast_expression
{
	  $$ = LLVMBuildSRem(Builder,$1,$3,"");
}
;

cast_expression:          unary_expression
{ $$ = $1; }
;

lhs_expression:           ID 
{
  int isArg=0;
  LLVMValueRef val = symbol_find($1,&isArg);
  if (isArg)
    {
      // error
    }
  else
    $$ = val;
}
                        | STAR ID
{
  int isArg=0;
  LLVMValueRef val = symbol_find($2,&isArg);
  if (isArg)
    {
      // error
    }
  else
    $$ = LLVMBuildLoad(Builder,val,"");
}
;

unary_expression:         postfix_expression
{
  $$ = $1;
}
                        | AMPERSAND primary_expression
{
 	printf("IN Rule    | AMPERSAND primary_expression");
	 if(LLVMIsALoadInst($2))
	 {
	 	$$ = LLVMGetOperand($2,0);
	 }
	 else
	 {
		 printf("IN Rule    | AMPERSAND primary_expression : $2 is not a Load.");
	 }
 
}
                        | STAR primary_expression
{
  $$ = LLVMBuildLoad(Builder,$2,"");
}
                        | MINUS unary_expression
{
  
  $$ = LLVMBuildNeg(Builder,$2,"");//negation operation
}
                        | PLUS unary_expression
{
  $$ = $2;//same as input
}
                        | BITWISE_INVERT unary_expression
{
  $$ = LLVMBuildNeg(Builder,$2,""); //bitwise_inversion can be done using Xor operation.
}
                        | NOT unary_expression
{

  LLVMValueRef zero = LLVMConstInt(LLVMTypeOf($2),0,1); 
  LLVMValueRef icmp = LLVMBuildICmp(Builder, LLVMIntEQ, $2,zero,"logical.neg");
  $$ = LLVMBuildZExt(Builder, icmp, LLVMInt32Type(),"logical.neg");
   
}
;


postfix_expression:       primary_expression
{
  $$ = $1;
}
;

primary_expression:       ID 
{ 
  int isArg=0;
  LLVMValueRef val = symbol_find($1,&isArg);
  if (isArg)
    $$ = val;
  else
    $$ = LLVMBuildLoad(Builder,val,"");
}
                        | constant
{
  $$ = $1;
}
                        | LPAREN expression RPAREN
{
  $$ = $2;
}
;

constant:	          NUMBER  
{ 
  $$ = LLVMConstInt(LLVMInt32Type(),$1,0);//32-bit integer
} 
;

%%

LLVMValueRef BuildFunction(LLVMTypeRef RetType, const char *name, 
			   paramlist_t *params)
{
  int i;
  int size = paramlist_size(params);
  LLVMTypeRef *ParamArray = malloc(sizeof(LLVMTypeRef)*size);
  LLVMTypeRef FunType;
  LLVMBasicBlockRef BasicBlock;

  paramlist_t *tmp = params;
  /* Build type for function */
  for(i=size-1; i>=0; i--) 
    {
      ParamArray[i] = tmp->type;
      tmp = next_param(tmp);
    }
  
  FunType = LLVMFunctionType(RetType,ParamArray,size,0);

  Function = LLVMAddFunction(Module,name,FunType);
  
  /* Add a new entry basic block to the function */
  BasicBlock = LLVMAppendBasicBlock(Function,"entry");

  /* Create an instruction builder class */
  Builder = LLVMCreateBuilder();

  /* Insert new instruction at the end of entry block */
  LLVMPositionBuilderAtEnd(Builder,BasicBlock);

  tmp = params;
  for(i=size-1; i>=0; i--)
    {
      LLVMValueRef alloca = LLVMBuildAlloca(Builder,tmp->type,tmp->name);
      LLVMBuildStore(Builder,LLVMGetParam(Function,i),alloca);
      symbol_insert(tmp->name,alloca,0);
      tmp=next_param(tmp);
    }

  return Function;
}

extern int line_num;
extern char *infile[];
static int   infile_cnt=0;
extern FILE * yyin;

int parser_error(const char *msg)
{
  printf("%s (%d): Error -- %s\n",infile[infile_cnt-1],line_num,msg);
  return 1;
}

int internal_error(const char *msg)
{
  printf("%s (%d): Internal Error -- %s\n",infile[infile_cnt-1],line_num,msg);
  return 1;
}

int yywrap() {
  static FILE * currentFile = NULL;

  if ( (currentFile != 0) ) {
    fclose(yyin);
  }
  
  if(infile[infile_cnt]==NULL)
    return 1;

  currentFile = fopen(infile[infile_cnt],"r");
  if(currentFile!=NULL)
    yyin = currentFile;
  else
    printf("Could not open file: %s",infile[infile_cnt]);

  infile_cnt++;
  
  return (currentFile)?0:1;
}

int yyerror()
{
  parser_error("Un-resolved syntax error.");
  return 1;
}

char * get_filename()
{
  return infile[infile_cnt-1];
}

int get_lineno()
{
  return line_num;
}


void minic_abort()
{
  parser_error("Too many errors to continue.");
  exit(1);
}
