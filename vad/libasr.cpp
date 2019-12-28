#include "cyVoice.h"
#include <iostream>
#include "online2/online-nnet2-feature-pipeline.h"
#include "online2/onlinebin-util.h"
#include "online2/online-timing.h"
#include "online2/online-endpoint.h"
#include "fstext/fstext-lib.h"
#include "lat/lattice-functions.h"
#include "util/kaldi-thread.h"
#include "nnet3/nnet-utils.h"
#include "feat/online-feature.h"
#include "nnet3-decoding.h"

using namespace kaldi;
using namespace kaldi::nnet3;
typedef kaldi::int32 int32;
using fst::SymbolTable;
using fst::Fst;
using fst::StdArc;

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
    chunk_length_secs = 0.18;
    cache_length_secs = 30;
    do_endpointing = false;
    online = true;
    global_cmvn_file = "global_cmvn";

    po = new ParseOptions("libasr");

    po->Register("use-gpu", &use_gpu,
            "yes|no|optional|wait, only has effect if compiled with CUDA");
    po->Register("chunk-length", &chunk_length_secs,
                "Length of chunk size in seconds, that we process.  Set to <= 0 "
                "to use all input in one chunk.");
    po->Register("cache-length", &cache_length_secs,
                "Length of wave data cache buffer size in seconds, default 30s.");
    po->Register("global-cmvn", &global_cmvn_file, 
                 "global cmvn file, default file name: global_cmvn");
    po->Register("do-endpointing", &do_endpointing,
                "If true, apply endpoint detection");
    po->Register("online", &online,
                "You can set this to false to disable online iVector estimation "
                "and have all the data for each utterance used, even at "
                "utterance start.  This is useful where you just want the best "
                "results and don't care about online operation.  Setting this to "
                "false has the same effect as setting "
                "--use-most-recent-ivector=true and --greedy-ivector-extractor=true "
                "in the file given to --ivector-extraction-config, and "
                "--chunk-length=-1.");
    // po->Register("num-threads-startup", &g_num_threads,
    //             "Number of threads used when initializing iVector extractor.");

    feature_opts.Register(po);
    decodable_opts.Register(po);
    decoder_opts.Register(po);
    endpoint_opts.Register(po);

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
  OnlineNnet2FeaturePipelineConfig feature_opts;
  nnet3::NnetSimpleLoopedComputationOptions decodable_opts;
  LatticeFasterDecoderConfig decoder_opts;
  OnlineEndpointConfig endpoint_opts;
  BaseFloat chunk_length_secs; // = 0.18;
  bool do_endpointing; // = false;
  bool online; // = true;
  int32 cache_length_secs;

////////////////////////////////////////////////

  std::string use_gpu;
  std::string am_module_file;
  std::string fst_file;
  std::string word_syms_filename;
  std::string global_cmvn_file;

  TransitionModel trans_model;
  AmNnetSimple am_nnet;
  Fst<StdArc> *decode_fst;
  fst::SymbolTable *word_syms;

  ParseOptions* po;
};

CAsrModuleManager *g_module_mng = nullptr;

class CAsrHandler
{
public:
  CAsrHandler() 
  {
    Init();
  }
  virtual ~CAsrHandler() 
  {
    Uninit();
  }

  void Reset()
  {
    Uninit();
    Init();
  }


//private:
  void Init();
  void Uninit();

  bool allow_partial;
  bool bstarted; // = false;
  bool bstop;    // = false;

  std::string ivector_rspecifier;
  std::string online_ivector_rspecifier;
  std::string utt2spk_rspecifier;

  CompactLatticeWriter compact_lattice_writer;
  LatticeWriter lattice_writer;
  std::string lattice_wspecifier;
  std::string words_wspecifier;
  std::string alignment_wspecifier; 

  owl::XDecodableNnetSimpleLoopedInfo* decodable_info;
  owl::XSingleUtteranceNnet3Decoder* decoder;
  
  FbankOptions fbank_op;
  OnlineCmvnOptions cmvn_opts;
  OnlineCmvnState cmvn_state;
  OnlineCmvn* online_cmvn;
  owl::XOnlineMatrixFeature* feat_online;

  int32 speech_status;
  int32 result_status;
  std::vector<SD_RESULT> result_sds;
  std::vector<CYVOICE_RESULT> result_txts;
};

