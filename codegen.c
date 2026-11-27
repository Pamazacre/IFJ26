/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * codegen.c -- generator IFJcode26 (zasobnikovy, vyrazy cez PUSHS/..S)
 * Autor: xbeniaj00 (Benjamin Joes Benian)
 * generovanie vysledneho IFJcode26 kodu na zaklade AST.
 *
 */

#include "codegen.h"
#include "symtable.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* rastuci string, sem sa sklada vystup */
typedef struct {
  char *data;
  size_t len;
  size_t cap;
} buf_t;

/* cely stav generatora, jeden na beh */
typedef struct {
  Node *prog;
  symtable_t *st;
  buf_t out;            /* hotovy program */
  buf_t decl;           /* DEFvary aktualnej funkcie */
  buf_t body;           /* telo aktualnej funkcie */
  unsigned label;       /* pocitadlo pre unikatne navestia */
  int err;              /* prva chyba co nastala, 0 = ok */
  symbol_t *fn;         /* aktualne generovana funkcia */
  const char *fn_name;  /* jej meno (pre $meno$ret) */
  unsigned char *infer; /* stav odvodenia ret typu: 0 nic, 1 bezi, 2 hotovo */
  bool use_substr;      /* helpery sa generuju len ked su pouzite */
  bool use_ord;
} gen_t;

static gen_t g;

/* zapamata prvu chybu, dalsie uz nic neprepisu */
static void fail(int code) {
  if (g.err == 0)
    g.err = code;
}

/* printf na koniec bufferu, pri malloc chybe nastavi 99 a nic nerobi */
static void buf_printf(buf_t *b, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int need = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (need < 0 || g.err == 99)
    return;
  if (b->len + (size_t)need + 1 > b->cap) {
    size_t cap = b->cap ? b->cap : 256;
    while (b->len + (size_t)need + 1 > cap)
      cap *= 2;
    char *data = realloc(b->data, cap);
    if (data == NULL) {
      fail(99);
      return;
    }
    b->data = data;
    b->cap = cap;
  }
  va_start(ap, fmt);
  vsnprintf(b->data + b->len, (size_t)need + 1, fmt, ap);
  va_end(ap);
  b->len += (size_t)need;
}

/* prilepi cely buffer src za dst */
static void buf_append(buf_t *dst, const buf_t *src) {
  if (src->len > 0)
    buf_printf(dst, "%s", src->data);
}

/* vyprazdni buffer ale pamat necha na dalsie pouzitie */
static void buf_clear(buf_t *b) {
  b->len = 0;
  if (b->data != NULL)
    b->data[0] = '\0';
}

/* jeden riadok instrukcie do tela aktualnej funkcie */
#define EMIT(...)                                                              \
  do {                                                                         \
    buf_printf(&g.body, __VA_ARGS__);                                          \
    buf_printf(&g.body, "\n");                                                 \
  } while (0)

/* typy */

/* DataType z AST -> typ symtable (enumy maju ine hodnoty, pozor) */
static data_type_t from_tree(DataType d) {
  switch (d) {
  case D_INT:
    return TYPE_INT;
  case D_FLOAT:
    return TYPE_DOUBLE;
  case D_STRING:
    return TYPE_STRING;
  case D_BOOL:
    return TYPE_BOOLEAN;
  case D_UNIT:
    return TYPE_UNIT;
  default:
    return TYPE_UNKNOWN;
  }
}

/* index funkcie v programe podla mena, -1 = nie je builtin*/
static int func_index(const char *name) {
  for (int i = 0; i < g.prog->as.program.count; i++)
    if (strcmp(g.prog->as.program.funcs[i]->as.func.name, name) == 0)
      return i;
  return -1;
}

static data_type_t expr_type(Node *n);
static data_type_t infer_ret(symbol_t *fn);

