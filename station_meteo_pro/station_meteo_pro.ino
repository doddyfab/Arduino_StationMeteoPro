/*
  Station Meteo Pro
  avec : 
     - Arduino Mega 
     - Carte Ethernet W55100 avec Carte SD
     - Anémomètre Lextronic   LEXCA003
     - Girouette Lextronic    LEXCA002
     - Pluviomètre Lextronic  LEXCA001
     - Interfacage Lextronic Grove avec module SLD01099P
     - Capteur BME280 pour temperature, humidité, pression atmosphérique

  Le programme sauvegarde les données : 
     - sur carte SD locale, par dossier Année/Mois puis un fichier par jour
     - sur un synology en fichier texte
     - dans une base MySQL sur le serveur météo

  La gestion du temps se fait par NTP, pas d'horloge externe type DS1307
  
  Source :     https://www.sla99.fr
  Site météo : https://www.meteo-four38.fr
  Date : 2010-09-05

  Changelog : 
  26/10/2019  v1.2  Ajout capteur SHT31 pour temperature et humidité en remplacement du BME280
  16/09/2019  v1.1  Calibration temperature avec ajout offset
  05/09/2019  v1    version initiale

*/


/* 
 *  Fichiers d'entête des librairies
 */
#include <Ethernet.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <Adafruit_SHT31.h>

/* 
 *  Variables statiques
 */
#define GIROUETTE   A1  //port analogique A1
#define ANEMOMETRE  1   //pin D3, interruption n°1
#define PLUVIOMETRE 0   //pin D2, interruption n°0
#define VALEUR_PLUVIOMETRE 0.2794 //valeur en mm d'eau à chaque bascule d'auget
#define LED_SD    6     //LED bleu pour état carte SD
#define LED_NET   7     //LED verte pour état réseau
#define LED_ERR  8      //LED rouge pour erreur 
#define BME_CS    10
#define BME_MOSI  11
#define BME_MISO  12
#define BME_SCK   13
#define PI        3.1415
#define RAYON     0.07  //rayon en mètre de l'anémomètre en mètre
#define ALTITUDE  360   //altitude de la station météo
#define TEMP_OFFSET -2  //offset température 

/* 
 *  Variables globales
 */
String KEY_WS="134567654345670012";   //clé d'appel du webservice
unsigned long previousMillis=   0;
unsigned long previousMillis2=  0;
unsigned long delaiAnemometre = 3000L;    //3 secondes
unsigned long delaiProgramme =  60000L;   //60 sec
float gust(0);        //vent max cumulé sur 1 min
float wind(0);        //vent moyen cumulé sur 1 min
int nbAnemo = 0;      //nb d'occurence de mesure Anemo
float gir(0);         //direction moyenne de la girouette sur 1 min (en degrés)
int nbGir = 0;        //nb d'occurence de mesure Anemo
float pluvio1min(0);  //pluie sur 1 min
float vitesseVent(0); //vent moyen cumulé sur 1 min
float temp(0);        //température moyenne sur 1 min
float hum(0);         //température moyenne sur 1 min
float pressure(0);    //température moyenne sur 1 min
int nbBME280 = 0;     //nb d'occurence d'appel du capteur BME280
bool SDStatus = false;//etat initial de la carte SD
volatile unsigned int countAnemometre = 0;  //variable pour l'interruption de l'anémomètre pour compter les impulsions
volatile unsigned int countPluviometre = 0; //variable pour l'interruption du pluviomètre pour compter les impulsions
byte mac[6] = { 0xBE, 0xEF, 0x00, 0xFD, 0xb7, 0x91 }; //mac address de la carte Ethernet Arduino
char macstr[18];
const byte SDCARD_CS_PIN = 10;  //port de la carte SD
boolean debug = true;  //TRUE = ecriture du programme, active tous les Serial.Println ; FALSE = aucun println affichés


/* 
 *  Variables globales d'initialisation
 */
Adafruit_BME280 bme;  //BME280
Adafruit_SHT31 sht31 = Adafruit_SHT31(); //SHT31
File myFile;          //fichier de stockage sur la carte SD
char server[] = "192.168.1.2";  //IP du synology
EthernetClient client;          //client pour appeler le webservice sur le synology

/* 
 *  Setup initial de l'arduino
 */
