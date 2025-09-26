#!/usr/bin/env bash

docker run -it \
	--mount type=bind,source=$PWD,destination=/temp \
	temp
