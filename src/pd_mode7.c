//
//  pd_mode7.c
//  PDMode7
//
//  Created by Matteo D'Ignazio on 18/11/23.
//

#include "pd_mode7.h"
#ifdef _WINDLL
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <ctype.h>

#define MODE7_MAX_DISPLAYS 4
#define MODE7_SPRITE_DSOURCE_LEN 4

PDMode7_API *mode7;
static PlaydateAPI *playdate;

typedef struct {
    char *ptr;
    char *originalPtr;
} _PDMode7_Pool;

typedef struct {
    unsigned int x, y;
} PDMode7_Vec2ui;

typedef struct {
    void **items;
    int length;
} _PDMode7_Array;

typedef enum {
    kPDMode7LuaItemSprite,
    kPDMode7LuaItemSpriteInstance,
    kPDMode7LuaItemBitmapLayer
} kPDMode7LuaItem;

typedef struct {
    _PDMode7_Array *array;
    kPDMode7LuaItem type;
    int freeArray;
} _PDMode7_LuaArray;

typedef struct {
    LCDBitmapTable *LCDBitmapTable;
    LuaUDObject *luaRef;
} _PDMode7_LuaBitmapTable;

typedef struct {
    LCDBitmap *LCDBitmap;
    LuaUDObject *luaRef;
} _PDMode7_LuaBitmap;

typedef struct {
    _PDMode7_Array *references;
} _PDMode7_GC;

typedef struct {
    LuaUDObject *luaRef;
    int count;
} _PDMode7_GCRef;

typedef enum {
    _PDMode7_CallbackTypeC,
    _PDMode7_CallbackTypeLua
} _PDMode7_CallbackType;

typedef struct {
    _PDMode7_CallbackType type;
    void *cFunction;
    char *luaFunction;
} _PDMode7_Callback;

typedef struct {
    PDMode7_Sprite *sprite;
} _PDMode7_LuaSpriteDataSource;

typedef struct {
    PDMode7_SpriteInstance *instance;
    int minimumWidth;
    int maximumWidth;
    unsigned int lengths[MODE7_SPRITE_DSOURCE_LEN];
    PDMode7_SpriteDataSourceKey layoutKeys[MODE7_SPRITE_DSOURCE_LEN];
} _PDMode7_SpriteDataSource;

typedef struct {
    PDMode7_Sprite *sprite;
    int index;
    int visible;
    PDMode7_Vec2ui roundingIncrement;
    PDMode7_Vec2 imageCenter;
    PDMode7_SpriteAlignment alignmentX;
    PDMode7_SpriteAlignment alignmentY;
    LCDBitmap *bitmap;
    PDMode7_Rect displayRect;
    float depth;
    unsigned int frame;
    PDMode7_Vec2 billboardSize;
    PDMode7_SpriteBillboardSizeBehavior billboardSizeBehavior;
    PDMode7_SpriteDataSource *dataSource;
    LCDBitmapTable *bitmapTable;
    _PDMode7_LuaBitmapTable *luaBitmapTable;
    _PDMode7_Callback *drawCallback;
    void *userdata;
} _PDMode7_SpriteInstance;

typedef struct {
    PDMode7_World *world;
    PDMode7_Vec3 size;
    PDMode7_Vec3 position;
    float angle;
    float pitch;
    PDMode7_SpriteInstance *instances[MODE7_MAX_DISPLAYS];
    _PDMode7_Array *gridCells;
    LuaUDObject *luaRef;
    _PDMode7_LuaSpriteDataSource *luaDataSource;
} _PDMode7_Sprite;

typedef struct {
    _PDMode7_Array *sprites;
} _PDMode7_GridCell;

typedef struct _PDMode7_Grid {
    _PDMode7_GridCell **cells;
    int cellSize;
    int widthLen;
    int heightLen;
    int depthLen;
    int numberOfCells;
} _PDMode7_Grid;

typedef struct {
    LCDBitmap *bitmap;
    _PDMode7_LuaBitmap *luaBitmap;
    int width;
    int height;
    PDMode7_Vec2 center;
    PDMode7_Vec2ui roundingIncrement;
    PDMode7_Display *display;
} _PDMode7_Background;

typedef struct {
    int width;
    int height;
    int depth;
    PDMode7_Display *mainDisplay;
    PDMode7_Display* displays[MODE7_MAX_DISPLAYS];
    int numberOfDisplays;
    PDMode7_Camera *mainCamera;
    PDMode7_Bitmap *planeBitmap;
    PDMode7_Color planeFillColor;
    PDMode7_Bitmap *ceilingBitmap;
    PDMode7_Color ceilingFillColor;
    _PDMode7_Array *sprites;
    _PDMode7_Grid *grid;
} _PDMode7_World;

typedef struct {
    float angle;
    PDMode7_Vec3 position;
    float fov;
    float pitch;
    int clipDistanceUnits;
    int isManaged;
    LuaUDObject *luaRef;
} _PDMode7_Camera;

typedef struct {
    PDMode7_World *world;
    PDMode7_Rect rect;
    PDMode7_Rect absoluteRect;
    PDMode7_DisplayOrientation orientation;
    PDMode7_DisplayFlipMode flipMode;
    LCDBitmap *secondaryFramebuffer;
    PDMode7_Camera *camera;
    PDMode7_DisplayScale scale;
    _PDMode7_Array *visibleInstances;
    PDMode7_Background *background;
    int isManaged;
    LuaUDObject *luaRef;
} _PDMode7_Display;

typedef struct {
    uint8_t *data;
    size_t size;
    PDMode7_Rect rect;
    int offsetX;
    int offsetY;
} _PDMode7_BitmapLayerCompositing;

typedef struct {
    PDMode7_Rect rect;
    int enabled;
    int canRestore;
    int visible;
    PDMode7_Bitmap *bitmap;
    PDMode7_Bitmap *parentBitmap;
    _PDMode7_BitmapLayerCompositing comp;
    LuaUDObject *luaRef;
} _PDMode7_BitmapLayer;

typedef struct _PDMode7_Bitmap {
    uint8_t *data;
    int width;
    int height;
    PDMode7_Bitmap *mask;
    _PDMode7_Array *layers;
    LuaUDObject *luaRef;
    int released;
} _PDMode7_Bitmap;

typedef struct {
    PDMode7_Vec2 screenRatio;
    float worldScaleInv;
    int planeHeight;
    int horizon;
    PDMode7_Vec2 halfFov;
    PDMode7_Vec2 tanHalfFov;
    PDMode7_Vec2 fovRatio;
    PDMode7_Vec3 forwardVec;
    PDMode7_Vec3 rightVec;
    PDMode7_Vec3 upVec;
} _PDMode7_Parameters;

static _PDMode7_Pool *pool;
static _PDMode7_GC *gc;

static uint8_t patterns4x4[17][4] = {
    { 0b00000000, 0b00000000, 0b00000000, 0b00000000 },
    { 0b10001000, 0b00000000, 0b00000000, 0b00000000 },
    { 0b10001000, 0b00000000, 0b00100010, 0b00000000 },
    { 0b10101010, 0b00000000, 0b00100010, 0b00000000 },
    { 0b10101010, 0b00000000, 0b10101010, 0b00000000 },
    { 0b10101010, 0b01000100, 0b10101010, 0b00000000 },
    { 0b10101010, 0b01000100, 0b10101010, 0b00010001 },
    { 0b10101010, 0b01010101, 0b10101010, 0b00010001 },
    { 0b10101010, 0b01010101, 0b10101010, 0b01010101 },
    { 0b11101110, 0b01010101, 0b10101010, 0b01010101 },
    { 0b11101110, 0b01010101, 0b10111011, 0b01010101 },
    { 0b11111111, 0b01010101, 0b10111011, 0b01010101 },
    { 0b11111111, 0b01010101, 0b11111111, 0b01010101 },
    { 0b11111111, 0b11011101, 0b11111111, 0b01010101 },
    { 0b11111111, 0b11011101, 0b11111111, 0b01110111 },
    { 0b11111111, 0b11111111, 0b11111111, 0b01110111 },
    { 0b11111111, 0b11111111, 0b11111111, 0b11111111 }
};

static inline int mode7_min(const int a, const int b)
{
    return a < b ? a : b;
}

static inline int mode7_max(const int a, const int b)
{
    return a > b ? a : b;
}

static PDMode7_WorldConfiguration defaultWorldConfiguration(void);
static PDMode7_Camera* newCamera(void);
static PDMode7_Display* newDisplay(int x, int y, int width, int height);
static void displaySetRect(PDMode7_Display *display, int x, int y, int width, int height);
static void displaySetOrientation(PDMode7_Display *display, PDMode7_DisplayOrientation orientation);
static int displayNeedsClip(PDMode7_Display *display);
static int indexForDisplay(PDMode7_World *world, PDMode7_Display *display);
static int addDisplay(PDMode7_World *world, PDMode7_Display *display);
static inline PDMode7_Rect newRect(int x, int y, int width, int height);
static int rectIntersect(PDMode7_Rect rectA, PDMode7_Rect rectB);
static void rectAdjust(PDMode7_Rect rect, int width, int height, PDMode7_Rect *adjustedRect, int *offsetX, int *offsetY);
static void displaySetCamera(PDMode7_Display *display, PDMode7_Camera *camera);
static void cameraSetAngle(PDMode7_Camera *camera, float angle);
static void cameraSetPitch(PDMode7_Camera *camera, float pitch);
static inline PDMode7_Vec2 newVec2(float x, float y);
static inline PDMode7_Vec3 newVec3(float x, float y, float z);
static inline PDMode7_Vec2ui newVec2ui(unsigned int x, unsigned int y);
static float worldGetRelativeAngle(PDMode7_Vec3 cameraPoint, float cameraAngle, PDMode7_Vec3 targetPoint, float targetAngle, _PDMode7_Parameters *parameters);
static float worldGetRelativePitch(PDMode7_Vec3 cameraPoint, float cameraPitch, PDMode7_Vec3 targetPoint, float targetPitch, _PDMode7_Parameters *p);
static int displayGetHorizon(PDMode7_Display *pDisplay);
static PDMode7_Vec3 displayToPlanePoint(PDMode7_Display *pDisplay, int displayX, int displayY, _PDMode7_Parameters *p);
static PDMode7_Vec3 worldToDisplayPoint(PDMode7_Display *pDisplay, PDMode7_Vec3 point, _PDMode7_Parameters *p);
static PDMode7_Vec3 displayMultiplierForScanlineAt(PDMode7_Display *pDisplay, PDMode7_Vec3 point, _PDMode7_Parameters *p);
static inline uint8_t worldSampleColor(PDMode7_Bitmap *pBitmap, int x, int y, uint8_t fillColor);
static inline void worldSetColor(uint8_t color, PDMode7_DisplayScale displayScale, uint8_t *ptr, int rowbytes, int bitPosition, int y, int dirY);
static void getDisplayScaleStep(PDMode7_DisplayScale scale, int *xStep, int *yStep);
static inline PDMode7_DisplayScale truncatedDisplayScale(PDMode7_DisplayScale scale);
static PDMode7_Camera* displayGetCamera(PDMode7_Display *display);
static void displayGetFramebuffer(PDMode7_Display *display, LCDBitmap **target, uint8_t **framebuffer, int *rowbytes);
static _PDMode7_Parameters worldGetParameters(PDMode7_Display *pDisplay);
static float worldGetScaleInv(PDMode7_World *pWorld);
static float worldGetScale(PDMode7_World *pWorld);
static PDMode7_Display* getDisplay(PDMode7_World *pWorld, PDMode7_Display *pDisplay);
static PDMode7_Vec2 backgroundGetOffset(PDMode7_Background *pBackground, _PDMode7_Parameters *parameters);
static void spriteSetPosition(PDMode7_Sprite *sprite, float x, float y, float z);
static void spriteBoundsDidChange(PDMode7_Sprite *sprite);
static unsigned int spriteGetTableIndex(PDMode7_SpriteInstance *pInstance, unsigned int angleIndex, unsigned int pitchIndex, int unsigned scaleIndex);
static void removeSprite(PDMode7_Sprite *pSprite);
static int sortSprites(const void *a, const void *b);
static float degToRad(float degrees);
static float roundToIncrement(float number, unsigned int multiple);
static PDMode7_Vec3 vec3_subtract(PDMode7_Vec3 v1, PDMode7_Vec3 v2);
static float vec3_dot(PDMode7_Vec3 v1, PDMode7_Vec3 v2);
static inline PDMode7_Color newGrayscaleColor(uint8_t gray, uint8_t alpha);
static _PDMode7_Grid* newGrid(float width, float height, float depth, int cellSize);
static void gridRemoveSprite(_PDMode7_Grid *grid, PDMode7_Sprite *pSprite);
static void gridUpdateSprite(_PDMode7_Grid *grid, PDMode7_Sprite *pSprite);
static _PDMode7_Array* gridGetSpritesAtPoint(_PDMode7_Grid *grid, PDMode7_Vec3 point, int distanceUnits);
static void releaseBitmap(PDMode7_Bitmap *pBitmap);
static void bitmapLayerSetBitmap(PDMode7_BitmapLayer *layer, PDMode7_Bitmap *bitmap);
static void bitmapLayerDidChange(PDMode7_BitmapLayer *pLayer);
static void bitmapLayerDraw(PDMode7_BitmapLayer *pLayer);
static void _removeBitmapLayer(PDMode7_BitmapLayer *layer, int restore);
static void removeBitmapLayer(PDMode7_BitmapLayer *layer);
static _PDMode7_GCRef* GC_retain(LuaUDObject *luaRef);
static void GC_release(LuaUDObject *luaRef);
static _PDMode7_Callback* newCallback_c(void *function);
static _PDMode7_Callback* newCallback_lua(const char *functionName);
static void freeCallback(_PDMode7_Callback *callback);
static _PDMode7_Array* newArray(void);
static void arrayPush(_PDMode7_Array *array, void *item);
static void arrayRemove(_PDMode7_Array *array, int index);
static int arrayIndexOf(_PDMode7_Array *array, void *item);
static void arrayClear(_PDMode7_Array *array);
static void freeArray(_PDMode7_Array *array);
static void freeCamera(PDMode7_Camera *camera);
static void removeDisplay(PDMode7_Display *display);
static void releaseDisplay(PDMode7_Display *display);
static void freeDisplay(PDMode7_Display *display);
static void freeGrid(_PDMode7_Grid *grid);
static void lua_spriteCallDrawCallback(PDMode7_SpriteInstance *pInstance);

static PDMode7_World* worldWithConfiguration(PDMode7_WorldConfiguration configuration)
{
    PDMode7_World *world = playdate->system->realloc(NULL, sizeof(PDMode7_World));
    
    _PDMode7_World *_world = playdate->system->realloc(NULL, sizeof(_PDMode7_World));
    world->prv = _world;
    
    _world->width = configuration.width;
    _world->height = configuration.height;
    _world->depth = configuration.depth;

    _world->mainCamera = newCamera();
    ((_PDMode7_Camera*)_world->mainCamera->prv)->isManaged = 1;
    
    _world->planeBitmap = NULL;
    _world->planeFillColor = newGrayscaleColor(255, 255);
    _world->ceilingBitmap = NULL;
    _world->ceilingFillColor = newGrayscaleColor(255, 255);
    _world->sprites = newArray();
    _world->numberOfDisplays = 0;
    
    _world->mainDisplay = newDisplay(0, 0, LCD_COLUMNS, LCD_ROWS);
    ((_PDMode7_Display*)_world->mainDisplay->prv)->isManaged = 1;
    addDisplay(world, _world->mainDisplay);
    
    displaySetCamera(_world->mainDisplay, _world->mainCamera);
    
    _world->grid = newGrid(configuration.width, configuration.height, configuration.depth, configuration.gridCellSize);
    
    return world;
}

static PDMode7_World* worldWithParameters(float width, float height, float depth, int gridCellSize)
{
    PDMode7_WorldConfiguration configuration = defaultWorldConfiguration();
    
    if(width > 0)
    {
        configuration.width = width;
    }
    if(height > 0)
    {
        configuration.height = height;
    }
    if(depth > 0)
    {
        configuration.depth = depth;
    }
    if(gridCellSize > 0)
    {
        configuration.gridCellSize = gridCellSize;
    }
    return worldWithConfiguration(configuration);
}

static PDMode7_WorldConfiguration defaultWorldConfiguration(void)
{
    return (PDMode7_WorldConfiguration){
        .width = 1024,
        .height = 1024,
        .depth = 1024,
        .gridCellSize = 256
    };
}

static _PDMode7_Parameters worldGetParameters(PDMode7_Display *pDisplay)
{
    _PDMode7_Display *display = pDisplay->prv;
    PDMode7_World *pWorld = display->world;
    _PDMode7_Camera *camera = display->camera->prv;
            
    PDMode7_Vec2 halfFov = newVec2(0, 0);
    PDMode7_Vec2 tanHalfFov = newVec2(0, 0);

    halfFov.x = camera->fov / 2;
    tanHalfFov.x = tanf(halfFov.x);
    
    halfFov.y = atanf(tanHalfFov.x * (float)display->rect.height / display->rect.width);
    tanHalfFov.y = tanf(halfFov.y);

    // Calculate camera forward vector
    PDMode7_Vec3 forwardVec = newVec3(0, 0, 0);
    forwardVec.x = cosf(camera->angle) * cosf(camera->pitch);
    forwardVec.y = sinf(camera->angle) * cosf(camera->pitch);
    forwardVec.z = sinf(camera->pitch);
    
    // Calculate the right vector
    PDMode7_Vec3 rightVec = newVec3(0, 0, 0);
    rightVec.x = -sinf(camera->angle);
    rightVec.y = cosf(camera->angle);

    // Calculate the up vector
    PDMode7_Vec3 upVec = newVec3(0, 0, 0);
    upVec.x = -cosf(camera->angle) * sinf(camera->pitch);
    upVec.y = -sinf(camera->angle) * sinf(camera->pitch);
    upVec.z = cosf(camera->pitch);
    
    // Formula derived from the point projection for directionZ = 0
    int horizon = 0;
    if(upVec.z != 0)
    {
        horizon = roundf((1 + (forwardVec.z / (upVec.z * tanHalfFov.y))) * (float)display->rect.height / 2);
    }
    horizon = mode7_max(0, mode7_min(horizon, display->rect.height));
    
    int planeHeight = display->rect.height - horizon;
    
    PDMode7_Vec2 fovRatio = newVec2(halfFov.x / (float)M_PI_2, halfFov.y / (float)M_PI_2);
    
    return (_PDMode7_Parameters){
        .screenRatio = newVec2(1.0f / display->rect.width * 2, 1.0f / display->rect.height * 2),
        .worldScaleInv = worldGetScaleInv(pWorld),
        .planeHeight = planeHeight,
        .horizon = horizon,
        .halfFov = halfFov,
        .tanHalfFov = tanHalfFov,
        .fovRatio = fovRatio,
        .forwardVec = forwardVec,
        .rightVec = rightVec,
        .upVec = upVec
    };
}

static void worldUpdateDisplay(PDMode7_World *pWorld, PDMode7_Display *pDisplay)
{
    int displayIndex = indexForDisplay(pWorld, pDisplay);
    if(displayIndex < 0)
    {
        return;
    }
    
    _PDMode7_Display *display = pDisplay->prv;
    _PDMode7_World *world = pWorld->prv;
    _PDMode7_Camera *camera = display->camera->prv;

    _PDMode7_Parameters parameters = worldGetParameters(pDisplay);
    
    // Clear the visible sprites for current display
    arrayClear(display->visibleInstances);
    
    // Get the close sprites from the grid
    _PDMode7_Array *closeSprites = gridGetSpritesAtPoint(world->grid, camera->position, camera->clipDistanceUnits);
    
    for(int i = 0; i < closeSprites->length; i++)
    {
        PDMode7_Sprite *pSprite = closeSprites->items[i];
        _PDMode7_Sprite *sprite = pSprite->prv;
        
        PDMode7_SpriteInstance *pInstance = sprite->instances[displayIndex];
        _PDMode7_SpriteInstance *instance = pInstance->prv;
        _PDMode7_SpriteDataSource *dataSource = instance->dataSource->prv;

        if(instance->visible)
        {
            // Calculate view vector
            PDMode7_Vec3 viewVec = vec3_subtract(sprite->position, camera->position);
            
            // Calculate local point
            float localZ = vec3_dot(viewVec, parameters.forwardVec);
            
            if(localZ > 0)
            {
                // Point is in front of camera
                
                PDMode7_Vec3 displayPoint = worldToDisplayPoint(pDisplay, sprite->position, &parameters);
                
                displayPoint.x += display->rect.x;
                displayPoint.y += display->rect.y;
                
                PDMode7_Vec3 multiplier = displayMultiplierForScanlineAt(pDisplay, sprite->position, &parameters);
                
                float spriteWidth = fmaxf(sprite->size.x, sprite->size.y);
                float spriteHeight = sprite->size.z;
                if(instance->billboardSizeBehavior == kMode7BillboardSizeCustom)
                {
                    spriteWidth = instance->billboardSize.x;
                    spriteHeight = instance->billboardSize.y;
                }
                
                int preferredWidth = roundf(multiplier.x * spriteWidth);
                int preferredHeight = roundf(multiplier.y * spriteHeight);
                
                int hasMaximumWidth = (dataSource->maximumWidth > 0);

                if(preferredWidth >= dataSource->minimumWidth && (!hasMaximumWidth || preferredWidth <= dataSource->maximumWidth))
                {
                    int finalWidth = preferredWidth;
                    int finalHeight = preferredHeight;
                    LCDBitmap *finalBitmap = NULL;
                    
                    if(instance->bitmapTable)
                    {
                        unsigned int angleLength = dataSource->lengths[kMode7SpriteDataSourceAngle];
                        unsigned int pitchLength = dataSource->lengths[kMode7SpriteDataSourcePitch];
                        unsigned int scaleLength = dataSource->lengths[kMode7SpriteDataSourceScale];
                        
                        unsigned int angleIndex = 0;
                        if(angleLength > 1)
                        {
                            float relativeAngle = worldGetRelativeAngle(camera->position, camera->angle, sprite->position, sprite->angle, &parameters);
                            angleIndex = roundf(relativeAngle / (2 * (float)M_PI) * angleLength);
                            if(angleIndex >= angleLength){
                                angleIndex = 0;
                            }
                        }
                        
                        unsigned int pitchIndex = 0;
                        if(pitchLength > 1)
                        {
                            float relativeAngle = worldGetRelativePitch(camera->position, camera->pitch, sprite->position, sprite->pitch, &parameters);
                            pitchIndex = roundf(relativeAngle / (2 * (float)M_PI) * pitchLength);
                            if(pitchIndex >= pitchLength){
                                pitchIndex = 0;
                            }
                        }
                        
                        unsigned int scaleIndex = 0;
                        if(scaleLength > 1 && hasMaximumWidth)
                        {
                            int deltaWidth = abs(dataSource->maximumWidth - dataSource->minimumWidth);
                            int scaleStep = ceilf(deltaWidth / (float)scaleLength);
                            if(scaleStep > 0)
                            {
                                scaleIndex = roundf((dataSource->maximumWidth - preferredWidth) / (float)scaleStep);
                            }
                        }
                        
                        unsigned int tableIndex = spriteGetTableIndex(pInstance, angleIndex, pitchIndex, scaleIndex);
                        
                        LCDBitmap *bitmap = playdate->graphics->getTableBitmap(instance->bitmapTable, tableIndex);
                        if(bitmap)
                        {
                            int bitmapWidth; int bitmapHeight;
                            playdate->graphics->getBitmapData(bitmap, &bitmapWidth, &bitmapHeight, NULL, NULL, NULL);
                            
                            finalWidth = bitmapWidth;
                            finalHeight = bitmapHeight;
                            
                            finalBitmap = bitmap;
                        }
                    }
                    
                    // finalBitmap can be NULL only if:
                    // A drawCallback is set AND instance->bitmapTable is NULL
                    if(finalBitmap || (instance->drawCallback && !instance->bitmapTable))
                    {
                        int rectX = roundToIncrement(displayPoint.x - finalWidth * instance->imageCenter.x, instance->roundingIncrement.x);
                        int rectY = roundToIncrement(displayPoint.y - finalHeight * instance->imageCenter.y, instance->roundingIncrement.y);
                        
                        if(instance->alignmentX == kMode7SpriteAlignmentEven)
                        {
                            if((rectX % 2) != 0)
                            {
                                rectX += 1;
                            }
                        }
                        else if(instance->alignmentX == kMode7SpriteAlignmentOdd)
                        {
                            if((rectX % 2) == 0)
                            {
                                rectX += 1;
                            }
                        }
                        
                        if(instance->alignmentY == kMode7SpriteAlignmentEven)
                        {
                            if((rectY % 2) != 0)
                            {
                                rectY += 1;
                            }
                        }
                        else if(instance->alignmentY == kMode7SpriteAlignmentOdd)
                        {
                            if((rectY % 2) == 0)
                            {
                                rectY += 1;
                            }
                        }
                        
                        PDMode7_Rect spriteRect = newRect(rectX, rectY, finalWidth, finalHeight);
                        if(rectIntersect(spriteRect, display->rect))
                        {
                            instance->depth = localZ;
                            instance->bitmap = finalBitmap;
                            instance->displayRect = spriteRect;
                            arrayPush(display->visibleInstances, pInstance);
                        }
                    }
                }
            }
        }
    }
    
    qsort(display->visibleInstances->items, display->visibleInstances->length, sizeof(PDMode7_SpriteInstance*), sortSprites);
    
    freeArray(closeSprites);
}

