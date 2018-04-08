/* Automatically generated.  Do not edit */
/* See the tool/mkopcodec.tcl script for details. */
#include "sqliteInt.h"
#if !defined(SQLITE_OMIT_EXPLAIN) \
 || defined(VDBE_PROFILE) \
 || defined(SQLITE_DEBUG)
#if defined(SQLITE_ENABLE_EXPLAIN_COMMENTS) || defined(SQLITE_DEBUG)
# define OpHelp(X) "\0" X
#else
# define OpHelp(X)
#endif
const char *sqlite3OpcodeName(int i){
 static const char *const azName[] = {
    /*   0 */ "Savepoint"        OpHelp(""),
    /*   1 */ "AutoCommit"       OpHelp(""),
    /*   2 */ "SorterNext"       OpHelp(""),
    /*   3 */ "PrevIfOpen"       OpHelp(""),
    /*   4 */ "NextIfOpen"       OpHelp(""),
    /*   5 */ "Or"               OpHelp("r[P3]=(r[P1] || r[P2])"),
    /*   6 */ "And"              OpHelp("r[P3]=(r[P1] && r[P2])"),
    /*   7 */ "Not"              OpHelp("r[P2]= !r[P1]"),
    /*   8 */ "Prev"             OpHelp(""),
    /*   9 */ "Next"             OpHelp(""),
    /*  10 */ "Goto"             OpHelp(""),
    /*  11 */ "Gosub"            OpHelp(""),
    /*  12 */ "InitCoroutine"    OpHelp(""),
    /*  13 */ "IsNull"           OpHelp("if r[P1]==NULL goto P2"),
    /*  14 */ "NotNull"          OpHelp("if r[P1]!=NULL goto P2"),
    /*  15 */ "Ne"               OpHelp("IF r[P3]!=r[P1]"),
    /*  16 */ "Eq"               OpHelp("IF r[P3]==r[P1]"),
    /*  17 */ "Gt"               OpHelp("IF r[P3]>r[P1]"),
    /*  18 */ "Le"               OpHelp("IF r[P3]<=r[P1]"),
    /*  19 */ "Lt"               OpHelp("IF r[P3]<r[P1]"),
    /*  20 */ "Ge"               OpHelp("IF r[P3]>=r[P1]"),
    /*  21 */ "ElseNotEq"        OpHelp(""),
    /*  22 */ "BitAnd"           OpHelp("r[P3]=r[P1]&r[P2]"),
    /*  23 */ "BitOr"            OpHelp("r[P3]=r[P1]|r[P2]"),
    /*  24 */ "ShiftLeft"        OpHelp("r[P3]=r[P2]<<r[P1]"),
    /*  25 */ "ShiftRight"       OpHelp("r[P3]=r[P2]>>r[P1]"),
    /*  26 */ "Add"              OpHelp("r[P3]=r[P1]+r[P2]"),
    /*  27 */ "Subtract"         OpHelp("r[P3]=r[P2]-r[P1]"),
    /*  28 */ "Multiply"         OpHelp("r[P3]=r[P1]*r[P2]"),
    /*  29 */ "Divide"           OpHelp("r[P3]=r[P2]/r[P1]"),
    /*  30 */ "Remainder"        OpHelp("r[P3]=r[P2]%r[P1]"),
    /*  31 */ "Concat"           OpHelp("r[P3]=r[P2]+r[P1]"),
    /*  32 */ "Yield"            OpHelp(""),
    /*  33 */ "BitNot"           OpHelp("r[P1]= ~r[P1]"),
    /*  34 */ "MustBeInt"        OpHelp(""),
    /*  35 */ "Jump"             OpHelp(""),
    /*  36 */ "Once"             OpHelp(""),
    /*  37 */ "If"               OpHelp(""),
    /*  38 */ "IfNot"            OpHelp(""),
    /*  39 */ "SeekLT"           OpHelp("key=r[P3@P4]"),
    /*  40 */ "SeekLE"           OpHelp("key=r[P3@P4]"),
    /*  41 */ "SeekGE"           OpHelp("key=r[P3@P4]"),
    /*  42 */ "SeekGT"           OpHelp("key=r[P3@P4]"),
    /*  43 */ "NoConflict"       OpHelp("key=r[P3@P4]"),
    /*  44 */ "NotFound"         OpHelp("key=r[P3@P4]"),
    /*  45 */ "Found"            OpHelp("key=r[P3@P4]"),
    /*  46 */ "Last"             OpHelp(""),
    /*  47 */ "SorterSort"       OpHelp(""),
    /*  48 */ "Sort"             OpHelp(""),
    /*  49 */ "Rewind"           OpHelp(""),
    /*  50 */ "IdxLE"            OpHelp("key=r[P3@P4]"),
    /*  51 */ "IdxGT"            OpHelp("key=r[P3@P4]"),
    /*  52 */ "IdxLT"            OpHelp("key=r[P3@P4]"),
    /*  53 */ "IdxGE"            OpHelp("key=r[P3@P4]"),
    /*  54 */ "Program"          OpHelp(""),
    /*  55 */ "FkIfZero"         OpHelp("if fkctr[P1]==0 goto P2"),
    /*  56 */ "IfPos"            OpHelp("if r[P1]>0 then r[P1]-=P3, goto P2"),
    /*  57 */ "IfNotZero"        OpHelp("if r[P1]!=0 then r[P1]--, goto P2"),
    /*  58 */ "DecrJumpZero"     OpHelp("if (--r[P1])==0 goto P2"),
    /*  59 */ "Init"             OpHelp("Start at P2"),
    /*  60 */ "Return"           OpHelp(""),
    /*  61 */ "EndCoroutine"     OpHelp(""),
    /*  62 */ "HaltIfNull"       OpHelp("if r[P3]=null halt"),
    /*  63 */ "Halt"             OpHelp(""),
    /*  64 */ "Integer"          OpHelp("r[P2]=P1"),
    /*  65 */ "Bool"             OpHelp("r[P2]=P1"),
    /*  66 */ "Int64"            OpHelp("r[P2]=P4"),
    /*  67 */ "LoadPtr"          OpHelp("r[P2] = P4"),
    /*  68 */ "String"           OpHelp("r[P2]='P4' (len=P1)"),
    /*  69 */ "NextAutoincValue" OpHelp("r[P2] = next value from space sequence, which pageno is r[P1]"),
    /*  70 */ "Null"             OpHelp("r[P2..P3]=NULL"),
    /*  71 */ "SoftNull"         OpHelp("r[P1]=NULL"),
    /*  72 */ "Blob"             OpHelp("r[P2]=P4 (len=P1, subtype=P3)"),
    /*  73 */ "Variable"         OpHelp("r[P2]=parameter(P1,P4)"),
    /*  74 */ "Move"             OpHelp("r[P2@P3]=r[P1@P3]"),
    /*  75 */ "String8"          OpHelp("r[P2]='P4'"),
    /*  76 */ "Copy"             OpHelp("r[P2@P3+1]=r[P1@P3+1]"),
    /*  77 */ "SCopy"            OpHelp("r[P2]=r[P1]"),
    /*  78 */ "IntCopy"          OpHelp("r[P2]=r[P1]"),
    /*  79 */ "ResultRow"        OpHelp("output=r[P1@P2]"),
    /*  80 */ "CollSeq"          OpHelp(""),
    /*  81 */ "Function0"        OpHelp("r[P3]=func(r[P2@P5])"),
    /*  82 */ "Function"         OpHelp("r[P3]=func(r[P2@P5])"),
    /*  83 */ "AddImm"           OpHelp("r[P1]=r[P1]+P2"),
    /*  84 */ "RealAffinity"     OpHelp(""),
    /*  85 */ "Real"             OpHelp("r[P2]=P4"),
    /*  86 */ "Cast"             OpHelp("affinity(r[P1])"),
    /*  87 */ "Permutation"      OpHelp(""),
    /*  88 */ "Compare"          OpHelp("r[P1@P3] <-> r[P2@P3]"),
    /*  89 */ "Column"           OpHelp("r[P3]=PX"),
    /*  90 */ "Affinity"         OpHelp("affinity(r[P1@P2])"),
    /*  91 */ "MakeRecord"       OpHelp("r[P3]=mkrec(r[P1@P2])"),
    /*  92 */ "Count"            OpHelp("r[P2]=count()"),
    /*  93 */ "FkCheckCommit"    OpHelp(""),
    /*  94 */ "TTransaction"     OpHelp(""),
    /*  95 */ "ReadCookie"       OpHelp(""),
    /*  96 */ "SetCookie"        OpHelp(""),
    /*  97 */ "ReopenIdx"        OpHelp("index id = P2, space ptr = P3"),
    /*  98 */ "OpenRead"         OpHelp("index id = P2, space ptr = P3"),
    /*  99 */ "OpenWrite"        OpHelp("index id = P2, space ptr = P3"),
    /* 100 */ "OpenTEphemeral"   OpHelp("nColumn = P2"),
    /* 101 */ "SorterOpen"       OpHelp(""),
    /* 102 */ "SequenceTest"     OpHelp("if (cursor[P1].ctr++) pc = P2"),
    /* 103 */ "OpenPseudo"       OpHelp("P3 columns in r[P2]"),
    /* 104 */ "Close"            OpHelp(""),
    /* 105 */ "ColumnsUsed"      OpHelp(""),
    /* 106 */ "Sequence"         OpHelp("r[P2]=cursor[P1].ctr++"),
    /* 107 */ "NextSequenceId"   OpHelp("r[P2]=get_max(_sequence)"),
    /* 108 */ "NextIdEphemeral"  OpHelp("r[P3]=get_max(space_index[P1]{Column[P2]})"),
    /* 109 */ "FCopy"            OpHelp("reg[P2@cur_frame]= reg[P1@root_frame(OPFLAG_SAME_FRAME)]"),
    /* 110 */ "Delete"           OpHelp(""),
    /* 111 */ "ResetCount"       OpHelp(""),
    /* 112 */ "SorterCompare"    OpHelp("if key(P1)!=trim(r[P3],P4) goto P2"),
    /* 113 */ "SorterData"       OpHelp("r[P2]=data"),
    /* 114 */ "RowData"          OpHelp("r[P2]=data"),
    /* 115 */ "NullRow"          OpHelp(""),
    /* 116 */ "SorterInsert"     OpHelp("key=r[P2]"),
    /* 117 */ "IdxReplace"       OpHelp("key=r[P2]"),
    /* 118 */ "IdxInsert"        OpHelp("key=r[P2]"),
    /* 119 */ "SInsert"          OpHelp("space id = P1, key = r[P2]"),
    /* 120 */ "SDelete"          OpHelp("space id = P1, key = r[P2]"),
    /* 121 */ "SIDtoPtr"         OpHelp("space id = P1, space[out] = r[P2]"),
    /* 122 */ "IdxDelete"        OpHelp("key=r[P2@P3]"),
    /* 123 */ "Clear"            OpHelp("space id = P1"),
    /* 124 */ "ResetSorter"      OpHelp(""),
    /* 125 */ "ParseSchema2"     OpHelp("rows=r[P1@P2]"),
    /* 126 */ "ParseSchema3"     OpHelp("name=r[P1] sql=r[P1+1]"),
    /* 127 */ "RenameTable"      OpHelp("P1 = root, P4 = name"),
    /* 128 */ "LoadAnalysis"     OpHelp(""),
    /* 129 */ "DropTable"        OpHelp(""),
    /* 130 */ "DropIndex"        OpHelp(""),
    /* 131 */ "DropTrigger"      OpHelp(""),
    /* 132 */ "Param"            OpHelp(""),
    /* 133 */ "FkCounter"        OpHelp("fkctr[P1]+=P2"),
    /* 134 */ "OffsetLimit"      OpHelp("if r[P1]>0 then r[P2]=r[P1]+max(0,r[P3]) else r[P2]=(-1)"),
    /* 135 */ "AggStep0"         OpHelp("accum=r[P3] step(r[P2@P5])"),
    /* 136 */ "AggStep"          OpHelp("accum=r[P3] step(r[P2@P5])"),
    /* 137 */ "AggFinal"         OpHelp("accum=r[P1] N=P2"),
    /* 138 */ "Expire"           OpHelp(""),
    /* 139 */ "IncMaxid"         OpHelp(""),
    /* 140 */ "Noop"             OpHelp(""),
    /* 141 */ "Explain"          OpHelp(""),
  };
  return azName[i];
}
#endif
