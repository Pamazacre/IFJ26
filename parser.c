/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * parser.c -- syntax. preklad
 * Autor: xsenkyp00 (Pavel Šenkýř)
 */

#include "token.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Token cur;
static Token la; // token za cur
static int has_la = 0;

// terminaly precedencni tabulky
typedef enum {
  P_UNARY,   // unarni - a ! (BOOLTHEN)
  P_MULDIV,  // * /
  P_PLUSMIN, // + -
  P_REL,     // < <= > >=
  P_EQ,      // == !=
  P_AND,     // && (BOOLTHEN)
  P_OR,      // || (BOOLTHEN)
  P_LPAR,    // (
  P_RPAR,    // )
  P_TERM,    // literal, promenna, ()
  P_END,     // $ = cokoliv, co do vyrazu nepatri
  P_E,       // neterminal E (uz zredukovany vyraz)
  P_MARK     // znacka '<' = zacatek handle
} PrecSym;

// Opus 5.5 s prilohou prednasky "Uprav mi precedencni tabulku abych ji mohl
// pouzit v jazyku C, podle prednasky".
#define L 1
#define R 2
#define Q 3
#define X 0

static const int prec_table[11][11] = {
    // prec tabulka musela byt zvetsena kvuli boolthen extension && a ||
    //           u  */ +- rl eq && || (  )  i  $
    /* u !   */ {L, R, R, R, R, R, R, L, R, L, R},
    /* * /   */ {L, R, R, R, R, R, R, L, R, L, R},
    /* + -   */ {L, L, R, R, R, R, R, L, R, L, R},
    /* rel   */ {L, L, L, X, R, R, R, L, R, L, R}, // a < b < c - chyba
    /* == != */ {L, L, L, L, X, R, R, L, R, L, R}, // a == b == c - chyba
    /* &&    */ {L, L, L, L, L, R, R, L, R, L, R},
    /* ||    */ {L, L, L, L, L, L, R, L, R, L, R},
    /* (     */ {L, L, L, L, L, L, L, L, Q, L, X},
    /* )     */ {X, R, R, R, R, R, R, X, R, X, R},
    /* i     */ {X, R, R, R, R, R, R, X, R, X, R},
    /* $     */ {L, L, L, L, L, L, L, L, X, L, X},
};

typedef struct {
  PrecSym sym;
  TokenType op; // u operatoru: ktery to je
  Node *node;   // u P_TERM a P_E: uzel AST
  int line;
} PrecItem;

typedef struct { // obyc dynam. pole
  PrecItem *items;
  int top; // pocet polozek
  int cap;
} PrecStack;

// prida polozku na vrchol prec. zasobniku, pri zaplneni zdvojnasobi kapacitu
// (exit 99 kdyz dojde pamet)
static void ps_push(PrecStack *s, PrecItem it) {
  if (s->top == s->cap) { // pole je full -> potreba zvetsit
    if (s->cap == 0)
      s->cap = 16;
    else
      s->cap = s->cap * 2;

    s->items = realloc(s->items, s->cap * sizeof(PrecItem));
    if (s->items == NULL)
      exit(99); // dosla pamet
  }
  s->items[s->top++] = it; // ulozit na vrchol
}

// index nejvyssiho terminalu (preskoci E a znacky)
static int ps_top_term(PrecStack *s) {
  for (int i = s->top - 1; i >= 0; i--) // od vrcholu dolu
    if (s->items[i].sym != P_E && s->items[i].sym != P_MARK)
      return i;
  return -1; // na dne je vzdy $, takze SNAD nenastane
}

// nacte dalsi token ze scanneru, pri chybe skonci: 1 lex, 2 spatny dedent, 99
// interni
static void read_token(Token *t) {
  int rc = get_token(t);
  if (rc == 1)
    exit(1); // lex chyba
  if (rc == 2)
    exit(2); // spatny dedent od scanneru je syntakticka chyba
  if (rc != 0)
    exit(99); // interni chyba
}

