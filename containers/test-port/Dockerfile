FROM alpine:latest
RUN apk add --no-cache bash
WORKDIR /usr/src/app

ADD fly-server-loop ./
ADD fly-server-test-port ./

EXPOSE 37015

RUN chmod +x /usr/src/app/fly*
#ENTRYPOINT /usr/src/app/fly-server-loop
