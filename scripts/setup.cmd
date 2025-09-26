git submodule update --init --recursive
docker build --build-arg=UNAME=%username% .\\docker\\ -t temp
