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

}State;

static int line = 1;

int getToken(Token *tokenT){

	State state = S_START;
	long long value = 0;

	while(1){

		int c = getchar();

		switch (state){ //Jeden velký swithc pro všechny stavy z typedef enum

		case S_START:

			if (c == '\n'){
				line++;
			}
			else if{

			}
			else if{

			}
			else if{

			}
			else if{

			}
			else if{

			}
			else if{

			}
			else if{

			}
		
			default:
			break;
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