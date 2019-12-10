#ifndef __CYVOICE_H__
#define __CYVOICE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * return value
 */
#define CYVOICE_EXIT_SUCCESS                     0x0000
#define CYVOICE_EXIT_FAILURE                     0x0001
#define CYVOICE_EXIT_INVALID_PARM                0x0002
#define CYVOICE_EXIT_LICENSE_FAIL                0x0003
#define CYVOICE_EXIT_INVALID_STATE               0x0004
#define CYVOICE_EXIT_OUT_OF_MEMORY               0x0005
#define CYVOICE_EXIT_INVALID_AUDIO_FORMAT        0x0006
#define CYVOICE_EXIT_EXCEPTION                   0x0007
#define CYVOICE_EXIT_GRAMMAR_OPEN_FAIL           0x0008
#define CYVOICE_EXIT_GRAMMAR_NOT_OPEN            0x0009
#define CYVOICE_EXIT_GRAMMAR_OPEN_ALREADY        0x0010
#define CYVOICE_EXIT_GRAMMAR_INIT_FAIL           0x0011
#define CYVOICE_EXIT_GRAMMAR_NOT_INIT            0x0012
#define CYVOICE_EXIT_GRAMMAR_INIT_ALREADY        0x0013
#define CYVOICE_EXIT_GRAMMAR_LOAD_FAIL           0x0014
#define CYVOICE_EXIT_GRAMMAR_NOT_LOAD            0x0015
#define CYVOICE_EXIT_GRAMMAR_LOAD_ALREADY        0x0016
#define CYVOICE_EXIT_GRAMMAR_NOT_ACTIVATED       0x0017


/**
 * status code
 */
#define CYVOICE_SD_NO_SPEECH                     0x0000
#define CYVOICE_SD_SPEECH_BEGIN_POSSIBLE         0x0001
#define CYVOICE_SD_SPEECH_BEGIN_FOUND            0x0002
#define CYVOICE_SD_SPEECH_END_POSSIBLE           0x0003
#define CYVOICE_SD_SPEECH_END_FOUND              0x0004
#define CYVOICE_SD_SPEECH_TOO_SOFT               0x0005
#define CYVOICE_SD_SPEECH_TOO_LONG               0x0006
#define CYVOICE_RESULT_NOT_READY_YET             CYVOICE_SD_NO_SPEECH
#define CYVOICE_RESULT_READY                     CYVOICE_SD_SPEECH_END_FOUND
#define CYVOICE_RESULT_NONE                      0x0015
#define CYVOICE_RESULT_NO_SPEECH                 0x0016
#define CYVOICE_RESULT_PARTIAL                   0x0018
#define CYVOICE_RESULT_WHOLE                     0x0019
#define CYVOICE_RESULT_FAILED                    0x001f

/**
 * audio format
 */
#define CYVOICE_AUDIO_DEFAULT                    0x0000 ///< Default format: pcm 16bits 8k sample rate.
#define CYVOICE_AUDIO_PCM_8K                     0x0001
#define CYVOICE_AUDIO_PCM_16K                    0x0002
#define CYVOICE_AUDIO_ALAW_8K                    0x0006
#define CYVOICE_AUDIO_MULAW_8K                   0x0007

/**
 * codeset
 */
#define CYVOICE_CODESET_DEFAULT                  0x0000 ///< Default codeset: ascii.
#define CYVOICE_CODESET_ASCII                    0x0001 ///< Ascii codeset.
#define CYVOICE_CODESET_UCS2_LITTLE_ENDIAN       0x0002 ///< Not supported for this version.
#define CYVOICE_CODESET_UCS2_BIG_ENDIAN          0x0003 ///< Not supported for this version.
#define CYVOICE_CODESET_UTF8_LITTLE_ENDIAN       0x0004 ///< UTF8 little endian.

/**
 * parameter
 */
