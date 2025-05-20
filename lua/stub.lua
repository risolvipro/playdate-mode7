mode7 = {}

---@class mode7.world
mode7.world = {}

---@class mode7.world.configuration
---@field width integer
---@field height integer
---@field depth integer
---@field gridCellSize integer
mode7.world.configuration = {}

---@class mode7.display
---@field kScale1x1 integer 0
---@field kScale2x1 integer 1
---@field kScale2x2 integer 2
---@field kScale4x1 integer 3
---@field kScale4x2 integer 4
---@field kOrientationLandscapeLeft integer 0
---@field kOrientationLandscapeRight integer 1
---@field kOrientationPortrait integer 2
---@field kOrientationPortraitUpsideDown integer 3
mode7.display = {}

---@class mode7.background
mode7.background = {}

---@class mode7.bitmap
mode7.bitmap = {}

---@class mode7.bitmap.layer
mode7.bitmap.layer = {}

---@class mode7.pool
mode7.pool = {}

---@class mode7.camera
mode7.camera = {}

---@class mode7.sprite
---@field kAlignmentNone integer 0
---@field kAlignmentEven integer 1
---@field kAlignmentOdd integer 2
---@field kBillboardSizeAutomatic integer 0
---@field kBillboardSizeCustom integer 1
mode7.sprite = {}

---@class mode7.sprite.datasource
mode7.sprite.datasource = {}

---@class mode7.sprite.instance
mode7.sprite.instance = {}

---@class mode7.sprite.instance.datasource
mode7.sprite.instance.datasource = {}

---@class mode7.image
mode7.image = {}

---@class mode7.imagetable
mode7.imagetable = {}

---@class mode7.array
mode7.array = {}

---@class mode7.color
---@field gray integer 255
---@field alpha integer 255
mode7.color = {}

---@param width integer
---@param height integer
---@param depth integer
---@param gridCellSize integer
---@return mode7.world
function mode7.world._new(width, height, depth, gridCellSize) return mode7.world end

---@param gray integer
---@param alpha integer
function mode7.world:_setPlaneFillColor(gray, alpha) end

---@return integer gray
---@return integer alpha
function mode7.world:_getPlaneFillColor() return 0, 0 end

---@param gray integer
---@param alpha integer
function mode7.world:_setCeilingFillColor(gray, alpha) end

---@return integer gray
---@return integer alpha
function mode7.world:_getCeilingFillColor() return 0, 0 end

---@param x integer
---@param y integer
---@return integer gray
---@return integer alpha
function mode7.bitmap:_colorAt(x, y) return 0, 0 end

--- Returns the world size.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-getSize
---@return integer width
---@return integer height
---@return integer depth
function mode7.world:getSize() return 0, 0, 0 end

--- Returns the main display managed by the world.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-getMainDisplay
---@return mode7.display
function mode7.world:getMainDisplay() return {} end

--- Sets a bitmap for the plane.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-setPlaneBitmap
---@param bitmap mode7.bitmap
function mode7.world:setPlaneBitmap(bitmap) end

--- Returns the plane bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-getPlaneBitmap
---@return mode7.bitmap
function mode7.world:getPlaneBitmap() return {} end

--- Sets a bitmap for the ceiling.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-setCeilingBitmap
---@param bitmap mode7.bitmap
function mode7.world:setCeilingBitmap(bitmap) end

--- Returns the ceiling bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-getCeilingBitmap
---@return mode7.bitmap
function mode7.world:getCeilingBitmap() return {} end

--- Converts a world point to a display point. The z component of the returned value is 1 if the point is in front of the camera or -1 if the point is behind the camera.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-worldToDisplayPoint
---@param x number
---@param y number
---@param z number
---@param display mode7.display?
---@return number displayX
---@return number displayY
---@return number displayZ
function mode7.world:worldToDisplayPoint(x, y, z, display) return 0, 0, 0 end

