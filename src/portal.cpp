#include "portal.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include "display.h"
#include "miner.h"
#include <time.h>   // pour getLocalTime, configTime

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include "version.h"  // si FW_VERSION n'est pas visible, mieux vaut le mettre dans un version.h
const char* GITHUB_VERSION_URL =
  "https://raw.githubusercontent.com/TON_USER/TON_REPO/main/firmware/version.txt";

const char* GITHUB_FIRMWARE_BASE_URL =
  "https://raw.githubusercontent.com/TON_USER/TON_REPO/main/firmware/";


// Décalage UTC en heures (-12 .. +12), par défaut 1 (Europe/Paris)
static int gUtcOffsetHours = 1;


// --- mesures DHT venant de main.cpp ---
extern float gTempC;
extern float gHum;

// =======================
// WiFi / Web
// =======================
static Preferences prefs;    // pour le WiFi uniquement
static WebServer server(80);

static String wifiSSID;
static String wifiPASS;

static bool configMode = false;

static const char* apSSID = "KON8_Config";
static const char* apPASS = "12345678";

static String ssidOptionsHTML;

// ---- Timezone / horloge ----
static String gTimezone = "Europe/Paris";

// =======================
// Helpers presentation
// =======================

// formatage Elapsed (en secondes) -> "Xj Yh Zm"
static String formatElapsed(const String &elapsedSecStr) {
  long sec = elapsedSecStr.toInt();
  if (sec <= 0) return String("N/A");

  long days = sec / 86400;
  sec %= 86400;
  long hours = sec / 3600;
  sec %= 3600;
  long minutes = sec / 60;

  String out;
  if (days > 0) {
    out += String(days) + "j ";
  }
  if (hours > 0) {
    out += String(hours) + "h ";
  }
  out += String(minutes) + "m";
  return out;
}




static String formatModeLabel(const String &mode) {
  if (mode == "eco")                 return "Eco 🌿";
  if (mode == "standard"
   || mode == "normal")             return "Standard ⚙️";
  if (mode == "super")               return "Super 🚀";
  return "Inconnu ❔";
}



static void loadTimeConfig() {
  prefs.begin("timecfg", true);
  gUtcOffsetHours = prefs.getInt("utcOffset", 1);  // défaut: +1h (Paris)
  prefs.end();
}

static void saveTimeConfig(int offsetHours) {
  prefs.begin("timecfg", false);
  prefs.putInt("utcOffset", offsetHours);
  prefs.end();
  gUtcOffsetHours = offsetHours;
}

static void applyTimeConfig() {
  int offset = gUtcOffsetHours;
  if (offset < -12) offset = -12;
  if (offset > 12) offset = 12;

  long gmtOffsetSec   = offset * 3600;
  long daylightOffset = 0;  // tu ne gères plus l’heure d’été ici, juste un décalage brut

  Serial.print("Apply UTC offset = ");
  Serial.println(offset);

  configTime(gmtOffsetSec, daylightOffset,
             "pool.ntp.org", "time.nist.gov");
}

static String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Heure indisponible";
  }

  char buf[32];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buf);
}




// =======================
// HTML PAGES
// =======================

static String htmlConfigPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Configuration WiFi</title>
  <link rel="icon" type="image/png" 
        href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAFkSURBVGhD7dhBDoIwEETRj/7/X9upsAo7EfxnSm/JSyHbHiSxME+k22yppWfQaSDeS2zwWu7gk7I+gSJJrDhBMS8n9JH7kA5IBOGBo8+cO9Lt5eYtQgkFH86hgPFqNgj7F/ySi1LszOp/9IvcF3lwMGkAszPSH8LiJOAUEgKqDsUQvYHx3M5AbWOVCq4wKZEgqN6Cxj8AyVqYpYQ6r5T2oFaqOoGBAaY8BJSpswEiY4uBQGmOAiYpYwAmZ4tAApnwAkKWMJImOFgIGmPAQEqbMBAmaLCwNApj0AQJmKWMJIuPBQGmOAgEqbMBAmaLCwNApgwNElp2/KN+YgUmBIHXEJ3sAAAAASUVORK5CYII="/>
  <style>
    body { font-family: Arial; text-align: center; background:#1b1b1b; color:white; }
    h1 { font-size: 28px; margin-top:20px; }
    h3 { font-size: 20px; }
    select, input[type=text], input[type=password] {
      width: 80%; padding: 12px; margin: 10px auto;
      border-radius: 10px; border: none; font-size:16px;
    }
    input[type=submit] {
      background:#00c853; color:white; width: 60%; padding:15px;
      border-radius: 10px; border:none; font-size:20px; margin-top:20px;
      cursor:pointer;
    }
    input[type=submit]:hover {
      opacity:0.9;
    }
    div { width: 90%; margin:auto; max-width:500px; }
  </style>
</head>
<body>

  <h1>⚙️ Configuration WiFi</h1>

  <div>
    <form action="/save" method="POST">
      <h3>Réseaux détectés :</h3>
      <select name="ssid">
)rawliteral";

  page += ssidOptionsHTML;

  page += R"rawliteral(
      </select>

      <h3>Ou SSID personnel :</h3>
      <input type="text" name="ssid_custom" placeholder="MonSSID">

      <h3>Mot de passe :</h3>
      <input type="password" name="pass" placeholder="password">

      <br><br>
      <input type="submit" value="Enregistrer">
    </form>
  </div>

