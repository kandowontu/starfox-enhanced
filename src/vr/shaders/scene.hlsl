// Column-major world-to-eye and Vulkan eye projection, matching EyeCamera.
struct Camera { float4 view_rows[3]; column_major float4x4 projection; uint4 effects; };
[[vk::push_constant]] ConstantBuffer<Camera> camera;
float4x4 view_matrix() {
    return float4x4(camera.view_rows[0],camera.view_rows[1],camera.view_rows[2],float4(0,0,0,1));
}
struct Vertex {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float4 color : COLOR0;
    [[vk::location(2)]] float4 odd_color : COLOR1;
    [[vk::location(3)]] uint dither_scale : TEXCOORD0;
    [[vk::location(4)]] float3 visibility_a : TEXCOORD1;
    [[vk::location(5)]] float3 visibility_b : TEXCOORD2;
    [[vk::location(6)]] float3 visibility_c : TEXCOORD3;
    [[vk::location(7)]] uint visibility_enabled : TEXCOORD4;
    [[vk::location(8)]] float3 group_a : TEXCOORD5;
    [[vk::location(9)]] float3 group_b : TEXCOORD6;
    [[vk::location(10)]] float3 group_c : TEXCOORD7;
    [[vk::location(11)]] uint group_enabled : TEXCOORD8;
    [[vk::location(12)]] float2 uv : TEXCOORD9;
    [[vk::location(13)]] uint4 texture : TEXCOORD10;
    [[vk::location(14)]] float2 billboard : TEXCOORD11;
};
struct Fragment {
    float4 position : SV_Position;
    [[vk::location(0)]] nointerpolation float4 color : COLOR0;
    [[vk::location(1)]] nointerpolation float4 odd_color : COLOR1;
    [[vk::location(2)]] nointerpolation uint dither_scale : TEXCOORD0;
    [[vk::location(3)]] nointerpolation uint visible : TEXCOORD1;
    [[vk::location(4)]] noperspective float2 uv : TEXCOORD2;
    [[vk::location(5)]] nointerpolation uint4 texture : TEXCOORD3;
    [[vk::location(6)]] nointerpolation float3 horizon : TEXCOORD4;
    [[vk::location(7)]] nointerpolation uint border_index : TEXCOORD5;
    [[vk::location(8)]] float2 perspective_uv : TEXCOORD6;
    [[vk::location(9)]] float3 surface_position : TEXCOORD7;
    [[vk::location(10)]] nointerpolation float3 orbital_low : TEXCOORD8;
    [[vk::location(11)]] nointerpolation float3 orbital_high : TEXCOORD9;
    [[vk::location(12)]] nointerpolation uint4 cloud0 : TEXCOORD10;
    [[vk::location(13)]] nointerpolation uint4 cloud1 : TEXCOORD11;
    [[vk::location(14)]] nointerpolation uint4 cloud2 : TEXCOORD12;
    [[vk::location(15)]] nointerpolation uint4 cloud3 : TEXCOORD13;
};
float4 tile_background_sample(Fragment input);
uint source_visible(float3 point_a,float3 point_b,float3 point_c) {
    float3 a=mul(view_matrix(),float4(point_a,1)).xyz;
    float3 b=mul(view_matrix(),float4(point_b,1)).xyz;
    float3 c=mul(view_matrix(),float4(point_c,1)).xyz;
    precise float determinant=a.x*(b.y*c.z-b.z*c.y)-a.y*(b.x*c.z-b.z*c.x)+a.z*(b.x*c.y-b.y*c.x);
    float3 maximum=max(max(abs(a),abs(b)),abs(c));
    float scale=max(max(maximum.x,maximum.y),max(maximum.z,1.0));
    return determinant<=scale*scale*scale*1.0e-12;
}
int explosion_round(float value) {return int(sign(value)*floor(abs(value)+0.5));}
int explosion_word(int value) {return int(uint(value)<<16)>>16;}
int explosion_q15(float3 row,float3 value) {
    int3 coefficients=int3(round(row*32768.));
    int3 words=int3(explosion_word(explosion_round(value.x)),explosion_word(explosion_round(value.y)),explosion_word(explosion_round(value.z)));
    int3 products=(coefficients*words)>>15;
    return explosion_word(products.x+products.y+products.z);
}
float3 explosion_rotate(Vertex input,float3 value) {
    if(input.group_c.z!=0) return float3(explosion_q15(input.visibility_a,value),explosion_q15(input.visibility_b,value),explosion_q15(input.visibility_c,value));
    return float3(dot(input.visibility_a,value),dot(input.visibility_b,value),dot(input.visibility_c,value));
}
Fragment vertex_main(Vertex input) {
    Fragment output;
    bool particle_visible=true;
    if((input.texture.w&0x80000000U)!=0) {
        int3 delta=int3(input.group_b)-int3(input.group_a);
        delta=(delta<<16)>>16;
        float3 current=input.group_a+float3(delta)*input.group_c.x;
        float depth=input.group_c.y+current.z;
        particle_visible=depth>=256 && (input.group_c.z==0 || input.group_c.y+input.group_a.z>=256);
        input.position=input.group_c.z==1?input.group_a:current;
        input.billboard*=depth/128.;
        input.texture.w&=~0x80000000U;
    }
    bool exploding=input.visibility_enabled==2;
    float3 position=input.position;
    if(exploding) {
        float3 source=explosion_rotate(input,input.position);
        if(input.group_c.z!=0) source=float3(
            explosion_word(int(source.x)+explosion_round(input.group_a.x)),
            explosion_word(int(source.y)+explosion_round(input.group_a.y)),
            explosion_word(int(source.z)+explosion_round(input.group_a.z)));
        else source+=input.group_a;
        float3 direction=explosion_rotate(input,input.group_b);
        direction.y=-abs(direction.y);
        int3 rounded=int3(explosion_round(direction.x),explosion_round(direction.y),explosion_round(direction.z));
        source+=float3((rounded*int(input.group_c.x))>>2);
        position=source*float3(1,-1,-1)/input.group_c.y;
    }
    float4 eye_position=mul(view_matrix(),float4(position,1));
    if((input.texture.w&4)!=0) {
        // Keep the centre's model transform, but orient the art toward each
        // eye. Model-to-world unit scaling survives without rotating the quad.
        float sx=length(mul(view_matrix(),float4(1,0,0,0)).xyz);
        float sy=length(mul(view_matrix(),float4(0,1,0,0)).xyz);
        if(exploding) {sx/=input.group_c.y;sy/=input.group_c.y;}
        eye_position.xy+=input.billboard*float2(sx,sy);
    }
    output.position=mul(camera.projection,eye_position);
    // Source coplanar texture decals must survive the backing polygon's depth
    // (including tiny interpolation rounding differences), but remain occluded
    // by genuinely nearer geometry. One part per million of normalized depth.
    if((input.texture.w&32768)!=0) output.position.z-=output.position.w*0.000001;
    output.color=input.color;
    output.odd_color=input.odd_color;
    output.dither_scale=input.dither_scale;
    output.uv=input.uv;
    output.perspective_uv=input.uv;
    output.surface_position=position;
    if((input.texture.w&65536U)!=0) {
        // Source projection and headset projection are different transforms.
        // Interpolate source XY numerators and depth, then divide in the
        // fragment shader. Ordinary UV interpolation slides on tilted faces.
        output.surface_position=float3(input.uv,input.billboard.x==0?1:input.billboard.x);
    }
    output.texture=input.texture;
    output.horizon=0;
    output.border_index=0;
    output.visible=particle_visible?1:0;
    output.orbital_low=1;
    output.orbital_high=0;
    output.cloud0=output.cloud1=output.cloud2=output.cloud3=0;
    if((input.texture.w&8193U)==8193U) {
        output.cloud0=uint4(input.visibility_a,input.visibility_b.x);
        output.cloud1=uint4(input.visibility_b.yz,input.visibility_c.xy);
        output.cloud2=uint4(input.visibility_c.z,input.group_a);
        output.cloud3=uint4(input.group_b,input.group_c.x);
    }
    if(input.visibility_enabled==1) {
        output.visible=source_visible(input.visibility_a,input.visibility_b,input.visibility_c);
    }
    if(input.group_enabled!=0)
        output.visible &= source_visible(input.group_a,input.group_b,input.group_c);
    return output;
}
float4 styled_colour(float4 colour) {
    uint style=camera.effects.x;
    if(style==0 || camera.effects.y==0) return colour;
    float3 rgb=colour.rgb,result=rgb;
    float light=dot(rgb,float3(77,150,29))/256;
    // Quantize brightness, not individual channels: retain material hue.
    if(style==1) {
        float peak=max(max(rgb.r,rgb.g),max(rgb.b,1./255));
        result=rgb*(min(1.,floor(peak*5.+.5)/5.)/peak);
    }
    else if(style==4) result=light.xxx;
    else if(style==8) result=saturate(float3(dot(rgb,float3(101,197,48)),
        dot(rgb,float3(89,176,43)),dot(rgb,float3(70,137,34)))/256);
    else if(style==9) {
        const float3 palette[5]={float3(8,5,40),float3(65,20,150),float3(220,30,70),
            float3(255,150,15),float3(255,255,210)};
        float value=light*255/64;uint band=min(uint(value),3U);
        result=lerp(palette[band],palette[band+1],value-band)/255;
    } else if(style==10) result=saturate(float3(light/7,24./255+light*1.2,light/4));
    else if(style==11) result=96./255+rgb*5/8;
    else if(style==13) result=lerp(float3(55,8,100),float3(70,250,245),light)/255;
    else if(style==14) result=floor(saturate(rgb)*5+.5)/5;
    else if(style==15) result=lerp(float3(8,24,65),float3(224,250,246),light)/255;
    else if(style==16) {
        float3 contrast=rgb*rgb*(3-2*rgb);
        result=saturate(contrast*float3(1.06,1.,.87)+float3(.035,.015,.025));
    }
    return float4(lerp(rgb,result,min(camera.effects.y,100U)/100.),colour.a);
}
float4 fragment_main(Fragment input) : SV_Target0 {
    if(input.visible==0) discard;
    if((input.texture.w&4096)!=0 && dot(input.perspective_uv,input.perspective_uv)>1.) discard;
    if(input.dither_scale!=0) {
        uint2 pixel=uint2(input.position.xy)/input.dither_scale;
        if(((pixel.x^pixel.y)&1)!=0) return styled_colour(input.odd_color);
    }
    return styled_colour(input.color);
}
[[vk::binding(0,0)]] StructuredBuffer<uint> texels;
#include "connected_grid.hlsli"
// Tile payload: 16 control words, 256 RGBA palette words, then packed 64KiB
// VRAM. Controls: character/screen bases (words), screen size, signed scroll
// X/Y, bpp, palette base, priority (0/all,1/low,2/high), 16px tile flag,
// mosaic cell size (0/1 disabled, otherwise 2..16), scanline flags (1/H,2/V).
// Enabled scanlines append 224 signed H offsets then 224 V offsets after VRAM.
// Control 11 enables the native renderer's single-water-cross-section margins.
uint tile_vram_byte(uint start,uint address) {
    address&=65535;
    return (texels[start+272+(address>>2)]>>((address&3)*8))&255;
}
// Control 12 selects the cartridge BG2 Mode 2 vertical-offset table.
float3 tile_horizon(uint start) {
    float count=0,sum_x=0,sum_y=0,sum_xx=0,sum_xy=0;
    int previous_raw=0,previous_unwrapped=0;
    for(int i=0;i<32;++i) {
        uint address=(0x2fa0U+uint(i))*2;
        uint value=tile_vram_byte(start,address)|(tile_vram_byte(start,address+1)<<8);
        if((value&0x4000)==0) continue;
        int raw=int(value&8191),delta=(raw-previous_raw)&8191;
        if(delta>4095) delta-=8192;
        int unwrapped=count==0?raw:previous_unwrapped+delta;
        previous_raw=raw;previous_unwrapped=unwrapped;
        float x=float(i+1),y=float(unwrapped);
        count+=1;sum_x+=x;sum_y+=y;sum_xx+=x*x;sum_xy+=x*y;
    }
    if(count==0) return 0;
    float denominator=count*sum_xx-sum_x*sum_x;
    float slope=count>1 && denominator!=0?(count*sum_xy-sum_x*sum_y)/denominator:0;
    return float3((sum_y-slope*sum_x)/count,slope,1);
}
Fragment vertex_textured_main(Vertex input) {
    const float3 authored_position=input.position;
    const float2 authored_billboard=input.billboard;
    uint grid_visible=1;
    [branch] if((input.texture.w&268435456U)!=0) {
        float ground_y=asfloat(texels[input.texture.x+272+16384]);
        float radial=length(input.position.xz);
        if(radial>.0001) {
            float distance=input.position.y<-.0001?min(4096.,radial*ground_y/input.position.y):4096.;
            input.position.xz*=distance/radial;
        } else input.position.xz=0;
        input.position.y=ground_y;
        input.texture.w&=~268435456U;
    }
    [branch] if((input.texture.w&134217728U)!=0) {
        float size=input.group_a.x,depth=input.group_a.y;
        bool valid=isfinite(size) && isfinite(depth) && size>0 && depth>=128;
        bool glyph=(input.texture.w&1024U)!=0;
        float dimension=valid?trunc(size*256./depth):0;
        if(glyph) {
            // Scaled source text has no whole-object sprite's 240px cap.
            precise float side=dimension*depth/256.;
            precise float left=input.group_b.x*side;
            input.billboard=float2(left+input.billboard.x*side,input.billboard.y*side);
        } else {
            dimension=clamp(dimension,0.,240.);
            input.billboard*=dimension*depth/512.;
        }
        if(!glyph && input.group_b.z==1) {
            // EX aiming stations retain the game-plane basis under head roll.
            input.position.xy+=input.billboard;
            input.billboard=0;
            input.texture.w&=~4U;
        }
        grid_visible=valid && dimension>0?1:0;
        input.texture.w&=~134217728U;
    }
    [branch] if((input.texture.w&67108864U)!=0) {
        // Axis endpoints stay in the source arena; each line vertex selects
        // one GPU-reduced camera-space endpoint, never a CPU readback copy.
        uint offset=input.texture.x+min(input.texture.y,1U)*8U;
        float4 endpoint=asfloat(uint4(texels[offset],texels[offset+1],texels[offset+2],texels[offset+3]));
        bool valid=input.texture.y<2 && endpoint.w>=0 && all(isfinite(endpoint))
            && isfinite(input.billboard.y) && input.billboard.y>0;
        input.position=valid?endpoint.xyz*float3(1,-1,-1)/input.billboard.y:float3(0,0,-1);
        uint lookup=input.texture.z;
        if(valid && (texels[lookup+15]&1U)!=0) {
            // Axis preparation supplies the authored first face's material.
            // Fetch palette entries each draw so fades need no geometry upload.
            uint material=texels[lookup+16],palette=texels[lookup+14];
            uint even=texels[palette+(texels[material+4]&255U)];
            uint odd=texels[palette+(texels[material+5]&255U)];
            input.color=float4(even&255U,(even>>8)&255U,(even>>16)&255U,even>>24)/255.;
            input.odd_color=float4(odd&255U,(odd>>8)&255U,(odd>>16)&255U,odd>>24)/255.;
            if((texels[lookup+15]&2U)!=0) {
                input.color.rgb=select(input.color.rgb<=.04045,input.color.rgb/12.92,pow((input.color.rgb+.055)/1.055,2.4));
                input.odd_color.rgb=select(input.odd_color.rgb<=.04045,input.odd_color.rgb/12.92,pow((input.odd_color.rgb+.055)/1.055,2.4));
            }
            input.dither_scale=texels[material+6]!=0?1U:0U;
        }
        input.billboard=0;input.texture=0;grid_visible=valid?1:0;
    }
    [branch] if((input.texture.w&131072U)!=0) {
        // Resident source model: texture.x is the lookup metadata and y the
        // ordered occurrence. Never confuse this slot with a source face ID.
        uint start=input.texture.x,slot=input.texture.y;
        bool unclipped=(input.texture.w&4194304U)!=0;
        uint result=texels[start+1],capacity=texels[start+4];
        uint count=texels[result];
        bool valid=texels[result+1]==0 && count<=capacity && slot<count;
        uint resident_face=0;
        if(valid) {
            // Expanded colour-warp outputs have one material/geometry slot
            // per occurrence. Applying authored BSP face order again would
            // collapse repeated faces back onto the wrong random material.
            resident_face=(texels[start+15]&4U)!=0?slot:texels[texels[start]+slot];
            valid=resident_face<texels[start+5];
        }
        if((input.texture.w&33554432U)!=0 && valid) {
            uint face=resident_face;
            uint primitive=texels[texels[start+8]+face*4+3];
            bool ordinary=(primitive&7U)!=0; // textured polygon, line or sprite
            unclipped=ordinary;
            input.texture.w&=~(262144U|4194304U|524288U);
            input.texture.w|=ordinary?(262144U|4194304U):524288U;
            // A line-template submission must not draw a solid effect face.
            if(!ordinary && (input.texture.w&16777216U)!=0) valid=false;
        }
        if((input.texture.w&524288U)!=0) {
            uint corner=input.texture.z;
            // Mixed models share a fan large enough for their largest face.
            // Effect covers have four corners; pad extra fan triangles to zero.
            if((input.texture.w&33554432U)!=0) corner=min(corner,3U);
            valid=valid && corner<4 && isfinite(input.billboard.y) && input.billboard.y>0;
            if(valid) {
                uint face=resident_face,polygon=texels[start+8]+face*4;
                uint first=texels[polygon],size=texels[polygon+1],corner_count=texels[start+10];
                uint clipped=texels[start+12]+face*129*4,clipped_count=texels[clipped];
                valid=first<=corner_count && size<=corner_count-first && size>=3 && size<=32
                    && texels[clipped+1]==0 && clipped_count>=3 && clipped_count<=128;
                float3 anchor=0,previous=0,normal=0;float best=0;
                if(valid) for(uint i=0;i<size;++i) {
                    uint point_index=texels[texels[start+9]+(first+i)*4];
                    if(point_index>=texels[start+6]) {valid=false;break;}
                    uint offset=texels[start+2]+point_index*8;
                    float4 p=asfloat(uint4(texels[offset],texels[offset+1],texels[offset+2],texels[offset+3]));
                    if(p.w<0 || !all(isfinite(p))) {valid=false;break;}
                    if(i==0) anchor=p.xyz;
                    else if(i>=2) {
                        float3 candidate=cross(previous-anchor,p.xyz-anchor);float magnitude=dot(candidate,candidate);
                        if(magnitude>best) {normal=candidate;best=magnitude;}
                    }
                    previous=p.xyz;
                }
                valid=valid && best>0 && isfinite(best);
                float2 low=float2(1e30,1e30),high=-low;
                if(valid) for(uint i=0;i<clipped_count;++i) {
                    uint offset=clipped+4+i*4;
                    float2 p=asfloat(uint2(texels[offset],texels[offset+1]));
                    if(!all(isfinite(p))) {valid=false;break;}
                    low=min(low,p);high=max(high,p);
                }
                if(valid) {
                    // Expanded warp geometry is occurrence-indexed, but
                    // projection parameters remain authored-face inputs.
                    uint projection_face=(texels[start+15]&4U)!=0?
                        texels[texels[start]+slot]:face;
                    uint projection=texels[start+11]+projection_face*4;
                    float3 parameters=asfloat(uint3(texels[projection],texels[projection+1],texels[projection+2]));
                    uint header=texels[start+3]+slot*4;
                    float wave=(texels[header+3]&0x80000000U)!=0?4.:0.;
                    low=max(floor(low)-float2(1,wave),0.);
                    high=min(ceil(high)+float2(1,wave),float2(texels[start+7],texels[start+13]));
                    valid=all(high>low) && all(isfinite(parameters)) && parameters.z>0;
                    float3 selected=0;float2 selected_screen=0;
                    // Reject the whole cover quad if it crosses the plane's
                    // projection horizon. Near-plane subdivision remains a
                    // separate path; never emit a huge/inverted triangle.
                    if(valid) for(uint i=0;i<4;++i) {
                        float2 screen=float2((i==1 || i==2)?high.x:low.x,i>=2?high.y:low.y);
                        float3 ray=float3((screen-parameters.xy)/parameters.z,1);
                        float denominator=dot(normal,ray),distance=dot(normal,anchor);
                        float depth=distance/denominator;
                        if(denominator==0 || !isfinite(depth) || depth<=0) {valid=false;break;}
                        if(i==corner) {selected=ray*depth;selected_screen=screen;}
                    }
                    if(valid) {
                        input.position=float3(selected.x,-selected.y,-selected.z)/input.billboard.y;
                        input.uv=selected_screen*selected.z;input.billboard.x=selected.z;
                        input.visibility_enabled=0;input.group_enabled=0;
                    }
                }
            }
            if(!valid) {input.position=0;input.uv=0;input.billboard.x=1;}
        } else if((input.texture.w&262144U)!=0) {
            uint corner=input.texture.z;
            valid=valid && isfinite(input.billboard.y) && input.billboard.y>0;
            if(valid) {
                uint face=resident_face,polygon=texels[start+8]+face*4;
                uint first=texels[polygon],size=texels[polygon+1],corner_count=texels[start+10];
                bool line_template=(input.texture.w&16777216U)!=0;
                bool line_face=(texels[polygon+3]&2U)!=0;
                bool sprite_face=unclipped && (texels[polygon+3]&4U)!=0;
                if(sprite_face) corner=min(corner,3U);
                if(unclipped && size>=3) corner=min(corner,size-1);
                valid=first<=corner_count && size<=corner_count-first && (sprite_face?size==1:corner<size)
                    && (!unclipped || (line_template==line_face && (line_face?size==2:(sprite_face || size>=3))));
                if(sprite_face) valid=valid && texels[texels[start+16]+face*24+21]!=0;
                if(valid && unclipped) {
                    uint visibility=texels[polygon+2];
                    valid=visibility<texels[start+18] && texels[texels[start+17]+visibility]!=0;
                }
                if(valid) {
                    uint point_index=texels[texels[start+9]+(first+(sprite_face?0:corner))*4];
                    valid=point_index<texels[start+6];
                    if(valid) {
                        uint offset=texels[start+2]+point_index*8;
                        float4 p=asfloat(uint4(texels[offset],texels[offset+1],texels[offset+2],texels[offset+3]));
                        float2 screen=asfloat(uint2(texels[offset+4],texels[offset+5]));
                        valid=p.w>=0 && (unclipped || p.z>0) && all(isfinite(p)) && all(isfinite(screen));
                        if(valid) {
                            float units=input.billboard.y;
                            input.position=float3(p.x,-p.y,-p.z)/units;
                            input.uv=screen*p.z;input.billboard.x=p.z;
                            if(unclipped) {
                                uint material=texels[start+16]+face*24;
                                if(texels[material+21]!=0) {
                                    if(sprite_face) {
                                        float width=float(texels[material+9]+1U);
                                        float half=width*(width==64.?1.:.5)/units;
                                        const float2 corners[4]={float2(-1,1),float2(1,1),float2(1,-1),float2(-1,-1)};
                                        input.billboard=corners[corner]*half;
                                        input.uv=float2(corners[corner].x>0?width:0,corners[corner].y<0?width:0);
                                    } else {
                                        uint uv_corner=texels[start+9]+(first+corner)*4;
                                        input.uv=float2(texels[uv_corner+1],texels[uv_corner+2])
                                            +float2(asint(texels[material+22]),asint(texels[material+23]));
                                    }
                                }
                            }
                            input.visibility_enabled=0;input.group_enabled=0;
                        }
                    }
                }
            }
            if(!valid) {input.position=0;input.uv=0;input.billboard.x=1;}
        }
        grid_visible=valid?1:0;
        // Invalid slots still need a valid header address; fragment visibility
        // rejects them before reading any source command data.
        if(unclipped) {
            uint4 indexed_texture=0;
            if(valid) {
                uint face=resident_face,material=texels[start+16]+face*24;
                // Native two-point faces use decoded material colours, never
                // texture artwork, even if a warp descriptor selects one.
                bool source_line=(texels[texels[start+8]+face*4+3]&2U)!=0;
                if(texels[material+21]!=0 && !source_line)
                    indexed_texture=uint4(material,texels[start+19],texels[start+14],
                        8388608U|((texels[start+15]&2U)!=0?2U:0U)
                        |((texels[texels[start+8]+face*4+3]&4U)!=0?4U:0U)
                        |((texels[texels[start+8]+face*4+3]&16U)!=0?32768U:0U));
                uint even=texels[texels[start+14]+(texels[material+4]&255U)];
                uint odd=texels[texels[start+14]+(texels[material+5]&255U)];
                input.color=float4(even&255U,(even>>8)&255U,(even>>16)&255U,even>>24)/255.;
                input.odd_color=float4(odd&255U,(odd>>8)&255U,(odd>>16)&255U,odd>>24)/255.;
                if((texels[start+15]&2U)!=0) {
                    input.color.rgb=select(input.color.rgb<=.04045,input.color.rgb/12.92,pow((input.color.rgb+.055)/1.055,2.4));
                    input.odd_color.rgb=select(input.odd_color.rgb<=.04045,input.odd_color.rgb/12.92,pow((input.odd_color.rgb+.055)/1.055,2.4));
                }
                input.dither_scale=texels[material+6]!=0?1U:0U;
            }
            input.texture=indexed_texture;
        } else input.texture=uint4(texels[start+3]+(valid?slot:0)*4,texels[start+7],texels[start+14],
            65536U | ((texels[start+15]&3U)<<20));
    }
    [branch] if((input.texture.w&128)!=0) {
        uint start=input.texture.x;
        float3 source_camera=asfloat(uint3(texels[start],texels[start+1],texels[start+2]));
        bool surround=(input.texture.w&2048)!=0;
        float period=surround?4096.:65536.;
        float3 delta=fmod(input.position-source_camera,period);
        delta=select(delta>=period*.5,delta-period,delta);
        delta=select(delta< -period*.5,delta+period,delta);
        precise float3 transformed;
        for(uint axis=0;axis<3;++axis)
            transformed[axis]=(delta.x*int(texels[start+3+axis])+delta.y*int(texels[start+6+axis])+delta.z*int(texels[start+9+axis]))/32768.;
        float distance=surround?length(transformed):transformed.z;
        grid_visible=distance>=256. && (input.texture.z==0 || distance<1024.);
        float depth=clamp(distance,0.,4095.);
        // VR retains all directions in the source star volume. Colour depends
        // on radial distance, and billboards retain angular size behind/above
        // the viewer instead of being flattened onto the forward depth plane.
        input.position=surround?transformed:float3(transformed.xy,depth);
        input.billboard*=(surround?distance:depth)/256.;
        uint colour=start+12+(input.texture.y*16+(uint(depth)>>8))*4;
        input.color=asfloat(uint4(texels[colour],texels[colour+1],texels[colour+2],texels[colour+3]));
        input.odd_color=input.color;
        if((input.texture.w&16384)!=0) {
            // A fixed ray is born inside CONT's 112x88 inset at far depth.
            // It may travel outside the panel; only its emission is bounded.
            float controls_depth=4096.-float((uint(int(asfloat(texels[start+2])))+uint(authored_position.z))&4095U);
            grid_visible=controls_depth>=256 && (input.texture.z==0 || controls_depth<1024);
            float2 panel_point=float2(80,68)+authored_position.xy*(4096./controls_depth);
            panel_point+=authored_billboard*float2(1,-1);
            uint controls_colour=start+12+(input.texture.y*16+(uint(min(controls_depth,4095.))>>8))*4;
            input.color=asfloat(uint4(texels[controls_colour],texels[controls_colour+1],texels[controls_colour+2],texels[controls_colour+3]));
            input.odd_color=input.color;
            input.position=float3(panel_point,0);input.uv=panel_point;
            input.billboard=0;input.texture.w&=~4U;
        }
    }
    [branch] if((input.texture.w&64)!=0) {
        // Grid payload: three signed source start words, then nine Q15 words.
        // Shift each product before addition, exactly as transform_q15 does.
        uint start=input.texture.x;
        int3 origin=int3(texels[start],texels[start+1],texels[start+2]);
        int3 grid_point=0;
        for(uint axis=0;axis<3;++axis) {
            int mx=int(texels[start+3+axis]);
            int my=int(texels[start+6+axis]);
            int mz=int(texels[start+9+axis]);
            int value=((origin.x*mx)>>15)+((origin.y*my)>>15)+((origin.z*mz)>>15);
            value+=int(input.position.x)*(mx>>7)+int(input.position.z)*(mz>>7);
            grid_point[axis]=(value<<16)>>16;
        }
        bool surround_grid=(input.texture.w&8192)!=0;
        float distance=surround_grid?length(float3(grid_point)):float(grid_point.z);
        grid_visible=distance>256 && (input.texture.y==0 || distance<512);
        input.position=surround_grid?float3(grid_point):float3(grid_point.xy,min(grid_point.z,12287));
        input.billboard*=min(distance,12287.)/256.;
    }
    Fragment output=vertex_main(input);
    output.visible &= grid_visible;
    [branch] if((input.texture.w&8)!=0) {
        if((texels[input.texture.x+15]&0x80000000U)!=0) {
            // Palette probes are constant across the entire sphere. Decode
            // with the same GPU sampler once per vertex, not per eye pixel.
            uint flags=texels[input.texture.x+15];
            bool entry=(flags&0x8000000U)!=0,thin=(flags&0x40000000U)!=0;
            float horizon=entry?424.:thin?400.:384.;
            float depth=entry?40.:thin?15.:80.;
            Fragment sample=output;sample.surface_position=0;
            [unroll] for(uint i=0;i<8;++i) {
                sample.uv=float2(29+i*61,horizon+depth*(.3+.25*(i%3)));
                float4 colour=tile_background_sample(sample);
                if(colour.a!=0) {
                    output.orbital_low=min(output.orbital_low,colour.rgb);
                    output.orbital_high=max(output.orbital_high,colour.rgb);
                }
            }
        }
        // Water receiver geometry is Y-scaled between source ticks. Keep its
        // inverse texture projection in that same interpolated world space.
        // The eye view is rigid, so this column's length extracts only the
        // receiver scale, independent of headset rotation or translation.
        if((texels[input.texture.x+15]&32U)!=0)
            output.surface_position.y*=length(mul(view_matrix(),float4(0,1,0,0)).xyz);
        if(texels[input.texture.x+12]==2)
            output.horizon=tile_horizon(input.texture.x);
        if(texels[input.texture.x+14]!=0 || ((texels[input.texture.x+15]>>8)&65535U)!=0) {
            uint darkest=0xffffffff;
            for(uint i=0;i<256;++i) {
                uint colour=texels[input.texture.x+16+i];
                uint luma=77*((colour&255)>>3)+150*(((colour>>8)&255)>>3)+29*(((colour>>16)&255)>>3);
                if(luma<darkest) {darkest=luma;output.border_index=i;}
                if(luma==0) break;
            }
        }
    }
    return output;
}
int tile_vertical_offset(uint start,int coordinate,int fallback,float3 horizon) {
    if(horizon.z!=0) {
        float value=horizon.x+horizon.y*(float(coordinate)/8);
        return int(sign(value)*floor(abs(value)+.5))&8191;
    }
    int column=coordinate>=0?coordinate/8:-((-coordinate+7)/8);
    if(column>=1 && column<=32) {
        uint address=(0x2fa0U+uint(column-1))*2;
        uint value=tile_vram_byte(start,address)|(tile_vram_byte(start,address+1)<<8);
        return (value&0x4000)!=0?int(value&8191):fallback;
    }
    uint values[32];
    int first=-1,last=-1;
    for(int i=0;i<32;++i) {
        uint address=(0x2fa0U+uint(i))*2;
        values[i]=tile_vram_byte(start,address)|(tile_vram_byte(start,address+1)<<8);
        if((values[i]&0x4000)!=0) {
            if(first<0) first=i;last=i;
        }
    }
    int anchor=column<=0?0:31;
    if((values[anchor]&0x4000)==0) return fallback;
    int delta=0,span=1;
    if(first>=0 && last!=first) {
        delta=(int(values[last]&8191)-int(values[first]&8191))&8191;
        if(delta>4095) delta-=8192;
        span=last-first;
    }
    int distance=column<=0?min(column+1,0):column-32;
    return (int(values[anchor]&8191)+delta*distance/span)&8191;
}
float4 tile_colour(uint start,uint index,uint flags) {
    uint packed=texels[start+16+index];
    uint brightness=15-texels[start+13];
    uint3 rgb=uint3(packed&255,(packed>>8)&255,(packed>>16)&255)*brightness/15;
    float4 colour=float4(rgb,packed>>24)/255.;
    if((flags&2)!=0)
        colour.rgb=select(colour.rgb<=.04045,colour.rgb/12.92,pow((colour.rgb+.055)/1.055,2.4));
    return colour;
}
bool game_over_front_window(Fragment input) {
    // Intersect the actual eye ray with the existing 256x224 panel at z=-2.
    // Both eyes/head translations retain exactly the same foreground window.
    float3 t=float3(camera.view_rows[0].w,camera.view_rows[1].w,camera.view_rows[2].w);
    float3 eye=-float3(dot(float3(camera.view_rows[0].x,camera.view_rows[1].x,camera.view_rows[2].x),t),
        dot(float3(camera.view_rows[0].y,camera.view_rows[1].y,camera.view_rows[2].y),t),
        dot(float3(camera.view_rows[0].z,camera.view_rows[1].z,camera.view_rows[2].z),t));
    float3 ray=input.surface_position-eye;
    if(ray.z>=0) return false;
    float distance=(-2.-eye.z)/ray.z;
    float2 hit=eye.xy+ray.xy*distance;
    return distance>0 && abs(hit.x)<1. && abs(hit.y)<.875;
}
float4 tile_background_sample(Fragment input) {
    uint start=input.texture.x;
    uint screen_size=texels[start+2],bpp=texels[start+5];
    uint edge=texels[start+8]!=0?16:8;
    int2 logical=int2(floor(input.uv));
    if((texels[start+15]&0x40000000U)!=0 && input.surface_position.y>0) {
        // Low menu horizons need the sky above the native screen's top row.
        // Clamping at that row extrudes smoke/cloud pixels up to the zenith.
        logical.y=int(floor(112-atan2(input.surface_position.y,
            max(length(input.surface_position.xz),.0001))*512));
    }
    if((texels[start+15]&0x10000000U)!=0 && input.surface_position.y<=0) {
        float radial=max(length(input.surface_position.xz),.0001);
        float x=128+atan2(input.surface_position.x,-input.surface_position.z)*512;
        // The native window ends at row 223, but the surrounding ground
        // continues below it. Clamping there extrudes a dithered transition
        // row into longitude stripes (EX 5-1: atlas row 455). Continue through
        // the atlas's remaining ground rows and clamp at its terminal row.
        float last_row=float(((screen_size&2)!=0?64:32)*edge-1)-float(int(texels[start+4]));
        float y=clamp(112-input.surface_position.y*512/radial,0.,max(223.,last_row));
        logical=int2(floor(float2(x,y)));
    }
    if((texels[start+15]&32U)!=0) {
        float depth=max(abs(input.surface_position.z),.01);
        float x=input.surface_position.z<0?128+input.surface_position.x*256/depth:-256;
        float y=clamp(112-input.surface_position.y*256/depth,0.,223.);
        logical=int2(floor(float2(x,y)));
    }
    int original_y=logical.y;
    int original_x=logical.x;
    if((texels[start+15]&16U)!=0 && (original_y<0 || original_y>=224)) return 0;
    if(texels[start+14]!=0 && (original_x<0 || original_x>=256))
        return tile_colour(start,texels[start+14]-1U,input.texture.w);
    int mosaic=int(texels[start+9]);
    if(mosaic>1) logical-=((logical%mosaic)+mosaic)%mosaic;
    int2 scroll=int2(texels[start+3],texels[start+4]);
    uint scanlines=texels[start+10];
    if((scanlines&1)!=0 && logical.y>=0 && logical.y<224)
        scroll.x=int(texels[start+272+16384+logical.y]);
    if((scanlines&2)!=0)
        scroll.y=int(texels[start+272+16384+224+clamp(logical.y,0,223)]);
    if(texels[start+12]!=0) {
        int coordinate=logical.x+(int(texels[start+3])&7);
        scroll.y=tile_vertical_offset(start,coordinate,scroll.y,input.horizon);
    }
    int2 source=logical+scroll;
    if((texels[start+7]&512U)!=0) {
        if(game_over_front_window(input)) return 0;
        uint seed=(uint(source.x)>>5)*0x9e3779b9U^(uint(source.y)>>5)*0x85ebca6bU;
        seed^=seed>>16;seed*=0x7feb352dU;seed^=seed>>15;
        uint patch=(seed>>3)%3;
        source=int2((seed&7)*32+uint(source.x&31),(patch==0?0:128+patch*32)+uint(source.y&31));
    }
    if((texels[start+15]&0x20000000U)!=0)
        source.y=clamp(source.y,0,int(((screen_size&2)!=0?64:32)*edge)-1);
    if((texels[start+15]&2)!=0 && (source.x<0 || source.x>=int(((screen_size&1)!=0?64:32)*edge))) return 0;
    uint unique_rows=(texels[start+15]>>8)&65535U;
    if(unique_rows==512 && (source.y<0 || source.y>=int(((screen_size&2)!=0?64:32)*edge))) return 0;
    if(unique_rows!=0 && original_y<int(unique_rows) && (original_x<0 || original_x>=256)
        && (source.x<0 || source.x>=int(((screen_size&1)!=0?64:32)*edge)))
    {
        if((texels[start+15]&1)!=0) return 0;
        return tile_colour(start,input.border_index,input.texture.w);
    }
    uint2 tile_position=uint2(source)&uint2(((screen_size&1)!=0?64:32)*edge-1,((screen_size&2)!=0?64:32)*edge-1);
    if((texels[start+15]&0x8000000U)!=0 && tile_position.x>=336 && tile_position.x<392
        && tile_position.y>=320 && tile_position.y<384) tile_position.x&=127U;
    if((texels[start+15]&0x1000000U)!=0 && (original_x<0 || original_x>=512)
        && tile_position.y>=344 && tile_position.y<360)
        tile_position.x&=127U;
    if((texels[start+15]&0x2000000U)!=0 && (original_x<0 || original_x>=512)
        && tile_position.y>=320 && tile_position.y<352)
        tile_position.x&=127U;
    // Fortuna's right atlas half matches the left except for the moon.
    // Preserve the authored forward occurrence; repeat only moon-free art.
    bool unique_right=(texels[start+15]&192U)==192U;
    uint landscape_half=((screen_size&1)!=0?64U:32U)*edge/2;
    if((texels[start+15]&64U)!=0 && (original_x<0 || original_x>=int(unique_right?512U:landscape_half)))
        tile_position.x=unique_right?(tile_position.x&255U):((tile_position.x&(landscape_half-1))|landscape_half);
    if((texels[start+15]&8)!=0) tile_position.y&=255U;
    if((texels[start+15]&192U)==128U) tile_position.x&=255U;
    if((texels[start+15]&0x4000000U)!=0) {
        // Planet art is drawn once on a distant tangent patch, not distorted
        // onto this sky sphere. Replace its atlas region with ordinary stars.
        if(tile_position.x>=384 && tile_position.x<440 && tile_position.y>=192 && tile_position.y<256)
            tile_position.x&=255U;
        if(source.y<208) {
            tile_position.y=208U+(uint(source.y)&31U);
            tile_position.x&=255U;
        } else if((original_x<0 || original_x>=512) && tile_position.y>=264 && tile_position.y<360) {
            tile_position.x=(tile_position.x&127U)+256U;
        }
    }
    if(texels[start+11]!=0 && (original_x<0 || original_x>=256)) {
        int width=int(((screen_size&1)!=0?64:32)*edge);
        int water_x=int(uint(128+scroll.x)&uint(width-1))+logical.x-128;
        tile_position.x=uint(clamp(water_x,0,width-1));
    }
    uint2 cell=tile_position/edge;
    uint page=(cell.x>=32?1024:0)+(cell.y>=32?((screen_size&1)!=0?2048:1024):0);
    uint address=(texels[start+1]+page+(cell.y&31)*32+(cell.x&31))*2;
    uint tile=tile_vram_byte(start,address)|(tile_vram_byte(start,address+1)<<8);
    uint priority=texels[start+7]&3U;
    if((priority==1 && (tile&8192)!=0) || (priority==2 && (tile&8192)==0)) return 0;
    uint2 pixel=tile_position&(edge-1);
    if((tile&16384)!=0) pixel.x=edge-1-pixel.x;
    if((tile&32768)!=0) pixel.y=edge-1-pixel.y;
    uint number=((tile&1023)+(pixel.x>>3)+(pixel.y>>3)*16)&1023;
    uint base=texels[start]*2+number*(bpp*8)+(pixel.y&7)*2;
    uint mask=128U>>(pixel.x&7),index=0;
    for(uint pair=0;pair<bpp/2;++pair) {
        if((tile_vram_byte(start,base+pair*16)&mask)!=0) index|=1U<<(pair*2);
        if((tile_vram_byte(start,base+pair*16+1)&mask)!=0) index|=2U<<(pair*2);
    }
    // Pure sampling also runs in the vertex stage. Coverage rejection belongs
    // to the fragment wrapper; orbital continuation can fill transparent ink.
    if(index==0) return 0;
    if(bpp!=8) index+=((tile>>10)&7)*(1U<<bpp);
    // Control 13 stores attenuation (0 = full brightness, 15 = black).
    // Integer truncation must match the source palette fade before sRGB conversion.
    index=(index+texels[start+6])&255;
    // EX BG_5_4 mixes two authored planets with repeatable clouds. Suppress
    // only their repeated ink, matching the native background-region policy.
    if((texels[start+15]&4)!=0 && (original_x<0 || original_x>=256)) {
        int width=int(((screen_size&1)!=0?64:32)*edge);
        int centered_scroll=((scroll.x+width/2)%width+width)%width-width/2;
        int unique_x=logical.x+centered_scroll;
        bool large=tile_position.x>=256 && tile_position.x<288
            && tile_position.y>=288 && tile_position.y<320 && index>=81 && index<=95;
        bool small=tile_position.x>=288 && tile_position.x<304
            && tile_position.y>=304 && tile_position.y<320 && index>=81 && index<=86;
        if((unique_x<0 || unique_x>=width) && (large || small)) index=88;
    }
    if((texels[start+7]&256U)!=0
        && (original_x<0 || original_x>=256 || original_y<0 || original_y>=224)) {
        int width=int(((screen_size&1)!=0?64:32)*edge);
        int centered_scroll=((scroll.x+width/2)%width+width)%width-width/2;
        int unique_x=logical.x+centered_scroll;
        bool face=false;
#define SF_FACE_PLANET_REGION(l,t,r,b) face=face || (tile_position.x>=l && tile_position.x<r && tile_position.y>=t && tile_position.y<b);
#include "../../../include/starfox/render/ex_face_planet_regions.inc"
#undef SF_FACE_PLANET_REGION
        int height=int(((screen_size&2)!=0?64:32)*edge);
        if((unique_x<0 || unique_x>=width || source.y<0 || source.y>=height) && face) index=14;
    }
    // EX bitmap guards/cleared ink are transparent based on the original
    // palette, not the faded result (a fade-to-black must retain its coverage).
    if((texels[start+15]&1)!=0 && (texels[start+16+index]&0xffffffU)==0) return 0;
    return tile_colour(start,index,input.texture.w);
}
float orbital_hash(int3 cell) {
    uint n=uint(cell.x)*1597334677U+uint(cell.y)*3812015801U+uint(cell.z)*2798796415U;
    n=(n^(n>>16))*2246822519U;n=(n^(n>>13))*3266489917U;
    return float((n^(n>>16))&0xffffffU)/16777215.;
}
float orbital_noise(float3 p) {
    int3 cell=int3(floor(p));float3 f=frac(p);f=f*f*(3-2*f);
    float4 a=float4(orbital_hash(cell),orbital_hash(cell+int3(1,0,0)),
        orbital_hash(cell+int3(0,1,0)),orbital_hash(cell+int3(1,1,0)));
    float4 b=float4(orbital_hash(cell+int3(0,0,1)),orbital_hash(cell+int3(1,0,1)),
        orbital_hash(cell+int3(0,1,1)),orbital_hash(cell+int3(1,1,1)));
    return lerp(lerp(lerp(a.x,a.y,f.x),lerp(a.z,a.w,f.x),f.y),
        lerp(lerp(b.x,b.y,f.x),lerp(b.z,b.w,f.x),f.y),f.z);
}
float4 tile_background(Fragment input) {
    uint flags=texels[input.texture.x+15];
    if((flags&0x80000000U)==0) {
        float4 colour=tile_background_sample(input);
        if(colour.a==0) discard;
        return colour;
    }
    bool entry=(flags&0x8000000U)!=0,thin=(flags&0x40000000U)!=0;
    float horizon=entry?424.:thin?400.:384.;
    float depth=entry?40.:thin?15.:80.;
    float3 direction=normalize(input.surface_position);
    float latitude=atan2(direction.y,max(length(direction.xz),.0001));
    Fragment sample=input;sample.surface_position=0;
    // Keep the authored limb at its native angular scale. Compressing the
    // source strip exponentially folds its last rows into a stacked-tile band.
    // Only a few rows below the limb are useful; the hemisphere is continued
    // independently instead of stretching those rows towards the nadir.
    sample.uv=float2(128+atan2(direction.x,-direction.z)*512,
        max(0.,horizon-latitude*512));
    float blend=smoothstep(0.,min(.045,depth/1024.),-latitude);
    float4 authored=blend<1?tile_background_sample(sample):float4(0,0,0,1);
    if(blend==0) {
        if(authored.a==0) discard;
        return authored;
    }
    // The source only contains a narrow surface strip. Preserve its horizon,
    // then continue its colours as a detailed world-locked cloud field, not a
    // stretched cap or rows of repeated atlas tiles. Sample interior colours
    // only: neither the bright limb nor a unique sky planet is repeated.
    float3 low=input.orbital_low,high=input.orbital_high;
    // Three-dimensional noise avoids the vertical streaks caused by flattening
    // the sphere onto X/Z near the horizon, and has no longitude/pole seam.
    // Mild three-dimensional domain variation breaks up the regular noise
    // cells without introducing a spherical UV seam or a pinched centre.
    float3 p=direction*32;
    p+=float3(orbital_noise(direction*3+11),orbital_noise(direction*3+23),
        orbital_noise(direction*3+37))*.8;
    float clouds=orbital_noise(p)*.5+orbital_noise(p*2.13+7.3)*.25
        +orbital_noise(p*4.37-4.1)*.15+orbital_noise(p*8.71+19.7)*.1;
    // Palette steps retain the source's crisp colour boundaries. The former
    // low-frequency continuous gradient looked like a blurred enlargement.
    float band_coordinate=saturate((clouds-.25)*2)*7;
    // Hard palette thresholds crawl across pixels when either eye moves.
    // Antialias only each boundary over its screen-space pixel footprint;
    // interiors retain the crisp palette steps instead of a global blur.
    float band_width=clamp(fwidth(band_coordinate),.0001,1.);
    float bands=(floor(band_coordinate)+smoothstep(.5-band_width*.5,
        .5+band_width*.5,frac(band_coordinate)))/7;
    float3 continuation=lerp(low,high,bands);
    return float4(lerp(authored.rgb,continuation,max(blend,1-authored.a)),1);
}
// Sprites share one palette/VRAM payload. Vertex texture.y packs OBSEL and
// the 9-bit character number; texture.z packs attributes and square size.
float4 object_sprite(Fragment input) {
    uint start=input.texture.x;
    int2 pixel=int2(floor(input.uv));
    uint size=input.texture.z>>8,attributes=input.texture.z&255;
    if(any(pixel<0) || any(pixel>=int(size))) discard;
    if((attributes&64)!=0) pixel.x=int(size)-1-pixel.x;
    if((attributes&128)!=0) pixel.y=int(size)-1-pixel.y;
    uint select=input.texture.y&255,number=input.texture.y>>8;
    uint table=(select&7)*0x2000;
    if((number&256)!=0) table+=(((select>>3)&3)+1)*0x1000;
    uint tile=(number&255)+uint(pixel.y>>3)*16+uint(pixel.x>>3);
    uint address=table*2+tile*32+uint(pixel.y&7)*2;
    uint mask=128U>>uint(pixel.x&7),index=0;
    for(uint pair=0;pair<2;++pair) {
        if((tile_vram_byte(start,address+pair*16)&mask)!=0) index|=1U<<(pair*2);
        if((tile_vram_byte(start,address+pair*16+1)&mask)!=0) index|=2U<<(pair*2);
    }
    if(index==0) discard;
    return tile_colour(start,128+((attributes>>1)&7)*16+index,input.texture.w);
}
float4 backdrop_pixel(uint level,int2 coordinate) {
    uint record=4+3*level;
    int2 size=int2(texels[record+1],texels[record+2]);
    coordinate.x=texels[2]!=0?((coordinate.x%size.x)+size.x)%size.x:clamp(coordinate.x,0,size.x-1);
    coordinate.y=clamp(coordinate.y,0,size.y-1);
    uint packed=texels[texels[record]+uint(coordinate.y*size.x+coordinate.x)];
    return float4(packed&255,(packed>>8)&255,(packed>>16)&255,packed>>24)/255.;
}
float4 backdrop_level(uint level,float2 uv) {
    uint record=4+3*level;
    float2 position=uv*float2(texels[record+1],texels[record+2])-.5;
    int2 origin=int2(floor(position));float2 fraction=frac(position);
    return lerp(lerp(backdrop_pixel(level,origin),backdrop_pixel(level,origin+int2(1,0)),fraction.x),
                lerp(backdrop_pixel(level,origin+int2(0,1)),backdrop_pixel(level,origin+1),fraction.x),fraction.y);
}
float3 cloud_shade(Fragment input,uint shade) {
    uint4 group=shade<4?input.cloud0:shade<8?input.cloud1:shade<12?input.cloud2:input.cloud3;
    uint packed=group[shade&3];
    uint3 colour=(packed>>uint3(0,5,10))&31;
    return float3((colour<<3)|(colour>>2))/255.;
}
float3 nebula_shade(Fragment input,uint bank,uint shade) {
    return shade>=7?float3(0,0,0):cloud_shade(input,1+bank*7+shade);
}
float4 backdrop_filtered(float2 uv,float2 size) {
    // Perspective-correct derivatives select an immutable mip pyramid. Native
    // source textures retain their original affine, nearest-sampled path.
    float footprint=max(length(ddx(uv)*size),length(ddy(uv)*size));
    float level=clamp(log2(max(footprint,1.)),0.,float(texels[1]-1));
    uint low=uint(floor(level)),high=min(low+1,texels[1]-1);
    return lerp(backdrop_level(low,uv),backdrop_level(high,uv),frac(level));
}
float4 backdrop_colour(Fragment input) {
    float2 uv=input.perspective_uv,size=float2(input.texture.yz+1);
    float4 colour;
    if(uint(input.odd_color.w)==4) {
        float3 direction=normalize(input.surface_position);
        float latitude=atan2(direction.y,max(length(direction.xz),.00001));
        if(latitude>=.08) discard;
        float2 limb_uv=float2(uv.x,clamp(.633-latitude*(512./224.),.002,.998)/3.);
        // Blend sampled colours, not UVs: longitude collapses at the nadir,
        // but this Cartesian continuation has no seam or radial tile fan.
        // Stereographic disk is conformal (unlike x/z on a sphere, which
        // collapses radial detail near its equator). Equal source-pixel scale
        // in both axes also prevents the wide panorama stretching its rows.
        float2 disk=direction.xz/max(1.-direction.y,.001);
        float2 cap_uv=.5+disk*.46;
        cap_uv.y=(1.+2.*cap_uv.y)/3.;
        float cap=smoothstep(.08,.18,-latitude);
        colour=lerp(backdrop_filtered(limb_uv,size),backdrop_filtered(cap_uv,size),cap);
        colour*=1.-smoothstep(.025,.08,latitude);
    }
    else colour=backdrop_filtered(uv,size);
    // A 1x1 mip cannot retain two different pole radiances. Clamped poles use
    // their prepared, uniform master row independently of the footprint.
    if((uint(input.odd_color.w)&1)!=0 && uv.y<=0) colour=backdrop_pixel(0,int2(0,0));
    else if((uint(input.odd_color.w)&2)!=0 && uv.y>=1) colour=backdrop_pixel(0,int2(0,int(texels[6])-1));
    if(colour.a<=0) discard;
    colour.rgb/=colour.a; // Pyramid is premultiplied: no coloured alpha fringes.
    if(input.cloud0.x==1) {
        float light=dot(colour.rgb,float3(.299,.587,.114))*255.;
        float shade=1.+14.*(1.-saturate((light-140.)/105.));
        uint a=uint(shade),b=min(a+1,15U);
        colour.rgb=lerp(cloud_shade(input,a),cloud_shade(input,b),frac(shade));
    }
    else if(input.cloud0.x==2) {
        float peak=max(colour.r,max(colour.g,colour.b));
        float chroma=(peak-min(colour.r,min(colour.g,colour.b)))/max(1./255.,peak);
        float cool=saturate(.5+2.*(colour.b-colour.r)/max(1./255.,peak));
        float shade=7.*(1.-saturate(peak*255./180.));
        uint a=min(uint(shade),6U),b=a+1;
        float3 warm=lerp(nebula_shade(input,0,a),nebula_shade(input,0,b),shade-a);
        float3 cold=lerp(nebula_shade(input,1,a),nebula_shade(input,1,b),shade-a);
        colour.rgb=lerp(colour.rgb,lerp(warm,cold,cool),saturate(chroma*4.));
    }
    else if(input.cloud0.x==3) {
        float light=smoothstep(0.,1.,(uv.x*2.-1.+.05)/.4);
        colour.rgb=lerp(cloud_shade(input,2),cloud_shade(input,1),light)*(.8+.2*colour.r);
    }
    else if(input.cloud0.x==4) {
        float shade=1.+13.*(1.-saturate(colour.r*255./240.));
        uint a=uint(shade),b=min(a+1,14U);
        colour.rgb=lerp(cloud_shade(input,a),cloud_shade(input,b),frac(shade));
    }
    else if(input.cloud0.x==5) {
        float shade=1.+14.*(1.-saturate(colour.r));
        uint a=uint(shade),b=min(a+1,15U);
        colour.rgb=lerp(cloud_shade(input,a),cloud_shade(input,b),frac(shade));
    }
    else if(input.cloud0.x==6 || input.cloud0.x==7 || input.cloud0.x==8) {
        uint count=input.cloud0.x==8?7U:input.cloud0.x==6?2U:1U;
        float shade=1.+float(count)*(1.-saturate(colour.r));
        uint a=uint(shade),b=min(a+1,count+1);
        colour.rgb=lerp(cloud_shade(input,a),cloud_shade(input,b),frac(shade));
    }
    colour*=input.color;
    colour.rgb=saturate(colour.rgb+input.odd_color.rgb);
    if((input.texture.w&2)!=0)
        colour.rgb=select(colour.rgb<=.04045,colour.rgb/12.92,pow((colour.rgb+.055)/1.055,2.4));
    return colour;
}
float4 fragment_textured_raw(Fragment input) {
    if(input.visible==0) discard;
    if((input.texture.w&8193U)==8193U) return backdrop_colour(input);
    if((input.texture.w&8388608U)!=0) {
        // Resident source textures retain byte indices, not expanded RGBA.
        // UV interpolation remains affine, matching the existing model path.
        uint material=input.texture.x;
        uint2 masks=uint2(texels[material+9],texels[material+10]);
        if((input.texture.w&4U)!=0 && (any(input.uv<0) || any(input.uv>=float2(masks+1U)))) discard;
        uint2 coordinate=uint2(int2(floor(input.uv)))&masks;
        uint address=texels[material+8]+coordinate.y*(masks.x+1U)+coordinate.x;
        uint index=(texels[input.texture.y+(address>>2)]>>((address&3U)*8U))&255U;
        if(index==0) discard;
        uint packed=texels[input.texture.z+((index+texels[material+11])&255U)];
        float4 colour=float4(packed&255U,(packed>>8)&255U,(packed>>16)&255U,255U)/255.;
        if((input.texture.w&2U)!=0)
            colour.rgb=select(colour.rgb<=.04045,colour.rgb/12.92,pow((colour.rgb+.055)/1.055,2.4));
        return colour;
    }
    if((input.texture.w&65536U)!=0) {
        // Source span coverage mapped onto a real 3D face. Perspective UVs
        // keep the coverage attached to that face as either eye moves; the
        // ordinary depth buffer still resolves nearer geometry.
        // Header: command word offset, mask word offset, row count, wave phase.
        // Wave phase: enabled bit 31, signed offset bits 0..15, frame 16..19.
        uint header=input.texture.x;
        if(input.surface_position.z<=0) discard;
        int2 pixel=int2(floor(input.surface_position.xy/input.surface_position.z));
        if(pixel.x<0 || pixel.x>=int(input.texture.y)) discard;
        uint phase=texels[header+3];
        int shift=0;
        if((phase&0x80000000U)!=0) {
            int wrapped=int(((phase&65535U)+uint(pixel.x))<<16)>>16;
            wrapped=((wrapped>>1)+int((phase>>16)&15U)-1);
            wrapped=(wrapped<<16)>>16;
            const int sine[16]={0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
            shift=sine[uint(wrapped)&15U];
        }
        for(uint candidate=0;candidate<2;++candidate) {
            bool wave=candidate!=0;
            if(wave && (phase&0x80000000U)==0) continue;
            int source_y=pixel.y-(wave?shift:0);
            if(source_y<0 || source_y>=int(texels[header+2])) continue;
            uint command=texels[header]+uint(source_y)*24U;
            uint flags=texels[command+23];
            if(((flags&2U)!=0)!=wave) continue;
            if(pixel.x<asint(texels[command]) || pixel.x>=asint(texels[command+2])) continue;
            if((flags&1U)!=0 && pixel.x!=asint(texels[command+12]) && pixel.x!=asint(texels[command+13])) continue;
            if((flags&4U)!=0) {
                uint mask=texels[header+1]+(texels[command+8]+uint(source_y)*texels[command+9])/4U+uint(pixel.x)/32U;
                if((texels[mask]&(1U<<(uint(pixel.x)&31U)))==0) continue;
            }
            if((input.texture.w&1048576U)!=0) {
                uint scale=max(1U,texels[command+22]);
                uint index=texels[command+6]!=0 && (((uint(pixel.x)/scale)^(uint(pixel.y)/scale))&1U)!=0
                    ?texels[command+5]:texels[command+4];
                uint packed=texels[input.texture.z+(index&255U)];
                float4 colour=float4(packed&255U,(packed>>8)&255U,(packed>>16)&255U,packed>>24)/255.;
                if((input.texture.w&2097152U)!=0)
                    colour.rgb=select(colour.rgb<=.04045,colour.rgb/12.92,pow((colour.rgb+.055)/1.055,2.4));
                return colour;
            }
            uint scale=max(1U,input.dither_scale);
            if(input.dither_scale!=0 && (((uint(pixel.x)/scale)^(uint(pixel.y)/scale))&1U)!=0) return input.odd_color;
            return input.color;
        }
        discard;
    }
    // Source models deliberately retain affine texture mapping. World tile
    // layers instead need projective interpolation when viewed obliquely in VR.
    if((input.texture.w&8)!=0) input.uv=input.perspective_uv;
    if((input.texture.w&1024)!=0) {
        if(any(input.uv<0) || any(input.uv>=16)) discard;
        uint2 pixel=uint2(floor(input.uv));
        uint row=texels[input.texture.x+1+(pixel.y>>1)]>>((pixel.y&1)*16);
        if((row&(0x8000U>>pixel.x))==0) discard;
        uint packed=texels[input.texture.x];
        if((packed>>24)==0) discard;
        float4 color=float4(packed&255,(packed>>8)&255,(packed>>16)&255,packed>>24)/255.0;
        if((input.texture.w&2)!=0)
            color.rgb=select(color.rgb<=.04045,color.rgb/12.92,pow((color.rgb+.055)/1.055,2.4));
        return color;
    }
    if((input.texture.w&512)!=0) {
        int2 pixel=int2(floor(input.uv));
        if(any(pixel<0) || pixel.x>=224 || pixel.y>=192) discard;
        uint row=input.texture.x+uint(pixel.y)*2;
        uint start=texels[row],count=texels[row+1];
        for(uint i=0;i<count;++i) {
            uint record=texels[start+i];
            int2 current=int2(texels[record+1],texels[record+2]);
            if(texels[record]!=0) {
                if(all(pixel==current)) return input.color;
                continue;
            }
            int2 previous=int2(texels[record+3],texels[record+4]);
            int dx=current.x-previous.x,dy=current.y-previous.y;
            int step=current.x-2-pixel.x;
            if(step<0 || step>max(dx,0)) continue;
            int y_steps=0;
            if(step>0 && dx>0) y_steps=min(step,max(0,(step*abs(dy)-1)/dx));
            if(pixel.y==current.y+y_steps*(current.y<previous.y?1:-1)) return input.color;
        }
        discard;
    }
    if((input.texture.w&256)!=0) {
        uint start=input.texture.x;
        int2 current=int2(texels[start],texels[start+1]);
        int2 previous=int2(texels[start+2],texels[start+3]);
        int2 pixel=int2(floor(input.uv));
        int dx=current.x-previous.x,dy=current.y-previous.y;
        int step=current.x-2-pixel.x;
        if(step<0 || step>max(dx,0)) discard;
        int y_steps=0;
        if(step>0 && dx>0) y_steps=min(step,max(0,(step*abs(dy)-1)/dx));
        if(pixel.y!=current.y+y_steps*(current.y<previous.y?1:-1)) discard;
        return input.color;
    }
    if((input.texture.w&32)!=0) return tile_colour(input.texture.x,input.texture.y,input.texture.w);
    if((input.texture.w&16)!=0) return object_sprite(input);
    if((input.texture.w&8)!=0) return tile_background(input);
    if((input.texture.w&1073741824U)!=0) {
        // DXR R8 output: each row is padded to four bytes, with no palette.
        uint2 size=input.texture.yz+1;
        if(any(input.uv<0) || any(input.uv>=float2(size))) discard;
        uint2 pixel=uint2(floor(input.uv));
        uint offset=pixel.y*((size.x+3U)&~3U)+pixel.x;
        uint coverage=(texels[input.texture.x+offset/4]>>((offset&3U)*8))&255U;
        if(coverage==0) discard;
        return float4(input.color.rgb,input.color.a*(float(coverage)/255.0));
    }
    if((input.texture.w&1)==0) return fragment_main(input);
    if((input.texture.w&4)!=0 && (any(input.uv<0) || any(input.uv>=float2(input.texture.yz+1)))) discard;
    uint2 coordinate=uint2(int2(floor(input.uv)))&input.texture.yz;
    uint offset=coordinate.y*(input.texture.y+1)+coordinate.x;
    uint packed;
    if((input.texture.w&536870912U)!=0)
        packed=texels[input.texture.x+((texels[input.texture.x+256+offset/4]>>((offset&3U)*8))&255U)];
    else packed=texels[input.texture.x+offset];
    if((packed>>24)==0) discard;
    float4 color=float4(packed&255,(packed>>8)&255,(packed>>16)&255,packed>>24)/255.0;
    if((input.texture.w&2)!=0)
        color.rgb=select(color.rgb<=.04045,color.rgb/12.92,pow((color.rgb+.055)/1.055,2.4));
    return color;
}
float4 fragment_textured_main(Fragment input) : SV_Target0 {
    return styled_colour(fragment_textured_raw(input));
}
