#FROM ubuntu:latest
FROM alpine:latest

MAINTAINER pavel.pimenov@gmail.com

RUN apk add --no-cache bash

WORKDIR /usr/src/app

COPY fly-server-loop ./
COPY fly-server-test-port ./

EXPOSE 37016

RUN chmod +x /usr/src/app/fly*

ENTRYPOINT /usr/src/app/fly-server-loop
