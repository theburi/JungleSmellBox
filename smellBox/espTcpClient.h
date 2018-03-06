#include <ELClient.h>
#include <ELClientSocket.h>
//#include <SoftwareSerial.h>


#define ESP_TCP_SERVER  "192.168.1.54" //Table IP address
#define ESP_TCP_PORT    7002
// IP address for ESP in SSID Jumanji K7 WiFi address 192.168.1.56

#define ESP_SOCKET_MODE SOCKET_TCP_CLIENT_LISTEN  //  wait response  //
//#define ESP_SOCKET_MODE SOCKET_TCP_CLIENT       //  not wait response  //

#define ESP_ISDEBUG false

//
//SoftwareSerial esp8266(2,3);
//  create connection to esp - SLIP, debug  //

//ELClient esp(&Serial, &Serial);//debug
ELClient esp(&Serial);//no debug

//  create socket  //
ELClientSocket tcp(&esp);

bool esp_isWifiConnected = false;

extern void espTcpClient_onRecieved(char *data, uint16_t len);

//
//  Errors  //
//

char* const errTxt[] PROGMEM = {"No error, everything OK.","Out of memory.","Unknown code.","Timeout.","Routing problem.","Operation in progress.",
          "Unknown code.","Total number exceeds the maximum limitation.","Connection aborted.","Connection reset.","Connection closed.",
          "Not connected.","Illegal argument.","Unknown code.","UDP send error.","Already connected."};
char * getErrTxt(int16_t commError) {
  commError = commError*-1;
  if (commError <= 15) {
    return (char *) pgm_read_word (&errTxt[commError]);
  } else {
    return (char *) pgm_read_word (&errTxt[2]); // Unknown code
  }
}

//
//  Wifi  //
//

void wifiCb(void *response) {
  ELClientResponse *res = (ELClientResponse*)response;
  if (res->argc() == 1) {
    uint8_t status;
    res->popArg(&status, 1);

    if(status == STATION_GOT_IP) {
      Serial.println("ESP: WIFI: CONNECTED");
      esp_isWifiConnected = true;
    } else {
      Serial.print("ESP: WIFI: NOT READY: ");
      Serial.println(status);
      esp_isWifiConnected = false;
    }
  }
}

//
//  TCP  //
//

void tcpCb(uint8_t resp_type, uint8_t client_num, uint16_t len, char *data) {
  if(ESP_ISDEBUG) Serial.println("ESP: TCP cb: Connection: " + String(client_num));
  if(ESP_ISDEBUG) Serial.println("ESP: Resp: " + String(resp_type));

  //  data sent  //
  if (resp_type == USERCB_SENT) {
    if(ESP_ISDEBUG) Serial.println("ESP: TCP cb: Con " + String(client_num) + ": Sent " + String(len) + " bytes");
  } 
  
  //  data recieved  //
  else if (resp_type == USERCB_RECV) {
    //  copy recieved data  //
    char recvData[len+1];
    memcpy(recvData, data, len);
    recvData[len] = '\0'; //neccessery  

    if(ESP_ISDEBUG) Serial.println("ESP: TCP cb: Con " + String(client_num) + ": Received " + String(len) + " bytes");
    if(ESP_ISDEBUG) Serial.println("ESP: TCP cb: Received: " + String(recvData));

    espTcpClient_onRecieved(recvData, len);
  } 
  
  //  reconnection  //
  else if (resp_type == USERCB_RECO) {
    if (len != -11) { // ignore "not connected" error, handled in USERCB_CONN
      Serial.print("ESP: TCP cb: Connection problem: ");
      Serial.println(getErrTxt(len));
    }

  } 
  
  //  connected / disconnected  //
  else if (resp_type == USERCB_CONN) {
    if (len == 0) {
      if(ESP_ISDEBUG) Serial.println("ESP: TCP cb: Disconnected");
    } else {
      if(ESP_ISDEBUG) Serial.println("ESP: TCP cb: Connected");
    }
  } 
  
  //  unknown  //
  else {
    Serial.println("ESP: TCP cb: Unknown cmd");
  }
}

//
//  Init   //
//

void espTcpClient_init() {

  //esp8266.begin(115200 );
  Serial.begin(115200);

  Serial.println("ESP: Started");

  //  attach wifi cb  //
  esp.wifiCb.attach(wifiCb);

  //  sync started  //
  bool ok;
  do {
    ok = esp.Sync();      // sync up with esp-link, blocks for up to 2 seconds
    if (!ok) Serial.println("ESP: Sync failed");
  } while(!ok);
  Serial.println("ESP: Sync done");

  // Wait for WiFi to be connected. 
  esp.GetWifiStatus();
  ELClientPacket *packet;
  Serial.print("ESP: Waiting for WiFi ");
  if ((packet=esp.WaitReturn()) != NULL) {
    Serial.print(".");
    Serial.println(packet->value);
  }
  Serial.println("");

  //  init socket var  //
  int tcpConnNum = tcp.begin(ESP_TCP_SERVER, ESP_TCP_PORT, ESP_SOCKET_MODE, tcpCb);
  if (tcpConnNum < 0) {
    Serial.println("ESP: TCP socket setup failed, wait for reboot");
    
    //  wait 10 sec  //
    delay(10000);

    //  reset  //
    asm volatile ("  jmp 0");
  } else {
    Serial.println("ESP: TCP: " + String(ESP_TCP_SERVER) + ":" + String(ESP_TCP_PORT) + ": Connection: " + String(tcpConnNum));
  }
  
  Serial.println("ESP: TCP: Ready");
}

void espTcpClient_check() {
  esp.Process();
}

void espTcpClient_send(const char* data) {
  tcp.send(data);
}

