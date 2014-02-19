/**
 * @file
 * AirBitz Core API functions.
 *
 * This file contains all of the functions available in the AirBitz Core API.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <curl/curl.h>
#include "ABC_Debug.h"
#include "ABC.h"
#include "ABC_Account.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Wallet.h"
#include "ABC_Crypto.h"

static tABC_Currency gaCurrencies[] = {
    { "AED", 784, "United Arab Emirates dirham", "United Arab Emirates" },
    { "AFN", 971, "Afghan afghani", "Afghanistan" },
    { "ALL",   8, "Albanian lek", "Albania" },
    { "AMD",  51,  "Armenian dram", "Armenia" },
    { "ANG", 532, "Netherlands Antillean guilder", "Curaçao, Sint Maarten" },
    { "AOA", 973, "Angolan kwanza", "Angola" },
    { "ARS",  32, "Argentine peso", "Argentina" },
    { "AUD",  36, "Australian dollar", "Australia, Australian Antarctic Territory, Christmas Island, Cocos (Keeling) Islands, Heard and McDonald Islands, Kiribati, Nauru, Norfolk Island, Tuvalu" },
    { "AWG", 533, "Aruban florin", "Aruba" },
    { "AZN", 944, "Azerbaijani manat", "Azerbaijan" },
    { "BAM", 977, "Bosnia and Herzegovina convertible mark", "Bosnia and Herzegovina" },
    { "BBD",  52, "Barbados dollar", "Barbados" },
    { "BDT",  50, "Bangladeshi taka", "Bangladesh" },
    { "BGN", 975, "Bulgarian lev", "Bulgaria" },
    { "BHD",  48, "Bahraini dinar", "Bahrain" },
    { "BIF", 108, "Burundian franc", "Burundi" },
    { "BMD",  60, "Bermudian dollar", "Bermuda" },
    { "BND",  96, "Brunei dollar", "Brunei, Singapore" },
    { "BOB",  68, "Boliviano", "Bolivia" },
    { "BOV", 984, "Bolivian Mvdol (funds code)", "Bolivia" },
    { "BRL", 986, "Brazilian real", "Brazil" },
    { "BSD",  44, "Bahamian dollar", "Bahamas" },
    { "BTN",  64, "Bhutanese ngultrum", "Bhutan" },
    { "BWP",  72, "Botswana pula", "Botswana" },
    { "BYR", 974, "Belarusian ruble", "Belarus" },
    { "BZD",  84, "Belize dollar", "Belize" },
    { "CAD", 124, "Canadian dollar", "Canada, Saint Pierre and Miquelon" },
    { "CDF", 976, "Congolese franc", "Democratic Republic of Congo" },
    { "CHE", 947, "WIR Euro (complementary currency)", "Switzerland" },
    { "CHF", 756, "Swiss franc", "Switzerland, Liechtenstein" },
    { "CHW", 948, "WIR Franc (complementary currency)", "Switzerland" },
    { "CLF", 990, "Unidad de Fomento (funds code)", "Chile" },
    { "CLP", 152, "Chilean peso", "Chile" },
    { "CNY", 156, "Chinese yuan", "China" },
    { "COP", 170, "Colombian peso", "Colombia" },
    { "COU", 970, "[7]  Unidad de Valor Real (UVR) (funds code)[7]", "Colombia" },
    { "CRC", 188, "Costa Rican colon", "Costa Rica" },
    { "CUC", 931, "Cuban convertible peso", "Cuba" },
    { "CUP", 192, "Cuban peso", "Cuba" },
    { "CVE", 132, "Cape Verde escudo", "Cape Verde" },
    { "CZK", 203, "Czech koruna", "Czech Republic" },
    { "DJF", 262, "Djiboutian franc", "Djibouti" },
    { "DKK", 208, "Danish krone", "Denmark, Faroe Islands, Greenland" },
    { "DOP", 214, "Dominican peso", "Dominican Republic" },
    { "DZD",  12, "Algerian dinar", "Algeria" },
    { "EGP", 818, "Egyptian pound", "Egypt, Palestinian territories" },
    { "ERN", 232, "Eritrean nakfa", "Eritrea" },
    { "ETB", 230, "Ethiopian birr", "Ethiopia" },
    { "EUR", 978, "Euro", "Andorra, Austria, Belgium, Cyprus, Estonia, Finland, France, Germany, Greece, Ireland, Italy, Kosovo, Latvia, Luxembourg, Malta, Monaco, Montenegro, Netherlands, Portugal, San Marino, Slovakia, Slovenia, Spain, Vatican City; see eurozone" },
    { "FJD", 242, "Fiji dollar", "Fiji" },
    { "FKP", 238, "Falkland Islands pound", "Falkland Islands" },
    { "GBP", 826, "Pound sterling", "United Kingdom, British Crown dependencies (the  Isle of Man and the Channel Islands), certain British Overseas Territories ( South Georgia and the South Sandwich Islands, British Antarctic Territory and  British Indian Ocean Territory)" },
    { "GEL", 981, "Georgian lari", "Georgia (country)" },
    { "GHS", 936, "Ghanaian cedi", "Ghana" },
    { "GIP", 292, "Gibraltar pound", "Gibraltar" },
    { "GMD", 270, "Gambian dalasi", "Gambia" },
    { "GNF", 324, "Guinean franc", "Guinea" },
    { "GTQ", 320, "Guatemalan quetzal", "Guatemala" },
    { "GYD", 328, "Guyanese dollar", "Guyana" },
    { "HKD", 344, "Hong Kong dollar", "Hong Kong, Macao" },
    { "HNL", 340, "Honduran lempira", "Honduras" },
    { "HRK", 191, "Croatian kuna", "Croatia" },
    { "HTG", 332, "Haitian gourde", "Haiti" },
    { "HUF", 348, "Hungarian forint", "Hungary" },
    { "IDR", 360, "Indonesian rupiah", "Indonesia" },
    { "ILS", 376, "Israeli new shekel", "Israel, State of Palestine[8]" },
    { "INR", 356, "Indian rupee", "India" },
    { "IQD", 368, "Iraqi dinar", "Iraq" },
    { "IRR", 364, "Iranian rial", "Iran" },
    { "ISK", 352, "Icelandic króna", "Iceland" },
    { "JMD", 388, "Jamaican dollar", "Jamaica" },
    { "JOD", 400, "Jordanian dinar", "Jordan" },
    { "JPY", 392, "Japanese yen", "Japan" },
    { "KES", 404, "Kenyan shilling", "Kenya" },
    { "KGS", 417, "Kyrgyzstani som", "Kyrgyzstan" },
    { "KHR", 116, "Cambodian riel", "Cambodia" },
    { "KMF", 174, "Comoro franc", "Comoros" },
    { "KPW", 408, "North Korean won", "North Korea" },
    { "KRW", 410, "South Korean won", "South Korea" },
    { "KWD", 414, "Kuwaiti dinar", "Kuwait" },
    { "KYD", 136, "Cayman Islands dollar", "Cayman Islands" },
    { "KZT", 398, "Kazakhstani tenge", "Kazakhstan" },
    { "LAK", 418, "Lao kip", "Laos" },
    { "LBP", 422, "Lebanese pound", "Lebanon" },
    { "LKR", 144, "Sri Lankan rupee", "Sri Lanka" },
    { "LRD", 430, "Liberian dollar", "Liberia" },
    { "LSL", 426, "Lesotho loti", "Lesotho" },
    { "LTL", 440, "Lithuanian litas", "Lithuania" },
    { "LYD", 434, "Libyan dinar", "Libya" },
    { "MAD", 504, "Moroccan dirham", "Morocco" },
    { "MDL", 498, "Moldovan leu", "Moldova (except  Transnistria)" },
    { "MGA", 969, "*[9]  Malagasy ariary", "Madagascar" },
    { "MKD", 807, "Macedonian denar", "Macedonia" },
    { "MMK", 104, "Myanma kyat", "Myanmar" },
    { "MNT", 496, "Mongolian tugrik", "Mongolia" },
    { "MOP", 446, "Macanese pataca", "Macao" },
    { "MRO", 478, "*[9]  Mauritanian ouguiya", "Mauritania" },
    { "MUR", 480, "Mauritian rupee", "Mauritius" },
    { "MVR", 462, "Maldivian rufiyaa", "Maldives" },
    { "MWK", 454, "Malawian kwacha", "Malawi" },
    { "MXN", 484, "Mexican peso", "Mexico" },
    { "MXV", 979, "Mexican Unidad de Inversion (UDI) (funds code)", "Mexico" },
    { "MYR", 458, "Malaysian ringgit", "Malaysia" },
    { "MZN", 943, "Mozambican metical", "Mozambique" },
    { "NAD", 516, "Namibian dollar", "Namibia" },
    { "NGN", 566, "Nigerian naira", "Nigeria" },
    { "NIO", 558, "Nicaraguan córdoba", "Nicaragua" },
    { "NOK", 578, "Norwegian krone", "Norway, Svalbard, Jan Mayen, Bouvet Island, Queen Maud Land, Peter I Island" },
    { "NPR", 524, "Nepalese rupee", "Nepal" },
    { "NZD", 554, "New Zealand dollar", "Cook Islands, New Zealand, Niue, Pitcairn, Tokelau, Ross Dependency" },
    { "OMR", 512, "Omani rial", "Oman" },
    { "PAB", 590, "Panamanian balboa", "Panama" },
    { "PEN", 604, "Peruvian nuevo sol", "Peru" },
    { "PGK", 598, "Papua New Guinean kina", "Papua New Guinea" },
    { "PHP", 608, "Philippine peso", "Philippines" },
    { "PKR", 586, "Pakistani rupee", "Pakistan" },
    { "PLN", 985, "Polish złoty", "Poland" },
    { "PYG", 600, "Paraguayan guaraní", "Paraguay" },
    { "QAR", 634, "Qatari riyal", "Qatar" },
    { "RON", 946, "Romanian new leu", "Romania" },
    { "RSD", 941, "Serbian dinar", "Serbia" },
    { "RUB", 643, "Russian ruble", "Russia, Abkhazia, South Ossetia" },
    { "RWF", 646, "Rwandan franc", "Rwanda" },
    { "SAR", 682, "Saudi riyal", "Saudi Arabia" },
    { "SBD", 90,  "Solomon Islands dollar", "Solomon Islands" },
    { "SCR", 690, "Seychelles rupee", "Seychelles" },
    { "SDG", 938, "Sudanese pound", "Sudan" },
    { "SEK", 752, "Swedish krona/kronor", "Sweden" },
    { "SGD", 702, "Singapore dollar", "Singapore, Brunei" },
    { "SHP", 654, "Saint Helena pound", "Saint Helena" },
    { "SLL", 694, "Sierra Leonean leone", "Sierra Leone" },
    { "SOS", 706, "Somali shilling", "Somalia (except  Somaliland)" },
    { "SRD", 968, "Surinamese dollar", "Suriname" },
    { "SSP", 728, "South Sudanese pound", "South Sudan" },
    { "STD", 678, "São Tomé and Príncipe dobra", "São Tomé and Príncipe" },
    { "SYP", 760, "Syrian pound", "Syria" },
    { "SZL", 748, "Swazi lilangeni", "Swaziland" },
    { "THB", 764, "Thai baht", "Thailand" },
    { "TJS", 972, "Tajikistani somoni", "Tajikistan" },
    { "TMT", 934, "Turkmenistani manat", "Turkmenistan" },
    { "TND", 788, "Tunisian dinar", "Tunisia" },
    { "TOP", 776, "Tongan paʻanga", "Tonga" },
    { "TRY", 949, "Turkish lira", "Turkey, Northern Cyprus" },
    { "TTD", 780, "Trinidad and Tobago dollar", "Trinidad and Tobago" },
    { "TWD", 901, "New Taiwan dollar", "Republic of China (Taiwan)" },
    { "TZS", 834, "Tanzanian shilling", "Tanzania" },
    { "UAH", 980, "Ukrainian hryvnia", "Ukraine" },
    { "UGX", 800, "Ugandan shilling", "Uganda" },
    { "USD", 840, "United States dollar", "American Samoa, Barbados (as well as Barbados Dollar), Bermuda (as well as Bermudian Dollar), British Indian Ocean Territory, British Virgin Islands, Caribbean Netherlands, Ecuador, El Salvador, Guam, Haiti, Marshall Islands, Federated States of Micronesia, Northern Mariana Islands, Palau, Panama, Puerto Rico, Timor-Leste, Turks and Caicos Islands, United States, U.S. Virgin Islands, Zimbabwe" },
    { "USN", 997, "United States dollar (next day) (funds code)", "United States" },
    { "USS", 998, "United States dollar (same day) (funds code)[10]", "United States" },
    { "UYI", 940, "Uruguay Peso en Unidades Indexadas (URUIURUI) (funds code)", "Uruguay" },
    { "UYU", 858, "Uruguayan peso", "Uruguay" },
    { "UZS", 860, "Uzbekistan som", "Uzbekistan" },
    { "VEF", 937, "Venezuelan bolívar", "Venezuela" },
    { "VND", 704, "Vietnamese dong", "Vietnam" },
    { "VUV", 548, "Vanuatu vatu", "Vanuatu" },
    { "WST", 882, "Samoan tala", "Samoa" },
    { "XAF", 950, "CFA franc BEAC", "Cameroon, Central African Republic, Republic of the Congo, Chad, Equatorial Guinea, Gabon" },
    { "XAG", 961, "Silver (one troy ounce)", "World-wide" },
    { "XAU", 959, "Gold (one troy ounce)", "World-wide" },
    { "XBA", 955, "European Composite Unit (EURCO) (bond market unit)", "World-wide" },
    { "XBB", 956, "European Monetary Unit (E.M.U.-6) (bond market unit)", "World-wide" },
    { "XBC", 957, "European Unit of Account 9 (E.U.A.-9) (bond market unit)", "World-wide" },
    { "XBD", 958, "European Unit of Account 17 (E.U.A.-17) (bond market unit)", "World-wide" },
    { "XCD", 951, "East Caribbean dollar", "Anguilla, Antigua and Barbuda, Dominica, Grenada, Montserrat, Saint Kitts and Nevis, Saint Lucia, Saint Vincent and the Grenadines" },
    { "XDR", 960, "Special drawing rights", "International Monetary Fund" },
    { "XFU",   0, "UIC franc (special settlement currency)", "International Union of Railways" },
    { "XOF", 952, "CFA franc BCEAO   Benin, Burkina Faso, Côte d'Ivoire, Guinea-Bissau, Mali, Niger, Senegal, Togo" },
    { "XPD", 964, "Palladium (one troy ounce)", "World-wide" },
    { "XPF", 953, "CFP franc (franc Pacifique)", "French territories of the Pacific Ocean:  French Polynesia, New Caledonia, Wallis and Futuna" },
    { "XPT", 962, "Platinum (one troy ounce)", "World-wide" },
    { "XTS", 963, "Code reserved for testing purposes", "World-wide" },
    { "XXX", 999, "No currency", "N/A" },
    { "YER", 886, "Yemeni rial", "Yemen" },
    { "ZAR", 710, "South African rand", "South Africa" },
    { "ZMW", 967, "Zambian kwacha", "Zambia" },
    { "ZWL", 932, "Zimbabwe dollar", "Zimbabwe" }
};
#define CURRENCY_ARRAY_COUNT ((int) (sizeof(gaCurrencies) / sizeof(gaCurrencies[0])))


/** globally accessable function pointer for BitCoin event callbacks */
static tABC_BitCoin_Event_Callback gfAsyncBitCoinEventCallback = NULL;
static void *pAsyncBitCoinCallerData = NULL;