</body>
</html>
)rawliteral";

  return page;
}

static String htmlInfoPage() {
  String ip = WiFi.localIP().toString();

  // Met à jour les infos miner
  minerUpdate();
  MinerStatus m = minerGetStatus();

  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

  <meta charset="utf-8">

<link rel="icon" type="image/png" 
      href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAFkSURBVGhD7dhBDoIwEETRj/7/X9upsAo7EfxnSm/JSyHbHiSxME+k22yppWfQaSDeS2zwWu7gk7I+gSJJrDhBMS8n9JH7kA5IBOGBo8+cO9Lt5eYtQgkFH86hgPFqNgj7F/ySi1LszOp/9IvcF3lwMGkAszPSH8LiJOAUEgKqDsUQvYHx3M5AbWOVCq4wKZEgqN6Cxj8AyVqYpYQ6r5T2oFaqOoGBAaY8BJSpswEiY4uBQGmOAiYpYwAmZ4tAApnwAkKWMJImOFgIGmPAQEqbMBAmaLCwNApj0AQJmKWMJIuPBQGmOAgEqbMBAmaLCwNApgwNElp2/KN+YgUmBIHXEJ3sAAAAASUVORK5CYII="/>
  <title>KON8 Avalon Controler Dashboard</title>
  <style>
    body { font-family: Arial; text-align:center; background:#121212; color:white; }
    h1 { font-size: 28px; margin-top:30px; color:#00e676; }
    h2 { font-size: 22px; margin-top:25px; }
    h3 { font-size: 20px; margin-top:15px; }
    p { font-size: 18px; }
    a.button, input[type=submit] {
      display:inline-block;
      margin-top:10px;
      background:#ff5252;
      color:white;
      padding:10px 20px;
      text-decoration:none;
      border-radius:10px;
      font-size:18px;
      border:none;
      cursor:pointer;
    }
    a.button:hover, input[type=submit]:hover {
      opacity:0.9;
    }
    .section {
      width: 95%;
      max-width: 900px;
      margin:20px auto;
      padding:15px;
      border-radius:15px;
      background:#1e1e1e;
      text-align:left;
    }
    label, input[type=text], select {
      font-size:16px;
    }
    input[type=text], select {
      width: 80%; padding: 8px; margin-top:5px;
      border-radius: 8px; border:none;
    }
  </style>
</head>
<body>

  <h1>📟 TTGO Avalon Dashboard</h1>

  <div class="section">
    <h2>📶 WiFi</h2>
)rawliteral";

  page += "<p><b>IP de la TTGO :</b> " + ip + "</p>";
  page += "<p><b>SSID :</b> " + wifiSSID + "</p>";
  page += "<p><b>RSSI :</b> " + String(WiFi.RSSI()) + " dBm</p>";
  page += "<a class=\"button\" href=\"/reconfig\">Reconfigurer le WiFi</a>";
  page += "</div>";

   page += R"rawliteral(
  <div class="section">
    <h2>🕒 Horloge</h2>
)rawliteral";

  // Affichage de la date/heure actuelle
  page += "<p><b>Date / Heure locale :</b> " + getTimeString() + "</p>";

  page += R"rawliteral(
    <form action="/time" method="POST">
      <label>Décalage UTC (heure) :</label><br>
      <select name="offset">
  )rawliteral";

  // Générer la liste -12 .. +12 avec la valeur actuelle pré-sélectionnée
  for (int i = -12; i <= 12; i++) {
    page += "<option value=\"";
    page += String(i);
    page += "\"";
    if (i == gUtcOffsetHours) page += " selected";
    page += ">";
    if (i >= 0) page += "+";
    page += String(i);
    page += " h</option>";
  }

  page += R"rawliteral(
      </select>
      <br><br>
      <input type="submit" value="Appliquer">
    </form>
  </div>
  )rawliteral";