#define CYVOICE_PARM_INVALID                     0x0000
#define CYVOICE_PARM_MAX_BEGIN_SIL               0x0001
#define CYVOICE_PARM_MAX_SPEECH                  0x0002
#define CYVOICE_PARM_MIN_SPEECH                  0x0022
#define CYVOICE_PARM_MAX_END_SIL                 0x0003
#define CYVOICE_PARM_ENABLE_BEGIN_SIL            0x0004
#define CYVOICE_PARM_AUDIO_FORMAT                0x0005
#define CYVOICE_PARM_RESULT_CODESET              0x0006
#define CYVOICE_PARM_NBEST                       0x0007
#define CYVOICE_PARM_ENABLE_CONFIDENCE_SCORE     0x0008
#define CYVOICE_PARM_RECALL_LEVEL                0x0009
#define CYVOICE_PARM_REJECTION_THRESHOLD         0x000A
#define CYVOICE_PARM_ENABLE_NOISE_REDUCTION      0x000B
#define CYVOICE_PARM_DATA_MIN_BLOCK              0x000C
#define CYVOICE_PARM_RESCALE_SLOPE               0x000D
#define CYVOICE_PARM_RESCALE_BIAS                0x000E
#define CYVOICE_PARM_SPEED                       0x0010
#define CYVOICE_PARM_INDEX_BEAM                  0x0011
#define CYVOICE_PARM_INDEX_ACTIVE_MAX            0x0012
#define CYVOICE_PARM_INDEX_LATTICE_BEAM          0x0013
#define CYVOICE_PARM_CVLOG_INDEX                 0x0014
#define CYVOICE_PARM_CONTINUAL_SD                0x0015
#define CYVOICE_PARM_CONTINUAL_SD_ONLINE         0x0016
#define CYVOICE_PARM_CMN_WINDOW_SIZE             0x0017
#define CYVOICE_PARM_SAVE_UTT_TAG                0x0018
    /**
 * typedef
 */
//typedef  char              char;
typedef  short             int16_t;
typedef  int               int32_t;
typedef  unsigned char     uint8_t;
typedef  unsigned short    uint16_t;
typedef  unsigned int      uint32_t;
typedef  void*             CYVOICE_HANDLE;


#define MAX_GRAMMAR_ID 128        // the max number of grammar to be opened in one instance. and also the max number of activated grammar in one instance

#ifndef CYVOICE_MAX_PATH
    #define CYVOICE_MAX_PATH 256
#endif

#define CYVOICE_MAX_NBEST 10
#define CYVOICE_MAX_RESULT_LEN 1024
#define CYVOICE_MAX_RESULT_WORD_NUM 128

typedef struct{
    char szRule[CYVOICE_MAX_PATH];
    char szGrammarName[CYVOICE_MAX_PATH];
    char szGrammarType[CYVOICE_MAX_PATH]; // ngram or fsg
    char szGrammarLang[CYVOICE_MAX_PATH];
    char szFileName[CYVOICE_MAX_PATH];
    char szDictFile[CYVOICE_MAX_PATH];
    int refcnt;
} CYVOICE_GRAMMAR;

typedef struct
{
    int        rank;
    float      score;
    char       result[CYVOICE_MAX_RESULT_LEN];
    char       szGrammarName[CYVOICE_MAX_PATH];
    char       result_raw[CYVOICE_MAX_RESULT_LEN];
    float      score_raw;
} CYVOICE_RESULT;

typedef struct
{
    int       words_num;                                 // number of words
    char      words[CYVOICE_MAX_RESULT_WORD_NUM][16];    // one-word
    float     times[CYVOICE_MAX_RESULT_WORD_NUM];        // one-word's begin time in second (float)
    float     lengths[CYVOICE_MAX_RESULT_WORD_NUM];      // one-word's time length in second (float)
} CYVOICE_RESULT_ALIGN;

typedef struct
{
    int        speech_last_frame;               // in frame

    int        speech_begin_frame;              // in frame
    int        speech_end_frame;                // in frame
    int        speech_frame;                    // total speech (including silence) processed in frame

    int        speech_begin_sample;             // in sample
    int        speech_end_sample;               // in sample
    int        speech_sample;                   // total speech (including silence) processed in sample

    float      f_speech_begin;                  // in seconds
    float      f_speech_end;                    // in seconds
    float      f_speech;                        // total speech (including silence) processed in seconds
} SD_RESULT;


/**
 * To initialize the cyVoice engine
 *
 *     input: configFile, [in] a config file name
 *    output: none
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_FAILURE
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_LICENSE_FAIL
 *            CYVOICE_EXIT_OUT_OF_MEMORY
 *            CYVOICE_EXIT_GRAMMAR_INIT_FAIL
 */
