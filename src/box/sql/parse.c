/*
** 2000-05-29
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Driver template for the LEMON parser generator.
**
** The "lemon" program processes an LALR(1) input grammar file, then uses
** this template to construct a parser.  The "lemon" program inserts text
** at each "%%" line.  Also, any "P-a-r-s-e" identifer prefix (without the
** interstitial "-" characters) contained in this template is changed into
** the value of the %name directive from the grammar.  Otherwise, the content
** of this template is copied straight through into the generate parser
** source file.
**
** The following is the concatenation of all %include directives from the
** input grammar file:
*/
#include <stdio.h>
#include <stdbool.h>
/************ Begin %include sections from the grammar ************************/
#line 52 "parse.y"

#include "sqliteInt.h"

/*
** Disable all error recovery processing in the parser push-down
** automaton.
*/
#define YYNOERRORRECOVERY 1

/*
** Make yytestcase() the same as testcase()
*/
#define yytestcase(X) testcase(X)

/*
** Indicate that sqlite3ParserFree() will never be called with a null
** pointer.
*/
#define YYPARSEFREENEVERNULL 1

/*
** Alternative datatype for the argument to the malloc() routine passed
** into sqlite3ParserAlloc().  The default is size_t.
*/
#define YYMALLOCARGTYPE  u64

/*
** An instance of this structure holds information about the
** LIMIT clause of a SELECT statement.
*/
struct LimitVal {
  Expr *pLimit;    /* The LIMIT expression.  NULL if there is no limit */
  Expr *pOffset;   /* The OFFSET expression.  NULL if there is none */
};

/*
** An instance of the following structure describes the event of a
** TRIGGER.  "a" is the event type, one of TK_UPDATE, TK_INSERT,
** TK_DELETE, or TK_INSTEAD.  If the event is of the form
**
**      UPDATE ON (a,b,c)
**
** Then the "b" IdList records the list "a,b,c".
*/
struct TrigEvent { int a; IdList * b; };

/*
** Disable lookaside memory allocation for objects that might be
** shared across database connections.
*/
static void disableLookaside(Parse *pParse){
  pParse->disableLookaside++;
  pParse->db->lookaside.bDisable++;
}

#line 416 "parse.y"

  /*
  ** For a compound SELECT statement, make sure p->pPrior->pNext==p for
  ** all elements in the list.  And make sure list length does not exceed
  ** SQLITE_LIMIT_COMPOUND_SELECT.
  */
  static void parserDoubleLinkSelect(Parse *pParse, Select *p){
    if( p->pPrior ){
      Select *pNext = 0, *pLoop;
      int mxSelect, cnt = 0;
      for(pLoop=p; pLoop; pNext=pLoop, pLoop=pLoop->pPrior, cnt++){
        pLoop->pNext = pNext;
        pLoop->selFlags |= SF_Compound;
      }
      if( (p->selFlags & SF_MultiValue)==0 && 
        (mxSelect = pParse->db->aLimit[SQLITE_LIMIT_COMPOUND_SELECT])>0 &&
        cnt>mxSelect
      ){
        sqlite3ErrorMsg(pParse, "Too many UNION or EXCEPT or INTERSECT operations");
      }
    }
  }
#line 855 "parse.y"

  /* This is a utility routine used to set the ExprSpan.zStart and
  ** ExprSpan.zEnd values of pOut so that the span covers the complete
  ** range of text beginning with pStart and going to the end of pEnd.
  */
  static void spanSet(ExprSpan *pOut, Token *pStart, Token *pEnd){
    pOut->zStart = pStart->z;
    pOut->zEnd = &pEnd->z[pEnd->n];
  }

  /* Construct a new Expr object from a single identifier.  Use the
  ** new Expr to populate pOut.  Set the span of pOut to be the identifier
  ** that created the expression.
  */
  static void spanExpr(ExprSpan *pOut, Parse *pParse, int op, Token t){
    Expr *p = sqlite3DbMallocRawNN(pParse->db, sizeof(Expr)+t.n+1);
    if( p ){
      memset(p, 0, sizeof(Expr));
      switch (op) {
      case TK_STRING:
	p->typeDef.type = SQLITE_AFF_TEXT;
        break;
      case TK_BLOB:
        p->typeDef.type = SQLITE_AFF_BLOB;
        break;
      case TK_INTEGER:
        p->typeDef.type = SQLITE_AFF_INTEGER;
        break;
      case TK_FLOAT:
        p->typeDef.type = SQLITE_AFF_REAL;
        break;
      }
      p->op = (u8)op;
      p->flags = EP_Leaf;
      p->iAgg = -1;
      p->u.zToken = (char*)&p[1];
      memcpy(p->u.zToken, t.z, t.n);
      p->u.zToken[t.n] = 0;
      if (op != TK_VARIABLE){
        sqlite3NormalizeName(p->u.zToken);
      }
#if SQLITE_MAX_EXPR_DEPTH>0
      p->nHeight = 1;
#endif  
    }
    pOut->pExpr = p;
    pOut->zStart = t.z;
    pOut->zEnd = &t.z[t.n];
  }
#line 977 "parse.y"

  /* This routine constructs a binary expression node out of two ExprSpan
  ** objects and uses the result to populate a new ExprSpan object.
  */
  static void spanBinaryExpr(
    Parse *pParse,      /* The parsing context.  Errors accumulate here */
    int op,             /* The binary operation */
    ExprSpan *pLeft,    /* The left operand, and output */
    ExprSpan *pRight    /* The right operand */
  ){
    pLeft->pExpr = sqlite3PExpr(pParse, op, pLeft->pExpr, pRight->pExpr);
    pLeft->zEnd = pRight->zEnd;
  }

  /* If doNot is true, then add a TK_NOT Expr-node wrapper around the
  ** outside of *ppExpr.
  */
  static void exprNot(Parse *pParse, int doNot, ExprSpan *pSpan){
    if( doNot ){
      pSpan->pExpr = sqlite3PExpr(pParse, TK_NOT, pSpan->pExpr, 0);
    }
  }
#line 1051 "parse.y"

  /* Construct an expression node for a unary postfix operator
  */
  static void spanUnaryPostfix(
    Parse *pParse,         /* Parsing context to record errors */
    int op,                /* The operator */
    ExprSpan *pOperand,    /* The operand, and output */
    Token *pPostOp         /* The operand token for setting the span */
  ){
    pOperand->pExpr = sqlite3PExpr(pParse, op, pOperand->pExpr, 0);
    pOperand->zEnd = &pPostOp->z[pPostOp->n];
  }                           
#line 1068 "parse.y"

  /* A routine to convert a binary TK_IS or TK_ISNOT expression into a
  ** unary TK_ISNULL or TK_NOTNULL expression. */
  static void binaryToUnaryIfNull(Parse *pParse, Expr *pY, Expr *pA, int op){
    sqlite3 *db = pParse->db;
    if( pA && pY && pY->op==TK_NULL ){
      pA->op = (u8)op;
      sql_expr_free(db, pA->pRight, false);
      pA->pRight = 0;
    }
  }
#line 1096 "parse.y"

  /* Construct an expression node for a unary prefix operator
  */
  static void spanUnaryPrefix(
    ExprSpan *pOut,        /* Write the new expression node here */
    Parse *pParse,         /* Parsing context to record errors */
    int op,                /* The operator */
    ExprSpan *pOperand,    /* The operand */
    Token *pPreOp         /* The operand token for setting the span */
  ){
    pOut->zStart = pPreOp->z;
    pOut->pExpr = sqlite3PExpr(pParse, op, pOperand->pExpr, 0);
    pOut->zEnd = pOperand->zEnd;
  }
#line 1301 "parse.y"

  /* Add a single new term to an ExprList that is used to store a
  ** list of identifiers.  Report an error if the ID list contains
  ** a COLLATE clause or an ASC or DESC keyword, except ignore the
  ** error while parsing a legacy schema.
  */
  static ExprList *parserAddExprIdListTerm(
    Parse *pParse,
    ExprList *pPrior,
    Token *pIdToken,
    int hasCollate,
    int sortOrder
  ){
    ExprList *p = sqlite3ExprListAppend(pParse, pPrior, 0);
    if( (hasCollate || sortOrder!=SQLITE_SO_UNDEFINED)
        && pParse->db->init.busy==0
    ){
      sqlite3ErrorMsg(pParse, "syntax error after column name \"%.*s\"",
                         pIdToken->n, pIdToken->z);
    }
    sqlite3ExprListSetName(pParse, p, pIdToken, 1);
    return p;
  }
#line 245 "parse.c"
/**************** End of %include directives **********************************/
/* These constants specify the various numeric values for terminal symbols
** in a format understandable to "makeheaders".  This section is blank unless
** "lemon" is run with the "-m" command-line option.
***************** Begin makeheaders token definitions *************************/
/**************** End makeheaders token definitions ***************************/

