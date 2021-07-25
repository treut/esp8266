#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <IRremoteESP8266.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

DynamicJsonDocument doc(2048);

long lastm=0;
String lastmq;

char ssid[32];
char wpass[32];
char padmin[8];
String url;

char mqttserver[100];
char mqttport[6];
char mqttuser[50];
char mqttpass[50];
char mqttclient[20];

char namedev[20];
int wifi_attempt=0;

int codes=0;

ESP8266WebServer server(80);

IRrecv irrecv(5);
decode_results results;


IPAddress ip(0,0,0,0);
IPAddress gateway(0,0,0,0);
IPAddress subnet(0,0,0,0);

WiFiClient wclient;
PubSubClient client(wclient);

int pins[] = {16, 14, 12, 13, 5, 4, 0};
int topics[] = {0, 0, 0, 0, 0, 0, 0};
int stat[] = {0, 0, 0, 0, 0, 0, 0};
int func[] = {0, 0, 0, 0, 0, 0, 0};

String header;
String nav="<div class='nav'><a href='/system'<div class='navLink'>System</div></a>\
<a href='/network'><div class='navLink'>Network</div></a>\
<a href='/mqtt'<div class='navLink'>MQTT</div></a>\
<a href='/pins'<div class='navLink'>Pins</div></a>\
</div>";
String footer="</div><div class='footer'>www.treut.ru, 2019</div></body></html>";

void mc38(int st, int pub){
    String topic="sh/status/"+String(pub);
    if(st==1){
      client.publish(topic, "0"); //closed door
    }
    if(st==0){
      client.publish(topic, "1"); //opened door
    }
  }

void lights(int st, int ch){
    if(st==1){
    client.publish("sh/status/"+String(ch), "1");
    }
    if(st==0){
    client.publish("sh/status/"+String(ch), "0");
    }
  }

void lightSwitch(int pin, String topic, String payl){ //для реле
    int currState=digitalRead(pin);
    if(payl=="get"){
      lights(currState, topic.toInt());
      }
    else if(payl=="set"){
      if(currState) digitalWrite(pin, 0);
      else digitalWrite(pin, 1);
      //lights(currState, topic.toInt());
      }
    else{
      if(payl=="1" && currState!=1) digitalWrite(pin, 1);
      if(payl=="0" && currState!=0) digitalWrite(pin, 0);
      lights(currState, topic.toInt());
      }
  }

void ir(int topic){
  if(irrecv.decode(&results)){
      codes=results.value;
      //if(codes>0){
        irrecv.resume();
        client.publish("sh/status/"+String(topic), String(codes));
       //}
      //irrecv.resume();
  }
}

void handcall(String payload){
  DeserializationError error = deserializeJson(doc, payload);
  for(int ind=0; ind<doc.size(); ind++){ // 2. запустил цикл из payload
    for(int indx=0; indx<7; indx++){ //sizeof(pins)
      if(doc[String(ind)][0].as<String>().toInt() == topics[indx]){
        switch(func[indx]){
          case 3:
            lightSwitch(pins[indx], doc[String(ind)][0].as<String>(), doc[String(ind)][1].as<String>());
            //lights(digitalRead(pins[indx]), topics[indx]);
          break;
          case 1:
            mc38(digitalRead(pins[indx]), topics[indx]);
          break;
        }
      }
    }
  }
}

void callback(const MQTT::Publish& pub){
  long now=millis();
  String payload=pub.payload_string();
  if(lastmq!=payload){
      handcall(payload);
      lastmq=payload;
      delay(100);
    }
  /*else if(lastmq==payload and now-lastm>=1000){
    lastm=now;
    handcall(payload);
    lastmq=payload;
    delay(100);
  }*/
  //lastmq=payload;
}

