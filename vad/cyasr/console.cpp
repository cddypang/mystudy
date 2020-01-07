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
#include "nnet3/decodable-online-looped.h"

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
    am_nnet_file = "cymodel/cyVoiceAm_PutEng_v7.0.dat";
    nnet_file = "cymodel/cyVoiceNnet_PutEng_v7.1.dat";
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
    Input isnnet(nnet_file, &binary);
    nnet.Read(isnnet.Stream(), binary);

    Input ki(am_nnet_file, &binary);
    trans_model.Read(ki.Stream(), binary);
    am_nnet.Read(ki.Stream(), binary);

    am_nnet.SetNnet(nnet);

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
  std::string am_nnet_file;
  std::string nnet_file;
  std::string fst_file;
  std::string word_syms_filename;
  std::string global_cmvn_file;

  TransitionModel trans_model;
  AmNnetSimple am_nnet;
  Nnet nnet;
  Fst<StdArc> *decode_fst;
  fst::SymbolTable *word_syms;

  ParseOptions* po;
};

CAsrModuleManager *g_module_mng = nullptr;

int main(int argc, char* argv[])
{
	if(argc != 4)
	{
		printf("./a.out config.cfg wavfile engine-cnt\n");
		return 0;
	}

  g_module_mng = new CAsrModuleManager();
  std::string cfgfile = argv[1];
  g_module_mng->Init(cfgfile);
  bool succ = g_module_mng->LoadStaticModules();

  if(g_module_mng)
  {
    delete g_module_mng;
    g_module_mng = nullptr;
  }
  return 0;
}
