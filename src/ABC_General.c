/**
 * @file
 * AirBitz general, non-account-specific server-supplied data.
 *
 * The data handled in this file is basically just a local cache of various
 * settings that AirBitz would like to adjust from time-to-time without
 * upgrading the entire app.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <jansson.h>
#include "ABC_General.h"
#include "ABC_Util.h"
#include "ABC_Account.h"
#include "ABC_FileIO.h"
#include "ABC_URL.h"
#include "ABC_Debug.h"
#include "ABC_ServerDefs.h"

#define GENERAL_QUESTIONS_FILENAME              "Questions.json"
#define JSON_QUESTIONS_FIELD                    "questions"

static tABC_CC ABC_GeneralServerGetQuestions(json_t **ppJSON_Q, tABC_Error *pError);

/**
 * Free question choices.
 *
 * This function frees the question choices given
 *
 * @param pQuestionChoices  Pointer to question choices to free.
 */
void ABC_GeneralFreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices)
{
    if (pQuestionChoices != NULL)
    {
        if ((pQuestionChoices->aChoices != NULL) && (pQuestionChoices->numChoices > 0))
        {
            for (int i = 0; i < pQuestionChoices->numChoices; i++)
            {
                tABC_QuestionChoice *pChoice = pQuestionChoices->aChoices[i];

                if (pChoice)
                {
                    ABC_FREE_STR(pChoice->szQuestion);
                    ABC_FREE_STR(pChoice->szCategory);
                }
            }

            ABC_CLEAR_FREE(pQuestionChoices->aChoices, sizeof(tABC_QuestionChoice *) * pQuestionChoices->numChoices);
        }

        ABC_CLEAR_FREE(pQuestionChoices, sizeof(tABC_QuestionChoices));
    }
}

/**
 * Gets the recovery question chioces with the given info.
 *
 * @param ppQuestionChoices Pointer to hold allocated pointer to recovery question chioces
 */
tABC_CC ABC_GeneralGetQuestionChoices(tABC_QuestionChoices    **ppQuestionChoices,
                                      tABC_Error              *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szRootDir = NULL;
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;
    tABC_QuestionChoices *pQuestionChoices = NULL;

    ABC_CHECK_NULL(ppQuestionChoices);

    // create the filename for the question json
    ABC_CHECK_RET(ABC_AccountGetRootDir(&szRootDir, pError));
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szRootDir, GENERAL_QUESTIONS_FILENAME);

    // if the file doesn't exist
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    if (true != bExists)
    {
        // get an update from the server
        ABC_CHECK_RET(ABC_GeneralUpdateQuestionChoices(pError));
    }

    // read in the recovery question choices json object
    ABC_CHECK_RET(ABC_FileIOReadFileObject(szFilename, &pJSON_Root, true, pError));

    // get the questions array field
    pJSON_Value = json_object_get(pJSON_Root, JSON_QUESTIONS_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_array(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON array value for recovery questions");

    // get the number of elements in the array
    unsigned int count = (unsigned int) json_array_size(pJSON_Value);
    if (count <= 0)
    {
        ABC_RET_ERROR(ABC_CC_JSONError, "No questions in the recovery question choices file")
    }

    // allocate the data
    ABC_ALLOC(pQuestionChoices, sizeof(tABC_QuestionChoices));
    pQuestionChoices->numChoices = count;
    ABC_ALLOC(pQuestionChoices->aChoices, sizeof(tABC_QuestionChoice *) * count);

    for (int i = 0; i < count; i++)
    {
        json_t *pJSON_Elem = json_array_get(pJSON_Value, i);
        ABC_CHECK_ASSERT((pJSON_Elem && json_is_object(pJSON_Elem)), ABC_CC_JSONError, "Error parsing JSON element value for recovery questions");

        // allocate this element
        ABC_ALLOC(pQuestionChoices->aChoices[i], sizeof(tABC_QuestionChoice));

        // get the category
        json_t *pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_CATEGORY_FIELD);
        ABC_CHECK_ASSERT((pJSON_Obj && json_is_string(pJSON_Obj)), ABC_CC_JSONError, "Error parsing JSON category value for recovery questions");
        ABC_STRDUP(pQuestionChoices->aChoices[i]->szCategory, json_string_value(pJSON_Obj));

        // get the question
        pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_QUESTION_FIELD);
        ABC_CHECK_ASSERT((pJSON_Obj && json_is_string(pJSON_Obj)), ABC_CC_JSONError, "Error parsing JSON question value for recovery questions");
        ABC_STRDUP(pQuestionChoices->aChoices[i]->szQuestion, json_string_value(pJSON_Obj));

        // get the min length
        pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_MIN_LENGTH_FIELD);
        ABC_CHECK_ASSERT((pJSON_Obj && json_is_integer(pJSON_Obj)), ABC_CC_JSONError, "Error parsing JSON min length value for recovery questions");
        pQuestionChoices->aChoices[i]->minAnswerLength = (unsigned int) json_integer_value(pJSON_Obj);
    }

    // assign final data
    *ppQuestionChoices = pQuestionChoices;
    pQuestionChoices = NULL; // so we don't free it below

