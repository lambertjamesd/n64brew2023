#!/bin/bash

# set -x

convert texture_unpack/combined.png -crop 1024x1024+0+0 texture_unpack/wall_left_0.png
convert texture_unpack/combined.png -crop 1024x1024+1024+0 texture_unpack/wall_left_1.png

convert texture_unpack/combined.png -crop 1024x1024+2080+0 texture_unpack/window_left_0.png
convert texture_unpack/combined.png -crop 1024x1024+2624+0 texture_unpack/window_left_1.png
convert texture_unpack/combined.png -crop 1024x1024+3168+0 texture_unpack/window_left_2.png

convert texture_unpack/combined.png -crop 1024x1024+0+1056 texture_unpack/wall_right_0.png
convert texture_unpack/combined.png -crop 1024x1024+1024+1056 texture_unpack/wall_right_1.png

convert texture_unpack/combined.png -crop 1024x1024+2080+1056 texture_unpack/window_right_0.png
convert texture_unpack/combined.png -crop 1024x1024+2624+1056 texture_unpack/window_right_1.png
convert texture_unpack/combined.png -crop 1024x1024+3168+1056 texture_unpack/window_right_2.png