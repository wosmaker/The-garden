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
bool isFirstConect = true;

int8_t MaxHour = 1;
int8_t Mode = 1; // 1 manual 2 auto
bool isFirstPumpON = true;
bool isTimeRun = false;
unsigned long timeStart;

void pinConfig(){
    pinMode(_pump,OUTPUT);
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
        Serial.print("DayofWeek :");
        Serial.println((int)weekday(now()));
    }
}

void loop()
{
    Blynk.run();
    everyMinute();
    state();
}

BLYNK_CONNECTED(){
    if(isFirstConect){
        Blynk.syncAll();
        isFirstConect = false;
    }
}

void pumpControl(int in){
    digitalWrite(_pump,in);
    Blynk.virtualWrite(V1,digitalRead(_pump));
    Blynk.virtualWrite(V2,digitalRead(_pump));
    if(in)
        Serial.println("Start PUMP");
    else
        Serial.println("Stop pump");
}

BLYNK_WRITE(V1){
    int pv = param.asInt();
    pumpControl(pv);
    if (Mode == 2 && isTimeRun){
        Blynk.virtualWrite(V9,1);
    }
}

BLYNK_READ(V2){
    Blynk.virtualWrite(V2,digitalRead(_pump));
}

BLYNK_WRITE(V3){
    int pv = param.asInt();
    digitalWrite(_valve, pv);
    Blynk.virtualWrite(V4,digitalRead(_valve));
}

BLYNK_READ(V4){
    Blynk.virtualWrite(V4,digitalRead(_valve));
}

typedef struct 
{   
    // 1 Sunday 2 Monday ...
    bool weekSelect[8];
    bool hasStartTime;
    bool hasStopTime;
    int  startHour;
    int  startMinute;
    
    int  stopHour;
    int  stopMinute;
}TimeINPUT;

TimeINPUT btime;

BLYNK_WRITE(V5){    
    Serial.println("Setup time");
    TimeInputParam t(param);
    btime.hasStartTime  = t.hasStartTime();
    btime.hasStopTime   = t.hasStopTime();
    btime.startHour     = t.getStartHour();
    btime.startMinute   = t.getStartMinute();
    btime.stopHour      = t.getStopHour();
    btime.stopMinute    = t.getStopMinute();

    for(int i = 1;i < 8;i++){
        btime.weekSelect[i] = t.isWeekdaySelected((i+5)%7+1);
        Serial.printf("i %d : %d\n",i,t.isWeekdaySelected((i+5)%7+1));
    }
    timeCheck();
}

BLYNK_WRITE(V7){
    MaxHour = param.asInt();
    Serial.print("Max hour : ");
    Serial.println(MaxHour);
}

BLYNK_WRITE(V9){
    int pv = param.asInt();
    Mode = pv;
    timeCheck();
}

BLYNK_READ(V10){
    Blynk.virtualWrite(V10,Mode);
}

void timeCheck(){
    Serial.println("Check timer");
    if(btime.weekSelect[weekday(now())] && Mode == 2){
        if(btime.hasStartTime){
            if(hour(now()) >= btime.startHour ){
                if(minute(now()) >= btime.startMinute){
                    Serial.println("time start ");
                    isTimeRun = true;
                    pumpControl(1);
                }
            }
        }

        if(btime.hasStopTime){
            if(hour(now()) >= btime.stopHour){
                if(minute(now()) >= btime.stopMinute){
                    isTimeRun = false;
                    pumpControl(0);
                }
            }
        }
    }
}

int lastMinute = 60;
void everyMinute(){
    if(lastMinute != minute(now())){
        lastMinute = minute(now());
        Serial.print("minute : ");
        Serial.println(minute(now()));
        timeCheck();
    }
}
void state(){
    if (digitalRead(_pump) && isFirstPumpON){
        isFirstPumpON = false;
        timeStart = millis();
    }
    else if (!digitalRead(_pump)){
        isFirstPumpON = true;
        timeStart = millis();
    }

    if (digitalRead(_pump) && millis() - timeStart > 1000*60*60 * MaxHour){
        Blynk.virtualWrite(V1,0);
        pumpControl(0);
        if (Mode == 2){
            isTimeRun = false;
        }
    }
}