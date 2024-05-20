/*
檔案名：esp32cam_arduinoCar_onArduino
更新時間：2024/05/18
功能簡介：
  目的：建立一個esp32 cam 的 I2C 主機, 並且傳訊息給地址0x39的從機, 預設從機為arduino uno, 傳輸訊息為以下步驟：
      1. esp32 cam 傳輸 " key value "  給 0x39位置的arduino uno
      2. arduino uno 接收指令, 並且逐個byte印製資料, 印製到 \n 位元資料時進行換行, 並且判斷車子四個直流馬達, 兩個伺服馬達的動態

  接線：esp32 cam             arduino
      pin 12 (sda)    ->      pin A4 (sda)
      pin 13 (scl)    ->      pin A5 (scl)
      pin GND         ->      pin GND
      
      L296D
      M1 M2 -> Right
      M3 M4 -> Left

      pin9  -> Servo 9
      pin10 -> Servo 10
      
      其餘接電自行處理, 記得確保系統工作在5V     

  備註：(1~7是給有需要燒錄或修改程式碼的開發者看的, 一般使用者直接從第8項開始看)
    1. 這個程式碼要上傳到arduino, 並且搭配檔案esp32cam_arduinoCar_onESP32cam, 將其燒錄到esp32 cam上
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
#include <Wire.h>
#include <Arduino.h>
#include <AFMotor.h>
#include <Servo.h> 

AF_DCMotor motor_M1(1, MOTOR12_8KHZ); //接腳座及頻率
AF_DCMotor motor_M2(2, MOTOR12_8KHZ); //接腳座及頻率
AF_DCMotor motor_M3(3, MOTOR12_8KHZ); //接腳座及頻率
AF_DCMotor motor_M4(4, MOTOR12_8KHZ); //接腳座及頻率

Servo servo_P9;
Servo servo_P10; 

#define boudrate 9600 

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

int servo_num = 0;
int move_num = 0;
int inputValue = 125 ;

void setup() {
  Wire.begin(0x39);              
  Wire.onReceive(receiveEvent); //從機收訊動作
  Wire.onRequest(requestEvent); //從機發訊動作
  Serial.begin(9600);      
  motor_M1.setSpeed(255);     //可調轉速約150~到255
  motor_M2.setSpeed(255);     //可調轉速約150~到255
  motor_M3.setSpeed(255);     //可調轉速約150~到255
  motor_M4.setSpeed(255);     //可調轉速約150~到255
  servo_P9.attach(9);         // 設定要將伺服馬達接到哪一個PIN腳
  servo_P10.attach(10);       // 設定要將伺服馬達接到哪一個PIN腳
  pinMode(13, OUTPUT);
}

void loop() {
  switch (move_num) {
    case UP:
      Serial.print("move_num Got value as ");
      Serial.println(UP);
      motor_M1.run(FORWARD);
      motor_M2.run(FORWARD);
      motor_M3.run(FORWARD);
      motor_M4.run(FORWARD);
      break;
    case DOWN:
      Serial.print("move_num Got value as ");
      Serial.println(DOWN);
      motor_M1.run(BACKWARD);
      motor_M2.run(BACKWARD);
      motor_M3.run(BACKWARD);
      motor_M4.run(BACKWARD);
      break;
    case LEFT:
      Serial.print("move_num Got value as ");
      Serial.println(LEFT);
      motor_M1.run(FORWARD);
      motor_M2.run(FORWARD);
      motor_M3.run(BACKWARD);
      motor_M4.run(BACKWARD);
      break;
    case RIGHT:
      Serial.print("move_num Got value as ");
      Serial.println(RIGHT);
      motor_M1.run(BACKWARD);
      motor_M2.run(BACKWARD);
      motor_M3.run(FORWARD);
      motor_M4.run(FORWARD);
      break;
    case shoot:
      Serial.print("move_num Got value as ");
      Serial.println(shoot);
      digitalWrite(13, 1);
      break;
    case STOP:
      motor_M1.run(RELEASE);
      motor_M2.run(RELEASE);
      motor_M3.run(RELEASE);
      motor_M4.run(RELEASE);
      digitalWrite(13, 0);
      break;
    default:
      break;
  }
  switch (servo_num) {
    case left_front_foot:
      Serial.print("left_front_foot Got value as ");
      Serial.println(inputValue);
      servo_P10.write(inputValue);
      servo_num = 99;
      inputValue = 125 ;
      break;
    case left_hind_foot:
      Serial.print("left_hind_foot Got value as ");
      Serial.println(inputValue);
      servo_num = 99;
      inputValue = 125 ;
      break;
    case right_front_foot:
      Serial.print("right_front_foot Got value as ");
      Serial.println(inputValue);
      servo_num = 99;
      inputValue = 125 ;
      break;
    case right_hind_foot:
      Serial.print("right_hind_foot Got value as ");
      Serial.println(inputValue);
      servo_num = 99;
      inputValue = 125 ;
      break;
    case head:
      Serial.print("head Got value as");
      Serial.println(inputValue);
      servo_P9.write(inputValue);
      servo_num = 99;
      inputValue = 125 ;
      break;
    default:
      break;
  }
  delay(200);
}

void receiveEvent() {
  char cmd_1 = Wire.read();
  char cmd_2 = Wire.read();
  Serial.print(cmd_1);
  Serial.print(cmd_2);
  while (Wire.available()>2) {
    char c = Wire.read();
    Serial.print(c);
  }
  int data = Wire.read() | (Wire.read() << 8);
  Serial.print(" data: ");
  Serial.println(data);

  if(cmd_1 == 'L'){
    if(cmd_2 == 'F') chang(servo_num, left_front_foot, inputValue, data);
    else if(cmd_2 == 'H') chang(servo_num, left_hind_foot, inputValue, data);
  }
  else if(cmd_1 == 'R'){
    if(cmd_2 == 'F') chang(servo_num, right_front_foot, inputValue, data);
    else if(cmd_2 == 'H') chang(servo_num, right_hind_foot, inputValue, data);
  }
  else if(cmd_1 == 'H'){
    chang(servo_num, head, inputValue, data);
  }
  
  if(cmd_1 == 'M'){
    chang_once(move_num, data);
  }
}

void requestEvent() {
  int data = 216; //test data
  Wire.write("hi");
  Wire.write(highByte(data));  
  Wire.write(lowByte(data)); 
}

void chang(int &x, int value_1, int &y, int value_2){ //會單獨寫出來是因為I2C打斷機制的問題
  x = value_1;
  y = value_2;
}

void chang_once(int &x, int value_1){
  x = value_1;
}