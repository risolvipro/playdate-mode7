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
#define MODE7_INFINITY_E 0.5f

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

typedef struct PDMode7_SpriteDataSource {
    PDMode7_SpriteInstance *instance;
    int minimumWidth;
    int maximumWidth;
    unsigned int lengths[MODE7_SPRITE_DSOURCE_LEN];
    PDMode7_SpriteDataSourceKey layoutKeys[MODE7_SPRITE_DSOURCE_LEN];
} PDMode7_SpriteDataSource;

typedef struct PDMode7_SpriteInstance {
    PDMode7_Sprite *sprite;
    int index;
    uint8_t visible;
    PDMode7_SpriteVisibilityMode visibilityMode;
    PDMode7_Vec2ui roundingIncrement;
    PDMode7_Vec2 imageCenter;
    PDMode7_SpriteAlignment alignmentX;
    PDMode7_SpriteAlignment alignmentY;
    LCDBitmap *bitmap;
    PDMode7_Rect displayRect;
    float distance;
    unsigned int frame;
    PDMode7_Vec2 billboardSize;
    PDMode7_SpriteBillboardSizeBehavior billboardSizeBehavior;
    PDMode7_SpriteDataSource *dataSource;
    LCDBitmapTable *bitmapTable;
    _PDMode7_LuaBitmapTable *luaBitmapTable;
    _PDMode7_Callback *drawCallback;
    void *userdata;
} PDMode7_SpriteInstance;

typedef struct PDMode7_Sprite {
    PDMode7_World *world;
    PDMode7_Vec3 size;
    PDMode7_Vec3 position;
    float angle;
    float pitch;
    PDMode7_SpriteInstance *instances[MODE7_MAX_DISPLAYS];
    _PDMode7_Array *gridCells;
    LuaUDObject *luaRef;
    _PDMode7_LuaSpriteDataSource *luaDataSource;
} PDMode7_Sprite;

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

typedef enum {
    PDMode7_ShaderTypeLinear,
    PDMode7_ShaderTypeRadial
} PDMode7_ShaderType;

typedef struct PDMode7_Shader {
    PDMode7_ShaderType objectType;
    void *object;
    LuaUDObject *luaRef;
} PDMode7_Shader;

typedef struct PDMode7_LinearShader {
    PDMode7_Shader shader;
    PDMode7_Color color;
    float minDistance;
    float maxDistance;
    uint8_t inverted;
    uint8_t alpha;
    float delta_inv;
} PDMode7_LinearShader;

typedef struct PDMode7_RadialShader {
    PDMode7_Shader shader;
    PDMode7_Color color;
    float minDistance;
    float maxDistance;
    uint8_t inverted;
    PDMode7_Vec2 offset;
    PDMode7_Vec2 origin;
    float minSquared;
    float delta_inv;
} PDMode7_RadialShader;

typedef struct PDMode7_Tile {
    PDMode7_Bitmap *bitmap;
    uint8_t scale_log;
} PDMode7_Tile;

typedef struct PDMode7_Tilemap {
    int tileWidth;
    int tileHeight;
    uint8_t tileWidth_log;
    uint8_t tileHeight_log;
    int rows;
    int columns;
    PDMode7_Tile *tiles;
    PDMode7_Bitmap *fillBitmap;
    uint8_t fillBitmapScale_log;
    LuaUDObject *luaRef;
} PDMode7_Tilemap;

typedef struct PDMode7_Background {
    LCDBitmap *bitmap;
    _PDMode7_LuaBitmap *luaBitmap;
    int width;
    int height;
    PDMode7_Vec2 center;
    PDMode7_Vec2ui roundingIncrement;
    PDMode7_Display *display;
} PDMode7_Background;

typedef struct PDMode7_Plane {
    PDMode7_Bitmap *bitmap;
    PDMode7_Color fillColor;
    PDMode7_Tilemap *tilemap;
} PDMode7_Plane;

typedef struct PDMode7_World {
    int width;
    int height;
    int depth;
    PDMode7_Display *mainDisplay;
    PDMode7_Display* displays[MODE7_MAX_DISPLAYS];
    int numberOfDisplays;
    PDMode7_Camera *mainCamera;
    PDMode7_Plane plane;
    PDMode7_Plane ceiling;
    _PDMode7_Array *sprites;
    _PDMode7_Grid *grid;
} PDMode7_World;

typedef struct PDMode7_Camera {
    float angle;
    PDMode7_Vec3 position;
    float fov;
    float pitch;
    int clipDistanceUnits;
    int isManaged;
    LuaUDObject *luaRef;
} PDMode7_Camera;

typedef struct PDMode7_Display {
    PDMode7_World *world;
    PDMode7_Rect rect;
    PDMode7_Rect absoluteRect;
    PDMode7_DitherType ditherType;
    PDMode7_DisplayOrientation orientation;
    PDMode7_DisplayFlipMode flipMode;
    LCDBitmap *secondaryFramebuffer;
    PDMode7_Camera *camera;
    PDMode7_DisplayScale scale;
    _PDMode7_Array *visibleInstances;
    PDMode7_Background *background;
    PDMode7_Shader *planeShader;
    PDMode7_Shader *ceilingShader;
    uint8_t isManaged;
    LuaUDObject *luaRef;
} PDMode7_Display;

typedef struct {
    uint8_t *data;
    size_t size;
    PDMode7_Rect rect;
    int offsetX;
    int offsetY;
} _PDMode7_BitmapLayerCompositing;

typedef struct PDMode7_BitmapLayer {
    PDMode7_Rect rect;
    uint8_t enabled;
    uint8_t canRestore;
    uint8_t visible;
    PDMode7_Bitmap *bitmap;
    PDMode7_Bitmap *parentBitmap;
    _PDMode7_BitmapLayerCompositing comp;
    LuaUDObject *luaRef;
} PDMode7_BitmapLayer;

typedef struct PDMode7_Bitmap {
    uint8_t *data;
    int width;
    int height;
    PDMode7_Bitmap *mask;
    _PDMode7_Array *layers;
    uint8_t isManaged;
    uint8_t freeData;
    LuaUDObject *luaRef;
} PDMode7_Bitmap;

typedef struct {
    PDMode7_Vec2 screenRatio;
    float worldScaleInv;
    float ndc_inf;
    int planeHeight;
    int horizon;
    PDMode7_Vec2 halfFov;
    PDMode7_Vec2 tanHalfFov;
    PDMode7_Vec2 fovRatio;
    PDMode7_Vec3 forwardVec;
    PDMode7_Vec3 rightVec;
    PDMode7_Vec3 upVec;
} _PDMode7_Parameters;

typedef struct {
    const uint8_t *data;
    uint8_t len;
    uint8_t mul;
    uint8_t mod;
} _PDMode7_DitherPattern;

static _PDMode7_Pool *pool;
static _PDMode7_GC *gc;

static const uint8_t patterns2x2[5 * 2] = {
    0b00000000, 0b00000000,
    0b10101010, 0b00000000,
    0b10101010, 0b01010101,
    0b11111111, 0b01010101,
    0b11111111, 0b11111111
};

static const uint8_t patterns4x4[17 * 4] = {
    0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b10001000, 0b00000000, 0b00000000, 0b00000000,
    0b10001000, 0b00000000, 0b00100010, 0b00000000,
    0b10101010, 0b00000000, 0b00100010, 0b00000000,
    0b10101010, 0b00000000, 0b10101010, 0b00000000,
    0b10101010, 0b01000100, 0b10101010, 0b00000000,
    0b10101010, 0b01000100, 0b10101010, 0b00010001,
    0b10101010, 0b01010101, 0b10101010, 0b00010001,
    0b10101010, 0b01010101, 0b10101010, 0b01010101,
    0b11101110, 0b01010101, 0b10101010, 0b01010101,
    0b11101110, 0b01010101, 0b10111011, 0b01010101,
    0b11111111, 0b01010101, 0b10111011, 0b01010101,
    0b11111111, 0b01010101, 0b11111111, 0b01010101,
    0b11111111, 0b11011101, 0b11111111, 0b01010101,
    0b11111111, 0b11011101, 0b11111111, 0b01110111,
    0b11111111, 0b11111111, 0b11111111, 0b01110111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111
};

static const uint8_t patterns8x8[65 * 8] = {
    0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
    0b10000000, 0b00000000, 0b00000000, 0b00000000, 0b00001000, 0b00000000, 0b00000000, 0b00000000,
    0b10001000, 0b00000000, 0b00000000, 0b00000000, 0b00001000, 0b00000000, 0b00000000, 0b00000000,
    0b10001000, 0b00000000, 0b00000000, 0b00000000, 0b10001000, 0b00000000, 0b00000000, 0b00000000,
    0b10001000, 0b00000000, 0b00100000, 0b00000000, 0b10001000, 0b00000000, 0b00000000, 0b00000000,
    0b10001000, 0b00000000, 0b00100000, 0b00000000, 0b10001000, 0b00000000, 0b00000010, 0b00000000,
    0b10001000, 0b00000000, 0b00100010, 0b00000000, 0b10001000, 0b00000000, 0b00000010, 0b00000000,
    0b10001000, 0b00000000, 0b00100010, 0b00000000, 0b10001000, 0b00000000, 0b00100010, 0b00000000,
    0b10101000, 0b00000000, 0b00100010, 0b00000000, 0b10001000, 0b00000000, 0b00100010, 0b00000000,
    0b10101000, 0b00000000, 0b00100010, 0b00000000, 0b10001010, 0b00000000, 0b00100010, 0b00000000,
    0b10101010, 0b00000000, 0b00100010, 0b00000000, 0b10001010, 0b00000000, 0b00100010, 0b00000000,
    0b10101010, 0b00000000, 0b00100010, 0b00000000, 0b10101010, 0b00000000, 0b00100010, 0b00000000,
    0b10101010, 0b00000000, 0b10100010, 0b00000000, 0b10101010, 0b00000000, 0b00100010, 0b00000000,
    0b10101010, 0b00000000, 0b10100010, 0b00000000, 0b10101010, 0b00000000, 0b00101010, 0b00000000,
    0b10101010, 0b00000000, 0b10101010, 0b00000000, 0b10101010, 0b00000000, 0b00101010, 0b00000000,
    0b10101010, 0b00000000, 0b10101010, 0b00000000, 0b10101010, 0b00000000, 0b10101010, 0b00000000,
    0b10101010, 0b01000000, 0b10101010, 0b00000000, 0b10101010, 0b00000000, 0b10101010, 0b00000000,
    0b10101010, 0b01000000, 0b10101010, 0b00000000, 0b10101010, 0b00000100, 0b10101010, 0b00000000,
    0b10101010, 0b01000100, 0b10101010, 0b00000000, 0b10101010, 0b00000100, 0b10101010, 0b00000000,
    0b10101010, 0b01000100, 0b10101010, 0b00000000, 0b10101010, 0b01000100, 0b10101010, 0b00000000,
    0b10101010, 0b01000100, 0b10101010, 0b00010000, 0b10101010, 0b01000100, 0b10101010, 0b00000000,
    0b10101010, 0b01000100, 0b10101010, 0b00010000, 0b10101010, 0b01000100, 0b10101010, 0b00000001,
    0b10101010, 0b01000100, 0b10101010, 0b00010001, 0b10101010, 0b01000100, 0b10101010, 0b00000001,
    0b10101010, 0b01000100, 0b10101010, 0b00010001, 0b10101010, 0b01000100, 0b10101010, 0b00010001,
    0b10101010, 0b01010100, 0b10101010, 0b00010001, 0b10101010, 0b01000100, 0b10101010, 0b00010001,
    0b10101010, 0b01010100, 0b10101010, 0b00010001, 0b10101010, 0b01000101, 0b10101010, 0b00010001,
    0b10101010, 0b01010101, 0b10101010, 0b00010001, 0b10101010, 0b01000101, 0b10101010, 0b00010001,
    0b10101010, 0b01010101, 0b10101010, 0b00010001, 0b10101010, 0b01010101, 0b10101010, 0b00010001,
    0b10101010, 0b01010101, 0b10101010, 0b01010001, 0b10101010, 0b01010101, 0b10101010, 0b00010001,
    0b10101010, 0b01010101, 0b10101010, 0b01010001, 0b10101010, 0b01010101, 0b10101010, 0b00010101,
    0b10101010, 0b01010101, 0b10101010, 0b01010101, 0b10101010, 0b01010101, 0b10101010, 0b00010101,
    0b10101010, 0b01010101, 0b10101010, 0b01010101, 0b10101010, 0b01010101, 0b10101010, 0b01010101,
    0b11101010, 0b01010101, 0b10101010, 0b01010101, 0b10101010, 0b01010101, 0b10101010, 0b01010101,
    0b11101010, 0b01010101, 0b10101010, 0b01010101, 0b10101110, 0b01010101, 0b10101010, 0b01010101,
    0b11101110, 0b01010101, 0b10101010, 0b01010101, 0b10101110, 0b01010101, 0b10101010, 0b01010101,
    0b11101110, 0b01010101, 0b10101010, 0b01010101, 0b11101110, 0b01010101, 0b10101010, 0b01010101,
    0b11101110, 0b01010101, 0b10111010, 0b01010101, 0b11101110, 0b01010101, 0b10101010, 0b01010101,
    0b11101110, 0b01010101, 0b10111010, 0b01010101, 0b11101110, 0b01010101, 0b10101011, 0b01010101,
    0b11101110, 0b01010101, 0b10111011, 0b01010101, 0b11101110, 0b01010101, 0b10101011, 0b01010101,
    0b11101110, 0b01010101, 0b10111011, 0b01010101, 0b11101110, 0b01010101, 0b10111011, 0b01010101,
    0b11111110, 0b01010101, 0b10111011, 0b01010101, 0b11101110, 0b01010101, 0b10111011, 0b01010101,
    0b11111110, 0b01010101, 0b10111011, 0b01010101, 0b11101111, 0b01010101, 0b10111011, 0b01010101,
    0b11111111, 0b01010101, 0b10111011, 0b01010101, 0b11101111, 0b01010101, 0b10111011, 0b01010101,
    0b11111111, 0b01010101, 0b10111011, 0b01010101, 0b11111111, 0b01010101, 0b10111011, 0b01010101,
    0b11111111, 0b01010101, 0b11111011, 0b01010101, 0b11111111, 0b01010101, 0b10111011, 0b01010101,
    0b11111111, 0b01010101, 0b11111011, 0b01010101, 0b11111111, 0b01010101, 0b10111111, 0b01010101,
    0b11111111, 0b01010101, 0b11111111, 0b01010101, 0b11111111, 0b01010101, 0b10111111, 0b01010101,
    0b11111111, 0b01010101, 0b11111111, 0b01010101, 0b11111111, 0b01010101, 0b11111111, 0b01010101,
    0b11111111, 0b11010101, 0b11111111, 0b01010101, 0b11111111, 0b01010101, 0b11111111, 0b01010101,
    0b11111111, 0b11010101, 0b11111111, 0b01010101, 0b11111111, 0b01011101, 0b11111111, 0b01010101,
    0b11111111, 0b11011101, 0b11111111, 0b01010101, 0b11111111, 0b01011101, 0b11111111, 0b01010101,
    0b11111111, 0b11011101, 0b11111111, 0b01010101, 0b11111111, 0b11011101, 0b11111111, 0b01010101,
    0b11111111, 0b11011101, 0b11111111, 0b01110101, 0b11111111, 0b11011101, 0b11111111, 0b01010101,
    0b11111111, 0b11011101, 0b11111111, 0b01110101, 0b11111111, 0b11011101, 0b11111111, 0b01010111,
    0b11111111, 0b11011101, 0b11111111, 0b01110111, 0b11111111, 0b11011101, 0b11111111, 0b01010111,
    0b11111111, 0b11011101, 0b11111111, 0b01110111, 0b11111111, 0b11011101, 0b11111111, 0b01110111,
    0b11111111, 0b11111101, 0b11111111, 0b01110111, 0b11111111, 0b11011101, 0b11111111, 0b01110111,
    0b11111111, 0b11111101, 0b11111111, 0b01110111, 0b11111111, 0b11011111, 0b11111111, 0b01110111,
    0b11111111, 0b11111111, 0b11111111, 0b01110111, 0b11111111, 0b11011111, 0b11111111, 0b01110111,
    0b11111111, 0b11111111, 0b11111111, 0b01110111, 0b11111111, 0b11111111, 0b11111111, 0b01110111,
    0b11111111, 0b11111111, 0b11111111, 0b11110111, 0b11111111, 0b11111111, 0b11111111, 0b01110111,
    0b11111111, 0b11111111, 0b11111111, 0b11110111, 0b11111111, 0b11111111, 0b11111111, 0b01111111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b01111111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111
};

