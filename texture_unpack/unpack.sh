#!/bin/bash

# set -x

convert texture_unpack/combined_textures.png -crop 1024x1024+0+0 assets/materials/megatextures/wall_left_0.png
convert texture_unpack/combined_textures.png -crop 1024x1024+1024+0 assets/materials/megatextures/wall_left_1.png
convert texture_unpack/combined_textures.png -crop 512x1024+2080+0 assets/materials/megatextures/window_left_2.png
convert texture_unpack/combined_textures.png -crop 512x1024+2624+0 assets/materials/megatextures/window_left_1.png
convert texture_unpack/combined_textures.png -crop 512x1024+3168+0 assets/materials/megatextures/window_left_0.png

convert texture_unpack/combined_textures.png -crop 1024x1024+0+1056 assets/materials/megatextures/wall_right_0.png
convert texture_unpack/combined_textures.png -crop 1024x1024+1024+1056 assets/materials/megatextures/wall_right_1.png
convert texture_unpack/combined_textures.png -crop 512x1024+2080+1056 assets/materials/megatextures/window_right_2.png
convert texture_unpack/combined_textures.png -crop 512x1024+2624+1056 assets/materials/megatextures/window_right_1.png
convert texture_unpack/combined_textures.png -crop 512x1024+3168+1056 assets/materials/megatextures/window_right_0.png

convert texture_unpack/combined_textures.png -crop 1024x1024+0+2112 assets/materials/megatextures/floor_main.png
convert texture_unpack/combined_textures.png -crop 1024x1024+1056+2112 assets/materials/megatextures/floor_upper.png
convert texture_unpack/combined_textures.png -crop 1024x1024+2112+2112 assets/materials/megatextures/wall_main.png
convert texture_unpack/combined_textures.png -crop 1024x1024+3168+2112 assets/materials/megatextures/window_main.png

convert texture_unpack/combined_textures.png -crop 1024x1024+0+3168 assets/materials/megatextures/benches.png
convert texture_unpack/combined_textures.png -crop 1024x1024+1056+3168 assets/materials/megatextures/ceiling.png
convert texture_unpack/combined_textures.png -crop 1024x1024+2112+3168 assets/materials/megatextures/wall_back.png
convert texture_unpack/combined_textures.png -crop 512x512+3168+3168 assets/materials/megatextures/entry.png