/**
 * Initialize the AirBitz Core library.
 *
 * The root directory for all file storage is set in this function.
 *
 * @param szRootDir                     The root directory for all files to be saved
 * @param fAsyncBitCoinEventCallback    The function that should be called when there is an asynchronous
 *                                      BitCoin event
 * @param pData                         Pointer to data to be returned back in callback
 * @param pSeedData                     Pointer to data to seed the random number generator
 * @param seedLength                    Length of the seed data
 * @param pError                        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_Initialize(const char                   *szRootDir,
                       tABC_BitCoin_Event_Callback  fAsyncBitCoinEventCallback,
                       void                         *pData,
                       const unsigned char          *pSeedData,
                       unsigned int                 seedLength,
                       tABC_Error                   *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf Seed = ABC_BUF_NULL;

    ABC_CHECK_NULL(szRootDir);
    ABC_CHECK_NULL(pSeedData);

    // initialize curl
    CURLcode curlCode;
    if ((curlCode = curl_global_init(CURL_GLOBAL_ALL)) != 0)
    {
        ABC_DebugLog("Curl init failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_CurlError, "Curl init failed");
    }

    gfAsyncBitCoinEventCallback = fAsyncBitCoinEventCallback;
    pAsyncBitCoinCallerData = pData;

    if (szRootDir)
    {
        ABC_CHECK_RET(ABC_FileIOSetRootDir(szRootDir, pError));
    }


    ABC_BUF_DUP_PTR(Seed, pSeedData, seedLength);
    ABC_CHECK_RET(ABC_CryptoSetRandomSeed(Seed, pError));

exit:
    ABC_BUF_FREE(Seed);

    return cc;
}

/**
 * Mark the end of use of the AirBitz Core library.
 *
 * This function is the counter to ABC_Initialize.
 * It should be called when all use of the library is complete.
 *
 */
