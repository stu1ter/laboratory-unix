FROM alpine:3.18

RUN apk add --no-cache util-linux

WORKDIR /app
RUN mkdir /data

COPY script.sh .
RUN chmod +x script.sh

CMD ["./script.sh"]
