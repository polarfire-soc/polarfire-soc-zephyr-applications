# Build a Zephyr Application Using Docker

A Dockerfile is provided which can generate an image to build a zephyr application on Linux, Mac and Windows operating systems.

## Install Prerequisites
Follow the [instructions here](https://docs.docker.com/get-docker/) to install docker on your host computer.


## Building the Docker Image
To build the docker image, open a command line inside the polarfire-soc-zephyr-applications directory and run:
```
$ docker build - < container-services/zephyr-builder/Dockerfile -t zephyr-builder:latest
```

## Building a Zephyr Application using Docker
The `zephyr-builder` Docker image can be used to build a Zephyr application. By mounting a volume witihin the Docker container, the Zephyr application that has been built will persist.

To run the `zephyr-builder` container to build a Zephyr application:
### On Linux
open a command prompt inside the polarfire-soc-zephyr-applications directory and run:
```
$ docker run --rm -it -w /$PWD -v /$PWD:/$PWD:rw,z zephyr-builder:latest bash
```
### On Windows
The following command has been tested on Windows OS. It is not possible to set the workspace path within the Docker execution environment to match the host system as the container is uses a base linux image.

```
docker run --rm -it -v %cd%:/polarfire-soc-zephyr-applications:rw,z zephyr-builder:latest bash
```
This will launch a Docker container in the terminal, mount the polarfire-soc-zephyr-applications directory into the Docker execution environment with read and write privileges.
To build a Zephyr application from within the Docker execution environment, [follow the instructions here](../mpfs-applications/README.md)


After building the Zephyr Application in the Docker container, enter ctrll+d to terminate the Docker exectution environment. The Zephyr application that was built will persist in the build directory.