static int sortSprites(const void *a, const void *b)
{
    _PDMode7_SpriteInstance *instance1 = (*(PDMode7_SpriteInstance**)a)->prv;
    _PDMode7_SpriteInstance *instance2 = (*(PDMode7_SpriteInstance**)b)->prv;
    
    if(instance1->depth > instance2->depth)
    {
        return -1;
    }
    else if(instance1->depth < instance2->depth)
    {
        return 1;
    }
    
    return 0;
}

static float worldGetRelativeAngle(PDMode7_Vec3 cameraPoint, float cameraAngle, PDMode7_Vec3 targetPoint, float targetAngle, _PDMode7_Parameters *p)
{
    PDMode7_Vec2 dirVec = newVec2(targetPoint.x - cameraPoint.x, targetPoint.y - cameraPoint.y);
    
    float dir_cos = cosf(-cameraAngle);
    float dir_sin = sinf(-cameraAngle);
    
    float rotatedX = dir_cos * dirVec.x - dir_sin * dirVec.y;
    float rotatedY = dir_sin * dirVec.x + dir_cos * dirVec.y;
        
    float relativeAngle = fmodf(atan2f(rotatedY, rotatedX) * p->fovRatio.x - atan2f(dirVec.y, dirVec.x) + targetAngle + (float)M_PI, 2 * (float)M_PI);
    if(relativeAngle < 0)
    {
        relativeAngle += 2 * (float)M_PI;
    }
    
    return relativeAngle;
}

static float worldGetRelativePitch(PDMode7_Vec3 cameraPoint, float cameraPitch, PDMode7_Vec3 targetPoint, float targetPitch, _PDMode7_Parameters *p)
{
    PDMode7_Vec2 viewVec = newVec2(targetPoint.x - cameraPoint.x, targetPoint.y - cameraPoint.y);
    float d = sqrtf(viewVec.x * viewVec.x + viewVec.y * viewVec.y);
    
    PDMode7_Vec2 dirVec = newVec2(d, targetPoint.z - cameraPoint.z);
    
    float dir_cos = cosf(-cameraPitch);
    float dir_sin = sinf(-cameraPitch);
    
    float rotatedX = dir_cos * dirVec.x - dir_sin * dirVec.y;
    float rotatedY = dir_sin * dirVec.x + dir_cos * dirVec.y;
    
    float relativeAngle = fmodf(atan2f(rotatedY, rotatedX) * p->fovRatio.y - atan2f(dirVec.y, dirVec.x) + targetPitch, 2 * (float)M_PI);
    if(relativeAngle < 0)
    {
        relativeAngle += 2 * (float)M_PI;
    }
    
    return relativeAngle;
}

static void worldUpdate(PDMode7_World *pWorld)
{
    _PDMode7_World *world = pWorld->prv;
    
    for(int i = 0; i < world->numberOfDisplays; i++)
    {
        PDMode7_Display *display = world->displays[i];
        worldUpdateDisplay(pWorld, display);
    }
}

static void drawBackground(PDMode7_Display *pDisplay, _PDMode7_Parameters *parameters)
{
    _PDMode7_Display *display = pDisplay->prv;
    PDMode7_Background *pBackground = display->background;
    _PDMode7_Background *background = pBackground->prv;

    if(!background->bitmap)
    {
        return;
    }
    
    PDMode7_Vec2 offset = backgroundGetOffset(pBackground, parameters);
    
    LCDBitmap *target;
    displayGetFramebuffer(pDisplay, &target, NULL, NULL);
    
    playdate->graphics->pushContext(target);
    playdate->graphics->setClipRect(display->rect.x, display->rect.y, display->rect.width, parameters->horizon);

    playdate->graphics->drawBitmap(background->bitmap, display->rect.x + offset.x, offset.y, kBitmapUnflipped);
    
    if(offset.x > 0)
    {
        int offsetX2 = offset.x - background->width;
        playdate->graphics->drawBitmap(background->bitmap, display->rect.x + offsetX2, offset.y, kBitmapUnflipped);
    }
    
    int maxOffset = -background->width + display->rect.width - offset.x;
    if(maxOffset > 0)
    {
        int offsetX2 = display->rect.width - maxOffset;
        playdate->graphics->drawBitmap(background->bitmap, display->rect.x + offsetX2, offset.y, kBitmapUnflipped);
    }
    
    playdate->graphics->popContext();
}

static void drawPlane(PDMode7_World *pWorld, PDMode7_Display *pDisplay, _PDMode7_Parameters *parameters)
{
    _PDMode7_World *world = pWorld->prv;
    _PDMode7_Display *display = pDisplay->prv;
    
    LCDBitmap *target; uint8_t *framebuffer; int rowbytes;
    displayGetFramebuffer(pDisplay, &target, &framebuffer, &rowbytes);
    
    // Set the framebuffer initial offset
    int startY = display->rect.y + parameters->horizon;
    uint8_t *frameStart = framebuffer + startY * rowbytes + display->rect.x / 8;
    int frameY = 0;

    int xStep; int yStep;
    getDisplayScaleStep(display->scale, &xStep, &yStep);
    
    // Pre-calculate displayWidth
    float displayWidthInv = 1.0f / display->rect.width * xStep;

    // Calculate the framebuffer increment
    int rowSize = rowbytes * yStep;

    for(int y = 0; y < parameters->planeHeight; y += yStep)
    {
        int relativeY = parameters->horizon + y;
        int absoluteY = display->rect.y + relativeY;
        
        // Left point for the scanline
        PDMode7_Vec3 leftPoint = displayToPlanePoint(pDisplay, 0, relativeY, parameters);
        leftPoint.x *= parameters->worldScaleInv;
        leftPoint.y *= parameters->worldScaleInv;

        // Right point for the scanline
        PDMode7_Vec3 rightPoint = displayToPlanePoint(pDisplay, display->rect.width, relativeY, parameters);
        rightPoint.x *= parameters->worldScaleInv;
        rightPoint.y *= parameters->worldScaleInv;
        
        // Calculate the delta between the scanline points
        // Then divide it by the display width
        float dxStep = (rightPoint.x - leftPoint.x) * displayWidthInv;
        float dyStep = (rightPoint.y - leftPoint.y) * displayWidthInv;
        
        // Set the initial framebuffer offset and bit position
        int frameX = 0;
        int bitPosition = 0;
        
        // If y exceeds display height, get truncated scale
        PDMode7_DisplayScale planeScale = display->scale;
        if((relativeY + yStep) > display->rect.height)
        {
            planeScale = truncatedDisplayScale(planeScale);
        }
        
#if PD_MODE7_CEILING
        int ceilingRelativeY = parameters->horizon - y;
        PDMode7_DisplayScale ceilingScale = display->scale;
        if((ceilingRelativeY - yStep) < 0)
        {
            ceilingScale = truncatedDisplayScale(ceilingScale);
        }
#endif
        // Advance pointLeft in the loop
        for(int x = 0; x < display->rect.width; x += xStep)
        {
            int mapX = floorf(leftPoint.x);
            int mapY = floorf(leftPoint.y);
            
            uint8_t color = worldSampleColor(world->planeBitmap, mapX, mapY, world->planeFillColor.gray);
            worldSetColor(color, planeScale, frameStart + frameY + frameX, rowbytes, bitPosition, absoluteY, 1);
            
#if PD_MODE7_CEILING
            if(world->ceilingBitmap && ceilingRelativeY > 0)
            {
                uint8_t ceilingColor = worldSampleColor(world->ceilingBitmap, mapX, mapY, world->ceilingFillColor.gray);
                worldSetColor(ceilingColor, ceilingScale, frameStart - frameY - rowbytes + frameX, -rowbytes, bitPosition, absoluteY - 1, -1);
            }
#endif
            // Advance the point by the step
            leftPoint.x += dxStep;
            leftPoint.y += dyStep;
                        
            bitPosition += xStep;
            
            if(bitPosition == 8)
            {
                // Increment the framebuffer offset and reset bitPosition
                frameX++;
                bitPosition = 0;
            }
        }
        
        // Increment the framebuffer index
        frameY += rowSize;
    }
    
    if(!target)
    {
        int absoluteHorizon = display->rect.y + parameters->horizon;
#if PD_MODE7_CEILING
        playdate->graphics->markUpdatedRows(absoluteHorizon - parameters->planeHeight, absoluteHorizon + parameters->planeHeight - 1);
#else
        playdate->graphics->markUpdatedRows(absoluteHorizon, absoluteHorizon + parameters->planeHeight - 1);
#endif
    }
}

static void drawSprite(PDMode7_SpriteInstance *pInstance)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    
    if(instance->bitmap)
    {
        playdate->graphics->drawBitmap(instance->bitmap, instance->displayRect.x, instance->displayRect.y, kBitmapUnflipped);
    }
}

static void drawSprites(PDMode7_Display *pDisplay)
{
    _PDMode7_Display *display = pDisplay->prv;
    
    for(int i = 0; i < display->visibleInstances->length; i++)
    {
        PDMode7_SpriteInstance *pInstance = display->visibleInstances->items[i];
        _PDMode7_SpriteInstance *instance = pInstance->prv;
        
        if(instance->drawCallback)
        {
            switch(instance->drawCallback->type)
            {
                case _PDMode7_CallbackTypeC:
                {
                    PDMode7_SpriteDrawCallbackFunction *drawFunction = instance->drawCallback->cFunction;
                    drawFunction(pInstance, instance->bitmap, instance->displayRect, drawSprite);
                    break;
                }
                case _PDMode7_CallbackTypeLua:
                {
                    lua_spriteCallDrawCallback(pInstance);
                    break;
                }
            }
        }
        else
        {
            drawSprite(pInstance);
        }
    }
}

static void worldDraw(PDMode7_World *pWorld, PDMode7_Display *pDisplay)
{
    pDisplay = getDisplay(pWorld, pDisplay);
    int displayIndex = indexForDisplay(pWorld, pDisplay);
    if(displayIndex < 0)
    {
        return;
    }
    
    _PDMode7_Display *display = pDisplay->prv;
    _PDMode7_Parameters parameters = worldGetParameters(pDisplay);
    
    LCDBitmap *target;
    displayGetFramebuffer(pDisplay, &target, NULL, NULL);
    
    playdate->graphics->pushContext(target);
    
    if(target)
    {
        playdate->graphics->clear(kColorWhite);
    }
    
    if(!target && displayNeedsClip(pDisplay))
    {
        playdate->graphics->setClipRect(display->rect.x, display->rect.y, display->rect.width, display->rect.height);
    }
    
    drawBackground(pDisplay, &parameters);
    drawPlane(pWorld, pDisplay, &parameters);
    drawSprites(pDisplay);
    
    playdate->graphics->popContext();
    
    if(target)
    {
        switch(display->orientation)
        {
            case kMode7DisplayOrientationLandscapeLeft:
            {
                LCDBitmapFlip bitmapFlip = kBitmapUnflipped;
                if(display->flipMode == kMode7DisplayFlipModeX)
                {
                    bitmapFlip = kBitmapFlippedX;
                }
                else if(display->flipMode == kMode7DisplayFlipModeY)
                {
                    bitmapFlip = kBitmapFlippedY;
                }
                else if(display->flipMode == kMode7DisplayFlipModeXY)
                {
                    bitmapFlip = kBitmapFlippedXY;
                }
                playdate->graphics->drawBitmap(target, display->absoluteRect.x, display->absoluteRect.y, bitmapFlip);
                break;
            }
            case kMode7DisplayOrientationLandscapeRight:
            {
                LCDBitmapFlip bitmapFlip = kBitmapFlippedXY;
                if(display->flipMode == kMode7DisplayFlipModeX)
                {
                    bitmapFlip = kBitmapFlippedY;
                }
                else if(display->flipMode == kMode7DisplayFlipModeY)
                {
                    bitmapFlip = kBitmapFlippedX;
                }
                else if(display->flipMode == kMode7DisplayFlipModeXY)
                {
                    bitmapFlip = kBitmapUnflipped;
                }
                playdate->graphics->drawBitmap(target, LCD_COLUMNS - (display->absoluteRect.width + display->absoluteRect.x), LCD_ROWS - (display->absoluteRect.height + display->absoluteRect.y), bitmapFlip);
                break;
            }
            case kMode7DisplayOrientationPortrait:
            {
                playdate->graphics->drawRotatedBitmap(target, LCD_COLUMNS - (display->absoluteRect.height + display->absoluteRect.y), display->absoluteRect.x, 90, 0, 0, 1, 1);
                break;
            }
            case kMode7DisplayOrientationPortraitUpsideDown:
            {
                playdate->graphics->drawRotatedBitmap(target, display->absoluteRect.y, LCD_ROWS - (display->absoluteRect.width + display->absoluteRect.x), -90, 0, 0, 1, 1);
                break;
            }
            default:
                break;
        }
    }
}

static inline uint8_t worldSampleColor(PDMode7_Bitmap *pBitmap, int x, int y, uint8_t fillColor)
{
    _PDMode7_Bitmap *bitmap = pBitmap->prv;
    
    // Check for point inside the bitmap
    if(x >= 0 && x < bitmap->width && y >= 0 && y < bitmap->height)
    {
        // Get color from bitmap
        return bitmap->data[bitmap->width * y + x];
    }
    return fillColor;
}

static inline void worldSetColor(uint8_t color, PDMode7_DisplayScale displayScale, uint8_t *ptr, int rowbytes, int bitPosition, int y, int dirY)
{
    uint8_t patternIndex = ((uint16_t)color * 17) >> 8;
    
    switch(displayScale)
    {
        case kMode7DisplayScale1x1:
        {
            uint8_t pattern = patterns4x4[patternIndex][y & 3];
            uint8_t mask = 0b00000001 << (7 - bitPosition);
            
            *ptr = (*ptr & ~mask) | (pattern & mask);
            
            break;
        }
        case kMode7DisplayScale2x1:
        {
            uint8_t pattern = patterns4x4[patternIndex][y & 3];
            uint8_t mask = 0b00000011 << (6 - bitPosition);
            
            *ptr = (*ptr & ~mask) | (pattern & mask);
            
            break;
        }
        case kMode7DisplayScale2x2:
        {
            uint8_t pattern1 = patterns4x4[patternIndex][y & 3];
            uint8_t pattern2 = patterns4x4[patternIndex][(y + dirY) & 3];

            uint8_t mask = 0b00000011 << (6 - bitPosition);
            
            *ptr = (*ptr & ~mask) | (pattern1 & mask);
            ptr += rowbytes;
            *ptr = (*ptr & ~mask) | (pattern2 & mask);

            break;
        }
        case kMode7DisplayScale4x1:
        {
            uint8_t pattern = patterns4x4[patternIndex][y & 3];
            uint8_t mask = 0b00001111 << (4 - bitPosition);
            
            *ptr = (*ptr & ~mask) | (pattern & mask);
            
            break;
        }
        case kMode7DisplayScale4x2:
        {
            uint8_t pattern1 = patterns4x4[patternIndex][y & 3];
            uint8_t pattern2 = patterns4x4[patternIndex][(y + dirY) & 3];

            uint8_t mask = 0b00001111 << (4 - bitPosition);
            
            *ptr = (*ptr & ~mask) | (pattern1 & mask);
            ptr += rowbytes;
            *ptr = (*ptr & ~mask) | (pattern2 & mask);
            
            break;
        }
        default:
            break;
    }
}

static PDMode7_Vec3 displayToPlanePoint(PDMode7_Display *pDisplay, int displayX, int displayY, _PDMode7_Parameters *p)
{
    _PDMode7_Display *display = pDisplay->prv;
    _PDMode7_Camera *camera = display->camera->prv;

    // Calculate the screen coordinates in world space
    float worldScreenX = (displayX * p->screenRatio.x - 1) * p->tanHalfFov.x;
    float worldScreenY = (1 - displayY * p->screenRatio.y) * p->tanHalfFov.y;

    // Calculate the direction vector in world space
    float directionX = p->forwardVec.x + p->rightVec.x * worldScreenX + p->upVec.x * worldScreenY;
    float directionY = p->forwardVec.y + p->rightVec.y * worldScreenX + p->upVec.y * worldScreenY;
    float directionZ = p->forwardVec.z + p->rightVec.z * worldScreenX + p->upVec.z * worldScreenY;
    
    float intersectionX = INFINITY;
    float intersectionY = INFINITY;
    float intersectionZ = -1;
    
    if(directionZ < 0)
    {
        // Calculate the intersection point with the plane at z = 0
        float t = -camera->position.z / directionZ;
        
        intersectionX = camera->position.x + t * directionX;
        intersectionY = camera->position.y + t * directionY;
        intersectionZ = 0;
    }
    
    return newVec3(intersectionX, intersectionY, intersectionZ);
}

static PDMode7_Vec3 displayToPlanePoint_public(PDMode7_World *pWorld, int displayX, int displayY, PDMode7_Display *pDisplay)
{
    pDisplay = getDisplay(pWorld, pDisplay);
    _PDMode7_Parameters parameters = worldGetParameters(pDisplay);
    return displayToPlanePoint(pDisplay, displayX, displayY, &parameters);
}

static PDMode7_Vec3 worldToDisplayPoint(PDMode7_Display *pDisplay, PDMode7_Vec3 point, _PDMode7_Parameters *p)
{
    _PDMode7_Display *display = pDisplay->prv;
    _PDMode7_Camera *camera = display->camera->prv;
    
    // Calculate view vector
    PDMode7_Vec3 viewVec = vec3_subtract(point, camera->position);
    
    // Calculate local point
    float localX = vec3_dot(viewVec, p->rightVec);
    float localY = vec3_dot(viewVec, p->upVec);
    float localZ = vec3_dot(viewVec, p->forwardVec);

    float displayX = 0;
    float displayY = 0;
    float displayZ = 0;
    
    // Prevent division by 0
    if(localZ != 0)
    {
        displayZ = 1;
        
        if(localZ < 0)
        {
            // Point is behind the camera
            localZ = -localZ;
            displayZ = -1;
        }
        
        // Find local coordinates
        float x_ndc = localX / (localZ * p->tanHalfFov.x);
        float y_ndc = localY / (localZ * p->tanHalfFov.y);
        
        displayX = (x_ndc + 1) * display->rect.width * 0.5f;
        displayY = (1 - y_ndc) * display->rect.height * 0.5f;
    }
    
    return newVec3(displayX, displayY, displayZ);
}

static PDMode7_Vec3 worldToDisplayPoint_public(PDMode7_World *pWorld, PDMode7_Vec3 point, PDMode7_Display *pDisplay)
{
    pDisplay = getDisplay(pWorld, pDisplay);
    _PDMode7_Parameters parameters = worldGetParameters(pDisplay);
    return worldToDisplayPoint(pDisplay, point, &parameters);
}

static PDMode7_Vec2 planeToBitmapPoint(PDMode7_World *pWorld, float pointX, float pointY)
{
    float scale = worldGetScaleInv(pWorld);
    return newVec2(pointX * scale, pointY * scale);
}

static PDMode7_Vec2 bitmapToPlanePoint(PDMode7_World *pWorld, float bitmapX, float bitmapY)
{
    float scale = worldGetScale(pWorld);
    return newVec2(bitmapX * scale, bitmapY * scale);
}

static PDMode7_Vec3 displayMultiplierForScanlineAt(PDMode7_Display *pDisplay, PDMode7_Vec3 point, _PDMode7_Parameters *p)
{
    _PDMode7_Display *display = pDisplay->prv;
    _PDMode7_Camera *camera = display->camera->prv;
    
    // Calculate view vector
    PDMode7_Vec3 viewVec = vec3_subtract(point, camera->position);
    
    float d = vec3_dot(viewVec, p->forwardVec);
    
    float sampleX = 0;
    float sampleY = 0;
    float sampleZ = 0;
    
    // Prevent division by 0
    if(d != 0)
    {
        sampleZ = 1;
        if(d < 0)
        {
            sampleZ = -1;
            d = -d;
        }
        
        sampleX = (display->rect.width * 0.5f) / (d * p->tanHalfFov.x);
        sampleY = (display->rect.height * 0.5f) / (d * p->tanHalfFov.y);
    }
    
    return newVec3(sampleX, sampleY, sampleZ);
}

static PDMode7_Vec3 displayMultiplierForScanlineAt_public(PDMode7_World *pWorld, PDMode7_Vec3 point, PDMode7_Display *pDisplay)
{
    pDisplay = getDisplay(pWorld, pDisplay);
    _PDMode7_Parameters parameters = worldGetParameters(pDisplay);
    return displayMultiplierForScanlineAt(pDisplay, point, &parameters);
}

static void addSprite(PDMode7_World *pWorld, PDMode7_Sprite *pSprite)
{
    _PDMode7_World *world = pWorld->prv;
    _PDMode7_Sprite *sprite = pSprite->prv;
    
    if(sprite->world)
    {
        return;
    }
    
    if(sprite->luaRef)
    {
        playdate->lua->retainObject(sprite->luaRef);
    }

    sprite->world = pWorld;
    arrayPush(world->sprites, pSprite);
    
    spriteBoundsDidChange(pSprite);
}

static int addDisplay(PDMode7_World *pWorld, PDMode7_Display *pDisplay)
{
    _PDMode7_World *world = pWorld->prv;
    _PDMode7_Display *display = pDisplay->prv;
    
    if(display->world)
    {
        return 0;
    }
    
    if(world->numberOfDisplays < MODE7_MAX_DISPLAYS)
    {
        display->world = pWorld;
        
        world->displays[world->numberOfDisplays] = pDisplay;
        world->numberOfDisplays++;
        
        if(display->luaRef && !display->isManaged)
        {
            playdate->lua->retainObject(display->luaRef);
        }
        
        return 1;
    }
    
    return 0;
}

static int indexForDisplay(PDMode7_World *pWorld, PDMode7_Display *pDisplay)
{
    _PDMode7_World *world = pWorld->prv;
    
    for(int i = 0; i < world->numberOfDisplays; i++)
    {
        PDMode7_Display *localDisplay = world->displays[i];
        if(pDisplay == localDisplay)
        {
            return i;
        }
    }
    
    return -1;
}

static _PDMode7_Array* _getSprites(PDMode7_World *pWorld)
{
    _PDMode7_World *world = pWorld->prv;
    return world->sprites;
}

static PDMode7_Sprite** getSprites(PDMode7_World *pWorld, int* len)
{
    _PDMode7_Array *sprites = _getSprites(pWorld);

    *len = sprites->length;
    return (PDMode7_Sprite**)sprites->items;
}

static _PDMode7_Array* _getVisibleSpriteInstances(PDMode7_World *pWorld, PDMode7_Display *pDisplay)
{
    pDisplay = getDisplay(pWorld, pDisplay);
    _PDMode7_Display *display = pDisplay->prv;
    return display->visibleInstances;
}

static PDMode7_SpriteInstance** getVisibleSpriteInstances(PDMode7_World *pWorld, int *length, PDMode7_Display *pDisplay)
{
    _PDMode7_Array *visibleSprites = _getVisibleSpriteInstances(pWorld, pDisplay);
    *length = visibleSprites->length;
    return (PDMode7_SpriteInstance**)visibleSprites->items;
}