/* The next sections is a series of control #defines.
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used to store the integer codes
**                       that represent terminal and non-terminal symbols.
**                       "unsigned char" is used if there are fewer than
**                       256 symbols.  Larger types otherwise.
**    YYNOCODE           is a number of type YYCODETYPE that is not used for
**                       any terminal or nonterminal symbol.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       (also known as: "terminal symbols") have fall-back
**                       values which should be used if the original symbol
**                       would not parse.  This permits keywords to sometimes
**                       be used as identifiers, for example.
**    YYACTIONTYPE       is the data type used for "action codes" - numbers
**                       that indicate what to do in response to the next
**                       token.
**    sqlite3ParserTOKENTYPE     is the data type used for minor type for terminal
**                       symbols.  Background: A "minor type" is a semantic
**                       value associated with a terminal or non-terminal
**                       symbols.  For example, for an "ID" terminal symbol,
**                       the minor type might be the name of the identifier.
**                       Each non-terminal can have a different minor type.
**                       Terminal symbols all have the same minor type, though.
**                       This macros defines the minor type for terminal 
**                       symbols.
**    YYMINORTYPE        is the data type used for all minor types.
**                       This is typically a union of many types, one of
**                       which is sqlite3ParserTOKENTYPE.  The entry in the union
**                       for terminal symbols is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    sqlite3ParserARG_SDECL     A static variable declaration for the %extra_argument
**    sqlite3ParserARG_PDECL     A parameter declaration for the %extra_argument
**    sqlite3ParserARG_STORE     Code to store %extra_argument into yypParser
**    sqlite3ParserARG_FETCH     Code to extract %extra_argument from yypParser
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YY_MAX_SHIFT       Maximum value for shift actions
**    YY_MIN_SHIFTREDUCE Minimum value for shift-reduce actions
**    YY_MAX_SHIFTREDUCE Maximum value for shift-reduce actions
**    YY_MIN_REDUCE      Maximum value for reduce actions
**    YY_ERROR_ACTION    The yy_action[] code for syntax error
**    YY_ACCEPT_ACTION   The yy_action[] code for accept
**    YY_NO_ACTION       The yy_action[] code for no-op
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/************* Begin control #defines *****************************************/
#define YYCODETYPE unsigned char
#define YYNOCODE 243
#define YYACTIONTYPE unsigned short int
#define YYWILDCARD 74
#define sqlite3ParserTOKENTYPE Token
typedef union {
  int yyinit;
  sqlite3ParserTOKENTYPE yy0;
  Select* yy91;
  IdList* yy232;
  ExprSpan yy258;
  struct {int value; int mask;} yy319;
  ExprList* yy322;
  With* yy323;
  int yy328;
  struct TrigEvent yy378;
  TypeDef yy386;
  struct LimitVal yy388;
  Expr* yy418;
  SrcList* yy439;
  TriggerStep* yy451;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define sqlite3ParserARG_SDECL Parse *pParse;
#define sqlite3ParserARG_PDECL ,Parse *pParse
#define sqlite3ParserARG_FETCH Parse *pParse = yypParser->pParse
#define sqlite3ParserARG_STORE yypParser->pParse = pParse
#define YYFALLBACK 1
#define YYNSTATE             413
#define YYNRULE              305
#define YY_MAX_SHIFT         412
#define YY_MIN_SHIFTREDUCE   617
#define YY_MAX_SHIFTREDUCE   921
#define YY_MIN_REDUCE        922
#define YY_MAX_REDUCE        1226
#define YY_ERROR_ACTION      1227
#define YY_ACCEPT_ACTION     1228
#define YY_NO_ACTION         1229
/************* End control #defines *******************************************/

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N <= YY_MAX_SHIFT             Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   N between YY_MIN_SHIFTREDUCE       Shift to an arbitrary state then
**     and YY_MAX_SHIFTREDUCE           reduce by rule N-YY_MIN_SHIFTREDUCE.
**
**   N between YY_MIN_REDUCE            Reduce by rule N-YY_MIN_REDUCE
**     and YY_MAX_REDUCE
**
**   N == YY_ERROR_ACTION               A syntax error has occurred.
**
**   N == YY_ACCEPT_ACTION              The parser accepts its input.
**
**   N == YY_NO_ACTION                  No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as either:
**
**    (A)   N = yy_action[ yy_shift_ofst[S] + X ]
**    (B)   N = yy_default[S]
**
** The (A) formula is preferred.  The B formula is used instead if:
**    (1)  The yy_shift_ofst[S]+X value is out of range, or
**    (2)  yy_lookahead[yy_shift_ofst[S]+X] is not equal to X, or
**    (3)  yy_shift_ofst[S] equal YY_SHIFT_USE_DFLT.
** (Implementation note: YY_SHIFT_USE_DFLT is chosen so that
** YY_SHIFT_USE_DFLT+X will be out of range for all possible lookaheads X.
** Hence only tests (1) and (2) need to be evaluated.)
**
** The formulas above are for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
**
*********** Begin parsing tables **********************************************/
#define YY_ACTTAB_COUNT (1422)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    91,   92,  286,   82,  795,  795,  807,  810,  799,  799,
 /*    10 */    89,   89,   90,   90,   90,   90,  168,   88,   88,   88,
 /*    20 */    88,   87,   87,   86,   86,   86,   85,  307,   90,   90,
 /*    30 */    90,   90,   83,   88,   88,   88,   88,   87,   87,   86,
 /*    40 */    86,   86,   85,  307,  207,  307,  906,   90,   90,   90,
 /*    50 */    90,  122,   88,   88,   88,   88,   87,   87,   86,   86,
 /*    60 */    86,   85,  307,   87,   87,   86,   86,   86,   85,  307,
 /*    70 */   906,   86,   86,   86,   85,  307,   91,   92,  286,   82,
 /*    80 */   795,  795,  807,  810,  799,  799,   89,   89,   90,   90,
 /*    90 */    90,   90,   64,   88,   88,   88,   88,   87,   87,   86,
 /*   100 */    86,   86,   85,  307,   91,   92,  286,   82,  795,  795,
 /*   110 */   807,  810,  799,  799,   89,   89,   90,   90,   90,   90,
 /*   120 */   654,   88,   88,   88,   88,   87,   87,   86,   86,   86,
 /*   130 */    85,  307,  767,   91,   92,  286,   82,  795,  795,  807,
 /*   140 */   810,  799,  799,   89,   89,   90,   90,   90,   90,   67,
 /*   150 */    88,   88,   88,   88,   87,   87,   86,   86,   86,   85,
 /*   160 */   307,  235,  320,  223,  764,  648,   93,  647,  648,  267,
 /*   170 */   267, 1228,  412,    3,  215,  366,  242,  638,  639,  640,
 /*   180 */   641,  642,  268,  268,  648,  157,  647,  648,  267,  267,
 /*   190 */   618,  310,  745,  746,   84,   81,  175,  109,   91,   92,
 /*   200 */   286,   82,  795,  795,  807,  810,  799,  799,   89,   89,
 /*   210 */    90,   90,   90,   90,  767,   88,   88,   88,   88,   87,
 /*   220 */    87,   86,   86,   86,   85,  307,   88,   88,   88,   88,
 /*   230 */    87,   87,   86,   86,   86,   85,  307,  344,  637,  637,
 /*   240 */   729,  662,  341,  235,  330,  234,  657,   91,   92,  286,
 /*   250 */    82,  795,  795,  807,  810,  799,  799,   89,   89,   90,
 /*   260 */    90,   90,   90,  870,   88,   88,   88,   88,   87,   87,
 /*   270 */    86,   86,   86,   85,  307,  656,  198,  871,  312,  356,
 /*   280 */   353,  352,  673,  744,  894,  872,  696,  348,  122,  200,
 /*   290 */   655,  351,  664,  697,  898,  743,   91,   92,  286,   82,
 /*   300 */   795,  795,  807,  810,  799,  799,   89,   89,   90,   90,
 /*   310 */    90,   90,  716,   88,   88,   88,   88,   87,   87,   86,
 /*   320 */    86,   86,   85,  307,   84,   81,  175,  705,  411,  411,
 /*   330 */    84,   81,  175,    5,  217,  207,  119,  906,  369,  786,
 /*   340 */   703,  646,   84,   81,  175,   91,   92,  286,   82,  795,
 /*   350 */   795,  807,  810,  799,  799,   89,   89,   90,   90,   90,
 /*   360 */    90,  906,   88,   88,   88,   88,   87,   87,   86,   86,
 /*   370 */    86,   85,  307,   22,  785,  771,  778,  288,  777,  854,
 /*   380 */   854,  325,  245,  851,  779,  850,  777,  636,  772,  636,
 /*   390 */   645,  323,  773,  672,   91,   92,  286,   82,  795,  795,
 /*   400 */   807,  810,  799,  799,   89,   89,   90,   90,   90,   90,
 /*   410 */   671,   88,   88,   88,   88,   87,   87,   86,   86,   86,
 /*   420 */    85,  307,   91,   92,  286,   82,  795,  795,  807,  810,
 /*   430 */   799,  799,   89,   89,   90,   90,   90,   90,  324,   88,
 /*   440 */    88,   88,   88,   87,   87,   86,   86,   86,   85,  307,
 /*   450 */    91,   92,  286,   82,  795,  795,  807,  810,  799,  799,
 /*   460 */    89,   89,   90,   90,   90,   90,  723,   88,   88,   88,
 /*   470 */    88,   87,   87,   86,   86,   86,   85,  307,   91,   92,
 /*   480 */   286,   82,  795,  795,  807,  810,  799,  799,   89,   89,
 /*   490 */    90,   90,   90,   90,  103,   88,   88,   88,   88,   87,
 /*   500 */    87,   86,   86,   86,   85,  307,   92,  286,   82,  795,
 /*   510 */   795,  807,  810,  799,  799,   89,   89,   90,   90,   90,
 /*   520 */    90,   70,   88,   88,   88,   88,   87,   87,   86,   86,
 /*   530 */    86,   85,  307,  401,  263,  401,  645,  206,  281, 1182,
 /*   540 */  1182,  279,  278,  277,  219,  275,   85,  307,  631,   73,
 /*   550 */   376,  796,  796,  808,  811,   91,   80,  286,   82,  795,
 /*   560 */   795,  807,  810,  799,  799,   89,   89,   90,   90,   90,
 /*   570 */    90,  122,   88,   88,   88,   88,   87,   87,   86,   86,
 /*   580 */    86,   85,  307,  286,   82,  795,  795,  807,  810,  799,
 /*   590 */   799,   89,   89,   90,   90,   90,   90,   78,   88,   88,
 /*   600 */    88,   88,   87,   87,   86,   86,   86,   85,  307,  287,
 /*   610 */   198,  867,  373,  356,  353,  352,   75,   76,  160,  159,
 /*   620 */   158,  371,  398,   77,  317,  351,  865,  155,  154,  163,
 /*   630 */   290,  289,  370,  800,  729,  240,  400,    2, 1129,  181,
 /*   640 */   333,  308,  308,  408,  109,  179,  313,  122,   95,    9,
 /*   650 */     9,  771,  658,  658,  109,  637,  637,  374,  389,  109,
 /*   660 */    48,   48,  848,  785,  729,  778,  387,  777,  331,  161,
 /*   670 */   173,   78,  222,  779,  785,  777,  778,  122,  777,  912,
 /*   680 */   333,  773,  408,   23,  779,  830,  777,  917,  329,  917,
 /*   690 */    75,   76,  773,  767,  916,  382,  367,   77,  239,   10,
 /*   700 */    10,  914,  183,  915,  182,  207,  775,  906,  729,  408,
 /*   710 */   400,    2,  408,  295,  292,  308,  308,  383,  780,  407,
 /*   720 */    18,  328,  235,  330,  234,  729,   48,   48,  231,   10,
 /*   730 */    10,  906,  389,  122,  408,  701,  408,  785,  729,  778,
 /*   740 */   339,  777,  363,  297,  188,   78,  184,  779,  166,  777,
 /*   750 */   109,   30,   30,   48,   48,  773,  408,  176,  176,  315,
 /*   760 */   408,  382,  384,  873,   75,   76,  717,  122,  109,  369,
 /*   770 */   316,   77,  335,   48,   48,  109,  321,   10,   10,  408,
 /*   780 */   775,  408,  729,  318,  400,    2,  364,  408,  302,  308,
 /*   790 */   308,  197,  780,  407,   18,  776,   48,   48,   48,   48,
 /*   800 */   408,  171,  729,  730,   34,   34,  389,  122,  382,  372,
 /*   810 */   408,  785,  339,  778,  162,  777,  339,   48,   48,   78,
 /*   820 */   702,  779,  214,  777,  254,  408,  187,   10,   10,  773,
 /*   830 */   256,  377,   54,  382,  381,  315,  314,  315,   75,   76,
 /*   840 */   698,  379,   47,   47,  408,   77,  840,  842,  840,  730,
 /*   850 */   867,  102,  382,  362,  775,  907,  907,  408,  400,    2,
 /*   860 */   285,   35,   35,  308,  308,  263,  780,  407,   18,  164,
 /*   870 */   730,  228,  176,  176,   36,   36,  729,  637,  637,  199,
 /*   880 */   389,  199,  771,  109,  369,  785,  870,  778,  676,  777,
 /*   890 */   907,  907,  258,   68,  357,  779,  262,  777,  233,  677,
 /*   900 */   871,  301,  190,  773,  261,  408,  137,  331,  872,  388,
 /*   910 */   637,  637,   75,   76,  230,  164,  730,  144,  408,   77,
 /*   920 */   908,  732,   37,   37,  637,  637,  298,  340,  775,  408,
 /*   930 */   299,  408,  400,    2,  227,   38,   38,  308,  308,    7,
 /*   940 */   780,  407,   18,  690,  669,  293,   26,   26,   27,   27,
 /*   950 */   408,  102,  690,  394,  389,  908,  731,  274,  408,  785,
 /*   960 */   282,  778,  226,  777,  225,  215,  366,   29,   29,  779,
 /*   970 */   142,  777,  635,  408,  138,   39,   39,  773,  637,  637,
 /*   980 */   695,  695,  337,  216,  153,  252,  359,  247,  358,  202,
 /*   990 */    40,   40,  759,  408,   24,  408,  245,  264,  637,  637,
 /*  1000 */   319,  408,  775,   66,  408,  109,  408,  718,  408,  264,
 /*  1010 */    41,   41,   11,   11,  780,  407,   18,  408,   42,   42,
 /*  1020 */   408,   97,   97,   43,   43,   44,   44,  338,  408,  296,
 /*  1030 */   637,  637,  200,  408,   31,   31,  251,   45,   45,  224,
 /*  1040 */   408,  305,  408,  232,  408,   46,   46,  250,  408,  667,
 /*  1050 */    32,   32, 1202,  408,  685,  408,  876,  112,  112,  113,
 /*  1060 */   113,  114,  114,  408,    1,   52,   52,  408,  637,  637,
 /*  1070 */    33,   33,   98,   98,  408,  361,  408,  391,  693,  693,
 /*  1080 */    49,   49,  408,  771,   99,   99,  408,  771,  408,  905,
 /*  1090 */   408,  100,  100,   96,   96,  408,   74,  395,   72,  111,
 /*  1100 */   111,  408,  875,  108,  108,  106,  106,  105,  105,  408,
 /*  1110 */   109,  399,  101,  101,  110,  408,  147,  408,  104,  104,
 /*  1120 */   288,  408,  365,  408,  708,  708,   51,   51,  174,  173,
 /*  1130 */   109,   20,   53,   53,   50,   50,  665,  665,   25,   25,
 /*  1140 */    28,   28,  745,  746,  109,  847,  294,  847,  675,  674,
 /*  1150 */   303,  713,  837,  837,  151,  713,  712,  846,  682,  846,
 /*  1160 */   712,  769,   19,  205,  332,  334,  205,  205,  236,  349,
 /*  1170 */    66,  211,  243,  710,   66,   69,  739,  833,  205,  211,
 /*  1180 */   651,  177,  403,  844,  781,  781,  291,  838,  167,  864,
 /*  1190 */   862,  336,  354,  861,  342,  343,  156,  238,  630,  241,
 /*  1200 */   669,  686,  670,  246,  249,  737,  770,  260,  719,  390,
 /*  1210 */   265,  835,  266,  653,  273,  152,  628,  890,  627,  629,
 /*  1220 */   135,  887,  124,  117,  834,  683,   64,  756,  322,   55,
 /*  1230 */   849,  327,  229,  347,  186,  866,  193,  143,  194,  145,
 /*  1240 */   195,  126,  360,  300,  667,  689,  128,   63,  942,  375,
 /*  1250 */     6,  688,  345,  304,  129,  687,  283,   71,  139,  130,
 /*  1260 */   131,  284,  766,  661,  680,   94,  660,  248,  679,  659,
 /*  1270 */   404,  829,  406,  380,  896,   65,   21,  643,  378,  649,
 /*  1280 */   888,  221,  393,  623,  620,  397,  309,  178,  280,  218,
 /*  1290 */   220,  409,  311,  410,  625,  624,  621,  123,  843,  841,
 /*  1300 */   180,  727,  253,  765,  728,  125,  120,  255,  127,  115,
 /*  1310 */   726,  185,  257,  699,  107,  201,  269,  725,  259,  116,
 /*  1320 */   270,  709,  852,  271,  205,  133,  272,  815,  326,  132,
 /*  1330 */   860,  208,  134,  136,  919,   56,  209,  210,   57,   58,
 /*  1340 */    59,  863,  121,  191,  859,  189,    8,   12,  146,  192,
 /*  1350 */   237,  633,  346,  196,  140,  250,  350,   60,   13,  355,
 /*  1360 */   678,  244,   14,   61,  203,  784,  783,  707,  118,  813,
 /*  1370 */    62,  169,   15,  204,  711,    4,  385,  368,  170,  172,
 /*  1380 */   141,  733,  828,   16,  738,   69,   66,   17,  814,  812,
 /*  1390 */   869,  817,  402,  165,  386,  868,  880,  148,  392,  213,
 /*  1400 */   881,  212,  149,  396,  816,  150,  617,  782,  652,  644,
 /*  1410 */    79,  276,  634, 1187,  924,  924,  924,  924,  924,  924,
 /*  1420 */   306,  405,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*    10 */    15,   16,   17,   18,   19,   20,   50,   22,   23,   24,
 /*    20 */    25,   26,   27,   28,   29,   30,   31,   32,   17,   18,
 /*    30 */    19,   20,   21,   22,   23,   24,   25,   26,   27,   28,
 /*    40 */    29,   30,   31,   32,   49,   32,   51,   17,   18,   19,
 /*    50 */    20,  143,   22,   23,   24,   25,   26,   27,   28,   29,
 /*    60 */    30,   31,   32,   26,   27,   28,   29,   30,   31,   32,
 /*    70 */    75,   28,   29,   30,   31,   32,    5,    6,    7,    8,
 /*    80 */     9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
 /*    90 */    19,   20,  126,   22,   23,   24,   25,   26,   27,   28,
 /*   100 */    29,   30,   31,   32,    5,    6,    7,    8,    9,   10,
 /*   110 */    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
 /*   120 */   170,   22,   23,   24,   25,   26,   27,   28,   29,   30,
 /*   130 */    31,   32,   69,    5,    6,    7,    8,    9,   10,   11,
 /*   140 */    12,   13,   14,   15,   16,   17,   18,   19,   20,   50,
 /*   150 */    22,   23,   24,   25,   26,   27,   28,   29,   30,   31,
 /*   160 */    32,   98,   99,  100,  161,   83,   67,   85,   86,   87,
 /*   170 */    88,  146,  147,  148,  111,  112,   48,   76,   77,   78,
 /*   180 */    79,   80,   81,   82,   83,   84,   85,   86,   87,   88,
 /*   190 */     1,    2,  121,  122,  220,  221,  222,  194,    5,    6,
 /*   200 */     7,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   210 */    17,   18,   19,   20,   69,   22,   23,   24,   25,   26,
 /*   220 */    27,   28,   29,   30,   31,   32,   22,   23,   24,   25,
 /*   230 */    26,   27,   28,   29,   30,   31,   32,  227,   51,   52,
 /*   240 */   153,   48,  232,   98,   99,  100,  170,    5,    6,    7,
 /*   250 */     8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
 /*   260 */    18,   19,   20,   39,   22,   23,   24,   25,   26,   27,
 /*   270 */    28,   29,   30,   31,   32,  170,   89,   53,  191,   92,
 /*   280 */    93,   94,  179,  173,  169,   61,   62,    7,  143,    9,
 /*   290 */    48,  104,  177,   69,  183,  173,    5,    6,    7,    8,
 /*   300 */     9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
 /*   310 */    19,   20,  208,   22,   23,   24,   25,   26,   27,   28,
 /*   320 */    29,   30,   31,   32,  220,  221,  222,  193,  149,  150,
 /*   330 */   220,  221,  222,   47,  155,   49,  157,   51,  204,   48,
 /*   340 */   161,  167,  220,  221,  222,    5,    6,    7,    8,    9,
 /*   350 */    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
 /*   360 */    20,   75,   22,   23,   24,   25,   26,   27,   28,   29,
 /*   370 */    30,   31,   32,  194,   73,  153,   75,   97,   77,   98,
 /*   380 */    99,  100,  102,   56,   83,   58,   85,  164,   48,  166,
 /*   390 */   167,   64,   91,  179,    5,    6,    7,    8,    9,   10,
 /*   400 */    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
 /*   410 */   179,   22,   23,   24,   25,   26,   27,   28,   29,   30,
 /*   420 */    31,   32,    5,    6,    7,    8,    9,   10,   11,   12,
 /*   430 */    13,   14,   15,   16,   17,   18,   19,   20,  216,   22,
 /*   440 */    23,   24,   25,   26,   27,   28,   29,   30,   31,   32,
 /*   450 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*   460 */    15,   16,   17,   18,   19,   20,  211,   22,   23,   24,
 /*   470 */    25,   26,   27,   28,   29,   30,   31,   32,    5,    6,
 /*   480 */     7,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   490 */    17,   18,   19,   20,   49,   22,   23,   24,   25,   26,
 /*   500 */    27,   28,   29,   30,   31,   32,    6,    7,    8,    9,
 /*   510 */    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
 /*   520 */    20,  132,   22,   23,   24,   25,   26,   27,   28,   29,
 /*   530 */    30,   31,   32,  164,  153,  166,  167,  208,   34,  111,
 /*   540 */   112,   37,   38,   39,   40,   41,   31,   32,   44,  132,
 /*   550 */   215,    9,   10,   11,   12,    5,    6,    7,    8,    9,
 /*   560 */    10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
 /*   570 */    20,  143,   22,   23,   24,   25,   26,   27,   28,   29,
 /*   580 */    30,   31,   32,    7,    8,    9,   10,   11,   12,   13,
 /*   590 */    14,   15,   16,   17,   18,   19,   20,    7,   22,   23,
 /*   600 */    24,   25,   26,   27,   28,   29,   30,   31,   32,  105,
 /*   610 */    89,  161,    7,   92,   93,   94,   26,   27,   98,   99,
 /*   620 */   100,  161,  241,   33,   90,  104,  161,   26,   27,  215,
 /*   630 */    26,   27,  153,   91,  153,   43,   46,   47,   48,  135,
 /*   640 */   153,   51,   52,  153,  194,  141,  142,  143,   47,  170,
 /*   650 */   171,  153,   51,   52,  194,   51,   52,   52,   68,  194,
 /*   660 */   170,  171,   38,   73,  153,   75,  189,   77,  218,  209,
 /*   670 */   210,    7,  191,   83,   73,   85,   75,  143,   77,   75,
 /*   680 */   153,   91,  153,  233,   83,   93,   85,   83,  238,   85,
 /*   690 */    26,   27,   91,   69,   90,  205,  206,   33,  106,  170,
 /*   700 */   171,   97,  191,   99,  217,   49,  116,   51,  153,  153,
 /*   710 */    46,   47,  153,  184,  216,   51,   52,  161,  128,  129,
 /*   720 */   130,  235,   98,   99,  100,  153,  170,  171,  136,  170,
 /*   730 */   171,   75,   68,  143,  153,  161,  153,   73,  153,   75,
 /*   740 */   153,   77,  161,  184,  217,    7,  191,   83,  234,   85,
 /*   750 */   194,  170,  171,  170,  171,   91,  153,  192,  193,  153,
 /*   760 */   153,  205,  206,  191,   26,   27,   28,  143,  194,  204,
 /*   770 */   153,   33,  153,  170,  171,  194,  191,  170,  171,  153,
 /*   780 */   116,  153,  153,  218,   46,   47,  205,  153,  205,   51,
 /*   790 */    52,  184,  128,  129,  130,  153,  170,  171,  170,  171,
 /*   800 */   153,   48,  153,   50,  170,  171,   68,  143,  205,  206,
 /*   810 */   153,   73,  153,   75,  153,   77,  153,  170,  171,    7,
 /*   820 */   191,   83,  197,   85,  208,  153,  239,  170,  171,   91,
 /*   830 */   208,  205,  207,  205,  206,  229,  230,  231,   26,   27,
 /*   840 */   191,  184,  170,  171,  153,   33,  229,  230,  231,   50,
 /*   850 */   161,  153,  205,  206,  116,   51,   52,  153,   46,   47,
 /*   860 */   162,  170,  171,   51,   52,  153,  128,  129,  130,  116,
 /*   870 */   117,   43,  192,  193,  170,  171,  153,   51,   52,  181,
 /*   880 */    68,  183,  153,  194,  204,   73,   39,   75,   59,   77,
 /*   890 */    51,   52,  208,    7,   65,   83,  224,   85,  239,   70,
 /*   900 */    53,    7,  239,   91,  153,  153,   47,  218,   61,   62,
 /*   910 */    51,   52,   26,   27,  191,  116,  117,  195,  153,   33,
 /*   920 */   116,  117,  170,  171,   51,   52,   32,  238,  116,  153,
 /*   930 */   101,  153,   46,   47,  106,  170,  171,   51,   52,  196,
 /*   940 */   128,  129,  130,  177,  178,  216,  170,  171,  170,  171,
 /*   950 */   153,  153,  186,  241,   68,  116,  117,  159,  153,   73,
 /*   960 */   162,   75,  134,   77,  136,  111,  112,  170,  171,   83,
 /*   970 */   144,   85,  161,  153,   47,  170,  171,   91,   51,   52,
 /*   980 */   188,  189,    7,   89,   90,   91,   92,   93,   94,   95,
 /*   990 */   170,  171,  199,  153,   47,  153,  102,  153,   51,   52,
 /*  1000 */   212,  153,  116,   50,  153,  194,  153,   28,  153,  153,
 /*  1010 */   170,  171,  170,  171,  128,  129,  130,  153,  170,  171,
 /*  1020 */   153,  170,  171,  170,  171,  170,  171,   52,  153,  185,
 /*  1030 */    51,   52,    9,  153,  170,  171,   91,  170,  171,  212,
 /*  1040 */   153,  185,  153,  240,  153,  170,  171,  102,  153,   96,
 /*  1050 */   170,  171,   48,  153,   50,  153,  153,  170,  171,  170,
 /*  1060 */   171,  170,  171,  153,   47,  170,  171,  153,   51,   52,
 /*  1070 */   170,  171,  170,  171,  153,   28,  153,  161,  188,  189,
 /*  1080 */   170,  171,  153,  153,  170,  171,  153,  153,  153,   50,
 /*  1090 */   153,  170,  171,  170,  171,  153,  131,  161,  133,  170,
 /*  1100 */   171,  153,  153,  170,  171,  170,  171,  170,  171,  153,
 /*  1110 */   194,  161,  170,  171,   47,  153,   49,  153,  170,  171,
 /*  1120 */    97,  153,  107,  153,  109,  110,  170,  171,  209,  210,
 /*  1130 */   194,   16,  170,  171,  170,  171,   51,   52,  170,  171,
 /*  1140 */   170,  171,  121,  122,  194,   83,  216,   85,   90,   91,
 /*  1150 */   216,  108,   51,   52,  115,  108,  113,   83,   36,   85,
 /*  1160 */   113,   48,   47,   50,   48,   48,   50,   50,   48,   48,
 /*  1170 */    50,   50,   48,   48,   50,   50,   48,   48,   50,   50,
 /*  1180 */    48,   47,   50,  153,   51,   52,  153,  153,  153,  199,
 /*  1190 */   153,  240,  174,  153,  153,  153,  182,  153,  153,  153,
 /*  1200 */   178,  153,  153,  153,  173,  153,  153,  212,  153,  226,
 /*  1210 */   153,  173,  153,  153,  198,  196,  153,  156,  153,  153,
 /*  1220 */    47,  153,  219,    5,  173,  103,  126,  199,   45,  131,
 /*  1230 */   237,  138,  236,   45,  158,  199,  158,   47,  158,  219,
 /*  1240 */   158,  187,   97,   63,   96,  172,  190,   97,  114,  119,
 /*  1250 */    47,  172,  175,   32,  190,  172,  175,  131,  187,  190,
 /*  1260 */   190,  175,  187,  172,  180,  125,  174,  172,  180,  172,
 /*  1270 */    47,  199,   47,  120,  172,  124,   50,  165,  123,  168,
 /*  1280 */    40,   35,  175,   36,    4,  175,    3,   42,  151,  154,
 /*  1290 */   154,  160,   72,  152,  152,  152,  152,   43,   48,   48,
 /*  1300 */   114,  214,  213,  112,  214,  127,  101,  213,  115,  163,
 /*  1310 */   214,   97,  213,   46,  176,  176,  202,  214,  213,  163,
 /*  1320 */   201,  203,  137,  200,   50,   97,  199,  223,  139,  137,
 /*  1330 */     1,  225,  115,  127,  140,   16,  228,  228,   16,   16,
 /*  1340 */    16,   52,  101,  114,    1,  118,   34,   47,   49,   97,
 /*  1350 */   134,   46,    7,   95,   47,  102,   66,   47,   47,   66,
 /*  1360 */    54,   48,   47,   47,   66,   48,   48,  108,   60,   48,
 /*  1370 */    50,  114,   47,  118,   48,   47,   75,   50,   48,   48,
 /*  1380 */    47,  117,   48,  118,   52,   50,   50,  118,   48,   48,
 /*  1390 */    48,   38,   83,   47,   50,   48,   48,   47,   49,  114,
 /*  1400 */    48,   50,   47,   49,   48,   47,    1,   48,   48,   48,
 /*  1410 */    47,   42,   48,    0,  242,  242,  242,  242,  242,  242,
 /*  1420 */    83,   83,
};
#define YY_SHIFT_USE_DFLT (1422)
#define YY_SHIFT_COUNT    (412)
#define YY_SHIFT_MIN      (-92)
#define YY_SHIFT_MAX      (1413)
static const short yy_shift_ofst[] = {
 /*     0 */   189,  590,  664,  504,  812,  812,  812,  812,  145,   -5,
 /*    10 */    71,   71,  812,  812,  812,  812,  812,  812,  812,  604,
 /*    20 */   604,  187,   63,  624,  428,   99,  128,  193,  242,  291,
 /*    30 */   340,  389,  417,  445,  473,  473,  473,  473,  473,  473,
 /*    40 */   473,  473,  473,  473,  473,  473,  473,  473,  473,  550,
 /*    50 */   473,  500,  576,  576,  738,  812,  812,  812,  812,  812,
 /*    60 */   812,  812,  812,  812,  812,  812,  812,  812,  812,  812,
 /*    70 */   812,  812,  812,  812,  812,  812,  812,  812,  812,  812,
 /*    80 */   812,  812,  886,  812,  812,  812,  812,  812,  812,  812,
 /*    90 */   812,  812,  812,  812,  812,  812,   11,   30,   30,   30,
 /*   100 */    30,   30,  101,  101,  204,   37,   43,  280,  515,  854,
 /*   110 */   873,   13, 1422, 1422, 1422,  894,  894,  224,  224,  592,
 /*   120 */   859,  859,  826,  873,  534,  873,  873,  873,  873,  873,
 /*   130 */   873,  873,  873,  873,  873,  873,  873,  873,  873,  873,
 /*   140 */   873,  873,  873,  873,  854,  -92,  -92,  -92,  -92,  -92,
 /*   150 */   -92, 1422, 1422,  601,  301,  301,  521,   82,  829,  829,
 /*   160 */   829,  753,  286,  804,  839,  847,  281,  327,  927,  979,
 /*   170 */   656,  656,  656,  947,  799, 1017, 1015, 1047,  873,  873,
 /*   180 */   873,  873,  -34,  605,  605,  873,  873,  975,  -34,  873,
 /*   190 */   975,  873,  873,  873,  873,  873,  873,  953,  873, 1004,
 /*   200 */   873, 1023,  873, 1021,  873,  873,  605,  873,  965, 1021,
 /*   210 */  1021,  873,  873,  873, 1039, 1043,  873, 1067,  873,  873,
 /*   220 */   873,  873, 1173, 1218, 1100, 1183, 1183, 1183, 1183, 1098,
 /*   230 */  1093, 1188, 1100, 1173, 1218, 1218, 1100, 1188, 1190, 1188,
 /*   240 */  1188, 1190, 1145, 1145, 1145, 1180, 1190, 1145, 1148, 1145,
 /*   250 */  1180, 1145, 1145, 1130, 1150, 1130, 1150, 1130, 1150, 1130,
 /*   260 */  1150, 1203, 1126, 1190, 1221, 1221, 1190, 1223, 1225, 1140,
 /*   270 */  1153, 1151, 1155, 1100, 1226, 1240, 1240, 1246, 1246, 1246,
 /*   280 */  1246, 1247, 1422, 1422, 1422, 1422,  542,  828,  520, 1062,
 /*   290 */  1074, 1115, 1113, 1116, 1117, 1120, 1121, 1124, 1085, 1058,
 /*   300 */  1122,  945, 1125, 1128, 1101, 1129, 1132, 1133, 1134, 1280,
 /*   310 */  1283, 1245, 1220, 1254, 1250, 1251, 1186, 1191, 1178, 1205,
 /*   320 */  1193, 1214, 1267, 1185, 1274, 1192, 1194, 1189, 1228, 1329,
 /*   330 */  1217, 1206, 1319, 1322, 1323, 1324, 1241, 1289, 1227, 1229,
 /*   340 */  1343, 1312, 1300, 1252, 1216, 1299, 1305, 1345, 1253, 1258,
 /*   350 */  1307, 1290, 1310, 1311, 1313, 1315, 1293, 1306, 1316, 1298,
 /*   360 */  1308, 1317, 1318, 1321, 1320, 1259, 1325, 1326, 1328, 1327,
 /*   370 */  1257, 1330, 1331, 1332, 1255, 1333, 1264, 1335, 1265, 1336,
 /*   380 */  1269, 1334, 1335, 1340, 1341, 1342, 1301, 1344, 1347, 1346,
 /*   390 */  1353, 1348, 1350, 1349, 1351, 1352, 1355, 1354, 1351, 1356,
 /*   400 */  1358, 1359, 1360, 1309, 1337, 1361, 1338, 1363, 1285, 1364,
 /*   410 */  1369, 1405, 1413,
};
#define YY_REDUCE_USE_DFLT (-51)
#define YY_REDUCE_COUNT (285)
#define YY_REDUCE_MIN   (-50)
#define YY_REDUCE_MAX   (1156)
static const short yy_reduce_ofst[] = {
 /*     0 */    25,  556,  581,  179,  490,  603,  628,  647,  450,  104,
 /*    10 */   110,  122,  529,  559,  607,  583,  626,  657,  672,  606,
 /*    20 */   617,  698,  565,  689,  460,  -26,  -26,  -26,  -26,  -26,
 /*    30 */   -26,  -26,  -26,  -26,  -26,  -26,  -26,  -26,  -26,  -26,
 /*    40 */   -26,  -26,  -26,  -26,  -26,  -26,  -26,  -26,  -26,  -26,
 /*    50 */   -26,  -26,  -26,  -26,  479,  634,  691,  704,  752,  765,
 /*    60 */   776,  778,  797,  805,  820,  840,  842,  848,  851,  853,
 /*    70 */   855,  864,  867,  875,  880,  887,  889,  891,  895,  900,
 /*    80 */   902,  910,  914,  921,  923,  929,  933,  935,  937,  942,
 /*    90 */   948,  956,  962,  964,  968,  970,  -26,  -26,  -26,  -26,
 /*   100 */   -26,  -26,  223,  369,  -26,  -26,  -26,  766,  -26,  680,
 /*   110 */   798,  -26,  -26,  -26,  -26,  115,  115,  792,  890,   10,
 /*   120 */   487,  527,  381,   87,    3,  481,  511,  555,  572,  585,
 /*   130 */   629,  649,  222,  723,  587,  498,  659,  729,  930,  663,
 /*   140 */   844,  934,  712,  856,  134,  465,  574,  811,  916,  936,
 /*   150 */   950,  919,  625,  -50,   76,  105,  111,  174,  103,  214,
 /*   160 */   231,  255,  329,  335,  414,  477,  486,  514,  619,  642,
 /*   170 */   616,  622,  684,  661,  255,  751,  722,  743,  903,  949,
 /*   180 */  1030, 1033,  793,  788,  827, 1034, 1035,  803,  990, 1037,
 /*   190 */   951, 1040, 1041, 1042, 1044, 1045, 1046, 1018, 1048, 1014,
 /*   200 */  1049, 1022, 1050, 1031, 1052, 1053,  995, 1055,  983, 1038,
 /*   210 */  1051, 1057, 1059,  642, 1016, 1019, 1060, 1061, 1063, 1065,
 /*   220 */  1066, 1068, 1003, 1054, 1028, 1056, 1064, 1069, 1070,  993,
 /*   230 */   996, 1076, 1036, 1020, 1071, 1075, 1072, 1078, 1077, 1080,
 /*   240 */  1082, 1081, 1073, 1079, 1083, 1084, 1086, 1091, 1092, 1095,
 /*   250 */  1088, 1097, 1102, 1087, 1089, 1090, 1094, 1096, 1099, 1103,
 /*   260 */  1105, 1104, 1106, 1107, 1108, 1109, 1110, 1111, 1112, 1118,
 /*   270 */  1114, 1119, 1123, 1127, 1131, 1135, 1136, 1141, 1142, 1143,
 /*   280 */  1144, 1137, 1146, 1138, 1139, 1156,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */  1188, 1182, 1182, 1182, 1129, 1129, 1129, 1129, 1182, 1025,
 /*    10 */  1052, 1052, 1227, 1227, 1227, 1227, 1227, 1227, 1128, 1227,
 /*    20 */  1227, 1227, 1227, 1182, 1029, 1058, 1227, 1227, 1227, 1130,
 /*    30 */  1131, 1227, 1227, 1227, 1163, 1068, 1067, 1066, 1065, 1039,
 /*    40 */  1063, 1056, 1060, 1130, 1124, 1125, 1123, 1127, 1131, 1227,
 /*    50 */  1059, 1093, 1108, 1092, 1227, 1227, 1227, 1227, 1227, 1227,
 /*    60 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*    70 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*    80 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*    90 */  1227, 1227, 1227, 1227, 1227, 1227, 1102, 1107, 1114, 1106,
 /*   100 */  1103, 1095, 1227, 1227, 1094, 1096, 1097,  996, 1098, 1227,
 /*   110 */  1227, 1099, 1111, 1110, 1109, 1197, 1196, 1227, 1227, 1136,
 /*   120 */  1227, 1227, 1227, 1227, 1182, 1227, 1227, 1227, 1227, 1227,
 /*   130 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*   140 */  1227, 1227, 1227, 1227, 1227, 1182, 1182, 1182, 1182, 1182,
 /*   150 */  1182, 1029, 1020, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*   160 */  1227, 1227, 1025, 1227, 1227, 1227, 1227, 1158, 1227, 1227,
 /*   170 */  1025, 1025, 1025, 1227, 1027, 1227, 1009, 1019, 1227, 1179,
 /*   180 */  1227, 1150, 1062, 1041, 1041, 1227, 1227, 1226, 1062, 1227,
 /*   190 */  1226, 1227, 1227, 1227, 1227, 1227, 1227,  971, 1227, 1205,
 /*   200 */  1227,  968, 1227, 1052, 1227, 1227, 1041, 1227, 1126, 1052,
 /*   210 */  1052, 1227, 1227, 1227, 1026, 1019, 1227, 1227, 1227, 1227,
 /*   220 */  1227, 1191, 1073,  999, 1062, 1005, 1005, 1005, 1005, 1162,
 /*   230 */  1223,  937, 1062, 1073,  999,  999, 1062,  937, 1137,  937,
 /*   240 */   937, 1137,  997,  997,  997,  986, 1137,  997,  971,  997,
 /*   250 */   986,  997,  997, 1045, 1040, 1045, 1040, 1045, 1040, 1045,
 /*   260 */  1040, 1132, 1227, 1137, 1141, 1141, 1137,  955, 1227, 1057,
 /*   270 */  1046, 1055, 1053, 1062,  989, 1194, 1194, 1190, 1190, 1190,
 /*   280 */  1190,  927, 1200,  973,  973, 1200, 1227, 1227, 1227, 1227,
 /*   290 */  1227, 1144, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*   300 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1079, 1227,
 /*   310 */   924, 1227, 1227, 1227, 1227, 1227, 1218, 1227, 1227, 1227,
 /*   320 */  1227, 1227, 1227, 1227, 1161, 1160, 1227, 1227, 1227, 1227,
 /*   330 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1225,
 /*   340 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*   350 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*   360 */  1227, 1227, 1227, 1227, 1227, 1011, 1227, 1227, 1227, 1209,
 /*   370 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1054, 1227, 1047,
 /*   380 */  1227, 1227, 1215, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*   390 */  1227, 1227, 1227, 1227, 1184, 1227, 1227, 1227, 1183, 1227,
 /*   400 */  1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227, 1227,
 /*   410 */   931, 1227, 1227,
};
/********** End of lemon-generated parsing tables *****************************/

