#include <Arduino.h>
#include "ESP8266WiFi.h"
#include "DHT.h"

#define USER_VAR_MAX_COUNT  5
#define USER_FUNC_MAX_COUNT  5
#define USER_VAR_KEY_LENGTH  50
#define USER_FUNC_KEY_LENGTH  50
#define USER_FUNC_ARG_LENGTH  50

typedef enum
{
  BOOLEAN = 1, INT = 2, STRING = 4, DOUBLE = 9
} Core_Data_TypeDef;

struct User_Var_Lookup_Table_t
{
  void *userVar;
  char userVarKey[USER_VAR_KEY_LENGTH];
  Core_Data_TypeDef userVarType;
} User_Var_Lookup_Table[USER_VAR_MAX_COUNT];

struct User_Func_Lookup_Table_t
{
	int (*pUserFunc)(String userArg);
	char userFuncKey[USER_FUNC_KEY_LENGTH];
	char userFuncArg[USER_FUNC_ARG_LENGTH];
	int userFuncRet;
	bool userFuncSchedule;
} User_Func_Lookup_Table[USER_FUNC_MAX_COUNT];

class CoreClass {
  private:
    WiFiClient client;
    float _startMillis = 0;
    int _timeout = 10;
    const char* _host = "103.22.180.136";
    int _httpPort = 5683; 
    int User_Var_Count = 0;
    int User_Func_Count = 0;
    int _id = 1;
    String _name;
    
    int timedRead();
    String readString();
    String readStringUntil(char terminator);
    void process(String value);
    int getVarIndex(const char *varKey, int len);
    int getFuncIndex(const char *funcKey, int len);
    
  public:
    CoreClass();
    void setId(int value);
    int getId();
    void setName(String value);
    String getName();
    int Init(const char* ssid, const char* password);
    void function(const char *funcKey, int (*pFunc)(String paramString));
    void variable(const char *varKey, void *userVar, Core_Data_TypeDef userVarType);
    void run();
};

// Private
int CoreClass::timedRead()
{
  int c;
  _startMillis = millis();
  do {
    c = client.read();
    if (c >= 0) return c;
  } while(millis() - _startMillis < _timeout);
  return -1;     // -1 indicates timeout  
}
String CoreClass::readString()
{
  String ret;
  int c = timedRead();
  while (c >= 0)
  {
    ret += (char)c;
    c = timedRead();
  }
  return ret;  
}
String CoreClass::readStringUntil(char terminator)
{
  String ret;
  int c = timedRead();
  while (c >= 0 && c != terminator)
  {
    ret += (char)c;
    c = timedRead();
  }
  return ret;  
}
void CoreClass::process(String value)
{
  Serial.print(value);
  char _type = value.charAt(0);
  value.setCharAt(1, ';');
  Serial.print("Type: ");
  Serial.println(_type); 
  
  int colonPosition = value.indexOf('|');
  String  func = value.substring(2, colonPosition);
  Serial.print("Function name: ");
  Serial.println(func);
  if (_type == 'F'){
    colonPosition++;
    String params = value.substring(colonPosition);
    //Serial.print("Params: ");
    //Serial.println(params);
    //Serial.print("Func Index: "); 
    char func_buf[USER_FUNC_KEY_LENGTH];
    //Serial.print(".");
    func.toCharArray(func_buf, func.length()); 
    //Serial.print(sizeof(func_buf));
    //Serial.print(" ");
    int i = getFuncIndex((const char *)func_buf, func.length()-1); 
    //Serial.print("."); 
    //Serial.println(i);
    
    int x = User_Func_Lookup_Table[i].pUserFunc(params);
    Serial.print("Return Value: ");
    Serial.println(x);
    
    String json = "{ \"type\": 3, \"data\": { \"id\": " + String(_id) + ", \"value\": " ;
    json = json + String(x) + "} }";
    client.print(json);
    
  }else if (_type == 'V'){
    //Serial.print("Var Index: ");
    char var_buf[USER_VAR_KEY_LENGTH];
    //Serial.print(".");
    func.toCharArray(var_buf, func.length()-2);
    //Serial.print(sizeof(var_buf));
    //Serial.print(" ");
    
    int i = getVarIndex((const char *)var_buf, func.length()-3); 
    //Serial.print(" . ");
    //Serial.println(i);
    
    if (i != -1){
      Serial.print("UserVar: "); 
      Core_Data_TypeDef _typeDef = User_Var_Lookup_Table[i].userVarType;
      String json = "{ \"type\": 2, \"data\": { \"id\": " + String(_id) + ", \"value\": ";
      switch(_typeDef){
        case BOOLEAN:
          Serial.println( *((bool*)User_Var_Lookup_Table[i].userVar) );
          if (*((bool*)User_Var_Lookup_Table[i].userVar) == true){
            json = json + "\"true\"";
          }else{
            json = json + "\"false\"";
          }
          break;
        case INT:
          Serial.println( *((int*)User_Var_Lookup_Table[i].userVar) );
          json = json + String(*((int*)User_Var_Lookup_Table[i].userVar));
          break;
        case STRING:
          Serial.println( *((String*)User_Var_Lookup_Table[i].userVar) );
          json = json + "\"" + *((String*)User_Var_Lookup_Table[i].userVar) + "\"";
          break;
        case DOUBLE:
          Serial.println( *((float*)User_Var_Lookup_Table[i].userVar) ); 
          json = json + String(*((float*)User_Var_Lookup_Table[i].userVar));
          break; 
      }
      json = json + "} }";
      client.print(json);
    }    
  }  
}
int CoreClass::getVarIndex(const char *varKey, int len)
{
  for (int i = 0; i < User_Var_Count; ++i)
  {
    int x = strncmp(User_Var_Lookup_Table[i].userVarKey, varKey, len);
    //Serial.println(x);
    if (0 == x)
    {
	return i;
    }
  }  
  return -1;  
}

