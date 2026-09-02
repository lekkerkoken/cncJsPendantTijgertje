#include <Arduino.h>

#include "Config.h"
#include "ButtonMatrix.h"
#include "Encoder.h"

#include "PendantController.h"


ButtonMatrix matrix;

Encoder encoder;

PendantController controller;


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);


    // --------------------------------------------------------
    // INPUT HARDWARE
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
        encoder
    );
}



// ============================================================
// LOOP
// ============================================================

void loop()
{
    controller.update();
}