// ---- Section Temperature / Humidite ----
  page += R"rawliteral(
  <div class="section">
    <h2>🌡️ Ambiance</h2>
)rawliteral";

  if (isnan(gTempC) || isnan(gHum)) {
    page += "<p>Pas encore de mesure disponible.</p>";
  } else {
    page += "<p><b>Temp&eacute;rature :</b> " + String(gTempC, 1) + " &deg;C</p>";
    page += "<p><b>Humidit&eacute; :</b> " + String(gHum, 1) + " %</p>";
  }

  page += "</div>";

  // Section configuration + infos miner
  page += R"rawliteral(
  <div class="section">
    <h2>⚒️ Miner Avalon</h2>
    <form action="/miner" method="POST">
      <label>Adresse IP du miner :</label><br>
      <input type="text" name="miner_ip" placeholder="192.168.1.x" value=")rawliteral";

  page += minerGetIP();
  page += R"rawliteral(">
      <br><br>
      <input type="submit" value="Enregistrer l'IP">
    </form>
)rawliteral";

  if (m.ip.length() == 0) {
    page += "<p><i>IP du miner non configuree.</i></p>";
  } else {
    page += "<p><b>📡 IP actuelle du miner :</b> " + m.ip + "</p>";
    
    if (m.lastError.length() > 0) {
      page += "<p style='color:#ff5252'><b>Erreur :</b> " + m.lastError + "</p>";
    } else {

            // ---- Etat veille / reveil ----
      page += R"rawliteral(
      <h3>💤 État du miner</h3>
      )rawliteral";

      if (m.isActive) {
        page += R"rawliteral(
        <p>Le miner est <b>actif</b>.</p>
        <form action="/miner_standby" method="POST">
          <input type="submit" value="Mettre en veille (softoff)">
        </form>
        )rawliteral";
      } else {
        page += R"rawliteral(
        <p>Le miner est <b>inactif / en veille</b>.</p>
        <form action="/miner_wakeup" method="POST">
          <input type="submit" value="Réveiller (softon)">
        </form>
        )rawliteral";
      }


      // ---- Infos generales ----
      page += "<h3>ℹ️ Infos generales</h3>";
      page += "<p>🧱 <b>Produit :</b> " + m.ver.prod + " (" + m.ver.model + ")</p>";
      //page += "<p>💾 <b>CGMiner :</b> " + m.ver.cgminer + " (API " + m.ver.api + ")</p>";
      //page += "<p>🔌 <b>MAC :</b> " + m.ver.mac + "</p>";

      // Hashrate : convertir en TH/s
      double mhs_av = m.sum.mhs_av.toDouble();
      double ths    = mhs_av / 1000000.0;

      //page += "<h3>⚡ Hashrate</h3>";
      page += "<p><b>Hashrate moyen :</b> " + String(ths, 2) + " TH/s</p>";
      //page += "<p><b>MHS 5s :</b> " + m.sum.mhs_5s + "</p>";

      // Puissance (si disponible)
      if (m.power.length() > 0) {
        page += "<p><b>Puissance :</b> " + m.power + " W</p>";
      }

      // Working status
      String elapsedNice = formatElapsed(m.sum.elapsed);
      String modeLabel   = formatModeLabel(m.workMode);

      //page += "<h3>🛠️ Working status</h3>";
      page += "<p>🎛️ <b>Working mode :</b> " + modeLabel + "</p>";
      page += "<p>🕒 <b>Elapsed :</b> " + elapsedNice + "</p>";

      // Shares
      page += "<h3>📊 Shares</h3>";
      page += "<p>✅ Accepted : " + m.sum.accepted +
              " &nbsp;&nbsp; ❌ Rejected : " + m.sum.rejected +
              " &nbsp;&nbsp; ⚠️ HW Errors : " + m.sum.hw_errors + "</p>";
    }

    // Formulaire changement de mode
    if (m.isActive) {
      page += R"rawliteral(
    <h3>🎚️ Changer de mode</h3>
    <form action="/miner_mode" method="POST">
      <label>Mode :</label><br>
      <select name="mode">
        <option value="eco">Eco 🌿</option>
        <option value="standard">Standard ⚙️</option>
        <option value="super">Super 🚀</option>
      </select>
      <br><br>
      <input type="submit" value="Envoyer au miner">
    </form>
)rawliteral";
    } else {
      page += R"rawliteral(
    <h3>🎚️ Changer de mode</h3>
    <p>Indisponible : le miner est en veille (inactif).</p>
)rawliteral";
    }

  }

  page += "</div></body></html>";

  return page;
}

