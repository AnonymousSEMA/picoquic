/*
* Author: Christian Huitema
* Copyright (c) 2023, Private Octopus, Inc.
* All rights reserved.
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL Private Octopus, Inc. BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef H3ZERO_COMMON_H
#define H3ZERO_COMMON_H

#include "picosplay.h"
#include "h3zero.h"

#ifdef __cplusplus
extern "C" {
#endif

    /* Define the per URL callback used to implement POST and other
    * REST-like interactions
    */
    typedef enum {
        picohttp_callback_get, /* Received a get command */
        picohttp_callback_post, /* Received a post command */
        picohttp_callback_connecting, /* Sending out a connect command */
        picohttp_callback_connect, /* Received a connect command */
        picohttp_callback_connect_refused, /* Connection request was refused by peer */
        picohttp_callback_connect_accepted, /* Connection request was accepted by peer */
        picohttp_callback_post_data, /* Data received from peer on stream N */
        picohttp_callback_post_fin, /* All posted data have been received on this stream */
        picohttp_callback_provide_data, /* Stack is ready to send chunk of data on stream N */
        picohttp_callback_post_datagram, /* Datagram received on this context */
        picohttp_callback_provide_datagram, /* Ready to send datagram in this context */
        picohttp_callback_reset, /* Stream has been abandoned by peer. */
        picohttp_callback_deregister, /* Context has been deregistered */
        picohttp_callback_free
    } picohttp_call_back_event_t;

    struct st_picohttp_server_stream_ctx_t;

    typedef int (*picohttp_post_data_cb_fn)(picoquic_cnx_t* cnx,
        uint8_t* bytes, size_t length,
        picohttp_call_back_event_t fin_or_event,
        struct st_picohttp_server_stream_ctx_t* stream_ctx,
        void * path_app_ctx);

    /* Define the table of special-purpose paths used for POST, REST, or connect queries */
    /* TODO: is there a need for path context? */
    typedef struct st_picohttp_server_path_item_t {
        char* path;
        size_t path_length;
        picohttp_post_data_cb_fn path_callback;
        void* path_app_ctx;
    } picohttp_server_path_item_t;

    /* Define stream context common to http 3 and http 09 callbacks
    */
#define PICOHTTP_SERVER_FRAME_MAX 1024

    typedef enum {
        picohttp_server_stream_status_none = 0,
        picohttp_server_stream_status_header,
        picohttp_server_stream_status_crlf,
        picohttp_server_stream_status_receiving,
        picohttp_server_stream_status_finished
    } picohttp_server_stream_status_t;

    typedef struct st_picohttp_server_stream_ctx_t {
        /* TODO-POST: identification of URL to process POST or GET? */
        /* TODO-POST: provide content-type */
        picosplay_node_t http_stream_node;
        picoquic_cnx_t* cnx;
        unsigned int is_h3:1;
        unsigned int is_upgraded:1;
        union {
            h3zero_data_stream_state_t stream_state; /* h3 only */
            struct {
                picohttp_server_stream_status_t status; 
                int proto; 
                uint8_t* path; 
                size_t path_length;
                size_t command_length;
                int method;
            } hq; /* h09 only */
        } ps; /* Protocol specific state */
        uint64_t stream_id;
        /* Server state file management */
        uint64_t response_length;
        uint64_t echo_length;
        uint64_t echo_sent;
        uint64_t post_received;
        /* Client state file management */
        unsigned int is_open : 1;
        unsigned int is_file_open : 1;
        unsigned int flow_opened : 1;
        uint64_t received_length;
        uint64_t post_size;
        uint64_t post_sent;
        char* f_name;
        /* Global state variables */
        uint8_t frame[PICOHTTP_SERVER_FRAME_MAX];
        char* file_path;
        FILE* F;
        /* Callback processing -- handling of POST and of Web Transport */
        uint64_t control_stream_id;
        picohttp_post_data_cb_fn path_callback;
        void* path_callback_ctx;
    } picohttp_server_stream_ctx_t;

    void* picohttp_stream_node_value(picosplay_node_t* node);
    void h3zero_init_stream_tree(picosplay_tree_t* h3_stream_tree);

    /* Handling of capsules */
