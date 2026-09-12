#include "NetworkManager.h"

#include <ArduinoJson.h>

#include "Secrets.h"


// ============================================================
// BEGIN
// ============================================================

void NetworkManager::begin()
{
    resetAuthentication();

    currentWiFiNetwork_ = 0;
}


// ============================================================
// WIFI START
// ============================================================

void NetworkManager::startWiFiConnection()
{
    Serial.println();

    Serial.print(
        "[WiFi] Trying network "
    );

    Serial.print(
        currentWiFiNetwork_ + 1
    );

    Serial.print(
        "/"
    );

    Serial.print(
        WIFI_NETWORK_COUNT
    );

    Serial.print(
        ": "
    );

    Serial.println(
        WIFI_NETWORKS[currentWiFiNetwork_].ssid
    );

    WiFi.mode(
        WIFI_STA
    );

    WiFi.disconnect(
        false
    );

    WiFi.begin(
        WIFI_NETWORKS[currentWiFiNetwork_].ssid,
        WIFI_NETWORKS[currentWiFiNetwork_].password
    );
}


// ============================================================
// WIFI UPDATE
// ============================================================

bool NetworkManager::updateWiFiConnection()
{
    if (
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        return false;
    }

    Serial.print(
        "[WiFi] Connected: "
    );

    Serial.println(
        WiFi.localIP()
    );

    return true;
}


// ============================================================
// WIFI STATUS
// ============================================================

bool NetworkManager::wifiConnected() const
{
    return WiFi.status() ==
           WL_CONNECTED;
}


// ============================================================
// WIFI ADVANCE
// ============================================================

void NetworkManager::advanceWiFiNetwork()
{
    if (
        WIFI_NETWORK_COUNT == 0
    )
    {
        return;
    }

    currentWiFiNetwork_++;

    if (
        currentWiFiNetwork_ >=
        WIFI_NETWORK_COUNT
    )
    {
        currentWiFiNetwork_ = 0;
    }

    Serial.print(
        "[WiFi] Next network: "
    );

    Serial.print(
        currentWiFiNetwork_ + 1
    );

    Serial.print(
        "/"
    );

    Serial.print(
        WIFI_NETWORK_COUNT
    );

    Serial.print(
        ": "
    );

    Serial.println(
        WIFI_NETWORKS[currentWiFiNetwork_].ssid
    );
}


// ============================================================
// DNS RESOLVE
// ============================================================

bool NetworkManager::resolve(
    const char* host,
    uint16_t port,
    IPAddress& address
)
{
    Serial.println();

    Serial.print(
        "[Network] Resolving "
    );

    Serial.print(
        host
    );

    Serial.print(
        ":"
    );

    Serial.println(
        port
    );

    if (
        WiFi.hostByName(
            host,
            address
        ) != 1
    )
    {
        Serial.println(
            "[Network] DNS/mDNS resolution failed"
        );

        return false;
    }

    Serial.print(
        "[Network] Resolved to: "
    );

    Serial.println(
        address
    );

    return true;
}


// ============================================================
// AUTHENTICATION RESET
// ============================================================

void NetworkManager::resetAuthentication()
{
    authClient.stop();

    authResponse =
        "";

    authHeaderBuffer =
        "";

    authRequestSent =
        false;

    authHeadersReceived =
        false;

    authContentLength =
        -1;

    authenticatedState =
        false;

    token_ =
        "";
}


// ============================================================
// AUTHENTICATION START
// ============================================================

