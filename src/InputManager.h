#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H


#include "Event.h"
#include "ButtonMatrix.h"
#include "Encoder.h"


class InputManager
{
public:

    void begin(
        ButtonMatrix& matrix,
        Encoder& encoder
    );

    void update();

    bool available();

    Event read();


private:

    ButtonMatrix* matrix = nullptr;
    Encoder* encoder = nullptr;


    Event lastEvent;


    // --------------------------------------------------------
    // Button debounce
    // --------------------------------------------------------

    static constexpr unsigned long DEBOUNCE_TIME = 10;

    int stableKey = -1;

    int candidateKey = -1;

    unsigned long candidateSince = 0;


    // --------------------------------------------------------
    // Event creation
    // --------------------------------------------------------

    Event buttonEvent(int key);

    Event encoderEvent(
        EncoderEvent event
    );
};


#endif