static const _PDMode7_DitherPattern patterns[3] = {
    { .data = patterns2x2, .len = 5, .mod = 1, .mul = 1},
    { .data = patterns4x4, .len = 17, .mod = 3, .mul = 2},
    { .data = patterns8x8, .len = 65, .mod = 7, .mul = 3}
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
static PDMode7_Plane newPlane(void);
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
static inline PDMode7_Vec2 vec2FromVec3(PDMode7_Vec3 vec);
static inline PDMode7_Vec2ui newVec2ui(unsigned int x, unsigned int y);
static float worldGetRelativeAngle(PDMode7_Vec3 cameraPoint, float cameraAngle, PDMode7_Vec3 targetPoint, float targetAngle, _PDMode7_Parameters *parameters);
static float worldGetRelativePitch(PDMode7_Vec3 cameraPoint, float cameraPitch, PDMode7_Vec3 targetPoint, float targetPitch, _PDMode7_Parameters *p);
static int displayGetHorizon(PDMode7_Display *display);
static PDMode7_Vec3 displayToPlanePoint(PDMode7_Display *display, int displayX, int displayY, _PDMode7_Parameters *p);
static PDMode7_Vec3 worldToDisplayPoint(PDMode7_Display *display, PDMode7_Vec3 point, _PDMode7_Parameters *p);
static PDMode7_Vec3 displayMultiplierForScanlineAt(PDMode7_Display *display, PDMode7_Vec3 point, _PDMode7_Parameters *p);
static inline uint8_t planeColorAt(PDMode7_World *world, PDMode7_Plane *plane, int x, int y);
static inline void worldSetColor(uint8_t color, PDMode7_DisplayScale displayScale, uint8_t *ptr, int rowbytes, _PDMode7_DitherPattern ditherPattern, int bit, int y, int dy);
#if PD_MODE7_SHADER
static void shaderPrepare(PDMode7_Shader *pShader, PDMode7_Display *display, _PDMode7_Parameters *p);
static void shaderPrepareRow(PDMode7_Shader *pShader, PDMode7_Display *display, int y, _PDMode7_Parameters *p);
static inline void shaderApply(PDMode7_Shader *shader, uint8_t *color, PDMode7_Vec3 point, _PDMode7_Parameters *p);
static int shaderSpriteIsVisible(PDMode7_Shader *shader, PDMode7_Sprite *sprite, float distance, _PDMode7_Parameters *p);
#endif
static float distanceAtScanline(int y, PDMode7_Display *display, _PDMode7_Parameters *p);
static void getDisplayScaleStep(PDMode7_DisplayScale scale, int *xStep, int *yStep);
static inline PDMode7_DisplayScale truncatedDisplayScale(PDMode7_DisplayScale scale);
static PDMode7_Camera* displayGetCamera(PDMode7_Display *display);
static void displayGetFramebuffer(PDMode7_Display *display, LCDBitmap **target, uint8_t **framebuffer, int *rowbytes);
static _PDMode7_Parameters worldGetParameters(PDMode7_Display *display);
static float worldGetScaleInv(PDMode7_World *pWorld);
static float worldGetScale(PDMode7_World *pWorld);
static PDMode7_Display* getDisplay(PDMode7_World *pWorld, PDMode7_Display *display);
static PDMode7_Vec2 backgroundGetOffset(PDMode7_Background *background, _PDMode7_Parameters *parameters);
static void spriteSetPosition(PDMode7_Sprite *sprite, float x, float y, float z);
static void spriteBoundsDidChange(PDMode7_Sprite *sprite);
static unsigned int spriteGetTableIndex(PDMode7_SpriteInstance *instance, unsigned int angleIndex, unsigned int pitchIndex, int unsigned scaleIndex);
static void removeSprite(PDMode7_Sprite *sprite);
static int sortSprites(const void *a, const void *b);
static float degToRad(float degrees);
static float roundToIncrement(float number, unsigned int multiple);
static PDMode7_Vec3 vec3_subtract(PDMode7_Vec3 v1, PDMode7_Vec3 v2);
static float vec3_dot(PDMode7_Vec3 v1, PDMode7_Vec3 v2);
static inline PDMode7_Color newGrayscaleColor(uint8_t gray, uint8_t alpha);
static _PDMode7_Grid* newGrid(float width, float height, float depth, int cellSize);
static void gridRemoveSprite(_PDMode7_Grid *grid, PDMode7_Sprite *sprite);
static void gridUpdateSprite(_PDMode7_Grid *grid, PDMode7_Sprite *sprite);
static _PDMode7_Array* gridGetSpritesAtPoint(_PDMode7_Grid *grid, PDMode7_Vec3 point, int distanceUnits);
static void releaseBitmap(PDMode7_Bitmap *bitmap);
static void freeBitmap(PDMode7_Bitmap *bitmap);
static void bitmapLayerSetBitmap(PDMode7_BitmapLayer *layer, PDMode7_Bitmap *bitmap);
static void bitmapLayerDidChange(PDMode7_BitmapLayer *layer);
static void bitmapLayerDraw(PDMode7_BitmapLayer *layer);
static void _removeBitmapLayer(PDMode7_BitmapLayer *layer, int restore);
static void removeBitmapLayer(PDMode7_BitmapLayer *layer);
static inline float radialShaderProgress(PDMode7_RadialShader *radial, float x, float y);
static inline float linearShaderProgress(PDMode7_LinearShader *linear, float distance);
static void releaseTilemap(PDMode7_Tilemap *tilemap);
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
static void releaseShader(PDMode7_Shader *shader);
static void freeGrid(_PDMode7_Grid *grid);
static void lua_spriteCallDrawCallback(PDMode7_SpriteInstance *instance);
static int log2_int(uint32_t n);

static PDMode7_World* worldWithConfiguration(PDMode7_WorldConfiguration configuration)
{
    PDMode7_World *world = playdate->system->realloc(NULL, sizeof(PDMode7_World));
    
    world->width = configuration.width;
    world->height = configuration.height;
    world->depth = configuration.depth;

    world->mainCamera = newCamera();
    world->mainCamera->isManaged = 1;
    
    world->plane = newPlane();
    world->ceiling = newPlane();
    
    world->sprites = newArray();
    world->numberOfDisplays = 0;
    
    world->mainDisplay = newDisplay(0, 0, LCD_COLUMNS, LCD_ROWS);
    world->mainDisplay->isManaged = 1;
    addDisplay(world, world->mainDisplay);
    
    displaySetCamera(world->mainDisplay, world->mainCamera);
    
    world->grid = newGrid(configuration.width, configuration.height, configuration.depth, configuration.gridCellSize);
    
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

static _PDMode7_Parameters worldGetParameters(PDMode7_Display *display)
{
    PDMode7_World *pWorld = display->world;
    PDMode7_Camera *camera = display->camera;
            
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
        .ndc_inf = (MODE7_INFINITY_E / display->rect.height),
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

static PDMode7_Plane newPlane(void)
{
    return (PDMode7_Plane){
        .bitmap = NULL,
        .fillColor = newGrayscaleColor(255, 255),
        .tilemap = NULL
    };
}

static void worldUpdateDisplay(PDMode7_World *world, PDMode7_Display *display)
{
    int displayIndex = indexForDisplay(world, display);
    if(displayIndex < 0)
    {
        return;
    }
    
    PDMode7_Camera *camera = display->camera;

    _PDMode7_Parameters parameters = worldGetParameters(display);
    
    // Clear the visible sprites for current display
    arrayClear(display->visibleInstances);
    
    // Get the close sprites from the grid
    _PDMode7_Array *closeSprites = gridGetSpritesAtPoint(world->grid, camera->position, camera->clipDistanceUnits);
    
    for(int i = 0; i < closeSprites->length; i++)
    {
        PDMode7_Sprite *sprite = closeSprites->items[i];
        
        PDMode7_SpriteInstance *instance = sprite->instances[displayIndex];
        PDMode7_SpriteDataSource *dataSource = instance->dataSource;

        if(instance->visible)
        {
            // Calculate view vector
            PDMode7_Vec3 viewVec = vec3_subtract(sprite->position, camera->position);
            
            // Calculate local point
            float distance = vec3_dot(viewVec, parameters.forwardVec);
            uint8_t visible = (distance > 0);
#if PD_MODE7_SHADER
            if(visible && instance->visibilityMode == kMode7SpriteVisibilityModeShader)
            {
                visible = shaderSpriteIsVisible(display->planeShader, sprite, distance, &parameters);
            }
#endif
            if(visible)
            {
                // Point is in front of camera
                
                PDMode7_Vec3 displayPoint = worldToDisplayPoint(display, sprite->position, &parameters);
                
                displayPoint.x += display->rect.x;
                displayPoint.y += display->rect.y;
                
                PDMode7_Vec3 multiplier = displayMultiplierForScanlineAt(display, sprite->position, &parameters);
                
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
                        
                        unsigned int tableIndex = spriteGetTableIndex(instance, angleIndex, pitchIndex, scaleIndex);
                        
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
                            instance->distance = distance;
                            instance->bitmap = finalBitmap;
                            instance->displayRect = spriteRect;
                            arrayPush(display->visibleInstances, instance);
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
    PDMode7_SpriteInstance *instance1 = *(PDMode7_SpriteInstance**)a;
    PDMode7_SpriteInstance *instance2 = *(PDMode7_SpriteInstance**)b;
    
    if(instance1->distance > instance2->distance)
    {
        return -1;
    }
    else if(instance1->distance < instance2->distance)
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

static void worldUpdate(PDMode7_World *world)
{
    for(int i = 0; i < world->numberOfDisplays; i++)
    {
        PDMode7_Display *display = world->displays[i];
        worldUpdateDisplay(world, display);
    }
}

static void drawBackground(PDMode7_Display *display, _PDMode7_Parameters *parameters)
{
    PDMode7_Background *background = display->background;

    if(!background->bitmap)
    {
        return;
    }
    
    PDMode7_Vec2 offset = backgroundGetOffset(background, parameters);
    
    LCDBitmap *target;
    displayGetFramebuffer(display, &target, NULL, NULL);
    
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

static void drawPlane(PDMode7_World *world, PDMode7_Display *display, _PDMode7_Parameters *parameters)
{
    LCDBitmap *target; uint8_t *framebuffer; int rowbytes;
    displayGetFramebuffer(display, &target, &framebuffer, &rowbytes);
    
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
    
    // Pattern
    int ditherType = (display->ditherType >= 0 && display->ditherType < 3) ? display->ditherType : 0;
    _PDMode7_DitherPattern ditherPattern = patterns[ditherType];
    
    for(int y = 0; y < parameters->planeHeight; y += yStep)
    {
        int relativeY = parameters->horizon + y;
        int absoluteY = display->rect.y + relativeY;
        
        // Left point for the scanline
        PDMode7_Vec3 leftPoint = displayToPlanePoint(display, 0, relativeY, parameters);
        leftPoint.x *= parameters->worldScaleInv;
        leftPoint.y *= parameters->worldScaleInv;
        
        // Right point for the scanline
        PDMode7_Vec3 rightPoint = displayToPlanePoint(display, display->rect.width, relativeY, parameters);
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
#if PD_MODE7_SHADER
        shaderPrepareRow(display->planeShader, display, y, parameters);
#if PD_MODE7_CEILING
        shaderPrepareRow(display->ceilingShader, display, y, parameters);
#endif
#endif
        // Advance pointLeft in the loop
        for(int x = 0; x < display->rect.width; x += xStep)
        {
            int mapX = floorf(leftPoint.x);
            int mapY = floorf(leftPoint.y);
            
            uint8_t color = planeColorAt(world, &world->plane, mapX, mapY);
#if PD_MODE7_SHADER
            shaderApply(display->planeShader, &color, leftPoint, parameters);
#endif
            worldSetColor(color, planeScale, frameStart + frameY + frameX, rowbytes, ditherPattern, bitPosition, absoluteY, 1);
            
#if PD_MODE7_CEILING
            if((world->ceiling.bitmap || world->ceiling.tilemap) && ceilingRelativeY > 0)
            {
                uint8_t ceilingColor = planeColorAt(world, &world->ceiling, mapX, mapY);
#if PD_MODE7_SHADER
                shaderApply(display->ceilingShader, &ceilingColor, leftPoint, parameters);
#endif
                worldSetColor(ceilingColor, ceilingScale, frameStart - frameY - rowbytes + frameX, -rowbytes, ditherPattern, bitPosition, absoluteY - 1, -1);
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
        int startY = absoluteHorizon;
#if PD_MODE7_CEILING
        startY = mode7_max(absoluteHorizon - parameters->planeHeight, 0);
#endif
        playdate->graphics->markUpdatedRows(startY, absoluteHorizon + parameters->planeHeight - 1);
    }
}

static void drawSprite(PDMode7_SpriteInstance *instance)
{
    if(instance->bitmap)
    {
        playdate->graphics->drawBitmap(instance->bitmap, instance->displayRect.x, instance->displayRect.y, kBitmapUnflipped);
    }
}

static void drawSprites(PDMode7_Display *display)
{
    for(int i = 0; i < display->visibleInstances->length; i++)
    {
        PDMode7_SpriteInstance *instance = display->visibleInstances->items[i];
        
        if(instance->drawCallback)
        {
            switch(instance->drawCallback->type)
            {
                case _PDMode7_CallbackTypeC:
                {
                    PDMode7_SpriteDrawCallbackFunction *drawFunction = instance->drawCallback->cFunction;
                    drawFunction(instance, instance->bitmap, instance->displayRect, drawSprite);
                    break;
                }
                case _PDMode7_CallbackTypeLua:
                {
                    lua_spriteCallDrawCallback(instance);
                    break;
                }
            }
        }
        else
        {
            drawSprite(instance);
        }
    }
}

static void worldDraw(PDMode7_World *pWorld, PDMode7_Display *display)
{
    display = getDisplay(pWorld, display);
    int displayIndex = indexForDisplay(pWorld, display);
    if(displayIndex < 0)
    {
        return;
    }
    
    _PDMode7_Parameters parameters = worldGetParameters(display);
    
    LCDBitmap *target;
    displayGetFramebuffer(display, &target, NULL, NULL);
    
    playdate->graphics->pushContext(target);
    
    if(target)
    {
        playdate->graphics->clear(kColorWhite);
    }
    
    if(!target && displayNeedsClip(display))
    {
        playdate->graphics->setClipRect(display->rect.x, display->rect.y, display->rect.width, display->rect.height);
    }
    
#if PD_MODE7_SHADER
    shaderPrepare(display->planeShader, display, &parameters);
#if PD_MODE7_CEILING
    shaderPrepare(display->ceilingShader, display, &parameters);
#endif
#endif
    
    drawBackground(display, &parameters);
    drawPlane(pWorld, display, &parameters);
    drawSprites(display);
    
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

static inline uint8_t planeColorAt(PDMode7_World *world, PDMode7_Plane *plane, int x, int y)
{
#if PD_MODE7_TILEMAP
    PDMode7_Tilemap *tilemap = plane->tilemap;
    PDMode7_Bitmap *bitmap = plane->bitmap;
    if(tilemap)
    {
        if(x >= 0 && y >= 0 && x < world->width && y < world->height)
        {
            int column = x >> tilemap->tileWidth_log;
            int row = y >> tilemap->tileHeight_log;
            
            PDMode7_Tile *tile = &tilemap->tiles[row * tilemap->columns + column];
            
            int tileX = (x & (tilemap->tileWidth - 1)) >> tile->scale_log;
            int tileY = (y & (tilemap->tileHeight - 1)) >> tile->scale_log;
            
            if(tile->bitmap)
            {
                return tile->bitmap->data[tileY * tile->bitmap->width + tileX];
            }
        }
        else if(tilemap->fillBitmap)
        {
            int tileX = (x & (tilemap->tileWidth - 1)) >> tilemap->fillBitmapScale_log;
            int tileY = (y & (tilemap->tileHeight - 1)) >> tilemap->fillBitmapScale_log;
            
            return tilemap->fillBitmap->data[tileY * tilemap->fillBitmap->width + tileX];
        }
    }
    else if(bitmap && x >= 0 && x < bitmap->width && y >= 0 && y < bitmap->height)
    {
        return bitmap->data[bitmap->width * y + x];
    }
#else
    PDMode7_Bitmap *bitmap = plane->bitmap;
    if(bitmap && x >= 0 && x < bitmap->width && y >= 0 && y < bitmap->height)
    {
        return bitmap->data[bitmap->width * y + x];
    }
#endif
    return plane->fillColor.gray;
}

static inline void worldSetColor(uint8_t color, PDMode7_DisplayScale displayScale, uint8_t *ptr, int rowbytes, _PDMode7_DitherPattern p, int bit, int y, int dy)
{
    uint8_t patternIndex = (color * p.len) >> 8;
    
    switch(displayScale)
    {
        case kMode7DisplayScale1x1:
        {
            uint8_t pattern = p.data[(patternIndex << p.mul) + (y & p.mod)];
            uint8_t mask = 0b00000001 << (7 - bit);
            
            *ptr = (*ptr & ~mask) | (pattern & mask);
            
            break;
        }
        case kMode7DisplayScale2x1:
        {
            uint8_t pattern = p.data[(patternIndex << p.mul) + (y & p.mod)];
            uint8_t mask = 0b00000011 << (6 - bit);
            
            *ptr = (*ptr & ~mask) | (pattern & mask);
            
            break;
        }
        case kMode7DisplayScale2x2:
        {
            uint8_t pattern1 = p.data[(patternIndex << p.mul) + (y & p.mod)];
            uint8_t pattern2 = p.data[(patternIndex << p.mul) + ((y + dy) & p.mod)];

            uint8_t mask = 0b00000011 << (6 - bit);
            
            *ptr = (*ptr & ~mask) | (pattern1 & mask);
            ptr += rowbytes;
            *ptr = (*ptr & ~mask) | (pattern2 & mask);

            break;
        }
        case kMode7DisplayScale4x1:
        {
            uint8_t pattern = p.data[(patternIndex << p.mul) + (y & p.mod)];
            uint8_t mask = 0b00001111 << (4 - bit);
            
            *ptr = (*ptr & ~mask) | (pattern & mask);
            
            break;
        }
        case kMode7DisplayScale4x2:
        {
            uint8_t pattern1 = p.data[(patternIndex << p.mul) + (y & p.mod)];
            uint8_t pattern2 = p.data[(patternIndex << p.mul) + ((y + dy) & p.mod)];

            uint8_t mask = 0b00001111 << (4 - bit);
            
            *ptr = (*ptr & ~mask) | (pattern1 & mask);
            ptr += rowbytes;
            *ptr = (*ptr & ~mask) | (pattern2 & mask);
            
            break;
        }
        default:
            break;
    }
}

static PDMode7_Vec3 displayToPlanePoint(PDMode7_Display *display, int displayX, int displayY, _PDMode7_Parameters *p)
{
    PDMode7_Camera *camera = display->camera;
    
    // Calculate the screen coordinates in world space
    float worldScreenX = (displayX * p->screenRatio.x - 1) * p->tanHalfFov.x;
    
    float y_ndc = 1 - displayY * p->screenRatio.y;
    if(y_ndc == 0)
    {
        y_ndc = -(p->ndc_inf);
    }
    float worldScreenY = y_ndc * p->tanHalfFov.y;
    
    // Calculate the direction vector in world space
    float directionX = p->forwardVec.x + p->rightVec.x * worldScreenX + p->upVec.x * worldScreenY;
    float directionY = p->forwardVec.y + p->rightVec.y * worldScreenX + p->upVec.y * worldScreenY;
    float directionZ = p->forwardVec.z + p->rightVec.z * worldScreenX + p->upVec.z * worldScreenY;
    
    float intersectionX = 0;
    float intersectionY = 0;
    float intersectionZ = -1;
    
    if(directionZ < 0)
    {
        // Calculate the intersection point with the plane at z = 0
        float t = -(camera->position.z) / directionZ;
        
        intersectionX = camera->position.x + t * directionX;
        intersectionY = camera->position.y + t * directionY;
        intersectionZ = 0;
    }
    
    return newVec3(intersectionX, intersectionY, intersectionZ);
}

#if PD_MODE7_SHADER
static void shaderPrepare(PDMode7_Shader *shader, PDMode7_Display *display, _PDMode7_Parameters *p)
{
    if(!shader)
    {
        return;
    }
        
    switch(shader->objectType)
    {
        case PDMode7_ShaderTypeLinear:
        {
            PDMode7_LinearShader *linear = shader->object;
            float delta_d = linear->maxDistance - linear->minDistance;
            linear->delta_inv = (delta_d != 0) ? 1.0f / delta_d : 0;
            break;
        }
        case PDMode7_ShaderTypeRadial:
        {
            PDMode7_RadialShader *radial = shader->object;
            PDMode7_Camera *camera = display->camera;
            radial->origin = vec2FromVec3(camera->position);
            float dx = radial->offset.x;
            float dy = radial->offset.y;
            radial->origin.x += dx * cosf(camera->angle + (float)M_PI_2) - dy * sinf(camera->angle + (float)M_PI_2);
            radial->origin.y += dx * sinf(camera->angle + (float)M_PI_2) + dy * cosf(camera->angle + (float)M_PI_2);
            radial->minSquared = radial->minDistance * radial->minDistance;
            float maxSquared = radial->maxDistance * radial->maxDistance;
            float delta_d = maxSquared - radial->minDistance;
            radial->delta_inv = (delta_d != 0) ? 1.0f / delta_d : 0;
            break;
        }
        default:
            break;
    }
}

static void shaderPrepareRow(PDMode7_Shader *shader, PDMode7_Display *display, int y, _PDMode7_Parameters *p)
{
    if(!shader)
    {
        return;
    }
    
    switch(shader->objectType)
    {
        case PDMode7_ShaderTypeLinear:
        {
            PDMode7_LinearShader *linear = shader->object;
            
            float progress = 1;
            float distance = distanceAtScanline(p->horizon + y, display, p);
            if(!isinf(distance)){
                progress = linearShaderProgress(linear, distance);
            }
            if(linear->inverted)
            {
                progress = 1 - progress;
            }
            linear->alpha = roundf(progress * linear->color.alpha);
            break;
        }
        default:
            break;
    }
}

static inline void shaderApply(PDMode7_Shader *shader, uint8_t *color, PDMode7_Vec3 point, _PDMode7_Parameters *p)
{
    if(!shader)
    {
        return;
    }
        
    switch(shader->objectType)
    {
        case PDMode7_ShaderTypeLinear:
        {
            PDMode7_LinearShader *linear = shader->object;
            
            uint8_t alpha = linear->alpha;
            unsigned int compositeColor = linear->color.gray * alpha + *color * (255 - alpha) + 127;
            *color = compositeColor / 255;
            
            break;
        }
        case PDMode7_ShaderTypeRadial:
        {
            PDMode7_RadialShader *radial = shader->object;
            
            float progress = 1;
            if(point.z >= 0)
            {
                progress = radialShaderProgress(radial, point.x, point.y);
            }
            if(radial->inverted)
            {
                progress = 1 - progress;
            }
            uint8_t alpha = roundf(progress * radial->color.alpha);
            unsigned int compositeColor = radial->color.gray * alpha + *color * (255 - alpha) + 127;
            *color = compositeColor / 255;
            break;
        }
        default:
            break;
    }
}

static int shaderSpriteIsVisible(PDMode7_Shader *shader, PDMode7_Sprite *sprite, float distance, _PDMode7_Parameters *p)
{
    if(!shader)
    {
        return 1;
    }
        
    switch(shader->objectType)
    {
        case PDMode7_ShaderTypeLinear:
        {
            PDMode7_LinearShader *linear = shader->object;
            
            float progress = linearShaderProgress(linear, distance);
            if(linear->inverted)
            {
                progress = 1 - progress;
            }
            return (progress < 1);
            break;
        }
        case PDMode7_ShaderTypeRadial:
        {
            PDMode7_RadialShader *radial = shader->object;
            
            float progress = radialShaderProgress(radial, sprite->position.x, sprite->position.y);
            if(radial->inverted)
            {
                progress = 1 - progress;
            }
            return (progress < 1);
            break;
        }
        default:
            break;
    }
    
    return 1;
}
#endif

static float distanceAtScanline(int y, PDMode7_Display *display, _PDMode7_Parameters *p)
{
    PDMode7_Camera *camera = display->camera;
    
    float worldScreenY = (1 - y * p->screenRatio.y) * p->tanHalfFov.y;
    float directionZ_max = p->forwardVec.z + p->upVec.z * worldScreenY;
    if(directionZ_max < 0)
    {
        float d = -camera->position.z / directionZ_max;
        return d;
    }
    return INFINITY;
}

static PDMode7_Vec3 displayToPlanePoint_public(PDMode7_World *pWorld, int displayX, int displayY, PDMode7_Display *display)
{
    display = getDisplay(pWorld, display);
    _PDMode7_Parameters parameters = worldGetParameters(display);
    return displayToPlanePoint(display, displayX, displayY, &parameters);
}

static PDMode7_Vec3 worldToDisplayPoint(PDMode7_Display *display, PDMode7_Vec3 point, _PDMode7_Parameters *p)
{
    PDMode7_Camera *camera = display->camera;
    
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

static PDMode7_Vec3 worldToDisplayPoint_public(PDMode7_World *pWorld, PDMode7_Vec3 point, PDMode7_Display *display)
{
    display = getDisplay(pWorld, display);
    _PDMode7_Parameters parameters = worldGetParameters(display);
    return worldToDisplayPoint(display, point, &parameters);
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

static PDMode7_Vec3 displayMultiplierForScanlineAt(PDMode7_Display *display, PDMode7_Vec3 point, _PDMode7_Parameters *p)
{
    PDMode7_Camera *camera = display->camera;
    
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

static PDMode7_Vec3 displayMultiplierForScanlineAt_public(PDMode7_World *pWorld, PDMode7_Vec3 point, PDMode7_Display *display)
{
    display = getDisplay(pWorld, display);
    _PDMode7_Parameters parameters = worldGetParameters(display);
    return displayMultiplierForScanlineAt(display, point, &parameters);
}

static void addSprite(PDMode7_World *world, PDMode7_Sprite *sprite)
{
    if(sprite->world)
    {
        return;
    }
    
    if(sprite->luaRef)
    {
        playdate->lua->retainObject(sprite->luaRef);
    }

    sprite->world = world;
    arrayPush(world->sprites, sprite);
    
    spriteBoundsDidChange(sprite);
}

static int addDisplay(PDMode7_World *world, PDMode7_Display *display)
{
    if(display->world)
    {
        return 0;
    }
    
    if(world->numberOfDisplays < MODE7_MAX_DISPLAYS)
    {
        display->world = world;
        
        world->displays[world->numberOfDisplays] = display;
        world->numberOfDisplays++;
        
        if(display->luaRef && !display->isManaged)
        {
            playdate->lua->retainObject(display->luaRef);
        }
        
        return 1;
    }
    
    return 0;
}

static int indexForDisplay(PDMode7_World *world, PDMode7_Display *display)
{
    for(int i = 0; i < world->numberOfDisplays; i++)
    {
        PDMode7_Display *localDisplay = world->displays[i];
        if(display == localDisplay)
        {
            return i;
        }
    }
    
    return -1;
}

static _PDMode7_Array* _getSprites(PDMode7_World *world)
{
    return world->sprites;
}

static PDMode7_Sprite** getSprites(PDMode7_World *world, int* len)
{
    _PDMode7_Array *sprites = _getSprites(world);

    *len = sprites->length;
    return (PDMode7_Sprite**)sprites->items;
}

static _PDMode7_Array* _getVisibleSpriteInstances(PDMode7_World *pWorld, PDMode7_Display *display)
{
    display = getDisplay(pWorld, display);
    return display->visibleInstances;
}

static PDMode7_SpriteInstance** getVisibleSpriteInstances(PDMode7_World *pWorld, int *length, PDMode7_Display *display)
{
    _PDMode7_Array *visibleSprites = _getVisibleSpriteInstances(pWorld, display);
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

static PDMode7_Display* getDisplay(PDMode7_World *world, PDMode7_Display *display)
{
    if(display)
    {
        return display;
    }
    return world->mainDisplay;
}

static PDMode7_Vec3 worldGetSize(PDMode7_World *world)
{
    return newVec3(world->width, world->height, world->depth);
}

static float worldGetScale(PDMode7_World *world)
{
    PDMode7_Bitmap *bitmap = world->plane.bitmap;
    if(bitmap && bitmap->width != 0)
    {
        return (float)world->width / bitmap->width;
    }
    return 1;
}

static float worldGetScaleInv(PDMode7_World *world)
{
    PDMode7_Bitmap *bitmap = world->plane.bitmap;
    if(bitmap && world->width != 0)
    {
        return (float)bitmap->width / world->width;
    }
    return 1;
}

static PDMode7_Display* worldGetMainDisplay(PDMode7_World *world)
{
    return world->mainDisplay;
}

static PDMode7_Camera* worldGetMainCamera(PDMode7_World *world)
{
    return world->mainCamera;
}

static PDMode7_Color planeColorAt_public(PDMode7_World *world, PDMode7_Plane *plane, int x, int y)
{
    float scaleInv = worldGetScaleInv(world);
    
    x = floorf(x * scaleInv);
    y = floorf(y * scaleInv);
    
    uint8_t color = planeColorAt(world, plane, x, y);
    return newGrayscaleColor(color, 255);
}

static PDMode7_Color worldPlaneColorAt(PDMode7_World *world, int x, int y)
{
    return planeColorAt_public(world, &world->plane, x, y);
}

static PDMode7_Color worldCeilingColorAt(PDMode7_World *world, int x, int y)
{
    return planeColorAt_public(world, &world->ceiling, x, y);
}

static void setPlaneTilemap_generic(PDMode7_Plane *plane, PDMode7_Tilemap *tilemap)
{
    if(tilemap && tilemap->luaRef)
    {
        GC_retain(tilemap->luaRef);
    }
    
    PDMode7_Tilemap *currentTilemap = plane->tilemap;
    if(currentTilemap && currentTilemap->luaRef)
    {
        GC_release(currentTilemap->luaRef);
    }
    
    plane->tilemap = tilemap;
}

static void setPlaneBitmap_generic(PDMode7_Plane *plane, PDMode7_Bitmap *bitmap)
{
    if(bitmap && bitmap->luaRef)
    {
        GC_retain(bitmap->luaRef);
    }
    
    PDMode7_Bitmap *currentBitmap = plane->bitmap;
    if(currentBitmap && currentBitmap->luaRef)
    {
        GC_release(currentBitmap->luaRef);
    }
    
    plane->bitmap = bitmap;
}

static PDMode7_Bitmap* getPlaneBitmap(PDMode7_World *world)
{
    return world->plane.bitmap;
}

static void setPlaneBitmap(PDMode7_World *world, PDMode7_Bitmap *bitmap)
{
    setPlaneBitmap_generic(&world->plane, bitmap);
}

static void setPlaneFillColor(PDMode7_World *world, PDMode7_Color color)
{
    world->plane.fillColor = color;
}

static PDMode7_Color getPlaneFillColor(PDMode7_World *world)
{
    return world->plane.fillColor;
}

static PDMode7_Bitmap* getCeilingBitmap(PDMode7_World *world)
{
    return world->ceiling.bitmap;
}

static void setCeilingBitmap(PDMode7_World *world, PDMode7_Bitmap *bitmap)
{
    setPlaneBitmap_generic(&world->ceiling, bitmap);
}

static void setCeilingFillColor(PDMode7_World *world, PDMode7_Color color)
{
    world->ceiling.fillColor = color;
}

static PDMode7_Color getCeilingFillColor(PDMode7_World *world)
{
    return world->ceiling.fillColor;
}

static void setPlaneTilemap(PDMode7_World *world, PDMode7_Tilemap *tilemap)
{
    setPlaneTilemap_generic(&world->plane, tilemap);
}

static PDMode7_Tilemap* getPlaneTilemap(PDMode7_World *world)
{
    return world->plane.tilemap;
}

static void setCeilingTilemap(PDMode7_World *world, PDMode7_Tilemap *tilemap)
{
    setPlaneTilemap_generic(&world->ceiling, tilemap);
}

static PDMode7_Tilemap* getCeilingTilemap(PDMode7_World *world)
{
    return world->ceiling.tilemap;
}

static void releasePlane(PDMode7_Plane *plane)
{
    if(plane->bitmap)
    {
        releaseBitmap(plane->bitmap);
    }
    if(plane->tilemap && plane->tilemap->luaRef)
    {
        GC_release(plane->tilemap->luaRef);
    }
}

static void freeWorld(PDMode7_World *world)
{
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
    
    releasePlane(&world->plane);
    releasePlane(&world->ceiling);

    if(world->mainCamera)
    {
        PDMode7_Camera *mainCamera = world->mainCamera;
        // Remove the "managed" flag so we can free it
        mainCamera->isManaged = 0;
        freeCamera(mainCamera);
    }
    
    PDMode7_Display *mainDisplay = world->mainDisplay;
    // Remove the "managed" flag so we can free it
    mainDisplay->isManaged = 0;
    if(mainDisplay->luaRef)
    {
        releaseDisplay(mainDisplay);
    }
    else
    {
        freeDisplay(mainDisplay);
    }

    playdate->system->realloc(world, 0);
}

static PDMode7_Display* newDisplay(int x, int y, int width, int height)
{
    PDMode7_Display *display = playdate->system->realloc(NULL, sizeof(PDMode7_Display));
    
    display->world = NULL;
    display->camera = NULL;
    display->rect = newRect(0, 0, 0, 0);
    display->absoluteRect = newRect(0, 0, 0, 0);
    display->scale = kMode7DisplayScale2x2;
    display->ditherType = kMode7DitherBayer4x4;
    display->orientation = kMode7DisplayOrientationLandscapeLeft;
    display->flipMode = kMode7DisplayFlipModeNone;
    display->secondaryFramebuffer = NULL;
    
    display->visibleInstances = newArray();
    display->planeShader = NULL;
    display->ceilingShader = NULL;

    displaySetRect(display, x, y, width, height);
    displaySetOrientation(display, kMode7DisplayOrientationLandscapeLeft);
    
    PDMode7_Background *background = playdate->system->realloc(NULL, sizeof(PDMode7_Background));
    display->background = background;
    
    background->display = display;
    background->bitmap = NULL;
    background->luaBitmap = NULL;
    background->width = 0;
    background->height = 0;
    background->center = newVec2(0.5, 0.5);
    background->roundingIncrement = newVec2ui(1, 1);
    
    display->isManaged = 0;
    display->luaRef = NULL;
    
    return display;
}

static PDMode7_DisplayScale displayGetScale(PDMode7_Display *display)
{
    return display->scale;
}

static void displaySetScale(PDMode7_Display *display, PDMode7_DisplayScale scale)
{
    display->scale = scale;
}

static PDMode7_DitherType displayGetDitherType(PDMode7_Display *display)
{
    return display->ditherType;
}

static void displaySetDitherType(PDMode7_Display *display, PDMode7_DitherType type)
{
    if(type >= 0 && type < 3)
    {
        display->ditherType = type;
    }
}

static PDMode7_Camera* displayGetCamera(PDMode7_Display *display)
{
    return display->camera;
}

static void displaySetCamera(PDMode7_Display *display, PDMode7_Camera *camera)
{
    if(camera->luaRef)
    {
        GC_retain(camera->luaRef);
    }
    
    PDMode7_Camera *currentCamera = display->camera;
    if(currentCamera && currentCamera->luaRef)
    {
        GC_release(currentCamera->luaRef);
    }
    
    display->camera = camera;
}

static PDMode7_Background* displayGetBackground(PDMode7_Display *display)
{
    return display->background;
}

static PDMode7_Shader* displayGetPlaneShader(PDMode7_Display *display)
{
    return display->planeShader;
}

static void displaySetPlaneShader(PDMode7_Display *display, PDMode7_Shader *shader)
{
    if(shader && shader->luaRef)
    {
        GC_retain(shader->luaRef);
    }
    if(display->planeShader)
    {
        releaseShader(display->planeShader);
    }
    display->planeShader = shader;
}

static PDMode7_Shader* displayGetCeilingShader(PDMode7_Display *display)
{
    return display->ceilingShader;
}

static void displaySetCeilingShader(PDMode7_Display *display, PDMode7_Shader *shader)
{
    if(shader && shader->luaRef)
    {
        GC_retain(shader->luaRef);
    }
    if(display->ceilingShader)
    {
        releaseShader(display->ceilingShader);
    }
    display->ceilingShader = shader;
}

static int displayNeedsClip(PDMode7_Display *display)
{
    return (display->rect.width < LCD_COLUMNS || display->rect.height < LCD_ROWS);
}

static void displayRectDidChange(PDMode7_Display *display)
{
    if(display->secondaryFramebuffer)
    {
        playdate->graphics->freeBitmap(display->secondaryFramebuffer);
        display->secondaryFramebuffer = NULL;
    }
    
    display->rect = display->absoluteRect;
    
    if(display->orientation != kMode7DisplayOrientationLandscapeLeft || display->flipMode != kMode7DisplayFlipModeNone)
    {
        display->secondaryFramebuffer = playdate->graphics->newBitmap(display->absoluteRect.width, display->absoluteRect.height, kColorWhite);
        display->rect = newRect(0, 0, display->absoluteRect.width, display->absoluteRect.height);
    }
}

static PDMode7_Rect displayGetRect(PDMode7_Display *display)
{
    return display->rect;
}

static void displaySetRect(PDMode7_Display *display, int x, int y, int width, int height)
{
    // x and width should be multiple of 8
    
    display->absoluteRect.x = x / 8 * 8;
    display->absoluteRect.y = y;
    display->absoluteRect.width = width / 8 * 8;
    display->absoluteRect.height = height;
    
    displayRectDidChange(display);
}

static PDMode7_DisplayOrientation displayGetOrientation(PDMode7_Display *display)
{
    return display->orientation;
}

static void displaySetOrientation(PDMode7_Display *display, PDMode7_DisplayOrientation orientation)
{
    display->orientation = orientation;
    displayRectDidChange(display);
}

static PDMode7_DisplayFlipMode displayGetFlipMode(PDMode7_Display *display)
{
    return display->flipMode;
}

static void displaySetFlipMode(PDMode7_Display *display, PDMode7_DisplayFlipMode flipMode)
{
    display->flipMode = flipMode;
    displayRectDidChange(display);
}

static int displayGetHorizon(PDMode7_Display *display)
{
    _PDMode7_Parameters parameters = worldGetParameters(display);
    return parameters.horizon;
}

static float displayPitchForHorizon(PDMode7_Display *display, float horizon)
{
    _PDMode7_Parameters parameters = worldGetParameters(display);
    float halfHeight = (float)display->rect.height / 2;
    float pitch = atanf((horizon / halfHeight - 1) * parameters.tanHalfFov.y);
    return pitch;
}

static PDMode7_World* displayGetWorld(PDMode7_Display *display)
{
    return display->world;
}

static PDMode7_Vec2 displayConvertPointFromOrientation(PDMode7_Display *display, float x, float y)
{
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

static PDMode7_Vec2 displayConvertPointToOrientation(PDMode7_Display *display, float x, float y)
{
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
    if(!display->secondaryFramebuffer)
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
            *target = display->secondaryFramebuffer;
        }
        playdate->graphics->getBitmapData(display->secondaryFramebuffer, NULL, NULL, rowbytes, NULL, framebuffer);
    }
}

static void removeDisplay(PDMode7_Display *display)
{
    if(display->world)
    {
        // Display is linked to world
        PDMode7_World *world = display->world;
        
        int index = indexForDisplay(world, display);
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
                releaseDisplay(display);
            }
        }
    }
}

static LCDBitmap* backgroundGetBitmap(PDMode7_Background *background)
{
    return background->bitmap;
}

static _PDMode7_LuaBitmap* backgroundGetLuaBitmap(PDMode7_Background *background)
{
    return background->luaBitmap;
}

static void backgroundSetBitmap(PDMode7_Background *background, LCDBitmap *bitmap, _PDMode7_LuaBitmap *luaBitmap)
{
    _PDMode7_LuaBitmap *currentLuaBitmap = background->luaBitmap;
    if(luaBitmap)
    {
        GC_retain(luaBitmap->luaRef);
    }
    if(currentLuaBitmap)
    {
        GC_release(currentLuaBitmap->luaRef);
    }
    
    background->bitmap = bitmap;
    background->luaBitmap = luaBitmap;
    background->width = 0;
    background->height = 0;
    
    if(bitmap)
    {
        int width; int height;
        playdate->graphics->getBitmapData(bitmap, &width, &height, NULL, NULL, NULL);
        
        background->width = width;
        background->height = height;
    }
}

static void backgroundSetBitmap_public(PDMode7_Background *background, LCDBitmap *bitmap)
{
    backgroundSetBitmap(background, bitmap, NULL);
}

static PDMode7_Vec2 backgroundGetCenter(PDMode7_Background *background)
{
    return background->center;
}

static void backgroundSetCenter(PDMode7_Background *background, float x, float y)
{
    background->center.x = x;
    background->center.y = y;
}

static void backgroundGetRoundingIncrement(PDMode7_Background *background, unsigned int *x, unsigned int *y)
{
    *x = background->roundingIncrement.x;
    *y = background->roundingIncrement.y;
}

static void backgroundSetRoundingIncrement(PDMode7_Background *background, unsigned int x, unsigned int y)
{
    background->roundingIncrement.x = x;
    background->roundingIncrement.y = y;
}

static PDMode7_Vec2 backgroundGetOffset(PDMode7_Background *background, _PDMode7_Parameters *parameters)
{
    PDMode7_Display *display = background->display;
    PDMode7_Camera *camera = display->camera;

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
    float offsetY = parameters->horizon - background->height * background->center.y;
    
    offsetX = roundToIncrement(offsetX, background->roundingIncrement.x);
    offsetY = roundToIncrement(offsetY, background->roundingIncrement.y);
    
    return newVec2(offsetX, offsetY);
}

static PDMode7_Vec2 backgroundGetOffset_public(PDMode7_Background *background)
{
    PDMode7_Display *display = background->display;
    _PDMode7_Parameters parameters = worldGetParameters(display);
    return backgroundGetOffset(background, &parameters);
}

static void releaseDisplay(PDMode7_Display *display)
{
    LuaUDObject *luaRef = display->luaRef;
    display->luaRef = NULL;
    playdate->lua->releaseObject(luaRef);
}

static void freeDisplay(PDMode7_Display *display)
{
    if(!display->isManaged)
    {
        removeDisplay(display);
        
        if(display->secondaryFramebuffer)
        {
            playdate->graphics->freeBitmap(display->secondaryFramebuffer);
        }
        
        if(display->camera && display->camera->luaRef)
        {
            GC_release(display->camera->luaRef);
        }
        
        PDMode7_Background *background = display->background;
        if(background->luaBitmap && background->luaBitmap->luaRef)
        {
            GC_release(background->luaBitmap->luaRef);
        }
        
        if(display->planeShader)
        {
            releaseShader(display->planeShader);
        }
        
        if(display->ceilingShader)
        {
            releaseShader(display->ceilingShader);
        }
        
        playdate->system->realloc(background, 0);
        
        freeArray(display->visibleInstances);
        
        playdate->system->realloc(display, 0);
    }
}

static PDMode7_Camera* newCamera(void)
{
    PDMode7_Camera *camera = playdate->system->realloc(NULL, sizeof(PDMode7_Camera));
    
    camera->fov = degToRad(90);
    camera->pitch = 0;
    
    camera->angle = 0;
    camera->position = newVec3(0, 0, 0);
    
    camera->clipDistanceUnits = 1;
    camera->isManaged = 0;
    camera->luaRef = NULL;
    
    return camera;
}

static PDMode7_Vec3 cameraGetPosition(PDMode7_Camera *camera)
{
    return camera->position;
}

static void cameraSetPosition(PDMode7_Camera *camera, float x, float y, float z)
{
    camera->position.x = x;
    camera->position.y = y;
    camera->position.z = fmaxf(0, z);
}

static float cameraGetAngle(PDMode7_Camera *camera)
{
    return camera->angle;
}

static void cameraSetAngle(PDMode7_Camera *camera, float angle)
{
    camera->angle = angle;
}

static float cameraGetFOV(PDMode7_Camera *camera)
{
    return camera->fov;
}

static void cameraSetFOV(PDMode7_Camera *camera, float fov)
{
    camera->fov = fov;
}

static float cameraGetPitch(PDMode7_Camera *camera)
{
    return camera->pitch;
}

static void cameraSetPitch(PDMode7_Camera *camera, float pitch)
{
    camera->pitch = pitch;
}

static int cameraGetClipDistanceUnits(PDMode7_Camera *camera)
{
    return camera->clipDistanceUnits;
}

static void cameraSetClipDistanceUnits(PDMode7_Camera *camera, int units)
{
    camera->clipDistanceUnits = units;
}

static void cameraLookAtPoint(PDMode7_Camera *camera, PDMode7_Vec3 point)
{
    PDMode7_Vec3 directionVec = vec3_subtract(point, camera->position);
    
    float roll = atan2f(directionVec.y, directionVec.x);
    float pitch = atan2f(directionVec.z, sqrtf(directionVec.x * directionVec.x + directionVec.y * directionVec.y));
    
    cameraSetAngle(camera, roll);
    cameraSetPitch(camera, pitch);
}

static void freeCamera(PDMode7_Camera *camera)
{
    if(!camera->isManaged)
    {
        playdate->system->realloc(camera, 0);
    }
}

static PDMode7_Sprite* newSprite(float width, float height, float depth)
{
    PDMode7_Sprite *sprite = playdate->system->realloc(NULL, sizeof(PDMode7_Sprite));
    
    sprite->world = NULL;
    sprite->size = newVec3(width, height, depth);
    sprite->position = newVec3(0, 0, 0);
    sprite->angle = 0;
    sprite->pitch = 0;
    
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = playdate->system->realloc(NULL, sizeof(PDMode7_SpriteInstance));
        
        instance->index = i;
        instance->sprite = sprite;
        
        instance->displayRect = newRect(0, 0, 0, 0);
        instance->visible = 1;
        instance->visibilityMode = kMode7SpriteVisibilityModeDefault;
        instance->roundingIncrement = newVec2ui(1, 1);
        instance->imageCenter = newVec2(0.5, 0.5);
        instance->alignmentX = kMode7SpriteAlignmentNone;
        instance->alignmentY = kMode7SpriteAlignmentNone;
        instance->distance = 0;
        instance->frame = 0;
        instance->billboardSizeBehavior = kMode7BillboardSizeAutomatic;
        instance->billboardSize = newVec2(0, 0);
        instance->bitmapTable = NULL;
        instance->luaBitmapTable = NULL;
        instance->bitmap = NULL;
        instance->drawCallback = NULL;
        
        PDMode7_SpriteDataSource *dataSource = playdate->system->realloc(NULL, sizeof(PDMode7_SpriteDataSource));
        instance->dataSource = dataSource;
        
        dataSource->instance = instance;

        dataSource->minimumWidth = 0;
        dataSource->maximumWidth = 0;
        
        dataSource->lengths[kMode7SpriteDataSourceFrame] = 1;
        dataSource->lengths[kMode7SpriteDataSourceAngle] = 1;
        dataSource->lengths[kMode7SpriteDataSourcePitch] = 1;
        dataSource->lengths[kMode7SpriteDataSourceScale] = 1;

        dataSource->layoutKeys[0] = kMode7SpriteDataSourceFrame;
        dataSource->layoutKeys[1] = kMode7SpriteDataSourceAngle;
        dataSource->layoutKeys[2] = kMode7SpriteDataSourcePitch;
        dataSource->layoutKeys[3] = kMode7SpriteDataSourceScale;

        sprite->instances[i] = instance;
    }
    
    sprite->gridCells = newArray();
    sprite->luaRef = NULL;
    
    _PDMode7_LuaSpriteDataSource *luaDataSource = playdate->system->realloc(NULL, sizeof(_PDMode7_LuaSpriteDataSource));
    luaDataSource->sprite = sprite;
    sprite->luaDataSource = luaDataSource;
        
    return sprite;
}

static void spriteSetSize(PDMode7_Sprite *sprite, float width, float height, float depth)
{
    sprite->size.x = width;
    sprite->size.y = height;
    sprite->size.z = depth;
    
    spriteBoundsDidChange(sprite);
}

static PDMode7_Vec3 spriteGetSize(PDMode7_Sprite *sprite)
{
    return sprite->size;
}

static PDMode7_Vec3 spriteGetPosition(PDMode7_Sprite *sprite)
{
    return sprite->position;
}

static void spriteSetPosition(PDMode7_Sprite *sprite, float x, float y, float z)
{
    sprite->position.x = x;
    sprite->position.y = y;
    sprite->position.z = fmaxf(0, z);
    
    spriteBoundsDidChange(sprite);
}

static void spriteBoundsDidChange(PDMode7_Sprite *sprite)
{
    PDMode7_World *world = sprite->world;
    if(world)
    {
        gridUpdateSprite(world->grid, sprite);
    }
}

static float spriteGetAngle(PDMode7_Sprite *sprite)
{
    return sprite->angle;
}

static void spriteSetAngle(PDMode7_Sprite *sprite, float angle)
{
    sprite->angle = angle;
    spriteBoundsDidChange(sprite);
}

static float spriteGetPitch(PDMode7_Sprite *sprite)
{
    return sprite->pitch;
}

static void spriteSetPitch(PDMode7_Sprite *sprite, float pitch)
{
    sprite->pitch = pitch;
}

static void _spriteSetVisible(PDMode7_SpriteInstance *instance, int flag)
{
    instance->visible = flag;
}

static void spriteSetVisible(PDMode7_Sprite *sprite, int flag)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetVisible(sprite->instances[i], flag);
    }
}

static int _spriteGetVisible(PDMode7_SpriteInstance *instance)
{
    return instance->visible;
}

static void _spriteSetVisibilityMode(PDMode7_SpriteInstance *instance, PDMode7_SpriteVisibilityMode mode)
{
    instance->visibilityMode = mode;
}

static void spriteSetVisibilityMode(PDMode7_Sprite *sprite, PDMode7_SpriteVisibilityMode mode)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetVisibilityMode(sprite->instances[i], mode);
    }
}

static PDMode7_SpriteVisibilityMode _spriteGetVisibilityMode(PDMode7_SpriteInstance *instance)
{
    return instance->visibilityMode;
}

static void _spriteSetImageCenter(PDMode7_SpriteInstance *instance, float cx, float cy)
{
    instance->imageCenter.x = cx;
    instance->imageCenter.y = cy;
}

static void spriteSetImageCenter(PDMode7_Sprite *sprite, float cx, float cy)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetImageCenter(sprite->instances[i], cx, cy);
    }
}

static PDMode7_Vec2 _spriteGetImageCenter(PDMode7_SpriteInstance *instance)
{
    return instance->imageCenter;
}

static void _spriteSetFrame(PDMode7_SpriteInstance *instance, unsigned int frame)
{
    instance->frame = frame;
}

static void spriteSetFrame(PDMode7_Sprite *sprite, unsigned int frame)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetFrame(sprite->instances[i], frame);
    }
}

static unsigned int _spriteGetFrame(PDMode7_SpriteInstance *instance)
{
    return instance->frame;
}

static void _spriteSetBillboardSizeBehavior(PDMode7_SpriteInstance *instance, PDMode7_SpriteBillboardSizeBehavior behavior)
{
    instance->billboardSizeBehavior = behavior;
}

static void spriteSetBillboardSizeBehavior(PDMode7_Sprite *sprite, PDMode7_SpriteBillboardSizeBehavior behavior)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetBillboardSizeBehavior(sprite->instances[i], behavior);
    }
}

static PDMode7_SpriteBillboardSizeBehavior _spriteGetBillboardSizeBehavior(PDMode7_SpriteInstance *instance)
{
    return instance->billboardSizeBehavior;
}

static void _spriteSetBillboardSize(PDMode7_SpriteInstance *instance, float width, float height)
{
    instance->billboardSize.x = width;
    instance->billboardSize.y = height;
}

static void spriteSetBillboardSize(PDMode7_Sprite *sprite, float width, float height)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetBillboardSize(sprite->instances[i], width, height);
    }
}

