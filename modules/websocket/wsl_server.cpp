/*************************************************************************/
/*  wsl_server.cpp                                                       */
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

#ifndef JAVASCRIPT_ENABLED

#include "wsl_server.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/string_utils.h"

using namespace eastl;

WSLServer::PendingPeer::PendingPeer() {
    use_ssl = false;
    time = 0;
    has_request = false;
    response_sent = 0;
    req_pos = 0;
    memset(req_buf, 0, sizeof(req_buf));
}

bool WSLServer::PendingPeer::_parse_request(const PoolVector<String> &p_protocols) {
    Vector<StringView> psa = StringUtils::split(StringView((const char *)req_buf),"\r\n");
    int len = psa.size();
    ERR_FAIL_COND_V_MSG(len < 4, false, "Not enough response headers, got: " + itos(len) + ", expected >= 4.");

    Vector<StringView> req = StringUtils::split(psa[0]," ", false);
    ERR_FAIL_COND_V_MSG(req.size() < 2, false, "Invalid protocol or status code.");

    // Wrong protocol
    ERR_FAIL_COND_V_MSG(req[0] != "GET"_sv || req[2] != "HTTP/1.1"_sv, false, "Invalid method or HTTP version.");

    Map<String, String> headers;
    for (int i = 1; i < len; i++) {
        Vector<StringView> header = StringUtils::split(psa[i],":", false, 1);
        ERR_FAIL_COND_V_MSG(header.size() != 2, false, String("Invalid header -> ") + psa[i]);
        String name = StringUtils::to_lower(header[0]);
        StringView value =StringUtils::strip_edges( header[1]);
        if (headers.contains(name))
            headers[name] += String(",") + value;
        else
            headers[name] = value;
    }
#define _WSL_CHECK(NAME, VALUE)                                                         \
    ERR_FAIL_COND_V_MSG(!headers.contains(NAME) || StringUtils::to_lower(headers[NAME]) != VALUE, false, \
            "Missing or invalid header '" + String(NAME) + "'. Expected value '" + VALUE + "'.");
#define _WSL_CHECK_EX(NAME) \
    ERR_FAIL_COND_V_MSG(!headers.contains(NAME), false, "Missing header '" + String(NAME) + "'.");
    _WSL_CHECK("upgrade", "websocket")
    _WSL_CHECK("sec-websocket-version", "13")
    _WSL_CHECK_EX("sec-websocket-key")
    _WSL_CHECK_EX("connection")
#undef _WSL_CHECK_EX
#undef _WSL_CHECK
    key = headers["sec-websocket-key"];
    if (headers.contains("sec-websocket-protocol")) {
        Vector<StringView> protos = StringUtils::split(headers["sec-websocket-protocol"],",");
        for (int i = 0; i < protos.size(); i++) {
            auto proto = StringUtils::strip_edges(protos[i]);
            // Check if we have the given protocol
            for (int j = 0; j < p_protocols.size(); j++) {
                if (proto != StringView(p_protocols[j]))
                    continue;
                protocol = proto;
                break;
            }
            // Found a protocol
            if (!protocol.empty())
                break;
        }
        if (protocol.empty()) // Invalid protocol(s) requested
            return false;
    } else if (p_protocols.size() > 0) // No protocol requested, but we need one
        return false;
    return true;
}

Error WSLServer::PendingPeer::do_handshake(const PoolVector<String> & p_protocols) {
    if (OS::get_singleton()->get_ticks_msec() - time > WSL_SERVER_TIMEOUT)
        return ERR_TIMEOUT;
    if (use_ssl) {
        Ref<StreamPeerSSL> ssl(static_cast<StreamPeerSSL *>(connection.get()));
        if (not ssl)
            return FAILED;
        ssl->poll();
        if (ssl->get_status() == StreamPeerSSL::STATUS_HANDSHAKING)
            return ERR_BUSY;
        else if (ssl->get_status() != StreamPeerSSL::STATUS_CONNECTED)
            return FAILED;
    }
    if (!has_request) {
        int read = 0;
        while (true) {
            ERR_FAIL_COND_V_MSG(req_pos >= WSL_MAX_HEADER_SIZE, ERR_OUT_OF_MEMORY, "Response headers too big.");
            Error err = connection->get_partial_data(&req_buf[req_pos], 1, read);
            if (err != OK) // Got an error
                return FAILED;
            else if (read != 1) // Busy, wait next poll
                return ERR_BUSY;
            char *r = (char *)req_buf;
            int l = req_pos;
            if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
                r[l - 3] = '\0';
                if (!_parse_request(p_protocols)) {
                    return FAILED;
                }
                String s = "HTTP/1.1 101 Switching Protocols\r\n";
                s += "Upgrade: websocket\r\n";
                s += "Connection: Upgrade\r\n";
                s += "Sec-WebSocket-Accept: " + WSLPeer::compute_key_response(key) + "\r\n";
                if (!protocol.empty())
                    s += "Sec-WebSocket-Protocol: " + protocol + "\r\n";
                s += "\r\n";
                response = s;
                has_request = true;
                break;
            }
            req_pos += 1;
        }
    }
    if (has_request && response_sent < response.size() - 1) {
        int sent = 0;
        Error err = connection->put_partial_data((const uint8_t *)response.data() + response_sent, response.size() - response_sent - 1, sent);
        if (err != OK) {
            return err;
        }
        response_sent += sent;
    }
    if (response_sent < response.size() - 1)
        return ERR_BUSY;
    return OK;
}

