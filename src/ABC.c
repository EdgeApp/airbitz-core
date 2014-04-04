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
#include <ctype.h>
#include <pthread.h>
#include <jansson.h>
#include <math.h>
#include "ABC_Debug.h"
#include "ABC.h"
#include "ABC_Account.h"
#include "ABC_Bridge.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Wallet.h"
#include "ABC_Crypto.h"
#include "ABC_URL.h"
#include "ABC_Mutex.h"
#include "ABC_Tx.h"

static bool gbInitialized = false;

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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Seed = ABC_BUF_NULL;

    ABC_CHECK_NULL(szRootDir);
    ABC_CHECK_NULL(pSeedData);
    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "The core library has already been initalized");

    // override the alloc and free of janson so we can have a secure method
    json_set_alloc_funcs(ABC_UtilJanssonSecureMalloc, ABC_UtilJanssonSecureFree);

    // initialize the mutex system
    ABC_CHECK_RET(ABC_MutexInitialize(pError));

    // initialize URL system
    ABC_CHECK_RET(ABC_URLInitialize(pError));

    // initialize the FileIO system
    ABC_CHECK_RET(ABC_FileIOInitialize(pError));

    // initialize Bitcoin transaction system
    ABC_CHECK_RET(ABC_TxInitialize(fAsyncBitCoinEventCallback, pData, pError));

    if (szRootDir)
    {
        ABC_CHECK_RET(ABC_FileIOSetRootDir(szRootDir, pError));
    }

    ABC_BUF_DUP_PTR(Seed, pSeedData, seedLength);
    ABC_CHECK_RET(ABC_CryptoSetRandomSeed(Seed, pError));

    gbInitialized = true;

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
    if (gbInitialized == true)
    {
        ABC_ClearKeyCache(NULL);

        ABC_URLTerminate();

        ABC_FileIOTerminate();

        ABC_MutexTerminate();

        gbInitialized = false;
    }
}

/**
 * Create a new account.
 *
 * This function kicks off a thread to signin to an account. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param fRequestCallback          The function that will be called when the account signin process has finished.
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_AccountRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_AccountRequestInfoAlloc(&pAccountRequestInfo,
                                              ABC_RequestType_AccountSignIn,
                                              szUserName,
                                              szPassword,
                                              NULL, // recovery questions
                                              NULL, // recovery answers
                                              NULL, // PIN
                                              NULL, // new password
                                              fRequestCallback,
                                              pData,
                                              pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountRequestThreaded, pAccountRequestInfo))
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_AccountRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szPIN);
    ABC_CHECK_ASSERT(strlen(szPIN) > 0, ABC_CC_Error, "No PIN provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_AccountRequestInfoAlloc(&pAccountRequestInfo,
                                              ABC_RequestType_CreateAccount,
                                              szUserName,
                                              szPassword,
                                              NULL, // recovery questions
                                              NULL, // recovery answers
                                              szPIN,
                                              NULL, // new password
                                              fRequestCallback,
                                              pData,
                                              pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountRequestThreaded, pAccountRequestInfo))
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
 * @param fRequestCallback          The function that will be called when the recovery questions are ready.
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_AccountRequestInfo *pInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szRecoveryQuestions);
    ABC_CHECK_ASSERT(strlen(szRecoveryQuestions) > 0, ABC_CC_Error, "No recovery questions provided");
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_ASSERT(strlen(szRecoveryAnswers) > 0, ABC_CC_Error, "No recovery answers provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_AccountRequestInfoAlloc(&pInfo,
                                              ABC_RequestType_SetAccountRecoveryQuestions,
                                              szUserName,
                                              szPassword,
                                              szRecoveryQuestions,
                                              szRecoveryAnswers,
                                              NULL, // PIN
                                              NULL, // new password
                                              fRequestCallback,
                                              pData,
                                              pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountRequestThreaded, pInfo))
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
 * The UUID of the new wallet will be provided in the callback pRetData as a (char *).
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_WalletCreateInfo *pWalletCreateInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(pszPIN);

    tABC_U08Buf PIN;
    ABC_CHECK_RET(ABC_AccountGetKey(szUserName, szPassword, ABC_AccountKey_PIN, &PIN, pError));

    ABC_STRDUP(*pszPIN, (char *)ABC_BUF_PTR(PIN));

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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
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

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_AccountGetCategories(szUserName, paszCategories, pCount, pError));

exit:

    return cc;
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

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_AccountAddCategory(szUserName, szCategory, pError));

exit:

    return cc;
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

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_AccountRemoveCategory(szUserName, szCategory, pError));

exit:

    return cc;
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

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_WalletSetName(szUserName, szPassword, szUUID, szNewWalletName, pError));

exit:

    return cc;
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

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_WalletSetAttributes(szUserName, szPassword, szUUID, attributes, pError));

exit:

    return cc;
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

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_AccountCheckRecoveryAnswers(szUserName, szRecoveryAnswers, pbValid, pError));

exit:

    return cc;
}

/**
 * Gets information on the given wallet.
 *
 * This function allocates and fills in a wallet info structure with the information
 * associated with the given wallet UUID
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param szUUID                UUID of the wallet
 * @param ppWalletInfo          Pointer to store the pointer of the allocated wallet info struct
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetWalletInfo(const char *szUserName,
                          const char *szPassword,
                          const char *szUUID,
                          tABC_WalletInfo **ppWalletInfo,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_WalletGetInfo(szUserName, szPassword, szUUID, ppWalletInfo, pError));

exit:

    return cc;
}

/**
 * Free the wallet info.
 *
 * This function frees the wallet info struct returned from ABC_GetWalletInfo.
 *
 * @param pWalletInfo   Wallet info to be free'd
 */