static void getDisplayScaleStep(PDMode7_DisplayScale displayScale, int *xStep, int *yStep)
{
    *xStep = 1;
    *yStep = 1;
    
    switch(displayScale)
    {
        case kMode7DisplayScale2x1: { *xStep = 2; *yStep = 1; break; }
        case kMode7DisplayScale2x2: { *xStep = 2; *yStep = 2; break; }
        case kMode7DisplayScale4x1: { *xStep = 4; *yStep = 1; break; }
        case kMode7DisplayScale4x2: { *xStep = 4; *yStep = 2; break; }
        default:
            break;
    }
}

static inline PDMode7_DisplayScale truncatedDisplayScale(PDMode7_DisplayScale displayScale)
{
    switch(displayScale)
    {
        case kMode7DisplayScale2x2: return kMode7DisplayScale2x1;
        case kMode7DisplayScale4x2: return kMode7DisplayScale4x1;
        default: return displayScale;
    }
}

static PDMode7_Display* getDisplay(PDMode7_World *world, PDMode7_Display *pDisplay)
{
    if(pDisplay)
    {
        return pDisplay;
    }
    return ((_PDMode7_World*)world->prv)->mainDisplay;
}

static PDMode7_Vec3 worldGetSize(PDMode7_World *pWorld)
{
    _PDMode7_World *world = pWorld->prv;
    return newVec3(world->width, world->height, world->depth);
}

static float worldGetScale(PDMode7_World *pWorld)
{
    _PDMode7_World *world = pWorld->prv;
    if(world->planeBitmap)
    {
        _PDMode7_Bitmap *bitmap = world->planeBitmap->prv;
        if(bitmap->width != 0)
        {
            return world->width / (float)bitmap->width;
        }
    }
    return 0;
}

static float worldGetScaleInv(PDMode7_World *pWorld)
{
    _PDMode7_World *world = pWorld->prv;
    if(world->planeBitmap)
    {
        _PDMode7_Bitmap *bitmap = world->planeBitmap->prv;
        if(world->width != 0)
        {
            return (float)bitmap->width / world->width;
        }
    }
    return 0;
}

static PDMode7_Display* worldGetMainDisplay(PDMode7_World *world)
{
    return ((_PDMode7_World*)world->prv)->mainDisplay;
}

static PDMode7_Camera* worldGetMainCamera(PDMode7_World *world)
{
    return ((_PDMode7_World*)world->prv)->mainCamera;
}

static PDMode7_Bitmap* getPlaneBitmap(PDMode7_World *world)
{
    return ((_PDMode7_World*)world->prv)->planeBitmap;
}

static void setPlaneBitmap(PDMode7_World *world, PDMode7_Bitmap *bitmap)
{
    _PDMode7_World *_world = world->prv;
    _PDMode7_Bitmap *_bitmap = bitmap->prv;

    if(_world->planeBitmap)
    {
        _PDMode7_Bitmap *currentBitmap = _world->planeBitmap->prv;
        if(currentBitmap->luaRef)
        {
            GC_release(currentBitmap->luaRef);
        }
    }
    
    if(bitmap && _bitmap->luaRef)
    {
        GC_retain(_bitmap->luaRef);
    }
    
    _world->planeBitmap = bitmap;
}

static void setPlaneFillColor(PDMode7_World *world, PDMode7_Color color)
{
    ((_PDMode7_World*)world->prv)->planeFillColor = color;
}

static PDMode7_Color getPlaneFillColor(PDMode7_World *world)
{
    return ((_PDMode7_World*)world->prv)->planeFillColor;
}

static PDMode7_Bitmap* getCeilingBitmap(PDMode7_World *world)
{
    return ((_PDMode7_World*)world->prv)->ceilingBitmap;
}

static void setCeilingBitmap(PDMode7_World *world, PDMode7_Bitmap *bitmap)
{
    _PDMode7_World *_world = world->prv;
    _PDMode7_Bitmap *_bitmap = bitmap->prv;

    if(_world->ceilingBitmap)
    {
        _PDMode7_Bitmap *currentBitmap = _world->ceilingBitmap->prv;
        if(currentBitmap->luaRef)
        {
            GC_release(currentBitmap->luaRef);
        }
    }
    
    if(bitmap && _bitmap->luaRef)
    {
        GC_retain(_bitmap->luaRef);
    }
    
    _world->ceilingBitmap = bitmap;
}

static void setCeilingFillColor(PDMode7_World *world, PDMode7_Color color)
{
    ((_PDMode7_World*)world->prv)->ceilingFillColor = color;
}

static PDMode7_Color getCeilingFillColor(PDMode7_World *world)
{
    return ((_PDMode7_World*)world->prv)->ceilingFillColor;
}

static void freeWorld(PDMode7_World *pWorld)
{
    _PDMode7_World *world = pWorld->prv;
    
    for(int i = world->numberOfDisplays - 1; i >= 0; i--)
    {
        PDMode7_Display *display = world->displays[i];
        removeDisplay(display);
    }
    
    for(int i = world->sprites->length - 1; i >= 0; i--)
    {
        PDMode7_Sprite *sprite = world->sprites->items[i];
        removeSprite(sprite);
    }
    
    freeGrid(world->grid);
    freeArray(world->sprites);
    
    if(world->planeBitmap)
    {
        releaseBitmap(world->planeBitmap);
    }
    if(world->ceilingBitmap)
    {
        releaseBitmap(world->ceilingBitmap);
    }
    
    if(world->mainCamera)
    {
        _PDMode7_Camera *_mainCamera = world->mainCamera->prv;
        // Remove the "managed" flag so we can free it
        _mainCamera->isManaged = 0;
        freeCamera(world->mainCamera);
    }
    
    _PDMode7_Display *_mainDisplay = world->mainDisplay->prv;
    // Remove the "managed" flag so we can free it
    _mainDisplay->isManaged = 0;
    if(_mainDisplay->luaRef)
    {
        releaseDisplay(world->mainDisplay);
    }
    else
    {
        freeDisplay(world->mainDisplay);
    }

    playdate->system->realloc(world, 0);
    playdate->system->realloc(pWorld, 0);
}

static PDMode7_Display* newDisplay(int x, int y, int width, int height)
{
    PDMode7_Display *display = playdate->system->realloc(NULL, sizeof(PDMode7_Display));
    
    _PDMode7_Display *_display = playdate->system->realloc(NULL, sizeof(_PDMode7_Display));
    display->prv = _display;
    
    _display->world = NULL;
    _display->camera = NULL;
    _display->rect = newRect(0, 0, 0, 0);
    _display->absoluteRect = newRect(0, 0, 0, 0);
    _display->scale = kMode7DisplayScale2x2;
    _display->orientation = kMode7DisplayOrientationLandscapeLeft;
    _display->flipMode = kMode7DisplayFlipModeNone;
    _display->secondaryFramebuffer = NULL;
    
    _display->visibleInstances = newArray();
    
    displaySetRect(display, x, y, width, height);
    displaySetOrientation(display, kMode7DisplayOrientationLandscapeLeft);
    
    PDMode7_Background *background = playdate->system->realloc(NULL, sizeof(PDMode7_Background));
    _display->background = background;
    
    _PDMode7_Background *_background = playdate->system->realloc(NULL, sizeof(_PDMode7_Background));
    background->prv = _background;
    
    _background->display = display;
    _background->bitmap = NULL;
    _background->luaBitmap = NULL;
    _background->width = 0;
    _background->height = 0;
    _background->center = newVec2(0.5, 0.5);
    _background->roundingIncrement = newVec2ui(1, 1);
    
    _display->isManaged = 0;
    _display->luaRef = NULL;
    
    return display;
}

static PDMode7_DisplayScale displayGetScale(PDMode7_Display *display)
{
    return ((_PDMode7_Display*)display->prv)->scale;
}

static void displaySetScale(PDMode7_Display *display, PDMode7_DisplayScale scale)
{
    ((_PDMode7_Display*)display->prv)->scale = scale;
}

static PDMode7_Camera* displayGetCamera(PDMode7_Display *display)
{
    return ((_PDMode7_Display*)display->prv)->camera;
}

static void displaySetCamera(PDMode7_Display *display, PDMode7_Camera *camera)
{
    _PDMode7_Display *_display = display->prv;
    _PDMode7_Camera *_camera = camera->prv;

    if(_display->camera)
    {
        _PDMode7_Camera *_camera = _display->camera->prv;
        if(_camera->luaRef)
        {
            GC_release(_camera->luaRef);
        }
    }
    
    if(_camera->luaRef)
    {
        GC_retain(_camera->luaRef);
    }
    
    _display->camera = camera;
}

static PDMode7_Background* displayGetBackground(PDMode7_Display *display)
{
    return ((_PDMode7_Display*)display->prv)->background;
}

static int displayNeedsClip(PDMode7_Display *display)
{
    _PDMode7_Display *_display = display->prv;
    return (_display->rect.width < LCD_COLUMNS || _display->rect.height < LCD_ROWS);
}

static void displayRectDidChange(PDMode7_Display *display)
{
    _PDMode7_Display *_display = display->prv;
    
    if(_display->secondaryFramebuffer)
    {
        playdate->graphics->freeBitmap(_display->secondaryFramebuffer);
        _display->secondaryFramebuffer = NULL;
    }
    
    _display->rect = _display->absoluteRect;
    
    if(_display->orientation != kMode7DisplayOrientationLandscapeLeft || _display->flipMode != kMode7DisplayFlipModeNone)
    {
        _display->secondaryFramebuffer = playdate->graphics->newBitmap(_display->absoluteRect.width, _display->absoluteRect.height, kColorWhite);
        _display->rect = newRect(0, 0, _display->absoluteRect.width, _display->absoluteRect.height);
    }
}

static PDMode7_Rect displayGetRect(PDMode7_Display *display)
{
    return ((_PDMode7_Display*)display->prv)->rect;
}

static void displaySetRect(PDMode7_Display *display, int x, int y, int width, int height)
{
    _PDMode7_Display *_display = display->prv;
    
    // x and width should be multiple of 8
    
    _display->absoluteRect.x = x / 8 * 8;
    _display->absoluteRect.y = y;
    _display->absoluteRect.width = width / 8 * 8;
    _display->absoluteRect.height = height;
    
    displayRectDidChange(display);
}

static PDMode7_DisplayOrientation displayGetOrientation(PDMode7_Display *display)
{
    return ((_PDMode7_Display*)display->prv)->orientation;
}

static void displaySetOrientation(PDMode7_Display *display, PDMode7_DisplayOrientation orientation)
{
    _PDMode7_Display *_display = display->prv;
    _display->orientation = orientation;
    displayRectDidChange(display);
}

static PDMode7_DisplayFlipMode displayGetFlipMode(PDMode7_Display *display)
{
    return ((_PDMode7_Display*)display->prv)->flipMode;
}

static void displaySetFlipMode(PDMode7_Display *display, PDMode7_DisplayFlipMode flipMode)
{
    _PDMode7_Display *_display = display->prv;
    _display->flipMode = flipMode;
    displayRectDidChange(display);
}

static int displayGetHorizon(PDMode7_Display *pDisplay)
{
    _PDMode7_Parameters parameters = worldGetParameters(pDisplay);
    return parameters.horizon;
}

static float displayPitchForHorizon(PDMode7_Display *pDisplay, float horizon)
{
    _PDMode7_Parameters parameters = worldGetParameters(pDisplay);
    _PDMode7_Display *display = pDisplay->prv;
    float halfHeight = (float)display->rect.height / 2;
    float pitch = atanf((horizon / halfHeight - 1) * parameters.tanHalfFov.y);
    return pitch;
}

static PDMode7_World* displayGetWorld(PDMode7_Display *display)
{
    return ((_PDMode7_Display*)display->prv)->world;
}

static PDMode7_Vec2 displayConvertPointFromOrientation(PDMode7_Display *pDisplay, float x, float y)
{
    _PDMode7_Display *display = pDisplay->prv;
    switch(display->orientation)
    {
        case kMode7DisplayOrientationLandscapeRight:
        {
            return newVec2(display->rect.width - x, display->rect.height - y);
            break;
        }
        case kMode7DisplayOrientationPortrait:
        {
            return newVec2(display->rect.height - y, x);
            break;
        }
        case kMode7DisplayOrientationPortraitUpsideDown:
        {
            return newVec2(y, display->rect.width - x);
            break;
        }
        default:
        {
            return newVec2(x, y);
            break;
        }
    }
}

static PDMode7_Vec2 displayConvertPointToOrientation(PDMode7_Display *pDisplay, float x, float y)
{
    _PDMode7_Display *display = pDisplay->prv;
    switch(display->orientation)
    {
        case kMode7DisplayOrientationLandscapeRight:
        {
            return newVec2(display->rect.width - x, display->rect.height - y);
            break;
        }
        case kMode7DisplayOrientationPortrait:
        {
            return newVec2(y, display->rect.height - x);
            break;
        }
        case kMode7DisplayOrientationPortraitUpsideDown:
        {
            return newVec2(display->rect.width - y, x);
            break;
        }
        default:
        {
            return newVec2(x, y);
            break;
        }
    }
}

static void displayGetFramebuffer(PDMode7_Display *display, LCDBitmap **target, uint8_t **framebuffer, int *rowbytes)
{
    _PDMode7_Display *_display = display->prv;
    
    if(!_display->secondaryFramebuffer)
    {
        if(target)
        {
            *target = NULL;
        }
        if(framebuffer)
        {
            *framebuffer = playdate->graphics->getFrame();
        }
        if(rowbytes)
        {
            *rowbytes = LCD_ROWSIZE;
        }
    }
    else
    {
        if(target)
        {
            *target = _display->secondaryFramebuffer;
        }
        playdate->graphics->getBitmapData(_display->secondaryFramebuffer, NULL, NULL, rowbytes, NULL, framebuffer);
    }
}

static void removeDisplay(PDMode7_Display *pDisplay)
{
    _PDMode7_Display *display = pDisplay->prv;
    
    if(display->world)
    {
        // Display is linked to world
        PDMode7_World *pWorld = display->world;
        _PDMode7_World *world = pWorld->prv;
        
        int index = indexForDisplay(pWorld, pDisplay);
        if(index >= 0)
        {
            for(int i = index; i < world->numberOfDisplays; i++)
            {
                world->displays[i] = world->displays[i + 1];
            }
            
            world->numberOfDisplays--;
            
            // Unlink world
            display->world = NULL;
            
            if(display->luaRef && !display->isManaged)
            {
                // Release non-managed display
                releaseDisplay(pDisplay);
            }
        }
    }
}

static LCDBitmap* backgroundGetBitmap(PDMode7_Background *background)
{
    return ((_PDMode7_Background*)background->prv)->bitmap;
}

static _PDMode7_LuaBitmap* backgroundGetLuaBitmap(PDMode7_Background *background)
{
    return ((_PDMode7_Background*)background->prv)->luaBitmap;
}

static void backgroundSetBitmap(PDMode7_Background *background, LCDBitmap *bitmap, _PDMode7_LuaBitmap *luaBitmap)
{
    _PDMode7_Background *_background = background->prv;
    
    _PDMode7_LuaBitmap *currentLuaBitmap = _background->luaBitmap;
    if(currentLuaBitmap)
    {
        GC_release(currentLuaBitmap->luaRef);
    }
    
    if(luaBitmap)
    {
        GC_retain(luaBitmap->luaRef);
    }
    
    _background->bitmap = bitmap;
    _background->luaBitmap = luaBitmap;
    _background->width = 0;
    _background->height = 0;
    
    if(bitmap)
    {
        int width; int height;
        playdate->graphics->getBitmapData(bitmap, &width, &height, NULL, NULL, NULL);
        
        _background->width = width;
        _background->height = height;
    }
}

static void backgroundSetBitmap_public(PDMode7_Background *background, LCDBitmap *bitmap)
{
    backgroundSetBitmap(background, bitmap, NULL);
}

static PDMode7_Vec2 backgroundGetCenter(PDMode7_Background *background)
{
    _PDMode7_Background *_background = background->prv;
    return _background->center;
}

static void backgroundSetCenter(PDMode7_Background *background, float x, float y)
{
    _PDMode7_Background *_background = background->prv;

    _background->center.x = x;
    _background->center.y = y;
}

static void backgroundGetRoundingIncrement(PDMode7_Background *background, unsigned int *x, unsigned int *y)
{
    _PDMode7_Background *_background = background->prv;

    *x = _background->roundingIncrement.x;
    *y = _background->roundingIncrement.y;
}

static void backgroundSetRoundingIncrement(PDMode7_Background *background, unsigned int x, unsigned int y)
{
    _PDMode7_Background *_background = background->prv;

    _background->roundingIncrement.x = x;
    _background->roundingIncrement.y = y;
}

static PDMode7_Vec2 backgroundGetOffset(PDMode7_Background *pBackground, _PDMode7_Parameters *parameters)
{
    _PDMode7_Background *background = pBackground->prv;
    _PDMode7_Display *display = background->display->prv;
    _PDMode7_Camera *camera = display->camera->prv;

    if(!background->bitmap)
    {
        return newVec2(0, 0);
    }
    
    float maxAngle = 2 * (float)M_PI;
    
    float angleFraction = fmodf(camera->angle, maxAngle);
    if(angleFraction < 0)
    {
        angleFraction += maxAngle;
    }
    angleFraction /= maxAngle;
    
    float offsetX = -background->width * fmodf(angleFraction + background->center.x, 1) + display->rect.width * 0.5f;
    float offsetY = parameters->horizon - background->height * background->center.y - display->rect.height * 0.5f;
    
    offsetX = roundToIncrement(offsetX, background->roundingIncrement.x);
    offsetY = roundToIncrement(offsetY, background->roundingIncrement.y);
    
    return newVec2(offsetX, offsetY);
}

static PDMode7_Vec2 backgroundGetOffset_public(PDMode7_Background *pBackground)
{
    _PDMode7_Background *background = pBackground->prv;
    PDMode7_Display *pDisplay = background->display;
    _PDMode7_Parameters parameters = worldGetParameters(pDisplay);
    return backgroundGetOffset(pBackground, &parameters);
}

static void releaseDisplay(PDMode7_Display *display)
{
    _PDMode7_Display *_display = display->prv;
    LuaUDObject *luaRef = _display->luaRef;
    _display->luaRef = NULL;
    playdate->lua->releaseObject(luaRef);
}

static void freeDisplay(PDMode7_Display *display)
{
    _PDMode7_Display *_display = display->prv;
    
    if(!_display->isManaged)
    {
        removeDisplay(display);
        
        if(_display->secondaryFramebuffer)
        {
            playdate->graphics->freeBitmap(_display->secondaryFramebuffer);
        }
        
        if(_display->camera)
        {
            _PDMode7_Camera *_camera = _display->camera->prv;
            if(_camera->luaRef)
            {
                GC_release(_camera->luaRef);
            }
        }
        
        PDMode7_Background *background = _display->background;
        _PDMode7_Background *_background = background->prv;
        if(_background->luaBitmap && _background->luaBitmap->luaRef)
        {
            GC_release(_background->luaBitmap->luaRef);
        }
        
        playdate->system->realloc(background->prv, 0);
        playdate->system->realloc(background, 0);
        
        freeArray(_display->visibleInstances);
        
        playdate->system->realloc(display->prv, 0);
        playdate->system->realloc(display, 0);
    }
}

static PDMode7_Camera* newCamera(void)
{
    PDMode7_Camera *camera = playdate->system->realloc(NULL, sizeof(PDMode7_Camera));
    
    _PDMode7_Camera *_camera = playdate->system->realloc(NULL, sizeof(_PDMode7_Camera));
    camera->prv = _camera;
    
    _camera->fov = degToRad(90);
    _camera->pitch = 0;
    
    _camera->angle = 0;
    _camera->position = newVec3(0, 0, 0);
    
    _camera->clipDistanceUnits = 1;
    _camera->isManaged = 0;
    _camera->luaRef = NULL;
    
    return camera;
}

static PDMode7_Vec3 cameraGetPosition(PDMode7_Camera *camera)
{
    return ((_PDMode7_Camera*)camera->prv)->position;
}

static void cameraSetPosition(PDMode7_Camera *camera, float x, float y, float z)
{
    _PDMode7_Camera *_camera = camera->prv;
    
    _camera->position.x = x;
    _camera->position.y = y;
    _camera->position.z = fmaxf(0, z);
}

static float cameraGetAngle(PDMode7_Camera *camera)
{
    return ((_PDMode7_Camera*)camera->prv)->angle;
}

static void cameraSetAngle(PDMode7_Camera *camera, float angle)
{
    ((_PDMode7_Camera*)camera->prv)->angle = angle;
}

static float cameraGetFOV(PDMode7_Camera *camera)
{
    return ((_PDMode7_Camera*)camera->prv)->fov;
}

static void cameraSetFOV(PDMode7_Camera *camera, float fov)
{
    ((_PDMode7_Camera*)camera->prv)->fov = fov;
}

static float cameraGetPitch(PDMode7_Camera *camera)
{
    return ((_PDMode7_Camera*)camera->prv)->pitch;
}

static void cameraSetPitch(PDMode7_Camera *camera, float pitch)
{
    ((_PDMode7_Camera*)camera->prv)->pitch = pitch;
}

static int cameraGetClipDistanceUnits(PDMode7_Camera *camera)
{
    return ((_PDMode7_Camera*)camera->prv)->clipDistanceUnits;
}

static void cameraSetClipDistanceUnits(PDMode7_Camera *camera, int units)
{
    ((_PDMode7_Camera*)camera->prv)->clipDistanceUnits = units;
}

static void cameraLookAtPoint(PDMode7_Camera *pCamera, PDMode7_Vec3 point)
{
    _PDMode7_Camera *camera = pCamera->prv;
    
    PDMode7_Vec3 directionVec = vec3_subtract(point, camera->position);
    
    float roll = atan2f(directionVec.y, directionVec.x);
    float pitch = atan2f(directionVec.z, sqrtf(directionVec.x * directionVec.x + directionVec.y * directionVec.y));
    
    cameraSetAngle(pCamera, roll);
    cameraSetPitch(pCamera, pitch);
}

static void freeCamera(PDMode7_Camera *camera)
{
    _PDMode7_Camera *_camera = camera->prv;
    
    if(!_camera->isManaged)
    {
        playdate->system->realloc(_camera, 0);
        playdate->system->realloc(camera, 0);
    }
}

static PDMode7_Sprite* newSprite(float width, float height, float depth)
{
    PDMode7_Sprite *sprite = playdate->system->realloc(NULL, sizeof(PDMode7_Sprite));
    
    _PDMode7_Sprite *_sprite = playdate->system->realloc(NULL, sizeof(_PDMode7_Sprite));
    sprite->prv = _sprite;
    
    _sprite->world = NULL;
    _sprite->size = newVec3(width, height, depth);
    _sprite->position = newVec3(0, 0, 0);
    _sprite->angle = 0;
    _sprite->pitch = 0;
    
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = playdate->system->realloc(NULL, sizeof(PDMode7_SpriteInstance));
        _PDMode7_SpriteInstance *_instance = playdate->system->realloc(NULL, sizeof(_PDMode7_SpriteInstance));
        instance->prv = _instance;
        
        _instance->index = i;
        _instance->sprite = sprite;
        
        _instance->displayRect = newRect(0, 0, 0, 0);
        _instance->visible = 1;
        _instance->roundingIncrement = newVec2ui(1, 1);
        _instance->imageCenter = newVec2(0.5, 0.5);
        _instance->alignmentX = kMode7SpriteAlignmentNone;
        _instance->alignmentY = kMode7SpriteAlignmentNone;
        _instance->depth = 0;
        _instance->frame = 0;
        _instance->billboardSizeBehavior = kMode7BillboardSizeAutomatic;
        _instance->billboardSize = newVec2(0, 0);
        _instance->bitmapTable = NULL;
        _instance->luaBitmapTable = NULL;
        _instance->bitmap = NULL;
        _instance->drawCallback = NULL;
        
        PDMode7_SpriteDataSource *dataSource = playdate->system->realloc(NULL, sizeof(PDMode7_SpriteDataSource));
        _instance->dataSource = dataSource;
        _PDMode7_SpriteDataSource *_dataSource = playdate->system->realloc(NULL, sizeof(_PDMode7_SpriteDataSource));
        dataSource->prv = _dataSource;
        
        _dataSource->instance = instance;

        _dataSource->minimumWidth = 0;
        _dataSource->maximumWidth = 0;
        
        _dataSource->lengths[kMode7SpriteDataSourceFrame] = 1;
        _dataSource->lengths[kMode7SpriteDataSourceAngle] = 1;
        _dataSource->lengths[kMode7SpriteDataSourcePitch] = 1;
        _dataSource->lengths[kMode7SpriteDataSourceScale] = 1;

        _dataSource->layoutKeys[0] = kMode7SpriteDataSourceFrame;
        _dataSource->layoutKeys[1] = kMode7SpriteDataSourceAngle;
        _dataSource->layoutKeys[2] = kMode7SpriteDataSourcePitch;
        _dataSource->layoutKeys[3] = kMode7SpriteDataSourceScale;

        _sprite->instances[i] = instance;
    }
    
    _sprite->gridCells = newArray();
    _sprite->luaRef = NULL;
    
    _PDMode7_LuaSpriteDataSource *luaDataSource = playdate->system->realloc(NULL, sizeof(_PDMode7_LuaSpriteDataSource));
    luaDataSource->sprite = sprite;
    _sprite->luaDataSource = luaDataSource;
        
    return sprite;
}

