#include "token.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// static FILE *src;
// static int line = 1, col = 0, prevCol = 0;
static int line = 1;

// MUSI SI PAMATOVAT SVOJI HODNOTU PRES NEKOLIK STAVU AUTOMATU, PROTO SEW JEDNA
// O STATICKE PROMENNE, respek. mezi volanimi jinymi tokeny.
#define INDENT_MAX 256
static int line_start = 1; // zacatek radku
static int indent = 0;     // pocet mezer na aktualnim radku
static int tab_at_start =
    0;                // pocet tabs na zacatku radku / musim implementovat?
static int paren = 0; // pocet ( na radku
static int indent_stack[INDENT_MAX] = {0}; // odsazeni
static int indent_top = 0;     // vrchol indentu / chova se jako lifo fronta
static int pending_dedent = 0; // kolik )) jeste zbyva vratit
static int last_eol = 1;
// pomocne pro rozsireni INTERPOLATEDS
static int istr = 0;
static int istr_expr = 0;
static int istr_pending_end = 0;

typedef enum { // podobně jako v token.h - jedná se o jakousi kolekci VŠECH
               // stavů, do které se může konečný automat dostat
               //-> místo tedy několik vnořených funkcí bude potom jen jeden
               // velký switch, kde v podstatě jednotlivé stavy budou definovány

  S_START, /*Začátek každého slova - rozřazení do jednoho z pěti základních
              "stromů"*/
  S_INT0,       /*Číslo začínající 0 -> může to být double / chbya*/
  S_INT,        /*Koncový stav pro INT*/
  S_INT__,      /* Z int -> double*/
  S_EOF,        /* koncový stav pro End of line*/
  S_DOUBLE_DOT, /* Jeden z koncových stavl pro double*/
  S_ID,         /**/
  S_AT,         /**/
  S_UNS,        /**/
  S_QFIRST,     /**/
  S_QSEC,       /**/
  S_COMMS,      /**/
  S_ASSIGN,     /**/
  S_EXCLAM,     /**/
  S_L_THAN,     /**/
  S_G_THAN,     /**/
  S_INT_DOT,    /**/
  S_EXP,        /**/
  S_FRAC,       /**/
  S_SUFFIX,     /**/
  S_FRAC_,      /**/
  S_EXPD,       /**/
  S_EXP_SIGN,   /**/
  S_EXPD_,      /**/
  S_ML,         /**/
  S_ML_Q1,      /**/
  S_ML_Q2,      /**/
  S_ML_END,     /**/
  S_LC,         /**/
  S_BC,         /**/
  S_BL_S,       /**/
  S_BL_SLASH,   /**/
  S_BL_END,     /**/
  S_ESC,
  S_STR,
  S_STR_END,
  S_ISTR_OPEN,
  S_ISTR_OPEN2,
  S_ISTR_TEXT,
  S_ISTR_DOLLAR,
  S_ISTR_ID,

} State;

static TokenType keyword(const char *s) {
  if (strcmp(s, "def") == 0)
    return TOKEN_DEF;
  if (strcmp(s, "do") == 0)
    return TOKEN_DO;
  if (strcmp(s, "Double") == 0)
    return TOKEN_TYPE_DOUBLE;
  if (strcmp(s, "else") == 0)
    return TOKEN_ELSE;
  if (strcmp(s, "if") == 0)
    return TOKEN_IF;
  if (strcmp(s, "import") == 0)
    return TOKEN_IMPORT;
  if (strcmp(s, "Int") == 0)
    return TOKEN_TYPE_INT;
  if (strcmp(s, "return") == 0)
    return TOKEN_RETURN;
  if (strcmp(s, "String") == 0)
    return TOKEN_TYPE_STRING;
  if (strcmp(s, "then") == 0)
    return TOKEN_THEN;
  if (strcmp(s, "Unit") == 0)
    return TOKEN_UNIT;
  if (strcmp(s, "val") == 0)
    return TOKEN_VAL;
  if (strcmp(s, "var") == 0)
    return TOKEN_VAR;
  if (strcmp(s, "while") == 0)
    return TOKEN_WHILE;
  if (strcmp(s, "Boolean") == 0)
    return TOKEN_RESERVED;
  if (strcmp(s, "case") == 0)
    return TOKEN_RESERVED;
  if (strcmp(s, "class") == 0)
    return TOKEN_RESERVED;
  if (strcmp(s, "extension") == 0)
    return TOKEN_RESERVED;
  if (strcmp(s, "for") == 0)
    return TOKEN_RESERVED;
  if (strcmp(s, "List") == 0)
    return TOKEN_RESERVED;
  if (strcmp(s, "match") == 0)
    return TOKEN_RESERVED;
  if (strcmp(s, "type") == 0)
    return TOKEN_RESERVED;
  return TOKEN_ID;
}

