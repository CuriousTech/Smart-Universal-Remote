
class jsonString
{
public:
  jsonString(const char *pLabel = NULL)
  {
    m_cnt = 0;
    s = String("{");
    if(pLabel)
    {
      s += "\"cmd\":\"";
      s += pLabel, s += "\",";
    }
  }
        
  String Close(void)
  {
    s += "}";
    return s;
  }

  void Var(const char *key, int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, uint32_t iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, long int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, float fVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += fVal;
    m_cnt++;
  }
  
  void Var(const char *key, bool bVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += bVal ? 1:0;
    m_cnt++;
  }
  
  void Var(const char *key, const char *sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }
  
  void Var(const char *key, String sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }

  void Array(const char *key, String sVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += "\"";
      s += sVal[i];
      s += "\"";
    }
    s += "]";
    m_cnt++;
  }

  void Array(const char *key, uint16_t iVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += iVal[i];
    }
    s += "]";
    m_cnt++;
  }

  void Array(const char *key, uint32_t iVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += iVal[i];
    }
    s += "]";
    m_cnt++;
  }

  void Array(const char *key, float fVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      if(fVal[i] == (int)fVal[i]) // reduces size
        s += (int)fVal[i];
      else
        s += fVal[i];
    }
    s += "]";
    m_cnt++;
  }

  void Array(const char *key, FileEntry entry[], bool bAddParent)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";

    if(bAddParent)
      s += "[\"..\",0,1,0],";
    for(int i = 0; entry[i].Name[0]; i++)
    {
      if(i) s += ",";
      s += "[\"";
      s += entry[i].Name;
      s += "\",";
      s += entry[i].Size;
      s += ",";
      s += entry[i].bDir;
      s += ",";
      s += entry[i].Date;
      s += "]";
    }
    s += "]";
    m_cnt++;
  }

protected:
  String s;
  int m_cnt;
};
