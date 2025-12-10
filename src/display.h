#pragma once

#include <Arduino.h>
#include <WiFi.h>  // pour IPAddress

// Initialisation du TFT
void displayInit();

// Ecran de démarrage
void displayShowBoot();

// Infos quand on est en AP de config
void displayShowAPInfo(IPAddress ip);

// Etat WiFi "fixe" (utilisé au moment de la connexion)
void displayShowWiFiOK(const String &ssid, const IPAddress &ip);
void displayShowWiFiError();
void displayShowConnecting(const String &ssid);

// Pages cycliques (toutes les 3s)
void displayShowWiFiPage(const String &ssid, const IPAddress &ip, int32_t rssi);
void displayShowMinerPage(const String &ip, const String &modeLabel, float ths, float powerW);

void displayShowEnvPage(float tempC, float hum);

void displayShowDateTimePage(const String &dateStr, const String &timeStr);

void displayBacklightOn();
void displayBacklightOff();

void displayShowResetCountdown(uint8_t seconds);

void displayShowOtaStatus(const String &line1, const String &line2);


