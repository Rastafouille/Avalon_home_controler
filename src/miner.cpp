#include "miner.h"

#include <WiFi.h>
#include <Preferences.h>

// =======================
// NVS / Etat interne
// =======================

static Preferences minerPrefs;

static String gMinerIP;
static String gLastError;
static String gCurrentMode;   // "eco"/"standard"/"super"/""
static String gPower;         // puissance instantanée en W (depuis PS[])
static String gWorkState;     // "In Work" / "In Idle" / ...
static bool   gIsActive = false;



static MinerVersionInfo gVerInfo;
static MinerSummaryInfo gSumInfo;

// =======================
// Helpers parsing
// =======================

static String getBlock(const String &src, const String &tag) {
  int start = src.indexOf(tag);
  if (start < 0) return "";
  int end = src.indexOf('|', start);
  if (end < 0) end = src.length();
  return src.substring(start, end);
}

static String getValueFromBlock(const String &block, const String &key) {
  String pattern = key + "=";
  int idx = block.indexOf(pattern);
  if (idx < 0) return "";
  idx += pattern.length();
  int end = block.indexOf(',', idx);
  if (end < 0) end = block.length();
  return block.substring(idx, end);
}

static MinerVersionInfo parseMinerVersion(const String &resp) {
  MinerVersionInfo info;
  String block = getBlock(resp, "VERSION,");
  if (block.length() == 0) return info;

  info.cgminer = getValueFromBlock(block, "CGMiner");
  info.api     = getValueFromBlock(block, "API");
  info.prod    = getValueFromBlock(block, "PROD");
  info.model   = getValueFromBlock(block, "MODEL");
  info.mac     = getValueFromBlock(block, "MAC");
  return info;
}

static MinerSummaryInfo parseMinerSummary(const String &resp) {
  MinerSummaryInfo info;
  String block = getBlock(resp, "SUMMARY,");
  Serial.println("SUMMARY block: " + block);
  if (block.length() == 0) return info;

  info.elapsed   = getValueFromBlock(block, "Elapsed");
  info.mhs_av    = getValueFromBlock(block, "MHS av");
  info.mhs_5s    = getValueFromBlock(block, "MHS 5s");
  info.accepted  = getValueFromBlock(block, "Accepted");
  info.rejected  = getValueFromBlock(block, "Rejected");
  info.hw_errors = getValueFromBlock(block, "Hardware Errors");
  return info;
}

// extrait WORKMODE[n] depuis la reponse estats
static String getWorkModeFromEstats(const String &resp) {
  int idx = resp.indexOf("WORKMODE[");
  if (idx < 0) return "";
  idx += String("WORKMODE[").length();
  int end = resp.indexOf(']', idx);
  if (end < 0) return "";
  String val = resp.substring(idx, end);
  val.trim();
  return val;  // "0", "1", "2", ...
}
// extrait power depuis la reponse estats

static String getPowerFromEstats(const String &resp) {
  int idx = resp.indexOf("PS[");
  if (idx < 0) return "";
  idx += 3; // saute "PS["

  int end = resp.indexOf(']', idx);
  if (end < 0) return "";

  String block = resp.substring(idx, end); // ex: "0 1209 2349 55 1306 2350 1364"
  block.trim();

  // Dernier nombre après le dernier espace
  int lastSpace = block.lastIndexOf(' ');
  if (lastSpace < 0) return block; // si un seul nombre

  String val = block.substring(lastSpace + 1);
  val.trim();
  return val;  // "1364"
}

// extrait Work depuis SYSTEMSTATU[...] dans la reponse estats
// extrait Work depuis SYSTEMSTATU[...] dans la reponse estats
// extrait "Work: In Work" ou "Work: In Idle" depuis SYSTEMSTATU[...]
static String getWorkStateFromEstats(const String &resp) {
  //Serial.println("=== getWorkStateFromEstats() ===");
  //Serial.println("Reponse ESTATS brute :");
  //Serial.println(resp);

  // On isole le bloc SYSTEMSTATU[ ... ]
  int idx = resp.indexOf("SYSTEMSTATU[");
  if (idx < 0) {
    Serial.println("Pas de SYSTEMSTATU[ trouvé");
    return "";
  }

  int end = resp.indexOf(']', idx);
  if (end < 0) {
    Serial.println("Pas de ']' pour SYSTEMSTATU");
    return "";
  }

  String block = resp.substring(idx, end);
  //Serial.println("Bloc SYSTEMSTATU :");
  //Serial.println(block);

  // Cherche "Work:"
  int w = block.indexOf("Work:");
  if (w < 0) {
    Serial.println("Pas de 'Work:' dans SYSTEMSTATU");
    return "";
  }

  w += 5; // saute "Work:"
  // saute les espaces
  while (w < (int)block.length() && (block[w] == ' ' || block[w] == '\t')) {
    w++;
  }

  // fin = virgule ou fin du bloc
  int stop = block.indexOf(',', w);
  if (stop < 0) stop = block.length();

  String val = block.substring(w, stop);
  val.trim();

  //Serial.println("WorkState parse : " + val);
  //Serial.println("===============================");

  return val;   // ex: "In Work" ou "In Idle"
}

// =======================
// Communication bas niveau
// =======================

static String avalonSendCommand(const char* ip, uint16_t port, const String& cmd) {
  WiFiClient client;
  String response;

  if (!client.connect(ip, port)) {
    Serial.println("Connexion au miner impossible");
    return "";
  }

  // Comme "echo -n" : pas de \n
  client.print(cmd);
  client.flush();

  unsigned long deadline = millis() + 3000; // timeout 3s
  while (client.connected() && millis() < deadline) {
    while (client.available()) {
      char c = client.read();
      response += c;
    }
  }

  client.stop();
  return response;
}

