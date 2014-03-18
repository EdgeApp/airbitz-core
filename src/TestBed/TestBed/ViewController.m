//
//  ViewController.m
//  TestBed
//
//  Created by Adam Harris on 1/21/14.
//  Copyright (c) 2014 Adam Harris. All rights reserved.
//

#include <qrencode.h>
#import "ViewController.h"
#import "ABC.h"
#import "ABC_Util.h"
#import "ABC_Crypto.h"
#import "ABC_Util.h"

@interface ViewController ()
{
    BOOL _bSuccess;
}

@property (weak, nonatomic) IBOutlet UILabel     *labelStatus;
@property (weak, nonatomic) IBOutlet UITextField *textUsername;
@property (weak, nonatomic) IBOutlet UITextField *textPassword;
@property (weak, nonatomic) IBOutlet UITextField *textTest;

@property (assign, nonatomic) BOOL      bSuccess;
@property (nonatomic, strong) NSString  *strReason;
@property (nonatomic, strong) UIView    *viewBlock;
@property (nonatomic, strong) NSString  *strWalletUUID;

void ABC_Results_Callback(const tABC_RequestResults *pResults);

@end

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    
    self.viewBlock = [[UIView alloc] initWithFrame:self.view.frame];
    self.viewBlock.backgroundColor = [UIColor colorWithRed:0.0 green:0.0 blue:0.0 alpha:.5];
    [self.view addSubview:self.viewBlock];
    UIActivityIndicatorView *spinner = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhiteLarge];
    spinner.center = self.viewBlock.center;
    [self.viewBlock addSubview:spinner];
    [spinner startAnimating];
    [self blockUser:NO];

    self.labelStatus.text = @"";
    
    NSLog(@"Calling initialize");
    
    NSMutableData *seedData = [[NSMutableData alloc] init];
    [self fillSeedData:seedData];
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *docs_dir = [paths objectAtIndex:0];
    
    ABC_Initialize([docs_dir UTF8String],
                   ABC_BitCoin_Event_Callback,
                   (__bridge void *) self,
                   (unsigned char *)[seedData bytes],
                   (unsigned int)[seedData length],
                   NULL);
    
    self.labelStatus.text = @"Initialized";
    
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

#pragma mark - Action Methods

- (IBAction)buttonSignInTouched:(id)sender
{
    [self signIn:self.textUsername.text password:self.textPassword.text];
}

- (IBAction)buttonCreateAccountTouched:(id)sender
{
    [self createAccount:self.textUsername.text password:self.textPassword.text];
}

- (IBAction)buttonSetRecoveryTouched:(id)sender
{
    [self setRecovery:self.textUsername.text password:self.textPassword.text];
}

