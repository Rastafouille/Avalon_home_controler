#pragma once
#include <Arduino.h>

struct MinerVersionInfo {
  String cgminer;
  String api;
  String prod;
  String model;
  String mac;
};

struct MinerSummaryInfo {
  String elapsed;   // secondes en string
  String mhs_av;
  String mhs_5s;
  String accepted;
  String rejected;
  String hw_errors;
};

struct MinerStatus {
  String ip;              // IP du miner
  MinerVersionInfo ver;   // infos version
  MinerSummaryInfo sum;   // infos summary
  String workMode;        // "eco" / "standard" / "super" / ""
  String power;           // puissance instantanée en W (depuis PS[])
  String lastError;       // vide si OK
  String workState;       // texte brut venant de SYSTEMSTATU[Work: ...]
  bool   isActive;        // true = In Work, false = In Idle
};

void minerInit();                      // charge IP + mode depuis NVS
void minerSetIP(const String &ip);     // set + sauvegarde IP du miner
String minerGetIP();                   // IP actuelle

bool minerUpdate();                    // interroge version/summary/stats/estats
MinerStatus minerGetStatus();          // dernier status connu

// Envoie ascset|0,workmode,set,<mode>
// mode : "eco" / "standard" / "super"
String minerSendMode(const String &mode, bool &ok);

String minerSetStandby(uint32_t ts, bool &ok);
String minerSetWakeup(uint32_t ts, bool &ok);

// Reset complet de la config miner (IP, mode, etc.)
void minerFactoryReset();