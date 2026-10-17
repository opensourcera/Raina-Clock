#ifndef UI_TRIP_H
#define UI_TRIP_H

#include <lvgl.h>

// The Trip tab: a psychedelic clock. The background is a vertical gradient whose hues drift round the colour
// wheel, four translucent blobs wander on Lissajous paths, and the time sits in the middle in the complementary
// colour. It only animates while its tab is the active one.
void createTripTab(lv_obj_t* tab, lv_obj_t* tabview, int tabIndex, bool isLandscape, bool isLargeScreen);
void tripSetTime(const char* time_str);

#endif // UI_TRIP_H