int16_t cyVoiceInit(char *configFile);

/**
 * To uninitialize the cyVoice engine
 *
 *     input: none
 *    output: none
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 */
int16_t cyVoiceUninit();


/**
 * To create one instance of cyVoice engine
 *
 *     input: none
 *    output: handle, [out] pointer to an engine instance to be used by following functions.
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_FAILURE
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_LICENSE_FAIL
 *            CYVOICE_EXIT_OUT_OF_MEMORY
 */
int16_t cyVoiceCreateInstance(CYVOICE_HANDLE *handle);
int16_t cyVoiceCreateInstanceEx(CYVOICE_HANDLE *handle);


/**
 * To release the instance of cyVoice engine
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 */
int16_t cyVoiceReleaseInstance(CYVOICE_HANDLE handle);

/**
 * To initialize the cyVoice engine and create one instance
 *
 *     input: configFile, [in] a config file name
 *    output: handle, [out] a pointer to an engine instance to be used by following functions
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_FAILURE
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_LICENSE_FAIL
 *            CYVOICE_EXIT_OUT_OF_MEMORY
 */
int16_t cyVoiceCreate(char *configFile, CYVOICE_HANDLE *handle);

/**
 * To release the cyVoice engine
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 */
int16_t cyVoiceRelease(CYVOICE_HANDLE handle);


/** 
 *  To get all the available static grammar name/file name/dict file    
 *
 *    output: p_grammar_number, [out] pointer to the total number of the static grammars
 *    output: pp_grammar, [out] pointer to the grammar array 
 *                    
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 */
int16_t cyVoiceGetStaticGrammar(int32_t *p_grammar_number, CYVOICE_GRAMMAR **pp_grammar);

/** 
 *  To get all the activated grammar    
 *
 *     input: handle, [in] pointer to an engine instance
 *    output: p_grammar_number, [out] pointer to the total number of the static grammars
 *    output: pp_grammar, [out] pointer to the grammar array 
 *                    
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 */
int16_t cyVoiceGetActiveGrammar(CYVOICE_HANDLE handle, int32_t *p_grammar_number, CYVOICE_GRAMMAR **pp_grammar);


/**
 *  To rename a static grammar
 *
 *     input: lp_grammar, [in] pointer to grammar structure
 *     input: new_grammar_name, [in] a new grammar name
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_FAILURE
 */
int16_t cyVoiceRenameGrammar(CYVOICE_GRAMMAR *p_grammar, char *new_grammar_name);


/**
 *  To open a grammar in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: lp_grammar, [in] pointer to grammar structure
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 *            CYVOICE_EXIT_GRAMMAR_NOT_INIT
 */
int16_t cyVoiceOpenGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar);


/**
 *  To init a static grammar
 *
 *     input: lp_grammar, [in] pointer to grammar structure
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_GRAMMAR_INIT_ALREADY
 *            CYVOICE_EXIT_GRAMMAR_INIT_FAIL
 */
int16_t cyVoiceInitGrammar(CYVOICE_GRAMMAR *p_grammar);
int16_t cyVoiceInitGrammarBuffer(CYVOICE_GRAMMAR *p_grammar, char *grammar_buffer, int buffer_size);


/**
 *  To load a dynamic grammar in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: lp_grammar, [in] pointer to grammar structure
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 *            CYVOICE_EXIT_GRAMMAR_INIT_ALREADY
 *            CYVOICE_EXIT_GRAMMAR_NOT_INIT
 *            CYVOICE_EXIT_GRAMMAR_INIT_FAIL
 */
int16_t cyVoiceLoadDgGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar);
int16_t cyVoiceLoadDgGrammarBuffer(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar, char *grammar_buffer, int buffer_size);


/*
 *  To close a grammar in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: lp_grammar, [in] pointer to grammar structure
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceCloseGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar);


/*
 *  To close all the opened grammar in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceCloseAllGrammar(CYVOICE_HANDLE handle);

/*
 *  To uninit a static grammar
 *
 *     input: lp_grammar, [in] pointer to grammar structure
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_GRAMMAR_NOT_INIT
 *            CYVOICE_EXIT_GRAMMAR_INIT_FAIL
 */
int16_t cyVoiceUninitGrammar(CYVOICE_GRAMMAR *p_grammar);