/* typ vysledku binarnej operacie */
static data_type_t binop_type(Node *n) {
  switch (n->as.binop.op) {
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_MUL:
  case TOKEN_DIV: {
    data_type_t l = expr_type(n->as.binop.l);
    data_type_t r = expr_type(n->as.binop.r);
    if (l == TYPE_STRING)
      return TYPE_STRING; /* len + dvoch stringov */
    if (l == TYPE_DOUBLE || r == TYPE_DOUBLE)
      return TYPE_DOUBLE;
    return l;
  }
  default:
    return TYPE_BOOLEAN; /* && || (BOOLTHEN) */
  }
}

/* staticky typ vyrazu v aktualnych scopoch */
static data_type_t expr_type(Node *n) {
  switch (n->kind) {
  case N_NUM:
    return TYPE_INT;
  case N_DOUBLE:
    return TYPE_DOUBLE;
  case N_STRING:
    return TYPE_STRING;
  case N_UNIT:
    return TYPE_UNIT;
  case N_BOOL:
    return TYPE_BOOLEAN;
  case N_VAR: {
    symbol_t *v = symtable_find_var(g.st, n->as.var.name);
    return v != NULL ? v->var.type : TYPE_UNKNOWN;
  }
  case N_UNOP: /* typ nemeni */
    if (n->as.unop.op == TOKEN_NOT)
      return TYPE_BOOLEAN;
    return expr_type(n->as.unop.operand);
  case N_BINOP:
    return binop_type(n);
  case N_CALL: {
    symbol_t *f = symtable_find_func(g.st, n->as.call.name);
    if (f == NULL)
      return TYPE_UNKNOWN;
    if (f->func.ret_type == TYPE_UNKNOWN && !f->func.is_builtin)
      return infer_ret(f);
    return f->func.ret_type;
  }
  default:
    /* TODO CONTROL if-vyraz, VALMEM .length.., INTERPOLATEDS, LISTS */
    // TODO vymaz nakoniec nerobime
    return TYPE_UNKNOWN;
  }
}

/* typ hodnoty prikazu, ked je posledny v tele funkcie */
static data_type_t stmt_type(Node *s) {
  switch (s->kind) {
  case N_VARDEF:
  case N_ASSIGN:
  case N_WHILE:
  case N_IF: /* bez CONTROL if nie je vyraz, vetvy su Unit */ // TODO Remove
                                                              // CONTROL if
                                                              // vyrazy
    return TYPE_UNIT;
  case N_RETURN:
    return s->as.ret.value ? expr_type(s->as.ret.value) : TYPE_UNIT;
  default:
    return expr_type(s);
  }
}

/* typ premennej z definicie: uvedeny typ ma prednost pred init */
static data_type_t vardef_type(Node *s) {
  data_type_t t = from_tree(s->as.vardef.declared);
  return t != TYPE_UNKNOWN ? t : expr_type(s->as.vardef.init);
}

/* vlozi premennu do najvnutornejsieho scope, NULL pri chybe */
static symbol_t *declare(const char *name, data_type_t type, bool mut) {
  symbol_t *v = NULL;
  st_status_t rc = symtable_insert_var(g.st, name, type, mut, &v);
  if (rc == ST_ERR_REDEF)
    fail(4); /* poistka, inak to chyti analyzer */
  else if (rc != ST_OK)
    fail(99);
  return rc == ST_OK ? v : NULL;
}

/* vysledny typ bloku = typ posledneho prikazu, definicie sa prechadzaju
 * kvoli typom premennych nested bloky ich nevidia */
static data_type_t block_type(Node *blk) {
  if (!symtable_push_scope(g.st)) {
    fail(99);
    return TYPE_UNKNOWN;
  }
  data_type_t t = TYPE_UNIT;
  for (int i = 0; i < blk->as.block.count; i++) {
    Node *s = blk->as.block.stmts[i];
    if (i == blk->as.block.count - 1)
      t = stmt_type(s); /* pred vlozenim, val x = x + 1 vidi outer x */
    if (s->kind == N_VARDEF)
      declare(s->as.vardef.name, vardef_type(s), s->as.vardef.is_mutable);
  }
  symtable_pop_scope(g.st);
  return t;
}