void CAsrHandler::Init()
{
  allow_partial = true;
  bstarted = bstop = false;
  speech_status = CYVOICE_SD_NO_SPEECH;
  result_status = CYVOICE_RESULT_NOT_READY_YET;
  result_sds.clear();
  result_txts.clear();
  lattice_wspecifier = "ark:|lattice-scale --acoustic-scale=10.0 ark:- ark,t:lat1-scale";
  words_wspecifier = "";
  alignment_wspecifier = ""; 

  //fbank
  fbank_op.use_energy = false;
  fbank_op.mel_opts.num_bins = 40;
  std::fstream in(g_module_mng->global_cmvn_file);  //, std::ios_base::binary);
  cmvn_state.global_cmvn_stats.Read(in, false);

  decodable_info = new owl::XDecodableNnetSimpleLoopedInfo(g_module_mng->decodable_opts,
                                                        &g_module_mng->am_nnet);

  feat_online = new owl::XOnlineMatrixFeature(g_module_mng->cache_length_secs, fbank_op.frame_opts.frame_shift_ms, 
                                            fbank_op.mel_opts.num_bins, cmvn_opts.cmn_window);
  online_cmvn = new OnlineCmvn(cmvn_opts, cmvn_state, feat_online);

  decoder = new owl::XSingleUtteranceNnet3Decoder(g_module_mng->decoder_opts, g_module_mng->trans_model,
                                            decodable_info->GetDecodableInfo(),
                                            *g_module_mng->decode_fst, online_cmvn);

  
}

