#!/bin/bash

usage() 
{
    echo "Usage: build_image.sh [[[ -s server_name] [-b [dev | main | test]] [-v version] | [-h]'"
}

while [ "$1" != "" ]; do
    case $1 in
        -s | --server )  shift
                         server_name=$1
                         ;;
        -b | --branch )  shift
                         branch_name=$1
                         ;;
        -v | --version ) shift
                         version=$1
                         ;;
        -h | --help )    usage
                         exit
                         ;;
        * )              usage
                         echo "Error script parameters!"
                         exit 1
    esac
    shift
done

if [ -z "$server_name" ]; then
    echo "The server name cannot empty, please input your project server name!"
    usage; exit 1
fi

if [ -z "$branch_name" ]; then
    branch_name="dev"
elif [ "$branch_name" != "dev" ] && [ "$branch_name" != "main" ] && [ "$branch_name" != "test" ]; then
    echo "Error branch name, the right name should be 'dev', 'main' or 'test'"
    usage; exit 1
fi

# check version number
if [ -z "$version" ]; then
    version="latest"
else 
    version=$(echo "${version}" | sed -ne 's/[^0-9]*\(\([0-9]\.\)\{0,4\}[0-9][^.]\)\([^ ]*\).*/\1\3/p')
    if [ -z "$version" ]; then
        version="latest"
    fi
fi

LAST_RUNNING_IMAGE=$(sudo docker ps -a | grep $server_name | awk '{print $1}')
LAST_IMAGE=$(sudo docker images -a | grep $server_name | awk '{print $3}')

# stop the previous image and remove it
if [ -n "${LAST_RUNNING_IMAGE}" ]; then
    sudo docker stop $LAST_RUNNING_IMAGE
    sudo docker rm $LAST_RUNNING_IMAGE 
fi

if [ -n "${LAST_IMAGE}" ]; then
    sudo docker rmi $LAST_IMAGE --force
fi

DEPLOY_PATH="/root/edu-deploy"
SERVER_PATH=${DEPLOY_PATH}/${server_name}

if [ ! -d "$SERVER_PATH" ]; then
   mkdir $SERVER_PATH
   rm $SERVER_PATH/*
   cp $DEPLOY_PATH/Dockerfile $SERVER_PATH
fi

if [ -d "$SERVER_PATH" ]; then
   JAR_FILE="${DEPLOY_PATH}/${server_name}-${version}.jar"
   mv $JAR_FILE $SERVER_PATH
fi

IMAGE_NAME="online-edu/${server_name}"
if [ "$version" = "latest" ]; then
    IMAGE="${IMAGE_NAME}:${branch_name}-${version}"
else
    DATE=$(date '+%Y%m%d%H%M%S')
    IMAGE="${IMAGE_NAME}:${branch_name}-${version}-${DATE}"
fi

# build a new image and run it.
echo "sudo docker build -t $IMAGE $SERVER_PATH"
sudo docker build -t $IMAGE $SERVER_PATH
sudo docker run --detach --interactive --tty --net=host --name=$server_name "${IMAGE}"