// Commande officielle : ascset|0,workmode,set,<mode>
// <mode> : 0=eco, 1=standard, 2=super
static String makeModeCommand(const String &mode) {
  if (mode == "eco")         return "ascset|0,workmode,set,0";
  if (mode == "standard"
   || mode == "normal")      return "ascset|0,workmode,set,1";  // compat "normal"
  if (mode == "super")       return "ascset|0,workmode,set,2";
  return "";
}

// =======================
// API publique
// =======================

void minerInit() {
  minerPrefs.begin("miner", true);
  gMinerIP     = minerPrefs.getString("ip", "");
  gCurrentMode = minerPrefs.getString("mode", "");
  gPower = "";
  minerPrefs.end();
}

void minerSetIP(const String &ip) {
  gMinerIP = ip;
  minerPrefs.begin("miner", false);
  minerPrefs.putString("ip", ip);
  minerPrefs.end();
}

String minerGetIP() {
  return gMinerIP;
}

bool minerUpdate() {
  gLastError = "";
  gVerInfo = MinerVersionInfo();
  gSumInfo = MinerSummaryInfo();
  gWorkState = "";
  gIsActive  = false;


  if (gMinerIP.length() == 0) {
    gLastError = "IP du miner non configuree.";
    return false;
  }

  const uint16_t port = 4028;
  Serial.print("Interrogation miner Avalon @ ");
  Serial.println(gMinerIP);

  // ----- VERSION -----
  String v = avalonSendCommand(gMinerIP.c_str(), port, "version");
  if (v.length() == 0) {
    gLastError = "Aucune reponse (version).";
    return false;
  }
  gVerInfo = parseMinerVersion(v);

  // ----- SUMMARY -----
  String s = avalonSendCommand(gMinerIP.c_str(), port, "summary");
  if (s.length() == 0) {
    gLastError = "Aucune reponse (summary).";
    return false;
  }
  gSumInfo = parseMinerSummary(s);

  // ----- STATS (pour l'instant on verifie juste qu'il repond) -----
  String st = avalonSendCommand(gMinerIP.c_str(), port, "stats");
  if (st.length() == 0) {
    gLastError = "Aucune reponse (stats).";
    return false;
  }

  // ----- ESTATS : WORKMODE / puissance etc. -----
  String es = avalonSendCommand(gMinerIP.c_str(), port, "estats");
  if (es.length() > 0) {
    String wm = getWorkModeFromEstats(es);   // "0", "1", "2", ...
    if (wm == "0")      gCurrentMode = "eco";
    else if (wm == "1") gCurrentMode = "standard";
    else if (wm == "2") gCurrentMode = "super";
    gPower = getPowerFromEstats(es);         // ex: "1364"
  }

  String ws = getWorkStateFromEstats(es);
    if (ws.length() > 0) {
      gWorkState = ws;
      if (ws.indexOf("In Work") >= 0) {
        gIsActive = true;
      } else if (ws.indexOf("In Idle") >= 0) {
        gIsActive = false;
      }
    }

  return true;
}

MinerStatus minerGetStatus() {
  MinerStatus st;
  st.ip        = gMinerIP;
  st.ver       = gVerInfo;
  st.sum       = gSumInfo;
  st.workMode  = gCurrentMode;
  st.power     = gPower;      
  st.lastError = gLastError;
  st.workState = gWorkState;
  st.isActive  = gIsActive;
  return st;
}

String minerSendMode(const String &mode, bool &ok) {
  ok = false;
  if (gMinerIP.length() == 0) {
    gLastError = "IP du miner non configuree.";
    return "";
  }

  String cmd = makeModeCommand(mode);
  if (cmd.length() == 0) {
    gLastError = "Mode inconnu.";
    return "";
  }

  const uint16_t port = 4028;
  String resp = avalonSendCommand(gMinerIP.c_str(), port, cmd);

  if (resp.indexOf("STATUS=S") >= 0) {
    ok = true;
    gCurrentMode = mode;
    minerPrefs.begin("miner", false);
    minerPrefs.putString("mode", mode);
    minerPrefs.end();
  } else {
    gLastError = "Reponse miner : " + resp;
  }

  return resp;
}

String minerSetStandby(uint32_t ts, bool &ok) {
  ok = false;
  gLastError = "";

  if (gMinerIP.length() == 0) {
    gLastError = "IP du miner non configuree.";
    return "";
  }

  const uint16_t port = 4028;
  String cmd = "ascset|0,softoff,1:" + String(ts);
  String resp = avalonSendCommand(gMinerIP.c_str(), port, cmd);

  if (resp.indexOf("STATUS=I") >= 0 &&
      resp.indexOf("success softoff:") >= 0) {
    ok = true;
  } else {
    gLastError = "Reponse miner : " + resp;
  }

  return resp;
}

String minerSetWakeup(uint32_t ts, bool &ok) {
  ok = false;
  gLastError = "";

  if (gMinerIP.length() == 0) {
    gLastError = "IP du miner non configuree.";
    return "";
  }

  const uint16_t port = 4028;
  String cmd = "ascset|0,softon,1:" + String(ts);
  String resp = avalonSendCommand(gMinerIP.c_str(), port, cmd);

  if (resp.indexOf("STATUS=I") >= 0 &&
      resp.indexOf("success softon:") >= 0) {
    ok = true;
  } else {
    gLastError = "Reponse miner : " + resp;
  }

  return resp;
}

void minerFactoryReset() {
  minerPrefs.begin("miner", false);
  minerPrefs.clear();
  minerPrefs.end();

  gMinerIP     = "";
  gCurrentMode = "";
  gPower       = "";
  gLastError   = "";
}
