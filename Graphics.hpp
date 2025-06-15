#pragma once
#include <cstdint>

namespace Graphics {
    // Initializes the graphics system by getting the console window handle.
    void init();

    // Sets a single pixel to a specified color.
    // This is the fundamental drawing operation.
    void pset(int x, int y, int color_index);

    // Draws a line from (x1, y1) to (x2, y2) with a specified color.
    void line(int x1, int y1, int x2, int y2, int color_index);

    // Draws a circle with a center at (x, y), a given radius, and color.
    void circle(int x, int y, int radius, int color_index);

} // namespace Graphics
