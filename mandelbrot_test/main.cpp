#include <cinttypes>
#include <SDL2/SDL.h>
#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 1000

struct RenderJob {
    int x0, y0;   // top-left pixel
    int w, h;     // tile size

    double cx, cy; // center of view
    double scale;  // zoom level

    int width, height; // screen size

    uint32_t* framebuffer; // shared output buffer
};

int main( int argc, char* args[] )
{
    //The window we'll be rendering to
    SDL_Window* window = NULL;
    
    //The surface contained by the window
    SDL_Surface* screenSurface = NULL;

    //Initialize SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
    }
    else
    {
        //Create window
        window = SDL_CreateWindow( "SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
        if( window == NULL )
        {
            printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        }
        else
        {
            //Get window surface
            screenSurface = SDL_GetWindowSurface( window );

            //Fill the surface white
            SDL_FillRect( screenSurface, NULL, SDL_MapRGB( screenSurface->format, 0xFF, 0xFF, 0xFF ) );
            
            //Update the surface
            SDL_UpdateWindowSurface( window );

            SDL_Event e; bool quit = false;
            while( quit == false )
            { 
                while( SDL_PollEvent( &e ) )
                {
                    switch (e.type)
                    {
                    case SDL_QUIT:
                        /* code */
                        quit = true;
                        break;
                    default:
                        break;
                    }
                } 
            }
        }
    }
    //Destroy window
    SDL_DestroyWindow( window );

    //Quit SDL subsystems
    SDL_Quit();

    return 0;
}
