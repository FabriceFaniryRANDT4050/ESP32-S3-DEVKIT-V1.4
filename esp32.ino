#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// --- Configuration Wi-Fi ---
const char* ssid     = "EspLink 2";
const char* password = "1234:56789";

// --- Nom d'hôte ---
// Permet d'accéder à l'ESP32 via http://esp32-rgb.local (mDNS) et de
// l'identifier facilement dans la liste des appareils connectés au routeur.
const char* hostname = "esp32-rgb";

// --- Configuration IP ---
// false (recommandé) : l'ESP32 obtient automatiquement son IP via DHCP,
//                       quel que soit le réseau auquel il se connecte.
// true               : force une IP fixe (utile seulement si votre routeur
//                       ne propose pas de réservation DHCP par adresse MAC).
#define USE_STATIC_IP false

#if USE_STATIC_IP
// ATTENTION : gateway et local_IP doivent être dans le même sous-réseau.
IPAddress local_IP(10, 190, 244, 200);
IPAddress gateway(10, 190, 244, 1);
IPAddress subnet(255, 255, 255, 0);
#endif

// --- Configuration Hardware ---
#define RGB_LED_PIN 48 // Si la LED ne s'allume pas, remplacez par 38

// Serveur Web sur le port 80
WebServer server(80);

unsigned long lastPrintTime = 0;

void setLEDColor(int r, int g, int b) {
  neopixelWrite(RGB_LED_PIN, r, g, b);
}

void handleRoot() {
  String html = R"html(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3 RGB Controller</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }
        body { background: #121212; color: #fff; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }
        .card { background: #1e1e1e; padding: 30px; border-radius: 20px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); text-align: center; max-width: 400px; width: 100%; }
        h1 { font-size: 1.5rem; margin-bottom: 20px; color: #e0e0e0; }
        .color-preview { width: 100px; height: 100px; border-radius: 50%; margin: 0 auto 25px; border: 4px solid #333; transition: all 0.3s ease; box-shadow: 0 0 20px rgba(255,255,255,0.1); }
        .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; margin-bottom: 20px; }
        .btn { padding: 14px; font-size: 1rem; border: none; border-radius: 12px; cursor: pointer; font-weight: bold; transition: transform 0.1s, opacity 0.2s; color: white; }
        .btn:active { transform: scale(0.95); }
        .btn-red { background: #ff3b30; }
        .btn-green { background: #34c759; }
        .btn-blue { background: #007aff; }
        .btn-yellow { background: #ffcc00; color: #000; }
        .btn-purple { background: #af52de; }
        .btn-cyan { background: #5ac8fa; color: #000; }
        .btn-white { background: #f2f2f7; color: #000; }
        .btn-off { background: #3a3a3c; grid-column: span 2; }
    </style>
</head>
<body>
    <div class="card">
        <h1>Contrôle LED RGB</h1>
        <div id="preview" class="color-preview"></div>
        <div class="grid">
            <button class="btn btn-red" onclick="setColor(255,0,0)">Rouge</button>
            <button class="btn btn-green" onclick="setColor(0,255,0)">Vert</button>
            <button class="btn btn-blue" onclick="setColor(0,0,255)">Bleu</button>
            <button class="btn btn-yellow" onclick="setColor(255,200,0)">Jaune</button>
            <button class="btn btn-purple" onclick="setColor(180,0,255)">Violet</button>
            <button class="btn btn-cyan" onclick="setColor(0,255,255)">Cyan</button>
            <button class="btn btn-white" onclick="setColor(50,50,50)">Blanc</button>
            <button class="btn btn-off" onclick="setColor(0,0,0)">Éteindre</button>
        </div>
    </div>

    <script>
        function setColor(r, g, b) {
            document.getElementById('preview').style.backgroundColor = `rgb(${r},${g},${b})`;
            document.getElementById('preview').style.boxShadow = `0 0 25px rgb(${r},${g},${b})`;
            fetch(`/set?r=${r}&g=${g}&b=${b}`);
        }
    </script>
</body>
</html>
  )html";
  server.send(200, "text/html", html);
}

void handleSetColor() {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    int r = server.arg("r").toInt();
    int g = server.arg("g").toInt();
    int b = server.arg("b").toInt();
    setLEDColor(r, g, b);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void setup() {
  Serial.begin(115200);
  setLEDColor(0, 0, 0);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);

#if USE_STATIC_IP
  // Configuration IP Statique (uniquement si USE_STATIC_IP == true)
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Erreur de configuration IP Statique !");
  }
#else
  Serial.println("Mode DHCP : l'IP sera attribuee automatiquement par le reseau.");
#endif

  Serial.print("Connexion au reseau : ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi connecte !");
  Serial.print("Adresse IP : http://");
  Serial.println(WiFi.localIP());
  Serial.print("Passerelle : ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Masque sous-reseau : ");
  Serial.println(WiFi.subnetMask());

  // --- mDNS : accès via un nom fixe, quelle que soit l'IP attribuee ---
  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.print("Accessible aussi via : http://");
    Serial.print(hostname);
    Serial.println(".local");
  } else {
    Serial.println("Erreur de demarrage mDNS");
  }

  server.on("/", handleRoot);
  server.on("/set", handleSetColor);

  server.begin();
  Serial.println("Serveur HTTP demarre.");
}

void loop() {
  server.handleClient();
  // Pas de MDNS.update() sur ESP32 : c'est géré automatiquement.

  if (millis() - lastPrintTime >= 1000) {
    lastPrintTime = millis();
    Serial.print("Acces HTTP : http://");
    Serial.print(WiFi.localIP());
    Serial.print("  ou  http://");
    Serial.print(hostname);
    Serial.println(".local");
  }
}
