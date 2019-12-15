#include "cyVoice.h"
#include <iostream>
#include "feat/feature-mfcc.h"
#include "feat/feature-fbank.h"
#include "feat/online-feature.h"
#include "feat/wave-reader.h"
#include "matrix/kaldi-matrix.h"
#include "transform/transform-common.h"
#include "ivector/voice-activity-detection.h"
#include "transform/cmvn.h"

#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "tree/context-dep.h"
#include "hmm/transition-model.h"
#include "fstext/fstext-lib.h"
#include "decoder/decoder-wrappers.h"
#include "nnet3/nnet-am-decodable-simple.h"
#include "nnet3/nnet-utils.h"
#include "base/timer.h"

using namespace kaldi;
using namespace kaldi::nnet3;
typedef kaldi::int32 int32;
using fst::SymbolTable;
using fst::Fst;
using fst::StdArc;

class CAsrHandler
{
public:
  CAsrHandler();
  virtual ~CAsrHandler();

//private:
  void Init();
  void Uninit();

  ParseOptions* po;
  std::string cfg_file;
  bool allow_partial;

  LatticeFasterDecoderConfig lat_decoder_config;
  NnetSimpleComputationOptions decodable_opts;
  std::string word_syms_filename;
  std::string ivector_rspecifier;
  std::string online_ivector_rspecifier;
  std::string utt2spk_rspecifier;
  int32 online_ivector_period;

  std::string model_in_filename;
  std::string fst_in_str;
  std::string lattice_wspecifier;
  std::string words_wspecifier;
  std::string alignment_wspecifier;

  TransitionModel trans_model;
  AmNnetSimple am_nnet;
  fst::SymbolTable *word_syms;

  double tot_like;
  kaldi::int64 frame_count;

  Fst<StdArc> *decode_fst;
  LatticeFasterDecoder *lat_decoder;
  CachingOptimizingCompiler *compiler;

  //feat
  FbankOptions fbank_op;
  VadEnergyOptions vad_opts;
  OnlineFbank *online_fbank;
  std::list<Vector<BaseFloat>* > sd_feat_frames;
  std::vector<Vector<BaseFloat>* > sd_vad_results;
  Matrix<BaseFloat> online_feats;
};

CAsrHandler::CAsrHandler()
{
  Init();
}

CAsrHandler::~CAsrHandler()
{
  Uninit();
}

void CAsrHandler::Init()
{
  online_ivector_period = 0;

  po = new ParseOptions("libasr");
  lat_decoder_config.Register(po);
  decodable_opts.Register(po);
  po->Register("word-symbol-table", &word_syms_filename,
              "Symbol table for words [for debug output]");
  po->Register("allow-partial", &allow_partial,
              "If true, produce output even if end state was not reached.");
  po->Register("ivectors", &ivector_rspecifier, "Rspecifier for "
              "iVectors as vectors (i.e. not estimated online); per utterance "
              "by default, or per speaker if you provide the --utt2spk option.");
  po->Register("utt2spk", &utt2spk_rspecifier, "Rspecifier for "
              "utt2spk option used to get ivectors per speaker");
  po->Register("online-ivectors", &online_ivector_rspecifier, "Rspecifier for "
              "iVectors estimated online, as matrices.  If you supply this,"
              " you must set the --online-ivector-period option.");
  po->Register("online-ivector-period", &online_ivector_period, "Number of frames "
              "between iVectors in matrices supplied to the --online-ivectors "
              "option");

  int argc = 5;
  //char* argv[argc] = {
  char* argv[5] = {
    "libasr",
    "--config=nnet3-offline.cfg",
    "cvte/exp/chain/tdnn/final.mdl",
    "cvte/exp/chain/tdnn/graph/HCLG.fst",
    "ark:|lattice-scale --acoustic-scale=10.0 ark:- ark,t:lat1-scale"
  };

  po->Read(argc, (const char* const *)argv);

  model_in_filename = po->GetArg(1);
  fst_in_str = po->GetArg(2);
  lattice_wspecifier = po->GetArg(3);
  words_wspecifier = po->GetOptArg(4);
  alignment_wspecifier = po->GetOptArg(5);
}

void CAsrHandler::Uninit()
{
  delete po;

  if(online_fbank)
    delete online_fbank;
  if(decode_fst)
    delete decode_fst;
  if(lat_decoder)
    delete lat_decoder;
  if(compiler)
    delete compiler;
  if(word_syms)
    delete word_syms;
  
  online_fbank = nullptr;
  decode_fst = nullptr;
  lat_decoder = nullptr;
  compiler = nullptr;
  word_syms = nullptr;
}