static PDMode7_Vec2 _spriteGetBillboardSize(PDMode7_SpriteInstance *instance)
{
    return instance->billboardSize;
}

static void _spriteSetRoundingIncrement(PDMode7_SpriteInstance *instance, unsigned int x, unsigned int y)
{
    instance->roundingIncrement.x = x;
    instance->roundingIncrement.y = y;
}

static void spriteSetRoundingIncrement(PDMode7_Sprite *sprite, unsigned int x, unsigned int y)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetRoundingIncrement(sprite->instances[i], x, y);
    }
}

static void _spriteGetRoundingIncrement(PDMode7_SpriteInstance *instance, unsigned int *x, unsigned int *y)
{
    *x = instance->roundingIncrement.x;
    *y = instance->roundingIncrement.y;
}

static void _spriteSetAlignment(PDMode7_SpriteInstance *instance, PDMode7_SpriteAlignment alignmentX, PDMode7_SpriteAlignment alignmentY)
{
    instance->alignmentX = alignmentX;
    instance->alignmentY = alignmentY;
}

static void spriteSetAlignment(PDMode7_Sprite *sprite, PDMode7_SpriteAlignment alignmentX, PDMode7_SpriteAlignment alignmentY)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetAlignment(sprite->instances[i], alignmentX, alignmentY);
    }
}