void ABC_FreeWalletInfo(tABC_WalletInfo *pWalletInfo)

{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_WalletFreeInfo(pWalletInfo);
}

/**
 * Gets wallets for a specified account.
 *
 * This function allocates and fills in an array of wallet info structures with the information
 * associated with the wallets of the given user
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param paWalletInfo          Pointer to store the allocated array of wallet info structs
 * @param pCount                Pointer to store number of wallets in the array
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetWallets(const char *szUserName,
                       const char *szPassword,
                       tABC_WalletInfo ***paWalletInfo,
                       unsigned int *pCount,
                       tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_WalletGetWallets(szUserName, szPassword, paWalletInfo, pCount, pError));

exit:

    return cc;
}

/**
 * Free the wallet info array.
 *
 * This function frees the wallet info array returned from ABC_GetWallets.
 *
 * @param aWalletInfo   Wallet info array to be free'd
 * @param nCount        Number of elements in the array
 */
void ABC_FreeWalletInfoArray(tABC_WalletInfo **aWalletInfo,
                             unsigned int nCount)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_WalletFreeInfoArray(aWalletInfo, nCount);
}

/**
 * Set the wallet order for a specified account.
 *
 * This function sets the order of the wallets for an account to the order in the given
 * array.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param aszUUIDArray          Array of UUID strings
 * @param countUUIDs            Number of UUID's in aszUUIDArray
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetWalletOrder(const char *szUserName,
                           const char *szPassword,
                           char **aszUUIDArray,
                           unsigned int countUUIDs,
                           tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_WalletSetOrder(szUserName, szPassword, aszUUIDArray, countUUIDs, pError));

exit:

    return cc;
}

/**
 * Get the recovery question choices.
 *
 * This function kicks off a thread to get the recovery question choices. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param fRequestCallback          The function that will be called when the recovery question choices are ready.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetQuestionChoices(const char *szUserName,
                               tABC_Request_Callback fRequestCallback,
                               void *pData,
                               tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_AccountRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_AccountRequestInfoAlloc(&pAccountRequestInfo,
                                              ABC_RequestType_GetQuestionChoices,
                                              szUserName,
                                              NULL, // password
                                              NULL, // recovery questions
                                              NULL, // recovery answers
                                              NULL, // PIN
                                              NULL, // new password
                                              fRequestCallback,
                                              pData,
                                              pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountRequestThreaded, pAccountRequestInfo))
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
 * Free question choices.
 *
 * This function frees the question choices given
 *
 * @param pQuestionChoices  Pointer to question choices to free.
 */
void ABC_FreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_AccountFreeQuestionChoices(pQuestionChoices);
}