static void spriteSetSize(PDMode7_Sprite *sprite, float width, float height, float depth)
{
    _PDMode7_Sprite *_sprite = sprite->prv;
    
    _sprite->size.x = width;
    _sprite->size.y = height;
    _sprite->size.z = depth;
    
    spriteBoundsDidChange(sprite);
}

static PDMode7_Vec3 spriteGetSize(PDMode7_Sprite *sprite)
{
    return ((_PDMode7_Sprite*)sprite->prv)->size;
}

static PDMode7_Vec3 spriteGetPosition(PDMode7_Sprite *sprite)
{
    return ((_PDMode7_Sprite*)sprite->prv)->position;
}

static void spriteSetPosition(PDMode7_Sprite *sprite, float x, float y, float z)
{
    _PDMode7_Sprite *_sprite = sprite->prv;
    
    _sprite->position.x = x;
    _sprite->position.y = y;
    _sprite->position.z = fmaxf(0, z);
    
    spriteBoundsDidChange(sprite);
}

static void spriteBoundsDidChange(PDMode7_Sprite *sprite)
{
    _PDMode7_Sprite *_sprite = sprite->prv;
    
    if(_sprite->world)
    {
        _PDMode7_World *world = _sprite->world->prv;
        gridUpdateSprite(world->grid, sprite);
    }
}

static float spriteGetAngle(PDMode7_Sprite *pSprite)
{
    return ((_PDMode7_Sprite*)pSprite->prv)->angle;
}

static void spriteSetAngle(PDMode7_Sprite *pSprite, float angle)
{
    ((_PDMode7_Sprite*)pSprite->prv)->angle = angle;
    spriteBoundsDidChange(pSprite);
}

static float spriteGetPitch(PDMode7_Sprite *pSprite)
{
    return ((_PDMode7_Sprite*)pSprite->prv)->pitch;
}

static void spriteSetPitch(PDMode7_Sprite *pSprite, float pitch)
{
    ((_PDMode7_Sprite*)pSprite->prv)->pitch = pitch;
}

static void _spriteSetVisible(PDMode7_SpriteInstance *pInstance, int flag)
{
    ((_PDMode7_SpriteInstance*)pInstance->prv)->visible = flag;
}

static void spriteSetVisible(PDMode7_Sprite *pSprite, int flag)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetVisible(((_PDMode7_Sprite*)pSprite->prv)->instances[i], flag);
    }
}

static int _spriteGetVisible(PDMode7_SpriteInstance *pInstance)
{
    return ((_PDMode7_SpriteInstance*)pInstance->prv)->visible;
}

static void _spriteSetImageCenter(PDMode7_SpriteInstance *pInstance, float cx, float cy)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    instance->imageCenter.x = cx;
    instance->imageCenter.y = cy;
}

static void spriteSetImageCenter(PDMode7_Sprite *pSprite, float cx, float cy)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetImageCenter(((_PDMode7_Sprite*)pSprite->prv)->instances[i], cx, cy);
    }
}

static PDMode7_Vec2 _spriteGetImageCenter(PDMode7_SpriteInstance *pInstance)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    return instance->imageCenter;
}

static void _spriteSetFrame(PDMode7_SpriteInstance *pInstance, unsigned int frame)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    instance->frame = frame;
}

static void spriteSetFrame(PDMode7_Sprite *pSprite, unsigned int frame)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetFrame(((_PDMode7_Sprite*)pSprite->prv)->instances[i], frame);
    }
}

static unsigned int _spriteGetFrame(PDMode7_SpriteInstance *pInstance)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    return instance->frame;
}

static void _spriteSetBillboardSizeBehavior(PDMode7_SpriteInstance *pInstance, PDMode7_SpriteBillboardSizeBehavior behavior)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    instance->billboardSizeBehavior = behavior;
}

static void spriteSetBillboardSizeBehavior(PDMode7_Sprite *pSprite, PDMode7_SpriteBillboardSizeBehavior behavior)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetBillboardSizeBehavior(((_PDMode7_Sprite*)pSprite->prv)->instances[i], behavior);
    }
}

static PDMode7_SpriteBillboardSizeBehavior _spriteGetBillboardSizeBehavior(PDMode7_SpriteInstance *pInstance)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    return instance->billboardSizeBehavior;
}

static void _spriteSetBillboardSize(PDMode7_SpriteInstance *pInstance, float width, float height)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    instance->billboardSize.x = width;
    instance->billboardSize.y = height;
}

static void spriteSetBillboardSize(PDMode7_Sprite *pSprite, float width, float height)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetBillboardSize(((_PDMode7_Sprite*)pSprite->prv)->instances[i], width, height);
    }
}

static PDMode7_Vec2 _spriteGetBillboardSize(PDMode7_SpriteInstance *pInstance)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    return instance->billboardSize;
}

static void _spriteSetRoundingIncrement(PDMode7_SpriteInstance *pInstance, unsigned int x, unsigned int y)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    instance->roundingIncrement.x = x;
    instance->roundingIncrement.y = y;
}

static void spriteSetRoundingIncrement(PDMode7_Sprite *pSprite, unsigned int x, unsigned int y)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetRoundingIncrement(((_PDMode7_Sprite*)pSprite->prv)->instances[i], x, y);
    }
}

static void _spriteGetRoundingIncrement(PDMode7_SpriteInstance *pInstance, unsigned int *x, unsigned int *y)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    *x = instance->roundingIncrement.x;
    *y = instance->roundingIncrement.y;
}

static void _spriteSetAlignment(PDMode7_SpriteInstance *pInstance, PDMode7_SpriteAlignment alignmentX, PDMode7_SpriteAlignment alignmentY)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    instance->alignmentX = alignmentX;
    instance->alignmentY = alignmentY;
}

static void spriteSetAlignment(PDMode7_Sprite *pSprite, PDMode7_SpriteAlignment alignmentX, PDMode7_SpriteAlignment alignmentY)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetAlignment(((_PDMode7_Sprite*)pSprite->prv)->instances[i], alignmentX, alignmentY);
    }
}

static void _spriteGetAlignment(PDMode7_SpriteInstance *pInstance, PDMode7_SpriteAlignment *alignmentX, PDMode7_SpriteAlignment *alignmentY)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    *alignmentX = instance->alignmentX;
    *alignmentY = instance->alignmentY;
}

static void _spriteSetDrawCallback(PDMode7_SpriteInstance *pInstance, _PDMode7_Callback *callback)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    if(instance->drawCallback)
    {
        freeCallback(instance->drawCallback);
    }
    instance->drawCallback = callback;
}

static void _spriteSetDrawFunction_c(PDMode7_SpriteInstance *pInstance, PDMode7_SpriteDrawCallbackFunction *function)
{
    _spriteSetDrawCallback(pInstance, newCallback_c(function));
}

static void spriteSetDrawFunction_c(PDMode7_Sprite *pSprite, PDMode7_SpriteDrawCallbackFunction *function)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetDrawCallback(((_PDMode7_Sprite*)pSprite->prv)->instances[i], newCallback_c(function));
    }
}

static void _spriteSetBitmapTable(PDMode7_SpriteInstance *pInstance, LCDBitmapTable *bitmapTable, _PDMode7_LuaBitmapTable *luaBitmapTable)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    if(instance->luaBitmapTable)
    {
        GC_release(instance->luaBitmapTable->luaRef);
    }
    if(luaBitmapTable)
    {
        GC_retain(luaBitmapTable->luaRef);
    }
    instance->bitmapTable = bitmapTable;
    instance->luaBitmapTable = luaBitmapTable;
}

static void _spriteSetBitmapTable_public(PDMode7_SpriteInstance *pInstance, LCDBitmapTable *bitmapTable)
{
    _spriteSetBitmapTable(pInstance, bitmapTable, NULL);
}

static void spriteSetBitmapTable(PDMode7_Sprite *pSprite, LCDBitmapTable *bitmapTable, _PDMode7_LuaBitmapTable *luaBitmapTable)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetBitmapTable(((_PDMode7_Sprite*)pSprite->prv)->instances[i], bitmapTable, luaBitmapTable);
    }
}

static void spriteSetBitmapTable_public(PDMode7_Sprite *pSprite, LCDBitmapTable *bitmapTable)
{
    spriteSetBitmapTable(pSprite, bitmapTable, NULL);
}

static LCDBitmapTable* _spriteGetBitmapTable(PDMode7_SpriteInstance *pInstance)
{
    return ((_PDMode7_SpriteInstance*)pInstance->prv)->bitmapTable;
}

static _PDMode7_LuaBitmapTable* _spriteGetLuaBitmapTable(PDMode7_SpriteInstance *pInstance)
{
    return ((_PDMode7_SpriteInstance*)pInstance->prv)->luaBitmapTable;
}

static PDMode7_SpriteInstance* spriteGetInstance(PDMode7_Sprite *pSprite, PDMode7_Display *pDisplay)
{
    _PDMode7_Sprite *sprite = pSprite->prv;
    PDMode7_World *targetWorld = sprite->world;

    if(!targetWorld)
    {
        // Retrieve world from display
        _PDMode7_Display *display = pDisplay->prv;
        targetWorld = display->world;
    }
    
    if(targetWorld)
    {
        pDisplay = getDisplay(targetWorld, pDisplay);
        
        int displayIndex = indexForDisplay(targetWorld, pDisplay);
        if(displayIndex >= 0)
        {
            return sprite->instances[displayIndex];
        }
    }
    
    return NULL;
}

static PDMode7_SpriteInstance** spriteGetInstances(PDMode7_Sprite *pSprite, int *length)
{
    _PDMode7_Sprite *sprite = pSprite->prv;
    
    *length = MODE7_MAX_DISPLAYS;
    return sprite->instances;
}

static void _spriteSetUserData(PDMode7_SpriteInstance *pInstance, void *userdata)
{
    ((_PDMode7_SpriteInstance*)pInstance->prv)->userdata = userdata;
}

static void spriteSetUserData(PDMode7_Sprite *pSprite, void *userdata)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetUserData(((_PDMode7_Sprite*)pSprite->prv)->instances[i], userdata);
    }
}

static void* _spriteGetUserData(PDMode7_SpriteInstance *pInstance)
{
    return ((_PDMode7_SpriteInstance*)pInstance->prv)->userdata;
}

static LCDBitmap* _spriteGetBitmap(PDMode7_SpriteInstance *pInstance)
{
    return ((_PDMode7_SpriteInstance*)pInstance->prv)->bitmap;
}

static PDMode7_Rect _spriteGetDisplayRect(PDMode7_SpriteInstance *pInstance)
{
    return ((_PDMode7_SpriteInstance*)pInstance->prv)->displayRect;
}

static int _spriteIsVisibleOnDisplay(PDMode7_SpriteInstance *pInstance)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    _PDMode7_Sprite *sprite = instance->sprite->prv;

    if(sprite->world)
    {
        _PDMode7_World *world = sprite->world->prv;
        _PDMode7_Display *display = world->displays[instance->index]->prv;
        int index = arrayIndexOf(display->visibleInstances, pInstance);
        if(index >= 0)
        {
            return 1;
        }
    }
    return 0;
}

static PDMode7_World* spriteGetWorld(PDMode7_Sprite *pSprite)
{
    return ((_PDMode7_Sprite*)pSprite->prv)->world;
}

static void removeSprite(PDMode7_Sprite *pSprite)
{
    _PDMode7_Sprite *sprite = pSprite->prv;
    
    if(sprite->world)
    {
        _PDMode7_World *world = sprite->world->prv;
        
        gridRemoveSprite(world->grid, pSprite);
        
        int index = arrayIndexOf(world->sprites, pSprite);
        if(index >= 0)
        {
            arrayRemove(world->sprites, index);
        }
        
        // Unlink world
        sprite->world = NULL;
        
        if(sprite->luaRef)
        {
            // Save luaRef before setting it to NULL
            LuaUDObject *luaRef = sprite->luaRef;
            sprite->luaRef = NULL;
            playdate->lua->releaseObject(luaRef);
        }
    }
}

static void freeSprite(PDMode7_Sprite *sprite)
{
    _PDMode7_Sprite *_sprite = sprite->prv;

    removeSprite(sprite);
    
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = _sprite->instances[i];
        _PDMode7_SpriteInstance *_instance = instance->prv;
        
        if(_instance->luaBitmapTable)
        {
            GC_release(_instance->luaBitmapTable->luaRef);
        }
        
        if(_instance->drawCallback)
        {
            freeCallback(_instance->drawCallback);
        }
        
        playdate->system->realloc(_instance->dataSource->prv, 0);
        playdate->system->realloc(_instance->dataSource, 0);

        playdate->system->realloc(_instance, 0);
        playdate->system->realloc(instance, 0);
    }
    
    freeArray(_sprite->gridCells);
    
    playdate->system->realloc(_sprite->luaDataSource, 0);
    
    playdate->system->realloc(_sprite, 0);
    playdate->system->realloc(sprite, 0);
}

static unsigned int _spriteDataSourceGetLength(PDMode7_SpriteDataSource *dataSource, PDMode7_SpriteDataSourceKey key)
{
    _PDMode7_SpriteDataSource *_dataSource = dataSource->prv;
    
    if(key >= 0 && key < MODE7_SPRITE_DSOURCE_LEN)
    {
        return _dataSource->lengths[key];
    }
    
    return 1;
}

static void _spriteDataSourceSetLength(PDMode7_SpriteDataSource *dataSource, unsigned int length, PDMode7_SpriteDataSourceKey key)
{
    _PDMode7_SpriteDataSource *_dataSource = dataSource->prv;
    
    if(length > 0)
    {
        _dataSource->lengths[key] = length;
    }
}

static void spriteDataSourceSetLength(PDMode7_Sprite *pSprite, unsigned int length, PDMode7_SpriteDataSourceKey key)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _PDMode7_SpriteInstance *instance = ((_PDMode7_Sprite*)pSprite->prv)->instances[i]->prv;
        _spriteDataSourceSetLength(instance->dataSource, length, key);
    }
}

static unsigned int spriteGetTableIndex(PDMode7_SpriteInstance *pInstance, unsigned int angleIndex, unsigned int pitchIndex, unsigned int scaleIndex)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    
    int tableIndex = 0;
    
    for(int i = 0; i < MODE7_SPRITE_DSOURCE_LEN; i++)
    {
        PDMode7_SpriteDataSource *pDataSource = instance->dataSource;
        _PDMode7_SpriteDataSource *dataSource = pDataSource->prv;
        PDMode7_SpriteDataSourceKey k1 = dataSource->layoutKeys[i];
        
        unsigned int kIndex = 0;
        switch(k1)
        {
            case kMode7SpriteDataSourceFrame: kIndex = instance->frame; break;
            case kMode7SpriteDataSourceAngle: kIndex = angleIndex; break;
            case kMode7SpriteDataSourcePitch: kIndex = pitchIndex; break;
            case kMode7SpriteDataSourceScale: kIndex = scaleIndex; break;
        }
        
        for(int j = (i + 1); j < MODE7_SPRITE_DSOURCE_LEN; j++)
        {
            PDMode7_SpriteDataSourceKey k2 = dataSource->layoutKeys[j];
            kIndex *= _spriteDataSourceGetLength(pDataSource, k2);
        }
        
        tableIndex += kIndex;
    }
    
    return tableIndex;
}

static void _spriteDataSourceGetLayout(PDMode7_SpriteDataSource *pDataSource, PDMode7_SpriteDataSourceKey *k1, PDMode7_SpriteDataSourceKey *k2, PDMode7_SpriteDataSourceKey *k3, PDMode7_SpriteDataSourceKey *k4)
{
    _PDMode7_SpriteDataSource *dataSource = pDataSource->prv;
    *k1 = dataSource->layoutKeys[0];
    *k2 = dataSource->layoutKeys[1];
    *k3 = dataSource->layoutKeys[2];
    *k4 = dataSource->layoutKeys[3];
}

static void _spriteDataSourceSetLayout(PDMode7_SpriteDataSource *pDataSource, PDMode7_SpriteDataSourceKey k1, PDMode7_SpriteDataSourceKey k2, PDMode7_SpriteDataSourceKey k3, PDMode7_SpriteDataSourceKey k4)
{
    _PDMode7_SpriteDataSource *dataSource = pDataSource->prv;
    
    if(k1 != k2 && k1 != k3 && k1 != k4 && k2 != k3 && k2 != k4 && k3 != k4)
    {
        dataSource->layoutKeys[0] = k1;
        dataSource->layoutKeys[1] = k2;
        dataSource->layoutKeys[2] = k3;
        dataSource->layoutKeys[3] = k4;
    }
}

static void spriteDataSourceSetLayout(PDMode7_Sprite *pSprite, PDMode7_SpriteDataSourceKey k1, PDMode7_SpriteDataSourceKey k2, PDMode7_SpriteDataSourceKey k3,  PDMode7_SpriteDataSourceKey k4)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _PDMode7_SpriteInstance *instance = ((_PDMode7_Sprite*)pSprite->prv)->instances[i]->prv;
        _spriteDataSourceSetLayout(instance->dataSource, k1, k2, k3, k4);
    }
}

static int _spriteDataSourceGetMinimumWidth(PDMode7_SpriteDataSource *pDataSource)
{
    return ((_PDMode7_SpriteDataSource*)pDataSource->prv)->minimumWidth;
}

static void _spriteDataSourceSetMinimumWidth(PDMode7_SpriteDataSource *pDataSource, int minimumWidth)
{
    ((_PDMode7_SpriteDataSource*)pDataSource->prv)->minimumWidth = minimumWidth;
}

static void spriteDataSourceSetMinimumWidth(PDMode7_Sprite *pSprite, int minimumWidth)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _PDMode7_SpriteInstance *instance = ((_PDMode7_Sprite*)pSprite->prv)->instances[i]->prv;
        _spriteDataSourceSetMinimumWidth(instance->dataSource, minimumWidth);
    }
}

static int _spriteDataSourceGetMaximumWidth(PDMode7_SpriteDataSource *pDataSource)
{
    return ((_PDMode7_SpriteDataSource*)pDataSource->prv)->maximumWidth;
}

static void _spriteDataSourceSetMaximumWidth(PDMode7_SpriteDataSource *pDataSource, int maximumWidth)
{
    ((_PDMode7_SpriteDataSource*)pDataSource->prv)->maximumWidth = maximumWidth;
}

static void spriteDataSourceSetMaximumWidth(PDMode7_Sprite *pSprite, int maximumWidth)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _PDMode7_SpriteInstance *instance = ((_PDMode7_Sprite*)pSprite->prv)->instances[i]->prv;
        _spriteDataSourceSetMaximumWidth(instance->dataSource, maximumWidth);
    }
}

static PDMode7_SpriteDataSource* _spriteGetDataSource(PDMode7_SpriteInstance *pInstance)
{
    return ((_PDMode7_SpriteInstance*)pInstance->prv)->dataSource;
}

static PDMode7_Sprite* spriteInstanceGetSprite(PDMode7_SpriteInstance *pInstance)
{
    return ((_PDMode7_SpriteInstance*)pInstance->prv)->sprite;
}

static inline PDMode7_Rect newRect(int x, int y, int width, int height)
{
    return (PDMode7_Rect){
        .x = x,
        .y = y,
        .width = width,
        .height = height
    };
}

static int rectIntersect(PDMode7_Rect rect1, PDMode7_Rect rect2)
{
    return (rect1.x < (rect2.x + rect2.width) &&
            rect2.x < (rect1.x + rect1.width) &&
            rect1.y < (rect2.y + rect2.height) &&
            rect2.y < (rect1.y + rect1.height));
}

static void rectAdjust(PDMode7_Rect rect, int width, int height, PDMode7_Rect *adjustedRect, int *offsetX, int *offsetY)
{
    int x1; int x2; int y1; int y2; *offsetX = 0; *offsetY = 0;
    if(rect.x >= 0)
    {
        x1 = rect.x;
    }
    else
    {
        x1 = 0;
        *offsetX = -rect.x;
    }
    if(rect.y >= 0)
    {
        y1 = rect.y;
    }
    else
    {
        y1 = 0;
        *offsetY = -rect.y;
    }
    x2 = mode7_min(x1 - *offsetX + rect.width, width);
    y2 = mode7_min(y1 - *offsetY + rect.height, height);
    *adjustedRect = newRect(x1, y1, x2 - x1, y2 - y1);
}

static inline PDMode7_Vec3 newVec3(float x, float y, float z)
{
    return (PDMode7_Vec3){
        .x = x,
        .y = y,
        .z = z
    };
}

static inline PDMode7_Vec2 newVec2(float x, float y)
{
    return (PDMode7_Vec2){
        .x = x,
        .y = y
    };
}

static inline PDMode7_Vec2ui newVec2ui(unsigned int x, unsigned int y)
{
    return (PDMode7_Vec2ui){
        .x = x,
        .y = y
    };
}

