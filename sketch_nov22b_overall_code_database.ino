#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <DHT.h> // Include the DHT sensor library

// Define the sensor pins
#define TRIG_PIN 25        // Digital pin for Trigger (Aj-SRO4M)
#define ECHO_PIN 26        // Digital pin for Echo (Aj-SRO4M)
#define MQ135_PIN 32       // Analog pin for MQ-135
#define MQ4_PIN 35         // Analog pin for MQ-4
#define MQ7_PIN 33         // Analog pin for MQ-7
#define DHTPIN 14         // GPIO pin connected to DHT11
#define DHTTYPE DHT11      // Define the sensor type (DHT11)

// Firebase Setup
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define WIFI_SSID "BCN-FIBER-FAST-NET 285" // Replace with your Wi-Fi SSID
#define WIFI_PASSWORD "85858585"           // Replace with your Wi-Fi Password

#define API_KEY "AIzaSyBoHVHELzc1qk3WTa_yBvWGln2UsmePScw" // Replace with your Firebase API Key
#define DATABASE_URL "sensors-bf1cc-default-rtdb.firebaseio.com/" // Replace with your Firebase RTDB URL

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// Poultry farm cycle parameters
unsigned long cycleStartTime;
const unsigned long broodingDuration = 60000; // Simulates 10 days = 1 min
const unsigned long growingDuration = 84000; // Simulates 14 days = 1.4 min

// Initialize the DHT sensor
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  dht.begin(); // Initialize the DHT sensor

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to Wi-Fi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase setup successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase signup error: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize cycle timer
  cycleStartTime = millis();
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2; // Distance in cm
}

void loop() {
  delay(2000);

  // Read DHT11 data
  float temperature = dht.readTemperature(); // Read temperature in Celsius
  float humidity = dht.readHumidity();       // Read humidity in percentage

  // Validate DHT data
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Read other sensors
  long distance = measureDistance();
  int airQuality = map(analogRead(MQ135_PIN), 0, 4095, 0, 100);
  int methaneLevel = map(analogRead(MQ4_PIN), 0, 4095, 0, 100);
  int coLevel = map(analogRead(MQ7_PIN), 0, 4095, 0, 100);

  // Determine current cycle
  unsigned long elapsedTime = millis() - cycleStartTime;
  String currentCycle;

  if (elapsedTime < broodingDuration) {
    currentCycle = "Brooding";
  } else if (elapsedTime < broodingDuration + growingDuration) {
    currentCycle = "Growing";
  } else {
    Serial.println("Cycle complete. Resetting...");
    cycleStartTime = millis();
    return;
  }

  // Print to Serial
  Serial.printf("Cycle: %s | Temp: %.1f °C | Humidity: %.1f %% | Distance: %ld cm | Air Quality: %d %% | Methane: %d %% | CO: %d %%\n",
                currentCycle.c_str(), temperature, humidity, distance, airQuality, methaneLevel, coLevel);

  // Upload data to Firebase
  if (Firebase.ready() && signupOK) {
    Firebase.RTDB.setString(&fbdo, "Cycle/current", currentCycle);
    Firebase.RTDB.setFloat(&fbdo, "DHT/temperature", temperature);
    Firebase.RTDB.setFloat(&fbdo, "DHT/humidity", humidity);
    Firebase.RTDB.setInt(&fbdo, "Aj-SRO4M/distance", distance);
    Firebase.RTDB.setInt(&fbdo, "MQ135/airQuality", airQuality);
    Firebase.RTDB.setInt(&fbdo, "MQ4/methane", methaneLevel);
    Firebase.RTDB.setInt(&fbdo, "MQ7/coLevel", coLevel);
  }
}
