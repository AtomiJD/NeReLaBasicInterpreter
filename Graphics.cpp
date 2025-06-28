#ifdef SDL3
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

    if (TTF_Init() == -1) {
        TextIO::print("SDL_ttf could not initialize! SDL_Error: " + std::string(SDL_GetError()) + "\n");
        SDL_Quit();
        return false;
    }

    bool success = SDL_CreateWindowAndRenderer(title.c_str(), width, height, 0, &window, &renderer); //SDL_WINDOW_FULLSCREEN removed
    if (!success) {
        TextIO::print("Window could not be created! SDL_Error: " + std::string(SDL_GetError()) + "\n");
        return false;
    }

    font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 24);
    if (!font) {
        // Use TextIO for consistency, but also cerr for visibility during debugging
        std::cerr << "Failed to load font! SDL_Error: " << SDL_GetError() << std::endl;
        TextIO::print("WARNING: Failed to load font. TEXT command will not work.\n");
    }

    is_initialized = true;
    clear_screen();
    update_screen();
    return true;
}

void Graphics::shutdown() {
    if (!is_initialized) return;

    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }

    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    TTF_Quit();
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

void Graphics::text(int x, int y, const std::string& text_to_draw, Uint8 r, Uint8 g, Uint8 b) {
    if (!renderer || !font) {
        // Don't try to draw if the system isn't ready or the font failed to load
        return;
    }

    SDL_Color color = { r, g, b, 255 };

    // Create a surface from the text using the loaded font
    SDL_Surface* text_surface = TTF_RenderText_Blended(font, text_to_draw.c_str(), 0, color);
    if (!text_surface) {
        std::cerr << "Unable to render text surface! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }

    // Create a texture from the surface
    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    if (!text_texture) {
        std::cerr << "Unable to create texture from rendered text! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroySurface(text_surface);
        return;
    }

    // Define the destination rectangle for the text
    SDL_FRect dest_rect = { (float)x, (float)y, (float)text_surface->w, (float)text_surface->h };

    // Copy the texture to the renderer at the specified position
    SDL_RenderTexture(renderer, text_texture, nullptr, &dest_rect);

    // Clean up the temporary resources
    SDL_DestroyTexture(text_texture);
    SDL_DestroySurface(text_surface);
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
#endif
