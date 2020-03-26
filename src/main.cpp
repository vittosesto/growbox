// Include the libraries we need
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <BlueDot_BME280.h>
//#include <elapsedMillis.h>
#include <neotimer.h>
//#include <Adafruit_SSD1306.h>
// #include <splash.h>
// #include <U8x8lib.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>

#define AIR_HEATER_PIN 8
#define GROUND_HEATER_PIN 6
#define FAN_PIN 5
// #define LIGHT_PIN 8
#define HUMIDIFIER_PIN 7
#define BUZZER_PIN A3

#define ONE_WIRE_BUS 9 //for ground sensors
#define RELAYS_ON LOW
#define RELAYS_OFF HIGH


// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);


//BlueDot_BME280
BlueDot_BME280 bme1;                                     //Object for Sensor 1
//BlueDot_BME280 bme2;                                     //Object for Sensor 2
int bme1Detected = 0;                                    //Checks if Sensor 1 is available
//int bme2Detected = 0;                                    //Checks if Sensor 2 is available

float BME_280_correction = -0.2; //-1.5;    //correzione della temperatura perche' si autoriscalda
/*
const int displayWidth = 128;
const int displayHeight = 32;

const int homeTopBoxWidth=displayWidth/3; //oled top menu sizes
const int homeTopBoxHeight=16;
bool groundTempDisplaying=true; //to alternate the temperature displaying on the home screen
*/
// Adafruit_SSD1306 display(displayWidth, displayHeight, &Wire, -1);
// U8X8_SSD1306_128X32_UNIVISION_SW_I2C u8x8(SCL, SDA, U8X8_PIN_NONE);

RTC_DS3231 rtc;
DateTime now;

const byte VER=1;
const byte BUFFERSIZE=100;
char strBuffer[BUFFERSIZE] = "";

float minAirTemp = 22.0; //20.0
float maxAirTemp = 30.0;  //28.0



float groundTempMin = 25.0; //30.0
float groundTempMax = 27.0; //35.0
float groundTempMinAlarm = 22.0; // below this temperature turn on the alarm
float groundTempMaxAlarm = 30.0; // above this temperature turn on the alarm

float airHumMin = 55.00;
float airHumMax = 75.00;
float airHumMinAlarm = 45.0;
float airHumMaxAlarm = 85.0;

float airTemp1 = 0.0;
float groundTemp1 = 0.0;
float airHum1 = 0.0;

bool airHeaterON = false;
bool groundHeaterON = false;
bool fanON = false;
bool humFanOn = false;
bool humidifierON = false;
bool alarm = false;

unsigned long fanInterval =  3600000; // 1 hour
unsigned long fanDuration =  300000;  // 5 minutes 

/* timers */
Neotimer printSerialTimer = Neotimer(60000);
Neotimer lcdRefreshTimer = Neotimer(1000);
Neotimer fanDurationTimer = Neotimer(fanDuration); 
Neotimer fanIntervalTimer = Neotimer(fanInterval);  
Neotimer logToSdIntervalTimer = Neotimer(60000); //every minute

// Neotimer airTemp1Delay = Neotimer(1000);
// Neotimer airHum1Delay = Neotimer(1000);

/* for data logging on SD */
const int chipSelect = 10;
File dataFile;

void readAllSensors(){
//TODO add check for reading frequency

  airTemp1 = bme1.readTempC()+BME_280_correction;
  sensors.requestTemperatures();
  groundTemp1 = sensors.getTempCByIndex(0);
  airHum1 = bme1.readHumidity();
}

float getAirTemp1 (){
  float temp;
  temp = bme1.readTempC()+BME_280_correction;
  return temp;
}

float getTempSuolo1(){
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}
float getTempSuolo2(){
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(1);
}



float getGroundTempMin(){
  sensors.requestTemperatures();
  float tmpTemp1 = sensors.getTempCByIndex(0);
  float tmpTemp2 = sensors.getTempCByIndex(1);
  
  if (tmpTemp1 == DEVICE_DISCONNECTED_C)
  {
    return tmpTemp2;
  }
  
  if (tmpTemp2 == DEVICE_DISCONNECTED_C)
  {
    return tmpTemp1;
  }

  return min(tmpTemp1, tmpTemp2);
}

float getGroundTempMax(){


  sensors.requestTemperatures();
  float tmpTemp1 = sensors.getTempCByIndex(0);
  float tmpTemp2 = sensors.getTempCByIndex(1);
  //not necessary check the DEVICE_DISCONNECTED_C, because is the minimum number
  return max(tmpTemp1, tmpTemp2);
}


float getHAria1(){
  return bme1.readHumidity();
}