int CoreClass::getFuncIndex(const char *funcKey, int len)
{
  for (int i = 0; i < User_Var_Count; ++i)
  {
    int x = strncmp(User_Func_Lookup_Table[i].userFuncKey, funcKey, len);
    //Serial.println(x);
    if (0 == x)
    {
	return i;
    }
  }   
  return -1;  
}

// ****************************************
// Public
// ****************************************
CoreClass::CoreClass()
{
  
}
 void CoreClass::run()
{
  if (client.connected()){  
    if(client.available()){
      String line = readString(); //client.readStringUntil('\r');
      process(line); 
    }
  }else{
    Serial.println("Client  disconnected...");
    while(1){};
  }   
}

void CoreClass::setId(int value)
{
  _id = value;
}
int CoreClass::getId()
{
  return _id;
}
void CoreClass::setName(String value)
{
  _name = value;
}
String CoreClass::getName()
{
  return _name;
}
int CoreClass::Init(const char* ssid, const char* password)
{
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  
  Serial.print("connecting to ");
  Serial.println(_host);  
  if (!client.connect(_host, _httpPort)) { 
    return 0;
  }
  delay(100);
  // { "type": 1, "data": "'.. node.chipid() .. '", "name": "'..node_name..'"}
  return 1;  
}

void CoreClass::function(const char *funcKey, int (*pFunc)(String paramString))
{
  int i = 0;
  if(NULL != pFunc && NULL != funcKey)
  {
    if(User_Func_Count == USER_FUNC_MAX_COUNT)
      return;

    for(i = 0; i < User_Func_Count; i++)
    {
	if(User_Func_Lookup_Table[i].pUserFunc == pFunc && (0 == strncmp(User_Func_Lookup_Table[i].userFuncKey, funcKey, USER_FUNC_KEY_LENGTH)))
	{
	  return;
	}
    }

    User_Func_Lookup_Table[User_Func_Count].pUserFunc = pFunc;
    memset(User_Func_Lookup_Table[User_Func_Count].userFuncArg, 0, USER_FUNC_ARG_LENGTH);
    memset(User_Func_Lookup_Table[User_Func_Count].userFuncKey, 0, USER_FUNC_KEY_LENGTH);
    memcpy(User_Func_Lookup_Table[User_Func_Count].userFuncKey, funcKey, USER_FUNC_KEY_LENGTH);
    User_Func_Lookup_Table[User_Func_Count].userFuncSchedule = false;
    User_Func_Count++;
    Serial.print("Add Function: ");
    Serial.println(User_Func_Count);
  }  
}
void CoreClass::variable(const char *varKey, void *userVar, Core_Data_TypeDef userVarType)
{
  if (NULL != userVar && NULL != varKey)
  {
    if (User_Var_Count == USER_VAR_MAX_COUNT)
      return;

    for (int i = 0; i < User_Var_Count; i++)
    {
      if (User_Var_Lookup_Table[i].userVar == userVar &&
          (0 == strncmp(User_Var_Lookup_Table[i].userVarKey, varKey, USER_VAR_KEY_LENGTH)))
      {
        return;
      }
    }

    User_Var_Lookup_Table[User_Var_Count].userVar = userVar;
    User_Var_Lookup_Table[User_Var_Count].userVarType = userVarType;
    memset(User_Var_Lookup_Table[User_Var_Count].userVarKey, 0, USER_VAR_KEY_LENGTH);
    memcpy(User_Var_Lookup_Table[User_Var_Count].userVarKey, varKey, USER_VAR_KEY_LENGTH);
    User_Var_Count++;
    Serial.print("Add Variable: ");
    Serial.println(User_Var_Count);
  }  
}

CoreClass core = CoreClass();

const char* ssid     = "see_dum";
const char* password = "0863219053"; 
float Humidity = 0;
float Temperature = 0;
unsigned long previousMillis = 0;
const long interval = 5000;  
DHT dht(13, DHT11, 15);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Init DHT");
  dht.begin();
  // put your setup code here, to run once:
  Serial.println("Init Core");
  if (!core.Init(ssid, password)){
    Serial.println("connection failed");
    while(1){};
  }
  Serial.println("Create variable");
  core.variable("temperature", &Temperature, DOUBLE);
  core.variable("humidity", &Humidity, DOUBLE);
  Serial.println("Create function");
  core.function("toggle", toggle);
}

void loop() {
  do_loop();
  
  core.run();
}

int toggle(String param){
  Serial.print("Toggle Called: ");
  Serial.println(param);
  return 100;
}

void do_loop(){
  unsigned long currentMillis = millis();
 
  if(currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED 
    previousMillis = currentMillis;   
    Humidity = dht.readHumidity();
    // Read temperature as Celsius
    Temperature = dht.readTemperature();
    // Read temperature as Fahrenheit
    float f = dht.readTemperature(true);
    if (isnan(Humidity) || isnan(Temperature) || isnan(f)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
    
    float hi = dht.computeHeatIndex(f, Humidity);
    Serial.print("Humidity: "); 
    Serial.print(Humidity);
    Serial.print(" %\t");
    Serial.print("Temperature: "); 
    Serial.print(Temperature);
    Serial.print(" *C ");
    Serial.print(f);
    Serial.print(" *F\t");
    Serial.print("Heat index: ");
    Serial.print(hi);
    Serial.println(" *F");
  } 
}


