#include "web.h"
#include "web_ui.h"          // INDEX_HTML
#include <ArduinoJson.h>

static uint16_t hex16(const String& s) { return (uint16_t)strtol(s.c_str(), nullptr, 16); }

void BridgeWeb::begin() {
    srv_.on("/status", HTTP_GET, [this]{ handleStatus(); });
    srv_.on("/connect", HTTP_POST, [this]{ handleConnect(); });
    srv_.on("/disconnect", HTTP_POST, [this]{ connected_ = false; srv_.send(200, "application/json", "{\"ok\":true}"); });
    srv_.on("/read", HTTP_GET, [this]{ handleRead(title::REQ_READ_RAM); });
    srv_.on("/read_rom", HTTP_GET, [this]{ handleRead(title::REQ_READ_ROM); });
    srv_.on("/snapshot", HTTP_GET, [this]{ handleSnapshot(); });
    srv_.on("/sniff", HTTP_GET, [this]{ handleSniff(); });
    srv_.on("/", HTTP_GET, [this]{ handleRoot(); });
    srv_.begin();
}

void BridgeWeb::handleStatus() {
    JsonDocument d;
    d["connected"] = connected_;
    d["kw1"] = ident_.kw1; d["kw2"] = ident_.kw2;
    d["part_number"] = ident_.part_number;
    d["baud"] = baud_;
    String out; serializeJson(d, out);
    srv_.send(200, "application/json", out);
}

void BridgeWeb::handleConnect() {
    if (srv_.hasArg("baud")) baud_ = srv_.arg("baud").toInt();
    ch_.set_baud(baud_);
    ch_.begin();
    uint8_t kw[2];
    KwpErr e = ch_.slow_init(0x01, kw);
    if (e != KwpErr::Ok) { srv_.send(200, "application/json", "{\"connected\":false,\"err\":\"init\"}"); return; }
    proto_.set_counter(0);
    ident_ = Ident{}; ident_.kw1 = kw[0]; ident_.kw2 = kw[1];
    e = client_.connect(ident_);
    connected_ = (e == KwpErr::Ok);
    handleStatus();
}

void BridgeWeb::handleRead(uint8_t req_title) {
    uint16_t addr = hex16(srv_.arg("addr"));
    uint8_t len = srv_.hasArg("len") ? (uint8_t)srv_.arg("len").toInt() : 1;
    std::vector<uint8_t> buf;
    KwpErr e = client_.read_mem(req_title, addr, len, buf);
    JsonDocument d;
    char a[8]; snprintf(a, sizeof(a), "%04X", addr);
    d["addr"] = a;
    JsonArray arr = d["bytes"].to<JsonArray>();
    for (uint8_t b : buf) arr.add(b);
    d["err"] = (e == KwpErr::Ok) ? "" : "read";
    String out; serializeJson(d, out);
    srv_.send(200, "application/json", out);
}

void BridgeWeb::handleSnapshot() {
    uint16_t start = hex16(srv_.arg("start"));
    uint16_t end = hex16(srv_.arg("end"));
    std::vector<std::pair<uint16_t, uint8_t>> vals;
    KwpErr e = client_.snapshot(start, end, vals);
    JsonDocument d;
    d["start"] = start; d["end"] = end;
    d["err"] = (e == KwpErr::Ok) ? "" : "snapshot";
    JsonArray arr = d["values"].to<JsonArray>();
    for (auto& kv : vals) arr.add(kv.second);
    String out; serializeJson(d, out);
    srv_.send(200, "application/json", out);
}

void BridgeWeb::handleSniff() {
    uint32_t ms = srv_.hasArg("ms") ? srv_.arg("ms").toInt() : 2000;
    static uint8_t buf[2048];
    int n = ch_.sniff(buf, sizeof(buf), ms);
    JsonDocument d;
    JsonArray arr = d["bytes"].to<JsonArray>();
    for (int i = 0; i < n; i++) arr.add(buf[i]);
    String out; serializeJson(d, out);
    srv_.send(200, "application/json", out);
}

void BridgeWeb::handleRoot() {
    srv_.send_P(200, "text/html", INDEX_HTML);
}
