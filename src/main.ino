#include "config.h" //change to change envrionment
Config conf;
#include "secrets.h"
Secrets sec;
//////////////////////////////////////////////
////////////CONFIG////////////////////////////
static String name = conf.name; 
static String ver = "1_10";

//value for these configkeys will be updated from google sheets config sheet, see getconfig()
int pinginterval=1; //the main loop interval, sec
int update_interval=5; //pinginterval*update_interval = how often check the update server for
float temp_target = conf.temp_target; // The heater (relay module) will switch off at greater than this temperature
float heating_start = conf.heating_start; //The heater (relay module) will switch on at lesser than this temperature 


const String update_server = sec.update_server; //at this is url is the python flask update server, which I wrote

#define USE_SERIAL Serial

const String GScriptId = sec.gID; //This is the secret ID of the Google script app which connects to the Google Spreadsheets
const String data_sheet = conf.data_sheet; //name of the sheet on the Spreadsheet where the data will be logged
const String log_sheet = conf.log_sheet; //name of the sheet on the Spreadsheet where the events will be logged
const String discord_chanel = sec.discord_chanel; //a discord channel webhook, we send startup messages there
#define ONE_WIRE_BUS D6
#define RELAYPIN D7
////////////CONFIG////////////////////////////
//////////////////////////////////////////////

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
ESP8266WiFiMulti WiFiMulti;
WiFiClient client;
#include <OneWire.h>
OneWire ds(ONE_WIRE_BUS);

//////////////////////////////////////////////
////////////SETUP ////////////////////////////
void setup()
{


  USE_SERIAL.begin(115200);
  USE_SERIAL.setDebugOutput(true);
  USE_SERIAL.println();
  USE_SERIAL.print("name: ");
  USE_SERIAL.println(name);
  USE_SERIAL.print("ver: ");
  USE_SERIAL.println(ver);

  //test the relay
  delay(1000);
  heater_stop();
  delay(1000);
  heater_start();
  delay(1000);
  heater_stop();

  WiFiManager wifiManager;
  wifiManager.setTimeout(300);
  wifiManager.autoConnect("mocsigoncska_ap");
  USE_SERIAL.println("connected...yeey :)");

  GsheetPost(F("log"), "startup: " + name + " " + ver); // TODO: extract the main log sheetname to the config
  discordPost("startup: " + name + " " + ver);

  getconfig();
  updateFunc(name, ver);
}

////////////SETUP ////////////////////////////
//////////////////////////////////////////////

//////////////////////////////////////////////
////////////LOOP ////////////////////////////
int i = 0;
int j = 0;
void loop()
{
  USE_SERIAL.println("loop...");
  float temp = dsfunc();
  if(temp>-80){
  heater(temp);
  }
  if (pinginterval<1) pinginterval=1;
  delay(pinginterval * 1000);
  
  i++;
  if (i > 10)
  {
    i = 0;
    getconfig();
  }

  j++;
  if (j > update_interval)
  {
    j = 0;
    updateFunc(name, ver);
  }
}

////////////LOOP ////////////////////////////
//////////////////////////////////////////////

/////////////////////////////////////////////
////////////HEATER///////////////////////////
boolean heating;
void heater(float temp)
{
  USE_SERIAL.println("---------- " + String(temp) + " -> " + String(heating));
  if (temp < heating_start && heating == false)
  {
    
    heater_start();
  }
  if (temp > temp_target && heating == true)
  {
    
    heater_stop();
  }
}

void heater_start()
{
  USE_SERIAL.println("Heater start!");
  heating = true;
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, LOW);
  GsheetPost(log_sheet, "Heater start");
}
void heater_stop()  
{
  /*
  We use this method so we dont need a switch transistor to switch of the relay input, 
  we simple let the heater's pull-up to do this work. This way, we save up on hardware, but
  be carefull with soldering the relay. If you get a short between relay_5V0 and relay_input,
  that can feed 5V current to the esp8266, damaging the used pin (been there done that).
  */
  USE_SERIAL.println("Heater stop!" );
  heating = false;
  digitalWrite(RELAYPIN, HIGH);
  pinMode(RELAYPIN, INPUT);
  GsheetPost(log_sheet, "Heater stop");
}

