#include "ButtonMatrix.h"
#include "Config.h"

#include <Arduino.h>


const int rows[] =
{
    D8,
    D9,
    D10
};


const int cols[] =
{
    D2,
    D1,
    D0
};


const int ROWS = MATRIX_ROWS;
const int COLS = MATRIX_COLS;



// ============================================================
// BEGIN
// ============================================================

void ButtonMatrix::begin()
{
    for(int r = 0; r < ROWS; r++)
    {
        pinMode(
            MATRIX_ROW_PINS[r],
            OUTPUT
        );

        digitalWrite(
            MATRIX_ROW_PINS[r],
            HIGH
        );
    }


    for(int c = 0; c < COLS; c++)
    {
        pinMode(
            MATRIX_COL_PINS[c],
            INPUT_PULLUP
        );
    }


    lastKey =
        -1;


    currentKeyState =
        -1;
}



// ============================================================
// UPDATE
// ============================================================

void ButtonMatrix::update()
{
    int detectedKey =
        -1;


    for(int r = 0; r < ROWS; r++)
    {
        digitalWrite(
            rows[r],
            LOW
        );


        for(int c = 0; c < COLS; c++)
        {
            if(
                digitalRead(cols[c]) == LOW
            )
            {
                detectedKey =
                    r * COLS + c + 1;

                break;
            }
        }


        digitalWrite(
            rows[r],
            HIGH
        );


        if(detectedKey != -1)
        {
            break;
        }
    }


    currentKeyState =
        detectedKey;


    /*
        lastKey is alleen bedoeld voor
        backwards-compatible available/read.

        InputManager gebruikt voor de
        debounce rechtstreeks currentKey().
    */

    lastKey =
        detectedKey;
}



// ============================================================
// AVAILABLE
// ============================================================

bool ButtonMatrix::available()
{
    return lastKey != -1;
}



// ============================================================
// READ
// ============================================================

int ButtonMatrix::read()
{
    int key =
        lastKey;


    lastKey =
        -1;


    return key;
}



// ============================================================
// CURRENT KEY
// ============================================================

int ButtonMatrix::currentKey() const
{
    return currentKeyState;
}