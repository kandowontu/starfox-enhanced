// GPU port of the pinned xBRZ algorithm by Zenju (GPL version 3).
// See THIRD_PARTY_NOTICES.md and the xbrz dependency for the original source.
#include "xbrz_weights.hlsli"
float xdist(uint4 a,uint4 b) {
    int3 difference=(int3(a.rgb)-int3(b.rgb)+255)/2*2-255;
    float y=dot(float3(difference),float3(.2627,.678,.0593));
    float cb=(difference.b-y)*(.5/(1-.0593));
    float cr=(difference.r-y)*(.5/(1-.2627));
    float distance=sqrt(y*y+cb*cb+cr*cr);
    return min(a.a,b.a)/255.0*distance+abs(int(a.a)-int(b.a));
}
uint4 xpre(int2 p) {
    uint4 b=sourceCell(p+int2(0,-1)),c=sourceCell(p+int2(1,-1));
    uint4 e=sourceCell(p+int2(-1,0)),f=sourceCell(p),g=sourceCell(p+int2(1,0)),h=sourceCell(p+int2(2,0));
    uint4 i=sourceCell(p+int2(-1,1)),j=sourceCell(p+int2(0,1)),k=sourceCell(p+int2(1,1)),l=sourceCell(p+int2(2,1));
    uint4 n=sourceCell(p+int2(0,2)),o=sourceCell(p+int2(1,2));
    uint4 result=0;
    if((all(f==g) && all(j==k)) || (all(f==j) && all(g==k))) return result;
    float jg=xdist(i,f)+xdist(f,c)+xdist(n,k)+xdist(k,h)+4*xdist(j,g);
    float fk=xdist(e,j)+xdist(j,o)+xdist(b,g)+xdist(g,l)+4*xdist(f,k);
    if(jg<fk) {
        uint blend=3.6*jg<fk?2:1;
        if(any(f!=g) && any(f!=j)) result.x=blend;
        if(any(k!=j) && any(k!=g)) result.w=blend;
    } else if(fk<jg) {
        uint blend=3.6*fk<jg?2:1;
        if(any(j!=f) && any(j!=k)) result.z=blend;
        if(any(g!=f) && any(g!=k)) result.y=blend;
    }
    return result;
}
int2 xrotate(int2 p,uint rotation) {
    for(uint r=0;r<rotation;++r) p=int2(p.y,-p.x);
    return p;
}
uint4 xbrzSample(uint2 p,uint factor) {
    int2 cell=int2(p/factor);uint2 pixel=p%factor;
    uint blend=xpre(cell-int2(1,1)).w | (xpre(cell-int2(0,1)).z<<2)
        | (xpre(cell).x<<4) | (xpre(cell-int2(1,0)).y<<6);
    uint4 result=sourceCell(cell);
    for(uint rotation=0;rotation<4;++rotation) {
        uint rotated=((blend<<(rotation*2)) | (blend>>(8-rotation*2))) & 255;
        uint br=(rotated>>4)&3,tr=(rotated>>2)&3,bl=(rotated>>6)&3;
        if(br==0) continue;
        uint4 b=sourceCell(cell+xrotate(int2(0,-1),rotation)),c=sourceCell(cell+xrotate(int2(1,-1),rotation));
        uint4 d=sourceCell(cell+xrotate(int2(-1,0),rotation)),e=sourceCell(cell),f=sourceCell(cell+xrotate(int2(1,0),rotation));
        uint4 g=sourceCell(cell+xrotate(int2(-1,1),rotation)),h=sourceCell(cell+xrotate(int2(0,1),rotation));
        uint4 i=sourceCell(cell+xrotate(int2(1,1),rotation));
        bool lineBlend=br==2 || !((tr!=0 && xdist(e,g)>=30) || (bl!=0 && xdist(e,c)>=30)
            || (xdist(e,i)>=30 && xdist(g,h)<30 && xdist(h,i)<30 && xdist(i,f)<30 && xdist(f,c)<30));
        uint4 chosen=xdist(e,f)<=xdist(e,h)?f:h;
        uint kind=4;
        if(lineBlend) {
            float fg=xdist(f,g),hc=xdist(h,c);
            bool shallow=2.2*fg<=hc && any(e!=g) && any(d!=g);
            bool steep=2.2*hc<=fg && any(e!=c) && any(b!=c);
            kind=shallow?(steep?2:0):(steep?1:3);
        }
        // OutputMatrix maps a rotated coordinate back to the original pixel.
        uint2 at=pixel;
        for(uint r=0;r<rotation;++r) at=uint2(factor-1-at.y,at.x);
        uint2 weight=xbrzWeight(factor,kind,at);
        if(weight.x==0) continue;
        uint front=chosen.a*weight.x,back=result.a*(weight.y-weight.x),total=front+back;
        result=total==0?uint4(0,0,0,0):uint4((chosen.rgb*front+result.rgb*back)/total,total/weight.y);
    }
    return result;
}
