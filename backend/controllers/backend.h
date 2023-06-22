#pragma once

#include <drogon/HttpController.h>
#include <drogon/WebSocketConnection.h>
#include <drogon/WebSocketController.h>

#include <unordered_map>
#include <vector>

using namespace drogon;

namespace trufflebb {
    struct Session {
        std::string orgId;
        std::string accessToken;
    };

    class Api: public drogon::WebSocketController<Api> {
        private:
            std::unordered_map<std::string, std::vector<WebSocketConnectionPtr>> connections;

        public:
            void handleNewConnection(const HttpRequestPtr &req, const WebSocketConnectionPtr &conn) override;
            void handleNewMessage(const WebSocketConnectionPtr &conn, std::string &&message, const WebSocketMessageType &type) override;
            void handleConnectionClosed(const WebSocketConnectionPtr &conn) override;

            WS_PATH_LIST_BEGIN
            WS_PATH_ADD("/api");
            WS_PATH_LIST_END
    };
}
