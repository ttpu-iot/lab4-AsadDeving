

#define BLYNK_FIRMWARE_VERSION "0.0.2"

/* Fill-in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID "TMPL4dQE16R37"
#define BLYNK_TEMPLATE_NAME "Lab4"
#define BLYNK_AUTH_TOKEN "W8bEbnTdLtNmqz5_07Rk5AB9gDDWPACT"


/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial
// #define BLYNK_DEBUG


#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
// #include <ESP32Servo.h>
#include <Servo.h>

#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Update.h>


//----------------------------------------------
// GLOBAL VARIABLES and CONSTANTS
const int RED_PIN = 26;
const int GREEN_PIN = 27;
const int BLUE_PIN = 14;
const int YELLOW_PIN = 12;
const int BUZZER_PIN = 32;
const int SERVO_PIN = 5;
const int BUTTON_PIN = 25;
const int LIGHT_SENSOR_PIN = 33; // Analog pin for light sensor

Servo myServo = Servo();
// Servo myBuzzer = Servo();

hd44780_I2Cexp lcd;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;

// char ssid[] = "Wokwi-GUEST";
// char pass[] = "";
char ssid[] = "MaxPC";
char pass[] = "polito2025";

int buzzer_freq = 1000;
bool buzzer_state = false;
unsigned long buzzer_start_time = 0;
const unsigned long BUZZER_MAX_DURATION = 3000; // 3 seconds

unsigned long last_sensor_read = 0;
const unsigned long SENSOR_INTERVAL = 5000; // 5 seconds
int light_value = 0;

String last_event = "Ready";

//----------------------------------------------
// FUNCTIONS
void updateLCD() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(last_event);
    lcd.setCursor(0, 1);
    lcd.print("L=");
    lcd.print(light_value);
}

BLYNK_WRITE(V1) {   
    int value = param.asInt();
    digitalWrite(RED_PIN, value);
    last_event = value ? "Red: ON" : "Red: OFF";
    updateLCD();
}

BLYNK_WRITE(V2) {   
    int value = param.asInt();
    digitalWrite(GREEN_PIN, value);
    last_event = value ? "Green: ON" : "Green: OFF";
    updateLCD();
}

BLYNK_WRITE(V5) {   
    int value = param.asInt();
    digitalWrite(BLUE_PIN, value);
    last_event = value ? "Blue: ON" : "Blue: OFF";
    updateLCD();
}

BLYNK_WRITE(V6) {   
    int value = param.asInt();
    digitalWrite(YELLOW_PIN, value);
    last_event = value ? "Yellow: ON" : "Yellow: OFF";
    updateLCD();
}

BLYNK_WRITE(V7) {   
    buzzer_freq = param.asInt();
}

BLYNK_WRITE(V3) {   
    int value = param.asInt();
    if (value == 1) {
        ledcWriteTone(2, buzzer_freq);
        // tone(BUZZER_PIN, buzzer_freq);
        // myBuzzer.tone(BUZZER_PIN, buzzer_freq, BUZZER_MAX_DURATION);
        buzzer_state = true;
        buzzer_start_time = millis();
        last_event = "Buz: ON " + String(buzzer_freq) + "Hz";
        Serial.println("Buzzer ON at " + String(buzzer_freq) + "Hz");
    } else {
        ledcWriteTone(2, 0);
        // noTone(BUZZER_PIN);
        // myBuzzer.writeMicroseconds(BUZZER_PIN, 0); // Turn off buzzer
        buzzer_state = false;
        last_event = "Buz: OFF";
        Serial.println("Buzzer OFF");
    }
    updateLCD();
}

BLYNK_WRITE(V8) {   
    int value = param.asInt();
    myServo.write(SERVO_PIN, value);
    last_event = "Servo: " + String(value) + (char)223; // degree symbol
    Serial.print("Servo Position: ");
    Serial.println(value);
    updateLCD();
}


//----------------------------------------------
// BLYNK_WRITE lets you listen for Blynk OTA triggers
// --- OTA Handler using Update.h ---
BLYNK_WRITE(InternalPinOTA) {
    String url = param.asString();
    
    // Convert HTTPS to HTTP by removing the 's'
    // if (url.startsWith("https://")) {
    //     url.replace("https://", "http://");
    // }
    
    Serial.printf("OTA update requested: %s\n", url.c_str());
    Serial.println("\r\nStarting OTA update...");

    // Disconnect Blynk to not interfere with OTA process
    Blynk.disconnect();

    HTTPClient http;
    http.begin(url);

    // Collect MD5 header if available
    const char* headerkeys[] = { "x-MD5" };
    http.collectHeaders(headerkeys, sizeof(headerkeys)/sizeof(char*));

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET failed, error: %d\n", httpCode);
        http.end();
        Blynk.connect();
        return;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("Content-Length not defined");
        http.end();
        Blynk.connect();
        return;
    }

    Serial.printf("Content-Length: %d bytes\n", contentLength);

    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
        Serial.println("Not enough space to begin OTA");
        http.end();
        Blynk.connect();
        return;
    }

    // Set MD5 hash if provided in headers
    if (http.hasHeader("x-MD5")) {
        String md5 = http.header("x-MD5");
        if (md5.length() == 32) {
            md5.toLowerCase();
            Serial.printf("Expected MD5: %s\n", md5.c_str());
            Update.setMD5(md5.c_str());
        }
    }

    WiFiClient* client = http.getStreamPtr();
    int written = Update.writeStream(*client);
    
    if (written != contentLength) {
        Serial.printf("OTA written %d / %d bytes\n", written, contentLength);
        Update.abort();
        http.end();
        Blynk.connect();
        return;
    }

    if (!Update.end()) {
        Serial.printf("Update error #%d\n", Update.getError());
        http.end();
        Blynk.connect();
        return;
    }

    if (!Update.isFinished()) {
        Serial.println("Update failed.");
        http.end();
        Blynk.connect();
        return;
    }

    Serial.println("=== Update successfully completed. Rebooting.");
    http.end();
    delay(100);
    ESP.restart();
}

//----------------------------------------------
// SETUP FUNCTION
void setup(void) {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting setup...");

    // Pin setup
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    pinMode(YELLOW_PIN, OUTPUT);
    // pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);

    pinMode(SERVO_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    // Buzzer setup
    ledcSetup(2, buzzer_freq, 16);
    ledcAttachPin(BUZZER_PIN, 2);
    // tone(BUZZER_PIN, 200); // Initialize buzzer to off
    // delay(2000);
    // noTone(BUZZER_PIN);

    // myBuzzer.attach(BUZZER_PIN);
    // myBuzzer.writeMicroseconds(BUZZER_PIN, 0); // Turn off buzzer
    
    // Servo setup
    myServo.attach(SERVO_PIN);

    // initial position to 90 degrees
    myServo.write(SERVO_PIN, 90);

    // LCD setup
    int status = lcd.begin(LCD_COLS, LCD_ROWS);
    if (status) {
        Serial.println("LCD initialization failed!");
        hd44780::fatalError(status);
    }

    lcd.noBacklight();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Initializing...");
    delay(1000);

    // Connect to Blynk
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    
    last_event = "Ready";
    updateLCD();
}

//----------------------------------------------
// LOOP FUNCTION
void loop(void) {
    Blynk.run();

    // Non-blocking button handling with debounce
    static bool lastButtonState = LOW;
    static unsigned long lastDebounceTime = 0;
    bool currentButtonState = digitalRead(BUTTON_PIN);
    
    if (currentButtonState != lastButtonState && (millis() - lastDebounceTime) > 100) {
        if (currentButtonState == HIGH) {
            Blynk.virtualWrite(V4, 1);
            last_event = "Btn: pressed";
            Serial.println("Button Pressed!");
            updateLCD();
        } else {
            Blynk.virtualWrite(V4, 0);
            last_event = "Btn: released";
            Serial.println("Button Released!");
            updateLCD();
        }
        lastButtonState = currentButtonState;
        lastDebounceTime = millis();
    }

    // Non-blocking buzzer auto-shutoff (3 seconds max)
    if (buzzer_state && (millis() - buzzer_start_time >= BUZZER_MAX_DURATION)) {
        ledcWriteTone(2, 0);
        // noTone(BUZZER_PIN);
        // myBuzzer.writeMicroseconds(BUZZER_PIN, 0); // Turn off buzzer
        buzzer_state = false;
        last_event = "Buz: timeout";
        Serial.println("Buzzer auto-off after 3s");
        updateLCD();
    }

    // Non-blocking light sensor polling (every 5 seconds)
    if (millis() - last_sensor_read >= SENSOR_INTERVAL) {
        light_value = analogRead(LIGHT_SENSOR_PIN);
        Blynk.virtualWrite(V9, light_value); // Send to Blynk
        Serial.print("Light Sensor: ");
        Serial.println(light_value);
        updateLCD();
        last_sensor_read = millis();
    }
}