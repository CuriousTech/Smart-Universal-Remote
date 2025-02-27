#include "Media.h"
#include "eeMem.h"
#include "jsonString.h"

Media::Media()
{ 
}

void Media::init()
{
  INTERNAL_FS.begin(true);
}

String Media::internalFileListJson(fs::FS &fs, const char* directory)
{
  File Path = fs.open(directory);
  if (!Path)
    return "";
  
  uint16_t fileCount = 0;
  File file = Path.openNextFile();

  String s = "{\"cmd\":\"files\",\"list\":[";

  while (file)
  {
    if (!file.isDirectory())
    {
      if(fileCount)
        s += ",";
      s += "[\"";
      s += file.name();
      s += "\",";
      s += file.size();
      s += ",";
      s += file.getLastWrite();
      s += "]";
      fileCount++;
    }
    file = Path.openNextFile();
  }
  Path.close();
  s += "]}";
  return s;
}

void Media::deleteIFile(char *pszFilename)
{
  String sRemoveFile = "/";
  sRemoveFile += pszFilename;
  INTERNAL_FS.remove(sRemoveFile);
}