static void _spriteGetAlignment(PDMode7_SpriteInstance *instance, PDMode7_SpriteAlignment *alignmentX, PDMode7_SpriteAlignment *alignmentY)
{
    *alignmentX = instance->alignmentX;
    *alignmentY = instance->alignmentY;
}

static void _spriteSetDrawCallback(PDMode7_SpriteInstance *instance, _PDMode7_Callback *callback)
{
    if(instance->drawCallback)
    {
        freeCallback(instance->drawCallback);
    }
    instance->drawCallback = callback;
}

static void _spriteSetDrawFunction_c(PDMode7_SpriteInstance *instance, PDMode7_SpriteDrawCallbackFunction *function)
{
    _spriteSetDrawCallback(instance, newCallback_c(function));
}

static void spriteSetDrawFunction_c(PDMode7_Sprite *sprite, PDMode7_SpriteDrawCallbackFunction *function)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetDrawCallback(sprite->instances[i], newCallback_c(function));
    }
}

static void _spriteSetBitmapTable(PDMode7_SpriteInstance *instance, LCDBitmapTable *bitmapTable, _PDMode7_LuaBitmapTable *luaBitmapTable)
{
    if(luaBitmapTable)
    {
        GC_retain(luaBitmapTable->luaRef);
    }
    if(instance->luaBitmapTable)
    {
        GC_release(instance->luaBitmapTable->luaRef);
    }
    instance->bitmapTable = bitmapTable;
    instance->luaBitmapTable = luaBitmapTable;
}

