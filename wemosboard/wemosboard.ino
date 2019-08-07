#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

const char* ssid = "mywifi";
const char* password = "password123";

ESP8266WebServer server(80);

const int led = D5;
bool readingData = true;
String out1 = "";
String out2 = "";
String out3 = "";
String out4 = "";
byte activeTrial = 1;
String newData = "";
int val = 0;

const byte numChars = 32;
char receivedMsg[numChars];
bool receivedAll = false;
bool recordData = false;

// HTML for index page
const char *indexHTML =
R"0(
  <!DOCTYPE html>
  <html>
  <head><meta charset="UTF-8">
    <script>
      function startCollection(trial) {
        window.location.href = "/start" + trial;
      };
      function showData(trial) {
        window.location.href = "/data" + trial;
      }
    </script>
  </head>
  <body>
    <p>Any previously recorded data will be lost upon clicking start.</p>
    <div style="padding:10px;">
      <button type="button" onclick="startCollection(1);">Start Trial 1 Collection</button>
      <button type="button" onclick="showData(1);">Show Trial 1 Data</button>
    </div>
    <div style="padding:10px;">
      <button type="button" onclick="startCollection(2);">Start Trial 2 Collection</button>
      <button type="button" onclick="showData(2);">Show Trial 2 Data</button>
    </div>
    <div style="padding:10px;">
      <button type="button" onclick="startCollection(3);">Start Trial 3 Collection</button>
      <button type="button" onclick="showData(3);">Show Trial 3 Data</button>
    </div>
    <div style="padding:10px;">
      <button type="button" onclick="startCollection(4);">Start Trial 4 Collection</button>
      <button type="button" onclick="showData(4);">Show Trial 4 Data</button>
    </div>
    
  </body>
  </html>
)0";

// HTML to display during data collection
const char *startHTML =
R"0(
  <!DOCTYPE html>
  <html>
  <head><meta charset="UTF-8">
    <script>
      function endCollection(){
        window.location.href = "/end";
      };
    </script>
  </head>
  <body>
    <p>Collection in progress...</p>
    <button type="button" onclick="endCollection();">End Collection</button>
  </body>
  </html>
)0";

// HTML for end data collection screen
const char *endHTML =
R"0(
  <!DOCTYPE html>
  <html>
  <head><meta charset="UTF-8">
    <script>
      function restartCollection(){
        window.location.href = "/start";
      };
      function returnToIndex() {
        window.location.href = "/";
      }
      function showData() {
        window.location.href = "/data";
      }
    </script>
  </head>
  <body>
    <p>Collection Complete. Check /data for results.</p>
    <button type="button" onclick="restartCollection();">Restart Collection</button>
    <p>Previously collected data will be lost upon restart.</p>
    <button type="button" onclick="returnToIndex();">Return to INDEX</button>
    <button type="button" onclick="showData();">Show Data</button>
  </body>
  </html>
)0";

void handleRoot() {
  // Make sure data is not being recorded on index page
  recordData = false;
  server.send(200, "text/html", indexHTML);
}

void handleStartCollect(String outputVar, byte trial) {
  // Reset output, start recording, update html
  outputVar = "";
  activeTrial = trial;
  recordData = true;
  server.send(200, "text/html", startHTML);
}

void handleEndCollect() {
  // Stop recording data and send html page
  recordData = false;
  server.send(200, "text/html", endHTML);
}

void handleDataViewRequest(String outputVar) {
  // Send data collected during test
  server.send(200, "text/plain", outputVar);
}

// Used for python script
void handleFeed() {
  // Send data received since last feed get request
  server.send(200, "text/plain", newData);
  // Clear data every time feed is read so string doesn't get so long
  newData = "";
}

// Display message if uri does not exist
void handleNotFound(){
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setup(void){
  pinMode(led, OUTPUT);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
//  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  // Functions to call at each uri
  server.on("/", handleRoot);

  // 4 starts for 4 different trial recordings
  server.on("/start1", [](){
    handleStartCollect(out1, 1);
  });
  server.on("/start2", [](){
    handleStartCollect(out2, 2);
  });
  server.on("/start3", [](){
    handleStartCollect(out3, 3);
  });
  server.on("/start4", [](){
    handleStartCollect(out4, 4);
  });
  
  server.on("/end", handleEndCollect);

  // Get data from any of the 4 recordings
  server.on("/data1", [](){
    handleDataViewRequest(out1);
  });
  server.on("/data2", [](){
    handleDataViewRequest(out2);
  });
  server.on("/data3", [](){
    handleDataViewRequest(out3);
  });
  server.on("/data4", [](){
    handleDataViewRequest(out4);
  });
  
  server.on("/feed", handleFeed);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void clearSerialData() {
  // Set char array to all 0's
  for (int i = 0; i < sizeof(receivedMsg); i++) {
    receivedMsg[i] = (char)0;
  }
}

void recvData() {
  byte i = 0;
  char endMarker = '>';
  char startMarker = '<';
  char charReceived;
  boolean beginTransmit = false;

  // Set char array to 0's before reading new message
  clearSerialData();

  // Serial only reads 1 char at a time
  while (Serial.available() > 0) {
    charReceived = Serial.read();

    // Append received character to array once start marker is received and before end marker
    if (beginTransmit) {
      if (charReceived != endMarker) {
        receivedMsg[i] = charReceived;
        i++;
      } else {
        receivedMsg[i] = '\0';
        i = 0;
        receivedAll = true;
      }
    }
    // Check if start marker is received & begin recording to char array
    // at this point so that the start marker is not included in the array
    if (charReceived == startMarker) {
      beginTransmit = true;
    }
  }

}

void processOutput(bool show, bool record) {
  // Process only if full message was received
  if (receivedAll == true) {
    // Clear new feed data if feed is not being read by python script
    if (newData.length() > 1000) {
      newData = "";
    }
    newData += receivedMsg;
    if (show) {
      Serial.println(receivedMsg);
    }
    if (record) {
      // Choose which output to update depending on which trial is being run
      switch (activeTrial) {
        case 1:
        out1 += receivedMsg;
        break;
        case 2:
        out2 += receivedMsg;
        break;
        case 3:
        out3 += receivedMsg;
        break;
        case 4:
        out4 += receivedMsg;
        break;
      }
      
    }
    receivedAll = false;
    
  }
}

void loop(void){
  recvData();
  processOutput(true, recordData);
  server.handleClient();
}

  
  
  



