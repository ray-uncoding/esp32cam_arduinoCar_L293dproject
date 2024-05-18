/*
檔案名：esp32cam_arduinoCar_onESP32cam
更新時間：2024/05/18
功能簡介：
  目的：建立一個esp32 cam 的 I2C 主機, 並且傳訊息給地址0x39的從機, 預設從機為arduino uno, 傳輸訊息為以下步驟：
      1. esp32 cam 傳輸 " key value "  給 0x39位置的arduino uno
      2. arduino uno 接收指令, 並且逐個byte印製資料, 印製到 \n 位元資料時進行換行, 並且判斷車子四個直流馬達, 兩個伺服馬達的動態

  接線：esp32 cam             arduino
      pin 12 (sda)    ->      pin A4 (sda)
      pin 13 (scl)    ->      pin A5 (scl)
      pin GND         ->      pin GND
      其餘接電自行處理, 記得確保系統工作在5V     

  備註：(1~7是給有需要燒錄或修改程式碼的開發者看的, 一般使用者直接從第8項開始看)
    1. 這個程式碼要上傳到arduino, 並且搭配檔案esp32cam_arduinoCar_onArduino, 將其燒錄到esp32 cam上
    2. 共地線不是必需的, 但有它信號會比較穩
    3. 燒錄esp32 cam的時候別插著其他線, 容易因為分壓導致燒錄失敗, 建議空載情況下燒錄, 並且使用5V pin和對角線上的GND pin為燒錄供電
    4. 這個程式碼最好不要給其他的esp32 系列使用, 有必要請查閱esp32 I2C說明
    5. 經過測試, esp32 cam 只適合做為主機使用, 不適合當作從機, 死了這條心吧, bug劇多
    6. 傳輸訊號時要注意byte的形式為0xFF等十六進位的ascll code形式, 需要自行處理整數跟字元的接收形式
    7. 從機端需要同時擁有 receiveEvent requestEvent 來應對收發狀況, 不然一定是亂碼
    8. wifi 遙控器 ID：ESPcar PASSWRAD：12345678, 聯wifi的時候要關掉你的行動數據, 並且在聯上之後打開瀏覽器, 搜尋192.168.4.1, 擺橫
    9. 前後左右鍵持續按才會有反應, 放開就停車
    10. 拉桿控制伺服馬達角度, 預設只有第一根和第二根會控pin9 pin10的伺服馬達, 拉桿放開後伺服馬達會轉到指定角度
    11. 如果你接上電源後發現無法操作, 試試看拔掉 pin A4 (sda) pin A5 (scl) 再重插
    12. 永遠確保你的電源供應系統分成給馬達和給arduino esp32cam, 不然容易掛掉
    13. 手機擺橫後記得鎖轉向
*/
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <iostream>
#include <sstream>
#include "Arduino.h"
#include "index.h"
#include "WIFI_ID.h"
#include <Wire.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long previousMillis = 0;
const int interval = 50;

#define SERVOMIN 100
#define SERVOMAX 640
#define nintydeg 370

#define UP    1
#define DOWN  2
#define LEFT  3
#define RIGHT 4
#define shoot 5
#define STOP  0


#define left_front_foot 0
#define left_hind_foot 1
#define right_front_foot 2
#define right_hind_foot 3
#define head 4

#define I2C_SDA 12
#define I2C_SCL 13

void setup() {
  Serial.begin(921600);

  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.begin();

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
}
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    ws.cleanupClients();
  }
}


void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    
    std::string myData = "";
    myData.assign((char *)data, len);
    std::istringstream ss(myData);
    std::string key, value;
    std::getline(ss, key, ',');
    std::getline(ss, value, ',');
    Serial.printf("Key [%s] Value[%s]\n", key.c_str(), value.c_str());
    int valueInt = atoi(value.c_str());

    if (key == "Move") {
      move_contrl(valueInt);
    } else if (key == "head") {
      servo_contrl(head, valueInt);
    } else if (key == "left_front_foot") {
      servo_contrl(left_front_foot, valueInt);
    } else if (key == "left_hind_foot") {
      servo_contrl(left_hind_foot, valueInt);
    } else if (key == "right_front_foot") {
      servo_contrl(right_front_foot, valueInt);
    } else if (key == "right_hind_foot") {
      servo_contrl(right_hind_foot, valueInt);
    }

  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void move_contrl(int move_num) {
  Serial.printf("Got value as %d\n", move_num);
  switch (move_num) {
    case UP:
      transmeet('M', '0', UP);
      break;
    case DOWN:
      transmeet('M', '0', DOWN);
      break;
    case LEFT:
      transmeet('M', '0', LEFT);
      break;
    case RIGHT:
      transmeet('M', '0', RIGHT);
      break;
    case shoot:
      transmeet('M', '0', shoot);
      break;
    case STOP:
      transmeet('M', '0', STOP);
      break;
    default:
      break;
  }
}

void servo_contrl(int servo_num, int inputValue) {
  Serial.printf("Got value as %d\n", inputValue);
  switch (servo_num) {
    case left_front_foot:
      transmeet('L', 'F', inputValue);
      break;
    case left_hind_foot:
      transmeet('L', 'H', inputValue);
      break;
    case right_front_foot:
      transmeet('R', 'F', inputValue);
      break;
    case right_hind_foot:
      transmeet('R', 'H', inputValue);
      break;
    case head:
      transmeet('H', '0', inputValue);
      break;
    default:
      break;
  }
}

void transmeet(char cmd_1 , char cmd_2, int inputValue){
  Wire.beginTransmission(0x39); 
  Wire.write(cmd_1);
  Wire.write(cmd_2);
  Wire.write("cmd \n");

  int data_1 = inputValue;
  Wire.write(lowByte(data_1));
  Wire.write(highByte(data_1));
  Wire.endTransmission();       
/*
  Wire.requestFrom(0x39, 4);
  while (Wire.available() > 2){
    char c = Wire.read();    
    Serial.print(c); 
  }
  int data_2 =  (Wire.read() << 8) | Wire.read();
  Serial.print(" data: ");
  Serial.println(data_2);
*/
}