Error WSLServer::listen(int p_port, const PoolVector<String> &p_protocols, bool gd_mp_api) {
    ERR_FAIL_COND_V(is_listening(), ERR_ALREADY_IN_USE);

    _is_multiplayer = gd_mp_api;
    _is_multiplayer = gd_mp_api;
    // Strip edges from protocols.
    _protocols.resize(p_protocols.size());
    auto pw(_protocols.write());
    for (int i = 0; i < p_protocols.size(); i++) {
        pw[i] = StringUtils::strip_edges(p_protocols[i]);
    }

    _protocols.append_array(p_protocols);

    return _server->listen(p_port, bind_ip);
}

void WSLServer::poll() {

    for (auto iter=_peer_map.begin(); iter!=_peer_map.end(); ) {
        Ref<WSLPeer> peer((WSLPeer *)iter->second.get());
        peer->poll();
        if (!peer->is_connected_to_host()) {
            _on_disconnect(iter->first, peer->close_code != -1);
            iter=_peer_map.erase(iter);
        }
        else
            ++iter;
    }
    
    for (auto iter = _pending.begin(); iter!= _pending.end(); ) {
        Ref<PendingPeer> ppeer = *iter;
        Error err = ppeer->do_handshake(_protocols);
        if (err == ERR_BUSY) {
            continue;
        }
        if (err != OK) {
            iter=_pending.erase(iter);
            continue;
        }
        // Creating new peer
        int32_t id = _gen_unique_id();

        WSLPeer::PeerData *data = memnew(struct WSLPeer::PeerData);
        data->obj = this;
        data->conn = ppeer->connection;
        data->tcp = ppeer->tcp;
        data->is_server = true;
        data->id = id;

        Ref<WSLPeer> ws_peer(make_ref_counted<WSLPeer>());
        ws_peer->make_context(data, _in_buf_size, _in_pkt_size, _out_buf_size, _out_pkt_size);
        ws_peer->set_no_delay(true);

        _peer_map[id] = ws_peer;
        iter = _pending.erase(iter);
        _on_connect(id, ppeer->protocol);
    }

    if (!_server->is_listening())
        return;

    while (_server->is_connection_available()) {
        Ref<StreamPeerTCP> conn = _server->take_connection();
        if (is_refusing_new_connections())
            continue; // Conn will go out-of-scope and be closed.

        Ref<PendingPeer> peer(make_ref_counted<PendingPeer>());
        if (private_key && ssl_cert) {
            Ref<StreamPeerSSL> ssl = Ref<StreamPeerSSL>(StreamPeerSSL::create());
            ssl->set_blocking_handshake_enabled(false);
            ssl->accept_stream(conn, private_key, ssl_cert, ca_chain);
            peer->connection = ssl;
            peer->use_ssl = true;
        } else {
        peer->connection = conn;
        }
        peer->tcp = conn;
        peer->time = OS::get_singleton()->get_ticks_msec();
        _pending.push_back(peer);
    }
}

bool WSLServer::is_listening() const {
    return _server->is_listening();
}

int WSLServer::get_max_packet_size() const {
    return (1 << _out_buf_size) - PROTO_SIZE;
}

void WSLServer::stop() {
    _server->stop();
    for (eastl::pair<const int,Ref<WebSocketPeer> > &E : _peer_map) {
        Ref<WSLPeer> peer((WSLPeer *)E.second.get());
        peer->close_now();
    }
    _pending.clear();
    _peer_map.clear();
    _protocols = {};
}

bool WSLServer::has_peer(int p_id) const {
    return _peer_map.contains(p_id);
}

Ref<WebSocketPeer> WSLServer::get_peer(int p_id) const {
    ERR_FAIL_COND_V(!has_peer(p_id), Ref<WebSocketPeer>());
    return _peer_map.at(p_id);
}

IP_Address WSLServer::get_peer_address(int p_peer_id) const {
    ERR_FAIL_COND_V(!has_peer(p_peer_id), IP_Address());

    return _peer_map.at(p_peer_id)->get_connected_host();
}

int WSLServer::get_peer_port(int p_peer_id) const {
    ERR_FAIL_COND_V(!has_peer(p_peer_id), 0);

    return _peer_map.at(p_peer_id)->get_connected_port();
}

void WSLServer::disconnect_peer(int p_peer_id, int p_code, StringView p_reason) {
    ERR_FAIL_COND(!has_peer(p_peer_id));

    get_peer(p_peer_id)->close(p_code, p_reason);
}

Error WSLServer::set_buffers(int p_in_buffer, int p_in_packets, int p_out_buffer, int p_out_packets) {
    ERR_FAIL_COND_V_MSG(_server->is_listening(), FAILED, "Buffers sizes can only be set before listening or connecting.");

    _in_buf_size = nearest_shift(p_in_buffer - 1) + 10;
    _in_pkt_size = nearest_shift(p_in_packets - 1);
    _out_buf_size = nearest_shift(p_out_buffer - 1) + 10;
    _out_pkt_size = nearest_shift(p_out_packets - 1);
    return OK;
}

WSLServer::WSLServer() {
    _in_buf_size = nearest_shift(GLOBAL_GET(WSS_IN_BUF).as<int>() - 1) + 10;
    _in_pkt_size = nearest_shift(GLOBAL_GET(WSS_IN_PKT).as<int>() - 1);
    _out_buf_size = nearest_shift(GLOBAL_GET(WSS_OUT_BUF).as<int>() - 1) + 10;
    _out_pkt_size = nearest_shift(GLOBAL_GET(WSS_OUT_PKT).as<int>() - 1);
    _server = make_ref_counted<TCP_Server>();
}

WSLServer::~WSLServer() {
    stop();
}

#endif // JAVASCRIPT_ENABLED
