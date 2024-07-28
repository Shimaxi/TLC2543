# はじめに

本資料は、ガスセンサ「Mics5524 (Adafruit)」11個から得たアナログ信号をArduino Unoで受け取るために「TLC2543 (Texas Instruments)」というAD変換器を使用した際の作業記録兼引継ぎ資料です。また、後半では「LTC6820 (Analog Devices)」というSPI通信延長モジュールを導入した際の作業についても記載しています。

# TLC2543を使用する

今回使用するAD変換器は「TLC2543」です。このAD変換器は、12bitの分解能、11入力対応、ブレッドボード対応という特長があり、これらが選定理由となりました。

## TLC2543の仕様

- **分解能**：12bit
- **対応入力数**：11
- **ブレッドボード対応**：対応

詳細は[TLC2543の販売ページ](https://jp.rs-online.com/web/p/analogue-to-digital-converters/5172087)をご参照ください。

Arduino UnoのAD変換は10bitの分解能ですが、TLC2543を使用することでより高い分解能のデータを取得できます。

## TLC2543の接続方法

TLC2543の基本的な構造を以下に示します。このAD変換器は、1から9、11、12のピンに接続された11本のアナログ信号をSPI信号に変換し、15から18の4本の線に変換してデジタル信号として送信します。

![TLC2543の構造図](https://github.com/Shimaxi/TLC2543/assets/61856215/8ab5dd63-39cc-4bb3-b542-46733be4cd88)

### SPI通信とは

SPI通信について詳しく知りたい場合は、以下のサイトが参考になります。

[SPIの基本を学ぶ](https://www.analog.com/jp/analog-dialogue/articles/introduction-to-spi-interface.html)

簡単に言うと、SPI通信は複数の信号を4本の信号に変換して送信し、受信側で複数の信号に復元する通信方式です。

### Arduinoとの接続手順

AD変換器をArduinoに接続する方法は非常に簡単です。Arduinoのデジタルインプットは次のように設定されています：

- **10番**：SS（スレーブ選択）
- **11番**：MOSI（Master Out, Slave In）
- **12番**：MISO（Master In, Slave Out）
- **13番**：SCLK（シリアルクロック）

これに対して、ADコンバータのピンは以下のように接続します：

- **CS（15番）**：Arduinoの10番
- **DATA OUT（16番）**：Arduinoの12番
- **DATA INPUT（17番）**：Arduinoの11番
- **I/O CLOCK（18番）**：Arduinoの13番

（ArduinoのSSピンは自由に設定できますが、説明の便宜上10番としています）

![Arduino SPI接続図](https://github.com/Shimaxi/TLC2543/assets/61856215/44645b96-6fd5-4bfa-bc01-8beca25afb5c)

### 電源と参照電圧

次に、AD変換器の電源と接地を行います：

- **Vcc（20番）**：Arduinoの5V
- **GND（10番）**：ArduinoのGND
- **REF+（14番）**：Arduinoの5V
- **REF-（15番）**：ArduinoのGND

（これらの接続にはブレッドボードなどを使用してください）

VccはAD変換器を動作させるための電力供給用ピンです。GNDは接地用ピンです。REF+/-は参照電圧を提供するピンで、5VとGNDに接続することで基準電圧として使用されます。

以上で接続が完了しました。次に、以下の.unoファイルを書き込んで実行してください。

```c
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
  ad = SPI.transfer((chx << 4) | 0x0C); //チャネル番号に対応した2進数コードを0を4つ分左にシフト、そして空いてる4つの0に0x0C = 16-Bit, LSB-First, Unipolarを入れる
  ad_l = SPI.transfer(0); //
  digitalWrite(chipSelectPin, HIGH);
  SPI.endTransaction();
  ad <<= 8; //adには欲しい12bitのデータのうち前から8bitが入る
  ad |= ad_l; //ad_lにはadの後の8bitが入ってくるので、それを右からくっつけて16bitとする
  ad >>= 4; //欲しいのは12bitの情報なので、後ろの4bit分は不要．よって削る
  
  delayMicroseconds(10);  // EOC
  return ad;
}
```

## SPI通信とデータ取得

これを実行すれば、11個のデジタル値が行ごとに表示されるはずです。（はず...）

### readAdc関数の解説

特に解説が必要な箇所は、`readAdc`関数内の「`ad = SPI.transfer((chx << 4) | 0x0C);`」部分です。これはSPI信号の特徴によるものです。

`SPI.transfer()`は、8bitのコマンドを送信してデータを取得する関数です。例えば、「AIN0を16bitsの長さでMSB firstの形式でUnipolarで取得したい場合、`SPI.transfer(0x0C);`」とすることでAIN0の情報を取得できます。（以下の図を参照）

![SPI通信例](https://github.com/Shimaxi/TLC2543/assets/61856215/b727472a-06f1-4285-bf7d-d93a09d415c9)

取得するデータも8bit形式で返ってくるため、全てのデータを取得するには`ad_l`も付けて、合計16bitの情報を取得し12bitに変換する必要があります。

単純にTLC2543を使うだけならこれで完了です。

# LTC6820 isoSPIの導入

しかし、実験環境の都合上SPI信号の通信線を3m程に延長すると、送信されるデータに0Vや5Vを示すバグが発生し、通信がうまくいかない問題が生じました。原因はSPI通信が長距離のデータ移送に対応しておらず、1mを超えるケーブルを使うと信号が劣化するためです。

参考：[SPIによって、3mのケーブルを介した通信を実現することは可能ですか？](https://www.analog.com/jp/education/landing-pages/003/bbs/bbs_18.html)

この問題を解決するために「LTC6820 isoSPI 絶縁型SPI延長モジュール」を導入しました。これは、SPI信号を2本の差動信号に変換し、LANケーブルを用いて安全に長距離信号を送信できるようにするモジュールです。LANケーブルは8本のツイステッドペアケーブルで構成されており、ノイズ耐性も優れています。

詳細は[販売サイト](https://strawberry-linux.com/catalog/items?code=16820)をご参照ください。

## 接続方法

このモジュールは、マスターとスレーブの2つのユニットから成り、Arduino側にマスター、AD変換器側にスレーブを接続します。

### Arduino側の接続

- **SS（10番）**：モジュールの7番
- **MOSI（11番）**：モジュールの6番
- **MISO（12番）**：モジュールの3番
- **SCLK（13番）**：モジュールの5番

### AD変換器側の接続

- **CS（15番）**：モジュールの7番
- **DATA OUT（16番）**：モジュールの3番
- **DATA INPUT（17番）**：モジュールの6番
- **I/O CLOCK（18番）**：モジュールの5番

![LTC6820接続図](https://github.com/Shimaxi/TLC2543/assets/61856215/6be390fd-6628-4ecc-affc-d229d0956f96)

番号を書き込んだ図を参考にしてください。さらに、モジュールのGNDとVDDSを適当にGNDと5Vに接続してください。

![接続図](https://github.com/Shimaxi/TLC2543/assets/61856215/4a7849c4-ac5f-4ebe-b460-bc591df3bc1f)

これで接続が完了しました。コードをArduinoに書き込んでください。

```c
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
  ad = SPI.transfer((chx << 4) | 0x0E); // チャネル番号に対応した2進数コードを0を4つ分左にシフト、そして空いてる4つの0に0x0E = 16-Bit, LSB-First, Unipolarを入れる
  ad_l = SPI.transfer(0); //
  digitalWrite(chipSelectPin, HIGH);
  SPI.endTransaction();
  ad <<= 8; //adには欲しい12bitのデータのうち前から8bitが入る
  ad |= ad_l; //ad_lにはadの後の8bitが入ってくるので、それを右からくっつけて16bitとする

  ad = ad & 0x0fff; //LSB-Firstでデータを受け取っているのでMSB-Firstになるようデータの方向を反転させる
  ad = (((ad & 0xaaaa) >> 1) | ((ad & 0x5555) << 1));
  ad = (((ad & 0xcccc) >> 2) | ((ad & 0x3333) << 2));
  ad = (((ad & 0xf0f0) >> 4) | ((ad & 0x0f0f) << 4));
  ad = ((ad >> 8) | (ad << 8));
  ad >>= 4; //欲しいのは12bitの情報なので、後ろの4bit分は不要．よって削る

  delayMicroseconds(10);  // EOC
  return ad;
}
```

これで、前述の手順通りに接続すれば、11個のデジタル値が行ごとに表示されるはずです。ここで注意が必要なのは、コード中の `readAdc` 関数です。

従来通りMSB-Firstでデータを取得しようとすると、なぜか最上位ビットに必ず1が印加されるバグが発生しました。これは差動信号を通したことで起こるバグのようですが、詳細な原因は分かりません。そこで、仕方なくデータをLSB-Firstで受け取り（`ad = SPI.transfer((chx << 4) | 0x0E);`）、コード内でビットの順番を反転して無理やりMSB-Firstに変換する方法を採用しました。

本来であれば、`SPISettings` で `LSBFirst` を選択することで解決できるはずですが、この設定では上手く動作しなかったため、やむを得ずこの方法を使用しています。

このようにゴリ押しで解決した点が多く、自身の未熟さを痛感しましたが、何とか動作させることができました。

ここから先は、自由にコードを書き換えてもらって構いません。

なお、コードに関しては、ある方がGitHub上に公開していたものを参考にさせていただきましたが、現在そのリポジトリは非公開となっています。この場を借りて、強く感謝の意を表します。

