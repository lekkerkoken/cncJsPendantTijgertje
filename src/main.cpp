#include <Arduino.h>

#include "Config.h"

#include "ButtonMatrix.h"
#include "Encoder.h"
#include "OLED.h"

#include "PendantController.h"


// ============================================================
// HARDWARE
// ============================================================

ButtonMatrix matrix;

Encoder encoder;

OLED oled;


// ============================================================
// CONTROLLER
// ============================================================

PendantController controller;


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(
        115200
    );
    


    // --------------------------------------------------------
    // OLED
    // --------------------------------------------------------

    if(
        !oled.begin()
    )
    {
        Serial.println(
            "[OLED] ERROR: initialization failed"
        );
    }
    else
    {
        Serial.println(
            "[OLED] initialized"
        );

        // ----------------------------------------------------
        // Tijdelijke hardwaretest
        // ----------------------------------------------------

        oled.clear();

        oled.setTextSize(
            1
        );

        oled.setTextColor(
            true
        );

        oled.setTextWrap(
            false
        );

        oled.setCursor(
            0,
            0
        );

        oled.print(
            "OLED TEST"
        );

        oled.setCursor(
            0,
            8
        );

        oled.print(
            "XIAO ESP32-S3"
        );

        oled.setCursor(
            0,
            16
        );

        oled.print(
            "SDA 4  SCL 5"
        );

        oled.setCursor(
            0,
            24
        );

        oled.print(
            "SSD1306 128x32"
        );

        oled.display();
    }


    // --------------------------------------------------------
    // INPUT
    // --------------------------------------------------------

    matrix.begin();

    encoder.begin(
        ENCODER_A_PIN,
        ENCODER_B_PIN,
        ENCODER_BUTTON_PIN
    );


    // --------------------------------------------------------
    // PENDANT
    // --------------------------------------------------------

    controller.begin(
        matrix,
        encoder,
        oled
    );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    controller.update();
}