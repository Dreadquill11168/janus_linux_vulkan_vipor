#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include "sha_utils.hpp"

struct Ctx{
    uint64_t bitlen; uint32_t state[8]; uint8_t data[64]; size_t datalen;
};
static inline uint32_t rotr(uint32_t x,uint32_t n){ return (x>>n)|(x<<(32-n)); }
static const uint32_t K[64]={
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2
};
static void transform(Ctx* c,const uint8_t d[64]){
    uint32_t a,b,cc,dd,e,f,g,h,m[64],t1,t2;
    for(int i=0,j=0;i<16;i++,j+=4) m[i]=(d[j]<<24)|(d[j+1]<<16)|(d[j+2]<<8)|(d[j+3]);
    for(int i=16;i<64;i++) m[i]=m[i-16]+(rotr(m[i-15],7)^rotr(m[i-15],18)^(m[i-15]>>3))+m[i-7]+(rotr(m[i-2],17)^rotr(m[i-2],19)^(m[i-2]>>10));
    a=c->state[0]; b=c->state[1]; cc=c->state[2]; dd=c->state[3]; e=c->state[4]; f=c->state[5]; g=c->state[6]; h=c->state[7];
    for(int i=0;i<64;i++){ t1=h+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^((~e)&g))+K[i]+m[i]; t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&cc)^(b&cc)); h=g; g=f; f=e; e=dd+t1; dd=cc; cc=b; b=a; a=t1+t2; }
    c->state[0]+=a; c->state[1]+=b; c->state[2]+=cc; c->state[3]+=dd; c->state[4]+=e; c->state[5]+=f; c->state[6]+=g; c->state[7]+=h;
}
static void init(Ctx* c){ c->datalen=0; c->bitlen=0; c->state[0]=0x6a09e667; c->state[1]=0xbb67ae85; c->state[2]=0x3c6ef372; c->state[3]=0xa54ff53a; c->state[4]=0x510e527f; c->state[5]=0x9b05688c; c->state[6]=0x1f83d9ab; c->state[7]=0x5be0cd19; }
static void update(Ctx* c,const uint8_t* data,size_t len){ for(size_t i=0;i<len;i++){ c->data[c->datalen++]=data[i]; if(c->datalen==64){ transform(c,c->data); c->bitlen+=512; c->datalen=0; } } }
static void final(Ctx* c,uint8_t* out){
    size_t i=c->datalen;
    if(i<56){ c->data[i++]=0x80; while(i<56) c->data[i++]=0; }
    else{ c->data[i++]=0x80; while(i<64) c->data[i++]=0; transform(c,c->data); std::memset(c->data,0,56); }
    c->bitlen += c->datalen*8;
    for(int j=0;j<8;j++) c->data[63-j]=(c->bitlen>>(8*j))&0xFF;
    transform(c,c->data);
    for(int j=0;j<8;j++){ out[4*j]=(c->state[j]>>24)&0xFF; out[4*j+1]=(c->state[j]>>16)&0xFF; out[4*j+2]=(c->state[j]>>8)&0xFF; out[4*j+3]=c->state[j]&0xFF; }
}
void sha256(const uint8_t* data,size_t len,uint8_t out[32]){ Ctx c; init(&c); update(&c,data,len); final(&c,out); }
std::string hex(const uint8_t* p,size_t n){ static const char* d="0123456789abcdef"; std::string s; s.resize(n*2); for(size_t i=0;i<n;i++){ s[2*i]=d[p[i]>>4]; s[2*i+1]=d[p[i]&0xF]; } return s; }
