#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>

Preferences preferences;

#define DEVICE_NAME         "Pump_1234"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


int counter = 0;
bool enableSerial = false;
String label = "";

typedef struct {
    volatile uint64_t total_accumulator;
    volatile int16_t counter;
} variable_t __attribute__((packed));

variable_t variable{ 0, 0 };

BLECharacteristic* pCharacteristic;


extern "C" {
#include "soc/pcnt_struct.h"
}
#include "driver/pcnt.h"
#include <EEPROM.h>

byte pulsePin = 5; // Pulse Counter

int16_t flowCounter = 0;
int16_t Pulses = 0;
int16_t x;
volatile uint32_t us_time, us_time_diff;


volatile byte state = LOW;
volatile byte state2 = LOW;
volatile byte state_tmr = 0;
volatile byte value_ready = 0;

#define PCNT_TEST_UNIT PCNT_UNIT_0
#define PCNT_H_LIM_VAL 32767
#define PCNT_L_LIM_VAL -1



void setup() {
    Serial.begin(9600);

    preferences.begin("my−app", false);

    label = preferences.getString("label");

    LoadStruct(&variable, sizeof(variable));



    InitBLE();


    pinMode(pulsePin, INPUT);
    

    InitCounter();

    
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Connected");
      pServer->startAdvertising(); // restart advertising
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Disconnected");
      pServer->startAdvertising(); // restart advertising
    }
};

class MyCallbacks : public BLECharacteristicCallbacks {

    void onRead(BLECharacteristic* pCharacteristic) {
        counter = 0;

        char buffer[40];
        sprintf(buffer, "%llu,%s", (variable.total_accumulator + variable.counter), label);

        pCharacteristic->setValue(buffer);
    }

    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();

        if (value.length() > 0) {

              String str = "";
            
             

            Serial.println("*********");
            Serial.print("New value: ");



            for (int i = 0; i < value.length(); i++) {

                str += value[i];
                Serial.print(value[i]);

                counter = 0;

                if (value[i] == '1') {
                    pinMode(2, OUTPUT);
                    digitalWrite(2, HIGH);
                }

                if (value[i] == '2') {
                    pinMode(2, OUTPUT);
                    digitalWrite(2, LOW);
                }

                if(value[i] == '3')
                {
                    ResetCounter();
                }
            }

            label = str;
            if(str.length() >= 3){
              preferences.putString("label", str);
            }
            

            Serial.println();
            Serial.println("*********");
        }
    }
};

void InitBLE() {
    BLEDevice::init(DEVICE_NAME);
    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);
    

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE
    );

    pCharacteristic->setCallbacks(new MyCallbacks());

    pCharacteristic->setValue("0");
    pService->start();

    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->start();
}

volatile uint64_t prior_count = 0;
long previousMillis = 0;


void ResetCounter()
{
    digitalWrite(2, LOW);

    variable.total_accumulator = 1;
    variable.counter = 0;

    pcnt_counter_clear(PCNT_TEST_UNIT);
    pcnt_counter_resume(PCNT_TEST_UNIT);

    delay(100);

    StoreStruct(&variable, sizeof(variable));
    preferences.putString("label", "");

    delay(300);

    ESP.restart();
}



void loop() {
  CalculatePulses();

  Serial.println(label);

  unsigned long currentMillis = millis();

  if(counter >= 2){
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
  }

  if(enableSerial){
    //Read Serial Data from TX and send it through BLE
    String message = Serial.readString();
    if (message.length() >= 1) {
      char buffer[100];
      sprintf(buffer, "%s", message);
      pCharacteristic->setValue(buffer);
    }
  }
  

  delay(1000);


    

    //Save the Values in Memory
    if ((currentMillis - previousMillis >= 10000)) {

        counter++;

        //if ((currentMillis - previousMillis >= 5000)) {
        prior_count = variable.counter;
        previousMillis = currentMillis;
        
        StoreStruct(&variable, sizeof(variable));

        delay(100);

        Serial.println("Saved");
    }

}

void InitCounter() {

    pcnt_config_t pcnt_config = {
    pulsePin, // Pulse input gpio_num, if you want to use gpio16, pulse_gpio_num = 16, a negative value will be ignored
    PCNT_PIN_NOT_USED, // Control signal input gpio_num, a negative value will be ignored
    PCNT_MODE_KEEP, // PCNT low control mode
    PCNT_MODE_KEEP, // PCNT high control mode
    PCNT_COUNT_INC, // PCNT positive edge count mode
    PCNT_COUNT_DIS, // PCNT negative edge count mode
    PCNT_H_LIM_VAL, // Maximum counter value
    PCNT_L_LIM_VAL, // Minimum counter value
    PCNT_TEST_UNIT, // PCNT unit number
    PCNT_CHANNEL_0, // the PCNT channel
    };

    if (pcnt_unit_config(&pcnt_config) == ESP_OK) //init unit
        Serial.println("Config Unit_0 = ESP_OK");

    pcnt_filter_enable(PCNT_TEST_UNIT);

    pcnt_intr_disable(PCNT_TEST_UNIT);
    pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_L_LIM);
    pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_H_LIM);
    pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_THRES_0);
    pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_THRES_1);
    pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_ZERO);

    pcnt_counter_pause(PCNT_TEST_UNIT);

    pcnt_counter_clear(PCNT_TEST_UNIT);

    pcnt_intr_enable(PCNT_TEST_UNIT);

    pcnt_counter_resume(PCNT_TEST_UNIT);

}

void CalculatePulses() {
    if (pcnt_get_counter_value(PCNT_TEST_UNIT, &flowCounter) == ESP_OK)
    {
        variable.counter = flowCounter;

        Serial.printf("[%llu]", (variable.total_accumulator + variable.counter));
        Serial.println();

        if (flowCounter >= 50) {
            variable.total_accumulator += flowCounter;

            pcnt_counter_clear(PCNT_TEST_UNIT);
            pcnt_counter_resume(PCNT_TEST_UNIT);
        }

    }


    Serial.print("FlowCounter: ");
    Serial.println(flowCounter);

    Serial.printf("Counter: %" PRId16 "\n", variable.counter);

    Serial.printf("Accumulator: %llu, Total: %llu", variable.total_accumulator, (variable.total_accumulator + variable.counter));
    Serial.println();
}

void StoreStruct(void* data_source, size_t size)
{
    EEPROM.begin(size * 2);
    for (size_t i = 0; i < size; i++)
    {
        char data = ((char*)data_source)[i];
        EEPROM.write(i, data);
    }
    EEPROM.commit();
}

void LoadStruct(void* data_dest, size_t size)
{
    EEPROM.begin(size * 2);
    for (size_t i = 0; i < size; i++)
    {
        char data = EEPROM.read(i);
        ((char*)data_dest)[i] = data;
    }
}