void GetSDOutput(OnlineFeatureInterface *a,
               std::list<Vector<BaseFloat>* >& feat_frames) 
{
  feat_frames.clear();
  int32 dim = a->Dim();
  int32 frame_idx = 0;
  OnlineCacheFeature cache(a);
  
  int32 cache_cnt = cache.NumFramesReady();

  while (frame_idx < cache_cnt) {
    Vector<BaseFloat> garbage(dim);
    cache.GetFrame(frame_idx, &garbage);
    feat_frames.push_back(new Vector<BaseFloat>(garbage));
    if (cache.IsLastFrame(frame_idx))
      break;
    frame_idx++;
  }
  KALDI_ASSERT(feat_frames.size() == a->NumFramesReady());
  cache.ClearCache();
}


////////////////////////////////////////////////////////////////////

int16_t cyVoiceInit(char *configFile)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceUninit()
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceCreateInstance(CYVOICE_HANDLE *handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceCreateInstanceEx(CYVOICE_HANDLE *handle)
{
  //TODO

  CAsrHandler* hd = new CAsrHandler();

  {
    bool binary;
    Input ki(hd->model_in_filename, &binary);
    hd->trans_model.Read(ki.Stream(), binary);
    hd->am_nnet.Read(ki.Stream(), binary);
    SetBatchnormTestMode(true, &(hd->am_nnet.GetNnet()));
    SetDropoutTestMode(true, &(hd->am_nnet.GetNnet()));
    CollapseModel(CollapseModelConfig(), &(hd->am_nnet.GetNnet()));
  }

  RandomAccessBaseFloatMatrixReader online_ivector_reader(
      hd->online_ivector_rspecifier);
  RandomAccessBaseFloatVectorReaderMapped ivector_reader(
      hd->ivector_rspecifier, hd->utt2spk_rspecifier);
  
  if (hd->word_syms_filename != "")
    if (!(hd->word_syms = fst::SymbolTable::ReadText(hd->word_syms_filename)))
      KALDI_ERR << "Could not read symbol table from file "
                 << hd->word_syms_filename;

  // this compiler object allows caching of computations across
  // different utterances.
  hd->compiler = new CachingOptimizingCompiler(hd->am_nnet.GetNnet(),
                                    hd->decodable_opts.optimize_config);

  hd->decode_fst = fst::ReadFstKaldiGeneric(hd->fst_in_str);
  hd->lat_decoder = new LatticeFasterDecoder(*(hd->decode_fst), hd->lat_decoder_config);

  *handle = hd;
  std::cout << "load model finish!" << std::endl;

  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceReleaseInstance(CYVOICE_HANDLE handle)
{
  //TODO
  if(handle)
  {
    CAsrHandler* hd = (CAsrHandler*)handle;
    delete hd;
  }
  
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceGetStaticGrammar(int32_t *p_grammar_number, CYVOICE_GRAMMAR **pp_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceGetActiveGrammar(CYVOICE_HANDLE handle, int32_t *p_grammar_number, CYVOICE_GRAMMAR **pp_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceRenameGrammar(CYVOICE_GRAMMAR *p_grammar, char *new_grammar_name)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceOpenGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceInitGrammar(CYVOICE_GRAMMAR *p_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceInitGrammarBuffer(CYVOICE_GRAMMAR *p_grammar, char *grammar_buffer, int buffer_size)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceLoadDgGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceLoadDgGrammarBuffer(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar, char *grammar_buffer, int buffer_size)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceCloseGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceCloseAllGrammar(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceUninitGrammar(CYVOICE_GRAMMAR *p_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceUnloadDgGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceActivateGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceDeactivateGrammar(CYVOICE_HANDLE handle, CYVOICE_GRAMMAR *p_grammar)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

/*
 *  To deacivate all the grammar in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceDeactivateAllGrammar(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceSetParameter(CYVOICE_HANDLE handle, uint16_t parameter, void *value_ptr)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceGetParameter(CYVOICE_HANDLE handle, uint16_t parameter, void *value_ptr)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

/**
 * To enable or disable to save utterance
 *
 *     input: handle, [in] pointer to an engine instance
 *     input: uttPath, [in] set a path to enable, or to set to NULL to disable
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 */
int16_t cyVoiceSaveUtterance(CYVOICE_HANDLE handle, const char *uttPath)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceGetSavePathName(CYVOICE_HANDLE handle, uint16_t bufferLength, char *pszBuffer)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceStart(CYVOICE_HANDLE handle)
{
  //TODO
  if(handle)
  {
    CAsrHandler* hd = (CAsrHandler*)handle;
    hd->fbank_op.use_energy = true;
    hd->fbank_op.mel_opts.num_bins = 40;

    hd->vad_opts.vad_energy_threshold = 5.5;
    hd->vad_opts.vad_energy_mean_scale = 0.5;

    hd->online_fbank = new OnlineFbank(hd->fbank_op);
  }

  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceReset(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

/**
 * To stop the search
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceStop(CYVOICE_HANDLE handle)
{
  //TODO
  if(handle)
  {
    CAsrHandler* hd = (CAsrHandler*)handle;

    hd->online_fbank->InputFinished();

    std::list<Vector<BaseFloat>* > feat_frames;
    GetSDOutput(hd->online_fbank, feat_frames);
    hd->sd_feat_frames.insert(hd->sd_feat_frames.end(), feat_frames.begin(), feat_frames.end());

    
    if(!hd->sd_feat_frames.empty())
    {
      hd->online_feats.Resize(hd->sd_feat_frames.size(), hd->online_fbank->Dim());
      int idx = 0;
      for (auto it(hd->sd_feat_frames.begin()); it!=hd->sd_feat_frames.end(); ++it) 
      {
        hd->online_feats.CopyRowFromVec(**it, idx);
        delete *it;
        idx++;
      }
      hd->sd_feat_frames.clear();

      //Vector<BaseFloat> vad_result(online_feats.NumRows());
      //ComputeVadEnergy(opts, online_feats, &vad_result);
      //utt = "uid" + std::to_string(num_piece);
      //ws = "t,ark:vad-" + utt + ".ark";
      //BaseFloatVectorWriter vad_writer(ws);
      //vad_writer.Write(utt, vad_result);
    
      Matrix<double> cmvn_stats;
      InitCmvnStats(hd->online_feats.NumCols(), &cmvn_stats);
      AccCmvnStats(hd->online_feats, NULL, &cmvn_stats);
      ApplyCmvn(cmvn_stats, false, &hd->online_feats);   

      delete hd->online_fbank;
      hd->online_fbank = nullptr;
    }
  }
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceStopFeat(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceProcessData(CYVOICE_HANDLE handle, void *speechBuffer, uint32_t bufferSize, uint16_t *statusCode)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceProcessDataToFeatOpen(CYVOICE_HANDLE handle, char *str_feat, char *str_feat_cmn, char *str_feat_cmn_delta)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceProcessDataToFeatClose(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceProcessDataToFeat(CYVOICE_HANDLE handle, char *str_key)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}


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
int16_t cyVoiceProcessData1(CYVOICE_HANDLE handle, void *speechBuffer, uint32_t *bufferSize, uint16_t *statusCode, int *num_sd_result, SD_RESULT **sd_result)
{
  //TODO
  if(handle == nullptr || *bufferSize <= 0)
  {
    *bufferSize = 0;
    return CYVOICE_EXIT_INVALID_PARM;
  }

  CAsrHandler* hd = (CAsrHandler*)handle;

  if((*bufferSize) % 2 == 1)
    (*bufferSize)--;

  Matrix<BaseFloat> data_;
  data_.Resize(0, 0);  // clear the data.
  BaseFloat samp_freq_ = static_cast<BaseFloat>(16000);  //!!! only support 16k 16bit

  int16 *data_ptr = reinterpret_cast<int16*>(speechBuffer);

  // The matrix is arranged row per channel, column per sample.
  data_.Resize(1, *bufferSize/2);
  for (uint32 i = 0; i < data_.NumCols(); ++i) {
    for (uint32 j = 0; j < data_.NumRows(); ++j) {
      int16 k = *data_ptr++;
      data_(j, i) =  k;
    }
  }

  SubVector<BaseFloat> waveform(data_, 0);
  hd->online_fbank->AcceptWaveform(samp_freq_, waveform);

  //std::list<Vector<BaseFloat>* > feat_frames;
  //GetSDOutput(&online_mfcc, feat_frames);
  //sd_feat_frames.insert(sd_feat_frames.end(), feat_frames.begin(), feat_frames.end());
  //Vector<BaseFloat> vad_result(online_mfcc_feats.NumRows());
  //ComputeVadEnergy(opts, online_mfcc_feats, &vad_result);

  //TODO vad !!!

  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceProcessData2(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceProcessFeat(CYVOICE_HANDLE handle, float **feats, int row, int col, const char *utt_key)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceProcessQueryResult(CYVOICE_HANDLE handle, void *speechBuffer, uint32_t bufferSize, int32_t *count, char **result, int32_t *score)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceQueryStatus(CYVOICE_HANDLE handle, uint16_t *statusCode,  float *speech_begin, float *speech_end)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceQueryStatusEx(CYVOICE_HANDLE handle, uint16_t *statusCode, int *num_sd_result, SD_RESULT **sd_result, int *num_sr_result, CYVOICE_RESULT **sr_result)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}


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
int16_t cyVoiceSearchForward(CYVOICE_HANDLE handle, uint16_t *statusCode)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceSearchForwardPartial(CYVOICE_HANDLE handle, uint16_t *statusCode)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

/**
 * To abort the search
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceAbort(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceQueryResult(CYVOICE_HANDLE handle, uint16_t *statusCode, char *result, int32_t *score)
{
  //TODO
  if(handle == nullptr)
    return CYVOICE_EXIT_INVALID_PARM;
  CAsrHandler* hd = (CAsrHandler*)handle;
  if (hd->online_feats.NumRows() > 0) 
  {
    const Matrix<BaseFloat> *online_ivectors = NULL;
    const Vector<BaseFloat> *ivector = NULL;
    DecodableAmNnetSimple nnet_decodable(
      hd->decodable_opts, hd->trans_model, hd->am_nnet,
      hd->online_feats.ColRange(1, hd->fbank_op.mel_opts.num_bins), 
      ivector, online_ivectors,
      hd->online_ivector_period, hd->compiler);

    double like;
    Int32VectorWriter words_writer(hd->words_wspecifier);
    Int32VectorWriter alignment_writer(hd->alignment_wspecifier);

    bool determinize = hd->lat_decoder_config.determinize_lattice;
    CompactLatticeWriter compact_lattice_writer;
    LatticeWriter lattice_writer;
    if (! (determinize ? compact_lattice_writer.Open(hd->lattice_wspecifier)
         : lattice_writer.Open(hd->lattice_wspecifier)))
    KALDI_ERR << "Could not open table for writing lattices: "
               << hd->lattice_wspecifier;

    if (DecodeUtteranceLatticeFaster(
      *hd->lat_decoder, nnet_decodable, hd->trans_model, hd->word_syms, "utt01",
      hd->decodable_opts.acoustic_scale, determinize, hd->allow_partial,
      &alignment_writer, &words_writer, &compact_lattice_writer,
      &lattice_writer,
      &like)) 
    {
        hd->tot_like += like;
        hd->frame_count += nnet_decodable.NumFramesReady();
    }
  }

  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceQueryResultPartial(CYVOICE_HANDLE handle, uint16_t *statusCode, char *result, int32_t *score)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceQueryResults(CYVOICE_HANDLE handle, uint16_t *statusCode, int *num_result, CYVOICE_RESULT *st_result)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceQueryResultAlign(CYVOICE_HANDLE handle, CYVOICE_RESULT_ALIGN *st_result_align)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}


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
int16_t cyVoiceQueryActiveResults(CYVOICE_HANDLE handle, int active_index, uint16_t *statusCode, int *num_active_grammar, int *num_result, CYVOICE_RESULT *st_result)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceQueryAllActiveResults(CYVOICE_HANDLE handle, uint16_t *statusCode, int *num_result, CYVOICE_RESULT *st_result, int max_num_result)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceQueryGrammarResults(CYVOICE_HANDLE handle, char *grammar_name, uint16_t *statusCode, int *num_result, CYVOICE_RESULT *st_result)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}


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
int16_t cyVoiceGetNbestResult(CYVOICE_HANDLE handle, int16_t n, char **result, int32_t *score)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

/**
 * To get the begin point of speech
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: the begin point of speech (in samples, 8K sampling rate, 16bits)
 *            
 */
int32_t cyVoiceGetSpeechBegin(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceSetSpeechBegin(CYVOICE_HANDLE handle, int32_t speech_begin)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

/**
 * To get the end point of speech
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: the begin point of speech (in samples, 8K sampling rate, 16bits)
 *            
 */
int32_t cyVoiceGetSpeechEnd(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceSetSpeechEnd(CYVOICE_HANDLE handle, int32_t speech_end)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceLoadUserDict(CYVOICE_HANDLE handle, char *dict_file)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceLoadUserDictBuffer(CYVOICE_HANDLE handle, char *dict_file_buffer, int buffer_size)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

/*
 *  To unload a user dict in the instance
 *
 *     input: handle, [in] pointer to an engine instance
 *
 *    return: CYVOICE_EXIT_SUCCESS
 *            CYVOICE_EXIT_INVALID_PARM
 *            CYVOICE_EXIT_INVALID_STATE
 */
int16_t cyVoiceUnloadUserDict(CYVOICE_HANDLE handle)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}

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
int16_t cyVoiceCalConfScore(CYVOICE_HANDLE handle, float rescale_slope, float rescale_bias, float raw_score, float raw_score_background, int *confidence_score)
{
  //TODO
  return CYVOICE_EXIT_SUCCESS;
}
