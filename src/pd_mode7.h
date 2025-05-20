//
//  pd_mode7.h
//  PDMode7
//
//  Created by Matteo D'Ignazio on 18/11/23.
//

#ifndef pd_mode7_h
#define pd_mode7_h

#include <stdio.h>
#include <pd_api.h>

#ifndef PD_MODE7_CEILING
#define PD_MODE7_CEILING 0
#endif

typedef struct PDMode7_Vec2 {
    float x, y;
} PDMode7_Vec2;

typedef struct PDMode7_Vec3 {
    float x, y, z;
} PDMode7_Vec3;

typedef struct PDMode7_Rect {
    int x, y, width, height;
} PDMode7_Rect;

typedef enum {
    kPDMode7ColorSpaceGrayscale
} PDMode7_ColorSpace;

typedef struct PDMode7_Color {
    PDMode7_ColorSpace space;
    uint8_t gray;
    uint8_t alpha;
} PDMode7_Color;

typedef enum {
    kMode7DisplayScale1x1,
    kMode7DisplayScale2x1,
    kMode7DisplayScale2x2,
    kMode7DisplayScale4x1,
    kMode7DisplayScale4x2
} PDMode7_DisplayScale;

typedef enum {
    kMode7DisplayOrientationLandscapeLeft,
    kMode7DisplayOrientationLandscapeRight,
    kMode7DisplayOrientationPortrait,
    kMode7DisplayOrientationPortraitUpsideDown
} PDMode7_DisplayOrientation;

typedef enum {
    kMode7DisplayFlipModeNone,
    kMode7DisplayFlipModeX,
    kMode7DisplayFlipModeY,
    kMode7DisplayFlipModeXY
} PDMode7_DisplayFlipMode;

typedef enum {
    kMode7SpriteDataSourceFrame,
    kMode7SpriteDataSourceAngle,
    kMode7SpriteDataSourcePitch,
    kMode7SpriteDataSourceScale
} PDMode7_SpriteDataSourceKey;

typedef enum {
    kMode7SpriteAlignmentNone,
    kMode7SpriteAlignmentEven,
    kMode7SpriteAlignmentOdd
} PDMode7_SpriteAlignment;

typedef enum {
    kMode7BillboardSizeAutomatic,
    kMode7BillboardSizeCustom
} PDMode7_SpriteBillboardSizeBehavior;

typedef struct PDMode7_Bitmap {
    void *prv;
} PDMode7_Bitmap;

typedef struct PDMode7_BitmapLayer {
    void *prv;
} PDMode7_BitmapLayer;

typedef struct PDMode7_WorldConfiguration {
    float width;
    float height;
    float depth;
    int gridCellSize;
} PDMode7_WorldConfiguration;

typedef struct PDMode7_World {
    void *prv;
} PDMode7_World;

typedef struct PDMode7_Background {
    void *prv;
} PDMode7_Background;

typedef struct PDMode7_Display {
    void *prv;
} PDMode7_Display;

typedef struct PDMode7_Camera {
    void *prv;
} PDMode7_Camera;

typedef struct {
    void *prv;
} PDMode7_SpriteDataSource;

typedef struct PDMode7_Sprite {
    void *prv;
} PDMode7_Sprite;

typedef struct {
    void *prv;
} PDMode7_SpriteInstance;

typedef void(PDMode7_SpriteDrawCallbackFunction)(PDMode7_SpriteInstance *instance, LCDBitmap *bitmap, PDMode7_Rect rect, void(*drawSprite)(PDMode7_SpriteInstance *instance));

typedef struct PDMode7_Pool_API {
    void(*realloc)(size_t size);
    void(*clear)(void);
} PDMode7_Pool_API;

