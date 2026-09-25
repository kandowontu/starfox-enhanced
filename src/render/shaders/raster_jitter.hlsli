// Exact 1/256-pixel phase mapping without shader int64. Split the large
// position product before multiplying its remainder by the phase denominator.
int jitterFloor(uint position,uint sourceSize,uint outputSize,float jitter,bool centered=false) {
    if(centered) jitter-=0.5;
    uint product=position*sourceSize;
    int numerator=int(product%outputSize)*256-int(round(jitter*256))*int(sourceSize);
    uint denominator=outputSize*256;
    int quotient=numerator<0?-int((uint(-numerator)+denominator-1)/denominator):int(uint(numerator)/denominator);
    return int(product/outputSize)+quotient;
}
int jitterCeil(int position,uint outputSize,uint sourceSize,float jitter,bool centered=false) {
    if(centered) jitter-=0.5;
    int product=position*int(outputSize);
    int base=product<0?-int(uint(-product)/sourceSize):int(uint(product)/sourceSize);
    int numerator=(product-base*int(sourceSize))*256+int(round(jitter*256))*int(sourceSize);
    uint denominator=sourceSize*256;
    int quotient=numerator<0?-int(uint(-numerator)/denominator):int((uint(numerator)+denominator-1)/denominator);
    return base+quotient;
}
