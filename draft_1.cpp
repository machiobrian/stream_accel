#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <FirebaseESP32.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

// projects API
#define API_KEY "AIzaSyBHn1jEAilZpcpaIsCz-jADsU3pNc07T14"
#define DATABASE_URL "https://stream-accel-data-default-rtdb.firebaseio.com"

// Create an instance of the ADXL345 library
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified();

// manage token generation
#include "addons/TokenHelper.h"
// streaming and printing sensor data
#include "addons/RTDBHelper.h"
// unique id incase we have multiple sensors
#define DEVICE_UID "Accelerometer 1"

// WIFI Credentials
#define WIFI_SSID "_braen"
#define WIFI_PASSWORD "Callmebr@3nn"

// required essentials for linking project to cloud
FirebaseData fbdo;     // realtime db object
FirebaseAuth auth;     // authentication object
FirebaseConfig config; // configuration object

// global variables
String device_location = "Elevator A (Naivas, Nrb)"; // device location config
String databasePath = "";
String fuid = "";
FirebaseData streamDb;                // Global Firebase RTDB object for streaming the data
unsigned long elapsedMillis = 0;      // store elapsed time from device startup
unsigned long update_interval = 1000; // 10 seconds before new data is sent to the cloud
int count = 0;                        // test firebase updates
bool isAuthenticated = false;         // device authentication status

// Define the sampling frequency in Hz
#define SAMPLING_FREQUENCY 1000

// Define the filter order and cutoff frequency
#define FILTER_ORDER 2
#define CUTOFF_FREQUENCY 5

// Define the buffer size for the filter
#define BUFFER_SIZE (FILTER_ORDER + 1)

// Define the buffer for the filter
float x_buffer[BUFFER_SIZE] = {0};
float y_buffer[BUFFER_SIZE] = {0};
float z_buffer[BUFFER_SIZE] = {0};

// Initialize the filter coefficients
float b[FILTER_ORDER + 1] = {0.2929, 0.5858, 0.2929};
float a[FILTER_ORDER + 1] = {1.0000, -0.1716, 0.2929};

// JSON object to hold the updated sensor values to be sent to the firebase
FirebaseJson accel_readings;

void Wifi_Init() // an arduino wifi API for wifi initialization
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wifi ...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void firebase_init()
{
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // enable wifi reconnection
  Firebase.reconnectWiFi(true);

  Serial.println("--------------------------------------------");
  Serial.println("Signing up a new User ... ");
  // we sign in anonymously:
  if (Firebase.signUp(&config, &auth, "", "")) // & for referencing
  {
    Serial.println("Success");
    isAuthenticated = true;

    // set the db path where updates ill be loaded for this device
    databasePath = "/Elevator A (Naivas, Nrb)"; //*******************
    fuid = auth.token.uid.c_str();
  }
  else
  {
    Serial.printf("Failed, %s\n", config.signer.signupError.message.c_str());
    isAuthenticated = false;
  }

  // callback function for the longrunning tokwn generation task
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth); // initialize the firebase library
}

void setup()
{

  Serial.begin(9600);
  Wifi_Init();
  firebase_init();

  // Initialize the ADXL345 library
  if (!accel.begin())
  {
    Serial.println("Could not find a valid ADXL345 sensor, check wiring!");
    while (1)
      ;
  }

  accel.setDataRate(ADXL345_DATARATE_800_HZ); // Set the data rate to the desired frequency
  accel.setRange(ADXL345_RANGE_2_G);          // Set the range to the desired value

  // initialize accelerometer json data
  accel_readings.add("device", DEVICE_UID);
  accel_readings.add("name", "Accelerometer 1");
  accel_readings.add("type", "Vibration Data");
  accel_readings.add("location", device_location);
  // accel_readings.add("x_axis", filtered_x);
  // accel_readings.add("y_axis", filtered_y);
  // accel_readings.add("z_axis", filtered_z);
}

void streamToFirebase(float x, float y, float z)
{
  // Clear the Firebase JSON object
  accel_readings.clear();

  // Add the filtered x, y and z axis data to the JSON object
  accel_readings.add("x_axis", x);
  accel_readings.add("y_axis", y);
  accel_readings.add("z_axis", z);

  // Push the JSON object to the Firebase Realtime Database
  String path = databasePath + "/data";
  if (Firebase.RTDB.push(&streamDb, path.c_str(), &accel_readings))
  {
    Serial.println("Data streamed successfully!");
  }
  else
  {
    Serial.println("Data stream failed!");
    Serial.println("Error: " + streamDb.errorReason());
  }
}

void loop()
{
  // Read the acceleration values from the ADXL345
  sensors_event_t event;
  accel.getEvent(&event);

  // Add the new acceleration values to the buffer
  for (int i = BUFFER_SIZE - 1; i > 0; i--)
  {
    x_buffer[i] = x_buffer[i - 1];
    y_buffer[i] = y_buffer[i - 1];
    z_buffer[i] = z_buffer[i - 1];
  }
  x_buffer[0] = event.acceleration.x;
  y_buffer[0] = event.acceleration.y;
  z_buffer[0] = event.acceleration.z;

  // Apply the filter to the acceleration data
  float filtered_x = 0;
  for (int i = 0; i <= FILTER_ORDER; i++)
  {
    filtered_x += b[i] * x_buffer[i] - a[i] * x_buffer[FILTER_ORDER - i];
  }
  float filtered_y = 0;
  for (int i = 0; i <= FILTER_ORDER; i++)
  {
    filtered_y += b[i] * y_buffer[i] - a[i] * y_buffer[FILTER_ORDER - i];
  }
  float filtered_z = 0;
  for (int i = 0; i <= FILTER_ORDER; i++)
  {
    filtered_z += b[i] * z_buffer[i] - a[i] * z_buffer[FILTER_ORDER - i];
  }

  // Stream the filtered data to Firebase
  if (elapsedMillis > update_interval && isAuthenticated)
  {
    streamToFirebase(filtered_x, filtered_y, filtered_z);
    elapsedMillis = 0;
    count++;
  }

  delay(1);
  elapsedMillis++;
}

