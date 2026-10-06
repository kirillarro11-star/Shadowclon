#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

PSP_MODULE_INFO("ShadowFightLite", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SW 480
#define SH 272
#define BW 512
#define GY 220

static unsigned int __attribute__((aligned(64))) fb[BW * SH];

#define RGB(r,g,b) (0xFF000000u | ((unsigned)(r)<<16) | ((unsigned)(g)<<8) | (unsigned)(b))
#define COL_BG_TOP    RGB(10,13,26)
#define COL_BG_MID    RGB(26,16,48)
#define COL_BG_BOT    RGB(18,7,26)
#define COL_GROUND    RGB(150,110,255)
#define COL_TEXT      RGB(240,230,255)
#define COL_TEXT_DIM  RGB(160,140,210)
#define COL_PLAYER    RGB(94,200,255)
#define COL_HP_P      RGB(79,195,247)
#define COL_HP_E      RGB(255,82,82)
#define COL_MOON      RGB(255,242,205)

/* ---------- Примитивы ---------- */
static void fillRect(int x,int y,int w,int h,unsigned int c){
    int x0=x,y0=y,x1=x+w,y1=y+h;
    if(x0<0)x0=0; if(y0<0)y0=0;
    if(x1>SW)x1=SW; if(y1>SH)y1=SH;
    if(x1<=x0||y1<=y0)return;
    for(int yy=y0;yy<y1;yy++){
        unsigned int* r=&fb[yy*BW+x0];
        int n=x1-x0; for(int i=0;i<n;i++) r[i]=c;
    }
}
static void px(int x,int y,unsigned int c){
    if(x<0||x>=SW||y<0||y>=SH)return;
    fb[y*BW+x]=c;
}
static void line(int x0,int y0,int x1,int y1,unsigned int c,int t){
    int dx=abs(x1-x0),dy=-abs(y1-y0);
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1;
    int err=dx+dy,r=t/2;
    while(1){
        for(int oy=-r;oy<=r;oy++)for(int ox=-r;ox<=r;ox++)px(x0+ox,y0+oy,c);
        if(x0==x1&&y0==y1)break;
        int e2=2*err;
        if(e2>=dy){err+=dy;x0+=sx;}
        if(e2<=dx){err+=dx;y0+=sy;}
    }
}
static void circle(int cx,int cy,int r,unsigned int c){
    for(int y=-r;y<=r;y++){
        int w=(int)sqrtf((float)(r*r-y*y));
        for(int x=-w;x<=w;x++)px(cx+x,cy+y,c);
    }
}

/* ---------- Шрифт 5x7 ---------- */
static const unsigned char FONT[][5]={
    [' ']={0,0,0,0,0}, ['!']={0,0,0x5F,0,0}, ['-']={0x08,0x08,0x08,0x08,0x08},
    ['.']={0,0x60,0x60,0,0}, [':']={0,0x36,0x36,0,0}, ['/']={0x20,0x10,0x08,0x04,0x02},
    ['0']={0x3E,0x51,0x49,0x45,0x3E}, ['1']={0,0x42,0x7F,0x40,0},
    ['2']={0x42,0x61,0x51,0x49,0x46}, ['3']={0x21,0x41,0x45,0x4B,0x31},
    ['4']={0x18,0x14,0x12,0x7F,0x10}, ['5']={0x27,0x45,0x45,0x45,0x39},
    ['6']={0x3C,0x4A,0x49,0x49,0x30}, ['7']={0x01,0x71,0x09,0x05,0x03},
    ['8']={0x36,0x49,0x49,0x49,0x36}, ['9']={0x06,0x49,0x49,0x29,0x1E},
    ['A']={0x7E,0x11,0x11,0x11,0x7E}, ['B']={0x7F,0x49,0x49,0x49,0x36},
    ['C']={0x3E,0x41,0x41,0x41,0x22}, ['D']={0x7F,0x41,0x41,0x22,0x1C},
    ['E']={0x7F,0x49,0x49,0x49,0x41}, ['F']={0x7F,0x09,0x09,0x09,0x01},
    ['G']={0x3E,0x41,0x49,0x49,0x7A}, ['H']={0x7F,0x08,0x08,0x08,0x7F},
    ['I']={0,0x41,0x7F,0x41,0},      ['J']={0x20,0x40,0x41,0x3F,0x01},
    ['K']={0x7F,0x08,0x14,0x22,0x41}, ['L']={0x7F,0x40,0x40,0x40,0x40},
    ['M']={0x7F,0x02,0x0C,0x02,0x7F}, ['N']={0x7F,0x04,0x08,0x10,0x7F},
    ['O']={0x3E,0x41,0x41,0x41,0x3E}, ['P']={0x7F,0x09,0x09,0x09,0x06},
    ['Q']={0x3E,0x41,0x51,0x21,0x5E}, ['R']={0x7F,0x09,0x19,0x29,0x46},
    ['S']={0x46,0x49,0x49,0x49,0x31}, ['T']={0x01,0x01,0x7F,0x01,0x01},
    ['U']={0x3F,0x40,0x40,0x40,0x3F}, ['V']={0x1F,0x20,0x40,0x20,0x1F},
    ['W']={0x7F,0x20,0x18,0x20,0x7F}, ['X']={0x63,0x14,0x08,0x14,0x63},
    ['Y']={0x03,0x04,0x78,0x04,0x03}, ['Z']={0x61,0x51,0x49,0x45,0x43},
};
static void drawChar(int x,int y,char ch,unsigned int col,int s){
    if(ch>='a'&&ch<='z')ch-=32;
    if(ch<32||ch>'Z')return;
    const unsigned char* g=FONT[(int)ch];
    if(!g)return;
    for(int c=0;c<5;c++){
        unsigned char b=g[c];
        for(int r=0;r<7;r++) if(b&(1<<r)) fillRect(x+c*s,y+r*s,s,s,col);
    }
}
static void drawText(int x,int y,const char* t,unsigned int col,int s){
    int cx=x;
    while(*t){
        if(*t=='\n'){cx=x;y+=8*s;t++;continue;}
        drawChar(cx,y,*t,col,s); cx+=6*s; t++;
    }
}
static int textW(const char* t,int s){
    int w=0,mx=0;
    while(*t){
        if(*t=='\n'){if(w>mx)mx=w;w=0;t++;continue;}
        w+=6*s;t++;
    }
    return w>mx?w:mx;
}
static void drawTextCentered(int y,const char* t,unsigned int col,int s){
    int w=textW(t,s);
    drawText((SW-w)/2,y,t,col,s);
}

/* ---------- Оружие и враги ---------- */
typedef struct {
    const char* name;
    int wt,dmg,range;
    float dur,hs,he,cd;
} WeaponDef;
static const WeaponDef WEAPONS[3]={
    {"DOUBLE KNIVES",0, 7,46,0.26f,0.06f,0.15f,0.26f},
    {"FISTS",       1, 5,36,0.20f,0.04f,0.11f,0.19f},
    {"SICKLES",     2,11,56,0.38f,0.09f,0.20f,0.42f},
};

typedef struct {
    const char* name;
    int hp;
    float speed;
    int dmg,range;
    float cd;
    int wt;
    unsigned int color;
} EnemyDef;
static const EnemyDef ENEMIES[3]={
    {"GUARD I",  60, 90, 6, 42,0.95f,0,RGB(120,120,140)},
    {"GUARD II", 85,105, 8, 46,0.85f,1,RGB(140,120,100)},
    {"LYNX",    160,140,13, 56,0.62f,2,RGB(215,145,60)},
};

/* ---------- Боец ---------- */
typedef struct {
    float x,y,vx,vy;
    int hp,maxHp;
    int facing,onGround,blocking,hitDone;
    float attackT,cdT,stunT,hurtT;
    unsigned int color;
    int isPlayer;
    int wt,dmg,range;
    float dur,hs,he,maxCd;
    float aiT;
    int aiMode;
    int cfgIdx;
} Fighter;

#define STATE_MENU    0
#define STATE_WEAPONS 1
#define STATE_FIGHT   2
#define STATE_END     3

static int gState=STATE_MENU;
static int gWeapon=0;
static int gEnemyIdx=0;
static Fighter gPlayer,gEnemy;
static float gMsgT=0;
static char gMsg[128];
static float gShakeT=0;
static int gTransition=0;
static int gPlayerWon=0;
static int gWasCross=0,gWasLeft=0,gWasRight=0,gWasUp=0,gWasDown=0,gWasStart=0;
static unsigned int gLastUs=0;

static float dt(void){
    unsigned int now=sceKernelGetSystemTimeLow();
    float d=(now-gLastUs)/1000000.0f;
    gLastUs=now;
    if(d>0.05f)d=0.05f;
    if(d<0)d=0;
    return d;
}

static void setMsg(const char* t,float time){
    strncpy(gMsg,t,sizeof(gMsg)-1);
    gMsg[sizeof(gMsg)-1]=0;
    gMsgT=time;
}

static void initPlayer(void){
    memset(&gPlayer,0,sizeof(gPlayer));
    const WeaponDef* w=&WEAPONS[gWeapon];
    gPlayer.isPlayer=1;
    gPlayer.hp=gPlayer.maxHp=100;
    gPlayer.x=SW*0.25f; gPlayer.y=GY;
    gPlayer.facing=1; gPlayer.onGround=1;
    gPlayer.color=COL_PLAYER;
    gPlayer.wt=w->wt; gPlayer.dmg=w->dmg; gPlayer.range=w->range;
    gPlayer.dur=w->dur; gPlayer.hs=w->hs; gPlayer.he=w->he;
    gPlayer.maxCd=w->cd;
}
static void initEnemy(int idx){
    memset(&gEnemy,0,sizeof(gEnemy));
    const EnemyDef* e=&ENEMIES[idx];
    gEnemy.isPlayer=0;
    gEnemy.hp=gEnemy.maxHp=e->hp;
    gEnemy.x=SW*0.75f; gEnemy.y=GY;
    gEnemy.facing=-1; gEnemy.onGround=1;
    gEnemy.color=e->color;
    gEnemy.wt=e->wt; gEnemy.dmg=e->dmg; gEnemy.range=e->range;
    gEnemy.dur=0.32f; gEnemy.hs=0.09f; gEnemy.he=0.20f;
    gEnemy.maxCd=e->cd;
    gEnemy.cfgIdx=idx;
    gEnemy.aiT=0;
}

static void startFight(int idx){
    gEnemyIdx=idx;
    initPlayer();
    initEnemy(idx);
    gTransition=0;
    gState=STATE_FIGHT;
    char b[64];
    snprintf(b,sizeof(b),"ROUND %d\n%s",idx+1,ENEMIES[idx].name);
    setMsg(b,1.7f);
}

static void physics(Fighter* f,float d){
    f->vy+=1800.0f*d;
    f->x+=f->vx*d;
    f->y+=f->vy*d;
    if(f->y>=GY){f->y=GY;f->vy=0;f->onGround=1;}
    else f->onGround=0;
    if(f->x<28){f->x=28;if(f->vx<0)f->vx=0;}
    if(f->x>SW-28){f->x=SW-28;if(f->vx>0)f->vx=0;}
    if(f->onGround && f->stunT<=0) f->vx*=0.85f;
}

static void checkHit(Fighter* a,Fighter* b){
    if(a->attackT<=0||a->hitDone)return;
    float el=a->dur-a->attackT;
    if(el<a->hs||el>a->he)return;
    float dx=b->x-a->x;
    if(fabsf(dx)>a->range)return;
    if((dx>0?1:-1)!=a->facing && fabsf(dx)>8)return;
    if(fabsf(a->y-b->y)>80)return;
    a->hitDone=1;
    float dmg=(float)a->dmg;
    if(b->blocking){dmg*=0.22f;b->vx=a->facing*80;b->stunT=0.10f;}
    else{b->vx=a->facing*190;b->stunT=0.26f;b->hurtT=0.22f;}
    b->hp-=(int)dmg;
    if(b->hp<0)b->hp=0;
    gShakeT=0.14f;
}

static void enemyAI(float d){
    Fighter* e=&gEnemy; Fighter* p=&gPlayer;
    if(e->stunT>0)return;
    float dx=p->x-e->x;
    float dist=fabsf(dx);
    int dir=dx>0?1:-1;
    e->aiT-=d;
    if(e->aiT<=0){
        e->aiT=0.25f+(rand()%100)/250.0f;
        if(dist<=e->range*0.92f&&e->cdT<=0&&(rand()%100)<75) e->aiMode=2;
        else if(dist>e->range*1.05f) e->aiMode=0;
        else e->aiMode=(rand()%100)<28?1:3;
    }
    e->blocking=0;
    if(e->aiMode==0){ e->vx=dir*ENEMIES[e->cfgIdx].speed; }
    else if(e->aiMode==2){
        e->vx*=0.7f;
        if(e->cdT<=0&&dist<=e->range){
            e->attackT=e->dur; e->cdT=e->maxCd; e->hitDone=0;
        }
    }
    else if(e->aiMode==1){ e->blocking=1; e->vx=-dir*40; }
    else e->vx*=0.85f;
}

static void update(float d){
    if(gState!=STATE_FIGHT)return;
    Fighter* p=&gPlayer; Fighter* e=&gEnemy;
    if(p->attackT<=0&&p->stunT<=0) p->facing=(e->x>=p->x)?1:-1;
    if(e->attackT<=0&&e->stunT<=0) e->facing=(p->x>=e->x)?1:-1;

    p->cdT-=d; if(p->cdT<0)p->cdT=0;
    p->stunT-=d; if(p->stunT<0)p->stunT=0;
    p->hurtT-=d; if(p->hurtT<0)p->hurtT=0;
    if(p->attackT>0){p->attackT-=d; if(p->attackT<0)p->attackT=0;}

    if(p->stunT>0){p->vx*=0.9f;}
    else if(p->attackT>0){p->vx*=0.84f;}
    else if(p->blocking){p->vx*=0.7f;}
    else {
        float ax=0;
        if(gWasLeft)ax-=1;
        if(gWasRight)ax+=1;
        p->vx=ax*250;
    }

    e->cdT-=d; if(e->cdT<0)e->cdT=0;
    e->stunT-=d; if(e->stunT<0)e->stunT=0;
    e->hurtT-=d; if(e->hurtT<0)e->hurtT=0;
    if(e->attackT>0){e->attackT-=d; if(e->attackT<0)e->attackT=0;}

    enemyAI(d);
    physics(p,d);
    physics(e,d);
    checkHit(p,e);
    checkHit(e,p);

    if(gShakeT>0)gShakeT-=d;
    if(gMsgT>0)gMsgT-=d;

    if(!gTransition){
        if(e->hp<=0){
            gTransition=1;
            if(gEnemyIdx<2){
                setMsg("WIN!\nNEXT OPPONENT...",1.4f);
                /* небольшая задержка перед сменой */
            } else {
                setMsg("VICTORY!",1.4f);
                gPlayerWon=1;
            }
        } else if(p->hp<=0){
            gTransition=1;
            setMsg("DEFEAT",1.4f);
            gPlayerWon=0;
        }
    } else if(gMsgT<=0){
        if(e->hp<=0 && gEnemyIdx<2) startFight(gEnemyIdx+1);
        else {
            gState=STATE_END;
        }
    }
}

/* ---------- Отрисовка оружия ---------- */
static void drawWeapon(int wt,float hx,float hy){
    if(wt==0){ /* нож */
        line((int)hx,(int)hy,(int)hx+18,(int)hy-6,RGB(215,227,234),4);
        line((int)hx-5,(int)hy,(int)hx+4,(int)hy,RGB(90,74,58),5);
    } else if(wt==1){ /* кастет */
        circle((int)hx,(int)hy,6,RGB(255,183,77));
        circle((int)hx,(int)hy,3,RGB(255,143,0));
    } else { /* серп */
        line((int)hx-6,(int)hy,(int)hx+6,(int)hy-2,RGB(141,110,99),4);
        /* дуга серпа */
        for(int a=-40;a<=30;a+=6){
            float r=13.0f;
            float rad=a*3.14159f/180.0f;
            px((int)(hx+8+cosf(rad)*r),(int)(hy-4+sinf(rad)*r),RGB(230,238,245));
            px((int)(hx+9+cosf(rad)*r),(int)(hy-4+sinf(rad)*r),RGB(230,238,245));
        }
    }
}

/* ---------- Отрисовка бойца ---------- */
static void drawFighter(Fighter* f){
    if(!f)return;
    int dir=f->facing;
    unsigned int col=f->hurtT>0?RGB(255,91,91):f->color;
    float bx=f->x, by=f->y;

    /* тень */
    for(int i=-20;i<=20;i++) px((int)bx+i,(int)by+2,RGB(20,20,20));

    /* ноги */
    float lx0=bx-5, ly0=by-32;
    float lx1=bx-10, ly1=by-1;
    float rx0=bx+5, ly2=by-32;
    float rx1=bx+10, ry1=by-1;
    if(!f->onGround){ lx1=bx-14; rx1=bx+14; }
    line((int)lx0,(int)ly0,(int)lx1,(int)ly1,col,6);
    line((int)rx0,(int)ly2,(int)rx1,(int)ry1,col,6);

    /* торс */
    line((int)bx,(int)(by-60),(int)bx,(int)(by-30),col,14);

    /* голова */
    circle((int)bx,(int)(by-72),9,col);

    /* руки */
    float frontX=bx+11, frontY=by-42;
    float backX=bx-10, backY=by-44;
    if(f->attackT>0){
        float prog=1.0f-(f->attackT/f->dur);
        float swing=sinf(prog*3.14159f);
        if(swing<0)swing=0;
        if(swing>1)swing=1;
        frontX=bx+9+(f->range*0.75f)*swing*dir;
        frontY=by-52+8*(1-swing);
    } else if(f->blocking){
        frontX=bx+12*dir; frontY=by-55;
        backX=bx+4*dir;   backY=by-52;
    }
    line((int)bx,(int)(by-58),(int)backX,(int)backY,col,5);
    line((int)bx,(int)(by-58),(int)frontX,(int)frontY,col,6);

    /* оружие в передней руке */
    drawWeapon(f->wt,frontX,frontY);
    /* для двойных ножей — второе в задней */
    if(f->wt==0){
        drawWeapon(0,backX,backY);
    }

    if(f->hurtT>0){
        circle((int)bx,(int)(by-50),22,RGB(255,255,255));
    }
}

/* ---------- Фон ---------- */
static void drawBackground(void){
    for(int y=0;y<GY;y++){
        unsigned int c;
        float t=(float)y/(float)GY;
        if(t<0.5f){
            float u=t*2.0f;
            int r=(int)(10+(26-10)*u);
            int g=(int)(13+(16-13)*u);
            int b=(int)(26+(48-26)*u);
            c=RGB(r,g,b);
        } else {
            float u=(t-0.5f)*2.0f;
            int r=(int)(26+(18-26)*u);
            int g=(int)(16+(7-16)*u);
            int b=(int)(48+(26-48)*u);
            c=RGB(r,g,b);
        }
        unsigned int* row=&fb[y*BW];
        for(int x=0;x<SW;x++) row[x]=c;
    }

    /* луна */
    int mx=SW-100,my=45;
    for(int y=-40;y<=40;y++)for(int x=-40;x<=40;x++){
        float d2=(float)(x*x+y*y);
        if(d2<1600.0f){
            float a=1.0f-sqrtf(d2)/40.0f;
            if(a<0)a=0;
            int base=fb[(my+y)*BW+mx+x]&0xFFFFFF;
            int br=(base>>16)&0xFF,bg=(base>>8)&0xFF,bb=base&0xFF;
            int nr=br+(int)((255-br)*a*0.35f);
            int ng=bg+(int)((235-bg)*a*0.35f);
            int nb=bb+(int)((190-bb)*a*0.35f);
            fb[(my+y)*BW+mx+x]=RGB(nr,ng,nb);
        }
    }
    circle(mx,my,20,COL_MOON);

    /* силуэты "зданий" */
    int hts[]={60,40,55,30,50,45,58,38};
    int step=SW/7;
    for(int i=0;i<8;i++){
        int x0=i*step-step/2;
        int top=GY-hts[i];
        fillRect(x0,top,step+2,GY-top,RGB(20,12,34));
    }
    fillRect(0,GY-3,SW,3,COL_GROUND);
}

/* ---------- HUD ---------- */
static void drawBar(int x,int y,int w,int h,float ratio,unsigned int c,int flip){
    if(ratio<0)ratio=0; if(ratio>1)ratio=1;
    fillRect(x-1,y-1,w+2,h+2,RGB(0,0,0));
    fillRect(x,y,w,h,RGB(40,20,60));
    int fw=(int)(w*ratio);
    if(flip) fillRect(x+(w-fw),y,fw,h,c);
    else     fillRect(x,y,fw,h,c);
}

static void drawHUD(void){
    if(gState!=STATE_FIGHT)return;
    int bw=180,bh=12,top=14;
    drawBar(14,top,bw,bh,(float)gPlayer.hp/gPlayer.maxHp,COL_HP_P,0);
    drawBar(SW-14-bw,top,bw,bh,(float)gEnemy.hp/gEnemy.maxHp,COL_HP_E,1);

    drawText(14,top+bh+4,"YOU",COL_TEXT_DIM,1);
    const char* nm=ENEMIES[gEnemyIdx].name;
    int w=textW(nm,1);
    drawText(SW-14-w,top+bh+4,nm,COL_TEXT_DIM,1);

    char r[16]; snprintf(r,sizeof(r),"%d/3",gEnemyIdx+1);
    drawTextCentered(top+2,r,COL_TEXT_DIM,1);

    if(gMsgT>0){
        float a=gMsgT*1.6f; if(a>1)a=1;
        /* текст с тенью */
        drawTextCentered(SH*0.32f+2,gMsg,RGB(0,0,0),2);
        drawTextCentered(SH*0.32f,gMsg,COL_TEXT,2);
        /* затемнение через альфу: просто рисуем поверх повторно */
    }
}

/* ---------- Экраны ---------- */
static void drawMenu(void){
    /* фон */
    for(int y=0;y<SH;y++){
        float t=(float)y/SH;
        int r=(int)(27-t*20);
        int g=(int)(18-t*12);
        int b=(int)(48-t*30);
        unsigned int* row=&fb[y*BW];
        unsigned int c=RGB(r,g,b);
        for(int x=0;x<SW;x++) row[x]=c;
    }
    drawTextCentered(50,"SHADOW FIGHT",RGB(232,213,255),4);
    drawTextCentered(95,"LITE",RGB(232,213,255),4);
    drawTextCentered(140,"ACT I - LYNX & BODYGUARDS",COL_TEXT_DIM,1);
    drawTextCentered(180,"PRESS X TO START",COL_TEXT,1);
    drawTextCentered(210,"DPAD LEFT/RIGHT - MOVE",RGB(140,130,170),1);
    drawTextCentered(224,"X - ATTACK   O - JUMP",RGB(140,130,170),1);
    drawTextCentered(238,"SQUARE - BLOCK",RGB(140,130,170),1);
}

static void drawWeaponSelect(void){
    for(int y=0;y<SH;y++){
        float t=(float)y/SH;
        int r=(int)(27-t*20);
        int g=(int)(18-t*12);
        int b=(int)(48-t*30);
        unsigned int c=RGB(r,g,b);
        unsigned int* row=&fb[y*BW];
        for(int x=0;x<SW;x++) row[x]=c;
    }
    drawTextCentered(20,"CHOOSE WEAPON",RGB(232,213,255),3);
    const char* subs[3]={
        "DMG 7  FAST  MID RANGE",
        "DMG 5  VERY FAST  CLOSE",
        "DMG 11 SLOW  LONG REACH"
    };
    for(int i=0;i<3;i++){
        int y=70+i*55;
        unsigned int c=(i==gWeapon)?RGB(160,110,255):RGB(60,50,90);
        fillRect(40,y,SW-80,46,c);
        drawText(55,y+8,WEAPONS[i].name,COL_TEXT,2);
        drawText(55,y+26,subs[i],RGB(200,190,220),1);
    }
    drawTextCentered(SH-24,"DPAD UP/DOWN  X - FIGHT",COL_TEXT_DIM,1);
}

static void drawEnd(void){
    for(int y=0;y<SH;y++){
        float t=(float)y/SH;
        int r=(int)(27-t*20);
        int g=(int)(18-t*12);
        int b=(int)(48-t*30);
        unsigned int c=RGB(r,g,b);
        unsigned int* row=&fb[y*BW];
        for(int x=0;x<SW;x++) row[x]=c;
    }
    if(gPlayerWon){
        drawTextCentered(70,"ACT I CLEARED",RGB(232,213,255),4);
        drawTextCentered(120,"LYNX DEFEATED",COL_TEXT,2);
        char b[64];
        snprintf(b,sizeof(b),"WEAPON: %s",WEAPONS[gWeapon].name);
        drawTextCentered(160,b,COL_TEXT_DIM,1);
    } else {
        drawTextCentered(70,"DEFEAT",RGB(255,120,120),4);
        char b[64];
        snprintf(b,sizeof(b),"KILLED BY: %s",ENEMIES[gEnemyIdx].name);
        drawTextCentered(130,b,COL_TEXT_DIM,1);
    }
    drawTextCentered(SH-30,"PRESS X TO MENU",COL_TEXT,1);
}

/* ---------- Ввод ---------- */
static void readInput(void){
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad,1);
    int l=(pad.Buttons&PSP_CTRL_LEFT)!=0;
    int r=(pad.Buttons&PSP_CTRL_RIGHT)!=0;
    int u=(pad.Buttons&PSP_CTRL_UP)!=0;
    int d=(pad.Buttons&PSP_CTRL_DOWN)!=0;
    int x=(pad.Buttons&PSP_CTRL_CROSS)!=0;
    int o=(pad.Buttons&PSP_CTRL_CIRCLE)!=0;
    int sq=(pad.Buttons&PSP_CTRL_SQUARE)!=0;
    int st=(pad.Buttons&PSP_CTRL_START)!=0;

    if(gState==STATE_MENU){
        if(x&&!gWasCross){
            gState=STATE_WEAPONS; gWeapon=0;
        }
    } else if(gState==STATE_WEAPONS){
        if(d&&!gWasDown) gWeapon=(gWeapon+1)%3;
        if(u&&!gWasUp)   gWeapon=(gWeapon+2)%3;
        if(x&&!gWasCross){ startFight(0); }
    } else if(gState==STATE_FIGHT){
        if(gWasLeft!=l)   gWasLeft=l;
        if(gWasRight!=r)  gWasRight=r;
        gPlayer.blocking=sq;
        if(o&&!gWasUp){
            if(gPlayer.onGround&&gPlayer.stunT<=0&&gPlayer.attackT<=0){
                gPlayer.vy=-600;
                gPlayer.onGround=0;
            }
        }
        if(x&&!gWasCross){
            if(gPlayer.cdT<=0&&gPlayer.attackT<=0&&gPlayer.stunT<=0){
                gPlayer.attackT=gPlayer.dur;
                gPlayer.cdT=gPlayer.maxCd;
                gPlayer.hitDone=0;
                gPlayer.blocking=0;
            }
        }
    } else if(gState==STATE_END){
        if(x&&!gWasCross){
            gState=STATE_MENU;
        }
    }

    gWasLeft=l; gWasRight=r; gWasUp=u; gWasDown=d;
    gWasCross=x; gWasStart=st;
}

/* ---------- Точка входа ---------- */
int main(void){
    sceDisplaySetMode(0,SW,SH);
    sceDisplaySetFrameBuf((void*)fb,BW,PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_IMMEDIATE);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    gLastUs=sceKernelGetSystemTimeLow();

    while(1){
        sceDisplayWaitVblankStart();
        float d=dt();
        readInput();
        update(d);

        /* очищаем фон и рисуем сцену */
        if(gShakeT>0){
            int sh=(int)(gShakeT*30);
            int ox=(rand()%(2*sh+1))-sh;
            int oy=(rand()%(2*sh+1))-sh;
            /* простой сдвиг — перерисовать весь кадр с offset через
               сохранение текущего буфера слишком дорого, поэтому
               трясём за счёт небольшого смещения при отрисовке бойцов */
            (void)ox;(void)oy;
        }

        if(gState==STATE_MENU)         drawMenu();
        else if(gState==STATE_WEAPONS) drawWeaponSelect();
        else if(gState==STATE_END)     drawEnd();
        else if(gState==STATE_FIGHT){
            drawBackground();
            drawFighter(&gEnemy);
            drawFighter(&gPlayer);
            drawHUD();
        }
    }
    return 0;
}