// posune cur na dalsi token
static void next(void) {
  if (has_la) { // (vezme ho z la, pokud uz byl nacteny pres peek)
    cur = la;
    has_la = 0;
  } else
    read_token(&cur);
}

// vrati token za cur
static Token peek(void) {
  if (!has_la) {
    read_token(&la);
    has_la = 1;
  }
  return la;
}

// prevede aktualni token (cur) na terminal tabulky
// prev_operand = 1, kdyz predchozi byl operand nebo ')', tj. '-' je binarni
static PrecSym token_to_prec(int prev_operand) {
  switch (cur.type) {
  case TOKEN_MINUS:
    return prev_operand ? P_PLUSMIN : P_UNARY;
  case TOKEN_NOT: // ! je vzdy unarni
    return P_UNARY;
  case TOKEN_AND:
    return P_AND;
  case TOKEN_OR:
    return P_OR;
  case TOKEN_PLUS:
    return P_PLUSMIN;
  case TOKEN_MUL:
    return P_MULDIV;
  case TOKEN_DIV:
    return P_MULDIV;
  case TOKEN_LT:
    return P_REL;
  case TOKEN_LE:
    return P_REL;
  case TOKEN_GT:
    return P_REL;
  case TOKEN_GE:
    return P_REL;
  case TOKEN_EQ:
    return P_EQ;
  case TOKEN_NEQ:
    return P_EQ;
  case TOKEN_LPAR: // "()" je Unit = term, jinak zavorka
    return peek().type == TOKEN_RPAR ? P_TERM : P_LPAR;
  case TOKEN_RPAR:
    return P_RPAR;
  case TOKEN_INT:
    return P_TERM;
  case TOKEN_DOUBLE:
    return P_TERM;
  case TOKEN_STRING:
    return P_TERM;
  case TOKEN_ID:
    return P_TERM;
  case TOKEN_TRUE:
  case TOKEN_FALSE:
  case TOKEN_ISTR_START: // s"neco tady" je cely jeden term
    return P_TERM;

  default:
    return P_END; // then, do, EOL atd-> konec vyrazu
  }
}

// Bez _Noreturn hazi warningy
_Noreturn static void error(void) {
  fprintf(stderr, "syntakticka chyba, radek %d\n", cur.line);
  exit(2);
}

// redukce: vsechno nad posledni znackou nahradi jednim E
static void reduce(PrecStack *s) {
  // najit posledni znacku '<'
  int m = s->top - 1;
  while (m >= 0 && s->items[m].sym != P_MARK)
    m--;
  if (m < 0)
    error(); // zadna znacka -> neni co redukovat

  // handle = vsechno nad znackou
  PrecItem *h = &s->items[m + 1]; // h[0] = prvni polozka za znackou
  int n = s->top - m - 1;         // kolik polozek handle ma
  Node *res;

  // porovnat s pravidly a postavit uzel AST
  if (n == 1 && h[0].sym == P_TERM) // E -> i
    res = h[0].node;
  else if (n == 3 && h[0].sym == P_LPAR && h[1].sym == P_E &&
           h[2].sym == P_RPAR) // E -> ( E )
    res = h[1].node;
  else if (n == 2 && h[0].sym == P_UNARY &&
           h[1].sym == P_E) // E -> - E  |  E -> ! E
    res = make_unop(h[0].op, h[1].node, h[0].line);
  else if (n == 3 && h[0].sym == P_E && h[2].sym == P_E &&
           h[1].sym >= P_MULDIV && h[1].sym <= P_OR) // E -> E op E
    res = make_binop(h[0].node, h[2].node, h[1].op, h[1].line);
  else
    error(); // handle neodpovida zadnemu pravidlu

  // odstranit znacku + handle a dat tam E
  s->top = m;
  ps_push(s, (PrecItem){P_E, 0, res, res->line});
}

// cur musi byt typu t, jinak chyba, pak posune na dalsi token
static void expect(TokenType t) {
  if (cur.type != t)
    error();
  next();
}

