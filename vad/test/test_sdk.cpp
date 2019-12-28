#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <unistd.h>

//#include <crtdbg.h>
//#ifdef _DEBUG
//    #define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
//    #define new DEBUG_NEW
//#endif

#include "../cyVoice.h"
#include "tieg.h"
// #include "./inc/nistTool.h"
// #include "./inc/ulawTool.h"
// #include "./inc/Wavefile.h"
// #include "./inc/Timer.h"
// #include "./inc/String.h"
// #include "./inc/tieg.h"
// #pragma comment(lib, "./inc/cyVoice.lib")
// #pragma comment(lib, "./inc/L_TestToolbox.lib")
// #pragma comment(lib, "./inc/tieg.lib")


#define _USE_ThreadSearchForward_
#define TRUE  1
#define FALSE 0

#ifndef _MAX_PATH
#define _MAX_PATH 512
#endif


#define RELEASE 0
#if RELEASE
#define tlog_printfx(a,b,...) do{}while(0);
#endif

typedef struct AudioStruct
{
  int nChannel;
  int nSampleRate;
  int nBits;
  int nAudioLength;
  char* pAudio;
  char pFile[_MAX_PATH];
}AudioStruct;


#define MAX_STRING_LEN 1024
#define LEN 512
bool gt_bThreadFile = false;
bool gt_bThreadSearch = false;
bool gt_bAlign = false;
int gt_grammar_number = 0;
CYVOICE_GRAMMAR *gt_inside_grammar;
long gt_nFileNum;
AudioStruct* gt_pAudioStruct = NULL;//所有的语音数据
std::atomic<long> gt_uttIndex;//索引
long gt_uttNum = 0;//识别的总个数
long gt_uttNumTemp;//当前识别了几条语音
unsigned long long gt_tt = 0;//预设的测试时长
long gt_loop = 0;
long gt_loopTemp = 0;
char *gt_dg_gram_file;

float gt_max_end_sil;
float gt_max_begin_sil;
float gt_max_speech;
float gt_min_speech;

int gt_nbest;
int gt_long_speech_online;
int gt_long_speech;
int gt_sample_rate;

std::mutex g_cs;

//typedef struct ArgList {
//
//}ArgList;

typedef struct ArgThread 
{
  
  int index;
  CYVOICE_HANDLE handle;

  uint64_t TotalProcessTime, Data1Time, Data2Time, SearchTime, GetResultTime, AlignTime;
  uint64_t uttTotalProcessTime, uttData1Time, uttData2Time, uttSearchTime, uttGetResultTime, uttAilgnTime;
  uint64_t uttData1Count, uttData2Count, uttSearchCount, uttGetResultCount, uttAlignCount;
  double TotalSpeechTime;
  double uttTotalSpeechTime;
  //int* pAudioIndex; //所有音频文件的索引
  //int nAudioNum;
  bool bSpeechEnd;
  bool bSearchEnd;
  bool bDataEnd;
  bool bStart;
  bool bSpeechExit;
  bool bSearchExit;
  //ArgList *pDataArglist;
  int nActive_grammar_num;
  int nUtt;
  int nLoop;
}ArgThread;

#define NUM_INSTANCE 200

// Size of audio buffer in sending audio input data to ASR engine.
int gt_BufferSize = 0;
int gt_UserPartial = 0;

unsigned long long tpart_timer(void)
{
  static unsigned long long i_nTimer = 0;

  if(!i_nTimer)
  {
    i_nTimer = ttime_timer();
  }

  return ttime_timer() - i_nTimer;
}

void print_return_code(char *module, int16_t iReturn) {
    switch(iReturn) {
    case CYVOICE_EXIT_SUCCESS:
      printf(" %s (0x%x) SUCCESS\n",module,iReturn);
      break;
    case CYVOICE_EXIT_FAILURE:
      printf(" %s (0x%x) FAILURE\n",module,iReturn);
      break;
    case CYVOICE_EXIT_INVALID_PARM:
      printf(" %s (0x%x) INVALID_PARM\n",module,iReturn);
      break;
    case CYVOICE_EXIT_LICENSE_FAIL:
      printf(" %s (0x%x) LICENSE_FAIL\n",module,iReturn);
      break;
    case CYVOICE_EXIT_INVALID_STATE:
      printf(" %s (0x%x) INVALID_STATE\n",module,iReturn);
      break;
    case CYVOICE_EXIT_OUT_OF_MEMORY:
      printf(" %s (0x%x) OUT_OF_MEMORY\n",module,iReturn);
      break;
    case CYVOICE_EXIT_INVALID_AUDIO_FORMAT:
      printf(" %s (0x%x) INVALID_AUDIO_FORMAT\n",module,iReturn);
      break;
    default:
      printf(" %s (0x%x) Unknown return code\n",module,iReturn);
      break;
    }
}