/**
 * Get the recovery questions for a given account.
 *
 * The questions will be returned in a single allocated string with
 * each questions seperated by a newline.
 *
 * @param szUserName                UserName for the account
 * @param pszQuestions              Pointer into which allocated string should be stored.
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetRecoveryQuestions(const char *szUserName,
                                 char **pszQuestions,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(pszQuestions);

    ABC_CHECK_RET(ABC_AccountGetRecoveryQuestions(szUserName, pszQuestions, pError));

exit:

    return cc;
}

/**
 * Change account password.
 *
 * This function kicks off a thread to change the password for an account.
 * The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szNewPassword             New Password for the account
 * @param szNewPIN                  New PIN for the account
 * @param fRequestCallback          The function that will be called when the password change has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ChangePassword(const char *szUserName,
                           const char *szPassword,
                           const char *szNewPassword,
                           const char *szNewPIN,
                           tABC_Request_Callback fRequestCallback,
                           void *pData,
                           tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_AccountRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szNewPassword);
    ABC_CHECK_ASSERT(strlen(szNewPassword) > 0, ABC_CC_Error, "No new password provided");
    ABC_CHECK_NULL(szNewPIN);
    ABC_CHECK_ASSERT(strlen(szNewPIN) > 0, ABC_CC_Error, "No new PIN provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_AccountRequestInfoAlloc(&pAccountRequestInfo,
                                              ABC_RequestType_ChangePassword,
                                              szUserName,
                                              szPassword,
                                              NULL, // recovery questions
                                              NULL, // recovery answers
                                              szNewPIN,
                                              szNewPassword,
                                              fRequestCallback,
                                              pData,
                                              pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountRequestThreaded, pAccountRequestInfo))
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
 * Change account password using recovery answers.
 *
 * This function kicks off a thread to change the password for an account using the
 * recovery answers as account validation.
 * The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szRecoveryAnswers         Recovery answers (each answer seperated by a newline)
 * @param szNewPassword             New Password for the account
 * @param szNewPIN                  New PIN for the account
 * @param fRequestCallback          The function that will be called when the password change has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ChangePasswordWithRecoveryAnswers(const char *szUserName,
                                              const char *szRecoveryAnswers,
                                              const char *szNewPassword,
                                              const char *szNewPIN,
                                              tABC_Request_Callback fRequestCallback,
                                              void *pData,
                                              tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_AccountRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_ASSERT(strlen(szRecoveryAnswers) > 0, ABC_CC_Error, "No recovery answers provided");
    ABC_CHECK_NULL(szNewPassword);
    ABC_CHECK_ASSERT(strlen(szNewPassword) > 0, ABC_CC_Error, "No new password provided");
    ABC_CHECK_NULL(szNewPIN);
    ABC_CHECK_ASSERT(strlen(szNewPIN) > 0, ABC_CC_Error, "No new PIN provided");
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_AccountRequestInfoAlloc(&pAccountRequestInfo,
                                              ABC_RequestType_ChangePassword,
                                              szUserName,
                                              NULL, // recovery questions
                                              NULL, // password
                                              szRecoveryAnswers,
                                              szNewPIN,
                                              szNewPassword,
                                              fRequestCallback,
                                              pData,
                                              pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountRequestThreaded, pAccountRequestInfo))
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
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param szURI     URI to parse
 * @param ppInfo    Pointer to location to store allocated info struct.
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ParseBitcoinURI(const char *szURI,
                            tABC_BitcoinURIInfo **ppInfo,
                            tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_BridgeParseBitcoinURI(szURI, ppInfo, pError));

exit:

    return cc;
}

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param pInfo Pointer to allocated info struct.
 */
void ABC_FreeURIInfo(tABC_BitcoinURIInfo *pInfo)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_BridgeFreeURIInfo(pInfo);
}

/**
 * Converts amount from Satoshi to Bitcoin
 *
 * @param satoshi Amount in Satoshi
 */
double ABC_SatoshiToBitcoin(int64_t satoshi)
{
    return(ABC_TxSatoshiToBitcoin(satoshi));
}

/**
 * Converts amount from Bitcoin to Satoshi
 *
 * @param bitcoin Amount in Bitcoin
 */
int64_t ABC_BitcoinToSatoshi(double bitcoin)
{
    return(ABC_TxBitcoinToSatoshi(bitcoin));
}

