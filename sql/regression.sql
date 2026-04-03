CREATE EXTENSION IF NOT EXISTS pg_libphonenumber;

--Test phone number parsing
select parse_packed_phone_number('555-555-5555', 'US');
--These two should produce errors because the number is too long.
--Produces an error in pg-libphonenumber's code
select parse_packed_phone_number('555-555-5555555555', 'US');
--Produces an error from libphonenumber
select parse_packed_phone_number('555-555-55555555555', 'US');

-- Do we get correct country codes?
-- TODO: expand.
select phone_number_country_code(parse_packed_phone_number('+1-555-555-5555', 'USA'));
select phone_number_country_code(parse_packed_phone_number('11987654321', 'BR'));

-- Region and area code helpers.
select phone_number_region_code(parse_packed_phone_number('11987654321', 'BR'));
select phone_number_geographical_area_code(parse_packed_phone_number('11987654321', 'BR'));
select phone_number_national_destination_code(parse_packed_phone_number('11987654321', 'BR'));

-- Number types.
select phone_number_type(parse_packed_phone_number('11987654321', 'BR'));
select phone_number_type(parse_packed_phone_number('08001234567', 'BR'));
select phone_number_geographical_area_code(parse_packed_phone_number('08001234567', 'BR'));