--- Converts a display point to a plane point. The z component of the returned value is 0 if the display point is on the plane, -1 if the display point is not on the plane.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-displayToPlanePoint
---@param displayX number
---@param displayY number
---@param display mode7.display?
---@return number planeX
---@return number planeY
---@return number planeZ
function mode7.world:displayToPlanePoint(displayX, displayY, display) return 0, 0, 0 end

--- Returns a multiplier that can be used to convert a length on a scanline, from the world coordinate system to the display coordinate system. This function is used to calculate the size of a sprite on display.
--- The z component of the multiplier is 1 if the point is in front of the camera or -1 if the point is behind the camera.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-displayMultiplierForScanlineAt
---@param x number
---@param y number
---@param z number
---@param display mode7.display?
---@return number multiplierX
---@return number multiplierY
---@return number multiplierZ
function mode7.world:displayMultiplierForScanlineAt(x, y, z, display) return 0, 0, 0 end

--- Returns a bitmap point from a plane point. If the world scale is 1, the two points are equal.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-planeToBitmapPoint
---@param planeX number
---@param planeY number
---@return number bitmapX
---@return number bitmapY
function mode7.world:planeToBitmapPoint(planeX, planeY) return 0, 0 end

--- Returns a bitmap point from a plane point. If the world scale is 1, the two points are equal.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-bitmapToPlanePoint
---@param bitmapX number
---@param bitmapY number
---@return number planeX
---@return number planeY
function mode7.world:bitmapToPlanePoint(bitmapX, bitmapY) return 0, 0 end

--- Adds the display to the world. It returns true if the display is added successfully, false if the maximum number of displays is reached or the display is already linked to a world.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-addDisplay
---@param display mode7.display
---@return boolean success
function mode7.world:addDisplay(display) return false end

--- Adds the sprite to the world.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-addSprite
---@param sprite mode7.sprite
function mode7.world:addSprite(sprite) end

--- Updates the world state before drawing. This function calculates the visible sprites and their rects on screen.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-update
function mode7.world:update() end

--- Draws the contents of the world for the given display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-draw
---@param display mode7.display?
function mode7.world:draw(display) end

--- Returns a mode7.array (not a Lua array) of all sprites added to the world.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-getSprites
---@return mode7.array
function mode7.world:getSprites() return {} end

--- Returns a mode7.array (not a Lua array) of all visible sprite instances for the display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-getVisibleSpriteInstances
---@param display mode7.display?
---@return mode7.array
function mode7.world:getVisibleSpriteInstances(display) return {} end

--- Creates a new display with the given rect.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-new
---@param x integer
---@param y integer
---@param width integer
---@param height integer
---@return mode7.display
function mode7.display.new(x, y, width, height) return {} end

--- Sets the rect of the display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-setRect
---@param x integer
---@param y integer
---@param width integer
---@param height integer
function mode7.display:setRect(x, y, width, height) end

--- Gets the rect of the display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-getRect
---@return integer x
---@return integer y
---@return integer width
---@return integer height
function mode7.display:getRect() return 0, 0, 0, 0 end

--- Sets the display orientation, default value is "landscape left".
--- You should set the display rect relative to the orientation. E.g. for a portrait display, the rect is x = 0, y = 0, width = 240, height = 400.
--- Functions such as world:worldToDisplayPoint return display coordinates relative to the orientation as well.
--- You can use display:convertPointFromOrientation to convert a point from the orientation coordinate system.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-setOrientation
---@param orientation integer
function mode7.display:setOrientation(orientation) end

--- Gets the display orientation.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-getOrientation
---@return integer
function mode7.display:getOrientation() return 0 end

--- Sets the display flip mode.
--- This parameter, in conjunction with the orientation, is used to determine the display visualization.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-setFlipMode
---@param flipMode integer
function mode7.display:setFlipMode(flipMode) end

--- Gets the display flip mode.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-getFlipMode
---@return integer
function mode7.display:getFlipMode() return 0 end

--- Sets the camera to the display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-setCamera
---@return integer x
---@return integer y
---@return integer width
---@return integer height
function mode7.display:setCamera() return 0, 0, 0, 0 end

