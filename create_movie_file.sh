#!/bin/bash

set -e

ffmpeg -y -i build/MUSIC01.mpg -c copy green.mp2
ffmpeg -y -i build/MUSIC01.mpg -c copy green.m1v
