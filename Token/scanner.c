#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "token.h"


//static FILE *src;
//static int line = 1, col = 0, prevCol = 0;
static int line = 1;

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
	S_FRAC,
	S_SUFFIX1,
	S_FRAC_,


}State;


int get_token(Token *t){

	State state = S_START; //první case jde vždy do S_Start
	int c;
	char buf[64];
	int len = 0;
 	*t = (Token){ 0 }; 

	while(1){

		if (len >= 63) return 1;   //buffer plný
		c = getchar();
		// t->type + return 0 = konec switch, přiřazení slova k typu ->konec
		//state = x = přesunutí do dalšího stavu
		//buf[len++] = Zde se ukládá text
		//

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
					buf[len++] = c;
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


			// int
			case S_INT0:

				if (c == '.'){
					buf[len++] = c;
					state = S_INT_DOT;
				}

				else if ((c == 'e') || c == 'E'){
					buf[len++] = c;
					state = S_EXP;
				}

				else if (((c >= '0') && c <= '9') || c == '_'){
					return 1;
				}

				else{ //uvažuji, že už jde o jiné slovo, musím se tedy posunout zpět
					ungetc(c, stdin);
					t->type = TOKEN_INT;
					t->intValue = 0;
					return 0;
				}

			break;

			case S_INT:

				if (c >= '0' && c <= '9'){
					buf[len++] = c; // stav se nemění, jen se posune buffer;
				}
				else if (c == 'e' || c == 'E'){
					buf[len++] = c;
					state = S_EXP;
				}
				else if (c =='_'){
					state = S_INT__;
				}
				else if (c == '.') {
				buf[len++] = c;
				state = S_INT_DOT;
				}
				else {    //INT je koncový, token končí
					ungetc(c, stdin);
					buf[len] = '\0';
					t->type = TOKEN_INT;
					t->intValue = strtoll(buf, NULL, 10);
					return 0;
				}
				
			break;

			case S_INT__:
				if (c >= '0' && c <= '9'){
					buf[len++] = c;
					state = S_INT;
				}
				else return 1;
			break;

			//double
			case S_INT_DOT:

				if (c >= '0' && c <= '9'){
					buf[len++] = c;
					state = S_FRAC;
				}
				else return 1;
			break;

			case S_FRAC:

				if (c >= '0' && c <= '9'){
					buf[len++] = c;
				}
				else if ((c == 'e') || c == 'E'){
					buf[len++] = c;
					state = S_EXP;
				}
				else if ((c == 'd') || (c == 'D')){
					buf[len] = '\0';
					t->type = TOKEN_DOUBLE;
					t->doubleValue = strtod(buf, NULL);
					return 0;
				}
				else if (c == '_'){
					state = S_FRAC_;
				}
				else{
					ungetc(c, stdin);
					buf[len] = '\0';
					t->type = TOKEN_DOUBLE;
					t->doubleValue = strtod(buf, NULL);
					return 0;
				}
			break;

			case S_FRAC_:

				if (c >= '0' && c <= '9'){
					buf[len++] = c;
					state = S_FRAC;
				}
			else return 1;
			break;

			default: return 1;
		}
	}
}