- (IBAction)buttonCreateWalletTouched:(id)sender
{
    [self createWallet:self.textUsername.text password:self.textPassword.text];
}
- (IBAction)buttonTestTouched:(id)sender
{
    tABC_Error Error;
    Error.code = ABC_CC_Ok;

#if 0 // cancel request
    ABC_CancelReceiveRequest([self.textUsername.text UTF8String],
                             [self.textPassword.text UTF8String],
                             [self.textTest.text UTF8String],
                             "1",
                             &Error);
    [self printABC_Error:&Error];

#endif

#if 0 // finalize request
    ABC_FinalizeReceiveRequest([self.textUsername.text UTF8String],
                               [self.textPassword.text UTF8String],
                               [self.textTest.text UTF8String],
                               "1",
                               &Error);
    [self printABC_Error:&Error];

#endif

#if 0 // pending requests and modify the first one
    tABC_RequestInfo **aRequests = NULL;
    unsigned int nCount = 0;
    ABC_GetPendingRequests([self.textUsername.text UTF8String],
                           [self.textPassword.text UTF8String],
                           [self.textTest.text UTF8String],
                           &aRequests,
                           &nCount,
                           &Error);
    [self printABC_Error:&Error];

    printf("Pending requests:\n");

    if (nCount > 0)
    {

        // list them
        for (int i = 0; i < nCount; i++)
        {
            tABC_RequestInfo *pInfo = aRequests[i];

            printf("Pending Request: %s, time: %lld, satoshi: %lld, currency: %lf, name: %s, category: %s, notes: %s, attributes: %u, existing_satoshi: %lld, owed_satoshi: %lld\n",
                   pInfo->szID,
                   pInfo->timeCreation,
                   pInfo->pDetails->amountSatoshi,
                   pInfo->pDetails->amountCurrency,
                   pInfo->pDetails->szName,
                   pInfo->pDetails->szCategory,
                   pInfo->pDetails->szNotes,
                   pInfo->pDetails->attributes,
                   pInfo->amountSatoshi,
                   pInfo->owedSatoshi);
        }

        // take the first one and duplicate the info
        tABC_TxDetails *pNewDetails;
        ABC_DuplicateTxDetails(&pNewDetails, aRequests[0]->pDetails, &Error);
        [self printABC_Error:&Error];

        // change the attributes
        pNewDetails->attributes++;

        // write it back out
        ABC_ModifyReceiveRequest([self.textUsername.text UTF8String],
                                 [self.textPassword.text UTF8String],
                                 [self.textTest.text UTF8String],
                                 aRequests[0]->szID,
                                 pNewDetails,
                                 &Error);
        [self printABC_Error:&Error];

        // free the duplicated details
        ABC_FreeTxDetails(pNewDetails);
    }

    ABC_FreeRequests(aRequests, nCount);
#endif

#if 0 // transactions
    tABC_TxInfo **aTransactions = NULL;
    unsigned int nCount = 0;
    ABC_GetTransactions([self.textUsername.text UTF8String],
                        [self.textPassword.text UTF8String],
                        [self.textTest.text UTF8String],
                        &aTransactions,
                        &nCount,
                        &Error);
    [self printABC_Error:&Error];

    printf("Transactions:\n");

    // list them
    for (int i = 0; i < nCount; i++)
    {
        tABC_TxInfo *pInfo = aTransactions[i];

        printf("Transaction: %s, time: %lld, satoshi: %lld, currency: %lf, name: %s, category: %s, notes: %s, attributes: %u\n",
               pInfo->szID,
               pInfo->timeCreation,
               pInfo->pDetails->amountSatoshi,
               pInfo->pDetails->amountCurrency,
               pInfo->pDetails->szName,
               pInfo->pDetails->szCategory,
               pInfo->pDetails->szNotes,
               pInfo->pDetails->attributes);
    }

    ABC_FreeTransactions(aTransactions, nCount);

#endif

#if 0 // bitcoin uri
    tABC_BitcoinURIInfo *uri;
    printf("Parsing URI: %s\n", [self.textTest.text UTF8String]);
    //ABC_ParseBitcoinURI("bitcoin:113Pfw4sFqN1T5kXUnKbqZHMJHN9oyjtgD?message=test", &uri, &Error);
    ABC_ParseBitcoinURI([self.textTest.text UTF8String], &uri, &Error);
    [self printABC_Error:&Error];

    if (uri != NULL)
    {
        if (uri->szAddress)
            printf("    address: %s\n", uri->szAddress);
        printf("    amount: %lld\n", uri->amountSatoshi);
        if (uri->szLabel)
            printf("    label: %s\n", uri->szLabel);
        if (uri->szMessage)
            printf("    message: %s\n", uri->szMessage);
    }
    else
    {
        printf("URI parse failed!");
    }
#endif

#if 0 // qrcode
    unsigned int width = 0;
    unsigned char *pData = NULL;

    ABC_GenerateRequestQRCode([self.textUsername.text UTF8String],
                              [self.textPassword.text UTF8String],
                              [self.textTest.text UTF8String],
                              "RequestID",
                              &pData,
                              &width,
                              &Error);
    [self printABC_Error:&Error];

    printf("QRCode width: %d\n", width);
    ABC_UtilHexDump("QRCode data", pData, width * width);
    for (int y = 0; y < width; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (pData[(y * width) + x] & 0x1)
            {
                printf("%c", '*');
            }
            else
            {
                printf(" ");
            }
        }
        printf("\n");
    }

    free(pData);
#endif