/* The next table maps tokens (terminal symbols) into fallback tokens.  
** If a construct like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
**
** This feature can be used, for example, to cause some keywords in a language
** to revert to identifiers if they keyword does not apply in the context where
** it appears.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
    0,  /*          $ => nothing */
    0,  /*       SEMI => nothing */
    0,  /*    EXPLAIN => nothing */
   51,  /*      QUERY => ID */
   51,  /*       PLAN => ID */
    0,  /*         OR => nothing */
    0,  /*        AND => nothing */
    0,  /*        NOT => nothing */
    0,  /*         IS => nothing */
   51,  /*      MATCH => ID */
    0,  /*    LIKE_KW => nothing */
    0,  /*    BETWEEN => nothing */
    0,  /*         IN => nothing */
   51,  /*     ISNULL => ID */
   51,  /*    NOTNULL => ID */
    0,  /*         NE => nothing */
    0,  /*         EQ => nothing */
    0,  /*         GT => nothing */
    0,  /*         LE => nothing */
    0,  /*         LT => nothing */
    0,  /*         GE => nothing */
    0,  /*     ESCAPE => nothing */
    0,  /*     BITAND => nothing */
    0,  /*      BITOR => nothing */
    0,  /*     LSHIFT => nothing */
    0,  /*     RSHIFT => nothing */
    0,  /*       PLUS => nothing */
    0,  /*      MINUS => nothing */
    0,  /*       STAR => nothing */
    0,  /*      SLASH => nothing */
    0,  /*        REM => nothing */
    0,  /*     CONCAT => nothing */
    0,  /*    COLLATE => nothing */
    0,  /*     BITNOT => nothing */
    0,  /*      BEGIN => nothing */
    0,  /* TRANSACTION => nothing */
   51,  /*   DEFERRED => ID */
    0,  /*     COMMIT => nothing */
   51,  /*        END => ID */
    0,  /*   ROLLBACK => nothing */
    0,  /*  SAVEPOINT => nothing */
   51,  /*    RELEASE => ID */
    0,  /*         TO => nothing */
    0,  /*      TABLE => nothing */
    0,  /*     CREATE => nothing */
   51,  /*         IF => ID */
    0,  /*     EXISTS => nothing */
    0,  /*         LP => nothing */
    0,  /*         RP => nothing */
    0,  /*         AS => nothing */
    0,  /*      COMMA => nothing */
    0,  /*         ID => nothing */
    0,  /*    INDEXED => nothing */
   51,  /*      ABORT => ID */
   51,  /*     ACTION => ID */
   51,  /*        ADD => ID */
   51,  /*      AFTER => ID */
   51,  /* AUTOINCREMENT => ID */
   51,  /*     BEFORE => ID */
   51,  /*    CASCADE => ID */
   51,  /*   CONFLICT => ID */
   51,  /*       FAIL => ID */
   51,  /*     IGNORE => ID */
   51,  /*  INITIALLY => ID */
   51,  /*    INSTEAD => ID */
   51,  /*         NO => ID */
   51,  /*        KEY => ID */
   51,  /*     OFFSET => ID */
   51,  /*      RAISE => ID */
   51,  /*    REPLACE => ID */
   51,  /*   RESTRICT => ID */
   51,  /*    REINDEX => ID */
   51,  /*     RENAME => ID */
   51,  /*   CTIME_KW => ID */
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
**
** After the "shift" half of a SHIFTREDUCE action, the stateno field
** actually contains the reduce action for the second half of the
** SHIFTREDUCE.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number, or reduce action in SHIFTREDUCE */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  yyStackEntry *yytos;          /* Pointer to top element of the stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyhwm;                    /* High-water mark of the stack */
#endif
#ifndef YYNOERRORRECOVERY
  int yyerrcnt;                 /* Shifts left before out of the error */
#endif
  bool is_fallback_failed;      /* Shows if fallback failed or not */
  sqlite3ParserARG_SDECL                /* A place to hold %extra_argument */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
  yyStackEntry yystk0;          /* First stack entry */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3ParserTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "SEMI",          "EXPLAIN",       "QUERY",       
  "PLAN",          "OR",            "AND",           "NOT",         
  "IS",            "MATCH",         "LIKE_KW",       "BETWEEN",     
  "IN",            "ISNULL",        "NOTNULL",       "NE",          
  "EQ",            "GT",            "LE",            "LT",          
  "GE",            "ESCAPE",        "BITAND",        "BITOR",       
  "LSHIFT",        "RSHIFT",        "PLUS",          "MINUS",       
  "STAR",          "SLASH",         "REM",           "CONCAT",      
  "COLLATE",       "BITNOT",        "BEGIN",         "TRANSACTION", 
  "DEFERRED",      "COMMIT",        "END",           "ROLLBACK",    
  "SAVEPOINT",     "RELEASE",       "TO",            "TABLE",       
  "CREATE",        "IF",            "EXISTS",        "LP",          
  "RP",            "AS",            "COMMA",         "ID",          
  "INDEXED",       "ABORT",         "ACTION",        "ADD",         
  "AFTER",         "AUTOINCREMENT",  "BEFORE",        "CASCADE",     
  "CONFLICT",      "FAIL",          "IGNORE",        "INITIALLY",   
  "INSTEAD",       "NO",            "KEY",           "OFFSET",      
  "RAISE",         "REPLACE",       "RESTRICT",      "REINDEX",     
  "RENAME",        "CTIME_KW",      "ANY",           "STRING",      
  "TEXT",          "BLOB",          "DATE",          "TIME",        
  "DATETIME",      "CHAR",          "VARCHAR",       "INTEGER",     
  "UNSIGNED",      "FLOAT",         "INT",           "DECIMAL",     
  "NUMERIC",       "CONSTRAINT",    "DEFAULT",       "NULL",        
  "PRIMARY",       "UNIQUE",        "CHECK",         "REFERENCES",  
  "AUTOINCR",      "ON",            "INSERT",        "DELETE",      
  "UPDATE",        "SET",           "DEFERRABLE",    "IMMEDIATE",   
  "FOREIGN",       "DROP",          "VIEW",          "UNION",       
  "ALL",           "EXCEPT",        "INTERSECT",     "SELECT",      
  "VALUES",        "DISTINCT",      "DOT",           "FROM",        
  "JOIN_KW",       "JOIN",          "BY",            "USING",       
  "ORDER",         "ASC",           "DESC",          "GROUP",       
  "HAVING",        "LIMIT",         "WHERE",         "INTO",        
  "VARIABLE",      "CAST",          "CASE",          "WHEN",        
  "THEN",          "ELSE",          "INDEX",         "PRAGMA",      
  "TRIGGER",       "OF",            "FOR",           "EACH",        
  "ROW",           "ANALYZE",       "ALTER",         "WITH",        
  "RECURSIVE",     "error",         "input",         "ecmd",        
  "explain",       "cmdx",          "cmd",           "transtype",   
  "trans_opt",     "nm",            "savepoint_opt",  "create_table",
  "create_table_args",  "createkw",      "ifnotexists",   "columnlist",  
  "conslist_opt",  "select",        "columnname",    "carglist",    
  "typedef",       "charlengthtypedef",  "numbertypedef",  "unsignednumbertypedef",
  "numlengthtypedef",  "ccons",         "term",          "expr",        
  "onconf",        "sortorder",     "autoinc",       "eidlist_opt", 
  "refargs",       "defer_subclause",  "refarg",        "refact",      
  "init_deferred_pred_opt",  "conslist",      "tconscomma",    "tcons",       
  "sortlist",      "eidlist",       "defer_subclause_opt",  "orconf",      
  "resolvetype",   "raisetype",     "ifexists",      "fullname",    
  "selectnowith",  "oneselect",     "with",          "multiselect_op",
  "distinct",      "selcollist",    "from",          "where_opt",   
  "groupby_opt",   "having_opt",    "orderby_opt",   "limit_opt",   
  "values",        "nexprlist",     "exprlist",      "sclp",        
  "as",            "seltablist",    "stl_prefix",    "joinop",      
  "indexed_opt",   "on_opt",        "using_opt",     "join_nm",     
  "idlist",        "setlist",       "insert_cmd",    "idlist_opt",  
  "likeop",        "between_op",    "in_op",         "paren_exprlist",
  "case_operand",  "case_exprlist",  "case_else",     "uniqueflag",  
  "collate",       "nmnum",         "minus_num",     "plus_num",    
  "trigger_decl",  "trigger_cmd_list",  "trigger_time",  "trigger_event",
  "foreach_clause",  "when_clause",   "trigger_cmd",   "trnm",        
  "tridxby",       "wqlist",      
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "ecmd ::= explain cmdx SEMI",
 /*   1 */ "ecmd ::= SEMI",
 /*   2 */ "explain ::= EXPLAIN",
 /*   3 */ "explain ::= EXPLAIN QUERY PLAN",
 /*   4 */ "cmd ::= BEGIN transtype trans_opt",
 /*   5 */ "transtype ::=",
 /*   6 */ "transtype ::= DEFERRED",
 /*   7 */ "cmd ::= COMMIT trans_opt",
 /*   8 */ "cmd ::= END trans_opt",
 /*   9 */ "cmd ::= ROLLBACK trans_opt",
 /*  10 */ "cmd ::= SAVEPOINT nm",
 /*  11 */ "cmd ::= RELEASE savepoint_opt nm",
 /*  12 */ "cmd ::= ROLLBACK trans_opt TO savepoint_opt nm",
 /*  13 */ "create_table ::= createkw TABLE ifnotexists nm",
 /*  14 */ "createkw ::= CREATE",
 /*  15 */ "ifnotexists ::=",
 /*  16 */ "ifnotexists ::= IF NOT EXISTS",
 /*  17 */ "create_table_args ::= LP columnlist conslist_opt RP",
 /*  18 */ "create_table_args ::= AS select",
 /*  19 */ "columnname ::= nm typedef",
 /*  20 */ "nm ::= ID|INDEXED",
 /*  21 */ "typedef ::= TEXT",
 /*  22 */ "typedef ::= BLOB",
 /*  23 */ "typedef ::= DATE",
 /*  24 */ "typedef ::= TIME",
 /*  25 */ "typedef ::= DATETIME",
 /*  26 */ "typedef ::= CHAR|VARCHAR charlengthtypedef",
 /*  27 */ "charlengthtypedef ::= LP INTEGER RP",
 /*  28 */ "numbertypedef ::= unsignednumbertypedef",
 /*  29 */ "numbertypedef ::= UNSIGNED unsignednumbertypedef",
 /*  30 */ "unsignednumbertypedef ::= FLOAT",
 /*  31 */ "unsignednumbertypedef ::= INT|INTEGER",
 /*  32 */ "unsignednumbertypedef ::= DECIMAL|NUMERIC numlengthtypedef",
 /*  33 */ "numlengthtypedef ::=",
 /*  34 */ "numlengthtypedef ::= LP INTEGER RP",
 /*  35 */ "numlengthtypedef ::= LP INTEGER COMMA INTEGER RP",
 /*  36 */ "ccons ::= CONSTRAINT nm",
 /*  37 */ "ccons ::= DEFAULT term",
 /*  38 */ "ccons ::= DEFAULT LP expr RP",
 /*  39 */ "ccons ::= DEFAULT PLUS term",
 /*  40 */ "ccons ::= DEFAULT MINUS term",
 /*  41 */ "ccons ::= DEFAULT ID|INDEXED",
 /*  42 */ "ccons ::= NOT NULL onconf",
 /*  43 */ "ccons ::= PRIMARY KEY sortorder onconf autoinc",
 /*  44 */ "ccons ::= UNIQUE onconf",
 /*  45 */ "ccons ::= CHECK LP expr RP",
 /*  46 */ "ccons ::= REFERENCES nm eidlist_opt refargs",
 /*  47 */ "ccons ::= defer_subclause",
 /*  48 */ "ccons ::= COLLATE ID|INDEXED",
 /*  49 */ "autoinc ::=",
 /*  50 */ "autoinc ::= AUTOINCR",
 /*  51 */ "refargs ::=",
 /*  52 */ "refargs ::= refargs refarg",
 /*  53 */ "refarg ::= MATCH nm",
 /*  54 */ "refarg ::= ON INSERT refact",
 /*  55 */ "refarg ::= ON DELETE refact",
 /*  56 */ "refarg ::= ON UPDATE refact",
 /*  57 */ "refact ::= SET NULL",
 /*  58 */ "refact ::= SET DEFAULT",
 /*  59 */ "refact ::= CASCADE",
 /*  60 */ "refact ::= RESTRICT",
 /*  61 */ "refact ::= NO ACTION",
 /*  62 */ "defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt",
 /*  63 */ "defer_subclause ::= DEFERRABLE init_deferred_pred_opt",
 /*  64 */ "init_deferred_pred_opt ::=",
 /*  65 */ "init_deferred_pred_opt ::= INITIALLY DEFERRED",
 /*  66 */ "init_deferred_pred_opt ::= INITIALLY IMMEDIATE",
 /*  67 */ "conslist_opt ::=",
 /*  68 */ "tconscomma ::= COMMA",
 /*  69 */ "tcons ::= CONSTRAINT nm",
 /*  70 */ "tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf",
 /*  71 */ "tcons ::= UNIQUE LP sortlist RP onconf",
 /*  72 */ "tcons ::= CHECK LP expr RP onconf",
 /*  73 */ "tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt",
 /*  74 */ "defer_subclause_opt ::=",
 /*  75 */ "onconf ::=",
 /*  76 */ "onconf ::= ON CONFLICT resolvetype",
 /*  77 */ "orconf ::=",
 /*  78 */ "orconf ::= OR resolvetype",
 /*  79 */ "resolvetype ::= IGNORE",
 /*  80 */ "resolvetype ::= REPLACE",
 /*  81 */ "cmd ::= DROP TABLE ifexists fullname",
 /*  82 */ "ifexists ::= IF EXISTS",
 /*  83 */ "ifexists ::=",
 /*  84 */ "cmd ::= createkw VIEW ifnotexists nm eidlist_opt AS select",
 /*  85 */ "cmd ::= DROP VIEW ifexists fullname",
 /*  86 */ "cmd ::= select",
 /*  87 */ "select ::= with selectnowith",
 /*  88 */ "selectnowith ::= selectnowith multiselect_op oneselect",
 /*  89 */ "multiselect_op ::= UNION",
 /*  90 */ "multiselect_op ::= UNION ALL",
 /*  91 */ "multiselect_op ::= EXCEPT|INTERSECT",
 /*  92 */ "oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt",
 /*  93 */ "values ::= VALUES LP nexprlist RP",
 /*  94 */ "values ::= values COMMA LP exprlist RP",
 /*  95 */ "distinct ::= DISTINCT",
 /*  96 */ "distinct ::= ALL",
 /*  97 */ "distinct ::=",
 /*  98 */ "sclp ::=",
 /*  99 */ "selcollist ::= sclp expr as",
 /* 100 */ "selcollist ::= sclp STAR",
 /* 101 */ "selcollist ::= sclp nm DOT STAR",
 /* 102 */ "as ::= AS nm",
 /* 103 */ "as ::=",
 /* 104 */ "from ::=",
 /* 105 */ "from ::= FROM seltablist",
 /* 106 */ "stl_prefix ::= seltablist joinop",
 /* 107 */ "stl_prefix ::=",
 /* 108 */ "seltablist ::= stl_prefix nm as indexed_opt on_opt using_opt",
 /* 109 */ "seltablist ::= stl_prefix nm LP exprlist RP as on_opt using_opt",
 /* 110 */ "seltablist ::= stl_prefix LP select RP as on_opt using_opt",
 /* 111 */ "seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt",
 /* 112 */ "fullname ::= nm",
 /* 113 */ "joinop ::= COMMA|JOIN",
 /* 114 */ "joinop ::= JOIN_KW JOIN",
 /* 115 */ "joinop ::= JOIN_KW join_nm JOIN",
 /* 116 */ "joinop ::= JOIN_KW join_nm join_nm JOIN",
 /* 117 */ "on_opt ::= ON expr",
 /* 118 */ "on_opt ::=",
 /* 119 */ "indexed_opt ::=",
 /* 120 */ "indexed_opt ::= INDEXED BY nm",
 /* 121 */ "indexed_opt ::= NOT INDEXED",
 /* 122 */ "using_opt ::= USING LP idlist RP",
 /* 123 */ "using_opt ::=",
 /* 124 */ "orderby_opt ::=",
 /* 125 */ "orderby_opt ::= ORDER BY sortlist",
 /* 126 */ "sortlist ::= sortlist COMMA expr sortorder",
 /* 127 */ "sortlist ::= expr sortorder",
 /* 128 */ "sortorder ::= ASC",
 /* 129 */ "sortorder ::= DESC",
 /* 130 */ "sortorder ::=",
 /* 131 */ "groupby_opt ::=",
 /* 132 */ "groupby_opt ::= GROUP BY nexprlist",
 /* 133 */ "having_opt ::=",
 /* 134 */ "having_opt ::= HAVING expr",
 /* 135 */ "limit_opt ::=",
 /* 136 */ "limit_opt ::= LIMIT expr",
 /* 137 */ "limit_opt ::= LIMIT expr OFFSET expr",
 /* 138 */ "limit_opt ::= LIMIT expr COMMA expr",
 /* 139 */ "cmd ::= with DELETE FROM fullname indexed_opt where_opt",
 /* 140 */ "where_opt ::=",
 /* 141 */ "where_opt ::= WHERE expr",
 /* 142 */ "cmd ::= with UPDATE orconf fullname indexed_opt SET setlist where_opt",
 /* 143 */ "setlist ::= setlist COMMA nm EQ expr",
 /* 144 */ "setlist ::= setlist COMMA LP idlist RP EQ expr",
 /* 145 */ "setlist ::= nm EQ expr",
 /* 146 */ "setlist ::= LP idlist RP EQ expr",
 /* 147 */ "cmd ::= with insert_cmd INTO fullname idlist_opt select",
 /* 148 */ "cmd ::= with insert_cmd INTO fullname idlist_opt DEFAULT VALUES",
 /* 149 */ "insert_cmd ::= INSERT orconf",
 /* 150 */ "insert_cmd ::= REPLACE",
 /* 151 */ "idlist_opt ::=",
 /* 152 */ "idlist_opt ::= LP idlist RP",
 /* 153 */ "idlist ::= idlist COMMA nm",
 /* 154 */ "idlist ::= nm",
 /* 155 */ "expr ::= LP expr RP",
 /* 156 */ "term ::= NULL",
 /* 157 */ "expr ::= ID|INDEXED",
 /* 158 */ "expr ::= JOIN_KW",
 /* 159 */ "expr ::= nm DOT nm",
 /* 160 */ "term ::= FLOAT|BLOB",
 /* 161 */ "term ::= STRING",
 /* 162 */ "term ::= INTEGER",
 /* 163 */ "expr ::= VARIABLE",
 /* 164 */ "expr ::= expr COLLATE ID|INDEXED",
 /* 165 */ "expr ::= CAST LP expr AS typedef RP",
 /* 166 */ "expr ::= ID|INDEXED LP distinct exprlist RP",
 /* 167 */ "expr ::= ID|INDEXED LP STAR RP",
 /* 168 */ "term ::= CTIME_KW",
 /* 169 */ "expr ::= LP nexprlist COMMA expr RP",
 /* 170 */ "expr ::= expr AND expr",
 /* 171 */ "expr ::= expr OR expr",
 /* 172 */ "expr ::= expr LT|GT|GE|LE expr",
 /* 173 */ "expr ::= expr EQ|NE expr",
 /* 174 */ "expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr",
 /* 175 */ "expr ::= expr PLUS|MINUS expr",
 /* 176 */ "expr ::= expr STAR|SLASH|REM expr",
 /* 177 */ "expr ::= expr CONCAT expr",
 /* 178 */ "likeop ::= LIKE_KW|MATCH",
 /* 179 */ "likeop ::= NOT LIKE_KW|MATCH",
 /* 180 */ "expr ::= expr likeop expr",
 /* 181 */ "expr ::= expr likeop expr ESCAPE expr",
 /* 182 */ "expr ::= expr ISNULL|NOTNULL",
 /* 183 */ "expr ::= expr NOT NULL",
 /* 184 */ "expr ::= expr IS expr",
 /* 185 */ "expr ::= expr IS NOT expr",
 /* 186 */ "expr ::= NOT expr",
 /* 187 */ "expr ::= BITNOT expr",
 /* 188 */ "expr ::= MINUS expr",
 /* 189 */ "expr ::= PLUS expr",
 /* 190 */ "between_op ::= BETWEEN",
 /* 191 */ "between_op ::= NOT BETWEEN",
 /* 192 */ "expr ::= expr between_op expr AND expr",
 /* 193 */ "in_op ::= IN",
 /* 194 */ "in_op ::= NOT IN",
 /* 195 */ "expr ::= expr in_op LP exprlist RP",
 /* 196 */ "expr ::= LP select RP",
 /* 197 */ "expr ::= expr in_op LP select RP",
 /* 198 */ "expr ::= expr in_op nm paren_exprlist",
 /* 199 */ "expr ::= EXISTS LP select RP",
 /* 200 */ "expr ::= CASE case_operand case_exprlist case_else END",
 /* 201 */ "case_exprlist ::= case_exprlist WHEN expr THEN expr",
 /* 202 */ "case_exprlist ::= WHEN expr THEN expr",
 /* 203 */ "case_else ::= ELSE expr",
 /* 204 */ "case_else ::=",
 /* 205 */ "case_operand ::= expr",
 /* 206 */ "case_operand ::=",
 /* 207 */ "exprlist ::=",
 /* 208 */ "nexprlist ::= nexprlist COMMA expr",
 /* 209 */ "nexprlist ::= expr",
 /* 210 */ "paren_exprlist ::=",
 /* 211 */ "paren_exprlist ::= LP exprlist RP",
 /* 212 */ "cmd ::= createkw uniqueflag INDEX ifnotexists nm ON nm LP sortlist RP where_opt",
 /* 213 */ "uniqueflag ::= UNIQUE",
 /* 214 */ "uniqueflag ::=",
 /* 215 */ "eidlist_opt ::=",
 /* 216 */ "eidlist_opt ::= LP eidlist RP",
 /* 217 */ "eidlist ::= eidlist COMMA nm collate sortorder",
 /* 218 */ "eidlist ::= nm collate sortorder",
 /* 219 */ "collate ::=",
 /* 220 */ "collate ::= COLLATE ID|INDEXED",
 /* 221 */ "cmd ::= DROP INDEX ifexists fullname ON nm",
 /* 222 */ "cmd ::= PRAGMA nm",
 /* 223 */ "cmd ::= PRAGMA nm EQ nmnum",
 /* 224 */ "cmd ::= PRAGMA nm LP nmnum RP",
 /* 225 */ "cmd ::= PRAGMA nm EQ minus_num",
 /* 226 */ "cmd ::= PRAGMA nm LP minus_num RP",
 /* 227 */ "cmd ::= PRAGMA nm EQ nm DOT nm",
 /* 228 */ "cmd ::= PRAGMA",
 /* 229 */ "plus_num ::= PLUS INTEGER|FLOAT",
 /* 230 */ "minus_num ::= MINUS INTEGER|FLOAT",
 /* 231 */ "cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END",
 /* 232 */ "trigger_decl ::= TRIGGER ifnotexists nm trigger_time trigger_event ON fullname foreach_clause when_clause",
 /* 233 */ "trigger_time ::= BEFORE",
 /* 234 */ "trigger_time ::= AFTER",
 /* 235 */ "trigger_time ::= INSTEAD OF",
 /* 236 */ "trigger_time ::=",
 /* 237 */ "trigger_event ::= DELETE|INSERT",
 /* 238 */ "trigger_event ::= UPDATE",
 /* 239 */ "trigger_event ::= UPDATE OF idlist",
 /* 240 */ "when_clause ::=",
 /* 241 */ "when_clause ::= WHEN expr",
 /* 242 */ "trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI",
 /* 243 */ "trigger_cmd_list ::= trigger_cmd SEMI",
 /* 244 */ "trnm ::= nm DOT nm",
 /* 245 */ "tridxby ::= INDEXED BY nm",
 /* 246 */ "tridxby ::= NOT INDEXED",
 /* 247 */ "trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt",
 /* 248 */ "trigger_cmd ::= insert_cmd INTO trnm idlist_opt select",
 /* 249 */ "trigger_cmd ::= DELETE FROM trnm tridxby where_opt",
 /* 250 */ "trigger_cmd ::= select",
 /* 251 */ "expr ::= RAISE LP IGNORE RP",
 /* 252 */ "expr ::= RAISE LP raisetype COMMA STRING RP",
 /* 253 */ "raisetype ::= ROLLBACK",
 /* 254 */ "raisetype ::= ABORT",
 /* 255 */ "raisetype ::= FAIL",
 /* 256 */ "cmd ::= DROP TRIGGER ifexists fullname",
 /* 257 */ "cmd ::= ANALYZE",
 /* 258 */ "cmd ::= ANALYZE nm",
 /* 259 */ "cmd ::= ALTER TABLE fullname RENAME TO nm",
 /* 260 */ "with ::=",
 /* 261 */ "with ::= WITH wqlist",
 /* 262 */ "with ::= WITH RECURSIVE wqlist",
 /* 263 */ "wqlist ::= nm eidlist_opt AS LP select RP",
 /* 264 */ "wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP",
 /* 265 */ "input ::= ecmd",
 /* 266 */ "explain ::=",
 /* 267 */ "cmdx ::= cmd",
 /* 268 */ "trans_opt ::=",
 /* 269 */ "trans_opt ::= TRANSACTION",
 /* 270 */ "trans_opt ::= TRANSACTION nm",
 /* 271 */ "savepoint_opt ::= SAVEPOINT",
 /* 272 */ "savepoint_opt ::=",
 /* 273 */ "cmd ::= create_table create_table_args",
 /* 274 */ "columnlist ::= columnlist COMMA columnname carglist",
 /* 275 */ "columnlist ::= columnname carglist",
 /* 276 */ "typedef ::= numbertypedef",
 /* 277 */ "carglist ::= carglist ccons",
 /* 278 */ "carglist ::=",
 /* 279 */ "ccons ::= NULL onconf",
 /* 280 */ "conslist_opt ::= COMMA conslist",
 /* 281 */ "conslist ::= conslist tconscomma tcons",
 /* 282 */ "conslist ::= tcons",
 /* 283 */ "tconscomma ::=",
 /* 284 */ "defer_subclause_opt ::= defer_subclause",
 /* 285 */ "resolvetype ::= raisetype",
 /* 286 */ "selectnowith ::= oneselect",
 /* 287 */ "oneselect ::= values",
 /* 288 */ "sclp ::= selcollist COMMA",
 /* 289 */ "as ::= ID|STRING",
 /* 290 */ "join_nm ::= ID|INDEXED",
 /* 291 */ "join_nm ::= JOIN_KW",
 /* 292 */ "expr ::= term",
 /* 293 */ "exprlist ::= nexprlist",
 /* 294 */ "nmnum ::= plus_num",
 /* 295 */ "nmnum ::= STRING",
 /* 296 */ "nmnum ::= nm",
 /* 297 */ "nmnum ::= ON",
 /* 298 */ "nmnum ::= DELETE",
 /* 299 */ "nmnum ::= DEFAULT",
 /* 300 */ "plus_num ::= INTEGER|FLOAT",
 /* 301 */ "foreach_clause ::=",
 /* 302 */ "foreach_clause ::= FOR EACH ROW",
 /* 303 */ "trnm ::= nm",
 /* 304 */ "tridxby ::=",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.  Return the number
