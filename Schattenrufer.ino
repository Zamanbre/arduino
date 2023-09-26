#include "driver/rtc_io.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>


#include <RCSwitch.h>
RCSwitch mySwitch = RCSwitch();

const char* ssid     = "MagischesNetz";
const char* password = ""; //Passwörter nicht ins Internet!

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

const int SCHLAFPIN = 13;
const int anzahlInputs = 8;
const int reedPins[anzahlInputs] = {27,26,25,33,32,18,19,21};
const int additor[anzahlInputs] = {1,2,4,8,16,32,64,128};
const int startknopf = 14; //nutzt touchRead
int eingabe = 0; //aktuell eingegebener Befehl
int letzteEingabe = 0; //speichert den zuletzt eingegebenen Befehl
unsigned long delayB = 0; //für Dauerbefehl
bool eingabeOffen = true;
int dauerbefehlnr = 0; //0 = aus, 1 = Leuchtkugeltest, 2 = Kombinationen
int dauerbefehlvar = 0;
int zustand = 0; //0 = aus; 1 = hochfahren; 2 = aktiv
bool blockiert = false;
const unsigned long hochfahrzeit = 1800000; //in Millisekunden
int dauerbefehlar[10];

char* schattenruferAD = "http://schattenrufer.local/json/state"; //"http://192.168.4.2/json/state"
char* sprungtorAD = "http://sprungtor.local/json/state";
char* saeulenAD = "http://saeulen.local/json/state";

const int kombistarter = 171;
const int anzahlKombinationen = 2;
const int laengeKombinationen = 7; //7
const int kombinationen [anzahlKombinationen][laengeKombinationen] = { //Befehle, die sich aus laengeKombinationen einzelbefehlen zusammensetzen
  {186, 244, 187, 214, 238, 174, 181},
  {124, 199, 117, 235, 186, 187, 238}
};

int kombinationsschritt = 0; //markiert an welcher Stelle der Kombination man gerade ist
int kombieingabe[laengeKombinationen]; //array in dem die aktuelle Kombinationseingabe gespeichert wird
int letztekombi = 0; //damit die an Get Requests rausgegeben werden kann
unsigned long timer;

void setup() {
  // put your setup code here, to run once:
  rtc_gpio_pullup_en(GPIO_NUM_13);
  //rtc_gpio_pulldown_dis(GPIO_NUM_13);
  //esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ALL_LOW);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13,0); //1 = High, 0 = Low

  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
  #endif

  pinMode(SCHLAFPIN, INPUT_PULLUP);
  for (int i=0; i < anzahlInputs; i++) {
    pinMode(reedPins[i], INPUT_PULLUP);
  }


  //pinMode(startknopf, INPUT_PULLUP);

  mySwitch.enableTransmit(16); // Transmitter is connected to Pin #16 

  Serial.begin(9600);


  // Connect to Wi-Fi network with SSID and password
  WiFi.mode(WIFI_AP);
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();

  Serial.println("Steuerkonsole online.");
}

