#!/bin/bash

PLATFORM=""
if [[ "$(arch)" != "arm64" ]]; then
  # requires 'apt install qemu-user-static'
  PLATFORM="--platform=aarch64"
fi

result=$( docker images -q a314 )
if [[ ! -n "$result" ]]; then
  docker build $PLATFORM -t a314 .
fi

docker run $PLATFORM --volume "$PWD":/host --workdir /host -it --rm a314 /bin/bash -c "./local.sh"