#if 0 // recovery questions
    char *szRecoveryQuestions;
    ABC_GetRecoveryQuestions([self.textUsername.text UTF8String], &szRecoveryQuestions, &Error);
    [self printABC_Error:&Error];

    if (szRecoveryQuestions)
    {
        printf("Recovery questions:\n%s\n", szRecoveryQuestions);
        free(szRecoveryQuestions);
    }
    else
    {
        printf("No recovery questions!");
    }
#endif

#if 0 // recovery question choices
    NSLog(@"Calling GetQuestionChoices");
    self.labelStatus.text = @"Calling GetQuestionChoices";
    ABC_GetQuestionChoices([self.textUsername.text UTF8String],
                           ABC_Request_Callback,
                           (__bridge void *)self,
                           &Error);
    [self printABC_Error:&Error];

    if (ABC_CC_Ok == Error.code)
    {
        [self blockUser:YES];
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"GetQuestionChoices failed:\n%s", Error.szDescription];
    }

    NSLog(@"Done calling GetQuestionChoices");

#endif

#if 0 // change password with old password
    NSLog(@"Calling Change Password");
    self.labelStatus.text = @"Calling Change Password";
    ABC_ChangePassword("a", "b", "a", "4321", ABC_Request_Callback, (__bridge void *)self, &Error);
    [self printABC_Error:&Error];

    if (ABC_CC_Ok == Error.code)
    {
        [self blockUser:YES];
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Change Password failed:\n%s", Error.szDescription];
    }

    NSLog(@"Done calling Change Password");
#endif

#if 0 // change password with recovery questions
    NSLog(@"Calling Change Password w/Recovery");
    self.labelStatus.text = @"Calling Change Password";
    ABC_ChangePasswordWithRecoveryAnswers("a", "Answer1\nAnswer2\nAnswer3\nAnswer4\nAnswer5", "a", "2222", ABC_Request_Callback, (__bridge void *)self, &Error);
    [self printABC_Error:&Error];

    if (ABC_CC_Ok == Error.code)
    {
        [self blockUser:YES];
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Change Password failed:\n%s", Error.szDescription];
    }

    NSLog(@"Done calling Change Password");
#endif

#if 0 // list wallets
    tABC_WalletInfo **aWalletInfo = NULL;
    unsigned int nCount;
    ABC_GetWallets([self.textUsername.text UTF8String], [self.textPassword.text UTF8String], &aWalletInfo, &nCount, &Error);
    [self printABC_Error:&Error];

    printf("Wallets:\n");

    // list them
    for (int i = 0; i < nCount; i++)
    {
        tABC_WalletInfo *pInfo = aWalletInfo[i];

        printf("Account: %s, UUID: %s, Name: %s, currency: %d, attributes: %u, balance: %lld\n",
               pInfo->szUserName,
               pInfo->szUUID,
               pInfo->szName,
               pInfo->currencyNum,
               pInfo->attributes,
               pInfo->balanceSatoshi);
    }

    ABC_FreeWalletInfoArray(aWalletInfo, nCount);
#endif

#if 0 // re-order wallets
    tABC_WalletInfo **aWalletInfo = NULL;
    unsigned int nCount;
    ABC_GetWallets([self.textUsername.text UTF8String], [self.textPassword.text UTF8String], &aWalletInfo, &nCount, &Error);
    [self printABC_Error:&Error];

    printf("Wallets:\n");

    // create an array of them in reverse order
    char **aszWallets = malloc(sizeof(char *) * nCount);
    for (int i = 0; i < nCount; i++)
    {
        tABC_WalletInfo *pInfo = aWalletInfo[i];

        printf("Account: %s, UUID: %s, Name: %s, currency: %d, attributes: %u, balance: %u\n",
               pInfo->szUserName,
               pInfo->szUUID,
               pInfo->szName,
               pInfo->currencyNum,
               pInfo->attributes,
               pInfo->balance);

        aszWallets[nCount - i - 1] = strdup(pInfo->szUUID);
    }

    ABC_FreeWalletInfoArray(aWalletInfo, nCount);

    // set them in the new order
    ABC_SetWalletOrder([self.textUsername.text UTF8String], [self.textPassword.text UTF8String], aszWallets, nCount, &Error);
    [self printABC_Error:&Error];

    ABC_UtilFreeStringArray(aszWallets, nCount);

    ABC_GetWallets([self.textUsername.text UTF8String], [self.textPassword.text UTF8String], &aWalletInfo, &nCount, &Error);
    [self printABC_Error:&Error];

    printf("Wallets:\n");

    // create an array of them in reverse order
    for (int i = 0; i < nCount; i++)
    {
        tABC_WalletInfo *pInfo = aWalletInfo[i];

        printf("Account: %s, UUID: %s, Name: %s, currency: %d, attributes: %u, balance: %u\n",
               pInfo->szUserName,
               pInfo->szUUID,
               pInfo->szName,
               pInfo->currencyNum,
               pInfo->attributes,
               pInfo->balance);
    }

    ABC_FreeWalletInfoArray(aWalletInfo, nCount);

