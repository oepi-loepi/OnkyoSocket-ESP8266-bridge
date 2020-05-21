
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
//#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h> 

const char* ssid     = "SSID"; //Change this value for your network
const char* password = "*******"; //Change this value for your network
char* onkyoadress = "192.168.10.241"; // Onkyo Receiver
int OnkyoPort = 60128;

String localIP;
String gatewayIP;
String subnetMask ;
String header;


char static_ip[18] = "192.168.10.65";
char static_gw[18] = "192.168.10.1";
char static_sn[18] = "255.255.255.0";
char static_dn[18] = "192.168.10.1";
bool dhcp = false;
char* wifiHostname = "OnkyoBridge";

//flag for saving data
bool shouldSaveConfig = false;


unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
unsigned long starttimeMillis;  //some global variables available anywhere in the program
unsigned long currenttimeMillis;
unsigned long period = 60000;  //time between pulses, initial 2000 ms


String my_command;
String webcommand = "";
String fromserver ="";
String header_web;
int found = 0;
int found3 = 1;

bool reset1 = false;
bool reset2 = false;


String title1;
String artist;
String timestamp;
String volume;
String input;
String tuner;
String preset;

String album;
String trackstat;
String power1;
String albumart;

String jsoncontentnew;
String jsoncontentold;

WiFiClient client;
WebSocketsServer webSocket = WebSocketsServer(81);

WiFiServer server(80);

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}




void setup()
{
    startMillis = millis();  //initial start time
    starttimeMillis = millis();  //initial start time
    
    Serial.begin(9600);
    Serial.println("mounting FS...");
    
    if (SPIFFS.begin()) {
      Serial.println("mounted file system");
      if (SPIFFS.exists("/config.json")) {
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
          Serial.println("opened config file");
          size_t size = configFile.size();
          std::unique_ptr<char[]> buf(new char[size]);
  
          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success()) {
            Serial.println("\nparsed json");
             strcpy(onkyoadress, json["onkyoadress"]);
          if(json["dhcp"]) {
            Serial.println("Setting up wifi from dhcp config");
            dhcp=true;
          } else{
            Serial.println("Setting up wifi from Static IP config");
          }        

          if(json["ip"]) {
            Serial.println("Last known ip from config");
            strcpy(static_ip, json["ip"]);
            strcpy(static_gw, json["gateway"]);
            strcpy(static_sn, json["subnet"]);
            Serial.println(static_ip);
          } else {
            Serial.println("no custom ip in config");
          }
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
    Serial.println(static_ip);
  
    WiFiManagerParameter custom_onkyoadress("Onkyo IP Adress", "onkyoadress", onkyoadress, 40);
    //WiFiManagerParameter custom_OnkyoPort("Onkyo Port (60128)", "OnkyoPort", OnkyoPort, 40);
    WiFiManagerParameter custom_text("<p>Select Checkbox for DHCP  ");
    WiFiManagerParameter custom_text2("</p><p>DHCP will be effective after reset (power off/on)</p>");
    WiFiManagerParameter custom_text3("</p><p>So first wait for a minute after saving and then reboot by removing power.</p>");
    WiFiManagerParameter custom_dhcp("dhcp", "dhcp on", "T", 2, "type=\"checkbox\" ");

    WiFiManager wifiManager;
    WiFi.hostname(wifiHostname);
    
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    if (!dhcp){
        IPAddress _ip,_gw,_sn;
        _ip.fromString(static_ip);
        _gw.fromString(static_gw);
        _sn.fromString(static_sn);
      
        wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
      }else{
        wifiManager.autoConnect("AutoConnectAP");
      }
  
    wifiManager.addParameter(&custom_onkyoadress);
    wifiManager.addParameter(&custom_text);
    wifiManager.addParameter(&custom_dhcp);
    wifiManager.addParameter(&custom_text2);
    wifiManager.addParameter(&custom_text3);
  
    wifiManager.setMinimumSignalQuality();
  
    if (!wifiManager.autoConnect("AutoConnectAP")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  
    Serial.println("connected...yeey :)");

    if (!dhcp){
      WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask());
      WiFi.begin();
    }

    delay(2000);

    Serial.println("local ip");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.gatewayIP());
    Serial.println(WiFi.subnetMask());
  
    dhcp = (strncmp(custom_dhcp.getValue(), "T", 1) == 0);

    strcpy(onkyoadress, custom_onkyoadress.getValue());
    //strcpy(GrowattPassWord, custom_OnkyoPort.getValue());
  
    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["onkyoadress"] = onkyoadress;
      json["dhcp"] = dhcp;
      json["ip"] = WiFi.localIP().toString();
      json["gateway"] = WiFi.gatewayIP().toString();
      json["subnet"] = WiFi.subnetMask().toString();
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }
      json.prettyPrintTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
    }
  
    WiFi.begin();
    delay(1000);

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    //client.connect(onkyoadress, OnkyoPort);
    client.connect("192.168.10.241",60128);
    Serial.println("Waiting for command...");

    server.begin();


}