/* if status true, turns on air heating, false turns off air heating */
void airHeater(bool status){
  //TODO
  if (status)
  {
    if (!airHeaterON)
    {
      airHeaterON = true;
      digitalWrite(AIR_HEATER_PIN,RELAYS_ON);
      Serial.println(F("Air heater on!"));
    }
  }else if (airHeaterON)
  {
    airHeaterON=false;
    digitalWrite(AIR_HEATER_PIN, RELAYS_OFF);
    Serial.println(F("Air heater off!"));
  }  
}

/* if status true, turns on ground heating, false turns off ground heating */
void groundHeater(bool status){
  //TODO
    if (status)
  {
    if (!groundHeaterON)
    {
      groundHeaterON = true;
      digitalWrite(GROUND_HEATER_PIN, RELAYS_ON);
      Serial.println(F("ground heater on!"));
    }
  }else if (groundHeaterON)
  {
    groundHeaterON = false;
    digitalWrite(GROUND_HEATER_PIN, RELAYS_OFF);
    Serial.println(F("ground heater off!"));
  }  
}

/* if status true, turns on fan, false turns off fan */
void fan(bool status){
  if (status)
  {
    if (!fanON)
    {
      fanON = true;
      digitalWrite(FAN_PIN,RELAYS_ON);
  }
    }else if (fanON)
    {
      fanON = false;
      digitalWrite(FAN_PIN, RELAYS_OFF);
    }
}

/* if status true, turns on humifier, false turns off humidifier */
void humidifier(bool status){
  if (status)
  {
    if (!humidifierON)
    {
      humidifierON = true;
      digitalWrite(HUMIDIFIER_PIN,RELAYS_ON);
    }
  }else if (humidifierON)
    {
      humidifierON = false;
      digitalWrite(HUMIDIFIER_PIN, RELAYS_OFF);
    }
}



/*
 *check if necessary turn on or off air heater
*/
void checkAirHeater(){
  if (airHeaterON){
    if (getAirTemp1() > maxAirTemp) airHeater(false);
  }else if (getAirTemp1() < minAirTemp) airHeater(true);
}

/*
 *check if necessary turn on or off ground heater
*/
void checkGroundHeater(){
//controllare lo stato quando un sensore è < mintemp e l'atro > maxtemp, si alterna...


  if (groundHeaterON){
    if (getGroundTempMax() > groundTempMax){
      groundHeater(false);
    }
  }else if (getGroundTempMin() < groundTempMin){
      groundHeater(true);
  }
}


/* if too high turns on the fan, if too low, turns on humidifier */
void checkHumidity(){
  /* humidity high check -- FAN*/
  if (!humFanOn && getHAria1() > airHumMax){
    humFanOn = true; //fan on for humidity too high
    fan(true);
    Serial.println("Humidity too High - Fan On");
  }else if(humFanOn && getHAria1() < ((airHumMax - airHumMin)/2)+airHumMin){  //if fan for humidity is runnig, check if humidity is ok and turn fan off
    humFanOn = false;
    fan(false);
    Serial.println("Humidity OK - Fan Off");
  }

  /* humidity low check - humidifier */
  if (!humidifierON && getHAria1() < airHumMin){
    humidifier(true);
    Serial.println(F("Humidity too Low - Humidifier On"));
  }else if(humidifierON && getHAria1() > ((airHumMax - airHumMin)/2)+airHumMin){ 
    humidifier(false);
    Serial.println(F("Humidity OK - Humidifier Off"));
  }
}

void checkFan(){
// periodical fan cycle only if the humidity fan is off

if(!humFanOn){
  if (fanON)
  {
    if (fanDurationTimer.done())
    {
      fan(false);
      sprintf(strBuffer,"Fan off, turns on every %d secons", fanInterval/1000); // maybe better getting it from te neotimer timer
      Serial.println(strBuffer );
    }
    
  }else if (fanIntervalTimer.repeat())
  {
    /* starts the timer */
    fanDurationTimer.start();
    fan(true);
    sprintf(strBuffer,"Fan on for %d secons", fanDuration/1000); // maybe better getting it from te neotimer timer
    Serial.println(strBuffer );
  }
  
  

  if (fanIntervalTimer.repeat())
  {
    fan(true);
  }
}
}