void setup()
{
  delay(2000);   //initialisation de la carte ethernet 
  Serial.begin(9600);

  pinMode(PLUVIOMETRE, INPUT_PULLUP);
  pinMode(ANEMOMETRE, INPUT_PULLUP);
  pinMode(LED_SD, OUTPUT);
  pinMode(LED_NET, OUTPUT);
  pinMode(LED_ERR, OUTPUT);  
  attachInterrupt(PLUVIOMETRE,interruptPluviometre,RISING) ;
  attachInterrupt(ANEMOMETRE,interruptAnemometre,RISING) ;
  bool status = bme.begin(0x76);

  //démarrage SHT31
  if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  //on teste l'ouverture de la carte SD.
  //si OK : clignotement de la led SD 1 fois
  //si KO : on laisse allumé la led ERROR et la led SD
  if (!SD.begin(4)) {    
    if(debug == true){
      Serial.println("initialization SD failed!");
    }
    digitalWrite(LED_ERR, HIGH);
    digitalWrite(LED_SD, HIGH);
  }
  else{
    SDStatus = true;
    digitalWrite(LED_SD, HIGH);
    delay(500);
    digitalWrite(LED_SD, LOW);
  }

  //Démarrage du réseau
  //Si OK : clignotement de la led NETWORK 1 fois
  //si KO : on laisse allumé la led ERROR et la led NETWORK
  if(!Ethernet.begin(mac)){
    if(debug == true){
      Serial.println("Network failed");
    }
    digitalWrite(LED_ERR, HIGH);
    digitalWrite(LED_NET, HIGH);
  }
  else{     
    if(debug == true){
      Serial.println(Ethernet.localIP());
      Serial.println(Ethernet.gatewayIP());
    }
    digitalWrite(LED_NET, HIGH);
    delay(500);
    digitalWrite(LED_NET, LOW);
  } 

  //Démarrage de la synchro NTP
  NTP.begin("fr.pool.ntp.org", 1, true);
  NTP.setInterval(60);
  NTP.setDayLight(true);
  if(debug == true){
    Serial.println (NTP.getTimeDateString ()); 
    Serial.println (NTP.getDateStr());
    Serial.println (NTP.getTimeStr());
    Serial.println (getNTPDateFR(NTP.getDateStr()));
    Serial.println (getNTPTimeFR(NTP.getTimeStr()));  
    Serial.println("initialization done.");
  }
}

/* 
 *  Fonction qui transforme la date fournie par le NTP en date YMD
 *  RETURN : date(YMD)
 */
String getNTPDateFR(String date){
  String dateY = getValue(date,'/',2);
  String dateM = getValue(date,'/',1);
  String dateD = getValue(date,'/',0);
  return dateY+dateM+dateD;
}

/* 
 *  Fonction qui transforme la date fournie par le NTP en date Y-M-D pour l'insertion MySQL
 *  RETURN : date(Y-M-D)
 */
String getNTPDateFRForMySQL(String date){
  String dateY = getValue(date,'/',2);
  String dateM = getValue(date,'/',1);
  String dateD = getValue(date,'/',0);
  return dateY+'-'+dateM+'-'+dateD;
}

/* 
 *  Fonction qui transforme l'heure fournie par le NTP en heure Hi
 *  RETURN : time(Hi)
 */
String getNTPTimeFR(String hour){
  String timeH = getValue(hour,':',0);
  String timeM = getValue(hour,':',1);
  return timeH+timeM;
}

/* 
 *  Fonction qui transforme l'heure fournie par le NTP en heure time(H:i) pour l'insertion MySQL
 *  RETURN : time(H:i)
 */
String getNTPTimeFRForMySQL(String hour){
  String timeH = getValue(hour,':',0);
  String timeM = getValue(hour,':',1);
  return timeH+':'+timeM;
}

/* 
 *  Fonction qui génère le nom du dossier d'enregistrement sur carte SD en fonction de l'année et mois en cours
 *  RETURN : nom du dossier au format date(YM)
 */
String setFolderName(String date){
  String dateY = getValue(date,'/',2);
  String dateM = getValue(date,'/',1);
  return dateY+'/'+dateM;
}

/* 
 *  Fonction qui génère le nom du fichier d'enregistrement sur carte SD en fonction de l'année, du mois et jour en cours
 *  RETURN : nom du fichier (incluant les répertoires) au format date(YMD).txt
 */
String setFileName(String date){
  String dateY = getValue(date,'/',2);
  String dateM = getValue(date,'/',1);
  String dateD = getValue(date,'/',0);
  return dateY+dateM+dateD+'.txt';
}