/***************************************************************************/
void *ThreadSearchForward(void* arg)
{
    int16_t iReturn = CYVOICE_EXIT_SUCCESS;
    CYVOICE_HANDLE handle;
    int index;
    ArgThread *pSearchArglist = (ArgThread*)arg;
    uint16_t statusCode = CYVOICE_RESULT_NOT_READY_YET;

    uint16_t codeSet = CYVOICE_CODESET_UTF8_LITTLE_ENDIAN;
    CYVOICE_RESULT st_result[CYVOICE_MAX_NBEST * 10];
  CYVOICE_RESULT_ALIGN st_result_align;
    char result_save[1024];
    int32_t num_result;

    SD_RESULT *my_sd_result;
    int32_t num_sd_result = 0;
    CYVOICE_RESULT *my_sr_result;
    int32_t num_sr_result = 0;

    int sd_count = 0;
    int i;
    int b_cyVoiceStop = 0;
  //struct cst_Timer timerInter;
  uint64_t timeInter;
  int iii = 0;
  int activeNum = 0;
    handle = pSearchArglist->handle;
    index = pSearchArglist->index;
  gt_bThreadSearch = true;

  int sflag = 0;
  while (1){
    if (pSearchArglist->bDataEnd)
    {
      break;
    }

    if (!pSearchArglist->bStart)
    {
      if (sflag == 1)
      {
        printf("Search Thread end ...\n");
        sflag = 0;
      }
      usleep(50*1000);
      continue;
    }
    if (sflag == 0)
    {
      printf("Search Thread begin ...\n");
      sflag = 1;
    }

    if (pSearchArglist->bSpeechEnd)
    {
            cyVoiceStop(handle);
      b_cyVoiceStop = 1;
        }
    else if(pSearchArglist->uttData1Count <= pSearchArglist->uttData2Count)
    {
      usleep(50*1000);
      continue;
    }
    timeInter = tpart_timer();
        cyVoiceProcessData2(handle);
    pSearchArglist->uttData2Time += tpart_timer() - timeInter;
    ++pSearchArglist->uttData2Count;

    timeInter = tpart_timer();
        cyVoiceSearchForward(handle, &statusCode);
    pSearchArglist->uttSearchTime += tpart_timer() - timeInter;   
    ++pSearchArglist->uttSearchCount;

        cyVoiceQueryStatusEx(handle, &statusCode, &num_sd_result, &my_sd_result, &num_sr_result, &my_sr_result);
    
    if (statusCode == CYVOICE_RESULT_PARTIAL || statusCode == CYVOICE_RESULT_WHOLE)
    {
      num_result = 0;
      //get the result
      for (int iii = 0; iii < pSearchArglist->nActive_grammar_num; ++iii)     
      { 
        timeInter = tpart_timer();
        iReturn = cyVoiceQueryActiveResults(handle, iii, &statusCode, &activeNum, &num_result, st_result);
        pSearchArglist->uttGetResultTime += tpart_timer() - timeInter;
        ++pSearchArglist->uttGetResultCount;
        if (!iReturn && num_result > 0 && st_result[0].result[0])
        {
          wchar_t wrelbuf[LEN];
          char relbuf[LEN * 2];
          wchar_t wrelbufResultRaw[LEN];
          char relbufResultRaw[LEN * 2];
          //TODO
          codeSet = CYVOICE_CODESET_UTF8_LITTLE_ENDIAN;
          if (codeSet == CYVOICE_CODESET_UTF8_LITTLE_ENDIAN)
          {
            // MultiByteToWideChar(CP_UTF8, 0, st_result[0].result, -1, wrelbuf, LEN);
            // WideCharToMultiByte(CP_ACP, 0, wrelbuf, -1, relbuf, LEN * 2, NULL, NULL);
            // MultiByteToWideChar(CP_UTF8, 0, st_result[0].result_raw, -1, wrelbufResultRaw, LEN);
            // WideCharToMultiByte(CP_ACP, 0, wrelbufResultRaw, -1, relbufResultRaw, LEN * 2, NULL, NULL);
            // TODO use GPU will crash !!! 
            tcode_utf82gbk(st_result[0].result, sizeof(st_result[0].result), relbuf, LEN*2);
            tcode_utf82gbk(st_result[0].result_raw, sizeof(st_result[0].result_raw), relbufResultRaw, LEN*2);
          }
          else
          {
            // ascii result
            strcpy(relbuf, st_result[0].result);
            strcpy(relbufResultRaw, st_result[0].result_raw);
          }

          if (statusCode == CYVOICE_RESULT_WHOLE && memcmp(result_save, relbuf, sizeof(relbuf)))
          {
            i = sd_count;
            tlog_printfx("loop_%d,thrd_%d,utt_%d,sd_%d:%s(%s)(%d)(%s)(%.2f),sd_info:Begin:%.3f,End: %.3f,Dur: %.3f,Len:%.3f\n",
              pSearchArglist->nLoop, pSearchArglist->index, pSearchArglist->nUtt, num_sd_result, relbuf, relbufResultRaw, 
              (int)st_result[0].score, st_result[0].szGrammarName, st_result[0].score_raw, 
              my_sd_result[i].f_speech_begin, my_sd_result[i].f_speech_end, 
              (my_sd_result[i].f_speech_end - my_sd_result[i].f_speech_begin), my_sd_result[i].f_speech);
            sd_count = num_sd_result;
            // keep the last partial result
            memcpy(result_save, relbuf, sizeof(relbuf));
          }
        }
        if(statusCode == CYVOICE_RESULT_WHOLE && gt_bAlign)
        {
          timeInter = tpart_timer();
          iReturn = cyVoiceQueryResultAlign(handle, &st_result_align);
          pSearchArglist->uttAilgnTime += tpart_timer() - timeInter;
          ++pSearchArglist->uttAlignCount;
          if (!iReturn)
          {
            for (i = 0; i < st_result_align.words_num; i++)
            {
              wchar_t wrelbuf[LEN];
              char relbuf[LEN * 2];
              //TODO
              // if (codeSet == CYVOICE_CODESET_UTF8_LITTLE_ENDIAN)
              // {
              //  MultiByteToWideChar(CP_UTF8, 0, st_result_align.words[i], -1, wrelbuf, LEN);
              //  WideCharToMultiByte(CP_ACP, 0, wrelbuf, -1, relbuf, LEN * 2, NULL, NULL);
              // }
              // else
              {
                // ascii result
                strcpy(relbuf, st_result_align.words[i]);
              }
              tlog_printfx("loop_%d,thrd_%d,utt_%d,sd_%d:word=%s, time=%f, length=%f\n", 
                pSearchArglist->nLoop, pSearchArglist->index, pSearchArglist->nUtt, num_sd_result, relbuf, 
                st_result_align.times[i], st_result_align.lengths[i]);
            }
          }
        }
      }
      if (statusCode == CYVOICE_RESULT_WHOLE && gt_long_speech > 0)
      {
        iReturn = cyVoiceReset(handle);
      }
    }
    

    if (b_cyVoiceStop)
    {
      pSearchArglist->bSpeechEnd = FALSE;
      pSearchArglist->bSearchEnd = TRUE;
      pSearchArglist->bStart = FALSE;
      b_cyVoiceStop = 0;
      sd_count = 0;
      strcpy(result_save, "");
    }
    }

    pSearchArglist->bSearchExit = TRUE;
    return NULL;
}