void CAsrHandler::Uninit()
{
  if(online_cmvn)
  {
    delete online_cmvn;
    online_cmvn = nullptr;
  }
  if(feat_online)
  {
    delete feat_online;
    feat_online = nullptr;
  }
  if(decodable_info)
  {
    delete decodable_info;
    decodable_info = nullptr;
  }
  if(decoder)
  {
    delete decoder;
    decoder = nullptr;
  }
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
    hd->Uninit();
    hd->Init();
    if(!hd->bstarted)
    {
      hd->decoder->InitDecoding();
      hd->bstarted = true;
    }

    // Int32VectorWriter words_writer(hd->words_wspecifier);
    // Int32VectorWriter alignment_writer(hd->alignment_wspecifier);

    // bool determinize = g_module_mng->lat_decoder_config.determinize_lattice;

    // if (! (determinize ? hd->compact_lattice_writer.Open(hd->lattice_wspecifier)
    //      : hd->lattice_writer.Open(hd->lattice_wspecifier)))
    // KALDI_ERR << "Could not open table for writing lattices: "
    //            << hd->lattice_wspecifier;
    
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
    hd->feat_online->InputFinished();
    hd->bstop = true;
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
int16_t cyVoiceProcessData1(CYVOICE_HANDLE handle, void *speechBuffer, uint32_t *bufferSize, uint16_t *statusCode, 
                            int *num_sd_result, SD_RESULT **sd_result)
{
  //TODO
  if(handle == nullptr || *bufferSize <= 0)
  {
    *bufferSize = 0;
    return CYVOICE_EXIT_INVALID_PARM;
  }

  CAsrHandler* hd = (CAsrHandler*)handle;

  if(hd->feat_online->NumFramesReady() == 0)
  {
    SD_RESULT sd_rst;
    memset(&sd_rst, 0, sizeof(SD_RESULT));
    hd->result_sds.push_back(sd_rst);
  }

  if((*bufferSize) % 2 == 1)
    (*bufferSize)--;

  int32 samp_freq_ = 16000;
  BaseFloat samp_freq = static_cast<BaseFloat>(samp_freq_);  //!!! only support 16k 16bit

  int16 *data_ptr = reinterpret_cast<int16*>(speechBuffer);

  Vector<BaseFloat> data;
  data.Resize(*bufferSize/2);
  for (uint32 i = 0; i < data.Dim(); ++i) 
  {
    int16 k = *data_ptr++;
    data(i) =  k;
  }

  int32 chunk_length;
  int32 num_samp = data.Dim();
  if (g_module_mng->chunk_length_secs > 0) 
  {
    chunk_length = int32(samp_freq * g_module_mng->chunk_length_secs);
    if (chunk_length == 0) chunk_length = 1;
    num_samp = chunk_length < num_samp ? chunk_length : num_samp;
  } 
  else 
  {
    chunk_length = std::numeric_limits<int32>::max();
  }

  SubVector<BaseFloat> wave_part(data, 0, num_samp);
  Fbank fbank(hd->fbank_op);
  Matrix<BaseFloat> feats;
  fbank.Compute(wave_part, 1.0, &feats);

  int32 frame_decoded = hd->decoder->NumFramesDecoded();
  hd->feat_online->SetNumFramesDecoded(frame_decoded);

  if((feats.NumRows() == 0)
     || (num_samp < (hd->fbank_op.frame_opts.frame_shift_ms * samp_freq_ / 1000)))
  {
    return CYVOICE_EXIT_WAVE_PIECE_TOO_SHORT;
  }

  int32 rows = hd->feat_online->AppendData(feats);
  
  *bufferSize = samp_freq_ * 2 * rows * hd->fbank_op.frame_opts.frame_shift_ms / 1000;

  auto sd_rst_ = hd->result_sds.begin();
  sd_rst_->speech_frame += feats.NumRows();
  sd_rst_->speech_end_frame = sd_rst_->speech_frame - 1;
  sd_rst_->speech_sample += *bufferSize / 2;
  sd_rst_->speech_end_sample = sd_rst_->speech_sample - 1;
  sd_rst_->f_speech += sd_rst_->speech_sample / samp_freq;
  sd_rst_->f_speech_end = sd_rst_->f_speech;
  sd_rst_->speech_last_frame = sd_rst_->speech_end_frame;

  *num_sd_result = hd->result_sds.size();
  *sd_result = hd->result_sds.data();

  // if (do_endpointing && decoder.EndpointDetected(endpoint_opts)) 
  // {
  //   break;
  // }

  if(hd->decoder->NumFramesDecoded() > 0)
  {
    hd->result_status = CYVOICE_RESULT_READY;
  }

  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceProcessData2(CYVOICE_HANDLE handle)
{
  if(handle == nullptr)
  {
    return CYVOICE_EXIT_INVALID_PARM;
  }

  CAsrHandler* hd = (CAsrHandler*)handle;
  if(hd->bstarted)
    hd->decoder->AdvanceDecoding();
  if(hd->bstop)
  {
    hd->decoder->FinalizeDecoding();
    hd->bstop = false;
    hd->bstarted = false;
  }

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
  if(handle == nullptr)
  {
    return CYVOICE_EXIT_INVALID_PARM;
  }

  CAsrHandler* hd = (CAsrHandler*)handle;
  *statusCode = std::max(hd->speech_status, hd->result_status);  
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
int16_t cyVoiceQueryStatusEx(CYVOICE_HANDLE handle, uint16_t *statusCode, 
  int *num_sd_result, SD_RESULT **sd_result, int *num_sr_result, CYVOICE_RESULT **sr_result)
{
  if(handle == nullptr)
  {
    return CYVOICE_EXIT_INVALID_PARM;
  }

  CAsrHandler* hd = (CAsrHandler*)handle;
  *statusCode = std::max(hd->speech_status, hd->result_status); 
  *num_sd_result = hd->result_sds.size();
  *sd_result = hd->result_sds.data();
  *num_sr_result = hd->result_txts.size();
  *sr_result = hd->result_txts.data();
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
  if(handle == nullptr || statusCode == nullptr)
    return CYVOICE_EXIT_INVALID_PARM;
  CAsrHandler* hd = (CAsrHandler*)handle;

  if(hd->decoder->NumFramesDecoded() <= 0)
  {
    hd->result_status = CYVOICE_RESULT_NOT_READY_YET;
    return CYVOICE_EXIT_SUCCESS;
  }

  if (!hd->decoder->Decoder().ReachedFinal()) 
  {
    hd->result_status = CYVOICE_RESULT_PARTIAL;
  }
  else
  {
    hd->result_status = CYVOICE_RESULT_WHOLE;
  }
  *statusCode = hd->result_status;

  using fst::VectorFst;
  std::string utt = "utt";

  double likelihood;
  LatticeWeight weight;
  int32 num_frames;
  { // First do some stuff with word-level traceback...
    VectorFst<LatticeArc> decoded;
    if (!hd->decoder->Decoder().GetBestPath(&decoded))
      // Shouldn't really reach this point as already checked success.
      KALDI_ERR << "Failed to get traceback for utterance " << utt;

    std::vector<int32> alignment;
    std::vector<int32> words;
    GetLinearSymbolSequence(decoded, &alignment, &words, &weight);
    num_frames = alignment.size();
    if (g_module_mng->word_syms != NULL) 
    {
      CYVOICE_RESULT result_txt;
      memset(&result_txt, 0, sizeof(result_txt));
      strcpy(result_txt.szGrammarName, "generic_ex");
      std::string txt, txt_raw;
      for (size_t i = 0; i < words.size(); i++) 
      {
        std::string s = g_module_mng->word_syms->Find(words[i]);
        if (s == "")
          KALDI_ERR << "Word-id " << words[i] << " not in symbol table.";
        txt_raw += s;
        txt += s;
        txt += " ";
      }

      int32 len = std::min(sizeof(result_txt.result_raw), txt_raw.length());
      memcpy(result_txt.result_raw, txt_raw.c_str(), len);
      len = std::min(sizeof(result_txt.result), txt.length());
      memcpy(result_txt.result, txt.c_str(), len);

      hd->result_txts.clear();
      hd->result_txts.push_back(result_txt);
    }
    likelihood = -(weight.Value1() + weight.Value2());
  }

  // Get lattice, and do determinization if requested.
  // Lattice lat;
  // bool determinize = false;
  // hd->decoder->decoder_.GetRawLattice(&lat);
  // if (lat.NumStates() == 0)
  //   KALDI_ERR << "Unexpected problem getting lattice for utterance " << utt;
  // fst::Connect(&lat);
  // if (determinize) {
  //   CompactLattice clat;
  //   if (!DeterminizeLatticePhonePrunedWrapper(
  //           trans_model,
  //           &lat,
  //           decoder.GetOptions().lattice_beam,
  //           &clat,
  //           decoder.GetOptions().det_opts))
  //     KALDI_WARN << "Determinization finished earlier than the beam for "
  //                << "utterance " << utt;
  //   // We'll write the lattice without acoustic scaling.
  //   if (acoustic_scale != 0.0)
  //     fst::ScaleLattice(fst::AcousticLatticeScale(1.0 / acoustic_scale), &clat);
  //   compact_lattice_writer->Write(utt, clat);
  // } else {
  //   // We'll write the lattice without acoustic scaling.
  //   if (acoustic_scale != 0.0)
  //     fst::ScaleLattice(fst::AcousticLatticeScale(1.0 / acoustic_scale), &lat);
  //   lattice_writer->Write(utt, lat);
  // }
  // KALDI_LOG << "Log-like per frame for utterance " << utt << " is "
  //           << (likelihood / num_frames) << " over "
  //           << num_frames << " frames.";
  // KALDI_VLOG(2) << "Cost for utterance " << utt << " is "
  //               << weight.Value1() << " + " << weight.Value2();
  // *like_ptr = likelihood;

  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceSearchForwardPartial(CYVOICE_HANDLE handle, uint16_t *statusCode)
{
  cyVoiceSearchForward(handle, statusCode);
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
  if(handle == nullptr)
    return CYVOICE_EXIT_INVALID_PARM;
  CAsrHandler* hd = (CAsrHandler*)handle;

  *statusCode = std::max(hd->speech_status, hd->result_status); 
  auto it = hd->result_txts.begin();
  if(it != hd->result_txts.end())
  {
    if(result)
      strcpy(result, it->result);
    if(score)
      *score = (int32)it->score;
  }

  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceQueryResultPartial(CYVOICE_HANDLE handle, uint16_t *statusCode, char *result, int32_t *score)
{
  cyVoiceQueryResult(handle, statusCode, result, score);
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
  if(handle == nullptr)
    return CYVOICE_EXIT_INVALID_PARM;
  CAsrHandler* hd = (CAsrHandler*)handle;

  *statusCode = std::max(hd->speech_status, hd->result_status); 
  *num_result = hd->result_txts.size();
  st_result = hd->result_txts.data();
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
int16_t cyVoiceQueryActiveResults(CYVOICE_HANDLE handle, int active_index, uint16_t *statusCode, 
                                  int *num_active_grammar, int *num_result, CYVOICE_RESULT *st_result)
{
  if(handle == nullptr)
    return CYVOICE_EXIT_INVALID_PARM;
  CAsrHandler* hd = (CAsrHandler*)handle;

  *statusCode = std::max(hd->speech_status, hd->result_status);   
  *num_active_grammar = 1;

  int32 num_rst = 0;
  for(int i=0; i<hd->result_txts.size();i++)
  {
    if(st_result)
    {
      memcpy(st_result, &hd->result_txts[i], sizeof(CYVOICE_RESULT));
      num_rst++;
      st_result++;
    }
    else
      break;
  }
  *num_result = num_rst;
  
  return CYVOICE_EXIT_SUCCESS;
}

int16_t cyVoiceQueryAllActiveResults(CYVOICE_HANDLE handle, uint16_t *statusCode, int *num_result, CYVOICE_RESULT *st_result, int max_num_result)
{
  if(handle == nullptr)
    return CYVOICE_EXIT_INVALID_PARM;
  CAsrHandler* hd = (CAsrHandler*)handle;

  *statusCode = std::max(hd->speech_status, hd->result_status); 
  *num_result = hd->result_txts.size();
  st_result = hd->result_txts.data();

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