/* 
 *  Fonction qui permet de séparer une chaine en fonction d'un caractère (equivalent explode PHP) et du numéro d'index voulu
 *  RETURN :chaine extraite dans la position de l'index
 */
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

/* 
 *  Fonction qui permet de générer la date et l'heure courante
 */
void dateTime(uint16_t* date, uint16_t* time) {
  time_t t = now();
 *date = FAT_DATE(year(t), month(t), day(t));  
 *time = FAT_TIME(hour(t), minute(t), second(t));
}

/* 
 *  Fonction qui corrige la pression atmosphérique (hPa) en fonction de l'altitude et la température
 *  RETURN : pression atmo corrigée
 */
//fonction qui corrige la pression en fonction de l'altitude
double getP(double Pact, double temp) {
  return Pact * pow((1 - ((0.0065 * ALTITUDE) / (temp + 0.0065 * ALTITUDE + 273.15))), -5.257);
}

/* 
 *  Fonction d'interruption de l'anémomètre qui incrémente un compteur à chaque impulsion
 */
void interruptAnemometre(){
  countAnemometre++;
}

/* 
 *  Fonction d'interruption du pluviomètre qui incrémente un compteur à chaque impulsion
 */
void interruptPluviometre(){
  countPluviometre++;
}


/* 
 *  Fonction qui converti en angle la valeur analogique renvoyée par l'arduino (valeurs fixes)
 *  RETURN : angle en degré
 */
float getGirouetteAngle(int value){
  float angle = 0;
  if (value > 280 && value < 290) angle = 180;
  if (value > 240 && value < 250) angle = 202.5;
  if (value > 628 && value < 636) angle = 225;
  if (value > 598 && value < 606) angle = 247.5;
  if (value > 940 && value < 950) angle = 270;
  if (value > 824 && value < 832) angle = 292.5;
  if (value > 884 && value < 892) angle = 315;
  if (value > 700 && value < 710) angle = 337.5;
  if (value > 784 && value < 792) angle = 0;
  if (value > 402 && value < 412) angle = 22.5;
  if (value > 458 && value < 468) angle = 45;
  if (value > 78 && value < 85)   angle = 67.5;
  if (value > 88 && value < 98)   angle = 90;
  if (value > 60 && value < 70)   angle = 112.5;
  if (value > 180 && value < 190) angle = 135;
  if (value > 122 && value < 132) angle = 157.5;
  return angle;
}

/* 
 *  Programme principal
 */
