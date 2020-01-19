#include <Time.h>
#include <TimeLib.h>

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp8266.h>

#include "pin.h"
/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial
#define AUTH  "vOH31bfdrDHlaYSyRibT9Bfvrb22aDNE"

WiFiManager wifiManager;
BlynkTimer timer;
WidgetLED ledPump(V2), pumpUseTimer(V11);
WidgetLED ledValve(V4), ValveUseTimer(V12);

bool isFirstConect = true;
bool trick = false;

bool _pumpON = false;
bool _valveON = false;
unsigned long time_pumpON = millis();
unsigned long time_valveON = millis();

typedef struct 
{   
    // 1 Sunday 2 Monday ...
    bool weekSelect[8];
    bool hasStartTime = false;
    bool hasStopTime = false;

    bool isTimerRun = false;
    bool isFirstRun = true;

    int8_t  startHour;
    int8_t  startMinute;
    int8_t  stopHour;
    int8_t  stopMinute;

    int8_t MaxHour = 1;
    int8_t Mode = 2;

    int TimeToUpdate = 3000; // default 3 second
}INPUTVALVE;

INPUTVALVE Vpump;
INPUTVALVE Vvalve;

void pinConfig(){
    pinMode(_pump,OUTPUT);
    pinMode(_valve,OUTPUT);
}

void PumpOutPUT(){
    if (digitalRead(_pump) != _pumpON) time_pumpON = millis();
    else if (millis() - time_pumpON > Vpump.TimeToUpdate){
        digitalWrite(_pump, !_pumpON);
        if (!digitalRead(_pump)) ledPump.on();
        else ledPump.off();

        Blynk.virtualWrite(V1, !digitalRead(_pump));
    }
}

void ValveOUTPUT(){
    if (digitalRead(_valve) != _valveON) time_valveON = millis();
    else if (millis() - time_valveON > Vvalve.TimeToUpdate) {
        digitalWrite(_valve, !_valveON);
        if (!digitalRead(_valve)) ledValve.on();
        else ledValve.off();
       
        Blynk.virtualWrite(V3, !digitalRead(_valve));
    }
}

void setup()
{
    Serial.begin(115200);
    Blynk.config(AUTH);
    pinConfig();

    //Ap config
    IPAddress _ip = IPAddress(1, 1, 1, 1);
    IPAddress _gw = IPAddress(1, 1, 1, 0);
    IPAddress _sn = IPAddress(255, 255, 255, 0);    
    wifiManager.setAPStaticIPConfig(_ip, _gw, _sn);
    // wifiManager.setSTAStaticIPConfig(IPAddress(192,168,1,132), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
    wifiManager.setTimeout(180);  

    if(!wifiManager.autoConnect("ESP8266 theGarden")){
        Serial.println("[wifiManager] Failed to connect and hit timeout");
        delay(3000);
        ESP.reset();
        delay(5000);
    }
    WiFi.printDiag(Serial);
    Serial.println("\nconnencted...OK");
    Serial.println("Local IP: " + WiFi.localIP().toString());

    fetchAndSync();
}

void fetchAndSync(){
    char url[100] = "http://worldtimeapi.org/api/timezone/Asia/Bangkok";
    HTTPClient http;
    http.begin(url);
    if (http.GET() == 200){
        const size_t capacity = JSON_OBJECT_SIZE(15) + 340;
        DynamicJsonDocument root(capacity);
        DeserializationError error;

        int8_t eCount = 0;
        do{
            error = deserializeJson(root, http.getString());
            if(error)
                Serial.println("JSON parsing error");
            else
                Serial.println("JSON parsing Complete");
            eCount++;
        }while(error && eCount < 3);
        setTime((long)root["unixtime"]);
        time_t t = now();
        setTime((hour(t)+7)%24,minute(t),second(t),day(t),month(t),year(t));
        Serial.print("now after Sync : ");
        Serial.println((long)now());
    }
    trick = true;
}

void loop()
{
    Blynk.run();
    runEveryMinute();
    PumpOutPUT();
    ValveOUTPUT();
}