static void _spriteSetBitmapTable_public(PDMode7_SpriteInstance *instance, LCDBitmapTable *bitmapTable)
{
    _spriteSetBitmapTable(instance, bitmapTable, NULL);
}

static void spriteSetBitmapTable(PDMode7_Sprite *sprite, LCDBitmapTable *bitmapTable, _PDMode7_LuaBitmapTable *luaBitmapTable)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetBitmapTable(sprite->instances[i], bitmapTable, luaBitmapTable);
    }
}

static void spriteSetBitmapTable_public(PDMode7_Sprite *sprite, LCDBitmapTable *bitmapTable)
{
    spriteSetBitmapTable(sprite, bitmapTable, NULL);
}

static LCDBitmapTable* _spriteGetBitmapTable(PDMode7_SpriteInstance *instance)
{
    return instance->bitmapTable;
}

static _PDMode7_LuaBitmapTable* _spriteGetLuaBitmapTable(PDMode7_SpriteInstance *instance)
{
    return instance->luaBitmapTable;
}

static PDMode7_SpriteInstance* spriteGetInstance(PDMode7_Sprite *sprite, PDMode7_Display *display)
{
    PDMode7_World *targetWorld = sprite->world;

    if(!targetWorld)
    {
        // Retrieve world from display
        targetWorld = display->world;
    }
    
    if(targetWorld)
    {
        display = getDisplay(targetWorld, display);
        
        int displayIndex = indexForDisplay(targetWorld, display);
        if(displayIndex >= 0)
        {
            return sprite->instances[displayIndex];
        }
    }
    
    return NULL;
}

static PDMode7_SpriteInstance** spriteGetInstances(PDMode7_Sprite *sprite, int *length)
{
    *length = MODE7_MAX_DISPLAYS;
    return sprite->instances;
}

static void _spriteSetUserData(PDMode7_SpriteInstance *instance, void *userdata)
{
    instance->userdata = userdata;
}

static void spriteSetUserData(PDMode7_Sprite *sprite, void *userdata)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        _spriteSetUserData(sprite->instances[i], userdata);
    }
}

static void* _spriteGetUserData(PDMode7_SpriteInstance *instance)
{
    return instance->userdata;
}

static LCDBitmap* _spriteGetBitmap(PDMode7_SpriteInstance *instance)
{
    return instance->bitmap;
}

static PDMode7_Rect _spriteGetDisplayRect(PDMode7_SpriteInstance *instance)
{
    return instance->displayRect;
}

static int _spriteIsVisibleOnDisplay(PDMode7_SpriteInstance *instance)
{
    PDMode7_Sprite *sprite = instance->sprite;

    if(sprite->world)
    {
        PDMode7_World *world = sprite->world;
        PDMode7_Display *display = world->displays[instance->index];
        int index = arrayIndexOf(display->visibleInstances, instance);
        if(index >= 0)
        {
            return 1;
        }
    }
    return 0;
}

static PDMode7_World* spriteGetWorld(PDMode7_Sprite *sprite)
{
    return sprite->world;
}

static void removeSprite(PDMode7_Sprite *sprite)
{
    PDMode7_World *world = sprite->world;
    
    if(world)
    {
        gridRemoveSprite(world->grid, sprite);
        
        int index = arrayIndexOf(world->sprites, sprite);
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
    removeSprite(sprite);
    
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = sprite->instances[i];
        
        if(instance->luaBitmapTable)
        {
            GC_release(instance->luaBitmapTable->luaRef);
        }
        
        if(instance->drawCallback)
        {
            freeCallback(instance->drawCallback);
        }
        
        playdate->system->realloc(instance->dataSource, 0);

        playdate->system->realloc(instance, 0);
    }
    
    freeArray(sprite->gridCells);
    
    playdate->system->realloc(sprite->luaDataSource, 0);
    
    playdate->system->realloc(sprite, 0);
}

static unsigned int _spriteDataSourceGetLength(PDMode7_SpriteDataSource *dataSource, PDMode7_SpriteDataSourceKey key)
{
    if(key >= 0 && key < MODE7_SPRITE_DSOURCE_LEN)
    {
        return dataSource->lengths[key];
    }
    return 1;
}

static void _spriteDataSourceSetLength(PDMode7_SpriteDataSource *dataSource, unsigned int length, PDMode7_SpriteDataSourceKey key)
{
    if(length > 0)
    {
        dataSource->lengths[key] = length;
    }
}

static void spriteDataSourceSetLength(PDMode7_Sprite *sprite, unsigned int length, PDMode7_SpriteDataSourceKey key)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = sprite->instances[i];
        _spriteDataSourceSetLength(instance->dataSource, length, key);
    }
}

static unsigned int spriteGetTableIndex(PDMode7_SpriteInstance *instance, unsigned int angleIndex, unsigned int pitchIndex, unsigned int scaleIndex)
{
    int tableIndex = 0;
    
    for(int i = 0; i < MODE7_SPRITE_DSOURCE_LEN; i++)
    {
        PDMode7_SpriteDataSource *dataSource = instance->dataSource;
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
            kIndex *= _spriteDataSourceGetLength(dataSource, k2);
        }
        
        tableIndex += kIndex;
    }
    
    return tableIndex;
}

static void _spriteDataSourceGetLayout(PDMode7_SpriteDataSource *dataSource, PDMode7_SpriteDataSourceKey *k1, PDMode7_SpriteDataSourceKey *k2, PDMode7_SpriteDataSourceKey *k3, PDMode7_SpriteDataSourceKey *k4)
{
    *k1 = dataSource->layoutKeys[0];
    *k2 = dataSource->layoutKeys[1];
    *k3 = dataSource->layoutKeys[2];
    *k4 = dataSource->layoutKeys[3];
}

static void _spriteDataSourceSetLayout(PDMode7_SpriteDataSource *dataSource, PDMode7_SpriteDataSourceKey k1, PDMode7_SpriteDataSourceKey k2, PDMode7_SpriteDataSourceKey k3, PDMode7_SpriteDataSourceKey k4)
{
    if(k1 != k2 && k1 != k3 && k1 != k4 && k2 != k3 && k2 != k4 && k3 != k4)
    {
        dataSource->layoutKeys[0] = k1;
        dataSource->layoutKeys[1] = k2;
        dataSource->layoutKeys[2] = k3;
        dataSource->layoutKeys[3] = k4;
    }
}

static void spriteDataSourceSetLayout(PDMode7_Sprite *sprite, PDMode7_SpriteDataSourceKey k1, PDMode7_SpriteDataSourceKey k2, PDMode7_SpriteDataSourceKey k3,  PDMode7_SpriteDataSourceKey k4)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = sprite->instances[i];
        _spriteDataSourceSetLayout(instance->dataSource, k1, k2, k3, k4);
    }
}

static int _spriteDataSourceGetMinimumWidth(PDMode7_SpriteDataSource *dataSource)
{
    return dataSource->minimumWidth;
}

static void _spriteDataSourceSetMinimumWidth(PDMode7_SpriteDataSource *dataSource, int minimumWidth)
{
    dataSource->minimumWidth = minimumWidth;
}

static void spriteDataSourceSetMinimumWidth(PDMode7_Sprite *sprite, int minimumWidth)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = sprite->instances[i];
        _spriteDataSourceSetMinimumWidth(instance->dataSource, minimumWidth);
    }
}

static int _spriteDataSourceGetMaximumWidth(PDMode7_SpriteDataSource *dataSource)
{
    return dataSource->maximumWidth;
}

static void _spriteDataSourceSetMaximumWidth(PDMode7_SpriteDataSource *dataSource, int maximumWidth)
{
    dataSource->maximumWidth = maximumWidth;
}

static void spriteDataSourceSetMaximumWidth(PDMode7_Sprite *sprite, int maximumWidth)
{
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = sprite->instances[i];
        _spriteDataSourceSetMaximumWidth(instance->dataSource, maximumWidth);
    }
}

static PDMode7_SpriteDataSource* _spriteGetDataSource(PDMode7_SpriteInstance *instance)
{
    return instance->dataSource;
}

static PDMode7_Sprite* spriteInstanceGetSprite(PDMode7_SpriteInstance *instance)
{
    return instance->sprite;
}

static PDMode7_Shader newShader(PDMode7_ShaderType type, void *object)
{
    return (PDMode7_Shader){
        .objectType = type,
        .object = object,
        .luaRef = NULL
    };
}

static void releaseShader(PDMode7_Shader *shader)
{
    if(shader->luaRef)
    {
        GC_release(shader->luaRef);
    }
}

static PDMode7_LinearShader* newLinearShader(void)
{
    PDMode7_LinearShader *linear = playdate->system->realloc(NULL, sizeof(PDMode7_LinearShader));
    linear->shader = newShader(PDMode7_ShaderTypeLinear, linear);
    
    linear->color = newGrayscaleColor(0, 255);
    linear->minDistance = 50;
    linear->maxDistance = 800;
    linear->inverted = 0;
    linear->alpha = 0;
    linear->delta_inv = 0;
    
    return linear;
}

static inline float linearShaderProgress(PDMode7_LinearShader *linear, float distance)
{
    return fmaxf(0, fminf((distance - linear->minDistance) * linear->delta_inv, 1));
}

static float linearShaderGetMinimumDistance(PDMode7_LinearShader *linear)
{
    return linear->minDistance;
}

static void linearShaderSetMinimumDistance(PDMode7_LinearShader *linear, float d)
{
    linear->minDistance = d;
}

static float linearShaderGetMaximumDistance(PDMode7_LinearShader *linear)
{
    return linear->maxDistance;
}

static void linearShaderSetMaximumDistance(PDMode7_LinearShader *linear, float d)
{
    linear->maxDistance = d;
}

static PDMode7_Color linearShaderGetColor(PDMode7_LinearShader *linear)
{
    return linear->color;
}

static void linearShaderSetColor(PDMode7_LinearShader *linear, PDMode7_Color color)
{
    linear->color = color;
}

static int linearShaderGetInverted(PDMode7_LinearShader *linear)
{
    return linear->inverted;
}

static void linearShaderSetInverted(PDMode7_LinearShader *linear, int inverted)
{
    linear->inverted = inverted;
}

static void freeLinearShader(PDMode7_LinearShader *linear)
{
    playdate->system->realloc(linear, 0);
}

