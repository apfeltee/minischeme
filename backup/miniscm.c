/*
*      ---------- Mini-Scheme Interpreter Version 0.85 ----------
*
*                coded by Atsushi Moriwaki (11/5/1989)
*
*            E-MAIL :  moriwaki@kurims.kurims.kyoto-u.ac.jp
*
*               THIS SOFTWARE IS IN THE PUBLIC DOMAIN
*               ------------------------------------
* This software is completely free to copy, modify and/or re-distribute.
* But I would appreciate it if you left my name on the code as the author.
*
*
*  This version has been modified by Chris Pressey.
*    current version is 0.85p1 (as yet unreleased)
*
*  This version has been modified by R.C. Secrist.
*
*  Mini-Scheme is now maintained by Akira KIDA.
*
*  This is a revised and modified version by Akira KIDA.
*    current version is 0.85k4 (15 May 1994)
*
*  Please send suggestions, bug reports and/or requests to:
*        <SDI00379@niftyserve.or.jp>
*--
*/

/* #define VERBOSE */    /* define this if you want verbose GC */

#define BACKQUOTE '`'

/* Basic memory allocation units */
#define CELL_SEGSIZE    5000    /* # of cells in one segment */
#define CELL_NSEGMENT   100    /* # of segments for cells */
#define STR_SEGSIZE     2500    /* bytes of one string segment */
#define STR_NSEGMENT    100    /* # of segments for strings */


#define banner "Hello, This is Mini-Scheme Interpreter Version 0.85p1.\n"

#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define prompt "> "
#define InitFile "init.scm"
#define FIRST_CELLSEGS 10



#define T_STRING         1    /* 0000000000000001 */
#define T_NUMBER         2    /* 0000000000000010 */
#define T_SYMBOL         4    /* 0000000000000100 */
#define T_SYNTAX         8    /* 0000000000001000 */
#define T_PROC          16    /* 0000000000010000 */
#define T_PAIR          32    /* 0000000000100000 */
#define T_CLOSURE       64    /* 0000000001000000 */
#define T_CONTINUATION 128    /* 0000000010000000 */
#define T_MACRO        256    /* 0000000100000000 */
#define T_PROMISE      512    /* 0000001000000000 */
#define T_ATOM       16384    /* 0100000000000000 */    /* only for gc */
#define CLRATOM      49151    /* 1011111111111111 */    /* only for gc */
#define MARK         32768    /* 1000000000000000 */
#define UNMARK       32767    /* 0111111111111111 */

/* macros for cell operations */
#define scm_type(p)         ((p)->_flag)
#define scm_isstring(p)     (scm_type(p)&T_STRING)
#define strvalue(p)     ((p)->_object._string._svalue)
#define keynum(p)       ((p)->_object._string._keynum)
#define isnumber(p)     (scm_type(p)&T_NUMBER)
#define ivalue(p)       ((p)->_object._number._ivalue)
#define ispair(p)       (scm_type(p)&T_PAIR)
#define car(p)          ((p)->_object._cons._car)
#define cdr(p)          ((p)->_object._cons._cdr)
#define issymbol(p)     (scm_type(p)&T_SYMBOL)
#define symname(p)      strvalue(car(p))
#define hasprop(p)      (scm_type(p)&T_SYMBOL)
#define symprop(p)      cdr(p)
#define issyntax(p)     (scm_type(p)&T_SYNTAX)
#define isproc(p)       (scm_type(p)&T_PROC)
#define syntaxname(p)   strvalue(car(p))
#define syntaxnum(p)    keynum(car(p))
#define procnum(p)      ivalue(p)
#define isclosure(p)    (scm_type(p)&T_CLOSURE)
#define ismacro(p)      (scm_type(p)&T_MACRO)
#define closure_code(p) car(p)
#define closure_env(p)  cdr(p)
#define iscontinuation(p) (scm_type(p)&T_CONTINUATION)
#define cont_dump(p)    cdr(p)
#define ispromise(p)    (scm_type(p)&T_PROMISE)
#define setpromise(p)   scm_type(p) |= T_PROMISE
#define isatom(p)       (scm_type(p)&T_ATOM)
#define setatom(p)      scm_type(p) |= T_ATOM
#define clratom(p)      scm_type(p) &= CLRATOM
#define ismark(p)       (scm_type(p)&MARK)
#define setmark(p)      scm_type(p) |= MARK
#define clrmark(p)      scm_type(p) &= UNMARK
#define caar(p)         car(car(p))
#define cadr(p)         car(cdr(p))
#define cdar(p)         cdr(car(p))
#define cddr(p)         cdr(cdr(p))
#define cadar(p)        car(cdr(car(p)))
#define caddr(p)        car(cdr(cdr(p)))
#define cadaar(p)       car(cdr(car(car(p))))
#define cadddr(p)       car(cdr(cdr(cdr(p))))
#define cddddr(p)       cdr(cdr(cdr(cdr(p))))

/* --------- */
#define TOK_LPAREN  0
#define TOK_RPAREN  1
#define TOK_DOT     2
#define TOK_ATOM    3
#define TOK_QUOTE   4
#define TOK_COMMENT 5
#define TOK_DQUOTE  6
#define TOK_BQUOTE  7
#define TOK_COMMA   8
#define TOK_ATMARK  9
#define TOK_SHARP   10
#define LINESIZE 1024

/* cell structure */
typedef struct cell
{
    unsigned short _flag;
    union
    {
        struct
        {
            char*   _svalue;
            short   _keynum;
        } _string;

        struct
        {
            long    _ivalue;
        } _number;

        struct
        {
            struct cell* _car;
            struct cell* _cdr;
        } _cons;
    } _object;
} cell;

typedef cell* pointer;

cell _NIL;
cell _T;
cell _F;
pointer LAMBDA;            /* pointer to syntax lambda */
pointer QUOTE;            /* pointer to syntax quote */
pointer QQUOTE;            /* pointer to symbol quasiquote */
pointer UNQUOTE;        /* pointer to symbol unquote */
pointer UNQUOTESP;        /* pointer to symbol unquote-splicing */
pointer NIL = &_NIL;        /* special cell representing empty cell */
pointer T = &_T;        /* special cell representing #t */
pointer F = &_F;        /* special cell representing #f */
pointer free_cell = &_NIL;    /* pointer to top of free cells */
pointer oblist = &_NIL;        /* pointer to symbol table */


typedef struct scheme_t
{
    char    linebuff[LINESIZE];
    char    strbuff[256];

    char* currentline;
    char* endline;
    
    int last_cell_seg;
    int str_seglast;
    int tok;
    int quiet;
    int all_errors_fatal;
    int print_flag;
    long fcells;
    short operator;

    FILE* tmpfp;
    FILE* infp;            /* input file */
    FILE* outfp;            /* output file */

    pointer value;
    pointer args;            /* register for arguments of function */
    pointer envir;            /* stack register for current ctx.environment */
    pointer code;            /* register for current code */
    pointer dump;            /* stack register for next evaluation */


    /* arrays for segments */
    pointer cell_seg[CELL_NSEGMENT];
    char*   str_seg[STR_NSEGMENT];
    pointer global_env;        /* pointer to global ctx.environment */
    jmp_buf error_jmp;
    char    gc_verbose;        /* if ctx.gc_verbose is not zero, print gc status */
} scheme_t;

struct scheme_t ctx;

/* pre */
void gc(register pointer a, register pointer b);
void init_globals();
int isdelim(char* s, char c);


/* ctx.operator code */

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


/* allocate new cell segment */
int alloc_cellseg(int n)
{
    register pointer p;
    register long i;
    register int k;
    for(k = 0; k < n; k++)
    {
        if(ctx.last_cell_seg >= CELL_NSEGMENT - 1)
        {
            return k;
        }
        p = (pointer) malloc(CELL_SEGSIZE * sizeof(cell));
        if(p == (pointer) 0)
        {
            return k;
        }
        ctx.cell_seg[++ctx.last_cell_seg] = p;
        ctx.fcells += CELL_SEGSIZE;
        for(i = 0; i < CELL_SEGSIZE - 1; i++, p++)
        {
            scm_type(p) = 0;
            car(p) = NIL;
            cdr(p) = p + 1;
        }
        scm_type(p) = 0;
        car(p) = NIL;
        cdr(p) = free_cell;
        free_cell = ctx.cell_seg[ctx.last_cell_seg];
    }
    return n;
}

/* allocate new string segment */
int alloc_strseg(int n)
{
    register char* p;
    register long i;
    register int k;
    for(k = 0; k < n; k++)
    {
        if(ctx.str_seglast >= STR_NSEGMENT)
        {
            return k;
        }
        p = (char*) malloc(STR_SEGSIZE * sizeof(char));
        if(p == (char*) 0)
        {
            return k;
        }
        ctx.str_seg[++ctx.str_seglast] = p;
        for(i = 0; i < STR_SEGSIZE; i++)
        {
            *p++ = (char)(-1);
        }
    }
    return n;
}

