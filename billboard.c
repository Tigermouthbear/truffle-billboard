#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <hiredis.h>
#include <curl/curl.h>

#include "mongoose.h"
#include "hashmap.h"

#define SESSION_TOKEN_LEN 64
#define SESSION_MAX_AGE 864000 // expire session in 10 days

struct bb_conn { // holds websocket state
    struct mg_connection  *c;
    char *org_id;
    bool is_admin;

    // used for bb_conn_list
    struct bb_conn *prev;
    struct bb_conn *next;

    struct bb_conn_list *list;// current list its in
};

struct bb_conn_list {
    struct bb_conn *head;
};

void conn_list_add(struct bb_conn_list *conn_list, struct bb_conn *conn) {
    if(conn_list->head== NULL) {
        conn_list->head = conn;
    } else {
        conn->next = conn_list->head;
        conn_list->head->prev = conn;
        conn_list->head = conn;
    }
}

void conn_list_remove(struct bb_conn_list *conn_list, struct bb_conn *conn) {
    if(conn->prev != NULL) conn->prev->next = conn->next;
    if(conn->next != NULL) conn->next->prev = conn->prev;
    if(conn == conn_list->head) conn_list->head = conn->next;
}

struct bb_server {
    bool running;
    redisContext *redis;

    struct hashmap_s conn_map; // mg_connection -> bb_conn
    struct hashmap_s org_map; // org_id -> bb_conn_list
} server = {
    .running = true
};

struct bb_buf {
    char *ptr;
    size_t len;
};

size_t req_write_callback(void *buf, size_t size, size_t nmemb, void *data) {
    struct bb_buf *str = (struct bb_buf*) data;
    size_t read_size = size * nmemb;

    char *ptr = realloc(str->ptr, str->len + read_size + 1);
    if(ptr == NULL) {
        fprintf(stderr, "req_write_callback failed to realloc\n");
        return 0;
    }

    str->ptr = ptr;
    memcpy(&(str->ptr[str->len]), buf, read_size);
    str->len += read_size;
    str->ptr[str->len] = 0;

    return read_size;
}

char* mycelium_request(char *data, char *access_token, char *org_id) {
    CURL *curl = curl_easy_init();
    if(curl == NULL) {
        fprintf(stderr, "curl_easy_init failed\n");
        return NULL;
    }

    char *access_token_header = mg_mprintf("x-access-token: %s", access_token);
    char *org_id_header = mg_mprintf("x-org-id: %s", org_id);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, access_token_header);
    headers = curl_slist_append(headers, org_id_header);

    free(access_token_header);
    free(org_id_header);

    curl_easy_setopt(curl, CURLOPT_URL, "https://mycelium.staging.bio/graphql");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/8.0.1");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

    struct bb_buf resp;
    resp.ptr = calloc(1, sizeof(char));
    resp.len = 0;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*) &resp);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, req_write_callback);

    CURLcode ret = curl_easy_perform(curl);
    if(ret != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform failed: %s\n", curl_easy_strerror(ret));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    return resp.ptr;
}

char* jwt_get_payload(char *jwt) {
    char *start = jwt;
    while(*start != '.' && *start != 0) start++;
    if(*start == 0) return NULL;

    char* end = ++start;
    while(*end != '.' && *end != 0) end++;
    if(*end == 0) return NULL;

    size_t size = end - start;
    char *buf = malloc(size * 4 / 3 + 5); // At least 4 chars and nul termination
    if(buf == NULL) return NULL;

    mg_base64_decode(start, size, buf);
    return buf;
}

char* get_org_id(char* access_token) {
    char *payload = jwt_get_payload(access_token);
    if(payload == NULL) return NULL;

    char *org_id = mg_json_get_str(mg_str(payload), "$.orgId");
    free(payload);
    return org_id;
}