////////////HEATER///////////////////////////
/////////////////////////////////////////////

//////////////////////////////////////////////
////////////ds18b20////////////////////////////

float dsfunc()
{
  //straight from the example program, only modified at the end

  byte i;
  byte type_s;
  byte present=0;
  byte data[12];
  byte addr[8];

  if (!ds.search(addr))
  {
    ds.reset_search();
    delay(250);
    return -98;
  }

  if (OneWire::crc8(addr, 7) != addr[7])
  {
    USE_SERIAL.println(F("CRC is not valid!"));
    return -99;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end

  delay(1000); // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read Scratchpad

  for (i = 0; i < 9; i++)
  { // we need 9 bytes
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s)
  {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10)
    {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  else
  {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00)
      raw = raw & ~7; // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20)
      raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40)
      raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  float result = ((float)raw / 16.0);
  USE_SERIAL.println(result);
  String datastring = String(result) + ";" + String(heating);
  datastring.replace(".",","); //this way, the sheet gets the correct format (and not use date)
  GsheetPost(data_sheet, datastring);
  return result;
}
////////////ds18b20////////////////////////////
//////////////////////////////////////////////

/////////////////////////////////////////////
////////////HTTPUPDATE////////////////////////
void updateFunc(String Name, String Version) 
{

  /*
  We have a custom update server, you can find the repo link in the readme for it.
      Scenario: There is a new version
        Given the server has a folder with binary named 'test'
        And it has a binary with version 0_1
        When I make a request with name: 'test' and version: '0_0'
        Then I get a response: '/static/bin/test/0_1.bin'

    Scenario: There is no new version
        Given the server has a folder with binary named 'test'
        And it has a binary with version 0_1
        When I make a request with name: 'test' and version: '0_1'
        Then I get a response: 'update not needed'

    Scenario: There is no such device name
        Given the server doesn't have a folder with name: 'notexist'
        And it has a binary with version 0_1
        When I make a request with name: 'notexist' and version: '0_0'
        Then I get a response with status code 404
  */

  HTTPClient http;

  String url = update_server + "/check?" + "name=" + Name + "&ver=" + Version;
  USE_SERIAL.print("[HTTP] check at " + url);
  if (http.begin(client, url))
  { // HTTP

    USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
    delay(10000); //wait for bootup of the server
    httpCode = http.GET();
    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = http.getString();
        USE_SERIAL.println(payload);
        if (payload.indexOf("bin") > 0)
        {
          httpUpdateFunc(update_server + payload);
        }
      }
    }
    else
    {
      USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}

void httpUpdateFunc(String update_url) 
{
  
  //this is from the core example

  if ((WiFiMulti.run() == WL_CONNECTED))
  {

    // The line below is optional. It can be used to blink the LED on the board during flashing
    // The LED will be on during download of one buffer of data from the network. The LED will
    // be off during writing that buffer to flash
    // On a good connection the LED should flash regularly. On a bad connection the LED will be
    // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
    // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

    // Add optional callback notifiers
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, update_url);
    // Or:
    //t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
      USE_SERIAL.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      USE_SERIAL.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      USE_SERIAL.println("HTTP_UPDATE_OK");
      break;
    }
  }
}

void update_started()
{
  USE_SERIAL.println("CALLBACK:  HTTP update process started");
}

void update_finished()
{
  USE_SERIAL.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total)
{
  USE_SERIAL.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err)
{
  USE_SERIAL.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}
////////////HTTPUPDATE////////////////////////
/////////////////////////////////////////////

//////////////////////////////////////////////
////////////HTTP  ////////////////////////////
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>

