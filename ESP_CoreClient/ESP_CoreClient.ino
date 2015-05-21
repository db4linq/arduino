/*
 *  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
 *
 *  You need to get streamId and privateKey at data.sparkfun.com and paste them
 *  below. Or just customize this script to talk to other HTTP servers.
 *
 */
#include "DHT.h"
#include <ESP8266WiFi.h>

#define DHTPIN 16 
#define DHTTYPE DHT11
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

const char* ssid     = "see_dum";
const char* password = "0863219053";
const char* host = "103.22.180.136";
String deviceId = "123456789";

float Humidity = 0;
float Temperature = 0;

DHT dht(DHTPIN, DHTTYPE, 15);
WiFiClient client;
const int httpPort = 5683;

unsigned long previousMillis = 0;        // will store last time LED was updated
// constants won't change :
const long interval = 5000;

int io_map[] = {16, 5, 4, 0, 2, 14, 12, 13, 15, 3, 1, 9, 10};

void function(const char *funcKey, int (*pFunc)(String paramString));
void variable(const char *varKey, void *userVar);
int getVarIndex(const char *varKey, int len);
int getFuncIndex(const char *funcKey, int len);


void setup() {
  Serial.begin(115200);
  delay(10);
  
  pinMode(io_map[5], OUTPUT);
  pinMode(io_map[6], OUTPUT);
  pinMode(io_map[7], OUTPUT);
  
  dht.begin();

  // We start by connecting to a WiFi network

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
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.subnetMask());

  Serial.print("connecting to ");
  Serial.println(host);  
  if (!client.connect("103.22.180.136", 5683)) {
    Serial.println("connection failed");
    while(1){};
  }
  
  client.print("{ \"type\": 1, \"data\": \"" + deviceId + "\", \"name\": \"Arduino-ESP\"}");
  
  variable("temperature", &Temperature);
  variable("humidity", &Humidity);
  function("toggle", toggle);
}

void loop() {  
  if (client.connected()){
    //do_loop();    
    if(client.available()){
      String line = readString(); //client.readStringUntil('\r');
      process(line); 
    }
  }else{
    Serial.println("Client  disconnected...");
    while(1);
  }
}
int User_Var_Count;
int User_Func_Count;

void variable(const char *varKey, void *userVar)
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
    //User_Var_Lookup_Table[User_Var_Count].userVarType = userVarType;
    memset(User_Var_Lookup_Table[User_Var_Count].userVarKey, 0, USER_VAR_KEY_LENGTH);
    memcpy(User_Var_Lookup_Table[User_Var_Count].userVarKey, varKey, USER_VAR_KEY_LENGTH);
    User_Var_Count++;
    Serial.print("Add Variable: ");
    Serial.println(User_Var_Count);
  }
}

void function(const char *funcKey, int (*pFunc)(String paramString))
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

int toggle(String param){
  Serial.print("Toggle Called: ");
  Serial.println(param);
  int pin = io_map[param.toInt()];
  digitalWrite(pin, !digitalRead(pin));
  
  return digitalRead(pin);
}


void process(String value){
  char *buf; 
  
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
    Serial.print("Params: ");
    Serial.println(params);
    
    Serial.print("Func Index: "); 
    char func_buf[USER_FUNC_KEY_LENGTH];
    Serial.print(".");
    func.toCharArray(func_buf, func.length()); 
    Serial.print(sizeof(func_buf));
    Serial.print(" ");
    int i = getFuncIndex((const char *)func_buf, func.length()-1); 
    Serial.print("."); 
    Serial.println(i);
    
    if (i != -1){
      int x = User_Func_Lookup_Table[i].pUserFunc(params);
      Serial.print("Return Value: ");
      Serial.println(x);
      String json = "{ \"type\": 3, \"data\": { \"id\": " + String(2) + ", \"value\": " ;
      json = json + String(x) + "} }";
      client.print(json);
    }
    
  }else if (_type == 'V'){
    Serial.print("Var Index: ");
    char var_buf[USER_VAR_KEY_LENGTH];
    Serial.print(".");
    func.toCharArray(var_buf, func.length()-2);
    Serial.print(sizeof(var_buf));
    Serial.print(" ");
    int i = getVarIndex((const char *)var_buf, func.length()-3); 
    Serial.print(" . ");
    Serial.println(i);
    
    if (i != -1){
      Serial.print("UserVar: ");
      Serial.println( *((float*)User_Var_Lookup_Table[i].userVar) ); 
      
      String json = "{ \"type\": 2, \"data\": { \"id\": " + String(2) + ", \"value\": ";
      json = json + String(*((float*)User_Var_Lookup_Table[i].userVar)) + "} }"; 
      client.print(json);
    }    
  }
}

int getVarIndex(const char *varKey, int len){
  for (int i = 0; i < User_Var_Count; ++i)
  {
    int x = strncmp(User_Var_Lookup_Table[i].userVarKey, varKey, len);
    Serial.println(x);
    if (0 == x)
    {
	return i;
    }
  }  
  return -1;
}

int getFuncIndex(const char *funcKey, int len){
  for (int i = 0; i < User_Var_Count; ++i)
  {
    int x = strncmp(User_Func_Lookup_Table[i].userFuncKey, funcKey, len);
    Serial.println(x);
    if (0 == x)
    {
	return i;
    }
  }   
  return -1;
}

float _startMillis = 0;
int _timeout = 10;
int timedRead()
{
  int c;
  _startMillis = millis();
  do {
    c = client.read();
    if (c >= 0) return c;
  } while(millis() - _startMillis < _timeout);
  return -1;     // -1 indicates timeout
}

String readString()
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

String readStringUntil(char terminator)
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
    //Serial.print("Humidity: "); 
    //Serial.print(Humidity);
    //Serial.print(" %\t");
    //Serial.print("Temperature: "); 
    //Serial.print(Temperature);
    //Serial.print(" *C ");
    //Serial.print(f);
    //Serial.print(" *F\t");
    //Serial.print("Heat index: ");
    //Serial.print(hi);
    //Serial.println(" *F");
     
  } 
}