static PDMode7_Vec3 vec3_subtract(PDMode7_Vec3 v1, PDMode7_Vec3 v2)
{
    return newVec3(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}

static float vec3_dot(PDMode7_Vec3 v1, PDMode7_Vec3 v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

static inline PDMode7_Color newGrayscaleColor(uint8_t gray, uint8_t alpha)
{
    return (PDMode7_Color){
        .space = kPDMode7ColorSpaceGrayscale,
        .gray = gray,
        .alpha = alpha
    };
}

static _PDMode7_Grid* newGrid(float width, float height, float depth, int cellSize)
{
    _PDMode7_Grid *grid = playdate->system->realloc(NULL, sizeof(_PDMode7_Grid));
    
    grid->cellSize = cellSize;
    
    grid->widthLen = ceilf(width / cellSize);
    grid->heightLen = ceilf(height / cellSize);
    grid->depthLen = ceilf(depth / cellSize);
    
    grid->numberOfCells = grid->widthLen * grid->heightLen * grid->depthLen;
    grid->cells = playdate->system->realloc(NULL, grid->numberOfCells * sizeof(_PDMode7_GridCell*));
    
    for(int i = 0; i < grid->numberOfCells; i++)
    {
        _PDMode7_GridCell *cell = playdate->system->realloc(NULL, sizeof(_PDMode7_GridCell));
        cell->sprites = newArray();
        grid->cells[i] = cell;
    }
    
    return grid;
}

static int gridIndexAtX(_PDMode7_Grid *grid, float x)
{
    return fmaxf(0, fminf(floorf(x / grid->cellSize), grid->widthLen - 1));
}

static int gridIndexAtY(_PDMode7_Grid *grid, float y)
{
    return fmaxf(0, fminf(floorf(y / grid->cellSize), grid->heightLen - 1));
}

static int gridIndexAtZ(_PDMode7_Grid *grid, float z)
{
    return fmaxf(0, fminf(floorf(z / grid->cellSize), grid->depthLen - 1));
}

static int gridIndexFor(_PDMode7_Grid *grid, int widthIndex, int heightIndex, int depthIndex)
{
    return depthIndex * (grid->widthLen * grid->heightLen) + widthIndex * grid->widthLen + heightIndex;
}

static _PDMode7_Array* gridGetSpritesAtPoint(_PDMode7_Grid *grid, PDMode7_Vec3 point, int distanceUnits)
{
    _PDMode7_Array *results = newArray();

    int midX = gridIndexAtY(grid, point.x);
    int startX = fmaxf(midX - distanceUnits, 0);
    int endX = fminf(midX + distanceUnits, grid->widthLen - 1);
    
    int midY = gridIndexAtY(grid, point.y);
    int startY = fmaxf(midY - distanceUnits, 0);
    int endY = fminf(midY + distanceUnits, grid->heightLen - 1);
    
    int midZ = gridIndexAtZ(grid, point.z);
    int startZ = fmaxf(midZ - distanceUnits, 0);
    int endZ = fminf(midZ + distanceUnits, grid->depthLen - 1);
    
    for(int z = startZ; z <= endZ; z++)
    {
        for(int x = startX; x <= endX; x++)
        {
            for(int y = startY; y <= endY; y++)
            {
                int cellIndex = gridIndexFor(grid, x, y, z);
                _PDMode7_GridCell *cell = grid->cells[cellIndex];
                
                for(int i = 0; i < cell->sprites->length; i++)
                {
                    PDMode7_Sprite *sprite = cell->sprites->items[i];
                    if(arrayIndexOf(results, sprite) < 0)
                    {
                        arrayPush(results, sprite);
                    }
                }
            }
        }
    }
    
    return results;
}

static void gridUpdateSprite(_PDMode7_Grid *grid, PDMode7_Sprite *pSprite)
{
    _PDMode7_Sprite *sprite = pSprite->prv;
    
    gridRemoveSprite(grid, pSprite);
    
    float boundsWidth = fabsf(sprite->size.x * cosf(sprite->angle)) + fabsf(sprite->size.y * sinf(sprite->angle));
    float boundsHeight = fabsf(sprite->size.x * sinf(sprite->angle)) + fabsf(sprite->size.y * cosf(sprite->angle));

    int startX = gridIndexAtX(grid, sprite->position.x - boundsWidth * 0.5f);
    int endX = gridIndexAtX(grid, sprite->position.x + boundsWidth * 0.5f);
    
    int startY = gridIndexAtY(grid, sprite->position.y - boundsHeight * 0.5f);
    int endY = gridIndexAtY(grid, sprite->position.y + boundsHeight * 0.5f);
    
    int startZ = gridIndexAtZ(grid, sprite->position.z - sprite->size.z * 0.5f);
    int endZ = gridIndexAtZ(grid, sprite->position.z + sprite->size.z * 0.5f);

    for(int z = startZ; z <= endZ; z++)
    {
        for(int x = startX; x <= endX; x++)
        {
            for(int y = startY; y <= endY; y++)
            {
                int cellIndex = gridIndexFor(grid, x, y, z);
                _PDMode7_GridCell *cell = grid->cells[cellIndex];
                                
                arrayPush(cell->sprites, pSprite);
                arrayPush(sprite->gridCells, cell);
            }
        }
    }
}

static void gridRemoveSprite(_PDMode7_Grid *grid, PDMode7_Sprite *pSprite)
{
    _PDMode7_Sprite *sprite = pSprite->prv;
    
    for(int i = 0; i < sprite->gridCells->length; i++)
    {
        _PDMode7_GridCell *cell = sprite->gridCells->items[i];
        int index = arrayIndexOf(cell->sprites, pSprite);
        if(index >= 0)
        {
            arrayRemove(cell->sprites, index);
        }
    }
    
    arrayClear(sprite->gridCells);
}

static void freeGrid(_PDMode7_Grid *grid)
{
    for(int i = 0; i < grid->numberOfCells; i++)
    {
        _PDMode7_GridCell *cell = grid->cells[i];
        freeArray(cell->sprites);
        playdate->system->realloc(cell, 0);
    }
    
    playdate->system->realloc(grid->cells, 0);
    playdate->system->realloc(grid, 0);
}

static _PDMode7_Array* newArray(void)
{
    _PDMode7_Array *array = playdate->system->realloc(NULL, sizeof(_PDMode7_Array));
    
    array->items = NULL;
    array->length = 0;
    
    return array;
}

static void arrayPush(_PDMode7_Array *array, void *item)
{
    array->length++;
    array->items = playdate->system->realloc(array->items, array->length * sizeof(void*));
    array->items[array->length - 1] = item;
}

static void arrayRemove(_PDMode7_Array *array, int index)
{
    if(index >= 0 && index < array->length)
    {
        for(int i = index; i < (array->length - 1); i++)
        {
            array->items[i] = array->items[i + 1];
        }
        
        array->length--;
        
        array->items = playdate->system->realloc(array->items, array->length * sizeof(void*));
    }
}

static int arrayIndexOf(_PDMode7_Array *array, void *item)
{
    for(int i = 0; i < array->length; i++)
    {
        if(array->items[i] == item)
        {
            return i;
        }
    }
    
    return -1;
}

static void arrayClear(_PDMode7_Array *array)
{
    if(array->items)
    {
        playdate->system->realloc(array->items, 0);
    }
    array->items = NULL;
    array->length = 0;
}

static void freeArray(_PDMode7_Array *array)
{
    if(array->items)
    {
        playdate->system->realloc(array->items, 0);
    }
    playdate->system->realloc(array, 0);
}

static _PDMode7_LuaArray* newLuaArray(_PDMode7_Array *array, kPDMode7LuaItem type, int freeArray)
{
    _PDMode7_LuaArray *luaArray = playdate->system->realloc(NULL, sizeof(_PDMode7_LuaArray));
    luaArray->array = array;
    luaArray->type = type;
    luaArray->freeArray = freeArray;
    return luaArray;
}

static void freeLuaArray(_PDMode7_LuaArray *luaArray)
{
    if(luaArray->freeArray)
    {
        freeArray(luaArray->array);
    }
    playdate->system->realloc(luaArray, 0);
}

static _PDMode7_LuaBitmapTable* newLuaBitmapTable(const char *filename)
{
    LCDBitmapTable *LCDBitmapTable = playdate->graphics->loadBitmapTable(filename, NULL);
    if(LCDBitmapTable)
    {
        _PDMode7_LuaBitmapTable *bitmapTable = playdate->system->realloc(NULL, sizeof(_PDMode7_LuaBitmapTable));
        bitmapTable->LCDBitmapTable = LCDBitmapTable;
        bitmapTable->luaRef = NULL;
        return bitmapTable;
    }
    return NULL;
}

static void freeLuaBitmapTable(_PDMode7_LuaBitmapTable *bitmapTable)
{
    playdate->graphics->freeBitmapTable(bitmapTable->LCDBitmapTable);
    playdate->system->realloc(bitmapTable, 0);
}

static _PDMode7_LuaBitmap* newLuaBitmap(const char *filename)
{
    LCDBitmap *LCDBitmap = playdate->graphics->loadBitmap(filename, NULL);
    if(LCDBitmap)
    {
        _PDMode7_LuaBitmap *bitmap = playdate->system->realloc(NULL, sizeof(_PDMode7_LuaBitmap));
        bitmap->LCDBitmap = LCDBitmap;
        bitmap->luaRef = NULL;
        return bitmap;
    }
    return NULL;
}

static void freeLuaBitmap(_PDMode7_LuaBitmap *bitmap)
{
    playdate->graphics->freeBitmap(bitmap->LCDBitmap);
    playdate->system->realloc(bitmap, 0);
}

static _PDMode7_Callback* newCallback_c(void *function)
{
    _PDMode7_Callback *callback = playdate->system->realloc(NULL, sizeof(_PDMode7_Callback));
    callback->type = _PDMode7_CallbackTypeC;
    callback->cFunction = function;
    callback->luaFunction = NULL;
    return callback;
}

static _PDMode7_Callback* newCallback_lua(const char *functionName)
{
    _PDMode7_Callback *callback = playdate->system->realloc(NULL, sizeof(_PDMode7_Callback));
    callback->type = _PDMode7_CallbackTypeLua;
    callback->luaFunction = playdate->system->realloc(NULL, strlen(functionName) + 1);
    strcpy(callback->luaFunction, functionName);
    callback->cFunction = NULL;
    return callback;
}

static void freeCallback(_PDMode7_Callback *callback)
{
    if(callback->luaFunction)
    {
        playdate->system->realloc(callback->luaFunction, 0);
    }
    playdate->system->realloc(callback, 0);
}

static _PDMode7_GCRef* newGCRef(LuaUDObject *luaRef)
{
    _PDMode7_GCRef *reference = playdate->system->realloc(NULL, sizeof(_PDMode7_GCRef));
    reference->count = 0;
    reference->luaRef = luaRef;
    return reference;
}

static void freeGCRef(_PDMode7_GCRef *reference)
{
    playdate->system->realloc(reference, 0);
}

static _PDMode7_GC* newGC(void)
{
    _PDMode7_GC *gc = playdate->system->realloc(NULL, sizeof(_PDMode7_GC));
    gc->references = newArray();
    return gc;
}

static _PDMode7_GCRef* GC_get(_PDMode7_GC *gc, LuaUDObject *luaRef, int *outIndex)
{
    if(outIndex)
    {
        *outIndex = -1;
    }
    
    for(int i = 0; i < gc->references->length; i++)
    {
        _PDMode7_GCRef *ref = gc->references->items[i];
        if(ref->luaRef == luaRef)
        {
            if(outIndex)
            {
                *outIndex = i;
            }
            return ref;
        }
    }
    return NULL;
}

static _PDMode7_GCRef* GC_retain(LuaUDObject *luaRef)
{
    _PDMode7_GCRef *ref = GC_get(gc, luaRef, NULL);
    if(!ref)
    {
        ref = newGCRef(luaRef);
        arrayPush(gc->references, ref);
        playdate->lua->retainObject(luaRef);
    }
    ref->count++;
    return ref;
}

static void GC_release(LuaUDObject *luaRef)
{
    int index;
    _PDMode7_GCRef *ref = GC_get(gc, luaRef, &index);
    if(ref)
    {
        ref->count--;
        if(ref->count <= 0)
        {
            playdate->lua->releaseObject(ref->luaRef);
            arrayRemove(gc->references, index);
            freeGCRef(ref);
        }
    }
}

static _PDMode7_Pool* newPool(void)
{
    _PDMode7_Pool *pool = playdate->system->realloc(NULL, sizeof(_PDMode7_Pool));
    pool->ptr = NULL;
    pool->originalPtr = NULL;
    return pool;
}

static void poolRealloc(_PDMode7_Pool *pool, size_t size)
{
    pool->originalPtr = playdate->system->realloc(pool->originalPtr, size);
    if(!pool->ptr)
    {
        pool->ptr = pool->originalPtr;
    }
}

static void poolRealloc_public(size_t size)
{
    poolRealloc(pool, size);
}

static void poolClear(_PDMode7_Pool *pool)
{
    pool->ptr = pool->originalPtr;
}

static void poolClear_public(void)
{
    poolClear(pool);
}

typedef struct {
    char *data;
    int position;
    int size;
} pgm_buffer;

static int pgm_getc(pgm_buffer *buffer)
{
    if(buffer->position == buffer->size)
    {
        // Buffer is empty
        return EOF;
    }
    
    return buffer->data[buffer->position++];
}

static int pgm_scan_int(pgm_buffer *buffer, int *n)
{
    *n = 0;
    
    int start = buffer->position;
    int end = buffer->position;

    while(isdigit(pgm_getc(buffer)))
    {
        end = buffer->position;
    }
    
    buffer->position = end;
    
    int len = end - start;
    if(len > 0)
    {
        char *str = playdate->system->realloc(NULL, len + 1);
        strncpy(str, buffer->data + start, len);
        str[len] = '\0';
        
        *n = atoi(str);
        playdate->system->realloc(str, 0);
        return 1;
    }
    
    return 0;
}

static void pgm_skip_comments(pgm_buffer *buffer)
{
    int ch;
    
    // Skip spaces
    while(isspace(ch = buffer->data[buffer->position]))
    {
        buffer->position++;
    }
    
    while((ch = buffer->data[buffer->position]) == '#')
    {
        buffer->position++;
        while((ch = pgm_getc(buffer)) != '\n' && ch != EOF);
        // Skip spaces
        while(isspace(ch = buffer->data[buffer->position]))
        {
            buffer->position++;
        }
    }
}

static PDMode7_Bitmap* loadPGM(const char *filename)
{
    SDFile *file = playdate->file->open(filename, kFileRead);
    if(!file)
    {
        return NULL;
    }
        
    playdate->file->seek(file, 0, SEEK_END);
    int fileSize = playdate->file->tell(file);
    playdate->file->seek(file, 0, SEEK_SET);
    
    playdate->file->read(file, pool->ptr, fileSize);
    playdate->file->close(file);
    
    pgm_buffer *buffer = playdate->system->realloc(NULL, sizeof(pgm_buffer));
    
    buffer->data = pool->ptr;
    buffer->position = 0;
    buffer->size = fileSize;
    
    char char1 = pgm_getc(buffer);
    char char2 = pgm_getc(buffer);
    pgm_skip_comments(buffer);
    
    int width; int height; int max;
    
    int c1 = pgm_scan_int(buffer, &width);
    pgm_skip_comments(buffer);
    
    int c2 = pgm_scan_int(buffer, &height);
    pgm_skip_comments(buffer);
    
    int c3 = pgm_scan_int(buffer, &max);
    
    if (char1 != 'P' || char2 != '5' || c1 != 1 || c2 != 1 || c3 != 1 || max > 255)
    {
        playdate->system->realloc(buffer, 0);
        return NULL;
    }
    
    pool->ptr += fileSize;
    
    // Discard one byte after header
    pgm_getc(buffer);
    
    PDMode7_Bitmap *bitmap = playdate->system->realloc(NULL, sizeof(PDMode7_Bitmap));
    _PDMode7_Bitmap *_bitmap = playdate->system->realloc(NULL, sizeof(_PDMode7_Bitmap));
    bitmap->prv = _bitmap;
    
    _bitmap->width = width;
    _bitmap->height = height;
    _bitmap->data = (uint8_t*)buffer->data + buffer->position;
    _bitmap->mask = NULL;
    _bitmap->layers = newArray();
    _bitmap->luaRef = NULL;
    _bitmap->released = 0;
    
    playdate->system->realloc(buffer, 0);
    
    return bitmap;
}

static void bitmapGetSize(PDMode7_Bitmap *pBitmap, int *width, int *height)
{
    _PDMode7_Bitmap *bitmap = pBitmap->prv;
    
    *width = bitmap->width;
    *height = bitmap->height;
}

static PDMode7_Color bitmapColorAt(PDMode7_Bitmap *pBitmap, int x, int y)
{
    _PDMode7_Bitmap *bitmap = pBitmap->prv;
    
    if(x >= 0 && x < bitmap->width && y >= 0 && y < bitmap->height)
    {
        int i = y * bitmap->width + x;
        uint8_t gray = bitmap->data[i];
        uint8_t alpha = 255;
        
        if(bitmap->mask)
        {
            _PDMode7_Bitmap *mask = bitmap->mask->prv;
            alpha = mask->data[i];
        }
        
        return newGrayscaleColor(gray, alpha);
    }
    
    return newGrayscaleColor(0, 0);
}

static PDMode7_Bitmap* bitmapGetMask(PDMode7_Bitmap *bitmap)
{
    _PDMode7_Bitmap *_bitmap = bitmap->prv;
    return _bitmap->mask;
}

static void bitmapSetMask(PDMode7_Bitmap *bitmap, PDMode7_Bitmap *mask)
{
    _PDMode7_Bitmap *_bitmap = bitmap->prv;
    
    if(mask)
    {
        // Mask size must be equal to bitmap size
        _PDMode7_Bitmap *_mask = mask->prv;
        if((_bitmap->width != _mask->width) || (_bitmap->height != _mask->height))
        {
            return;
        }
    }
    
    if(_bitmap->mask)
    {
        _PDMode7_Bitmap *currentMask = _bitmap->mask->prv;
        if(currentMask->luaRef)
        {
            GC_release(currentMask->luaRef);
        }
    }
    
    if(mask)
    {
        _PDMode7_Bitmap *_mask = mask->prv;
        if(_mask->luaRef)
        {
            GC_retain(_mask->luaRef);
        }
    }
    
    _bitmap->mask = mask;
}

static void bitmapAddLayer(PDMode7_Bitmap *pBitmap, PDMode7_BitmapLayer *pLayer)
{
    _PDMode7_Bitmap *bitmap = pBitmap->prv;
    _PDMode7_BitmapLayer *layer = pLayer->prv;

    if(!layer->parentBitmap)
    {
        int index = arrayIndexOf(bitmap->layers, layer);
        if(index < 0)
        {
            if(layer->luaRef)
            {
                playdate->lua->retainObject(layer->luaRef);
            }
            
            layer->parentBitmap = pBitmap;
            arrayPush(bitmap->layers, pLayer);
            
            bitmapLayerDidChange(pLayer);
            bitmapLayerDraw(pLayer);
        }
    }
}

static _PDMode7_Array* _bitmapGetLayers(PDMode7_Bitmap *pBitmap)
{
    _PDMode7_Bitmap *bitmap = pBitmap->prv;
    return bitmap->layers;
}

static PDMode7_BitmapLayer** bitmapGetLayers(PDMode7_Bitmap *pBitmap, int *len)
{
    _PDMode7_Array *layers = _bitmapGetLayers(pBitmap);
    *len = layers->length;
    return (PDMode7_BitmapLayer**)layers->items;
}

static void bitmapRemoveAllLayers(PDMode7_Bitmap *pBitmap)
{
    _PDMode7_Bitmap *bitmap = pBitmap->prv;
    for(int i = bitmap->layers->length - 1; i >= 0; i--)
    {
        PDMode7_BitmapLayer *layer = bitmap->layers->items[i];
        removeBitmapLayer(layer);
    }
}

static void releaseBitmap(PDMode7_Bitmap *pBitmap)
{
    _PDMode7_Bitmap *bitmap = pBitmap->prv;
    if(bitmap->luaRef)
    {
        GC_release(bitmap->luaRef);
    }
}

static void freeBitmap(PDMode7_Bitmap *pBitmap)
{
    _PDMode7_Bitmap *bitmap = pBitmap->prv;

    for(int i = bitmap->layers->length - 1; i >= 0; i--)
    {
        PDMode7_BitmapLayer *layer = bitmap->layers->items[i];
        _removeBitmapLayer(layer, 0);
    }
    
    freeArray(bitmap->layers);
    
    if(bitmap->mask)
    {
        _PDMode7_Bitmap *_mask = bitmap->mask->prv;
        if(_mask->luaRef)
        {
            GC_release(_mask->luaRef);
        }
    }
    
    playdate->system->realloc(bitmap, 0);
    playdate->system->realloc(pBitmap, 0);
}

static PDMode7_BitmapLayer* newBitmapLayer(PDMode7_Bitmap *bitmap)
{
    PDMode7_BitmapLayer *layer = playdate->system->realloc(NULL, sizeof(PDMode7_BitmapLayer));
    _PDMode7_BitmapLayer *_layer = playdate->system->realloc(NULL, sizeof(_PDMode7_BitmapLayer));
    layer->prv = _layer;
    
    _layer->bitmap = NULL;
    _layer->parentBitmap = NULL;
    _layer->rect = newRect(0, 0, 0, 0);
    _layer->canRestore = 0;
    _layer->enabled = 0;
    _layer->visible = 1;
    
    _layer->comp = (_PDMode7_BitmapLayerCompositing){
        .data = NULL,
        .size = 0,
        .rect = newRect(0, 0, 0, 0),
        .offsetX = 0,
        .offsetY = 0,
    };
    
    _layer->luaRef = NULL;
    
    bitmapLayerSetBitmap(layer, bitmap);

    return layer;
}

static void bitmapLayerRestore(PDMode7_BitmapLayer *pLayer)
{
    _PDMode7_BitmapLayer *layer = pLayer->prv;
    
    if(layer->parentBitmap && layer->canRestore)
    {
        _PDMode7_Bitmap *parentBitmap = layer->parentBitmap->prv;
        
        for(int y = 0; y < layer->comp.rect.height; y++)
        {
            int src_y = layer->comp.offsetY + y;
            int dst_y = layer->comp.rect.y + y;
            int src_offset = src_y * layer->rect.width + layer->comp.offsetX;
            int dst_offset = dst_y * parentBitmap->width + layer->comp.rect.x;

            memcpy(parentBitmap->data + dst_offset, layer->comp.data + src_offset, layer->comp.rect.width);
        }
    }
}

static void bitmapLayerDraw(PDMode7_BitmapLayer *pLayer)
{
    _PDMode7_BitmapLayer *layer = pLayer->prv;

    if(layer->parentBitmap && layer->bitmap && layer->enabled)
    {
        _PDMode7_Bitmap *parentBitmap = layer->parentBitmap->prv;
        _PDMode7_Bitmap *layerBitmap = layer->bitmap->prv;
        
        if((layer->rect.x + layer->rect.width) <= 0 || layer->rect.x >= parentBitmap->width || (layer->rect.y + layer->rect.height) <= 0 || layer->rect.y >= parentBitmap->height)
        {
            return;
        }
        
        PDMode7_Rect outRect; int offsetX; int offsetY;
        rectAdjust(layer->rect, parentBitmap->width, parentBitmap->height, &outRect, &offsetX, &offsetY);
        
        layer->comp.rect = outRect;
        layer->comp.offsetX = offsetX;
        layer->comp.offsetY = offsetY;
        
        for(int y = 0; y < layer->comp.rect.height; y++)
        {
            int src_y = layer->comp.offsetY + y;
            int dst_y = layer->comp.rect.y + y;
            int src_offset = src_y * layer->rect.width + layer->comp.offsetX;
            int dst_offset = dst_y * parentBitmap->width + layer->comp.rect.x;
            
            memcpy(layer->comp.data + src_offset, parentBitmap->data + dst_offset, layer->comp.rect.width);
        }
        
        layer->canRestore = 1;
        
        _PDMode7_Bitmap *layerMask = NULL;
        if(layerBitmap->mask)
        {
            layerMask = layerBitmap->mask->prv;
        }
        
        for(int y = 0; y < layer->comp.rect.height; y++)
        {
            int src_y = layer->comp.offsetY + y;
            int dst_y = layer->comp.rect.y + y;
            int src_rows = src_y * layer->rect.width + layer->comp.offsetX;
            int dst_rows = dst_y * parentBitmap->width + layer->comp.rect.x;
            
            if(layerMask)
            {
                for(int x = 0; x < layer->comp.rect.width; x++)
                {
                    int src_offset = src_rows + x;
                    int dst_offset = dst_rows + x;
                    
                    uint8_t color = layerBitmap->data[src_offset];
                    uint8_t backgroundColor = parentBitmap->data[dst_offset];
                    
                    uint8_t alpha = layerMask->data[src_offset];
                    color = (alpha * (color - backgroundColor)) / 255 + backgroundColor;
                    
                    parentBitmap->data[dst_offset] = color;
                }
            }
            else
            {
                memcpy(parentBitmap->data + dst_rows, layerBitmap->data + src_rows, layer->comp.rect.width);
            }
        }
    }
}

static PDMode7_Bitmap* bitmapLayerGetBitmap(PDMode7_BitmapLayer *layer)
{
    return ((_PDMode7_BitmapLayer*)layer->prv)->bitmap;
}

static void bitmapLayerSetBitmap(PDMode7_BitmapLayer *layer, PDMode7_Bitmap *bitmap)
{
    _PDMode7_BitmapLayer *_layer = layer->prv;
    _PDMode7_Bitmap *_bitmap = bitmap->prv;
    
    bitmapLayerRestore(layer);
    
    if(_layer->bitmap)
    {
        _PDMode7_Bitmap *_currentBitmap = _layer->bitmap->prv;
        if(_currentBitmap->luaRef)
        {
            GC_release(_currentBitmap->luaRef);
        }
    }
    
    if(_bitmap->luaRef)
    {
        GC_retain(_bitmap->luaRef);
    }
    
    _layer->bitmap = bitmap;
    
    _layer->rect.width = _bitmap->width;
    _layer->rect.height = _bitmap->height;
    
    size_t data_size = _bitmap->width * _bitmap->height;
    if(data_size != _layer->comp.size)
    {
        _layer->comp.size = data_size;
        _layer->comp.data = playdate->system->realloc(_layer->comp.data, data_size);
    }
    
    _layer->canRestore = 0;
    _layer->enabled = 0;
    
    bitmapLayerDidChange(layer);
    bitmapLayerDraw(layer);
}

static void bitmapLayerGetPosition(PDMode7_BitmapLayer *pLayer, int *x, int *y)
{
    _PDMode7_BitmapLayer *layer = pLayer->prv;
    *x = layer->rect.x;
    *y = layer->rect.y;
}

static void bitmapLayerSetPosition(PDMode7_BitmapLayer *pLayer, int x, int y)
{
    _PDMode7_BitmapLayer *layer = pLayer->prv;
    
    bitmapLayerRestore(pLayer);
    
    layer->rect.x = x;
    layer->rect.y = y;
    
    bitmapLayerDidChange(pLayer);
    bitmapLayerDraw(pLayer);
}

static void bitmapLayerSetVisible(PDMode7_BitmapLayer *pLayer, int visible)
{
    _PDMode7_BitmapLayer *layer = pLayer->prv;
    
    bitmapLayerRestore(pLayer);
    
    layer->visible = visible;
    
    bitmapLayerDidChange(pLayer);
    bitmapLayerDraw(pLayer);
}

static int bitmapLayerIsVisible(PDMode7_BitmapLayer *layer)
{
    return ((_PDMode7_BitmapLayer*)layer->prv)->visible;
}

static void bitmapLayerInvalidate(PDMode7_BitmapLayer *pLayer)
{
    bitmapLayerRestore(pLayer);
    bitmapLayerDidChange(pLayer);
    bitmapLayerDraw(pLayer);
}

static int bitmapLayerIntersect(PDMode7_BitmapLayer *layer)
{
    _PDMode7_BitmapLayer *_layer = layer->prv;
    
    if(_layer->parentBitmap)
    {
        _PDMode7_Bitmap *parentBitmap = _layer->parentBitmap->prv;
        
        for(int i = 0; i < parentBitmap->layers->length; i++)
        {
            PDMode7_BitmapLayer *localLayer = parentBitmap->layers->items[i];
            _PDMode7_BitmapLayer *_localLayer = localLayer->prv;
            
            if(localLayer != layer)
            {
                if(rectIntersect(_localLayer->rect, _layer->rect) && _localLayer->visible)
                {
                    return 1;
                }
            }
        }
    }
    
    return 0;
}

static void bitmapLayerDidChange(PDMode7_BitmapLayer *layer)
{
    _PDMode7_BitmapLayer *_layer = layer->prv;
    
    if(_layer->parentBitmap)
    {
        _PDMode7_Bitmap *parentBitmap = _layer->parentBitmap->prv;
        
        int intersect = 0;
        
        for(int i = 0; i < parentBitmap->layers->length; i++)
        {
            PDMode7_BitmapLayer *localLayer = parentBitmap->layers->items[i];
            _PDMode7_BitmapLayer *_localLayer = localLayer->prv;
            
            if(localLayer != layer)
            {
                if(rectIntersect(_localLayer->rect, _layer->rect) && _localLayer->visible && _layer->visible)
                {
                    // Local layer intersects with the layer, we disable both
                    // Layer is disabled out of the loop
                    intersect = 1;
                    
                    bitmapLayerRestore(localLayer);
                    _localLayer->enabled = 0;
                    _localLayer->canRestore = 0;
                }
                else if(!_localLayer->enabled && _localLayer->visible)
                {
                    // Check if a disabled layer can be enabled
                    if(!bitmapLayerIntersect(localLayer))
                    {
                        // A disabled layer is now enabled
                        _localLayer->enabled = 1;
                        bitmapLayerDraw(localLayer);
                    }
                }
            }
        }
        
        if(intersect || !_layer->visible)
        {
            _layer->enabled = 0;
            _layer->canRestore = 0;
        }
        else
        {
            _layer->enabled = 1;
        }
    }
}

static void _removeBitmapLayer(PDMode7_BitmapLayer *layer, int restore)
{
    _PDMode7_BitmapLayer *_layer = layer->prv;
    
    if(_layer->parentBitmap)
    {
        _PDMode7_Bitmap *parentBitmap = _layer->parentBitmap->prv;
        int index = arrayIndexOf(parentBitmap->layers, layer);
        if(index >= 0)
        {
            if(restore)
            {
                bitmapLayerRestore(layer);
            }
            
            // Reset state
            _layer->enabled = 0;
            _layer->canRestore = 0;
            
            arrayRemove(parentBitmap->layers, index);
            _layer->parentBitmap = NULL;
            
            if(_layer->luaRef)
            {
                LuaUDObject *luaRef = _layer->luaRef;
                _layer->luaRef = NULL;
                playdate->lua->releaseObject(luaRef);
            }
        }
    }
}

static void removeBitmapLayer(PDMode7_BitmapLayer *layer)
{
    _removeBitmapLayer(layer, 1);
}

static void freeBitmapLayer(PDMode7_BitmapLayer *layer)
{
    _PDMode7_BitmapLayer *_layer = layer->prv;
    
    removeBitmapLayer(layer);
    
    if(_layer->bitmap)
    {
        _PDMode7_Bitmap *_bitmap = _layer->bitmap->prv;
        if(_bitmap->luaRef)
        {
            GC_release(_bitmap->luaRef);
        }
    }
    
    if(_layer->comp.data)
    {
        playdate->system->realloc(_layer->comp.data, 0);
    }
    playdate->system->realloc(_layer, 0);
    playdate->system->realloc(layer, 0);
}

static inline float degToRad(float degrees)
{
    return degrees * ((float)M_PI / 180.0f);
}

static float roundToIncrement(float number, unsigned int step)
{
    if(step != 0)
    {
        return roundf(number / step) * step;
    }
    return number;
}

static char *lua_kWorld = "mode7.world";
static char *lua_kDisplay = "mode7.display";
static char *lua_kBitmap = "mode7.bitmap";
static char *lua_kBitmapLayer = "mode7.bitmap.layer";
static char *lua_kCamera = "mode7.camera";
static char *lua_kSprite = "mode7.sprite";
static char *lua_kMode7SpriteDataSource = "mode7.sprite.datasource";
static char *lua_kSpriteInstance = "mode7.sprite.instance";
static char *lua_kSpriteInstanceDataSource = "mode7.sprite.instance.datasource";
static char *lua_kImage = "mode7.image";
static char *lua_kImageTable = "mode7.imagetable";
static char *lua_kBackground = "mode7.background";
static char *lua_kArray = "mode7.array";

static PDMode7_Color lua_readGrayscaleColor(lua_State *L, int i)
{
    int argCount = playdate->lua->getArgCount();
    int gray = playdate->lua->getArgInt(i);
    int alpha = 255;
    if((i + 1) < argCount)
    {
        alpha = playdate->lua->getArgInt(i + 1);
    }
    if(gray >= 0 && gray <= 255 && alpha >= 0 && alpha <= 255)
    {
        return newGrayscaleColor(gray, alpha);
    }
    return newGrayscaleColor(0, 0);
}

static int lua_poolRealloc(lua_State *L)
{
    int size = playdate->lua->getArgInt(1);
    poolRealloc(pool, size);
    return 0;
}

static int lua_poolClear(lua_State *L)
{
    poolClear(pool);
    return 0;
}

static int lua_newWorld(lua_State *L)
{
    float width = playdate->lua->getArgFloat(1);
    float height = playdate->lua->getArgFloat(2);
    float depth = playdate->lua->getArgFloat(3);
    int gridCellSize = playdate->lua->getArgInt(4);
    
    PDMode7_World *world = worldWithParameters(width, height, depth, gridCellSize);
    _PDMode7_World *_world = world->prv;
    
    PDMode7_Display *mainDisplay = _world->mainDisplay;
    _PDMode7_Display *_mainDisplay = mainDisplay->prv;
    
    LuaUDObject *displayRef = playdate->lua->pushObject(mainDisplay, lua_kDisplay, 0);
    _mainDisplay->luaRef = displayRef;
    playdate->lua->retainObject(displayRef);
    
    PDMode7_Camera *mainCamera = _world->mainCamera;
    _PDMode7_Camera *_mainCamera = mainCamera->prv;
    
    LuaUDObject *cameraRef = playdate->lua->pushObject(mainCamera, lua_kCamera, 0);
    _mainCamera->luaRef = cameraRef;
    GC_retain(cameraRef);
    // In Lua, mainCamera is not managed by world
    _mainCamera->isManaged = 0;
    _world->mainCamera = NULL;
    
    playdate->lua->pushObject(world, lua_kWorld, 0);
    
    return 1;
}

static int lua_getPlaneBitmap(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Bitmap *bitmap = getPlaneBitmap(world);
    playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
    return 1;
}

static int lua_setPlaneBitmap(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    setPlaneBitmap(world, bitmap);
    return 0;
}

static int lua_getCeilingBitmap(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Bitmap *bitmap = getCeilingBitmap(world);
    playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
    return 1;
}

static int lua_setCeilingBitmap(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    setCeilingBitmap(world, bitmap);
    return 0;
}

static int lua_worldGetMainDisplay(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Display *display = worldGetMainDisplay(world);
    playdate->lua->pushObject(display, lua_kDisplay, 0);
    return 1;
}

static int lua_addDisplay(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Display *display = playdate->lua->getArgObject(2, lua_kDisplay, NULL);
    int added = addDisplay(world, display);
    playdate->lua->pushBool(added);
    return 1;
}

static int lua_getPlaneFillColor(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    _PDMode7_World *_world = world->prv;
    playdate->lua->pushInt(_world->planeFillColor.gray);
    playdate->lua->pushInt(_world->planeFillColor.alpha);
    return 2;
}

static int lua_setPlaneFillColor(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    setPlaneFillColor(world, lua_readGrayscaleColor(L, 2));
    return 0;
}

static int lua_getCeilingFillColor(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    _PDMode7_World *_world = world->prv;
    playdate->lua->pushInt(_world->ceilingFillColor.gray);
    playdate->lua->pushInt(_world->ceilingFillColor.alpha);
    return 2;
}

static int lua_setCeilingFillColor(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    setCeilingFillColor(world, lua_readGrayscaleColor(L, 2));
    return 0;
}

static int lua_worldUpdate(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    worldUpdate(world);
    return 0;
}

static int lua_worldDraw(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Display *display = playdate->lua->getArgObject(2, lua_kDisplay, NULL);
    worldDraw(world, display);
    return 0;
}

static int lua_addSprite(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(2, lua_kSprite, NULL);
    addSprite(world, sprite);
    return 0;
}

static int lua_getSprites(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    _PDMode7_Array *sprites = _getSprites(world);
    _PDMode7_LuaArray *luaArray = newLuaArray(sprites, kPDMode7LuaItemSprite, 0);
    playdate->lua->pushObject(luaArray, lua_kArray, 0);
    return 1;
}

static int lua_getVisibleSpriteInstances(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Display *display = playdate->lua->getArgObject(2, lua_kDisplay, NULL);
    _PDMode7_Array *visibleSprites = _getVisibleSpriteInstances(world, display);
    _PDMode7_LuaArray *luaArray = newLuaArray(visibleSprites, kPDMode7LuaItemSpriteInstance, 0);
    playdate->lua->pushObject(luaArray, lua_kArray, 0);
    return 1;
}

static int lua_displayToPlanePoint(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    int displayX = playdate->lua->getArgInt(2);
    int displayY = playdate->lua->getArgInt(3);
    PDMode7_Display *display = playdate->lua->getArgObject(4, lua_kDisplay, NULL);
    PDMode7_Vec3 point = displayToPlanePoint_public(world, displayX, displayY, display);
    playdate->lua->pushFloat(point.x);
    playdate->lua->pushFloat(point.y);
    playdate->lua->pushFloat(point.z);
    return 3;
}

static int lua_worldToDisplayPoint(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    float pointX = playdate->lua->getArgFloat(2);
    float pointY = playdate->lua->getArgFloat(3);
    float pointZ = playdate->lua->getArgFloat(4);
    PDMode7_Display *display = playdate->lua->getArgObject(5, lua_kDisplay, NULL);
    PDMode7_Vec3 point = worldToDisplayPoint_public(world, newVec3(pointX, pointY, pointZ), display);
    playdate->lua->pushFloat(point.x);
    playdate->lua->pushFloat(point.y);
    playdate->lua->pushFloat(point.z);
    return 3;
}

static int lua_displayMultiplierForScanlineAt(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    float pointX = playdate->lua->getArgFloat(2);
    float pointY = playdate->lua->getArgFloat(3);
    float pointZ = playdate->lua->getArgFloat(4);
    PDMode7_Display *display = playdate->lua->getArgObject(5, lua_kDisplay, NULL);
    PDMode7_Vec3 point = displayMultiplierForScanlineAt_public(world, newVec3(pointX, pointY, pointZ), display);
    playdate->lua->pushFloat(point.x);
    playdate->lua->pushFloat(point.y);
    playdate->lua->pushFloat(point.z);
    return 3;
}

static int lua_bitmapToPlanePoint(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    float bitmapX = playdate->lua->getArgFloat(2);
    float bitmapY = playdate->lua->getArgFloat(3);
    PDMode7_Vec2 point = bitmapToPlanePoint(world, bitmapX, bitmapY);
    playdate->lua->pushFloat(point.x);
    playdate->lua->pushFloat(point.y);
    return 2;
}

static int lua_planeToBitmapPoint(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    float pointX = playdate->lua->getArgFloat(2);
    float pointY = playdate->lua->getArgFloat(3);
    PDMode7_Vec2 point = planeToBitmapPoint(world, pointX, pointY);
    playdate->lua->pushFloat(point.x);
    playdate->lua->pushFloat(point.y);
    return 2;
}

static int lua_freeWorld(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    freeWorld(world);
    return 0;
}

static const lua_reg lua_world[] = {
    { "_new", lua_newWorld },
    { "addSprite", lua_addSprite },
    { "getSprites", lua_getSprites },
    { "getVisibleSpriteInstances", lua_getVisibleSpriteInstances },
    { "_getPlaneFillColor", lua_getPlaneFillColor },
    { "_setPlaneFillColor", lua_setPlaneFillColor },
    { "_getCeilingFillColor", lua_getCeilingFillColor },
    { "_setCeilingFillColor", lua_setCeilingFillColor },
    { "getPlaneBitmap", lua_getPlaneBitmap },
    { "setPlaneBitmap", lua_setPlaneBitmap },
    { "getCeilingBitmap", lua_getCeilingBitmap },
    { "setCeilingBitmap", lua_setCeilingBitmap },
    { "getMainDisplay", lua_worldGetMainDisplay },
    { "addDisplay", lua_addDisplay },
    { "displayToPlanePoint", lua_displayToPlanePoint },
    { "worldToDisplayPoint", lua_worldToDisplayPoint },
    { "displayMultiplierForScanlineAt", lua_displayMultiplierForScanlineAt },
    { "bitmapToPlanePoint", lua_bitmapToPlanePoint },
    { "planeToBitmapPoint", lua_planeToBitmapPoint },
    { "update", lua_worldUpdate },
    { "draw", lua_worldDraw },
    { "__gc", lua_freeWorld },
    { NULL, NULL }
};

static int lua_newDisplay(lua_State *L)
{
    int x = playdate->lua->getArgInt(1);
    int y = playdate->lua->getArgInt(2);
    int width = playdate->lua->getArgInt(3);
    int height = playdate->lua->getArgInt(4);
    PDMode7_Display *display = newDisplay(x, y, width, height);
    _PDMode7_Display *_display = display->prv;
    _display->luaRef = playdate->lua->pushObject(display, lua_kDisplay, 0);
    return 1;
}

static int lua_displayGetCamera(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_Camera *camera = displayGetCamera(display);
    playdate->lua->pushObject(camera, lua_kCamera, 0);
    return 1;
}

static int lua_displaySetCamera(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_Camera *camera = playdate->lua->getArgObject(2, lua_kCamera, NULL);
    displaySetCamera(display, camera);
    return 0;
}

static int lua_displayGetScale(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_DisplayScale scale = displayGetScale(display);
    playdate->lua->pushInt(scale);
    return 1;
}

static int lua_displaySetScale(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_DisplayScale scale = playdate->lua->getArgInt(2);
    displaySetScale(display, scale);
    return 0;
}

static int lua_displayGetHorizon(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_DisplayScale scale = displayGetHorizon(display);
    playdate->lua->pushInt(scale);
    return 1;
}

static int lua_displayPitchForHorizon(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    float horizon = playdate->lua->getArgFloat(2);
    float pitch = displayPitchForHorizon(display, horizon);
    playdate->lua->pushFloat(pitch);
    return 1;
}

static int lua_displayGetWorld(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_World *world = displayGetWorld(display);
    playdate->lua->pushObject(world, lua_kWorld, 0);
    return 1;
}

static int lua_displayGetBackground(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_Background *background = displayGetBackground(display);
    playdate->lua->pushObject(background, lua_kBackground, 0);
    return 1;
}

static int lua_displayGetRect(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_Rect rect = displayGetRect(display);
    playdate->lua->pushInt(rect.x);
    playdate->lua->pushInt(rect.y);
    playdate->lua->pushInt(rect.width);
    playdate->lua->pushInt(rect.height);
    return 4;
}

static int lua_displaySetRect(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    int x = playdate->lua->getArgInt(2);
    int y = playdate->lua->getArgInt(3);
    int width = playdate->lua->getArgInt(4);
    int height = playdate->lua->getArgInt(5);
    displaySetRect(display, x, y, width, height);
    return 0;
}

static int lua_displayGetOrientation(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_DisplayOrientation orientation = displayGetOrientation(display);
    playdate->lua->pushInt(orientation);
    return 1;
}

static int lua_displaySetOrientation(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_DisplayOrientation orientation = playdate->lua->getArgInt(2);
    displaySetOrientation(display, orientation);
    return 0;
}

static int lua_displayGetFlipMode(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_DisplayFlipMode flipMode = displayGetFlipMode(display);
    playdate->lua->pushInt(flipMode);
    return 1;
}

static int lua_displaySetFlipMode(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_DisplayFlipMode flipMode = playdate->lua->getArgInt(2);
    displaySetFlipMode(display, flipMode);
    return 0;
}

static int lua_displayConvertPointFromOrientation(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    float x = playdate->lua->getArgFloat(2);
    float y = playdate->lua->getArgFloat(3);
    PDMode7_Vec2 point = displayConvertPointFromOrientation(display, x, y);
    playdate->lua->pushFloat(point.x);
    playdate->lua->pushFloat(point.y);
    return 2;
}

static int lua_displayConvertPointToOrientation(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    float x = playdate->lua->getArgFloat(2);
    float y = playdate->lua->getArgFloat(3);
    PDMode7_Vec2 point = displayConvertPointToOrientation(display, x, y);
    playdate->lua->pushFloat(point.x);
    playdate->lua->pushFloat(point.y);
    return 2;
}

static int lua_removeDisplay(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    removeDisplay(display);
    return 0;
}

static int lua_freeDisplay(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    freeDisplay(display);
    return 0;
}

static const lua_reg lua_display[] = {
    { "new", lua_newDisplay },
    { "getCamera", lua_displayGetCamera },
    { "setCamera", lua_displaySetCamera },
    { "getScale", lua_displayGetScale },
    { "setScale", lua_displaySetScale },
    { "getRect", lua_displayGetRect },
    { "setRect", lua_displaySetRect },
    { "getOrientation", lua_displayGetOrientation },
    { "setOrientation", lua_displaySetOrientation },
    { "getFlipMode", lua_displayGetFlipMode },
    { "setFlipMode", lua_displaySetFlipMode },
    { "getBackground", lua_displayGetBackground },
    { "getHorizon", lua_displayGetHorizon },
    { "pitchForHorizon", lua_displayPitchForHorizon },
    { "convertPointFromOrientation", lua_displayConvertPointFromOrientation },
    { "convertPointToOrientation", lua_displayConvertPointToOrientation },
    { "getWorld", lua_displayGetWorld },
    { "removeFromWorld", lua_removeDisplay },
    { "__gc", lua_freeDisplay },
    { NULL, NULL }
};

static int lua_newCamera(lua_State *L)
{
    PDMode7_Camera *camera = newCamera();
    _PDMode7_Camera *_camera = camera->prv;
    _camera->luaRef = playdate->lua->pushObject(camera, lua_kCamera, 0);
    return 1;
}

static int lua_cameraGetPosition(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    PDMode7_Vec3 position = cameraGetPosition(camera);
    playdate->lua->pushFloat(position.x);
    playdate->lua->pushFloat(position.y);
    playdate->lua->pushFloat(position.z);
    return 3;
}

static int lua_cameraSetPosition(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    float x = playdate->lua->getArgFloat(2);
    float y = playdate->lua->getArgFloat(3);
    float z = playdate->lua->getArgFloat(4);
    cameraSetPosition(camera, x, y, z);
    return 0;
}

static int lua_cameraGetAngle(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    float angle = cameraGetAngle(camera);
    playdate->lua->pushFloat(angle);
    return 1;
}

static int lua_cameraSetAngle(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    float angle = playdate->lua->getArgFloat(2);
    cameraSetAngle(camera, angle);
    return 0;
}

static int lua_cameraGetPitch(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    float pitch = cameraGetPitch(camera);
    playdate->lua->pushFloat(pitch);
    return 1;
}

static int lua_cameraSetPitch(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    float pitch = playdate->lua->getArgFloat(2);
    cameraSetPitch(camera, pitch);
    return 0;
}

static int lua_cameraGetFOV(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    float fov = cameraGetFOV(camera);
    playdate->lua->pushFloat(fov);
    return 1;
}

static int lua_cameraSetFOV(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    float fov = playdate->lua->getArgFloat(2);
    cameraSetFOV(camera, fov);
    return 0;
}

static int lua_cameraGetClipDistanceUnits(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    int units = cameraGetClipDistanceUnits(camera);
    playdate->lua->pushInt(units);
    return 1;
}

static int lua_cameraSetClipDistanceUnits(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    int units = playdate->lua->getArgInt(2);
    cameraSetClipDistanceUnits(camera, units);
    return 0;
}

static int lua_cameraLookAtPoint(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    PDMode7_Vec3 point = newVec3(0, 0, 0);
    point.x = playdate->lua->getArgFloat(2);
    point.y = playdate->lua->getArgFloat(3);
    point.z = playdate->lua->getArgFloat(4);
    cameraLookAtPoint(camera, point);
    return 0;
}

static int lua_freeCamera(lua_State *L)
{
    PDMode7_Camera *camera = playdate->lua->getArgObject(1, lua_kCamera, NULL);
    freeCamera(camera);
    return 0;
}

static const lua_reg lua_camera[] = {
    { "new", lua_newCamera },
    { "getPosition", lua_cameraGetPosition },
    { "setPosition", lua_cameraSetPosition },
    { "getAngle", lua_cameraGetAngle },
    { "setAngle", lua_cameraSetAngle },
    { "getPitch", lua_cameraGetPitch },
    { "setPitch", lua_cameraSetPitch },
    { "getFOV", lua_cameraGetFOV },
    { "setFOV", lua_cameraSetFOV },
    { "getClipDistanceUnits", lua_cameraGetClipDistanceUnits },
    { "setClipDistanceUnits", lua_cameraSetClipDistanceUnits },
    { "lookAtPoint", lua_cameraLookAtPoint },
    { "__gc", lua_freeCamera },
    { NULL, NULL }
};

static int lua_newImage(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    _PDMode7_LuaBitmap *bitmap = newLuaBitmap(filename);
    if(bitmap)
    {
        bitmap->luaRef = playdate->lua->pushObject(bitmap, lua_kImage, 0);
    }
    else {
        playdate->lua->pushNil();
    }
    return 1;
}

static int lua_imageGetPDImage(lua_State *L)
{
    _PDMode7_LuaBitmap *bitmap = playdate->lua->getArgObject(1, lua_kImage, NULL);
    playdate->lua->pushBitmap(bitmap->LCDBitmap);
    return 1;
}

static int lua_freeImage(lua_State *L)
{
    _PDMode7_LuaBitmap *bitmap = playdate->lua->getArgObject(1, lua_kImage, NULL);
    freeLuaBitmap(bitmap);
    return 0;
}

static const lua_reg lua_image[] = {
    { "new", lua_newImage },
    { "getPDImage", lua_imageGetPDImage },
    { "__gc", lua_freeImage },
    { NULL, NULL }
};

static int lua_newImageTable(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    _PDMode7_LuaBitmapTable *bitmapTable = newLuaBitmapTable(filename);
    if(bitmapTable)
    {
        bitmapTable->luaRef = playdate->lua->pushObject(bitmapTable, lua_kImageTable, 0);
    }
    else {
        playdate->lua->pushNil();
    }
    return 1;
}

static int lua_freeImageTable(lua_State *L)
{
    _PDMode7_LuaBitmapTable *bitmapTable = playdate->lua->getArgObject(1, lua_kImageTable, NULL);
    freeLuaBitmapTable(bitmapTable);
    return 0;
}

static const lua_reg lua_imageTable[] = {
    { "new", lua_newImageTable },
    { "__gc", lua_freeImageTable },
    { NULL, NULL }
};

static int lua_arrayGet(lua_State *L)
{
    _PDMode7_LuaArray *luaArray = playdate->lua->getArgObject(1, lua_kArray, NULL);
    int i = playdate->lua->getArgInt(2);
    if(i > 0 && (i - 1) < luaArray->array->length)
    {
        void *item = luaArray->array->items[i - 1];
        switch(luaArray->type)
        {
            case kPDMode7LuaItemSprite: {
                playdate->lua->pushObject(item, lua_kSprite, 0);
                return 1;
            }
            case kPDMode7LuaItemSpriteInstance: {
                playdate->lua->pushObject(item, lua_kSpriteInstance, 0);
                return 1;
            }
            case kPDMode7LuaItemBitmapLayer: {
                playdate->lua->pushObject(item, lua_kBitmapLayer, 0);
                return 1;
            }
        }
    }
    playdate->lua->pushNil();
    return 1;
}

static int lua_arraySize(lua_State *L)
{
    _PDMode7_LuaArray *luaArray = playdate->lua->getArgObject(1, lua_kArray, NULL);
    playdate->lua->pushInt(luaArray->array->length);
    return 1;
}

static int lua_freeArray(lua_State *L)
{
    _PDMode7_LuaArray *luaArray = playdate->lua->getArgObject(1, lua_kArray, NULL);
    freeLuaArray(luaArray);
    return 0;
}

static const lua_reg lua_array[] = {
    { "get", lua_arrayGet },
    { "size", lua_arraySize },
    { "__gc", lua_freeArray },
    { NULL, NULL }
};

static int lua_newSprite(lua_State *L)
{
    float width = playdate->lua->getArgFloat(1);
    float height = playdate->lua->getArgFloat(2);
    float depth = playdate->lua->getArgFloat(3);
    PDMode7_Sprite *sprite = newSprite(width, height, depth);
    _PDMode7_Sprite *_sprite = sprite->prv;
    _sprite->luaRef = playdate->lua->pushObject(sprite, lua_kSprite, 0);
    return 1;
}

static int lua_spriteGetPosition(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    PDMode7_Vec3 position = spriteGetPosition(sprite);
    playdate->lua->pushFloat(position.x);
    playdate->lua->pushFloat(position.y);
    playdate->lua->pushFloat(position.z);
    return 3;
}

static int lua_spriteSetPosition(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float x = playdate->lua->getArgFloat(2);
    float y = playdate->lua->getArgFloat(3);
    float z = playdate->lua->getArgFloat(4);
    spriteSetPosition(sprite, x, y, z);
    return 0;
}

static int lua_spriteGetAngle(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float angle = spriteGetAngle(sprite);
    playdate->lua->pushFloat(angle);
    return 1;
}

static int lua_spriteSetAngle(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float angle = playdate->lua->getArgFloat(2);
    spriteSetAngle(sprite, angle);
    return 0;
}

static int lua_spriteGetPitch(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float pitch = spriteGetPitch(sprite);
    playdate->lua->pushFloat(pitch);
    return 1;
}

static int lua_spriteSetPitch(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float pitch = playdate->lua->getArgFloat(2);
    spriteSetPitch(sprite, pitch);
    return 0;
}

static int lua_spriteGetSize(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    PDMode7_Vec3 size = spriteGetSize(sprite);
    playdate->lua->pushFloat(size.x);
    playdate->lua->pushFloat(size.y);
    playdate->lua->pushFloat(size.z);
    return 3;
}

static int lua_spriteSetSize(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float width = playdate->lua->getArgFloat(2);
    float height = playdate->lua->getArgFloat(3);
    float depth = playdate->lua->getArgFloat(4);
    spriteSetSize(sprite, width, height, depth);
    return 0;
}

static int lua_spriteSetBitmapTable(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _PDMode7_LuaBitmapTable *luaBitmapTable = playdate->lua->getArgObject(2, lua_kImageTable, NULL);
    
    LCDBitmapTable *LCDBitmapTable = NULL;
    if(luaBitmapTable)
    {
        LCDBitmapTable = luaBitmapTable->LCDBitmapTable;
    }
    
    spriteSetBitmapTable(sprite, LCDBitmapTable, luaBitmapTable);

    return 0;
}

static int lua_spriteInstanceGetBitmapTable(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    _PDMode7_LuaBitmapTable *luaBitmapTable = _spriteGetLuaBitmapTable(instance);
    playdate->lua->pushObject(luaBitmapTable, lua_kImageTable, 0);
    return 1;
}

static int lua_spriteInstanceSetBitmapTable(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    _PDMode7_LuaBitmapTable *luaBitmapTable = playdate->lua->getArgObject(2, lua_kImageTable, NULL);

    LCDBitmapTable *LCDBitmapTable = NULL;
    if(luaBitmapTable)
    {
        LCDBitmapTable = luaBitmapTable->LCDBitmapTable;
    }
    
    _spriteSetBitmapTable(instance, LCDBitmapTable, luaBitmapTable);

    return 0;
}

static int lua_spriteSetVisible(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int visible = playdate->lua->getArgBool(2);
    spriteSetVisible(sprite, visible);
    return 0;
}

static int lua_spriteInstanceGetVisible(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    int visible = _spriteGetVisible(instance);
    playdate->lua->pushBool(visible);
    return 1;
}

static int lua_spriteInstanceSetVisible(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    int visible = playdate->lua->getArgBool(2);
    _spriteSetVisible(instance, visible);
    return 0;
}

static int lua_spriteSetImageCenter(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float cx = playdate->lua->getArgFloat(2);
    float cy = playdate->lua->getArgFloat(3);
    spriteSetImageCenter(sprite, cx, cy);
    return 0;
}

static int lua_spriteInstanceGetImageCenter(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_Vec2 center = _spriteGetImageCenter(instance);
    playdate->lua->pushFloat(center.x);
    playdate->lua->pushFloat(center.y);
    return 2;
}

static int lua_spriteInstanceSetImageCenter(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    float cx = playdate->lua->getArgFloat(2);
    float cy = playdate->lua->getArgFloat(3);
    _spriteSetImageCenter(instance, cx, cy);
    return 0;
}

static int lua_spriteSetFrame(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int frame = playdate->lua->getArgInt(2);
    spriteSetFrame(sprite, frame);
    return 0;
}

static int lua_spriteInstanceGetFrame(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    unsigned int frame = _spriteGetFrame(instance);
    playdate->lua->pushInt(frame);
    return 1;
}

static int lua_spriteInstanceSetFrame(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    int frame = playdate->lua->getArgInt(2);
    _spriteSetFrame(instance, frame);
    return 0;
}

static int lua_spriteSetBillboardSizeBehavior(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    PDMode7_SpriteBillboardSizeBehavior behavior = playdate->lua->getArgInt(2);
    spriteSetBillboardSizeBehavior(sprite, behavior);
    return 0;
}

static int lua_spriteInstanceGetBillboardSizeBehavior(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_SpriteBillboardSizeBehavior behavior = _spriteGetBillboardSizeBehavior(instance);
    playdate->lua->pushInt(behavior);
    return 1;
}

static int lua_spriteInstanceSetBillboardSizeBehavior(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_SpriteBillboardSizeBehavior behavior = playdate->lua->getArgInt(2);
    _spriteSetBillboardSizeBehavior(instance, behavior);
    return 0;
}

static int lua_spriteSetBillboardSize(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    float width = playdate->lua->getArgFloat(2);
    float height = playdate->lua->getArgFloat(3);
    spriteSetBillboardSize(sprite, width, height);
    return 0;
}

static int lua_spriteInstanceGetBillboardSize(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_Vec2 size = _spriteGetBillboardSize(instance);
    playdate->lua->pushFloat(size.x);
    playdate->lua->pushFloat(size.y);
    return 2;
}

static int lua_spriteInstanceSetBillboardSize(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    float width = playdate->lua->getArgFloat(2);
    float height = playdate->lua->getArgFloat(3);
    _spriteSetBillboardSize(instance, width, height);
    return 0;
}

static int lua_spriteSetRoundingIncrement(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    int x = playdate->lua->getArgInt(2);
    int y = playdate->lua->getArgInt(3);
    spriteSetRoundingIncrement(sprite, x, y);
    return 0;
}

static int lua_spriteInstanceGetRoundingIncrement(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    unsigned int x; unsigned int y;
    _spriteGetRoundingIncrement(instance, &x, &y);
    playdate->lua->pushInt(x);
    playdate->lua->pushInt(y);
    return 2;
}

static int lua_spriteInstanceSetRoundingIncrement(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    unsigned int x = playdate->lua->getArgInt(2);
    unsigned int y = playdate->lua->getArgInt(3);
    _spriteSetRoundingIncrement(instance, x, y);
    return 0;
}

static int lua_spriteSetAlignment(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    PDMode7_SpriteAlignment alignmentX = playdate->lua->getArgInt(2);
    PDMode7_SpriteAlignment alignmentY = playdate->lua->getArgInt(3);
    spriteSetAlignment(sprite, alignmentX, alignmentY);
    return 0;
}

static int lua_spriteInstanceGetAlignment(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_SpriteAlignment alignmentX; PDMode7_SpriteAlignment alignmentY;
    _spriteGetAlignment(instance, &alignmentX, &alignmentY);
    playdate->lua->pushInt(alignmentX);
    playdate->lua->pushInt(alignmentY);
    return 2;
}

static int lua_spriteInstanceSetAlignment(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_SpriteAlignment alignmentX = playdate->lua->getArgInt(2);
    PDMode7_SpriteAlignment alignmentY = playdate->lua->getArgInt(3);
    _spriteSetAlignment(instance, alignmentX, alignmentY);
    return 0;
}

static int lua_spriteGetInstance(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    PDMode7_Display *display = playdate->lua->getArgObject(2, lua_kDisplay, NULL);
    PDMode7_SpriteInstance *instance = spriteGetInstance(sprite, display);
    playdate->lua->pushObject(instance, lua_kSpriteInstance, 0);
    return 1;
}

static int lua_spriteGetInstances(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    
    int length;
    PDMode7_SpriteInstance** instances = spriteGetInstances(sprite, &length);
    _PDMode7_Array *array = newArray();
    for(int i = 0; i < length; i++)
    {
        arrayPush(array, instances[i]);
    }
    _PDMode7_LuaArray *luaArray = newLuaArray(array, kPDMode7LuaItemSpriteInstance, 1);
    playdate->lua->pushObject(luaArray, lua_kArray, 0);
    
    return 1;
}

static int lua_spriteGetDataSource(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _PDMode7_Sprite *_sprite = sprite->prv;
    _PDMode7_LuaSpriteDataSource *dataSource = _sprite->luaDataSource;
    playdate->lua->pushObject(dataSource, lua_kMode7SpriteDataSource, 0);
    return 1;
}

static int lua_spriteInstanceGetDataSource(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_SpriteDataSource *dataSource = _spriteGetDataSource(instance);
    playdate->lua->pushObject(dataSource, lua_kSpriteInstanceDataSource, 0);
    return 1;
}

static int lua_spriteDataSourceSetLength(lua_State *L)
{
    _PDMode7_LuaSpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kMode7SpriteDataSource, NULL);
    PDMode7_Sprite *sprite = dataSource->sprite;
    int length = playdate->lua->getArgInt(2);
    PDMode7_SpriteDataSourceKey k = playdate->lua->getArgInt(3);
    spriteDataSourceSetLength(sprite, length, k);
    return 0;
}

static int lua_spriteInstanceDataSourceGetLength(lua_State *L)
{
    PDMode7_SpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kSpriteInstanceDataSource, NULL);
    PDMode7_SpriteDataSourceKey k = playdate->lua->getArgInt(2);
    int length = _spriteDataSourceGetLength(dataSource, k);
    playdate->lua->pushInt(length);
    return 1;
}