static PDMode7_RadialShader* newRadialShader(void)
{
    PDMode7_RadialShader *radial = playdate->system->realloc(NULL, sizeof(PDMode7_RadialShader));
    radial->shader = newShader(PDMode7_ShaderTypeRadial, radial);
    
    radial->color = newGrayscaleColor(0, 255);
    radial->minDistance = 10;
    radial->maxDistance = 100;
    radial->inverted = 0;
    radial->offset = newVec2(0, 0);
    radial->origin = newVec2(0, 0);
    radial->minSquared = 0;
    radial->delta_inv = 0;
    
    return radial;
}

static inline float radialShaderProgress(PDMode7_RadialShader *radial, float x, float y)
{
    float dx = x - radial->origin.x;
    float dy = y - radial->origin.y;
    float squared_d = dx * dx + dy * dy;
    return fmaxf(0, fminf((squared_d - radial->minSquared) * radial->delta_inv, 1));
}

static float radialShaderGetMinimumDistance(PDMode7_RadialShader *radial)
{
    return radial->minDistance;
}

static void radialShaderSetMinimumDistance(PDMode7_RadialShader *radial, float d)
{
    radial->minDistance = d;
}

static float radialShaderGetMaximumDistance(PDMode7_RadialShader *radial)
{
    return radial->maxDistance;
}

static void radialShaderSetMaximumDistance(PDMode7_RadialShader *radial, float d)
{
    radial->maxDistance = d;
}

static PDMode7_Color radialShaderGetColor(PDMode7_RadialShader *radial)
{
    return radial->color;
}

static void radialShaderSetColor(PDMode7_RadialShader *radial, PDMode7_Color color)
{
    radial->color = color;
}

static PDMode7_Vec2 radialShaderGetOffset(PDMode7_RadialShader *radial)
{
    return radial->offset;
}

static void radialShaderSetOffset(PDMode7_RadialShader *radial, float dx, float dy)
{
    radial->offset = newVec2(dx, dy);
}

static int radialShaderGetInverted(PDMode7_RadialShader *radial)
{
    return radial->inverted;
}

static void radialShaderSetInverted(PDMode7_RadialShader *radial, int inverted)
{
    radial->inverted = inverted;
}

static void freeRadialShader(PDMode7_RadialShader *radial)
{
    playdate->system->realloc(radial, 0);
}

static int tileScaleIsValid(int scale)
{
    return (scale == 1 || (scale % 2) == 0);
}

static PDMode7_Tilemap* newTilemap(PDMode7_World *world, int tileWidth, int tileHeight)
{
    int valid = (tileWidth > 0 && tileHeight > 0 && (tileWidth % 2) == 0 && (tileHeight % 2) == 0);
    if(!valid)
    {
        return NULL;
    }
    
    int rows = (world->width + tileWidth - 1) / tileWidth;
    int columns = (world->height + tileHeight - 1) / tileHeight;
    
    PDMode7_Tilemap *tilemap = playdate->system->realloc(NULL, sizeof(PDMode7_Tilemap));
    
    tilemap->rows = rows;
    tilemap->columns = columns;
    tilemap->tileWidth = tileWidth;
    tilemap->tileHeight = tileHeight;
    tilemap->tileWidth_log = log2_int(tileWidth);
    tilemap->tileHeight_log = log2_int(tileHeight);
    
    int len = rows * columns;
    tilemap->tiles = playdate->system->realloc(NULL, len * sizeof(PDMode7_Tile));
    
    for(int i = 0; i < len; i++)
    {
        PDMode7_Tile *tile = &tilemap->tiles[i];
        tile->bitmap = NULL;
        tile->scale_log = 0;
    }
    
    tilemap->fillBitmap = NULL;
    tilemap->fillBitmapScale_log = 0;
    tilemap->luaRef = NULL;
    
    return tilemap;
}

static void tilemapSetBitmapAtIndex(PDMode7_Tilemap *tilemap, PDMode7_Bitmap *bitmap, int index)
{
    int scale = bitmap ? tilemap->tileWidth / bitmap->width : 1;
    int valid = tileScaleIsValid(scale);
    if(!valid)
    {
        return;
    }
    
    if(index >= 0 && index < (tilemap->rows * tilemap->columns))
    {
        PDMode7_Tile *tile = &tilemap->tiles[index];
                
        if(bitmap && bitmap->luaRef)
        {
            GC_retain(bitmap->luaRef);
        }
        
        PDMode7_Bitmap *currentBitmap = tile->bitmap;
        if(currentBitmap && currentBitmap->luaRef)
        {
            GC_release(currentBitmap->luaRef);
        }
        
        tile->bitmap = bitmap;
        tile->scale_log = log2_int(scale);
    }
}

static void tilemapSetBitmapAtPosition(PDMode7_Tilemap *tilemap, PDMode7_Bitmap *bitmap, int row, int column)
{
    tilemapSetBitmapAtIndex(tilemap, bitmap, row * tilemap->columns + column);
}

static PDMode7_Bitmap* tilemapGetBitmapAtPosition(PDMode7_Tilemap *tilemap, int row, int column)
{
    int index = row * tilemap->columns + column;
    if(index >= 0 && index < (tilemap->rows * tilemap->columns))
    {
        PDMode7_Tile *tile = &tilemap->tiles[index];
        return tile->bitmap;
    }
    return NULL;
}

static void tilemapSetBitmapAtRange(PDMode7_Tilemap *tilemap, PDMode7_Bitmap *bitmap, int startRow, int startColumn, int endRow, int endColumn)
{
    if(endRow < 0)
    {
        endRow = tilemap->rows - 1;
    }
    if(endColumn < 0)
    {
        endColumn = tilemap->columns - 1;
    }
    
    int startIndex = startRow * tilemap->columns + startColumn;
    int endIndex = endRow * tilemap->columns + endColumn;
    
    for(int i = startIndex; i <= endIndex; i++)
    {
        tilemapSetBitmapAtIndex(tilemap, bitmap, i);
    }
}

static void tilemapSetBitmapAtIndexes(PDMode7_Tilemap *tilemap, PDMode7_Bitmap *bitmap, int *indexes, int len)
{
    for(int i = 0; i < len; i++)
    {
        tilemapSetBitmapAtIndex(tilemap, bitmap, indexes[i]);
    }
}

static void tilemapSetFillBitmap(PDMode7_Tilemap *tilemap, PDMode7_Bitmap *bitmap)
{
    int scale = bitmap ? tilemap->tileWidth / bitmap->width : 1;
    int valid = tileScaleIsValid(scale);
    if(!valid)
    {
        return;
    }
    
    if(bitmap && bitmap->luaRef)
    {
        GC_retain(bitmap->luaRef);
    }
    
    PDMode7_Bitmap *currentBitmap = tilemap->fillBitmap;
    if(currentBitmap && currentBitmap->luaRef)
    {
        GC_release(currentBitmap->luaRef);
    }
    
    tilemap->fillBitmap = bitmap;
    tilemap->fillBitmapScale_log = log2_int(scale);
}

static PDMode7_Bitmap* tilemapGetFillBitmap(PDMode7_Tilemap *tilemap)
{
    return tilemap->fillBitmap;
}

static void freeTilemap(PDMode7_Tilemap *tilemap)
{
    int len = tilemap->rows * tilemap->columns;
    for(int i = 0; i < len; i++)
    {
        PDMode7_Tile *tile = &tilemap->tiles[i];
        if(tile->bitmap)
        {
            releaseBitmap(tile->bitmap);
        }
    }
    
    if(tilemap->fillBitmap)
    {
        releaseBitmap(tilemap->fillBitmap);
    }
    
    playdate->system->realloc(tilemap->tiles, 0);
    playdate->system->realloc(tilemap, 0);
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

static inline PDMode7_Vec2 vec2FromVec3(PDMode7_Vec3 vec)
{
    return (PDMode7_Vec2){
        .x = vec.x,
        .y = vec.y
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

static void gridUpdateSprite(_PDMode7_Grid *grid, PDMode7_Sprite *sprite)
{
    gridRemoveSprite(grid, sprite);
    
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
                                
                arrayPush(cell->sprites, sprite);
                arrayPush(sprite->gridCells, cell);
            }
        }
    }
}

