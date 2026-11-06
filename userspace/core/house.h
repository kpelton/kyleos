#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
struct itemi{
char name[20];
char desc[20];
};

typedef struct itemi item;
struct room{

char desc[1000];
struct room *west;
struct room *east;
struct room *north;
struct room *south;
item * items[50];
char name[50];
int locked;
struct monster *monsters;

};




typedef struct room room;


struct monster {


char name[20];
char * dialog[5]; // array of pointers for random sayings;
int hp;
int power;

};


typedef struct monster mob;

struct people{

char name[50];
item * inventory[50];
int hp;
int power;

};
typedef struct people person;
void move(room * beg,person *);
void roomdisc (room * head);
void inventory (person * me);
void icheck(int item);
void get(person * me, room * head,char *);
int iname(char * arg);
int checkroom(room * head,char dir);
void use(person * me, room * head,char * arg);
void killm(room * head,person * me);
void drop(person * me, room * head, char * arg);
void quit();
void where(room *head);
