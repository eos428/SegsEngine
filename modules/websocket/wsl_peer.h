/*************************************************************************/
/*  wsl_peer.h                                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef WSLPEER_H
#define WSLPEER_H

#ifndef JAVASCRIPT_ENABLED

#include "core/error_list.h"
#include "core/io/packet_peer.h"
#include "core/io/stream_peer_tcp.h"
#include "core/ring_buffer.h"
#include "packet_buffer.h"
#include "websocket_peer.h"
#include "wslay/wslay.h"

#define WSL_MAX_HEADER_SIZE 4096

class WSLPeer : public WebSocketPeer {

    GDCIIMPL(WSLPeer, WebSocketPeer);

public:
    struct PeerData {
        bool polling;
        bool destroy;
        bool valid;
        bool is_server;
        bool closing;
        void *obj;
        void *peer;
        Ref<StreamPeer> conn;
        Ref<StreamPeerTCP> tcp;
        int id;
        wslay_event_context_ptr ctx;

        PeerData() {
            polling = false;
            destroy = false;
            valid = false;
            is_server = false;
            closing = false;
            id = 1;
            ctx = nullptr;
            obj = nullptr;
            peer = nullptr;
        }
    };

    static String compute_key_response(StringView p_key);
    static String generate_key();

private:
    static bool _wsl_poll(struct PeerData *p_data);
    static void _wsl_destroy(struct PeerData **p_data);

    struct PeerData *_data;
    uint8_t _is_string;
    // Our packet info is just a boolean (is_string), using uint8_t for it.
    PacketBuffer<uint8_t> _in_buffer;

    PoolVector<uint8_t> _packet_buffer;

    WriteMode write_mode;

public:
    int close_code;
    String close_reason;
    void poll(); // Used by client and server.

    int get_available_packet_count() const override;
    Error get_packet(const uint8_t **r_buffer, int &r_buffer_size) override;
    Error put_packet(const uint8_t *p_buffer, int p_buffer_size) override;
    int get_max_packet_size() const override { return _packet_buffer.size(); }

    virtual void close_now();
    void close(int p_code = 1000, StringView p_reason = "") override;
    bool is_connected_to_host() const override;
    IP_Address get_connected_host() const override;
    uint16_t get_connected_port() const override;

    WriteMode get_write_mode() const override;
    void set_write_mode(WriteMode p_mode) override;
    bool was_string_packet() const override;
    void set_no_delay(bool p_enabled) override;

    void make_context(PeerData *p_data, unsigned int p_in_buf_size, unsigned int p_in_pkt_size, unsigned int p_out_buf_size, unsigned int p_out_pkt_size);
    Error parse_message(const wslay_event_on_msg_recv_arg *arg);
    void invalidate();

    WSLPeer();
    ~WSLPeer() override;
};

#endif // JAVASCRIPT_ENABLED

#endif // LSWPEER_H