/*
 *  To unload a dynamic grammar in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: lp_grammar, [in] pointer to grammar structure
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 *            CYVOICE_EXIT_GRAMMAR_NOT_INIT
 *            CYVOICE_EXIT_GRAMMAR_INIT_FAIL
 */
int16_t cyVoiceUnloadDgGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar);

/*
 *  To acivate a grammar in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: lp_grammar, [in] pointer to grammar structure
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceActivateGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar);

/*
 *  To deacivate a grammar in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: lp_grammar, [in] pointer to grammar structure
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceDeactivateGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar);

/*
 *  To deacivate all the grammar in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceDeactivateAllGrammar(CYVOICE_HANDLE handle);

/**
 * To set a parameter
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: parameter, [in] a parameter (CYVOICE_PARM_MAX_SPEECH
 *                                         CYVOICE_PARM_MAX_BEGIN_SIL
 *                                         CYVOICE_PARM_MAX_END_SIL)
 *                                         CYVOICE_PARM_ENABLE_BEGIN_SIL)
 *
 *     input: value_ptr, [in] the value pointer
 *
 *     CYVOICE_PARM_ENABLE_BEGIN_SIL, value = 0, disable; other, enable.
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceSetParameter(CYVOICE_HANDLE handle, uint16_t parameter, void *value_ptr);

/**
 * To get a parameter
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: parameter, [in] a parameter (CYVOICE_PARM_MAX_SPEECH
 *                                         CYVOICE_PARM_MAX_BEGIN_SIL
 *                                         CYVOICE_PARM_MAX_END_SIL)
 *
 *    output: value_ptr, [out] point of the value
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceGetParameter(CYVOICE_HANDLE handle, uint16_t parameter, void *value_ptr);

/**
 * To enable or disable to save utterance
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: uttPath, [in] set a path to enable, or to set to NULL to disable
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 */
int16_t cyVoiceSaveUtterance(CYVOICE_HANDLE handle, const char *uttPath);

/**
 * To get saved utterance path name
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: bufferLength, [in] Specifies the size, in byte, of the buffer for the drive and path. 
 *
 *    output: pszBuffer, [out] pointer to a buffer that receives the null-terminated string for the name of the path
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_FAILURE
 *            CYVOICE_EXIT_INVALID_STATE
 *            CYVOICE_EXIT_OUT_OF_MEMORY (bufferLength is small)
 */
int16_t cyVoiceGetSavePathName(CYVOICE_HANDLE handle, uint16_t bufferLength, char *pszBuffer);

/**
 * To start a recognition and reset the data
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_FAILURE
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceStart(CYVOICE_HANDLE handle);

/**
* To reset the engine for continual speech recognition（to enable continual by CYVOICE_PARM_CONTINUAL_SD）
*
*     input: handle, [in] pointer to an engine instance
*
*    return: CYVOICE_EXIT_SUCCESS
*            CYVOICE_EXIT_FAILURE
*            CYVOICE_EXIT_INVALID_PARM
*            CYVOICE_EXIT_INVALID_STATE
*/
int16_t cyVoiceReset(CYVOICE_HANDLE handle);