void befehl(int eingabe) {
  JSONVar outputjson;
  String jsonString;
  //dauerbefehlnr = 0;
  if (eingabe == letzteEingabe) {
    return;
  }
  else {
    letzteEingabe = eingabe;
  }
  Serial.print("Befehl gesendet: ");
  Serial.println(eingabe);

  bool gueltig = true;

  if (eingabe == 0) {
    backToNormal();

  } else if (eingabe == 300) { //blockade ein
    blockiert = true;
  
  } else if (eingabe == 301) { //blockade aus
  blockiert = false;

  } else if (eingabe == 41 && zustand == 0) { //Hochfahren
    //SR
    outputjson["on"] = true;
    outputjson["ps"] = 1;
    jsonString = JSON.stringify(outputjson);
    sendJson(schattenruferAD,jsonString);
    //ST
    outputjson["on"] = true;
    outputjson["ps"] = 1;
    jsonString = JSON.stringify(outputjson);
    sendJson(saeulenAD,jsonString);

    mySwitch.send(16776966,24); //fade
    timer = millis();
    zustand = 1;

  } else if (eingabe == 242) { //Direkt Hochfahren
    standartAn();
    zustand = 2;

  } else if (eingabe == 85 && zustand == 2) { //rufen
    //SR
    outputjson["on"] = true;
    outputjson["ps"] = 3;   
    jsonString = JSON.stringify(outputjson);
    sendJson(schattenruferAD,jsonString);
    //TO
    outputjson["on"] = true;
    outputjson["ps"] = 1;   
    jsonString = JSON.stringify(outputjson);
    sendJson(sprungtorAD,jsonString);
    //ST
    outputjson["on"] = true;
    outputjson["ps"] = 3;   
    jsonString = JSON.stringify(outputjson);
    sendJson(saeulenAD,jsonString);

    mySwitch.send(16776975,24); //B2 Violet

  } else if (eingabe == 198 && zustand == 2) { //zurückschicken 
    //SR
    outputjson["on"] = true;
    outputjson["ps"] = 4;   
    jsonString = JSON.stringify(outputjson);
    sendJson(schattenruferAD,jsonString);
    //TO
    outputjson["on"] = true;
    outputjson["ps"] = 2;   
    jsonString = JSON.stringify(outputjson);
    sendJson(sprungtorAD,jsonString);
    //ST
    outputjson["on"] = true;
    outputjson["ps"] = 4;   
    jsonString = JSON.stringify(outputjson);
    sendJson(saeulenAD,jsonString);

    mySwitch.send(16776974,24); //G2 Cyan

  } else if (eingabe == 147 && zustand == 2) { //agressiv 
    //SR
    outputjson["on"] = true;
    outputjson["ps"] = 6;
    jsonString = JSON.stringify(outputjson);
    sendJson(schattenruferAD,jsonString);
    //TO
    outputjson["on"] = false;
    outputjson["ps"] = -1;    
    jsonString = JSON.stringify(outputjson);
    sendJson(sprungtorAD,jsonString);
    //ST
    outputjson["on"] = true;
    outputjson["ps"] = 6;
    jsonString = JSON.stringify(outputjson);
    sendJson(saeulenAD,jsonString);

    mySwitch.send(16776970,24); //Rot

  } else if (eingabe == 170 && zustand == 2) { //passiv
    //SR
    outputjson["on"] = true;
    outputjson["ps"] = 7; 
    jsonString = JSON.stringify(outputjson);
    sendJson(schattenruferAD,jsonString);
    //TO
    outputjson["on"] = false;
    outputjson["ps"] = -1;  
    jsonString = JSON.stringify(outputjson);
    sendJson(sprungtorAD,jsonString);
    //ST
    outputjson["on"] = true;
    outputjson["ps"] = 7;
    jsonString = JSON.stringify(outputjson);
    sendJson(saeulenAD,jsonString);

    mySwitch.send(16776972,24); //Blau

  } else if ((eingabe == 146 && zustand != 1) || eingabe == 248) { //Herunterfahren
    outputjson["on"] = false;   
    jsonString = JSON.stringify(outputjson);
    sendJson(schattenruferAD,jsonString);
    sendJson(sprungtorAD,jsonString);
    sendJson(saeulenAD,jsonString);

    mySwitch.send(16776963,24); //Off
    zustand = 0;

  } else if (eingabe == 244 && zustand == 0) {
    dauerbefehlnr = 1; // Dauersenden Rot / Blau Wechsel

  } else if (eingabe == kombistarter && zustand == 2) {
    Serial.println("Kombinationsmodus gestartet");
    kombinationsschritt = 1;
    dauerbefehlnr = 2; //Kombinationsmodus
    portalEingabe();
  
  } else {
    gueltig = false;
  }

  if (gueltig && eingabe != kombistarter) { //gültige Befehle brechen Kombieingabe ab
    dauerbefehlnr = 0;
  } 

  if (dauerbefehlnr == 2 && eingabe != kombistarter) {
    kombicheck(eingabe);

  }
}

