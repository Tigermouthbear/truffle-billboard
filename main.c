#include <stdio.h>

#include "mongoose.h"

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if(ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        struct mg_http_serve_opts opts = {.root_dir = "web"};
        mg_http_serve_dir(c, hm, &opts);
    }
}

int main() {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, "http://localhost:8000", fn, &mgr);
    for(;;) mg_mgr_poll(&mgr, 1000);
    return 0;
}