** of errors.  Return 0 on success.
*/
static int yyGrowStack(yyParser *p){
  int newSize;
  int idx;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  idx = p->yytos ? (int)(p->yytos - p->yystack) : 0;
  if( p->yystack==&p->yystk0 ){
    pNew = malloc(newSize*sizeof(pNew[0]));
    if( pNew ) pNew[0] = p->yystk0;
  }else{
    pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  }
  if( pNew ){
    p->yystack = pNew;
    p->yytos = &p->yystack[idx];
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows from %d to %d entries.\n",
              yyTracePrompt, p->yystksz, newSize);
    }
#endif
    p->yystksz = newSize;
  }
  return pNew==0; 
}
#endif

/* Datatype of the argument to the memory allocated passed as the
** second argument to sqlite3ParserAlloc() below.  This can be changed by
** putting an appropriate #define in the %include section of the input
** grammar.
*/
#ifndef YYMALLOCARGTYPE
# define YYMALLOCARGTYPE size_t
#endif

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to sqlite3Parser and sqlite3ParserFree.
*/
void *sqlite3ParserAlloc(void *(*mallocProc)(YYMALLOCARGTYPE)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (YYMALLOCARGTYPE)sizeof(yyParser) );
  if( pParser ){
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyhwm = 0;
    pParser->is_fallback_failed = false;
#endif
#if YYSTACKDEPTH<=0
    pParser->yytos = NULL;
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    if( yyGrowStack(pParser) ){
      pParser->yystack = &pParser->yystk0;
      pParser->yystksz = 1;
    }
#endif
#ifndef YYNOERRORRECOVERY
    pParser->yyerrcnt = -1;
#endif
    pParser->yytos = pParser->yystack;
    pParser->yystack[0].stateno = 0;
    pParser->yystack[0].major = 0;
  }
  return pParser;
}

/* The following function deletes the "minor type" or semantic value
** associated with a symbol.  The symbol can be either a terminal
** or nonterminal. "yymajor" is the symbol code, and "yypminor" is
** a pointer to the value to be deleted.  The code used to do the 
** deletions is derived from the %destructor and/or %token_destructor
** directives of the input grammar.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  sqlite3ParserARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are *not* used
    ** inside the C code.
    */
/********* Begin destructor definitions ***************************************/
    case 161: /* select */
    case 192: /* selectnowith */
    case 193: /* oneselect */
    case 204: /* values */
{
#line 410 "parse.y"
sqlite3SelectDelete(pParse->db, (yypminor->yy91));
#line 1501 "parse.c"
}
      break;
    case 170: /* term */
    case 171: /* expr */
{
#line 853 "parse.y"
sql_expr_free(pParse->db, (yypminor->yy258).pExpr, false);
#line 1509 "parse.c"
}
      break;
    case 175: /* eidlist_opt */
    case 184: /* sortlist */
    case 185: /* eidlist */
    case 197: /* selcollist */
    case 200: /* groupby_opt */
    case 202: /* orderby_opt */
    case 205: /* nexprlist */
    case 206: /* exprlist */
    case 207: /* sclp */
    case 217: /* setlist */
    case 223: /* paren_exprlist */
    case 225: /* case_exprlist */
{
#line 1299 "parse.y"
sqlite3ExprListDelete(pParse->db, (yypminor->yy322));
#line 1527 "parse.c"
}
      break;
    case 191: /* fullname */
    case 198: /* from */
    case 209: /* seltablist */
    case 210: /* stl_prefix */
{
#line 637 "parse.y"
sqlite3SrcListDelete(pParse->db, (yypminor->yy439));
#line 1537 "parse.c"
}
      break;
    case 194: /* with */
    case 241: /* wqlist */
{
#line 1550 "parse.y"
sqlite3WithDelete(pParse->db, (yypminor->yy323));
#line 1545 "parse.c"
}
      break;
    case 199: /* where_opt */
    case 201: /* having_opt */
    case 213: /* on_opt */
    case 224: /* case_operand */
    case 226: /* case_else */
    case 237: /* when_clause */
{
#line 762 "parse.y"
sql_expr_free(pParse->db, (yypminor->yy418), false);
#line 1557 "parse.c"
}
      break;
    case 214: /* using_opt */
    case 216: /* idlist */
    case 219: /* idlist_opt */
{
#line 674 "parse.y"
sqlite3IdListDelete(pParse->db, (yypminor->yy232));
#line 1566 "parse.c"
}
      break;
    case 233: /* trigger_cmd_list */
    case 238: /* trigger_cmd */
{
#line 1423 "parse.y"
sqlite3DeleteTriggerStep(pParse->db, (yypminor->yy451));
#line 1574 "parse.c"
}
      break;
    case 235: /* trigger_event */
{
#line 1409 "parse.y"
sqlite3IdListDelete(pParse->db, (yypminor->yy378).b);
#line 1581 "parse.c"
}
      break;
/********* End destructor definitions *****************************************/
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
*/
static void yy_pop_parser_stack(yyParser *pParser){
  yyStackEntry *yytos;
  assert( pParser->yytos!=0 );
  assert( pParser->yytos > pParser->yystack );
  yytos = pParser->yytos--;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yy_destructor(pParser, yytos->major, &yytos->minor);
}

/* 
** Deallocate and destroy a parser.  Destructors are called for
** all stack elements before shutting the parser down.
**
** If the YYPARSEFREENEVERNULL macro exists (for example because it
** is defined in a %include section of the input grammar) then it is
** assumed that the input pointer is never NULL.
*/
void sqlite3ParserFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
#ifndef YYPARSEFREENEVERNULL
  if( pParser==0 ) return;
