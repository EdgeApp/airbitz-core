/**
 * @file
 * AirBitz general, non-account-specific server-supplied data.
 */

#ifndef ABC_General_h
#define ABC_General_h

#include "ABC.h"

namespace abcd {

    /**
     * Contains info on bitcoin miner fee
     */
    typedef struct sABC_GeneralMinerFee
    {
        uint64_t amountSatoshi;
        uint64_t sizeTransaction;
    } tABC_GeneralMinerFee;

    /**
     * Contains information on AirBitz fees
     */
    typedef struct sABC_GeneralAirBitzFee
    {
        double percentage; // maximum value 100.0
        uint64_t minSatoshi;
        uint64_t maxSatoshi;
        char *szAddresss;
    } tABC_GeneralAirBitzFee;

    /**
     * Contains general info from the server
     */
    typedef struct sABC_GeneralInfo
    {
        unsigned int            countMinersFees;
        tABC_GeneralMinerFee    **aMinersFees;
        tABC_GeneralAirBitzFee  *pAirBitzFee;
        unsigned int            countObeliskServers;
        char                    **aszObeliskServers;
        unsigned int            countSyncServers;
        char                    **aszSyncServers;
    } tABC_GeneralInfo;

    void ABC_GeneralFreeInfo(tABC_GeneralInfo *pInfo);

    tABC_CC ABC_GeneralGetInfo(tABC_GeneralInfo **ppInfo,
                               tABC_Error *pError);

    tABC_CC ABC_GeneralUpdateInfo(tABC_Error *pError);

    void ABC_GeneralFreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices);

    tABC_CC ABC_GeneralGetQuestionChoices(tABC_QuestionChoices **ppQuestionChoices,
                                          tABC_Error *pError);

    tABC_CC ABC_GeneralUpdateQuestionChoices(tABC_Error *pError);

} // namespace abcd

#endif
