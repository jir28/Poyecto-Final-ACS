#define DEVICE "ESP32"
#include "WiFi.h"
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "m8eDP4kZfzF0EzrprydCwKyZjfVwn5r3fA1-FU4HecA9uq0ngiQ-iC6qgHsaF6ba6repn8dEpzuBa0LrGCpfbw=="
#define INFLUXDB_ORG "5ac60ca81cc1a27e"
#define INFLUXDB_BUCKET "FINISH"
#define INFLUXDB_MEASUREMENT "esp32_1_sensors"
#define TZ_INFO "UTC-6"

#define ONE_WIRE_BUS 27  // Pin  DS18B20
#define RELAY_PIN 26     // Pin RELAY
#define LPG_THRESHOLD 40 // Umbral de concentración de LPG en ppm para activar el relé


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor(INFLUXDB_MEASUREMENT);

const char* ssid = "Totalplay-C9A8_EXT";
const char* pass = "Password28..";
const int MQ6_Pin = 34;       // Define GPIO 34 for MQ6 Sensor Pin
const int MQ135_Pin = 35;     // Define GPIO 35 for MQ135 Sensor Pin
float Referance_V = 3300.0;   // ESP32 Reference Voltage in mV
float RL = 1.0;               // In Module RL value is 1k Ohm
float Ro_MQ6 = 10.0;          // The Ro value is 10k Ohm
float Ro_MQ135 = 10.0;        // The Ro value is 10k Ohm
const float Ro_clean_air_factor = 10.0;
int cons = 0;

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, pass);
  Serial.print("Se está conectando a la red WiFi denominada ");
  Serial.println(ssid);
  pinMode(MQ6_Pin, INPUT);  
  pinMode(MQ135_Pin, INPUT);  
  delay(500);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  Serial.println("Wait for 30 sec warmup");
  delay(30000); 
  Serial.println("Warmup Complete");
  Ro_MQ6 = calibrateSensor(MQ6_Pin);
  Ro_MQ135 = calibrateSensor(MQ135_Pin);
  pinMode(RELAY_PIN, OUTPUT); // Configura el pin del relé como salida
  digitalWrite(RELAY_PIN, LOW); // Inicializa el relé en estado apagado

  sensors.begin();  // Inicia el sensor DS18B20


  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  sensor.clearFields();
  float lpg_ppm = readSensor(MQ6_Pin, Ro_MQ6);
  float air_quality_ppm = readSensor(MQ135_Pin, Ro_MQ135);

  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);  // Lee la temperatura del DS18B20

  if (temperature != DEVICE_DISCONNECTED_C) {
    sensor.addField("Temperature", temperature);
    Serial.print("Temperature: ");
    Serial.println(temperature);
  } else {
    Serial.println("Failed to read from DS18B20 sensor!");
  }

  sensor.addField("LPG", lpg_ppm);
  sensor.addField("AirQuality", air_quality_ppm);

  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());

  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  } else {
    Serial.println("Data written successfully.");
  }
  if (lpg_ppm > LPG_THRESHOLD && cons == 0) {
    digitalWrite(RELAY_PIN, HIGH); //Relay On
    cons = 1;
    Serial.println("LPG concentration above threshold! Relay activated.");
  } else {
    digitalWrite(RELAY_PIN, LOW);  // Relay Off
    Serial.println("LPG concentration below threshold. Relay deactivated.");
  }
  delay(10000); // Delay for 10 seconds before next reading
}

float calibrateSensor(int pin) {
  float mVolt = 0.0;
  for (int i = 0; i < 30; i++) {
    mVolt += Get_mVolt(pin);
  }
  mVolt /= 30.0;
  return Calculate_Rs(mVolt) / Ro_clean_air_factor;
}

float readSensor(int pin, float Ro) {
  float mVolt = 0.0;
  for (int i = 0; i < 500; i++) {
    mVolt += Get_mVolt(pin);
  }
  mVolt /= 500.0;

  float Rs = Calculate_Rs(mVolt);
  float Ratio_RsRo = Rs / Ro;
  return calculatePPM(Ratio_RsRo);
}

float Calculate_Rs(float Vo) {
  return (Referance_V - Vo) * (RL / Vo);
}

unsigned int calculatePPM(float RsRo_ratio) {
  return (unsigned int) pow((RsRo_ratio / 18.446), (1 / -0.421));
}

float Get_mVolt(int AnalogPin) {
  int ADC_Value = analogRead(AnalogPin); 
  delay(1);
  return ADC_Value * (Referance_V / 4096.0);
}