void Sendcommand(String my_command2){
  my_command2.toUpperCase();
  if (client.connected()){
      Serial.println("Sending command: " + my_command2);
        int eiscpDataSize = 2 + my_command2.length() + 1;
        String eISCPCommand = "ISCP";
        // 4 char Big Endian Header
        eISCPCommand +=((char) 0x00);
        eISCPCommand +=((char) 0x00);
        eISCPCommand +=((char) 0x00);
        eISCPCommand +=((char) 0x10);
        // 4 char Big Endian data size
        eISCPCommand +=((char) ((eiscpDataSize >> 24) & 0xFF));
        eISCPCommand +=((char) ((eiscpDataSize >> 16) & 0xFF));
        eISCPCommand +=((char) ((eiscpDataSize >> 8) & 0xFF));
        eISCPCommand +=((char) (eiscpDataSize & 0xFF));
        // eISCP version = "01";
        eISCPCommand +=((char) 0x01);
        // 3 chars reserved = "00"+"00"+"00";
        eISCPCommand +=((char) 0x00);
        eISCPCommand +=((char) 0x00);
        eISCPCommand +=((char) 0x00);
        // eISCP data
        // Start Character
        eISCPCommand +=("!");
        // eISCP data - unit type char '1' is receiver
        eISCPCommand +=("1");
        // eISCP data - 3 char command and param ie PWR01
        eISCPCommand +=my_command2;
        // msg end - EOF
        eISCPCommand +=((char) 0x0D);

        //Serial.println(eISCPCommand);
        client.print(eISCPCommand);
        my_command = "";
    } else {
      Serial.println("connection failed");
    }

    if (!client.connected()) {
          Serial.println();
          Serial.println("disconnecting.");
          client.stop();
          for(;;)
          ;
    }
}

void sendjson(bool checkvoornieuw){
  jsoncontentnew = "";
  StaticJsonBuffer<200> jsonBuffer2;
  JsonObject& json2 = jsonBuffer2.createObject();
  json2["title"] = title1;
  json2["artist"] = artist;
  json2["timestamp"] = timestamp;
  json2["power1"] = power1;
  json2["volume"] = volume;
  //json2["random"] =  random(1000);
  json2["input"] = input;
  json2["tuner"] = tuner;
  json2["preset"] = preset;
  json2.printTo(jsoncontentnew);

  if (checkvoornieuw){
      if (jsoncontentnew != jsoncontentold) {   
          webSocketWrite(jsoncontentnew);   
          jsoncontentold = jsoncontentnew;
       }
  }
  else{
       webSocketWrite(jsoncontentnew);
       jsoncontentold = jsoncontentnew;
  }
}

void parsedata(String datastring){
  Serial.println(datastring);
      found = datastring.indexOf("!1NTI");   
      if (found>=0){
        found3 = datastring.indexOf("iPhone");
        if (found3 < 0){
          title1= datastring.substring(found+5);
          found =-1;
          found3 =-1;
          Serial.println(title1);
        }
      }
      
      found = datastring.indexOf("!1NAT");   if (found>=0){artist = datastring.substring(found+5);found =-1; Serial.println(artist);}
      found = datastring.indexOf("!1MVL");   if (found>=0){
                                                        volume = datastring.substring(found+5);found =-1;
                                                        int x;  char *endptr;  const char* nw = volume.c_str();  
                                                        x = strtol(nw, &endptr, 16); volume = x; Serial.println(volume);
                                              }
      found = datastring.indexOf("!1SLI");   if (found>=0){input = datastring.substring(found+5);found =-1; Serial.println(input);}
      found = datastring.indexOf("!1TUN");   if (found>=0){tuner = datastring.substring(found+5);found =-1; Serial.println(tuner);}
      found = datastring.indexOf("!1NAL");   if (found>0){album = datastring.substring(found+5);found =-1; Serial.println(album);}
      found = datastring.indexOf("!1PWR");   if (found>=0){power1 = datastring.substring(found+5);found =-1; Serial.println(power1);}
      found = datastring.indexOf("!1PRS");   if (found>=0){preset = datastring.substring(found+5);found =-1; Serial.println(preset);}
      found = datastring.indexOf("!1NJA2-"); if (found>=0){albumart = datastring.substring(found+7);found =-1; Serial.println(albumart);}


      currenttimeMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
      if (currenttimeMillis - starttimeMillis >= 5000) { //test whether the pulsetime has eleapsed
          found = datastring.indexOf("!1NTM");   if (found>=0){timestamp = datastring.substring(found+5);found =-1; Serial.println(timestamp);}
          starttimeMillis = currenttimeMillis; 
      }
      sendjson(true);
}

