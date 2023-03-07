#include <iostream>
#include <string>
#include <cmath>
#include <cstring>

#include <SDL/SDL.h>

using namespace std;

const int SCREEN_W = 1024;
const int SCREEN_H = 640;
const int SCREEN_W_CENTER = SCREEN_W / 2;
const int SCREEN_H_CENTER = SCREEN_H / 2;

const int MAP_W = 20;
const int MAP_H = 20;

const float BLOCK_SIZE = 64;

const float SPRITE_SIZE = 64;

const int NUM_SPRITES = 10;

const float FOV = 60;
const float STEP_ANGLE = FOV / SCREEN_W;
const float PLANE_DIST = ( float )SCREEN_W_CENTER / tan( ( FOV / 2 ) / 180 * 3.14 );

struct sprite_t {
    float x, y;
    SDL_Surface* texture;
};

enum { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ALT, NUM_KEYS };

float Absf( float x ) {
    return x >= 0 ? x : -x;
}

void RotatePoint( float angle, float x0, float y0, float* x1, float* y1 ) {
    *x1 = x0 * cos( angle / 180 * 3.1415 ) - y0 * sin( angle / 180 * 3.1415 );
    *y1 = x0 * sin( angle / 180 * 3.1415 ) + y0 * cos( angle / 180 * 3.1415 );
}

SDL_Surface* LoadSurface( const char* path ) {
    SDL_Surface* loadedSurface = SDL_LoadBMP( path );
    
    if ( loadedSurface == nullptr ) {
        cout << "Unable to load image!\n";
    }
    
    SDL_Surface* optimizedSurface = SDL_DisplayFormat( loadedSurface );
    SDL_FreeSurface( loadedSurface );
    return optimizedSurface;
}

void Clear( SDL_Surface* screen ) {
    memset( screen->pixels, 0, screen->w * screen->h * screen->format->BytesPerPixel );
}

void PutPixel32( SDL_Surface* screen, int x, int y, Uint32 cl ) {
    ( ( Uint32* )screen->pixels )[ y * screen->w + x ] = cl;
}

Uint32 GetPixel32( SDL_Surface* screen, int x, int y ) {
    return ( ( Uint32* )screen->pixels )[ y * screen->w + x ];
}

void VLine( SDL_Surface* screen, int x, int y0, int y1, Uint32 cl ) {
    for ( int y = y0; y <= y1; y++ ) {
        PutPixel32( screen, x, y, cl );
    }
}

void TextureVLine( SDL_Surface* screen, int x, int y0, int y1, int tx, int ty0, int ty1, SDL_Surface* tex ) {
    int startY = y0, endY = y1;
    
    if ( startY < 0 ) { startY = 0; }
    if ( endY > screen->h - 1 ) { endY = screen->h - 1; }
    
    for ( int y = startY; y <= endY; y++ ) {
        float t = ( float )( y - y0 ) / ( float )( y1 - y0 );
        int ty = ty0 + ( ty1 - ty0 ) * t;
        Uint32 cl = GetPixel32( tex, tx, ty );
        PutPixel32( screen, x, y, cl );
    }
}

void TintedTextureVLine( SDL_Surface* screen, int x, int y0, int y1, int tx, int ty0, int ty1, float tintR, float tintG, float tintB, SDL_Surface* tex ) {
    int startY = y0, endY = y1;
    
    if ( startY < 0 ) { startY = 0; }
    if ( endY >= screen->h ) { endY = screen->h - 1; }
    
    for ( int y = startY; y <= endY; y++ ) {
        float t = ( float )( y - y0 ) / ( float )( y1 - y0 );
        int ty = ty0 + ( ty1 - ty0 ) * t;
        Uint32 cl = GetPixel32( tex, tx, ty );
        Uint8 r, g, b;
        SDL_GetRGB( cl, screen->format, &r, &g, &b );
        r *= tintR; g *= tintG; b *= tintB;
        PutPixel32( screen, x, y, SDL_MapRGB( screen->format, r, g, b ) );
    }
}

