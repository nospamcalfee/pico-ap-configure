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
/*
 * These functions are protocol specific clients and server functions.
 * called from generic client routines
 *
 */

//sample original userspace functions.
// local adjustments after the server accepts. If protocol demands, start a send.
static err_t tcp_server_test1_accept(void *arg, struct tcp_pcb *tpcb)
{
    (void)tpcb; //ignore arg
#if 1
    //example code where server sends on connect
    struct server_per_client *per_client = (struct server_per_client *)arg;
    // app specific send on connnect
    return per_client->parent->user_send(per_client, per_client->client_pcb);
#else
    (void)arg; //ignore arg
    return ERR_OK;
#endif
}

static err_t tcp_server_send_data(void *arg, struct tcp_pcb *tpcb)
{
    struct server_per_client *per_client = (struct server_per_client *)arg;
    //fixme this could be moved to a separate function.
    //fixme as now, it rebuilds the buffer on send retries
    for(int i=0; i< BUF_SIZE; i++) {
        per_client->buffer_sent[i] = rand();
    }

    per_client->sent_len = 0;
    DEBUG_printf("tcp_server_send_data writing %ld\n", BUF_SIZE);
    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    err_t err = tcp_write(tpcb, per_client->buffer_sent, BUF_SIZE, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        DEBUG_printf("tcp_server_send_data Failed to write data %d\n", err);
        per_client->status = err;
        if (err == ERR_MEM) {
            return ERR_OK; //wait for memory, will be called again by poll
        }
        //real errors exit here
        return tcp_server_result(per_client, err);
    }
    return ERR_OK;
}

//another app specific routine
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    struct server_per_client *per_client = (struct server_per_client *)arg;
    if ((err == ERR_OK || err == ERR_ABRT) && p == NULL) {
        // //remote client closed the connections, free up client stuff
        return tcp_server_result(per_client, err);
    }

    if ((err == ERR_OK || err == ERR_ABRT) && p != NULL) {
        //this callback means we have some data from the client

        // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
        // can use this method to cause an assertion in debug mode, if this method is called when
        // cyw43_arch_lwip_begin IS needed
        cyw43_arch_lwip_check();
        if (p->tot_len > 0) {
            DEBUG_printf("tcp_server_recv %d/%d err %d\n", p->tot_len, per_client->recv_len, err);

            // Receive the buffer
            const uint16_t buffer_left = BUF_SIZE - per_client->recv_len;
            per_client->recv_len += pbuf_copy_partial(p, per_client->buffer_recv + per_client->recv_len,
                                                 p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);
            tcp_recved(tpcb, p->tot_len);
        }
        pbuf_free(p);

        // Have we have received the whole buffer
        if (per_client->recv_len == BUF_SIZE) {

            // check it matches
            if (memcmp(per_client->buffer_sent, per_client->buffer_recv, BUF_SIZE) != 0) {
                DEBUG_printf("buffer mismatch\n");
                return tcp_server_result(per_client, ERR_USER);
            }
            DEBUG_printf("tcp_server_recv buffer ok\n");

            // Test completed?
            per_client->count++;
            if (per_client->count >= TEST_ITERATIONS) {
                tcp_server_result(per_client, 0);
                return ERR_OK;
            }

            // Send another buffer
            return per_client->parent->user_send(per_client, per_client->client_pcb);
        }
    } else {
        DEBUG_printf("tcp_server_recv some funny error condition, still free the pbuf\n");
        pbuf_free(p);
    }
    return ERR_OK;
}
err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct server_per_client *per_client = (struct server_per_client*) arg;
    DEBUG_printf("tcp_server_sent %u\n", len);
    per_client->sent_len += len;

    if (per_client->sent_len >= BUF_SIZE) {

        // We should get the data back from the client
        per_client->recv_len = 0;
        DEBUG_printf("tcp_server_sent Waiting for buffer\n");
    }

    return ERR_OK;
}

/* every client protocol will have a custom open call.
   The mainloop only knows to start it up, every so often.

   For this test, malloc the server, but never free it. frees cause
   fragmentation and stale pointers. Embedded wants predictable.
*/

err_t tcp_server_sendtest_init_open(uint16_t port,
                               complete_callback completed_callback) {
    TCP_SERVER_T *tcp_serv = tcp_server_init(NULL, MAX_TCPTEST1_CONNECTIONS);

    err_t err = tcp_server_open(tcp_serv, port,
                                tcp_server_recv,
                                tcp_server_sent,
                                tcp_server_send_data,
                                tcp_server_test1_accept,
                                completed_callback);
    return err;
}

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

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    DEBUG_printf("tcp_client_poll stat=%d\n", state->status);
    if (state->status == ERR_MEM) {
        state->user_sending(state, tpcb); //retry send
    }
    return ERR_OK;
}

static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
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

static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
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
// If I use client priv pointer, I must have a complete callback to cleanup
// only the priv area can be safely accessed
// must clean up after init, freeing memory.
static void tcp_client_complete(void *arg, int status) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    (void) status; //ignore status
    DEBUG_printf("tcp_client_complete\n");

    //fixme embedded guys hate free - it causes fragmentation. ALternative is
    //to reuse the allocated memory.
    free(state); //so multiple init/open cycles can work.
}

/* every client protocol will have a custom open call.
   The mainloop only knows to start it up, every so often.
*/
static uint8_t test1_buffer[BUF_SIZE];
static int fail_count;
static TCP_CLIENT_T *state;

err_t tcp_client_sendtest_init_open(char *hostname, uint16_t port,
                                    void *spriv) {
    state = tcp_client_init(spriv);
    //fixme don't start unless I know the earlier call is complete.
    if (state->busy) {
        if (fail_count++ > 10) {
            //if something happened,
            printf("clear the busy flag to restart\n");
            state->busy = false;
        } else {
            free(state); //clean up on failure
            return false; //could not open, last operation is in progress
        }
    }
    return tcp_client_open(state, hostname, port, test1_buffer,
                            tcp_client_recv,
                            tcp_client_sent,
                            tcp_client_sending,
                            tcp_client_poll,
                            tcp_client_connected,
                            tcp_client_complete);
}
