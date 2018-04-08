#define TK_SEMI                             1
#define TK_EXPLAIN                          2
#define TK_QUERY                            3
#define TK_PLAN                             4
#define TK_OR                               5
#define TK_AND                              6
#define TK_NOT                              7
#define TK_IS                               8
#define TK_MATCH                            9
#define TK_LIKE_KW                         10
#define TK_BETWEEN                         11
#define TK_IN                              12
#define TK_ISNULL                          13
#define TK_NOTNULL                         14
#define TK_NE                              15
#define TK_EQ                              16
#define TK_GT                              17
#define TK_LE                              18
#define TK_LT                              19
#define TK_GE                              20
#define TK_ESCAPE                          21
#define TK_BITAND                          22
#define TK_BITOR                           23
#define TK_LSHIFT                          24
#define TK_RSHIFT                          25
#define TK_PLUS                            26
#define TK_MINUS                           27
#define TK_STAR                            28
#define TK_SLASH                           29
#define TK_REM                             30
#define TK_CONCAT                          31
#define TK_COLLATE                         32
#define TK_BITNOT                          33
#define TK_BEGIN                           34
#define TK_TRANSACTION                     35
#define TK_DEFERRED                        36
#define TK_COMMIT                          37
#define TK_END                             38
#define TK_ROLLBACK                        39
#define TK_SAVEPOINT                       40
#define TK_RELEASE                         41
#define TK_TO                              42
#define TK_TABLE                           43
#define TK_CREATE                          44
#define TK_IF                              45
#define TK_EXISTS                          46
#define TK_LP                              47
#define TK_RP                              48
#define TK_AS                              49
#define TK_COMMA                           50
#define TK_ID                              51
#define TK_INDEXED                         52
#define TK_ABORT                           53
#define TK_ACTION                          54
#define TK_ADD                             55
#define TK_AFTER                           56
#define TK_AUTOINCREMENT                   57
#define TK_BEFORE                          58
#define TK_CASCADE                         59
#define TK_CONFLICT                        60
#define TK_FAIL                            61
#define TK_IGNORE                          62
#define TK_INITIALLY                       63
#define TK_INSTEAD                         64
#define TK_NO                              65
#define TK_KEY                             66
#define TK_OFFSET                          67
#define TK_RAISE                           68
#define TK_REPLACE                         69
#define TK_RESTRICT                        70
#define TK_REINDEX                         71
#define TK_RENAME                          72
#define TK_CTIME_KW                        73
#define TK_ANY                             74
#define TK_STRING                          75
#define TK_TEXT                            76
#define TK_BLOB                            77
#define TK_DATE                            78
#define TK_TIME                            79
#define TK_DATETIME                        80
#define TK_CHAR                            81
#define TK_VARCHAR                         82
#define TK_INTEGER                         83
#define TK_UNSIGNED                        84
#define TK_FLOAT                           85
#define TK_INT                             86
#define TK_DECIMAL                         87
#define TK_NUMERIC                         88
#define TK_CONSTRAINT                      89
#define TK_DEFAULT                         90
#define TK_NULL                            91
#define TK_PRIMARY                         92
#define TK_UNIQUE                          93
#define TK_CHECK                           94
#define TK_REFERENCES                      95
#define TK_AUTOINCR                        96
#define TK_ON                              97
#define TK_INSERT                          98
#define TK_DELETE                          99
#define TK_UPDATE                         100
#define TK_SET                            101
#define TK_DEFERRABLE                     102
#define TK_IMMEDIATE                      103
#define TK_FOREIGN                        104
#define TK_DROP                           105
#define TK_VIEW                           106
#define TK_UNION                          107
#define TK_ALL                            108
#define TK_EXCEPT                         109
#define TK_INTERSECT                      110
#define TK_SELECT                         111
#define TK_VALUES                         112
#define TK_DISTINCT                       113
#define TK_DOT                            114
#define TK_FROM                           115
#define TK_JOIN_KW                        116
#define TK_JOIN                           117
#define TK_BY                             118
#define TK_USING                          119
#define TK_ORDER                          120
#define TK_ASC                            121
#define TK_DESC                           122
#define TK_GROUP                          123
#define TK_HAVING                         124
#define TK_LIMIT                          125
#define TK_WHERE                          126
#define TK_INTO                           127
#define TK_VARIABLE                       128
#define TK_CAST                           129
#define TK_CASE                           130
#define TK_WHEN                           131
#define TK_THEN                           132
#define TK_ELSE                           133
#define TK_INDEX                          134
#define TK_PRAGMA                         135
#define TK_TRIGGER                        136
#define TK_OF                             137
#define TK_FOR                            138
#define TK_EACH                           139
#define TK_ROW                            140
#define TK_ANALYZE                        141
#define TK_ALTER                          142
#define TK_WITH                           143
#define TK_RECURSIVE                      144
#define TK_STANDARD                       145
#define TK_TO_TEXT                        146
#define TK_TO_BLOB                        147
#define TK_TO_NUMERIC                     148
#define TK_TO_INT                         149
#define TK_TO_REAL                        150
#define TK_ISNOT                          151
#define TK_END_OF_FILE                    152
#define TK_UNCLOSED_STRING                153
#define TK_FUNCTION                       154
#define TK_COLUMN                         155
#define TK_AGG_FUNCTION                   156
#define TK_AGG_COLUMN                     157
#define TK_UMINUS                         158
#define TK_UPLUS                          159
#define TK_REGISTER                       160
#define TK_VECTOR                         161
#define TK_SELECT_COLUMN                  162
#define TK_ASTERISK                       163
#define TK_SPAN                           164
#define TK_SPACE                          165
#define TK_ILLEGAL                        166

/* The token codes above must all fit in 8 bits */
#define TKFLG_MASK           0xff  

/* Flags that can be added to a token code when it is not
** being stored in a u8: */
#define TKFLG_DONTFOLD       0x100  /* Omit constant folding optimizations */
