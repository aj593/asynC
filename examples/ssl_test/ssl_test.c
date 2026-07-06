#include "../../asynC/asynC.h"

#include <stdio.h>

int main(int argc, char** argv){

    async_ssl_connect(
        NULL,
        0,
        NULL,
        NULL,
        NULL
    );

    return 0;
}