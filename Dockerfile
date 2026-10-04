FROM debian:bookworm

WORKDIR /app

RUN apt-get update \
    && apt-get install -y gcc libc6-dev make procps \
    && rm -rf /var/lib/apt/lists/*

COPY src/ ./src/
COPY experiment.sh ./

RUN sed -i 's/\r$//' experiment.sh \
    && chmod +x experiment.sh

RUN gcc -Wall -Wextra -O2 -pthread src/server.c -o server -lrt \
    && gcc -Wall -Wextra -O2 src/client.c -o client -lrt

CMD ["bash"]