typedef struct PDMode7_World_API {
    PDMode7_WorldConfiguration(*defaultConfiguration)(void);
    PDMode7_World*(*newWorld)(PDMode7_WorldConfiguration configuration);
    PDMode7_Vec3(*getSize)(PDMode7_World *world);
    float(*getScale)(PDMode7_World *world);
    PDMode7_Display*(*getMainDisplay)(PDMode7_World *world);
    PDMode7_Camera*(*getMainCamera)(PDMode7_World *world);
    void(*setPlaneBitmap)(PDMode7_World *world, PDMode7_Bitmap *bitmap);
    PDMode7_Bitmap*(*getPlaneBitmap)(PDMode7_World *world);
    PDMode7_Color(*getPlaneFillColor)(PDMode7_World *world);
    void(*setPlaneFillColor)(PDMode7_World *world, PDMode7_Color color);
    void(*setCeilingBitmap)(PDMode7_World *world, PDMode7_Bitmap *bitmap);
    PDMode7_Bitmap*(*getCeilingBitmap)(PDMode7_World *world);
    PDMode7_Color(*getCeilingFillColor)(PDMode7_World *world);
    void(*setCeilingFillColor)(PDMode7_World *world, PDMode7_Color color);
    PDMode7_Vec3(*worldToDisplayPoint)(PDMode7_World *world, PDMode7_Vec3 point, PDMode7_Display *display);
    PDMode7_Vec3(*displayToPlanePoint)(PDMode7_World *world, int displayX, int displayY, PDMode7_Display *display);
    PDMode7_Vec2(*planeToBitmapPoint)(PDMode7_World *world, float pointX, float pointY);
    PDMode7_Vec2(*bitmapToPlanePoint)(PDMode7_World *world, float bitmapX, float bitmapY);
    PDMode7_Vec3(*displayMultiplierForScanlineAt)(PDMode7_World *world, PDMode7_Vec3 point, PDMode7_Display *display);
    void(*addSprite)(PDMode7_World *world, PDMode7_Sprite *sprite);
    PDMode7_Sprite**(*getSprites)(PDMode7_World *world, int* length);
    PDMode7_SpriteInstance**(*getVisibleSpriteInstances)(PDMode7_World *world, int *length, PDMode7_Display *display);
    int(*addDisplay)(PDMode7_World *world, PDMode7_Display *display);
    void(*update)(PDMode7_World *world);
    void(*draw)(PDMode7_World *world, PDMode7_Display *display);
    void(*freeWorld)(PDMode7_World *world);
} PDMode7_World_API;

typedef struct PDMode7_Camera_API {
    PDMode7_Camera*(*newCamera)(void);
    PDMode7_Vec3(*getPosition)(PDMode7_Camera *camera);
    void(*setPosition)(PDMode7_Camera *camera, float x, float y, float z);
    float(*getPitch)(PDMode7_Camera *camera);
    void(*setPitch)(PDMode7_Camera *camera, float pitch);
    float(*getAngle)(PDMode7_Camera *camera);
    void(*setAngle)(PDMode7_Camera *camera, float angle);
    float(*getFOV)(PDMode7_Camera *camera);
    void(*setFOV)(PDMode7_Camera *camera, float fov);
    int(*getClipDistanceUnits)(PDMode7_Camera *camera);
    void(*setClipDistanceUnits)(PDMode7_Camera *camera, int units);
    void(*lookAtPoint)(PDMode7_Camera *camera, PDMode7_Vec3 point);
    void(*freeCamera)(PDMode7_Camera *camera);
} PDMode7_Camera_API;

typedef struct PDMode7_Background_API {
    LCDBitmap*(*getBitmap)(PDMode7_Background *background);
    void(*setBitmap)(PDMode7_Background *background, LCDBitmap *bitmap);
    PDMode7_Vec2(*getCenter)(PDMode7_Background *background);
    void(*setCenter)(PDMode7_Background *background, float x, float y);
    void(*getRoundingIncrement)(PDMode7_Background *background, unsigned int *x, unsigned int *y);
    void(*setRoundingIncrement)(PDMode7_Background *background, unsigned int x, unsigned int y);
    PDMode7_Vec2(*getOffset)(PDMode7_Background *background);
} PDMode7_Background_API;