static int lua_spriteInstanceDataSourceSetLength(lua_State *L)
{
    PDMode7_SpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kSpriteInstanceDataSource, NULL);
    int length = playdate->lua->getArgInt(2);
    PDMode7_SpriteDataSourceKey k = playdate->lua->getArgInt(3);
    _spriteDataSourceSetLength(dataSource, length, k);
    return 0;
}

static int lua_spriteDataSourceSetMaximumWidth(lua_State *L)
{
    _PDMode7_LuaSpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kMode7SpriteDataSource, NULL);
    PDMode7_Sprite *sprite = dataSource->sprite;
    int width = playdate->lua->getArgInt(2);
    spriteDataSourceSetMaximumWidth(sprite, width);
    return 0;
}

static int lua_spriteInstanceDataSourceGetMaximumWidth(lua_State *L)
{
    PDMode7_SpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kSpriteInstanceDataSource, NULL);
    int width = _spriteDataSourceGetMaximumWidth(dataSource);
    playdate->lua->pushInt(width);
    return 1;
}

static int lua_spriteInstanceDataSourceSetMaximumWidth(lua_State *L)
{
    PDMode7_SpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kSpriteInstanceDataSource, NULL);
    int width = playdate->lua->getArgInt(2);
    _spriteDataSourceSetMaximumWidth(dataSource, width);
    return 0;
}