void TextureRectangle( SDL_Surface* screen, int x0, int y0, int x1, int y1, int tx0, int ty0, int tx1, int ty1, SDL_Surface* tex ) {
    const Uint32 TRANSPARENT_PIXEL = SDL_MapRGB( screen->format, 0, 255, 0 );
    
    int startX = x0, startY = y0, endX = x1, endY = y1;
    
    if ( startX >= screen->w || startY >= screen->h || endX < 0 || endY < 0 ) { return; }
    
    if ( startX < 0 ) { startX = 0; }
    if ( startY < 0 ) { startY = 0; }
    if ( endX >= screen->w ) { endX = screen->w - 1; }
    if ( endY >= screen->h ) { endY = screen->h - 1; }
    
    for ( int y = startY; y <= endY; y++ ) {
        for ( int x = startX; x <= endX; x++ ) {
            float at = ( float )( x - x0 ) / ( float )( x1 - x0 );
            float bt = ( float )( y - y0 ) / ( float )( y1 - y0 );
            
            int tx = tx0 + ( tx1 - tx0 ) * at;
            int ty = ty0 + ( ty1 - ty0 ) * bt;
            
            Uint32 color = GetPixel32( tex, tx, ty );
            
            if ( color == TRANSPARENT_PIXEL ) { continue; }
            
            PutPixel32( screen, x, y, color );
        }
    }
}

void TextureRectangle3D( SDL_Surface* screen, int x0, int y0, int x1, int y1, int tx0, int ty0, int tx1, int ty1, SDL_Surface* tex, float perpDist, float* zBuffer ) {
    const Uint32 TRANSPARENT_PIXEL = SDL_MapRGB( screen->format, 0, 255, 0 );
    
    int startX = x0, startY = y0, endX = x1, endY = y1;
    
    if ( startX >= screen->w || startY >= screen->h || endX < 0 || endY < 0 ) { return; }
    
    if ( startX < 0 ) { startX = 0; }
    if ( startY < 0 ) { startY = 0; }
    if ( endX >= screen->w ) { endX = screen->w - 1; }
    if ( endY >= screen->h ) { endY = screen->h - 1; }
    
    for ( int x = startX; x <= endX; x++ ) {
        if ( zBuffer[ x ] < perpDist ) { continue; }
        
        for ( int y = startY; y <= endY; y++ ) {
            float at = ( float )( x - x0 ) / ( float )( x1 - x0 );
            float bt = ( float )( y - y0 ) / ( float )( y1 - y0 );
            
            int tx = tx0 + ( tx1 - tx0 ) * at;
            int ty = ty0 + ( ty1 - ty0 ) * bt;
            
            Uint32 color = GetPixel32( tex, tx, ty );
            
            if ( color == TRANSPARENT_PIXEL ) { continue; }
            
            PutPixel32( screen, x, y, color );
        }
    }
}

