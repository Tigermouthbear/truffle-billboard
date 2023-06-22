FROM node:20-alpine as frontend

WORKDIR /app

COPY frontend/package*.json .

RUN npm install

COPY frontend /app

RUN npm run build


FROM drogonframework/drogon:latest as backend

WORKDIR /app

COPY backend /app

RUN cmake -DCMAKE_BUILD_TYPE=Release . && make

COPY --from=frontend /app/build assets/

EXPOSE 8000

ENTRYPOINT [ "/app/truffle-billboard" ]
