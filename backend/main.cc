#include <drogon/drogon.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

const char* getEnvString(const char *name, const char *def) {
    const char *val = std::getenv(name);
    return val == nullptr ? def : val;
}

int getEnvInteger(const char *name, int def) {
    const char *string = std::getenv(name);
    if(string == nullptr) return def;
    return std::stoi(string);
}

void* getSockInAddr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET) return &(((struct sockaddr_in*)sa)->sin_addr);
    else return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

std::string getIpFromHostname(const char *hostname) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(hostname, nullptr, &hints, &res) != 0)
        return std::string(hostname); // if this fails, just try hostname anyways

    char ip[INET6_ADDRSTRLEN];
    inet_ntop(res->ai_family, getSockInAddr((struct sockaddr*) res->ai_addr), ip, sizeof(ip));

    freeaddrinfo(res);
    return std::string(ip);
}

int main() {
    drogon::app().addListener("0.0.0.0", 8000)
    .createRedisClient(getIpFromHostname(getEnvString("REDIS_HOSTNAME", "0.0.0.0")), getEnvInteger("REDIS_PORT", 6379))
    .setDocumentRoot("assets")
    .setImplicitPageEnable(true)
    .setGzipStatic(true)
    .setBrStatic(true)
    .setStaticFilesCacheTime(0) // always keep static files in memory
    .setThreadNum(getEnvInteger("NUM_THREADS", 4))
    .setMaxConnectionNum(1000000)
    .run();
    return 0;
}