--- Gets the camera of the display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-getCamera
---@return mode7.camera
function mode7.display:getCamera() return {} end

--- Sets the scale factor for the plane. It changes the plane resolution similarly to playdate.display.setScale(scale). You can change it to improve performance, default value is 2x2.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-setScale
---@param scale integer
function mode7.display:setScale(scale) end

--- Gets the scale factor for the plane.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-getScale
---@return integer
function mode7.display:getScale() return 0 end

--- Returns the background interface associated to the display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-getBackground
---@return mode7.background
function mode7.display:getBackground() return {} end

--- Returns the y position of the horizon, relative to the current display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-getHorizon
---@return integer
function mode7.display:getHorizon() return 0 end

--- Returns the pitch angle needed to align the horizon at the specified y position.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-pitchForHorizon
---@return number
function mode7.display:pitchForHorizon() return 0 end

--- It converts a point from the orientation coordinate system, to the standard coordinate system. For a portrait orientation, the portrait point (0, 0) is converted to the standard point (400, 0).
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-convertPointFromOrientation
---@param x number
---@param y number
---@return number x
---@return number y
function mode7.display:convertPointFromOrientation(x, y) return 0, 0 end

--- It converts a point from the standard coordinate system, to the orientation coordinate system. For a portrait orientation, the standard point (0, 0) is converted to the portrait point (0, 400).
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-convertPointToOrientation
---@param x number
---@param y number
---@return number x
---@return number y
function mode7.display:convertPointToOrientation(x, y) return 0, 0 end

--- Returns the world associated to the display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-getWorld
---@return mode7.world
function mode7.display:getWorld() return {} end

--- Removes the display from the world.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-display-removeFromWorld
function mode7.display:removeFromWorld() end

--- Sets the background bitmap. You should use mode7.image.new(filename) to load it, you can't directly pass a Playdate image.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-background-setBitmap
---@param image mode7.image?
function mode7.background:setImage(image) end

--- Gets the background bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-background-getBitmap
---@return mode7.image?
function mode7.background:getImage() return {} end

--- Gets the background offset.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-background-getOffset
---@return number
function mode7.background:getOffset() return 0 end

--- Sets the rounding increment for the background. This value is used for rounding the position to the nearest integer. Default value is 1.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-background-setRoundingIncrement
---@param x integer
---@param y integer
function mode7.background:setRoundingIncrement(x, y) end

--- Gets the rounding increment for the background.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-background-getRoundingIncrement
---@return number x
---@return number y
function mode7.background:getRoundingIncrement() return 0, 0 end

--- Sets the background center (0.0 - 1.0). Default value is (0.5, 0.5) so that the background is centered when the camera angle is 0.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-background-setCenter
---@param cx number
---@param cy number
function mode7.background:setCenter(cx, cy) end

--- Gets the background center.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-background-getCenter
---@return number cx
---@return number cy
function mode7.background:getCenter() return 0, 0 end

--- Loads a PGM file at the given path, it returns NULL if the file can't be opened.
--- PGM is a grayscale image format, you can encode it with ImageMagick (recommended):
--- magick input.png -fx '(r+g+b)/3' -colorspace gray output.pgm
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmap-loadPGM
---@param path string
---@return mode7.bitmap
function mode7.bitmap.loadPGM(path) return {} end

--- Gets the bitmap size.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmap-getSize
---@return integer width
---@return integer height
function mode7.bitmap:getSize() return 0, 0 end

--- Sets a mask for the bitmap (transparent pixels are black). The mask size must be equal to the bitmap size.
--- You can create a mask with ImageMagick:
--- magick input.png -alpha extract mask.pgm
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmap-setMask
---@param mask mode7.bitmap
function mode7.bitmap:setMask(mask) end

--- Gets the bitmap mask.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmap-getMask
---@return mode7.bitmap mask
function mode7.bitmap:getMask() return {} end