/* odvodi navratovy typ funkcie bez typu v hlavicke z jej tela
 * rekurzia bez typu v hlavicke je chyba 6 (analyzer) -> tu UNKNOWN */
static data_type_t infer_ret(symbol_t *fn) {
  int idx = func_index(fn->name);
  if (idx < 0 || g.infer[idx] == 1)
    return TYPE_UNKNOWN;
  if (g.infer[idx] == 2)
    return fn->func.ret_type;
  g.infer[idx] = 1;

  Node *f = g.prog->as.program.funcs[idx];
  data_type_t t = TYPE_UNKNOWN;
  if (symtable_push_scope(g.st)) {
    for (int i = 0; i < f->as.func.nparams; i++)
      declare(f->as.func.params[i].name, from_tree(f->as.func.params[i].type),
              false);
    Node *body = f->as.func.body;
    t = body->kind == N_BLOCK ? block_type(body) : stmt_type(body);
    symtable_pop_scope(g.st);
  } else {
    fail(99);
  }
  fn->func.ret_type = t;
  g.infer[idx] = 2;
  return t;
}

/* generovanie vyrazov, kazdy vyraz necha na zasobniku presne 1 hodnotu */

/* string konstanta: vsetko mimo tlacitelnych ASCII, # a \ ide ako \ddd */
static void emit_string_const(const char *s) {
  buf_printf(&g.body, "PUSHS string@");
  for (; *s != '\0'; s++) {
    unsigned char c = (unsigned char)*s;
    if (c <= 32 || c == 35 || c == 92 || c > 126)
      buf_printf(&g.body, "\\%03u", (unsigned)c);
    else
      buf_printf(&g.body, "%c", c);
  }
  buf_printf(&g.body, "\n");
}

/* najde premennu od vnutra von, ked neexistuje tak fail(3) a NULL */
static symbol_t *find_var(const char *name) {
  symbol_t *v = symtable_find_var(g.st, name);
  if (v == NULL)
    fail(3); /* poistka */
  return v;
}

static void gen_expr(Node *n);

/* vyraz a prevod int Double ked to ciel chce
 * target UNKNOWN  bez konverzie */
static void gen_expr_to(Node *n, data_type_t target) {
  gen_expr(n);
  if (target == TYPE_DOUBLE && expr_type(n) == TYPE_INT)
    EMIT("INT2FLOATS");
}

/* && a || zkratovo (BOOLTHEN): pravy operand sa vyhodnoti len ked treba,
 * hodnoty idu cez zasobnik */
static void gen_logic(Node *n) {
  unsigned l = ++g.label;
  bool is_and = n->as.binop.op == TOKEN_AND;
  const char *stop = is_and ? "bool@false" : "bool@true";
  gen_expr(n->as.binop.l);
  EMIT("PUSHS %s", stop);
  EMIT("JUMPIFEQS !short%u", l);
  gen_expr(n->as.binop.r); /* inak rozhodne pravy operand */
  EMIT("JUMP !logic%u", l);
  EMIT("LABEL !short%u", l);
  EMIT("PUSHS %s", stop);
  EMIT("LABEL !logic%u", l);
}

