#pragma once
#include <DNSServer.h>
#include <WebServer.h>

class CaptivePortal {
public:
    void begin();
    void handle();

private:
    DNSServer _dns;
    WebServer _http{80};

    void serveForm();
    void handleSave();
    void handleNotFound();
};

extern CaptivePortal portal;