static int lua_spriteDataSourceSetMinimumWidth(lua_State *L)
{
    _PDMode7_LuaSpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kMode7SpriteDataSource, NULL);
    PDMode7_Sprite *sprite = dataSource->sprite;
    int width = playdate->lua->getArgInt(2);
    spriteDataSourceSetMinimumWidth(sprite, width);
    return 0;
}

static int lua_spriteInstanceDataSourceGetMinimumWidth(lua_State *L)
{
    PDMode7_SpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kSpriteInstanceDataSource, NULL);
    int width = _spriteDataSourceGetMinimumWidth(dataSource);
    playdate->lua->pushInt(width);
    return 1;
}

static int lua_spriteInstanceDataSourceSetMinimumWidth(lua_State *L)
{
    PDMode7_SpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kSpriteInstanceDataSource, NULL);
    int width = playdate->lua->getArgInt(2);
    _spriteDataSourceSetMinimumWidth(dataSource, width);
    return 0;
}

static int lua_spriteInstanceDataSourceGetLayout(lua_State *L)
{
    PDMode7_SpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kSpriteInstanceDataSource, NULL);
    PDMode7_SpriteDataSourceKey k1, k2, k3, k4;
    _spriteDataSourceGetLayout(dataSource, &k1, &k2, &k3, &k4);
    playdate->lua->pushInt(k1);
    playdate->lua->pushInt(k2);
    playdate->lua->pushInt(k3);
    playdate->lua->pushInt(k4);
    return 4;
}

static int lua_spriteInstanceDataSourceSetLayout(lua_State *L)
{
    PDMode7_SpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kSpriteInstanceDataSource, NULL);
    PDMode7_SpriteDataSourceKey k1 = playdate->lua->getArgInt(2);
    PDMode7_SpriteDataSourceKey k2 = playdate->lua->getArgInt(3);
    PDMode7_SpriteDataSourceKey k3 = playdate->lua->getArgInt(4);
    PDMode7_SpriteDataSourceKey k4 = playdate->lua->getArgInt(5);
    _spriteDataSourceSetLayout(dataSource, k1, k2, k3, k4);
    return 0;
}

static int lua_spriteDataSourceSetLayout(lua_State *L)
{
    _PDMode7_LuaSpriteDataSource *dataSource = playdate->lua->getArgObject(1, lua_kMode7SpriteDataSource, NULL);
    PDMode7_Sprite *sprite = dataSource->sprite;
    PDMode7_SpriteDataSourceKey k1 = playdate->lua->getArgInt(2);
    PDMode7_SpriteDataSourceKey k2 = playdate->lua->getArgInt(3);
    PDMode7_SpriteDataSourceKey k3 = playdate->lua->getArgInt(4);
    PDMode7_SpriteDataSourceKey k4 = playdate->lua->getArgInt(5);
    spriteDataSourceSetLayout(sprite, k1, k2, k3, k4);
    return 0;
}

static int lua_spriteSetDrawFunctionName(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    _PDMode7_Sprite *_sprite = sprite->prv;
    
    const char *functionName = playdate->lua->getArgString(2);
    
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = _sprite->instances[i];
        _spriteSetDrawCallback(instance, newCallback_lua(functionName));
    }
    
    return 0;
}

static int lua_spriteInstanceSetDrawFunctionName(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    const char *functionName = playdate->lua->getArgString(2);
    _spriteSetDrawCallback(instance, newCallback_lua(functionName));
    return 0;
}

static int lua_spriteGetWorld(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    PDMode7_World *world = spriteGetWorld(sprite);
    playdate->lua->pushObject(world, lua_kWorld, 0);
    return 1;
}

static int lua_removeSprite(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    removeSprite(sprite);
    return 0;
}

static int lua_freeSprite(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    freeSprite(sprite);
    return 0;
}

static int lua_drawSpriteFunction(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    drawSprite(instance);
    return 0;
}

static void lua_spriteCallDrawCallback(PDMode7_SpriteInstance *pInstance)
{
    _PDMode7_SpriteInstance *instance = pInstance->prv;
    
    if(instance->drawCallback && instance->drawCallback->type == _PDMode7_CallbackTypeLua)
    {
        playdate->lua->pushObject(pInstance, lua_kSpriteInstance, 0);
        
        playdate->lua->pushInt(instance->displayRect.x);
        playdate->lua->pushInt(instance->displayRect.y);
        playdate->lua->pushInt(instance->displayRect.width);
        playdate->lua->pushInt(instance->displayRect.height);
        
        playdate->lua->pushFunction(lua_drawSpriteFunction);
        
        playdate->lua->callFunction(instance->drawCallback->luaFunction, 6, NULL);
    }
}

static const lua_reg lua_sprite[] = {
    { "new", lua_newSprite },
    { "setImageTable", lua_spriteSetBitmapTable },
    { "getPosition", lua_spriteGetPosition },
    { "setPosition", lua_spriteSetPosition },
    { "getAngle", lua_spriteGetAngle },
    { "setAngle", lua_spriteSetAngle },
    { "getPitch", lua_spriteGetPitch },
    { "setPitch", lua_spriteSetPitch },
    { "setFrame", lua_spriteSetFrame },
    { "setBillboardSizeBehavior", lua_spriteSetBillboardSizeBehavior },
    { "setBillboardSize", lua_spriteSetBillboardSize },
    { "setFrame", lua_spriteSetFrame },
    { "setVisible", lua_spriteSetVisible },
    { "setImageCenter", lua_spriteSetImageCenter },
    { "setRoundingIncrement", lua_spriteSetRoundingIncrement },
    { "setAlignment", lua_spriteSetAlignment },
    { "getSize", lua_spriteGetSize },
    { "setSize", lua_spriteSetSize },
    { "getInstance", lua_spriteGetInstance },
    { "getInstances", lua_spriteGetInstances },
    { "getDataSource", lua_spriteGetDataSource },
    { "setDrawFunctionName", lua_spriteSetDrawFunctionName },
    { "getWorld", lua_spriteGetWorld },
    { "removeFromWorld", lua_removeSprite },
    { "__gc", lua_freeSprite },
    { NULL, NULL }
};

static const lua_reg lua_spriteDataSource[] = {
    { "setMaximumWidth", lua_spriteDataSourceSetMaximumWidth },
    { "setMinimumWidth", lua_spriteDataSourceSetMinimumWidth },
    { "setLengthForKey", lua_spriteDataSourceSetLength },
    { "setLayout", lua_spriteDataSourceSetLayout },
    { NULL, NULL }
};

static int lua_spriteInstanceGetSprite(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_Sprite *sprite = spriteInstanceGetSprite(instance);
    playdate->lua->pushObject(sprite, lua_kSprite, 0);
    return 1;
}

static int lua_spriteInstanceGetBitmap(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    LCDBitmap *bitmap = _spriteGetBitmap(instance);
    playdate->lua->pushBitmap(bitmap);
    return 1;
}

static int lua_spriteInstanceGetDisplayRect(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_Rect rect = _spriteGetDisplayRect(instance);
    playdate->lua->pushInt(rect.x);
    playdate->lua->pushInt(rect.y);
    playdate->lua->pushInt(rect.width);
    playdate->lua->pushInt(rect.height);
    return 4;
}

static int lua_spriteInstanceIsVisibleOnDisplay(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    int visible = _spriteIsVisibleOnDisplay(instance);
    playdate->lua->pushBool(visible);
    return 1;
}

static const lua_reg lua_spriteInstance[] = {
    { "getSprite", lua_spriteInstanceGetSprite },
    { "getImageTable", lua_spriteInstanceGetBitmapTable },
    { "setImageTable", lua_spriteInstanceSetBitmapTable },
    { "isVisible", lua_spriteInstanceGetVisible },
    { "setVisible", lua_spriteInstanceSetVisible },
    { "getFrame", lua_spriteInstanceGetFrame },
    { "setFrame", lua_spriteInstanceSetFrame },
    { "getBillboardSizeBehavior", lua_spriteInstanceGetBillboardSizeBehavior },
    { "setBillboardSizeBehavior", lua_spriteInstanceSetBillboardSizeBehavior },
    { "getBillboardSize", lua_spriteInstanceGetBillboardSize },
    { "setBillboardSize", lua_spriteInstanceSetBillboardSize },
    { "getImageCenter", lua_spriteInstanceGetImageCenter },
    { "setImageCenter", lua_spriteInstanceSetImageCenter },
    { "getRoundingIncrement", lua_spriteInstanceGetRoundingIncrement },
    { "setRoundingIncrement", lua_spriteInstanceSetRoundingIncrement },
    { "getAlignment", lua_spriteInstanceGetAlignment },
    { "setAlignment", lua_spriteInstanceSetAlignment },
    { "getDataSource", lua_spriteInstanceGetDataSource },
    { "setDrawFunctionName", lua_spriteInstanceSetDrawFunctionName },
    { "getImage", lua_spriteInstanceGetBitmap },
    { "getDisplayRect", lua_spriteInstanceGetDisplayRect },
    { "isVisibleOnDisplay", lua_spriteInstanceIsVisibleOnDisplay },
    { NULL, NULL }
};

static const lua_reg lua_spriteInstanceDataSource[] = {
    { "getMaximumWidth", lua_spriteInstanceDataSourceGetMaximumWidth },
    { "setMaximumWidth", lua_spriteInstanceDataSourceSetMaximumWidth },
    { "getMinimumWidth", lua_spriteInstanceDataSourceGetMinimumWidth },
    { "setMinimumWidth", lua_spriteInstanceDataSourceSetMinimumWidth },
    { "getLengthForKey", lua_spriteInstanceDataSourceGetLength },
    { "setLengthForKey", lua_spriteInstanceDataSourceSetLength },
    { "getLayout", lua_spriteInstanceDataSourceGetLayout },
    { "setLayout", lua_spriteInstanceDataSourceSetLayout },
    { NULL, NULL }
};

static int lua_loadPGM(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    PDMode7_Bitmap *bitmap = loadPGM(filename);
    _PDMode7_Bitmap *_bitmap = bitmap->prv;
    LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
    _bitmap->luaRef = luaRef;
    return 1;
}

static int lua_bitmapGetSize(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    _PDMode7_Bitmap *_bitmap = bitmap->prv;
    playdate->lua->pushInt(_bitmap->width);
    playdate->lua->pushInt(_bitmap->height);
    return 2;
}

static int lua_bitmapColorAt(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    int x = playdate->lua->getArgInt(2);
    int y = playdate->lua->getArgInt(3);
    PDMode7_Color color = bitmapColorAt(bitmap, x, y);
    playdate->lua->pushInt(color.gray);
    playdate->lua->pushInt(color.alpha);
    return 2;
}

