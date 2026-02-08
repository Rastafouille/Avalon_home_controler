#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "display.h"
#include "portal.h"
#include "miner.h"
#include "DHT.h"
#include <Preferences.h>

static uint32_t lastUpdate = 0;
const uint32_t UPDATE_INTERVAL = 10000; // toutes les 2s
// Données stockées pour affichage immédiat
String cachedDate = "";
String cachedTime = "";
String cachedMinerIP = "";
String cachedMinerMode = "";
float cachedTHs = 0.0f;
float cachedPower = 0.0f;
float cachedTemp = NAN;
float cachedHum = NAN;
int cachedRSSI = 0;
bool minerOk = false;


void updateDateTimeStrings() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char buf[16];
        strftime(buf, sizeof(buf), "%d/%m/%Y", &timeinfo);
        cachedDate = buf;
        strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
        cachedTime = buf;
    }
}




// =======================
// DHT22 / AM2302
// =======================
#define DHTPIN 13       // GPIO13 sur TTGO T-Display
#define DHTTYPE DHT22   // AM2302 = DHT22

DHT dht(DHTPIN, DHTTYPE);

// Mesures capteur (utilisées aussi dans portal.cpp)
float gTempC = NAN;
float gHum   = NAN;

void updateDHT() {
    if (!isnan(gTempC)) cachedTemp = gTempC;
    if (!isnan(gHum))   cachedHum  = gHum;
}
// =======================
// Boutons TTGO T-Display
// =======================
// Sur la plupart des TTGO T-Display : 0 et 35
const uint8_t BUTTON_NEXT_PIN = 35;   // bouton droit  -> page suivante
const uint8_t BUTTON_PREV_PIN = 0;    // bouton gauche -> page précédente

// Gestion des pages et de la veille écran
const uint8_t NUM_PAGES = 4;          // 0=WiFi, 1=Miner, 2=Horloge, 3=Climat
static uint8_t currentPage = 0;

const uint32_t SCREEN_TIMEOUT_MS = 30000; // 30s avant extinction
static uint32_t lastInteractionMs = 0;
static bool backlightOn = true;

static int lastNextState = HIGH;
static int lastPrevState = HIGH;


// intervalle de lecture DHT (ms)
const uint32_t DHT_INTERVAL_MS = 5000;
static uint32_t lastDhtRead = 0;


const uint32_t RESET_HOLD_MS = 5000;   // 5s d'appui long

static bool     resetInProgress     = false;
static uint32_t resetStartMs        = 0;
static uint32_t lastResetDisplayMs  = 0;


// petit helper pour le label du mode
static String formatModeLabel(const String &mode) {
  if (mode == "eco")                          return "Eco";
  if (mode == "standard" || mode == "normal") return "Standard";
  if (mode == "super")                        return "Super";
  return "Inconnu";
}

// Formate date / heure depuis l'horloge locale
static bool getDateTimeStrings(String &dateStr, String &timeStr) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false;
  }

  char buf[16];

  strftime(buf, sizeof(buf), "%d/%m/%Y", &timeinfo);
  dateStr = buf;

  strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  timeStr = buf;

  return true;
}

// Lecture périodique du DHT
static void updateDht() {
  uint32_t now = millis();
  if (now - lastDhtRead < DHT_INTERVAL_MS) return;
  lastDhtRead = now;

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Erreur lecture DHT/AM2302");
    return;
  }

  gHum   = h;
  gTempC = t;

  Serial.print("DHT OK  Temp: ");
  Serial.print(gTempC, 1);
  Serial.print(" C  Hum: ");
  Serial.print(gHum, 1);
  Serial.println(" %");
}
static void showCurrentPage() {
  if (!backlightOn) return;

    // 👉 Si on est en mode AP/config, on force l’affichage config
  if (portalIsConfigMode()) {
    // Affiche les infos AP (SSID TTGO_Config + IP AP)
    displayShowAPInfo(WiFi.softAPIP());
    return;
  }

  if (currentPage == 0) {
    // Page WiFi
    if (WiFi.isConnected()) {
      displayShowWiFiPage(WiFi.SSID(), WiFi.localIP(), cachedRSSI);
    } else {
      displayShowWiFiError();
    }
  }
  else if (currentPage == 1) {
    // Page Miner
    //bool minerOk = minerUpdate();
    //MinerStatus st = minerGetStatus();

    if (minerOk ){//&& st.ip.length() > 0) {
     // double mhs_av = st.sum.mhs_av.toDouble();
     // float ths = mhs_av / 1000000.0f;  // MH/s -> TH/s
     // float powerW = st.power.toFloat();

     // String modeLabel;
     // if (!st.isActive) {
     //   modeLabel = "Inactif";
     //   ths    = 0.0f;
     //   powerW = 0.0f;
     // } else {
      //  modeLabel = formatModeLabel(st.workMode);
      //}

      displayShowMinerPage(
          cachedMinerIP,
          formatModeLabel(cachedMinerMode),
          cachedTHs,
          cachedPower);    
} else {
      displayShowMinerPage("N/A", "Inconnu", 0.0f, 0.0f);
    }
  }
  else if (currentPage == 2) {
    // Page Horloge

    if (!portalIsNtpReady()) {
        // NTP pas encore synchronisé → on affiche un message propre
        displayShowDateTimePage("TIme Sync...", "");
        return;
    }

   // String d, t;
   // if (!getDateTimeStrings(d, t)) {
    //  d = "Date N/A";
    //  t = "Heure N/A";
    //}
    displayShowDateTimePage(cachedDate, cachedTime);
  }
  else {
    // Page Climat
    displayShowEnvPage(cachedTemp, cachedHum);
  }
}