void kombicheck(int eingabe) {
  kombieingabe[kombinationsschritt-1] = eingabe;
  if (kombinationsschritt == laengeKombinationen) {
    Serial.print("Kombination: ");
    for (int i = 0; i < laengeKombinationen; i++) {
      Serial.print(kombieingabe[i]);
      Serial.print(", ");
    }
    Serial.println();
    for (int i=0; i < anzahlKombinationen; i++) {
      bool gleich = true; //überprüfe, ob die Arrays gleich sind
      for (int x=0; x < laengeKombinationen; x++) {
        if (kombieingabe[x] != kombinationen[i][x]) {gleich = false;}
      }

      if (gleich) {
        Serial.print("Kombination erkannt: ");
        Serial.println(i);
        kombiDo(i);
        return; //gültige Kombination
      }
    }
    backToNormal();
    Serial.print("Keine gültige Kombination erkannt.");
    return; //keine gültige Kombination
  }
  kombinationsschritt++;
  Serial.print("Neuer Kombinationsschritt:");
  Serial.println(kombinationsschritt);
  portalEingabe();
}

void standartAn() {
  JSONVar outputjson;
  String jsonString;
  mySwitch.send(16776961,24); //On
  outputjson["on"] = true; 
  outputjson["ps"] = 2;
  jsonString = JSON.stringify(outputjson);
  sendJson(schattenruferAD,jsonString);
  //ST
  outputjson["on"] = true;
  outputjson["ps"] = 2;
  jsonString = JSON.stringify(outputjson);
  sendJson(saeulenAD,jsonString);
  //TO
  outputjson["on"] = false;
  outputjson["ps"] = -1;
  jsonString = JSON.stringify(outputjson);
  sendJson(sprungtorAD,jsonString);

  mySwitch.send(16776971,24); //Grün  
}

void portalEingabe() {
  JSONVar outputjson;
  String jsonString;  
  //SR
  outputjson["ps"] = 8;   
  jsonString = JSON.stringify(outputjson);
  sendJson(schattenruferAD,jsonString);  
}

void portalEffekt() {
  JSONVar outputjson;
  String jsonString;
  //SR
  outputjson["on"] = true;
  outputjson["ps"] = 5;   
  jsonString = JSON.stringify(outputjson);
  sendJson(schattenruferAD,jsonString);
  //TO
  outputjson["on"] = true;
  outputjson["ps"] = 3;   
  jsonString = JSON.stringify(outputjson);
  sendJson(sprungtorAD,jsonString);
  //ST
  outputjson["on"] = true;
  outputjson["ps"] = 5;   
  jsonString = JSON.stringify(outputjson);
  sendJson(saeulenAD,jsonString);

  mySwitch.send(16776968,24); //smooth
}

void kombiDo (int kombi) {
  Serial.println("Kombination wird ausgeführt:");
  Serial.print(kombi);
  backToNormal();
  letztekombi = kombi+1;
  portalEffekt();
}

void backToNormal() {
  Serial.println("Back to normal.");
  dauerbefehlnr = 0;
  dauerbefehlvar = 0;
  memset(dauerbefehlar, 0, sizeof(dauerbefehlar));
  kombinationsschritt = 0;
}

void sendJson (char* serverName, String jsonString) {
  WiFiClient client;
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  
  // If you need an HTTP request with a content type: application/json, use the following:
  http.addHeader("Content-Type", "application/json");
  //int httpResponseCode = http.POST("{\"api_key\":\"tPmAT5Ab3j7F9\",\"sensor\":\"BME280\",\"value1\":\"24.25\",\"value2\":\"49.54\",\"value3\":\"1005.14\"}");
  
  int httpResponseCode = http.POST(jsonString);
  
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
    
  // Free resources
  http.end();
}

