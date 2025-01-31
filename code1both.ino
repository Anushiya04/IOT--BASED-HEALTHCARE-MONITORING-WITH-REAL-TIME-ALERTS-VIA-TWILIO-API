#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <LiquidCrystal_I2C.h>

#define REPORTING_PERIOD_MS 1000
PulseOximeter pox;
uint32_t tsLastReport = 0;

// Initialize the LCD display with I2C address 0x27 and dimensions 16x2
LiquidCrystal_I2C lcd(0x27, 16, 2);

void onBeatDetected() {
    // Serial.println("Beat!");
}

void setup() {
    Serial.begin(57600);

    // Initialize LCD
    lcd.begin();
    lcd.backlight();

    if (!pox.begin()) {
        Serial.println("FAILED");
        lcd.setCursor(0, 0);
        lcd.print("Sensor FAILED");
        for (;;);
    } else {
        Serial.println("SUCCESS");
        lcd.setCursor(0, 0);
        lcd.print("Sensor SUCCESS");
    }

    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    pox.setOnBeatDetectedCallback(onBeatDetected);
}

void loop() {
    pox.update();
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        // Get heart rate and SpO2 readings
        float heartRate = pox.getHeartRate();
        float spO2 = pox.getSpO2();

        // Print to Serial Monitor
        Serial.print("Heart Rate: ");
        Serial.print(heartRate);
        Serial.print(" bpm / SpO2: ");
        Serial.print(spO2);
        Serial.println("%");

        // Display on LCD
        lcd.setCursor(0, 0);
        lcd.print("HR: ");
        lcd.print(heartRate);
        lcd.print(" bpm ");

        lcd.setCursor(0, 1);
        lcd.print("SpO2: ");
        lcd.print(spO2);
        lcd.print(" %    ");

        tsLastReport = millis();
    }
}