// =======================
// HTTP HANDLERS
// =======================

static void handleRoot() {
  if (configMode) {
    // Mode AP / configuration WiFi
    server.send(200, "text/html", htmlConfigPage());
  } else {
    // Mode normal : dashboard
    minerUpdate();
    String page = htmlInfoPage();
    server.send(200, "text/html", page);
  }
}


static void handleSave() {
  if (server.method() == HTTP_POST) {
    String ssid = server.arg("ssid");
    String ssidCustom = server.arg("ssid_custom");
    String pass = server.arg("pass");

    if (ssidCustom.length() > 0) {
      ssid = ssidCustom;
    }

    if (ssid.length() > 0) {
      prefs.begin("wifi", false);
      prefs.putString("ssid", ssid);
      prefs.putString("pass", pass);
      prefs.end();

      server.send(200, "text/html",
        "<html><body><h1>OK</h1><p>Redemarrage...</p></body></html>");
      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/html", "SSID manquant");
    }
  } else {
    server.send(405, "text/html", "Method not allowed");
  }
}

static void handleReconfig() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  server.send(200, "text/html",
              "<html><body><h1>OK</h1><p>Redemarrage en mode config...</p></body></html>");
  delay(1000);
  ESP.restart();
}

// Enregistrement de l'IP du miner
static void handleMinerSave() {
  if (server.method() == HTTP_POST) {
    String ip = server.arg("miner_ip");
    ip.trim();

    minerSetIP(ip);

    server.send(200, "text/html",
      "<html><body><h1>IP enregistree</h1><p>Retour...</p>"
      "<script>setTimeout(function(){window.location='/'},1000);</script>"
      "</body></html>");
  } else {
    server.send(405, "text/html", "Method not allowed");
  }
}

// Changement de mode du miner
static void handleMinerMode() {
  String ip = minerGetIP();
  if (ip.length() == 0) {
    server.send(400, "text/html", "IP du miner non configuree.");
    return;
  }

  String mode = server.arg("mode");
  mode.trim();

  bool ok = false;
  String resp = minerSendMode(mode, ok);

  String page = "<html><body><h1>Commande mode envoyee</h1>";
  page += "<p>Mode demande : " + formatModeLabel(mode) + "</p>";

  if (ok) {
    page += "<p style='color:#00e676'><b>OK :</b> mode applique.</p>";
  } else {
    page += "<p style='color:#ff5252'><b>Erreur :</b> " + resp + "</p>";
  }

  page += "<h3>Reponse brute :</h3><pre>" + resp + "</pre>";
  page += "<script>setTimeout(function(){window.location='/'},2000);</script>";
  page += "</body></html>";

  server.send(200, "text/html", page);
}

static void handleMinerStandby() {
  String ip = minerGetIP();
  if (ip.length() == 0) {
    server.send(400, "text/html", "IP du miner non configuree.");
    return;
  }

  bool ok = false;
  time_t now = time(nullptr);
  uint32_t ts = (uint32_t)now + 5;  // dans 5 secondes

  String resp = minerSetStandby(ts, ok);

  String page = "<html><body><h1>Commande standby envoyee</h1>";
  if (ok) {
    page += "<p>Le miner va passer en veille dans quelques secondes.</p>";
  } else {
    page += "<p style='color:#ff5252'><b>Erreur :</b> " + resp + "</p>";
  }
  page += "<script>setTimeout(function(){window.location='/'},2000);</script>";
  page += "</body></html>";

  server.send(200, "text/html", page);
}

static void handleMinerWakeup() {
  String ip = minerGetIP();
  if (ip.length() == 0) {
    server.send(400, "text/html", "IP du miner non configuree.");
    return;
  }

  bool ok = false;
  time_t now = time(nullptr);
  uint32_t ts = (uint32_t)now + 5;  // dans 5 secondes

  String resp = minerSetWakeup(ts, ok);

  String page = "<html><body><h1>Commande wake-up envoyee</h1>";
  if (ok) {
    page += "<p>Le miner va redevenir actif.</p>";
  } else {
    page += "<p style='color:#ff5252'><b>Erreur :</b> " + resp + "</p>";
  }
  page += "<script>setTimeout(function(){window.location='/'},2000);</script>";
  page += "</body></html>";

  server.send(200, "text/html", page);
}


