#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    int val;
    char *names[] = {
    "Emily Pelton",
    "Simone Pelton",
    "Eva Pelton",
    "Bianna Pelton",
    "Bianca Pelton",
    "Isla Pelton",
    "Vera Pelton",
    "Leila Pelton",
    "Ava Pelton"
    };
    //srand(time(NULL));
    while (1) {
    sizeof(names)/sizeof(names[0]);
    val = rand() % sizeof(names)/sizeof(names[0]);
    printf("The chosen name is: %s\n",names[val]);
    }
    return 0;
}

