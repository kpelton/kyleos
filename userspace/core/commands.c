#include "house.h"














void roomdisc (room * head) {
int i;
printf("%s\n%s",head->name,head->desc);

if(head->monsters !=0) {

		
puts("\n\nMonsters you see:\n");
printf("%s -- HP:%i\n",head->monsters->name,head->monsters->hp);
}



puts("\nItems you see:\n");
for(i=0; i<5; i++) {
if(head->items[i] !=0){
printf("%s %s\n",head->items[i]->name,head->items[i]->desc);

		}
	}

}


void inventory (person * me) {
int i;
puts("You are carrying:\n");
for(i=0; i<50; i++){
if(me->inventory[i] != 0) {
puts(me->inventory[i]->name);

		}			
	}


}



void get(person * me, room * head,char * arg) {
int i;
int j;
int check;


for(j=0; j<49; j++){
if (head->items[j]!= 0 && arg != 0 && strcmp(head->items[j]->name,arg) == 0){

break;
}
if(j==49 || arg == 0) {

puts("\nnothing to get\n");
check = 1;
break; //break out of loop if arg = 0
	}



}

	if (check != 1)
	for(i=0; i<50; i++){

	if(me->inventory[i] == 0){			//find free space in inventory
	me->inventory[i] = head->items[j];
		break;

	}
}
head->items[j] = 0;				//take item out of room
return;

}



int checkroom(room * head,char dir){
int blah=0;
switch (dir) {

case 'n':
	if(head->north->locked == 1) {
	printf("Door Locked\n\n");
	blah =1;
		}
break;

case 's':
								//check rooms for locks
	if(head->south->locked == 1) {
	printf("Door Locked\n\n");
	blah =1;
	}
break;

case 'e':

	if(head->east->locked == 1) {
	printf("Door Locked\n\n");
	blah =1;
	}
break;

case 'w':

	if(head->west->locked == 1) {
	printf("Door Locked\n\n");
	blah =1;
	}
break;

	}
return blah;

}





void use(person * me, room * head,char * arg) {
int i;
int check=0;

if (arg == 0)
return;


for (i=0; i<6; i++) {

		if(me->inventory[i]->name == 0 ){
		continue;
		}
		if(strcmp(me->inventory[i]->name,arg) == 0 ){   //item matches inventory
		
		if(strcmp(me->inventory[i]->name,"hammer") == 0 && head->north != 0 && strcmp(head->north->name,"--Laundry room--\n\n") == 0 && head->north->locked == 1) {
		
		puts("You smash the lock with the hammer, the lock breaks\n\n");	//unlock garage

		head->north->locked = 0;
		check = 2;
		break; // make sure to break out of loop
		}
		


		///////////medpack use code/////////
		if(strcmp(me->inventory[i]->name,"medpack") == 0) {
		
		puts("\n Your health is replenished\n");   //medipack use code
		free(me->inventory[i]);
		me->inventory[i] = 0;
		me->hp +=50;
		if(me->hp >100)
		{
		me->hp = 100;		//make sure its not over 100
		}			
		check = 2;
		break;
		}
		/////////////////////




		//////soap bar use code
		if(strcmp(me->inventory[i]->name,"soap-bar") == 0) {
		puts("\n You use the soap to wash your hands...");
		sleep(1);
		puts("\n You feel more awake and more powerfull!\n");
			me->power +=10;
			check = 2;
		free(me->inventory[i]);
		me->inventory[i] = 0;
		break;
		}
		//////////



		//////tea use code//////////
	if(strcmp(me->inventory[i]->name,"tea") == 0) {
	  puts("\n You take a sip of hot tea");
	  sleep(1);
	  puts("\n You take another sip");
          sleep(1);
		puts("\n Suddenly you feel more refreshed! \n");
		me->power +=5;  //increase power
			check = 2;
			me->inventory[i] = 0; //set the pointer to null
		break;
		}
	        //////////////////////


		////default
		if(check !=2)
		puts("Nothing to use it on\n\n");
		check =1;
		break;
		}

		//////////
		}


//////no item
if(check <1)
puts("You dont have that item\n\n");
return;

}



void killm(room * head,person * me) {
	
	int d1;
	int d2;
	int fight=1;
	char *mdisc;
	char *hdisc;
	//srand(time(NULL));
	if(!head->monsters) 
	{
	fight = 0;
	puts("No one to kill");
	}
	
	if(fight) 
	{
	//system("clear");
	while(head->monsters->hp > 0 && me->hp > 0){   //loop until someone dies

	//srand(time(NULL));				//seed generator
	d1 = rand() % head->monsters->power;		//monster dice
	d2 = rand() % me->power;				//human dice
	if(d1 > d2) {
	
	  if(d1 < 10) {

	    mdisc = "hits";                        //hitting descriptions
	  }
	  else{

	    mdisc = "DESTROYS";

	  }


		
		me->hp-= d1;
		printf("\n%s %s you with %i damage %i hp left\n",head->monsters->name,mdisc,d1,me->hp);
		}
	if(d2 > d1) {
	


	
	  if(d1 < 10) {

	    hdisc = "hit";
	  }
	  else{                            //hitting discriptions

	    hdisc = "DESTROY";

	  }



		
	head->monsters->hp -= d2;
	printf("\n You %s %s with %i damage %i hp left\n",hdisc,head->monsters->name,d2,me->hp);
			}

	sleep(1);
		}

		if( head->monsters->hp <= 0) {
		   
		printf("\nYou Win, %s Has been Killed\n\n",head->monsters->name);
		 head->monsters=0;
		} else {
			
	printf("\n\n\n\nYou HAVE DIED!!!!!!!!!\n\n");
		exit(1); 
		}
	}
	return;
	
}



void drop(person * me, room * head, char * arg) {
int check = 0;
int i;
int j;





	for (i=0; i<49; i++) {
	
  if(me->inventory[i] == 0) {
continue;
}

	if(strcmp(me->inventory[i]->name,arg) == 0) { //check to see if person has item
	check=1;
		for(j=0; j<49; j++) {
			if(head->items[j]  == 0) {
		
		head->items[j] = me->inventory[i];   //put in an empty room inventory spot
		me->inventory[i] = NULL;
		break;
				}
			}
				if(check) 
				{
				break;
				}


		}


	}
				if(!check) 
				{
				printf("\nYou do not have that item\n");
				}
				return;

}


void quit() {

  exit (0);   //quit




}
void where(room * head) {

  printf("\n%s\n",head->name);
}
