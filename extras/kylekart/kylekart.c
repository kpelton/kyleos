#include <stdio.h>
#include <string.h>
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"

struct kart { int x, speed, distance, lap; };

static void draw(struct kui_client *c, const struct kui_event *e, const struct kart *k)
{
    char text[64]; int horizon=e->height/3, center=e->width/2;
    kui_fill_rect(&c->surface,0,0,e->width,horizon,0x006cbcf2U);
    kui_fill_rect(&c->surface,0,horizon,e->width,e->height-horizon,0x003e9b42U);
    for(int y=horizon;y<e->height;y++){
        int d=y-horizon+1, half=30+d*3, bend=((k->distance+y*5)/80%120)-60;
        int cx=center+bend*d/(e->height-horizon);
        kui_fill_rect(&c->surface,cx-half,y,half*2,1,0x00595d62U);
        if(((k->distance+y*3)/36)&1){
            kui_fill_rect(&c->surface,cx-half,y,8,1,KUI_WHITE);
            kui_fill_rect(&c->surface,cx+half-8,y,8,1,0x00e33d35U);
        }
    }
    int kart_x=center+k->x-24, kart_y=e->height-92;
    kui_fill_rect(&c->surface,kart_x,kart_y+15,48,28,0x001b65c7U);
    kui_fill_rect(&c->surface,kart_x+8,kart_y,32,24,0x00e43d35U);
    kui_fill_rect(&c->surface,kart_x+14,kart_y+4,20,10,0x00ffd64aU);
    kui_fill_rect(&c->surface,kart_x-5,kart_y+31,10,15,KUI_BLACK);
    kui_fill_rect(&c->surface,kart_x+43,kart_y+31,10,15,KUI_BLACK);
    snprintf(text,sizeof(text),"KYLEKART  Speed %d  Lap %d",k->speed,k->lap+1);
    kui_draw_text(&c->surface,12,12,text,KUI_WHITE,0x006cbcf2U,0);
    kui_draw_text(&c->surface,12,31,"Arrows/WASD: drive",KUI_WHITE,0x006cbcf2U,0);
}

int main(void){
    struct kui_client c; struct kui_event e; struct kart k={0};
    kui_client_init(&c);
    while(kui_client_read_event(&e)==0){
        if(e.type==KUI_EVENT_CLOSE)break;
        if(e.type==KUI_EVENT_TIMER){ k.distance+=k.speed; if(k.speed>0)k.speed--; if(k.distance>=12000){k.distance-=12000;k.lap++;} }
        if(e.type==KUI_EVENT_KEY&&e.key_pressed){
            if(e.key_extended&&e.key_code==75)k.x-=12;
            else if(e.key_extended&&e.key_code==77)k.x+=12;
            else if((e.key_extended&&e.key_code==72)||e.character=='w')k.speed+=5;
            else if((e.key_extended&&e.key_code==80)||e.character=='s')k.speed-=8;
            else if(e.character=='a')k.x-=12; else if(e.character=='d')k.x+=12;
        }
        if(k.speed<0)k.speed=0;
        if(k.speed>120)k.speed=120;
        if(k.x<-180)k.x=-180;
        if(k.x>180)k.x=180;
        kui_client_begin(&c,&e,"KyleKart");kui_client_set_constraints(&c,520,380,760,560);draw(&c,&e,&k);kui_client_set_timer(&c,32);if(kui_client_send(&c)<0)break;
    } kui_client_free(&c);return 0;
}