#endif
  while( pParser->yytos>pParser->yystack ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  if( pParser->yystack!=&pParser->yystk0 ) free(pParser->yystack);
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int sqlite3ParserStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyhwm;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
*/
static unsigned int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yytos->stateno;
 
  if( stateno>=YY_MIN_REDUCE ) return stateno;
  assert( stateno <= YY_SHIFT_COUNT );
  do{
    i = yy_shift_ofst[stateno];
    assert( iLookAhead!=YYNOCODE );
    i += iLookAhead;
    if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback = -1;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        assert( yyFallback[iFallback]==0 ); /* Fallback loop must terminate */
        iLookAhead = iFallback;
        continue;
      } else if ( iFallback==0 ) {
        pParser->is_fallback_failed = true;
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( 
#if YY_SHIFT_MIN+YYWILDCARD<0
          j>=0 &&
#endif
#if YY_SHIFT_MAX+YYWILDCARD>=YY_ACTTAB_COUNT
          j<YY_ACTTAB_COUNT &&
#endif
          yy_lookahead[j]==YYWILDCARD && iLookAhead>0
        ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead],
               yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
      return yy_default[stateno];
    }else{
      return yy_action[i];
    }
  }while(1);
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser){
   sqlite3ParserARG_FETCH;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
/******** Begin %stack_overflow code ******************************************/
#line 41 "parse.y"

  sqlite3ErrorMsg(pParse, "parser stack overflow");
#line 1756 "parse.c"
/******** End %stack_overflow code ********************************************/
   sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Print tracing information for a SHIFT action
*/
#ifndef NDEBUG
static void yyTraceShift(yyParser *yypParser, int yyNewState){
  if( yyTraceFILE ){
    if( yyNewState<YYNSTATE ){
      fprintf(yyTraceFILE,"%sShift '%s', go to state %d\n",
         yyTracePrompt,yyTokenName[yypParser->yytos->major],
         yyNewState);
    }else{
      fprintf(yyTraceFILE,"%sShift '%s'\n",
         yyTracePrompt,yyTokenName[yypParser->yytos->major]);
    }
  }
}
#else
# define yyTraceShift(X,Y)
#endif

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  sqlite3ParserTOKENTYPE yyMinor        /* The minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yytos++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
    yypParser->yyhwm++;
    assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack) );
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yytos>=&yypParser->yystack[YYSTACKDEPTH] ){
    yypParser->yytos--;
    yyStackOverflow(yypParser);
    return;
  }
#else
  if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz] ){
    if( yyGrowStack(yypParser) ){
      yypParser->yytos--;
      yyStackOverflow(yypParser);
      return;
    }
  }
#endif
  if( yyNewState > YY_MAX_SHIFT ){
    yyNewState += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
  }
  yytos = yypParser->yytos;
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor.yy0 = yyMinor;
  yyTraceShift(yypParser, yyNewState);
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 147, 3 },
  { 147, 1 },
  { 148, 1 },
  { 148, 3 },
  { 150, 3 },
  { 151, 0 },
  { 151, 1 },
  { 150, 2 },
  { 150, 2 },
  { 150, 2 },
  { 150, 2 },
  { 150, 3 },
  { 150, 5 },
  { 155, 4 },
  { 157, 1 },
  { 158, 0 },
  { 158, 3 },
  { 156, 4 },
  { 156, 2 },
  { 162, 2 },
  { 153, 1 },
  { 164, 1 },
  { 164, 1 },
  { 164, 1 },
  { 164, 1 },
  { 164, 1 },
  { 164, 2 },
  { 165, 3 },
  { 166, 1 },
  { 166, 2 },
  { 167, 1 },
  { 167, 1 },
  { 167, 2 },
  { 168, 0 },
  { 168, 3 },
  { 168, 5 },
  { 169, 2 },
  { 169, 2 },
  { 169, 4 },
  { 169, 3 },
  { 169, 3 },
  { 169, 2 },
  { 169, 3 },
  { 169, 5 },
  { 169, 2 },
  { 169, 4 },
  { 169, 4 },
  { 169, 1 },
  { 169, 2 },
  { 174, 0 },
  { 174, 1 },
  { 176, 0 },
  { 176, 2 },
  { 178, 2 },
  { 178, 3 },
  { 178, 3 },
  { 178, 3 },
  { 179, 2 },
  { 179, 2 },
  { 179, 1 },
  { 179, 1 },
  { 179, 2 },
  { 177, 3 },
  { 177, 2 },
  { 180, 0 },
  { 180, 2 },
  { 180, 2 },
  { 160, 0 },
  { 182, 1 },
  { 183, 2 },
  { 183, 7 },
  { 183, 5 },
  { 183, 5 },
  { 183, 10 },
  { 186, 0 },
  { 172, 0 },
  { 172, 3 },
  { 187, 0 },
  { 187, 2 },
  { 188, 1 },
  { 188, 1 },
  { 150, 4 },
  { 190, 2 },
  { 190, 0 },
  { 150, 7 },
  { 150, 4 },
  { 150, 1 },
  { 161, 2 },
  { 192, 3 },
  { 195, 1 },
  { 195, 2 },
  { 195, 1 },
  { 193, 9 },
  { 204, 4 },
  { 204, 5 },
  { 196, 1 },
  { 196, 1 },
  { 196, 0 },
  { 207, 0 },
  { 197, 3 },
  { 197, 2 },
  { 197, 4 },
  { 208, 2 },
  { 208, 0 },
  { 198, 0 },
  { 198, 2 },
  { 210, 2 },
  { 210, 0 },
  { 209, 6 },
  { 209, 8 },
  { 209, 7 },
  { 209, 7 },
  { 191, 1 },
  { 211, 1 },
  { 211, 2 },
  { 211, 3 },
  { 211, 4 },
  { 213, 2 },
  { 213, 0 },
  { 212, 0 },
  { 212, 3 },
  { 212, 2 },
  { 214, 4 },
  { 214, 0 },
  { 202, 0 },
  { 202, 3 },
  { 184, 4 },
  { 184, 2 },
  { 173, 1 },
  { 173, 1 },
  { 173, 0 },
  { 200, 0 },
  { 200, 3 },
  { 201, 0 },
  { 201, 2 },
  { 203, 0 },
  { 203, 2 },
  { 203, 4 },
  { 203, 4 },
  { 150, 6 },
  { 199, 0 },
  { 199, 2 },
  { 150, 8 },
  { 217, 5 },
  { 217, 7 },
  { 217, 3 },
  { 217, 5 },
  { 150, 6 },
  { 150, 7 },
  { 218, 2 },
  { 218, 1 },
  { 219, 0 },
  { 219, 3 },
  { 216, 3 },
  { 216, 1 },
  { 171, 3 },
  { 170, 1 },
  { 171, 1 },
  { 171, 1 },
  { 171, 3 },
  { 170, 1 },
  { 170, 1 },
  { 170, 1 },
  { 171, 1 },
  { 171, 3 },
  { 171, 6 },
  { 171, 5 },
  { 171, 4 },
  { 170, 1 },
  { 171, 5 },
  { 171, 3 },
  { 171, 3 },
  { 171, 3 },
  { 171, 3 },
  { 171, 3 },
  { 171, 3 },
  { 171, 3 },
  { 171, 3 },
  { 220, 1 },
  { 220, 2 },
  { 171, 3 },
  { 171, 5 },
  { 171, 2 },
  { 171, 3 },
  { 171, 3 },
  { 171, 4 },
  { 171, 2 },
  { 171, 2 },
  { 171, 2 },
  { 171, 2 },
  { 221, 1 },
  { 221, 2 },
  { 171, 5 },
  { 222, 1 },
  { 222, 2 },
  { 171, 5 },
  { 171, 3 },
  { 171, 5 },
  { 171, 4 },
  { 171, 4 },
  { 171, 5 },
  { 225, 5 },
  { 225, 4 },
  { 226, 2 },
  { 226, 0 },
  { 224, 1 },
  { 224, 0 },
  { 206, 0 },
  { 205, 3 },
  { 205, 1 },
  { 223, 0 },
  { 223, 3 },
  { 150, 11 },
  { 227, 1 },
  { 227, 0 },
  { 175, 0 },
  { 175, 3 },
  { 185, 5 },
  { 185, 3 },
  { 228, 0 },
  { 228, 2 },
  { 150, 6 },
  { 150, 2 },
  { 150, 4 },
  { 150, 5 },
  { 150, 4 },
  { 150, 5 },
  { 150, 6 },
  { 150, 1 },
  { 231, 2 },
  { 230, 2 },
  { 150, 5 },
  { 232, 9 },
  { 234, 1 },
  { 234, 1 },
  { 234, 2 },
  { 234, 0 },
  { 235, 1 },
  { 235, 1 },
  { 235, 3 },
  { 237, 0 },
  { 237, 2 },
  { 233, 3 },
  { 233, 2 },
  { 239, 3 },
  { 240, 3 },
  { 240, 2 },
  { 238, 7 },
  { 238, 5 },
  { 238, 5 },
  { 238, 1 },
  { 171, 4 },
  { 171, 6 },
  { 189, 1 },
  { 189, 1 },
  { 189, 1 },
  { 150, 4 },
  { 150, 1 },
  { 150, 2 },
  { 150, 6 },
  { 194, 0 },
  { 194, 2 },
  { 194, 3 },
  { 241, 6 },
  { 241, 8 },
  { 146, 1 },
  { 148, 0 },
  { 149, 1 },
  { 152, 0 },
  { 152, 1 },
  { 152, 2 },
  { 154, 1 },
  { 154, 0 },
  { 150, 2 },
  { 159, 4 },
  { 159, 2 },
  { 164, 1 },
  { 163, 2 },
  { 163, 0 },
  { 169, 2 },
  { 160, 2 },
  { 181, 3 },
  { 181, 1 },
  { 182, 0 },
  { 186, 1 },
  { 188, 1 },
  { 192, 1 },
  { 193, 1 },
  { 207, 2 },
  { 208, 1 },
  { 215, 1 },
  { 215, 1 },
  { 171, 1 },
  { 206, 1 },
  { 229, 1 },
  { 229, 1 },
  { 229, 1 },
  { 229, 1 },
  { 229, 1 },
  { 229, 1 },
  { 231, 1 },
  { 236, 0 },
  { 236, 3 },
  { 239, 1 },
  { 240, 0 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  unsigned int yyruleno        /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  sqlite3ParserARG_FETCH;
  yymsp = yypParser->yytos;
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) ){
    yysize = yyRuleInfo[yyruleno].nrhs;
    fprintf(yyTraceFILE, "%sReduce [%s], go to state %d.\n", yyTracePrompt,
      yyRuleName[yyruleno], yymsp[-yysize].stateno);
  }
#endif /* NDEBUG */

  /* Check that the stack is large enough to grow by a single entry
  ** if the RHS of the rule is empty.  This ensures that there is room
  ** enough on the stack to push the LHS value */
  if( yyRuleInfo[yyruleno].nrhs==0 ){
#ifdef YYTRACKMAXSTACKDEPTH
    if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
      yypParser->yyhwm++;
      assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack));
    }
#endif
#if YYSTACKDEPTH>0 
    if( yypParser->yytos>=&yypParser->yystack[YYSTACKDEPTH-1] ){
      yyStackOverflow(yypParser);
      return;
    }
#else
    if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz-1] ){
      if( yyGrowStack(yypParser) ){
        yyStackOverflow(yypParser);
        return;
      }
      yymsp = yypParser->yytos;
    }
#endif
  }

  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
/********** Begin reduce actions **********************************************/
        YYMINORTYPE yylhsminor;
      case 0: /* ecmd ::= explain cmdx SEMI */
#line 111 "parse.y"
{
	if (!pParse->parse_only)
		sqlite3FinishCoding(pParse);
}
#line 2204 "parse.c"
        break;
      case 1: /* ecmd ::= SEMI */
#line 115 "parse.y"
{
  sqlite3ErrorMsg(pParse, "syntax error: empty request");
}
#line 2211 "parse.c"
        break;
      case 2: /* explain ::= EXPLAIN */
#line 120 "parse.y"
{ pParse->explain = 1; }
#line 2216 "parse.c"
        break;
      case 3: /* explain ::= EXPLAIN QUERY PLAN */
#line 121 "parse.y"
{ pParse->explain = 2; }
#line 2221 "parse.c"
        break;
      case 4: /* cmd ::= BEGIN transtype trans_opt */
#line 153 "parse.y"
{sqlite3BeginTransaction(pParse, yymsp[-1].minor.yy328);}
#line 2226 "parse.c"
        break;
      case 5: /* transtype ::= */
#line 158 "parse.y"
{yymsp[1].minor.yy328 = TK_DEFERRED;}
#line 2231 "parse.c"
        break;
      case 6: /* transtype ::= DEFERRED */
#line 159 "parse.y"
{yymsp[0].minor.yy328 = yymsp[0].major; /*A-overwrites-X*/}
#line 2236 "parse.c"
        break;
      case 7: /* cmd ::= COMMIT trans_opt */
      case 8: /* cmd ::= END trans_opt */ yytestcase(yyruleno==8);
#line 160 "parse.y"
{sqlite3CommitTransaction(pParse);}
#line 2242 "parse.c"
        break;
      case 9: /* cmd ::= ROLLBACK trans_opt */
#line 162 "parse.y"
{sqlite3RollbackTransaction(pParse);}
#line 2247 "parse.c"
        break;
      case 10: /* cmd ::= SAVEPOINT nm */
#line 166 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_BEGIN, &yymsp[0].minor.yy0);
}
#line 2254 "parse.c"
        break;
      case 11: /* cmd ::= RELEASE savepoint_opt nm */
#line 169 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_RELEASE, &yymsp[0].minor.yy0);
}
#line 2261 "parse.c"
        break;
      case 12: /* cmd ::= ROLLBACK trans_opt TO savepoint_opt nm */
#line 172 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_ROLLBACK, &yymsp[0].minor.yy0);
}
#line 2268 "parse.c"
        break;
      case 13: /* create_table ::= createkw TABLE ifnotexists nm */
#line 179 "parse.y"
{
   sqlite3StartTable(pParse,&yymsp[0].minor.yy0,yymsp[-1].minor.yy328);
}
#line 2275 "parse.c"
        break;
      case 14: /* createkw ::= CREATE */
#line 182 "parse.y"
{disableLookaside(pParse);}
#line 2280 "parse.c"
        break;
      case 15: /* ifnotexists ::= */
      case 49: /* autoinc ::= */ yytestcase(yyruleno==49);
      case 64: /* init_deferred_pred_opt ::= */ yytestcase(yyruleno==64);
      case 74: /* defer_subclause_opt ::= */ yytestcase(yyruleno==74);
      case 83: /* ifexists ::= */ yytestcase(yyruleno==83);
      case 97: /* distinct ::= */ yytestcase(yyruleno==97);
      case 219: /* collate ::= */ yytestcase(yyruleno==219);
#line 185 "parse.y"
{yymsp[1].minor.yy328 = 0;}
#line 2291 "parse.c"
        break;
      case 16: /* ifnotexists ::= IF NOT EXISTS */
#line 186 "parse.y"
{yymsp[-2].minor.yy328 = 1;}
#line 2296 "parse.c"
        break;
      case 17: /* create_table_args ::= LP columnlist conslist_opt RP */
#line 188 "parse.y"
{
  sqlite3EndTable(pParse,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy0,0);
}
#line 2303 "parse.c"
        break;
      case 18: /* create_table_args ::= AS select */
#line 191 "parse.y"
{
  sqlite3EndTable(pParse,0,0,yymsp[0].minor.yy91);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy91);
}
#line 2311 "parse.c"
        break;
      case 19: /* columnname ::= nm typedef */
#line 197 "parse.y"
{sqlite3AddColumn(pParse,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy386);}
#line 2316 "parse.c"
        break;
      case 20: /* nm ::= ID|INDEXED */
#line 235 "parse.y"
{
  if(yymsp[0].minor.yy0.isReserved) {
    sqlite3ErrorMsg(pParse, "keyword \"%T\" is reserved", &yymsp[0].minor.yy0);
  }
}
#line 2325 "parse.c"
        break;
      case 21: /* typedef ::= TEXT */
#line 242 "parse.y"
{yymsp[0].minor.yy386.type = SQLITE_AFF_TEXT;}
#line 2330 "parse.c"
        break;
      case 22: /* typedef ::= BLOB */
#line 243 "parse.y"
{yymsp[0].minor.yy386.type = SQLITE_AFF_BLOB; }
#line 2335 "parse.c"
        break;
      case 23: /* typedef ::= DATE */
#line 244 "parse.y"
{/*FIXME: use native type*/ yymsp[0].minor.yy386.type = SQLITE_AFF_INTEGER;}
#line 2340 "parse.c"
        break;
      case 24: /* typedef ::= TIME */
      case 25: /* typedef ::= DATETIME */ yytestcase(yyruleno==25);
#line 245 "parse.y"
{/*FIXME: use native type*/ yymsp[0].minor.yy386.type = SQLITE_AFF_REAL;}
#line 2346 "parse.c"
        break;
      case 26: /* typedef ::= CHAR|VARCHAR charlengthtypedef */
#line 249 "parse.y"
{yymsp[-1].minor.yy386.type = SQLITE_AFF_TEXT;(void)yymsp[0].minor.yy386;}
#line 2351 "parse.c"
        break;
      case 27: /* charlengthtypedef ::= LP INTEGER RP */
#line 250 "parse.y"
{sqlite3TokenToLong(&yymsp[-1].minor.yy0, &yymsp[-2].minor.yy386.s.length);}
#line 2356 "parse.c"
        break;
      case 28: /* numbertypedef ::= unsignednumbertypedef */
#line 256 "parse.y"
{yylhsminor.yy386 = yymsp[0].minor.yy386; yylhsminor.yy386.n.positive = true;}
#line 2361 "parse.c"
  yymsp[0].minor.yy386 = yylhsminor.yy386;
        break;
      case 29: /* numbertypedef ::= UNSIGNED unsignednumbertypedef */
#line 257 "parse.y"
{yymsp[-1].minor.yy386 = yymsp[0].minor.yy386; yymsp[-1].minor.yy386.n.positive = true;}
#line 2367 "parse.c"
        break;
      case 30: /* unsignednumbertypedef ::= FLOAT */
#line 258 "parse.y"
{yymsp[0].minor.yy386.type = SQLITE_AFF_REAL;}
#line 2372 "parse.c"
        break;
      case 31: /* unsignednumbertypedef ::= INT|INTEGER */
#line 259 "parse.y"
{yymsp[0].minor.yy386.type = SQLITE_AFF_INTEGER; yymsp[0].minor.yy386.n.size = 16; yymsp[0].minor.yy386.n.precision = 0; }
#line 2377 "parse.c"
        break;
      case 32: /* unsignednumbertypedef ::= DECIMAL|NUMERIC numlengthtypedef */
#line 262 "parse.y"
{yymsp[-1].minor.yy386.type = SQLITE_AFF_INTEGER; yymsp[-1].minor.yy386.n = yymsp[0].minor.yy386.n; }
#line 2382 "parse.c"
        break;
      case 33: /* numlengthtypedef ::= */
#line 263 "parse.y"
{yymsp[1].minor.yy386.n.size = 16; yymsp[1].minor.yy386.n.precision = 0;}
#line 2387 "parse.c"
        break;
      case 34: /* numlengthtypedef ::= LP INTEGER RP */
#line 264 "parse.y"
{
    sqlite3TokenToLong(&yymsp[-1].minor.yy0, &yymsp[-2].minor.yy386.n.size);
    yymsp[-2].minor.yy386.n.precision = 0;}
#line 2394 "parse.c"
        break;
      case 35: /* numlengthtypedef ::= LP INTEGER COMMA INTEGER RP */
#line 267 "parse.y"
{
    sqlite3TokenToLong(&yymsp[-3].minor.yy0, &yymsp[-4].minor.yy386.n.size);
    sqlite3TokenToLong(&yymsp[-1].minor.yy0, &yymsp[-4].minor.yy386.n.precision);}
#line 2401 "parse.c"
        break;
      case 36: /* ccons ::= CONSTRAINT nm */
      case 69: /* tcons ::= CONSTRAINT nm */ yytestcase(yyruleno==69);
#line 276 "parse.y"
{pParse->constraintName = yymsp[0].minor.yy0;}
#line 2407 "parse.c"
        break;
      case 37: /* ccons ::= DEFAULT term */
      case 39: /* ccons ::= DEFAULT PLUS term */ yytestcase(yyruleno==39);
#line 277 "parse.y"
{sqlite3AddDefaultValue(pParse,&yymsp[0].minor.yy258);}
#line 2413 "parse.c"
        break;
      case 38: /* ccons ::= DEFAULT LP expr RP */
#line 278 "parse.y"
{sqlite3AddDefaultValue(pParse,&yymsp[-1].minor.yy258);}
#line 2418 "parse.c"
        break;
      case 40: /* ccons ::= DEFAULT MINUS term */
#line 280 "parse.y"
{
  ExprSpan v;
  v.pExpr = sqlite3PExpr(pParse, TK_UMINUS, yymsp[0].minor.yy258.pExpr, 0);
  v.zStart = yymsp[-1].minor.yy0.z;
  v.zEnd = yymsp[0].minor.yy258.zEnd;
  sqlite3AddDefaultValue(pParse,&v);
}
#line 2429 "parse.c"
        break;
      case 41: /* ccons ::= DEFAULT ID|INDEXED */
#line 287 "parse.y"
{
  ExprSpan v;
  spanExpr(&v, pParse, TK_STRING, yymsp[0].minor.yy0);
  sqlite3AddDefaultValue(pParse,&v);
}
#line 2438 "parse.c"
        break;
      case 42: /* ccons ::= NOT NULL onconf */
#line 297 "parse.y"
{sqlite3AddNotNull(pParse, yymsp[0].minor.yy328);}
#line 2443 "parse.c"
        break;
      case 43: /* ccons ::= PRIMARY KEY sortorder onconf autoinc */
#line 299 "parse.y"
{sqlite3AddPrimaryKey(pParse,0,yymsp[-1].minor.yy328,yymsp[0].minor.yy328,yymsp[-2].minor.yy328);}
#line 2448 "parse.c"
        break;
      case 44: /* ccons ::= UNIQUE onconf */
#line 300 "parse.y"
{sqlite3CreateIndex(pParse,0,0,0,yymsp[0].minor.yy328,0,0,0,0,
                                   SQLITE_IDXTYPE_UNIQUE);}
#line 2454 "parse.c"
        break;
      case 45: /* ccons ::= CHECK LP expr RP */
#line 302 "parse.y"
{sqlite3AddCheckConstraint(pParse,yymsp[-1].minor.yy258.pExpr);}
#line 2459 "parse.c"
        break;
      case 46: /* ccons ::= REFERENCES nm eidlist_opt refargs */
#line 304 "parse.y"
{sqlite3CreateForeignKey(pParse,0,&yymsp[-2].minor.yy0,yymsp[-1].minor.yy322,yymsp[0].minor.yy328);}
#line 2464 "parse.c"
        break;
      case 47: /* ccons ::= defer_subclause */
#line 305 "parse.y"
{sqlite3DeferForeignKey(pParse,yymsp[0].minor.yy328);}
#line 2469 "parse.c"
        break;
      case 48: /* ccons ::= COLLATE ID|INDEXED */
#line 306 "parse.y"
{sqlite3AddCollateType(pParse, &yymsp[0].minor.yy0);}
#line 2474 "parse.c"
        break;
      case 50: /* autoinc ::= AUTOINCR */
#line 311 "parse.y"
{yymsp[0].minor.yy328 = 1;}
#line 2479 "parse.c"
        break;
      case 51: /* refargs ::= */
#line 319 "parse.y"
{ yymsp[1].minor.yy328 = ON_CONFLICT_ACTION_NONE*0x0101; /* EV: R-19803-45884 */}
#line 2484 "parse.c"
        break;
      case 52: /* refargs ::= refargs refarg */
#line 320 "parse.y"
{ yymsp[-1].minor.yy328 = (yymsp[-1].minor.yy328 & ~yymsp[0].minor.yy319.mask) | yymsp[0].minor.yy319.value; }
#line 2489 "parse.c"
        break;
      case 53: /* refarg ::= MATCH nm */
#line 322 "parse.y"
{ yymsp[-1].minor.yy319.value = 0;     yymsp[-1].minor.yy319.mask = 0x000000; }
#line 2494 "parse.c"
        break;
      case 54: /* refarg ::= ON INSERT refact */
#line 323 "parse.y"
{ yymsp[-2].minor.yy319.value = 0;     yymsp[-2].minor.yy319.mask = 0x000000; }
#line 2499 "parse.c"
        break;
      case 55: /* refarg ::= ON DELETE refact */
#line 324 "parse.y"
{ yymsp[-2].minor.yy319.value = yymsp[0].minor.yy328;     yymsp[-2].minor.yy319.mask = 0x0000ff; }
#line 2504 "parse.c"
        break;
      case 56: /* refarg ::= ON UPDATE refact */
#line 325 "parse.y"
{ yymsp[-2].minor.yy319.value = yymsp[0].minor.yy328<<8;  yymsp[-2].minor.yy319.mask = 0x00ff00; }
#line 2509 "parse.c"
        break;
      case 57: /* refact ::= SET NULL */
#line 327 "parse.y"
{ yymsp[-1].minor.yy328 = OE_SetNull;  /* EV: R-33326-45252 */}
#line 2514 "parse.c"
        break;
      case 58: /* refact ::= SET DEFAULT */
#line 328 "parse.y"
{ yymsp[-1].minor.yy328 = OE_SetDflt;  /* EV: R-33326-45252 */}
#line 2519 "parse.c"
        break;
      case 59: /* refact ::= CASCADE */
#line 329 "parse.y"
{ yymsp[0].minor.yy328 = OE_Cascade;  /* EV: R-33326-45252 */}
#line 2524 "parse.c"
        break;
      case 60: /* refact ::= RESTRICT */
#line 330 "parse.y"
{ yymsp[0].minor.yy328 = OE_Restrict; /* EV: R-33326-45252 */}
#line 2529 "parse.c"
        break;
      case 61: /* refact ::= NO ACTION */
#line 331 "parse.y"
{ yymsp[-1].minor.yy328 = ON_CONFLICT_ACTION_NONE;     /* EV: R-33326-45252 */}
#line 2534 "parse.c"
        break;
      case 62: /* defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt */
#line 333 "parse.y"
{yymsp[-2].minor.yy328 = 0;}
#line 2539 "parse.c"
        break;
      case 63: /* defer_subclause ::= DEFERRABLE init_deferred_pred_opt */
      case 78: /* orconf ::= OR resolvetype */ yytestcase(yyruleno==78);
      case 149: /* insert_cmd ::= INSERT orconf */ yytestcase(yyruleno==149);
#line 334 "parse.y"
{yymsp[-1].minor.yy328 = yymsp[0].minor.yy328;}
#line 2546 "parse.c"
        break;
      case 65: /* init_deferred_pred_opt ::= INITIALLY DEFERRED */
      case 82: /* ifexists ::= IF EXISTS */ yytestcase(yyruleno==82);
      case 191: /* between_op ::= NOT BETWEEN */ yytestcase(yyruleno==191);
      case 194: /* in_op ::= NOT IN */ yytestcase(yyruleno==194);
      case 220: /* collate ::= COLLATE ID|INDEXED */ yytestcase(yyruleno==220);
