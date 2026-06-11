#include "EEPROM.h"
#include "BluetoothSerial.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
BluetoothSerial SerialBT;
#define EEPROM_SIZE 64
#define pulsePine 13

#define analogPin A0
#define batLevPin A7
int timer = 200;
bool clientConnected = true;

char dataG;
//**************IMPORTANT*******************
//******************************************
const int delayTime = 60;
//*****************************************
//*****************************************
// EEPROM variables
int addr_duty = 0;
int addr_freq = 1;
int stored_value;
int duty_cycle;
int duty_cycle_temp;
int freq;
int freq_temp;
int duty_def_value = 13;
int freq_def_value = 60;
// Balance variables
int value_count = 0;
int value_count_def = 100;
int balance_value = 0;
int balance_value_temp = 0;

//****
unsigned long startMillis;
unsigned long currentMillis;
long period = 100000; // the value is a number of microseconds
// Measuring of level of the battery
float resistencia1 = 100000; // Resistencia de 100K para medir la tencion (Voltios)/Resistance of 100k for test volts
float resistencia2 = 47000;  // Resistencia de 47k para medir la tencion (Voltios)/Resistance 47k for test volts
float const arefVolt = 3.6f; // pin "3.3v" SET EXACT VALUE HERE
float voutv;
float vinv;
int raw_bat_data;
unsigned long startMillisVolts;
unsigned long currentMillisVolts;
long periodVolts = 1000; // the value is a number of microseconds
int sensorValue = 0.0f;

void setup()
{
  SerialBT.begin("ESP32_Spirit_PI-2"); // Bluetooth device name
  Serial.begin(115200);
  SerialBT.register_callback(callback);

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM");
    delay(1000000);
  }

  readFromStorage(addr_duty);
  duty_cycle = stored_value;
  readFromStorage(addr_freq);
  freq = stored_value;
  if (duty_cycle == 255)
  {
    writeToStorage(duty_def_value, addr_duty);
    readFromStorage(addr_duty);
    duty_cycle = stored_value;
  }

  if (freq == 255)
  {
    writeToStorage(freq_def_value, addr_freq);
    readFromStorage(addr_freq);
    freq = stored_value;
  }

  pinMode(pulsePine, OUTPUT);
}

void loop()
{

  currentMillis = micros();
  currentMillisVolts = millis();

  if (SerialBT.available() > 0)
  {
    dataG = SerialBT.read();
    setDutyAndFreq(dataG);
  }

  if (currentMillis - startMillis >= period && clientConnected)
  {

    period = 1000000 / freq;
    // Serial.println(period);
    digitalWrite(pulsePine, HIGH);
    duty_cycle_temp = duty_cycle * 10;
    delayMicroseconds(duty_cycle_temp);
    digitalWrite(pulsePine, LOW);
    // sensorValue = analogRead(analogPin);
    delayMicroseconds(delayTime);
    sensorValue = analogRead(analogPin);
    sensorValue = sensorValue / 10;

    sendData();

    startMillis = currentMillis;
  }
  // Lectura voltios
  if (currentMillisVolts - startMillisVolts >= periodVolts)
  {
    lecturaVoltios();
    // Serial.println("Lectura voltios");
    startMillisVolts = currentMillisVolts;
  }
}

void writeToStorage(int valor, int addr)
{
  EEPROM.write(addr, valor);
  EEPROM.commit();
}
int readFromStorage(int addr)
{
  stored_value = EEPROM.read(addr);

  return stored_value;
}
void setDutyAndFreq(char valor)
{
  //"n" valor para aumentar duty cycle
  //"m" valor para disminuir duty cycle
  //"j" valor para aumentar la frequencia
  //"k" valor para des,inuir la frequencia
  //"+" valor para aumentar el balance
  //"-" valor para desminuir el balance
  if (valor == 'n')
  {
    // Serial.println("n Recived");
    readFromStorage(addr_duty);
    duty_cycle = stored_value;
    duty_cycle = duty_cycle + 1;
    writeToStorage(duty_cycle, addr_duty);
  }
  else if (valor == 'm')
  {
    //  Serial.println("m Recived");
    readFromStorage(addr_duty);
    duty_cycle = stored_value;
    duty_cycle = duty_cycle - 1;
    writeToStorage(duty_cycle, addr_duty);
  }
  else if (valor == 'j')
  {
    //    Serial.println("j Recived");
    readFromStorage(addr_freq);
    freq = stored_value;
    freq = freq + 10;
    writeToStorage(freq, addr_freq);
  }
  else if (valor == 'k')
  {
    //   Serial.println("k Recived");
    readFromStorage(addr_freq);
    freq = stored_value;
    freq = freq - 10;
    writeToStorage(freq, addr_freq);
  }
  else if (valor == 'p')
  {
    //  Serial.println("m Recived");

    writeToStorage(0, addr_freq);
    writeToStorage(0, addr_duty);
  }
}
// Volt function
void lecturaVoltios()
{
  vinv = 0.0f;
  voutv = 0.0f;
  for (int i = 0; i < 10; i++)
  {

    voutv = (analogRead(batLevPin) * arefVolt) / (4095); // Lee el voltaje de entrada

    vinv += ((resistencia1 + resistencia2) * voutv) / resistencia2; // Fórmula del divisor resistivo para el voltaje final
    if (vinv < 0.9)
    {
      vinv = 0.0f;
    }
  }
  vinv = vinv / 10;
}
void sendData()

{
  /* Serial.print("<");
Serial.print(sensorValue);
Serial.print("/");
Serial.print(freq);
 Serial.print("/");
  Serial.print( duty_cycle);
    Serial.print("/");
  Serial.print( vinv);
Serial.print(">");
Serial.println();*/
  String dataG = "<";
  dataG += sensorValue;
  dataG += "/";
  dataG += freq;
  dataG += "/";
  dataG += duty_cycle;
  dataG += "/";
  dataG += vinv;
  dataG += ">";
  /* bluetooth.print("<");
  bluetooth.print(sensorValue);
   bluetooth.print("/");
    bluetooth.print(freq);
     bluetooth.print("/");
     bluetooth.print( duty_cycle);
        bluetooth.print("/");
      bluetooth.print( vinv);
  bluetooth.print(">");*/
  SerialBT.println(dataG);
  Serial.println(dataG);
}
void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
  // Callback function implementation
  if (event == ESP_SPP_SRV_OPEN_EVT)
  {
    Serial.println("Client Connected");
    clientConnected = true;
  }

  if (event == ESP_SPP_CLOSE_EVT)
  {
    Serial.println("Client disconnected");
    //  SerialBT.flush();
    // SerialBT.end();
    clientConnected = false;
    delay(1000);
    ESP.restart();
  }
}