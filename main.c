// -----------------------------------------------------------------------------
//  worldtweak
//  by Thomas Foster
// -----------------------------------------------------------------------------
#include "mylib/mathlib.h"
#include "mylib/text.h"
#include "mylib/video.h"

#include <SDL2/SDL.h>

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } dir_t;

typedef struct {
    const char * name;

    // this is a pointer because some properties
    // are members of a separate array (e.g. layers)
    float * value; // could also be an int

    int decimal_places; // for display purposes
    float step; // amount by which value increases/decreases when stepped
    float default_value;
} property_t;

SDL_Texture * world;
SDL_Texture * background;
enum { clean, dirty, generating } generation_state;
int generation_ms; // time GenerateWorld() takes, in milliseconds

//
// property list
//

#define NUM_LAYERS (int)(sizeof(layers) / sizeof(layers[0]))

float layers[] = {
    -1.00, // deep ocean
    -0.45, // shallow ocean
    -0.20, // beach
    -0.15, // grass
     0.05, // forest
     0.30, // mountain
     0.70, // snow
};

float world_width = 512;
float world_height = 512;
float world_seed = 0;
float frequency = 0.01f;
float octaves = 6;
float mask_on = 1.0f;

#define NUM_PROPERTIES (int)(sizeof(properties) / sizeof(properties[0]))
int selection;
property_t properties[] = {
    { "World Width",        &world_width,   0,  16,     },
    { "World Height",       &world_height,  0,  16      },
    { "World Seed",         &world_seed,    0,  1       },
    { "Noise Frequency",    &frequency,     3,  0.001f  },
    { "Noise Octaves",      &octaves,       0,  1       },
    { "Deep Ocean",         &layers[0],     2,  0.05f   },
    { "Shallow Ocean",      &layers[1],     2,  0.05f   },
    { "Beach",              &layers[2],     2,  0.05f   },
    { "Grass",              &layers[3],     2,  0.05f   },
    { "Forest",             &layers[4],     2,  0.05f   },
    { "Mountain",           &layers[5],     2,  0.05f   },
    { "Snow",               &layers[6],     2,  0.05f   },
    { "Mask On",            &mask_on,       0,  1      },
};

// TODO: name and define these colors somewhere
SDL_Color layer_colors[] = {
    { 0x00, 0x00,  160, 0xFF },
    {   32,   32,  200, 0xFF },
    { 0xD2, 0xC2, 0x90, 0xFF },
    { 0x22, 0x8B, 0x22, 0xFF },
    { 0x11, 0x60, 0x11, 0xFF },
    {   80,   80,   90, 0xFF },
    {  248,  248,  248, 0xFF },
};

SDL_Rect GetWindowSize(void)
{
    window_info_t info = WindowInfo();
    return (SDL_Rect){ 0, 0, info.width, info.height };
}

float Distance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx*dx + dy*dy);
}

// draw pixels to world texture based on noise value and layer elevations
void GenerateWorld(void)
{
    if ( world != NULL ) {
        SDL_DestroyTexture(world);
        world = NULL;
    }

    world = SDL_CreateTexture
    (   renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        world_width, world_height );

    if ( world == NULL ) {
        puts("failed to create world texture!");
        exit(1);
    }

    SDL_SetTextureBlendMode(world, SDL_BLENDMODE_BLEND);

    SDL_SetRenderTarget(renderer, world);
    SetGray(0);
    Clear();

    int ms = SDL_GetTicks();

    RandomizeNoise((int)world_seed);

    for ( int y = 0; y < world_height; y++ ) {
        for ( int x = 0; x < world_width; x++ ) {

            float dist = Distance(x, y, world_height / 2, world_height / 2);

            float gradient;
            float noise;
            float z = 1.0f;
            if ( dist < world_height / 2 ) {
                gradient = mask_on
                ? MAP(dist, 0.0f, world_height / 2.0f, 0.0f, 1.0f)
                : 0;
                noise = Noise2(x, y, z, frequency, octaves, 1.0, 0.5, 2.0) - gradient;
            } else {
                noise = -1.0f;
            }

            for ( int i = 0; i < NUM_LAYERS - 1; i++ ) {
                if ( noise < layers[i + 1] ) {
                    SetColor(layer_colors[i]);
                    break;
                }
            }

            DrawPoint(x, y);
        }
    }

    generation_ms = SDL_GetTicks() - ms;
    SDL_SetRenderTarget(renderer, NULL);
}

// user pressed up/down/left/right
void ListDirectionKey(dir_t dir)
{
    property_t * p = &properties[selection];

    switch ( dir ) {
        case DIR_DOWN:
            selection = MIN(selection + 1, NUM_PROPERTIES - 1);
            break;
        case DIR_UP:
            selection = MAX(selection - 1, 0);
            break;
        case DIR_RIGHT:
            *p->value += p->step;
            //generation_state = dirty;
            GenerateWorld();
            break;
        case DIR_LEFT:
            *p->value -= p->step;
            //generation_state = dirty;
            GenerateWorld();
            break;
        default:
            break;
    }
}

void PrintLabel(int x, int y, const char * format, ...)
{
    u8 r, g, b, a; // saved, text color
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

    va_list args[2];
    va_start(args[0], format);
    va_copy(args[1], args[0]);

    int len = vsnprintf(NULL, 0, format, args[0]);
    char * buffer = calloc(len + 1, sizeof(char));
    vsnprintf(buffer, len + 1, format, args[1]);
    va_end(args[0]);
    va_end(args[1]);

    // create a neat lil box to go behind the text
    {
        int margin = 6;
        int w = CharWidth() * len + margin * 2;
        int h = CharHeight() + margin * 2;
        SDL_Rect background_rect = { x, y, w, h };

        SetGray(0);
        FillRect(background_rect);

        background_rect.x -= margin;
        background_rect.y -= margin;
        SetGray(32);
        FillRect(background_rect);
    }

    // restore text color
    SetRGBA(r, g, b, a);
    Print(x, y, buffer);

    free(buffer);
}

