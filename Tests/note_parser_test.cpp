#include "../Source/Engine/NovaNotes.h"
#include <cstdio>
int main(){ struct{const char* n; int e;} c[]={{"C3",60},{"C#3",61},{"Db3",61},{"A1",45},{"A-1",21},{"C-2",0},{"G8",127},{"60",60},{"C3_vel100",60},{"Bb2",58},{"B2",59},{"F#-1",18},{"xyz",-1},{"H3",-1},{"C",-1},{"A9",-1},{"128",-1},{"Piano C3",-1}};
 int bad=0; for(auto& x:c){int r=nova::parseNoteName(x.n); if(r!=x.e){printf("FAIL %s -> %d (erwartet %d)\n",x.n,r,x.e);++bad;}} printf("parseNoteName: %s\n",bad?"FEHLER":"alle 18 Faelle ok"); }