bool mycelium_is_admin(char* access_token) { 
    char* org_id = get_org_id(access_token);
    if(org_id == NULL) return false;

    // this is a long query lol, graphql moment
    char *query = mg_mprintf("{\"query\":\"query RoleConnectionQuery ($input: RoleConnectionInput, $first: Int, $after: String, $last: Int, $before: String) { roleConnection(input: $input, first: $first, after: $after, last: $last, before: $before) { totalCount nodes { isSuperAdmin }}}\",\"variables\":{\"input\":{\"orgId\":%Q}}}", org_id);
    char *resp = mycelium_request(query, access_token, org_id);
    if(resp == NULL) {
        free(query);
        free(org_id);
        return false;
    }

    struct mg_str resp_str = mg_str(resp);
    long roles_len = mg_json_get_long(resp_str, "$.data.roleConnection.totalCount", -1);

    bool is_super_admin = false;
    if(roles_len > 0) {
        for(long i = 0; i < roles_len; i++) {
            char* path = mg_mprintf("$.data.roleConnection.nodes[%d].isSuperAdmin", i);
            mg_json_get_bool(resp_str, path, &is_super_admin);
            free(path);

            if(is_super_admin) break;
        }
    }

    free(resp);
    free(query);
    free(org_id);

    return is_super_admin;
}

bool is_reply_ok(redisReply *reply) {
    return reply->type == REDIS_REPLY_STATUS && reply->len == 2
        && reply->str[0] == 'O' && reply->str[1] == 'K';
}

bool set_access_token(char *org_id, char *session_token, char *access_token) {
    redisReply *reply = redisCommand(server.redis, "GET %s", session_token);
    if(reply == NULL) return false;

    if(reply->type != REDIS_REPLY_STRING || reply->len < 1) {
        freeReplyObject(reply);
        return false;
    }

    char expires[reply->len + 1];
    memcpy(expires, reply->str, reply->len);
    expires[reply->len] = 0;
    freeReplyObject(reply);

    reply = redisCommand(server.redis, "SET session:%s:%s %s EXAT %s", org_id, session_token, access_token, expires);
    if(reply == NULL) return false;

    bool ok = is_reply_ok(reply);
    freeReplyObject(reply);
    return ok;
}

char* get_access_token(char *org_id, char *session_token) {
    redisReply *reply = redisCommand(server.redis, "GET session:%s:%s", org_id, session_token);
    if(reply == NULL) return NULL;

    if(reply->type != REDIS_REPLY_STRING || reply->len < 1) {
        freeReplyObject(reply);
        return NULL;
    }

    char* access_token = malloc(reply->len + 1);
    if(access_token == NULL) {
        freeReplyObject(reply);
        return NULL;
    }

    memcpy(access_token, reply->str, reply->len);
    access_token[reply->len] = 0;

    freeReplyObject(reply);
    return access_token;
}

bool generate_session_token(time_t expires, char *session_token/*[SESSION_TOKEN_LEN + 1]*/) {
    redisReply *reply;

    for(;;) {
        mg_random_str(session_token, SESSION_TOKEN_LEN + 1);

        reply = redisCommand(server.redis, "GET %s", session_token);
        if(reply == NULL) return false;

        bool nil = reply->type == REDIS_REPLY_NIL;
        freeReplyObject(reply);
        if(nil) break;
    }

    reply = redisCommand(server.redis, "SET %s %lld EXAT %lld", session_token, expires, expires);
    if(reply == NULL) return false;

    bool ok = is_reply_ok(reply);
    freeReplyObject(reply);
    return ok;
}

bool get_session_token(struct mg_http_message *hm, char *session_token/*[SESSION_TOKEN_LEN + 1]*/) {
    struct mg_str *cookie = mg_http_get_header(hm, "Cookie");
    if(cookie == NULL) return false;

    struct mg_str str = mg_http_get_header_var(*cookie, mg_str("SESSION_TOKEN"));
    if(str.len != SESSION_TOKEN_LEN) return false;

    memcpy(session_token, str.ptr, str.len);
    session_token[str.len] = 0;
    return true;
}