exit:
    ABC_FREE_STR(szRootDir);
    ABC_FREE_STR(szFilename);
    if (pJSON_Root) json_decref(pJSON_Root);
    if (pQuestionChoices) ABC_GeneralFreeQuestionChoices(pQuestionChoices);

    return cc;
}

/**
 * Gets the recovery question choices from the server and saves them
 * to local storage.
 *
 * @param szUserName UserName for a valid account to retrieve questions
 */
tABC_CC ABC_GeneralUpdateQuestionChoices(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;
    json_t *pJSON_Q    = NULL;
    char   *szRootDir  = NULL;
    char   *szFilename = NULL;
    char   *szJSON     = NULL;

    // get the questions from the server
    ABC_CHECK_RET(ABC_GeneralServerGetQuestions(&pJSON_Q, pError));

    // create the json object that will be our questions
    pJSON_Root = json_object();

    // set our final json for the array element
    json_object_set(pJSON_Root, JSON_QUESTIONS_FIELD, pJSON_Q);

    // create the filename for the question json
    ABC_CHECK_RET(ABC_AccountGetRootDir(&szRootDir, pError));
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szRootDir, GENERAL_QUESTIONS_FILENAME);

    // get the JSON for the file
    szJSON = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

    // write the file
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szJSON, pError));

exit:

    if (pJSON_Root)     json_decref(pJSON_Root);
    if (pJSON_Q)        json_decref(pJSON_Q);
    ABC_FREE_STR(szRootDir);
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szJSON);

    return cc;
}

/**
 * Gets the recovery question choices from the server.
 *
 * This function gets the recovery question choices from the server in
 * the form of a JSON object which is an array of the choices
 *
 * @param L1            Login hash for the account
 * @param ppJSON_Q      Pointer to store allocated json object
 *                      (it is the responsibility of the caller to free the ref)
 */
static
tABC_CC ABC_GeneralServerGetQuestions(json_t **ppJSON_Q, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t  *pJSON_Root     = NULL;
    char    *szURL          = NULL;
    char    *szResults      = NULL;

    ABC_CHECK_NULL(ppJSON_Q);
    // create the URL
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_GET_QUESTIONS_PATH);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szURL, "", &szResults, pError));
    ABC_DebugLog("Server results: %s", szResults);

    // decode the result
    json_t *pJSON_Value = NULL;
    json_error_t error;
    pJSON_Root = json_loads(szResults, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing server JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // get the status code
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_STATUS_CODE_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_number(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON status code");
    int statusCode = (int) json_integer_value(pJSON_Value);

    // if there was a failure
    if (ABC_Server_Code_Success != statusCode)
    {
        if (ABC_Server_Code_NoAccount == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "Account does not exist on server");
        }
        else
        {
            // get the message
            pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_MESSAGE_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_DebugLog("Server message: %s", json_string_value(pJSON_Value));
            ABC_RET_ERROR(ABC_CC_ServerError, json_string_value(pJSON_Value));
        }
    }

    // get the questions
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_RESULTS_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_array(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON question results");
    *ppJSON_Q = pJSON_Value;
    json_incref(*ppJSON_Q);

exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szResults);

    return cc;
}