typedef struct PDMode7_Display_API {
    PDMode7_Display*(*newDisplay)(int x, int y, int width, int height);
    PDMode7_Rect(*getRect)(PDMode7_Display *display);
    void(*setRect)(PDMode7_Display *display, int x, int y, int width, int height);
    PDMode7_DisplayOrientation(*getOrientation)(PDMode7_Display *display);
    void(*setOrientation)(PDMode7_Display *display, PDMode7_DisplayOrientation orientation);
    PDMode7_DisplayFlipMode(*getFlipMode)(PDMode7_Display *display);
    void(*setFlipMode)(PDMode7_Display *display, PDMode7_DisplayFlipMode flipMode);
    PDMode7_Camera*(*getCamera)(PDMode7_Display *display);
    void(*setCamera)(PDMode7_Display *display, PDMode7_Camera *camera);
    PDMode7_Background*(*getBackground)(PDMode7_Display *display);
    PDMode7_DisplayScale(*getScale)(PDMode7_Display *display);
    void(*setScale)(PDMode7_Display *display, PDMode7_DisplayScale scale);
    int(*getHorizon)(PDMode7_Display *display);
    float(*pitchForHorizon)(PDMode7_Display *display, float horizon);
    PDMode7_Vec2(*convertPointFromOrientation)(PDMode7_Display *display, float x, float y);
    PDMode7_Vec2(*convertPointToOrientation)(PDMode7_Display *display, float x, float y);
    PDMode7_World*(*getWorld)(PDMode7_Display *display);
    void(*removeFromWorld)(PDMode7_Display *display);
} PDMode7_Display_API;

typedef struct PDMode7_SpriteDataSource_API {
    void(*setMaximumWidth)(PDMode7_Sprite *sprite, int maximumWidth);
    void(*setMinimumWidth)(PDMode7_Sprite *sprite, int minimumWidth);
    void(*setLengthForKey)(PDMode7_Sprite *sprite, unsigned int length, PDMode7_SpriteDataSourceKey key);
    void(*setLayout)(PDMode7_Sprite *sprite, PDMode7_SpriteDataSourceKey k1, PDMode7_SpriteDataSourceKey k2, PDMode7_SpriteDataSourceKey k3, PDMode7_SpriteDataSourceKey k4);
} PDMode7_SpriteDataSource_API;

typedef struct PDMode7_Sprite_API {
    PDMode7_Sprite*(*newSprite)(float width, float height, float depth);
    PDMode7_SpriteInstance*(*getInstance)(PDMode7_Sprite *sprite, PDMode7_Display *display);
    PDMode7_Vec3(*getPosition)(PDMode7_Sprite *sprite);
    void(*setPosition)(PDMode7_Sprite *sprite, float x, float y, float z);
    PDMode7_Vec3(*getSize)(PDMode7_Sprite *sprite);
    void(*setSize)(PDMode7_Sprite *sprite, float width, float height, float depth);
    float(*getAngle)(PDMode7_Sprite *sprite);
    void(*setAngle)(PDMode7_Sprite *sprite, float angle);
    float(*getPitch)(PDMode7_Sprite *sprite);
    void(*setPitch)(PDMode7_Sprite *sprite, float pitch);
    void(*setVisible)(PDMode7_Sprite *sprite, int flag);
    void(*setImageCenter)(PDMode7_Sprite *sprite, float cx, float cy);
    void(*setRoundingIncrement)(PDMode7_Sprite *sprite, unsigned int x, unsigned int y);
    void(*setAlignment)(PDMode7_Sprite *sprite, PDMode7_SpriteAlignment alignmentX, PDMode7_SpriteAlignment alignmentY);
    void(*setDrawFunction)(PDMode7_Sprite *sprite, PDMode7_SpriteDrawCallbackFunction *function);
    void(*setBitmapTable)(PDMode7_Sprite *sprite, LCDBitmapTable *bitmapTable);
    void(*setFrame)(PDMode7_Sprite *sprite, unsigned int frame);
    void(*setBillboardSizeBehavior)(PDMode7_Sprite *sprite, PDMode7_SpriteBillboardSizeBehavior behavior);
    void(*setBillboardSize)(PDMode7_Sprite *sprite, float width, float height);
    void(*setUserData)(PDMode7_Sprite *sprite, void *userdata);
    void(*removeFromWorld)(PDMode7_Sprite *sprite);
    PDMode7_World*(*getWorld)(PDMode7_Sprite *sprite);
    void(*freeSprite)(PDMode7_Sprite *sprite);
    PDMode7_SpriteDataSource_API *dataSource;
} PDMode7_Sprite_API;

