-- Historical releases registered > and >= as their own commutators.
-- Preserve the phone type and all values. RESTRICT makes the upgrade atomic
-- if an installation has an index or expression that needs separate migration.
DO $upgrade$
BEGIN
    IF NOT EXISTS (
        SELECT 1
        FROM pg_catalog.pg_operator
        WHERE oprleft = 'packed_phone_number'::regtype
          AND oprright = 'packed_phone_number'::regtype
          AND oprname IN ('>', '>=')
          AND oprcom = oid
    ) THEN
        RETURN;
    END IF;

    DROP OPERATOR CLASS packed_phone_number_ops USING btree RESTRICT;
    DROP OPERATOR < (packed_phone_number, packed_phone_number),
                  <= (packed_phone_number, packed_phone_number),
                  > (packed_phone_number, packed_phone_number),
                  >= (packed_phone_number, packed_phone_number) RESTRICT;

    CREATE OPERATOR < (
        leftarg = packed_phone_number,
        rightarg = packed_phone_number,
        procedure = packed_phone_number_less,
        commutator = >,
        negator = >=,
        restrict = scalarltsel,
        join = scalarltjoinsel
    );

    CREATE OPERATOR <= (
        leftarg = packed_phone_number,
        rightarg = packed_phone_number,
        procedure = packed_phone_number_less_or_equal,
        commutator = >=,
        negator = >,
        restrict = scalarltsel,
        join = scalarltjoinsel
    );

    CREATE OPERATOR > (
        leftarg = packed_phone_number,
        rightarg = packed_phone_number,
        procedure = packed_phone_number_greater,
        commutator = <,
        negator = <=,
        restrict = scalargtsel,
        join = scalargtjoinsel
    );

    CREATE OPERATOR >= (
        leftarg = packed_phone_number,
        rightarg = packed_phone_number,
        procedure = packed_phone_number_greater_or_equal,
        commutator = <=,
        negator = <,
        restrict = scalargtsel,
        join = scalargtjoinsel
    );

    CREATE OPERATOR CLASS packed_phone_number_ops
        DEFAULT FOR TYPE packed_phone_number USING btree AS
            OPERATOR 1 <,
            OPERATOR 2 <=,
            OPERATOR 3 =,
            OPERATOR 4 >=,
            OPERATOR 5 >,
            FUNCTION 1 packed_phone_number_cmp(packed_phone_number, packed_phone_number);
END;
$upgrade$;