#line 337 "parse.y"
{yymsp[-1].minor.yy328 = 1;}
#line 2555 "parse.c"
        break;
      case 66: /* init_deferred_pred_opt ::= INITIALLY IMMEDIATE */
#line 338 "parse.y"
{yymsp[-1].minor.yy328 = 0;}
#line 2560 "parse.c"
        break;
      case 67: /* conslist_opt ::= */
      case 103: /* as ::= */ yytestcase(yyruleno==103);
#line 340 "parse.y"
{yymsp[1].minor.yy0.n = 0; yymsp[1].minor.yy0.z = 0;}
#line 2566 "parse.c"
        break;
      case 68: /* tconscomma ::= COMMA */
#line 344 "parse.y"
{pParse->constraintName.n = 0;}
#line 2571 "parse.c"
        break;
      case 70: /* tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf */
#line 348 "parse.y"
{sqlite3AddPrimaryKey(pParse,yymsp[-3].minor.yy322,yymsp[0].minor.yy328,yymsp[-2].minor.yy328,0);}
#line 2576 "parse.c"
        break;
      case 71: /* tcons ::= UNIQUE LP sortlist RP onconf */
#line 350 "parse.y"
{sqlite3CreateIndex(pParse,0,0,yymsp[-2].minor.yy322,yymsp[0].minor.yy328,0,0,0,0,
                                       SQLITE_IDXTYPE_UNIQUE);}
#line 2582 "parse.c"
        break;
      case 72: /* tcons ::= CHECK LP expr RP onconf */
#line 353 "parse.y"
{sqlite3AddCheckConstraint(pParse,yymsp[-2].minor.yy258.pExpr);}
#line 2587 "parse.c"
        break;
      case 73: /* tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt */
#line 355 "parse.y"
{
    sqlite3CreateForeignKey(pParse, yymsp[-6].minor.yy322, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy322, yymsp[-1].minor.yy328);
    sqlite3DeferForeignKey(pParse, yymsp[0].minor.yy328);
}
#line 2595 "parse.c"
        break;
      case 75: /* onconf ::= */
      case 77: /* orconf ::= */ yytestcase(yyruleno==77);
#line 369 "parse.y"
{yymsp[1].minor.yy328 = ON_CONFLICT_ACTION_DEFAULT;}
#line 2601 "parse.c"
        break;
      case 76: /* onconf ::= ON CONFLICT resolvetype */
#line 370 "parse.y"
{yymsp[-2].minor.yy328 = yymsp[0].minor.yy328;}
#line 2606 "parse.c"
        break;
      case 79: /* resolvetype ::= IGNORE */
#line 374 "parse.y"
{yymsp[0].minor.yy328 = ON_CONFLICT_ACTION_IGNORE;}
#line 2611 "parse.c"
        break;
      case 80: /* resolvetype ::= REPLACE */
      case 150: /* insert_cmd ::= REPLACE */ yytestcase(yyruleno==150);
#line 375 "parse.y"
{yymsp[0].minor.yy328 = ON_CONFLICT_ACTION_REPLACE;}
#line 2617 "parse.c"
        break;
      case 81: /* cmd ::= DROP TABLE ifexists fullname */
#line 379 "parse.y"
{
  sql_drop_table(pParse, yymsp[0].minor.yy439, 0, yymsp[-1].minor.yy328);
}
#line 2624 "parse.c"
        break;
      case 84: /* cmd ::= createkw VIEW ifnotexists nm eidlist_opt AS select */
#line 390 "parse.y"
{
  sqlite3CreateView(pParse, &yymsp[-6].minor.yy0, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy322, yymsp[0].minor.yy91, yymsp[-4].minor.yy328);
}
#line 2631 "parse.c"
        break;
      case 85: /* cmd ::= DROP VIEW ifexists fullname */
#line 393 "parse.y"
{
  sql_drop_table(pParse, yymsp[0].minor.yy439, 1, yymsp[-1].minor.yy328);
}
#line 2638 "parse.c"
        break;
      case 86: /* cmd ::= select */
#line 400 "parse.y"
{
  SelectDest dest = {SRT_Output, 0, 0, 0, 0, 0};
  if(!pParse->parse_only)
	  sqlite3Select(pParse, yymsp[0].minor.yy91, &dest);
  else
	  sql_expr_extract_select(pParse, yymsp[0].minor.yy91);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy91);
}
#line 2650 "parse.c"
        break;
      case 87: /* select ::= with selectnowith */
#line 440 "parse.y"
{
  Select *p = yymsp[0].minor.yy91;
  if( p ){
    p->pWith = yymsp[-1].minor.yy323;
    parserDoubleLinkSelect(pParse, p);
  }else{
    sqlite3WithDelete(pParse->db, yymsp[-1].minor.yy323);
  }
  yymsp[-1].minor.yy91 = p; /*A-overwrites-W*/
}
#line 2664 "parse.c"
        break;
      case 88: /* selectnowith ::= selectnowith multiselect_op oneselect */
#line 453 "parse.y"
{
  Select *pRhs = yymsp[0].minor.yy91;
  Select *pLhs = yymsp[-2].minor.yy91;
  if( pRhs && pRhs->pPrior ){
    SrcList *pFrom;
    Token x;
    x.n = 0;
    parserDoubleLinkSelect(pParse, pRhs);
    pFrom = sqlite3SrcListAppendFromTerm(pParse,0,0,&x,pRhs,0,0);
    pRhs = sqlite3SelectNew(pParse,0,pFrom,0,0,0,0,0,0,0);
  }
  if( pRhs ){
    pRhs->op = (u8)yymsp[-1].minor.yy328;
    pRhs->pPrior = pLhs;
    if( ALWAYS(pLhs) ) pLhs->selFlags &= ~SF_MultiValue;
    pRhs->selFlags &= ~SF_MultiValue;
    if( yymsp[-1].minor.yy328!=TK_ALL ) pParse->hasCompound = 1;
  }else{
    sqlite3SelectDelete(pParse->db, pLhs);
  }
  yymsp[-2].minor.yy91 = pRhs;
}
#line 2690 "parse.c"
        break;
      case 89: /* multiselect_op ::= UNION */
      case 91: /* multiselect_op ::= EXCEPT|INTERSECT */ yytestcase(yyruleno==91);
#line 476 "parse.y"
{yymsp[0].minor.yy328 = yymsp[0].major; /*A-overwrites-OP*/}
#line 2696 "parse.c"
        break;
      case 90: /* multiselect_op ::= UNION ALL */
#line 477 "parse.y"
{yymsp[-1].minor.yy328 = TK_ALL;}
#line 2701 "parse.c"
        break;
      case 92: /* oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt */
#line 481 "parse.y"
{
#ifdef SELECTTRACE_ENABLED
  Token s = yymsp[-8].minor.yy0; /*A-overwrites-S*/
#endif
  yymsp[-8].minor.yy91 = sqlite3SelectNew(pParse,yymsp[-6].minor.yy322,yymsp[-5].minor.yy439,yymsp[-4].minor.yy418,yymsp[-3].minor.yy322,yymsp[-2].minor.yy418,yymsp[-1].minor.yy322,yymsp[-7].minor.yy328,yymsp[0].minor.yy388.pLimit,yymsp[0].minor.yy388.pOffset);
#ifdef SELECTTRACE_ENABLED
  /* Populate the Select.zSelName[] string that is used to help with
  ** query planner debugging, to differentiate between multiple Select
  ** objects in a complex query.
  **
  ** If the SELECT keyword is immediately followed by a C-style comment
  ** then extract the first few alphanumeric characters from within that
  ** comment to be the zSelName value.  Otherwise, the label is #N where
  ** is an integer that is incremented with each SELECT statement seen.
  */
  if( yymsp[-8].minor.yy91!=0 ){
    const char *z = s.z+6;
    int i;
    sqlite3_snprintf(sizeof(yymsp[-8].minor.yy91->zSelName), yymsp[-8].minor.yy91->zSelName, "#%d",
                     ++pParse->nSelect);
    while( z[0]==' ' ) z++;
    if( z[0]=='/' && z[1]=='*' ){
      z += 2;
      while( z[0]==' ' ) z++;
      for(i=0; sqlite3Isalnum(z[i]); i++){}
      sqlite3_snprintf(sizeof(yymsp[-8].minor.yy91->zSelName), yymsp[-8].minor.yy91->zSelName, "%.*s", i, z);
    }
  }
#endif /* SELECTRACE_ENABLED */
}
#line 2735 "parse.c"
        break;
      case 93: /* values ::= VALUES LP nexprlist RP */
#line 515 "parse.y"
{
  yymsp[-3].minor.yy91 = sqlite3SelectNew(pParse,yymsp[-1].minor.yy322,0,0,0,0,0,SF_Values,0,0);
}
#line 2742 "parse.c"
        break;
      case 94: /* values ::= values COMMA LP exprlist RP */
#line 518 "parse.y"
{
  Select *pRight, *pLeft = yymsp[-4].minor.yy91;
  pRight = sqlite3SelectNew(pParse,yymsp[-1].minor.yy322,0,0,0,0,0,SF_Values|SF_MultiValue,0,0);
  if( ALWAYS(pLeft) ) pLeft->selFlags &= ~SF_MultiValue;
  if( pRight ){
    pRight->op = TK_ALL;
    pRight->pPrior = pLeft;
    yymsp[-4].minor.yy91 = pRight;
  }else{
    yymsp[-4].minor.yy91 = pLeft;
  }
}
#line 2758 "parse.c"
        break;
      case 95: /* distinct ::= DISTINCT */
#line 535 "parse.y"
{yymsp[0].minor.yy328 = SF_Distinct;}
#line 2763 "parse.c"
        break;
      case 96: /* distinct ::= ALL */
#line 536 "parse.y"
{yymsp[0].minor.yy328 = SF_All;}
#line 2768 "parse.c"
        break;
      case 98: /* sclp ::= */
      case 124: /* orderby_opt ::= */ yytestcase(yyruleno==124);
      case 131: /* groupby_opt ::= */ yytestcase(yyruleno==131);
      case 207: /* exprlist ::= */ yytestcase(yyruleno==207);
      case 210: /* paren_exprlist ::= */ yytestcase(yyruleno==210);
      case 215: /* eidlist_opt ::= */ yytestcase(yyruleno==215);
#line 549 "parse.y"
{yymsp[1].minor.yy322 = 0;}
#line 2778 "parse.c"
        break;
      case 99: /* selcollist ::= sclp expr as */
#line 550 "parse.y"
{
   yymsp[-2].minor.yy322 = sqlite3ExprListAppend(pParse, yymsp[-2].minor.yy322, yymsp[-1].minor.yy258.pExpr);
   if( yymsp[0].minor.yy0.n>0 ) sqlite3ExprListSetName(pParse, yymsp[-2].minor.yy322, &yymsp[0].minor.yy0, 1);
   sqlite3ExprListSetSpan(pParse,yymsp[-2].minor.yy322,&yymsp[-1].minor.yy258);
}
#line 2787 "parse.c"
        break;
      case 100: /* selcollist ::= sclp STAR */
#line 555 "parse.y"
{
  Expr *p = sqlite3Expr(pParse->db, TK_ASTERISK, 0);
  yymsp[-1].minor.yy322 = sqlite3ExprListAppend(pParse, yymsp[-1].minor.yy322, p);
}
#line 2795 "parse.c"
        break;
      case 101: /* selcollist ::= sclp nm DOT STAR */
#line 559 "parse.y"
{
  Expr *pRight = sqlite3PExpr(pParse, TK_ASTERISK, 0, 0);
  Expr *pLeft = sqlite3ExprAlloc(pParse->db, TK_ID, 0, &yymsp[-2].minor.yy0, 1);
  Expr *pDot = sqlite3PExpr(pParse, TK_DOT, pLeft, pRight);
  yymsp[-3].minor.yy322 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy322, pDot);
}
#line 2805 "parse.c"
        break;
      case 102: /* as ::= AS nm */
      case 229: /* plus_num ::= PLUS INTEGER|FLOAT */ yytestcase(yyruleno==229);
      case 230: /* minus_num ::= MINUS INTEGER|FLOAT */ yytestcase(yyruleno==230);
#line 570 "parse.y"
{yymsp[-1].minor.yy0 = yymsp[0].minor.yy0;}
#line 2812 "parse.c"
        break;
      case 104: /* from ::= */
#line 584 "parse.y"
{yymsp[1].minor.yy439 = sqlite3DbMallocZero(pParse->db, sizeof(*yymsp[1].minor.yy439));}
#line 2817 "parse.c"
        break;
      case 105: /* from ::= FROM seltablist */
#line 585 "parse.y"
{
  yymsp[-1].minor.yy439 = yymsp[0].minor.yy439;
  sqlite3SrcListShiftJoinType(yymsp[-1].minor.yy439);
}
#line 2825 "parse.c"
        break;
      case 106: /* stl_prefix ::= seltablist joinop */
#line 593 "parse.y"
{
   if( ALWAYS(yymsp[-1].minor.yy439 && yymsp[-1].minor.yy439->nSrc>0) ) yymsp[-1].minor.yy439->a[yymsp[-1].minor.yy439->nSrc-1].fg.jointype = (u8)yymsp[0].minor.yy328;
}
#line 2832 "parse.c"
        break;
      case 107: /* stl_prefix ::= */
#line 596 "parse.y"
{yymsp[1].minor.yy439 = 0;}
#line 2837 "parse.c"
        break;
      case 108: /* seltablist ::= stl_prefix nm as indexed_opt on_opt using_opt */
#line 598 "parse.y"
{
  yymsp[-5].minor.yy439 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-5].minor.yy439,&yymsp[-4].minor.yy0,&yymsp[-3].minor.yy0,0,yymsp[-1].minor.yy418,yymsp[0].minor.yy232);
  sqlite3SrcListIndexedBy(pParse, yymsp[-5].minor.yy439, &yymsp[-2].minor.yy0);
}
#line 2845 "parse.c"
        break;
      case 109: /* seltablist ::= stl_prefix nm LP exprlist RP as on_opt using_opt */
#line 603 "parse.y"
{
  yymsp[-7].minor.yy439 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-7].minor.yy439,&yymsp[-6].minor.yy0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy418,yymsp[0].minor.yy232);
  sqlite3SrcListFuncArgs(pParse, yymsp[-7].minor.yy439, yymsp[-4].minor.yy322);
}
#line 2853 "parse.c"
        break;
      case 110: /* seltablist ::= stl_prefix LP select RP as on_opt using_opt */
#line 609 "parse.y"
{
    yymsp[-6].minor.yy439 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy439,0,&yymsp[-2].minor.yy0,yymsp[-4].minor.yy91,yymsp[-1].minor.yy418,yymsp[0].minor.yy232);
  }
#line 2860 "parse.c"
        break;
      case 111: /* seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt */
#line 613 "parse.y"
{
    if( yymsp[-6].minor.yy439==0 && yymsp[-2].minor.yy0.n==0 && yymsp[-1].minor.yy418==0 && yymsp[0].minor.yy232==0 ){
      yymsp[-6].minor.yy439 = yymsp[-4].minor.yy439;
    }else if( yymsp[-4].minor.yy439->nSrc==1 ){
      yymsp[-6].minor.yy439 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy439,0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy418,yymsp[0].minor.yy232);
      if( yymsp[-6].minor.yy439 ){
        struct SrcList_item *pNew = &yymsp[-6].minor.yy439->a[yymsp[-6].minor.yy439->nSrc-1];
        struct SrcList_item *pOld = yymsp[-4].minor.yy439->a;
        pNew->zName = pOld->zName;
        pNew->pSelect = pOld->pSelect;
        pOld->zName =  0;
        pOld->pSelect = 0;
      }
      sqlite3SrcListDelete(pParse->db, yymsp[-4].minor.yy439);
    }else{
      Select *pSubquery;
      sqlite3SrcListShiftJoinType(yymsp[-4].minor.yy439);
      pSubquery = sqlite3SelectNew(pParse,0,yymsp[-4].minor.yy439,0,0,0,0,SF_NestedFrom,0,0);
      yymsp[-6].minor.yy439 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy439,0,&yymsp[-2].minor.yy0,pSubquery,yymsp[-1].minor.yy418,yymsp[0].minor.yy232);
    }
  }
#line 2885 "parse.c"
        break;
      case 112: /* fullname ::= nm */
#line 639 "parse.y"
{yymsp[0].minor.yy439 = sqlite3SrcListAppend(pParse->db,0,&yymsp[0].minor.yy0); /*A-overwrites-X*/}
#line 2890 "parse.c"
        break;
      case 113: /* joinop ::= COMMA|JOIN */
#line 645 "parse.y"
{ yymsp[0].minor.yy328 = JT_INNER; }
#line 2895 "parse.c"
        break;
      case 114: /* joinop ::= JOIN_KW JOIN */
#line 647 "parse.y"
{yymsp[-1].minor.yy328 = sqlite3JoinType(pParse,&yymsp[-1].minor.yy0,0,0);  /*X-overwrites-A*/}
#line 2900 "parse.c"
        break;
      case 115: /* joinop ::= JOIN_KW join_nm JOIN */
#line 649 "parse.y"
{yymsp[-2].minor.yy328 = sqlite3JoinType(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0,0); /*X-overwrites-A*/}
#line 2905 "parse.c"
        break;
      case 116: /* joinop ::= JOIN_KW join_nm join_nm JOIN */
#line 651 "parse.y"
{yymsp[-3].minor.yy328 = sqlite3JoinType(pParse,&yymsp[-3].minor.yy0,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0);/*X-overwrites-A*/}
#line 2910 "parse.c"
        break;
      case 117: /* on_opt ::= ON expr */
      case 134: /* having_opt ::= HAVING expr */ yytestcase(yyruleno==134);
      case 141: /* where_opt ::= WHERE expr */ yytestcase(yyruleno==141);
      case 203: /* case_else ::= ELSE expr */ yytestcase(yyruleno==203);
#line 655 "parse.y"
{yymsp[-1].minor.yy418 = yymsp[0].minor.yy258.pExpr;}
#line 2918 "parse.c"
        break;
      case 118: /* on_opt ::= */
      case 133: /* having_opt ::= */ yytestcase(yyruleno==133);
      case 140: /* where_opt ::= */ yytestcase(yyruleno==140);
      case 204: /* case_else ::= */ yytestcase(yyruleno==204);
      case 206: /* case_operand ::= */ yytestcase(yyruleno==206);
#line 656 "parse.y"
{yymsp[1].minor.yy418 = 0;}
#line 2927 "parse.c"
        break;
      case 119: /* indexed_opt ::= */
#line 669 "parse.y"
{yymsp[1].minor.yy0.z=0; yymsp[1].minor.yy0.n=0;}
#line 2932 "parse.c"
        break;
      case 120: /* indexed_opt ::= INDEXED BY nm */
#line 670 "parse.y"
{yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;}
#line 2937 "parse.c"
        break;
      case 121: /* indexed_opt ::= NOT INDEXED */
#line 671 "parse.y"
{yymsp[-1].minor.yy0.z=0; yymsp[-1].minor.yy0.n=1;}
#line 2942 "parse.c"
        break;
      case 122: /* using_opt ::= USING LP idlist RP */
#line 675 "parse.y"
{yymsp[-3].minor.yy232 = yymsp[-1].minor.yy232;}
#line 2947 "parse.c"
        break;
      case 123: /* using_opt ::= */
      case 151: /* idlist_opt ::= */ yytestcase(yyruleno==151);
#line 676 "parse.y"
{yymsp[1].minor.yy232 = 0;}
#line 2953 "parse.c"
        break;
      case 125: /* orderby_opt ::= ORDER BY sortlist */
      case 132: /* groupby_opt ::= GROUP BY nexprlist */ yytestcase(yyruleno==132);
#line 690 "parse.y"
{yymsp[-2].minor.yy322 = yymsp[0].minor.yy322;}
#line 2959 "parse.c"
        break;
      case 126: /* sortlist ::= sortlist COMMA expr sortorder */
#line 691 "parse.y"
{
  yymsp[-3].minor.yy322 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy322,yymsp[-1].minor.yy258.pExpr);
  sqlite3ExprListSetSortOrder(yymsp[-3].minor.yy322,yymsp[0].minor.yy328);
}
#line 2967 "parse.c"
        break;
      case 127: /* sortlist ::= expr sortorder */
#line 695 "parse.y"
{
  yymsp[-1].minor.yy322 = sqlite3ExprListAppend(pParse,0,yymsp[-1].minor.yy258.pExpr); /*A-overwrites-Y*/
  sqlite3ExprListSetSortOrder(yymsp[-1].minor.yy322,yymsp[0].minor.yy328);
}
#line 2975 "parse.c"
        break;
      case 128: /* sortorder ::= ASC */
#line 702 "parse.y"
{yymsp[0].minor.yy328 = SQLITE_SO_ASC;}
#line 2980 "parse.c"
        break;
      case 129: /* sortorder ::= DESC */
#line 703 "parse.y"
{yymsp[0].minor.yy328 = SQLITE_SO_DESC;}
#line 2985 "parse.c"
        break;
      case 130: /* sortorder ::= */
#line 704 "parse.y"
{yymsp[1].minor.yy328 = SQLITE_SO_UNDEFINED;}
#line 2990 "parse.c"
        break;
      case 135: /* limit_opt ::= */
#line 729 "parse.y"
{yymsp[1].minor.yy388.pLimit = 0; yymsp[1].minor.yy388.pOffset = 0;}
#line 2995 "parse.c"
        break;
      case 136: /* limit_opt ::= LIMIT expr */
#line 730 "parse.y"
{yymsp[-1].minor.yy388.pLimit = yymsp[0].minor.yy258.pExpr; yymsp[-1].minor.yy388.pOffset = 0;}
#line 3000 "parse.c"
        break;
      case 137: /* limit_opt ::= LIMIT expr OFFSET expr */
#line 732 "parse.y"
{yymsp[-3].minor.yy388.pLimit = yymsp[-2].minor.yy258.pExpr; yymsp[-3].minor.yy388.pOffset = yymsp[0].minor.yy258.pExpr;}
#line 3005 "parse.c"
        break;
      case 138: /* limit_opt ::= LIMIT expr COMMA expr */
