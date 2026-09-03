#include "OLED.h"

#include "Config.h"

#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// ============================================================
// SSD1306
// ============================================================

static constexpr uint8_t OLED_WIDTH =
    128;

static constexpr uint8_t OLED_HEIGHT =
    32;

static constexpr uint8_t OLED_ADDRESS =
    0x3C;


static Adafruit_SSD1306 oledDriver(
    OLED_WIDTH,
    OLED_HEIGHT,
    &Wire,
    -1
);


// ============================================================
// BEGIN
// ============================================================

bool OLED::begin()
{
    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );


    if(
        !oledDriver.begin(
            SSD1306_SWITCHCAPVCC,
            OLED_ADDRESS
        )
    )
    {
        return false;
    }


    oledDriver.clearDisplay();

    oledDriver.setTextColor(
        SSD1306_WHITE
    );

    oledDriver.setTextSize(
        1
    );

    oledDriver.setTextWrap(
        false
    );

    oledDriver.display();


    return true;
}


// ============================================================
// CLEAR
// ============================================================

void OLED::clear()
{
    oledDriver.clearDisplay();
}


// ============================================================
// SET CURSOR
// ============================================================

void OLED::setCursor(
    int16_t x,
    int16_t y
)
{
    oledDriver.setCursor(
        x,
        y
    );
}


// ============================================================
// SET TEXT SIZE
// ============================================================

void OLED::setTextSize(
    uint8_t size
)
{
    oledDriver.setTextSize(
        size
    );
}


// ============================================================
// SET TEXT COLOR
// ============================================================

void OLED::setTextColor(
    bool white
)
{
    oledDriver.setTextColor(
        white
            ? SSD1306_WHITE
            : SSD1306_BLACK
    );
}


// ============================================================
// SET TEXT WRAP
// ============================================================

void OLED::setTextWrap(
    bool wrap
)
{
    oledDriver.setTextWrap(
        wrap
    );
}


// ============================================================
// PRINT
// ============================================================

void OLED::print(
    const char* text
)
{
    oledDriver.print(
        text
    );
}


// ============================================================
// DISPLAY
// ============================================================

void OLED::display()
{
    oledDriver.display();
}