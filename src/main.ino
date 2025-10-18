#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <InfluxDbClient.h>
#include <OneWire.h>

#include "config.h" //change to change envrionment
Config conf;
#include "secrets.h"
Secrets sec;
//////////////////////////////////////////////
////////////CONFIG////////////////////////////
static String name = conf.name; 
static String ver = "2_5";

//value for these configkeys will be updated from InfluxDB bucket 'noszlop', see getconfig()
long pinginterval=1; //the main loop interval, sec
long update_interval=5; //pinginterval*update_interval = how often check the update server for
float temp_target = conf.temp_target; // The heater (relay module) will switch off at greater than this temperature
float heating_start_temp = conf.heating_start_temp; //The heater (relay module) will switch on at lesser than this temperature 
boolean invert_heating = conf.invert_heating; // Invert heating logic


const String update_server = sec.update_server; //at this is url is the python flask update server, which I wrote

#define USE_SERIAL Serial

#define ONE_WIRE_BUS D6
#define RELAYPIN D7

#define INFLUXDB_ORG "influxdata"
// InfluxDB 2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
#define INFLUXDB_BUCKET "noszlop"
InfluxDBClient influx_client(sec.influx_url, INFLUXDB_ORG, INFLUXDB_BUCKET, sec.influx_token);
Point influxdb_line(conf.name); //measurement name
////////////CONFIG////////////////////////////
//////////////////////////////////////////////

ESP8266WiFiMulti WiFiMulti;
WiFiClient client;

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

  WiFiMulti.addAP(sec.known_ap.c_str(), sec.known_ap_pw.c_str());
  WiFiMulti.run();

  WiFiManager wifiManager;
  wifiManager.setTimeout(300);
  wifiManager.autoConnect("mocsigoncska_ap");
  USE_SERIAL.println("connected...yeey :)");


  // Print WiFi diagnostics
  USE_SERIAL.print("IP address: ");
  USE_SERIAL.println(WiFi.localIP());
  USE_SERIAL.print("Gateway: ");
  USE_SERIAL.println(WiFi.gatewayIP());
  USE_SERIAL.print("DNS: ");
  USE_SERIAL.println(WiFi.dnsIP());
  USE_SERIAL.print("Signal strength (RSSI): ");
  USE_SERIAL.print(WiFi.RSSI());
  USE_SERIAL.println(" dBm");

  // Test DNS resolution
  USE_SERIAL.println("Testing DNS resolution...");
  IPAddress testIP;
  if (WiFi.hostByName("discord.com", testIP)) {
    USE_SERIAL.print("discord.com resolved to: ");
    USE_SERIAL.println(testIP);
  } else {
    USE_SERIAL.println("DNS resolution failed for discord.com! Set DNS to google ones");
    // Set custom DNS servers (Google DNS) to fix DNS resolution issues
    IPAddress dns1(1, 1, 1, 1);
    IPAddress dns2(1, 1, 1, 1);
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
    if (WiFi.hostByName("discord.com", testIP)) {
      USE_SERIAL.print("discord.com resolved to: ");
      USE_SERIAL.println(testIP);
    } else {
      USE_SERIAL.println("DNS resolution failed for discord.com! uh-oh");
    }
      // Print WiFi diagnostics
  USE_SERIAL.print("IP address: ");
  USE_SERIAL.println(WiFi.localIP());
  USE_SERIAL.print("Gateway: ");
  USE_SERIAL.println(WiFi.gatewayIP());
  USE_SERIAL.print("DNS: ");
  USE_SERIAL.println(WiFi.dnsIP());
  USE_SERIAL.print("Signal strength (RSSI): ");
  USE_SERIAL.print(WiFi.RSSI());
  USE_SERIAL.println(" dBm");
  }

  // Configure WiFi settings for better stability
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  influxdb_line.addTag("name", name);
  influxdb_line.addTag("version", ver);
  influxdb_line.addField("event", "Startup");
  influx_client.writePoint(influxdb_line);
  discordPost("startup: " + name + " " + ver);
  influx_client.writePoint(influxdb_line);
  influxdb_line.clearFields();
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
  float temp_sum = 0;
  int valid_count = 0;
  for (int k = 0; k < 3; k++) {
    float t = dsfunc();
    if (t > -90) {
      temp_sum += t;
      valid_count++;
    }
    delay(100); // small delay between readings
  }
  float temp = (valid_count > 0) ? (temp_sum / valid_count) : -100;
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
  influxdb_line.addField("temp", temp);
  USE_SERIAL.println("Writing to InfluxDB: " + influxdb_line.toLineProtocol());
  influx_client.writePoint(influxdb_line);
  influxdb_line.clearFields();
}
////////////LOOP ////////////////////////////
//////////////////////////////////////////////

