#include "terminal.hpp"
#include <unistd.h>  // For usleep()

int main() {
    // Create a Terminal instance of size 80x24.
    term::Terminal termInstance(80, 24);

    // Starting positions and movement directions.
    // String 1 moves horizontally.
    int x1 = 0, y1 = 5;
    int dx1 = 1;  // moving right initially

    // String 2 moves vertically.
    int x2 = 40, y2 = 0;
    int dy2 = 1;  // moving downward initially

    int width1 = 3;  // Length of "aló"
    int width2 = 2;  // Length of "λδ"

    // Animation loop: run for 200 frames.
    for (int frame = 0; frame < 200; ++frame) {
        // Clear the front buffer.
        termInstance.clear();

        // Place strings at their current positions with styles
        termInstance.putString(x1, y1, "aló", term::Blue, term::Default, term::STYLE_BOLD + term::STYLE_UNDERLINE);
        termInstance.putString(x2, y2, "λδ",  term::Black,term::Green, 0);

        // Refresh the terminal (updates only changed cells).
        termInstance.refresh();

        // Update string 1's horizontal position.
        x1 += dx1;
        if (x1 < 0 || x1 + width1 >= 80) {
            dx1 = -dx1;  // bounce off left/right edges
            x1 += dx1;
        }

        // Update string 2's vertical position.
        y2 += dy2;
        if (y2 < 0 || y2 >= 24) {
            dy2 = -dy2;  // bounce off top/bottom edges
            y2 += dy2;
        }

        usleep(10000);  // Delay 100 ms between frames.
    }

    return 0;
}