void ABC_Terminate()
{
    ABC_ClearKeyCache(NULL);

    // cleanup curl
    curl_global_cleanup();
}

/**
 * Create a new account.
 *
 * This function kicks off a thread to signin to an account. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param fRequestCallback          The function that will be called when the account create process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SignIn(const char *szUserName,
                   const char *szPassword,
                   tABC_Request_Callback fRequestCallback,
                   void *pData,
                   tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;

    tABC_AccountSignInInfo *pAccountSignInInfo = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_AccountSignInInfoAlloc(&pAccountSignInInfo,
                                             szUserName,
                                             szPassword,
                                             fRequestCallback,
                                             pData,
                                             pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountSignInThreaded, pAccountSignInInfo))
    {
        //printf("Thread create successfully !!!\n");
        if ( ! pthread_detach(handle) )
        {
            //printf("Thread detached successfully !!!\n");
        }
    }

exit:

    return cc;
}

/**
 * Create a new account.
 *
 * This function kicks off a thread to create a new account. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szPIN                     PIN for the account
 * @param fRequestCallback          The function that will be called when the account create process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateAccount(const char *szUserName,
                          const char *szPassword,
                          const char *szPIN,
                          tABC_Request_Callback fRequestCallback,
                          void *pData,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;

    tABC_AccountCreateInfo *pAccountCreateInfo = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szPIN);
    ABC_CHECK_ASSERT(strlen(szPIN) > 0, ABC_CC_Error, "No PIN provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_AccountCreateInfoAlloc(&pAccountCreateInfo,
                                             szUserName,
                                             szPassword,
                                             szPIN,
                                             fRequestCallback,
                                             pData,
                                             pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountCreateThreaded, pAccountCreateInfo))
    {
        //printf("Thread create successfully !!!\n");
        if ( ! pthread_detach(handle) )
        {
            //printf("Thread detached successfully !!!\n");
        }
    }

exit:

    return cc;
}

/**
 * Set the recovery questions for an account
 *
 * This function kicks off a thread to set the recovery questions for an account.
 * The callback will be called when it has finished.
 *
 * @param szUserName                UserName of the account
 * @param szPassword                Password of the account
 * @param szRecoveryQuestions       Recovery questions - newline seperated
 * @param szRecoveryAnswers         Recovery answers - newline seperated
 * @param fRequestCallback          The function that will be called when the account create process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetAccountRecoveryQuestions(const char *szUserName,
                                        const char *szPassword,
                                        const char *szRecoveryQuestions,
                                        const char *szRecoveryAnswers,
                                        tABC_Request_Callback fRequestCallback,
                                        void *pData,
                                        tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;

    tABC_AccountSetRecoveryInfo *pInfo = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szRecoveryQuestions);
    ABC_CHECK_ASSERT(strlen(szRecoveryQuestions) > 0, ABC_CC_Error, "No recovery questions provided");
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_ASSERT(strlen(szRecoveryAnswers) > 0, ABC_CC_Error, "No recovery answers provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_AccountSetRecoveryInfoAlloc(&pInfo,
                                                  szUserName,
                                                  szPassword,
                                                  szRecoveryQuestions,
                                                  szRecoveryAnswers,
                                                  fRequestCallback,
                                                  pData,
                                                  pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountSetRecoveryThreaded, pInfo))
    {
        //printf("Thread create successfully !!!\n");
        if ( ! pthread_detach(handle) )
        {
            //printf("Thread detached successfully !!!\n");
        }
    }

exit:

    return cc;
}


/**
 * Create a new wallet.
 *
 * This function kicks off a thread to create a new wallet. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szWalletName              Wallet Name
 * @param currencyNum               ISO 4217 currency number
 * @param attributes                Attributes to be used for filtering (e.g., archive bit)
 * @param fRequestCallback          The function that will be called when the wallet create process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateWallet(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletName,
                         int        currencyNum,
                         unsigned int attributes,
                         tABC_Request_Callback fRequestCallback,
                         void *pData,
                         tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;

    tABC_WalletCreateInfo *pWalletCreateInfo = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletName);
    ABC_CHECK_ASSERT(strlen(szWalletName) > 0, ABC_CC_Error, "No wallet name provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_WalletCreateInfoAlloc(&pWalletCreateInfo,
                                            szUserName,
                                            szPassword,
                                            szWalletName,
                                            currencyNum,
                                            attributes,
                                            fRequestCallback,
                                            pData,
                                            pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_WalletCreateThreaded, pWalletCreateInfo))
    {
        //printf("Thread create successfully !!!\n");
        if ( ! pthread_detach(handle) )
        {
            //printf("Thread detached successfully !!!\n");
        }
    }

exit:

    return cc;
}


/**
 * Clear cached keys.
 *
 * This function clears any keys that might be cached.
 *
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ClearKeyCache(tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_AccountClearKeyCache(pError));

    ABC_CHECK_RET(ABC_WalletClearCache(pError));

exit:

    return cc;
}

/**
 * Create a new wallet.
 *
 * This function provides the array of currencies.
 * The array returned should not be modified and should not be deallocated.
 *
 * @param paCurrencyArray           Pointer in which to store the currency array
 * @param pCount                    Pointer in which to store the count of array entries
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetCurrencies(tABC_Currency **paCurrencyArray,
                          int *pCount,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(paCurrencyArray);
    ABC_CHECK_NULL(pCount);

    *paCurrencyArray = gaCurrencies;
    *pCount = CURRENCY_ARRAY_COUNT;

exit:

    return cc;
}

/**
 * Get a PIN number.
 *
 * This function retrieves the PIN for a given account.
 * The string is allocated and must be free'd by the caller.
 *
 * @param szUserName             UserName for the account
 * @param szPassword             Password for the account
 * @param pszPIN                 Pointer where to store allocated PIN string
 * @param pError                 A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetPIN(const char *szUserName,
                   const char *szPassword,
                   char **pszPIN,
                   tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(pszPIN);

    tABC_U08Buf PIN;
    ABC_CHECK_RET(ABC_AccountGetKey(szUserName, szPassword, ABC_AccountKey_PIN, &PIN, pError));

    *pszPIN = strdup((char *)ABC_BUF_PTR(PIN));

exit:

    return cc;
}

/**
 * Set PIN number for an account.
 *
 * This function sets the PIN for a given account.
 *
 * @param szUserName            UserName for the account
 * @param szPassword            Password for the account
 * @param szPIN                 PIN string
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetPIN(const char *szUserName,
                   const char *szPassword,
                   const char *szPIN,
                   tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szPIN);

    ABC_CHECK_RET(ABC_AccountSetPIN(szUserName, szPassword, szPIN, pError));

exit:

    return cc;
}

/**
 * Get the categories for an account.
 *
 * This function gets the categories for an account.
 * An array of allocated strings is allocated so the user is responsible for
 * free'ing all the elements as well as the array itself.
 *
 * @param szUserName            UserName for the account
 * @param paszCategories        Pointer to store results (NULL is stored if no categories)
 * @param pCount                Pointer to store result count (can be 0)
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetCategories(const char *szUserName,
                          char ***paszCategories,
                          unsigned int *pCount,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    return ABC_AccountGetCategories(szUserName, paszCategories, pCount, pError);
}

/**
 * Add a category for an account.
 *
 * This function adds a category to an account.
 * No attempt is made to avoid a duplicate entry.
 *
 * @param szUserName            UserName for the account
 * @param szCategory            Category to add
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AddCategory(const char *szUserName,
                        char *szCategory,
                        tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    return ABC_AccountAddCategory(szUserName, szCategory, pError);
}

/**
 * Remove a category from an account.
 *
 * This function removes a category from an account.
 * If there is more than one category with this name, all categories by this name are removed.
 * If the category does not exist, no error is returned.
 *
 * @param szUserName            UserName for the account
 * @param szCategory            Category to remove
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_RemoveCategory(const char *szUserName,
                           char *szCategory,
                           tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    return ABC_AccountRemoveCategory(szUserName, szCategory, pError);
}

/**
 * Renames a wallet.
 *
 * This function renames the wallet of a given UUID.
 * If there is more than one category with this name, all categories by this name are removed.
 * If the category does not exist, no error is returned.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param szUUID                UUID of the wallet
 * @param szNewWalletName       New name for the wallet
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_RenameWallet(const char *szUserName,
                         const char *szPassword,
                         const char *szUUID,
                         const char *szNewWalletName,
                         tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    return ABC_WalletSetName(szUserName, szPassword, szUUID, szNewWalletName, pError);
}

/**
 * Sets the attributes on a wallet.
 *
 * This function sets the attributes on a given wallet to those given.
 * The attributes would have already been set when the wallet was created so this would allow them
 * to be changed.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param szUUID                UUID of the wallet
 * @param attributes            Attributes to be used for filtering (e.g., archive bit)
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetWalletAttributes(const char *szUserName,
                                const char *szPassword,
                                const char *szUUID,
                                unsigned int attributes,
                                tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    return ABC_WalletSetAttributes(szUserName, szPassword, szUUID, attributes, pError);
}

/**
 * Checks the validity of the given account answers.
 *
 * This function sets the attributes on a given wallet to those given.
 * The attributes would have already been set when the wallet was created so this would allow them
 * to be changed.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szRecoveryAnswers     Recovery answers - newline seperated
 * @param pbValid               Pointer to boolean into which to store result
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CheckRecoveryAnswers(const char *szUserName,
                                 const char *szRecoveryAnswers,
                                 bool *pbValid,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    return ABC_AccountCheckRecoveryAnswers(szUserName, szRecoveryAnswers, pbValid, pError);
}

void tempEventA()
{
    if (gfAsyncBitCoinEventCallback)
    {
        tABC_AsyncBitCoinInfo info;
        info.pData = pAsyncBitCoinCallerData;
        strcpy(info.szDescription, "Event A");
        gfAsyncBitCoinEventCallback(&info);
    }
}

void tempEventB()
{
    if (gfAsyncBitCoinEventCallback)
    {
        tABC_AsyncBitCoinInfo info;
        strcpy(info.szDescription, "Event B");
        info.pData = pAsyncBitCoinCallerData;
        gfAsyncBitCoinEventCallback(&info);
    }
}
