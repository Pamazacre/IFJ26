#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

//čtení vstup s ukládáním počtu mezer
static FILE *src;
static int line = 1, col = 0, prevCol = 0;

static int next_char(void){
	int c = getc(src);
	if(c=='\n'){
		line++;
		prevCol = col;
		col = 0;
	}
	else if(c != EOF){
		col++;
	}
	return c;
}	
