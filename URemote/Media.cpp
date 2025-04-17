#include "Media.h"
#include "eeMem.h"
#include "jsonString.h"

extern void WsSend(String s);

#ifdef USE_AUDIO
Audio audio;
#endif

Media::Media()
{ 
}

void Media::service()
{
#ifdef USE_AUDIO
  audio.loop();
#endif

#ifdef USE_SDCARD
  static File Path;
  static File file;
  static uint16_t fileCount;

  // non-blocking dir list because this can take a while
  if(!m_bCardIn)
    return;

  if(m_bDirty)
  {
    m_bDirty = false;
    if(Path = EXTERNAL_FS.open(m_sdPath))
    {
      memset(SDList, 0, sizeof(SDList));
      file = Path.openNextFile();
    }
    fileCount = 0;
  }

  if(Path && file)
  {
    strncpy(SDList[fileCount].Name, file.name(), maxPathLength - 1);
    SDList[fileCount].Size = file.size();
    SDList[fileCount].bDir = file.isDirectory();
    SDList[fileCount].Date = file.getLastWrite();
    fileCount++;
    file = Path.openNextFile();
    if(!file || fileCount >= FILELIST_CNT - 1 )
    {
      file.close();
      Path.close();

      jsonString js("files");
      js.Array("list", SDList, m_sdPath[1] ? true:false );
      WsSend(js.Close());
    }
  }
#endif
}

void Media::init()
{

#ifdef USE_AUDIO
  Audio_Init();
#endif
  
  INTERNAL_FS.begin(true);

#ifdef USE_SDCARD
 #if (USER_SETUP_ID==303) // 240x320 2.8"
  pinMode(SD_D3_PIN, OUTPUT);
  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN,-1,-1,-1);
  digitalWrite(SD_D3_PIN, HIGH); //enable
  if (!SD_MMC.begin("/sdcard", true, true) )
    return;
 #elif (USER_SETUP_ID==304) // 240x320 2"
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS))
    return;
 #endif

  uint8_t cardType = EXTERNAL_FS.cardType();

  if(cardType == CARD_NONE || cardType == CARD_UNKNOWN)
    return;

  m_bCardIn = true;
  strcpy(m_sdPath, "/"); // start with root
  setDirty();
#endif
}

bool Media::SDCardAvailable()
{
#ifdef USE_SDCARD
  return m_bCardIn;
#else
  return false;
#endif
}

const char *Media::currFS()
{
  return m_bSDActive ? "SDCard" : "Internal";
}

void Media::setFS(char *pszValue)
{
#ifdef USE_SDCARD
  m_bSDActive = !strcmp(pszValue, "SD"); // For now, just use SD
#endif
  setPath("/");
}

uint32_t Media::freeSpace()
{
  uint32_t nFree;

  if(m_bSDActive)
  {
#ifdef USE_SDCARD
    if(m_bCardIn)
      nFree = EXTERNAL_FS.totalBytes() - EXTERNAL_FS.usedBytes();
#endif
  }
  else
  {
    nFree = INTERNAL_FS.totalBytes() - INTERNAL_FS.usedBytes();
  }
  return nFree;
}

void Media::setPath(char *szPath)
{
  if(m_bSDActive)
  {
#ifdef USE_SDCARD
    strncpy(m_sdPath, szPath, sizeof(m_sdPath)-1);
    setDirty();

    String s = "{\"cmd\":\"files\",\"list\":[";
    if(szPath[1])
      s += "[\"..\",0,1,0]";
    s += "]}";
    WsSend(s); // fill in for an empty dir
#endif
  }
  else
  {
    File Path = INTERNAL_FS.open(szPath);
    if (!Path)
      return;

    uint16_t fileCount = 0;
    File file = Path.openNextFile();
  
    String s = "{\"cmd\":\"files\",\"list\":[";
    if(szPath[1])
    {
      s += "[\"..\",0,1,0]";
      fileCount++;
    }

    while (file)
    {
      if(fileCount)
        s += ",";
      s += "[\"";
      s += file.name();
      s += "\",";
      s += file.size();
      s += ",";
      s += file.isDirectory();
      s += ",";
      s += file.getLastWrite();
      s += "]";
      fileCount++;
      file = Path.openNextFile();
    }
    Path.close();
    s += "]}";
    WsSend(s);
  }
}