/**************************************************************************/
void *ThreadDataFiles(void* argv)
{
  int b_print_results = 1;
  int16_t iReturn = CYVOICE_EXIT_SUCCESS;
  int i;
  ArgThread* mArg = (ArgThread*)argv;
  int count = 0;
  CYVOICE_HANDLE handle;
  CYVOICE_GRAMMAR dg_grammar;
  int num_active_grammar = 0;

  uint16_t audioFormat = CYVOICE_AUDIO_PCM_8K;
  uint16_t codeSet = CYVOICE_CODESET_UTF8_LITTLE_ENDIAN;
  uint64_t start, stop;

  uint64_t timeInter = 0;
  uint64_t timeRtf = 0;
  uint64_t timeOther = 0;
  uint64_t uttOther = 0;

  CYVOICE_RESULT st_result[CYVOICE_MAX_NBEST * 10];
  CYVOICE_RESULT_ALIGN st_result_align;

    char result_save[1024];
    int32_t num_result;
  int activeNum = 0;
    SD_RESULT *my_sd_result;
    int32_t num_sd_result = 0;
    CYVOICE_RESULT *my_sr_result;
    int32_t num_sr_result = 0;

    int sd_count = 0;

  tlog_printfx("Thread %d start...\n", mArg->index);

  iReturn = cyVoiceCreateInstanceEx(&handle);
  mArg->handle = handle;
  if (iReturn != CYVOICE_EXIT_SUCCESS) {
    print_return_code("cyVoiceCreateInstanceEx", iReturn);
    mArg->handle = NULL;
    iReturn = CYVOICE_EXIT_FAILURE;
    return NULL;
  }

  printf("Thread %d start...\n", mArg->index);
  usleep(5*1000);


  float f_value;
  f_value = (float)gt_max_end_sil;
  cyVoiceSetParameter(handle, CYVOICE_PARM_MAX_END_SIL, &f_value);

  f_value = (float)gt_max_begin_sil;
  cyVoiceSetParameter(handle, CYVOICE_PARM_MAX_BEGIN_SIL, &f_value);

  f_value = (float)gt_max_speech;
  cyVoiceSetParameter(handle, CYVOICE_PARM_MAX_SPEECH, &f_value);

  f_value = (float)gt_min_speech;
  cyVoiceSetParameter(handle, CYVOICE_PARM_MIN_SPEECH, &f_value);

  if (gt_nbest > 0){
    uint16_t uint16_nbest = gt_nbest;
    cyVoiceSetParameter(handle, CYVOICE_PARM_NBEST, &uint16_nbest);
  }
  else{
    uint16_t uint16_nbest;
    cyVoiceGetParameter(handle, CYVOICE_PARM_NBEST, &uint16_nbest);
    tlog_printfx("nbest in system config = %d\n", uint16_nbest);
  }

  uint16_t system_continual_sd_setting;
  cyVoiceGetParameter(handle, CYVOICE_PARM_CONTINUAL_SD, &system_continual_sd_setting);
  tlog_printfx("!!!the system continual sd setting = %d\n", system_continual_sd_setting);

  system_continual_sd_setting = gt_long_speech;
  cyVoiceSetParameter(handle, CYVOICE_PARM_CONTINUAL_SD, &system_continual_sd_setting);

  uint16_t system_continual_sd_online_setting;
  cyVoiceGetParameter(handle, CYVOICE_PARM_CONTINUAL_SD_ONLINE, &system_continual_sd_online_setting);
  tlog_printfx("!!!the system continual sd online setting = %d\n", system_continual_sd_online_setting);

  system_continual_sd_online_setting = gt_long_speech_online;
  cyVoiceSetParameter(handle, CYVOICE_PARM_CONTINUAL_SD_ONLINE, &system_continual_sd_online_setting);

  cyVoiceGetParameter(handle, CYVOICE_PARM_RESULT_CODESET, &codeSet);

  // open static grammars
  for (i = 0; i < gt_grammar_number; i++){
    iReturn = cyVoiceOpenGrammar(handle, &gt_inside_grammar[i]);
    if (iReturn != CYVOICE_EXIT_SUCCESS) {
      print_return_code("cyVoiceOpenGrammar", iReturn);
      iReturn = CYVOICE_EXIT_FAILURE;;
    }
  }
  // searchForward
#ifdef _USE_ThreadSearchForward_
  mArg->bSpeechExit= FALSE;
  mArg->bSearchExit = FALSE;
  mArg->bSpeechEnd = FALSE;
  mArg->bSearchEnd = FALSE;
  mArg->bDataEnd = FALSE;
  mArg->bStart = FALSE;
  mArg->nLoop = 0;
  mArg->nUtt = 0;
  mArg->nActive_grammar_num = 0;
  gt_bThreadSearch = false;
  std::thread search_thread(ThreadSearchForward, mArg);
  //_beginthread((void(__cdecl *)(void *))ThreadSearchForward, 0, mArg);
  while(!gt_bThreadSearch)
  {
    usleep(1*1000);
  }
#endif
  gt_bThreadFile = true;

  num_active_grammar = 1;  //TODO for CVTE test  0;
  cyVoiceDeactivateAllGrammar(handle);
  // load dg grammar and open static grammars
  if (!iReturn && gt_dg_gram_file)
  {
    FILE *fp;
    char *grammar_buffer;
    int buffer_size;
    if (fp = fopen(gt_dg_gram_file, "rb"))
    {
      fseek(fp, 0, SEEK_END);
      buffer_size = ftell(fp);
      fseek(fp, 0, SEEK_SET);

      grammar_buffer = (char*)malloc(buffer_size);

      fread(grammar_buffer, sizeof(char), buffer_size, fp);
      fclose(fp);
    }

    // define dg grammar
    strcpy(dg_grammar.szRule, "");
    strcpy(dg_grammar.szGrammarName, "jsgf/phone");
    strcpy(dg_grammar.szGrammarType, "next/jsgf");
    strcpy(dg_grammar.szGrammarLang, "putonghua");
    strcpy(dg_grammar.szFileName, "");
    //strcpy(grammar.szFileName, "");
    strcpy(dg_grammar.szDictFile, "");

    // load dg grammar
    iReturn = cyVoiceLoadDgGrammarBuffer(handle, &dg_grammar, grammar_buffer, buffer_size);
    if (iReturn != CYVOICE_EXIT_SUCCESS) {
      print_return_code("cyVoiceLoadDgGrammar", iReturn);
      iReturn = CYVOICE_EXIT_FAILURE;;
    }
    free(grammar_buffer);

    // activate the dg grammar
    iReturn = cyVoiceActivateGrammar(handle, &dg_grammar);
    if (iReturn != CYVOICE_EXIT_SUCCESS) {
      print_return_code("cyVoiceActivateGrammar", iReturn);
      iReturn = CYVOICE_EXIT_FAILURE;;
    }
    else
      num_active_grammar++;
  }

  // acivate a grammar in the instance
  if (!iReturn) 
  {
    // open static grammars
    for (i = 0; i<gt_grammar_number; i++){
      iReturn = cyVoiceActivateGrammar(handle, &gt_inside_grammar[i]);
      if (iReturn != CYVOICE_EXIT_SUCCESS) {
        print_return_code("cyVoiceActivateGrammar", iReturn);
        iReturn = CYVOICE_EXIT_FAILURE;;
      }
      num_active_grammar++;
    }
  }
  cyVoiceGetParameter(handle, CYVOICE_PARM_RESULT_CODESET, &codeSet);

  start = tpart_timer();
  AudioStruct* pAudioStruct;
  uint16_t statusCode;
  int readSize = 0;

  
  double rtf = 0;
  uint16_t audioFormatTemp = 0;
  uint32_t nBufferSize = 0;
  uint32_t bufferSize = 0;
  while (true)
  {
    stop = tpart_timer();   
    if(gt_tt > 0) //如果设置了测试时长
    {
      if((stop - start) >=  gt_tt)//测试时长已到
      {
        break;
      }
      g_cs.lock();
      if(gt_uttIndex >= gt_nFileNum) //一轮识别结束
      {
        ++gt_loopTemp;
        gt_uttIndex = 0;
      }
      mArg->nUtt = gt_uttIndex; 
      mArg->nLoop = gt_loopTemp;
      gt_uttIndex++;
      g_cs.unlock();
    }
    else if(gt_uttNum > 0) //如果设置了测试语音个数
    {
      std::unique_lock<std::mutex> lock(g_cs);
      if(gt_uttNumTemp >= gt_uttNum) //识别的语音个数，已经达到指定数目
      {
        break;  
      }
      if(gt_uttIndex >= gt_nFileNum) //一轮识别结束
      {
        ++gt_loopTemp;
        gt_uttIndex = 0;
      }
      mArg->nUtt = gt_uttIndex; 
      mArg->nLoop = gt_loopTemp;  
      gt_uttIndex++;
      gt_uttNumTemp++;
    }
    else if(gt_loop > 0) //如果设置了测试轮数
    {
      bool bLoopAgain = false;
      std::unique_lock<std::mutex> lock(g_cs);
      if(gt_uttIndex >= gt_nFileNum) //一轮识别结束
      {
        ++gt_loopTemp;
        gt_uttIndex = 0;        
      }
      if(gt_loopTemp > mArg->nLoop)
      {
        bLoopAgain = true;
      }
      mArg->nLoop = gt_loopTemp;  
      mArg->nUtt = gt_uttIndex;

      if(mArg->nLoop >= gt_loop) //识别的轮数，已经达到指定数目
      {
        break;
      }   
      gt_uttIndex++;

      if(bLoopAgain)
      {
        tlog_printfx("loop_%d,thrd_%d, begin loop", mArg->nLoop,  mArg->index);
      }
    }
    else//没设置测试时长，也没设置识别的语音个数，也没设置识别的遍数，那就只识别一轮
    {
      std::unique_lock<std::mutex> lock(g_cs);
      if(gt_uttIndex >= gt_nFileNum) //一轮识别结束
      {
        break;
      }
      mArg->nUtt = gt_uttIndex;
      gt_uttIndex++;
      tlog_printfx("order:loop_%d,thrd_%d,utt_%d", mArg->nLoop, mArg->index, mArg->nUtt);   
    }
    timeOther = tpart_timer();

    pAudioStruct = &(gt_pAudioStruct[mArg->nUtt]);    

    if(pAudioStruct->pAudio == NULL || pAudioStruct->nAudioLength <= 0)
    {
      gt_uttIndex++;
      continue;
    }
    iReturn = CYVOICE_EXIT_SUCCESS;
    mArg->uttTotalProcessTime = 0;
    mArg->uttData1Time = 0;
    mArg->uttData2Time = 0;
    mArg->uttSearchTime = 0;
    mArg->uttGetResultTime = 0;
    mArg->uttAilgnTime = 0;
    mArg->uttTotalSpeechTime = 0;
    mArg->uttData1Count = 0;
    mArg->uttData2Count = 0;
    mArg->uttSearchCount = 0;
    mArg->uttGetResultCount = 0;
    mArg->uttAlignCount = 0;
    mArg->nActive_grammar_num = num_active_grammar;

    uttOther = 0;
    statusCode = CYVOICE_RESULT_NOT_READY_YET;
    readSize = 0;
    nBufferSize = gt_BufferSize;

    //cyVoiceGetParameter(handle, CYVOICE_PARM_AUDIO_FORMAT, &audioFormat);
    audioFormatTemp = (pAudioStruct->nSampleRate == 8000 ? CYVOICE_AUDIO_PCM_8K:CYVOICE_AUDIO_PCM_16K);
    //if (audioFormatTemp != audioFormat)
    {
      cyVoiceSetParameter(handle, CYVOICE_PARM_AUDIO_FORMAT, &audioFormatTemp);
    }

    // printf("loop_%d,thrd_%d, begin loop\n", mArg->nLoop, mArg->index);
    // usleep(1000 * 30);

    // start engine
    iReturn = cyVoiceStart(handle);
    if (iReturn != CYVOICE_EXIT_SUCCESS) {
      print_return_code("cyVoiceStart", iReturn);
      iReturn = CYVOICE_EXIT_FAILURE;
    }
    

#ifdef _USE_ThreadSearchForward_    
    mArg->bStart = TRUE;
#endif
    uttOther += tpart_timer() - timeOther;
    timeRtf = tpart_timer();
    //FILE* fs = fopen("./1.pcm", "wb");
    // process data
    if(!iReturn)
    {
      if (nBufferSize <= 0)
      {
        nBufferSize = pAudioStruct->nAudioLength;
      }
      while(1)
      {
        int reverse = pAudioStruct->nAudioLength - readSize;
        bufferSize = reverse > nBufferSize ? nBufferSize : reverse;
        timeInter = tpart_timer();
        iReturn = cyVoiceProcessData1(handle, pAudioStruct->pAudio + readSize, &bufferSize, &statusCode, &num_sd_result, &my_sd_result);
        mArg->uttData1Time += tpart_timer() - timeInter;  
        ++mArg->uttData1Count;    
        readSize += bufferSize;

        if(iReturn == CYVOICE_EXIT_WAVE_PIECE_TOO_SHORT)
          break;
        if(iReturn != CYVOICE_EXIT_SUCCESS)
        {
          print_return_code("cyVoiceProcessData1",iReturn);
          iReturn = CYVOICE_EXIT_FAILURE;
          goto exit;
        }
        if (!gt_long_speech && statusCode >= CYVOICE_SD_SPEECH_END_FOUND)
        {
          goto exit;
        }

#ifndef _USE_ThreadSearchForward_
        timeInter = tpart_timer();
        cyVoiceProcessData2(handle);
        mArg->uttData2Time += tpart_timer() - timeInter;  
        ++mArg->uttData2Count;

        timeInter = tpart_timer();
        cyVoiceSearchForward(handle, &statusCode);
        mArg->uttSearchTime += tpart_timer() - timeInter; 
        ++mArg->uttSearchCount;

        cyVoiceQueryStatusEx(handle, &statusCode, &num_sd_result, &my_sd_result, &num_sr_result, &my_sr_result);

        if (statusCode == CYVOICE_RESULT_PARTIAL || statusCode == CYVOICE_RESULT_WHOLE)
        { 
          num_result = 0;
          //get the result
          for (int iii = 0; iii < mArg->nActive_grammar_num; ++iii)     
          { 
            timeInter = tpart_timer();
            iReturn = cyVoiceQueryActiveResults(handle, iii, &statusCode, &activeNum, &num_result, st_result);
            mArg->uttGetResultTime += tpart_timer() - timeInter;
            ++mArg->uttGetResultCount;
            if (!iReturn && num_result > 0 && st_result[0].result[0])
            {
              wchar_t wrelbuf[LEN];
              char relbuf[LEN * 2];
              wchar_t wrelbufResultRaw[LEN];
              char relbufResultRaw[LEN * 2];
              // if (codeSet == CYVOICE_CODESET_UTF8_LITTLE_ENDIAN)
              // {
              //  MultiByteToWideChar(CP_UTF8, 0, st_result[0].result, -1, wrelbuf, LEN);
              //  WideCharToMultiByte(CP_ACP, 0, wrelbuf, -1, relbuf, LEN * 2, NULL, NULL);
              //  MultiByteToWideChar(CP_UTF8, 0, st_result[0].result_raw, -1, wrelbufResultRaw, LEN);
              //  WideCharToMultiByte(CP_ACP, 0, wrelbufResultRaw, -1, relbufResultRaw, LEN * 2, NULL, NULL);
              // }
              // else
              {
                // ascii result
                strcpy(relbuf, st_result[0].result);
                strcpy(relbufResultRaw, st_result[0].result_raw);
              }

              if (statusCode == CYVOICE_RESULT_WHOLE && strcmp(result_save, st_result[0].result))
              {
                i = sd_count;
                tlog_printfx("loop_%d,thrd_%d,utt_%d,sd_%d:%s(%s)(%d)(%s)(%.2f),sd_info:Begin:%.3f,End: %.3f,Dur: %.3f,Len:%.3f\n", 
                  mArg->nLoop, mArg->index, mArg->nUtt, num_sd_result, relbuf, relbufResultRaw, (int)st_result[0].score, 
                  st_result[0].szGrammarName, st_result[0].score_raw, my_sd_result[i].f_speech_begin, my_sd_result[i].f_speech_end, 
                  (my_sd_result[i].f_speech_end - my_sd_result[i].f_speech_begin), my_sd_result[i].f_speech);
                sd_count = num_sd_result;
                // keep the last partial result
                strcpy(result_save, st_result[0].result);
              }

            }
            if(statusCode == CYVOICE_RESULT_WHOLE && gt_bAlign)
            {
              timeInter = tpart_timer();
              iReturn = cyVoiceQueryResultAlign(handle, &st_result_align);
              mArg->uttAilgnTime += tpart_timer() - timeInter;
              ++mArg->uttAlignCount;
              if (!iReturn)
              {
                for (i = 0; i < st_result_align.words_num; i++)
                {
                  wchar_t wrelbuf[LEN];
                  char relbuf[LEN * 2];
                  // if (codeSet == CYVOICE_CODESET_UTF8_LITTLE_ENDIAN)
                  // {
                  //  MultiByteToWideChar(CP_UTF8, 0, st_result_align.words[i], -1, wrelbuf, LEN);
                  //  WideCharToMultiByte(CP_ACP, 0, wrelbuf, -1, relbuf, LEN * 2, NULL, NULL);
                  // }
                  // else
                  {
                    // ascii result
                    strcpy(relbuf, st_result_align.words[i]);
                  }
                  tlog_printfx("word=%s, time=%f, length=%f\n", relbuf, st_result_align.times[i], st_result_align.lengths[i]);
                }
              }
            }
          }
          if (statusCode == CYVOICE_RESULT_WHOLE && gt_long_speech > 0)
          {
            iReturn = cyVoiceReset(handle);
          }
        }
#endif
      }
    }

exit:
#ifndef _USE_ThreadSearchForward_
    cyVoiceStop(handle);

    timeInter = tpart_timer();
    cyVoiceProcessData2(handle);
    mArg->uttData2Time += tpart_timer() - timeInter;  
    ++mArg->uttData2Count;

    timeInter = tpart_timer();
    cyVoiceSearchForward(handle, &statusCode);
    mArg->uttSearchTime += tpart_timer() - timeInter; 
    ++mArg->uttSearchCount;

    cyVoiceQueryStatusEx(handle, &statusCode, &num_sd_result, &my_sd_result, &num_sr_result, &my_sr_result);
    
    if (statusCode == CYVOICE_RESULT_PARTIAL || statusCode == CYVOICE_RESULT_WHOLE)
    {
      num_result = 0;
      //get the result
      for (int iii = 0; iii < mArg->nActive_grammar_num; ++iii)     
      { 
        timeInter = tpart_timer();
        iReturn = cyVoiceQueryActiveResults(handle, iii, &statusCode, &activeNum, &num_result, st_result);
        mArg->uttGetResultTime += tpart_timer() - timeInter;
        ++mArg->uttGetResultCount;
        if (!iReturn && num_result > 0 && st_result[0].result[0])
        {
          wchar_t wrelbuf[LEN];
          char relbuf[LEN * 2];
          wchar_t wrelbufResultRaw[LEN];
          char relbufResultRaw[LEN * 2];
          // if (codeSet == CYVOICE_CODESET_UTF8_LITTLE_ENDIAN)
          // {
          //  MultiByteToWideChar(CP_UTF8, 0, st_result[0].result, -1, wrelbuf, LEN);
          //  WideCharToMultiByte(CP_ACP, 0, wrelbuf, -1, relbuf, LEN * 2, NULL, NULL);
          //  MultiByteToWideChar(CP_UTF8, 0, st_result[0].result_raw, -1, wrelbufResultRaw, LEN);
          //  WideCharToMultiByte(CP_ACP, 0, wrelbufResultRaw, -1, relbufResultRaw, LEN * 2, NULL, NULL);
          // }
          // else
          {
            // ascii result
            strcpy(relbuf, st_result[0].result);
            strcpy(relbufResultRaw, st_result[0].result_raw);
          }

          if (statusCode == CYVOICE_RESULT_WHOLE && strcmp(result_save, st_result[0].result))
          {
            i = sd_count;
            tlog_printfx("loop_%d,thrd_%d,utt_%d,sd_%d:%s(%s)(%d)(%s)(%.2f),sd_info:Begin:%.3f,End: %.3f,Dur: %.3f,Len:%.3f\n", mArg->nLoop, mArg->index, mArg->nUtt, num_sd_result, relbuf, relbufResultRaw, (int)st_result[0].score, st_result[0].szGrammarName, st_result[0].score_raw, my_sd_result[i].f_speech_begin, my_sd_result[i].f_speech_end, (my_sd_result[i].f_speech_end - my_sd_result[i].f_speech_begin), my_sd_result[i].f_speech);
            sd_count = num_sd_result;
            // keep the last partial result
            strcpy(result_save, st_result[0].result);
          }
        }
        if(statusCode == CYVOICE_RESULT_WHOLE && gt_bAlign)
        {
          timeInter = tpart_timer();
          iReturn = cyVoiceQueryResultAlign(handle, &st_result_align);
          mArg->uttAilgnTime += tpart_timer() - timeInter;
          ++mArg->uttAlignCount;
          if (!iReturn)
          {
            for (i = 0; i < st_result_align.words_num; i++)
            {
              wchar_t wrelbuf[LEN];
              char relbuf[LEN * 2];
              // if (codeSet == CYVOICE_CODESET_UTF8_LITTLE_ENDIAN)
              // {
              //  MultiByteToWideChar(CP_UTF8, 0, st_result_align.words[i], -1, wrelbuf, LEN);
              //  WideCharToMultiByte(CP_ACP, 0, wrelbuf, -1, relbuf, LEN * 2, NULL, NULL);
              // }
              // else
              {
                // ascii result
                strcpy(relbuf, st_result_align.words[i]);
              }
              tlog_printfx("word=%s, time=%f, length=%f\n", relbuf, st_result_align.times[i], st_result_align.lengths[i]);
            }
          }
        }
      }

    }
#endif

    // do search forward
#ifdef _USE_ThreadSearchForward_
    mArg->bSpeechEnd = TRUE;
    // wait for the end of ThreadSearchForward
    while(!mArg->bSearchEnd){
      usleep(5*1000);
    }
#endif    
    mArg->uttTotalProcessTime = tpart_timer() - timeRtf;

    timeOther = tpart_timer();
    mArg->bSpeechEnd = FALSE;
    mArg->bSearchEnd = FALSE;
    mArg->bStart = FALSE;


    mArg->Data1Time += mArg->uttData1Time;
    mArg->Data2Time += mArg->uttData2Time;
    mArg->SearchTime += mArg->uttSearchTime;
    mArg->GetResultTime += mArg->uttGetResultTime;
    mArg->AlignTime += mArg->uttAilgnTime;

    mArg->uttTotalSpeechTime = ((double)pAudioStruct->nAudioLength / (pAudioStruct->nSampleRate * 2)) * 1000;
    mArg->TotalProcessTime += mArg->uttTotalProcessTime;
    mArg->TotalSpeechTime += mArg->uttTotalSpeechTime;

    rtf = (mArg->uttTotalSpeechTime > 0 ? (mArg->uttTotalProcessTime / mArg->uttTotalSpeechTime) : mArg->uttTotalProcessTime);

    uttOther += tpart_timer() - timeOther;

    tlog_printfx("loop_%d,thrd_%d,utt_%d,file_%s,Utt RTF:%.3f,data1=%llu,data2=%llu,Search=%llu,GetResult=%llu,Align=%llu,speech=%.0f, process=%llu,other=%llu,data1_count=%llu,data2_count=%llu,search_count=%llu,getresult_count=%llu,align_count=%llu\n",  
      mArg->nLoop, mArg->index, mArg->nUtt, pAudioStruct->pFile, rtf, mArg->uttData1Time, mArg->uttData2Time, 
      mArg->uttSearchTime, mArg->uttGetResultTime, mArg->uttAilgnTime, mArg->uttTotalSpeechTime, mArg->uttTotalProcessTime, 
      uttOther, mArg->uttData1Count, mArg->uttData2Count,mArg->uttSearchCount, mArg->uttGetResultCount, mArg->uttAlignCount); 
    ++mArg->nUtt;

    printf("one loop finish \n");
    usleep(3*1000);
  }//main loop

  if (gt_dg_gram_file)
  {
    // deactivate dg grammar
    if (!iReturn)
    {
      iReturn = cyVoiceDeactivateGrammar(handle, &dg_grammar);
      if (iReturn != CYVOICE_EXIT_SUCCESS) {
        print_return_code("cyVoiceDeactivateGrammar", iReturn);
        iReturn = CYVOICE_EXIT_FAILURE;;
      }
      else
        num_active_grammar--;
    }
    //deactivate static grammar
    for (i = 0; i<gt_grammar_number; i++){
      iReturn = cyVoiceDeactivateGrammar(handle, &gt_inside_grammar[i]);
      if (iReturn != CYVOICE_EXIT_SUCCESS) {
        print_return_code("cyVoiceDeactivateGrammar", iReturn);
        iReturn = CYVOICE_EXIT_FAILURE;;
      }
      else
        num_active_grammar--;
    }
    // unload dg grammar
    if (!iReturn)
    {
      iReturn = cyVoiceUnloadDgGrammar(handle, &dg_grammar);
      if (iReturn != CYVOICE_EXIT_SUCCESS) {
        print_return_code("cyVoiceUnloadDgGrammar", iReturn);
        iReturn = CYVOICE_EXIT_FAILURE;;
      }
    }
  }

#ifdef _USE_ThreadSearchForward_
  mArg->bDataEnd = TRUE;
  while (!mArg->bSearchExit)
  {
    usleep(5*1000);
  }
  if(search_thread.joinable())
    search_thread.join();
#endif

  //End of Test
  uint64_t dTotal = mArg->Data1Time + mArg->Data2Time + mArg->SearchTime + mArg->GetResultTime;
  tlog_printfx("\nthrd_%d--- End of Test---\nProcessData1:%llu ms (%.2f)\nProcessData2:%llu ms (%.2f)\nSearchForward:%llu ms (%.2f)\nGetResult:%llu ms (%.2f)\nAlign:%llu ms (%.2f)\nTotal Speech Time: %.0f ms\nTotal Process Time: %llu ms\nAverage Real Time Factor: %.3f", 
    mArg->index, mArg->Data1Time, (double)mArg->Data1Time / dTotal * 100, mArg->Data2Time, 
    (double)mArg->Data2Time / dTotal * 100, mArg->SearchTime, (double)mArg->SearchTime / dTotal * 100, 
    mArg->GetResultTime, (double)mArg->GetResultTime / dTotal * 100,  mArg->AlignTime, 
    (double)mArg->AlignTime / dTotal * 100, mArg->TotalSpeechTime, mArg->TotalProcessTime, 
    mArg->TotalProcessTime / mArg->TotalSpeechTime);

  // close the grammar in the instance
  if(!iReturn) {
    // close static grammars
    for(i=0; i<gt_grammar_number; i++){
      iReturn = cyVoiceCloseGrammar(handle, &gt_inside_grammar[i]);
      if(iReturn != CYVOICE_EXIT_SUCCESS) {
        print_return_code("cyVoiceCloseGrammar",iReturn);
        iReturn = CYVOICE_EXIT_FAILURE;;
      }
    }
    num_active_grammar = 0;
  }
  mArg->bSpeechExit = TRUE;
  return NULL;
}