// preskoci prazdne konce radku
static void skip_eols(void) {
  while (cur.type == TOKEN_EOL)
    next();
}

static Node *parse_expr(void);
static Node *parse_block(void);

// parsne typy
static DataType parse_type(void) {
  DataType d;
  switch (cur.type) {
  case TOKEN_TYPE_INT:
    d = D_INT;
    break;
  case TOKEN_TYPE_DOUBLE:
    d = D_FLOAT;
    break;
  case TOKEN_TYPE_STRING:
    d = D_STRING;
    break;
  case TOKEN_UNIT:
    d = D_UNIT;
    break;
  case TOKEN_TYPE_BOOLEAN:
    d = D_BOOL;
    break;
  case TOKEN_ID: // neznamy typ, chybu 3 nahlasi semantika
    d = D_UNDEFINED;
    free(cur.stringValue);
    break;
  default:
    error();
  }
  next();
  return d;
}

// parametry pro fce
static void parse_param(Node *f) {
  if (cur.type != TOKEN_ID)
    error();
  char *name = cur.stringValue;
  next();
  expect(TOKEN_COLON);
  func_add_param(f, name, parse_type());
}

static Node *parse_istr(void);
static Node *parse_call(void);

// parsne var, nebo ()
static Node *parse_var(void) {
  Node *n;
  int line = cur.line;
  switch (cur.type) {
  case TOKEN_INT:
    n = make_num(cur.intValue, line);
    break;
  case TOKEN_DOUBLE:
    n = make_double(cur.doubleValue, line);
    break;
  case TOKEN_STRING:
    n = make_string(cur.stringValue, line);
    break;
  case TOKEN_ID:
    // cele jeden term (FUNEXP)
    if (peek().type == TOKEN_LPAR)
      return parse_call();
    n = make_var(cur.stringValue, line);
    break;
  case TOKEN_LPAR: // () = Unit
    next();
    expect(TOKEN_RPAR);
    return make_unit(line);
  case TOKEN_TRUE:
    n = make_bool(1, line);
    break;
  case TOKEN_FALSE:
    n = make_bool(0, line);
    break;
  case TOKEN_ISTR_START: // INTERPOLATEDS
    return parse_istr();
  default:
    error();
  }
  next();
  return n;
}

// precedencni analyza vyrazu nad zasobnikem podle prec_table, vraci koren
// podstromu vyrazu
static Node *parse_expr(void) {
  PrecStack s = {0}; // prazdny zasobnik (items=NULL, top=0, cap=0)
  ps_push(&s, (PrecItem){P_END, 0, NULL, cur.line}); // $ na dno
  int prev_operand = 0; // na zacatku nebyl zadny operand
  int depth = 0;        // pocet otevrenych '(' v tomto vyrazu (funexp)

  while (1) {
    PrecSym a = token_to_prec(prev_operand); // co prichazi

    // funkce, ktere tenhle vyraz (argument) obsahuje -> konec vyrazu
    if (a == P_RPAR && depth == 0)
      a = P_END;

    int t = ps_top_term(&s);    // kde je nejvyssi terminal
    PrecSym b = s.items[t].sym; // co to je

    if (a == P_END && b == P_END)
      break; // vstup skoncil a zasobnik je zredukovany -> hotovo

    switch (prec_table[b][a]) {
    case L: { // '<' : vlozit znacku hned za nejvyssi terminal...
      ps_push(&s, (PrecItem){0}); // udelat misto navic
      for (int i = s.top - 1; i > t + 1; i--)
        s.items[i] = s.items[i - 1]; // posunout E (pokud tam je) o 1 nahoru
      s.items[t + 1] = (PrecItem){P_MARK, 0, NULL, 0};
    }
      // fall through - po vlozeni znacky normalne shift (proto tam neni break,
      // ale myslim ze to bude hlasit warning)
    case Q: { // '=' : shift bez znacky
      if (a == P_LPAR)
        depth++; // funexp
      else if (a == P_RPAR)
        depth--; // funexp
      PrecItem it = {a, cur.type, NULL, cur.line};
      if (a == P_TERM)
        it.node = parse_var(); // vytvori uzel a posune na dalsi token
      else
        next();
      ps_push(&s, it);
      prev_operand = (a == P_TERM || a == P_RPAR);
      break; // <-- tohle ti v commitu chybi, bez nej to propadne do reduce
    }
    case R: // '>' : redukce
      reduce(&s);
      break;
    default: // X : chyba
      error();
      break;
    }
  }

  // na zasobniku musi zustat presne: $ E
  if (s.top != 2 || s.items[1].sym != P_E)
    error();
  Node *res = s.items[1].node;
  free(s.items);
  return res;
}