/* initialization of Mini-Scheme */
void init_scheme()
{
    register pointer i;
    if(alloc_cellseg(FIRST_CELLSEGS) != FIRST_CELLSEGS)
    {
        FatalError("Unable to allocate initial cell segments");
    }
    if(!alloc_strseg(1))
    {
        FatalError("Unable to allocate initial string segments");
    }
#ifdef VERBOSE
    ctx.gc_verbose = 1;
#else
    ctx.gc_verbose = 0;
#endif
    init_globals();
}

/* get new cell.  parameter a, b is marked by gc. */
pointer get_cell(register pointer a, register pointer b)
{
    register pointer x;
    if(free_cell == NIL)
    {
        gc(a, b);
        if(free_cell == NIL)
        {
            if(!alloc_cellseg(1))
            {
                ctx.args = ctx.envir = ctx.code = ctx.dump = NIL;
                gc(NIL, NIL);
                if(free_cell != NIL)
                {
                    Error("run out of cells --- rerurn to top level");
                }
                else
                {
                    FatalError("run out of cells --- unable to recover cells");
                }
            }
        }
    }
    x = free_cell;
    free_cell = cdr(x);
    --ctx.fcells;
    return (x);
}

/* get new cons cell */
pointer cons(register pointer a, register pointer b)
{
    register pointer x;
    x = get_cell(a, b);
    scm_type(x) = T_PAIR;
    car(x) = a;
    cdr(x) = b;
    return (x);
}

/* get number atom */
pointer mk_number(register long num)
{
    register pointer x;
    x = get_cell(NIL, NIL);
    scm_type(x) = (T_NUMBER | T_ATOM);
    ivalue(x) = num;
    return (x);
}

/* allocate name to string area */
char* store_string(char* name)
{
    register char* q;
    register short i;
    long    len, remain;
    /* first check name has already listed */
    for(i = 0; i <= ctx.str_seglast; i++)
    {
        for(q = ctx.str_seg[i]; *q != (char)(-1);)
        {
            if(!strcmp(q, name))
            {
                goto FOUND;
            }
            while(*q++)
            {
                /* get next string */;
            }
        }
    }
    len = strlen(name) + 2;
    remain = (long) STR_SEGSIZE - ((long) q - (long) ctx.str_seg[ctx.str_seglast]);
    if(remain < len)
    {
        if(!alloc_strseg(1))
        {
            FatalError("run out of string area");
        }
        q = ctx.str_seg[ctx.str_seglast];
    }
    strcpy(q, name);
FOUND:
    return (q);
}

/* get new string */
pointer mk_string(char* str)
{
    register pointer x = get_cell(NIL, NIL);
    strvalue(x) = store_string(str);
    scm_type(x) = (T_STRING | T_ATOM);
    keynum(x) = (short)(-1);
    return (x);
}

/* get new symbol */
pointer mk_symbol(char* name)
{
    register pointer x;
    /* fisrt check oblist */
    for(x = oblist; x != NIL; x = cdr(x))
    {
        if(!strcmp(name, symname(car(x))))
        {
            break;
        }
    }
    if(x != NIL)
    {
        return (car(x));
    }
    else
    {
        x = cons(mk_string(name), NIL);
        scm_type(x) = T_SYMBOL;
        oblist = cons(x, oblist);
        return (x);
    }
}

/* make symbol or number atom from string */
pointer mk_atom(char* q)
{
    char    c, *p;
    p = q;
    if(!isdigit(c = *p++))
    {
        if((c != '+' && c != '-') || !isdigit(*p))
        {
            return (mk_symbol(q));
        }
    }
    for(; (c = *p) != 0; ++p)
    {
        if(!isdigit(c))
        {
            return (mk_symbol(q));
        }
    }
    return (mk_number(atol(q)));
}

/* make constant */
pointer mk_const(char* name)
{
    long    x;
    char    tmp[256];
    if(!strcmp(name, "t"))
    {
        return (T);
    }
    else if(!strcmp(name, "f"))
    {
        return (F);
    }
    else if(*name == 'o')   /* #o (octal) */
    {
        sprintf(tmp, "0%s", &name[1]);
        sscanf(tmp, "%lo", (unsigned long int*)&x);
        return (mk_number(x));
    }
    else if(*name == 'd')         /* #d (decimal) */
    {
        sscanf(&name[1], "%ld", &x);
        return (mk_number(x));
    }
    else if(*name == 'x')         /* #x (hex) */
    {
        sprintf(tmp, "0x%s", &name[1]);
        sscanf(tmp, "%lx", (unsigned long int*)&x);
        return (mk_number(x));
    }
    else
    {
        return (NIL);
    }
}

/*
* ========== garbage collector ==========
*
*  We use algorithm E (Kunuth, The Art of Computer Programming Vol.1, sec.3.5) for marking.
*/
void mark(pointer a)
{
    register pointer t, q, p;
E1:
    t = (pointer) 0;
    p = a;
E2:
    setmark(p);
E3:
    if(isatom(p))
    {
        goto E6;
    }
E4:
    q = car(p);
    if(q && !ismark(q))
    {
        setatom(p);
        car(p) = t;
        t = p;
        p = q;
        goto E2;
    }
E5:
    q = cdr(p);
    if(q && !ismark(q))
    {
        cdr(p) = t;
        t = p;
        p = q;
        goto E2;
    }
E6:
    if(!t)
    {
        return;
    }
    q = t;
    if(isatom(q))
    {
        clratom(q);
        t = car(q);
        car(q) = p;
        p = q;
        goto E5;
    }
    else
    {
        t = cdr(q);
        cdr(q) = p;
        p = q;
        goto E6;
    }
}


/* garbage collection. parameter a, b is marked. */
void gc(register pointer a, register pointer b)
{
    register pointer p;
    register short i;
    register long j;
    if(ctx.gc_verbose)
    {
        printf("gc...");
    }
    /* mark system globals */
    mark(oblist);
    mark(ctx.global_env);
    /* mark current registers */
    mark(ctx.args);
    mark(ctx.envir);
    mark(ctx.code);
    mark(ctx.dump);
    /* mark variables a, b */
    mark(a);
    mark(b);
    /* garbage collect */
    clrmark(NIL);
    ctx.fcells = 0;
    free_cell = NIL;
    for(i = 0; i <= ctx.last_cell_seg; i++)
    {
        for(j = 0, p = ctx.cell_seg[i]; j < CELL_SEGSIZE; j++, p++)
        {
            if(ismark(p))
            {
                clrmark(p);
            }
            else
            {
                scm_type(p) = 0;
                cdr(p) = free_cell;
                car(p) = NIL;
                free_cell = p;
                ++ctx.fcells;
            }
        }
    }
    if(ctx.gc_verbose)
    {
        printf(" done %ld cells are recovered.\n", ctx.fcells);
    }
}


/* ========== Rootines for Reading ========== */

/* get new character from input file */
int inchar()
{
    if(ctx.currentline >= ctx.endline)       /* input buffer is empty */
    {
        if(feof(ctx.infp))
        {
            fclose(ctx.infp);
            ctx.infp = stdin;
            if(!ctx.quiet)
            {
                printf(prompt);
            }
        }
        strcpy(ctx.linebuff, "\n");
        if(fgets(ctx.currentline = ctx.linebuff, LINESIZE, ctx.infp) == NULL)
        {
            if(ctx.infp == stdin)
            {
                if(!ctx.quiet)
                {
                    fprintf(stderr, "Good-bye\n");
                }
                exit(0);
            }
        }
        ctx.endline = ctx.linebuff + strlen(ctx.linebuff);
    }
    return (*ctx.currentline++);
}

/* clear input buffer */
void clearinput()
{
    ctx.currentline = ctx.endline = ctx.linebuff;
}

/* back to standard input */
void flushinput()
{
    if(ctx.infp != stdin)
    {
        fclose(ctx.infp);
        ctx.infp = stdin;
    }
    clearinput();
}

/* back character to input buffer */
void backchar()
{
    ctx.currentline--;
}

/* read chacters to delimiter */
char* readstr(char* delim)
{
    char* p;
    p = ctx.strbuff;
    while(isdelim(delim, (*p++ = inchar())))
    {
        /* nop */;
    }
    backchar();
    *--p = '\0';
    return (ctx.strbuff);
}

/* read string expression "xxx...xxx" */
char* readstrexp()
{
    char c;
    char* p;
    p = ctx.strbuff;
    for(;;)
    {
        if((c = inchar()) != '"')
        {
            *p++ = c;
        }
        else if(p > ctx.strbuff && *(p - 1) == '\\')
        {
            *(p - 1) = '"';
        }
        else
        {
            *p = '\0';
            return (ctx.strbuff);
        }
    }
}

/* check c is delimiter */
int isdelim(char* s, char c)
{
    while(*s)
    {
        if(*s++ == c)
        {
            return (0);
        }
    }
    return (1);
}


/* skip white characters */
void skipspace()
{
    while(isspace(inchar()))
    {
        /* nop */;
    }
    backchar();
}