static void gen_binop(Node *n) {
  Node *l = n->as.binop.l;
  Node *r = n->as.binop.r;
  if (n->as.binop.op == TOKEN_AND || n->as.binop.op == TOKEN_OR) {
    gen_logic(n);
    return;
  }
  data_type_t lt = expr_type(l);
  data_type_t rt = expr_type(r);
  /* zmiesane int a double -> obe na double, operandy sa hned konvertuju aby
   * sa nemuselo hrabat pod vrchol zasobnika */
  data_type_t num =
      (lt == TYPE_DOUBLE || rt == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_UNKNOWN;
  gen_expr_to(l, num);
  gen_expr_to(r, num);

  switch (n->as.binop.op) {
  case TOKEN_PLUS:
    if (lt == TYPE_STRING) { /* nema zasobnikovu verziu */
      EMIT("POPS GF@%%b");
      EMIT("POPS GF@%%a");
      EMIT("CONCAT GF@%%a GF@%%a GF@%%b");
      EMIT("PUSHS GF@%%a");
    } else {
      EMIT("ADDS");
    }
    break;
  /* TODO overflow? */
  case TOKEN_MINUS:
    EMIT("SUBS");
    break;
  case TOKEN_MUL:
    EMIT("MULS");
    break;
  case TOKEN_DIV:
    EMIT("%s", num == TYPE_DOUBLE ? "DIVS" : "IDIVS");
    break;
  case TOKEN_LT:
    EMIT("LTS");
    break;
  case TOKEN_GT:
    EMIT("GTS");
    break;
  case TOKEN_LE: /* a <= b == !(a > b) */
    EMIT("GTS");
    EMIT("NOTS");
    break;
  case TOKEN_GE:
    EMIT("LTS");
    EMIT("NOTS");
    break;
  case TOKEN_EQ: /* () == () funguje */
    EMIT("EQS");
    break;
  case TOKEN_NEQ:
    EMIT("EQS");
    EMIT("NOTS");
    break;
  /* TODO LISTS */
  default:
    fail(99);
  }
}

/* vstavane funkcie, true = bola to vstavana a je vygenerovana */
static bool gen_builtin(Node *n) {
  const char *name = n->as.call.name;
  Node **args = n->as.call.args;
  unsigned l = ++g.label;

  if (strcmp(name, "print") == 0) {
    gen_expr(args[0]);
    EMIT("POPS GF@%%a");
    if (expr_type(args[0]) == TYPE_UNIT)
      EMIT("WRITE string@()");
    else
      EMIT("WRITE GF@%%a"); /* Int %d, Double %a, String ako je */
    EMIT("PUSHS nil@nil");  /* print vracia () */
  } else if (strcmp(name, "readLine") == 0) {
    EMIT("READ GF@%%a string");
    EMIT("JUMPIFNEQ !read%u GF@%%a nil@nil", l);
    EMIT("MOVE GF@%%a string@"); /* EOF -> prazdny string */
    EMIT("LABEL !read%u", l);
    EMIT("PUSHS GF@%%a");
  } else if (strcmp(name, "readInt") == 0) {
    /* READ cita 64 bit, Int je 32 bit -> mimo rozsahu tiez 0 */
    EMIT("READ GF@%%a int");
    EMIT("JUMPIFEQ !readzero%u GF@%%a nil@nil", l);
    EMIT("LT GF@%%b GF@%%a int@-2147483648");
    EMIT("JUMPIFEQ !readzero%u GF@%%b bool@true", l);
    EMIT("GT GF@%%b GF@%%a int@2147483647");
    EMIT("JUMPIFEQ !readzero%u GF@%%b bool@true", l);
    EMIT("JUMP !read%u", l);
    EMIT("LABEL !readzero%u", l);
    EMIT("MOVE GF@%%a int@0");
    EMIT("LABEL !read%u", l);
    EMIT("PUSHS GF@%%a");
  } else if (strcmp(name, "readDouble") == 0) {
    EMIT("READ GF@%%a float");
    EMIT("JUMPIFNEQ !read%u GF@%%a nil@nil", l);
    EMIT("MOVE GF@%%a float@0x0p+0");
    EMIT("LABEL !read%u", l);
    EMIT("PUSHS GF@%%a");
  } else if (strcmp(name, "toStr") == 0) {
    data_type_t t = expr_type(args[0]);
    gen_expr(args[0]);
    if (t == TYPE_INT) {
      EMIT("INT2STRS");
    } else if (t == TYPE_DOUBLE) {
      EMIT("FLOAT2STRS"); /* %.2f */
    } else if (t == TYPE_UNIT) {
      EMIT("POPS GF@%%a");
      EMIT("PUSHS string@()");
    } else if (t == TYPE_BOOLEAN) { /* BOOLTHEN: "true" / "false" */
      EMIT("PUSHS bool@true");
      EMIT("JUMPIFEQS !strue%u", l);
      EMIT("PUSHS string@false");
      EMIT("JUMP !send%u", l);
      EMIT("LABEL !strue%u", l);
      EMIT("PUSHS string@true");
      EMIT("LABEL !send%u", l);
    }
    /* String ostava ako je */
  } else if (strcmp(name, "toInt") == 0) {
    gen_expr_to(args[0], TYPE_DOUBLE); /* toInt(5) je ok, Int sa rozsiri */
    EMIT("FLOAT2INTS");
  } else if (strcmp(name, "strLength") == 0) {
    gen_expr(args[0]);
    EMIT("POPS GF@%%a");
    EMIT("STRLEN GF@%%a GF@%%a");
    EMIT("PUSHS GF@%%a");
  } else if (strcmp(name, "chr") == 0) {
    gen_expr(args[0]);
    EMIT("INT2CHARS"); /* mimo 0..127 -> chyba interpretu 58 */
  } else if (strcmp(name, "subStr") == 0) {
    gen_expr(args[0]);
    gen_expr(args[1]);
    gen_expr(args[2]);
    EMIT("CALL !subStr");
    g.use_substr = true;
  } else if (strcmp(name, "ord") == 0) {
    gen_expr(args[0]);
    gen_expr(args[1]);
    EMIT("CALL !ord");
    g.use_ord = true;
  } else if (strcmp(name, "readBoolean") == 0) { /* BOOLTHEN, ako READ bool */
    EMIT("READ GF@%%a bool");
    EMIT("JUMPIFNEQ !read%u GF@%%a nil@nil", l);
    EMIT("MOVE GF@%%a bool@false"); /* EOF -> false (ako 0 pri readInt) */
    EMIT("LABEL !read%u", l);
    EMIT("PUSHS GF@%%a");
  } else {
    return false;
  }
  return true;
}

/* volanie, vysledok ostane na zasobniku (aj Unit = nil) */
static void gen_call(Node *n) {
  symbol_t *f = symtable_find_func(g.st, n->as.call.name);
  if (f == NULL) {
    fail(3); /* poistka */
    return;
  }
  if ((size_t)n->as.call.nargs != f->func.param_count) {
    fail(5); /* poistka, inak by builtin cital mimo pola args */
    return;
  }
  if (f->func.is_builtin) {
    if (!gen_builtin(n))
      fail(99);
    return;
  }
  /* argumenty zlava doprava, Int do Double parametra sa rozsiri */
  for (int i = 0; i < n->as.call.nargs; i++) {
    data_type_t pt =
        (size_t)i < f->func.param_count ? f->func.param_types[i] : TYPE_UNKNOWN;
    gen_expr_to(n->as.call.args[i], pt);
  }
  EMIT("CALL $%s", n->as.call.name);
}

static void gen_expr(Node *n) {
  if (g.err != 0)
    return;
  switch (n->kind) {
  case N_NUM:
    EMIT("PUSHS int@%lld", n->as.num.value);
    break;
  case N_DOUBLE:
    EMIT("PUSHS float@%a", n->as.double_.value);
    break;
  case N_STRING:
    emit_string_const(n->as.string.value);
    break;
  case N_UNIT:
    EMIT("PUSHS nil@nil");
    break;
  case N_BOOL: /* BOOLTHEN */
    EMIT("PUSHS bool@%s", n->as.bool_.value ? "true" : "false");
    break;
  case N_VAR: {
    symbol_t *v = find_var(n->as.var.name);
    if (v != NULL)
      EMIT("PUSHS LF@%s$%u", v->name, v->var.uid);
    break;
  }
  case N_UNOP:
    gen_expr(n->as.unop.operand);
    if (n->as.unop.op == TOKEN_NOT) { /* ! (BOOLTHEN) */
      EMIT("NOTS");
      break;
    }
    if (expr_type(n->as.unop.operand) == TYPE_DOUBLE)
      EMIT("PUSHS float@-0x1p+0");
    else
      EMIT("PUSHS int@-1");
    EMIT("MULS");
    break;
  case N_BINOP:
    gen_binop(n);
    break;
  case N_CALL: /* FUNEXP uz funguje, volanie moze byt kdekolvek vo vyraze */
    gen_call(n);
    break;
  default:
    fail(99);
  }
}

/* prikazy */

static void gen_block(Node *blk, bool want_value);

/* podmienka ifu/whilu, pri false skoci na label */
static void gen_cond(Node *cond, const char *label, unsigned id) {
  gen_expr(cond);
  EMIT("PUSHS bool@true");
  EMIT("JUMPIFNEQS %s%u", label, id);
}

static void ret_convert(data_type_t t) {
  if (!g.fn->func.ret_explicit)
    return;
  if (g.fn->func.ret_type == TYPE_UNIT && t != TYPE_UNIT) {
    EMIT("POPS GF@%%a");
    EMIT("PUSHS nil@nil");
  } else if (g.fn->func.ret_type == TYPE_DOUBLE && t == TYPE_INT) {
    EMIT("INT2FLOATS");
  }
}

/* want_value = prikaz je posledny v tela funkcie, jeho hodnota sa vracia */
static void gen_stmt(Node *s, bool want_value) {
  if (g.err != 0)
    return;
  unsigned l;
  switch (s->kind) {
  case N_VARDEF: {
    data_type_t t = vardef_type(s);
    gen_expr_to(s->as.vardef.init, t); /* init sa pocita este bez novej x */
    symbol_t *v = declare(s->as.vardef.name, t, s->as.vardef.is_mutable);
    if (v == NULL)
      return;
    buf_printf(&g.decl, "DEFVAR LF@%s$%u\n", v->name, v->var.uid);
    EMIT("POPS LF@%s$%u", v->name, v->var.uid);
    break;
  }
  case N_ASSIGN: {
    symbol_t *v = find_var(s->as.assign.name);
    if (v == NULL)
      return;
    gen_expr_to(s->as.assign.value, v->var.type);
    EMIT("POPS LF@%s$%u", v->name, v->var.uid);
    break;
  }
  case N_IF:
    l = ++g.label;
    gen_cond(s->as.if_.cond, "!else", l);
    gen_block(s->as.if_.then_b, false);
    EMIT("JUMP !endif%u", l);
    EMIT("LABEL !else%u", l);
    if (s->as.if_.else_b !=
        NULL) /* bez else / else if = blok s if (BOOLTHEN) */
      gen_block(s->as.if_.else_b, false);
    EMIT("LABEL !endif%u", l);
    break;
  case N_WHILE:
    l = ++g.label;
    EMIT("LABEL !while%u", l);
    gen_cond(s->as.while_.cond, "!endwhile", l);
    gen_block(s->as.while_.body, false);
    EMIT("JUMP !while%u", l);
    EMIT("LABEL !endwhile%u", l);
    break;

  case N_RETURN:
    if (s->as.ret.value != NULL) {
      gen_expr(s->as.ret.value);
      ret_convert(expr_type(s->as.ret.value));
    } else {
      EMIT("PUSHS nil@nil");
    }
    EMIT("JUMP $%s$ret", g.fn_name);
    return; /* dalej sa nejde, zasobnik je ok */
  default:  /* samostatny vyraz alebo volanie */
    gen_expr(s);
    if (want_value) {
      ret_convert(expr_type(s));
      return;
    }
    EMIT("POPS GF@%%a"); /* vysledok sa zahodi */
    return;
  }
  if (want_value) { /* definicia, priradenie, if, while -> () */
    EMIT("PUSHS nil@nil");
  }
}

/* blok = novy scope, len posledny prikaz moze returnovat hodnotu */
static void gen_block(Node *blk, bool want_value) {
  if (!symtable_push_scope(g.st)) {
    fail(99);
    return;
  }
  for (int i = 0; i < blk->as.block.count; i++)
    gen_stmt(blk->as.block.stmts[i],
             want_value && i == blk->as.block.count - 1);
  if (want_value && blk->as.block.count == 0)
    EMIT("PUSHS nil@nil");
  symtable_pop_scope(g.st);
}

/* jedna funkcia navestie ramec,DEFVARy parametre,telo, navrat */
static void gen_func(Node *f) {
  g.fn = symtable_find_func(g.st, f->as.func.name);
  g.fn_name = f->as.func.name;
  buf_clear(&g.decl);
  buf_clear(&g.body);
  if (!symtable_push_scope(g.st)) {
    fail(99);
    return;
  }

  /* parametre popujeme  odzadu lebo posledny argument je na vrchu */
  buf_t pops = {NULL, 0, 0};
  for (int i = 0; i < f->as.func.nparams; i++) {
    Param *p = &f->as.func.params[i];
    symbol_t *v = declare(p->name, from_tree(p->type), false);
    if (v == NULL)
      break;
    buf_printf(&g.decl, "DEFVAR LF@%s$%u\n", v->name, v->var.uid);
  }
  for (int i = f->as.func.nparams - 1; i >= 0 && g.err == 0; i--) {
    symbol_t *v = symtable_find_var(g.st, f->as.func.params[i].name);
    buf_printf(&pops, "POPS LF@%s$%u\n", v->name, v->var.uid);
  }

  Node *body = f->as.func.body;
  if (body->kind == N_BLOCK)
    gen_block(body, true);
  else
    gen_stmt(body, true); /* oneliner body */
  symtable_pop_scope(g.st);

  buf_printf(&g.out, "\n# def %s\n", f->as.func.name);
  buf_printf(&g.out, "LABEL $%s\n", f->as.func.name);
  buf_printf(&g.out, "CREATEFRAME\nPUSHFRAME\n");
  buf_append(&g.out, &g.decl);
  buf_append(&g.out, &pops);
  buf_append(&g.out, &g.body);
  buf_printf(&g.out, "LABEL $%s$ret\n", f->as.func.name);
  buf_printf(&g.out, "POPFRAME\nRETURN\n");
  free(pops.data);
}

/* helpery pre vstavane fcie co potrebuju cyklus */
static void gen_helpers(void) {
  if (g.use_substr) {
    /* chyba 40 ked i<0, j<0, i>j, i>=len, j>len */
    buf_printf(&g.out,
               "\n# subStr(s, i, j)\n"
               "LABEL !subStr\nCREATEFRAME\nPUSHFRAME\n"
               "DEFVAR LF@s\nDEFVAR LF@i\nDEFVAR LF@j\nDEFVAR LF@len\n"
               "DEFVAR LF@r\nDEFVAR LF@c\nDEFVAR LF@b\n"
               "POPS LF@j\nPOPS LF@i\nPOPS LF@s\nSTRLEN LF@len LF@s\n"
               "LT LF@b LF@i int@0\nJUMPIFEQ !subStr$err LF@b bool@true\n"
               "LT LF@b LF@j int@0\nJUMPIFEQ !subStr$err LF@b bool@true\n"
               "GT LF@b LF@i LF@j\nJUMPIFEQ !subStr$err LF@b bool@true\n"
               "LT LF@b LF@i LF@len\nJUMPIFEQ !subStr$err LF@b bool@false\n"
               "GT LF@b LF@j LF@len\nJUMPIFEQ !subStr$err LF@b bool@true\n"
               "MOVE LF@r string@\n"
               "LABEL !subStr$loop\n"
               "JUMPIFEQ !subStr$done LF@i LF@j\n"
               "GETCHAR LF@c LF@s LF@i\nCONCAT LF@r LF@r LF@c\n"
               "ADD LF@i LF@i int@1\nJUMP !subStr$loop\n"
               "LABEL !subStr$done\nPUSHS LF@r\nPOPFRAME\nRETURN\n"
               "LABEL !subStr$err\nEXIT int@40\n");
  }
  if (g.use_ord) {
    /* -1 ked je s prazdny alebo i mimo */
    buf_printf(&g.out,
               "\n# ord(s, i)\n"
               "LABEL !ord\nCREATEFRAME\nPUSHFRAME\n"
               "DEFVAR LF@s\nDEFVAR LF@i\nDEFVAR LF@len\nDEFVAR LF@b\n"
               "POPS LF@i\nPOPS LF@s\nSTRLEN LF@len LF@s\n"
               "LT LF@b LF@i int@0\nJUMPIFEQ !ord$bad LF@b bool@true\n"
               "LT LF@b LF@i LF@len\nJUMPIFEQ !ord$bad LF@b bool@false\n"
               "STRI2INT LF@b LF@s LF@i\nPUSHS LF@b\nPOPFRAME\nRETURN\n"
               "LABEL !ord$bad\nPUSHS int@-1\nPOPFRAME\nRETURN\n");
  }
}

/* naplni symtable funkciami z programu 2 prechody kvoli volaniu pred
 * definiciou  potom odvodi chybajuce ret typy */
static void collect_funcs(void) {
  int n = g.prog->as.program.count;
  for (int i = 0; i < n && g.err == 0; i++) {
    Node *f = g.prog->as.program.funcs[i];
    symbol_t *sym = NULL;
    st_status_t rc = symtable_insert_func(g.st, f->as.func.name,
                                          from_tree(f->as.func.ret), &sym);
    if (rc != ST_OK) {
      fail(rc == ST_ERR_REDEF ? 4 : 99);
      return;
    }
    sym->func.is_main = f->as.func.is_main != 0;
    for (int p = 0; p < f->as.func.nparams; p++)
      if (!symtable_add_param(sym, from_tree(f->as.func.params[p].type)))
        fail(99);
  }
  for (int i = 0; i < n && g.err == 0; i++) {
    symbol_t *sym =
        symtable_find_func(g.st, g.prog->as.program.funcs[i]->as.func.name);
    if (sym->func.ret_type == TYPE_UNKNOWN)
      infer_ret(sym);
  }
}

int codegen(Node *prog, FILE *out) {
  memset(&g, 0, sizeof g);
  g.prog = prog;
  g.st = symtable_create();
  int n = prog->as.program.count;
  g.infer = calloc(n > 0 ? (size_t)n : 1, 1);
  if (g.st == NULL || g.infer == NULL || !symtable_insert_builtins(g.st))
    fail(99);
  else
    collect_funcs();

  const char *main_name = NULL;
  for (int i = 0; i < n; i++)
    if (prog->as.program.funcs[i]->as.func.is_main)
      main_name = prog->as.program.funcs[i]->as.func.name;
  if (main_name == NULL)
    fail(3); /* poistka, chyba @main */

  if (g.err == 0) {
    buf_printf(&g.out, ".IFJcode26\n");
    buf_printf(&g.out, "DEFVAR GF@%%a\nDEFVAR GF@%%b\n");
    buf_printf(&g.out, "CALL $%s\nEXIT int@0\n", main_name);
    for (int i = 0; i < n && g.err == 0; i++)
      gen_func(prog->as.program.funcs[i]);
    gen_helpers();
  }

  if (g.err == 0 && g.out.data != NULL)
    fputs(g.out.data, out);

  free(g.out.data);
  free(g.decl.data);
  free(g.body.data);
  free(g.infer);
  symtable_free(g.st);
  return g.err;
}

// EOF TODO// pozri este -clang formatovanie dik