bool NetworkManager::startAuthentication(
    const IPAddress& address,
    uint16_t port,
    const char* host
)
{
    resetAuthentication();

    Serial.println();
    Serial.println(
        "=== CNCjs signin ==="
    );

    if (
        !authClient.connect(
            address,
            port
        )
    )
    {
        Serial.println(
            "[Network] Authentication TCP connection failed"
        );

        return false;
    }

    String body =
        "{\"token\":\"\"}";

    authClient.print(
        "POST /api/signin HTTP/1.1\r\n"
    );

    authClient.print(
        "Host: "
    );

    authClient.print(
        host
    );

    authClient.print(
        "\r\n"
    );

    authClient.print(
        "Content-Type: application/json\r\n"
    );

    authClient.print(
        "Content-Length: "
    );

    authClient.print(
        body.length()
    );

    authClient.print(
        "\r\n"
    );

    authClient.print(
        "Connection: close\r\n"
    );

    authClient.print(
        "\r\n"
    );

    authClient.print(
        body
    );

    authRequestSent =
        true;

    Serial.println(
        "[Network] Authentication request sent"
    );

    return true;
}


// ============================================================
// AUTHENTICATION UPDATE
// ============================================================

bool NetworkManager::updateAuthentication(
    String& token
)
{
    if (
        !authRequestSent
    )
    {
        return false;
    }

    while (
        authClient.available()
    )
    {
        char c =
            static_cast<char>(
                authClient.read()
            );

        authResponse +=
            c;

        if (
            !authHeadersReceived &&
            authResponse.endsWith(
                "\r\n\r\n"
            )
        )
        {
            authHeadersReceived =
                true;

            int contentLengthPosition =
                authResponse.indexOf(
                    "Content-Length:"
                );

            if (
                contentLengthPosition >= 0
            )
            {
                int lineEnd =
                    authResponse.indexOf(
                        "\r\n",
                        contentLengthPosition
                    );

                if (
                    lineEnd >= 0
                )
                {
                    String line =
                        authResponse.substring(
                            contentLengthPosition,
                            lineEnd
                        );

                    int colon =
                        line.indexOf(
                            ':'
                        );

                    if (
                        colon >= 0
                    )
                    {
                        authContentLength =
                            line.substring(
                                colon + 1
                            ).toInt();
                    }
                }
            }
        }
    }

    if (
        !authHeadersReceived
    )
    {
        return false;
    }

    String body;

    if (
        !extractAuthenticationBody(
            body
        )
    )
    {
        return false;
    }

    JsonDocument doc;

    DeserializationError error =
        deserializeJson(
            doc,
            body
        );

    if (
        error
    )
    {
        Serial.print(
            "[Network] Authentication JSON error: "
        );

        Serial.println(
            error.c_str()
        );

        authClient.stop();

        return false;
    }

    token_ =
        "";

    if (
        doc["token"].is<const char*>()
    )
    {
        token_ =
            doc["token"].as<String>();
    }
    else if (
        doc["accessToken"].is<const char*>()
    )
    {
        token_ =
            doc["accessToken"].as<String>();
    }

    if (
        token_.length() > 0
    )
    {
        authenticatedState =
            true;

        token =
            token_;

        authClient.stop();

        Serial.println(
            "[Network] JWT received"
        );

        return true;
    }

    Serial.println(
        "[Network] Authentication response contained no token"
    );

    authClient.stop();

    return false;
}


// ============================================================
// EXTRACT AUTHENTICATION BODY
// ============================================================

bool NetworkManager::extractAuthenticationBody(
    String& body
)
{
    int separator =
        authResponse.indexOf(
            "\r\n\r\n"
        );

    if (
        separator < 0
    )
    {
        return false;
    }

    int bodyStart =
        separator + 4;

    int bodyLength =
        authResponse.length() -
        bodyStart;

    if (
        authContentLength >= 0 &&
        bodyLength < authContentLength
    )
    {
        return false;
    }

    if (
        authContentLength >= 0
    )
    {
        body =
            authResponse.substring(
                bodyStart,
                bodyStart +
                authContentLength
            );
    }
    else
    {
        body =
            authResponse.substring(
                bodyStart
            );
    }

    return body.length() > 0;
}


// ============================================================
// AUTHENTICATION STATUS
// ============================================================

bool NetworkManager::authenticationStarted() const
{
    return authRequestSent;
}


bool NetworkManager::authenticated() const
{
    return authenticatedState;
}