/* get token */
int token()
{
    skipspace();
    switch(inchar())
    {
        case '(':
            return (TOK_LPAREN);
        case ')':
            return (TOK_RPAREN);
        case '.':
            return (TOK_DOT);
        case '\'':
            return (TOK_QUOTE);
        case ';':
            return (TOK_COMMENT);
        case '"':
            return (TOK_DQUOTE);
        case BACKQUOTE:
            return (TOK_BQUOTE);
        case ',':
            if(inchar() == '@')
            {
                return (TOK_ATMARK);
            }
            else
            {
                backchar();
                return (TOK_COMMA);
            }
        case '#':
            return (TOK_SHARP);
        default:
            backchar();
            return (TOK_ATOM);
    }
}

/* ========== Rootines for Printing ========== */

#define ok_abbrev(x)    (ispair(x) && cdr(x) == NIL)

void strunquote(char* p, char* s)
{
    *p++ = '"';
    for(; *s; ++s)
    {
        if(*s == '"')
        {
            *p++ = '\\';
            *p++ = '"';
        }
        else if(*s == '\n')
        {
            *p++ = '\\';
            *p++ = 'n';
        }
        else
        {
            *p++ = *s;
        }
    }
    *p++ = '"';
    *p = '\0';
}

/* print atoms */
int printatom(pointer l, int f)
{
    char*    p;
    if(l == NIL)
    {
        p = "()";
    }
    else if(l == T)
    {
        p = "#t";
    }
    else if(l == F)
    {
        p = "#f";
    }
    else if(isnumber(l))
    {
        p = ctx.strbuff;
        sprintf(p, "%ld", ivalue(l));
    }
    else if(scm_isstring(l))
    {
        if(!f)
        {
            p = strvalue(l);
        }
        else
        {
            p = ctx.strbuff;
            strunquote(p, strvalue(l));
        }
    }
    else if(issymbol(l))
    {
        p = symname(l);
    }
    else if(isproc(l))
    {
        p = ctx.strbuff;
        sprintf(p, "#<PROCEDURE %ld>", procnum(l));
    }
    else if(ismacro(l))
    {
        p = "#<MACRO>";
    }
    else if(isclosure(l))
    {
        p = "#<CLOSURE>";
    }
    else if(iscontinuation(l))
    {
        p = "#<CONTINUATION>";
    }
    if(f < 0)
    {
        return strlen(p);
    }
    fputs(p, ctx.outfp);
    return 0;
}

/* ========== Rootines for Evaluation Cycle ========== */

/* make closure. c is code. e is ctx.environment */
pointer mk_closure(register pointer c, register pointer e)
{
    register pointer x = get_cell(c, e);
    scm_type(x) = T_CLOSURE;
    car(x) = c;
    cdr(x) = e;
    return (x);
}

/* make continuation. */
pointer mk_continuation(register pointer d)
{
    register pointer x = get_cell(NIL, d);
    scm_type(x) = T_CONTINUATION;
    cont_dump(x) = d;
    return (x);
}

/* reverse list -- make new cells */
pointer reverse(register pointer a)
{
    register pointer p;
    p = NIL;
    for(; ispair(a); a = cdr(a))
    {
        p = cons(car(a), p);
    }
    return (p);
}

/* reverse list --- no make new cells */
pointer non_alloc_rev(pointer term, pointer list)
{
    register pointer p;
    register pointer q;
    register pointer result;
    p = list;
    result = term;
    while(p != NIL)
    {
        q = cdr(p);
        cdr(p) = result;
        result = p;
        p = q;
    }
    return (result);
}

/* append list -- make new cells */
pointer append(register pointer a, register pointer b)
{
    register pointer p;
    register pointer q;
    p = b;
    if(a != NIL)
    {
        a = reverse(a);
        while(a != NIL)
        {
            q = cdr(a);
            cdr(a) = p;
            p = a;
            a = q;
        }
    }
    return (p);
}

/* equivalence of atoms */
int eqv(register pointer a, register pointer b)
{
    if(scm_isstring(a))
    {
        if(scm_isstring(b))
        {
            return (strvalue(a) == strvalue(b));
        }
        else
        {
            return (0);
        }
    }
    else if(isnumber(a))
    {
        if(isnumber(b))
        {
            return (ivalue(a) == ivalue(b));
        }
        else
        {
            return (0);
        }
    }
    else
    {
        return (a == b);
    }
}

/* true or false value macro */
#define istrue(p)       ((p) != NIL && (p) != F)
#define isfalse(p)      ((p) == NIL || (p) == F)

/* Error macro */
#define BEGIN do {
#define END } while (0)

#define Error_0(s) \
    BEGIN \
        ctx.args = cons(mk_string((s)), NIL); \
        ctx.operator = (short)OP_ERR0; \
        return T; \
    END

#define Error_1(s, a) \
    BEGIN \
        ctx.args = cons((a), NIL); \
        ctx.args = cons(mk_string((s)), ctx.args); \
        ctx.operator = (short)OP_ERR0; \
        return T; \
    END

/* control macros for Eval_Cycle */
#define s_goto(a) \
    BEGIN  \
        ctx.operator = (short)(a); \
        return T; \
    END

#define s_save(a, b, c) \
    (  \
        ctx.dump = cons(ctx.envir, cons((c), ctx.dump)), \
        ctx.dump = cons((b), ctx.dump), \
        ctx.dump = cons(mk_number((long)(a)), ctx.dump) \
    )

#define s_return(a) \
    BEGIN \
        ctx.value = (a); \
        ctx.operator = ivalue(car(ctx.dump)); \
        ctx.args = cadr(ctx.dump); \
        ctx.envir = caddr(ctx.dump); \
        ctx.code = cadddr(ctx.dump); \
        ctx.dump = cddddr(ctx.dump); \
        return T; \
    END

#define s_retbool(tf)    s_return((tf) ? T : F)

/* ========== Evaluation Cycle ========== */

