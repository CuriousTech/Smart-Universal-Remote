#ifndef MEDIA_H
#define MEDIA_H

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>

#define INTERNAL_FS FFat // Should be same as the partition scheme uploaded (SPIFFS, LittleFS, FFat)

//#define USE_SDCARD // uncomment to add SD card support
//#define USE_AUDIO // uncomment for audio (requires SDCard as well) Do not use: Audio.h and IRemote.h conflict

#ifdef USE_SDCARD
#include <SD_MMC.h>
#define SD_CLK_PIN  14
#define SD_CMD_PIN  17
#define SD_D0_PIN   16 
#define SD_D1_PIN   -1 
#define SD_D2_PIN   -1 
#define SD_D3_PIN   21 
#endif

#ifdef USE_AUDIO
#define I2S_DOUT    47
#define I2S_BCLK    48  
#define I2S_LRC     38      // I2S_WS

#define Volume_MAX  21

enum _sounds
{
  SND_CLICK,
  SND_ALERT,
};
#include <Audio.h>  // ESP32-audioI2S (ESP32 2.0.14 compatible version from WaveShare example) https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-2.8/ESP32-S3-Touch-LCD-2.8-Demo.zip
#endif

#define maxPathLength 100

struct FileEntry
{
  char Name[maxPathLength];
  uint32_t Size;
  time_t Date;
  bool bDir;
};

class Media
{
public:
  Media(void);
  void init(void);
  void service(void);

  bool SDCardAvailable(void);
  void setFS(char *pszValue);
  void deleteFile(char *pszFilename);
  File createFile(String sFilename);
  void createDir(char *pszName);
  void setPath(char *szPath);
  uint32_t freeSpace(void);
  void setDirty(void);
  const char *currFS(void);

#ifdef USE_AUDIO
  void Sound(uint8_t n);
  void setVolume(uint8_t volume);
  void Play(const char* directory, const char* fileName);
  void Pause(void);
  void Resume(void);
  uint32_t Music_Duration(void);
  uint32_t Music_Elapsed(void);
  uint16_t Music_Energy(void);
  bool m_bPlaying;
  uint8_t m_volume = 100;
#endif

private:
#ifdef USE_AUDIO
  void Audio_Init(void);
#endif
  
  bool m_bCardIn;
  bool m_bSDActive;
  bool m_bDirty;
#ifdef USE_SDCARD
  char m_sdPath[maxPathLength * 2];
#define FILELIST_CNT 32
  FileEntry SDList[FILELIST_CNT];
#endif
};

extern Media media;

#endif // MEDIA_H
