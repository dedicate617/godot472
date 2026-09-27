/*
 * MindSCADA Transparent Network Tunnel (C Version - Option A)
 * High-performance WebSocket <-> Raw TCP bridge using Mongoose.
 * 
 * Supports:
 *   Tunnel 1: ws://0.0.0.0:8083 -> tcp://127.0.0.1:1883 (MQTT)
 *   Tunnel 2: ws://0.0.0.0:4841 -> tcp://127.0.0.1:4840 (OPC-UA)
 */

#include "mongoose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

typedef struct {
    char listen_url[64];
    char target_url[64];
    const char *name;
} tunnel_route_t;

typedef struct {
    tunnel_route_t *route;
    struct mg_connection *ws_client;
    struct mg_connection *backend;
} tunnel_session_t;

static tunnel_route_t g_routes[] = {
    {"http://0.0.0.0:8083", "tcp://127.0.0.1:1883", "MQTT Tunnel (WS:8083 -> TCP:1883)"},
    {"http://0.0.0.0:4841", "tcp://127.0.0.1:4840", "OPC-UA Tunnel (WS:4841 -> TCP:4840)"},
};

#define NUM_ROUTES (sizeof(g_routes) / sizeof(g_routes[0]))

// Callback for the backend raw TCP socket
static void tcp_backend_fn(struct mg_connection *c, int ev, void *ev_data) {
    tunnel_session_t *s = (tunnel_session_t *)c->fn_data;

    if (ev == MG_EV_CONNECT) {
        if (s != NULL && s->route != NULL) {
            MG_INFO(("[%s] Backend TCP connection established to %s", s->route->name, s->route->target_url));
        }
    } else if (ev == MG_EV_READ) {
        // Received raw bytes from MQTT broker / OPC-UA server
        if (s != NULL && s->ws_client != NULL && s->ws_client->is_websocket) {
            mg_ws_send(s->ws_client, c->recv.buf, c->recv.len, WEBSOCKET_OP_BINARY);
            mg_iobuf_del(&c->recv, 0, c->recv.len);
        }
    } else if (ev == MG_EV_CLOSE) {
        // Backend socket closed; disconnect browser client
        if (s != NULL) {
            s->backend = NULL;
            if (s->ws_client != NULL) {
                s->ws_client->is_closing = 1;
            }
        }
        c->fn_data = NULL;
    }
}

// Callback for the frontend WebSocket connection
static void ws_listen_fn(struct mg_connection *c, int ev, void *ev_data) {
    if (c->is_listening) {
        return;
    }

    if (ev == MG_EV_OPEN) {
        // Inbound connection accepted from listener:
        // c->fn_data initially points to the listener's route
        tunnel_route_t *route = (tunnel_route_t *)c->fn_data;
        tunnel_session_t *s = (tunnel_session_t *)calloc(1, sizeof(tunnel_session_t));
        if (s != NULL) {
            s->route = route;
            s->ws_client = c;
            s->backend = NULL;
            c->fn_data = s;
        } else {
            c->is_closing = 1;
        }
        return;
    }

    tunnel_session_t *s = (tunnel_session_t *)c->fn_data;

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        // Upgrade incoming HTTP request to WebSocket (Mongoose automatically echoes Sec-WebSocket-Protocol)
        mg_ws_upgrade(c, hm, NULL);
    } else if (ev == MG_EV_WS_OPEN) {
        // WebSocket handshake completed: connect to target raw TCP server
        if (s != NULL && s->route != NULL) {
            s->backend = mg_connect(c->mgr, s->route->target_url, tcp_backend_fn, s);
            if (s->backend != NULL) {
                MG_INFO(("[%s] Client connected, bridged to %s", s->route->name, s->route->target_url));
            } else {
                MG_ERROR(("[%s] Failed to connect to backend: %s", s->route->name, s->route->target_url));
                c->is_closing = 1;
            }
        }
    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
        if (s != NULL && s->backend != NULL) {
            mg_send(s->backend, wm->data.buf, wm->data.len);
        }
    } else if (ev == MG_EV_CLOSE) {
        if (s != NULL) {
            if (s->backend != NULL) {
                s->backend->fn_data = NULL;
                s->backend->is_closing = 1;
                s->backend = NULL;
            }
            free(s);
            c->fn_data = NULL;
        }
    }
}

int main(int argc, char *argv[]) {
    size_t i;
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_log_set(MG_LL_INFO);

    printf("===============================================================\n");
    printf("MindSCADA Transparent Network Tunnel Gateway (C / Mongoose)\n");
    printf("===============================================================\n");

    for (i = 0; i < NUM_ROUTES; i++) {
        struct mg_connection *listener = mg_http_listen(&mgr, g_routes[i].listen_url, ws_listen_fn, &g_routes[i]);
        if (listener == NULL) {
            fprintf(stderr, "[ERROR] Cannot bind %s\n", g_routes[i].listen_url);
        } else {
            printf("[RUNNING] %s\n", g_routes[i].name);
        }
    }
    printf("Listening for Web browser connections. Press Ctrl+C to exit.\n\n");

    for (;;) {
        mg_mgr_poll(&mgr, 50);
    }

    mg_mgr_free(&mgr);
    return 0;
}
