#ifndef BUTTON_MATRIX_H
#define BUTTON_MATRIX_H


class ButtonMatrix
{
public:

    void begin();
    void update();

    bool available();
    int read();

    // Huidige fysieke toestand van de matrix
    int currentKey() const;


private:

    int lastKey = -1;

    int currentKeyState = -1;
};


#endif