char* get_billboard_config(char *org_id) {
    redisReply *reply = redisCommand(server.redis, "GET config:%s", org_id);
    if(reply == NULL) return NULL;

    if(reply->type != REDIS_REPLY_STRING || reply->len < 1) {
        freeReplyObject(reply);
        return NULL;
    }

    char *config = malloc(reply->len + 1);
    if(config == NULL) {
        freeReplyObject(reply);
        return NULL;
    }

    memcpy(config, reply->str, reply->len);
    config[reply->len] = 0;

    return config;
}

bool set_billboard_config(char* org_id, char* config) {
    redisReply *reply = redisCommand(server.redis, "SET config:%s %b", org_id, config, strlen(config));
    if(reply == NULL) return false;

    bool ok = is_reply_ok(reply);
    freeReplyObject(reply);
    return ok;
}

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if(ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message*) ev_data;

        if(mg_http_match_uri(hm, "/auth")) {
            char access_token[hm->body.len + 1];
            memcpy(access_token, hm->body.ptr, hm->body.len);
            access_token[hm->body.len] = 0;

            char *org_id = get_org_id(access_token);
            if(org_id == NULL) {
                mg_http_reply(c, 400, NULL, "Bad Request");
                return;
            }

            char *headers = NULL;
            char session_token[SESSION_TOKEN_LEN + 1];
            if(!get_session_token(hm, session_token)) {
                time_t expires;
                time(&expires);
                expires += SESSION_MAX_AGE;

                if(!generate_session_token(expires, session_token)) {
                    mg_http_reply(c, 500, NULL, "Internal Server Error");
                    return;
                }

                char expires_str[64];
                struct tm *timeinfo = localtime(&expires);
                strftime(expires_str, sizeof(expires_str), "%a, %d %b %Y %H:%M:%S %Z", timeinfo);

                headers = mg_mprintf("Set-Cookie: SESSION_TOKEN=%s; HttpOnly; Expires=%s\r\n", session_token, expires_str);
            }

            if(!set_access_token(org_id, session_token, access_token)) {
                mg_http_reply(c, 500, NULL, "Internal Server Error");
                if(headers != NULL) free(headers);
                free(org_id);
                return;
            }

            mg_http_reply(c, 200, headers, "OK");

            if(headers != NULL) free(headers);
            free(org_id);
        } else if(mg_http_match_uri(hm, "/api")) { 
            char session_token[SESSION_TOKEN_LEN + 1];
            bool has_session = get_session_token(hm, session_token);

            char org_id[64]; // org_id should be under 64 bytes
            size_t org_id_len = mg_http_get_var(&hm->query, "org", org_id, sizeof(org_id));
            if(org_id_len < 1) {
                mg_http_reply(c, 404, NULL, "Not Found");
                return;
            }

            bool is_admin = false;
            if(has_session) {
                char *access_token = get_access_token(org_id, session_token);
                if(access_token != NULL) {
                    is_admin = mycelium_is_admin(access_token);
                    free(access_token);
                }
            }

            struct bb_conn_list *conn_list = hashmap_get(&server.org_map, org_id, org_id_len);
            if(conn_list == NULL) {
                conn_list = calloc(1, sizeof(struct bb_conn_list)); // this will be freed at exit during hashmap_iterate_free_conn_list
                if(conn_list == NULL) {
                    mg_http_reply(c, 500, NULL, "Internal Server Error");
                    return;
                }

                hashmap_put(&server.org_map, org_id, org_id_len, conn_list);
            }

            struct bb_conn *conn = calloc(1, sizeof(struct bb_conn));
            if(conn == NULL) {
                mg_http_reply(c, 500, NULL, "Internal Server Error");
                return;
            }

            conn->org_id = malloc(org_id_len + 1);
            if(conn->org_id == NULL) {
                free(conn);
                mg_http_reply(c, 500, NULL, "Internal Server Error");
                return;
            }

            memcpy(conn->org_id, org_id, org_id_len);
            conn->org_id[org_id_len] = 0;

            conn->c = c;
            conn->list = conn_list;
            conn->is_admin = is_admin;

            mg_ws_upgrade(c, hm, NULL);
            if(!c->is_websocket) {
                free(conn->org_id);
                free(conn);
                return;
            }

            hashmap_put(&server.conn_map, c, sizeof(struct mg_connection*), conn);
            conn_list_add(conn_list, conn);

            // send current billboard data
            char *config = get_billboard_config(conn->org_id);
            mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{\"type\":\"update\",\"admin\":%s,\"config\":%s}",
                    is_admin ? "true" : "false",
                    config == NULL ? "undefined" : config);
            if(config != NULL) free(config);
        } else {
            struct mg_http_serve_opts opts = {.root_dir = "web"};
            mg_http_serve_dir(c, hm, &opts);
        }
    } else if(ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;

        struct bb_conn *conn = hashmap_get(&server.conn_map, c, sizeof(struct mg_connection*));
        if(conn == NULL || !conn->is_admin || conn->list == NULL) return;

        char *type = mg_json_get_str(wm->data, "$.type");
        if(type != NULL) {
            if(strcmp(type, "update") == 0) {
                int length;
                int offset = mg_json_get(wm->data, "$.config", &length);
                if(offset >= 0) {
                    char config[length + 1];
                    memcpy(config, wm->data.ptr + offset, length);
                    config[length] = 0;

                    set_billboard_config(conn->org_id, config);
                }
            }

            free(type);
        }

        // just forward all messages to all clients
        struct bb_conn *it = conn->list->head;
        while(it != NULL) {
            mg_ws_send(it->c, wm->data.ptr, wm->data.len, WEBSOCKET_OP_TEXT);
            it = it->next;
        }
    } else if(ev == MG_EV_CLOSE) {
        if(!c->is_websocket) return;

        struct bb_conn *conn = hashmap_get(&server.conn_map, c, sizeof(struct mg_connection*));
        if(conn == NULL) return;

        conn_list_remove(conn->list, conn);

        free(conn->org_id);
        free(conn);
        hashmap_remove(&server.conn_map, c, sizeof(struct mg_connection*));
    }
}

