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

  bool allow_partial;

  std::string ivector_rspecifier;
  std::string online_ivector_rspecifier;
  std::string utt2spk_rspecifier;

  std::string lattice_wspecifier;
  std::string words_wspecifier;
  std::string alignment_wspecifier;  

  double tot_like;
  kaldi::int64 frame_count;

  LatticeFasterDecoder *lat_decoder;
  CachingOptimizingCompiler *compiler;
  DecodableAmNnetSimple *nnet_decodable;
  CompactLatticeWriter compact_lattice_writer;
  LatticeWriter lattice_writer;

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
  lattice_wspecifier = "ark:|lattice-scale --acoustic-scale=10.0 ark:- ark,t:lat1-scale";
  words_wspecifier = "";
  alignment_wspecifier = "";
}

void CAsrHandler::Uninit()
{
  if(online_fbank)
    delete online_fbank;
  if(lat_decoder)
    delete lat_decoder;
  if(compiler)
    delete compiler;
  if(nnet_decodable)
    delete nnet_decodable;
  
  online_fbank = nullptr;
  lat_decoder = nullptr;
  compiler = nullptr;
  nnet_decodable = nullptr;
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

class CAsrModuleManager
{
public:
  CAsrModuleManager()
  {
    po = nullptr;
    decode_fst = nullptr;
    word_syms = nullptr;
    am_module_file = "cvte/exp/chain/tdnn/final.mdl";
    fst_file = "cvte/exp/chain/tdnn/graph/HCLG.fst";
    word_syms_filename = "cvte/exp/chain/tdnn/graph/words.txt";
  }

  virtual ~CAsrModuleManager()
  {
    if(decode_fst)
      delete decode_fst;
    if(word_syms)
      delete word_syms;
    if(po)
      delete po;
    decode_fst = nullptr;
    word_syms = nullptr;
    po = nullptr;
  }

  void Init(const std::string& cfgfile)
  {
    po = new ParseOptions("libasr");
    lat_decoder_config.Register(po);
    decodable_opts.Register(po);
    po->Register("use-gpu", &use_gpu,
            "yes|no|optional|wait, only has effect if compiled with CUDA");
    int argc = 2;
    char* argv[argc];
    for(int i=0; i<argc; i++)
    {
      argv[i] = (char*)malloc(128);
      if(argv[i] == nullptr)
        break;
    }
    strcpy(argv[0], "libasr\0");
    std::string cfgfile_ = "--config=" + cfgfile;
    strcpy(argv[1], cfgfile_.c_str());
    po->Read(argc, (const char* const *)argv);
    for(int i=0; i<argc; i++)
    {
      free(argv[i]);
    }
  }

  bool LoadStaticModules()
  {
    bool binary;
    Input ki(am_module_file, &binary);
    trans_model.Read(ki.Stream(), binary);
    am_nnet.Read(ki.Stream(), binary);
    SetBatchnormTestMode(true, &(am_nnet.GetNnet()));
    SetDropoutTestMode(true, &(am_nnet.GetNnet()));
    CollapseModel(CollapseModelConfig(), &(am_nnet.GetNnet()));
    decode_fst = fst::ReadFstKaldiGeneric(fst_file);

    if (!(word_syms = fst::SymbolTable::ReadText(word_syms_filename)))
      KALDI_ERR << "Could not read symbol table from file " << word_syms_filename;

    return true;
  }

//member
  std::string use_gpu;
  std::string am_module_file;
  std::string fst_file;
  std::string word_syms_filename;

  TransitionModel trans_model;
  AmNnetSimple am_nnet;
  Fst<StdArc> *decode_fst;
  fst::SymbolTable *word_syms;

  LatticeFasterDecoderConfig lat_decoder_config;
  NnetSimpleComputationOptions decodable_opts;
  ParseOptions* po;
};

////////////////////////////////////////////////////////////////////

CAsrModuleManager *g_module_mng = nullptr;

int16_t cyVoiceInit(char *configFile)
{
  if(!configFile)
  {
    return CYVOICE_EXIT_INVALID_PARM;
  }

  g_module_mng = new CAsrModuleManager();
  g_module_mng->Init(configFile);

#if HAVE_CUDA==1
    CuDevice::RegisterDeviceOptions(g_module_mng->po);
    CuDevice::Instantiate().AllowMultithreading();
    CuDevice::Instantiate().SelectGpuId(g_module_mng->use_gpu);
#endif

  if(g_module_mng->LoadStaticModules() == false)
    return CYVOICE_EXIT_FAILURE;
  std::cout << "load model finish!" << std::endl;
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceUninit()
{
#if HAVE_CUDA==1
  CuDevice::Instantiate().PrintProfile();
#endif

  if(g_module_mng)
    delete g_module_mng;
  g_module_mng = nullptr;
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceCreateInstance(CYVOICE_HANDLE *handle)
{
  return cyVoiceCreateInstanceEx(handle);
  //return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceCreateInstanceEx(CYVOICE_HANDLE *handle)
{
  CAsrHandler* hd = new CAsrHandler();

  // this compiler object allows caching of computations across
  // different utterances.
  hd->compiler = new CachingOptimizingCompiler(g_module_mng->am_nnet.GetNnet(),
                                    g_module_mng->decodable_opts.optimize_config);

  hd->lat_decoder = new LatticeFasterDecoder(*(g_module_mng->decode_fst), g_module_mng->lat_decoder_config);

  *handle = hd;
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceReleaseInstance(CYVOICE_HANDLE handle)
{
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

    Int32VectorWriter words_writer(hd->words_wspecifier);
    Int32VectorWriter alignment_writer(hd->alignment_wspecifier);

    bool determinize = g_module_mng->lat_decoder_config.determinize_lattice;

    if (! (determinize ? hd->compact_lattice_writer.Open(hd->lattice_wspecifier)
         : hd->lattice_writer.Open(hd->lattice_wspecifier)))
    KALDI_ERR << "Could not open table for writing lattices: "
               << hd->lattice_wspecifier;
    
    hd->lat_decoder->InitDecoding();
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
      //ApplyCmvn(cmvn_stats, false, &hd->online_feats);   

      delete hd->online_fbank;
      hd->online_fbank = nullptr;
    }
    hd->nnet_decodable = new DecodableAmNnetSimple(
      g_module_mng->decodable_opts, g_module_mng->trans_model, g_module_mng->am_nnet,
      hd->online_feats.ColRange(1, hd->fbank_op.mel_opts.num_bins), 
      NULL, NULL, 0, hd->compiler);

    hd->lat_decoder->AdvanceDecoding(hd->nnet_decodable, hd->online_feats.NumRows());
    hd->lat_decoder->FinalizeDecoding();
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
  using fst::VectorFst;
  if(handle == nullptr)
    return CYVOICE_EXIT_INVALID_PARM;
  CAsrHandler* hd = (CAsrHandler*)handle;

  //if (!decoder.ReachedFinal()) {
  //  if (allow_partial) {
  //    KALDI_WARN << "Outputting partial output for utterance " << utt
  //               << " since no final-state reached\n";
  //  } else {
  //    KALDI_WARN << "Not producing output for utterance " << utt
  //               << " since no final-state reached and "
  //               << "--allow-partial=false.\n";
  //    return false;
  //  }
  //}

  double likelihood;
  LatticeWeight weight;
  int32 num_frames;
  { // First do some stuff with word-level traceback...
    VectorFst<LatticeArc> decoded;
    if (!hd->lat_decoder->GetBestPath(&decoded))
      // Shouldn't really reach this point as already checked success.
      KALDI_ERR << "Failed to get traceback for utterance " << "utt";

    std::vector<int32> alignment;
    std::vector<int32> words;
    GetLinearSymbolSequence(decoded, &alignment, &words, &weight);
    num_frames = alignment.size();

    if (g_module_mng->word_syms != NULL) {
      std::cerr << "utt" << ' ';
      for (size_t i = 0; i < words.size(); i++) {
        std::string s = g_module_mng->word_syms->Find(words[i]);
        if (s == "")
          KALDI_ERR << "Word-id " << words[i] << " not in symbol table.";
        std::cerr << s << ' ';
      }
      std::cerr << '\n';
    }
    likelihood = -(weight.Value1() + weight.Value2());
  }

  // Get lattice, and do determinization if requested.
  Lattice lat;
  hd->lat_decoder->GetRawLattice(&lat);
  if (lat.NumStates() == 0)
    KALDI_ERR << "Unexpected problem getting lattice for utterance " << "utt";
  fst::Connect(&lat);
  if (g_module_mng->lat_decoder_config.determinize_lattice) 
  {
    CompactLattice clat;
    if (!DeterminizeLatticePhonePrunedWrapper(
            g_module_mng->trans_model,
            &lat,
            g_module_mng->lat_decoder_config.lattice_beam,
            &clat,
            g_module_mng->lat_decoder_config.det_opts))
      KALDI_WARN << "Determinization finished earlier than the beam for "
                 << "utterance " << "utt";
    // We'll write the lattice without acoustic scaling.
    if (g_module_mng->decodable_opts.acoustic_scale != 0.0)
      fst::ScaleLattice(fst::AcousticLatticeScale(1.0 / g_module_mng->decodable_opts.acoustic_scale), &clat);
    hd->compact_lattice_writer.Write("utt", clat);
  } else {
    // We'll write the lattice without acoustic scaling.
    if (g_module_mng->decodable_opts.acoustic_scale != 0.0)
      fst::ScaleLattice(fst::AcousticLatticeScale(1.0 / g_module_mng->decodable_opts.acoustic_scale), &lat);
    hd->lattice_writer.Write("utt", lat);
  }
  KALDI_LOG << "Log-like per frame for utterance " << "utt" << " is "
            << (likelihood / num_frames) << " over "
            << num_frames << " frames.";
  KALDI_VLOG(2) << "Cost for utterance " << "utt" << " is "
                << weight.Value1() << " + " << weight.Value2();
  //*like_ptr = likelihood;

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
