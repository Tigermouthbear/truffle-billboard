#include "backend.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpTypes.h>
#include <drogon/WebSocketConnection.h>
#include <drogon/nosql/RedisResult.h>
#include <drogon/utils/Utilities.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <json/writer.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdio.h>
#include <vector>

#define DEFAULT_CONFIG "{\"interval\":30,\"groups\":[]}"
#define CLEANUP_INTERVAL 30

#ifndef NDEBUG
#define DEBUG_PRINTF(...) { \
    std::cout << "[DEBUG] "; \
    std::fprintf(stdout, __VA_ARGS__); \
    std::cout << std::endl; \
}
#else
#define DEBUG_PRINTF(...) {}
#endif

using namespace trufflebb;

static std::string getOrgId(const std::string &accessToken) {
    size_t first = accessToken.find('.');
    size_t last = accessToken.find_last_not_of('.');
    if(first == std::string::npos || last == std::string::npos || first == last) return "";

    std::string payload = utils::base64Decode(accessToken.substr(first + 1, last - first));

    Json::Value root;
    Json::Reader reader;
    if(!reader.parse(payload, root) || !root.isObject()) return "";

    Json::Value orgId = root["orgId"];
    if(!orgId.isString()) return "";

    return orgId.asString();
}

static void isAdmin(const std::string &orgId, const std::string &accessToken, const std::function<void(bool)>& callback) {
    static HttpClientPtr client = HttpClient::newHttpClient("https://mycelium.staging.bio");

    HttpRequestPtr req = HttpRequest::newHttpRequest();
    req->setMethod(Post);
    req->setPath("/graphql");
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->addHeader("x-org-id", orgId);
    req->addHeader("x-access-token", accessToken);
    req->setBody(R"({"query":"query RoleConnectionQuery ($input: RoleConnectionInput, $first: Int, $after: String, $last: Int, $before: String) { roleConnection(input: $input, first: $first, after: $after, last: $last, before: $before) { totalCount nodes { slug }}}","variables":{"input":{"orgId":")" + orgId + R"("}}})");

    client->sendRequest(req, [callback](ReqResult result, const HttpResponsePtr &response) {
        if(result == ReqResult::Ok) {
            std::shared_ptr<Json::Value> root = response->jsonObject();
            if(root == nullptr || !root->isObject()) {
                callback(false);
                return;
            }

            Json::Value data = (*root)["data"];
            if(!data.isObject()) {
                callback(false);
                return;
            }

            Json::Value roleConnection = data["roleConnection"];
            if(!roleConnection.isObject()) {
                callback(false);
                return;
            }

            Json::Value nodes = roleConnection["nodes"];
            if(!nodes.isArray()) {
                callback(false);
                return;
            }

            for(Json::Value role: nodes) {
                if(!role.isObject()) continue;

                Json::Value slug = role["slug"];
                if(!slug.isString()) continue;

                if(slug.asString() == "admin") {
                    callback(true);
                    return;
                }
            }
        }

        callback(false);
    });
}

void Api::handleNewConnection(const HttpRequestPtr &req, const WebSocketConnectionPtr &conn) {
    optional<std::string> accessTokenOptional = req->getOptionalParameter<std::string>("token");
    if(!accessTokenOptional) {
        conn->forceClose();
        return;
    }

    std::string accessToken = accessTokenOptional.value();
    std::string orgId = getOrgId(accessToken);
    if(orgId == "") {
        conn->forceClose();
        return;
    }

    Session session = { orgId, accessToken };
    conn->setContext(std::make_shared<Session>(std::move(session)));

    std::string config;
    {
        std::shared_lock guard(billboardsMutex);

        std::shared_ptr<Billboard> billboard;
        std::unordered_map<std::string, std::shared_ptr<Billboard>>::iterator it = billboards.find(orgId);
        if(it != billboards.end()) billboard = it->second;
        else {
            guard.unlock();
            {
                std::unique_lock guard(billboardsMutex);

                // try again in case billboard set by another thread during transfer to unique lock
                it = billboards.find(orgId);
                if(it != billboards.end()) billboard = it->second;
                else {
                    billboard = std::make_shared<Billboard>();
                    billboards[orgId] = billboard;
                }
            }
            guard.lock();
        }

        {
            std::unique_lock guard(billboard->connectionsMutex);
            billboard->connections.push_back(conn);
        }

        bool stale;
        {
            std::shared_lock guard(billboard->configMutex);
            stale = billboard->stale; 
            if(!stale) config = billboard->config;
        } 

        if(stale) {
            std::unique_lock guard(billboard->configMutex);

            billboard->config = config = drogon::app().getRedisClient()->execCommandSync<std::string>(
                [orgId](const nosql::RedisResult &r) {
                    std::string config = r.type() == nosql::RedisResultType::kString ? r.asString() : DEFAULT_CONFIG;

                    if(r.type() != nosql::RedisResultType::kString) {
                        drogon::app().getRedisClient()->execCommandAsync(
                            [](const nosql::RedisResult &r){},
                            [](const std::exception &err){},
                            "SET config:%s %s", orgId.c_str(), config.c_str()
                        );
                    }

                    return config;
                },
                "GET config:%s", orgId.c_str()
            );

            billboard->stale = false;
        }
    }

    conn->send(R"({"type":"update","config":)" + config + "}");
}