void checkAlarm(){
  if(getGroundTempMin() < groundTempMinAlarm || getGroundTempMax() > groundTempMaxAlarm || getHAria1() < airHumMinAlarm || getHAria1() > airHumMaxAlarm){
    alarm = true;
    digitalWrite(BUZZER_PIN, HIGH);
  }else
  {
    alarm = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
  
}



void printToSerial(){
  /*
    Serial.println(F("| TAria1 | TSuolo1 | TSuolo2 | HAria1 |"));
    strcpy(strBuffer, "| ");
    dtostrf(getAirTemp1(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,"  | ");
    dtostrf(getTempSuolo1(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,"   | ");
    dtostrf(getTempSuolo2(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,"   | ");
    dtostrf(getHAria1(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,"  |\n");
    Serial.print( strBuffer );

    */
    now=rtc.now();

    // YYYY/MM/DD HH:MM:SS; Fan; AirHeater; GroundHeater; Humidifier; Alarm; AirTemp; AirHumidity; GroundTemp1; GroundTemp2
    sprintf(strBuffer,"%04d/%02d/%02d %02d:%02d:%02d;%d;%d;%d;%d;%d;", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), fanON, airHeaterON, groundHeaterON, humidifierON, alarm );

    dtostrf(getAirTemp1(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,";");
    dtostrf(getHAria1(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,";");
    dtostrf(getTempSuolo1(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,";");
    dtostrf(getTempSuolo2(), 4, 2, &strBuffer[strlen(strBuffer)]);
   // strcat(strBuffer,";");


   // if (logToSdIntervalTimer.repeat()){
      dataFile.println(strBuffer);
      dataFile.flush();
  //  }
    
    Serial.println(strBuffer);
}

/*

void printMainScreen(){
  // display.clearDisplay();  //Pulisce il buffer da inviare al display
  display.setTextSize(1);  //Imposta la grandezza del testo
  display.setTextColor(WHITE,BLACK); //Imposta il colore del testo (Solo bianco)

  //display air temp
  display.setCursor(5,homeTopBoxHeight-8); //Imposta la posizione del cursore (Larghezza,Altezza)
  display.print(getAirTemp1());

  //display ground temp1 and ground temp 2
  display.setCursor(homeTopBoxWidth*2-6, 0);
  if (groundTempDisplaying) display.print("1"); else display.print("2");

  display.setCursor(homeTopBoxWidth+5,homeTopBoxHeight-8);

  float tmpTemp = groundTempDisplaying?getTempSuolo1():getTempSuolo2();
  //checks if is a valid value
  if (tmpTemp == DEVICE_DISCONNECTED_C){
    display.print("---");
  } else {
    display.print(tmpTemp);
  }
  
  groundTempDisplaying = !groundTempDisplaying;
  
  //display air humidity
  display.setCursor(homeTopBoxWidth*2+5,homeTopBoxHeight-8);
  display.print(getHAria1());
  display.setCursor(0, homeTopBoxHeight+1);
  now = rtc.now();
  sprintf(strBuffer, "%02d:%02d",now.hour(), now.minute());
  display.print(strBuffer);

  // relais status
  display.setCursor(0,displayHeight-8);
  sprintf(strBuffer,"fan:%d heat:%d grou:%d", fanON, airHeaterON, groundHeaterON);
  display.print(strBuffer);
  display.display();
}

*/

/*PRINTS TRHE HEADER*/

/*
void printHeader(){
  int y=0;
  u8x8.setFont(u8x8_font_artosserif8_u);
  u8x8.drawString(0, y, "AIR");
  u8x8.drawString(5, y, "HUM");
  u8x8.drawString(9, y, "G1");
  u8x8.drawString(13, y, "G2");
}

void printValues(){
  int y=1;
  u8x8.setFont(u8x8_font_victoriabold8_n);

  u8x8.drawString(0, y, (int)getAirTemp1());
  u8x8.drawString(5, y, (int)getHAria1());
  u8x8.drawString(9, y, (int)getTempSuolo1());
  u8x8.drawString(13, y, (int)getTempSuolo2());


  y=3;
  u8x8.setFont(u8x8_font_artosserif8_u);
  u8x8.drawString(0, y, "FAN");
  u8x8.drawString(5, y, "HUM");
  u8x8.drawString(11, y, "GND");
}
*/
void setup(void)
{
  // start serial port
  Serial.begin(9600);
  //initialize oled display
  // u8x8.begin();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally

   
  /*
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  sprintf(strBuffer,"   Growbox V.%d          Vittorio Sestito",VER);
  display.clearDisplay();  //Pulisce il buffer da inviare al display
  display.setTextSize(1);  //Imposta la grandezza del testo
  display.setTextColor(WHITE); //Imposta il colore del testo (Solo bianco)
  display.setCursor(0,0); //Imposta la posizione del cursore (Larghezza,Altezza)
  display.println(strBuffer); //Stringa da visualizzare
  display.display(); //Invia il buffer da visualizzare al display
*/
  

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    //1 display.println("Couldn't find RTC");
    //1 display.display();
    while (1);
  }

  
/**
  if (rtc.lostPower()) {
//  if (true) { //forzo il settaggio dell'ora, DA TOGLIERE
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
*/

  pinMode(AIR_HEATER_PIN,OUTPUT);
  pinMode(GROUND_HEATER_PIN,OUTPUT);
  pinMode(FAN_PIN,OUTPUT);
 // pinMode(LIGHT_PIN, OUTPUT);
  pinMode(HUMIDIFIER_PIN,OUTPUT);
  pinMode(BUZZER_PIN,OUTPUT);

  digitalWrite(AIR_HEATER_PIN,RELAYS_OFF);
  digitalWrite(GROUND_HEATER_PIN,RELAYS_OFF);
  digitalWrite(FAN_PIN,RELAYS_OFF);
//  digitalWrite(LIGHT_PIN,RELAYS_OFF);
  digitalWrite(HUMIDIFIER_PIN,RELAYS_OFF);
  digitalWrite(BUZZER_PIN,LOW);

  Serial.print(F("Initializing SD card..."));
  //1 display.println("Initializing SD card...");
  //1 display.display();
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(SS, OUTPUT);
  
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println(F("Card failed, or not present"));
    //1 display.println("Card failed, or not present");
    //1 display.display();
    // don't do anything more:
    while (1) ;
  }
  Serial.println(F("card initialized."));
  //1 display.println("card initialized.");
  //1 display.display();
  
  // Open up the file we're going to log to!
  dataFile = SD.open("growbox.csv", FILE_WRITE);
  if (! dataFile) {
    Serial.println(F("error opening growbox.csv"));
    //1 display.println("error opening growboxLog.csv");
    //1 display.display();
    // Wait forever since we cant write data
    while (1) ;
  }



  // Start up the library DallasTemperature
  sensors.begin();


  //configurazione BME280  
  //This program is set for the I2C mode

    bme1.parameter.communication = 0;                    //I2C communication for Sensor 1 (bme1)
  //  bme2.parameter.communication = 0;                    //I2C communication for Sensor 2 (bme2)
    
  //Set the I2C address of your breakout board  

    bme1.parameter.I2CAddress = 0x76;                    //I2C Address for Sensor 1 (bme1)
  //  bme2.parameter.I2CAddress = 0x77;                    //I2C Address for Sensor 2 (bme2)
  
  bme1.parameter.sensorMode = 0b11;                    //Setup Sensor mode for Sensor 2 
  bme1.parameter.IIRfilter = 0b100;                   //IIR Filter for Sensor 1
  bme1.parameter.humidOversampling = 0b101;            //Humidity Oversampling for Sensor 1
  bme1.parameter.tempOversampling = 0b101;              //Temperature Oversampling for Sensor 1
  bme1.parameter.pressOversampling = 0b101;             //Pressure Oversampling for Sensor 1


   if (bme1.init() != 0x60)
  {    
    Serial.println(F("Ops! First BME280 Sensor not found!"));
    bme1Detected = 0;
  }

  else
  {
    Serial.println(F("First BME280 Sensor detected!"));
    bme1Detected = 1;
  }
/*
  if (bme2.init() != 0x60)
  {    
    Serial.println(F("Ops! Second BME280 Sensor not found!"));
    bme2Detected = 0;
  }

  else
  {
    Serial.println(F("Second BME280 Sensor detected!"));
    bme2Detected = 1;
  }

 */ 


  
  /**draw home */

//1
/**
  display.clearDisplay();
  display.drawLine(0,homeTopBoxHeight,displayWidth,homeTopBoxHeight,WHITE); //top horizontal line for temp and humidity
  display.drawLine(homeTopBoxWidth,0,homeTopBoxWidth,homeTopBoxHeight,WHITE);//vertical lines
  display.drawLine(homeTopBoxWidth*2,0,homeTopBoxWidth*2,homeTopBoxHeight,WHITE);
  display.setCursor(10,0);
  display.print("AirT");
  display.setCursor(homeTopBoxWidth+5,0);
  display.print("GndT");
  display.setCursor(homeTopBoxWidth*2+10,0);
  display.print("AirH");


  display.display();
*/

// printHeader();
  printSerialTimer.start();
  lcdRefreshTimer.start();
  fanIntervalTimer.start();
  fanDurationTimer.start();
  logToSdIntervalTimer.start();
  fan(true);
}

/*
 * Main function
 */
void loop(void)
{
  if(printSerialTimer.repeat()){
    /*
    *scrive su monitor seriale le temperature
    */ 
   printToSerial();
  };

  checkAirHeater();
  checkGroundHeater();
  checkFan();
  checkHumidity();
  checkAlarm();


if (lcdRefreshTimer.repeat()){

  // String dataString = "aaa";
  // dataFile.println(dataString);
  // Serial.println(dataString);
  // dataFile.flush();


  //display info on display
  //printMainScreen();
 // printValues();
};
}

