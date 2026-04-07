#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

int main()
{
	int i;
    double rand_number[32] = {0};
    unsigned short seed[3] = {155,0,155};
	int var1 = 'k';
	
    seed48(&seed[0]);

    for (i = 0; i < 32; i++){
        rand_number[i] = drand48();
        printf("%f\n", rand_number[i]);
    }
    
    if(isprint(var1))
    	printf("%c\n", var1);
    	
    return 0;
}