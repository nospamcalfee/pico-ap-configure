/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"


#include "picow_tcp.h"

#define TEST_ITERATIONS 10
#define POLL_TIME_S 5

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb); //defn
#if 0
static void dump_bytes(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;

    printf("dump_bytes %d", len);
    for (i = 0; i < len;) {
        if ((i & 0x0f) == 0) {
            printf("\n");
        } else if ((i & 0x07) == 0) {
            printf(" ");
        }
        printf("%02x ", bptr[i++]);
    }
    printf("\n");
}
#define DUMP_BYTES dump_bytes
#else
#define DUMP_BYTES(A,B)
#endif

/*
 * These functions are protocol specific clients, called from generic client routines
 */
/*
 * This little function tries to start a send. It may fail mainly due to
 * ERR_MEM memory pressure. In that case, it will be retried by the
 * tcp_client_poll function.
 */
static err_t tcp_client_sending(void *arg, struct tcp_pcb *pcb) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    // If we have received the whole buffer, send it back to the server
    if (state->recv_len == BUF_SIZE) {
        DEBUG_printf("tcp_client_sending %d bytes to server\n", state->recv_len);
        err_t err = tcp_write(pcb, state->buffer, state->recv_len, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            state->status = err;
            DEBUG_printf("tcp_client_sending Failed to write data %d\n", err);
            if (err == ERR_MEM) {
                return ERR_OK; //wait for memory, will be called again by poll
            }
            return tcp_client_result(state, err);
        }
    }
    return ERR_OK;
}

err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("tcp_client_poll stat=%d\n", state->status);
    if (state->status == ERR_MEM) {
        state->user_sending(state, tpcb); //retry send
    }
    return ERR_OK;
}

err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("tcp_client_sent %u\n", len);
    state->sent_len += len;

    if (state->sent_len >= BUF_SIZE) {

        state->count++;
        if (state->count >= TEST_ITERATIONS) {
            tcp_client_result(arg, 0);
            return ERR_OK;
        }

        // We should receive a new buffer from the server
        state->recv_len = 0;
        state->sent_len = 0;
        DEBUG_printf("Waiting for buffer from server\n");
    }

    return ERR_OK;
}

err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (!p) {
        return tcp_client_result(arg, ERR_USER);
    }
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (p->tot_len > 0) {
        DEBUG_printf("tcp_client_recv %d err %d\n", p->tot_len, err);
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            DUMP_BYTES(q->payload, q->len);
        }
        // Receive the buffer
        const uint16_t buffer_left = BUF_SIZE - state->recv_len;
        state->recv_len += pbuf_copy_partial(p, state->buffer + state->recv_len,
                                               p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);

    return state->user_sending(state, tpcb);
}

/* every client protocol will have a custom open call.
   The mainloop only knows to start it up, every so often.
*/
static uint8_t buffer[BUF_SIZE];
static int fail_count;

bool tcp_client_sendtest_open(void *arg, const char *hostname, uint16_t port,
                            complete_callback completed_callback) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    //fixme don't start unless I know the earlier call is complete.
    if (state->busy) {
        if (fail_count++ > 10) {
            //if something happened,
            printf("clear the busy flag to restart\n");
            state->busy = false;
        } else {
            return false; //could not open, last operation is in progress
        }
    }
    return tcp_client_open(arg, hostname, port, buffer,
                            tcp_client_recv,
                            tcp_client_sent,
                            tcp_client_sending,
                            tcp_client_poll,
                            tcp_client_connected,
                            completed_callback);
}