void Media::deleteFile(char *pszFilename)
{
  if(m_bSDActive)
  {
#ifdef USE_SDCARD
    if(!m_bCardIn)
      return;
    EXTERNAL_FS.remove(pszFilename);
    m_bDirty = true;
#endif
  }
  else
  {
    INTERNAL_FS.remove(pszFilename);
  }
}

File Media::createFile(String sFilename)
{
#ifdef USE_SDCARD
  if(m_bSDActive)
    return EXTERNAL_FS.open(sFilename, "w");
#endif
  return INTERNAL_FS.open(sFilename, "w");
}

void Media::createDir(char *pszName)
{
  if(m_bSDActive)
  {
#ifdef USE_SDCARD
   if( !EXTERNAL_FS.exists(pszName) )
    EXTERNAL_FS.mkdir(pszName);
#endif
  }
  else
  {
    if( !INTERNAL_FS.exists(pszName) )
      INTERNAL_FS.mkdir(pszName);
  }
}

void Media::setDirty()
{
  if(m_bCardIn)
    m_bDirty = true;
}

#ifdef USE_AUDIO
void Media::Audio_Init()
{
  // Audio
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  setVolume(m_volume);
}

void Media::setVolume(uint8_t volume)
{
  if(volume > Volume_MAX )
    return;
  m_volume = volume;
  audio.setVolume(volume * 21 / 100); // 0...21
}

const char *pSounds[] = {
  "/click.wav",
  "/alert.wav",
};

// Play a sound from the internal FS
void Media::Sound(uint8_t n)
{
  if( !INTERNAL_FS.exists(pSounds[n]) )
    return;

  if(audio.isRunning())
    return;

  if(audio.connecttoFS(INTERNAL_FS, pSounds[n]))
  {
    Pause();
    Resume();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void Media::Play(const char* directory, const char* fileName)
{
  static const char *pLast;

  if(fileName == pLast && audio.isRunning())
  {
    audio.pauseResume();
    pLast = NULL;
    return;
  }
  pLast = fileName;

  // SD Card
  char filePath[maxPathLength];
  if(!strcmp(directory, "/"))
    snprintf(filePath, maxPathLength, "%s%s", directory, fileName);   
  else
    snprintf(filePath, maxPathLength, "%s/%s", directory, fileName);

  if(m_bCardIn)
  {
    if ( EXTERNAL_FS.exists(filePath))
    {
      audio.pauseResume();
      vTaskDelay(pdMS_TO_TICKS(100));
      if( !audio.connecttoFS(EXTERNAL_FS,(char*)filePath) )
      {
  //      ets_printf("Music Read Failed\r\n");
        return;
      }
  //    ets_printf("playing\r\n");
      Pause();
      Resume();
      vTaskDelay(pdMS_TO_TICKS(100));
      return;
    }
  }

  if ( INTERNAL_FS.exists(filePath))
  {
    audio.pauseResume();
    if( !audio.connecttoFS(INTERNAL_FS, (char*)filePath) )
    {
//      ets_printf("Music Read Failed\r\n");
      return;
    }
    Pause();
    Resume();
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }
}

void Media::Pause()
{
  if(!audio.isRunning())
    return;
  audio.pauseResume();
}

void Media::Resume()
{
  if(audio.isRunning())
    return;
  audio.pauseResume();
}

uint32_t Media::Music_Duration()
{
  uint32_t Audio_duration = audio.getAudioFileDuration(); 
/*
  if(Audio_duration > 60)
    ets_printf("Audio duration is %d minutes and %d seconds\r\n", Audio_duration/60, Audio_duration%60);
  else{
    if(Audio_duration != 0)
      ets_printf("Audio duration is %d seconds\r\n", Audio_duration);
    else
      ets_printf("Fail : Failed to obtain the audio duration.\r\n");
  }*/
  vTaskDelay(pdMS_TO_TICKS(10));

  return Audio_duration;
}

uint32_t Media::Music_Elapsed()
{
  return audio.getAudioCurrentTime();
}

uint16_t Media::Music_Energy()
{
  return audio.getVUlevel();
}
#endif