#line 734 "parse.y"
{yymsp[-3].minor.yy388.pOffset = yymsp[-2].minor.yy258.pExpr; yymsp[-3].minor.yy388.pLimit = yymsp[0].minor.yy258.pExpr;}
#line 3010 "parse.c"
        break;
      case 139: /* cmd ::= with DELETE FROM fullname indexed_opt where_opt */
#line 751 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-5].minor.yy323, 1);
  sqlite3SrcListIndexedBy(pParse, yymsp[-2].minor.yy439, &yymsp[-1].minor.yy0);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3DeleteFrom(pParse,yymsp[-2].minor.yy439,yymsp[0].minor.yy418);
}
#line 3022 "parse.c"
        break;
      case 142: /* cmd ::= with UPDATE orconf fullname indexed_opt SET setlist where_opt */
#line 784 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-7].minor.yy323, 1);
  sqlite3SrcListIndexedBy(pParse, yymsp[-4].minor.yy439, &yymsp[-3].minor.yy0);
  sqlite3ExprListCheckLength(pParse,yymsp[-1].minor.yy322,"set list"); 
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Update(pParse,yymsp[-4].minor.yy439,yymsp[-1].minor.yy322,yymsp[0].minor.yy418,yymsp[-5].minor.yy328);
}
#line 3035 "parse.c"
        break;
      case 143: /* setlist ::= setlist COMMA nm EQ expr */
#line 798 "parse.y"
{
  yymsp[-4].minor.yy322 = sqlite3ExprListAppend(pParse, yymsp[-4].minor.yy322, yymsp[0].minor.yy258.pExpr);
  sqlite3ExprListSetName(pParse, yymsp[-4].minor.yy322, &yymsp[-2].minor.yy0, 1);
}
#line 3043 "parse.c"
        break;
      case 144: /* setlist ::= setlist COMMA LP idlist RP EQ expr */
#line 802 "parse.y"
{
  yymsp[-6].minor.yy322 = sqlite3ExprListAppendVector(pParse, yymsp[-6].minor.yy322, yymsp[-3].minor.yy232, yymsp[0].minor.yy258.pExpr);
}
#line 3050 "parse.c"
        break;
      case 145: /* setlist ::= nm EQ expr */
#line 805 "parse.y"
{
  yylhsminor.yy322 = sqlite3ExprListAppend(pParse, 0, yymsp[0].minor.yy258.pExpr);
  sqlite3ExprListSetName(pParse, yylhsminor.yy322, &yymsp[-2].minor.yy0, 1);
}
#line 3058 "parse.c"
  yymsp[-2].minor.yy322 = yylhsminor.yy322;
        break;
      case 146: /* setlist ::= LP idlist RP EQ expr */
#line 809 "parse.y"
{
  yymsp[-4].minor.yy322 = sqlite3ExprListAppendVector(pParse, 0, yymsp[-3].minor.yy232, yymsp[0].minor.yy258.pExpr);
}
#line 3066 "parse.c"
        break;
      case 147: /* cmd ::= with insert_cmd INTO fullname idlist_opt select */
#line 815 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-5].minor.yy323, 1);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Insert(pParse, yymsp[-2].minor.yy439, yymsp[0].minor.yy91, yymsp[-1].minor.yy232, yymsp[-4].minor.yy328);
}
#line 3077 "parse.c"
        break;
      case 148: /* cmd ::= with insert_cmd INTO fullname idlist_opt DEFAULT VALUES */
#line 823 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-6].minor.yy323, 1);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Insert(pParse, yymsp[-3].minor.yy439, 0, yymsp[-2].minor.yy232, yymsp[-5].minor.yy328);
}
#line 3088 "parse.c"
        break;
      case 152: /* idlist_opt ::= LP idlist RP */
#line 841 "parse.y"
{yymsp[-2].minor.yy232 = yymsp[-1].minor.yy232;}
#line 3093 "parse.c"
        break;
      case 153: /* idlist ::= idlist COMMA nm */
#line 843 "parse.y"
{yymsp[-2].minor.yy232 = sqlite3IdListAppend(pParse->db,yymsp[-2].minor.yy232,&yymsp[0].minor.yy0);}
#line 3098 "parse.c"
        break;
      case 154: /* idlist ::= nm */
#line 845 "parse.y"
{yymsp[0].minor.yy232 = sqlite3IdListAppend(pParse->db,0,&yymsp[0].minor.yy0); /*A-overwrites-Y*/}
#line 3103 "parse.c"
        break;
      case 155: /* expr ::= LP expr RP */
#line 908 "parse.y"
{spanSet(&yymsp[-2].minor.yy258,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/  yymsp[-2].minor.yy258.pExpr = yymsp[-1].minor.yy258.pExpr;}
#line 3108 "parse.c"
        break;
      case 156: /* term ::= NULL */
      case 160: /* term ::= FLOAT|BLOB */ yytestcase(yyruleno==160);
      case 161: /* term ::= STRING */ yytestcase(yyruleno==161);
#line 909 "parse.y"
{spanExpr(&yymsp[0].minor.yy258,pParse,yymsp[0].major,yymsp[0].minor.yy0);/*A-overwrites-X*/}
#line 3115 "parse.c"
        break;
      case 157: /* expr ::= ID|INDEXED */
      case 158: /* expr ::= JOIN_KW */ yytestcase(yyruleno==158);
#line 910 "parse.y"
{spanExpr(&yymsp[0].minor.yy258,pParse,TK_ID,yymsp[0].minor.yy0); /*A-overwrites-X*/}
#line 3121 "parse.c"
        break;
      case 159: /* expr ::= nm DOT nm */
#line 912 "parse.y"
{
  Expr *temp1 = sqlite3ExprAlloc(pParse->db, TK_ID, 0, &yymsp[-2].minor.yy0, 1);
  Expr *temp2 = sqlite3ExprAlloc(pParse->db, TK_ID, 0, &yymsp[0].minor.yy0, 1);
  spanSet(&yymsp[-2].minor.yy258,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-2].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_DOT, temp1, temp2);
}
#line 3131 "parse.c"
        break;
      case 162: /* term ::= INTEGER */
#line 920 "parse.y"
{
  yylhsminor.yy258.pExpr = sqlite3ExprAlloc(pParse->db, TK_INTEGER, 0, &yymsp[0].minor.yy0, 1);
  yylhsminor.yy258.zStart = yymsp[0].minor.yy0.z;
  yylhsminor.yy258.zEnd = yymsp[0].minor.yy0.z + yymsp[0].minor.yy0.n;
  if( yylhsminor.yy258.pExpr ) yylhsminor.yy258.pExpr->flags |= EP_Leaf;
}
#line 3141 "parse.c"
  yymsp[0].minor.yy258 = yylhsminor.yy258;
        break;
      case 163: /* expr ::= VARIABLE */
#line 926 "parse.y"
{
  if( !(yymsp[0].minor.yy0.z[0]=='#' && sqlite3Isdigit(yymsp[0].minor.yy0.z[1])) ){
    u32 n = yymsp[0].minor.yy0.n;
    spanExpr(&yymsp[0].minor.yy258, pParse, TK_VARIABLE, yymsp[0].minor.yy0);
    sqlite3ExprAssignVarNumber(pParse, yymsp[0].minor.yy258.pExpr, n);
  }else{
    /* When doing a nested parse, one can include terms in an expression
    ** that look like this:   #1 #2 ...  These terms refer to registers
    ** in the virtual machine.  #N is the N-th register. */
    Token t = yymsp[0].minor.yy0; /*A-overwrites-X*/
    assert( t.n>=2 );
    spanSet(&yymsp[0].minor.yy258, &t, &t);
    if( pParse->nested==0 ){
      sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &t);
      yymsp[0].minor.yy258.pExpr = 0;
    }else{
      yymsp[0].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_REGISTER, 0, 0);
      if( yymsp[0].minor.yy258.pExpr ) sqlite3GetInt32(&t.z[1], &yymsp[0].minor.yy258.pExpr->iTable);
    }
  }
}
#line 3167 "parse.c"
        break;
      case 164: /* expr ::= expr COLLATE ID|INDEXED */
#line 947 "parse.y"
{
  yymsp[-2].minor.yy258.pExpr = sqlite3ExprAddCollateToken(pParse, yymsp[-2].minor.yy258.pExpr, &yymsp[0].minor.yy0, 1);
  yymsp[-2].minor.yy258.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
}
#line 3175 "parse.c"
        break;
      case 165: /* expr ::= CAST LP expr AS typedef RP */
#line 952 "parse.y"
{
  spanSet(&yymsp[-5].minor.yy258,&yymsp[-5].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-5].minor.yy258.pExpr = sqlite3ExprAlloc(pParse->db, TK_CAST, &yymsp[-1].minor.yy386, 0, 1);
  sqlite3ExprAttachSubtrees(pParse->db, yymsp[-5].minor.yy258.pExpr, yymsp[-3].minor.yy258.pExpr, 0);
}
#line 3184 "parse.c"
        break;
      case 166: /* expr ::= ID|INDEXED LP distinct exprlist RP */
#line 958 "parse.y"
{
  if( yymsp[-1].minor.yy322 && yymsp[-1].minor.yy322->nExpr>pParse->db->aLimit[SQLITE_LIMIT_FUNCTION_ARG] ){
    sqlite3ErrorMsg(pParse, "too many arguments on function %T", &yymsp[-4].minor.yy0);
  }
  yylhsminor.yy258.pExpr = sqlite3ExprFunction(pParse, yymsp[-1].minor.yy322, &yymsp[-4].minor.yy0);
  spanSet(&yylhsminor.yy258,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0);
  if( yymsp[-2].minor.yy328==SF_Distinct && yylhsminor.yy258.pExpr ){
    yylhsminor.yy258.pExpr->flags |= EP_Distinct;
  }
}
#line 3198 "parse.c"
  yymsp[-4].minor.yy258 = yylhsminor.yy258;
        break;
      case 167: /* expr ::= ID|INDEXED LP STAR RP */
#line 968 "parse.y"
{
  yylhsminor.yy258.pExpr = sqlite3ExprFunction(pParse, 0, &yymsp[-3].minor.yy0);
  spanSet(&yylhsminor.yy258,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);
}
#line 3207 "parse.c"
  yymsp[-3].minor.yy258 = yylhsminor.yy258;
        break;
      case 168: /* term ::= CTIME_KW */
#line 972 "parse.y"
{
  yylhsminor.yy258.pExpr = sqlite3ExprFunction(pParse, 0, &yymsp[0].minor.yy0);
  spanSet(&yylhsminor.yy258, &yymsp[0].minor.yy0, &yymsp[0].minor.yy0);
}
#line 3216 "parse.c"
  yymsp[0].minor.yy258 = yylhsminor.yy258;
        break;
      case 169: /* expr ::= LP nexprlist COMMA expr RP */
#line 1001 "parse.y"
{
  ExprList *pList = sqlite3ExprListAppend(pParse, yymsp[-3].minor.yy322, yymsp[-1].minor.yy258.pExpr);
  yylhsminor.yy258.pExpr = sqlite3PExpr(pParse, TK_VECTOR, 0, 0);
  if( yylhsminor.yy258.pExpr ){
    yylhsminor.yy258.pExpr->x.pList = pList;
    spanSet(&yylhsminor.yy258, &yymsp[-4].minor.yy0, &yymsp[0].minor.yy0);
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  }
}
#line 3231 "parse.c"
  yymsp[-4].minor.yy258 = yylhsminor.yy258;
        break;
      case 170: /* expr ::= expr AND expr */
      case 171: /* expr ::= expr OR expr */ yytestcase(yyruleno==171);
      case 172: /* expr ::= expr LT|GT|GE|LE expr */ yytestcase(yyruleno==172);
      case 173: /* expr ::= expr EQ|NE expr */ yytestcase(yyruleno==173);
      case 174: /* expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr */ yytestcase(yyruleno==174);
      case 175: /* expr ::= expr PLUS|MINUS expr */ yytestcase(yyruleno==175);
      case 176: /* expr ::= expr STAR|SLASH|REM expr */ yytestcase(yyruleno==176);
      case 177: /* expr ::= expr CONCAT expr */ yytestcase(yyruleno==177);
#line 1012 "parse.y"
{spanBinaryExpr(pParse,yymsp[-1].major,&yymsp[-2].minor.yy258,&yymsp[0].minor.yy258);}
#line 3244 "parse.c"
        break;
      case 178: /* likeop ::= LIKE_KW|MATCH */
#line 1025 "parse.y"
{yymsp[0].minor.yy0=yymsp[0].minor.yy0;/*A-overwrites-X*/}
#line 3249 "parse.c"
        break;
      case 179: /* likeop ::= NOT LIKE_KW|MATCH */
#line 1026 "parse.y"
{yymsp[-1].minor.yy0=yymsp[0].minor.yy0; yymsp[-1].minor.yy0.n|=0x80000000; /*yymsp[-1].minor.yy0-overwrite-yymsp[0].minor.yy0*/}
#line 3254 "parse.c"
        break;
      case 180: /* expr ::= expr likeop expr */
#line 1027 "parse.y"
{
  ExprList *pList;
  int bNot = yymsp[-1].minor.yy0.n & 0x80000000;
  yymsp[-1].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[0].minor.yy258.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-2].minor.yy258.pExpr);
  yymsp[-2].minor.yy258.pExpr = sqlite3ExprFunction(pParse, pList, &yymsp[-1].minor.yy0);
  exprNot(pParse, bNot, &yymsp[-2].minor.yy258);
  yymsp[-2].minor.yy258.zEnd = yymsp[0].minor.yy258.zEnd;
  if( yymsp[-2].minor.yy258.pExpr ) yymsp[-2].minor.yy258.pExpr->flags |= EP_InfixFunc;
}
#line 3269 "parse.c"
        break;
      case 181: /* expr ::= expr likeop expr ESCAPE expr */
#line 1038 "parse.y"
{
  ExprList *pList;
  int bNot = yymsp[-3].minor.yy0.n & 0x80000000;
  yymsp[-3].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy258.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-4].minor.yy258.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy258.pExpr);
  yymsp[-4].minor.yy258.pExpr = sqlite3ExprFunction(pParse, pList, &yymsp[-3].minor.yy0);
  exprNot(pParse, bNot, &yymsp[-4].minor.yy258);
  yymsp[-4].minor.yy258.zEnd = yymsp[0].minor.yy258.zEnd;
  if( yymsp[-4].minor.yy258.pExpr ) yymsp[-4].minor.yy258.pExpr->flags |= EP_InfixFunc;
}
#line 3285 "parse.c"
        break;
      case 182: /* expr ::= expr ISNULL|NOTNULL */
#line 1065 "parse.y"
{spanUnaryPostfix(pParse,yymsp[0].major,&yymsp[-1].minor.yy258,&yymsp[0].minor.yy0);}
#line 3290 "parse.c"
        break;
      case 183: /* expr ::= expr NOT NULL */
#line 1066 "parse.y"
{spanUnaryPostfix(pParse,TK_NOTNULL,&yymsp[-2].minor.yy258,&yymsp[0].minor.yy0);}
#line 3295 "parse.c"
        break;
      case 184: /* expr ::= expr IS expr */
#line 1087 "parse.y"
{
  spanBinaryExpr(pParse,TK_IS,&yymsp[-2].minor.yy258,&yymsp[0].minor.yy258);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy258.pExpr, yymsp[-2].minor.yy258.pExpr, TK_ISNULL);
}
#line 3303 "parse.c"
        break;
      case 185: /* expr ::= expr IS NOT expr */
#line 1091 "parse.y"
{
  spanBinaryExpr(pParse,TK_ISNOT,&yymsp[-3].minor.yy258,&yymsp[0].minor.yy258);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy258.pExpr, yymsp[-3].minor.yy258.pExpr, TK_NOTNULL);
}
#line 3311 "parse.c"
        break;
      case 186: /* expr ::= NOT expr */
      case 187: /* expr ::= BITNOT expr */ yytestcase(yyruleno==187);
#line 1115 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy258,pParse,yymsp[-1].major,&yymsp[0].minor.yy258,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3317 "parse.c"
        break;
      case 188: /* expr ::= MINUS expr */
#line 1119 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy258,pParse,TK_UMINUS,&yymsp[0].minor.yy258,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3322 "parse.c"
        break;
      case 189: /* expr ::= PLUS expr */
#line 1121 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy258,pParse,TK_UPLUS,&yymsp[0].minor.yy258,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3327 "parse.c"
        break;
      case 190: /* between_op ::= BETWEEN */
      case 193: /* in_op ::= IN */ yytestcase(yyruleno==193);
#line 1124 "parse.y"
{yymsp[0].minor.yy328 = 0;}
#line 3333 "parse.c"
        break;
      case 192: /* expr ::= expr between_op expr AND expr */
#line 1126 "parse.y"
{
  ExprList *pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy258.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy258.pExpr);
  yymsp[-4].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_BETWEEN, yymsp[-4].minor.yy258.pExpr, 0);
  if( yymsp[-4].minor.yy258.pExpr ){
    yymsp[-4].minor.yy258.pExpr->x.pList = pList;
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  } 
  exprNot(pParse, yymsp[-3].minor.yy328, &yymsp[-4].minor.yy258);
  yymsp[-4].minor.yy258.zEnd = yymsp[0].minor.yy258.zEnd;
}
#line 3349 "parse.c"
        break;
      case 195: /* expr ::= expr in_op LP exprlist RP */
#line 1142 "parse.y"
{
    if( yymsp[-1].minor.yy322==0 ){
      /* Expressions of the form
      **
      **      expr1 IN ()
      **      expr1 NOT IN ()
      **
      ** simplify to constants 0 (false) and 1 (true), respectively,
      ** regardless of the value of expr1.
      */
      sql_expr_free(pParse->db, yymsp[-4].minor.yy258.pExpr, false);
      yymsp[-4].minor.yy258.pExpr = sqlite3ExprAlloc(pParse->db, TK_INTEGER,0,&sqlite3IntTokens[yymsp[-3].minor.yy328],1);
    }else if( yymsp[-1].minor.yy322->nExpr==1 ){
      /* Expressions of the form:
      **
      **      expr1 IN (?1)
      **      expr1 NOT IN (?2)
      **
      ** with exactly one value on the RHS can be simplified to something
      ** like this:
      **
      **      expr1 == ?1
      **      expr1 <> ?2
      **
      ** But, the RHS of the == or <> is marked with the EP_Generic flag
      ** so that it may not contribute to the computation of comparison
      ** affinity or the collating sequence to use for comparison.  Otherwise,
      ** the semantics would be subtly different from IN or NOT IN.
      */
      Expr *pRHS = yymsp[-1].minor.yy322->a[0].pExpr;
      yymsp[-1].minor.yy322->a[0].pExpr = 0;
      sqlite3ExprListDelete(pParse->db, yymsp[-1].minor.yy322);
      /* pRHS cannot be NULL because a malloc error would have been detected
      ** before now and control would have never reached this point */
      if( ALWAYS(pRHS) ){
        pRHS->flags &= ~EP_Collate;
        pRHS->flags |= EP_Generic;
      }
      yymsp[-4].minor.yy258.pExpr = sqlite3PExpr(pParse, yymsp[-3].minor.yy328 ? TK_NE : TK_EQ, yymsp[-4].minor.yy258.pExpr, pRHS);
    }else{
      yymsp[-4].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy258.pExpr, 0);
      if( yymsp[-4].minor.yy258.pExpr ){
        yymsp[-4].minor.yy258.pExpr->x.pList = yymsp[-1].minor.yy322;
        sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy258.pExpr);
      }else{
        sqlite3ExprListDelete(pParse->db, yymsp[-1].minor.yy322);
      }
      exprNot(pParse, yymsp[-3].minor.yy328, &yymsp[-4].minor.yy258);
    }
    yymsp[-4].minor.yy258.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
  }
#line 3404 "parse.c"
        break;
      case 196: /* expr ::= LP select RP */
#line 1193 "parse.y"
{
    spanSet(&yymsp[-2].minor.yy258,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/
    yymsp[-2].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_SELECT, 0, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-2].minor.yy258.pExpr, yymsp[-1].minor.yy91);
  }
#line 3413 "parse.c"
        break;
      case 197: /* expr ::= expr in_op LP select RP */
#line 1198 "parse.y"
{
    yymsp[-4].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy258.pExpr, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-4].minor.yy258.pExpr, yymsp[-1].minor.yy91);
    exprNot(pParse, yymsp[-3].minor.yy328, &yymsp[-4].minor.yy258);
    yymsp[-4].minor.yy258.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
  }
#line 3423 "parse.c"
        break;
      case 198: /* expr ::= expr in_op nm paren_exprlist */
#line 1204 "parse.y"
{
    SrcList *pSrc = sqlite3SrcListAppend(pParse->db, 0,&yymsp[-1].minor.yy0);
    Select *pSelect = sqlite3SelectNew(pParse, 0,pSrc,0,0,0,0,0,0,0);
    if( yymsp[0].minor.yy322 )  sqlite3SrcListFuncArgs(pParse, pSelect ? pSrc : 0, yymsp[0].minor.yy322);
    yymsp[-3].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-3].minor.yy258.pExpr, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-3].minor.yy258.pExpr, pSelect);
    exprNot(pParse, yymsp[-2].minor.yy328, &yymsp[-3].minor.yy258);
    yymsp[-3].minor.yy258.zEnd = &yymsp[-1].minor.yy0.z[yymsp[-1].minor.yy0.n];
  }
#line 3436 "parse.c"
        break;
      case 199: /* expr ::= EXISTS LP select RP */
#line 1213 "parse.y"
{
    Expr *p;
    spanSet(&yymsp[-3].minor.yy258,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/
    p = yymsp[-3].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_EXISTS, 0, 0);
    sqlite3PExprAddSelect(pParse, p, yymsp[-1].minor.yy91);
  }
#line 3446 "parse.c"
        break;
      case 200: /* expr ::= CASE case_operand case_exprlist case_else END */
#line 1222 "parse.y"
{
  spanSet(&yymsp[-4].minor.yy258,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-C*/
  yymsp[-4].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_CASE, yymsp[-3].minor.yy418, 0);
  if( yymsp[-4].minor.yy258.pExpr ){
    yymsp[-4].minor.yy258.pExpr->x.pList = yymsp[-1].minor.yy418 ? sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy322,yymsp[-1].minor.yy418) : yymsp[-2].minor.yy322;
    sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy258.pExpr);
  }else{
    sqlite3ExprListDelete(pParse->db, yymsp[-2].minor.yy322);
    sql_expr_free(pParse->db, yymsp[-1].minor.yy418, false);
  }
}
#line 3461 "parse.c"
        break;
      case 201: /* case_exprlist ::= case_exprlist WHEN expr THEN expr */
