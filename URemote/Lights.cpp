
// lights/dimmers/switches control for CuriousTech/Dimmer repo

#include "Lights.h"
#include "Display.h"

extern Display display;

Lights::Lights()
{
  m_ac.onConnect([](void* obj, AsyncClient* c) { (static_cast<Lights*>(obj))->_onConnect(c); }, this);
  m_ac.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<Lights*>(obj))->_onDisconnect(c); }, this);
  m_ac.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<Lights*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);
  m_ac.setRxTimeout(3); // give it 3 seconds
}

void Lights::init()
{
  m_bQuery = true;
  IPAddress ip(ee.lightIp);
  send(ip, 80, "/mdns");
}

bool Lights::setSwitch(int8_t nSwitch, bool bPwr, uint8_t nLevel)
{
  if(nSwitch != -1) // -1 = last used (dimmer)
    m_nSwitch = nSwitch - 1; // ID starts at 1
  m_bQuery = false;

  if(ee.lights[m_nSwitch].ip[0] == 0) // no IP
    return false;

  IPAddress ip(ee.lights[m_nSwitch].ip);

  String s = "/wifi?key=";
  s += ee.iotPassword;
  s += "&pwr0=";
  s += bPwr ? 1:0;
  if(nLevel)
  {
    s += "&level=";
    s += nLevel << 1;
  }
  if(send(ip, 80, s.c_str()) )
  {
    m_bOn[m_nSwitch][0] = bPwr;
    if(nLevel)
      m_nLevel[m_nSwitch] = nLevel; // valid level is 1-200
    return true;
  }
  return false;
}

bool Lights::getSwitch(int8_t nSwitch)
{
  if(nSwitch < 1) return false; // 1 = first
  return m_bOn[nSwitch - 1][0];
}

bool Lights::send(IPAddress serverIP, uint16_t port, const char *pURI)
{
  if(m_ac.connected() || m_ac.connecting())
    return false;
  if(m_pBuffer == NULL)
  {
    m_pBuffer = new char[LBUF_SIZE];
    if(m_pBuffer) m_pBuffer[0] = 0;
    else{
      m_status = LI_MemoryError;
      return false;
    }
  }
  m_status = LI_Busy;
  strcpy(m_pBuffer, pURI);
  if(!m_ac.connect(serverIP, port))
  {
    m_status = LI_ConnectError;
    return false;
  }
  return true;
}

int Lights::checkStatus()
{
  if(m_status == LI_Done)
  {
    m_status = LI_Idle;
    return LI_Done;
  }
  return m_status;
}

void Lights::_onConnect(AsyncClient* client)
{
  if(m_pBuffer == NULL)
    return;
  String path = "GET ";
  path += m_pBuffer;
  path += " HTTP/1.1\n"
    "Host: ";
  path += client->remoteIP().toString();
  path += "\n"
    "Connection: close\n"
    "Accept: */*\n\n";

  m_ac.add(path.c_str(), path.length());
  m_bufIdx = 0;
}

// build file in chunks
void Lights::_onData(AsyncClient* client, char* data, size_t len)
{
  if(m_pBuffer == NULL || m_bufIdx + len >= LBUF_SIZE)
    return;
  memcpy(m_pBuffer + m_bufIdx, data, len);
  m_bufIdx += len;
  m_pBuffer[m_bufIdx] = 0;
}