void setup() {
  Serial.begin(115200);
  delay(500);

  displayInit();
  displayShowBoot();

  dht.begin();     // init capteur

  pinMode(BUTTON_NEXT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PREV_PIN, INPUT_PULLUP);

  backlightOn = true;

  // WiFi + serveur web + minerInit() + (config NTP si tu l'ajoutes dans portalSetup)
  portalSetup();
  updateDateTimeStrings();
  
  lastInteractionMs = millis();
  currentPage = 2;
  showCurrentPage();

}

void loop() {
  portalLoop();    // HTTP, WiFi, etc.
  updateDht();     // met à jour gTempC/gHum

  uint32_t now = millis();

  // Lecture boutons (INPUT_PULLUP ⇒ appui = LOW)
  int nextState = digitalRead(BUTTON_NEXT_PIN);
  int prevState = digitalRead(BUTTON_PREV_PIN);

  bool bothPressed = (nextState == LOW && prevState == LOW);
  bool pressedNext = (lastNextState == HIGH && nextState == LOW);
  bool pressedPrev = (lastPrevState == HIGH && prevState == LOW);

  lastNextState = nextState;
  lastPrevState = prevState;

  if (now - lastUpdate > UPDATE_INTERVAL) {
    lastUpdate = now;
    // 🔥 Lire capteur DHT
    updateDHT();   // fonction que je te fournis plus bas

    // 🔥 Mise à jour miner
    minerOk = minerUpdate();
    MinerStatus st = minerGetStatus();

    cachedMinerIP   = st.ip;
    cachedMinerMode = st.workMode;

    double mhs = st.sum.mhs_av.toDouble();
    cachedTHs = mhs / 1000000.0;

    if (st.power.length() > 0)
        cachedPower = st.power.toFloat();

    cachedRSSI = WiFi.RSSI();

    // 🔥 Mise à jour heure/date
    updateDateTimeStrings();  // fonction fournie plus bas
}

  // === 1) GESTION RESET : les 2 boutons enfoncés ===
  if (bothPressed) {
    if (!resetInProgress) {
      // Début de l'appui long
      resetInProgress    = true;
      resetStartMs       = now;
      lastResetDisplayMs = 0;

      if (!backlightOn) {
        displayBacklightOn();
        backlightOn = true;
      }
    }

    uint32_t elapsed = now - resetStartMs;

    if (elapsed >= RESET_HOLD_MS) {
      // Temps écoulé ⇒ on fait le reset
      displayShowResetCountdown(0);
      delay(500);

      portalFactoryReset();
      minerFactoryReset();
      // Pose un flag pour forcer le mode AP au prochain boot
      Preferences p;
      p.begin("sys", false);
      p.putBool("forceAP", true);
      p.end();

      delay(500);
      ESP.restart();
    } else {
      // Mise à jour du compte à rebours toutes les 200 ms
      if (now - lastResetDisplayMs > 200) {
        uint8_t remaining =
          (uint8_t)((RESET_HOLD_MS - elapsed + 999) / 1000); // arrondi au dessus
        displayShowResetCountdown(remaining);
        lastResetDisplayMs = now;
      }
    }

    // tant qu'on maintient les 2 boutons, on ne fait rien d'autre
    lastInteractionMs = now;   // évite la mise en veille pendant le reset
    return;
  } else if (resetInProgress) {
    // Boutons relâchés avant les 5s ⇒ annulation
    resetInProgress = false;
    lastInteractionMs = now;
    showCurrentPage();   // on revient à la page courante
  }

  // === 2) Navigation normale (un seul bouton à la fois) ===
 if (pressedNext || pressedPrev) {
  if (!backlightOn) {
    // Réveil écran
    displayBacklightOn();
    backlightOn = true;
    lastInteractionMs = now;
    displayShowBoot();   // logo 3s
    //delay(3000);
    currentPage = 2;
    showCurrentPage();
  } else {
    // 👉 Si on est en mode config (AP), PAS de changement de page
    if (portalIsConfigMode()) {
      lastInteractionMs = now;
      showCurrentPage();   // juste refresh de la page config
    } else {
      // 👉 Mode normal : on navigue entre les pages
      if (pressedNext) {
        currentPage = (currentPage + 1) % NUM_PAGES;
      }
      if (pressedPrev) {
        if (currentPage == 0) currentPage = NUM_PAGES - 1;
        else currentPage--;
      }
      lastInteractionMs = now;
      showCurrentPage();
    }
  }
}

  // === 3) Veille auto de l'écran ===
  if (backlightOn && (now - lastInteractionMs > SCREEN_TIMEOUT_MS)) {
    displayShowBoot();   // logo 3s
    //delay(3000);

    displayBacklightOff();
    backlightOn = false;
  }
}
