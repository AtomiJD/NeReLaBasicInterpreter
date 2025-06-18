#pragma once
#ifdef SDL3
#include <SDL3/SDL.h>
#include <string>
#include <vector>

class Graphics {
public:
    Graphics();
    ~Graphics();

    bool init(const std::string& title, int width, int height);
    void shutdown();
    void update_screen(); // Shows whatever has been drawn
    void clear_screen();  // Clears the screen to the default color
    void clear_screen(Uint8 r, Uint8 g, Uint8 b); 
    void pset(int x, int y, Uint8 r, Uint8 g, Uint8 b); 
    void line(int x1, int y1, int x2, int y2, Uint8 r, Uint8 g, Uint8 b);
    void rect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, bool is_filled);
    void circle(int center_x, int center_y, int radius, Uint8 r, Uint8 g, Uint8 b);


    bool handle_events(); 
    bool should_quit();   
    bool is_initialized = false;
    bool quit_event_received = false;

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
};
#endif