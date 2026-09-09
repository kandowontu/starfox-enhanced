// Derived from xBRZ, Copyright (C) Zenju, licensed under GPL version 3.
// Per-output-pixel kernel weights, retaining the pinned CPU backend rules.
uint2 xbrzWeight(uint factor,uint kind,uint2 p) {
    if(factor==2) {
        if(kind==0) {
            if(all(p==uint2(0,1))) return uint2(1,4);
            if(all(p==uint2(1,1))) return uint2(3,4);
        }
        if(kind==1) {
            if(all(p==uint2(1,0))) return uint2(1,4);
            if(all(p==uint2(1,1))) return uint2(3,4);
        }
        if(kind==2) {
            if(all(p==uint2(0,1))) return uint2(1,4);
            if(all(p==uint2(1,0))) return uint2(1,4);
            if(all(p==uint2(1,1))) return uint2(5,6);
        }
        if(kind==3) {
            if(all(p==uint2(1,1))) return uint2(1,2);
        }
        if(kind==4) {
            if(all(p==uint2(1,1))) return uint2(21,100);
        }
    }
    if(factor==3) {
        if(kind==0) {
            if(all(p==uint2(0,2))) return uint2(1,4);
            if(all(p==uint2(2,1))) return uint2(1,4);
            if(all(p==uint2(1,2))) return uint2(3,4);
            if(all(p==uint2(2,2))) return uint2(1,1);
        }
        if(kind==1) {
            if(all(p==uint2(2,0))) return uint2(1,4);
            if(all(p==uint2(1,2))) return uint2(1,4);
            if(all(p==uint2(2,1))) return uint2(3,4);
            if(all(p==uint2(2,2))) return uint2(1,1);
        }
        if(kind==2) {
            if(all(p==uint2(0,2))) return uint2(1,4);
            if(all(p==uint2(2,0))) return uint2(1,4);
            if(all(p==uint2(1,2))) return uint2(3,4);
            if(all(p==uint2(2,1))) return uint2(3,4);
            if(all(p==uint2(2,2))) return uint2(1,1);
        }
        if(kind==3) {
            if(all(p==uint2(2,1))) return uint2(1,8);
            if(all(p==uint2(1,2))) return uint2(1,8);
            if(all(p==uint2(2,2))) return uint2(7,8);
        }
        if(kind==4) {
            if(all(p==uint2(2,2))) return uint2(45,100);
        }
    }
    if(factor==4) {
        if(kind==0) {
            if(all(p==uint2(0,3))) return uint2(1,4);
            if(all(p==uint2(2,2))) return uint2(1,4);
            if(all(p==uint2(1,3))) return uint2(3,4);
            if(all(p==uint2(3,2))) return uint2(3,4);
            if(all(p==uint2(2,3))) return uint2(1,1);
            if(all(p==uint2(3,3))) return uint2(1,1);
        }
        if(kind==1) {
            if(all(p==uint2(3,0))) return uint2(1,4);
            if(all(p==uint2(2,2))) return uint2(1,4);
            if(all(p==uint2(3,1))) return uint2(3,4);
            if(all(p==uint2(2,3))) return uint2(3,4);
            if(all(p==uint2(3,2))) return uint2(1,1);
            if(all(p==uint2(3,3))) return uint2(1,1);
        }
        if(kind==2) {
            if(all(p==uint2(1,3))) return uint2(3,4);
            if(all(p==uint2(3,1))) return uint2(3,4);
            if(all(p==uint2(0,3))) return uint2(1,4);
            if(all(p==uint2(3,0))) return uint2(1,4);
            if(all(p==uint2(2,2))) return uint2(1,3);
            if(all(p==uint2(3,3))) return uint2(1,1);
            if(all(p==uint2(2,3))) return uint2(1,1);
            if(all(p==uint2(3,2))) return uint2(1,1);
        }
        if(kind==3) {
            if(all(p==uint2(2,3))) return uint2(1,2);
            if(all(p==uint2(3,2))) return uint2(1,2);
            if(all(p==uint2(3,3))) return uint2(1,1);
        }
        if(kind==4) {
            if(all(p==uint2(3,3))) return uint2(68,100);
            if(all(p==uint2(2,3))) return uint2(9,100);
            if(all(p==uint2(3,2))) return uint2(9,100);
        }
    }
    if(factor==5) {
        if(kind==0) {
            if(all(p==uint2(0,4))) return uint2(1,4);
            if(all(p==uint2(2,3))) return uint2(1,4);
            if(all(p==uint2(4,2))) return uint2(1,4);
            if(all(p==uint2(1,4))) return uint2(3,4);
            if(all(p==uint2(3,3))) return uint2(3,4);
            if(all(p==uint2(2,4))) return uint2(1,1);
            if(all(p==uint2(3,4))) return uint2(1,1);
            if(all(p==uint2(4,4))) return uint2(1,1);
            if(all(p==uint2(4,3))) return uint2(1,1);
        }
        if(kind==1) {
            if(all(p==uint2(4,0))) return uint2(1,4);
            if(all(p==uint2(3,2))) return uint2(1,4);
            if(all(p==uint2(2,4))) return uint2(1,4);
            if(all(p==uint2(4,1))) return uint2(3,4);
            if(all(p==uint2(3,3))) return uint2(3,4);
            if(all(p==uint2(4,2))) return uint2(1,1);
            if(all(p==uint2(4,3))) return uint2(1,1);
            if(all(p==uint2(4,4))) return uint2(1,1);
            if(all(p==uint2(3,4))) return uint2(1,1);
        }
        if(kind==2) {
            if(all(p==uint2(4,0))) return uint2(1,4);
            if(all(p==uint2(3,2))) return uint2(1,4);
            if(all(p==uint2(4,1))) return uint2(3,4);
            if(all(p==uint2(0,4))) return uint2(1,4);
            if(all(p==uint2(2,3))) return uint2(1,4);
            if(all(p==uint2(1,4))) return uint2(3,4);
            if(all(p==uint2(3,3))) return uint2(2,3);
            if(all(p==uint2(4,2))) return uint2(1,1);
            if(all(p==uint2(4,3))) return uint2(1,1);
            if(all(p==uint2(4,4))) return uint2(1,1);
            if(all(p==uint2(2,4))) return uint2(1,1);
            if(all(p==uint2(3,4))) return uint2(1,1);
        }
        if(kind==3) {
            if(all(p==uint2(2,4))) return uint2(1,8);
            if(all(p==uint2(3,3))) return uint2(1,8);
            if(all(p==uint2(4,2))) return uint2(1,8);
            if(all(p==uint2(3,4))) return uint2(7,8);
            if(all(p==uint2(4,3))) return uint2(7,8);
            if(all(p==uint2(4,4))) return uint2(1,1);
        }
        if(kind==4) {
            if(all(p==uint2(4,4))) return uint2(86,100);
            if(all(p==uint2(3,4))) return uint2(23,100);
            if(all(p==uint2(4,3))) return uint2(23,100);
        }
    }
    if(factor==6) {
        if(kind==0) {
            if(all(p==uint2(0,5))) return uint2(1,4);
            if(all(p==uint2(2,4))) return uint2(1,4);
            if(all(p==uint2(4,3))) return uint2(1,4);
            if(all(p==uint2(1,5))) return uint2(3,4);
            if(all(p==uint2(3,4))) return uint2(3,4);
            if(all(p==uint2(5,3))) return uint2(3,4);
            if(all(p==uint2(2,5))) return uint2(1,1);
            if(all(p==uint2(3,5))) return uint2(1,1);
            if(all(p==uint2(4,5))) return uint2(1,1);
            if(all(p==uint2(5,5))) return uint2(1,1);
            if(all(p==uint2(4,4))) return uint2(1,1);
            if(all(p==uint2(5,4))) return uint2(1,1);
        }
        if(kind==1) {
            if(all(p==uint2(5,0))) return uint2(1,4);
            if(all(p==uint2(4,2))) return uint2(1,4);
            if(all(p==uint2(3,4))) return uint2(1,4);
            if(all(p==uint2(5,1))) return uint2(3,4);
            if(all(p==uint2(4,3))) return uint2(3,4);
            if(all(p==uint2(3,5))) return uint2(3,4);
            if(all(p==uint2(5,2))) return uint2(1,1);
            if(all(p==uint2(5,3))) return uint2(1,1);
            if(all(p==uint2(5,4))) return uint2(1,1);
            if(all(p==uint2(5,5))) return uint2(1,1);
            if(all(p==uint2(4,4))) return uint2(1,1);
            if(all(p==uint2(4,5))) return uint2(1,1);
        }
        if(kind==2) {
            if(all(p==uint2(5,0))) return uint2(1,4);
            if(all(p==uint2(4,2))) return uint2(1,4);
            if(all(p==uint2(5,1))) return uint2(3,4);
            if(all(p==uint2(4,3))) return uint2(3,4);
            if(all(p==uint2(0,5))) return uint2(1,4);
            if(all(p==uint2(2,4))) return uint2(1,4);
            if(all(p==uint2(1,5))) return uint2(3,4);
            if(all(p==uint2(3,4))) return uint2(3,4);
            if(all(p==uint2(5,2))) return uint2(1,1);
            if(all(p==uint2(5,3))) return uint2(1,1);
            if(all(p==uint2(5,4))) return uint2(1,1);
            if(all(p==uint2(5,5))) return uint2(1,1);
            if(all(p==uint2(4,4))) return uint2(1,1);
            if(all(p==uint2(4,5))) return uint2(1,1);
            if(all(p==uint2(2,5))) return uint2(1,1);
            if(all(p==uint2(3,5))) return uint2(1,1);
        }
        if(kind==3) {
            if(all(p==uint2(3,5))) return uint2(1,2);
            if(all(p==uint2(4,4))) return uint2(1,2);
            if(all(p==uint2(5,3))) return uint2(1,2);
            if(all(p==uint2(5,4))) return uint2(1,1);
            if(all(p==uint2(5,5))) return uint2(1,1);
            if(all(p==uint2(4,5))) return uint2(1,1);
        }
        if(kind==4) {
            if(all(p==uint2(5,5))) return uint2(97,100);
            if(all(p==uint2(5,4))) return uint2(42,100);
            if(all(p==uint2(4,5))) return uint2(42,100);
            if(all(p==uint2(3,5))) return uint2(6,100);
            if(all(p==uint2(5,3))) return uint2(6,100);
        }
    }
    return uint2(0,1);
}
