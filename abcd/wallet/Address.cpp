/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Address.hpp"
#include "Details.hpp"
#include "Wallet.hpp"
#include "../account/AccountSettings.hpp"
#include "../bitcoin/Text.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../util/Util.hpp"
#include <qrencode.h>

namespace abcd {

static tABC_CC  ABC_TxBuildFromLabel(Wallet &self, const char **pszLabel,
                                     tABC_Error *pError);

tABC_CC ABC_TxWatchAddresses(Wallet &self,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    auto addresses = self.addresses.list();
    for (const auto &i: addresses)
    {
        ABC_CHECK_NEW(bridgeWatchAddress(self, i));
    }

exit:
    return cc;
}

/**
 * Sets the recycle status on an address as specified
 *
 * @param szAddress     ID of the address
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxSetAddressRecycle(Wallet &self,
                                const char *szAddress,
                                bool bRecyclable,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    Address address;
    ABC_CHECK_NEW(self.addresses.get(address, szAddress));
    if (address.recyclable != bRecyclable)
    {
        address.recyclable = bRecyclable;
        ABC_CHECK_NEW(self.addresses.save(address));
    }

exit:
    return cc;
}

/**
 * Generate the QR code for a previously created receive request.
 *
 * @param szRequestID   ID of this request
 * @param pszURI        Pointer to string to store URI(optional)
 * @param paData        Pointer to store array of data bytes (0x0 white, 0x1 black)
 * @param pWidth        Pointer to store width of image (image will be square)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGenerateRequestQRCode(Wallet &self,
                                    const char *szRequestID,
                                    char **pszURI,
                                    unsigned char **paData,
                                    unsigned int *pWidth,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    QRcode *qr = NULL;
    unsigned char *aData = NULL;
    unsigned int length = 0;
    char *szURI = NULL;

    // load the request/address
    Address address;
    ABC_CHECK_NEW(self.addresses.get(address, szRequestID));

    // Get the URL string for this info
    tABC_BitcoinURIInfo infoURI;
    memset(&infoURI, 0, sizeof(tABC_BitcoinURIInfo));
    infoURI.amountSatoshi = address.metadata.amountSatoshi;
    infoURI.szAddress = address.address.c_str();

    // Set the label if there is one
    ABC_CHECK_RET(ABC_TxBuildFromLabel(self, &(infoURI.szLabel), pError));

    // if there is a note
    if (!address.metadata.notes.empty())
    {
        infoURI.szMessage = address.metadata.notes.c_str();
    }
    ABC_CHECK_RET(ABC_BridgeEncodeBitcoinURI(&szURI, &infoURI, pError));

    // encode our string
    ABC_DebugLog("Encoding: %s", szURI);
    qr = QRcode_encodeString(szURI, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    ABC_CHECK_ASSERT(qr != NULL, ABC_CC_Error, "Unable to create QR code");
    length = qr->width * qr->width;
    ABC_ARRAY_NEW(aData, length, unsigned char);
    for (unsigned i = 0; i < length; i++)
    {
        aData[i] = qr->data[i] & 0x1;
    }
    *pWidth = qr->width;
    *paData = aData;
    aData = NULL;

    if (pszURI != NULL)
    {
        *pszURI = stringCopy(szURI);
    }

exit:
    ABC_FREE_STR(szURI);
    QRcode_free(qr);
    ABC_CLEAR_FREE(aData, length);

    return cc;
}

/**
 * Create a label based off the user settings
 *
 * @param pszLabel       The label will be returned in this parameter
 * @param pError         A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxBuildFromLabel(Wallet &self,
                             const char **pszLabel, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoFree<tABC_AccountSettings, ABC_FreeAccountSettings> pSettings;

    ABC_CHECK_NULL(pszLabel);
    *pszLabel = NULL;

    ABC_CHECK_RET(ABC_AccountSettingsLoad(self.account, &pSettings.get(), pError));

    if (pSettings->bNameOnPayments && pSettings->szFullName)
    {
        *pszLabel = stringCopy(pSettings->szFullName);
    }

exit:
    return cc;
}

} // namespace abcd