void setup(void){
  Serial.begin(115200);
  ArduinoOTA.begin();
  readset();
  client.set_server(String(mqttserver), (String(mqttport[0])+String(mqttport[1])+String(mqttport[2])+String(mqttport[3])+String(mqttport[4])).toInt());
  //clearNetwork(); //for reset network settings

  for(int ind=0; ind<7; ind++){
    switch(func[ind]){
      case 1:
        pinMode(pins[ind], INPUT_PULLUP);
      break;
      case 3:
        pinMode(pins[ind], OUTPUT);
        digitalWrite(pins[ind], 1);
      break;
      case 4:
        irrecv.enableIRIn(pins[ind]);
      break;
      }
  }
    

  header="<html>\
  <head>\
        <meta http-equiv='Content-Type' content='text/html; charset=utf-8'>\
        <title>controller auth</title>\
        <style>\
        body{ background:#f2f5fa;color:#585858;font-size:18px;font-family:'andika';margin:0px }\
        .nav{ width:100%;height: 40px;margin-bottom: 20px;background: #f2f5fa; }\
        .navLink{ max-width:24%;display:inline-block;vertical-align:top;height: 100%;line-height: 40px;padding: 0px 10px;box-sizing: border-box;text-align: center; }\
        .content,.footer,.header{ margin:0px auto;box-sizing:border-box;max-width:1000px; }\
        .header{ background:#0d7fa3;height:65px;line-height:65px;text-align:center;color:#fff;font-size:30px; }\
        .content{ background:#fff;padding:20px; }\
        .footer{ height:50px;line-height:50px;color:#fff;background:#424242;text-align:center; }\
        .group{ border:1px solid #f2f5fa;background:#f2f5fa;border-radius:7px;margin-bottom:10px;padding:10px;box-sizing:border-box; }\
        .line{ margin-bottom:5px; }\
        input[type='text'], input[type='password']{ border:1px solid #ccc;border-radius:5px;padding:3px; }\
        input[type='submit']{ padding:8px;border-radius:5px;border:1px solid #01579B;background:#01579B;color:#fff; }\
        .ip>input[type='text'], .ip>input[type='password'], .ip>.th{ text-align:center;width:50px;display:inline-block; }\
        </style>\
        <meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>\
 </head>\
 <body>\
 <div class='header'>HAS / "+String(namedev)+"</div>\
 <div class='content'>"+nav;

 server.on("/", [](){
  readset();
  if(!server.authenticate("admin", padmin))
    return server.requestAuthentication();
  else{
    readset();
    auth();
    }
  });
 server.on("/save", save);
 server.on("/reboot", reboot);
 server.on("/clear", clearNetwork);
 server.begin();

 if((ip[0]+ip[1]+ip[2]+ip[3])>0){
  if(WiFi.status()!=WL_CONNECTED){
    WiFi.config(ip, gateway, subnet);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, wpass);
    if(WiFi.waitForConnectResult()!=WL_CONNECTED){
      if(wifi_attempt<2){
        wifi_attempt++;
        return;
      }
      else{
        resets();
        //readset();
        startAP();
        }
      }
    }
  }
  else{
    resets();
    //readset();
    startAP();
    }
}

void loop(void){
  server.handleClient();
  //int codes=0;
  if(WiFi.status()==WL_CONNECTED){
    if(!client.connected()){
      if(client.connect(MQTT::Connect(mqttclient)
                        .set_auth(mqttuser, mqttpass))){
                            client.set_callback(callback);
                            client.subscribe("sh/#");
                            client.publish("hello", mqttclient);
                          }
                          else{
                            Serial.println("BAD CONNECT");
                            }
    }
      if(client.connected()){
        client.loop();
      }
  }

  for(int ind=0; ind<7; ind++){
    if(stat[ind]!=digitalRead(pins[ind])){
      stat[ind]=digitalRead(pins[ind]);
      switch(func[ind]){
        case 1:
          mc38(digitalRead(pins[ind]), topics[ind]);
        break;
        case 2:
        lightSwitch(pins[ind], String(topics[ind]), String(digitalRead(pins[ind])));
        //(int pin, String topic, String payl)
        break;
        case 3:
          lights(digitalRead(pins[ind]), topics[ind]);
        break;
        case 4:
          ir(topics[ind]);
        break;
        }
      }
    }
  }

