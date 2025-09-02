#include "stratum.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <string>

static void parseUrl(const std::string& url,std::string& host,std::string& port){
    auto p=url.find("://"); std::string r=p==std::string::npos?url:url.substr(p+3);
    auto c=r.find(':'); host=r.substr(0,c==std::string::npos?r.size():c); port=(c==std::string::npos)?"3032":r.substr(c+1);
}
bool StratumClient::connect(const std::string& url,const std::string& user,const std::string& pass, OnJob oj, OnLog ol){
    onJob=oj; onLog=ol; std::string host,port; parseUrl(url,host,port);
    struct addrinfo hints{}; hints.ai_socktype=SOCK_STREAM; struct addrinfo* res=nullptr;
    if(getaddrinfo(host.c_str(),port.c_str(),&hints,&res)!=0) return false;
    sock=::socket(res->ai_family,res->ai_socktype,res->ai_protocol); if(sock<0) return false;
    if(::connect(sock,res->ai_addr,res->ai_addrlen)<0){ ::close(sock); sock=-1; return false; }
    freeaddrinfo(res);

    sendLine(R"({"id":1,"method":"mining.subscribe","params":["janusv/0.2",""]})");
    std::string line, extranonce1=""; int e2size=2;
    while(true){ line=readLine(); if(line.empty()) return false; if(line.find(R"("id":1)")!=std::string::npos) break; }
    auto q1=line.find("\"result\":"); auto s1=line.find('"',q1); auto s2=line.find('"',s1+1); extranonce1=line.substr(s1+1,s2-s1-1);
    auto lb=line.rfind(',',line.find(']',s2)); e2size=std::atoi(line.substr(lb+1).c_str());
    onLog("Subscribed; extranonce1="+extranonce1+" e2size="+std::to_string(e2size));
    std::ostringstream auth; auth<<R"({"id":2,"method":"mining.authorize","params":[")"<<user<<R"(","")"<<pass<<R"("]})"; sendLine(auth.str());

    while(true){
        line=readLine(); if(line.empty()) break;
        if(line.find("\"method\":\"mining.notify\"")!=std::string::npos){
            Job j; auto pid=line.find("\"params\":");
            auto q=line.find('"',pid+9),qn=line.find('"',q+1); j.jobId=line.substr(q+1,qn-q-1);
            q=line.find('"',qn+1); qn=line.find('"',q+1); j.prevHashHex=line.substr(q+1,qn-q-1);
            q=line.find('"',qn+1); qn=line.find('"',q+1); j.merklePrefixHex=line.substr(q+1,qn-q-1);
            q=line.find('"',qn+1); qn=line.find('"',q+1); j.versionHex=line.substr(q+1,qn-q-1);
            q=line.find('"',qn+1); qn=line.find('"',q+1); j.nbitsHex=line.substr(q+1,qn-q-1);
            q=line.find('"',qn+1); qn=line.find('"',q+1); j.ntimeHex=line.substr(q+1,qn-q-1);
            j.clean=(line.find("true",qn)!=std::string::npos); j.extranonce1Hex=extranonce1; j.extranonce2Size=e2size; onJob(j);
        } else if(line.find("\"method\":\"mining.set_difficulty\"")!=std::string::npos){
            auto lb2=line.find('['); auto rb2=line.find(']',lb2+1); Job j; j.difficulty=atof(line.substr(lb2+1,rb2-lb2-1).c_str()); onJob(j);
        } else if(line.find("\"id\":4")!=std::string::npos){
            onLog(line.find("\"result\":true")!=std::string::npos?"Share accepted":"Share rejected");
        }
    }
    return true;
}
void StratumClient::submit(const std::string& jobId,const std::string& e2,const std::string& ntime,const std::string& nonce){
    if(sock<0) return; std::ostringstream oss;
    oss<<R"({"id":4,"method":"mining.submit","params":[")"<<jobId<<R"(","")"<<e2<<R"(","")"<<ntime<<R"(","")"<<nonce<<R"("]})";
    sendLine(oss.str());
}
void StratumClient::close(){ if(sock>=0) ::close(sock); sock=-1; }
std::string StratumClient::readLine(){ std::string s; char ch; while(true){ ssize_t r=::recv(sock,&ch,1,0); if(r<=0) return ""; if(ch=='\n') break; s.push_back(ch);} return s; }
void StratumClient::sendLine(const std::string& s){ std::string t=s+"\n"; ::send(sock,t.data(),t.size(),0); }
