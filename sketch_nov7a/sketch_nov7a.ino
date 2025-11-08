/*
   ESP32 Wi-Fi Scanner + Web Page Viewer
   Works on ESP32 DevKit V1 with zero extra parts.
   The board creates its own Wi-Fi network called ESP32_Scanner.
   Connect your phone or PC to that Wi-Fi and visit:  http://192.168.4.1
*/

#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);  // create web server on port 80

// --- HTML page helpers ---
String htmlHeader() {
  return "<!DOCTYPE html><html><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>ESP32 Wi-Fi Scanner</title>"
         "<style>body{font-family:sans-serif;margin:20px;}"
         "table{border-collapse:collapse;width:100%;}"
         "th,td{border:1px solid #ccc;padding:6px;text-align:left;}"
         "th{background:#f2f2f2;}</style></head><body><h2>ESP32 Wi-Fi Scanner</h2>";
}

String htmlFooter() {
  return "<p style='color:#666;font-size:12px'>Passive scan — shows only visible networks. Use responsibly.</p></body></html>";
}

// --- Build the network table ---
String buildNetworkTable() {
  String s = "<table><tr><th>#</th><th>SSID</th><th>RSSI(dBm)</th><th>Channel</th><th>Encryption</th></tr>";
  int n = WiFi.scanNetworks();
  if (n == 0) {
    s += "<tr><td colspan='5'>No networks found</td></tr>";
  } else {
    for (int i = 0; i < n; i++) {
      s += "<tr><td>" + String(i + 1) + "</td>";
      s += "<td>" + WiFi.SSID(i) + "</td>";
      s += "<td>" + String(WiFi.RSSI(i)) + "</td>";
      s += "<td>" + String(WiFi.channel(i)) + "</td>";
      s += "<td>" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted") + "</td></tr>";
    }
  }
  s += "</table>";
  return s;
}


// --- HTTP handlers ---
void handleRoot() {
  String page = htmlHeader();
  page += "<p><a href='/rescan'>Rescan Wi-Fi</a></p>";
  page += buildNetworkTable();
  page += htmlFooter();
  server.send(200, "text/html", page);
}

void handleRescan() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true); // asynchronous scan
  server.sendHeader("Location", "/");
  server.send(303);  // redirect
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.mode(WIFI_AP_STA);       // act as Access Point + STA
  WiFi.softAP("ESP32_Scanner"); // create open network
  Serial.print("Access Point started. Connect to 'ESP32_Scanner' and visit ");
  Serial.println(WiFi.softAPIP());

  WiFi.scanNetworks(true); // start initial async scan

  server.on("/", handleRoot);
  server.on("/rescan", handleRescan);
  server.begin();

  pinMode(2, OUTPUT);  // onboard LED
  digitalWrite(2, HIGH);
}

void loop() {
  server.handleClient();
}
