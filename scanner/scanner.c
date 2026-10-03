#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "token.h"

//čtení vstup s ukládáním počtu mezer
static FILE *src;
static int line = 1, col = 0, prevCol = 0;

typedef enum { // podobně jako v token.h - jedná se o jakousi kolekci VŠECH stavů, do které se může konečný automat dostat
	    	   //-> místo tedy několik vnořených funkcí bude potom jen jeden velký switch, kde v podstatě jednotlivé stavy budou definovány

	S_START,
	S_INT0,
	S_INT,
	S_INT__,
	S_EOF,
	S_DOUBLE_DOT,
	S_ID,
	S_AT,
	S_UNS,
	S_QFIRST,
	S_QSEC,
	S_COMMS,
	S_ASSIGN,
	S_EXCLAM,
	S_L_THAN,
	S_G_THAN,
	S_INT_DOT,
	S_EXP,


}State;

static int line = 1;

int get_token(Token *t){

	State state = S_START; //první case jde vždy do S_Start
	long long value = 0;

	while(1){

	int c = getchar();

	switch (state){ //Jeden velký switch pro všechny stavy z typedef enum

		case S_START: // opsání tabulky přechodů stavů ze Startu
			t->line = line;                     // token začíná na aktuálním řádku

			if (c == '\n') {
				line++;                        
			}
			else if (c == ' ' || c == '\t' || c == '\r') {
				// bílý znak přeskočit, zůstat v S_START
			}
			else if (c == EOF) {
				t->type = TOKEN_EOF;
				return 0;
			}

			// INT
			else if (c >= '1' && c <= '9') {
				value = c - '0';
				state = S_INT;
			}
			else if (c == '0') state = S_INT0;

			//DOUBLE 
			else if (c == '.') state = S_DOUBLE_DOT;

			//Identifikátory + klíčová slova
			else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) state = S_ID;
			else if (c == '@') state = S_AT;
			else if (c == '_') state = S_UNS;

			//řetězce
			else if (c == '"') state = S_QFIRST;

			//komentáře (a dělení)
			else if (c == '/') state = S_COMMS;

			//operátory, po kterých může přijít druhý znakelk
			else if (c == '=') state = S_ASSIGN;
			else if (c == '!') state = S_EXCLAM;
			else if (c == '<') state = S_L_THAN;
			else if (c == '>') state = S_G_THAN;

			//koncový stav bez dalších hran, token je hned hotový
			else if (c == '+'){
				t->type = TOKEN_PLUS;      
				return 0; 
			}

			else if (c == '-'){
				t->type = TOKEN_MINUS;    
				return 0; 
			}

			else if (c == '*'){
				t->type = TOKEN_MUL;      
				return 0; 
			}

			else if (c == ','){
				t->type = TOKEN_COMMA;    
				return 0; 
			}

			else if (c == '('){
				t->type = TOKEN_LPAR;      
				return 0; 
			}

			else if (c == ')'){
				t->type = TOKEN_RPAR;      
				return 0; 
			}

			else if (c == ':'){
				t->type = TOKEN_COLON;     
				return 0;
			}

			else if (c == ';'){
				t->type = TOKEN_SEMICOLON; 
				return 0; 
			}
			//neznámý znak
			else return 1;
		break;

		case S_INT0:

			if (c == '.'){
				t->type = S_INT_DOT;
				return 0;
			}

			else if ((c == 'e') || c == 'E'){
				t->type = S_EXP;
				return 0;
			}

			else if (((c >= '0') && c <= '9') || c == '_'){
				return 1;
			}

			else return 1; //správně?

		break;


	}




}

#ifdef SCANNER_TEST

/* prompt - přečti si ssoubor scanner.c a token.h a vytvoř jednoduchý test zatím dosažitelných funkcí
 * Jen pro vyzkoušení: main test od chatgpt:
 *   gcc -DSCANNER_TEST scanner.c -o scanner
 *   printf '42 0\n1_000' | ./scanner
 */

int main(void)
{
    Token t;
 
    while (1) {
        if (get_token(&t) != 0) {
            printf("lexikální chyba na řádku %d\n", line);
            return 1;
        }
        if (t.type == TOKEN_EOF) {
            printf("řádek %d: EOF\n", t.line);
            return 0;
        }
        printf("řádek %d: INT %lld\n", t.line, t.int_value);
    }
}
#endif