// s"text $id text ${vyraz}"
// tokeny: ISTR_START (STRING | ID | ISTR_EXPR_START vyraz ISTR_EXPR_END)*
// ISTR_END
static Node *parse_istr(void) {
  Node *s = make_istr(cur.line);
  expect(TOKEN_ISTR_START);
  while (cur.type != TOKEN_ISTR_END) {
    switch (cur.type) {
    case TOKEN_STRING: // obycejny text
      istr_add(s, make_string(cur.stringValue, cur.line));
      next();
      break;
    case TOKEN_ID: // $id
      istr_add(s, make_var(cur.stringValue, cur.line));
      next();
      break;
    case TOKEN_ISTR_EXPR_START: // ${vyraz}
      next();
      istr_add(s, parse_expr());
      expect(TOKEN_ISTR_EXPR_END);
      break;
    default:
      error();
    }
  }
  next(); // ISTR_END
  return s;
}

// volani funkce: id ( args ), argumenty jsou jen termy, koncova carka jen pred
// koncem radku
static Node *parse_call(void) {
  Node *c = make_call(cur.stringValue, cur.line);
  next();
  expect(TOKEN_LPAR);
  if (cur.type != TOKEN_RPAR) {
    call_add_arg(c, parse_expr());
    while (cur.type == TOKEN_COMMA) {
      int comma_line = cur.line;
      next();
      if (cur.type == TOKEN_RPAR) { // koncova carka
        if (cur.line == comma_line)
          error(); // mezi carkou a ) musi byt konec radku
        break;
      }
      call_add_arg(c, parse_expr());
    }
  }
  expect(TOKEN_RPAR);
  return c;
}

// rozhodne podle id + '(' (peek), jestli jde o volani nebo vyraz (tady se to
// vlastne kvuli funexp zmenilo jenom na parse_expr)
static Node *parse_expr_or_call(void) { return parse_expr(); }

// jeden prikaz: val/var, return, if, while, prirazeni nebo vyraz/volani
static Node *parse_stmt(void) {
  int line = cur.line;
  switch (cur.type) {
  case TOKEN_VAL:
  case TOKEN_VAR: {
    int is_mutable = (cur.type == TOKEN_VAR);
    next();
    if (cur.type != TOKEN_ID)
      error();
    char *name = cur.stringValue;
    next();
    DataType declared = D_UNKNOWN;
    if (cur.type == TOKEN_COLON) {
      next();
      declared = parse_type();
    }
    expect(TOKEN_ASSIGN);
    return make_vardef(name, is_mutable, declared, parse_expr_or_call(), line);
  }
  case TOKEN_RETURN: { // return
    next();
    Node *value = NULL;
    if (cur.type != TOKEN_EOL && cur.type != TOKEN_SEMICOLON &&
        cur.type != TOKEN_DEDENT && cur.type != TOKEN_EOF)
      value = parse_expr_or_call();
    return make_return(value, line);
  }
  case TOKEN_IF: { // if cond then EOL blok
    next();
    Node *cond = parse_expr();
    expect(TOKEN_THEN);
    expect(TOKEN_EOL);
    Node *then_b = parse_block();
    Node *else_b = NULL; // BOOL else je nepovinny
    if (cur.type == TOKEN_ELSE) {
      next();
      if (cur.type == TOKEN_IF) { // else if - vnoreny if zabaleny do bloku
        else_b = new_node(N_BLOCK, cur.line);
        block_add(else_b, parse_stmt());
      } else {
        expect(TOKEN_EOL);
        else_b = parse_block();
      }
    }
    return make_if(cond, then_b, else_b, line);
  }
  case TOKEN_WHILE: { // while cond do EOL blok
    next();
    Node *cond = parse_expr();
    expect(TOKEN_DO);
    expect(TOKEN_EOL);
    return make_while(cond, parse_block(), line);
  }
  case TOKEN_ID:
    if (peek().type == TOKEN_ASSIGN) { // id =
      char *name = cur.stringValue;
      next(); // id
      next(); // =
      return make_assign(name, parse_expr_or_call(), line);
    }
    return parse_expr_or_call(); // volani nebo vyraz
  default:
    return parse_expr(); // samotny vyraz, napr. ()
  }
}