int get_wav(const char* pFile, char** pAudio, int* nAudioLen, int* nSampleRate, int* nSampleBits, short* nChannel)
{
  FILE* fs = NULL;
  int nFileSize =0;
  int nHeadLen = 44;
  char buffer[5] = {0};
  int riffSize = 0;
  int formatAttr = 0;
  short compressionCode = 0;
  int bytesPerSecond = 0;
  short blockAlign = 0;
  int nDataReadLen = 0;
  *nSampleRate = 0;
  *nSampleBits = 0;
  *nChannel = 0;
  *pAudio = NULL;
  *nAudioLen = 0;
  //4byte,资源交换文件标志:RIFF
  fs = fopen(pFile, "rb");
  if(fs == NULL)
  {
    return -1;
  }
  fseek(fs, 0L, SEEK_END); 
    nFileSize = ftell(fs);
  if(nFileSize < nHeadLen)//wav头部字节长度是44，
  {
    fclose(fs);
    return -2;
  }
  fseek(fs, 0L, SEEK_SET); 
  //4byte, "RIFF"  0~3
  buffer[0] = '\0';
  fread(buffer, 4, 1, fs);
  if(strcmp(buffer, "RIFF") != 0) 
  {
    fclose(fs);
    return -3;
  }

  //4byte,从下个地址到文件结尾的总字节数  4~7
  riffSize = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
  //4byte,wave文件标志:"WAVE"  8~11
  memset(buffer, 0, 5);
  fread(buffer, 4, 1, fs);
  if(strcmp(buffer, "WAVE") != 0)
  {
    fclose(fs);
    return -4;
  }
  //4byte,波形文件标志:"FMT "  12~15
  memset(buffer, 0, 5);
  fread(buffer, 4, 1, fs);
  if(strcmp(buffer, "fmt ") != 0)
  {
    fclose(fs);
    return -5;
  }
  //4byte,音频属性 16~19
  formatAttr = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
  //2byte,编码格式(1-线性pcm-WAVE_FORMAT_PCM,WAVEFORMAT_ADPCM)   20~21
  compressionCode = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8);
  //2byte,通道数 22~23
  *nChannel = (fgetc(fs) & 255) + (fgetc(fs) << 8);
  //4byte,采样率  24~27
  *nSampleRate = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
  //4byte,传输速率, 其值为通道数×每秒数据位数×每样本的数据位数／8    28~31
  bytesPerSecond = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
  //2byte,数据块的对齐 ，其值为通道数×每样本的数据位值／8  32~33
  blockAlign = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8);
  //2byte,采样精度 34~35
  *nSampleBits = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8);
  //4byte,数据标志: "data"  36~39
  memset(buffer, 0, 5);
  fread(buffer, 4, 1, fs);
  int nList = 0;
  while(strcmp(buffer, "LIST") == 0)
  {
    nList = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
    fseek(fs, nList, SEEK_CUR);
    memset(buffer, 0, 5);
    fread(buffer, 4, 1, fs);
  }

  if(strcmp(buffer, "data") != 0)
  {
    fclose(fs);
    return -6;
  }
  //4byte,从下个地址到文件结尾的总字节数，即除了wav header以外的pcm data length  40~43
  *nAudioLen = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
  if(*nAudioLen <= 0)
  {
    fclose(fs);
    return -7;
  }
  *pAudio = (char*)malloc(*nAudioLen * sizeof(char));
  if(*pAudio == NULL)
  {
    fclose(fs);
    return -8;
  }
  fseek(fs, nHeadLen, SEEK_SET);
  nDataReadLen = fread(*pAudio, 1, *nAudioLen, fs);
  if (nDataReadLen != *nAudioLen)
  {
    free(*pAudio);
    *pAudio = NULL;
    fclose(fs);
    return -9;
  }
  fclose(fs);
  return 1;
}