void Api::handleNewMessage(const WebSocketConnectionPtr &conn, std::string &&message, const WebSocketMessageType &type) {
    std::shared_ptr<Session> session = conn->getContext<Session>();
    if(session == nullptr) {
        conn->forceClose();
        return;
    }

    isAdmin(session->orgId, session->accessToken, [this, conn, session, message](bool admin) {
        if(!admin) {
            conn->forceClose(); // non admins should not be sending messages
            return;
        }

        Json::Value root;
        Json::Reader reader;
        if(!reader.parse(message, root) || !root.isObject()) return;

        Json::Value type = root["type"];
        if(!type.isString()) return;

        if(type.asString() == "update") {
            Json::Value config = root["config"];
            if(!config.isObject()) return;

            Json::FastWriter writer;
            drogon::app().getRedisClient()->execCommandAsync(
                [](const nosql::RedisResult &r){},
                [](const std::exception &err){},
                "SET config:%s %s", session->orgId.c_str(), writer.write(config).c_str()
            );
        }

        {
            std::shared_lock guard(billboardsMutex);
            std::shared_ptr<Billboard> billboard = billboards[session->orgId];

            {
                std::shared_lock guard(billboard->connectionsMutex);

#ifndef NDEBUG
                const auto& start = std::chrono::high_resolution_clock::now();
#endif // !NDEBUG

                for(WebSocketConnectionPtr conn: billboard->connections) {
                    if(conn == nullptr || conn->disconnected()) continue;
                    conn->send(message);
                }

#ifndef NDEBUG
                const auto& stop = std::chrono::high_resolution_clock::now();
                DEBUG_PRINTF("Sent about %ld updates in %ld milliseconds", billboard->connections.size(), std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
#endif // !NDEBUG
            }
        }
    });
}

void Api::handleConnectionClosed(const WebSocketConnectionPtr &conn) {
    conn->clearContext();
}

Api::Api(): cleanupThread([this]() {
    while(true) {
        for(int i = 0; i < CLEANUP_INTERVAL * 10; ++i) {
            {
                std::shared_lock guard(cleanupMutex);
                if(!cleaning) return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::shared_lock guard(billboardsMutex);
        std::unordered_map<std::string, std::shared_ptr<Billboard>>::iterator it;
        for(it = billboards.begin(); it != billboards.end(); ++it) {
            std::shared_ptr<Billboard> billboard = it->second;
            std::unique_lock guard(billboard->connectionsMutex);

#ifndef NDEBUG
            long before = billboard->connections.size();
#endif // !NDEBUG

            billboard->connections.erase(std::remove_if(
                billboard->connections.begin(),
                billboard->connections.end(),
                [](WebSocketConnectionPtr conn) { return conn == nullptr || conn->disconnected(); }
            ), billboard->connections.end());

#ifndef NDEBUG
            long after = billboard->connections.size();
            DEBUG_PRINTF("%ld connections cleaned, %ld connections remaining", before - after, after);
#endif // !NDEBUG
        }
    }
}) {
}

Api::~Api() {
    {
        std::unique_lock guard(cleanupMutex);
        cleaning = false;
    }

    cleanupThread.join();
}
