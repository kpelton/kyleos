
#include "house.h"


int main(int argc, char **argv){





     /*
       TODO
       dynamic room loading of some kind
       clean code up
       more rooms more use codes
     */







     //system("clear");

     person * me;
     room * beg;
     room * garage;
     room * stor;
     room * bed;
     room * laundry;
     room * family;
     room * kitchen;
     room * hallway;
     room * downbathroom;
     room * den;
     room * stairs;

     item * hammer;
     item * knife;
     item * soap;
     item * medpack;
     item * tea;

     mob * parent1;
     mob * bear;

     me = 		(person *)malloc(sizeof( person));
     beg = 		(room *)malloc(sizeof(room));
     garage =	(room *)malloc(sizeof(room));
     stor = 	(room *)malloc(sizeof(room));
     bed  =  	(room *)malloc(sizeof(room));
     laundry = 	(room *)malloc(sizeof(room));
     family =	(room *)malloc(sizeof(room));
     kitchen =	(room *)malloc(sizeof(room));
     hallway =	(room *)malloc(sizeof(room));
     downbathroom=	(room *)malloc(sizeof(room));
     den =		(room *)malloc(sizeof(room));
     stairs =        (room *)malloc(sizeof(room));


     knife    =	(item *)malloc(sizeof(item));
     hammer   =	(item *)malloc(sizeof(item));
     soap    =	(item *)malloc(sizeof(item));
     medpack = 	(item *)malloc(sizeof(item));
     tea     =       (item *)malloc(sizeof(item));


     parent1 = 	(mob *)malloc(sizeof(mob));
     bear    = 	(mob *)malloc(sizeof(mob));



///////////room names and descriptions//////////
     strcpy(beg->desc, "You are in front of Kyles house the garage is to the north");
     strcpy(beg->name,"--Front of house--\n\n");

     strcpy(garage->desc,"You are in the garage of the house there are two cars parked here you see a door to the north of you leading into the house.The front of the house is south\n\n");
     strcpy(garage->name,"--Garage--\n\n");

     strcpy(laundry->desc," You are in the Laundry room there are clothes all over the floor. There is a washing maching and Dryer humming away. To the north is the family room,South is garage");

     strcpy(laundry->name,"--Laundry room--\n\n");

     strcpy(family->desc,"You are in the Family room you see a tv in the corner pictures on wall. The kitchen is to the east,laundry room to the south");
     strcpy(family->name,"--Family Room--\n\n");

     strcpy(kitchen->desc,"You are in the Kitchen. You smell lobster bisk cooking away on the stove. Family room to the West.Dining room to the east.Hallway to the south.");
     strcpy(kitchen->name,"--Kitchen--\n\n");

     strcpy(hallway->desc,"You are in the hallway you see pictures of kyles family hanging on the wall to the west of you is the Downstairs bathroom to the south is the den. To the east is the stairs.");
     strcpy(hallway->name,"--Hallway--\n\n");



     strcpy(stairs->desc,"You are on the stairs you see pictures on the wall up the stairs (north) there are more rooms. down (south) is the hallway.");
     strcpy(stairs->name,"--Stairs--\n\n");



     strcpy(downbathroom->desc,"You are in the downstairs bathroom, there is dirty sink in the room and toilet that looks like it hasnt been cleaned in years. The hallway is to the east.");
     strcpy(downbathroom->name,"--Downstairs Bathroom--\n\n");


     strcpy(den->desc,"You are in the Bear den, this looks like the office of some kind of bear there. the hallway is to the north");
     strcpy(den->name,"--Den--\n\n");
////////////






//////////////// mob names/////////


     strcpy(parent1->name,"Evil Monster");
     parent1->hp=100;
     parent1->power=5;

     strcpy(bear->name,"Grizzly Bear");
     bear->hp=100;
     bear->power=30;	
///////

//items names and descriptions///
     strcpy(knife->name,"knife" );
     strcpy(hammer->name,"hammer" );
     strcpy(soap->name,"soap-bar" );
     strcpy(medpack->name,"medpack" );
     strcpy(tea->name,"tea" );

     strcpy(hammer->desc,"-- slighty worn" );
     strcpy(knife->desc,"-- very sharp" );
     strcpy(soap->desc,"-- Brand New" );
     strcpy(medpack->desc,"-- This might be of use" );
     strcpy(tea->desc,"-- Looks fresh" );

////

///rooom mappings///
     beg->west=0;
     beg->east=0;
     beg->north=garage;				
     beg->south=0;


     garage->north=laundry;
     garage->south=beg;
     garage->east=0;
     garage->west=0;
     garage->items[0]=hammer;

     laundry->north=family;
     laundry->south=garage;
     laundry->east=0;				
     laundry->west=0;
     laundry->locked=1;


     family->south=laundry;
     family->north=0;
     family->west=0;
     family->east=kitchen;
     family->monsters=parent1;

     kitchen->south=hallway;
     kitchen->north=0;
     kitchen->west=family;
     kitchen->east=0;
     kitchen->items[0]=knife;

     hallway->south=den;
     hallway->north=kitchen;
     hallway->west=downbathroom;
     hallway->east=stairs;

     downbathroom->south=0;
     downbathroom->north=0;
     downbathroom->west=0;
     downbathroom->east=hallway;
     downbathroom->items[0]=soap;
     downbathroom->items[1]=medpack;


     den->south=0;
     den->north=hallway;
     den->west=0;
     den->east=0;
     den->monsters=bear;


     stairs->south=hallway;
     stairs->north=hallway;
     stairs->west=0;
     stairs->east=0;
     stairs->monsters=0;
     stairs->items[0]=tea;


///////////////////


//BEGIN REAL CODE///
     printf("Welcome to Kyles House\nYou are trying to find the panda in the house and bring her back outside. Watch out for monsters. type help for commands\n");
     puts("Enter your name:");
     me->hp=100; //set hp to 100;
     me->power=20; //set power to 20
    gets(me->name);
     //fgets(me->name,20,stdin);
     //system("clear");

     move(beg,me);
     return 0;
}



