#include "Graphics.hpp"
#include "TextIO.hpp"

Graphics::Graphics() {}

Graphics::~Graphics() {
    shutdown();
}

bool Graphics::init(const std::string& title, int width, int height) {
    if (is_initialized) {
        shutdown(); // Close existing window if any
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        TextIO::print("SDL could not initialize! SDL_Error: " + std::string(SDL_GetError()) + "\n");
        return false;
    }

    std:bool success = SDL_CreateWindowAndRenderer("Hello World", 800, 600, SDL_WINDOW_FULLSCREEN, &window, &renderer);
    if (!success) {
        TextIO::print("Window could not be created! SDL_Error: " + std::string(SDL_GetError()) + "\n");
        return false;
    }

    is_initialized = true;
    clear_screen();
    update_screen();
    return true;
}

void Graphics::shutdown() {
    if (!is_initialized) return;
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
    is_initialized = false;
}

void Graphics::clear_screen() {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black
    SDL_RenderClear(renderer);
}

void Graphics::update_screen() {
    if (!renderer) return;
    SDL_RenderPresent(renderer);
}

bool Graphics::handle_events() {
    if (!is_initialized) return true;

    SDL_Event event;
    // Poll for all pending events
    while (SDL_PollEvent(&event) != 0) {
        // Check if the event is the user trying to close the window
        if (event.type == SDL_EVENT_QUIT) {
            quit_event_received = true;
        }
    }
    return !quit_event_received;
}

bool Graphics::should_quit() {
    return quit_event_received;
}

void Graphics::clear_screen(Uint8 r, Uint8 g, Uint8 b) {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, r, g, b, 255); // Use specified color
    SDL_RenderClear(renderer);
}

void Graphics::pset(int x, int y, Uint8 r, Uint8 g, Uint8 b) {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderPoint(renderer, (float)x, (float)y);
}

void Graphics::line(int x1, int y1, int x2, int y2, Uint8 r, Uint8 g, Uint8 b) {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderLine(renderer, (float)x1, (float)y1, (float)x2, (float)y2);
}

void Graphics::rect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, bool is_filled) {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_FRect rect = { (float)x, (float)y, (float)w, (float)h };
    if (is_filled) {
        SDL_RenderFillRect(renderer, &rect);
    }
    else {
        SDL_RenderRect(renderer, &rect);
    }
}

void Graphics::circle(int center_x, int center_y, int radius, Uint8 r, Uint8 g, Uint8 b) {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);

    // Simple trigonometric algorithm to draw a circle
    for (int i = 0; i < 360; ++i) {
        float angle = i * 3.14159f / 180.0f; // Convert degree to radian
        float x = center_x + radius * cos(angle);
        float y = center_y + radius * sin(angle);
        SDL_RenderPoint(renderer, x, y);
    }
}