pointer opexe_0(register short op)
{
    register pointer x, y;
    switch(op)
    {
        case OP_LOAD:        /* load */
            if(!scm_isstring(car(ctx.args)))
            {
                Error_0("load -- argument is not string");
            }
            if((ctx.infp = fopen(strvalue(car(ctx.args)), "r")) == NULL)
            {
                ctx.infp = stdin;
                Error_1("Unable to open", car(ctx.args));
            }
            if(!ctx.quiet)
            {
                fprintf(ctx.outfp, "loading %s", strvalue(car(ctx.args)));
            }
            s_goto(OP_T0LVL);
        case OP_T0LVL:    /* top level */
            if(!ctx.quiet)
            {
                fprintf(ctx.outfp, "\n");
            }
            ctx.dump = NIL;
            ctx.envir = ctx.global_env;
            s_save(OP_VALUEPRINT, NIL, NIL);
            s_save(OP_T1LVL, NIL, NIL);
            if(ctx.infp == stdin && !ctx.quiet)
            {
                printf(prompt);
            }
            s_goto(OP_READ);
        case OP_T1LVL:    /* top level */
            ctx.code = ctx.value;
            s_goto(OP_EVAL);
        case OP_READ:        /* read */
            ctx.tok = token();
            s_goto(OP_RDSEXPR);
        case OP_VALUEPRINT:    /* print evalution result */
            ctx.print_flag = 1;
            ctx.args = ctx.value;
            if(ctx.quiet)
            {
                s_goto(OP_T0LVL);
            }
            else
            {
                s_save(OP_T0LVL, NIL, NIL);
                s_goto(OP_P0LIST);
            }
        case OP_EVAL:        /* main part of evalution */
            if(issymbol(ctx.code))       /* symbol */
            {
                for(x = ctx.envir; x != NIL; x = cdr(x))
                {
                    for(y = car(x); y != NIL; y = cdr(y))
                        if(caar(y) == ctx.code)
                        {
                            break;
                        }
                    if(y != NIL)
                    {
                        break;
                    }
                }
                if(x != NIL)
                {
                    s_return(cdar(y));
                }
                else
                {
                    Error_1("Unbounded variable", ctx.code);
                }
            }
            else if(ispair(ctx.code))
            {
                if(issyntax(x = car(ctx.code)))       /* SYNTAX */
                {
                    ctx.code = cdr(ctx.code);
                    s_goto(syntaxnum(x));
                }
                else    /* first, eval top element and eval arguments */
                {
                    s_save(OP_E0ARGS, NIL, ctx.code);
                    ctx.code = car(ctx.code);
                    s_goto(OP_EVAL);
                }
            }
            else
            {
                s_return(ctx.code);
            }
        case OP_E0ARGS:    /* eval arguments */
            if(ismacro(ctx.value))       /* macro expansion */
            {
                s_save(OP_DOMACRO, NIL, NIL);
                ctx.args = cons(ctx.code, NIL);
                ctx.code = ctx.value;
                s_goto(OP_APPLY);
            }
            else
            {
                ctx.code = cdr(ctx.code);
                s_goto(OP_E1ARGS);
            }
        case OP_E1ARGS:    /* eval arguments */
            ctx.args = cons(ctx.value, ctx.args);
            if(ispair(ctx.code))       /* continue */
            {
                s_save(OP_E1ARGS, ctx.args, cdr(ctx.code));
                ctx.code = car(ctx.code);
                ctx.args = NIL;
                s_goto(OP_EVAL);
            }
            else        /* end */
            {
                ctx.args = reverse(ctx.args);
                ctx.code = car(ctx.args);
                ctx.args = cdr(ctx.args);
                s_goto(OP_APPLY);
            }
        case OP_APPLY:        /* apply 'code' to 'args' */
            if(isproc(ctx.code))
            {
                s_goto(procnum(ctx.code));    /* PROCEDURE */
            }
            else if(isclosure(ctx.code))         /* CLOSURE */
            {
                /* make ctx.environment */
                ctx.envir = cons(NIL, closure_env(ctx.code));
                for(x = car(closure_code(ctx.code)), y=ctx.args; ispair(x); x=cdr(x), y=cdr(y))
                {
                    if(y == NIL)
                    {
                        Error_0("Few arguments");
                    }
                    else
                    {
                        car(ctx.envir) = cons(cons(car(x), car(y)), car(ctx.envir));
                    }
                }
                if(x == NIL)
                {
                    /*--
                    if (y != NIL)
                    {
                        Error_0("Many arguments");
                    }
                    */
                }
                else if(issymbol(x))
                {
                    car(ctx.envir) = cons(cons(x, y), car(ctx.envir));
                }
                else
                {
                    Error_0("Syntax error in closure");
                }
                ctx.code = cdr(closure_code(ctx.code));
                ctx.args = NIL;
                s_goto(OP_BEGIN);
            }
            else if(iscontinuation(ctx.code))         /* CONTINUATION */
            {
                ctx.dump = cont_dump(ctx.code);
                s_return(ctx.args != NIL ? car(ctx.args) : NIL);
            }
            else
            {
                Error_0("Illegal function");
            }
        case OP_DOMACRO:    /* do macro */
            ctx.code = ctx.value;
            s_goto(OP_EVAL);
        case OP_LAMBDA:    /* lambda */
            s_return(mk_closure(ctx.code, ctx.envir));
        case OP_QUOTE:        /* quote */
            s_return(car(ctx.code));
        case OP_DEF0:    /* define */
            if(ispair(car(ctx.code)))
            {
                x = caar(ctx.code);
                ctx.code = cons(LAMBDA, cons(cdar(ctx.code), cdr(ctx.code)));
            }
            else
            {
                x = car(ctx.code);
                ctx.code = cadr(ctx.code);
            }
            if(!issymbol(x))
            {
                Error_0("Variable is not symbol");
            }
            s_save(OP_DEF1, NIL, x);
            s_goto(OP_EVAL);
        case OP_DEF1:    /* define */
            for(x = car(ctx.envir); x != NIL; x = cdr(x))
                if(caar(x) == ctx.code)
                {
                    break;
                }
            if(x != NIL)
            {
                cdar(x) = ctx.value;
            }
            else
            {
                car(ctx.envir) = cons(cons(ctx.code, ctx.value), car(ctx.envir));
            }
            s_return(ctx.code);
        case OP_SET0:        /* set! */
            s_save(OP_SET1, NIL, car(ctx.code));
            ctx.code = cadr(ctx.code);
            s_goto(OP_EVAL);
        case OP_SET1:        /* set! */
            for(x = ctx.envir; x != NIL; x = cdr(x))
            {
                for(y = car(x); y != NIL; y = cdr(y))
                    if(caar(y) == ctx.code)
                    {
                        break;
                    }
                if(y != NIL)
                {
                    break;
                }
            }
            if(x != NIL)
            {
                cdar(y) = ctx.value;
                s_return(ctx.value);
            }
            else
            {
                Error_1("Unbounded variable", ctx.code);
            }
        case OP_BEGIN:        /* begin */
            if(!ispair(ctx.code))
            {
                s_return(ctx.code);
            }
            if(cdr(ctx.code) != NIL)
            {
                s_save(OP_BEGIN, NIL, cdr(ctx.code));
            }
            ctx.code = car(ctx.code);
            s_goto(OP_EVAL);
        case OP_IF0:        /* if */
            s_save(OP_IF1, NIL, cdr(ctx.code));
            ctx.code = car(ctx.code);
            s_goto(OP_EVAL);
        case OP_IF1:        /* if */
            if(istrue(ctx.value))
            {
                ctx.code = car(ctx.code);
            }
            else
            {
                ctx.code = cadr(ctx.code);
            }    /* (if #f 1) ==> () because car(NIL) = NIL */
            s_goto(OP_EVAL);
        case OP_LET0:        /* let */
            ctx.args = NIL;
            ctx.value = ctx.code;
            ctx.code = issymbol(car(ctx.code)) ? cadr(ctx.code) : car(ctx.code);
            s_goto(OP_LET1);
        case OP_LET1:        /* let (caluculate parameters) */
            ctx.args = cons(ctx.value, ctx.args);
            if(ispair(ctx.code))       /* continue */
            {
                s_save(OP_LET1, ctx.args, cdr(ctx.code));
                ctx.code = cadar(ctx.code);
                ctx.args = NIL;
                s_goto(OP_EVAL);
            }
            else        /* end */
            {
                ctx.args = reverse(ctx.args);
                ctx.code = car(ctx.args);
                ctx.args = cdr(ctx.args);
                s_goto(OP_LET2);
            }
        case OP_LET2:        /* let */
            ctx.envir = cons(NIL, ctx.envir);
            for(x = issymbol(car(ctx.code)) ? cadr(ctx.code) : car(ctx.code), y = ctx.args;
                    y != NIL; x = cdr(x), y = cdr(y))
            {
                car(ctx.envir) = cons(cons(caar(x), car(y)), car(ctx.envir));
            }
            if(issymbol(car(ctx.code)))       /* named let */
            {
                for(x = cadr(ctx.code), ctx.args = NIL; x != NIL; x = cdr(x))
                {
                    ctx.args = cons(caar(x), ctx.args);
                }
                x = mk_closure(cons(reverse(ctx.args), cddr(ctx.code)), ctx.envir);
                car(ctx.envir) = cons(cons(car(ctx.code), x), car(ctx.envir));
                ctx.code = cddr(ctx.code);
                ctx.args = NIL;
            }
            else
            {
                ctx.code = cdr(ctx.code);
                ctx.args = NIL;
            }
            s_goto(OP_BEGIN);
        case OP_LET0AST:    /* let* */
            if(car(ctx.code) == NIL)
            {
                ctx.envir = cons(NIL, ctx.envir);
                ctx.code = cdr(ctx.code);
                s_goto(OP_BEGIN);
            }
            s_save(OP_LET1AST, cdr(ctx.code), car(ctx.code));
            ctx.code = cadaar(ctx.code);
            s_goto(OP_EVAL);
        case OP_LET1AST:    /* let* (make new frame) */
            ctx.envir = cons(NIL, ctx.envir);
            s_goto(OP_LET2AST);
        case OP_LET2AST:    /* let* (caluculate parameters) */
            car(ctx.envir) = cons(cons(caar(ctx.code), ctx.value), car(ctx.envir));
            ctx.code = cdr(ctx.code);
            if(ispair(ctx.code))       /* continue */
            {
                s_save(OP_LET2AST, ctx.args, ctx.code);
                ctx.code = cadar(ctx.code);
                ctx.args = NIL;
                s_goto(OP_EVAL);
            }
            else        /* end */
            {
                ctx.code = ctx.args;
                ctx.args = NIL;
                s_goto(OP_BEGIN);
            }
        default:
            sprintf(ctx.strbuff, "%d is illegal ctx.operator", ctx.operator);
            Error_0(ctx.strbuff);
    }
    return T;
}