void auth(){
  String page=header+"\
  <form method='POST' action='save'>\
  <div class='group'>\
  <div class='line'>Login admin: admin</div>\
  <div class='line'>Pass admin:<br><input name='pass' type='password' placeholder='123456' value='"+String(padmin)+"'></div>\
  </div>\
  <div class='group'>\
  <div class='line'>Name Your WiFi:<br><input name='ssid' type='text' placeholder='ssid' value='"+String(ssid)+"'></div>\
  <div class='line'>Pass Your WiFi:<br><input name='wpass' type='password' placeholder='password' value='"+String(wpass)+"'></div>\
  </div>\
  <div class='group'>\
  <div class='line ip'>IP:<br><input name='ip0' type='text' placeholder='192' value='"+String(ip[0])+"'>.<input name='ip1' type='text' placeholder='168' value='"+String(ip[1])+"'>.<input name='ip2' type='text' placeholder='9' value='"+String(ip[2])+"'>.<input name='ip3' type='text' placeholder='190' value='"+String(ip[3])+"'></div>\
  <div class='line ip'>Gateway:<br><input name='gateway0' type='text' placeholder='192' value='"+String(gateway[0])+"'>.<input name='gateway1' type='text' placeholder='168' value='"+String(gateway[1])+"'>.<input name='gateway2' type='text' placeholder='9' value='"+String(gateway[2])+"'>.<input name='gateway3' type='text' placeholder='1' value='"+String(gateway[3])+"'></div>\
  <div class='line ip'>Subnet:<br><input name='subnet0' type='text' placeholder='255' value='"+String(subnet[0])+"'>.<input name='subnet1' type='text' placeholder='255' value='"+String(subnet[1])+"'>.<input name='subnet2' type='text' placeholder='255' value='"+String(subnet[2])+"'>.<input name='subnet3' type='text' placeholder='0' value='"+String(subnet[3])+"'></div>\
  </div>\
  <div class='group'>\
  <div class='line'>MQTT Server:<br><input name='mqttserver' type='text' placeholder='mqtt.com' value='"+String(mqttserver)+"'></div>\
  <div class='line'>MQTT Port:<br><input name='mqttport'  type='text' placeholder='1883' value='"+String(mqttport)+"'></div>\
  <div class='line'>MQTT User:<br><input name='mqttuser'  type='text' placeholder='admin' value='"+String(mqttuser)+"'></div>\
  <div class='line'>MQTT Pass:<br><input name='mqttpass'  type='password' placeholder='test11' value='"+String(mqttpass)+"'></div>\
   <div class='line'>MQTT Client name:<br><input name='mqttclient'  type='text' placeholder='espHall' value='"+String(mqttclient)+"'></div>\
  </div>\
  <div class='group'>\
  <div class='line'>Name device:<br><input name='namedev' type='text' placeholder='bath' value='"+String(namedev)+"'></div>\
  </div>\
  <div class='group'>\
  <div class='line'>Pins manage:</div>\
  <div class='line ip'><div class='th'>pin</div> <div class='th'>topic</div> <div class='th'>func</div> <div class='th'>status</div></div>";
  for(int ind=0; ind<7; ind++){
    page+="<div class='line ip'><input disabled name='p"+String(pins[ind])+"' type='text' placeholder='16' value='"+String(pins[ind])+"'> <input name='p"+String(pins[ind])+"t' type='text' placeholder='168' value='"+String(topics[ind])+"'> <input name='p"+String(pins[ind])+"f' type='text' placeholder='9' value='"+String(func[ind])+"'> <input disabled name='p"+String(pins[ind])+"s' type='text' placeholder='190' value='"+String(stat[ind])+"'></div>";
  }
  page+="</div>\
  <input type='submit' value='Save'>\
  </form>\
  "+footer;
  
  server.send(200, "text/html", page);
  }

  void reboot(){
    server.sendHeader("Location", url, true);
    server.send(302, "text/plain", "");
    delay(10);
    ESP.reset();
    }
 
  void readset(){
    EEPROM.begin(512);
    EEPROM.get(25, ssid);
    EEPROM.get(57, wpass);
    EEPROM.get(5, padmin);
    EEPROM.get(89, mqttserver);
    EEPROM.get(194, mqttuser);
    EEPROM.get(243, mqttpass);
    EEPROM.get(294, namedev);
    EEPROM.get(342, mqttclient);

    for(int i=0; i<4; i++){
      ip[i]=char(EEPROM.read(i+13));
      gateway[i]=char(EEPROM.read(i+17));
      subnet[i]=char(EEPROM.read(i+21));
    }
    for(int ind=0; ind<7; ind++){
        topics[ind]=EEPROM.read(ind*4+315);
        func[ind]=EEPROM.read(ind*4+316);
      }
    for(int ii=0; ii<=4; ii++){
      mqttport[ii]=char(EEPROM.read(ii+189));
    }
    EEPROM.end();
    }

   void save(){
    server.arg("pass").toCharArray(padmin, 8);
    server.arg("ssid").toCharArray(ssid, 32);
    server.arg("wpass").toCharArray(wpass, 32);
    server.arg("mqttserver").toCharArray(mqttserver, 100);
    server.arg("mqttport").toCharArray(mqttport, 6);
    server.arg("mqttuser").toCharArray(mqttuser, 50);
    server.arg("mqttpass").toCharArray(mqttpass, 50);
    server.arg("namedev").toCharArray(namedev, 20);
    server.arg("mqttclient").toCharArray(mqttclient, 20);
  
    EEPROM.begin(512);
    EEPROM.put(5, padmin);
    EEPROM.put(25, ssid);
    EEPROM.put(57, wpass);
    for(int i=0; i<4; i++){
      EEPROM.write(i+13, server.arg("ip"+String(i)).toInt());
      EEPROM.write(i+17, server.arg("gateway"+String(i)).toInt());
      EEPROM.write(i+21, server.arg("subnet"+String(i)).toInt());
    }
    for(int ind=0; ind<7; ind++){
        EEPROM.write(ind*4+315, server.arg("p"+String(pins[ind])+"t").toInt());
        EEPROM.write(ind*4+316, server.arg("p"+String(pins[ind])+"f").toInt());
      }
    for(int ii=0; ii<=4; ii++){
      EEPROM.write(ii+189, mqttport[ii]);
    }
    EEPROM.put(89, mqttserver);
    EEPROM.put(194, mqttuser);
    EEPROM.put(243, mqttpass);
    EEPROM.put(294, namedev);
    EEPROM.put(342, mqttclient);
    url="http://"+String(server.arg(3))+"."+String(server.arg(4))+"."+String(server.arg(5))+"."+String(server.arg(6)); //+"/auth";
    EEPROM.commit();
    EEPROM.end();
  
    String page=header+"\
    <div class='group'>\
    <div class='line'>Settings saved succesfully!</div>\
    <a href ='/'>Return</a><br><br>\
    <a href='/reboot'><input type='submit' value='Reboot'></a>\
    </div>\
    "+footer;
    server.send(200, "text/html", page);
    }

    void startAP(){
      IPAddress ips(192,168,1,1);
      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(ips, ips, IPAddress(255,255,255,0));
      WiFi.softAP("HAS_treut", "00000000", 15, 0);
     }

    void clearNetwork(){
      EEPROM.begin(30);
      for(int i=13; i<25; i++){
        EEPROM.write(i,0);
      }
      EEPROM.commit();
      EEPROM.end();
      delay(3000);
      server.sendHeader("Location", String("http://192.168.1.1"), true);
      server.send(302, "text/plain", "");
      delay(2000);
      ESP.reset();
    }
    void resets(){
      char respsw[8];
      String psw="000000";
      psw.toCharArray(respsw, 8);
      EEPROM.begin(13);
      EEPROM.put(5, respsw);
      EEPROM.commit();
      EEPROM.end();
      }