--- Adds a layer to the bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmap-addLayer
---@param layer mode7.bitmap.layer
function mode7.bitmap:addLayer(layer) end

--- Returns a mode7.array (not a Lua array) of all layers added to the bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmap-getLayers
---@return mode7.array
function mode7.bitmap:getLayers() return {} end

--- Removes all the layers from the bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmap-removeAllLayers
function mode7.bitmap:removeAllLayers() end

--- Creates a new layer with the given bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmapLayer-newLayer
---@param bitmap mode7.bitmap
---@return mode7.bitmap.layer
function mode7.bitmap.layer.new(bitmap) return {} end

--- Sets a bitmap for the layer.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmapLayer-setBitmap
---@param bitmap mode7.bitmap
function mode7.bitmap.layer:setBitmap(bitmap) end

--- Gets the bitmap of the layer.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmapLayer-getBitmap
---@return mode7.bitmap
function mode7.bitmap.layer:getBitmap() return {} end

--- Sets the left-top position of the layer.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmapLayer-setPosition
---@param x integer
---@param y integer
function mode7.bitmap.layer:setPosition(x, y) end

--- Gets the layer position.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmapLayer-getPosition
---@return integer x
---@return integer y
function mode7.bitmap.layer:getPosition() return 0, 0 end

--- Sets the layer visibility.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmapLayer-setVisible
---@param visible boolean
function mode7.bitmap.layer:setVisible(visible) end

--- Gets the layer visibility.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmapLayer-isVisible
---@return boolean
function mode7.bitmap.layer:isVisible() return false end

--- Invalidates the layer forcing a redraw. This function is needed only for special cases, such as setting a new mask for the bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmapLayer-invalidate
function mode7.bitmap.layer:invalidate() end

--- Removes the layer from the bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmapLayer-removeFromBitmap
function mode7.bitmap.layer:removeFromBitmap() end

--- Resize the pool to the given size in bytes. We recommend to call this function only once.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-pool-realloc
---@param size number
function mode7.pool.realloc(size) end

--- Clear the pool. You should call it before loading a new bitmap.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-pool-clear
function mode7.pool.clear() end

--- Creates a new camera.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-new
---@return mode7.camera
function mode7.camera.new() return {} end

--- Sets the camera position.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-setPosition
---@param x number
---@param y number
---@param z number
function mode7.camera:setPosition(x, y, z) end

--- Gets the camera position.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-getPosition
---@return number x
---@return number y
---@return number z
function mode7.camera:getPosition() return 0, 0, 0 end

--- Sets the camera yaw angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-setAngle
---@param angle number
function mode7.camera:setAngle(angle) end

--- Gets the camera yaw angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-getAngle
---@return number
function mode7.camera:getAngle() return 0 end

--- Sets the camera pitch angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-setPitch
---@param pitch number
function mode7.camera:setPitch(pitch) end

--- Gets the camera pitch angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-getPitch
---@return number
function mode7.camera:getPitch() return 0 end

--- Sets the camera FOV angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-setFOV
---@param fov number
function mode7.camera:setFOV(fov) end

--- Gets the camera FOV angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-getFOV
---@return number
function mode7.camera:getFOV() return 0 end

--- Sets the camera clip distance (distance within sprites are visible) expressed as units of the grid. A value of 1 will mark the sorrounding cells by a movement of 1 unit in all directions.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-setClipDistanceUnits
---@param units integer
function mode7.camera:setClipDistanceUnits(units) end

--- Gets the camera clip distance units.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-getClipDistanceUnits
---@return number
function mode7.camera:getClipDistanceUnits() return 0 end

--- Adjusts the camera orientation (angle, pitch) to look at the given point.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-camera-lookAtPoint
---@param x number
---@param y number
---@param z number
function mode7.camera:lookAtPoint(x, y, z) end

--- Creates a new sprite with the given size. Width and height are 2D values on the plane, depth is the third dimension.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-new
---@param width number
---@param height number
---@param depth number
---@return mode7.sprite
function mode7.sprite.new(width, height, depth) return {} end

