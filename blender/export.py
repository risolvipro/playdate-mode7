import bpy
import os
import shutil
import shlex
import math
import imbuf

# Config BEGIN

name = "suzanne"

scale_max = 120
scale_min = 20
scale_step = 10

yaw_rotations = 18
pitch_rotations = 1

parent_name = "Camera Parent"
folder_name = "output"

# Config END

def quote_arg(str):
    if os.name == "nt":
        return '"' + str + '"'
    return shlex.quote(str)

output_dir = bpy.path.abspath("//" + folder_name)

if os.path.isdir(output_dir):
    shutil.rmtree(output_dir)
os.mkdir(output_dir)

camera_parent = bpy.data.objects.get(parent_name)

yaw_step = round(360 / yaw_rotations)
pitch_step = round(360 / pitch_rotations)
number_of_scales = round((scale_max - scale_min + scale_step) / scale_step)

original_rotation = tuple(camera_parent.rotation_euler)
render_path = output_dir + os.sep + "render.png"

i = 1
for yaw_iter in range(yaw_rotations):
    yaw = yaw_iter * yaw_step
    
    for pitch_iter in range(pitch_rotations):
        pitch = pitch_iter * pitch_step
        
        camera_parent.rotation_euler = (math.radians(-pitch), 0, math.radians(yaw))
        
        # Render
        bpy.context.scene.render.filepath = render_path
        bpy.ops.render.render(write_still = True)
        
        im = imbuf.load(render_path)
        
        # Resize
        for scale_iter in range(number_of_scales):
            width = round(scale_max - scale_iter * scale_step)
            height = round(width * im.size[1] / im.size[0])
            
            image_path = output_dir + os.sep + name + "-table-" + str(i) + ".png"  
            
            command = "magick " + quote_arg(render_path) + " "
            command += "-resize "+ str(width) + "x" + str(height) + " "
            command += "-colorspace gray -ordered-dither o4x4" + " "
            command += quote_arg(image_path)
                
            os.system(command)
            
            i += 1

if os.path.exists(render_path):
    os.remove(render_path)
    
camera_parent.rotation_euler = original_rotation