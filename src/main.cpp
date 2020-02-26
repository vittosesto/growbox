// Include the libraries we need
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <BlueDot_BME280.h>
//#include <elapsedMillis.h>
#include <neotimer.h>
#include <Adafruit_SSD1306.h>
// #include <splash.h>
#include <RTClib.h>

#define AIR_HEATER_PIN 5
#define GROUND_HEATER_PIN 6
#define FAN_PIN 7
#define LIGHT_PIN 8
#define ONE_WIRE_BUS 12 //for ground sensors
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

const int displayWidth = 128;
const int displayHeight = 32;

const int homeTopBoxWidth=displayWidth/3; //oled top menu sizes
const int homeTopBoxHeight=16;
bool groundTempDisplaying=true; //to alternate the temperature displaying on the home screen
Adafruit_SSD1306 display(displayWidth, displayHeight, &Wire, -1);

RTC_DS3231 rtc;
DateTime now;

const byte VER=1;
const byte BUFFERSIZE=100;
char strBuffer[BUFFERSIZE] = "";

float minAirTemp = 24.0; //20.0
float maxAirTemp = 26.0;  //28.0

float minAirHum = 50.0;
float maxAirHum = 80.0;

float minGroundT = 25.0; //30.0
float maxGroundT = 28.0; //35.0

float airHumMin = 55.00;
float airHumMax = 75.00;


int fanDuration = 10000;
int fanInterval = 30000;


float airTemp1 = 0.0;
float groundTemp1 = 0.0;
float airHum1 = 0.0;

bool airHeaterON = false;
bool groundHeaterON = false;
bool fanON = false;
bool humFanOn = false;


/* timers */
Neotimer printSerialTimer = Neotimer(3000);
Neotimer lcdRefreshTimer = Neotimer(1000);
Neotimer fanDurationTimer = Neotimer(fanDuration);
Neotimer fanIntervalTimer = Neotimer(fanInterval);

// Neotimer airTemp1Delay = Neotimer(1000);
// Neotimer airHum1Delay = Neotimer(1000);


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
      Serial.println("Air heater on!");
    }
  }else if (airHeaterON)
  {
    airHeaterON=false;
    digitalWrite(AIR_HEATER_PIN, RELAYS_OFF);
    Serial.println("Air heater off!");
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
      Serial.println("ground heater on!");
    }
  }else if (groundHeaterON)
  {
    groundHeaterON = false;
    digitalWrite(GROUND_HEATER_PIN, RELAYS_OFF);
    Serial.println("ground heater off!");
  }  
}

/* if status true, turns on fan, false turns off fan */
void fan(bool status){
  //TODO
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
    if (getGroundTempMax() > maxGroundT){
      groundHeater(false);
    }
  }else if (getGroundTempMin() < minGroundT){
      groundHeater(true);
  }
}

void checkFan(){

  if (getHAria1() > airHumMax){
    humFanOn = true;
    fan(true);
    Serial.println("Humidity too High");
  }else if(humFanOn && getHAria1() < airHumMin){  //if fan for humidity is runnig, check if humidity is ok and turn fan off
    humFanOn = false;
    fan(false);
    Serial.println("Humidity OK");
  }
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

void printToSerial(){
    Serial.println("| TAria1 | TSuolo1 | TSuolo2 | HAria1 |");
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
    now=rtc.now();
    sprintf(strBuffer,"log:%04d/%02d/%02d %02d:%02d:%02d fan:%d heat:%d grou:%d - ", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), fanON, airHeaterON, groundHeaterON );

    dtostrf(getAirTemp1(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,";");
    dtostrf(getHAria1(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,";");
    dtostrf(getTempSuolo1(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,";");
    dtostrf(getTempSuolo2(), 4, 2, &strBuffer[strlen(strBuffer)]);
    strcat(strBuffer,";");

    Serial.println(strBuffer);
}

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

  /* relais status*/
  display.setCursor(0,displayHeight-8);
  sprintf(strBuffer,"fan:%d heat:%d grou:%d", fanON, airHeaterON, groundHeaterON);
  display.print(strBuffer);
  display.display();
}


/*
 * The setup function. We only start the sensors here
 */
void setup(void)
{
  // start serial port
  Serial.begin(9600);
  //initialize oled display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
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
  pinMode(LIGHT_PIN, OUTPUT);

  digitalWrite(AIR_HEATER_PIN,RELAYS_OFF);
  digitalWrite(GROUND_HEATER_PIN,RELAYS_OFF);
  digitalWrite(FAN_PIN,RELAYS_OFF);
  digitalWrite(LIGHT_PIN,RELAYS_OFF);


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

  display.clearDisplay();
  sprintf(strBuffer,"   Growbox V.%d          Vittorio Sestito",VER);
  
  display.clearDisplay();  //Pulisce il buffer da inviare al display
  display.setTextSize(1);  //Imposta la grandezza del testo
  display.setTextColor(WHITE); //Imposta il colore del testo (Solo bianco)
  display.setCursor(0,0); //Imposta la posizione del cursore (Larghezza,Altezza)
  display.println(strBuffer); //Stringa da visualizzare
  display.display(); //Invia il buffer da visualizzare al display

  
  /**draw home */


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

  printSerialTimer.start();
  lcdRefreshTimer.start();
  fanIntervalTimer.start();
  fanDurationTimer.start();
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

if (lcdRefreshTimer.repeat()){
  //display info aon display
  printMainScreen();
};
}

