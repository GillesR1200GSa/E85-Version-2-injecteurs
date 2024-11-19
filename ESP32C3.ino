// Utiliser Geekble Mini ESP32-C3
#include <WiFi.h>                                          // Wi-Fi
#include <WebServer.h>                                     // Serveur web
#include <Wire.h>                                                 // I2C
//#include <Adafruit_GFX.h>                                         // graphique pour SSD1306 Ecran OLED
#include <Adafruit_SSD1306.h>                                     // controleur OLED SSD1306
#include <OneWire.h>                                              // communication 1 fil
#include <DallasTemperature.h>                                    // 18b20 mesure de -55°C à + 125°C
#include <EEPROM.h>                                               // gestion de l'eeprom
// déclaration des variables --------------------------------------------------------------------------------------------------
const char* ssid = "NodeMCU";                                     // SSID
const char* password = "NodeMCU85";                               // Mdp
byte coef;                                                        // coef = coéfficient d'enrichissement
bool Fifty;                                                       // A true, enrichissement à 100% pour démarrer
byte TStart;                                                      // TStart = Temp max pour utilisation du starter
byte TpS;                                                         // Décompte du temps de starter
int TpsV = 120;                                                   // Tempo pour la Veille    
byte Duree;                                                       // Durée du starter en secondes
byte TSonde;                                                      // Température de la sonde
String Page;                                                      // Page est une chaine qui contient la page HTML 
bool Veille = false;                                              // True = ecran éteint
bool Starter;                                                     // false = pas de starter, true = démarrage au starter
bool Demar = false;                                               // Démarrage moteur envoyé par le PIC
byte Msg = 0;                                                     // Compteur pour afficher les messages de connection WiFi
byte NbC = 0;                                                     // Nombre de Cycles, pour le tps d'affichage de chaque info            
// déclaration des E/S --------------------------------------------------------------------------------------------------------
int BP = 5;                                                       // le Bouton Poussoir est sur GPIO 05 = pin 09
int PicA = 3;                                                     // A envoi au PIC du Coef sur GPIO 03 = pin 04
int PicB = 4;                                                     // B envoi au PIC du Coef sur GPIO 04 = pin 05
int PicC = 0;                                                     // C envoi au PIC du Coef sur GPIO 00 = pin 01
int PicD = 1;                                                     // D envoi au PIC du Coef sur GPIO 01 = pin 02
int RMA = 2;    //                                                // PIC envoie Démarrage   sur GPIO 02 = pin 03
int TH = 6;                                                      // capteur température 1 fil   GPIO 06 = pin 10
// INITIALISATION ECRAN OLED --------------------------------------------------------------------------------------------------
Adafruit_SSD1306 display(128, 64, &Wire, -1);                     // Paramètres : OLED Hauteur, Largeur, Protocole, Reset
// INITIALISATION Capteur temp DALLAS 18B20
OneWire oneWire(TH);                                              // Initialisation Com 1 fil sur variable TH
DallasTemperature sensors(&oneWire);                              // Init capteur température                  
// INITIALISATION RESEAU AVANT SETUP
IPAddress local_ip(10,10,10,1);
IPAddress gateway(10,10,10,1);
IPAddress subnet(255,255,255,0);
WebServer server(80);
void setup() {//------------------------------------------------------------------------------------------------------------
  //Serial.begin(921600);                                           // Moniteur série
  //Serial.println("Démarrage");
  sensors.begin();                                                // Démarrage DS 18b20  
  // Lecture EEPROM---------------------------------------------------------------------------------------------------------
  EEPROM.begin(3);                                                // Initialisation de l'EEPROM pour 2 octets
  EEPROM.get(0, coef);                                            // Lecture EEPROM adresse 0, pour Coef
  if (coef > 10) {                                                 // Si Coef lu dans EEPROM > 10 
    coef = 0;                                                     //   alors Coef = 0
   EEPROM.put(0, coef); EEPROM.commit();                          //   écriture dans l'EEPROM
  }
  EEPROM.get(1, TStart);
  if (TStart > 50) {
    TStart = 20;
    EEPROM.put(1, TStart); EEPROM.commit();
  }  
  EEPROM.get(2, Duree);
  if (Duree > 30) {
    Duree = 10;
    EEPROM.put(2, Duree); EEPROM.commit();
  }
  // Démarrage serveur web -------------------------------------------------------------------------------------------------
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  server.on("/", handle_OnConnect);         //Le serveur reçoit une requète sur la racine ==> appel de la fonction "handle_OnConnect"
  server.on("/bp1plus", handle_bp1plus);
  server.on("/bp1moins", handle_bp1moins);
  server.on("/bp2plus", handle_bp2plus);
  server.on("/bp2moins", handle_bp2moins);
  server.on("/bp3plus", handle_bp3plus);
  server.on("/bp3moins", handle_bp3moins);
  server.onNotFound(handle_NotFound);  
  server.begin();
  // CONFIG Ecran OLED
  Wire.begin(8,9);                                              // Initialisation liaison I2C : SDA=GPIO14 & SCL=GPIO12
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);                      // Initialisation OLED
  display.clearDisplay();                                         // Clear the buffer
  display.display();                                              // Refresh (apply command)
  display.setTextColor(1);                                        // caractères en blanc
  display.display();                                              // Rafraichissement de l'écran = application des commandes précédentes
  //---------------------
  sensors.requestTemperatures();                                  // Acquisition de la température
  TSonde = sensors.getTempCByIndex(0);                            // TSonde = la température mesurée
  // INITIALISATION DES ENTREES/SORTIES
  pinMode(PicA, OUTPUT);                                          // PicA est une sortie A
  pinMode(PicB, OUTPUT);                                          // PicB est une sortie B
  pinMode(PicC, OUTPUT);                                          // PicC est une sortie C
  pinMode(PicD, OUTPUT);                                          // PicD est une sortie D
  pinMode(RMA, INPUT);                                            // RMA est une entrée Retour MArche
  pinMode(BP, INPUT_PULLUP);                                      // GPIO10 (RX) est une entrée BP Coef +1 
 }
