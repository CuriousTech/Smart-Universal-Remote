#ifndef MEDIA_H
#define MEDIA_H

#include <Arduino.h>
#include <cstring>
#include <FS.h>
#include <FFat.h>

#define INTERNAL_FS FFat // Make it the same as the partition scheme uploaded (SPIFFS, LittleFS, FFat)

class Media
{
public:
  Media(void);
  void init(void);

  String internalFileListJson(fs::FS &fs, const char* directory);
  void deleteIFile(char *pszFilename);
};

extern Media media;

#endif // MEDIA_H
