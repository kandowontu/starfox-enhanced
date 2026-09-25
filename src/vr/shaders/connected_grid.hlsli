// Two compute passes. Only the compact camera/matrix/start words are uploaded.
// Output uses the same row-list ABI as the independent CPU reference.
[[vk::binding(1,0)]] RWStructuredBuffer<uint> grid_output;
static const uint grid_lists=384+225*15;
static const uint grid_points=grid_lists+192*675;
int grid_word(int value) {return (value<<16)>>16;}
[numthreads(64,1,1)]
void connected_project_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=225) return;
    int3 raw=int3(((int(texels[0])&255)^255)-1920,
        grid_word(-int(texels[1])),((int(texels[2])&255)^255)-1920);
    int3 position;
    for(uint axis=0;axis<3;++axis) {
        int a=int(texels[3+axis]),b=int(texels[6+axis]),c=int(texels[9+axis]);
        int origin=grid_word(((raw.x*a)>>15)+((raw.y*b)>>15)+((raw.z*c)>>15));
        position[axis]=grid_word(origin+int(id.x%15)*(a>>7)+int(id.x/15)*(c>>7));
    }
    int2 pixel=0;bool visible=false;
    if(position.z>256) {
        int reciprocal=(32767*256)/(min(position.z,12287)&~1);
        pixel=int2(grid_word(((position.x*reciprocal)>>15)+112),
            grid_word(((position.y*reciprocal)>>15)+96));
        visible=all(pixel>=0) && pixel.x<224 && pixel.y<192;
    }
    uint start=grid_points+id.x*4;
    grid_output[start]=uint(pixel.x);grid_output[start+1]=uint(pixel.y);
    grid_output[start+2]=uint(position.z);grid_output[start+3]=visible?1:0;
}
int grid_line_y(int2 current,int2 previous,int step) {
    int dx=current.x-previous.x,dy=current.y-previous.y;
    int ys=step>0 && dx>0?min(step,max(0,(step*abs(dy)-1)/dx)):0;
    return current.y+ys*(current.y<previous.y?1:-1);
}
[numthreads(64,1,1)]
void connected_rows_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=192) return;
    uint list=grid_lists+id.x*675,count=0;
    int2 previous=int2(texels[12],texels[13]);
    for(uint i=0;i<225;++i) {
        uint offset=grid_points+i*4;
        if(grid_output[offset+3]==0) continue;
        int2 current=int2(grid_output[offset],grid_output[offset+1])-int2(1,0);
        int x=current.x,y=current.y,dx=x-previous.x;
        uint record=384+i*15;
        // Exactly one writer per primitive; row threads only share reads of
        // the preceding projection pass. Records are consumed after a barrier.
        if(id.x==0) {
            grid_output[record]=1;grid_output[record+1]=uint(x);grid_output[record+2]=uint(y+2);
            grid_output[record+3]=grid_output[record+4]=0;
            grid_output[record+5]=0;grid_output[record+6]=uint(x);grid_output[record+7]=uint(y);
            grid_output[record+8]=uint(previous.x);grid_output[record+9]=uint(previous.y);
            grid_output[record+10]=1;grid_output[record+11]=uint(x-1);grid_output[record+12]=uint(y+1);
            grid_output[record+13]=grid_output[record+14]=0;
        }
        if(x>=0 && x<224 && int(id.x)==y+2) grid_output[list+count++]=record;
        int first=max(0,x-2-223),last=min(max(dx,0),x-2);
        if(first<=last) {
            int a=grid_line_y(current,previous,first),b=grid_line_y(current,previous,last);
            if(int(id.x)>=min(a,b) && int(id.x)<=max(a,b)) grid_output[list+count++]=record+5;
        }
        if(int(grid_output[offset+2])<512 && x-1>=0 && x-1<224 && int(id.x)==y+1)
            grid_output[list+count++]=record+10;
        previous=current;
    }
    grid_output[id.x*2]=list;grid_output[id.x*2+1]=count;
}
