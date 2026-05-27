#pragma once
#include <Arduino.h>

void displayInit();
void displayShowDate();
void displayShowCalendar();
void displayShowAlbum(const String &path);
void displayRegisterHandlers();
void displayHandleUpdate(const String &body);