#endif

#if 0 // check recovery questions
    bool bValid = false;
    ABC_CheckRecoveryAnswers([self.textUsername.text UTF8String], (char *)"Answer1\nAnswer2\nAnswer3\nAnswer4\nAnswer5", &bValid, &Error);
    [self printABC_Error:&Error];
    if (bValid)
    {
        NSLog(@"answers are valid");
    }
    else
    {
        NSLog(@"answers are not valid");
    }
#endif

#if 0 // categories
    Error.code = ABC_CC_Ok;
    char **aszCategories;
    unsigned int count;
    NSMutableArray *arrayCategories = [[NSMutableArray alloc] init];

    ABC_GetCategories("a", &aszCategories, &count, &Error);
    for (int i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);
    NSLog(@"Categories: %@", arrayCategories);


    ABC_AddCategory("a", "firstCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (int i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);
    NSLog(@"Categories: %@", arrayCategories);

    ABC_AddCategory("a", "secondCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (int i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);
    NSLog(@"Categories: %@", arrayCategories);

    ABC_AddCategory("a", "thirdCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (int i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);
    NSLog(@"Categories: %@", arrayCategories);

    ABC_RemoveCategory("a", "secondCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (int i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);
    NSLog(@"Categories: %@", arrayCategories);

    ABC_RemoveCategory("a", "firstCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (int i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);
    NSLog(@"Categories: %@", arrayCategories);

    ABC_RemoveCategory("a", "thirdCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (int i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);
    NSLog(@"Categories: %@", arrayCategories);

#endif

#if 0 // get and set PIN
    char *szPIN = NULL;

    ABC_GetPIN([self.textUsername.text UTF8String], [self.textPassword.text UTF8String], &szPIN, &Error);
    [self printABC_Error:&Error];
    NSLog(@"current PIN: %s", szPIN);
    free(szPIN);

    ABC_SetPIN([self.textUsername.text UTF8String], [self.textPassword.text UTF8String], "1111", &Error);
    [self printABC_Error:&Error];

    ABC_GetPIN([self.textUsername.text UTF8String], [self.textPassword.text UTF8String], &szPIN, &Error);
    [self printABC_Error:&Error];
    NSLog(@"current PIN: %s", szPIN);
    free(szPIN);
#endif

#if 0 // encrypted a string and decrypt it
    tABC_U08Buf Data;
    tABC_U08Buf Key;
    char *szDataToEncrypt = "Data to be encrypted so we can check it";
    ABC_BUF_SET_PTR(Data, (unsigned char *)szDataToEncrypt, strlen(szDataToEncrypt) + 1);
    ABC_BUF_SET_PTR(Key, (unsigned char *)"Key", strlen("Key") + 1);
    char *szEncDataJSON;
    //printf("Calling encrypt...\n");
    printf("          data length: %lu\n", strlen(szDataToEncrypt) + 1);
    printf("          data: %s\n", szDataToEncrypt);
    ABC_CryptoEncryptJSONString(Data,
                                Key,
                                ABC_CryptoType_AES256,
                                &szEncDataJSON,
                                &Error);
    [self printABC_Error:&Error];

    printf("JSON: \n%s\n", szEncDataJSON);

    tABC_U08Buf Data2;
    ABC_CryptoDecryptJSONString(szEncDataJSON,
                                Key,
                                &Data2,
                                &Error);
    [self printABC_Error:&Error];

    printf("Decrypted data length: %d\n", (int)ABC_BUF_SIZE(Data2));
    printf("Decrypted data: %s\n", ABC_BUF_PTR(Data2));

    free(szEncDataJSON);
#endif
#if 0 // encrypted a string and decrypt it with scrypt
    tABC_U08Buf Data;
    tABC_U08Buf Key;
    char *szDataToEncrypt = "Data to be encrypted so we can check it";
    ABC_BUF_SET_PTR(Data, (unsigned char *)szDataToEncrypt, strlen(szDataToEncrypt) + 1);
    ABC_BUF_SET_PTR(Key, (unsigned char *)"Key", strlen("Key") + 1);
    char *szEncDataJSON;
    //printf("Calling encrypt...\n");
    printf("          data length: %lu\n", strlen(szDataToEncrypt) + 1);
    printf("          data: %s\n", szDataToEncrypt);
    ABC_CryptoEncryptJSONString(Data,
                                Key,
                                ABC_CryptoType_AES256_Scrypt,
                                &szEncDataJSON,
                                &Error);
    [self printABC_Error:&Error];

    printf("JSON: \n%s\n", szEncDataJSON);

    tABC_U08Buf Data2;
    ABC_CryptoDecryptJSONString(szEncDataJSON,
                                Key,
                                &Data2,
                                &Error);
    [self printABC_Error:&Error];

    printf("Decrypted data length: %d\n", (int)ABC_BUF_SIZE(Data2));
    printf("Decrypted data: %s\n", ABC_BUF_PTR(Data2));

    free(szEncDataJSON);
#endif

#if 0 // get the currencies
    tABC_Currency *aCurrencyArray;
    int currencyCount;
    ABC_GetCurrencies(&aCurrencyArray, &currencyCount, &Error);
    for (int i = 0; i < currencyCount; i++)
    {
        printf("%d, %s, %s, %s\n", aCurrencyArray[i].num, aCurrencyArray[i].szCode, aCurrencyArray[i].szDescription, aCurrencyArray[i].szCountries);
    }
#endif
}

#pragma mark - Misc Methods

- (void)blockUser:(BOOL)bBlock
{
    self.viewBlock.hidden = !bBlock;
}

- (void)fillSeedData:(NSMutableData *)data
{
    NSMutableString *strSeed = [[NSMutableString alloc] init];
    
    // add the advertiser identifier
    if ([[UIDevice currentDevice] respondsToSelector:@selector(identifierForVendor)])
    {
        [strSeed appendString:[[[UIDevice currentDevice] identifierForVendor] UUIDString]];
    }
    
    // add the UUID
    CFUUIDRef theUUID = CFUUIDCreate(NULL);
    CFStringRef string = CFUUIDCreateString(NULL, theUUID);
    CFRelease(theUUID);
    [strSeed appendString:[[NSString alloc] initWithString:(__bridge NSString *)string]];
    CFRelease(string);
    
    // add the device name
    [strSeed appendString:[[UIDevice currentDevice] name]];
    
    // add the string to the data
    //NSLog(@"seed string: %@", strSeed);
    [data appendData:[strSeed dataUsingEncoding:NSUTF8StringEncoding]];
    
    double time = CACurrentMediaTime();
    
    [data appendBytes:&time length:sizeof(double)];
    
    u_int32_t rand = arc4random();
    
    [data appendBytes:&rand length:sizeof(u_int32_t)];
}

- (void)printABC_Error:(const tABC_Error *)pError
{
    if (pError)
    {
        if (pError->code != ABC_CC_Ok)
        {
            printf("Code: %d, Desc: %s, Func: %s, File: %s, Line: %d\n",
                   pError->code,
                   pError->szDescription,
                   pError->szSourceFunc,
                   pError->szSourceFile,
                   pError->nSourceLine
                   );
        }
    }
}


- (void)signIn:(NSString *)strUsername password:(NSString *)strPassword
{
    _bSuccess = NO;
    NSLog(@"Calling sign-in");
    self.labelStatus.text = @"Calling sign-in";
    tABC_Error Error;
    ABC_SignIn([strUsername UTF8String],
               [strPassword UTF8String],
               ABC_Request_Callback,
               (__bridge void *)self,
               &Error);
    [self printABC_Error:&Error];
    
    if (ABC_CC_Ok == Error.code)
    {
        [self blockUser:YES];
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Sign-in failed:\n%s", Error.szDescription];
    }
    
    NSLog(@"Done calling sign-in");
}

- (void)signInComplete
{
    [self blockUser:NO];
    NSLog(@"SignIn complete");
    if (_bSuccess)
    {
        self.labelStatus.text = @"Successfully Signed In";
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Sign-in failed\n%@", self.strReason];
    }
}

- (void)createAccount:(NSString *)strUsername password:(NSString *)strPassword
{
    _bSuccess = NO;
    NSLog(@"Calling create account");
    self.labelStatus.text = @"Calling create account...";
    tABC_Error Error;
    ABC_CreateAccount([strUsername UTF8String],
                      [strPassword UTF8String],
                      (char *)"1234",
                      ABC_Request_Callback,
                      (__bridge void *)self,
                      &Error);
    [self printABC_Error:&Error];
    
    if (ABC_CC_Ok == Error.code)
    {
        [self blockUser:YES];
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Account creation failed:\n%s", Error.szDescription];
    }
    
    NSLog(@"Done calling create account");
}

- (void)createAccountComplete
{
    [self blockUser:NO];
    NSLog(@"Account create complete");
    if (_bSuccess)
    {
        self.labelStatus.text = @"Account created";
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Account creation failed\n%@", self.strReason];
    }
}

- (void)setRecovery:(NSString *)strUsername password:(NSString *)strPassword
{
    _bSuccess = NO;
    NSLog(@"Calling set recovery");
    self.labelStatus.text = @"Calling set recovery";
    tABC_Error Error;
    ABC_SetAccountRecoveryQuestions([strUsername UTF8String],
                                    [strPassword UTF8String],
                                    (char *)"Question1\nQuestion2\nQuestion3\nQuestion4\nQuestion5",
                                    (char *)"Answer1\nAnswer2\nAnswer3\nAnswer4\nAnswer5",
                                    ABC_Request_Callback,
                                    (__bridge void *)self,
                                    &Error);
    [self printABC_Error:&Error];
    
    if (ABC_CC_Ok == Error.code)
    {
        [self blockUser:YES];
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Set recovery failed:\n%s", Error.szDescription];
    }
    
    
    NSLog(@"Done calling set recovery");
}

- (void)setRecoveryComplete
{
    [self blockUser:NO];
    NSLog(@"Recovery set complete");
    if (_bSuccess)
    {
        self.labelStatus.text = @"Recovery set";
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Set recovery failed\n%@", self.strReason];
    }
}

- (void)createWallet:(NSString *)strUsername password:(NSString *)strPassword
{
    _bSuccess = NO;
    NSLog(@"Calling create wallet");
    self.labelStatus.text = @"Calling create wallet";
    tABC_Error Error;
    
    ABC_CreateWallet([strUsername UTF8String],
                     [strPassword UTF8String],
                     (char *)"MyWallet",
                     840, // currency num
                     0, // attributes
                     ABC_Request_Callback,
                     (__bridge void *)self,
                     &Error);
    [self printABC_Error:&Error];
    
    if (ABC_CC_Ok == Error.code)
    {
        [self blockUser:YES];
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Create wallet failed:\n%s", Error.szDescription];
    }
    
    
    NSLog(@"Done calling create wallet");
}

- (void)createWalletComplete
{
    [self blockUser:NO];
    NSLog(@"Wallet create complete");
    if (_bSuccess)
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Wallet created: %@", self.strWalletUUID];
    }
    else
    {
        self.labelStatus.text = [NSString stringWithFormat:@"Wallet creation failed\n%@", self.strReason];
    }
}

- (void)getQuestionsComplete
{
    [self blockUser:NO];
    NSLog(@"ABC_GetQuestionChoices complete");
    if (_bSuccess)
    {
        self.labelStatus.text = @"ABC_GetQuestionChoices success";
    }
    else
    {
        self.labelStatus.text = @"ABC_GetQuestionChoices failure";
    }
}

- (void)changePasswordComplete
{
    [self blockUser:NO];
    NSLog(@"Change password complete");
    if (_bSuccess)
    {
        self.labelStatus.text = @"Change password success";
    }
    else
    {
        self.labelStatus.text = @"Change password failure";
    }
}


- (void)printQuestionChoices:(tABC_QuestionChoices *)pChoices
{
    if (pChoices)
    {
        if (pChoices->aChoices)
        {
            for (int i = 0; i < pChoices->numChoices; i++)
            {
                tABC_QuestionChoice *pChoice = pChoices->aChoices[i];
                printf("question: %s, category: %s, min: %d\n", pChoice->szQuestion, pChoice->szCategory, pChoice->minAnswerLength);
            }
        }
    }
}


#pragma mark - ABC Callbacks

void ABC_BitCoin_Event_Callback(const tABC_AsyncBitCoinInfo *pInfo)
{
    NSLog(@"Async BitCoin event: %s", pInfo->szDescription);
}

void ABC_Request_Callback(const tABC_RequestResults *pResults)
{
    NSLog(@"Request callback");
    
    if (pResults)
    {
        ViewController *controller = (__bridge id)pResults->pData;
        controller.bSuccess = (BOOL)pResults->bSuccess;
        controller.strReason = [NSString stringWithFormat:@"%s", pResults->errorInfo.szDescription];
        if (pResults->requestType == ABC_RequestType_CreateAccount)
        {
            NSLog(@"Create account completed with cc: %ld (%s)", (unsigned long) pResults->errorInfo.code, pResults->errorInfo.szDescription);
            [controller performSelectorOnMainThread:@selector(createAccountComplete) withObject:nil waitUntilDone:FALSE];
        }
        else if (pResults->requestType == ABC_RequestType_SetAccountRecoveryQuestions)
        {
            NSLog(@"Set recovery completed with cc: %ld (%s)", (unsigned long) pResults->errorInfo.code, pResults->errorInfo.szDescription);
            [controller performSelectorOnMainThread:@selector(setRecoveryComplete) withObject:nil waitUntilDone:FALSE];
        }
        else if (pResults->requestType == ABC_RequestType_CreateWallet)
        {
            if (pResults->pRetData)
            {
                controller.strWalletUUID = [NSString stringWithFormat:@"%s", (char *)pResults->pRetData];
                free(pResults->pRetData);
            }
            else
            {
                controller.strWalletUUID = @"(Unknown UUID)";
            }
            NSLog(@"Create wallet completed with cc: %ld (%s)", (unsigned long) pResults->errorInfo.code, pResults->errorInfo.szDescription);
            [controller performSelectorOnMainThread:@selector(createWalletComplete) withObject:nil waitUntilDone:FALSE];
        }
        else if (pResults->requestType == ABC_RequestType_AccountSignIn)
        {
            NSLog(@"Sign-in completed with cc: %ld (%s)", (unsigned long) pResults->errorInfo.code, pResults->errorInfo.szDescription);
            [controller performSelectorOnMainThread:@selector(signInComplete) withObject:nil waitUntilDone:FALSE];
        }
        else if (pResults->requestType == ABC_RequestType_GetQuestionChoices)
        {
            NSLog(@"GetQuestionChoices completed with cc: %ld (%s)", (unsigned long) pResults->errorInfo.code, pResults->errorInfo.szDescription);
            if (pResults->bSuccess)
            {
                tABC_QuestionChoices *pQuestionChoices = (tABC_QuestionChoices *)pResults->pRetData;
                [controller printQuestionChoices:pQuestionChoices];
                ABC_FreeQuestionChoices(pQuestionChoices);
            }
            [controller performSelectorOnMainThread:@selector(getQuestionsComplete) withObject:nil waitUntilDone:FALSE];
        }
        else if (pResults->requestType == ABC_RequestType_ChangePassword)
        {
            NSLog(@"Change completed with cc: %ld (%s)", (unsigned long) pResults->errorInfo.code, pResults->errorInfo.szDescription);
            [controller performSelectorOnMainThread:@selector(changePasswordComplete) withObject:nil waitUntilDone:FALSE];
        }
    }
}

@end