pointer opexe_1(register short op)
{
    register pointer x;
    register pointer y;
    switch(op)
    {
        case OP_LET0REC:    /* letrec */
            ctx.envir = cons(NIL, ctx.envir);
            ctx.args = NIL;
            ctx.value = ctx.code;
            ctx.code = car(ctx.code);
            s_goto(OP_LET1REC);
        case OP_LET1REC:    /* letrec (caluculate parameters) */
            ctx.args = cons(ctx.value, ctx.args);
            if(ispair(ctx.code))       /* continue */
            {
                s_save(OP_LET1REC, ctx.args, cdr(ctx.code));
                ctx.code = cadar(ctx.code);
                ctx.args = NIL;
                s_goto(OP_EVAL);
            }
            else        /* end */
            {
                ctx.args = reverse(ctx.args);
                ctx.code = car(ctx.args);
                ctx.args = cdr(ctx.args);
                s_goto(OP_LET2REC);
            }
        case OP_LET2REC:    /* letrec */
            for(x = car(ctx.code), y = ctx.args; y != NIL; x = cdr(x), y = cdr(y))
            {
                car(ctx.envir) = cons(cons(caar(x), car(y)), car(ctx.envir));
            }
            ctx.code = cdr(ctx.code);
            ctx.args = NIL;
            s_goto(OP_BEGIN);
        case OP_COND0:        /* cond */
            if(!ispair(ctx.code))
            {
                Error_0("Syntax error in cond");
            }
            s_save(OP_COND1, NIL, ctx.code);
            ctx.code = caar(ctx.code);
            s_goto(OP_EVAL);
        case OP_COND1:        /* cond */
            if(istrue(ctx.value))
            {
                if((ctx.code = cdar(ctx.code)) == NIL)
                {
                    s_return(ctx.value);
                }
                s_goto(OP_BEGIN);
            }
            else
            {
                if((ctx.code = cdr(ctx.code)) == NIL)
                {
                    s_return(NIL);
                }
                else
                {
                    s_save(OP_COND1, NIL, ctx.code);
                    ctx.code = caar(ctx.code);
                    s_goto(OP_EVAL);
                }
            }
        case OP_DELAY:        /* delay */
            x = mk_closure(cons(NIL, ctx.code), ctx.envir);
            setpromise(x);
            s_return(x);
        case OP_AND0:        /* and */
            if(ctx.code == NIL)
            {
                s_return(T);
            }
            s_save(OP_AND1, NIL, cdr(ctx.code));
            ctx.code = car(ctx.code);
            s_goto(OP_EVAL);
        case OP_AND1:        /* and */
            if(isfalse(ctx.value))
            {
                s_return(ctx.value);
            }
            else if(ctx.code == NIL)
            {
                s_return(ctx.value);
            }
            else
            {
                s_save(OP_AND1, NIL, cdr(ctx.code));
                ctx.code = car(ctx.code);
                s_goto(OP_EVAL);
            }
        case OP_OR0:        /* or */
            if(ctx.code == NIL)
            {
                s_return(F);
            }
            s_save(OP_OR1, NIL, cdr(ctx.code));
            ctx.code = car(ctx.code);
            s_goto(OP_EVAL);
        case OP_OR1:        /* or */
            if(istrue(ctx.value))
            {
                s_return(ctx.value);
            }
            else if(ctx.code == NIL)
            {
                s_return(ctx.value);
            }
            else
            {
                s_save(OP_OR1, NIL, cdr(ctx.code));
                ctx.code = car(ctx.code);
                s_goto(OP_EVAL);
            }
        case OP_C0STREAM:    /* cons-stream */
            s_save(OP_C1STREAM, NIL, cdr(ctx.code));
            ctx.code = car(ctx.code);
            s_goto(OP_EVAL);
        case OP_C1STREAM:    /* cons-stream */
            ctx.args = ctx.value;    /* save value to register ctx.args for gc */
            x = mk_closure(cons(NIL, ctx.code), ctx.envir);
            setpromise(x);
            s_return(cons(ctx.args, x));
        case OP_0MACRO:    /* macro */
            x = car(ctx.code);
            ctx.code = cadr(ctx.code);
            if(!issymbol(x))
            {
                Error_0("Variable is not symbol");
            }
            s_save(OP_1MACRO, NIL, x);
            s_goto(OP_EVAL);
        case OP_1MACRO:    /* macro */
            scm_type(ctx.value) |= T_MACRO;
            for(x = car(ctx.envir); x != NIL; x = cdr(x))
                if(caar(x) == ctx.code)
                {
                    break;
                }
            if(x != NIL)
            {
                cdar(x) = ctx.value;
            }
            else
            {
                car(ctx.envir) = cons(cons(ctx.code, ctx.value), car(ctx.envir));
            }
            s_return(ctx.code);
        case OP_CASE0:        /* case */
            s_save(OP_CASE1, NIL, cdr(ctx.code));
            ctx.code = car(ctx.code);
            s_goto(OP_EVAL);
        case OP_CASE1:        /* case */
            for(x = ctx.code; x != NIL; x = cdr(x))
            {
                if(!ispair(y = caar(x)))
                {
                    break;
                }
                for(; y != NIL; y = cdr(y))
                    if(eqv(car(y), ctx.value))
                    {
                        break;
                    }
                if(y != NIL)
                {
                    break;
                }
            }
            if(x != NIL)
            {
                if(ispair(caar(x)))
                {
                    ctx.code = cdar(x);
                    s_goto(OP_BEGIN);
                }
                else    /* else */
                {
                    s_save(OP_CASE2, NIL, cdar(x));
                    ctx.code = caar(x);
                    s_goto(OP_EVAL);
                }
            }
            else
            {
                s_return(NIL);
            }
        case OP_CASE2:        /* case */
            if(istrue(ctx.value))
            {
                s_goto(OP_BEGIN);
            }
            else
            {
                s_return(NIL);
            }
        case OP_PAPPLY:    /* apply */
            ctx.code = car(ctx.args);
            ctx.args = cadr(ctx.args);
            s_goto(OP_APPLY);
        case OP_PEVAL:    /* eval */
            ctx.code = car(ctx.args);
            ctx.args = NIL;
            s_goto(OP_EVAL);
        case OP_CONTINUATION:    /* call-with-current-continuation */
            ctx.code = car(ctx.args);
            ctx.args = cons(mk_continuation(ctx.dump), NIL);
            s_goto(OP_APPLY);
        default:
            sprintf(ctx.strbuff, "%d is illegal ctx.operator", ctx.operator);
            Error_0(ctx.strbuff);
    }
    return T;
}

pointer opexe_2(register short op)
{
    register pointer x;
    register pointer y;
    register long v;
    switch(op)
    {
        case OP_ADD:        /* + */
            for(x = ctx.args, v = 0; x != NIL; x = cdr(x))
            {
                v += ivalue(car(x));
            }
            s_return(mk_number(v));
        case OP_SUB:        /* - */
            for(x = cdr(ctx.args), v = ivalue(car(ctx.args)); x != NIL; x = cdr(x))
            {
                v -= ivalue(car(x));
            }
            s_return(mk_number(v));
        case OP_MUL:        /* * */
            for(x = ctx.args, v = 1; x != NIL; x = cdr(x))
            {
                v *= ivalue(car(x));
            }
            s_return(mk_number(v));
        case OP_DIV:        /* / */
            for(x = cdr(ctx.args), v = ivalue(car(ctx.args)); x != NIL; x = cdr(x))
            {
                if(ivalue(car(x)) != 0)
                {
                    v /= ivalue(car(x));
                }
                else
                {
                    Error_0("Divided by zero");
                }
            }
            s_return(mk_number(v));
        case OP_REM:        /* remainder */
            for(x = cdr(ctx.args), v = ivalue(car(ctx.args)); x != NIL; x = cdr(x))
            {
                if(ivalue(car(x)) != 0)
                {
                    v %= ivalue(car(x));
                }
                else
                {
                    Error_0("Divided by zero");
                }
            }
            s_return(mk_number(v));
        case OP_CAR:        /* car */
            if(ispair(car(ctx.args)))
            {
                s_return(caar(ctx.args));
            }
            else
            {
                Error_0("Unable to car for non-cons cell");
            }
        case OP_CDR:        /* cdr */
            if(ispair(car(ctx.args)))
            {
                s_return(cdar(ctx.args));
            }
            else
            {
                Error_0("Unable to cdr for non-cons cell");
            }
        case OP_CONS:        /* cons */
            cdr(ctx.args) = cadr(ctx.args);
            s_return(ctx.args);
        case OP_SETCAR:    /* set-car! */
            if(ispair(car(ctx.args)))
            {
                caar(ctx.args) = cadr(ctx.args);
                s_return(car(ctx.args));
            }
            else
            {
                Error_0("Unable to set-car! for non-cons cell");
            }
        case OP_SETCDR:    /* set-cdr! */
            if(ispair(car(ctx.args)))
            {
                cdar(ctx.args) = cadr(ctx.args);
                s_return(car(ctx.args));
            }
            else
            {
                Error_0("Unable to set-cdr! for non-cons cell");
            }
        default:
            sprintf(ctx.strbuff, "%d is illegal ctx.operator", ctx.operator);
            Error_0(ctx.strbuff);
    }
    return T;
}

