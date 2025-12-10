#include "display.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include "kon8-2.h"


static TFT_eSPI tft = TFT_eSPI();
static bool gBacklightOn = true;


// =======================
// INITIALISATION
// =======================

void displayInit() {
  tft.init();
  tft.setRotation(1);   // paysage
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
  gBacklightOn = true;
  tft.fillScreen(TFT_BLACK);
}

// =======================
// ECRANS "ETAT" INITIAUX
// =======================

void displayShowBoot_OLD() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(20, 40);
  tft.println("TTGO");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 80);
  tft.println("Initialisation...");
}

void displayShowBoot() {
  tft.fillScreen(TFT_BLACK);

  // Centrage automatique
  int x = (tft.width()  - KON8_W) / 2;
  int y = (tft.height() - KON8_H) / 2;

  // Affichage du logo KON8
  tft.pushImage(x, y, KON8_W, KON8_H, kon8_logo);

  delay(3000); // afficher le logo 1.5 sec
}

void displayShowAPInfo(IPAddress ip) {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(20, 10);
  tft.println("CONFIG");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 60);
  tft.println("SSID: KON8_Config");

  tft.setCursor(10, 90);
  tft.println("PASS: 12345678");

  tft.setCursor(10, 120);
  tft.print("IP : ");
  tft.println(ip.toString());

  tft.setCursor(10, 160);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("http://192.168.4.1");
}

void displayShowWiFiOK(const String &ssid, const IPAddress &ip) {
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(3);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(30, 10);
  tft.println("WiFi OK");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 70);
  tft.print("SSID : ");
  tft.println(ssid);

  tft.setCursor(10, 110);
  tft.print("IP   : ");
  tft.println(ip.toString());
}

void displayShowWiFiError() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(3);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("ERREUR");

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 70);
  tft.println("Connexion WiFi");
  tft.setCursor(10, 100);
  tft.println("impossible !");

  tft.setCursor(10, 140);
  tft.println("Mode CONFIG...");
}

void displayShowConnecting(const String &ssid) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Connexion...");

  tft.setCursor(10, 40);
  tft.print("SSID: ");
  tft.println(ssid);
}

// =======================
// PAGES CYCLIQUES
// =======================

void displayShowWiFiPage(const String &ssid, const IPAddress &ip, int32_t rssi) {
  tft.fillScreen(TFT_BLACK);

  // Titre centré
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("WiFi", tft.width() / 2, 5);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);

  // SSID
  tft.setCursor(5, 40);
  tft.print("SSID: ");
  tft.println(ssid);

  // IP
  tft.setCursor(5, 70);
  tft.print("IP:   ");
  tft.println(ip.toString());

  // RSSI
  tft.setCursor(5, 100);
  tft.print("RSSI: ");
  tft.print(rssi);
  tft.println(" dBm");
}

void displayShowMinerPage(const String &ip, const String &modeLabel, float ths, float powerW) {
  tft.fillScreen(TFT_BLACK);

  // Titre
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("Miner", tft.width() / 2, 5);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);

  // IP du miner
  tft.setCursor(5, 38);
  tft.print("IP:   ");
  tft.println(ip);

  // Mode
  tft.setCursor(5, 66);
  tft.print("Mode: ");
  tft.println(modeLabel);

  // Hashrate
  tft.setCursor(5, 92);
  tft.print("Hash: ");
  tft.print(ths, 2);
  tft.println(" TH/s");

  // Puissance (si >0)
  tft.setCursor(5, 120);
  tft.print("Pwr:  ");
  tft.print(powerW, 0);
  tft.println(" W");
}


void displayShowEnvPage(float tempC, float hum) {
  tft.fillScreen(TFT_BLACK);

  // Titre centré
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("Climat", tft.width() / 2, 5);

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Température
  tft.setCursor(5, 40);
  tft.print("TEMP: ");
  if (isnan(tempC)) {
    tft.print("N/A");
  } else {
    tft.print(tempC, 1);
    tft.print(" C");
  }

  // Humidité
  tft.setCursor(5, 70);
  tft.print("HUM:  ");
  if (isnan(hum)) {
    tft.print("N/A");
  } else {
    tft.print(hum, 1);
    tft.print(" %");
  }
}

void displayShowDateTimePage(const String &dateStr, const String &timeStr) {
  tft.fillScreen(TFT_BLACK);

  // Titre
  tft.setTextDatum(TC_DATUM);
  //tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  //tft.setTextSize(3);
  //tft.drawString("Horloge", tft.width() / 2, 5);
  // Heure centré
  tft.setTextSize(6);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(timeStr, tft.width() / 2, 20);

  // Date centrée
  tft.setTextSize(3);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(dateStr, tft.width() / 2, 105);

}

void displayBacklightOn() {
#ifdef TFT_BL
  digitalWrite(TFT_BL, HIGH);
#endif
  gBacklightOn = true;
}

void displayBacklightOff() {
#ifdef TFT_BL
  digitalWrite(TFT_BL, LOW);
#endif
  gBacklightOn = false;
  tft.fillScreen(TFT_BLACK);
}

void displayShowResetCountdown(uint8_t seconds) {
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Reset dans :", tft.width() / 2, 40);

  char buf[8];
  sprintf(buf, "%us", (unsigned int)seconds);

  tft.setTextSize(4);
  tft.drawString(buf, tft.width() / 2, 90);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Relache pour annuler", tft.width() / 2, 140);
}

void displayShowOtaStatus(const String &line1, const String &line2) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setTextSize(2);
  tft.drawString(line1, tft.width() / 2, 40);

  if (line2.length() > 0) {
    tft.setTextSize(2);
    tft.drawString(line2, tft.width() / 2, 80);
  }
}
