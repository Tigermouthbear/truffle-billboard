#include "backend.h"

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpTypes.h>
#include <drogon/WebSocketConnection.h>
#include <drogon/nosql/RedisResult.h>
#include <drogon/utils/Utilities.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <json/writer.h>

#define SESSION_KEY "SESSION"
#define DEFAULT_CONFIG "{\"interval\":30,\"groups\":[]}"

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

void Auth::authenticate(const HttpRequestPtr &req, std::function<void (const HttpResponsePtr&)> &&callback) {
    std::string accessToken = std::string(req->body());
    std::string orgId = getOrgId(accessToken);

    HttpResponsePtr resp = HttpResponse::newHttpResponse();

    if(orgId == "") {
        resp->setStatusCode(HttpStatusCode::k401Unauthorized);
    } else {
        req->session()->insert(SESSION_KEY, Session { orgId, accessToken });
        resp->setStatusCode(HttpStatusCode::k200OK);
    }

    callback(resp);
}

void Api::handleNewConnection(const HttpRequestPtr &req, const WebSocketConnectionPtr &conn) {
    if(!req->session()->find(SESSION_KEY)) conn->forceClose();
    else {
        Session session = req->session()->get<Session>(SESSION_KEY);

        drogon::app().getRedisClient()->execCommandAsync(
            [session, conn, req](const nosql::RedisResult &r) {
                std::string config = r.type() == nosql::RedisResultType::kString ? r.asString() : DEFAULT_CONFIG;
                if(r.type() != nosql::RedisResultType::kString) {
                    drogon::app().getRedisClient()->execCommandAsync(
                        [](const nosql::RedisResult &r){},
                        [](const std::exception &err){},
                        "SET config:%s %s", session.orgId.c_str(), config.c_str()
                    );
                }

                isAdmin(session.orgId, session.accessToken, [config, conn, req](bool admin) { 
                    conn->send(R"({"type":"update","admin":)" + (std::string) (admin ? "true" : "false") + R"(,"config":)" + config + "}");

                    // little bit of cleanup to save on resources. only admins need their
                    // session to last while ws connected because of the editor
                    if(!admin) req->session()->erase(SESSION_KEY);
                });
            },
            [](const std::exception &err) {
                std::cerr << err.what();
            },
            "GET config:%s", session.orgId.c_str()
        );

        connections.try_emplace(session.orgId);
        connections[session.orgId].push_back(conn);

        conn->setContext(std::make_shared<Session>(std::move(session)));
    }
}

void Api::handleNewMessage(const WebSocketConnectionPtr &conn, std::string &&message, const WebSocketMessageType &type) {
    std::shared_ptr<Session> session = conn->getContext<Session>();
    if(session == nullptr) return;

    isAdmin(session->orgId, session->accessToken, [this, session, message](bool admin) {
        if(!admin) return;

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

        for(WebSocketConnectionPtr conn: connections[session->orgId])
            conn->send(message);
    });
}

void Api::handleConnectionClosed(const WebSocketConnectionPtr &conn) {
    std::shared_ptr<Session> session = conn->getContext<Session>();
    if(session == nullptr) return;

    std::vector<WebSocketConnectionPtr> vec = connections[session->orgId];
    for(std::vector<WebSocketConnectionPtr>::iterator iter = vec.begin(); iter != vec.end(); ++iter) {
        if(*iter == conn) {
            vec.erase(iter);
            break;
        }
    }
}
