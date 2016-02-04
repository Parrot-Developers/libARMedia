#!/bin/sh

rm -rf gen
python ../ARSDKBuildUtils/Utils/Python/ARSDK_PrebuildActions.py --lib libARMedia --root . || exit 1
