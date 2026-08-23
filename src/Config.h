#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// Button Matrix
// ============================================================

#define MATRIX_ROWS 3
#define MATRIX_COLS 3

const int MATRIX_ROW_PINS[MATRIX_ROWS] =
{
    7,
    8,
    9
};

const int MATRIX_COL_PINS[MATRIX_COLS] =
{
    3,
    2,
    1
};


// ============================================================
// Encoder
// ============================================================

#define ENCODER_A_PIN       D1
#define ENCODER_B_PIN       D2
#define ENCODER_BUTTON_PIN  D3

#define DISPLAY_DEBUG

#endif