#include <Arduino.h>

#include "Config.h"
#include "ButtonMatrix.h"
#include "Encoder.h"

#include "InputManager.h"
#include "Display.h"
#include "PendantController.h"
#include "MachineState.h"
#include "JogPlanner.h"
#include "CNCjsClient.h"
#include "MachineMapper.h"


ButtonMatrix matrix;

Encoder encoder;

InputManager input;

Display display;

PendantController controller;

MachineState machineState;

CNCjsClient cnc;

JogPlanner jogPlanner;

MachineMapper machineMapper;


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);


    // --------------------------------------------------------
    // CNCjs
    // --------------------------------------------------------

    cnc.begin(
        machineState
    );


    // --------------------------------------------------------
    // INPUT
    // --------------------------------------------------------

    matrix.begin();

    encoder.begin(
        ENCODER_A_PIN,
        ENCODER_B_PIN,
        ENCODER_BUTTON_PIN
    );

    input.begin(
        matrix,
        encoder
    );


    // --------------------------------------------------------
    // DISPLAY
    // --------------------------------------------------------

    display.begin();


    // --------------------------------------------------------
    // PENDANT
    // --------------------------------------------------------

    controller.begin(
        display,
        machineState,
        cnc
    );


    // --------------------------------------------------------
    // JOG
    // --------------------------------------------------------

    jogPlanner.begin();


    // --------------------------------------------------------
    // MACHINE MAPPER
    // --------------------------------------------------------

    machineMapper.begin();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // CNCjs
    // --------------------------------------------------------

    cnc.update();


    // --------------------------------------------------------
    // SERIAL TEST INPUT
    // --------------------------------------------------------

    /*
        Tijdelijke encoder-emulatie via Serial.

        l = encoder pulse links
        r = encoder pulse rechts

        De injectie gebeurt rechtstreeks in Encoder,
        zodat de rest van de inputketen identiek blijft
        aan de echte hardware.
    */

    if(Serial.available())
    {
        char command =
            Serial.read();


        if(command == 'l')
        {
            encoder.injectPulse(
                -1
            );
        }
        else if(command == 'r')
        {
            encoder.injectPulse(
                1
            );
        }else if(command == 's')
        {
            Serial.println();
            Serial.println(
                "[TEST] Sending CNCjs statusreport"
            );

            bool success =
                cnc.sendCommand(
                    "statusreport"
                );

            Serial.print(
                "[TEST] statusreport: "
            );

            Serial.println(
                success
                    ? "SENT"
                    : "FAILED"
            );
        }
    }


    // --------------------------------------------------------
    // INPUT
    // --------------------------------------------------------

    input.update();

    if(input.available())
    {
        Event event =
            input.read();

        if(event.type == EVENT_ENCODER_PULSE)
        {
            jogPlanner.encoder(event);
        }
        else
        {
            controller.handle(event);
        }
    }

    // --------------------------------------------------------
    // PENDANT CONTROLLER
    // --------------------------------------------------------

    controller.update();


    // --------------------------------------------------------
    // JOG PLANNER
    // --------------------------------------------------------

    /*
        De planner gebruikt MachineState als werkelijkheid.

        update() levert direct één JogCommand terug.
        Als er niets hoeft te gebeuren is het type JOG_NONE.
    */

    JogCommand jog =
        jogPlanner.update(
            machineState
        );


    // --------------------------------------------------------
    // MACHINE MAPPER
    // --------------------------------------------------------

    if(jog.type != JOG_NONE)
    {
        machineMapper.update(
            jog,
            machineState
        );
    }


    // --------------------------------------------------------
    // MACHINE COMMAND
    // --------------------------------------------------------

    if(machineMapper.available())
    {
        MachineCommand command =
            machineMapper.read();


        Serial.println();
        Serial.println(
            "[TEST] MachineCommand received"
        );


        Serial.print(
            " type: "
        );

        Serial.println(
            command.type
        );


        Serial.print(
            " command: "
        );

        Serial.println(
            command.command
        );


        /*
            Nu daadwerkelijk naar CNCjs.
        */

        bool success =
            cnc.execute(
                command
            );


        Serial.print(
            "[CNCjs] execute: "
        );

        Serial.println(
            success
                ? "OK"
                : "FAILED"
        );


        Serial.println();
    }
}