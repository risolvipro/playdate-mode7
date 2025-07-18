import "CoreLibs/object"

local lib = {}

-- Color class

class('Color', {gray=255,alpha=255,space=0}, lib).extends()

function lib.Color:init(space)
    lib.Color.super.init(self)
    self.space = space
end

mode7.color = lib.Color
mode7.color.grayscale = {}
mode7.color.kSpaceGrayscale = 0

--- Creates a new grayscale color.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-color-grayscale-new
---@param gray integer
---@param alpha integer
---@return mode7.color
mode7.color.grayscale.new = function(gray, alpha)
    local color = lib.Color(mode7.color.kSpaceGrayscale)
    color.gray = gray
    color.alpha = alpha
    return color
end

-- World

--- Returns a new default configuration for the world.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-defaultConfiguration
---@return mode7.world.configuration
mode7.world.defaultConfiguration = function()
    return {
        width = 1024,
        height = 1024,
        depth = 1024,
        gridCellSize = 256
    }
end

--- Creates a new world with the given configuration. By default, a world has a main display and a main camera.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-new
---@param configuration mode7.world.configuration
---@return mode7.world
mode7.world.new = function(configuration)
    return mode7.world._new(
        configuration.width, configuration.height, configuration.depth,
        configuration.gridCellSize
    )
end

--- Returns the plane color to be used for the out-of-bounds space.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-getPlaneFillColor
---@return mode7.color
function mode7.world:getPlaneFillColor()
    local gray, alpha = self:_getPlaneFillColor()
    return mode7.color.grayscale.new(gray, alpha)
end

--- Sets the plane color to be used for the out-of-bounds space.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-setPlaneFillColor
---@param color mode7.color
function mode7.world:setPlaneFillColor(color)
    self:_setPlaneFillColor(color.gray, color.alpha)
end

--- Returns the ceiling color to be used for the out-of-bounds space.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-getCeilingFillColor
---@return mode7.color
function mode7.world:getCeilingFillColor()
    local gray, alpha = self:_getCeilingFillColor()
    return mode7.color.grayscale.new(gray, alpha)
end

--- Sets the ceiling color to be used for the out-of-bounds space.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-setCeilingFillColor
---@param color mode7.color
function mode7.world:setCeilingFillColor(color)
    self:_setCeilingFillColor(color.gray, color.alpha)
end

--- Gets the color of the plane at the specified point.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-planeColorAt
---@param x number
---@param y number
---@return mode7.color
function mode7.world:planeColorAt(x, y)
    local gray, alpha = self:_planeColorAt(x, y)
    return mode7.color.grayscale.new(gray, alpha)
end

--- Gets the color of the ceiling at the specified point.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-world-ceilingColorAt
---@param x number
---@param y number
---@return mode7.color
function mode7.world:ceilingColorAt(x, y)
    local gray, alpha = self:_ceilingColorAt(x, y)
    return mode7.color.grayscale.new(gray, alpha)
end

-- Display

mode7.display.kScale1x1 = 0
mode7.display.kScale2x1 = 1
mode7.display.kScale2x2 = 2
mode7.display.kScale4x1 = 3
mode7.display.kScale4x2 = 4

mode7.display.kDitherBayer2x2 = 0
mode7.display.kDitherBayer4x4 = 1
mode7.display.kDitherBayer8x8 = 2

mode7.display.kOrientationLandscapeLeft = 0
mode7.display.kOrientationLandscapeRight = 1
mode7.display.kOrientationPortrait = 2
mode7.display.kOrientationPortraitUpsideDown = 3

mode7.display.kFlipModeNone = 0
mode7.display.kFlipModeX = 1
mode7.display.kFlipModeY = 2
mode7.display.kFlipModeXY = 3

-- Sprite

mode7.sprite.datasource.kFrame = 0
mode7.sprite.datasource.kAngle = 1
mode7.sprite.datasource.kPitch = 2
mode7.sprite.datasource.kScale = 3

mode7.sprite.kAlignmentNone = 0
mode7.sprite.kAlignmentEven = 1
mode7.sprite.kAlignmentOdd = 2

mode7.sprite.kBillboardSizeAutomatic = 0
mode7.sprite.kBillboardSizeCustom = 1

mode7.sprite.kVisibilityModeDefault = 0
mode7.sprite.kVisibilityModeShader = 1

-- Bitmap

--- Creates a new bitmap filled with bgColor.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmap-new
---@param width integer
---@param height integer
---@param bgColor mode7.color
---@return mode7.bitmap
mode7.bitmap.new = function(width, height, bgColor)
    return mode7.bitmap._new(width, height, bgColor.gray, bgColor.alpha)
end

--- Gets the color value at the given position
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-bitmap-colorAt
---@param x integer
---@param y integer
---@return mode7.color
function mode7.bitmap:colorAt(x, y)
    local gray, alpha = self:_colorAt(x, y)
    return mode7.color.grayscale.new(gray, alpha)
end

-- Shader

--- Sets the shader color. Alpha is supported.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-shader-linear-setColor
---@param color mode7.color
function mode7.shader.linear:setColor(color)
    self:_setColor(color.gray, color.alpha)
end

--- Gets the shader color.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-shader-linear-getColor
---@return mode7.color
function mode7.shader.linear:getColor()
    local gray, alpha = self:_getColor()
    return mode7.color.grayscale.new(gray, alpha)
end

--- Sets the shader color. Alpha is supported.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-shader-radial-setColor
---@param color mode7.color
function mode7.shader.radial:setColor(color)
    self:_setColor(color.gray, color.alpha)
end

--- Gets the shader color.
---
--- https://risolvipro.github.io/playdate-mode7/Lua-API.html#def-shader-radial-getColor
---@return mode7.color
function mode7.shader.radial:getColor()
    local gray, alpha = self:_getColor()
    return mode7.color.grayscale.new(gray, alpha)
end