/**
 * Converts Satoshi to given currency
 *
 * @param satoshi     Amount in Satoshi
 * @param pCurrency   Pointer to location to store amount converted to currency.
 * @param currencyNum Currency ISO 4217 num
 * @param pError      A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SatoshiToCurrency(int64_t satoshi,
                              double *pCurrency,
                              int currencyNum,
                              tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxSatoshiToCurrency(satoshi, pCurrency, currencyNum, pError));

exit:

    return cc;
}

/**
 * Converts given currency to Satoshi
 *
 * @param currency    Amount in given currency
 * @param currencyNum Currency ISO 4217 num
 * @param pSatoshi    Pointer to location to store amount converted to Satoshi
 * @param pError      A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CurrencyToSatoshi(double currency,
                              int currencyNum,
                              int64_t *pSatoshi,
                              tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxCurrencyToSatoshi(currency, currencyNum, pSatoshi, pError));

exit:

    return cc;
}

/**
 * Creates a receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param pDetails      Pointer to transaction details
 * @param pszRequestID  Pointer to store allocated ID for this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 tABC_TxDetails *pDetails,
                                 char **pszRequestID,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxCreateReceiveRequest(szUserName, szPassword, szWalletUUID, pDetails, pszRequestID, pError));

exit:

    return cc;
}

/**
 * Modifies a previously created receive request.
 * Note: the previous details will be free'ed so if the user is using the previous details for this request
 * they should not assume they will be valid after this call.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pDetails      Pointer to transaction details
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ModifyReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szRequestID,
                                 tABC_TxDetails *pDetails,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxModifyReceiveRequest(szUserName, szPassword, szWalletUUID, szRequestID, pDetails, pError));

exit:

    return cc;
}

/**
 * Finalizes a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_FinalizeReceiveRequest(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   const char *szRequestID,
                                   tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxFinalizeReceiveRequest(szUserName, szPassword, szWalletUUID, szRequestID, pError));
exit:

    return cc;
}

/**
 * Cancels a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CancelReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szRequestID,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxCancelReceiveRequest(szUserName, szPassword, szWalletUUID, szRequestID, pError));

exit:

    return cc;
}

/**
 * Generate the QR code for a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param paData        Pointer to store array of data bytes (0x0 white, 0x1 black)
 * @param pWidth        Pointer to store width of image (image will be square)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GenerateRequestQRCode(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  const char *szRequestID,
                                  unsigned char **paData,
                                  unsigned int *pWidth,
                                  tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGenerateRequestQRCode(szUserName, szPassword, szWalletUUID, szRequestID, paData, pWidth, pError));

exit:
    return cc;
}

/**
 * Initiates a send request.
 *
 * Once the given send has been submitted to the block chain, the given callback will
 * be called and the results data will have a pointer to the request id
 *
 * @param szUserName        UserName for the account associated with this request
 * @param szPassword        Password for the account associated with this request
 * @param szWalletUUID      UUID of the wallet associated with this request
 * @param szDestAddress     Bitcoin address (Base58) to which the funds are to be sent
 * @param pDetails          Pointer to transaction details
 * @param fRequestCallback  The function that will be called when the send request process has finished.
 * @param pData             Pointer to data to be returned back in callback
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_InitiateSendRequest(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szDestAddress,
                                tABC_TxDetails *pDetails,
                                tABC_Request_Callback fRequestCallback,
                                void *pData,
                                tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxSendInfo *pTxSendInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet name provided");
    ABC_CHECK_NULL(pDetails);
    ABC_CHECK_NULL(fRequestCallback);

    ABC_CHECK_RET(ABC_TxSendInfoAlloc(&pTxSendInfo,
                                            szUserName,
                                            szPassword,
                                            szWalletUUID,
                                            szDestAddress,
                                            pDetails,
                                            fRequestCallback,
                                            pData,
                                            pError));

    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_TxSendThreaded, pTxSendInfo))
    {
        if ( ! pthread_detach(handle) )
        {
            //printf("Thread detached successfully !!!\n");
        }
    }

exit:

    return cc;
}

/**
 * Gets the transactions associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetTransactions(const char *szUserName,
                            const char *szPassword,
                            const char *szWalletUUID,
                            tABC_TxInfo ***paTransactions,
                            unsigned int *pCount,
                            tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGetTransactions(szUserName, szPassword, szWalletUUID, paTransactions, pCount, pError));

exit:
    return cc;
}

/**
 * Frees the given array of transactions
 *
 * @param aTransactions Array of transactions
 * @param count         Number of transactions
 */
void ABC_FreeTransactions(tABC_TxInfo **aTransactions,
                            unsigned int count)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeTransactions(aTransactions, count);
}

