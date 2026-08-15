FROM postgres:16-bookworm

RUN apt-get update && apt-get install -y \
    build-essential \
    postgresql-server-dev-${PG_MAJOR} \
    libphonenumber-dev \
    sudo

COPY ./ /data/
RUN chown -R postgres:postgres /data/

# Build pg_libphonenumber.
WORKDIR /data
RUN make clean && make && make install

USER postgres

CMD ["/data/docker/run-tests.sh", "${PG_MAJOR}"]