pointer opexe_3(register short op)
{
    register pointer x;
    register pointer y;
    switch(op)
    {
        case OP_NOT:        /* not */
            s_retbool(isfalse(car(ctx.args)));
        case OP_BOOL:        /* boolean? */
            s_retbool(car(ctx.args) == F || car(ctx.args) == T);
        case OP_NULL:        /* null? */
            s_retbool(car(ctx.args) == NIL);
        case OP_ZEROP:        /* zero? */
            s_retbool(ivalue(car(ctx.args)) == 0);
        case OP_POSP:        /* positive? */
            s_retbool(ivalue(car(ctx.args)) > 0);
        case OP_NEGP:        /* negative? */
            s_retbool(ivalue(car(ctx.args)) < 0);
        case OP_NEQ:        /* = */
            s_retbool(ivalue(car(ctx.args)) == ivalue(cadr(ctx.args)));
        case OP_LESS:        /* < */
            s_retbool(ivalue(car(ctx.args)) < ivalue(cadr(ctx.args)));
        case OP_GRE:        /* > */
            s_retbool(ivalue(car(ctx.args)) > ivalue(cadr(ctx.args)));
        case OP_LEQ:        /* <= */
            s_retbool(ivalue(car(ctx.args)) <= ivalue(cadr(ctx.args)));
        case OP_GEQ:        /* >= */
            s_retbool(ivalue(car(ctx.args)) >= ivalue(cadr(ctx.args)));
        case OP_SYMBOL:    /* symbol? */
            s_retbool(issymbol(car(ctx.args)));
        case OP_NUMBER:    /* number? */
            s_retbool(isnumber(car(ctx.args)));
        case OP_STRING:    /* string? */
            s_retbool(scm_isstring(car(ctx.args)));
        case OP_PROC:        /* procedure? */
            /*--
            * continuation should be procedure by the example
            * (call-with-current-continuation procedure?) ==> #t
            * in R^3 report sec. 6.9
            */
            s_retbool(isproc(car(ctx.args)) || isclosure(car(ctx.args))
                      || iscontinuation(car(ctx.args)));
        case OP_PAIR:        /* pair? */
            s_retbool(ispair(car(ctx.args)));
        case OP_EQ:        /* eq? */
            s_retbool(car(ctx.args) == cadr(ctx.args));
        case OP_EQV:        /* eqv? */
            s_retbool(eqv(car(ctx.args), cadr(ctx.args)));
        default:
            sprintf(ctx.strbuff, "%d is illegal ctx.operator", ctx.operator);
            Error_0(ctx.strbuff);
    }
    return T;
}

pointer opexe_4(register short op)
{
    register pointer x;
    register pointer y;
    switch(op)
    {
        case OP_FORCE:        /* force */
            ctx.code = car(ctx.args);
            if(ispromise(ctx.code))
            {
                ctx.args = NIL;
                s_goto(OP_APPLY);
            }
            else
            {
                s_return(ctx.code);
            }
        case OP_WRITE:        /* write */
            ctx.print_flag = 1;
            ctx.args = car(ctx.args);
            s_goto(OP_P0LIST);
        case OP_DISPLAY:    /* display */
            ctx.print_flag = 0;
            ctx.args = car(ctx.args);
            s_goto(OP_P0LIST);
        case OP_NEWLINE:    /* newline */
            fprintf(ctx.outfp, "\n");
            s_return(T);
        case OP_ERR0:    /* error */
            if(!scm_isstring(car(ctx.args)))
            {
                Error_0("error -- first argument must be string");
            }
            ctx.tmpfp = ctx.outfp;
            ctx.outfp = stderr;
            if(ctx.all_errors_fatal)
            {
                FatalError(strvalue(car(ctx.args)));
            }
            fprintf(ctx.outfp, "Error: ");
            fprintf(ctx.outfp, "%s", strvalue(car(ctx.args)));
            ctx.args = cdr(ctx.args);
            s_goto(OP_ERR1);
        case OP_ERR1:    /* error */
            fprintf(ctx.outfp, " ");
            if(ctx.args != NIL)
            {
                s_save(OP_ERR1, cdr(ctx.args), NIL);
                ctx.args = car(ctx.args);
                ctx.print_flag = 1;
                s_goto(OP_P0LIST);
            }
            else
            {
                fprintf(ctx.outfp, "\n");
                flushinput();
                ctx.outfp = ctx.tmpfp;
                s_goto(OP_T0LVL);
            }
        case OP_REVERSE:    /* reverse */
            s_return(reverse(car(ctx.args)));
        case OP_APPEND:    /* append */
            s_return(append(car(ctx.args), cadr(ctx.args)));
        case OP_PUT:        /* put */
            if(!hasprop(car(ctx.args)) || !hasprop(cadr(ctx.args)))
            {
                Error_0("Illegal use of put");
            }
            for(x = symprop(car(ctx.args)), y = cadr(ctx.args); x != NIL; x = cdr(x))
                if(caar(x) == y)
                {
                    break;
                }
            if(x != NIL)
            {
                cdar(x) = caddr(ctx.args);
            }
            else
                symprop(car(ctx.args)) = cons(cons(y, caddr(ctx.args)),
                                          symprop(car(ctx.args)));
            s_return(T);
        case OP_GET:        /* get */
            if(!hasprop(car(ctx.args)) || !hasprop(cadr(ctx.args)))
            {
                Error_0("Illegal use of get");
            }
            for(x = symprop(car(ctx.args)), y = cadr(ctx.args); x != NIL; x = cdr(x))
                if(caar(x) == y)
                {
                    break;
                }
            if(x != NIL)
            {
                s_return(cdar(x));
            }
            else
            {
                s_return(NIL);
            }
        case OP_QUIT:        /* quit */
            return (NIL);
        case OP_GC:        /* gc */
            gc(NIL, NIL);
            s_return(T);
        case OP_GCVERB:        /* gc-verbose */
            {
                int    was = ctx.gc_verbose;
                ctx.gc_verbose = (car(ctx.args) != F);
                s_retbool(was);
            }
        case OP_NEWSEGMENT:    /* new-segment */
            if(!isnumber(car(ctx.args)))
            {
                Error_0("new-segment -- argument must be number");
            }
            fprintf(ctx.outfp, "allocate %d new segments\n", alloc_cellseg((int) ivalue(car(ctx.args))));
            s_return(T);
    }
}

pointer opexe_5(register short op)
{
    register pointer x;
    register pointer y;
    switch(op)
    {
        /* ========== reading part ========== */
        case OP_RDSEXPR:
            switch(ctx.tok)
            {
                case TOK_COMMENT:
                    while(inchar() != '\n')
                        ;
                    ctx.tok = token();
                    s_goto(OP_RDSEXPR);
                case TOK_LPAREN:
                    ctx.tok = token();
                    if(ctx.tok == TOK_RPAREN)
                    {
                        s_return(NIL);
                    }
                    else if(ctx.tok == TOK_DOT)
                    {
                        Error_0("syntax error -- illegal dot expression");
                    }
                    else
                    {
                        s_save(OP_RDLIST, NIL, NIL);
                        s_goto(OP_RDSEXPR);
                    }
                case TOK_QUOTE:
                    s_save(OP_RDQUOTE, NIL, NIL);
                    ctx.tok = token();
                    s_goto(OP_RDSEXPR);
                case TOK_BQUOTE:
                    s_save(OP_RDQQUOTE, NIL, NIL);
                    ctx.tok = token();
                    s_goto(OP_RDSEXPR);
                case TOK_COMMA:
                    s_save(OP_RDUNQUOTE, NIL, NIL);
                    ctx.tok = token();
                    s_goto(OP_RDSEXPR);
                case TOK_ATMARK:
                    s_save(OP_RDUQTSP, NIL, NIL);
                    ctx.tok = token();
                    s_goto(OP_RDSEXPR);
                case TOK_ATOM:
                    s_return(mk_atom(readstr("();\t\n ")));
                case TOK_DQUOTE:
                    s_return(mk_string(readstrexp()));
                case TOK_SHARP:
                    if((x = mk_const(readstr("();\t\n "))) == NIL)
                    {
                        Error_0("Undefined sharp expression");
                    }
                    else
                    {
                        s_return(x);
                    }
                default:
                    Error_0("syntax error -- illegal token");
            }
            break;
        case OP_RDLIST:
            ctx.args = cons(ctx.value, ctx.args);
            ctx.tok = token();
            if(ctx.tok == TOK_COMMENT)
            {
                while(inchar() != '\n')
                {
                    ;
                }
                ctx.tok = token();
            }
            if(ctx.tok == TOK_RPAREN)
            {
                s_return(non_alloc_rev(NIL, ctx.args));
            }
            else if(ctx.tok == TOK_DOT)
            {
                s_save(OP_RDDOT, ctx.args, NIL);
                ctx.tok = token();
                s_goto(OP_RDSEXPR);
            }
            else
            {
                s_save(OP_RDLIST, ctx.args, NIL);;
                s_goto(OP_RDSEXPR);
            }
        case OP_RDDOT:
            if(token() != TOK_RPAREN)
            {
                Error_0("syntax error -- illegal dot expression");
            }
            else
            {
                s_return(non_alloc_rev(ctx.value, ctx.args));
            }
        case OP_RDQUOTE:
            s_return(cons(QUOTE, cons(ctx.value, NIL)));
        case OP_RDQQUOTE:
            s_return(cons(QQUOTE, cons(ctx.value, NIL)));
        case OP_RDUNQUOTE:
            s_return(cons(UNQUOTE, cons(ctx.value, NIL)));
        case OP_RDUQTSP:
            s_return(cons(UNQUOTESP, cons(ctx.value, NIL)));
            /* ========== printing part ========== */
        case OP_P0LIST:
            if(!ispair(ctx.args))
            {
                printatom(ctx.args, ctx.print_flag);
                s_return(T);
            }
            else if(car(ctx.args) == QUOTE && ok_abbrev(cdr(ctx.args)))
            {
                fprintf(ctx.outfp, "'");
                ctx.args = cadr(ctx.args);
                s_goto(OP_P0LIST);
            }
            else if(car(ctx.args) == QQUOTE && ok_abbrev(cdr(ctx.args)))
            {
                fprintf(ctx.outfp, "`");
                ctx.args = cadr(ctx.args);
                s_goto(OP_P0LIST);
            }
            else if(car(ctx.args) == UNQUOTE && ok_abbrev(cdr(ctx.args)))
            {
                fprintf(ctx.outfp, ",");
                ctx.args = cadr(ctx.args);
                s_goto(OP_P0LIST);
            }
            else if(car(ctx.args) == UNQUOTESP && ok_abbrev(cdr(ctx.args)))
            {
                fprintf(ctx.outfp, ",@");
                ctx.args = cadr(ctx.args);
                s_goto(OP_P0LIST);
            }
            else
            {
                fprintf(ctx.outfp, "(");
                s_save(OP_P1LIST, cdr(ctx.args), NIL);
                ctx.args = car(ctx.args);
                s_goto(OP_P0LIST);
            }
        case OP_P1LIST:
            if(ispair(ctx.args))
            {
                s_save(OP_P1LIST, cdr(ctx.args), NIL);
                fprintf(ctx.outfp, " ");
                ctx.args = car(ctx.args);
                s_goto(OP_P0LIST);
            }
            else
            {
                if(ctx.args != NIL)
                {
                    fprintf(ctx.outfp, " . ");
                    printatom(ctx.args, ctx.print_flag);
                }
                fprintf(ctx.outfp, ")");
                s_return(T);
            }
        default:
            sprintf(ctx.strbuff, "%d is illegal ctx.operator", ctx.operator);
            Error_0(ctx.strbuff);
    }
    return T;
}

