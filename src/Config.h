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

#define ENCODER_A_PIN       43
#define ENCODER_B_PIN       44
#define ENCODER_BUTTON_PIN  6

// ============================================================
// SSD1306 128×32 OLED
// ============================================================

#define SDA_PIN       4
#define SCL_PIN       5

//#define IOC_DEBUG
#define CNCJS_EVENT_DEBUG
//#define SOCKETIO_DEBUG
//#define ENCODER_DEBUG
#define DISPLAY_DEBUG

#endif