#line 1235 "parse.y"
{
  yymsp[-4].minor.yy322 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy322, yymsp[-2].minor.yy258.pExpr);
  yymsp[-4].minor.yy322 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy322, yymsp[0].minor.yy258.pExpr);
}
#line 3469 "parse.c"
        break;
      case 202: /* case_exprlist ::= WHEN expr THEN expr */
#line 1239 "parse.y"
{
  yymsp[-3].minor.yy322 = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy258.pExpr);
  yymsp[-3].minor.yy322 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy322, yymsp[0].minor.yy258.pExpr);
}
#line 3477 "parse.c"
        break;
      case 205: /* case_operand ::= expr */
#line 1249 "parse.y"
{yymsp[0].minor.yy418 = yymsp[0].minor.yy258.pExpr; /*A-overwrites-X*/}
#line 3482 "parse.c"
        break;
      case 208: /* nexprlist ::= nexprlist COMMA expr */
#line 1260 "parse.y"
{yymsp[-2].minor.yy322 = sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy322,yymsp[0].minor.yy258.pExpr);}
#line 3487 "parse.c"
        break;
      case 209: /* nexprlist ::= expr */
#line 1262 "parse.y"
{yymsp[0].minor.yy322 = sqlite3ExprListAppend(pParse,0,yymsp[0].minor.yy258.pExpr); /*A-overwrites-Y*/}
#line 3492 "parse.c"
        break;
      case 211: /* paren_exprlist ::= LP exprlist RP */
      case 216: /* eidlist_opt ::= LP eidlist RP */ yytestcase(yyruleno==216);
#line 1270 "parse.y"
{yymsp[-2].minor.yy322 = yymsp[-1].minor.yy322;}
#line 3498 "parse.c"
        break;
      case 212: /* cmd ::= createkw uniqueflag INDEX ifnotexists nm ON nm LP sortlist RP where_opt */
#line 1277 "parse.y"
{
  sqlite3CreateIndex(pParse, &yymsp[-6].minor.yy0, 
                     sqlite3SrcListAppend(pParse->db,0,&yymsp[-4].minor.yy0), yymsp[-2].minor.yy322, yymsp[-9].minor.yy328,
                      &yymsp[-10].minor.yy0, yymsp[0].minor.yy418, SQLITE_SO_ASC, yymsp[-7].minor.yy328, SQLITE_IDXTYPE_APPDEF);
}
#line 3507 "parse.c"
        break;
      case 213: /* uniqueflag ::= UNIQUE */
      case 254: /* raisetype ::= ABORT */ yytestcase(yyruleno==254);
#line 1284 "parse.y"
{yymsp[0].minor.yy328 = ON_CONFLICT_ACTION_ABORT;}
#line 3513 "parse.c"
        break;
      case 214: /* uniqueflag ::= */
#line 1285 "parse.y"
{yymsp[1].minor.yy328 = ON_CONFLICT_ACTION_NONE;}
#line 3518 "parse.c"
        break;
      case 217: /* eidlist ::= eidlist COMMA nm collate sortorder */
#line 1328 "parse.y"
{
  yymsp[-4].minor.yy322 = parserAddExprIdListTerm(pParse, yymsp[-4].minor.yy322, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy328, yymsp[0].minor.yy328);
}
#line 3525 "parse.c"
        break;
      case 218: /* eidlist ::= nm collate sortorder */
#line 1331 "parse.y"
{
  yymsp[-2].minor.yy322 = parserAddExprIdListTerm(pParse, 0, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy328, yymsp[0].minor.yy328); /*A-overwrites-Y*/
}
#line 3532 "parse.c"
        break;
      case 221: /* cmd ::= DROP INDEX ifexists fullname ON nm */
#line 1342 "parse.y"
{
    sql_drop_index(pParse, yymsp[-2].minor.yy439, &yymsp[0].minor.yy0, yymsp[-3].minor.yy328);
}
#line 3539 "parse.c"
        break;
      case 222: /* cmd ::= PRAGMA nm */
#line 1349 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[0].minor.yy0,0,0,0);
}
#line 3546 "parse.c"
        break;
      case 223: /* cmd ::= PRAGMA nm EQ nmnum */
#line 1352 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0,0,0);
}
#line 3553 "parse.c"
        break;
      case 224: /* cmd ::= PRAGMA nm LP nmnum RP */
#line 1355 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,&yymsp[-1].minor.yy0,0,0);
}
#line 3560 "parse.c"
        break;
      case 225: /* cmd ::= PRAGMA nm EQ minus_num */
#line 1358 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0,0,1);
}
#line 3567 "parse.c"
        break;
      case 226: /* cmd ::= PRAGMA nm LP minus_num RP */
#line 1361 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,&yymsp[-1].minor.yy0,0,1);
}
#line 3574 "parse.c"
        break;
      case 227: /* cmd ::= PRAGMA nm EQ nm DOT nm */
#line 1364 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0,&yymsp[-2].minor.yy0,0);
}
#line 3581 "parse.c"
        break;
      case 228: /* cmd ::= PRAGMA */
#line 1367 "parse.y"
{
    sqlite3Pragma(pParse, 0,0,0,0);
}
#line 3588 "parse.c"
        break;
      case 231: /* cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END */
#line 1387 "parse.y"
{
  Token all;
  all.z = yymsp[-3].minor.yy0.z;
  all.n = (int)(yymsp[0].minor.yy0.z - yymsp[-3].minor.yy0.z) + yymsp[0].minor.yy0.n;
  pParse->initiateTTrans = false;
  sqlite3FinishTrigger(pParse, yymsp[-1].minor.yy451, &all);
}
#line 3599 "parse.c"
        break;
      case 232: /* trigger_decl ::= TRIGGER ifnotexists nm trigger_time trigger_event ON fullname foreach_clause when_clause */
#line 1397 "parse.y"
{
  sqlite3BeginTrigger(pParse, &yymsp[-6].minor.yy0, yymsp[-5].minor.yy328, yymsp[-4].minor.yy378.a, yymsp[-4].minor.yy378.b, yymsp[-2].minor.yy439, yymsp[0].minor.yy418, yymsp[-7].minor.yy328);
  yymsp[-8].minor.yy0 = yymsp[-6].minor.yy0; /*yymsp[-8].minor.yy0-overwrites-T*/
}
#line 3607 "parse.c"
        break;
      case 233: /* trigger_time ::= BEFORE */
#line 1403 "parse.y"
{ yymsp[0].minor.yy328 = TK_BEFORE; }
#line 3612 "parse.c"
        break;
      case 234: /* trigger_time ::= AFTER */
#line 1404 "parse.y"
{ yymsp[0].minor.yy328 = TK_AFTER;  }
#line 3617 "parse.c"
        break;
      case 235: /* trigger_time ::= INSTEAD OF */
#line 1405 "parse.y"
{ yymsp[-1].minor.yy328 = TK_INSTEAD;}
#line 3622 "parse.c"
        break;
      case 236: /* trigger_time ::= */
#line 1406 "parse.y"
{ yymsp[1].minor.yy328 = TK_BEFORE; }
#line 3627 "parse.c"
        break;
      case 237: /* trigger_event ::= DELETE|INSERT */
      case 238: /* trigger_event ::= UPDATE */ yytestcase(yyruleno==238);
#line 1410 "parse.y"
{yymsp[0].minor.yy378.a = yymsp[0].major; /*A-overwrites-X*/ yymsp[0].minor.yy378.b = 0;}
#line 3633 "parse.c"
        break;
      case 239: /* trigger_event ::= UPDATE OF idlist */
#line 1412 "parse.y"
{yymsp[-2].minor.yy378.a = TK_UPDATE; yymsp[-2].minor.yy378.b = yymsp[0].minor.yy232;}
#line 3638 "parse.c"
        break;
      case 240: /* when_clause ::= */
#line 1419 "parse.y"
{ yymsp[1].minor.yy418 = 0; }
#line 3643 "parse.c"
        break;
      case 241: /* when_clause ::= WHEN expr */
#line 1420 "parse.y"
{ yymsp[-1].minor.yy418 = yymsp[0].minor.yy258.pExpr; }
#line 3648 "parse.c"
        break;
      case 242: /* trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI */
#line 1424 "parse.y"
{
  assert( yymsp[-2].minor.yy451!=0 );
  yymsp[-2].minor.yy451->pLast->pNext = yymsp[-1].minor.yy451;
  yymsp[-2].minor.yy451->pLast = yymsp[-1].minor.yy451;
}
#line 3657 "parse.c"
        break;
      case 243: /* trigger_cmd_list ::= trigger_cmd SEMI */
#line 1429 "parse.y"
{ 
  assert( yymsp[-1].minor.yy451!=0 );
  yymsp[-1].minor.yy451->pLast = yymsp[-1].minor.yy451;
}
#line 3665 "parse.c"
        break;
      case 244: /* trnm ::= nm DOT nm */
#line 1440 "parse.y"
{
  yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;
  sqlite3ErrorMsg(pParse, 
        "qualified table names are not allowed on INSERT, UPDATE, and DELETE "
        "statements within triggers");
}
#line 3675 "parse.c"
        break;
      case 245: /* tridxby ::= INDEXED BY nm */
#line 1452 "parse.y"
{
  sqlite3ErrorMsg(pParse,
        "the INDEXED BY clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
#line 3684 "parse.c"
        break;
      case 246: /* tridxby ::= NOT INDEXED */
#line 1457 "parse.y"
{
  sqlite3ErrorMsg(pParse,
        "the NOT INDEXED clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
#line 3693 "parse.c"
        break;
      case 247: /* trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt */
#line 1470 "parse.y"
{yymsp[-6].minor.yy451 = sqlite3TriggerUpdateStep(pParse->db, &yymsp[-4].minor.yy0, yymsp[-1].minor.yy322, yymsp[0].minor.yy418, yymsp[-5].minor.yy328);}
#line 3698 "parse.c"
        break;
      case 248: /* trigger_cmd ::= insert_cmd INTO trnm idlist_opt select */
#line 1474 "parse.y"
{yymsp[-4].minor.yy451 = sqlite3TriggerInsertStep(pParse->db, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy232, yymsp[0].minor.yy91, yymsp[-4].minor.yy328);/*A-overwrites-R*/}
#line 3703 "parse.c"
        break;
      case 249: /* trigger_cmd ::= DELETE FROM trnm tridxby where_opt */
#line 1478 "parse.y"
{yymsp[-4].minor.yy451 = sqlite3TriggerDeleteStep(pParse->db, &yymsp[-2].minor.yy0, yymsp[0].minor.yy418);}
#line 3708 "parse.c"
        break;
      case 250: /* trigger_cmd ::= select */
#line 1482 "parse.y"
{yymsp[0].minor.yy451 = sqlite3TriggerSelectStep(pParse->db, yymsp[0].minor.yy91); /*A-overwrites-X*/}
#line 3713 "parse.c"
        break;
      case 251: /* expr ::= RAISE LP IGNORE RP */
#line 1485 "parse.y"
{
  spanSet(&yymsp[-3].minor.yy258,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-X*/
  yymsp[-3].minor.yy258.pExpr = sqlite3PExpr(pParse, TK_RAISE, 0, 0); 
  if( yymsp[-3].minor.yy258.pExpr ){
//FIXME    yymsp[-3].minor.yy258.pExpr->affinity = ON_CONFLICT_ACTION_IGNORE;
  }
}
#line 3724 "parse.c"
        break;
      case 252: /* expr ::= RAISE LP raisetype COMMA STRING RP */
#line 1492 "parse.y"
{
  spanSet(&yymsp[-5].minor.yy258,&yymsp[-5].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-X*/
  yymsp[-5].minor.yy258.pExpr = sqlite3ExprAlloc(pParse->db, TK_RAISE, 0, &yymsp[-1].minor.yy0, 1); 
  if( yymsp[-5].minor.yy258.pExpr ) {
//FIXME    yymsp[-5].minor.yy258.pExpr->affinity = (char)yymsp[-3].minor.yy328;
  }
}
#line 3735 "parse.c"
        break;
      case 253: /* raisetype ::= ROLLBACK */
#line 1502 "parse.y"
{yymsp[0].minor.yy328 = ON_CONFLICT_ACTION_ROLLBACK;}
#line 3740 "parse.c"
        break;
      case 255: /* raisetype ::= FAIL */
#line 1504 "parse.y"
{yymsp[0].minor.yy328 = ON_CONFLICT_ACTION_FAIL;}
#line 3745 "parse.c"
        break;
      case 256: /* cmd ::= DROP TRIGGER ifexists fullname */
#line 1509 "parse.y"
{
  sqlite3DropTrigger(pParse,yymsp[0].minor.yy439,yymsp[-1].minor.yy328);
}
#line 3752 "parse.c"
        break;
      case 257: /* cmd ::= ANALYZE */
#line 1524 "parse.y"
{sqlite3Analyze(pParse, 0);}
#line 3757 "parse.c"
        break;
      case 258: /* cmd ::= ANALYZE nm */
#line 1525 "parse.y"
{sqlite3Analyze(pParse, &yymsp[0].minor.yy0);}
#line 3762 "parse.c"
        break;
      case 259: /* cmd ::= ALTER TABLE fullname RENAME TO nm */
#line 1530 "parse.y"
{
  sqlite3AlterRenameTable(pParse,yymsp[-3].minor.yy439,&yymsp[0].minor.yy0);
}
#line 3769 "parse.c"
        break;
      case 260: /* with ::= */
#line 1553 "parse.y"
{yymsp[1].minor.yy323 = 0;}
#line 3774 "parse.c"
        break;
      case 261: /* with ::= WITH wqlist */
#line 1555 "parse.y"
{ yymsp[-1].minor.yy323 = yymsp[0].minor.yy323; }
#line 3779 "parse.c"
        break;
      case 262: /* with ::= WITH RECURSIVE wqlist */
#line 1556 "parse.y"
{ yymsp[-2].minor.yy323 = yymsp[0].minor.yy323; }
#line 3784 "parse.c"
        break;
      case 263: /* wqlist ::= nm eidlist_opt AS LP select RP */
#line 1558 "parse.y"
{
  yymsp[-5].minor.yy323 = sqlite3WithAdd(pParse, 0, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy322, yymsp[-1].minor.yy91); /*A-overwrites-X*/
}
#line 3791 "parse.c"
        break;
      case 264: /* wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP */
#line 1561 "parse.y"
{
  yymsp[-7].minor.yy323 = sqlite3WithAdd(pParse, yymsp[-7].minor.yy323, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy322, yymsp[-1].minor.yy91);
}
#line 3798 "parse.c"
        break;
      default:
      /* (265) input ::= ecmd */ yytestcase(yyruleno==265);
      /* (266) explain ::= */ yytestcase(yyruleno==266);
      /* (267) cmdx ::= cmd (OPTIMIZED OUT) */ assert(yyruleno!=267);
      /* (268) trans_opt ::= */ yytestcase(yyruleno==268);
      /* (269) trans_opt ::= TRANSACTION */ yytestcase(yyruleno==269);
      /* (270) trans_opt ::= TRANSACTION nm */ yytestcase(yyruleno==270);
      /* (271) savepoint_opt ::= SAVEPOINT */ yytestcase(yyruleno==271);
      /* (272) savepoint_opt ::= */ yytestcase(yyruleno==272);
      /* (273) cmd ::= create_table create_table_args */ yytestcase(yyruleno==273);
      /* (274) columnlist ::= columnlist COMMA columnname carglist */ yytestcase(yyruleno==274);
      /* (275) columnlist ::= columnname carglist */ yytestcase(yyruleno==275);
      /* (276) typedef ::= numbertypedef (OPTIMIZED OUT) */ assert(yyruleno!=276);
      /* (277) carglist ::= carglist ccons */ yytestcase(yyruleno==277);
      /* (278) carglist ::= */ yytestcase(yyruleno==278);
      /* (279) ccons ::= NULL onconf */ yytestcase(yyruleno==279);
      /* (280) conslist_opt ::= COMMA conslist */ yytestcase(yyruleno==280);
      /* (281) conslist ::= conslist tconscomma tcons */ yytestcase(yyruleno==281);
      /* (282) conslist ::= tcons (OPTIMIZED OUT) */ assert(yyruleno!=282);
      /* (283) tconscomma ::= */ yytestcase(yyruleno==283);
      /* (284) defer_subclause_opt ::= defer_subclause (OPTIMIZED OUT) */ assert(yyruleno!=284);
      /* (285) resolvetype ::= raisetype (OPTIMIZED OUT) */ assert(yyruleno!=285);
      /* (286) selectnowith ::= oneselect (OPTIMIZED OUT) */ assert(yyruleno!=286);
      /* (287) oneselect ::= values */ yytestcase(yyruleno==287);
      /* (288) sclp ::= selcollist COMMA */ yytestcase(yyruleno==288);
      /* (289) as ::= ID|STRING */ yytestcase(yyruleno==289);
      /* (290) join_nm ::= ID|INDEXED */ yytestcase(yyruleno==290);
      /* (291) join_nm ::= JOIN_KW */ yytestcase(yyruleno==291);
      /* (292) expr ::= term (OPTIMIZED OUT) */ assert(yyruleno!=292);
      /* (293) exprlist ::= nexprlist */ yytestcase(yyruleno==293);
      /* (294) nmnum ::= plus_num (OPTIMIZED OUT) */ assert(yyruleno!=294);
      /* (295) nmnum ::= STRING */ yytestcase(yyruleno==295);
      /* (296) nmnum ::= nm */ yytestcase(yyruleno==296);
      /* (297) nmnum ::= ON */ yytestcase(yyruleno==297);
      /* (298) nmnum ::= DELETE */ yytestcase(yyruleno==298);
      /* (299) nmnum ::= DEFAULT */ yytestcase(yyruleno==299);
      /* (300) plus_num ::= INTEGER|FLOAT */ yytestcase(yyruleno==300);
      /* (301) foreach_clause ::= */ yytestcase(yyruleno==301);
      /* (302) foreach_clause ::= FOR EACH ROW */ yytestcase(yyruleno==302);
      /* (303) trnm ::= nm */ yytestcase(yyruleno==303);
      /* (304) tridxby ::= */ yytestcase(yyruleno==304);
        break;
/********** End reduce actions ************************************************/
  };
  assert( yyruleno<sizeof(yyRuleInfo)/sizeof(yyRuleInfo[0]) );
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact <= YY_MAX_SHIFTREDUCE ){
    if( yyact>YY_MAX_SHIFT ){
      yyact += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
    }
    yymsp -= yysize-1;
    yypParser->yytos = yymsp;
    yymsp->stateno = (YYACTIONTYPE)yyact;
    yymsp->major = (YYCODETYPE)yygoto;
    yyTraceShift(yypParser, yyact);
  }else{
    assert( yyact == YY_ACCEPT_ACTION );
    yypParser->yytos -= yysize;
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
/************ Begin %parse_failure code ***************************************/
/************ End %parse_failure code *****************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  sqlite3ParserTOKENTYPE yyminor         /* The minor type of the error token */
){
  sqlite3ParserARG_FETCH;
#define TOKEN yyminor
/************ Begin %syntax_error code ****************************************/
#line 32 "parse.y"

  UNUSED_PARAMETER(yymajor);  /* Silence some compiler warnings */
  assert( TOKEN.z[0] );  /* The tokenizer always gives us a token */
  if (yypParser->is_fallback_failed && TOKEN.isReserved) {
    sqlite3ErrorMsg(pParse, "keyword \"%T\" is reserved", &TOKEN);
  } else {
    sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &TOKEN);
  }
#line 3906 "parse.c"
/************ End %syntax_error code ******************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
#ifndef YYNOERRORRECOVERY
  yypParser->yyerrcnt = -1;
#endif
  assert( yypParser->yytos==yypParser->yystack );
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
/*********** Begin %parse_accept code *****************************************/
/*********** End %parse_accept code *******************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "sqlite3ParserAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3Parser(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  sqlite3ParserTOKENTYPE yyminor       /* The value for the token */
  sqlite3ParserARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  unsigned int yyact;   /* The parser action. */
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  int yyendofinput;     /* True if we are at the end of input */
#endif
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  yypParser = (yyParser*)yyp;
  assert( yypParser->yytos!=0 );
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  yyendofinput = (yymajor==0);
#endif
  sqlite3ParserARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput '%s'\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);
    if( yyact <= YY_MAX_SHIFTREDUCE ){
      yy_shift(yypParser,yyact,yymajor,yyminor);
#ifndef YYNOERRORRECOVERY
      yypParser->yyerrcnt--;
#endif
      yymajor = YYNOCODE;
    }else if( yyact <= YY_MAX_REDUCE ){
      yy_reduce(yypParser,yyact-YY_MIN_REDUCE);
    }else{
      assert( yyact == YY_ERROR_ACTION );
      yyminorunion.yy0 = yyminor;
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminor);
      }
      yymx = yypParser->yytos->major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor, &yyminorunion);
        yymajor = YYNOCODE;
      }else{
        while( yypParser->yytos >= yypParser->yystack
            && yymx != YYERRORSYMBOL
            && (yyact = yy_find_reduce_action(
                        yypParser->yytos->stateno,
                        YYERRORSYMBOL)) >= YY_MIN_REDUCE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yytos < yypParser->yystack || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
          yypParser->yyerrcnt = -1;
#endif
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          yy_shift(yypParser,yyact,YYERRORSYMBOL,yyminor);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor, yyminor);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;
      
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor, yyminor);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
        yypParser->yyerrcnt = -1;
#endif
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yytos>yypParser->yystack );
#ifndef NDEBUG
  if( yyTraceFILE ){
    yyStackEntry *i;
    char cDiv = '[';
    fprintf(yyTraceFILE,"%sReturn. Stack=",yyTracePrompt);
    for(i=&yypParser->yystack[1]; i<=yypParser->yytos; i++){
      fprintf(yyTraceFILE,"%c%s", cDiv, yyTokenName[i->major]);
      cDiv = ' ';
    }
    fprintf(yyTraceFILE,"]\n");
  }
#endif
  return;
}