int get_token(Token *t) {

  State state = S_START; // první case jde vždy do S_Start
  int c;
  char buf[64];
  int len = 0;
  int depth = 0;
  int comment_nl = 0;
  *t = (Token){0};

  if (pending_dedent > 0) {
    pending_dedent--;
    t->type = TOKEN_DEDENT;
    t->line = line;
    return 0;
  }
  if (istr_pending_end) { // rozsireni pro INTERPOLADES
    istr_pending_end = 0;
    t->type = TOKEN_ISTR_END;
    t->line = line;
    return 0;
  }

  if (istr != 0 && !istr_expr) {
    t->line = line;
    state = S_ISTR_TEXT;
  }

  while (1) {

    if (len >= 63)
      return 1; // buffer plný
    c = getchar();
    // t->type + return 0 = konec switch, přiřazení slova k typu ->konec
    // state = x = přesunutí do dalšího stavu
    // buf[len++] = Zde se ukládá text
    //

    switch (state) { // Jeden velký switch pro všechny stavy z typedef enum

    case S_START:     // opsání tabulky přechodů stavů ze Startu
      t->line = line; // token začíná na aktuálním řádku

      if (c == '\n') {
        line++;
        if (paren == 0) {
          line_start = 1;
          indent = 0;
          tab_at_start = 0;
          if (!last_eol) {
            last_eol = 1;
            t->type = TOKEN_EOL;
            return 0;
          }
        }
      } else if (c == ' ') {
        if (line_start)
          indent++;
      } else if (c == '\t') {
        if (line_start)
          tab_at_start = 1;
      } else if (c == '\r') {
        // bílý znak přeskočit, zůstat v S_START
      } else if (c == EOF) {
        if (!last_eol) {
          last_eol = 1;
          t->type = TOKEN_EOL;
          return 0;
        }
        if (indent_top > 0) {
          indent_top--;
          t->type = TOKEN_DEDENT;
          return 0;
        }
        t->type = TOKEN_EOF;
        return 0;
      }
      // Fable 5.2 / "s pomoci kontextu
      // celeho projektu mi vysvetli a pomoz, jak mam spravne implementovat
      // zasobnik zavorek tak, aby se vzdy ohlidal spravny pocetvnoreni, na
      // jakem jsem stupni v nejakem bloku atd. nejprve mi vse vysvetli, pak
      // nacrtni jak ma fungovat cela funkce.
      else if (line_start && c != '/') {
        line_start = 0;
        last_eol = 0;
        if (tab_at_start)
          return 1;
        ungetc(c, stdin);
        if (indent > indent_stack[indent_top]) {
          if (indent_top + 1 >= INDENT_MAX)
            return 99;
          indent_stack[++indent_top] = indent;
          t->type = TOKEN_INDENT;
          return 0;
        }
        if (indent < indent_stack[indent_top]) {
          while (indent < indent_stack[indent_top]) {
            indent_top--;
            pending_dedent++;
          }
          if (indent != indent_stack[indent_top])
            return 2;
          pending_dedent--;
          t->type = TOKEN_DEDENT;
          return 0;
        }
      }

      // INT
      else if (c >= '1' && c <= '9') {
        buf[len++] = c;
        state = S_INT;
      } else if (c == '0') {
        buf[len++] = c;
        state = S_INT0;
      }

      // DOUBLE
      else if (c == '.') {
        buf[len++] = c;
        state = S_DOUBLE_DOT;
      }

      // Identifikátory + klíčová slova
      else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        buf[len++] = c;
        state = S_ID;
      } else if (c == '@') {
        buf[len++] = c;
        state = S_AT;
      } else if (c == '_') {
        buf[len++] = c;
        state = S_UNS;
      }
      // řetězce
      else if (c == '"') {
        state = S_QFIRST;
      }

      // komentáře (a dělení)
      else if (c == '/') {
        buf[len++] = c;
        state = S_COMMS;
      }

      // operátory, po kterých může přijít druhý znakelk
      else if (c == '=') {
        buf[len++] = c;
        state = S_ASSIGN;
      } else if (c == '!') {
        buf[len++] = c;
        state = S_EXCLAM;
      } else if (c == '<') {
        buf[len++] = c;
        state = S_L_THAN;
      } else if (c == '>') {
        buf[len++] = c;
        state = S_G_THAN;
      }

      // koncový stav bez dalších hran, token je hned hotový
      else if (c == '+') {
        t->type = TOKEN_PLUS;
        return 0;
      }

      else if (c == '-') {
        t->type = TOKEN_MINUS;
        return 0;
      }

      else if (c == '*') {
        t->type = TOKEN_MUL;
        return 0;
      }

      else if (c == ',') {
        t->type = TOKEN_COMMA;
        return 0;
      }

      else if (c == '(') {
        paren++;
        t->type = TOKEN_LPAR;
        return 0;
      }

      else if (c == ')') {
        if (paren > 0)
          paren--;
        t->type = TOKEN_RPAR;
        return 0;
      }

      else if (c == ':') {
        t->type = TOKEN_COLON;
        return 0;
      }

      else if (c == ';') {
        t->type = TOKEN_SEMICOLON;
        return 0;
      } else if (c == '}' && istr_expr) {
        istr_expr = 0;
        t->type = TOKEN_ISTR_EXPR_END;
        return 0;
      }
      // neznámý znak
      else
        return 1;
      break;

    // int
    case S_INT0:

      if (c == '.') {
        buf[len++] = c;
        state = S_INT_DOT;
      }

      else if ((c == 'e') || c == 'E') {
        buf[len++] = c;
        state = S_EXP;
      }

      else if (((c >= '0') && c <= '9') || c == '_') {
        return 1;
      }

      else { // uvažuji, že už jde o jiné slovo, musím se tedy posunout zpět
        ungetc(c, stdin);
        t->type = TOKEN_INT;
        t->intValue = 0;
        return 0;
      }

      break;

    case S_INT:

      if (c >= '0' && c <= '9') {
        buf[len++] = c; // stav se nemění, jen se posune buffer;
      } else if (c == 'e' || c == 'E') {
        buf[len++] = c;
        state = S_EXP;
      } else if (c == '_') {
        state = S_INT__;
      } else if (c == '.') {
        buf[len++] = c;
        state = S_INT_DOT;
      } else { // INT je koncový, token končí
        ungetc(c, stdin);
        buf[len] = '\0';
        t->type = TOKEN_INT;
        t->intValue = strtoll(buf, NULL, 10);
        return 0;
      }

      break;

    case S_INT__:
      if (c >= '0' && c <= '9') {
        buf[len++] = c;
        state = S_INT;
      } else
        return 1;
      break;

    // double
    case S_INT_DOT:

      if (c >= '0' && c <= '9') {
        buf[len++] = c;
        state = S_FRAC;
      } else
        return 1;
      break;

    case S_FRAC:

      if (c >= '0' && c <= '9') {
        buf[len++] = c;
      } else if ((c == 'e') || c == 'E') {
        buf[len++] = c;
        state = S_EXP;
      } else if ((c == 'd') || (c == 'D')) {
        buf[len] = '\0';
        t->type = TOKEN_DOUBLE;
        t->doubleValue = strtod(buf, NULL);
        return 0;
      } else if (c == '_') {
        state = S_FRAC_;
      } else {
        ungetc(c, stdin);
        buf[len] = '\0';
        t->type = TOKEN_DOUBLE;
        t->doubleValue = strtod(buf, NULL);
        return 0;
      }
      break;

    case S_FRAC_:

      if (c >= '0' && c <= '9') {
        buf[len++] = c;
        state = S_FRAC;
      } else
        return 1;
      break;

    case S_DOUBLE_DOT:
      if (c >= '0' && c <= '9') {
        buf[len++] = c;
        state = S_FRAC;
      } else {
        ungetc(c, stdin);
        t->type = TOKEN_DOT;
        return 0;
      }
      break;

    case S_ID:
      if ((c >= '0' && c <= '9') ||
          ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) || c == '_') {
        buf[len++] = c;
      } else if (c == '"' && len == 1 && buf[0] == 's') {
        len = 0;
        state = S_ISTR_OPEN;
      } else {
        ungetc(c, stdin);          // o char zpět
        buf[len] = '\0';           // ukončení slova
        t->type = keyword(buf);    // TOKEN_WHILE, TOKEN_IF, … nebo TOKEN_ID
        if (t->type == TOKEN_ID) { // stačí se opět ukládat jen název tokenu
          t->stringValue =
              malloc(len + 1); // potřebuji si držet název identifikátoru někde
                               // jinde než ve statickém bufferu, jinak bych o
                               // název přišel - > +1 kvůli "\n"
          if (t->stringValue == NULL)
            return 99; // malloc by pak měl uvolňovat až parser po použití
          strcpy(t->stringValue, buf); // vlastní kopie názvu ID do bufferu
        }
        return 0;
      }
      break;

    case S_AT:
      if ((c >= '0' && c <= '9') ||
          ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) || c == '_') {
        buf[len++] = c;
      } else {
        ungetc(c, stdin);
        buf[len] = '\0';
        if (strcmp(buf, "@main") != 0)
          return 1;
        t->type = TOKEN_AT_MAIN;
        return 0;
      }
      break;

    case S_UNS:
      if ((c >= '0' && c <= '9') ||
          ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) || c == '_') {
        buf[len++] = c;
        state = S_ID;
      } else
        return 1;
      break;

    case S_EXP:
      if (c == '+' || c == '-') {
        buf[len++] = c;
        state = S_EXP_SIGN;
      } else if (c >= '0' && c <= '9') {
        buf[len++] = c;
        state = S_EXPD;
      } else
        return 1;
      break;

    case S_EXPD:
      if (c >= '0' && c <= '9') {
        buf[len++] = c;
      } else if (c == 'd' || c == 'D') {
        buf[len] = '\0';
        t->type = TOKEN_DOUBLE;
        t->doubleValue = strtod(buf, NULL);
        return 0;
      } else if (c == '_') {
        state = S_EXPD_;
      } else {
        ungetc(c, stdin);
        buf[len] = '\0';
        t->type = TOKEN_DOUBLE;
        t->doubleValue = strtod(buf, NULL);
        return 0;
      }
      break;

    case S_SUFFIX:
      printf("není ještě hotovo");
      return 1;

    case S_EXP_SIGN:
      if (c >= '0' && c <= '9') {
        buf[len++] = c;
        state = S_EXPD;
      } else
        return 1;
      break;

    case S_EXPD_:
      if (c >= '0' && c <= '9') {
        buf[len++] = c;
        state = S_EXPD;
      } else
        return 1;
      break;

    case S_QFIRST:
      if (c != '"' && c != '\\' && c != '\n' && c != EOF) {
        buf[len++] = c;
        state = S_STR;
      } else if (c == '"') {
        state = S_QSEC;
      }
      /*else if(c == "\"){ //jak napsat se rovná \?

      }*/
      else if (c == '\\') {
        state = S_ESC;
      } else
        return 1;
      break;

    case S_QSEC:
      if (c == '"') {
        state = S_ML;
      } else {
        ungetc(c, stdin);
        buf[len] = '\0';
        t->type = TOKEN_STRING;
        t->stringValue = malloc(len + 1);
        if (t->stringValue == NULL)
          return 99;
        strcpy(t->stringValue, buf);
        return 0;
      }
      break;

    case S_ESC:
      if (c == 'n')
        buf[len++] = '\n';
      else if (c == 'r')
        buf[len++] = '\r';
      else if (c == 't')
        buf[len++] = '\t';
      else if (c == '\\')
        buf[len++] = '\\';
      else if (c == '"')
        buf[len++] = '"';
      else
        return 1;
      state = (istr && !istr_expr) ? S_ISTR_TEXT : S_STR;
      break;

    case S_STR:
      if (c == '"') {
        buf[len] = '\0';
        t->type = TOKEN_STRING;
        t->stringValue = malloc(len + 1);
        if (t->stringValue == NULL)
          return 99;
        strcpy(t->stringValue, buf);
        return 0;
      } else if (c == '\\') {
        state = S_ESC;
      } else if (c != '\n' && c != EOF) {
        buf[len++] = c;
      } else
        return 1;
      break;

    case S_STR_END:
      printf("není ještě hotovo");
      return 1;

    case S_ML:
      if (c == '"') {
        state = S_ML_Q1;
      } else if (c == EOF)
        return 1;
      else {
        if (c == '\n')
          line++;
        buf[len++] = c;
      }
      break;

    case S_ML_Q1:
      if (c == '"') {
        state = S_ML_Q2;
      } else if (c == EOF)
        return 1;
      else {
        ungetc(c, stdin);
        buf[len++] = '"';
        state = (istr && !istr_expr) ? S_ISTR_TEXT : S_ML;
      }
      break;

    case S_ML_Q2:
      if (c == '"') {
        state = S_ML_END;
      } else if (c == EOF)
        return 1;
      else {
        ungetc(c, stdin);
        buf[len++] = '"';
        state = S_ML_Q1;
      }
      break;

    case S_ML_END:
      if (c == '"') {
        buf[len++] = c;
      } else if (istr && !istr_expr) {
        ungetc(c, stdin);
        istr = 0;
        if (len == 0) {
          t->type = TOKEN_ISTR_END;
          return 0;
        }
        istr_pending_end = 1;
        buf[len] = '\0';
        t->type = TOKEN_STRING;
        t->stringValue = malloc(len + 1);
        if (t->stringValue == NULL)
          return 99;
        strcpy(t->stringValue, buf);
        return 0;
      } else {
        ungetc(c, stdin);
        buf[len] = '\0';
        t->type = TOKEN_STRING;
        t->stringValue = malloc(len + 1);
        if (t->stringValue == NULL)
          return 99;
        strcpy(t->stringValue, buf);
        return 0;
      }
      break;

    case S_COMMS:
      if (c == '/') {
        state = S_LC;
      } else if (c == '*') {
        depth = 1;
        comment_nl = 0;
        state = S_BC;
      } else {
        ungetc(c, stdin);
        line_start = 0;
        last_eol = 0;
        t->type = TOKEN_DIV;
        return 0;
      }
      break;

    case S_LC:
      if (c == '\n' || c == EOF) {
        ungetc(c, stdin);
        len = 0;
        state = S_START;
      }
      break;

    case S_BC:
      if (c == EOF)
        return 1;
      else if (c == '*')
        state = S_BL_S;
      else if (c == '/')
        state = S_BL_SLASH;
      else if (c == '\n') {
        line++;
        comment_nl = 1;
      }
      break;

    case S_BL_S:
      if (c == EOF)
        return 1;
      else if (c == '/') {
        depth--;
        if (depth == 0)
          state = S_BL_END;
        else
          state = S_BC;
      } else if (c != '*') {
        if (c == '\n') {
          line++;
          comment_nl = 1;
        }
        state = S_BC;
      }
      break;

    case S_BL_END:
      if (c == ' ' || c == '\t' || c == '\r') {
      } else if (c == '\n' || c == EOF || !comment_nl) {
        ungetc(c, stdin);
        len = 0;
        state = S_START;
      } else
        return 1;
      break;

    case S_BL_SLASH:
      if (c == EOF)
        return 1;
      else if (c == '*') {
        depth++;
        state = S_BC;
      } else if (c != '/') {
        if (c == '\n') {
          line++;
          comment_nl = 1;
        }
        state = S_BC;
      }
      break;

    case S_ASSIGN:
      if (c == '=') {
        t->type = TOKEN_EQ;
        return 0;
      } else {
        ungetc(c, stdin);
        t->type = TOKEN_ASSIGN;
        return 0;
      }
      break;

    case S_EXCLAM:
      if (c == '=') {
        t->type = TOKEN_NEQ;
        return 0;
      } else
        return 1;
      break;

    case S_L_THAN:
      if (c == '=') {
        t->type = TOKEN_LE;
        return 0;
      } else {
        ungetc(c, stdin);
        t->type = TOKEN_LT;
        return 0;
      }
      break;

    case S_G_THAN:
      if (c == '=') {
        t->type = TOKEN_GE;
        return 0;
      } else {
        ungetc(c, stdin);
        t->type = TOKEN_GT;
        return 0;
      }
      break;
    case S_ISTR_OPEN:
      if (c == '"') {
        state = S_ISTR_OPEN2;
      } else {
        ungetc(c, stdin);
        istr = 1;
        t->type = TOKEN_ISTR_START;
        return 0;
      }
      break;

    case S_ISTR_OPEN2:
      if (c == '"') {
        istr = 2;
      } else {
        ungetc(c, stdin);
        istr_pending_end = 1;
      }
      t->type = TOKEN_ISTR_START;
      return 0;

    case S_ISTR_TEXT:
      if (c == '$') {
        if (len > 0) {
          ungetc(c, stdin);
          buf[len] = '\0';
          t->type = TOKEN_STRING;
          t->stringValue = malloc(len + 1);
          if (t->stringValue == NULL)
            return 99;
          strcpy(t->stringValue, buf);
          return 0;
        }
        state = S_ISTR_DOLLAR;
      } else if (c == '"' && istr == 2) {
        state = S_ML_Q1;
      } else if (c == '"') {
        istr = 0;
        if (len == 0) {
          t->type = TOKEN_ISTR_END;
          return 0;
        }
        istr_pending_end = 1;
        buf[len] = '\0';
        t->type = TOKEN_STRING;
        t->stringValue = malloc(len + 1);
        if (t->stringValue == NULL)
          return 99;
        strcpy(t->stringValue, buf);
        return 0;
      } else if (c == '\\' && istr == 1) {
        state = S_ESC;
      } else if (c == EOF || (c == '\n' && istr == 1)) {
        return 1;
      } else {
        if (c == '\n')
          line++;
        buf[len++] = c;
      }
      break;

    case S_ISTR_DOLLAR:
      if (c == '$') {
        buf[len++] = c;
        state = S_ISTR_TEXT;
      } else if (c == '{') {
        istr_expr = 1;
        t->type = TOKEN_ISTR_EXPR_START;
        return 0;
      } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        buf[len++] = c;
        state = S_ISTR_ID;
      } else
        return 1;
      break;

    case S_ISTR_ID:
      if ((c >= '0' && c <= '9') ||
          ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) || c == '_') {
        buf[len++] = c;
      } else {
        ungetc(c, stdin);
        buf[len] = '\0';
        if (keyword(buf) != TOKEN_ID || strcmp(buf, "_") == 0)
          return 1;
        t->type = TOKEN_ID;
        t->stringValue = malloc(len + 1);
        if (t->stringValue == NULL)
          return 99;
        strcpy(t->stringValue, buf);
        return 0;
      }
      break;

    default:
      return 1;
    }
  }
}