static void handleTimeSave() {
  if (server.method() == HTTP_POST) {
    String v = server.arg("offset");
    int offset = v.toInt();
    if (offset < -12) offset = -12;
    if (offset > 12) offset = 12;

    saveTimeConfig(offset);
    applyTimeConfig();   // ⚡ applique immédiatement le nouvel offset

    server.send(200, "text/html",
      "<html><body><h1>Fuseau mis à jour</h1>"
      "<p>Nouveau décalage UTC : " + String(offset) + " h</p>"
      "<script>setTimeout(function(){window.location='/'},1000);</script>"
      "</body></html>");
  } else {
    server.send(405, "text/html", "Method not allowed");
  }
}




// =======================
// WIFI
// =======================

static bool connectToSavedWiFi(uint16_t timeoutMs = 15000) {
  prefs.begin("wifi", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPASS = prefs.getString("pass", "");
  prefs.end();

  if (wifiSSID.length() == 0) {
    Serial.println("Aucun WiFi sauvegarde.");
    return false;
  }

  Serial.println("Tentative de connexion a : " + wifiSSID);
  displayShowConnecting(wifiSSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi CONNECTE, IP: " + WiFi.localIP().toString());
    //displayShowWiFiOK(wifiSSID, WiFi.localIP());
    return true;
  } else {
    Serial.println("Echec de connexion WiFi.");
    displayShowWiFiError();
    return false;
  }
}

static void startAPMode() {
  configMode = true;
  Serial.println("Demarrage du mode AP de configuration");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSSID, apPASS);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);

  displayShowAPInfo(ip);

  // Scan des reseaux
  Serial.println("Scan des reseaux WiFi...");
  int n = WiFi.scanNetworks();
  ssidOptionsHTML = "";
  if (n == 0) {
    ssidOptionsHTML = "<option value=\"\">Aucun reseau trouve</option>";
  } else {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      ssidOptionsHTML += "<option value=\"";
      ssidOptionsHTML += ssid;
      ssidOptionsHTML += "\">";
      ssidOptionsHTML += ssid;
      ssidOptionsHTML += " (";
      ssidOptionsHTML += String(rssi);
      ssidOptionsHTML += " dBm)</option>";
    }
  }
  WiFi.scanDelete();

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/reconfig", handleReconfig);
  server.on("/miner", handleMinerSave);
  server.on("/miner_mode", handleMinerMode);
  server.on("/time", handleTimeSave);
  server.on("/miner_standby", handleMinerStandby);
  server.on("/miner_wakeup", handleMinerWakeup);
  server.begin();
}

static void startNormalMode() {
  configMode = false;
  Serial.println("Mode normal (connecte au WiFi).");

  //displayShowWiFiOK(wifiSSID, WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/reconfig", handleReconfig);
  server.on("/miner", handleMinerSave);
  server.on("/miner_mode", handleMinerMode);
  server.on("/time", handleTimeSave);
  server.on("/miner_standby", handleMinerStandby);
  server.on("/miner_wakeup", handleMinerWakeup);
  server.begin();
}

// =======================
// API PUBLIC
// =======================

void portalSetup() {
  minerInit();   // charge IP + mode du miner

  // 1) On regarde si on doit FORCER le mode AP
  bool forceAP = false;
  prefs.begin("sys", true);
  forceAP = prefs.getBool("forceAP", false);
  prefs.end();

  if (forceAP) {
    // On consomme le flag (pour ne pas rester bloqué à vie)
    prefs.begin("sys", false);
    prefs.putBool("forceAP", false);
    prefs.end();

    Serial.println("Force AP mode au demarrage (flag forceAP)");
    startAPMode();
    return;
  }

  // 2) Comportement normal : tentative de connexion WiFi
  bool ok = connectToSavedWiFi();

  if (ok) {
    startNormalMode();
  } else {
    startAPMode();
  }
}


void portalLoop() {
  server.handleClient();
}

bool portalIsConfigMode() {
  return configMode;
}


void portalFactoryReset() {
  Preferences p;

  // Efface WiFi
  p.begin("wifi", false);
  p.clear();
  p.end();

  // Si un jour tu stockes le fuseau dans "timecfg", ça le nettoiera aussi
  p.begin("timecfg", false);
  p.clear();
  p.end();
}