int get_nist(const char* pFile, char** pAudio, int* nAudioLen, int* nSampleRate, int* nSampleBits, short* nChannel)
{
  int nFileSize = 0;
  FILE* fs = NULL;
  char buffer[MAX_STRING_LEN] = {0};
  char* pReturn = NULL;
  int nReturn = 1;
  char* pSplit = NULL;
  char* pKey = NULL;
  char* pValue = NULL;
  int i = 0;
  int nHeadLen = 0;
  int nDataReadLen = 0;
  *nSampleRate = 0;
  *nSampleBits = 0;
  *nChannel = 0;
  *pAudio = NULL;
  *nAudioLen = 0;
  fs = fopen(pFile, "rb");
  if(fs == NULL)
  {
    return -1;
  }
  fseek(fs, 0L, SEEK_END); 
    nFileSize = ftell(fs);
  if(nFileSize < 25)//nist头部字节长度至少25
  {
    fclose(fs);
    return -2;
  }
  fseek(fs, 0L, SEEK_SET); 
  //第一行 "NIST_1A"  nist文件类型
  buffer[0] = '\0';
  pReturn = fgets(buffer, MAX_STRING_LEN, fs);  
  if(pReturn == NULL) 
  {
    fclose(fs);
    return -3;
  }
  //buffer[strlen(buffer) - 1] == '\r' || buffer[strlen(buffer) - 1] == '\n'
  while(isspace(buffer[strlen(buffer) - 1]))
  {
    buffer[strlen(buffer) - 1] = '\0';
  }
  if(strcmp(buffer, "NIST_1A") != 0)
  {
    return -13;
  }
  //第二行 nist文件头长度
  memset(buffer, 0, MAX_STRING_LEN);
  pReturn = fgets(buffer, MAX_STRING_LEN, fs);
  if(pReturn == NULL) 
  {
    fclose(fs);
    return -4;
  }
  nHeadLen = atoi(buffer);
  if(nHeadLen <= 25 || nHeadLen > nFileSize)
  {
    fclose(fs);
    return -5;
  }
  nReturn = 1;
  pSplit = NULL;
  pKey = NULL;
  pValue = NULL;
  while(1)
  {
    pKey = NULL;
    pValue = NULL;
    if(feof(fs) != 0)
    {
      nReturn = -6;
      break;
    }
    memset(buffer, 0, MAX_STRING_LEN);
    pReturn = fgets(buffer, MAX_STRING_LEN, fs);
    if(pReturn == NULL) 
    {
      nReturn = -7;
      break;
    }
    while(isspace(buffer[strlen(buffer) - 1]))
    {
      buffer[strlen(buffer) - 1] = '\0';
    }
    if(strcmp(buffer, "end_head") == 0) //说明头部是完整的
    {     
      break;
    }
    pSplit = strtok(buffer, " ");
    if(pSplit == NULL)
    {
      continue;
    }
    pKey = pSplit; //参数名
    pValue = pSplit; //参数值
    for(i = 0; pSplit != NULL; ++i)
    {
      pValue = pSplit;
      pSplit = strtok(NULL, " ");
    }
    if(pKey != NULL && pValue != NULL) 
    {
      if(strcmp(pKey, "channel_count") == 0) //声道
      {
        *nChannel = atoi(pValue);
      }
      else if(strcmp(pKey, "sample_rate") == 0) //采样率
      {
        *nSampleRate = atoi(pValue);
      }
      else if(strcmp(pKey, "sample_sig_bits") == 0) //样本精度
      {
        *nSampleBits = atoi(pValue);
      }
    }
  }
  if(nReturn > 0) //参数解析正确的话，接下来拷贝语音数据
  {
    *nAudioLen = nFileSize - nHeadLen;
    *pAudio = (char*)malloc(*nAudioLen * sizeof(char));
    if(*pAudio == NULL)
    {
      nReturn = -11;
    }
    else
    {
      fseek(fs, nHeadLen, SEEK_SET);
      nDataReadLen = fread(*pAudio, 1, *nAudioLen, fs);
      if (nDataReadLen != *nAudioLen)
      {
        free(*pAudio);
        *pAudio = NULL;
        nReturn = -12;
      }
    }   
  }
  fclose(fs);
  return nReturn;
}