void Lights::_onDisconnect(AsyncClient* client)
{
  (void)client;

  char *p = m_pBuffer;
  m_status = LI_Done;
  if(p == NULL)
    return;
  if(m_bufIdx == 0)
  {
    delete m_pBuffer;
    m_pBuffer = NULL;
    return;
  }

  const char *jsonList[] = { // switch control mode
    "ch",
    "on0",
    "on1",
    "level",
    NULL
  };

  const char *jsonQ[] = { // query mode
    "*",     // 0
    NULL
  };

  if(p[0] != '{')
    while(p[4]) // skip all the header lines
    {
      if(p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n')
      {
        p += 4;
        break;
      }
      p++;
    }

  if(m_bQuery)
    processJson(p, jsonQ);
  else
    processJson(p, jsonList);
  delete m_pBuffer;
  m_pBuffer = NULL;
}

void Lights::callback(int8_t iName, char *pName, int32_t iValue, char *psValue)
{
  String s;
  
  switch(iName)
  {
    case -1: // wildcard
      {
        uint8_t nIdx;
        for(nIdx = 0; nIdx < EE_LIGHT_CNT; nIdx++)
        {
          if(ee.lights[nIdx].szName[0] == 0) // new slot
            break;
          else if(!strcmp(ee.lights[nIdx].szName, pName) ) // name already exists
            break;
        }
        if(nIdx == EE_LIGHT_CNT) // overflow
          break;
        strncpy(ee.lights[nIdx].szName, pName, sizeof(ee.lights[0].szName) );

        char *pIP = psValue + 1;
        char *pEnd = pIP;
        while(*pEnd && *pEnd != '\"') pEnd++;
        if(*pEnd == '\"'){*pEnd = 0; pEnd += 2;}
        IPAddress ip;
        ip.fromString(pIP);

        ee.lights[nIdx].ip[0] = ip[0];
        ee.lights[nIdx].ip[1] = ip[1];
        ee.lights[nIdx].ip[2] = ip[2];
        ee.lights[nIdx].ip[3] = ip[3];

        while(*pEnd && *pEnd != ',') pEnd++; // skip channel count
        m_bOn[nIdx][0] = atoi(pEnd); // channel 0 state
      }
      break;
    case 0: // ch
      break;
    case 1: // on0
      m_bOn[m_nSwitch][0] = iValue;
      break;
    case 2: // on1
      m_bOn[m_nSwitch][1] = iValue;
      break;
    case 3: // level
      m_nLevel[m_nSwitch] = iValue;
      display.setSliderValue(SFN_Lights, m_nLevel[m_nSwitch] );
      break;
  }
}

void Lights::processJson(char *p, const char **jsonList)
{
  char *pPair[2]; // param:data pair
  int8_t brace = 0;
  int8_t bracket = 0;
  int8_t inBracket = 0;
  int8_t inBrace = 0;

  while(*p)
  {
    p = skipwhite(p);
    if(*p == '{'){p++; brace++;}
    if(*p == '['){p++; bracket++;}
    if(*p == ',') p++;
    p = skipwhite(p);

    bool bInQ = false;
    if(*p == '"'){p++; bInQ = true;}
    pPair[0] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }else
    {
      while(*p && *p != ':') p++;
    }
    if(*p != ':')
      return;

    *p++ = 0;
    p = skipwhite(p);
    bInQ = false;
    if(*p == '{') inBrace = brace+1; // data: {
    else if(*p == '['){p++; inBracket = bracket+1;} // data: [
    else if(*p == '"'){p++; bInQ = true;}
    pPair[1] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }else if(inBrace)
    {
      while(*p && inBrace != brace){
        p++;
        if(*p == '{') inBrace++;
        if(*p == '}') inBrace--;
      }
      if(*p=='}') p++;
    }else if(inBracket)
    {
      while(*p && inBracket != bracket){
        p++;
        if(*p == '[') inBracket++;
        if(*p == ']') inBracket--;
      }
      if(*p == ']') *p++ = 0;
    }else while(*p && *p != ',' && *p != '\r' && *p != '\n') p++;
    if(*p) *p++ = 0;
    p = skipwhite(p);
    if(*p == ',') *p++ = 0;

    inBracket = 0;
    inBrace = 0;
    p = skipwhite(p);

    if(pPair[0][0])
    {
      if(jsonList[0][0] == '*') // wildcard
      {
          int32_t n = atol(pPair[1]);
          if(!strcmp(pPair[1], "true")) n = 1; // bool case
          callback(-1, pPair[0], n, pPair[1]);
      }
      else for(int i = 0; jsonList[i]; i++)
      {
        if(!strcmp(pPair[0], jsonList[i]))
        {
          int32_t n = atol(pPair[1]);
          if(!strcmp(pPair[1], "true")) n = 1; // bool case
          callback(i, pPair[0], n, pPair[1]);
          break;
        }
      }
    }

  }
}

char *Lights::skipwhite(char *p)
{
  while(*p == ' ' || *p == '\t' || *p =='\r' || *p == '\n')
    p++;
  return p;
}
