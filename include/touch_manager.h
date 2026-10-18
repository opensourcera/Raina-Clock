#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

class TouchManager {
public:
    static void begin();
    static void reattach();   // take the touch panel's SPI pins back (the speaker DAC borrows GPIO 25, its clock)
    static bool isTouched();
    static bool getTouchPoint(int& x, int& y);
    static bool isCapacitive();
};

#if defined(NATIVE_TEST)
void setMockTouch(bool touched, int x, int y);
#endif

#endif // TOUCH_MANAGER_H
