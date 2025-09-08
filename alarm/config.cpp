#include "config.h"

// WLAN-Zugangsdaten
const char* ssid           = "your-ssid";
const char* wifi_password  = "s3cr3t-p4ssw0rd";

// Twitch-Zugangsdaten
const char* twitch_username = "user"; // Dein Twitch-Benutzername
const char* twitch_channel  = "#channel";          // Twitch Channel (mit führendem "#")

// OAuth / API
const char* twitch_client_id = "your-client-id";
const char* twitch_client_secret = "your-secret";
const char* twitch_refresh_token = "refresh-token";  // dauerhaft speichern

// Optional: leer lassen, wird automatisch gefüllt
String twitch_access_token = "";        // wird zur Laufzeit gesetzt