#define H3ZERO_CAPSULE_HEADER_SIZE_MAX 16
    typedef struct st_h3zero_capsule_t {
        uint8_t header_buffer[H3ZERO_CAPSULE_HEADER_SIZE_MAX];
        size_t header_length;
        size_t header_read;
        size_t value_read;
        size_t capsule_buffer_size;
        uint64_t capsule_type;
        size_t capsule_length;
        uint8_t* capsule;
        unsigned int is_length_known:1;
        unsigned int is_stored;
    } h3zero_capsule_t;

    void h3zero_release_capsule(h3zero_capsule_t* capsule);

    const uint8_t* h3zero_accumulate_capsule(const uint8_t* bytes, const uint8_t* bytes_max, h3zero_capsule_t* capsule);

    /* handling of setting frames */
    uint8_t* h3zero_settings_encode(uint8_t* bytes, const uint8_t* bytes_max, const h3zero_settings_t* settings);
    const uint8_t* h3zero_settings_decode(const uint8_t* bytes, const uint8_t* bytes_max, h3zero_settings_t* settings);

    /* Handling of stream prefixes, for applications that use it.
     */
    typedef struct st_h3zero_stream_prefix_t {
        struct st_h3zero_stream_prefix_t* next;
        struct st_h3zero_stream_prefix_t* previous;
        uint64_t prefix;
        unsigned int ready_to_send_datagrams : 1;
        picohttp_post_data_cb_fn function_call;
        void* function_ctx;
    } h3zero_stream_prefix_t;

    typedef struct st_h3zero_stream_prefixes_t {
        struct st_h3zero_stream_prefix_t* first;
        struct st_h3zero_stream_prefix_t* last;
    } h3zero_stream_prefixes_t;

    int h3zero_protocol_init(picoquic_cnx_t* cnx);

    /* CLIENT DEFINITIONS 
     */
    int h3zero_client_create_stream_request(
        uint8_t * buffer, size_t max_bytes, uint8_t const * path, size_t path_len, uint64_t post_size, const char * host, size_t * consumed);

    /* Common callback definitions */
    typedef struct st_picohttp_server_parameters_t {
        char const* web_folder;
        picohttp_server_path_item_t* path_table;
        size_t path_table_nb;
    } picohttp_server_parameters_t;

    typedef struct st_h3zero_callback_ctx_t {
        picosplay_tree_t h3_stream_tree;
        picohttp_server_path_item_t * path_table;
        size_t path_table_nb;
        char const* web_folder;
        /* connection wide tracking of stream prefixes */
        h3zero_stream_prefixes_t stream_prefixes;
        uint64_t last_datagram_prefix;
        /* Flag  and variables used by clients*/
        unsigned int no_disk : 1;
        unsigned int no_print : 1;
        unsigned int connection_closed : 1;
        int nb_open_streams;
        int nb_open_files;
        uint32_t nb_client_streams;
    } h3zero_callback_ctx_t;

    h3zero_callback_ctx_t* h3zero_callback_create_context(picohttp_server_parameters_t* param);
    void h3zero_callback_delete_context(picoquic_cnx_t* cnx, h3zero_callback_ctx_t* ctx);

    int h3zero_post_data_or_fin(picoquic_cnx_t* cnx, uint8_t* bytes, size_t length, picoquic_call_back_event_t fin_or_event, picohttp_server_stream_ctx_t* stream_ctx);

    void h3zero_delete_stream(picoquic_cnx_t * cnx, h3zero_callback_ctx_t* ctx, picohttp_server_stream_ctx_t* stream_ctx);
    
    picohttp_server_stream_ctx_t* h3zero_find_stream(h3zero_callback_ctx_t* ctx, 
        uint64_t stream_id);
    
    picohttp_server_stream_ctx_t* h3zero_find_or_create_stream(
        picoquic_cnx_t* cnx,
        uint64_t stream_id,
        h3zero_callback_ctx_t* ctx,
        int should_create,
        int is_h3);

    uint8_t* h3zero_parse_incoming_remote_stream(
        uint8_t* bytes, uint8_t* bytes_max,
        picohttp_server_stream_ctx_t* stream_ctx,
        h3zero_callback_ctx_t* ctx);

    void h3zero_forget_stream(picoquic_cnx_t* cnx, picohttp_server_stream_ctx_t* stream_ctx);

    int h3zero_set_datagram_ready(picoquic_cnx_t* cnx, uint64_t stream_id);
    uint8_t* h3zero_provide_datagram_buffer(void* context, size_t length, int ready_to_send);

    int h3zero_callback(picoquic_cnx_t* cnx,
        uint64_t stream_id, uint8_t* bytes, size_t length,
        picoquic_call_back_event_t fin_or_event, void* callback_ctx, void* v_stream_ctx);

    h3zero_stream_prefix_t* h3zero_find_stream_prefix(h3zero_callback_ctx_t* ctx, uint64_t prefix);
    int h3zero_declare_stream_prefix(h3zero_callback_ctx_t* ctx, uint64_t prefix, picohttp_post_data_cb_fn function_call, void* function_ctx);
    void h3zero_delete_stream_prefix(picoquic_cnx_t * cnx, h3zero_callback_ctx_t* ctx, uint64_t prefix);
    void h3zero_delete_all_stream_prefixes(picoquic_cnx_t* cnx, h3zero_callback_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* H3ZERO_COMMON_H */