void loop() {//____________________________________________________________________________________________________
  server.handleClient();                                                // Gestion des actions du client Web
  // Gestion des compteurs pour affichage des messages de connexion WIFI ----------------------------------------------
  NbC +=1;                                                              // Compteur de temps de cycles pour durée d'affichage des messages
  if (NbC == 5){
    NbC = 0;
    Msg +=1;
    if (Msg ==8) {Msg = 0;}
  }
  if (digitalRead(BP) == LOW){                                         // SI appui sur BP
    if (Veille == true){                                                  // si on est en veille = écran éteint
      Veille = false; TpsV = 120;                                         // on n'y est plus et réinitialise la tempo
    } else {                                                            // SINON
      if (coef < 10 ){coef +=1;} else {coef =0;}                          // SI Coef < 10 alors coef +1 sinon Coef = 0
      EEPROM.put(0, coef); EEPROM.commit();                               // écriture de COEF dans l'EEPROM à l'adresse 0
    }
  }
  sensors.requestTemperatures();                                        // Acquisition de la température
  TSonde = sensors.getTempCByIndex(0);                                  // Acquisition de la température
  if (TSonde <= TStart) {Starter = true;} else {Starter = false;}       // Starter = true/false
  // AFFICHAGE OLED : 1ère ligne -------------------------------------------------------------
  display.clearDisplay();                                             // RAZ écran
  if (Veille == false){
    display.setTextSize(1);                                             // Taille des caractères = 1
    display.setCursor(0,0);                                             // Curseur en position x=0 y=0
    display.print("T Starter = ");                                      // Affiche 
    display.print(TStart);
    display.print("C / ");
    display.print(Duree);
    display.print("s");
    // AFFICHAGE OLED : 2è ligne-----------------------
    display.setCursor(0,9);
    if(TSonde > 125) {                                                  // SI la température sonde sup à 125 °C
      display.print("DEFAUT CAPTEUR TEMP.");                                //   Affiche " DEFAUT CAPTEUR TEMP."
    } else {                                                            // Sinon Température différente de -127
      display.print("TEMP. SONDE = ");                                     //      Affiche " TEMP. = "
      display.print(TSonde);                                               //      Affiche xx 
      display.print(" C");                                                 //      Affiche " C"
    }
    // AFFICHAGE OLED : 3è ligne-----------------------
    display.setTextSize(3);
    if(coef==0){                                                        // Si COEF = 0
      display.setCursor(0,26); display.print("ESSENCE");                   // Affiche ESSENCE
    } else {                                                            // SINON
      if(coef <4){display.setCursor(35,26);} else {display.setCursor(24,26);}
      if(Fifty==true){
        display.setCursor(0,26); display.print("STARTER");                // Affiche STARTER
      } else {
        display.print(coef *3); display.print(" %");                      // Affiche COEF %
      }
    }
    // AFFICHAGE OLED : 4è ligne-------------------------------------
    display.setCursor(0,56);
    display.setTextSize(1);                                             // Taille des caractères = 1
    if(Msg==0){display.print("En WiFi connecter :");}  
    if(Msg==1){display.print("SSID : NodeMCU");}
    if(Msg==2){display.print("Pass : NodeMCU85");}
    if(Msg==3){display.print("Dans un navigateur,");}
    if(Msg==4){display.print("taper et valider :");}
    if(Msg==5){display.print("http://10.10.10.1");}
    if(Msg==6){display.print("Penser a arreter les");}
    if(Msg==7){display.print("donnees mobiles.");}
  }  
  display.display();                              // Refresh (apply command)
// DEMARRAGE -----------------------------------------
if (coef == 0){Demar = true;}                             // si on est à l'essence ou chaud, 
if(Demar == false && digitalRead(RMA) == HIGH ){          // Gestion du démarrage au starter
  Demar = true;                                                               // pour ne le faire qu'une fois par retour marche
  TpS = Duree;                                                                // temps restant de starter = Durée
}
 // Envoi du COEF au PIC---------------------------------------------------------------------
  if(TpS > 0 && coef > 3 && TSonde <= TStart) {                               // si Tempo starter + >9% + froid 
    digitalWrite(PicA,HIGH); digitalWrite(PicB,HIGH);                         // Les 4 bits à 1
    digitalWrite(PicC,HIGH); digitalWrite(PicD,HIGH);                         //
    Fifty = true;                                                             // Flag Starter en cours
    TpS -=1;
  } else {                                                   // Sinon le reste du temps
    if(coef & 1) {digitalWrite(PicA,HIGH);} else {digitalWrite(PicA,LOW);}    // Valeur binaire A
    if(coef & 2) {digitalWrite(PicB,HIGH);} else {digitalWrite(PicB,LOW);}    // Valeur binaire B
    if(coef & 4) {digitalWrite(PicC,HIGH);} else {digitalWrite(PicC,LOW);}    // Valeur binaire C
    if(coef & 8) {digitalWrite(PicD,HIGH);} else {digitalWrite(PicD,LOW);}    // Valeur binaire D
    Fifty = false;
    TpS = 0;
  }
  if (TpsV > 0){                                              // Si timer Tempo positif
    TpsV -=1;                                                 // -1
  } else {                                                    // Mise en veille
    Veille=true;
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);                                      // Arrêt du WIFI 100mA --> 20mA
  }                                    // Décompte du TIMER de mise en veille
  //Serial.println(".");
  delay(970);  // ajuster avec le moniteur série avec horodatage pour un temps de cycle de 1 seconde
}// Void loop() terminé____________________________________________________________________________________________
void handle_OnConnect() {                     //
  server.send(200, "text/html", SendHTML(coef,TStart,Duree,TSonde)); //200 = requète OK donc réponse
}
void handle_bp1plus() {  
  if(coef < 10){coef +=1;}                                                // si COEF < 10 alors +1
  EEPROM.put(0, coef); EEPROM.commit();                                   // écriture dans l'EEPROM
  server.send(200, "text/html", SendHTML(coef,TStart,Duree,TSonde)); 
  if (Veille == true){Veille = false;}                                    // si on est en veille, on y est plus
  TpsV = 120;                                                             // tempo de veille au max
}
void handle_bp1moins() {
  if(coef > 0){coef -=1;}                                                 // si COEF > 1 alors COEF =  -1
  EEPROM.put(0, coef); EEPROM.commit();                                   //   écriture dans l'EEPROM
  server.send(200, "text/html", SendHTML(coef,TStart,Duree,TSonde));
  if (Veille == true){Veille = false;}                                    // si on est en veille, on y est plus
  TpsV = 120;                                                             // tempo de veille au max 
}
void handle_bp2plus() {  
  if(TStart < 50){TStart +=1;}                                            // TEMPERATURE POUR STARTER : +1 et Valeur max = 50 °C
  EEPROM.put(1, TStart); EEPROM.commit();
  server.send(200, "text/html", SendHTML(coef,TStart,Duree,TSonde)); 
  if (Veille == true){Veille = false;}                                    // si on est en veille, on y est plus
  TpsV = 120;                                                             // tempo de veille au max 
}
void handle_bp2moins() {
  if(TStart > 0){TStart -=1;}                                             // TEMPERATURE POUR STARTER : -1 et Valeur min = 0 °C
  EEPROM.put(1, TStart); EEPROM.commit();
  server.send(200, "text/html", SendHTML(coef,TStart,Duree,TSonde));
  if (Veille == true){Veille = false;}                                    // si on est en veille, on y est plus
  TpsV = 120;                                                             // tempo de veille au max 
}
void handle_bp3plus() {
  if(Duree < 30){Duree +=1;}                                              // DUREE STARTER : +1 et valeur max 30s
  EEPROM.put(2, Duree); EEPROM.commit();
  server.send(200, "text/html", SendHTML(coef,TStart,Duree,TSonde));
  if (Veille == true){Veille = false;}                                    // si on est en veille, on y est plus
  TpsV = 120;                                                             // tempo de veille au max 
}
void handle_bp3moins() {
  if(Duree > 2){Duree -=1;}                                               // DUREE STARTER : -1 et valeur min 2s
  EEPROM.put(2, Duree); EEPROM.commit();
  server.send(200, "text/html", SendHTML(coef,TStart,Duree,TSonde));
  if (Veille == true){Veille = false;}                                    // si on est en veille, on y est plus
  TpsV = 120;                                                             // tempo de veille au max 
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}
  String SendHTML(uint8_t Coef,uint8_t TStart,uint8_t Duree,uint8_t TSonde){                                                                    // Variables transmises pour la création de la page HTML
  Page = "<!DOCTYPE html> <html>\n";
  Page +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  Page +="<title>E85</title>\n";                                                                                                                // Titre dans l'onglet du navigateur
  Page +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  Page +="body{margin-top: 0px;} h1 {color: #444444;margin: 10px auto 30px;} h3 {color: #444444;margin-bottom: 10px;}\n";
  Page +=".button {display: block;width: 80px;background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;";   // Description du bouton
  //Page +="font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  Page +="font-size: 25px;margin: 0px auto 6px;cursor: pointer;border-radius: 4px;}\n";
  Page +=".button-on {background-color: #1abc9c;}\n";
  Page +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  Page +="</style>\n";
  Page +="</head>\n";
  Page +="<body>\n";
// --------------------------------------- MA ZONE MODIFIABLE -----------------------------------------------------------------------------
  Page +="<h1> Enrichissement = ";                                        // Affichage de l'enrichissement
  Page += (coef *3);                                                      // soit COEF x 3
  Page +=" %";
  Page +="<a class=\"button button-on\" href=\"/bp1plus\">+</a>\n";       //    BP 1 plus = Enrichissement +
  Page +="<a class=\"button button-on\" href=\"/bp1moins\">-</a>\n";      //    BP 1 moins= Enrichissement -
  Page +="<hr />";                                                        // Ligne grise de séparation
  Page += "T. max starter = ";                                       // Affichage de la T° maxi starter      
  Page += TStart;
  Page +=" &#8451";                                                       // code HTML du °C
  Page +="<a class=\"button button-on\" href=\"/bp2plus\">+</a>\n";       //    BP 2 plus = Temp maxi starter +
  Page +="<a class=\"button button-on\" href=\"/bp2moins\">-</a>\n";      //    BP 2 moins = Temp maxi starter -
  Page +="<hr />";                                                        // Ligne grise de séparation
  Page +="Dur";
  Page += "&eacute";  // = é
  Page += "e du starter = ";
  Page += Duree;
  Page += " s";
  Page +="<a class=\"button button-on\" href=\"/bp3plus\">+</a>\n";       //    BP 3 plus = Temp maxi starter +
  Page +="<a class=\"button button-on\" href=\"/bp3moins\">-</a>\n";      //    BP 3 moins = Temp maxi starter -
  Page +="<hr />";                                                        // Ligne grise de séparation
  if(TSonde > 125){
    Page +="Defaut capteur temperature";
  } else {
    Page += "T. sonde = ";                                              // Température sonde =
    Page += TSonde;
    Page +=" &#8451";                                                       // code HTML du °C
  }
  Page +=" </h1>";
//-------------------------------------------------------------------------------------------------------------------------------------------
  Page +="</body>\n";
  Page +="</html>\n";
  return Page;
}