BLYNK_CONNECTED(){
    if(isFirstConect){
        isFirstConect = false;
        // for pump default config
        Blynk.virtualWrite(V9,2);
        Blynk.virtualWrite(V1, !digitalRead(_pump));

        if (!digitalRead(_pump)) ledPump.on();
        else ledPump.off();

        // for valve default config
        Blynk.virtualWrite(V10,2);
        Blynk.virtualWrite(V3, !digitalRead(_valve));

        if (!digitalRead(_valve)) ledValve.on();
        else ledValve.off();

        Blynk.syncAll();
    }
}

// Set time to update for pump control
BLYNK_WRITE(V13){
    Vpump.TimeToUpdate = param.asInt() * 1000;
}

// Set time to update for valve control
BLYNK_WRITE(V14){
    Vvalve.TimeToUpdate = param.asInt() * 1000;
}

// Blynk button for control pump
BLYNK_WRITE(V1){
    trick = true;
    int pv = param.asInt();
    _pumpON = pv == 1 ? true : false;
    Blynk.syncVirtual(V2);
    if (Vpump.Mode == 2 && Vpump.isTimerRun){
        Blynk.virtualWrite(V9,1);
        Blynk.syncVirtual(V9);
    }
}

// Blynk button for contorl valve
BLYNK_WRITE(V3){
    trick = true;
    int pv = param.asInt();
    _valveON = pv == 1 ? true : false;
    Blynk.syncVirtual(V4);
    if (Vvalve.Mode == 2 && Vvalve.isTimerRun){
        Blynk.virtualWrite(V10,1);
        Blynk.syncVirtual(V10);
    }
}

// Pump time setup
BLYNK_WRITE(V5){    
    trick = true;
    Serial.println("Pump time setup");
    TimeInputParam t(param);
    Vpump.hasStartTime  = t.hasStartTime();
    Vpump.hasStopTime   = t.hasStopTime();
    Vpump.startHour     = t.getStartHour();
    Vpump.startMinute   = t.getStartMinute();
    Vpump.stopHour      = t.getStopHour();
    Vpump.stopMinute    = t.getStopMinute();

    for(int i = 1;i < 8;i++){
        Vpump.weekSelect[i] = t.isWeekdaySelected((i+5)%7+1);
    }
}

// Valve time Setup
BLYNK_WRITE(V6){
    trick = true;
    Serial.println("Valve time setup");
    TimeInputParam t(param);
    Vvalve.hasStartTime  = t.hasStartTime();
    Vvalve.hasStopTime   = t.hasStopTime();
    Vvalve.startHour     = t.getStartHour();
    Vvalve.startMinute   = t.getStartMinute();
    Vvalve.stopHour      = t.getStopHour();
    Vvalve.stopMinute    = t.getStopMinute();

    for(int i = 1;i < 8;i++){
        Vvalve.weekSelect[i] = t.isWeekdaySelected((i+5)%7+1);
    }
}

// Pump setup Max Hour
BLYNK_WRITE(V7){
    Vpump.MaxHour = param.asInt();
    Serial.print("Pump Max hour : ");
    Serial.println(Vpump.MaxHour);
}

// Valve setup Max Hour
BLYNK_WRITE(V8){
    Vvalve.MaxHour = param.asInt();
    Serial.print("Valve Max hour : ");
    Serial.println(Vvalve.MaxHour);
}

// Pump setup Mode
BLYNK_WRITE(V9){
    trick = true;
    Vpump.Mode  = param.asInt();
    Serial.print("pump Mode : ");
    Serial.println(Vpump.Mode);
}

// Valve setup Mode
BLYNK_WRITE(V10){
    trick = true;
    Vvalve.Mode = param.asInt();
    Serial.println("valve Mode : ");
    Serial.println(Vvalve.Mode);
}

