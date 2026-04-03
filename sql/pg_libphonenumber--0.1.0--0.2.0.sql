CREATE FUNCTION phone_number_region_code(packed_phone_number) RETURNS text
    LANGUAGE c IMMUTABLE STRICT
    AS 'pg_libphonenumber', 'packed_phone_number_region_code';

CREATE FUNCTION phone_number_geographical_area_code(packed_phone_number) RETURNS text
    LANGUAGE c IMMUTABLE STRICT
    AS 'pg_libphonenumber', 'packed_phone_number_geographical_area_code';

CREATE FUNCTION phone_number_national_destination_code(packed_phone_number) RETURNS text
    LANGUAGE c IMMUTABLE STRICT
    AS 'pg_libphonenumber', 'packed_phone_number_national_destination_code';

CREATE FUNCTION phone_number_type(packed_phone_number) RETURNS text
    LANGUAGE c IMMUTABLE STRICT
    AS 'pg_libphonenumber', 'packed_phone_number_type';

