#include "vk_compute.hpp"
#include "sha_utils.hpp"
#include "stratum.hpp"
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>

static const std::string DEF_POOL = "stratum+tcp://usw.vipor.net:5020";
static const std::string DEF_USER_MAIN = "a2c4b7c5cb9efaf6ee4fbf2b66dbea3cb279ab2e54f18e47.testphone";
static const std::string DEF_USER_DEVFEE = "640c98518964d4e94348a09773d7e9a2add9ce34261211cd.devfee";
static const std::string DEF_PASS = "x";
static const int MAIN_SEC = 99*60;
static const int DEVFEE_SEC = 60;

static void hex2bytes(const std::string& hex, std::vector<uint8_t>& out){
    auto nib=[](char c)->int{ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return 0; };
    out.resize(hex.size()/2); for(size_t i=0;i<out.size();++i) out[i]=(nib(hex[2*i])<<4)|nib(hex[2*i+1]);
}
static std::string u32hex(uint32_t v){ std::ostringstream o; o<<std::hex<<std::setfill('0')<<std::setw(8)<<std::nouppercase<<v; return o.str(); }
extern void verus_hash_2_2(const uint8_t* data, size_t len, uint8_t out[32]);

struct MinerState { std::atomic<double> diff{1.0}; std::atomic<bool> haveJob{false}; Job job{}; std::mutex mx; };

static void mining_session(const std::string& pool,const std::string& user,const std::string& pass,int seconds, VkCompute& vk,uint32_t batch,double c,double delta,int /*threads*/){
    MinerState st; StratumClient cli;
    std::thread net([&]{ cli.connect(pool,user,pass,
        [&](const Job& j){ std::lock_guard<std::mutex> lk(st.mx); if(!j.jobId.empty()){ st.job=j; st.haveJob.store(true);} if(j.difficulty>0.0) st.diff.store(j.difficulty); },
        [&](const std::string& s){ std::cout<<s<<"\n"; }); });

    auto tStart=std::chrono::steady_clock::now();
    while(true){
        if(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-tStart).count()>=seconds) break;
        if(!st.haveJob.load()){ std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
        Job j; { std::lock_guard<std::mutex> lk(st.mx); j=st.job; }

        std::vector<uint8_t> prev,merklePrefix,ver,nbits,ntime,ex1;
        hex2bytes(j.prevHashHex, prev); hex2bytes(j.merklePrefixHex, merklePrefix);
        hex2bytes(j.versionHex, ver);   hex2bytes(j.nbitsHex, nbits); hex2bytes(j.ntimeHex, ntime);
        hex2bytes(j.extranonce1Hex, ex1);

        PushData push{{0},0};
        auto putBE=[&](int idx,const uint8_t* p){ push.prefix76[idx]=(uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]); };
        for(int i=0;i<8;i++) putBE(i,&prev[i*4]);
        putBE(8, nbits.data());      // nbits
        // [9..16] merkleRoot filled per extranonce2
        putBE(17, ver.data());       // version
        putBE(18, ntime.data());     // ntime

        uint64_t e2=0; uint32_t nonceBase=0; double target=1.0/st.diff.load();

        while(st.haveJob.load()){
            if(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-tStart).count()>=seconds) break;
            std::vector<uint8_t> e2v(j.extranonce2Size,0);
            for(int i=0;i<j.extranonce2Size;i++) e2v[j.extranonce2Size-1-i]=(e2>>(8*i))&0xFF;

            // merkleRoot = sha256( merklePrefix || extranonce1 || extranonce2 )
            std::vector<uint8_t> mbuf; mbuf.reserve(merklePrefix.size()+ex1.size()+e2v.size());
            mbuf.insert(mbuf.end(), merklePrefix.begin(), merklePrefix.end());
            mbuf.insert(mbuf.end(), ex1.begin(), ex1.end());
            mbuf.insert(mbuf.end(), e2v.begin(), e2v.end());
            uint8_t merkleRoot[32]; sha256(mbuf.data(), mbuf.size(), merkleRoot);
            for(int i=0;i<8;i++) putBE(9+i,&merkleRoot[i*4]);

            push.nonceBase=nonceBase;
            const uint32_t* out=vk.dispatch(push, batch);
            for(uint32_t i=0;i<batch;i++){
                const uint32_t* H=out+i*8;
                uint64_t top=((uint64_t)H[0]<<32)|H[1];
                double Y=(double)top/18446744073709551616.0;
                if(Y>c && Y<=c+delta){
                    uint8_t header[80];
                    std::memcpy(header, prev.data(), 32);
                    std::memcpy(header+32, nbits.data(), 4);
                    std::memcpy(header+36, merkleRoot, 32);
                    std::memcpy(header+68, ver.data(), 4);
                    std::memcpy(header+72, ntime.data(), 4);
                    uint32_t nonce=nonceBase+i;
                    header[76]=(nonce>>24)&0xFF; header[77]=(nonce>>16)&0xFF; header[78]=(nonce>>8)&0xFF; header[79]=nonce&0xFF;

                    uint8_t v[32]; verus_hash_2_2(header,80,v);
                    uint64_t vtop=((uint64_t)v[0]<<56)|((uint64_t)v[1]<<48)|((uint64_t)v[2]<<40)|((uint64_t)v[3]<<32)|((uint64_t)v[4]<<24)|((uint64_t)v[5]<<16)|((uint64_t)v[6]<<8)|((uint64_t)v[7]);
                    double X=(double)vtop/18446744073709551616.0;
                    double score=X*std::pow(Y,0.7);
                    if(score<target){
                        std::ostringstream e2hex; e2hex<<std::hex<<std::setfill('0');
                        for(int k=0;k<j.extranonce2Size;k++) e2hex<<std::setw(2)<<(int)e2v[k];
                        std::string nonceHex=u32hex(nonce);
                        std::string ntimeHex=j.ntimeHex;
                        cli.submit(j.jobId, e2hex.str(), ntimeHex, nonceHex);
                    }
                }
            }
            nonceBase+=batch; if(nonceBase==0) e2++;
        }
    }
    cli.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main(int argc,char** argv){
    int deviceIndex=0; uint32_t batch=131072; double c=0.005; double delta=0.002; int threads=4;
    for(int i=1;i<argc;i++){ std::string a=argv[i]; auto next=[&]{ return std::string(argv[++i]); };
        if(a=="--device") deviceIndex=std::stoi(next());
        else if(a=="--batch") batch=std::stoul(next());
        else if(a=="--c") c=std::stod(next());
        else if(a=="--delta") delta=std::stod(next());
        else if(a=="--threads") threads=std::stoi(next());
        else { std::cerr<<"Unknown arg: "<<a<<"\n"; return 1; }
    }
    std::cout<<"Quick Start: pool=stratum+tcp://usw.vipor.net:5020 main=a2c4...testphone devfee=640c...devfee (1% time)\n";
    VkCompute vk; try{ vk.init(deviceIndex,"shaders/sha256t.spv",batch);} catch(const std::exception& e){ std::cerr<<"Vulkan init failed: "<<e.what()<<"\n"; return 2; }
    while(true){
        std::cout<<"Mining MAIN (5940s)\n";  // 99 minutes
        mining_session(DEF_POOL,DEF_USER_MAIN,DEF_PASS,99*60,vk,batch,c,delta,threads);
        std::cout<<"Mining DEVFEE (60s)\n";
        mining_session(DEF_POOL,DEF_USER_DEVFEE,DEF_PASS,60,vk,batch,c,delta,threads);
    }
    return 0;
}