void webSocketWrite(String TextToSend){
     webSocket.broadcastTXT(TextToSend); 
     Serial.println("Send text over websocket: " + TextToSend);
}
 

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t pllength){
  if(type == WStype_TEXT){
      
          Serial.print("Payload : ");
          String a = "";
          for(int i = 0; i < pllength; i++) { 
              a += (char) payload[i]; 
          }
          //int found2;
          Serial.println(a);
          delay(100);
          //found2 = a.indexOf("This is Toon for ESP"); 
          if (a.indexOf("This is Toon for ESP")>=0){
              webSocketWrite("Hello Toon all OK?");
          }else{
              Sendcommand(a);
          }        
  }
  else 
  {
    //Serial.print("WStype = ");   Serial.println(type);  
    //Serial.print("WS payload = ");
    for(int i = 0; i < pllength; i++) { Serial.print((char) payload[i]); }
  }
}

void webserver(){
    // Set web server port number to 80 and create a webpage
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        //Serial.write(c);                    // print it out the serial monitor
        header_web += c;
        if (c == '\n') {                    // if the byte is a newline character
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<title>OnkyoBridge</title>");
            client.println("<meta name=\"author\" content=\"SolarBridge by oepi-loepi\">");
            client.println("<style>html {font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println("p.small {line-height: 1; font-size:70%;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 10px 20px;");
            client.println("text-decoration: none; font-size: 24px; margin: 2px; cursor: pointer;}</style>");
            
            client.println("</head>");
            client.println("<body>");
            client.println("<h1>OnkyoBridge</h1>");
            client.println("<hr>");
            client.println("<p></p>");
             
            if ((header_web.indexOf("GET /") >= 0) && (header_web.indexOf("GET /reset/") <0)) {

              reset1 = false;
              reset2 = false;

              client.println("<p class=\"small\">IP: " +WiFi.localIP().toString() + "<br>");
              client.println("Gateway: " +WiFi.gatewayIP().toString()+ "<br>");
              client.println("Subnet: " +WiFi.subnetMask().toString() + "<br><br>");
            

              client.println("<p><a href=\"/reset/req\"><button class=\"button\">Reset</button></a></p>");
              client.println("<p>When reset is pressed, all settings are deleted and AutoConnect is restarted<br>");
              client.println("Please connect to wifi network AutoConnectAP and after connect go to: 192.168.4.1 for configuration page </p>");
              client.println("</body></html>");
            }
            
            if (header_web.indexOf("GET /reset/req") >= 0) {
              reset1 = true;
              reset2 = false;
              Serial.print("Reset1 :");
              Serial.print(reset1);
              Serial.print(", Reset2 :");
              Serial.println(reset2);
              client.println("<p>Weet u zeker ?</p>");
              client.println("<p> </p>");
              client.println("<p><a href=\"/reset/ok\"><button class=\"button\">Yes</button></a><a href=\"/../..\"><button class=\"button\">No</button></a></p>");
              client.println("</body></html>");
            }

            if (header_web.indexOf("GET /reset/ok") >= 0) {
              reset2 = true;
              Serial.print("Reset1 :");
              Serial.print(reset1);
              Serial.print(", Reset2 :");
              Serial.println(reset2);
              client.println("<p>Reset carried out</p>");
              client.println("<p>Please connect to wifi network AutoConnectAP and goto 192.168.4.1 for configuration</p>");
              client.println("</body></html>");
            }
            
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header_web = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }  
}




void loop(){
      webSocket.loop();
      webserver();
      if ((reset1) && (reset2)) { //all reset pages have been acknowledged and reset parameters have been set from the webpages
        delay(2000);
        Serial.println("Reset requested");
        Serial.println("deleting configuration file");
        SPIFFS.remove("/config.json");
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        delay(500);
        ESP.reset();
      }
      
      if (client.available()) {
          char c =  char(client.read());
          if (((32 <= c) && (c <= 126)) || (c <= '\n')){
              if (((32 <= c) && (c <= 126))){
                fromserver += c;
              }
              if (c =='\n'){
                  parsedata(fromserver); 
                  fromserver = "";
              }
          }
      } 

      if(Serial.available()){
        char inchar = (char)  Serial.read();
        if ((' ' <= inchar) && (inchar <= '~')){
            my_command += inchar;
        }
        else{
            if (my_command.length()>1){
              //Serial.println(my_command);
              Sendcommand(my_command);
            }
       }
    }

    currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
    if (currentMillis - startMillis >= period) { //test whether the pulsetime has eleapsed
        //Serial.println("Every 60 seconds test on complete json ");
        Sendcommand("PWRQSTN");
        delay(30);
        Sendcommand("NTI");         
        delay(30);
        Sendcommand("NATQSTN");         
        delay(30);
        Sendcommand("NTMQSTN");         
        delay(30);
        Sendcommand("MVLQSTN");         
        delay(30);
        Sendcommand("SLIQSTN");         
        delay(30);
        Sendcommand("TUNQSTN");         
        delay(30);
        Sendcommand("NSTQSTN");         
        delay(30);
        Sendcommand("NALQSTN");         
        delay(30);
        Sendcommand("PWRQSTN");         
        delay(30);
        Sendcommand("PRSQSTN");         
        delay(60);
        sendjson(false);
        
        startMillis = currentMillis; 
    }
}