pointer opexe_6(register short op)
{
    register pointer x;
    register pointer y;
    register long v;
    static long w;
    char    buffer[32];
    switch(op)
    {
        case OP_LIST_LENGTH:    /* list-length */    /* a.k */
            for(x = car(ctx.args), v = 0; ispair(x); x = cdr(x))
            {
                ++v;
            }
            s_return(mk_number(v));
        case OP_ASSQ:        /* assq */    /* a.k */
            x = car(ctx.args);
            for(y = cadr(ctx.args); ispair(y); y = cdr(y))
            {
                if(!ispair(car(y)))
                {
                    Error_0("Unable to handle non pair element");
                }
                if(x == caar(y))
                {
                    break;
                }
            }
            if(ispair(y))
            {
                s_return(car(y));
            }
            else
            {
                s_return(F);
            }
        case OP_PRINT_WIDTH:    /* print-width */    /* a.k */
            w = 0;
            ctx.args = car(ctx.args);
            ctx.print_flag = -1;
            s_goto(OP_P0_WIDTH);
        case OP_P0_WIDTH:
            if(!ispair(ctx.args))
            {
                w += printatom(ctx.args, ctx.print_flag);
                s_return(mk_number(w));
            }
            else if(car(ctx.args) == QUOTE
                    && ok_abbrev(cdr(ctx.args)))
            {
                ++w;
                ctx.args = cadr(ctx.args);
                s_goto(OP_P0_WIDTH);
            }
            else if(car(ctx.args) == QQUOTE
                    && ok_abbrev(cdr(ctx.args)))
            {
                ++w;
                ctx.args = cadr(ctx.args);
                s_goto(OP_P0_WIDTH);
            }
            else if(car(ctx.args) == UNQUOTE
                    && ok_abbrev(cdr(ctx.args)))
            {
                ++w;
                ctx.args = cadr(ctx.args);
                s_goto(OP_P0_WIDTH);
            }
            else if(car(ctx.args) == UNQUOTESP
                    && ok_abbrev(cdr(ctx.args)))
            {
                w += 2;
                ctx.args = cadr(ctx.args);
                s_goto(OP_P0_WIDTH);
            }
            else
            {
                ++w;
                s_save(OP_P1_WIDTH, cdr(ctx.args), NIL);
                ctx.args = car(ctx.args);
                s_goto(OP_P0_WIDTH);
            }
        case OP_P1_WIDTH:
            if(ispair(ctx.args))
            {
                s_save(OP_P1_WIDTH, cdr(ctx.args), NIL);
                ++w;
                ctx.args = car(ctx.args);
                s_goto(OP_P0_WIDTH);
            }
            else
            {
                if(ctx.args != NIL)
                {
                    w += 3 + printatom(ctx.args, ctx.print_flag);
                }
                ++w;
                s_return(mk_number(w));
            }
        case OP_GET_CLOSURE:    /* get-closure-code */    /* a.k */
            ctx.args = car(ctx.args);
            if(ctx.args == NIL)
            {
                s_return(F);
            }
            else if(isclosure(ctx.args))
            {
                s_return(cons(LAMBDA, closure_code(ctx.value)));
            }
            else if(ismacro(ctx.args))
            {
                s_return(cons(LAMBDA, closure_code(ctx.value)));
            }
            else
            {
                s_return(F);
            }
        case OP_CLOSUREP:        /* closure? */
            /*
            * Note, macro object is also a closure.
            * Therefore, (closure? <#MACRO>) ==> #t
            */
            if(car(ctx.args) == NIL)
            {
                s_return(F);
            }
            s_retbool(isclosure(car(ctx.args)));
        case OP_MACROP:        /* macro? */
            if(car(ctx.args) == NIL)
            {
                s_return(F);
            }
            s_retbool(ismacro(car(ctx.args)));
        default:
            sprintf(ctx.strbuff, "%d is illegal ctx.operator", ctx.operator);
            Error_0(ctx.strbuff);
    }
    return T;    /* NOTREACHED */
}

pointer(*dispatch_table[])(register short) =
{
    opexe_0,    /* OP_LOAD = 0, */
    opexe_0,    /* OP_T0LVL, */
    opexe_0,    /* OP_T1LVL, */
    opexe_0,    /* OP_READ, */
    opexe_0,    /* OP_VALUEPRINT, */
    opexe_0,    /* OP_EVAL, */
    opexe_0,    /* OP_E0ARGS, */
    opexe_0,    /* OP_E1ARGS, */
    opexe_0,    /* OP_APPLY, */
    opexe_0,    /* OP_DOMACRO, */
    opexe_0,    /* OP_LAMBDA, */
    opexe_0,    /* OP_QUOTE, */
    opexe_0,    /* OP_DEF0, */
    opexe_0,    /* OP_DEF1, */
    opexe_0,    /* OP_BEGIN, */
    opexe_0,    /* OP_IF0, */
    opexe_0,    /* OP_IF1, */
    opexe_0,    /* OP_SET0, */
    opexe_0,    /* OP_SET1, */
    opexe_0,    /* OP_LET0, */
    opexe_0,    /* OP_LET1, */
    opexe_0,    /* OP_LET2, */
    opexe_0,    /* OP_LET0AST, */
    opexe_0,    /* OP_LET1AST, */
    opexe_0,    /* OP_LET2AST, */
    opexe_1,    /* OP_LET0REC, */
    opexe_1,    /* OP_LET1REC, */
    opexe_1,    /* OP_LETREC2, */
    opexe_1,    /* OP_COND0, */
    opexe_1,    /* OP_COND1, */
    opexe_1,    /* OP_DELAY, */
    opexe_1,    /* OP_AND0, */
    opexe_1,    /* OP_AND1, */
    opexe_1,    /* OP_OR0, */
    opexe_1,    /* OP_OR1, */
    opexe_1,    /* OP_C0STREAM, */
    opexe_1,    /* OP_C1STREAM, */
    opexe_1,    /* OP_0MACRO, */
    opexe_1,    /* OP_1MACRO, */
    opexe_1,    /* OP_CASE0, */
    opexe_1,    /* OP_CASE1, */
    opexe_1,    /* OP_CASE2, */
    opexe_1,    /* OP_PEVAL, */
    opexe_1,    /* OP_PAPPLY, */
    opexe_1,    /* OP_CONTINUATION, */
    opexe_2,    /* OP_ADD, */
    opexe_2,    /* OP_SUB, */
    opexe_2,    /* OP_MUL, */
    opexe_2,    /* OP_DIV, */
    opexe_2,    /* OP_REM, */
    opexe_2,    /* OP_CAR, */
    opexe_2,    /* OP_CDR, */
    opexe_2,    /* OP_CONS, */
    opexe_2,    /* OP_SETCAR, */
    opexe_2,    /* OP_SETCDR, */
    opexe_3,    /* OP_NOT, */
    opexe_3,    /* OP_BOOL, */
    opexe_3,    /* OP_NULL, */
    opexe_3,    /* OP_ZEROP, */
    opexe_3,    /* OP_POSP, */
    opexe_3,    /* OP_NEGP, */
    opexe_3,    /* OP_NEQ, */
    opexe_3,    /* OP_LESS, */
    opexe_3,    /* OP_GRE, */
    opexe_3,    /* OP_LEQ, */
    opexe_3,    /* OP_GEQ, */
    opexe_3,    /* OP_SYMBOL, */
    opexe_3,    /* OP_NUMBER, */
    opexe_3,    /* OP_STRING, */
    opexe_3,    /* OP_PROC, */
    opexe_3,    /* OP_PAIR, */
    opexe_3,    /* OP_EQ, */
    opexe_3,    /* OP_EQV, */
    opexe_4,    /* OP_FORCE, */
    opexe_4,    /* OP_WRITE, */
    opexe_4,    /* OP_DISPLAY, */
    opexe_4,    /* OP_NEWLINE, */
    opexe_4,    /* OP_ERR0, */
    opexe_4,    /* OP_ERR1, */
    opexe_4,    /* OP_REVERSE, */
    opexe_4,    /* OP_APPEND, */
    opexe_4,    /* OP_PUT, */
    opexe_4,    /* OP_GET, */
    opexe_4,    /* OP_QUIT, */
    opexe_4,    /* OP_GC, */
    opexe_4,    /* OP_GCVERB, */
    opexe_4,    /* OP_NEWSEGMENT, */
    opexe_5,    /* OP_RDSEXPR, */
    opexe_5,    /* OP_RDLIST, */
    opexe_5,    /* OP_RDDOT, */
    opexe_5,    /* OP_RDQUOTE, */
    opexe_5,    /* OP_RDQQUOTE, */
    opexe_5,    /* OP_RDUNQUOTE, */
    opexe_5,    /* OP_RDUQTSP, */
    opexe_5,    /* OP_P0LIST, */
    opexe_5,    /* OP_P1LIST, */
    opexe_6,    /* OP_LIST_LENGTH, */
    opexe_6,    /* OP_ASSQ, */
    opexe_6,    /* OP_PRINT_WIDTH, */
    opexe_6,    /* OP_P0_WIDTH, */
    opexe_6,    /* OP_P1_WIDTH, */
    opexe_6,    /* OP_GET_CLOSURE, */
    opexe_6,    /* OP_CLOSUREP, */
    opexe_6,    /* OP_MACROP, */
};


