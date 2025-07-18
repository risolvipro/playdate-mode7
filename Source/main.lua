import "CoreLibs/graphics"
import "mode7"

playdate.display.setRefreshRate(0)

-- Set the pool size
mode7.pool.realloc(5000 * 1000)

function newWorld()
    -- Clear the pool before loading a new PGM
    mode7.pool.clear()

    local configuration = mode7.world.defaultConfiguration()
    configuration.width = 2048
    configuration.height = 2048
    configuration.depth = 2048

    local world = mode7.world.new(configuration)

    local bitmap = mode7.bitmap.loadPGM("images/track-0.pgm")
    world:setPlaneBitmap(bitmap)
    
    world:setPlaneFillColor(mode7.color.grayscale.new(60, 255))

    --[[
    local tilemap = world:newTilemap(64, 64)

    local tileBitmap = mode7.bitmap.loadPGM("images/tile.pgm")
    tilemap:setBitmapAtRange(tileBitmap, 0, 0, -1, -1)
    
    local tileBitmap1 = mode7.bitmap.loadPGM("images/tile1.pgm")
    tilemap:setBitmapAtPosition(tileBitmap1, 16, 3)
    tilemap:setBitmapAtPosition(tileBitmap1, 16, 4)

    tilemap:setFillBitmap(tileBitmap)

    world:setPlaneTilemap(tilemap)
    --]]

    local imageTable = mode7.imagetable.new("images/full-car")
    
    addCar(world, imageTable, 246, 1106, 1.5)
    addCar(world, imageTable, 195, 1134, 1.5)

    local display = world:getMainDisplay()
    local camera = display:getCamera()

    -- display:setRect(0, 0, 240, 400)
    -- display:setOrientation(mode7.display.kOrientationPortrait)
    -- camera:setFOV(math.rad(45))

    local backgroundImage = mode7.image.new("images/background")
    display:getBackground():setImage(backgroundImage)

    camera:setPosition(220, 1200, 12)
    camera:setAngle(math.rad(-90))

    --[[
    local coinBitmap = mode7.bitmap.loadPGM("images/coin.pgm")
    local coinMask = mode7.bitmap.loadPGM("images/coin-mask.pgm")
    coinBitmap:setMask(coinMask)

    local coinLayer1 = mode7.bitmap.layer.new(coinBitmap)
    coinLayer1:setPosition(200, 1100)

    local coinLayer2 = mode7.bitmap.layer.new(coinBitmap)
    coinLayer2:setPosition(220, 1100)

    bitmap:addLayer(coinLayer1)
    bitmap:addLayer(coinLayer2)
    --]]

    --[[
    local shader = mode7.shader.linear.new()
    shader:setMinimumDistance(50)
    shader:setMaximumDistance(400)
    display:setPlaneShader(shader)
    --]]

    return world
end

function addCar(world, imageTable, x, y, z)
    local car = mode7.sprite.new(10, 10, 3)
    car:setPosition(x, y, z)
    car:setImageTable(imageTable)
    car:setImageCenter(0.5, 0.3)
    car:setAngle(math.rad(-90))
    car:setAlignment(mode7.sprite.kAlignmentOdd, mode7.sprite.kAlignmentOdd)
    
    local dataSource = car:getDataSource()

    dataSource:setMinimumWidth(4)
    dataSource:setMaximumWidth(160)

    dataSource:setLengthForKey(40, mode7.sprite.datasource.kScale)
    dataSource:setLengthForKey(36, mode7.sprite.datasource.kAngle)

    world:addSprite(car)

    return car
end

function updateCamera(dt)
    local display = world:getMainDisplay()
    local camera = display:getCamera()

	local angle = camera:getAngle()
    local posX, posY, posZ = camera:getPosition()
    
    local angleDelta = 1 * dt

    if playdate.buttonIsPressed(playdate.kButtonLeft) then
        angle = angle - angleDelta
    elseif playdate.buttonIsPressed(playdate.kButtonRight) then
        angle = angle + angleDelta
	end
    
    local moveDelta = 100 * dt
    local moveVelocity = 0
    
    if playdate.buttonIsPressed(playdate.kButtonUp) then
        moveVelocity = moveDelta
    elseif playdate.buttonIsPressed(playdate.kButtonDown) then
        moveVelocity = -moveDelta
	end
    
    local heightDelta = 60 * dt
    local height = posZ

    if playdate.buttonIsPressed(playdate.kButtonA) then
        height = height + heightDelta
    elseif playdate.buttonIsPressed(playdate.kButtonB) then
        height = height - heightDelta
	end

    camera:setAngle(angle)

	local crankChange = playdate.getCrankChange()
    camera:setPitch(camera:getPitch() + crankChange * 0.005)

	local cameraX = posX + moveVelocity * math.cos(angle)
    local cameraY = posY + moveVelocity * math.sin(angle)

	camera:setPosition(cameraX, cameraY, height)
end

world = newWorld()

local menu = playdate.getSystemMenu()
local menuItem, error = menu:addMenuItem("Restart", function()
    world = newWorld()
end)

function playdate.update()
	local dt = playdate.getElapsedTime()
	playdate.resetElapsedTime()

    updateCamera(dt)

    playdate.graphics.clear()

    world:update()

    --[[
    local visibleSprites = world:getVisibleSpriteInstances()
    local size = visibleSprites:size()
    for i=1, size do
        local instance = visibleSprites:get(i)
        print(instance:getDisplayRect())
    end
    --]]
    
    world:draw()

	playdate.drawFPS(0, 0)
end