void DrawPropertyList(void)
{
    int char_h = CharHeight();
    SDL_Rect win_size = GetWindowSize();

    for ( int i = 0; i < NUM_PROPERTIES; i++ ) {
        property_t * p = &properties[i];

        if ( selection == i && *p->value == p->default_value ) {
            SetRGBA(90, 255, 90, 255); // highlight it, default value
        } else if ( selection == i ) {
            SetRGBA(255, 100, 100, 255); // highlight it, normal
        } else {
            SetGray(248);
        }

        PrintLabel
        (   win_size.w - 350,
            (char_h * i) + 16,
            "%s %.*f", p->name, p->decimal_places, *p->value);
    }
}

void MakeBigDumbBackground(void)
{
    const int bg_size = 2000; // probably big enough
    background = CreateTexture(bg_size, bg_size);
    SDL_SetRenderTarget(renderer, background);
    for ( int y = 0; y < bg_size; y++ ) {
        for ( int x = 0; x < bg_size; x++ ) {
            if ( (x + y) % 2 == 0 ) {
                SetGray(64);
            } else {
                SetRGBA(255, 100, 100, 255);
            }
            DrawPoint(x, y);
        }
    }
    SDL_SetRenderTarget(renderer, NULL);
}

void SetUpWindowEtCetera(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(0, &mode);

    window_info_t info = {
        .title = "WorldTweak",
        .width = mode.w * 0.625,
        .height = mode.h * 0.625,
        .flags = SDL_WINDOW_RESIZABLE
    };
    InitWindow(info);
    MakeBigDumbBackground();

    //
    // set up text
    //
    SetTextRenderer(renderer);
    SetFont(FONT_CP437_8X16);
    SetTextScale(2.0f, 2.0f);
}

int main()
{
    puts("worldtweak");
    puts("Perlin noise world generation tweaking tool");
    puts("by Thomas Foster\n");

    // viewCenter is the world coordinate that should
    // appear centered on screen.
    int viewCenterX = world_width / 2;
    int viewCenterY = world_height / 2;
    float scale = 2.0f;

    // init default property values
    for ( int i = 0; i < NUM_PROPERTIES; i++ ) {
        properties[i].default_value = *properties[i].value;
    }

    SetUpWindowEtCetera();
    int char_w = CharWidth();
    int char_h = CharHeight();

    puts("generating world");
    GenerateWorld();

    //
    // program loop
    //
    while ( 1 ) {

        //
        // key input
        //
        SDL_Event ev;
        while ( SDL_PollEvent( &ev ) ) {
            if ( ev.type == SDL_QUIT ) {
                SDL_DestroyTexture(world);
                SDL_Quit();
                return 0;
            } else if ( ev.type == SDL_KEYDOWN ) {
                switch ( ev.key.keysym.sym ) {
                    case SDLK_EQUALS:   scale *= 2.0f; break;
                    case SDLK_MINUS:    scale = MAX(scale / 2, 1.0f); break;
                    case SDLK_DOWN:     ListDirectionKey(DIR_DOWN); break;
                    case SDLK_UP:       ListDirectionKey(DIR_UP); break;
                    case SDLK_RIGHT:    ListDirectionKey(DIR_RIGHT); break;
                    case SDLK_LEFT:     ListDirectionKey(DIR_LEFT); break;
                    case SDLK_RETURN:
                        if ( generation_state == dirty ) {
                            generation_state = generating;
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        //
        // scroll map
        //
        {
            int step = 2;
            const u8 * keyboard = SDL_GetKeyboardState(NULL);

            if ( keyboard[SDL_SCANCODE_W] )
                viewCenterY -= step;

            if ( keyboard[SDL_SCANCODE_S] )
                viewCenterY += step;

            if ( keyboard[SDL_SCANCODE_A] )
                viewCenterX -= step;

            if ( keyboard[SDL_SCANCODE_D] )
                viewCenterX += step;
        }

        //
        // clear and draw background
        //
        SetGray(0);
        Clear();
        SDL_Rect win_size = GetWindowSize();
        SDL_Rect bg_src = { 0, 0, win_size.w / 2, win_size.h / 2 };
        DrawTexture(background, &bg_src, NULL);

        SDL_Rect window_size = GetWindowSize();

        //
        // draw map view
        //
        SDL_Rect dst = {
            .x = (window_size.w / 2) - viewCenterX * scale,
            .y = (window_size.h / 2) - viewCenterY * scale,
            .w = world_width * scale,
            .h = world_height * scale
        };
        DrawTexture(world, NULL, &dst);

        //
        // draw list
        //
        DrawPropertyList();

        //
        // generation status message
        //
        int half_w = window_size.w / 2;
        int half_h = window_size.h / 2;
        switch ( generation_state ) {
            case dirty: {
                int len = 18 * char_w;
                PrintLabel(half_w - len / 2, half_h - char_h / 2, "Regenerate [ENTER]");
                break;
            }
            case generating:
                PrintLabel(window_size.w / 2, window_size.h / 2, "Regenerating...");
                Present();
                GenerateWorld();
                generation_state = clean;
                break;
            default:
                break;
        }

        //
        // draw other handy labels
        //
        SetRGBA(255, 255, 100, 255);
        PrintLabel(16, 16, "Adjust Map: WASD, -/+");
        PrintLabel(16, window_size.h - 48, "Generation Time: %d ms", generation_ms);

        Present();
        SDL_Delay(10);
    }
}