int get_pcm(const char* pFile, char** pAudio, int* nAudioLen)
{
  FILE* fs = fopen(pFile, "rb");
  int nFileSize = 0;
  int nDataReadLen = 0;
  *pAudio = NULL;
  *nAudioLen = 0; 
  if(fs == NULL)
  {
    return -1;
  }
  fseek(fs, 0L, SEEK_END); 
    nFileSize = ftell(fs);
  if(nFileSize <= 0)//空文件
  {
    fclose(fs);
    return -2;
  }
  *nAudioLen = nFileSize;
  *pAudio = (char*)malloc(*nAudioLen);
  if(*pAudio == NULL)
  {
    fclose(fs);
    return -3;
  }
  fseek(fs, 0L, SEEK_SET);
  nDataReadLen = fread(*pAudio, 1, *nAudioLen, fs);
  if (nDataReadLen != *nAudioLen)
  {
    free(*pAudio);
    *pAudio = NULL;
    fclose(fs);
    return -4;
  }
  fclose(fs);
  return 1;
}

void FreeAudio(AudioStruct* pAudioStruct, int nAudioNum)
{
  if(pAudioStruct != NULL && nAudioNum > 0)
  {
    for(int i = 0; i < nAudioNum; ++i)
    {
      if(pAudioStruct[i].pAudio != NULL)
      {
        free(pAudioStruct[i].pAudio);
        pAudioStruct[i].pAudio = NULL;
      }

    }
    free(pAudioStruct);
    pAudioStruct = NULL;
  }
}

