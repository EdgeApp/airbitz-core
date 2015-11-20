/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "RecoveryQuestions.hpp"
#include "../Context.hpp"
#include "../auth/LoginServer.hpp"
#include "../json/JsonObject.hpp"
#include "../util/Debug.hpp"
#include "../util/FileIO.hpp"
#include "../util/Util.hpp"
#include <time.h>

namespace abcd {

#define GENERAL_QUESTIONS_FILENAME              "Questions.json"
#define GENERAL_ACCEPTABLE_INFO_FILE_AGE_SECS   (24 * 60 * 60) // how many seconds old can the info file before it should be updated

#define ABC_SERVER_JSON_CATEGORY_FIELD      "category"
#define ABC_SERVER_JSON_MIN_LENGTH_FIELD    "min_length"
#define ABC_SERVER_JSON_QUESTION_FIELD      "question"

struct QuestionsFile:
    public JsonObject
{
    ABC_JSON_VALUE(questions, "questions", JsonPtr);
};

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
            for (unsigned i = 0; i < pQuestionChoices->numChoices; i++)
            {
                tABC_QuestionChoice *pChoice = pQuestionChoices->aChoices[i];
                if (pChoice)
                {
                    ABC_FREE_STR(pChoice->szQuestion);
                    ABC_FREE_STR(pChoice->szCategory);
                }
                ABC_CLEAR_FREE(pChoice, sizeof(tABC_QuestionChoice));
            }

            ABC_CLEAR_FREE(pQuestionChoices->aChoices,
                           sizeof(tABC_QuestionChoice *) * pQuestionChoices->numChoices);
        }

        ABC_CLEAR_FREE(pQuestionChoices, sizeof(tABC_QuestionChoices));
    }
}

/**
 * Gets the recovery question chioces with the given info.
 *
 * @param ppQuestionChoices Pointer to hold allocated pointer to recovery question chioces
 */
tABC_CC ABC_GeneralGetQuestionChoices(tABC_QuestionChoices
                                      **ppQuestionChoices,
                                      tABC_Error              *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string filename = gContext->rootDir() + GENERAL_QUESTIONS_FILENAME;
    QuestionsFile file;
    json_t *pJSON_Value = NULL;
    time_t lastTime;
    AutoFree<tABC_QuestionChoices, ABC_GeneralFreeQuestionChoices>
    pQuestionChoices(structAlloc<tABC_QuestionChoices>());
    unsigned int count = 0;

    ABC_CHECK_NULL(ppQuestionChoices);

    // Update the file if it is too old or does not exist:
    if (!fileTime(lastTime, filename) ||
            lastTime + GENERAL_ACCEPTABLE_INFO_FILE_AGE_SECS < time(nullptr))
    {
        JsonPtr resultsJson;
        ABC_CHECK_NEW(loginServerGetQuestions(resultsJson));
        ABC_CHECK_NEW(file.questionsSet(resultsJson));
        ABC_CHECK_NEW(file.save(gContext->rootDir() + GENERAL_QUESTIONS_FILENAME));
    }

    // Read in the recovery question choices json object
    ABC_CHECK_NEW(file.load(filename));
    pJSON_Value = file.questions().get();
    if (!json_is_array(pJSON_Value))
        ABC_RET_ERROR(ABC_CC_JSONError,
                      "No questions in the recovery question choices file")

        // get the number of elements in the array
        count = (unsigned int) json_array_size(pJSON_Value);
    if (count <= 0)
    {
        ABC_RET_ERROR(ABC_CC_JSONError,
                      "No questions in the recovery question choices file")
    }

    // allocate the data
    pQuestionChoices->numChoices = count;
    ABC_ARRAY_NEW(pQuestionChoices->aChoices, count, tABC_QuestionChoice *);

    for (unsigned i = 0; i < count; i++)
    {
        json_t *pJSON_Elem = json_array_get(pJSON_Value, i);
        ABC_CHECK_ASSERT((pJSON_Elem
                          && json_is_object(pJSON_Elem)), ABC_CC_JSONError,
                         "Error parsing JSON element value for recovery questions");

        // allocate this element
        pQuestionChoices->aChoices[i] = structAlloc<tABC_QuestionChoice>();

        // get the category
        json_t *pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_CATEGORY_FIELD);
        ABC_CHECK_ASSERT((pJSON_Obj
                          && json_is_string(pJSON_Obj)), ABC_CC_JSONError,
                         "Error parsing JSON category value for recovery questions");
        pQuestionChoices->aChoices[i]->szCategory = stringCopy(json_string_value(
                    pJSON_Obj));

        // get the question
        pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_QUESTION_FIELD);
        ABC_CHECK_ASSERT((pJSON_Obj
                          && json_is_string(pJSON_Obj)), ABC_CC_JSONError,
                         "Error parsing JSON question value for recovery questions");
        pQuestionChoices->aChoices[i]->szQuestion = stringCopy(json_string_value(
                    pJSON_Obj));

        // get the min length
        pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_MIN_LENGTH_FIELD);
        ABC_CHECK_ASSERT((pJSON_Obj
                          && json_is_integer(pJSON_Obj)), ABC_CC_JSONError,
                         "Error parsing JSON min length value for recovery questions");
        pQuestionChoices->aChoices[i]->minAnswerLength = (unsigned int)
                json_integer_value(pJSON_Obj);
    }

    // assign final data
    *ppQuestionChoices = pQuestionChoices.release();

exit:
    return cc;
}

} // namespace abcd