/**
 * Sets the details for a specific transaction.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param pDetails          Details for the transaction
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetTransactionDetails(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  const char *szID,
                                  tABC_TxDetails *pDetails,
                                  tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxSetTransactionDetails(szUserName, szPassword, szWalletUUID, szID, pDetails, pError));

exit:
    return cc;
}

/**
 * Gets the details for a specific existing transaction.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param ppDetails         Location to store allocated details for the transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetTransactionDetails(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szID,
                                    tABC_TxDetails **ppDetails,
                                    tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGetTransactionDetails(szUserName, szPassword, szWalletUUID, szID, ppDetails, pError));

exit:
    return cc;
}

/**
 * Gets the pending requests associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the requests
 * @param szPassword        Password for the account associated with the requests
 * @param szWalletUUID      UUID of the wallet associated with the requests
 * @param paTransactions    Pointer to store array of requests info pointers
 * @param pCount            Pointer to store number of requests
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetPendingRequests(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               tABC_RequestInfo ***paRequests,
                               unsigned int *pCount,
                               tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGetPendingRequests(szUserName, szPassword, szWalletUUID, paRequests, pCount, pError));

exit:
    return cc;
}

/**
 * Frees the given array of requets
 *
 * @param aRequests Array of requests
 * @param count     Number of requests
 */
void ABC_FreeRequests(tABC_RequestInfo **aRequests,
                      unsigned int count)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeRequests(aRequests, count);
}

/**
 * Duplicates transaction details.
 * This can be used when changing the details on a transaction.
 *
 * @param ppNewDetails  Address to store pointer to copy of details
 * @param pOldDetails   Ptr to details to copy
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_DuplicateTxDetails(tABC_TxDetails **ppNewDetails,
                               const tABC_TxDetails *pOldDetails,
                               tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxDupDetails(ppNewDetails, pOldDetails, pError));

exit:

    return cc;
}

/**
 * Frees the given transaction details
 *
 * @param pDetails Ptr to details to free
 */
void ABC_FreeTxDetails(tABC_TxDetails *pDetails)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeDetails(pDetails);
}

