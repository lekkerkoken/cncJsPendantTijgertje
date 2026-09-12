#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class NetworkManager
{
public:

    void begin();

    void startWiFiConnection();
    bool updateWiFiConnection();
    bool wifiConnected() const;

    void advanceWiFiNetwork();

    bool resolve(
        const char* host,
        uint16_t port,
        IPAddress& address
    );

    void resetAuthentication();

    bool startAuthentication(
        const IPAddress& address,
        uint16_t port,
        const char* host
    );

    bool updateAuthentication(
        String& token
    );

    bool authenticationStarted() const;
    bool authenticated() const;

private:

    WiFiClient authClient;

    String authResponse;
    String authHeaderBuffer;

    bool authRequestSent = false;
    bool authHeadersReceived = false;
    int authContentLength = -1;

    bool authenticatedState = false;

    String token_;

    size_t currentWiFiNetwork_ = 0;

    bool extractAuthenticationBody(
        String& body
    );
};

#endif