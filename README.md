# `pg_libphonenumber`

A (partially implemented!) PostgreSQL extension that provides access to
[Google's `libphonenumber`](https://github.com/googlei18n/libphonenumber)

## Project status

This extension is in an **alpha** state. It's not complete or tested enough for
critical production deployments, but with a little help, we should be able to
get it there.

## Synopsis

```sql
CREATE EXTENSION pg_libphonenumber;
SELECT parse_packed_phone_number('03 7010 1234', 'AU');
SELECT parse_packed_phone_number('2819010011', 'US');
SELECT phone_number_country_code(parse_packed_phone_number('11987654321', 'BR'));
SELECT phone_number_region_code(parse_packed_phone_number('11987654321', 'BR'));
SELECT phone_number_geographical_area_code(parse_packed_phone_number('11987654321', 'BR'));
SELECT phone_number_type(parse_packed_phone_number('11987654321', 'BR'));
SELECT phone_number_is_valid(parse_packed_phone_number('11987654321', 'BR'));
SELECT phone_number_possible_reason(parse_packed_phone_number('1187654321', 'BR'));

CREATE TABLE foo ( ph packed_phone_number );
```

`parse_packed_phone_number`, `phone_number_type`, `phone_number_is_valid` and
`phone_number_possible_reason` are declared `PARALLEL SAFE`, allowing bulk
validation queries to use PostgreSQL parallel workers. These functions use
only backend-local memory and read-only libphonenumber metadata.

## Installation

### Debian/Ubuntu

First you'll need to install `libphonenumber-dev` and the corresponding
`postgresql-server-dev` package.

```shell-script
sudo apt-get update && sudo apt-get install \
    build-essential \
    postgresql-server-dev-<major> \
    libphonenumber-dev
```

Then clone this repository and build.

```shell-script
git clone https://github.com/blm768/pg-libphonenumber
cd pg-libphonenumber
make
sudo make install
```

## Running tests

For convenience, we provide a Docker image that sets up a test environment.
Run the script `./run-tests.sh` to build and run the image.