/**
 * Gets password rules results for a given password.
 *
 * This function takes a password and evaluates how long it will take to crack as well as
 * returns an array of rules with information on whether it satisfied each rule.
 *
 * @param szPassword        Paassword to check.
 * @param pSecondsToCrack   Location to store the number of seconds it would take to crack the password
 * @param paRules           Pointer to store the allocated array of password rules
 * @param pCountRules       Pointer to store number of password rules in the array
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CheckPassword(const char *szPassword,
                          double *pSecondsToCrack,
                          tABC_PasswordRule ***paRules,
                          unsigned int *pCountRules,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    double secondsToCrack;
    tABC_PasswordRule **aRules = NULL;
    unsigned int count = 0;
    tABC_PasswordRule *pRuleCount = NULL;
    tABC_PasswordRule *pRuleLC = NULL;
    tABC_PasswordRule *pRuleUC = NULL;
    tABC_PasswordRule *pRuleNum = NULL;
    tABC_PasswordRule *pRuleSpec = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(pSecondsToCrack);
    ABC_CHECK_NULL(paRules);
    ABC_CHECK_NULL(pCountRules);

    // we know there will be 5 rules (lots of magic numbers in this function...sorry)
    ABC_ALLOC(aRules, sizeof(tABC_PasswordRule *) * 5);

    // must have upper case letter
    ABC_ALLOC(pRuleUC, sizeof(tABC_PasswordRule));
    pRuleUC->szDescription = "Must have at least one upper case letter";
    pRuleUC->bPassed = false;
    aRules[count] = pRuleUC;
    count++;

    // must have lower case letter
    ABC_ALLOC(pRuleLC, sizeof(tABC_PasswordRule));
    pRuleLC->szDescription = "Must have at least one lower case letter";
    pRuleLC->bPassed = false;
    aRules[count] = pRuleLC;
    count++;

    // must have number
    ABC_ALLOC(pRuleNum, sizeof(tABC_PasswordRule));
    pRuleNum->szDescription = "Must have at least one number";
    pRuleNum->bPassed = false;
    aRules[count] = pRuleNum;
    count++;

    // must have special character
    ABC_ALLOC(pRuleSpec, sizeof(tABC_PasswordRule));
    pRuleSpec->szDescription = "Must have at least one special character";
    pRuleSpec->bPassed = false;
    aRules[count] = pRuleSpec;
    count++;

    // must have 10 characters
    ABC_ALLOC(pRuleCount, sizeof(tABC_PasswordRule));
    pRuleCount->szDescription = "Must have at least 10 characters";
    pRuleCount->bPassed = false;
    aRules[count] = pRuleCount;
    count++;

    // check the length
    if (strlen(szPassword) >= 10)
    {
        pRuleCount->bPassed = true;
    }

    // check the other rules
    for (int i = 0; i < strlen(szPassword); i++)
    {
        char c = szPassword[i];
        if (isdigit(c))
        {
            pRuleNum->bPassed = true;
        }
        else if (isalpha(c))
        {
            if (islower(c))
            {
                pRuleLC->bPassed = true;
            }
            else
            {
                pRuleUC->bPassed = true;
            }
        }
        else
        {
            pRuleSpec->bPassed = true;
        }
    }

    // calculate the time to crack

    /*
     From: http://blog.shay.co/password-entropy/
        A common and easy way to estimate the strength of a password is its entropy.
        The entropy is given by H=LlogBase2(N) where L is the length of the password and N is the size of the alphabet, and it is usually measured in bits.
        The entropy measures the number of bits it would take to represent every password of length L under an alphabet with N different symbols.

        For example, a password of 7 lower-case characters (such as: example, polmnni, etc.) has an entropy of H=7logBase2(26)≈32.9bits.
        A password of 10 alpha-numeric characters (such as: P4ssw0Rd97, K5lb42eQa2) has an entropy of H=10logBase2(62)≈59.54bits.

        Entropy makes it easy to compare password strengths, higher entropy means stronger password (in terms of resistance to brute force attacks).
     */
    // Note: (a) the following calculation of is just based upon one method
    //       (b) the guesses per second is arbitrary
    //       (c) it does not take dictionary attacks into account
    int L = (int) strlen(szPassword);
    if (L > 0)
    {
        int N = 0;
        if (pRuleLC->bPassed)
        {
            N += 26; // number of lower-case letters
        }
        if (pRuleUC->bPassed)
        {
            N += 26; // number of upper-case letters
        }
        if (pRuleNum->bPassed)
        {
            N += 10; // number of numeric charcters
        }
        if (pRuleSpec->bPassed)
        {
            N += 35; // number of non-alphanumeric characters on keyboard (iOS)
        }
        const double guessesPerSecond = 1000.0; // this can be changed based upon the speed of the computer
        double entropy = (double) L * (double)log2(N);
        double vars = pow(2, entropy);
        secondsToCrack = vars / guessesPerSecond;
    }
    else
    {
        secondsToCrack = 0;
    }

    // store final values
    *pSecondsToCrack = secondsToCrack;
    *paRules = aRules;
    aRules = NULL;
    *pCountRules = count;
    count = 0;

exit:
    ABC_FreePasswordRuleArray(aRules, count);

    return cc;
}

/**
 * Free the password rule array.
 *
 * This function frees the password rule array returned from ABC_CheckPassword.
 *
 * @param aRules   Array of pointers to password rules to be free'd
 * @param nCount   Number of elements in the array
 */
void ABC_FreePasswordRuleArray(tABC_PasswordRule **aRules,
                               unsigned int nCount)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    if ((aRules != NULL) && (nCount > 0))
    {
        for (int i = 0; i < nCount; i++)
        {
            // note we aren't free'ing the string because it uses heap strings
            ABC_CLEAR_FREE(aRules[i], sizeof(tABC_PasswordRule));
        }
        ABC_CLEAR_FREE(aRules, sizeof(tABC_PasswordRule *) * nCount);
    }
}

/**
 * Loads the settings for a specific account
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param ppSettings    Location to store ptr to allocated settings (caller must free)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_LoadAccountSettings(const char *szUserName,
                                const char *szPassword,
                                tABC_AccountSettings **ppSettings,
                                tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_AccountLoadSettings(szUserName, szPassword, ppSettings, pError));

exit:

    return cc;
}

/**
 * Updates the settings for a specific account
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param pSettings    Pointer to settings
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_UpdateAccountSettings(const char *szUserName,
                                  const char *szPassword,
                                  tABC_AccountSettings *pSettings,
                                  tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_AccountSaveSettings(szUserName, szPassword, pSettings, pError));

exit:

    return cc;
}

/**
 * Frees the given account settings
 *
 * @param pSettings Ptr to setting to free
 */
void ABC_FreeAccountSettings(tABC_AccountSettings *pSettings)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_AccountFreeSettings(pSettings);
}