static int lua_bitmapGetMask(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    PDMode7_Bitmap *mask = bitmapGetMask(bitmap);
    playdate->lua->pushObject(mask, lua_kBitmap, 0);
    return 1;
}

static int lua_bitmapSetMask(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    PDMode7_Bitmap *mask = playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    bitmapSetMask(bitmap, mask);
    return 0;
}

static int lua_bitmapAddLayer(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(2, lua_kBitmapLayer, NULL);
    bitmapAddLayer(bitmap, layer);
    return 0;
}

static int lua_bitmapGetLayers(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    _PDMode7_Array *layers = _bitmapGetLayers(bitmap);
    _PDMode7_LuaArray *luaArray = newLuaArray(layers, kPDMode7LuaItemBitmapLayer, 0);
    playdate->lua->pushObject(luaArray, lua_kArray, 0);
    return 1;
}

static int lua_bitmapRemoveAllLayers(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    bitmapRemoveAllLayers(bitmap);
    return 0;
}

static int lua_freeBitmap(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    freeBitmap(bitmap);
    return 0;
}

static const lua_reg lua_bitmap[] = {
    { "loadPGM", lua_loadPGM },
    { "getSize", lua_bitmapGetSize },
    { "_colorAt", lua_bitmapColorAt },
    { "getMask", lua_bitmapGetMask },
    { "setMask", lua_bitmapSetMask },
    { "addLayer", lua_bitmapAddLayer },
    { "getLayers", lua_bitmapGetLayers },
    { "removeAllLayers", lua_bitmapRemoveAllLayers },
    { "__gc", lua_freeBitmap },
    { NULL, NULL }
};

static int lua_newBitmapLayer(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    PDMode7_BitmapLayer *layer = newBitmapLayer(bitmap);
    _PDMode7_BitmapLayer *_layer = layer->prv;
    _layer->luaRef = playdate->lua->pushObject(layer, lua_kBitmapLayer, 0);
    return 1;
}

static int lua_bitmapLayerGetPosition(lua_State *L)
{
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(1, lua_kBitmapLayer, NULL);
    int x; int y;
    bitmapLayerGetPosition(layer, &x, &y);
    playdate->lua->pushInt(x);
    playdate->lua->pushInt(y);
    return 2;
}

static int lua_bitmapLayerSetPosition(lua_State *L)
{
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(1, lua_kBitmapLayer, NULL);
    int x = playdate->lua->getArgInt(2);
    int y = playdate->lua->getArgInt(3);
    bitmapLayerSetPosition(layer, x, y);
    return 0;
}

static int lua_bitmapLayerGetBitmap(lua_State *L)
{
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(1, lua_kBitmapLayer, NULL);
    PDMode7_Bitmap *bitmap = bitmapLayerGetBitmap(layer);
    playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
    return 1;
}

static int lua_bitmapLayerSetBitmap(lua_State *L)
{
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(1, lua_kBitmapLayer, NULL);
    PDMode7_Bitmap *bitmap =  playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    bitmapLayerSetBitmap(layer, bitmap);
    return 0;
}

static int lua_bitmapLayerIsVisible(lua_State *L)
{
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(1, lua_kBitmapLayer, NULL);
    int visible = bitmapLayerIsVisible(layer);
    playdate->lua->pushBool(visible);
    return 1;
}

static int lua_bitmapLayerSetVisible(lua_State *L)
{
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(1, lua_kBitmapLayer, NULL);
    int visible = playdate->lua->getArgBool(2);
    bitmapLayerSetVisible(layer, visible);
    return 0;
}

static int lua_bitmapLayerInvalidate(lua_State *L)
{
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(1, lua_kBitmapLayer, NULL);
    bitmapLayerInvalidate(layer);
    return 0;
}

static int lua_removeBitmapLayer(lua_State *L)
{
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(1, lua_kBitmapLayer, NULL);
    removeBitmapLayer(layer);
    return 0;
}

static int lua_freeBitmapLayer(lua_State *L)
{
    PDMode7_BitmapLayer *layer = playdate->lua->getArgObject(1, lua_kBitmapLayer, NULL);
    freeBitmapLayer(layer);
    return 0;
}

static const lua_reg lua_bitmapLayer[] = {
    { "new", lua_newBitmapLayer },
    { "getPosition", lua_bitmapLayerGetPosition },
    { "setPosition", lua_bitmapLayerSetPosition },
    { "getBitmap", lua_bitmapLayerGetBitmap },
    { "setBitmap", lua_bitmapLayerSetBitmap },
    { "isVisible", lua_bitmapLayerIsVisible },
    { "setVisible", lua_bitmapLayerSetVisible },
    { "invalidate", lua_bitmapLayerInvalidate },
    { "removeFromBitmap", lua_removeBitmapLayer },
    { "__gc", lua_freeBitmapLayer },
    { NULL, NULL }
};

static int lua_backgroundGetBitmap(lua_State *L)
{
    PDMode7_Background *background = playdate->lua->getArgObject(1, lua_kBackground, NULL);
    _PDMode7_LuaBitmap *bitmap = backgroundGetLuaBitmap(background);
    playdate->lua->pushObject(bitmap, lua_kImage, 0);
    return 1;
}

static int lua_backgroundSetBitmap(lua_State *L)
{
    PDMode7_Background *background = playdate->lua->getArgObject(1, lua_kBackground, NULL);
    _PDMode7_LuaBitmap *luaBitmap = playdate->lua->getArgObject(2, lua_kImage, NULL);
    
    LCDBitmap *LCDBitmap = NULL;
    if(luaBitmap)
    {
        LCDBitmap = luaBitmap->LCDBitmap;
    }
    
    backgroundSetBitmap(background, LCDBitmap, luaBitmap);
    
    return 0;
}

static int lua_backgroundGetRoundingIncrement(lua_State *L)
{
    PDMode7_Background *background = playdate->lua->getArgObject(1, lua_kBackground, NULL);
    unsigned int x; unsigned int y;
    backgroundGetRoundingIncrement(background, &x, &y);
    playdate->lua->pushInt(x);
    playdate->lua->pushInt(y);
    return 2;
}

static int lua_backgroundSetRoundingIncrement(lua_State *L)
{
    PDMode7_Background *background = playdate->lua->getArgObject(1, lua_kBackground, NULL);
    int x = playdate->lua->getArgInt(2);
    int y = playdate->lua->getArgInt(3);
    backgroundSetRoundingIncrement(background, x, y);
    return 0;
}

static int lua_backgroundGetOffset(lua_State *L)
{
    PDMode7_Background *background = playdate->lua->getArgObject(1, lua_kBackground, NULL);
    PDMode7_Vec2 offset = backgroundGetOffset_public(background);
    playdate->lua->pushFloat(offset.x);
    playdate->lua->pushFloat(offset.y);
    return 2;
}

static int lua_backgroundGetCenter(lua_State *L)
{
    PDMode7_Background *background = playdate->lua->getArgObject(1, lua_kBackground, NULL);
    PDMode7_Vec2 center = backgroundGetCenter(background);
    playdate->lua->pushFloat(center.x);
    playdate->lua->pushFloat(center.y);
    return 2;
}

static int lua_backgroundSetCenter(lua_State *L)
{
    PDMode7_Background *background = playdate->lua->getArgObject(1, lua_kBackground, NULL);
    float x = playdate->lua->getArgFloat(2);
    float y = playdate->lua->getArgFloat(3);
    backgroundSetCenter(background, x, y);
    return 0;
}

static const lua_reg lua_background[] = {
    { "getImage", lua_backgroundGetBitmap },
    { "setImage", lua_backgroundSetBitmap },
    { "getCenter", lua_backgroundGetCenter },
    { "setCenter", lua_backgroundSetCenter },
    { "getOffset", lua_backgroundGetOffset },
    { "getRoundingIncrement", lua_backgroundGetRoundingIncrement },
    { "setRoundingIncrement", lua_backgroundSetRoundingIncrement },
    { NULL, NULL }
};

void PDMode7_init(PlaydateAPI *pd, int enableLua)
{
    playdate = pd;
    
    pool = newPool();
    gc = newGC();
    
    mode7 = playdate->system->realloc(NULL, sizeof(PDMode7_API));

    mode7->pool = playdate->system->realloc(NULL, sizeof(PDMode7_Pool_API));
    mode7->pool->realloc = poolRealloc_public;
    mode7->pool->clear = poolClear_public;
    
    mode7->world = playdate->system->realloc(NULL, sizeof(PDMode7_World_API));
    mode7->world->defaultConfiguration = defaultWorldConfiguration;
    mode7->world->newWorld = worldWithConfiguration;
    mode7->world->getMainDisplay = worldGetMainDisplay; // LUACHECK
    mode7->world->getMainCamera = worldGetMainCamera;
    mode7->world->getSize = worldGetSize;
    mode7->world->getScale = worldGetScale;
    mode7->world->update = worldUpdate; // LUACHECK
    mode7->world->draw = worldDraw; // LUACHECK
    mode7->world->getPlaneBitmap = getPlaneBitmap; // LUACHECK
    mode7->world->setPlaneBitmap = setPlaneBitmap; // LUACHECK
    mode7->world->getPlaneFillColor = getPlaneFillColor; // LUACHECK
    mode7->world->setPlaneFillColor = setPlaneFillColor; // LUACHECK
    mode7->world->getCeilingBitmap = getCeilingBitmap; // LUACHECK
    mode7->world->setCeilingBitmap = setCeilingBitmap; // LUACHECK
    mode7->world->getCeilingFillColor = getCeilingFillColor; // LUACHECK
    mode7->world->setCeilingFillColor = setCeilingFillColor; // LUACHECK
    mode7->world->addSprite = addSprite; // LUACHECK
    mode7->world->addDisplay = addDisplay; // LUACHECK
    mode7->world->getSprites = getSprites; // LUACHECK
    mode7->world->getVisibleSpriteInstances = getVisibleSpriteInstances; // LUACHECK
    mode7->world->displayToPlanePoint = displayToPlanePoint_public; // LUACHECK
    mode7->world->worldToDisplayPoint = worldToDisplayPoint_public; // LUACHECK
    mode7->world->displayMultiplierForScanlineAt = displayMultiplierForScanlineAt_public; // LUACHECK
    mode7->world->bitmapToPlanePoint = bitmapToPlanePoint; // LUACHECK
    mode7->world->planeToBitmapPoint = planeToBitmapPoint; // LUACHECK
    mode7->world->freeWorld = freeWorld; // LUACHECK
    
    mode7->display = playdate->system->realloc(NULL, sizeof(PDMode7_Display_API));
    mode7->display->newDisplay = newDisplay; // LUACHECK
    mode7->display->getRect = displayGetRect; // LUACHECK
    mode7->display->setRect = displaySetRect; // LUACHECK
    mode7->display->getOrientation = displayGetOrientation; // LUACHECK
    mode7->display->setOrientation = displaySetOrientation; // LUACHECK
    mode7->display->getFlipMode = displayGetFlipMode; // LUACHECK
    mode7->display->setFlipMode = displaySetFlipMode; // LUACHECK
    mode7->display->setOrientation = displaySetOrientation; // LUACHECK
    mode7->display->getCamera = displayGetCamera; // LUACHECK
    mode7->display->setCamera = displaySetCamera; // LUACHECK
    mode7->display->getScale = displayGetScale; // LUACHECK
    mode7->display->setScale = displaySetScale; // LUACHECK
    mode7->display->getHorizon = displayGetHorizon; // LUACHECK
    mode7->display->getHorizon = displayGetHorizon; // LUACHECK
    mode7->display->pitchForHorizon = displayPitchForHorizon; // LUACHECK
    mode7->display->getBackground = displayGetBackground; // LUACHECK
    mode7->display->convertPointFromOrientation = displayConvertPointFromOrientation; // LUACHECK
    mode7->display->convertPointToOrientation = displayConvertPointToOrientation; // LUACHECK
    mode7->display->getWorld = displayGetWorld; // LUACHECK
    mode7->display->removeFromWorld = removeDisplay; // LUACHECK
    
    mode7->background = playdate->system->realloc(NULL, sizeof(PDMode7_Background_API));
    mode7->background->getBitmap = backgroundGetBitmap; // LUACHECK
    mode7->background->setBitmap = backgroundSetBitmap_public; // LUACHECK
    mode7->background->getCenter = backgroundGetCenter; // LUACHECK
    mode7->background->setCenter = backgroundSetCenter; // LUACHECK
    mode7->background->getRoundingIncrement = backgroundGetRoundingIncrement; // LUACHECK
    mode7->background->setRoundingIncrement = backgroundSetRoundingIncrement; // LUACHECK
    mode7->background->getOffset = backgroundGetOffset_public; // LUACHECK

    mode7->camera = playdate->system->realloc(NULL, sizeof(PDMode7_Camera_API));
    mode7->camera->newCamera = newCamera; // LUACHECK
    mode7->camera->getPosition = cameraGetPosition; // LUACHECK
    mode7->camera->setPosition = cameraSetPosition; // LUACHECK
    mode7->camera->getAngle = cameraGetAngle; // LUACHECK
    mode7->camera->setAngle = cameraSetAngle; // LUACHECK
    mode7->camera->getFOV = cameraGetFOV; // LUACHECK
    mode7->camera->setFOV = cameraSetFOV; // LUACHECK
    mode7->camera->getPitch = cameraGetPitch; // LUACHECK
    mode7->camera->setPitch = cameraSetPitch; // LUACHECK
    mode7->camera->getClipDistanceUnits = cameraGetClipDistanceUnits; // LUACHECK
    mode7->camera->setClipDistanceUnits = cameraSetClipDistanceUnits; // LUACHECK
    mode7->camera->lookAtPoint = cameraLookAtPoint; // LUACHECK
    mode7->camera->freeCamera = freeCamera; // LUACHECK

    mode7->sprite = playdate->system->realloc(NULL, sizeof(PDMode7_Sprite_API));
    mode7->sprite->newSprite = newSprite; // LUACHECK
    mode7->sprite->setPosition = spriteSetPosition; // LUACHECK
    mode7->sprite->getPosition = spriteGetPosition; // LUACHECK
    mode7->sprite->getSize = spriteGetSize; // LUACHECK
    mode7->sprite->setSize = spriteSetSize; // LUACHECK
    mode7->sprite->setVisible = spriteSetVisible; // LUACHECK
    mode7->sprite->setImageCenter = spriteSetImageCenter; // LUACHECK
    mode7->sprite->setRoundingIncrement = spriteSetRoundingIncrement; // LUACHECK
    mode7->sprite->setAlignment = spriteSetAlignment; // LUACHECK
    mode7->sprite->getAngle = spriteGetAngle; // LUACHECK
    mode7->sprite->setAngle = spriteSetAngle; // LUACHECK
    mode7->sprite->getPitch = spriteGetPitch; // LUACHECK
    mode7->sprite->setPitch = spriteSetPitch; // LUACHECK
    mode7->sprite->setFrame = spriteSetFrame; // LUACHECK
    mode7->sprite->setBillboardSizeBehavior = spriteSetBillboardSizeBehavior; // LUACHECK
    mode7->sprite->setBillboardSize = spriteSetBillboardSize; // LUACHECK
    mode7->sprite->setBitmapTable = spriteSetBitmapTable_public; // LUACHECK
    mode7->sprite->getInstance = spriteGetInstance; // LUACHECK
    mode7->sprite->setUserData = spriteSetUserData;
    mode7->sprite->setDrawFunction = spriteSetDrawFunction_c;
    mode7->sprite->getWorld = spriteGetWorld; // LUACHECK
    mode7->sprite->removeFromWorld = removeSprite; // LUACHECK
    mode7->sprite->freeSprite = freeSprite; // LUACHECK
    
    mode7->sprite->dataSource = playdate->system->realloc(NULL, sizeof(PDMode7_SpriteDataSource_API));
    mode7->sprite->dataSource->setMinimumWidth = spriteDataSourceSetMinimumWidth; // LUACHECK
    mode7->sprite->dataSource->setMaximumWidth = spriteDataSourceSetMaximumWidth; // LUACHECK
    mode7->sprite->dataSource->setLengthForKey = spriteDataSourceSetLength; // LUACHECK
    mode7->sprite->dataSource->setLayout = spriteDataSourceSetLayout; // LUACHECK
    
    mode7->spriteInstance = playdate->system->realloc(NULL, sizeof(PDMode7_SpriteInstance_API));
    mode7->spriteInstance->getSprite = spriteInstanceGetSprite; // LUACHECK
    mode7->spriteInstance->getDataSource = _spriteGetDataSource; // LUACHECK=spriteInstanceGetDataSource
    mode7->spriteInstance->isVisible = _spriteGetVisible; // LUACHECK=spriteInstanceGetVisible
    mode7->spriteInstance->setVisible = _spriteSetVisible; // LUACHECK=spriteInstanceSetVisible
    mode7->spriteInstance->getImageCenter = _spriteGetImageCenter; // LUACHECK=spriteInstanceGetImageCenter
    mode7->spriteInstance->setImageCenter = _spriteSetImageCenter; // LUACHECK=spriteInstanceSetImageCenter
    mode7->spriteInstance->getRoundingIncrement = _spriteGetRoundingIncrement; // LUACHECK=spriteInstanceGetRoundingIncrement
    mode7->spriteInstance->setRoundingIncrement = _spriteSetRoundingIncrement; // LUACHECK=spriteInstanceSetRoundingIncrement
    mode7->spriteInstance->getAlignment = _spriteGetAlignment; // LUACHECK=spriteInstanceSetAlignment
    mode7->spriteInstance->setAlignment = _spriteSetAlignment; // LUACHECK=spriteInstanceGetAlignment
    mode7->spriteInstance->getBitmapTable = _spriteGetBitmapTable; // LUACHECK=spriteInstanceGetBitmapTable
    mode7->spriteInstance->setBitmapTable = _spriteSetBitmapTable_public; // LUACHECK=spriteInstanceSetBitmapTable
    mode7->spriteInstance->setDrawFunction = _spriteSetDrawFunction_c;
    mode7->spriteInstance->getFrame = _spriteGetFrame; // LUACHECK=spriteInstanceGetFrame
    mode7->spriteInstance->setFrame = _spriteSetFrame; // LUACHECK=spriteInstanceSetFrame
    mode7->spriteInstance->getBillboardSizeBehavior = _spriteGetBillboardSizeBehavior; // LUACHECK=spriteInstanceGetBillboardSizeBehavior
    mode7->spriteInstance->setBillboardSizeBehavior = _spriteSetBillboardSizeBehavior; // LUACHECK=spriteInstanceSetBillboardSizeBehavior
    mode7->spriteInstance->getBillboardSize = _spriteGetBillboardSize; // LUACHECK=spriteInstanceGetBillboardSize
    mode7->spriteInstance->setBillboardSize = _spriteSetBillboardSize; // LUACHECK=spriteInstanceSetBillboardSize
    mode7->spriteInstance->getBitmap = _spriteGetBitmap; // LUACHECK=spriteInstanceGetBitmap
    mode7->spriteInstance->getDisplayRect = _spriteGetDisplayRect; // LUACHECK=spriteInstanceGetDisplayRect
    mode7->spriteInstance->isVisibleOnDisplay = _spriteIsVisibleOnDisplay; // LUACHECK=spriteInstanceIsVisibleOnDisplay
    mode7->spriteInstance->getUserData = _spriteGetUserData;
    mode7->spriteInstance->setUserData = _spriteSetUserData;
    
    mode7->spriteInstance->dataSource = playdate->system->realloc(NULL, sizeof(PDMode7_SpriteInstanceDataSource_API));
    mode7->spriteInstance->dataSource->getMinimumWidth = _spriteDataSourceGetMinimumWidth; // LUACHECK=spriteInstanceDataSourceGetMinimumWidth
    mode7->spriteInstance->dataSource->setMinimumWidth = _spriteDataSourceSetMinimumWidth; // LUACHECK=spriteInstanceDataSourceSetMinimumWidth
    mode7->spriteInstance->dataSource->getMaximumWidth = _spriteDataSourceGetMaximumWidth; // LUACHECK=spriteInstanceDataSourceGetMaximumWidth
    mode7->spriteInstance->dataSource->setMaximumWidth = _spriteDataSourceSetMaximumWidth; // LUACHECK=spriteInstanceDataSourceSetMaximumWidth
    mode7->spriteInstance->dataSource->getLengthForKey = _spriteDataSourceGetLength; // LUACHECK=spriteInstanceDataSourceGetLength
    mode7->spriteInstance->dataSource->setLengthForKey = _spriteDataSourceSetLength; // LUACHECK=spriteInstanceDataSourceSetLength
    mode7->spriteInstance->dataSource->getLayout = _spriteDataSourceGetLayout; // LUACHECK=spriteInstanceDataSourceGetLayout
    mode7->spriteInstance->dataSource->setLayout = _spriteDataSourceSetLayout; // LUACHECK=spriteInstanceDataSourceSetLayout
    
    mode7->bitmap = playdate->system->realloc(NULL, sizeof(PDMode7_Bitmap_API));
    mode7->bitmap->loadPGM = loadPGM; // LUACHECK
    mode7->bitmap->getSize = bitmapGetSize; // LUACHECK
    mode7->bitmap->colorAt = bitmapColorAt; // LUACHECK
    mode7->bitmap->getMask = bitmapGetMask; // LUACHECK
    mode7->bitmap->setMask = bitmapSetMask; // LUACHECK
    mode7->bitmap->addLayer = bitmapAddLayer; // LUACHECK
    mode7->bitmap->getLayers = bitmapGetLayers; // LUACHECK
    mode7->bitmap->removeAllLayers = bitmapRemoveAllLayers; // LUACHECK
    mode7->bitmap->freeBitmap = freeBitmap; // LUACHECK
    
    mode7->bitmap->layer = playdate->system->realloc(NULL, sizeof(PDMode7_BitmapLayer_API));
    mode7->bitmap->layer->newLayer = newBitmapLayer; // LUACHECK
    mode7->bitmap->layer->getBitmap = bitmapLayerGetBitmap; // LUACHECK
    mode7->bitmap->layer->setBitmap = bitmapLayerSetBitmap; // LUACHECK
    mode7->bitmap->layer->getPosition = bitmapLayerGetPosition; // LUACHECK
    mode7->bitmap->layer->setPosition = bitmapLayerSetPosition; // LUACHECK
    mode7->bitmap->layer->isVisible = bitmapLayerIsVisible; // LUACHECK
    mode7->bitmap->layer->setVisible = bitmapLayerSetVisible; // LUACHECK
    mode7->bitmap->layer->invalidate = bitmapLayerInvalidate; // LUACHECK
    mode7->bitmap->layer->removeFromBitmap = removeBitmapLayer; // LUACHECK
    mode7->bitmap->layer->freeLayer = freeBitmapLayer; // LUACHECK
    
    mode7->vec2 = playdate->system->realloc(NULL, sizeof(PDMode7_Vec2_API));
    mode7->vec2->newVec2 = newVec2;

    mode7->vec3 = playdate->system->realloc(NULL, sizeof(PDMode7_Vec3_API));
    mode7->vec3->newVec3 = newVec3;
    
    mode7->rect = playdate->system->realloc(NULL, sizeof(PDMode7_Rect_API));
    mode7->rect->newRect = newRect;
    
    mode7->color = playdate->system->realloc(NULL, sizeof(PDMode7_Color_API));
    mode7->color->newGrayscaleColor = newGrayscaleColor;

    if(enableLua)
    {
        playdate->lua->registerClass(lua_kWorld, lua_world, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kDisplay, lua_display, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kBitmap, lua_bitmap, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kBitmapLayer, lua_bitmapLayer, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kCamera, lua_camera, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kSprite, lua_sprite, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kMode7SpriteDataSource, lua_spriteDataSource, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kSpriteInstance, lua_spriteInstance, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kSpriteInstanceDataSource, lua_spriteInstanceDataSource, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kImageTable, lua_imageTable, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kImage, lua_image, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kBackground, lua_background, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kArray, lua_array, NULL, 0, NULL);

        playdate->lua->addFunction(lua_poolRealloc, "mode7.pool.realloc", NULL);
        playdate->lua->addFunction(lua_poolClear, "mode7.pool.clear", NULL);
    }
}