/**
 * To stop the search
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceStop(CYVOICE_HANDLE handle);
int16_t cyVoiceStopFeat(CYVOICE_HANDLE handle);

/**
 * To process a buffer of speech data
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: speechBuffer, [in] pointer to speech buffer (8Khz, 16bit, raw-pcm)
 *     input: bufferSize, [in] the speech buffer size in bytes
 *
 *    output: statusCode, [out] pointer to the status code (CYVOICE_RESULT_NOT_READY_YET
 *                                                          CYVOICE_RESULT_NO_SPEECH
 *                                                          CYVOICE_RESULT_READY)
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_FAILURE
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceProcessData(CYVOICE_HANDLE handle, void *speechBuffer, uint32_t bufferSize, uint16_t *statusCode);
int16_t cyVoiceProcessDataToFeatOpen(CYVOICE_HANDLE handle, char *str_feat, char *str_feat_cmn, char *str_feat_cmn_delta);
int16_t cyVoiceProcessDataToFeatClose(CYVOICE_HANDLE handle);
int16_t cyVoiceProcessDataToFeat(CYVOICE_HANDLE handle, char *str_key);


/**
 * To process a buffer of speech data
 *    cyVoiceProcessData1 (to do speech detection and feature extraction only, should be running on the audio thread)
 *    cyVoiceProcessData2 (to calculate nnet score on GPU, should be running on a different thread from function cyVoiceProcessData1 and cyVoiceSearchForward)
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: speechBuffer, [in] pointer to speech buffer (8Khz, 16bit, raw-pcm)
 *     input: *bufferSize, [in] the speech buffer size in bytes
 *
 *    output: statusCode, [out] pointer to the status code (CYVOICE_RESULT_NOT_READY_YET
 *                                                          CYVOICE_RESULT_NO_SPEECH
 *                                                          CYVOICE_RESULT_READY)
 *    output: num_sd_result, [out] number of the result (multiple SD results)
 *    output: sd_result, [out] the result struct array (multiple SD results)
 *    output: *bufferSize, [out] the speech buffer size in bytes to be processed
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_FAILURE
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceProcessData1(CYVOICE_HANDLE handle, void *speechBuffer, uint32_t *bufferSize, uint16_t *statusCode, int *num_sd_result, SD_RESULT **sd_result);
int16_t cyVoiceProcessData2(CYVOICE_HANDLE handle);

int16_t cyVoiceProcessFeat(CYVOICE_HANDLE handle, float **feats, int row, int col, const char *utt_key);

/**
 * To process a buffer of speech data (long utterance), to do search and query the result when it is available 
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: speechBuffer, [in] pointer to speech buffer (8Khz, 16bit, raw-pcm)
 *     input: bufferSize, [in] the speech buffer size in bytes
 *
 *    output: count, [out] pointer to the count of result's sentences
 *    output: result, [out] pointer to the top best result (GBK or UTF8 string), may include many sentences.
 *    output: score, [out] pointer to the score

 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_FAILURE
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceProcessQueryResult(CYVOICE_HANDLE handle, void *speechBuffer, uint32_t bufferSize, int32_t *count, char **result, int32_t *score);

int16_t cyVoiceQueryStatus(CYVOICE_HANDLE handle, uint16_t *statusCode,  float *speech_begin, float *speech_end);

/**
* To query the status
*
*     input: handle, [in] pointer to an engine instance
*    output: statusCode, [out] pointer to the status code (CYVOICE_RESULT_READY)
*    output: num_sd_result, [out] number of the result (multiple SD results)
*    output: sd_result, [out] the result struct array (multiple SD results)
*    output: num_sr_result, [out] number of the result (multiple SR results)
*    output: sr_result, [out] the result struct array (multiple SR results)
*
*    return: CYVOICE_EXIT_SUCCESS
*            CYVOICE_EXIT_FAILURE
*            CYVOICE_EXIT_INVALID_PARM
*
*/
int16_t cyVoiceQueryStatusEx(CYVOICE_HANDLE handle, uint16_t *statusCode, int *num_sd_result, SD_RESULT **sd_result, int *num_sr_result, CYVOICE_RESULT **sr_result);


/**
 * To do search once there is data processed in the buffer
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    output: statusCode, [out] pointer to the status code (CYVOICE_RESULT_READY)
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceSearchForward(CYVOICE_HANDLE handle, uint16_t *statusCode);
int16_t cyVoiceSearchForwardPartial(CYVOICE_HANDLE handle, uint16_t *statusCode);

/**
 * To abort the search
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceAbort(CYVOICE_HANDLE handle);

/**
 * To query result to get top 1 result
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    output: statusCode, [out] pointer to the status code (CYVOICE_RESULT_READY)
 *    output: result, [out] the top best result (GBK or UTF8 string)
 *    output: score, [out] pointer to the score
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceQueryResult(CYVOICE_HANDLE handle, uint16_t *statusCode, char *result, int32_t *score);
int16_t cyVoiceQueryResultPartial(CYVOICE_HANDLE handle, uint16_t *statusCode, char *result, int32_t *score);

/**
 * To query result to get Nbest results
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    output: statusCode, [out] pointer to the status code (CYVOICE_RESULT_READY)
 *    output: num_result, [out] number of the result (Nbest)
 *    output: st_result, [out] the nbest result struct array
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceQueryResults(CYVOICE_HANDLE handle, uint16_t *statusCode, int *num_result, CYVOICE_RESULT *st_result);

/**
* To query and get the result alignment
*
*     input: handle, [in] pointer to an engine instance
*
*    output: st_result_align, [out] the result align struct array
*
*    return: CYVOICE_EXIT_SUCCESS
*            CYVOICE_EXIT_INVALID_PARM
*            CYVOICE_EXIT_INVALID_STATE
*/
int16_t cyVoiceQueryResultAlign(CYVOICE_HANDLE handle, CYVOICE_RESULT_ALIGN *st_result_align);