--- Gets the sprite instance for the given display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-getInstance
---@param display mode7.display
---@return mode7.sprite.instance
function mode7.sprite:getInstance(display) return {} end

--- Sets the sprite center at the given position.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setPosition
---@param x number
---@param y number
---@param z number
function mode7.sprite:setPosition(x, y, z) end

--- Gets the sprite position.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-getPosition
---@return number x
---@return number y
---@return number z
function mode7.sprite:getPosition() return 0, 0, 0 end

--- Sets the sprite size.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setSize
---@param width number
---@param height number
---@param depth number
function mode7.sprite:setSize(width, height, depth) end

--- Gets the sprite size.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-getSize
---@return number width
---@return number height
---@return number depth
function mode7.sprite:getSize() return 0, 0, 0 end

--- Sets the sprite yaw angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setAngle
---@param angle number
function mode7.sprite:setAngle(angle) end

--- Gets the sprite yaw angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-getAngle
---@return number angle
function mode7.sprite:getAngle() return 0 end

--- Sets the sprite pitch angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setPitch
---@param pitch number
function mode7.sprite:setPitch(pitch) end

--- Gets the sprite pitch angle.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-getPitch
---@return number pitch
function mode7.sprite:getPitch() return 0 end

--- Sets the sprite frame index (all instances).
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setFrame
---@param frame integer
function mode7.sprite:setFrame(frame) end

--- Sets the sprite visibility for all the instances.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setVisible
---@param visible boolean
function mode7.sprite:setVisible(visible) end

--- Sets the image center for all the instances.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setImageCenter
---@param cx number
---@param cy number
function mode7.sprite:setImageCenter(cx, cy) end

--- Sets the sprite rounding increment for all the instances.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setRoundingIncrement
---@param x integer
---@param y integer
function mode7.sprite:setRoundingIncrement(x, y) end

--- Sets the sprite alignment for all the instances.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setAlignment
---@param alignmentX integer
---@param alignmentY integer
function mode7.sprite:setAlignment(alignmentX, alignmentY) end

--- Sets the sprite image table for all the instances.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setBimapTable
---@param imageTable mode7.imagetable?
function mode7.sprite:setImageTable(imageTable) end

--- Sets the billboard size behavior for all the instances.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setBillboardSizeBehavior
---@param behavior integer
function mode7.sprite:setBillboardSizeBehavior(behavior) end

--- Sets the billboard size for all the instances.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setBillboardSize
---@param width number
---@param height number
function mode7.sprite:setBillboardSize(width, height) end

--- Sets a custom draw function for all the instances.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-setDrawFunction
---@param functionName string
function mode7.sprite:setDrawFunctionName(functionName) end

--- Gets the sprite data source interface.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-datasource-setMaximumWidth
---@return mode7.sprite.datasource
function mode7.sprite:getDataSource() return {} end

--- Sets the maximum width for the data source (all instances).
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-datasource-setMaximumWidth
---@param width integer
function mode7.sprite.datasource:setMaximumWidth(width) end

--- Sets the minimum width for the data source (all instances).
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-datasource-setMinimumWidth
---@param width integer
function mode7.sprite.datasource:setMinimumWidth(width) end

--- Sets the length for the given data source key (all instances).
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-datasource-setLengthForKey
---@param length integer
---@param key integer
function mode7.sprite.datasource:setLengthForKey(length, key) end

--- Sets the layout for the data source (all instances).
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-datasource-setLayout
---@param k1 integer
---@param k2 integer
---@param k3 integer
---@param k4 integer
function mode7.sprite.datasource:setLayout(k1, k2, k3, k4) end

--- Returns the world associated to the sprite.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-getWorld
---@return mode7.world?
function mode7.sprite:getWorld() return {} end

--- Removes the sprite from the world.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-sprite-removeFromWorld
function mode7.sprite:removeFromWorld() end

--- Returns the sprite associated to the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-spriteInstance
---@return mode7.sprite
function mode7.sprite.instance:getSprite() return {} end