void move(room * beg,person *me) {
     char *Pcom;
     char *command;
     room * head;
     head = beg;


     roomdisc(head);
     while(1){

	  char *com;
	  com = (char *)malloc(sizeof(char)*50);
	 printf("\n");
     printf("HP:%i PW:%i>\n",me->hp,me->power);
	  //fgets(com,20,stdin);
      Pcom = com;
      //replace \n
      //while(*Pcom != '\n')
      //  Pcom++;
      //*Pcom = '\0';

	   //Pcom = com; 
       gets(com);

      command = strsep(&Pcom," "); 



	  if(strcmp(command,"n") == 0 || strcmp(command,"north") == 0){

	       if(head->north == 0) {
		    printf("You cant go that way\n");
	       }else{

		    if (checkroom(head,'n') == 1) {
			 continue;
		    }


		    head= head->north;
		    roomdisc(head);




	       }
	  }
	  if(strcmp(command,"s") == 0 || strcmp(command,"south") == 0){
	       if(head->south == 0){
		    printf("You cant go that way\n");
	       }else{
		    if (checkroom(head,'s') == 1) {
			 continue;
		    }
		    head= head->south;
		    roomdisc(head);

	       }
	  }

	  if(strcmp(command,"e") == 0 || strcmp(command,"east") == 0){
	       if(head->east == 0){
		    printf("You cant go that way\n");
	       }else{
		    if (checkroom(head,'e') == 1) {
			 continue;
		    }
		    head= head->east;
		    roomdisc(head);
	       }
	  }

	  if(strcmp(command,"w") == 0 || strcmp(command,"west") == 0){
	       if(head->west == 0){
		    printf("You cant go that way\n");
	       }else{
		    if (checkroom(head,'w') == 1) {
			 continue;
		    }
		    head= head->west;
		    roomdisc(head);
	       }
	  }

	  if(strcmp(command,"i") == 0 || strcmp(command,"inventory") == 0){
	       inventory(me);	
	  }


	  if(strcmp(command,"g") == 0 || strcmp(command,"get") == 0){
	       get(me,head,Pcom);
	  }

	  if(strcmp(command,"u") == 0 || strcmp(command,"use") == 0){
	       use(me,head,Pcom);
	  }

	  if(strcmp(command,"l") == 0 || strcmp(command,"look") == 0){
	       roomdisc(head);
	  }

	  if(strcmp(command,"k") == 0 || strcmp(command,"kill") == 0){
	       killm(head,me);
	       roomdisc(head);


	  }
	  if(strcmp(command,"drop") == 0){
	       drop(me,head,Pcom);
	  }

	  if(strcmp(command,"quit") == 0){
	       quit();
	  }


	  if(strcmp(command,"where") == 0){
	       where(head);

	  }


	  free(com);

	

     }

	
}