int GetAudio(const char* strDir, AudioStruct** pAudioStruct, long* pAudioNum, int nSampleRateDefault)
{
  if(pAudioStruct != NULL)
  {
    *pAudioStruct = NULL;
  }
  if(pAudioNum != NULL)
  {
    *pAudioNum = 0;
  }
  if(strDir == NULL || pAudioStruct == NULL || pAudioNum == NULL)
  {
    return -1;
  }

  void* pFileInst = teach_file_init(strDir);
  if(pFileInst == NULL)
  {
    return -2;
  }
  int nRet = 1;
  int nFileNum = 0;
  if(nRet > 0)
  {
    nFileNum = teach_file_count(pFileInst);
    if(nFileNum <= 0)
    {
      nRet = -3;
    }
  }
  if(nRet > 0)
  {
    *pAudioStruct = (AudioStruct*)malloc(sizeof(AudioStruct) * nFileNum);
    if(pAudioStruct == NULL)
    {
      nRet = -4;
    }
  }
  if(nRet > 0)
  {
    const char* pFile = NULL;
    const char* pExtension = NULL;
    char* pAudio = NULL; //语音数据
    int nAudioLen = 0;
    int nSampleRate = 0;
    int nSampleBits = 0;
    short nChannel = 0;
    int nIndex = 0;
    while(true)
    {
      pFile = teach_file_get(pFileInst);
      if(pFile == NULL)
      {
        break;
      }
      pExtension = strrchr(pFile, '.');//判断文件类型
      if(pExtension == NULL)
      {
        nRet = -5;
        break;
      }
      if(strcmp(pExtension, ".wav")!=0 && strcmp(pExtension, ".pcm")!=0 && strcmp(pExtension, ".nist")!=0)
      {
        nRet = -6;
        break;
      }
      if(strcmp(pExtension, ".wav") == 0)
      {
        nRet = get_wav(pFile, &pAudio, &nAudioLen, &nSampleRate, &nSampleBits, &nChannel);
      }
      else if(strcmp(pExtension, ".nist") == 0)
      {
        nRet = get_nist(pFile, &pAudio, &nAudioLen, &nSampleRate, &nSampleBits, &nChannel);
      }
      else
      {
        nSampleRate = nSampleRateDefault;//默认采样率
        nSampleBits =  16;
        nChannel = 1;
        nRet = get_pcm(pFile, &pAudio, &nAudioLen);
      }
      if(nRet <= 0)
      {
        nRet = -7;
        break;
      }
      (*pAudioStruct)[nIndex].nAudioLength = nAudioLen;
      (*pAudioStruct)[nIndex].nSampleRate = nSampleRate;
      (*pAudioStruct)[nIndex].nBits = nSampleBits;
      (*pAudioStruct)[nIndex].pFile[0] = 0;
      (*pAudioStruct)[nIndex].nChannel = nChannel;
      char gbk_file[512] = {0};
      tcode_utf82gbk(pFile, strlen(pFile), gbk_file, sizeof(gbk_file));
      strcpy((*pAudioStruct)[nIndex].pFile, gbk_file);
      (*pAudioStruct)[nIndex].pAudio = pAudio;
      ++nIndex;
    }
    *pAudioNum = nIndex;
  } 
  teach_file_fini(pFileInst);
  if(nRet <= 0)
  {
    FreeAudio(*pAudioStruct, *pAudioNum);
  }
  return nRet;
}

