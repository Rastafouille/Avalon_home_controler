#pragma once
#include <Arduino.h>

void portalSetup();
void portalLoop();
void portalFactoryReset();
bool portalIsConfigMode();
bool portalIsNtpReady();
bool waitForNtp(uint32_t timeoutMs = 5000);


