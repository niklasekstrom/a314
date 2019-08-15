This setup allows you to use docker to build the drivers with no vbcc install required. Or to setup a build agent etc. 

to build the docker image :-

```cd Software```
```docker build -t terriblefire/vbcc .```

then to build the a314.device

```cd a314device```
```docker run --rm --volume "$PWD":/usr/src/compile --workdir /usr/src/compile -i -t terriblefire/vbcc make``` 

The device driver appears in the a314device folder. 