int main(int argc, char* argv[])
{
    int16_t iReturn = CYVOICE_EXIT_SUCCESS;
    ArgThread myArgList[NUM_INSTANCE];
    int i;

    char *config_file = NULL;
  char test_dir[_MAX_PATH];
    char *gram_name = NULL;
    char *gram_file = NULL;
    int num_instance = 1;
    int sample_rate = 8000;
    uint16_t audioFormat = CYVOICE_AUDIO_PCM_8K;
    FILE * fp_sil = NULL;
    int silSize = 0;

    gt_max_end_sil = 1;
    gt_max_begin_sil = 20;
    gt_max_speech = 20;
    gt_min_speech = 0.2;
    gt_nbest = 0;
  gt_long_speech_online = 1;
  gt_long_speech = 0;
  gt_uttNum = 0;
  gt_uttIndex = 0;
  gt_uttNumTemp = 0;
  gt_tt = 0;
  gt_loop = 0;
  gt_loopTemp = 0;
  gt_bAlign = false;

    // read arg
    if(argc < 7) {
        printf("TestSDKThread.exe -c config_file -t test_dir  -g gram_name -gf jsgf_file -u user_dict_file -n num_instance\n");
        return -1;
    }

    for (i=1; i<argc; i++)
    {
        if(!strcmp(argv[i],"-c") && (i+1 < argc))
        {
            config_file = argv[i+1];
            i++;
        }
        else if(!strcmp(argv[i],"-t") && (i+1 < argc))
        {
      test_dir[0] = '\0';
            strcpy(test_dir, argv[i+1]);
            i++;
        }     
        else if(!strcmp(argv[i],"-gf") && (i+1 < argc))
        {
            gram_file = argv[i+1];
            i++;
        }

        else if(!strcmp(argv[i],"-n") && (i+1 < argc))
        {
            num_instance = atoi(argv[i+1]);
            i++;
        }
        else if(!strcmp(argv[i],"-rate") && (i+1 < argc))
        {
            sample_rate = atoi(argv[i+1]);
            i++;
        }
    else if(!strcmp(argv[i], "-loop") && (i + 1 < argc))
    {
      gt_loop = atoi(argv[i+1]);
      i++;
    }
        else if(!strcmp(argv[i],"-tt") && (i+1 < argc))
        {
            gt_tt = atoi(argv[i+1]) * 1000;
            i++;
        }
    else if(!strcmp(argv[i],"-utt") && (i+1 < argc))
    {
      gt_uttNum = atoi(argv[i+1]);
            i++;
    }
        else if (!strcmp(argv[i], "-end") && (i + 1 < argc))
        {
            gt_max_end_sil = atof(argv[i + 1]);
            i++;
        }
    else if(!strcmp(argv[i], "-begin") && (i + 1 < argc))
    {
      gt_max_begin_sil = atof(argv[i + 1]);
      i++;
    }
        else if (!strcmp(argv[i], "-sp") && (i + 1 < argc))
        {
            gt_max_speech = atof(argv[i + 1]);
            i++;
        }
        else if (!strcmp(argv[i], "-nb") && (i + 1 < argc))
        {
            gt_nbest = atoi(argv[i + 1]);
            i++;
        }
    else if (!strcmp(argv[i], "-online") && (i + 1 < argc))
    {
      gt_long_speech_online = atoi(argv[i + 1]);
      i++;
    }
    else if (!strcmp(argv[i], "-long") && (i + 1 < argc))
    {
      gt_long_speech = atoi(argv[i + 1]);
      i++;
    }
    else if (!strcmp(argv[i], "-size") && (i + 1 < argc))
    {
      gt_BufferSize = atoi(argv[i + 1]);
      ++i;
    }
    else if (!strcmp(argv[i], "-p") && (i + 1 < argc))
    {
      gt_UserPartial = atoi(argv[i + 1]);
      ++i;
    }
    else if(!strcmp(argv[i], "-align") && (i + 1 < argc))
    {
      gt_bAlign = (atoi(argv[i + 1]) > 0 ? true : false);
    }
  }

  strcat(test_dir, "/*.(pcm|wav|nist)");
  gt_nFileNum = 0;
  gt_pAudioStruct = NULL;
  tlog_init("./clientLog/%Y-%m-%d/%Y-%m-%d.*.log");

  tlog_printfx("config_file = %s\n", config_file);
    tlog_printfx("test_dir = %s\n", test_dir);
    tlog_printfx("gram_file = %s\n", (gram_file == NULL ? "":gram_file));
    tlog_printfx("num_instance = %d\n", num_instance);
    if( num_instance <=0 )
  {
        num_instance = 1;
  }
    else if(num_instance > NUM_INSTANCE)
    {
     num_instance = NUM_INSTANCE;
  }

    tlog_printfx("sample_rate = %d\n", sample_rate);
    tlog_printfx("max_end_sil = %f\n", gt_max_end_sil);
    tlog_printfx("max_begin_sil = %f\n", gt_max_begin_sil);
    tlog_printfx("max_speech = %f\n", gt_max_speech);
    tlog_printfx("min_speech = %f\n", gt_min_speech);
    tlog_printfx("nbest = %d\n", gt_nbest);
  tlog_printfx("long_speech_online = %d\n", gt_long_speech_online);
  tlog_printfx("long_speech = %d\n", gt_long_speech);
  tlog_printfx("test_time = %llu\n", gt_tt);
  tlog_printfx("test_utt_num = %lu\n", gt_uttNum);
    tlog_printfx("\n");

  printf("load file...\n");
  tlog_printfx("load file...");
  int nRet = GetAudio(test_dir, &gt_pAudioStruct, &gt_nFileNum, sample_rate);
  if(nRet <= 0)
  {
    printf("test file is err\n");
    tlog_printfx("test file is err\n");
    FreeAudio(gt_pAudioStruct, gt_nFileNum);
    tlog_fini();
    return -1;
  }
  printf("get %d audio files\n", gt_nFileNum);
  tlog_printfx("get %d audio files", gt_nFileNum);
  if(gt_nFileNum <= 0)
  {
    FreeAudio(gt_pAudioStruct, gt_nFileNum);
    tlog_fini();
    return -3;
  }
  printf("init instance...\n");
  tlog_printfx("init instance...");
   // create n instances
    for (i=0; i<num_instance; i++) 
  {
        myArgList[i].index = i;
    myArgList[i].bSearchExit = FALSE;
    myArgList[i].bSpeechExit = FALSE;
    myArgList[i].TotalProcessTime = 0;
    myArgList[i].Data1Time = 0;
    myArgList[i].Data2Time = 0;
    myArgList[i].SearchTime = 0;
    myArgList[i].GetResultTime = 0;
    myArgList[i].uttTotalProcessTime = 0;
    myArgList[i].uttData1Time = 0;
    myArgList[i].uttData2Time = 0;
    myArgList[i].uttSearchTime = 0;
    myArgList[i].uttGetResultTime = 0;
    myArgList[i].TotalSpeechTime = 0;
    myArgList[i].uttTotalSpeechTime = 0;
    }

  if(i < num_instance)
  {
    printf("init instance err\n");
    tlog_printfx("init instance err\n");
    FreeAudio(gt_pAudioStruct, gt_nFileNum);
    tlog_fini();
    return -2;
  }

  tpart_timer();
  system("pause");
  printf("loading model...\n");
  tlog_printfx("loading model...\n");
    // init the engine
    if(!iReturn) {
        iReturn = cyVoiceInit(config_file);
        if(iReturn != CYVOICE_EXIT_SUCCESS) {
            print_return_code("cyVoiceInit",iReturn);
            iReturn = CYVOICE_EXIT_FAILURE;
        }
    }
  
    // get all the static grammars
    if(!iReturn) {
        iReturn = cyVoiceGetStaticGrammar(&gt_grammar_number, &gt_inside_grammar);

        if(iReturn != CYVOICE_EXIT_SUCCESS) {
            print_return_code("cyVoiceInit",iReturn);
            iReturn = CYVOICE_EXIT_FAILURE;
        }

        if(!iReturn) {
            tlog_printfx("-------------------------------\n");
            tlog_printfx("gramNumber = %d\n", gt_grammar_number);
            for(i=0; i<gt_grammar_number; i++){
                tlog_printfx("%d: gramName=%s gramFile=%s dictFile=%s\n", i+1, gt_inside_grammar[i].szGrammarName, gt_inside_grammar[i].szFileName, gt_inside_grammar[i].szDictFile);
            }
            tlog_printfx("-------------------------------\n");
        }
    }

  printf("recognize...\n");

  std::vector<std::thread*> threads;
  if(!iReturn) 
  {
    for (i=0; i<num_instance; i++) 
    {
      std::thread* th = new std::thread(ThreadDataFiles, &myArgList[i]);
      threads.push_back(th);
    }

    while(1)
    {
      usleep(500*1000);
#ifdef _WIN32      
      BOOL bThread = TRUE;

      SIZE_T set = 0;
      SIZE_T page = 0;
      PROCESS_MEMORY_COUNTERS pmc;           

      if(GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))  
      {  
        set = pmc.WorkingSetSize;
        page = pmc.PagefileUsage;
      }

      for(i=0; i<num_instance; i++) 
      {
        bThread &= myArgList[i].bSpeechExit;
      }

      if(bThread)
      {
        break;
      }

      for(i=0; i<num_instance; i++)
      {
        usleep(5*1000);

        if( kbhit() )
        {
          char key = _getch();
          if(key == 'a') 
          {
            tlog_printfx("\ncyVoiceAbort\n");
            //cyVoiceAbort(myArgList[i].handle);
          }
        }
      }
#endif
    } // end while
  }
   
unload:
  printf("unload model...\n");
  // release ASR engine
  for (i=0; i<num_instance; i++) 
  {
    if(myArgList[i].handle) 
    {
      iReturn = cyVoiceReleaseInstance(myArgList[i].handle);    
      if(iReturn != CYVOICE_EXIT_SUCCESS) 
      {
        print_return_code("cyVoiceReleaseInstance",iReturn);
        iReturn = CYVOICE_EXIT_FAILURE;
        break;
      }
    }   
  }
  // Uninit the engine
  if(!iReturn) 
  {
    iReturn = cyVoiceUninit();
    if(iReturn != CYVOICE_EXIT_SUCCESS) 
    {
      print_return_code("cyVoiceUnInit",iReturn);
      iReturn = CYVOICE_EXIT_FAILURE;;
    }
  }
  FreeAudio(gt_pAudioStruct, gt_nFileNum);

  tlog_printfx("app exit");
  tlog_fini();

  printf("exit, press any key to continue...\n");
  getchar();
  return iReturn;
}
