FROM gcc:latest
WORKDIR /app
COPY . .
RUN gcc -o client client.c
CMD ["./client"]
