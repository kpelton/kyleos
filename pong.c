#include "asm.h"
#include "vga.h"
#include "tables.h"
#include "irq.h"
struct ball {
  int x;
  int y;
  int yv;
  int xv;
  int color;
};

struct paddle {
  int x;
  int y;
  int length;
  int color;
  int up_key;
  int down_key;
};

struct paddle* lpaddle = 0;
struct paddle* rpaddle = 0;
struct ball* ball = 0;

void paddle_down( struct paddle* p ) {
  if(p->y + p->length <24) 
    p->y++;
 }

void paddle_up( struct paddle* p ) {

  if(p->y > 0)
    p->y--;
}

void paddle_handle_key( struct paddle* p, int key ) {
  if(p->up_key == key)
    paddle_up(p);
  else if(p->down_key == key)
    paddle_down(p);
}

void paddle_draw( const struct paddle *p ) { 
  int i;
  
  //Draw each block
  for(i=0; i <p->length; i++) 
    print_loc(p->x,p->y+i,4,4,' ',0);
}


void ball_draw( const struct ball *b ) {
  
  print_loc(b->x,b->y,b->color,0,' ',0);

}


void pong_handle_key( const int key ) {
  //Called inside irq handler to update paddles
  if(lpaddle){
    paddle_handle_key(lpaddle,key);
    paddle_handle_key(rpaddle,key);
   
  }
}

void ball_update(struct paddle* lpaddle,struct paddle* rpaddle,struct ball* ball) { 

  if (ball->x <= lpaddle->x) {
    if( ball->y >= lpaddle->y && ball->y <= lpaddle->y+lpaddle->length){
      ball->xv *=-1;
      if (ball->y >(lpaddle->y+lpaddle->length/2)){
          ball->yv =1;
        }
        else if (ball->y <(lpaddle->y+lpaddle->length/2)){
        ball->yv =-1;
        }  
      
    }
    else{
      ball->x = 40;
      ball->y = 12;
      ball->yv = 0;
      ball->xv = 1;

    }
  } 
  else if( ball->x >=rpaddle->x ) {
    if  (ball->y >= rpaddle->y && ball->y <= rpaddle->y+rpaddle->length) {
      ball->xv *=-1;
      if (ball->y >(rpaddle->y+rpaddle->length/2)){
        ball->yv =1;
      }
      else if( ball->y <(rpaddle->y+rpaddle->length/2)){
        ball->yv =-1;
      }
    } else{
      ball->x = 40;
      ball->y = 12;
      ball->yv = 0;
      ball->xv = 1;
    }
  }
     
  if (ball->y <=0){
    ball->y =0;
    ball->yv*=-1;
  }
  else if( ball->y >=24){
    ball->y=24;
    ball->yv*=-1;
  }
  ball->x =ball->x+ball->xv;
  ball->y =ball->y+ball->yv;
  
}
    
void pong_start( void ) {
  struct paddle right;
  struct paddle left;
  struct ball ball; 
  int i=0;
  right.x = 79;
  right.y = 10;
  right.color = 5;
  right.length = 5;
  right.up_key = 0x91;
  right.down_key = 0x9f;

  left.x = 0;
  left.y = 10;
  left.color = 5;
  left.length = 5;
  left.up_key = 0xc8;
  left.down_key = 0xd0;

  ball.y = 12;
  ball.x = 40;
  ball.color = 2;
  ball.xv = -1;
  ball.yv = 0;
  lpaddle = &left;
  rpaddle = &right;

  for(;;){
    for(i=0;i<20000000;i++);
    vga_clear();
    ball_update(&left,&right,&ball);
    paddle_draw(&left);
    paddle_draw(&right);
   
    
    ball_draw(&ball);
  }
}


