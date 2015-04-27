/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_EXCHANGE_CURRENCY_HPP
#define ABCD_EXCHANGE_CURRENCY_HPP

#include "../util/Status.hpp"
#include <set>

namespace abcd {

/**
 * The complete currency list, in a format the preprocessor can understand.
 * The `_` parameter is a macro that specifies
 * how to format each list item for the C++ compiler.
 *
 * Taken from https://en.wikipedia.org/wiki/ISO_4217 on 2015-04-21
 *
 * Currencies without exchange-rate sources are commented out
 * (see the exchange-validate cli command).
 *
 * Besides these currencies, BraveNewCoin returns some obsolete currencies
 * (EEK, LTL, LVL, MTL, RUR, SVC, ZMK) and one unofficial curency (JEP).
 */
#define ABC_CURRENCY_LIST(_) \
    _(AED, 784, "United Arab Emirates dirham") \
    _(AFN, 971, "Afghan afghani") \
    _(ALL,   8, "Albanian lek") \
    _(AMD,  51, "Armenian dram") \
    _(ANG, 532, "Netherlands Antillean guilder") \
    _(AOA, 973, "Angolan kwanza") \
    _(ARS,  32, "Argentine peso") \
    _(AUD,  36, "Australian dollar") \
    _(AWG, 533, "Aruban florin") \
    _(AZN, 944, "Azerbaijani manat") \
    _(BAM, 977, "Bosnia and Herzegovina convertible mark") \
    _(BBD,  52, "Barbados dollar") \
    _(BDT,  50, "Bangladeshi taka") \
    _(BGN, 975, "Bulgarian lev") \
    _(BHD,  48, "Bahraini dinar") \
    _(BIF, 108, "Burundian franc") \
    _(BMD,  60, "Bermudian dollar") \
    _(BND,  96, "Brunei dollar") \
    _(BOB,  68, "Boliviano") \
    /*_(BOV, 984, "Bolivian Mvdol (funds code)")*/ \
    _(BRL, 986, "Brazilian real") \
    _(BSD,  44, "Bahamian dollar") \
    _(BTN,  64, "Bhutanese ngultrum") \
    _(BWP,  72, "Botswana pula") \
    _(BYR, 974, "Belarusian ruble") \
    _(BZD,  84, "Belize dollar") \
    _(CAD, 124, "Canadian dollar") \
    _(CDF, 976, "Congolese franc") \
    /*_(CHE, 947, "WIR Euro (complementary currency)")*/ \
    _(CHF, 756, "Swiss franc") \
    /*_(CHW, 948, "WIR Franc (complementary currency)")*/ \
    _(CLF, 990, "Unidad de Fomento (funds code)") \
    _(CLP, 152, "Chilean peso") \
    _(CNY, 156, "Chinese yuan") \
    _(COP, 170, "Colombian peso") \
    /*_(COU, 970, "Unidad de Valor Real (UVR) (funds code)")*/ \
    _(CRC, 188, "Costa Rican colon") \
    _(CUC, 931, "Cuban convertible peso") \
    _(CUP, 192, "Cuban peso") \
    _(CVE, 132, "Cape Verde escudo") \
    _(CZK, 203, "Czech koruna") \
    _(DJF, 262, "Djiboutian franc") \
    _(DKK, 208, "Danish krone") \
    _(DOP, 214, "Dominican peso") \
    _(DZD,  12, "Algerian dinar") \
    _(EGP, 818, "Egyptian pound") \
    _(ERN, 232, "Eritrean nakfa") \
    _(ETB, 230, "Ethiopian birr") \
    _(EUR, 978, "Euro") \
    _(FJD, 242, "Fiji dollar") \
    _(FKP, 238, "Falkland Islands pound") \
    _(GBP, 826, "Pound sterling") \
    _(GEL, 981, "Georgian lari") \
    _(GHS, 936, "Ghanaian cedi") \
    _(GIP, 292, "Gibraltar pound") \
    _(GMD, 270, "Gambian dalasi") \
    _(GNF, 324, "Guinean franc") \
    _(GTQ, 320, "Guatemalan quetzal") \
    _(GYD, 328, "Guyanese dollar") \
    _(HKD, 344, "Hong Kong dollar") \
    _(HNL, 340, "Honduran lempira") \
    _(HRK, 191, "Croatian kuna") \
    _(HTG, 332, "Haitian gourde") \
    _(HUF, 348, "Hungarian forint") \
    _(IDR, 360, "Indonesian rupiah") \
    _(ILS, 376, "Israeli new shekel") \
    _(INR, 356, "Indian rupee") \
    _(IQD, 368, "Iraqi dinar") \
    _(IRR, 364, "Iranian rial") \
    _(ISK, 352, "Icelandic króna") \
    _(JMD, 388, "Jamaican dollar") \
    _(JOD, 400, "Jordanian dinar") \
    _(JPY, 392, "Japanese yen") \
    _(KES, 404, "Kenyan shilling") \
    _(KGS, 417, "Kyrgyzstani som") \
    _(KHR, 116, "Cambodian riel") \
    _(KMF, 174, "Comoro franc") \
    _(KPW, 408, "North Korean won") \
    _(KRW, 410, "South Korean won") \
    _(KWD, 414, "Kuwaiti dinar") \
    _(KYD, 136, "Cayman Islands dollar") \
    _(KZT, 398, "Kazakhstani tenge") \
    _(LAK, 418, "Lao kip") \
    _(LBP, 422, "Lebanese pound") \
    _(LKR, 144, "Sri Lankan rupee") \
    _(LRD, 430, "Liberian dollar") \
    _(LSL, 426, "Lesotho loti") \
    _(LYD, 434, "Libyan dinar") \
    _(MAD, 504, "Moroccan dirham") \
    _(MDL, 498, "Moldovan leu") \
    _(MGA, 969, "Malagasy ariary") \
    _(MKD, 807, "Macedonian denar") \
    _(MMK, 104, "Myanmar kyat") \
    _(MNT, 496, "Mongolian tugrik") \
    _(MOP, 446, "Macanese pataca") \
    _(MRO, 478, "Mauritanian ouguiya") \
    _(MUR, 480, "Mauritian rupee") \
    _(MVR, 462, "Maldivian rufiyaa") \
    _(MWK, 454, "Malawian kwacha") \
    _(MXN, 484, "Mexican peso") \
    /*_(MXV, 979, "Mexican Unidad de Inversion (UDI) (funds code)")*/ \
    _(MYR, 458, "Malaysian ringgit") \
    _(MZN, 943, "Mozambican metical") \
    _(NAD, 516, "Namibian dollar") \
    _(NGN, 566, "Nigerian naira") \
    _(NIO, 558, "Nicaraguan córdoba") \
    _(NOK, 578, "Norwegian krone") \
    _(NPR, 524, "Nepalese rupee") \
    _(NZD, 554, "New Zealand dollar") \
    _(OMR, 512, "Omani rial") \
    _(PAB, 590, "Panamanian balboa") \
    _(PEN, 604, "Peruvian nuevo sol") \
    _(PGK, 598, "Papua New Guinean kina") \
    _(PHP, 608, "Philippine peso") \
    _(PKR, 586, "Pakistani rupee") \
    _(PLN, 985, "Polish złoty") \
    _(PYG, 600, "Paraguayan guaraní") \
    _(QAR, 634, "Qatari riyal") \
    _(RON, 946, "Romanian new leu") \
    _(RSD, 941, "Serbian dinar") \
    _(RUB, 643, "Russian ruble") \
    _(RWF, 646, "Rwandan franc") \
    _(SAR, 682, "Saudi riyal") \
    _(SBD,  90, "Solomon Islands dollar") \
    _(SCR, 690, "Seychelles rupee") \
    _(SDG, 938, "Sudanese pound") \
    _(SEK, 752, "Swedish krona/kronor") \
    _(SGD, 702, "Singapore dollar") \
    _(SHP, 654, "Saint Helena pound") \
    _(SLL, 694, "Sierra Leonean leone") \
    _(SOS, 706, "Somali shilling") \
    _(SRD, 968, "Surinamese dollar") \
    /*_(SSP, 728, "South Sudanese pound")*/ \
    _(STD, 678, "São Tomé and Príncipe dobra") \
    _(SYP, 760, "Syrian pound") \
    _(SZL, 748, "Swazi lilangeni") \
    _(THB, 764, "Thai baht") \
    _(TJS, 972, "Tajikistani somoni") \
    _(TMT, 934, "Turkmenistani manat") \
    _(TND, 788, "Tunisian dinar") \
    _(TOP, 776, "Tongan paʻanga") \
    _(TRY, 949, "Turkish lira") \
    _(TTD, 780, "Trinidad and Tobago dollar") \
    _(TWD, 901, "New Taiwan dollar") \
    _(TZS, 834, "Tanzanian shilling") \
    _(UAH, 980, "Ukrainian hryvnia") \
    _(UGX, 800, "Ugandan shilling") \
    _(USD, 840, "United States dollar") \
    /*_(USN, 997, "United States dollar (next day) (funds code)")*/ \
    /*_(USS, 998, "United States dollar (same day) (funds code)")*/ \
    /*_(UYI, 940, "Uruguay Peso en Unidades Indexadas (URUIURUI) (funds code)")*/ \
    _(UYU, 858, "Uruguayan peso") \
    _(UZS, 860, "Uzbekistan som") \
    _(VEF, 937, "Venezuelan bolívar") \
    _(VND, 704, "Vietnamese dong") \
    _(VUV, 548, "Vanuatu vatu") \
    _(WST, 882, "Samoan tala") \
    _(XAF, 950, "CFA franc BEAC") \
    _(XAG, 961, "Silver (one troy ounce)") \
    _(XAU, 959, "Gold (one troy ounce)") \
    /*_(XBA, 955, "European Composite Unit (EURCO) (bond market unit)")*/ \
    /*_(XBB, 956, "European Monetary Unit (E.M.U.-6) (bond market unit)")*/ \
    /*_(XBC, 957, "European Unit of Account 9 (E.U.A.-9) (bond market unit)")*/ \
    /*_(XBD, 958, "European Unit of Account 17 (E.U.A.-17) (bond market unit)")*/ \
    _(XCD, 951, "East Caribbean dollar") \
    _(XDR, 960, "Special drawing rights") \
    /*_(XFU,   0, "UIC franc (special settlement currency)")*/ \
    _(XOF, 952, "CFA franc BCEAO") \
    /*_(XPD, 964, "Palladium (one troy ounce)")*/ \
    _(XPF, 953, "CFP franc (franc Pacifique)") \
    /*_(XPT, 962, "Platinum (one troy ounce)")*/ \
    /*_(XSU, 994, "SUCRE")*/ \
    /*_(XTS, 963, "Code reserved for testing purposes")*/ \
    /*_(XUA, 965, "ADB Unit of Account")*/ \
    /*_(XXX, 999, "No currency")*/ \
    _(YER, 886, "Yemeni rial") \
    _(ZAR, 710, "South African rand") \
    _(ZMW, 967, "Zambian kwacha") \
    _(ZWL, 932, "Zimbabwean dollar") \

/** Helper macro for Currency enum. */
#define ABC_CURRENCY_ENUM_ROW(code, number, name) code=number,

/**
 * ISO 4217 currency numbers.
 */
enum class Currency
{
    ABC_CURRENCY_LIST(ABC_CURRENCY_ENUM_ROW)
};

typedef std::set<Currency> Currencies;

/**
 * Looks up the number for an ISO 4217 currency code.
 */
Status
currencyNumber(Currency &result, const std::string &code);

/**
 * Looks up the code for an ISO 4217 currency number.
 */
Status
currencyCode(std::string &result, Currency number);

/**
 * Looks up English name for an ISO 4217 currency number.
 */
Status
currencyName(std::string &result, Currency number);

} // namespace abcd

#endif
