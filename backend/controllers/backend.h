#pragma once

#include <drogon/HttpController.h>
#include <drogon/WebSocketConnection.h>
#include <drogon/WebSocketController.h>

#include <vector>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

using namespace drogon;

namespace trufflebb {
    struct Session {
        std::string orgId;
        std::string accessToken;
    };

    struct Billboard {
        std::shared_mutex connectionsMutex;
        std::vector<WebSocketConnectionPtr> connections;

        std::shared_mutex configMutex;
        std::string config;
        bool stale = true;
    };

    class Api: public drogon::WebSocketController<Api> {
        private:
            std::shared_mutex billboardsMutex;
            std::unordered_map<std::string, std::shared_ptr<Billboard>> billboards;

            std::shared_mutex cleanupMutex;
            std::thread cleanupThread;
            bool cleaning = true;

        public:
            Api();

            void handleNewConnection(const HttpRequestPtr &req, const WebSocketConnectionPtr &conn) override;
            void handleNewMessage(const WebSocketConnectionPtr &conn, std::string &&message, const WebSocketMessageType &type) override;
            void handleConnectionClosed(const WebSocketConnectionPtr &conn) override;

            ~Api();

            WS_PATH_LIST_BEGIN
            WS_PATH_ADD("/api");
            WS_PATH_LIST_END
    };
}
