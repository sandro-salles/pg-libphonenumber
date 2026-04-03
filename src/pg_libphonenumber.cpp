#include <exception>
#include <string>

#include "phonenumbers/phonenumberutil.h"

extern "C" {
    #include "postgres.h"
    #include "libpq/pqformat.h"
    #include "fmgr.h"
    #include "varatt.h"
    #include "utils/builtins.h"
}

#include "error_handling.h"
#include "packed_phone_number.h"

using namespace i18n::phonenumbers;

static const PhoneNumberUtil* const phoneUtil = PhoneNumberUtil::GetInstance();

/**
 * Clamps a value to the given (inclusive) range
 */
template <typename T>
T clamp(const T& n, const T& lower, const T& upper) {
  return std::max(lower, std::min(n, upper));
}

/*
 * Utility functions
 */

/**
 * Converts a text object to a C-style string
 */
static char* text_to_c_string(const text* text) {
    size_t len = VARSIZE(text) - VARHDRSZ;
    char* str = (char*)palloc(len + 1);
    memcpy(str, VARDATA(text), len);
    str[len] = '\0';
    return str;
}

/**
 * Converts a std::string into a PostgreSQL text object.
 */
static text* string_to_text(const std::string& value) {
    return cstring_to_text_with_len(value.data(), value.size());
}

/**
 * Converts a packed phone number back into the libphonenumber object.
 */
static PhoneNumber packed_phone_number_to_phone_number(const PackedPhoneNumber* number) {
    return *number;
}

/**
 * Returns the national significant number as defined by libphonenumber.
 */
static std::string get_national_significant_number(const PhoneNumber& number) {
    std::string national_significant_number;
    phoneUtil->GetNationalSignificantNumber(number, &national_significant_number);
    return national_significant_number;
}

/**
 * Returns the geographical area code for a valid number, or an empty string
 * when the number has no geographical area code.
 */
static std::string get_geographical_area_code(const PhoneNumber& number) {
    int area_code_length = phoneUtil->GetLengthOfGeographicalAreaCode(number);
    if(area_code_length <= 0) {
        return "";
    }

    std::string national_significant_number = get_national_significant_number(number);
    if(static_cast<size_t>(area_code_length) > national_significant_number.size()) {
        return "";
    }

    return national_significant_number.substr(0, area_code_length);
}

/**
 * Returns the national destination code for a valid number, or an empty string
 * when the number has no national destination code.
 */
static std::string get_national_destination_code(const PhoneNumber& number) {
    int ndc_length = phoneUtil->GetLengthOfNationalDestinationCode(number);
    if(ndc_length <= 0) {
        return "";
    }

    std::string national_significant_number = get_national_significant_number(number);
    if(static_cast<size_t>(ndc_length) > national_significant_number.size()) {
        return "";
    }

    return national_significant_number.substr(0, ndc_length);
}

/**
 * Maps libphonenumber number types to stable textual identifiers.
 */
