FROM drogonframework/drogon:latest

RUN apt-get update; apt-get install -yqq minify

WORKDIR /app

COPY . .

RUN cmake .; make; minify -r -o assets-min/ assets/

EXPOSE 8000

ENTRYPOINT [ "/app/truffle-billboard" ]
