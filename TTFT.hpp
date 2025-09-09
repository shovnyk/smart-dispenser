#pragma once

void tftsetup();
void tftOrderReceived(const char *);
void tftOrderCancelled();
void tftSystemCheck();
void tftSysCheckFailed(const char *);
void tftSysCheckPassed(const char *message);
void tftDispenseComplete();

