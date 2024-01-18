//
//  main.h
//  PDMode7
//
//  Created by Matteo D'Ignazio on 18/11/23.
//

#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"
#include "pd_mode7.h"

#ifdef _WINDLL
#define _USE_MATH_DEFINES
#endif
#include <math.h>

static int update(void* userdata);
static void restartWorld(void);
static void restartCallback(void *userdata);
static PDMode7_World *newWorld(void);

static PDMode7_World *world;
static PlaydateAPI *playdate;
static LCDBitmapTable *carTable;
static PDMode7_Bitmap *bitmap;
static PDMode7_Sprite *sprite;
static LCDBitmap *backgroundImage;

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI *pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed
    
	if(event == kEventInit)
    {
        playdate = pd;
        PDMode7_init(pd, 0);
        
        mode7->pool->realloc(5000 * 1000);
        
        playdate->display->setRefreshRate(0);
        
        carTable = playdate->graphics->loadBitmapTable("images/full-car", NULL);
        
        world = newWorld();
        
        playdate->system->addMenuItem("Restart", restartCallback, NULL);
        
        // Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
        playdate->system->setUpdateCallback(update, playdate);
	}
	
	return 0;
}

PDMode7_World *newWorld(void)
{
    // Clear the pool before loading a new PGM
    mode7->pool->clear();
    
    PDMode7_WorldConfiguration configuration = mode7->world->defaultConfiguration();
    configuration.width = 2048;
    configuration.height = 2048;
    configuration.depth = 2048;
    
    world = mode7->world->newWorld(configuration);
    
    bitmap = mode7->bitmap->loadPGM("images/track-0.pgm");
    mode7->world->setPlaneBitmap(world, bitmap);
    mode7->world->setPlaneFillColor(world, mode7->color->newGrayscaleColor(60, 255));

    PDMode7_Display *display = mode7->world->getMainDisplay(world);
    PDMode7_Camera *camera = mode7->display->getCamera(display);
    
    //mode7->display->setRect(display, 0, 0, LCD_ROWS, LCD_COLUMNS);
    //mode7->display->setOrientation(display, kMode7DisplayOrientationPortrait);
    //mode7->camera->setFOV(camera, M_PI_2 / 2);
    
    backgroundImage = playdate->graphics->loadBitmap("images/background", NULL);

    PDMode7_Background *background = mode7->display->getBackground(display);
    mode7->background->setBitmap(background, backgroundImage);
    
    mode7->camera->setPosition(camera, 219, 1200, 12);
    mode7->camera->setAngle(camera, -M_PI_2);
    mode7->camera->setClipDistanceUnits(camera, 4);

    sprite = mode7->sprite->newSprite(10, 10, 3);
    
    mode7->sprite->setPosition(sprite, 247, 1106, 1.5);
    mode7->sprite->setBitmapTable(sprite, carTable);
    mode7->sprite->setImageCenter(sprite, 0.5, 0.3);
    mode7->sprite->setAngle(sprite, -M_PI_2);
    mode7->sprite->setAlignment(sprite, kMode7SpriteAlignmentOdd, kMode7SpriteAlignmentOdd);

    mode7->sprite->dataSource->setMinimumWidth(sprite, 4);
    mode7->sprite->dataSource->setMaximumWidth(sprite, 160);
    
    mode7->sprite->dataSource->setLengthForKey(sprite, 40, kMode7SpriteDataSourceScale);
    mode7->sprite->dataSource->setLengthForKey(sprite, 36, kMode7SpriteDataSourceAngle);

    mode7->world->addSprite(world, sprite);

    return world;
}

static int update(void *userdata)
{
    PlaydateAPI *playdate = userdata;
    
    float dt = playdate->system->getElapsedTime();
    playdate->system->resetElapsedTime();
    
    PDButtons pressed; PDButtons pushed;
    playdate->system->getButtonState(&pressed, &pushed, NULL);
    
    PDMode7_Display *display = mode7->world->getMainDisplay(world);
    PDMode7_Camera *camera = mode7->display->getCamera(display);
    
    float angle = mode7->camera->getAngle(camera);
    PDMode7_Vec3 position = mode7->camera->getPosition(camera);
    
    float angleDelta = 1 * dt;

    if(pressed & kButtonLeft)
    {
        angle -= angleDelta;
    }
    else if(pressed & kButtonRight)
    {
        angle += angleDelta;
    }
    
    float moveDelta = 100 * dt;
    float moveVelocity = 0;
    
    if(pressed & kButtonUp)
    {
        moveVelocity = moveDelta;
    }
    else if(pressed & kButtonDown)
    {
        moveVelocity = -moveDelta;
    }
    
    float heightDelta = 60 * dt;
    float height = position.z;

    if(pressed & kButtonA)
    {
        height += heightDelta;
    }
    else if(pressed & kButtonB)
    {
        height -= heightDelta;
    }
    
    mode7->camera->setAngle(camera, angle);
    mode7->camera->setPitch(camera, mode7->camera->getPitch(camera) + playdate->system->getCrankChange() * 0.005f);

    float cameraX = position.x + moveVelocity * cosf(angle);
    float cameraY = position.y + moveVelocity * sinf(angle);

    mode7->camera->setPosition(camera, cameraX, cameraY, height);

    playdate->graphics->clear(kColorWhite);

    mode7->world->update(world);

    mode7->world->draw(world, NULL);
    
    playdate->system->drawFPS(0, 0);
    
	return 1;
}

void restartWorld(void)
{
    if(sprite)
    {
        mode7->sprite->freeSprite(sprite);
    }
    
    playdate->graphics->freeBitmap(backgroundImage);
    mode7->bitmap->freeBitmap(bitmap);
    mode7->world->freeWorld(world);
    
    world = newWorld();
}

void restartCallback(void *userdata)
{
    restartWorld();
}