int main( int argc, char** argv ) {
    SDL_Init( SDL_INIT_VIDEO );
    SDL_Surface* screen = SDL_SetVideoMode( SCREEN_W, SCREEN_H, 32, SDL_HWSURFACE );
    SDL_WM_SetCaption( "Ray Caster", nullptr );
    
    SDL_Surface* wallTexture = LoadSurface( "texture1.bmp" );
    SDL_Surface* floorTexture = LoadSurface( "texture2.bmp" );
    SDL_Surface* ceilingTexture = LoadSurface( "texture3.bmp" );
    
    SDL_Surface* spriteTexture = LoadSurface( "sprite.bmp" );
    
    int worldMap[ MAP_W * MAP_H ] = {
            //0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19  x
      /*00*/  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      /*01*/  1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      /*02*/  1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      /*03*/  1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      /*04*/  1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1,
      /*05*/  1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
      /*06*/  1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1,
      /*07*/  1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1,
      /*08*/  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1,
      /*09*/  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1,
      /*10*/  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1,
      /*11*/  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
      /*12*/  1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1,
      /*13*/  1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
      /*14*/  1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
      /*15*/  1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
      /*16*/  1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1,
      /*17*/  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
      /*18*/  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
      /*19*/  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
      /*y*/
    };
    
    sprite_t sprites[ NUM_SPRITES ] = {
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 3, BLOCK_SIZE / 2 + BLOCK_SIZE * 3, spriteTexture },
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 3, BLOCK_SIZE / 2 + BLOCK_SIZE * 5, spriteTexture },
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 4, BLOCK_SIZE / 2 + BLOCK_SIZE * 13, spriteTexture },
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 5, BLOCK_SIZE / 2 + BLOCK_SIZE * 17, spriteTexture },
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 11, BLOCK_SIZE / 2 + BLOCK_SIZE * 10, spriteTexture },
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 12, BLOCK_SIZE / 2 + BLOCK_SIZE * 10, spriteTexture },
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 13, BLOCK_SIZE / 2 + BLOCK_SIZE * 10, spriteTexture },
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 18, BLOCK_SIZE / 2 + BLOCK_SIZE * 2, spriteTexture },
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 18, BLOCK_SIZE / 2 + BLOCK_SIZE * 3, spriteTexture },
        { BLOCK_SIZE / 2 + BLOCK_SIZE * 12, BLOCK_SIZE / 2 + BLOCK_SIZE * 17, spriteTexture }
    };
    
    float playerX = BLOCK_SIZE + BLOCK_SIZE / 2;
    float playerY = BLOCK_SIZE + BLOCK_SIZE / 2;
    float playerDir = 45;
    float xVel = 0, yVel = 0;
    
    float zBuffer[ SCREEN_W ];
    
    float time = 0, oldTime = 0;
    
    bool keys[ NUM_KEYS ] = { false };
    bool run = true;
    
    while ( run ) {
        oldTime = time;
        time = SDL_GetTicks();
        float frameTime = ( time - oldTime ) / 1000;
        
        float moveSpeed = 200 * frameTime;
        float rotSpeed = 200 * frameTime;
        
        SDL_Event event;
        
        while ( SDL_PollEvent( &event ) ) {
            if ( event.type == SDL_QUIT ) {
                run = false;
            } else if ( event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ) {
                if ( event.key.keysym.sym == SDLK_UP ) { keys[ KEY_UP ] = event.type == SDL_KEYDOWN; }
                if ( event.key.keysym.sym == SDLK_DOWN ) { keys[ KEY_DOWN ] = event.type == SDL_KEYDOWN; }
                if ( event.key.keysym.sym == SDLK_LEFT ) { keys[ KEY_LEFT ] = event.type == SDL_KEYDOWN; }
                if ( event.key.keysym.sym == SDLK_RIGHT ) { keys[ KEY_RIGHT ] = event.type == SDL_KEYDOWN; }
                if ( event.key.keysym.sym == SDLK_LALT ) { keys[ KEY_ALT ] = event.type == SDL_KEYDOWN; }
            }
        }
        
        xVel = yVel = 0;
        
        if ( keys[ KEY_UP ] ) {
            xVel += cos( playerDir / 180 * 3.1415 ) * moveSpeed;
            yVel += sin( playerDir / 180 * 3.1415 ) * moveSpeed;
        }
        if ( keys[ KEY_DOWN ] ) {
            xVel += -cos( playerDir / 180 * 3.1415 ) * moveSpeed;
            yVel += -sin( playerDir / 180 * 3.1415 ) * moveSpeed;
        }
        if ( keys[ KEY_LEFT ] && keys[ KEY_ALT ] ) {
            xVel += cos( ( playerDir - 90 ) / 180 * 3.1415 ) * moveSpeed;
            yVel += sin( ( playerDir - 90 ) / 180 * 3.1415 ) * moveSpeed;
        }
        if ( keys[ KEY_RIGHT ] && keys[ KEY_ALT ] ) {
            xVel += cos( ( playerDir + 90 ) / 180 * 3.1415 ) * moveSpeed;
            yVel += sin( ( playerDir + 90 ) / 180 * 3.1415 ) * moveSpeed;
        }
        if ( keys[ KEY_LEFT ] && !keys[ KEY_ALT ] ) {
            playerDir -= rotSpeed;
        }
        if ( keys[ KEY_RIGHT ] && !keys[ KEY_ALT ] ) {
            playerDir += rotSpeed;
        }
        
        playerX += xVel;
        if ( worldMap[ ( int )( playerY / BLOCK_SIZE ) * MAP_W + ( int )( playerX / BLOCK_SIZE ) ] != 0 ) { playerX -= xVel; }
        playerY += yVel;
        if ( worldMap[ ( int )( playerY / BLOCK_SIZE ) * MAP_W + ( int )( playerX / BLOCK_SIZE ) ] != 0 ) { playerY -= yVel; }
        
        Clear( screen );
        
        //ray casting//////////////////////////////////////////
        
        float currentAngle = playerDir - FOV / 2;
        
        for ( int x = 0; x < SCREEN_W; x++ ) {
            float dx = cos( currentAngle / 180 * 3.14 );
            float dy = sin( currentAngle / 180 * 3.14 );
            
            float ax, ay, bx, by;
            float xa, ya;
            float distH, distV;
            
            // horizontal intersection
            if ( dy != 0 ) {
                if ( dy < 0 ) { ay = ( int )( playerY / BLOCK_SIZE ) * BLOCK_SIZE; }
                if ( dy > 0 ) { ay = ( int )( playerY / BLOCK_SIZE ) * BLOCK_SIZE + BLOCK_SIZE; }
    
                float dist = Absf( ay - playerY ) / Absf( dy );
                ax = playerX + dist * dx;
    
                ya = dy > 0 ? BLOCK_SIZE : -BLOCK_SIZE;
                dist = BLOCK_SIZE / Absf( dy );
                xa = dist * dx;
    
                while ( true ) {
                    int mapX = ( int )( ax / BLOCK_SIZE );
                    int mapY = ( int )( ay / BLOCK_SIZE );
      
                    if ( dy < 0 ) { mapY--; }
                    
                    if ( mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H ) {
                        distH = -1;
                        break;
                    } else if ( worldMap[ mapY * MAP_W + mapX ] != 0 ) {
                        distH = sqrt( ( ax - playerX ) * ( ax - playerX ) + ( ay - playerY ) * ( ay - playerY ) );
                        break;
                    }
      
                    ax += xa;
                    ay += ya;
                }
            } else {
                distH = -1;
            }
            //
            // vertical intersection
            if ( dx != 0 ) {
                if ( dx < 0 ) { bx = ( int )( playerX / BLOCK_SIZE ) * BLOCK_SIZE; }
                if ( dx > 0 ) { bx = ( int )( playerX / BLOCK_SIZE ) * BLOCK_SIZE + BLOCK_SIZE; }
    
                float dist = Absf( bx - playerX ) / Absf( dx );
                by = playerY + dist * dy;
    
                xa = dx > 0 ? BLOCK_SIZE : -BLOCK_SIZE;
                dist = BLOCK_SIZE / Absf( dx );
                ya = dist * dy;
    
                while ( true ) {
                    int mapX = ( int )( bx / BLOCK_SIZE );
                    int mapY = ( int )( by / BLOCK_SIZE );
      
                    if ( dx < 0 ) { mapX--; }
      
                    if ( mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H ) {
                        distV = -1;
                        break;
                    } else if ( worldMap[ mapY * MAP_W + mapX ] ) {
                        distV = sqrt( ( bx - playerX ) * ( bx - playerX ) + ( by - playerY ) * ( by - playerY ) );
                        break;
                    }  
      
                    bx += xa;
                    by += ya;
                }
            } else {
                distV = -1;
            }
            //
            
            float idist;
            bool isH;
            //int iMapX, iMapY;
            float ix, iy;
            
            if ( distH != -1 && distV != -1 ) {
                if ( distH < distV ) {
                    //iMapX = hMapX;
                    //iMapY = hMapY;
                    ix = ax;
                    iy = ay;
                    idist = distH;
                    isH = true;
                } else if ( distV < distH ) {
                    //iMapX = vMapX;
                    //iMapY = vMapY;
                    ix = bx;
                    iy = by;
                    idist = distV;
                    isH = false;
                }
            } else if ( distH == -1 ) {
                //iMapX = vMapX;
                //iMapY = vMapY;
                ix = bx;
                iy = by;
                idist = distV;
                isH = false;
            } else if ( distV == -1 ) {
                //iMapX = hMapX;
                //iMapY = hMapY;
                ix = ax;
                iy = ay;
                idist = distH;
                isH = true;
            }
            
            float cs = cos( Absf( playerDir - currentAngle ) / 180 * 3.1415 );
            float projectionDist = idist * cs;
            
            float lineLength = BLOCK_SIZE / projectionDist * PLANE_DIST;
            float drawStart = SCREEN_H_CENTER - lineLength / 2, drawEnd = drawStart + lineLength;
            
            float tintR, tintG, tintB;
            int texX;
            
            if ( isH ) { texX = ix - ( int )( ix / BLOCK_SIZE ) * BLOCK_SIZE; tintR = tintG = tintB = 0.5; }
            else       { texX = iy - ( int )( iy / BLOCK_SIZE ) * BLOCK_SIZE; tintR = tintG = tintB = 1; }
            
            TintedTextureVLine( screen, x, drawStart, drawEnd, texX, 0, 63, tintR, tintG, tintB, wallTexture );
            
            zBuffer[ x ] = projectionDist;
            
            // floor and ceiling
            
            for ( int y = drawEnd + 1; y < SCREEN_H; y++ ) {
                float currentDist = ( PLANE_DIST * ( BLOCK_SIZE / 2 ) ) / ( y - SCREEN_H_CENTER ) / cs;
                float t = currentDist / idist;
                float floorX = playerX + ( ix - playerX ) * t;
                float floorY = playerY + ( iy - playerY ) * t;
                int floorTexX = floorX - ( int )( floorX / BLOCK_SIZE ) * BLOCK_SIZE;
                int floorTexY = floorY - ( int )( floorY / BLOCK_SIZE ) * BLOCK_SIZE;
                
                Uint32 clFloor = GetPixel32( floorTexture, floorTexX, floorTexY );
                Uint32 clCeiling = GetPixel32( ceilingTexture, floorTexX, floorTexY );
                
                //floor
                PutPixel32( screen, x, y, clFloor );
                //ceiling (symmetrical!)
                PutPixel32( screen, x, SCREEN_H - y - 1, clCeiling );       
            }
            
            //
            
            currentAngle += STEP_ANGLE;
        }
        
        ///////////////////////////////////////////////////////
        
        //sprites///////////////
    
        float xs[ NUM_SPRITES ], ys[ NUM_SPRITES ], perpDists[ NUM_SPRITES ];
        
        for ( int i = 0; i < NUM_SPRITES; i++ ) {
            xs[ i ] = sprites[ i ].x - playerX;
            ys[ i ] = sprites[ i ].y - playerY;
            RotatePoint( -playerDir, xs[ i ], ys[ i ], &xs[ i ], &ys[ i ] );
            perpDists[ i ] = xs[ i ];
        }
        
        for ( int n = NUM_SPRITES; n > 1; n-- ) {
            for ( int i = 0; i < n - 1; i++ ) {
                if ( perpDists[ i ] < perpDists[ n - 1 ] ) {
                    swap( perpDists[ n - 1 ], perpDists[ i ] );
                    swap( xs[ n - 1 ], xs[ i ] );
                    swap( ys[ n - 1 ], ys[ i ] );
                    swap( sprites[ n - 1 ], sprites[ i ] );
                }   
            }
        }
        
        for ( int i = 0; i < NUM_SPRITES; i++ ) {
            float x = xs[ i ];
            float y = ys[ i ];
            float perpDist = perpDists[ i ];
            sprite_t sprite = sprites[ i ];
            
            if ( x > 0 ) {
                float scale = 1 / perpDist * PLANE_DIST;
            
                float xProjected = y * scale;
                float size = SPRITE_SIZE * scale;
                
                float screenX = ( xProjected + SCREEN_W_CENTER );
            
                float startX = screenX - size / 2;
                float startY = SCREEN_H_CENTER - size / 2;
            
                TextureRectangle3D( screen,
                                    startX, startY,
                                    startX + size, startY + size,
                                    0, 0,
                                    SPRITE_SIZE - 1, SPRITE_SIZE - 1,
                                    sprite.texture,
                                    perpDist,
                                    zBuffer );
            }
        }
        
        ////////////////////////////////
        
        SDL_Flip( screen );
    }
    
    SDL_FreeSurface( spriteTexture );
    
    SDL_FreeSurface( wallTexture );
    SDL_FreeSurface( floorTexture );
    SDL_FreeSurface( ceilingTexture );
    SDL_Quit();
    return 0;
}