--- Sets the visibility for the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-setVisible
---@param visible boolean
function mode7.sprite.instance:setVisible(visible) end

--- Gets the visibility for the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-isVisible
---@return boolean
function mode7.sprite.instance:isVisible() return false end

--- Sets the sprite frame index for the instance. If you want to animate your sprite, use this property to set the animation frame (starting from 0).
--- Animation length must be set in the data source by calling sprite.instance.datasource:setLengthForKey with the mode7.sprite.datasource.kFrame key.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-setFrame
---@param frame integer
function mode7.sprite.instance:setFrame(frame) end

--- Gets the sprite frame index for the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getFrame
---@return integer
function mode7.sprite.instance:getFrame() return 0 end

--- Sets the image center (0.0 - 1.0) for the instance. Use this property to adjust the sprite position on screen, default value is (0.5, 0.5).
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-setImageCenter
---@param cx number
---@param cy number
function mode7.sprite.instance:setImageCenter(cx, cy) end

--- Gets the image center for the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getImageCenter
---@return number cx
---@return number cy
function mode7.sprite.instance:getImageCenter() return 0, 0 end

--- Sets the sprite rounding increment for the instance. This value is used for rounding the position on screen to the nearest integer. Default value is 1.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-setRoundingIncrement
---@param x integer
---@param y integer
function mode7.sprite.instance:setRoundingIncrement(x, y) end

--- Gets the sprite rounding increment for the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getRoundingIncrement
---@return integer x
---@return integer y
function mode7.sprite.instance:getRoundingIncrement() return 0, 0 end

--- Sets the sprite alignment for the instance. It aligns the sprite position on screen to even or odd values.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-setAlignment
---@param alignmentX integer
---@param alignmentY integer
function mode7.sprite.instance:setAlignment(alignmentX, alignmentY) end

--- Gets the sprite alignment for the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getAlignment
---@return integer alignmentX
---@return integer alignmentY
function mode7.sprite.instance:getAlignment() return 0, 0 end

--- Sets the image table for the instance. You should use mode7.imagetable.new to load it, you can't directly pass a Playdate image table.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-setBitmapTable
---@param imageTable mode7.imagetable?
function mode7.sprite.instance:setImageTable(imageTable) end

--- Gets the image table for the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getBitmapTable
---@return mode7.imagetable?
function mode7.sprite.instance:getImageTable() return {} end

--- Sets a custom draw function for the instance, you should pass the function name as a string (passing a table path with dots is not recommended).
--- To draw the sprite, call the provided callback drawSprite with instance as the first parameter.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-setDrawFunction
---@param name string
function mode7.sprite.instance:setDrawFunctionName(name) end

--- Sets the behavior for determining the billboard size.
--- * When the behavior is set to automatic, the billboard width is determined by the longer side of the sprite.
--- * When the behavior is set to custom, the billboard width is determined by the custom width.
--- 
--- Typically, only the billboard width is used to calculate the sprite rect on screen. The billboard height serves as a fallback value when the image is unavailable.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-setBillboardSizeBehavior
---@param behavior integer
function mode7.sprite.instance:setBillboardSizeBehavior(behavior) end

--- Sets a custom size for the billboard. The size is expressed in the world coordinate system. In order to use this property, set the behavior to custom.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-setBillboardSize
---@param width number
---@param height number
function mode7.sprite.instance:setBillboardSize(width, height) end

--- Gets the billboard size behavior.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getBillboardSizeBehavior
---@return integer
function mode7.sprite.instance:getBillboardSizeBehavior() return 0 end

--- Gets the billboard custom size.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getBillboardSize
---@return number width
---@return number height
function mode7.sprite.instance:getBillboardSize() return 0, 0 end

--- It returns the visible image for the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getImage
---@return mode7.image
function mode7.sprite.instance:getImage() return {} end

--- It returns the display rect for the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getDisplayRect
---@return integer x
---@return integer y
---@return integer width
---@return integer height
function mode7.sprite.instance:getDisplayRect() return 0, 0, 0, 0 end