typedef struct PDMode7_SpriteInstanceDataSource_API {
    int(*getMaximumWidth)(PDMode7_SpriteDataSource *dataSource);
    void(*setMaximumWidth)(PDMode7_SpriteDataSource *dataSource, int maximumWidth);
    int(*getMinimumWidth)(PDMode7_SpriteDataSource *dataSource);
    void(*setMinimumWidth)(PDMode7_SpriteDataSource *dataSource, int minimumWidth);
    unsigned int(*getLengthForKey)(PDMode7_SpriteDataSource *dataSource, PDMode7_SpriteDataSourceKey key);
    void(*setLengthForKey)(PDMode7_SpriteDataSource *dataSource, unsigned int length, PDMode7_SpriteDataSourceKey key);
    void(*getLayout)(PDMode7_SpriteDataSource *dataSource, PDMode7_SpriteDataSourceKey *k1, PDMode7_SpriteDataSourceKey *k2, PDMode7_SpriteDataSourceKey *k3, PDMode7_SpriteDataSourceKey *k4);
    void(*setLayout)(PDMode7_SpriteDataSource *dataSource, PDMode7_SpriteDataSourceKey k1, PDMode7_SpriteDataSourceKey k2, PDMode7_SpriteDataSourceKey k3, PDMode7_SpriteDataSourceKey k4);
} PDMode7_SpriteInstanceDataSource_API;

typedef struct PDMode7_SpriteInstance_API {
    PDMode7_Sprite*(*getSprite)(PDMode7_SpriteInstance *instance);
    PDMode7_SpriteDataSource*(*getDataSource)(PDMode7_SpriteInstance *instance);
    int(*isVisible)(PDMode7_SpriteInstance *instance);
    void(*setVisible)(PDMode7_SpriteInstance *instance, int flag);
    PDMode7_Vec2(*getImageCenter)(PDMode7_SpriteInstance *instance);
    void(*setImageCenter)(PDMode7_SpriteInstance *instance, float cx, float cy);
    void(*getRoundingIncrement)(PDMode7_SpriteInstance *instance, unsigned int *x, unsigned int *y);
    void(*setRoundingIncrement)(PDMode7_SpriteInstance *instance, unsigned int x, unsigned int y);
    void(*getAlignment)(PDMode7_SpriteInstance *instance, PDMode7_SpriteAlignment *alignmentX, PDMode7_SpriteAlignment *alignmentY);
    void(*setAlignment)(PDMode7_SpriteInstance *instance, PDMode7_SpriteAlignment alignmentX, PDMode7_SpriteAlignment alignmentY);
    void(*setBitmapTable)(PDMode7_SpriteInstance *instance, LCDBitmapTable *bitmapTable);
    LCDBitmapTable*(*getBitmapTable)(PDMode7_SpriteInstance *instance);
    void(*setDrawFunction)(PDMode7_SpriteInstance *instance, PDMode7_SpriteDrawCallbackFunction *callback);
    unsigned int(*getFrame)(PDMode7_SpriteInstance *instance);
    void(*setFrame)(PDMode7_SpriteInstance *instance, unsigned int frame);
    PDMode7_SpriteBillboardSizeBehavior(*getBillboardSizeBehavior)(PDMode7_SpriteInstance *instance);
    void(*setBillboardSizeBehavior)(PDMode7_SpriteInstance *instance, PDMode7_SpriteBillboardSizeBehavior behavior);
    PDMode7_Vec2(*getBillboardSize)(PDMode7_SpriteInstance *instance);
    void(*setBillboardSize)(PDMode7_SpriteInstance *instance, float width, float height);
    LCDBitmap*(*getBitmap)(PDMode7_SpriteInstance *instance);
    PDMode7_Rect(*getDisplayRect)(PDMode7_SpriteInstance *instance);
    int(*isVisibleOnDisplay)(PDMode7_SpriteInstance *instance);
    void*(*getUserData)(PDMode7_SpriteInstance *instance);
    void(*setUserData)(PDMode7_SpriteInstance *instance, void *userdata);
    PDMode7_SpriteInstanceDataSource_API *dataSource;
} PDMode7_SpriteInstance_API;