/* kernel of this intepreter */
pointer Eval_Cycle(register short op)
{
    ctx.operator = op;
    for(;;)
    {
        if((*dispatch_table[ctx.operator])(ctx.operator) == NIL)
        {
            return NIL;
        }
    }
}

/* ========== Initialization of internal keywords ========== */

void mk_syntax(unsigned short op, char* name)
{
    pointer x;
    x = cons(mk_string(name), NIL);
    scm_type(x) = (T_SYNTAX | T_SYMBOL);
    syntaxnum(x) = op;
    oblist = cons(x, oblist);
}

void mk_proc(unsigned short op, char* name)
{
    pointer x, y;
    x = mk_symbol(name);
    y = get_cell(NIL, NIL);
    scm_type(y) = (T_PROC | T_ATOM);
    ivalue(y) = (long) op;
    car(ctx.global_env) = cons(cons(x, y), car(ctx.global_env));
}

void init_vars_global()
{
    pointer x;
    /* init input/output file */
    ctx.infp = stdin;
    ctx.outfp = stdout;
    /* init NIL */
    scm_type(NIL) = (T_ATOM | MARK);
    car(NIL) = cdr(NIL) = NIL;
    /* init T */
    scm_type(T) = (T_ATOM | MARK);
    car(T) = cdr(T) = T;
    /* init F */
    scm_type(F) = (T_ATOM | MARK);
    car(F) = cdr(F) = F;
    /* init ctx.global_env */
    ctx.global_env = cons(NIL, NIL);
    /* init else */
    x = mk_symbol("else");
    car(ctx.global_env) = cons(cons(x, T), car(ctx.global_env));
}

void init_syntax()
{
    /* init syntax */
    mk_syntax(OP_LAMBDA, "lambda");
    mk_syntax(OP_QUOTE, "quote");
    mk_syntax(OP_DEF0, "define");
    mk_syntax(OP_IF0, "if");
    mk_syntax(OP_BEGIN, "begin");
    mk_syntax(OP_SET0, "set!");
    mk_syntax(OP_LET0, "let");
    mk_syntax(OP_LET0AST, "let*");
    mk_syntax(OP_LET0REC, "letrec");
    mk_syntax(OP_COND0, "cond");
    mk_syntax(OP_DELAY, "delay");
    mk_syntax(OP_AND0, "and");
    mk_syntax(OP_OR0, "or");
    mk_syntax(OP_C0STREAM, "cons-stream");
    mk_syntax(OP_0MACRO, "macro");
    mk_syntax(OP_CASE0, "case");
}

void init_procs()
{
    /* init procedure */
    mk_proc(OP_PEVAL, "eval");
    mk_proc(OP_PAPPLY, "apply");
    mk_proc(OP_CONTINUATION, "call-with-current-continuation");
    mk_proc(OP_FORCE, "force");
    mk_proc(OP_CAR, "car");
    mk_proc(OP_CDR, "cdr");
    mk_proc(OP_CONS, "cons");
    mk_proc(OP_SETCAR, "set-car!");
    mk_proc(OP_SETCDR, "set-cdr!");
    mk_proc(OP_ADD, "+");
    mk_proc(OP_SUB, "-");
    mk_proc(OP_MUL, "*");
    mk_proc(OP_DIV, "/");
    mk_proc(OP_REM, "remainder");
    mk_proc(OP_NOT, "not");
    mk_proc(OP_BOOL, "boolean?");
    mk_proc(OP_SYMBOL, "symbol?");
    mk_proc(OP_NUMBER, "number?");
    mk_proc(OP_STRING, "string?");
    mk_proc(OP_PROC, "procedure?");
    mk_proc(OP_PAIR, "pair?");
    mk_proc(OP_EQV, "eqv?");
    mk_proc(OP_EQ, "eq?");
    mk_proc(OP_NULL, "null?");
    mk_proc(OP_ZEROP, "zero?");
    mk_proc(OP_POSP, "positive?");
    mk_proc(OP_NEGP, "negative?");
    mk_proc(OP_NEQ, "=");
    mk_proc(OP_LESS, "<");
    mk_proc(OP_GRE, ">");
    mk_proc(OP_LEQ, "<=");
    mk_proc(OP_GEQ, ">=");
    mk_proc(OP_READ, "read");
    mk_proc(OP_WRITE, "write");
    mk_proc(OP_DISPLAY, "display");
    mk_proc(OP_NEWLINE, "newline");
    mk_proc(OP_LOAD, "load");
    mk_proc(OP_ERR0, "error");
    mk_proc(OP_REVERSE, "reverse");
    mk_proc(OP_APPEND, "append");
    mk_proc(OP_PUT, "put");
    mk_proc(OP_GET, "get");
    mk_proc(OP_GC, "gc");
    mk_proc(OP_GCVERB, "gc-verbose");
    mk_proc(OP_NEWSEGMENT, "new-segment");
    mk_proc(OP_LIST_LENGTH, "list-length");    /* a.k */
    mk_proc(OP_ASSQ, "assq");    /* a.k */
    mk_proc(OP_PRINT_WIDTH, "print-width");    /* a.k */
    mk_proc(OP_GET_CLOSURE, "get-closure-code");    /* a.k */
    mk_proc(OP_CLOSUREP, "closure?");    /* a.k */
    mk_proc(OP_MACROP, "macro?");    /* a.k */
    mk_proc(OP_QUIT, "quit");
}


/* initialize several globals */
void init_globals()
{
    init_vars_global();
    init_syntax();
    init_procs();
    /* intialization of global pointers to special symbols */
    LAMBDA = mk_symbol("lambda");
    QUOTE = mk_symbol("quote");
    QQUOTE = mk_symbol("quasiquote");
    UNQUOTE = mk_symbol("unquote");
    UNQUOTESP = mk_symbol("unquote-splicing");
}



/* ========== Error ==========  */

FatalError(const char* fmt, const char* a, const char* b, const char* c)
{
    fprintf(stderr, "Fatal error: ");
    fprintf(stderr, fmt, a, b, c);
    fprintf(stderr, "\n");
    exit(1);
}

Error(const char* fmt, const char* a, const char* b, const char* c)
{
    fprintf(stderr, "Error: ");
    fprintf(stderr, fmt, a, b, c);
    fprintf(stderr, "\n");
    flushinput();
    longjmp(ctx.error_jmp, OP_T0LVL);
}

/* ========== Main ========== */

int main(int argc, char** argv)
{
    short i;
    short op;
    op = (short) OP_LOAD;


    ctx.currentline = ctx.linebuff;
    ctx.endline = ctx.linebuff;
    ctx.last_cell_seg = -1;
    ctx.str_seglast = -1;
    ctx.fcells = 0;
    ctx.quiet = 0;
    ctx.all_errors_fatal = 0;


    for(i =1; i < argc; i++)
    {
        if(strcmp(argv[i], "-e") == 0)
        {
            ctx.all_errors_fatal = 1;
        }
        else if(strcmp(argv[i], "-q") == 0)
        {
            ctx.quiet = 1;
        }
    }
    if(!ctx.quiet)
    {
        printf(banner);
    }
    init_scheme();
    ctx.args = cons(mk_string(InitFile), NIL);
    op = setjmp(ctx.error_jmp);
    Eval_Cycle(op);
    exit(0);
}



