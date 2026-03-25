#include "http_client.hpp"
#include "config.hpp"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"

#include <cstdio>
#include <cstring>

namespace http
{

    static volatile bool g_dns_done = false;
    static ip_addr_t g_dns_ip;

    static ip_addr_t s_cached_ip;
    static bool s_ip_cached = false;

    struct RequestCtx
    {
        bool response_done = false;
        bool success = false;
        char request[512];
        uint16_t request_len = 0;
    };

    static void on_dns(const char *name, const ip_addr_t *addr, void *arg)
    {
        (void)name;
        (void)arg;
        if (addr != nullptr)
        {
            g_dns_ip = *addr;
        }
        g_dns_done = true;
    }

    static err_t on_connected(void *arg, struct tcp_pcb *pcb, err_t err)
    {
        if (err != ERR_OK)
            return err;

        auto *ctx = static_cast<RequestCtx *>(arg);
        tcp_write(pcb, ctx->request, ctx->request_len, TCP_WRITE_FLAG_COPY);
        tcp_output(pcb);
        return ERR_OK;
    }

    static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
    {
        auto *ctx = static_cast<RequestCtx *>(arg);

        if (p == nullptr)
        {
            ctx->response_done = true;
            return ERR_OK;
        }

        char buf[128];
        uint16_t len = (p->tot_len < sizeof(buf) - 1) ? p->tot_len : sizeof(buf) - 1;
        pbuf_copy_partial(p, buf, len, 0);
        buf[len] = '\0';

        if (strstr(buf, "202") != nullptr)
            ctx->success = true;

        tcp_recved(pcb, p->tot_len);
        pbuf_free(p);
        return ERR_OK;
    }

    static void on_error(void *arg, err_t err)
    {
        (void)err;
        auto *ctx = static_cast<RequestCtx *>(arg);
        ctx->response_done = true;
    }

    static bool poll_until(volatile bool &flag, uint32_t timeout_ms)
    {
        uint32_t start = to_ms_since_boot(get_absolute_time());
        while (!flag)
        {
            cyw43_arch_poll();
            sleep_ms(5);
            if ((to_ms_since_boot(get_absolute_time()) - start) > timeout_ms)
                return false;
        }
        return true;
    }

    bool post(const char *json_body, uint16_t body_len)
    {
        // Resolve DNS
        if (!s_ip_cached)
        {
            g_dns_done = false;
            err_t dns_err = dns_gethostbyname(config::BACKEND_HOST, &s_cached_ip, on_dns, nullptr);

            if (dns_err == ERR_INPROGRESS)
            {
                if (!poll_until(g_dns_done, 5000))
                {
                    printf("[HTTP] Timeout DNS\n");
                    return false;
                }
                s_cached_ip = g_dns_ip;
            }
            else if (dns_err != ERR_OK)
            {
                printf("[HTTP] Erro DNS: %d\n", dns_err);
                return false;
            }

            s_ip_cached = true;
            printf("[HTTP] DNS resolvido: %s\n", config::BACKEND_HOST);
        }

        // Monta request HTTP
        RequestCtx ctx{};
        ctx.request_len = static_cast<uint16_t>(snprintf(
            ctx.request, sizeof(ctx.request),
            "POST %s HTTP/1.1\r\n"
            "Host: %s:%u\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %u\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            config::BACKEND_PATH,
            config::BACKEND_HOST, config::BACKEND_PORT,
            body_len, json_body));

        // Conecta e envia
        struct tcp_pcb *pcb = tcp_new();
        if (pcb == nullptr)
        {
            printf("[HTTP] Falha ao criar PCB\n");
            return false;
        }

        tcp_arg(pcb, &ctx);
        tcp_recv(pcb, on_recv);
        tcp_err(pcb, on_error);

        if (tcp_connect(pcb, &s_cached_ip, config::BACKEND_PORT, on_connected) != ERR_OK)
        {
            printf("[HTTP] Erro TCP connect\n");
            tcp_close(pcb);
            return false;
        }

        if (!poll_until(ctx.response_done, 10000))
        {
            printf("[HTTP] Timeout resposta\n");
            tcp_abort(pcb);
            return false;
        }

        tcp_close(pcb);

        if (ctx.success)
            printf("[HTTP] 202 Accepted\n");
        else
            printf("[HTTP] Falha\n");

        return ctx.success;
    }

}