static void stop_server(int signo) {
    server.running = false;
}

static int hashmap_iterate_free_conn(void* const context, void* const value) {
    struct bb_conn *conn = (struct bb_conn*) value;
    free(conn->org_id);
    free(conn);
    return 1;
}

static int hashmap_iterate_free_conn_list(void* const context, void* const value) {
    struct bb_conn_list *conn_list = (struct bb_conn_list*) value;
    free(conn_list);
    return 1;
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, stop_server);
    signal(SIGTERM, stop_server);

    server.redis = redisConnect("localhost", 6379);
    if(server.redis == NULL) {
        fprintf(stderr, "Can't allocate redis context\n");
        return 1;
    } else if(server.redis->err) {
        fprintf(stderr, "Error connecting to redis: %s\n", server.redis->errstr);
        redisFree(server.redis);
        return 1;
    }

    const unsigned hashmap_initial_size = 2;
    if(hashmap_create(hashmap_initial_size, &server.conn_map) != 0) {
        fprintf(stderr, "Failed to create connection hashmap\n");
        redisFree(server.redis);
        return 1;
    } else if(hashmap_create(hashmap_initial_size, &server.org_map) != 0) {
        fprintf(stderr, "Failed to create org hashmap\n");
        hashmap_destroy(&server.conn_map);
        redisFree(server.redis);
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, "http://localhost:8000", fn, &mgr);
    while(server.running) mg_mgr_poll(&mgr, 1000);

    curl_global_cleanup();
    hashmap_iterate(&server.conn_map, hashmap_iterate_free_conn, NULL);
    hashmap_iterate(&server.org_map, hashmap_iterate_free_conn_list, NULL);
    hashmap_destroy(&server.conn_map);
    hashmap_destroy(&server.org_map);
    redisFree(server.redis);

    return 0;
}