static const char* get_phone_number_type_name(PhoneNumberUtil::PhoneNumberType type) {
    switch(type) {
        case PhoneNumberUtil::FIXED_LINE:
            return "FIXED_LINE";
        case PhoneNumberUtil::MOBILE:
            return "MOBILE";
        case PhoneNumberUtil::FIXED_LINE_OR_MOBILE:
            return "FIXED_LINE_OR_MOBILE";
        case PhoneNumberUtil::TOLL_FREE:
            return "TOLL_FREE";
        case PhoneNumberUtil::PREMIUM_RATE:
            return "PREMIUM_RATE";
        case PhoneNumberUtil::SHARED_COST:
            return "SHARED_COST";
        case PhoneNumberUtil::VOIP:
            return "VOIP";
        case PhoneNumberUtil::PERSONAL_NUMBER:
            return "PERSONAL_NUMBER";
        case PhoneNumberUtil::PAGER:
            return "PAGER";
        case PhoneNumberUtil::UAN:
            return "UAN";
        case PhoneNumberUtil::VOICEMAIL:
            return "VOICEMAIL";
        case PhoneNumberUtil::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

//Internal function used by packed_phone_number_in and parse_packed_phone_number
//TODO: take a std::string to minimize copying?
PackedPhoneNumber* do_parse_packed_phone_number(const char* number_str, const char* country) {
    PhoneNumber number;
    PackedPhoneNumber* short_number;

    short_number = (PackedPhoneNumber*)palloc0(sizeof(PackedPhoneNumber));
    if(short_number == nullptr) {
        throw std::bad_alloc();
    }

    PhoneNumberUtil::ErrorType error;
    error = phoneUtil->Parse(number_str, country, &number);
    if(error == PhoneNumberUtil::NO_PARSING_ERROR) {
        //Initialize short_number using placement new.
        new(short_number) PackedPhoneNumber(number);
        return short_number;
    } else {
        reportParseError(number_str, error);
        return nullptr;
    }
    //TODO: check number validity.
}

//TODO: check null args (PG_ARGISNULL) and make non-strict?

/*
 * Extension functions
 */

extern "C" {
    #ifdef PG_MODULE_MAGIC
        PG_MODULE_MAGIC;
    #endif

    /*
     * I/O functions
     */

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_in);

    PGDLLEXPORT Datum
    packed_phone_number_in(PG_FUNCTION_ARGS) {
        try {
            const char *number_str = PG_GETARG_CSTRING(0);

            //TODO: use international format instead.
            PackedPhoneNumber* number = do_parse_packed_phone_number(number_str, "US");
            if(number) {
                PG_RETURN_POINTER(number);
            } else {
                PG_RETURN_NULL();
            }
        } catch(std::exception& e) {
            reportException(e);
            PG_RETURN_NULL();
        }
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_out);

    PGDLLEXPORT Datum
    packed_phone_number_out(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* short_number = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            PhoneNumber number = *short_number;

            std::string formatted;
            phoneUtil->Format(number, PhoneNumberUtil::INTERNATIONAL, &formatted);

            //Copy the formatted number to a C-style string.
            //We must use the PostgreSQL allocator, not new/malloc.
            size_t len = formatted.length();
            char* result = (char*)palloc(len + 1);
            if(result == nullptr) {
                throw std::bad_alloc();
            }
            memcpy(result, formatted.data(), len);
            result[len] = '\0';

            PG_RETURN_CSTRING(result);
        } catch (const std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_recv);

    PGDLLEXPORT Datum
    packed_phone_number_recv(PG_FUNCTION_ARGS) {
        try {
            StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
            PackedPhoneNumber* number;

            number = (PackedPhoneNumber*)palloc(sizeof(PackedPhoneNumber));
            //TODO: make portable (fix endianness issues, etc.).
            pq_copymsgbytes(buf, (char*)number, sizeof(PackedPhoneNumber));
            PG_RETURN_POINTER(number);
        } catch (const std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_send);

    PGDLLEXPORT Datum
    packed_phone_number_send(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber *number = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            StringInfoData buf;

            pq_begintypsend(&buf);
            pq_sendbytes(&buf, (const char*)number, sizeof(PackedPhoneNumber));
            PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
        } catch (const std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    /*
     * Operator functions
     */

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_equal);

    PGDLLEXPORT Datum
    packed_phone_number_equal(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* number1 = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            const PackedPhoneNumber* number2 = (PackedPhoneNumber*)PG_GETARG_POINTER(1);

            PG_RETURN_BOOL(*number1 == *number2);
        } catch(std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_not_equal);

    PGDLLEXPORT Datum
    packed_phone_number_not_equal(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* number1 = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            const PackedPhoneNumber* number2 = (PackedPhoneNumber*)PG_GETARG_POINTER(1);

            PG_RETURN_BOOL(*number1 != *number2);
        } catch(std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_less);

    PGDLLEXPORT Datum
    packed_phone_number_less(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* number1 = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            const PackedPhoneNumber* number2 = (PackedPhoneNumber*)PG_GETARG_POINTER(1);

            PG_RETURN_BOOL(number1->compare_fast(*number2) < 0);
        } catch(std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_less_or_equal);

    PGDLLEXPORT Datum
    packed_phone_number_less_or_equal(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* number1 = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            const PackedPhoneNumber* number2 = (PackedPhoneNumber*)PG_GETARG_POINTER(1);

            PG_RETURN_BOOL(number1->compare_fast(*number2) <= 0);
        } catch(std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_greater);

    PGDLLEXPORT Datum
    packed_phone_number_greater(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* number1 = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            const PackedPhoneNumber* number2 = (PackedPhoneNumber*)PG_GETARG_POINTER(1);

            PG_RETURN_BOOL(number1->compare_fast(*number2) > 0);
        } catch(std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_greater_or_equal);

    PGDLLEXPORT Datum
    packed_phone_number_greater_or_equal(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* number1 = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            const PackedPhoneNumber* number2 = (PackedPhoneNumber*)PG_GETARG_POINTER(1);

            PG_RETURN_BOOL(number1->compare_fast(*number2) >= 0);
        } catch(std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_cmp);

    PGDLLEXPORT Datum
    packed_phone_number_cmp(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* number1 = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            const PackedPhoneNumber* number2 = (PackedPhoneNumber*)PG_GETARG_POINTER(1);

            int64_t compared = number1->compare_fast(*number2);

            PG_RETURN_INT32(clamp<int64_t>(compared, -1, 1));
        } catch(std::exception& e) {
            reportException(e);
        }

        PG_RETURN_NULL();
    }

    /*
     * Other functions
     */

    PGDLLEXPORT PG_FUNCTION_INFO_V1(parse_packed_phone_number);

    PGDLLEXPORT Datum
    parse_packed_phone_number(PG_FUNCTION_ARGS) {
        try {
            const text* number_text = PG_GETARG_TEXT_P(0);
            const text* country_text = PG_GETARG_TEXT_P(1);

            char* number_str = text_to_c_string(number_text);
            char* country = text_to_c_string(country_text);

            PackedPhoneNumber* number = do_parse_packed_phone_number(number_str, country);
            //TODO: prevent leaks.
            pfree(number_str);
            pfree(country);
            if(number) {
                PG_RETURN_POINTER(number);
            } else {
                PG_RETURN_NULL();
            }
        } catch(std::exception& e) {
            reportException(e);
            PG_RETURN_NULL();
        }
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_country_code);

    PGDLLEXPORT Datum
    packed_phone_number_country_code(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* number = (PackedPhoneNumber*)PG_GETARG_POINTER(0);

            PG_RETURN_INT32(number->country_code());
        } catch(std::exception& e) {
            reportException(e);
            PG_RETURN_NULL();
        }
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_region_code);

    PGDLLEXPORT Datum
    packed_phone_number_region_code(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* packed_number = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            PhoneNumber number = packed_phone_number_to_phone_number(packed_number);
            std::string region_code;

            phoneUtil->GetRegionCodeForNumber(number, &region_code);
            if(region_code.empty()) {
                PG_RETURN_NULL();
            }

            PG_RETURN_TEXT_P(string_to_text(region_code));
        } catch(std::exception& e) {
            reportException(e);
            PG_RETURN_NULL();
        }
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_geographical_area_code);

    PGDLLEXPORT Datum
    packed_phone_number_geographical_area_code(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* packed_number = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            PhoneNumber number = packed_phone_number_to_phone_number(packed_number);
            std::string area_code = get_geographical_area_code(number);

            if(area_code.empty()) {
                PG_RETURN_NULL();
            }

            PG_RETURN_TEXT_P(string_to_text(area_code));
        } catch(std::exception& e) {
            reportException(e);
            PG_RETURN_NULL();
        }
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_national_destination_code);

    PGDLLEXPORT Datum
    packed_phone_number_national_destination_code(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* packed_number = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            PhoneNumber number = packed_phone_number_to_phone_number(packed_number);
            std::string national_destination_code = get_national_destination_code(number);

            if(national_destination_code.empty()) {
                PG_RETURN_NULL();
            }

            PG_RETURN_TEXT_P(string_to_text(national_destination_code));
        } catch(std::exception& e) {
            reportException(e);
            PG_RETURN_NULL();
        }
    }

    PGDLLEXPORT PG_FUNCTION_INFO_V1(packed_phone_number_type);

    PGDLLEXPORT Datum
    packed_phone_number_type(PG_FUNCTION_ARGS) {
        try {
            const PackedPhoneNumber* packed_number = (PackedPhoneNumber*)PG_GETARG_POINTER(0);
            PhoneNumber number = packed_phone_number_to_phone_number(packed_number);
            const char* type_name = get_phone_number_type_name(phoneUtil->GetNumberType(number));

            PG_RETURN_TEXT_P(cstring_to_text(type_name));
        } catch(std::exception& e) {
            reportException(e);
            PG_RETURN_NULL();
        }
    }
}