static void gridRemoveSprite(_PDMode7_Grid *grid, PDMode7_Sprite *sprite)
{
    for(int i = 0; i < sprite->gridCells->length; i++)
    {
        _PDMode7_GridCell *cell = sprite->gridCells->items[i];
        int index = arrayIndexOf(cell->sprites, sprite);
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

static int log2_int(uint32_t n)
{
    int r = 0;
    while(n > 1)
    {
        n /= 2;
        r++;
    }
    return r;
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
    // Skip spaces
    while(isspace((int)buffer->data[buffer->position]))
    {
        buffer->position++;
    }
    
    while(buffer->data[buffer->position] == '#')
    {
        buffer->position++;
        while(buffer->data[buffer->position] != '\n' && buffer->data[buffer->position] != EOF)
        {
            buffer->position++;
        }
        // Skip spaces
        while(isspace((int)buffer->data[buffer->position]))
        {
            buffer->position++;
        }
    }
}

static PDMode7_Bitmap* newBaseBitmap(void)
{
    PDMode7_Bitmap *bitmap = playdate->system->realloc(NULL, sizeof(PDMode7_Bitmap));
    
    bitmap->width = 0;
    bitmap->height = 0;
    bitmap->data = NULL;
    bitmap->mask = NULL;
    bitmap->layers = newArray();
    bitmap->isManaged = 0;
    bitmap->freeData = 0;
    bitmap->luaRef = NULL;

    return bitmap;
}

static PDMode7_Bitmap* newBitmap(int width, int height, PDMode7_Color bgColor)
{
    PDMode7_Bitmap *bitmap = newBaseBitmap();
    
    bitmap->width = width;
    bitmap->height = height;
    
    size_t dataLen = width * height;
    
    bitmap->data = playdate->system->realloc(NULL, dataLen);
    memset(bitmap->data, bgColor.gray, dataLen);
    bitmap->freeData = 1;
    
    if(bgColor.alpha < 255)
    {
        PDMode7_Bitmap *mask = newBaseBitmap();
        
        mask->width = width;
        mask->height = height;
        
        mask->data = playdate->system->realloc(NULL, dataLen);
        memset(mask->data, 0x00, dataLen);
        
        mask->freeData = 1;
        mask->isManaged = 1;
        
        bitmap->mask = mask;
    }
    
    return bitmap;
}

static PDMode7_Bitmap* copyBitmap(PDMode7_Bitmap *src)
{
    PDMode7_Bitmap *dst = newBaseBitmap();
    
    dst->width = src->width;
    dst->height = src->height;
    
    size_t dataLen = src->width * src->height;
    
    dst->data = playdate->system->realloc(NULL, dataLen);
    memcpy(dst->data, src->data, dataLen);
    dst->freeData = 1;
    
    if(src->mask)
    {
        PDMode7_Bitmap *mask = newBaseBitmap();
        
        mask->width = src->mask->width;
        mask->height = src->mask->height;
        
        size_t maskLen = src->mask->width * src->mask->height;
        
        mask->data = playdate->system->realloc(NULL, maskLen);
        memcpy(mask->data, src->mask->data, maskLen);
        
        mask->freeData = 1;
        mask->isManaged = 1;
        
        dst->mask = mask;
    }
    
    return dst;
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
    
    PDMode7_Bitmap *bitmap = newBaseBitmap();
    
    bitmap->width = width;
    bitmap->height = height;
    bitmap->data = (uint8_t*)buffer->data + buffer->position;
    
    playdate->system->realloc(buffer, 0);
    
    return bitmap;
}

static void bitmapGetSize(PDMode7_Bitmap *bitmap, int *width, int *height)
{
    *width = bitmap->width;
    *height = bitmap->height;
}

static PDMode7_Color bitmapColorAt(PDMode7_Bitmap *bitmap, int x, int y)
{
    if(x >= 0 && x < bitmap->width && y >= 0 && y < bitmap->height)
    {
        int i = y * bitmap->width + x;
        uint8_t gray = bitmap->data[i];
        uint8_t alpha = 255;
        
        if(bitmap->mask)
        {
            alpha = bitmap->mask->data[i];
        }
        
        return newGrayscaleColor(gray, alpha);
    }
    
    return newGrayscaleColor(0, 0);
}

static PDMode7_Bitmap* bitmapGetMask(PDMode7_Bitmap *bitmap)
{
    return bitmap->mask;
}

static void bitmapSetMask(PDMode7_Bitmap *bitmap, PDMode7_Bitmap *mask)
{
    if(mask)
    {
        // The new mask can't be managed
        if(mask->isManaged)
        {
            return;
        }
        // Mask size must be equal to bitmap size
        if((bitmap->width != mask->width) || (bitmap->height != mask->height))
        {
            return;
        }
    }
    
    if(mask && mask->luaRef)
    {
        GC_retain(mask->luaRef);
    }
    
    if(bitmap->mask)
    {
        if(bitmap->mask->isManaged)
        {
            freeBitmap(bitmap->mask);
        }
        else
        {
            releaseBitmap(bitmap->mask);
        }
    }
    
    bitmap->mask = mask;
}

static void bitmapDrawInto(PDMode7_Bitmap *bitmap, PDMode7_Bitmap *target, int bitmapX, int bitmapY)
{
    if((bitmapX + bitmap->width) <= 0 || bitmapX >= target->width || (bitmapY + bitmap->height) <= 0 || bitmapY >= target->height)
    {
        return;
    }
    
    PDMode7_Rect rect = newRect(bitmapX, bitmapY, bitmap->width, bitmap->height);
    
    PDMode7_Rect adjustedRect; int offsetX; int offsetY;
    rectAdjust(rect, target->width, target->height, &adjustedRect, &offsetX, &offsetY);
    
    for(int y = 0; y < adjustedRect.height; y++)
    {
        int src_y = offsetY + y;
        int dst_y = adjustedRect.y + y;
        int src_rows = src_y * bitmap->width + offsetX;
        int dst_rows = dst_y * target->width + adjustedRect.x;
        
        if(bitmap->mask || target->mask)
        {
            for(int x = 0; x < adjustedRect.width; x++)
            {
                int src_offset = src_rows + x;
                int dst_offset = dst_rows + x;
                
                uint8_t color = bitmap->data[src_offset];
                uint8_t backgroundColor = target->data[dst_offset];
                
                uint8_t alpha = bitmap->mask ? bitmap->mask->data[src_offset] : 255;
                uint8_t backgroundAlpha = target->mask ? target->mask->data[dst_offset] : 255;
                
                uint8_t alphaOut = alpha + (unsigned int)(backgroundAlpha * (255 - alpha) + 127) / 255;
                uint8_t colorOut = (alphaOut != 0) ? (color * alpha + backgroundColor * backgroundAlpha * (uint8_t)(255 - alpha) / 255 + alphaOut / 2) / alphaOut : 0;

                target->data[dst_offset] = colorOut;
                if(target->mask)
                {
                    target->mask->data[dst_offset] = alphaOut;
                }
            }
        }
        else
        {
            memcpy(target->data + dst_rows, bitmap->data + src_rows, adjustedRect.width);
        }
    }
}

static void bitmapAddLayer(PDMode7_Bitmap *bitmap, PDMode7_BitmapLayer *layer)
{
    if(!layer->parentBitmap)
    {
        int index = arrayIndexOf(bitmap->layers, layer);
        if(index < 0)
        {
            if(layer->luaRef)
            {
                playdate->lua->retainObject(layer->luaRef);
            }
            
            layer->parentBitmap = bitmap;
            arrayPush(bitmap->layers, layer);
            
            bitmapLayerDidChange(layer);
            bitmapLayerDraw(layer);
        }
    }
}

static _PDMode7_Array* _bitmapGetLayers(PDMode7_Bitmap *bitmap)
{
    return bitmap->layers;
}

static PDMode7_BitmapLayer** bitmapGetLayers(PDMode7_Bitmap *bitmap, int *len)
{
    _PDMode7_Array *layers = _bitmapGetLayers(bitmap);
    *len = layers->length;
    return (PDMode7_BitmapLayer**)layers->items;
}

static void bitmapRemoveAllLayers(PDMode7_Bitmap *bitmap)
{
    for(int i = bitmap->layers->length - 1; i >= 0; i--)
    {
        PDMode7_BitmapLayer *layer = bitmap->layers->items[i];
        removeBitmapLayer(layer);
    }
}

static void releaseBitmap(PDMode7_Bitmap *bitmap)
{
    if(bitmap->luaRef)
    {
        GC_release(bitmap->luaRef);
    }
}

static void freeBitmap(PDMode7_Bitmap *bitmap)
{
    for(int i = bitmap->layers->length - 1; i >= 0; i--)
    {
        PDMode7_BitmapLayer *layer = bitmap->layers->items[i];
        _removeBitmapLayer(layer, 0);
    }
    
    freeArray(bitmap->layers);
    
    if(bitmap->mask)
    {
        if(bitmap->mask->isManaged)
        {
            freeBitmap(bitmap->mask);
        }
        else
        {
            releaseBitmap(bitmap->mask);
        }
    }
    
    if(bitmap->freeData)
    {
        playdate->system->realloc(bitmap->data, 0);
    }
    
    playdate->system->realloc(bitmap, 0);
}

static PDMode7_BitmapLayer* newBitmapLayer(PDMode7_Bitmap *bitmap)
{
    PDMode7_BitmapLayer *layer = playdate->system->realloc(NULL, sizeof(PDMode7_BitmapLayer));
    
    layer->bitmap = NULL;
    layer->parentBitmap = NULL;
    layer->rect = newRect(0, 0, 0, 0);
    layer->canRestore = 0;
    layer->enabled = 0;
    layer->visible = 1;
    
    layer->comp = (_PDMode7_BitmapLayerCompositing){
        .data = NULL,
        .size = 0,
        .rect = newRect(0, 0, 0, 0),
        .offsetX = 0,
        .offsetY = 0,
    };
    
    layer->luaRef = NULL;
    
    bitmapLayerSetBitmap(layer, bitmap);

    return layer;
}

static void bitmapLayerRestore(PDMode7_BitmapLayer *layer)
{
    if(layer->parentBitmap && layer->canRestore)
    {
        PDMode7_Bitmap *parentBitmap = layer->parentBitmap;
        
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

static void bitmapLayerDraw(PDMode7_BitmapLayer *layer)
{
    if(layer->parentBitmap && layer->bitmap && layer->enabled)
    {
        PDMode7_Bitmap *parentBitmap = layer->parentBitmap;
        PDMode7_Bitmap *layerBitmap = layer->bitmap;
        
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
        
        for(int y = 0; y < layer->comp.rect.height; y++)
        {
            int src_y = layer->comp.offsetY + y;
            int dst_y = layer->comp.rect.y + y;
            int src_rows = src_y * layer->rect.width + layer->comp.offsetX;
            int dst_rows = dst_y * parentBitmap->width + layer->comp.rect.x;
            
            if(layerBitmap->mask)
            {
                for(int x = 0; x < layer->comp.rect.width; x++)
                {
                    int src_offset = src_rows + x;
                    int dst_offset = dst_rows + x;
                    
                    uint8_t color = layerBitmap->data[src_offset];
                    uint8_t backgroundColor = parentBitmap->data[dst_offset];
                    
                    uint8_t alpha = layerBitmap->mask->data[src_offset];
                    color = backgroundColor + (unsigned int)(alpha * (color - backgroundColor) + 127) / 255;
                    
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
    return layer->bitmap;
}

static void bitmapLayerSetBitmap(PDMode7_BitmapLayer *layer, PDMode7_Bitmap *bitmap)
{
    bitmapLayerRestore(layer);
    
    if(bitmap->luaRef)
    {
        GC_retain(bitmap->luaRef);
    }
    
    if(layer->bitmap && layer->bitmap->luaRef)
    {
        GC_release(layer->bitmap->luaRef);
    }
    
    layer->bitmap = bitmap;
    
    layer->rect.width = bitmap->width;
    layer->rect.height = bitmap->height;
    
    size_t data_size = bitmap->width * bitmap->height;
    if(data_size != layer->comp.size)
    {
        layer->comp.size = data_size;
        layer->comp.data = playdate->system->realloc(layer->comp.data, data_size);
    }
    
    layer->canRestore = 0;
    layer->enabled = 0;
    
    bitmapLayerDidChange(layer);
    bitmapLayerDraw(layer);
}

static void bitmapLayerGetPosition(PDMode7_BitmapLayer *layer, int *x, int *y)
{
    *x = layer->rect.x;
    *y = layer->rect.y;
}

static void bitmapLayerSetPosition(PDMode7_BitmapLayer *layer, int x, int y)
{
    bitmapLayerRestore(layer);
    
    layer->rect.x = x;
    layer->rect.y = y;
    
    bitmapLayerDidChange(layer);
    bitmapLayerDraw(layer);
}

static void bitmapLayerSetVisible(PDMode7_BitmapLayer *layer, int visible)
{
    bitmapLayerRestore(layer);
    
    layer->visible = visible;
    
    bitmapLayerDidChange(layer);
    bitmapLayerDraw(layer);
}

static int bitmapLayerIsVisible(PDMode7_BitmapLayer *layer)
{
    return layer->visible;
}

static void bitmapLayerInvalidate(PDMode7_BitmapLayer *layer)
{
    bitmapLayerRestore(layer);
    bitmapLayerDidChange(layer);
    bitmapLayerDraw(layer);
}

static int bitmapLayerIntersect(PDMode7_BitmapLayer *layer)
{
    if(layer->parentBitmap)
    {
        PDMode7_Bitmap *parentBitmap = layer->parentBitmap;
        
        for(int i = 0; i < parentBitmap->layers->length; i++)
        {
            PDMode7_BitmapLayer *localLayer = parentBitmap->layers->items[i];
            if(localLayer != layer)
            {
                if(rectIntersect(localLayer->rect, layer->rect) && localLayer->visible)
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
    if(layer->parentBitmap)
    {
        PDMode7_Bitmap *parentBitmap = layer->parentBitmap;
        
        int intersect = 0;
        
        for(int i = 0; i < parentBitmap->layers->length; i++)
        {
            PDMode7_BitmapLayer *localLayer = parentBitmap->layers->items[i];
            
            if(localLayer != layer)
            {
                if(rectIntersect(localLayer->rect, layer->rect) && localLayer->visible && layer->visible)
                {
                    // Local layer intersects with the layer, we disable both
                    // Layer is disabled out of the loop
                    intersect = 1;
                    
                    bitmapLayerRestore(localLayer);
                    localLayer->enabled = 0;
                    localLayer->canRestore = 0;
                }
                else if(!localLayer->enabled && localLayer->visible)
                {
                    // Check if a disabled layer can be enabled
                    if(!bitmapLayerIntersect(localLayer))
                    {
                        // A disabled layer is now enabled
                        localLayer->enabled = 1;
                        bitmapLayerDraw(localLayer);
                    }
                }
            }
        }
        
        if(intersect || !layer->visible)
        {
            layer->enabled = 0;
            layer->canRestore = 0;
        }
        else
        {
            layer->enabled = 1;
        }
    }
}

static void _removeBitmapLayer(PDMode7_BitmapLayer *layer, int restore)
{
    if(layer->parentBitmap)
    {
        PDMode7_Bitmap *parentBitmap = layer->parentBitmap;
        int index = arrayIndexOf(parentBitmap->layers, layer);
        if(index >= 0)
        {
            if(restore)
            {
                bitmapLayerRestore(layer);
            }
            
            // Reset state
            layer->enabled = 0;
            layer->canRestore = 0;
            
            arrayRemove(parentBitmap->layers, index);
            layer->parentBitmap = NULL;
            
            if(layer->luaRef)
            {
                LuaUDObject *luaRef = layer->luaRef;
                layer->luaRef = NULL;
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
    removeBitmapLayer(layer);
    
    if(layer->bitmap && layer->bitmap->luaRef)
    {
        GC_release(layer->bitmap->luaRef);
    }
    
    if(layer->comp.data)
    {
        playdate->system->realloc(layer->comp.data, 0);
    }
    
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
static char *lua_kShader = "mode7.shader";
static char *lua_kShaderLinear = "mode7.shader.linear";
static char *lua_kShaderRadial = "mode7.shader.radial";
static char *lua_kTilemap = "mode7.tilemap";

static PDMode7_Color lua_getColor(lua_State *L, int *i)
{
    int gray = playdate->lua->getArgInt(*i);
    int alpha = playdate->lua->getArgInt(*i + 1);
    *i = *i + 2;
    if(gray >= 0 && gray <= 255 && alpha >= 0 && alpha <= 255)
    {
        return newGrayscaleColor(gray, alpha);
    }
    return newGrayscaleColor(0, 0);
}

static void lua_pushColor(lua_State *L, PDMode7_Color color, int *len)
{
    playdate->lua->pushInt(color.gray);
    playdate->lua->pushInt(color.alpha);
    *len = 2;
}

static PDMode7_Shader* lua_getShader(lua_State *L, int i)
{
    const char *objectClass;
    enum LuaType shaderType = playdate->lua->getArgType(i, &objectClass);
    if(shaderType == kTypeObject && objectClass)
    {
        if(strcmp(objectClass, lua_kShaderLinear) == 0 || strcmp(objectClass, lua_kShaderRadial) == 0)
        {
            PDMode7_Shader *shader = playdate->lua->getArgObject(i, (char*)objectClass, NULL);
            return shader;
        }
    }
    return NULL;
}

static void lua_pushShader(lua_State *L, PDMode7_Shader *shader)
{
    if(shader->objectType == PDMode7_ShaderTypeLinear)
    {
        playdate->lua->pushObject(shader->object, lua_kShaderLinear, 0);
    }
    else if(shader->objectType == PDMode7_ShaderTypeRadial)
    {
        playdate->lua->pushObject(shader->object, lua_kShaderLinear, 0);
    }
    else
    {
        playdate->lua->pushNil();
    }
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
    
    PDMode7_Display *mainDisplay = world->mainDisplay;
    
    LuaUDObject *displayRef = playdate->lua->pushObject(mainDisplay, lua_kDisplay, 0);
    mainDisplay->luaRef = displayRef;
    playdate->lua->retainObject(displayRef);
    
    PDMode7_Camera *mainCamera = world->mainCamera;
    
    LuaUDObject *cameraRef = playdate->lua->pushObject(mainCamera, lua_kCamera, 0);
    mainCamera->luaRef = cameraRef;
    GC_retain(cameraRef);
    // In Lua, mainCamera is not managed by world
    mainCamera->isManaged = 0;
    world->mainCamera = NULL;
    
    playdate->lua->pushObject(world, lua_kWorld, 0);
    
    return 1;
}

static int lua_newTilemap(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    int tileWidth = playdate->lua->getArgInt(2);
    int tileHeight = playdate->lua->getArgInt(3);
    PDMode7_Tilemap *tilemap = newTilemap(world, tileWidth, tileHeight);
    tilemap->luaRef = playdate->lua->pushObject(tilemap, lua_kTilemap, 0);
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
    int clen;
    lua_pushColor(L, getPlaneFillColor(world), &clen);
    return clen;
}

static int lua_setPlaneFillColor(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    int ci = 2;
    setPlaneFillColor(world, lua_getColor(L, &ci));
    return 0;
}

static int lua_getCeilingFillColor(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    int clen;
    lua_pushColor(L, getCeilingFillColor(world), &clen);
    return clen;
}

static int lua_setCeilingFillColor(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    int ci = 2;
    setCeilingFillColor(world, lua_getColor(L, &ci));
    return 0;
}

static int lua_planeColorAt(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    int x = playdate->lua->getArgInt(2);
    int y = playdate->lua->getArgInt(3);
    int clen;
    lua_pushColor(L, planeColorAt_public(world, &world->plane, x, y), &clen);
    return clen;
}

static int lua_ceilingColorAt(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    int x = playdate->lua->getArgInt(2);
    int y = playdate->lua->getArgInt(3);
    int clen;
    lua_pushColor(L, planeColorAt_public(world, &world->ceiling, x, y), &clen);
    return clen;
}

static int lua_setPlaneTilemap(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Tilemap *tilemap = playdate->lua->getArgObject(2, lua_kTilemap, NULL);
    setPlaneTilemap_generic(&world->plane, tilemap);
    return 0;
}

static int lua_getPlaneTilemap(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    playdate->lua->pushObject(world->plane.tilemap, lua_kTilemap, 0);
    return 1;
}

static int lua_setCeilingTilemap(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    PDMode7_Tilemap *tilemap = playdate->lua->getArgObject(2, lua_kTilemap, NULL);
    setPlaneTilemap_generic(&world->ceiling, tilemap);
    return 0;
}

static int lua_getCeilingTilemap(lua_State *L)
{
    PDMode7_World *world = playdate->lua->getArgObject(1, lua_kWorld, NULL);
    playdate->lua->pushObject(world->ceiling.tilemap, lua_kTilemap, 0);
    return 1;
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
    { "newTilemap", lua_newTilemap },
    { "setPlaneTilemap", lua_setPlaneTilemap },
    { "getPlaneTilemap", lua_getPlaneTilemap },
    { "setCeilingTilemap", lua_setCeilingTilemap },
    { "getCeilingTilemap", lua_getCeilingTilemap },
    { "_planeColorAt", lua_planeColorAt },
    { "_ceilingColorAt", lua_ceilingColorAt },
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
    display->luaRef = playdate->lua->pushObject(display, lua_kDisplay, 0);
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

static int lua_displayGetDitherType(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_DitherType type = displayGetDitherType(display);
    playdate->lua->pushInt(type);
    return 1;
}

static int lua_displaySetDitherType(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_DitherType type = playdate->lua->getArgInt(2);
    displaySetDitherType(display, type);
    return 0;
}

static int lua_displaySetPlaneShader(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_Shader *shader = lua_getShader(L, 2);
    displaySetPlaneShader(display, shader);
    return 0;
}

static int lua_displayGetPlaneShader(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_Shader *shader = displayGetPlaneShader(display);
    lua_pushShader(L, shader);
    return 1;
}

static int lua_displaySetCeilingShader(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_Shader *shader = lua_getShader(L, 2);
    displaySetCeilingShader(display, shader);
    return 0;
}

static int lua_displayGetCeilingShader(lua_State *L)
{
    PDMode7_Display *display = playdate->lua->getArgObject(1, lua_kDisplay, NULL);
    PDMode7_Shader *shader = displayGetCeilingShader(display);
    lua_pushShader(L, shader);
    return 1;
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
    { "getDitherType", lua_displayGetDitherType },
    { "setDitherType", lua_displaySetDitherType },
    { "getPlaneShader", lua_displayGetPlaneShader },
    { "setPlaneShader", lua_displaySetPlaneShader },
    { "getCeilingShader", lua_displayGetCeilingShader },
    { "setCeilingShader", lua_displaySetCeilingShader },
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

static const lua_reg lua_shader[] = {
    { "new", lua_displaySetFlipMode },
    { NULL, NULL }
};

static int lua_newLinearShader(lua_State *L)
{
    PDMode7_LinearShader *linear = newLinearShader();
    PDMode7_Shader *shader = &linear->shader;
    shader->luaRef = playdate->lua->pushObject(linear, lua_kShaderLinear, 0);
    return 1;
}

static int lua_linearShaderGetMinimumDistance(lua_State *L)
{
    PDMode7_LinearShader *linear = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    playdate->lua->pushFloat(linearShaderGetMinimumDistance(linear));
    return 1;
}

static int lua_linearShaderSetMinimumDistance(lua_State *L)
{
    PDMode7_LinearShader *linear = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    float d = playdate->lua->getArgFloat(2);
    linearShaderSetMinimumDistance(linear, d);
    return 0;
}

static int lua_linearShaderGetMaximumDistance(lua_State *L)
{
    PDMode7_LinearShader *linear = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    playdate->lua->pushFloat(linearShaderGetMaximumDistance(linear));
    return 1;
}

static int lua_linearShaderSetMaximumDistance(lua_State *L)
{
    PDMode7_LinearShader *linear = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    float d = playdate->lua->getArgFloat(2);
    linearShaderSetMaximumDistance(linear, d);
    return 0;
}

static int lua_linearShaderSetColor(lua_State *L)
{
    PDMode7_LinearShader *linear = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    int ci = 2;
    linearShaderSetColor(linear, lua_getColor(L, &ci));
    return 0;
}

static int lua_linearShaderGetColor(lua_State *L)
{
    PDMode7_LinearShader *linear = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    int ci;
    lua_pushColor(L, linearShaderGetColor(linear), &ci);
    return ci;
}

static int lua_linearShaderGetInverted(lua_State *L)
{
    PDMode7_LinearShader *linear = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    playdate->lua->pushBool(linearShaderGetInverted(linear));
    return 1;
}

static int lua_linearShaderSetInverted(lua_State *L)
{
    PDMode7_LinearShader *linear = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    linearShaderSetInverted(linear, playdate->lua->getArgBool(2));
    return 0;
}

static int lua_freeLinearShader(lua_State *L)
{
    PDMode7_LinearShader *linear = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    freeLinearShader(linear);
    return 0;
}

static const lua_reg lua_linearShader[] = {
    { "new", lua_newLinearShader },
    { "getMinimumDistance", lua_linearShaderGetMinimumDistance },
    { "setMinimumDistance", lua_linearShaderSetMinimumDistance },
    { "getMaximumDistance", lua_linearShaderGetMaximumDistance },
    { "setMaximumDistance", lua_linearShaderSetMaximumDistance },
    { "_getColor", lua_linearShaderGetColor },
    { "_setColor", lua_linearShaderSetColor },
    { "getInverted", lua_linearShaderGetInverted },
    { "setInverted", lua_linearShaderSetInverted },
    { "__gc", lua_freeLinearShader },
    { NULL, NULL }
};

static int lua_newRadialShader(lua_State *L)
{
    PDMode7_RadialShader *radial = newRadialShader();
    PDMode7_Shader *shader = &radial->shader;
    shader->luaRef = playdate->lua->pushObject(radial, lua_kShaderRadial, 0);
    return 1;
}

static int lua_radialShaderGetMinimumDistance(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    playdate->lua->pushFloat(radialShaderGetMinimumDistance(radial));
    return 1;
}

static int lua_radialShaderSetMinimumDistance(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    float d = playdate->lua->getArgFloat(2);
    radialShaderSetMinimumDistance(radial, d);
    return 0;
}

static int lua_radialShaderGetMaximumDistance(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    playdate->lua->pushFloat(radialShaderGetMaximumDistance(radial));
    return 1;
}

static int lua_radialShaderSetMaximumDistance(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    float d = playdate->lua->getArgFloat(2);
    radialShaderSetMaximumDistance(radial, d);
    return 0;
}

static int lua_radialShaderGetOffset(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    PDMode7_Vec2 offset = radialShaderGetOffset(radial);
    playdate->lua->pushFloat(offset.x);
    playdate->lua->pushFloat(offset.y);
    return 2;
}

static int lua_radialShaderSetOffset(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    float dx = playdate->lua->getArgFloat(2);
    float dy = playdate->lua->getArgFloat(3);
    radialShaderSetOffset(radial, dx, dy);
    return 0;
}

static int lua_radialShaderSetColor(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    int ci = 2;
    radialShaderSetColor(radial, lua_getColor(L, &ci));
    return 0;
}

static int lua_radialShaderGetColor(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    int ci;
    lua_pushColor(L, radialShaderGetColor(radial), &ci);
    return ci;
}

static int lua_radialShaderGetInverted(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    playdate->lua->pushBool(radialShaderGetInverted(radial));
    return 1;
}

static int lua_radialShaderSetInverted(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderRadial, NULL);
    radialShaderSetInverted(radial, playdate->lua->getArgBool(2));
    return 0;
}

static int lua_freeRadialShader(lua_State *L)
{
    PDMode7_RadialShader *radial = playdate->lua->getArgObject(1, lua_kShaderLinear, NULL);
    freeRadialShader(radial);
    return 0;
}

static const lua_reg lua_radialShader[] = {
    { "new", lua_newRadialShader },
    { "getMinimumDistance", lua_radialShaderGetMinimumDistance },
    { "setMinimumDistance", lua_radialShaderSetMinimumDistance },
    { "getMaximumDistance", lua_radialShaderGetMaximumDistance },
    { "setMaximumDistance", lua_radialShaderSetMaximumDistance },
    { "getOffset", lua_radialShaderGetOffset },
    { "setOffset", lua_radialShaderSetOffset },
    { "_getColor", lua_radialShaderGetColor },
    { "_setColor", lua_radialShaderSetColor },
    { "getInverted", lua_radialShaderGetInverted },
    { "setInverted", lua_radialShaderSetInverted },
    { "__gc", lua_freeRadialShader },
    { NULL, NULL }
};

static int lua_newCamera(lua_State *L)
{
    PDMode7_Camera *camera = newCamera();
    camera->luaRef = playdate->lua->pushObject(camera, lua_kCamera, 0);
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
    sprite->luaRef = playdate->lua->pushObject(sprite, lua_kSprite, 0);
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

static int lua_spriteSetVisibilityMode(lua_State *L)
{
    PDMode7_Sprite *sprite = playdate->lua->getArgObject(1, lua_kSprite, NULL);
    PDMode7_SpriteVisibilityMode mode = playdate->lua->getArgInt(2);
    spriteSetVisibilityMode(sprite, mode);
    return 0;
}

static int lua_spriteInstanceGetVisibilityMode(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_SpriteVisibilityMode mode = _spriteGetVisibilityMode(instance);
    playdate->lua->pushInt(mode);
    return 1;
}

static int lua_spriteInstanceSetVisibilityMode(lua_State *L)
{
    PDMode7_SpriteInstance *instance = playdate->lua->getArgObject(1, lua_kSpriteInstance, NULL);
    PDMode7_SpriteVisibilityMode mode = playdate->lua->getArgInt(2);
    _spriteSetVisibilityMode(instance, mode);
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
    _PDMode7_LuaSpriteDataSource *dataSource = sprite->luaDataSource;
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
    
    const char *functionName = playdate->lua->getArgString(2);
    
    for(int i = 0; i < MODE7_MAX_DISPLAYS; i++)
    {
        PDMode7_SpriteInstance *instance = sprite->instances[i];
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

static void lua_spriteCallDrawCallback(PDMode7_SpriteInstance *instance)
{
    if(instance->drawCallback && instance->drawCallback->type == _PDMode7_CallbackTypeLua)
    {
        playdate->lua->pushObject(instance, lua_kSpriteInstance, 0);
        
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
    { "setVisibilityMode", lua_spriteSetVisibilityMode },
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
    { "getVisibilityMode", lua_spriteInstanceGetVisibilityMode },
    { "setVisibilityMode", lua_spriteInstanceSetVisibilityMode },
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

static int lua_bitmapNew(lua_State *L)
{
    int width = playdate->lua->getArgInt(1);
    int height = playdate->lua->getArgInt(2);
    int ci = 3;
    PDMode7_Color bgColor = lua_getColor(L, &ci);
    PDMode7_Bitmap *bitmap = newBitmap(width, height, bgColor);
    LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
    bitmap->luaRef = luaRef;
    return 1;
}

static int lua_loadPGM(lua_State *L)
{
    const char *filename = playdate->lua->getArgString(1);
    PDMode7_Bitmap *bitmap = loadPGM(filename);
    LuaUDObject *luaRef = playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
    bitmap->luaRef = luaRef;
    return 1;
}

static int lua_copyBitmap(lua_State *L)
{
    PDMode7_Bitmap *src = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    PDMode7_Bitmap *dst = copyBitmap(src);

    LuaUDObject *luaRef = playdate->lua->pushObject(dst, lua_kBitmap, 0);
    dst->luaRef = luaRef;
    return 1;
}

static int lua_bitmapGetSize(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    playdate->lua->pushInt(bitmap->width);
    playdate->lua->pushInt(bitmap->height);
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

static int lua_bitmapDrawInto(lua_State *L)
{
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(1, lua_kBitmap, NULL);
    PDMode7_Bitmap *target = playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    int x = playdate->lua->getArgInt(3);
    int y = playdate->lua->getArgInt(4);
    bitmapDrawInto(bitmap, target, x, y);
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
    { "_new", lua_bitmapNew },
    { "loadPGM", lua_loadPGM },
    { "copy", lua_copyBitmap },
    { "getSize", lua_bitmapGetSize },
    { "_colorAt", lua_bitmapColorAt },
    { "getMask", lua_bitmapGetMask },
    { "setMask", lua_bitmapSetMask },
    { "drawInto", lua_bitmapDrawInto },
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
    layer->luaRef = playdate->lua->pushObject(layer, lua_kBitmapLayer, 0);
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

static int lua_tilemapSetBitmapAtPosition(lua_State *L)
{
    PDMode7_Tilemap *tilemap = playdate->lua->getArgObject(1, lua_kTilemap, NULL);
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    int row = playdate->lua->getArgInt(3);
    int column = playdate->lua->getArgInt(4);
    tilemapSetBitmapAtPosition(tilemap, bitmap, row, column);
    return 0;
}

static int lua_tilemapSetBitmapAtRange(lua_State *L)
{
    PDMode7_Tilemap *tilemap = playdate->lua->getArgObject(1, lua_kTilemap, NULL);
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    int row1 = playdate->lua->getArgInt(3);
    int column1 = playdate->lua->getArgInt(4);
    int row2 = playdate->lua->getArgInt(5);
    int column2 = playdate->lua->getArgInt(6);
    tilemapSetBitmapAtRange(tilemap, bitmap, row1, column1, row2, column2);
    return 0;
}

static int lua_tilemapGetBitmapAtPosition(lua_State *L)
{
    PDMode7_Tilemap *tilemap = playdate->lua->getArgObject(1, lua_kTilemap, NULL);
    int row = playdate->lua->getArgInt(2);
    int column = playdate->lua->getArgInt(3);
    PDMode7_Bitmap *bitmap = tilemapGetBitmapAtPosition(tilemap, row, column);
    playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
    return 1;
}

static int lua_tilemapSetFillBitmap(lua_State *L)
{
    PDMode7_Tilemap *tilemap = playdate->lua->getArgObject(1, lua_kTilemap, NULL);
    PDMode7_Bitmap *bitmap = playdate->lua->getArgObject(2, lua_kBitmap, NULL);
    tilemapSetFillBitmap(tilemap, bitmap);
    return 0;
}

static int lua_tilemapGetFillBitmap(lua_State *L)
{
    PDMode7_Tilemap *tilemap = playdate->lua->getArgObject(1, lua_kTilemap, NULL);
    PDMode7_Bitmap *bitmap = tilemapGetFillBitmap(tilemap);
    playdate->lua->pushObject(bitmap, lua_kBitmap, 0);
    return 1;
}

static int lua_freeTilemap(lua_State *L)
{
    PDMode7_Tilemap *tilemap = playdate->lua->getArgObject(1, lua_kTilemap, NULL);
    freeTilemap(tilemap);
    return 0;
}

static const lua_reg lua_tilemap[] = {
    { "setBitmapAtPosition", lua_tilemapSetBitmapAtPosition },
    { "setBitmapAtRange", lua_tilemapSetBitmapAtRange },
    { "getBitmapAtPosition", lua_tilemapGetBitmapAtPosition },
    { "setFillBitmap", lua_tilemapSetFillBitmap },
    { "getFillBitmap", lua_tilemapGetFillBitmap },
    { "__gc", lua_freeTilemap },
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
    mode7->world->newTilemap = newTilemap;  // LUACHECK
    mode7->world->getMainDisplay = worldGetMainDisplay; // LUACHECK
    mode7->world->getMainCamera = worldGetMainCamera;
    mode7->world->planeColorAt = worldPlaneColorAt;  // LUACHECK=planeColorAt
    mode7->world->ceilingColorAt = worldCeilingColorAt; // LUACHECK=ceilingColorAt
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
    mode7->world->setPlaneTilemap = setPlaneTilemap; // LUACHECK
    mode7->world->getPlaneTilemap = getPlaneTilemap; // LUACHECK
    mode7->world->setCeilingTilemap = setCeilingTilemap; // LUACHECK
    mode7->world->getCeilingTilemap = getCeilingTilemap; // LUACHECK
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
    mode7->display->getDitherType = displayGetDitherType; // LUACHECK
    mode7->display->setDitherType = displaySetDitherType; // LUACHECK
    mode7->display->getPlaneShader = displayGetPlaneShader; // LUACHECK
    mode7->display->setPlaneShader = displaySetPlaneShader; // LUACHECK
    mode7->display->getCeilingShader = displayGetCeilingShader; // LUACHECK
    mode7->display->setCeilingShader = displaySetCeilingShader; // LUACHECK
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
    mode7->sprite->setVisibilityMode = spriteSetVisibilityMode; // LUACHECK
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
    mode7->spriteInstance->setVisibilityMode = _spriteSetVisibilityMode; // LUACHECK=spriteInstanceSetVisibilityMode
    mode7->spriteInstance->getVisibilityMode = _spriteGetVisibilityMode; // LUACHECK=spriteInstanceGetVisibilityMode
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
    mode7->bitmap->newBitmap = newBitmap;
    mode7->bitmap->loadPGM = loadPGM; // LUACHECK
    mode7->bitmap->copyBitmap = copyBitmap; // LUACHECK
    mode7->bitmap->getSize = bitmapGetSize; // LUACHECK
    mode7->bitmap->colorAt = bitmapColorAt; // LUACHECK
    mode7->bitmap->getMask = bitmapGetMask; // LUACHECK
    mode7->bitmap->setMask = bitmapSetMask; // LUACHECK
    mode7->bitmap->drawInto = bitmapDrawInto; // LUACHECK
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
    
    mode7->shader = playdate->system->realloc(NULL, sizeof(PDMode7_Shader_API));
    
    mode7->shader->linear = playdate->system->realloc(NULL, sizeof(PDMode7_LinearShader_API));
    mode7->shader->linear->newLinear = newLinearShader; // LUACHECK
    mode7->shader->linear->getMinimumDistance = linearShaderGetMinimumDistance; // LUACHECK
    mode7->shader->linear->setMinimumDistance = linearShaderSetMinimumDistance; // LUACHECK
    mode7->shader->linear->getMaximumDistance = linearShaderGetMaximumDistance; // LUACHECK
    mode7->shader->linear->setMaximumDistance = linearShaderSetMaximumDistance; // LUACHECK
    mode7->shader->linear->getColor = linearShaderGetColor; // LUACHECK
    mode7->shader->linear->setColor = linearShaderSetColor; // LUACHECK
    mode7->shader->linear->getInverted = linearShaderGetInverted; // LUACHECK
    mode7->shader->linear->setInverted = linearShaderSetInverted; // LUACHECK
    mode7->shader->linear->freeLinear = freeLinearShader; // LUACHECK
    
    mode7->shader->radial = playdate->system->realloc(NULL, sizeof(PDMode7_RadialShader_API));
    mode7->shader->radial->newRadial = newRadialShader; // LUACHECK
    mode7->shader->radial->getMinimumDistance = radialShaderGetMinimumDistance; // LUACHECK
    mode7->shader->radial->setMinimumDistance = radialShaderSetMinimumDistance; // LUACHECK
    mode7->shader->radial->getMaximumDistance = radialShaderGetMaximumDistance; // LUACHECK
    mode7->shader->radial->setMaximumDistance = radialShaderSetMaximumDistance; // LUACHECK
    mode7->shader->radial->getOffset = radialShaderGetOffset; // LUACHECK
    mode7->shader->radial->setOffset = radialShaderSetOffset; // LUACHECK
    mode7->shader->radial->getColor = radialShaderGetColor; // LUACHECK
    mode7->shader->radial->setColor = radialShaderSetColor; // LUACHECK
    mode7->shader->radial->getInverted = radialShaderGetInverted; // LUACHECK
    mode7->shader->radial->setInverted = radialShaderSetInverted; // LUACHECK
    mode7->shader->radial->freeRadial = freeRadialShader; // LUACHECK
    
    mode7->tilemap = playdate->system->realloc(NULL, sizeof(PDMode7_Tilemap_API));
    mode7->tilemap->setBitmapAtPosition = tilemapSetBitmapAtPosition; // LUACHECK
    mode7->tilemap->setBitmapAtRange = tilemapSetBitmapAtRange; // LUACHECK
    mode7->tilemap->setBitmapAtIndexes = tilemapSetBitmapAtIndexes;
    mode7->tilemap->getBitmapAtPosition = tilemapGetBitmapAtPosition; // LUACHECK
    mode7->tilemap->setFillBitmap = tilemapSetFillBitmap; // LUACHECK
    mode7->tilemap->getFillBitmap = tilemapGetFillBitmap; // LUACHECK
    mode7->tilemap->freeTilemap = freeTilemap; // LUACHECK

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
        playdate->lua->registerClass(lua_kShader, lua_shader, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kShaderLinear, lua_linearShader, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kShaderRadial, lua_radialShader, NULL, 0, NULL);
        playdate->lua->registerClass(lua_kTilemap, lua_tilemap, NULL, 0, NULL);

        playdate->lua->addFunction(lua_poolRealloc, "mode7.pool.realloc", NULL);
        playdate->lua->addFunction(lua_poolClear, "mode7.pool.clear", NULL);
    }
}
