/*
  TLC 2543 AnalogReadSerial
  Reads an analog input on pin AIN0 to AIN10 , prints the result to the serial monitor.
  Attach the center pin of a potentiometer to pin A0, and the outside pins to +5V and ground.
  This example code is in the public domain.
 */

/*
   Pin configuration
 
   Arduino      TLC2543
 
   13-SCK       18-I/O CLOCK 
   12-MISO      16-DATA OUT
   11-MOSI      17-DATA INPUT
   10-CS        15-CS
 
   VCC          14-REF+
   GND          13-REF-
   VCC          20-VCC
   GND          10-GND
                19-EOC  NC
 */
 
#include <SPI.h>
const int chipSelectPin = 10; //Select CS pin(今回は10に設定)
uint16_t values[11] = { 0 }; //uint16_t:2バイトの符号なし整数 計算容量節約
float Vref = 5.0; //参照電圧は5V
float floatvalues[11] = { 0 }; //今回はArduino側で電圧までの変換を行う
SPISettings MySetting (1000000, MSBFIRST, SPI_MODE0); //iso-SPIの最大クロック周波数が1MHz

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); //大きいほど高速通信 ※注シリアルモニタの表示も一緒に変えるように
  SPI.begin();
  pinMode(chipSelectPin, OUTPUT);
}

void loop() {
  readAdcAll(); 
  for(uint8_t channel = 0; channel < 11; channel++)
  {    
    floatvalues[channel] = values[channel]*Vref/4095; //12bit表記を実数に変換
    Serial.print(String(floatvalues[channel],2) + ","); //string(values,n)で小数点以下どこまで書くか決められる
  }  
  Serial.print("\n");
}

void readAdcAll() {
  uint16_t value = 0;

  readAdc(0); //readAdc(1)をして得る値がAIN0の物になるように調整
  for(uint8_t channel = 1; channel < 12; channel++)
  {
    values[channel - 1] = readAdc(channel); //SPI通信の都合上valuesに入る値とreadAdcで読む値が一つズレる
  }
}

uint16_t readAdc(uint8_t chx) {
  uint16_t ad;
  uint8_t ad_l;  

  SPI.beginTransaction(MySetting);
  digitalWrite(chipSelectPin, LOW);
  delayMicroseconds(10);
  ad = SPI.transfer((chx << 4) | 0x0E); // チャネル番号に対応した2進数コードを0を4つ分左にシフト，そして空いてる4つの0に0x0E = 16-Bit, LSB-First, Unipolarを入れる
  ad_l = SPI.transfer(0); //
  digitalWrite(chipSelectPin, HIGH);
  SPI.endTransaction();
  ad <<= 8; //adには欲しい12bitのデータのうち前から8bitが入る
  ad |= ad_l; //ad_lにはadの後の8bitが入ってくるので，それを右からくっつけて16bitとする

  ad = ad & 0x0fff; //LSB-Firstでデータを受け取っているのでMSB-Firstになるようデータの方向を反転させる
  ad = (((ad & 0xaaaa) >> 1) | ((ad & 0x5555) << 1));
  ad = (((ad & 0xcccc) >> 2) | ((ad & 0x3333) << 2));
  ad = (((ad & 0xf0f0) >> 4) | ((ad & 0x0f0f) << 4));
  ad = ((ad >> 8) | (ad << 8));
  ad >>= 4; //欲しいのは12bitの情報なので，後ろの4bit分は不要．よって削る

  delayMicroseconds(10);  // EOC
  return ad;
}