/////////////////////////////////////////////
////////////HEATER///////////////////////////
boolean heating;
void heater(float temp)
{
  USE_SERIAL.println("---------- " + String(temp) + " -> " + String(heating));
  if (temp < heating_start_temp && heating == false)
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
  digitalWrite(RELAYPIN, invert_heating ? 1 : 0);
  influxdb_line.addField("event", "Heater start");
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
  digitalWrite(RELAYPIN, invert_heating ? 0 : 1);
  pinMode(RELAYPIN, INPUT);
  influxdb_line.addField("event", "Heater stop");
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
  USE_SERIAL.print("[HTTP] check at " + url + "\n");
  if (http.begin(client, url))
  { // HTTP

    USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
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


void discordPost(String message)
{

  String payload = "{\"content\": \"" + message + "\"}";
  String url = sec.discord_url;
  USE_SERIAL.println(POSTTask(url, payload));
};

String GETTask(String url)
{
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure(); //This will set the http connection to insecure! This is not advised, but I have found no good way to use real SSL, and my application doesn't need the added security
  HTTPClient https;
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
////////////CONFIG QUERY HELPERS//////////////

/**
 * Helper method to query a long/integer config value from InfluxDB
 * @param fieldName The config field name to query
 * @param value Reference to the variable to update
 * @return true if value was found and updated, false otherwise
 */
boolean queryConfigLong(String fieldName, long &value)
{
  String query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"" + fieldName + "\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  
  if (result.next()) {
    value = result.getValueByName("_value").getLong();
    USE_SERIAL.println("Config got " + fieldName + " = " + String(value));
    result.close();
    return true;
  }
  
  result.close();
  USE_SERIAL.println("Config field " + fieldName + " not found");
  return false;
}

/**
 * Helper method to query a double/float config value from InfluxDB
 * @param fieldName The config field name to query
 * @param value Reference to the variable to update
 * @return true if value was found and updated, false otherwise
 */
boolean queryConfigDouble(String fieldName, float &value)
{
  String query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"" + fieldName + "\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  
  if (result.next()) {
    value = result.getValueByName("_value").getDouble();
    USE_SERIAL.println("Config got " + fieldName + " = " + String(value));
    result.close();
    return true;
  }
  
  result.close();
  USE_SERIAL.println("Config field " + fieldName + " not found");
  return false;
}

/**
 * Helper method to query a boolean config value from InfluxDB
 * @param fieldName The config field name to query
 * @param value Reference to the variable to update
 * @return true if value was found and updated, false otherwise
 */
boolean queryConfigBool(String fieldName, boolean &value)
{
  String query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"" + fieldName + "\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  
  if (result.next()) {
    value = result.getValueByName("_value").getBool();
    USE_SERIAL.println("Config got " + fieldName + " = " + String(value));
    result.close();
    return true;
  }
  
  result.close();
  USE_SERIAL.println("Config field " + fieldName + " not found");
  return false;
}

////////////CONFIG QUERY HELPERS//////////////
//////////////////////////////////////////////

//////////////////////////////////////////////
////////////GETCONFIG/////////////////////////

void getconfig()
{
  /*
  The config data is now retrieved from InfluxDB bucket 'noszlop'.
  We query for the latest config values using the device name as a tag filter.
  Config values are stored as separate measurements: pinginterval, update_interval, temp_target, heating_start_temp
  */
  
  USE_SERIAL.println("Getting config from InfluxDB...");

  // Check InfluxDB connection
  if (!influx_client.validateConnection())
  {
    USE_SERIAL.print("InfluxDB connection failed: ");
    USE_SERIAL.println(influx_client.getLastErrorMessage());
    return;
  }
  
  // Query long/integer config values
  queryConfigLong("pinginterval", pinginterval);
  queryConfigLong("update_interval", update_interval);
  
  // Query float/double config values
  queryConfigDouble("temp_target", temp_target);
  queryConfigDouble("heating_start_temp", heating_start_temp);
  
  // Query boolean config values
  queryConfigBool("invert_heating", invert_heating);

  USE_SERIAL.println("Config retrieval completed from InfluxDB");
}

////////////GETCONFIG/////////////////////////
//////////////////////////////////////////////