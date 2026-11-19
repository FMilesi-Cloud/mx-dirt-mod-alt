/* fork: check for the live reset / smooth path. After mxdirt_height_field_rebase the neutral sim (every texel at
   the reference value) must publish exactly current*factor: 0 = flat track, .5 = ruts halved.
   Build and run:  zig cc -std=c11 -I src/core tests/test_rebase.c src/core/mxdirt_height_field.c -o build/test_rebase.exe && build/test_rebase.exe */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "mxdirt_height_field.h"
int main(void){
    enum {SRC=8,DST=9,CELLS=DST*DST};
    MxdirtHeightConfig c={SRC,SRC,SRC,DST,DST,100.f,100.f,1.f,.5f,1.,51.,1};
    int16_t base[CELLS];int32_t initial[CELLS],current[CELLS],out[CELLS];float neutral[SRC*SRC];
    for(int k=0;k<CELLS;k++){base[k]=0;initial[k]=(k%7-3)*40000;current[k]=(k%5-2)*90001;}
    for(int k=0;k<SRC*SRC;k++)neutral[k]=c.reference_encoded;
    MxdirtHeightField *p;assert(mxdirt_height_field_create(&p,&c,base,initial,CELLS));
    assert(mxdirt_height_field_convert(p,1,neutral,SRC*SRC,out,CELLS,NULL));
    for(int k=0;k<CELLS;k++)assert(out[k]==initial[k]);             /* neutral sim = loaded surface */
    assert(mxdirt_height_field_rebase(p,current,CELLS,0.f));
    assert(mxdirt_height_field_convert(p,1,neutral,SRC*SRC,out,CELLS,NULL));
    for(int k=0;k<CELLS;k++)assert(out[k]==0);                      /* reset: no deformation at all */
    assert(mxdirt_height_field_rebase(p,current,CELLS,.5f));
    assert(mxdirt_height_field_convert(p,1,neutral,SRC*SRC,out,CELLS,NULL));
    for(int k=0;k<CELLS;k++){long e=(long)current[k]/2;assert(labs(out[k]-e)<=1);} /* smooth: halved */
    assert(!mxdirt_height_field_rebase(p,current,CELLS,1.5f));      /* refuses a factor above 1 */
    mxdirt_height_field_destroy(p);puts("test_rebase ok");return 0;
}
