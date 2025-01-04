FROM gcc:latest
WORKDIR /app
COPY . .
RUN gcc -o server server.c
CMD ["./server"]
