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


}State;

static int line = 1;

int get_token(Token *tokenT){

	State state = S_START; //první case jde vždy do S_Start
	long long value = 0;

	while(1){

		int c = getchar();

		switch (state){ //Jeden velký switch pro všechny stavy z typedef enum

		case S_START: // opsání tabulky přechodů stavů ze Startu

			if (isspace(c)){
			}
			else if (c == EOF) state = TOKEN_EOF; // v tomto případě už končí


			//INT
			else if (c >= '1' && c <= '9') state = S_INT;
			else if (c == '0') state = S_INT0;
			
			//DOUBLE
			else if(c == '.') S_DOUBLE_DOT;

			//Identifikátory + klíčová slova
			else if ((c >= 'a' && c <= 'z')||(c >= 'A' && c <= 'Z')) S_ID;
			else if (c == '@') S_AT;
			else if (c == '_') S_UNS;

			//řetězce
			//else if(c == '""') S_QFIRST; //to je taky asi špatně

			//komentáře
			else if( c == '/') S_COMMS;

			//operátory
			else if( c == '=')  S_ASSIGN;
			else if( c == '!')  S_EXCLAM;
			else if( c == '<')  S_L_THAN;
			else if( c == '>')  S_G_THAN;
			else if( c == '+')  TOKEN_PLUS;
			else if( c == '-')  TOKEN_MINUS;
			else if( c == '*')  TOKEN_MUL;
			//else if( c == '') ; //jak udělat comma?
			else if( c == '(')  TOKEN_LPAR;
			else if( c == ')')  TOKEN_RPAR;
			else if( c == ':')  TOKEN_COLON;
			else if( c == ';')  TOKEN_SEMICOLON;
		}
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
