// URI builder. No encoding, so only simple strings

class uriString
{
public:
  uriString(const char *pPath = NULL)
  {
    m_cnt = 0;
    s = pPath;
  }
        
  String string(void)
  {
    return s;
  }

  void addSeperator(void)
  {
    if(m_cnt) s += "&";
    else s += "?";
  }

  void Param(const char *key, int iVal)
  {
    addSeperator();
    s += key;
    s += "=";
    s += iVal;
    m_cnt++;
  }

  void Param(const char *key, uint32_t iVal)
  {
    addSeperator();
    s += key;
    s += "=";
    s += iVal;
    m_cnt++;
  }

  void Param(const char *key, long int iVal)
  {
    addSeperator();
    s += key;
    s += "=";
    s += iVal;
    m_cnt++;
  }

  void Param(const char *key, float fVal)
  {
    addSeperator();
    s += key;
    s += "=";
    s += fVal;
    m_cnt++;
  }
  
  void Param(const char *key, bool bVal)
  {
    addSeperator();
    s += key;
    s += "=";
    s += bVal ? 1:0;
    m_cnt++;
  }
  
  void Param(const char *key, const char *sVal)
  {
    addSeperator();
    s += key;
    s += "=";
    s += sVal;
    m_cnt++;
  }
  
  void Param(const char *key, String sVal)
  {
    addSeperator();
    s += key;
    s += "=";
    s += sVal;
    m_cnt++;
  }

protected:
  String s;
  int m_cnt;
};