typedef struct PDMode7_Rect_API {
    PDMode7_Rect(*newRect)(int x, int y, int width, int height);
} PDMode7_Rect_API;

typedef struct PDMode7_Vec2_API {
    PDMode7_Vec2(*newVec2)(float x, float y);
} PDMode7_Vec2_API;

typedef struct PDMode7_Vec3_API {
    PDMode7_Vec3(*newVec3)(float x, float y, float z);
} PDMode7_Vec3_API;

typedef struct PDMode7_Color_API {
    PDMode7_Color(*newGrayscaleColor)(uint8_t gray, uint8_t alpha);
} PDMode7_Color_API;

typedef struct PDMode7_BitmapLayer_API {
    PDMode7_BitmapLayer*(*newLayer)(PDMode7_Bitmap *bitmap);
    PDMode7_Bitmap*(*getBitmap)(PDMode7_BitmapLayer *layer);
    void(*setBitmap)(PDMode7_BitmapLayer *layer, PDMode7_Bitmap *bitmap);
    void(*getPosition)(PDMode7_BitmapLayer *layer, int *x, int *y);
    void(*setPosition)(PDMode7_BitmapLayer *layer, int x, int y);
    int(*isVisible)(PDMode7_BitmapLayer *layer);
    void(*setVisible)(PDMode7_BitmapLayer *layer, int visible);
    void(*invalidate)(PDMode7_BitmapLayer *layer);
    void(*removeFromBitmap)(PDMode7_BitmapLayer *layer);
    void(*freeLayer)(PDMode7_BitmapLayer *layer);
} PDMode7_BitmapLayer_API;

typedef struct PDMode7_Bitmap_API {
    PDMode7_Bitmap*(*loadPGM)(const char *filename);
    void(*getSize)(PDMode7_Bitmap *bitmap, int *width, int *height);
    PDMode7_Color(*colorAt)(PDMode7_Bitmap *bitmap, int x, int y);
    PDMode7_Bitmap*(*getMask)(PDMode7_Bitmap *bitmap);
    void(*setMask)(PDMode7_Bitmap *bitmap, PDMode7_Bitmap *mask);
    void(*addLayer)(PDMode7_Bitmap *bitmap, PDMode7_BitmapLayer *layer);
    PDMode7_BitmapLayer**(*getLayers)(PDMode7_Bitmap *bitmap, int *length);
    void(*removeAllLayers)(PDMode7_Bitmap *bitmap);
    void(*freeBitmap)(PDMode7_Bitmap *bitmap);
    PDMode7_BitmapLayer_API *layer;
} PDMode7_Bitmap_API;

typedef struct PDMode7_API {
    PDMode7_Pool_API *pool;
    PDMode7_World_API *world;
    PDMode7_Display_API *display;
    PDMode7_Background_API *background;
    PDMode7_Camera_API *camera;
    PDMode7_Sprite_API *sprite;
    PDMode7_SpriteInstance_API *spriteInstance;
    PDMode7_Bitmap_API *bitmap;
    PDMode7_Vec2_API *vec2;
    PDMode7_Vec3_API *vec3;
    PDMode7_Color_API *color;
    PDMode7_Rect_API *rect;
} PDMode7_API;

extern PDMode7_API *mode7;

void PDMode7_init(PlaydateAPI *playdate, int enableLua);

#endif /* pd_mode7_h */