--- It returns true if the instance is visible on the associated display.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-isVisibleOnDisplay
---@return boolean
function mode7.sprite.instance:isVisibleOnDisplay() return false end

--- Returns the data source of the instance.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteInstance-getDataSource
---@return mode7.sprite.instance.datasource
function mode7.sprite.instance.getDataSource() return {} end

--- Sets the maximum width for the data source. This is the maximum width supported by the image table.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteDataSource-setMaximumWidth
---@param width integer
function mode7.sprite.instance.datasource:setMaximumWidth(width) end

--- Gets the maximum width for the data source.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteDataSource-getMaximumWidth
---@return integer
function mode7.sprite.instance.datasource:getMaximumWidth() return 0 end

--- Sets the minimum width for the data source. This is the minimum width supported by the image table.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteDataSource-setMinimumWidth
---@param width integer
function mode7.sprite.instance.datasource:setMinimumWidth(width) end

--- Gets the minimum width for the data source.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteDataSource-getMinimumWidth
---@return integer
function mode7.sprite.instance.datasource:getMinimumWidth() return 0 end

--- Sets the length for the given data source key. The default length for each key is 1 (length must be greater than 0).
--- Use this function to specify how many times a sprite is scaled and rotated in the image table.
--- E.g. For a rotation by 10 degrees, set a length of 36. For a scaling from 100px to 10px, set a length of 10.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteDataSource-setLengthForKey
---@param width integer
---@param key integer
function mode7.sprite.instance.datasource:setLengthForKey(width, key) end

--- Sets the layout for the data source. It defines the order of the image table, the default value "frame, angle, pitch, scale" implies the following structure:
--- begin frame-1
---    begin angle-1
---        begin pitch-1
---            save [scale-1, scale-2, scale-3, ...]
---        end pitch-1
---        begin pitch-2
---            save [scale-1, scale-2, scale-3, ...]
---        end pitch-2
---        ...
---    end angle-1
---    ...
--- end frame-1
--- ...
--- A sprite is: rotated clockwise starting from the frontal position (for a character sprite, angle-1 will show the character's face), rotated in the positive pitch direction (for a character sprite, the head will move towards you), scaled from maximum to minimum width.
--- Notes:
--- * An image is never repeated twice in the table.
--- * Keys with a length of 1 are considered optional. For example, you can have a sprite with a single frame, angle, pitch but multiple scales.
--- * Layout is not valid if you use the same key twice.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteDataSource-setLayout
---@param k1 integer
---@param k2 integer
---@param k3 integer
---@param k4 integer
function mode7.sprite.instance.datasource:setLayout(k1, k2, k3, k4) end

--- Gets the length for the given data source key.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteDataSource-getLengthForKey
---@param key integer
---@return integer
function mode7.sprite.instance.datasource:getLengthForKey(key) return 0 end

--- Gets the layout for the data source.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-spriteDataSource-getLayout
---@return integer k1
---@return integer k2
---@return integer k3
---@return integer k4
function mode7.sprite.instance.datasource:getLayout() return 0, 0, 0, 0 end

--- Loads the image at the given path. Equivalent to playdate.graphics.image.new(path). Returns nil if the image can't be opened.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-luaImage-new
---@param path string
---@return mode7.image?
function mode7.image.new(path) return {} end

--- Returns the underlying Playdate image.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-luaImage-getPDImage
---@return any
function mode7.image:getPDImage() return {} end

--- Loads the image table at the given path. Equivalent to playdate.graphics.imagetable.new(path). Returns nil if the image table can't be opened.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-luaImageTable-new
---@param path string
---@return mode7.imagetable?
function mode7.imagetable.new(path) return {} end

--- Gets the number of items in the array.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-luaArray-size
---@return integer
function mode7.array:size() return 0 end

--- Gets the item at the given index, array starts at index 1.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-luaArray-get
---@param index integer
---@return any
function mode7.array:get(index) return {} end