void GsheetPost(String sheet_name, String datastring)
{
  USE_SERIAL.println(F("POST to spreadsheet:"));
  String url = String(F("https://script.google.com/macros/s/")) + String(GScriptId) + "/exec";
  String payload = String("{\"command\": \"appendRow\", \  \"sheet_name\": \"") + sheet_name + "\", \ \"values\": " + "\"" + datastring + "\"}";

  USE_SERIAL.println(POSTTask(url, payload));
};

void discordPost(String message)
{

  String payload = "{\"content\": \"" + message + "\"}";
  String url = discord_chanel;
  USE_SERIAL.println(POSTTask(url, payload));
};

String GETTask(String url)
{
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure(); //This will set the http connection to insecure! This is not advised, but I have found no good way to use real SSL, and my application doesn't need the added security
  HTTPClient https;
  https.setFollowRedirects(true); //this is needed for the Google backend, which always redirects
  if (https.begin(*client, url))
  {
    USE_SERIAL.print(F("[HTTPS] GET "));
    USE_SERIAL.println(url);


    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.printf("[HTTPS] GET... code: %d\n", httpCode);
      if (httpCode == 302)
      {
        String redirectUrl = https.getLocation();
        https.end();
        return redirectUrl;
      }
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = https.getString();
        USE_SERIAL.println(payload);
        https.end();

        return payload;
      }
    }
    else
    {
      USE_SERIAL.print(F("[HTTPS] GET... failed, error: "));
      USE_SERIAL.println(httpCode);
      https.end();
      return "";
    }

    https.end();
  }
  else
  {
    USE_SERIAL.println(F("[HTTPS] Unable to connect"));
    return "";
  }
  return "";
};

String POSTTask(String url,  String payload)
{
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;
  if (https.begin(*client, url))
  {
    USE_SERIAL.print(F("[HTTPS] POST "));
    USE_SERIAL.print(url);
    USE_SERIAL.print(" --> ");
    USE_SERIAL.println(payload);
    https.addHeader(F("Content-Type"), F("application/json"));
    https.setFollowRedirects(true);

    int httpCode = https.POST(payload);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.print(F("[HTTPS] POST... code: "));
      USE_SERIAL.println(httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = https.getString();
        https.end();
        return payload;
      }
    }
    else
    {
      USE_SERIAL.print(F("[HTTPS] POST... failed, error: "));
      USE_SERIAL.println(httpCode);
      https.end();
      return "";
    }

    https.end();
  }
  else
  {
    USE_SERIAL.println(F("[HTTPS] Unable to connect"));
    return "";
  }
  return "";
};

////////////HTTP  ////////////////////////////
//////////////////////////////////////////////

//////////////////////////////////////////////
////////////GETCONFIG/////////////////////////



void getconfig()
{
  /*
  The config data is held by the google spreadsheet. You should find a link in the readme
  for the google apps script code which processes the spreadsheet.
  The script search for a cell in the config sheet with the same string as the http parameter, then gives back the value of the cell right next to it. 
  */
  const size_t capacity = JSON_OBJECT_SIZE(5) + 200;
  DynamicJsonDocument doc(capacity);
  String baseurl = String(F("https://script.google.com/macros/s/")) + String(GScriptId) + "/exec?";
  const String my_temp_target=name+"_"+"temp_target";
  const String my_temp_start=name+"_"+"indit";
  const String my_heating_stop=name+"_"+"stop";
  const String params = "pinginterval=0&update_interval=0&"+my_temp_target+"=0&"+my_temp_start+"=0&"+my_heating_stop+"=0";
 const String url = baseurl + params;
  String response = GETTask(url);

  if (response.length() > 1)
  {
    deserializeJson(doc, response);

    pinginterval = doc["pinginterval"];
    update_interval = doc["update_interval"];
    temp_target = doc.getMember(my_temp_target);
    heating_start = doc.getMember(my_temp_start);

    USE_SERIAL.println("Config got: \n pinginterval = " + String(pinginterval));
    USE_SERIAL.println(my_temp_target + " = " + temp_target);
    USE_SERIAL.println(my_temp_start + " = " + heating_start);
  }
}

////////////GETCONFIG/////////////////////////
//////////////////////////////////////////////

