#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    double* ptr = sf_malloc(4032);
    // double* ptr = sf_malloc(4000);
    
    *ptr = 4;
  
    printf("%f\n", *ptr);

    //sf_free(ptr);

    return EXIT_SUCCESS;
}