unsigned long pumpStartAt;
void pumpRun(){
    Serial.println("Pump Run");
    if (Vpump.Mode == 2 ){
        if (Vpump.weekSelect[weekday(now())] && Vpump.hasStartTime && Vpump.hasStopTime){
            Serial.println("Time check");
            time_t t = now();
            int HMtime = hour(t) * 100 + minute(t);
            int StartTime = Vpump.startHour * 100 + Vpump.startMinute;
            int StopTime = Vpump.stopHour * 100 + Vpump.stopMinute;
    
            Serial.printf("Start Time :: %d || Stop Time :: %d >> Now Time :: %d\n", StartTime, StopTime, HMtime);
            if ( HMtime >= StartTime && HMtime <= StopTime && StartTime < StopTime){
                if(!_pumpON) Serial.println("Pump Start with Timer");
                Vpump.isTimerRun = true;
                _pumpON = true;
            }
            else if(Vpump.isTimerRun) {
                if(_pumpON) Serial.println("Pump Stop By Timer");
                Vpump.isTimerRun = false;
                _pumpON = false;
            }
        }
        else if(Vpump.isTimerRun) {
            if(_pumpON) Serial.println("Pump Stop By DAY Change");
            Vpump.isTimerRun = false;
            _pumpON = false;
        }
    }

    // state
    if (Vpump.Mode == 1){
        if (Vpump.isTimerRun) 
            _pumpON = false;
        Vpump.isTimerRun = false;
    }
    
    if (!_pumpON){
        Vpump.isFirstRun = true;
    }

    if ((Vpump.isFirstRun && _pumpON) || Vpump.isTimerRun){
        Vpump.isFirstRun = false;
        pumpStartAt = millis();
    }

    if (!Vpump.isTimerRun && _pumpON && millis() - pumpStartAt > Vpump.MaxHour *60*60*1000){
        _pumpON = false;
    }

    // use timer led
    if (_pumpON && Vpump.isTimerRun) pumpUseTimer.on();
    else pumpUseTimer.off();
}

unsigned long valveStartAt;
void valveRun(){
    Serial.println("Valve Run");
    if (Vvalve.Mode == 2 ){
        if (Vvalve.weekSelect[weekday(now())] && Vvalve.hasStartTime && Vvalve.hasStopTime){
            Serial.println("Time check");
            time_t t = now();
            int HMtime = hour(t) * 100 + minute(t);
            int StartTime = Vvalve.startHour * 100 + Vvalve.startMinute;
            int StopTime = Vvalve.stopHour * 100 + Vvalve.stopMinute;
    
            Serial.printf("Start Time :: %d || Stop Time :: %d >> Now Time :: %d\n", StartTime, StopTime, HMtime);
            if ( HMtime >= StartTime && HMtime <= StopTime && StartTime < StopTime){
                if(!_valveON) Serial.println("Pump Start with Timer");
                Vvalve.isTimerRun = true;
                _valveON = true;
            }
            else if(Vvalve.isTimerRun) {
                if(_valveON) Serial.println("Pump Stop By Timer");
                Vvalve.isTimerRun = false;
                _valveON = false;
            }
        }
        else if(Vvalve.isTimerRun) {
            if(_valveON) Serial.println("Pump Stop By DAY Change");
            Vvalve.isTimerRun = false;
            _valveON = false;
        }
    }

    // state
    if (Vvalve.Mode == 1){
        if (Vvalve.isTimerRun) 
            _valveON = false;
        Vvalve.isTimerRun = false;
    }
    
    if (!_valveON){
        Vvalve.isFirstRun = true;
    }

    if ((Vvalve.isFirstRun && _valveON) || Vvalve.isTimerRun){
        Vvalve.isFirstRun = false;
        valveStartAt = millis();
    }

    if (!Vvalve.isTimerRun && _valveON && millis() - valveStartAt > Vvalve.MaxHour *60*60*1000){
        _valveON = false;
    }

    // use timer led
    if (_valveON && Vvalve.isTimerRun) ValveUseTimer.on();
    else ValveUseTimer.off();
}

int8_t lastMinute = 60;
void runEveryMinute(){
    if(lastMinute != minute(now()) || trick){
        trick = false;
        pumpRun();
        valveRun();

        lastMinute = minute(now());
        Serial.print("minute : ");
        Serial.println(minute(now()));
        Serial.print("pumpStatus :: ");
        Serial.println(_pumpON);
        Serial.print("valveStatus :: ");
        Serial.println(_valveON);
    }
}

