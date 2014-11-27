
#pragma once

/* true or false value macro */
#define istrue(p)       ((p) != NIL && (p) != F)
#define isfalse(p)      ((p) == NIL || (p) == F)

/* Error macro */
#define BEGIN do {
#define END } while (0)

#define Error_0(s) \
    BEGIN \
        args = cons(mk_string((s)), NIL); \
        operator = (short)OP_ERR0; \
        return T; \
    END

#define Error_1(s, a) \
    BEGIN \
        args = cons((a), NIL); \
        args = cons(mk_string((s)), args); \
        operator = (short)OP_ERR0; \
        return T; \
    END

/* control macros for Eval_Cycle */
#define s_goto(a) \
    BEGIN  \
        operator = (short)(a); \
        return T; \
    END

#define s_save(a, b, c) \
    (  \
        dump = cons(envir, cons((c), dump)), \
        dump = cons((b), dump), \
        dump = cons(mk_number((long)(a)), dump) \
    )

#define s_return(a) \
    BEGIN \
        value = (a); \
        operator = ivalue(car(dump)); \
        args = cadr(dump); \
        envir = caddr(dump); \
        code = cadddr(dump); \
        dump = cddddr(dump); \
        return T; \
    END

#define s_retbool(tf)    s_return((tf) ? T : F)

/* ========== Evaluation Cycle ========== */
/* operator code */

#define    OP_LOAD            0
#define    OP_T0LVL        1
#define    OP_T1LVL        2
#define    OP_READ            3
#define    OP_VALUEPRINT        4
#define    OP_EVAL            5
#define    OP_E0ARGS        6
#define    OP_E1ARGS        7
#define    OP_APPLY        8
#define    OP_DOMACRO        9
#define    OP_LAMBDA        10
#define    OP_QUOTE        11
#define    OP_DEF0            12
#define    OP_DEF1            13
#define    OP_BEGIN        14
#define    OP_IF0            15
#define    OP_IF1            16
#define    OP_SET0            17
#define    OP_SET1            18
#define    OP_LET0            19
#define    OP_LET1            20
#define    OP_LET2            21
#define    OP_LET0AST        22
#define    OP_LET1AST        23
#define    OP_LET2AST        24
#define    OP_LET0REC        25
#define    OP_LET1REC        26
#define    OP_LET2REC        27
#define    OP_COND0        28
#define    OP_COND1        29
#define    OP_DELAY        30
#define    OP_AND0            31
#define    OP_AND1            32
#define    OP_OR0            33
#define    OP_OR1            34
#define    OP_C0STREAM        35
#define    OP_C1STREAM        36
#define    OP_0MACRO        37
#define    OP_1MACRO        38
#define    OP_CASE0        39
#define    OP_CASE1        40
#define    OP_CASE2        41
#define    OP_PEVAL        42
#define    OP_PAPPLY        43
#define    OP_CONTINUATION        44
#define    OP_ADD            45
#define    OP_SUB            46
#define    OP_MUL            47
#define    OP_DIV            48
#define    OP_REM            49
#define    OP_CAR            50
#define    OP_CDR            51
#define    OP_CONS            52
#define    OP_SETCAR        53
#define    OP_SETCDR        54
#define    OP_NOT            55
#define    OP_BOOL            56
#define    OP_NULL            57
#define    OP_ZEROP        58
#define    OP_POSP            59
#define    OP_NEGP            60
#define    OP_NEQ            61
#define    OP_LESS            62
#define    OP_GRE            63
#define    OP_LEQ            64
#define    OP_GEQ            65
#define    OP_SYMBOL        66
#define    OP_NUMBER        67
#define    OP_STRING        68
#define    OP_PROC            69
#define    OP_PAIR            70
#define    OP_EQ            71
#define    OP_EQV            72
#define    OP_FORCE        73
#define    OP_WRITE        74
#define    OP_DISPLAY        75
#define    OP_NEWLINE        76
#define    OP_ERR0            77
#define    OP_ERR1            78
#define    OP_REVERSE        79
#define    OP_APPEND        80
#define    OP_PUT            81
#define    OP_GET            82
#define    OP_QUIT            83
#define    OP_GC            84
#define    OP_GCVERB        85
#define    OP_NEWSEGMENT        86
#define    OP_RDSEXPR        87
#define    OP_RDLIST        88
#define    OP_RDDOT        89
#define    OP_RDQUOTE        90
#define    OP_RDQQUOTE        91
#define    OP_RDUNQUOTE        92
#define    OP_RDUQTSP        93
#define    OP_P0LIST        94
#define    OP_P1LIST        95
#define    OP_LIST_LENGTH        96
#define    OP_ASSQ            97
#define    OP_PRINT_WIDTH        98
#define    OP_P0_WIDTH        99
#define    OP_P1_WIDTH        100
#define    OP_GET_CLOSURE        101
#define    OP_CLOSUREP        102
#define    OP_MACROP        103