// blok prikazu mezi INDENT a DEDENT, prikazy oddelene EOL nebo ';'
static Node *parse_block(void) {
  Node *blk = new_node(N_BLOCK, cur.line);
  expect(TOKEN_INDENT);
  while (1) {
    Node *s = parse_stmt();
    block_add(blk, s);
    if (s->kind == N_IF || s->kind == N_WHILE) {
      // vnoreny blok uz snedl EOL i DEDENT, oddelovac tu neni
    } else if (cur.type == TOKEN_EOL || cur.type == TOKEN_SEMICOLON)
      next();
    else if (cur.type != TOKEN_DEDENT)
      error(); // dva prikazy bez oddelovace
    if (cur.type == TOKEN_DEDENT) {
      next();
      return blk;
    }
  }
}

/// telo funkce: bud blok na dalsim radku, nebo jeden vyraz/volani na stejnem
/// radku
static Node *parse_body(void) {
  if (cur.type == TOKEN_EOL) {
    next();
    return parse_block();
  }
  Node *n = parse_expr_or_call();
  if (cur.type != TOKEN_EOL && cur.type != TOKEN_EOF)
    error();
  return n;
}

// funkce i main funkce
static Node *parse_func(void) {
  int is_main = 0;
  if (cur.type == TOKEN_AT_MAIN) { // @main muze byt i na samotnem radu
    is_main = 1;
    next();
    skip_eols();
  }

  int line = cur.line;
  expect(TOKEN_DEF);
  if (cur.type != TOKEN_ID)
    error();
  Node *f = make_func(cur.stringValue, is_main, line);
  next();

  expect(TOKEN_LPAR);
  if (cur.type != TOKEN_RPAR) { // aspon jeden parametr
    parse_param(f);
    while (cur.type == TOKEN_COMMA) {
      int comma_line = cur.line;
      next();
      if (cur.type == TOKEN_RPAR) { // koncova carka
        if (cur.line == comma_line)
          error(); // mezi carkou a ) musi byt konec radku
        break;
      }
      parse_param(f);
    }
  }
  expect(TOKEN_RPAR);

  if (cur.type == TOKEN_COLON) { // return type je nepovinny
    next();
    f->as.func.ret = parse_type();
  }
  expect(TOKEN_ASSIGN);
  f->as.func.body = parse_body();
  return f;
}

// hlavicka souboru a prida funkce do arraye programu
Node *parse_program(void) {
  next(); // nacti prvni token
  skip_eols();
  Node *prog = make_program(cur.line);

  // import ifj26.*
  expect(TOKEN_IMPORT);
  if (cur.type != TOKEN_ID || strcmp(cur.stringValue, "ifj26") != 0)
    error();
  free(cur.stringValue); // "ifj26" uz nepotrebujeme
  next();
  expect(TOKEN_DOT);
  expect(TOKEN_MUL);
  skip_eols();

  while (cur.type != TOKEN_EOF) {
    program_add(prog, parse_func());
    skip_eols();
  }
  return prog;
}

#undef L
#undef R
#undef Q
#undef X