/**
 * To query result to get Nbest results by active index
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: active_index, [in] result from the index (from 0 to num_active_grammar-1)
 *
 *    output: statusCode, [out] pointer to the status code (CYVOICE_RESULT_READY)
 *    output: num_active_grammar, [out] number of active grammar in the instance
 *    output: num_result, [out] number of the result (Nbest)
 *    output: st_result, [out] the nbest result struct array
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceQueryActiveResults(CYVOICE_HANDLE handle, int active_index, uint16_t *statusCode, int *num_active_grammar, int *num_result, CYVOICE_RESULT *st_result);
int16_t cyVoiceQueryAllActiveResults(CYVOICE_HANDLE handle, uint16_t *statusCode, int *num_result, CYVOICE_RESULT *st_result, int max_num_result);

/**
 * To query result the get Nbest results by a grammar name
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: grammar name, [in] a grammar name
 *
 *    output: statusCode, [out] pointer to the status code (CYVOICE_RESULT_READY)
 *    output: num_result, [out] number of the result (Nbest)
 *    output: st_result, [out] the nbest result struct array
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceQueryGrammarResults(CYVOICE_HANDLE handle, char *grammar_name, uint16_t *statusCode, int *num_result, CYVOICE_RESULT *st_result);


/**
 * To query result when all the speech data is processed
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: n, [in] the n-th best result, from 1 to MAX_NBEST(10)
 *
 *    output: result, [out] pointer to the result array (GBK string)
 *    output: score, [out] pointer to the score
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 */
int16_t cyVoiceGetNbestResult(CYVOICE_HANDLE handle, int16_t n, char **result, int32_t *score);

/**
 * To get the begin point of speech
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: the begin point of speech (in samples, 8K sampling rate, 16bits)
 *            
 */
int32_t cyVoiceGetSpeechBegin(CYVOICE_HANDLE handle);
int16_t cyVoiceSetSpeechBegin(CYVOICE_HANDLE handle, int32_t speech_begin);

/**
 * To get the end point of speech
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: the begin point of speech (in samples, 8K sampling rate, 16bits)
 *            
 */
int32_t cyVoiceGetSpeechEnd(CYVOICE_HANDLE handle);
int16_t cyVoiceSetSpeechEnd(CYVOICE_HANDLE handle, int32_t speech_end);

/**
 *  To load a user dict in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: pdict, [in] a dict file name
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceLoadUserDict(CYVOICE_HANDLE handle, char *dict_file);
int16_t cyVoiceLoadUserDictBuffer(CYVOICE_HANDLE handle, char *dict_file_buffer, int buffer_size);

/*
 *  To unload a user dict in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceUnloadUserDict(CYVOICE_HANDLE handle);


/*
*  To calculate the confidence score by using the raw_score of a grammar and the raw_score of a background grammar
*
*     input: handle, [in] pointer to an engine instance
*     input: rescale_slope, [in] the rescale_slope parameter, if rescale_slope = 0, to use the default value
*     input: rescale_bias, [in] the rescale_bias parameter, if rescale_bias = 0, to use the default value
*     input: raw_score, [in] the input raw score of a grammar
*     input: raw_score, [in] the input raw score of a background grammar
*
*    output: score, [out] pointer to the confidence score
*
*    return: CYVOICE_EXIT_SUCCESS
*            CYVOICE_EXIT_INVALID_PARM
*            CYVOICE_EXIT_INVALID_STATE
*/
int16_t cyVoiceCalConfScore(CYVOICE_HANDLE handle, float rescale_slope, float rescale_bias, float raw_score, float raw_score_background, int *confidence_score);


#ifdef __cplusplus
}
#endif

#endif // #ifndef __CYVOICE_H__
