#pragma once
#include <string>
#include <functional>
struct Job {
    std::string jobId, prevHashHex, merklePrefixHex, versionHex, nbitsHex, ntimeHex;
    bool clean{false};
    std::string extranonce1Hex; int extranonce2Size{2};
    double difficulty{0.0};
};
class StratumClient {
public:
    using OnJob = std::function<void(const Job&)>;
    using OnLog = std::function<void(const std::string&)>;
    bool connect(const std::string& url,const std::string& user,const std::string& pass, OnJob onJob, OnLog onLog);
    void submit(const std::string& jobId,const std::string& e2,const std::string& ntime,const std::string& nonce);
    void close();
private:
    int sock{-1}; OnJob onJob; OnLog onLog;
    std::string readLine(); void sendLine(const std::string& s);
};
