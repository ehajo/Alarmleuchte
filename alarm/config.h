#pragma once
#include <Arduino.h>

// WLAN
extern const char* ssid;
extern const char* wifi_password;

// Twitch IRC
extern const char* twitch_username;     // z.B. "deinlogin"
extern const char* twitch_channel;      // z.B. "#deinlogin"

// OAuth / API
extern const char* twitch_client_id;
extern const char* twitch_client_secret;
extern const char* twitch_refresh_token;

// Laufzeit: wird im Code gesetzt
extern String twitch_access_token;
