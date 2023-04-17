// necessary files for the development environment
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
// library to allow esp32 CRUD data from/to Google
// Create, Read, Update and Delete
#include <FirebaseESP32.h>
// sensor libraries adxl345
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h> // dependent on Adafruit_Sensor.h

            /* Essential setup for the FIREBASE */

// Projects API keys and link to the data streaming site 
// created and fetched from google firebase
#define API_KEY "AIzaSyBHn1jEAilZpcpaIsCz-jADsU3pNc07T14"
#define DATABASE_URL "https://stream-accel-data-default-rtdb.firebaseio.com"


#include "addons/TokenHelper.h" // manages token generation ; access tokens
#include "addons/RTDBHelper.h" // manage streaming and printing sensor data on the RTDB

// objects for linking the local project to cloud
FirebaseAuth auth; // authentication object
FirebaseConfig config; // configuration object
FirebaseData streamDB; // global firebase object for streaming the sensor data
FirebaseJson accel_readings; // json obj that holds the updated sensor data to be sent to firebase

            /* Essential Sensor Setup */

// initialize an instance of the adxl345 library
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified();
// for connectivity : esp32 and internet
#define WIFI_SSD "_braen"
#define WIFI_PASSWORD "1234567890"
#define DEVICE_ID "Sensor_1"

            /* Define some global variables */
String deviceLocation = "Naivas(Nyr)";
String databasePath = " "; // empty variable
String fUID = " ";

            /* Timing */
unsigned long elapsedMillis = 0; // initialize a  variable that stores elapsed time 
                                 // from device startup
unsigned long update_interval = 100; // wait for 0.1 seconds before sending in new sensor values
bool isAuthenticated = false;    // Check for device authentication status
int count = 0; // a counter variable to add to elapsedMillis

            /* Filter Setup. 2nd Order Low-Pass filter: 
            Attenuates any vibrations above the cutoff frequency
            Initialize buffer to hold acceleration values to be filtered and calculate the avg
            - moving avg filter : smooths out data */
#define FILTER_ORDER 2
#define CUTOFF_FREQ 5
#define BUFFER_SIZE (FILTER_ORDER + 1)

// define the buffer for the filter
float x_buff[BUFFER_SIZE] = {0};
float y_buff[BUFFER_SIZE] = {0};
float z_buff[BUFFER_SIZE] = {0};

// initialize filter co-efficients: can be manually tuned
float b[FILTER_ORDER + 1] = {0.2929, 0.5858, 0.2929};
float a[FILTER_ORDER + 1] = {1.0000, -0.1716, 0.2929};

                    
                        /* DEFINING CUSTOM FUNCTIONS */

/* WiFi initialization Function */
void Wifi_Init(){
    WiFi.begin(WIFI_SSD, WIFI_PASSWORD);
    Serial.print("Connecting to Wifi (_braen)");
    while (WiFi.status() != WL_CONNECTED){
        Serial.print("."); // what to displat while connecting
        delay(300); // milliseconds
    }
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
}

/* Firebase Initialization Function */
void Firebase_init(){
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    Firebase.reconnectWiFi(true); // enable wifi reconnection
    Serial.println("Signing up a new User ... ");

    // we initialized our connection policy to anonymous
    if (Firebase.signUp(&config, &auth, "", "")){
        Serial.println("Success.");
        isAuthenticated = true;
        databasePath = "/" + deviceLocation; // path where all updates will be loaded for this device 
        fUID = auth.token.uid.c_str();
    }
    else{
        Serial.printf("Failed, %s\n", config.signer.signupError.message.c_str());
    isAuthenticated = false;
    }

    // we initialize a callback function since we have a lengthy token gen. process
    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
}
/* Stream to Firebase */
void streamToFirebase(float x, float y, float z){
    // Clear the firebase JSON object
    accel_readings.clear();
    // Add the filtred x,y,z axis data to the JSON object
    accel_readings.add("x_reading", x);
    accel_readings.add("y_reading", y);
    accel_readings.add("z_reading", z);
    // Push the JSON object to firebase RTDB
    String path = databasePath + "/Vibration DataStream";
    // check the integrity of the datastream
    if (Firebase.RTDB.push(&streamDB, path.c_str(), &accel_readings)){
        // path.c_str() - convert the string object to a c-style string 
        // databasePath + "/Vibration DataStream"
        Serial.println("Streamed Successfully.");
    } 
    else{
        Serial.println("Data stream failed!");
        Serial.println("Error: " + streamDB.errorReason());
    }
}


                    /* GENERAL FUNCTIONS */
void setup(){
    Serial.begin(9600);
    Wifi_Init();
    Firebase_init();
    // initialize the adxl345 library
    if (!accel.begin()){
        Serial.println("No valid sensor found. Check wiring or protocol");
        while(1);
    }

    accel.setDataRate(ADXL345_DATARATE_800_HZ);
    accel.setRange(ADXL345_RANGE_16_G);
}

void loop(){
    // read acceleration values from adxl345
    sensors_event_t event;
    accel.getEvent(&event);
    // add new acceleration values to the buffer
    for (int i = BUFFER_SIZE - 1; i>0; i--){
        x_buff[i] = x_buff[i-1];
        y_buff[i] = y_buff[i-1];
        z_buff[i] = z_buff[i-1];
    }
    x_buff[0] = event.acceleration.x;
    y_buff[0] = event.acceleration.y;
    z_buff[0] = event.acceleration.z;
    // apply the filter to the acceleration data
    float filtered_x = 0;
    for(int i=0; i<=FILTER_ORDER; i++)
    {
        filtered_x += b[i]*x_buff[i] - a[i]*x_buff[FILTER_ORDER-i];
    }
    float filtered_y = 0;
    for(int i=0; i<=FILTER_ORDER; i++)
    {
        filtered_y += b[i]*y_buff[i] - a[i]*y_buff[FILTER_ORDER-i];
    }
    float filtered_z = 0;
    for(int i=0; i<=FILTER_ORDER; i++)
    {
        filtered_z += b[i]*z_buff[i] - a[i]*z_buff[FILTER_ORDER-i];
    }
    // stream the filtred data to Firebase ********* changes can be made incase we wanna see unfiltred stream
    if (elapsedMillis > update_interval && isAuthenticated){
        // the above if function may introduce some sort of delay to prog execution
        // streamToFirebase(
        //     filtered_x,
        //     filtered_y,
        //     filtered_z
        // );

        streamToFirebase(
            x_buff[0],
            y_buff[0],
            z_buff[0]
        );


        elapsedMillis = 0;
        count++;
    }
    // delay(1000);
    elapsedMillis++;
}