#include <Arduino.h>

#include "Config.h"
#include "ButtonMatrix.h"
#include "Encoder.h"

#include "InputManager.h"
#include "Display.h"
#include "PendantController.h"
#include "MachineState.h"
#include "JogPlanner.h"
#include "CNCjsInterface.h"
#include "MachineMapper.h"


ButtonMatrix matrix;

Encoder encoder;

InputManager input;

Display display;

PendantController controller;

MachineState machineState;

CNCjsInterface cnc;

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
    // SERIAL TEST INPUT
    // --------------------------------------------------------

    /*
        Tijdelijke testinput via Serial.

        l = encoder pulse links
        r = encoder pulse rechts
        s = CNCjs statusreport
        select <poort> <controller>
            = selecteer CNCjs poort en controllertype

        Voorbeeld:

        select 0 0

        betekent:
            poort      0
            controller 0
    */

    if(Serial.available())
    {
        String command =
            Serial.readStringUntil(
                '\n'
            );

        command.trim();


        if(command == "l")
        {
            encoder.injectPulse(
                -1
            );
        }
        else if(command == "r")
        {
            encoder.injectPulse(
                1
            );
        }
        else if(command == "s")
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
        else if(
            command.startsWith(
                "select "
            )
        )
        {
            int separator =
                command.indexOf(
                    ' ',
                    7
                );


            if(separator > 0)
            {
                String portText =
                    command.substring(
                        7,
                        separator
                    );

                String controllerText =
                    command.substring(
                        separator + 1
                    );


                int portIndex =
                    portText.toInt();

                int controllerIndex =
                    controllerText.toInt();


                bool success =
                    cnc.selectController(
                        portIndex,
                        controllerIndex
                    );


                Serial.print(
                    "[TEST] select: "
                );

                Serial.println(
                    success
                        ? "OK"
                        : "FAILED"
                );
            }
        }
    }


    // --------------------------------------------------------
    // MACHINE STATE
    // --------------------------------------------------------

    /*
        CNCjsClientCore bezit de werkelijke MachineState.

        De applicatie gebruikt hiervan een lokale snapshot.

        Deze snapshot is de enige MachineState die door
        PendantController, JogPlanner en MachineMapper wordt
        gebruikt.

        De Core blijft eigenaar van zijn eigen state.
    */

machineState =
    cnc.machineStateSnapshot();


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
            jogPlanner.encoder(
                event,
                controller.axis()
            );
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
        De planner gebruikt de actuele MachineState-snapshot
        als werkelijkheid.

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

            execute() plaatst G-code in de bestaande pending
            command. De network task verstuurt deze vervolgens.
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