void loop(){  
  
  unsigned long currentMillis = millis(); // read time passed

  //Récupération des infos de l'anémomètre et girouette toutes les 3 sec
  //Enregistrement cumulé des valeurs
  if (currentMillis - previousMillis > delaiAnemometre){
    previousMillis = millis();
    vitesseVent = (PI * RAYON * 2 * countAnemometre)/3*3.6; //3 = durée de prise de mesure (3sec)
    
    if(vitesseVent>gust) gust = vitesseVent;
    wind += vitesseVent;
    nbAnemo++;
    countAnemometre = 0;

    int gdir = analogRead(GIROUETTE);
    gir += getGirouetteAngle(gdir);
    nbGir++;

    /*
    double val1 = bme.readTemperature();
    val1 = val1 + TEMP_OFFSET;
   */
    double val1 = sht31.readTemperature();
    temp += val1;
    
    double P = getP((bme.readPressure() / 100.0F), val1);
    pressure += P;
   // double humidity = bme.readHumidity();
    double humidity = sht31.readHumidity();
    hum += humidity;
    nbBME280++;

    if(debug == true){
      Serial.println("------------------------");
      Serial.print("#");
      Serial.println(nbGir);
      Serial.println("------------------------");
      Serial.print("Temp:");
      Serial.println(val1);
      Serial.print("Pression:");
      Serial.println(P);
      Serial.print("Humidité:");
      Serial.println(humidity);
      Serial.print("Vent:");
      Serial.println(vitesseVent);
      Serial.print("Pluvio:");
      Serial.println(countPluviometre);
      Serial.print("Girouette:");
      Serial.println(getGirouetteAngle(gdir));
      Serial.println("------------------------");
    }    
  }

  //Toutes les minutes, compilation des valeurs et envoi au serveur  
  if (currentMillis - previousMillis2 > delaiProgramme){
    previousMillis2 = millis();
    float avgwind = wind / nbAnemo;
    float avggir = gir / nbGir;

    float avgtemp = temp / nbBME280;
    float avghum = hum / nbBME280;
    float avgpressure = pressure / nbBME280;

    pluvio1min = countPluviometre*VALEUR_PLUVIOMETRE;
    countPluviometre = 0;

    if(debug == true){
      Serial.println("------------------------");
      Serial.println (NTP.getTimeDateString ());
      Serial.println (NTP.getUptimeString()); 
      Serial.println("------------------------");
      Serial.print("Wind AVG : ");
      Serial.println(avgwind);
      Serial.print("Gust : ");
      Serial.println(gust);
      Serial.print("Girouette : ");
      Serial.println(avggir);    
      Serial.print("pluvio1min : ");
      Serial.println(pluvio1min);
      Serial.print("temp1min : ");
      Serial.println(avgtemp);
      Serial.print("hum1min : ");
      Serial.println(avghum);
      Serial.print("pressure1min : ");
      Serial.println(avgpressure);
      Serial.println("------------------------");
      Serial.println("------------------------");
    }

    //ligne qui sera enregistrée sur la carte SD, sur le synology et qui servira pour l'insertion SQL
    String lineToWrite = getNTPDateFRForMySQL(NTP.getDateStr())+' '+getNTPTimeFRForMySQL(NTP.getTimeStr())+';'+String(avgtemp)+';'+String(avghum)+';'+String(avgwind)+';'+String(gust)+';'+String(avggir)+';'+String(avgpressure)+';'+String(pluvio1min);

    //20/10/2019
    //Modification de l'emplacement des RAZ : fin de programme -> avant insertion BDD et carte SD pour ne plus avoir la latence.
    //RAZ des compteurs qui ont servi a calculé les valeurs moyennes sur 1 min
    wind = 0;
    gust = 0;
    nbAnemo = 0;
    gir = 0;
    nbGir = 0;
    temp = 0;
    hum = 0;
    pressure = 0;
    nbBME280 = 0;
    
    //si la carte SD est opérationnelle, écriture de la ligne
    if(SDStatus == true){
      //création de l'arborescence Y/M si non existante
      SD.mkdir(setFolderName(NTP.getDateStr()));
      if(debug == true) Serial.println(setFolderName(NTP.getDateStr()));
      String filename = setFolderName(NTP.getDateStr())+'/'+getNTPDateFR(NTP.getDateStr());
      filename = filename + ".txt";
      myFile = SD.open(filename, FILE_WRITE);
      if (myFile) {
        digitalWrite(LED_SD, HIGH);
        if(debug == true){
          Serial.print("Writing to ");
          Serial.println(filename);
        }        
        
        //On ajoute au fichier texte une date et heure de modification (01/01/1970 00:00 par défaut sinon)
        SdFile::dateTimeCallback(dateTime);
        myFile.println(lineToWrite);
        myFile.close();
        
        //on éteint les LED si elles étaient en erreur
        digitalWrite(LED_SD, LOW);
        if(digitalRead(LED_ERR) == HIGH) digitalWrite(LED_ERR, LOW);    
      } 
      else {
        if(debug == true){
          Serial.print("error opening ");
          Serial.println(filename);
        }
        digitalWrite(LED_ERR, HIGH);
        digitalWrite(LED_SD, HIGH);
      }
    }
    
    //On va appeler l'url sur le synology pour sauvegarder les données
    //La clé sert pour ne pas appeler abusivement de webservice avec les données erronées depuis n'importe où
    if (client.connect(server, 81)) { 
      digitalWrite(LED_NET, HIGH);
      Serial.println("connected");
      client.println("GET /stationmeteo/stationmeteo.php?key="+KEY_WS+"&line="+lineToWrite+" HTTP/1.1");
      client.println("Host: 192.168.1.2");
      client.println("Connection: close");
      client.println();
      digitalWrite(LED_NET, LOW);
    } 
    else {
      digitalWrite(LED_ERR, HIGH);
      digitalWrite(LED_NET, HIGH);
     if(debug == true)  Serial.println("connection failed");
    }

    
    
  }

  //Affichage de la réponse du serveur dans le moniteur série si on est en débug
  if(client.available()) {
    char c = client.read();
    Serial.print(c);
  }
  if(!client.connected()) {
    client.stop();
  }  
} 