void dauerBefehl() {
  //Leuchtkugeltest
  if (dauerbefehlnr == 1 && millis() - delayB > 1000) {
    if (dauerbefehlvar == 0) {
      mySwitch.send(16776970,24); //Rot
      dauerbefehlvar = 1;
    } else {
      mySwitch.send(16776972,24); //Blau
      dauerbefehlvar = 0;
    }
    Serial.println("Dauerbefehl 1 aktiv.");
    delayB = millis();
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {
    if (client) {                             // If a new client connects,
      Serial.println("New Client.");          // print a message out in the serial port
      String currentLine = "";                // make a String to hold incoming data from the client
      while (client.connected()) {            // loop while the client's connected
        if (client.available()) {             // if there's bytes to read from the client,
          char c = client.read();             // read a byte, then
          Serial.write(c);                    // print it out the serial monitor
          header += c;
          if (c == '\n') {                    // if the byte is a newline character
            // if the current line is blank, you got two newline characters in a row.
            // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0) {
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();

              if (header.indexOf("GET /SR/") >= 0) {
                Serial.println("Fernbefehl: ");
                int startwert = 8;
                String fernbefehl = "";
                for (int i=0; i < header.length(); i++) {
                  if (header[startwert+i] == ' ') {
                    break;
                  }
                  fernbefehl += header[startwert+i];
                }
                Serial.println(fernbefehl);
                befehl(fernbefehl.toInt());
              }
              if (header.indexOf("GET /info/") >= 0) {
                client.println("HTTP/1.1 200 OK");
                client.println(" <br>Zustand (0 = aus; 1 = hochfahren; 2 = aktiv): ");
                client.print(zustand);
                client.println(" <br>Dauerbefehl (0 = aus, 1 = Leuchtkugeltest, 2 = Kombinationen): ");
                client.print(dauerbefehlnr);                
                client.println(" <br>Blockiert: ");
                client.print(blockiert);
                client.println(" <br>Letze Eingabe: ");
                client.print(letzteEingabe);
                client.println(" <br>Letze Kombi: ");
                client.print(letztekombi);

                if (dauerbefehlnr == 2) {
                  client.println(" <br>Kombinationsschritt: ");
                  client.print(kombinationsschritt);
                  client.println(" <br>Aktuelle Kombieingabe: ");
                  for (int i = 0; i < laengeKombinationen; i++) {
                    client.print(kombieingabe[i]);
                    client.print(", ");
                  }
                }

                if (zustand == 1) {
                  client.println(" <br>Timer noch in ms: ");
                  client.print(hochfahrzeit-(millis()-timer));
                  client.println(" <br>Timer noch in min: ");
                  client.print((hochfahrzeit-(millis()-timer))/60000);
                  // client.println(" <br>Timer: ");
                  // client.print(timer);
                  // client.println(" <br>millis aktuell: ");
                  // client.print(millis());
                  // client.println(" <br>Differenz: ");
                  // client.print(millis()-timer);
                  // client.println(" <br>Hochfahrzeit: ");
                  // client.print(hochfahrzeit);                      
                }
              }

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
      header = "";
      // Close the connection
      client.stop();
      Serial.println("Client disconnected.");
      Serial.println("");
    }
  }

  //Eingabe feststellen und anzeigen
  if (eingabeOffen) {
    eingabe = 0;
    Serial.print("Zustand: ");
    Serial.print(zustand);
    Serial.print(" Inputs: ");
    for (int i=0; i < anzahlInputs; i++) {
      if (digitalRead(reedPins[i]) == LOW) {
        eingabe += additor[i];
      }
      Serial.print(digitalRead(reedPins[i]));
      Serial.print(", ");
    }
    Serial.println(eingabe);
  }
  //Eingabe festsetzen wenn Startknopf gedrückt
  if (eingabeOffen && touchRead(startknopf) < 20 && !blockiert) { // && eingabe != 0 && digitalRead(startknopf) == LOW) {
    Serial.print("Festgesetzt: ");
    eingabeOffen = false;
    befehl(eingabe);
  }
  if (touchRead(startknopf) >= 20) { //digitalRead(startknopf) == HIGH) { //Eingabe öffnen, wenn Startknopf nicht gedrückt
    eingabeOffen = true;
  }

  if (dauerbefehlnr != 0) {
    dauerBefehl();
  }

  if (zustand == 1 && millis()-timer >= hochfahrzeit) {
    zustand = 2;
    standartAn();
    Serial.println("Timer durch.");
  }

  // //Deepsleep starten
  // if (digitalRead(SCHLAFPIN) == HIGH) {
  //   Serial.println("Going to sleep now");
  //   esp_deep_sleep_start();
  // }
}
