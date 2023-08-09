#!/bin/bash

# set -x

convert texture_unpack/combined_textures.png -crop 1024x1024+0+0 texture_unpack/wall_left_0.png
convert texture_unpack/combined_textures.png -crop 1024x1024+1024+0 texture_unpack/wall_left_1.png
convert texture_unpack/combined_textures.png -crop 512x1024+2080+0 texture_unpack/window_left_2.png
convert texture_unpack/combined_textures.png -crop 512x1024+2624+0 texture_unpack/window_left_1.png
convert texture_unpack/combined_textures.png -crop 512x1024+3168+0 texture_unpack/window_left_0.png

convert texture_unpack/combined_textures.png -crop 1024x1024+0+1056 texture_unpack/wall_right_0.png
convert texture_unpack/combined_textures.png -crop 1024x1024+1024+1056 texture_unpack/wall_right_1.png
convert texture_unpack/combined_textures.png -crop 512x1024+2080+1056 texture_unpack/window_right_2.png
convert texture_unpack/combined_textures.png -crop 512x1024+2624+1056 texture_unpack/window_right_1.png
convert texture_unpack/combined_textures.png -crop 512x1024+3168+1056 texture_unpack/window_right_0.png

convert texture_unpack/combined_textures.png -crop 1024x1024+0+2112 texture_unpack/floor_main.png
convert texture_unpack/combined_textures.png -crop 1024x1024+1056+2112 texture_unpack/floor_upper.png
convert texture_unpack/combined_textures.png -crop 1024x1024+2112+2112 texture_unpack/wall_main.png
convert texture_unpack/combined_textures.png -crop 1024x1024+3168+2112 texture_unpack/window_main.png

convert texture_unpack/combined_textures.png -crop 1024x1024+0+3168 texture_unpack/benches.png
convert texture_unpack/combined_textures.png -crop 1024x1024+1056+3168 texture_unpack/ceiling.png
convert texture_unpack/combined_textures.png -crop 1024x1024+2112+3168 texture_unpack/wall_back.png
convert texture_unpack/combined_textures.png -crop 512x512+3168+3168 texture_unpack/entry.png