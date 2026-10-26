#pragma once
#include <WebServer.h>
#include "kline_channel.h"
#include "kwp_client.h"

class BridgeWeb {
public:
    BridgeWeb(WebServer& srv, KLineChannel& ch, Protocol& proto, KwpClient& client)
        : srv_(srv), ch_(ch), proto_(proto), client_(client) {}
    void begin();      // register routes
private:
    void handleStatus();
    void handleConnect();
    void handleRead(uint8_t req_title);
    void handleSnapshot();
    void handleSniff();
    void handleRoot();
    WebServer& srv_;
    KLineChannel& ch_;
    Protocol& proto_;
    KwpClient& client_;
    bool connected_ = false;
    Ident ident_;
    uint32_t baud_ = 10400;
};
