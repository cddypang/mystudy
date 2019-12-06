#include "feat/feature-mfcc.h"
#include "feat/feature-fbank.h"
#include "feat/online-feature.h"
#include "feat/wave-reader.h"
#include "matrix/kaldi-matrix.h"
#include "transform/transform-common.h"
#include "ivector/voice-activity-detection.h"

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

// Only generate random length for each piece
bool RandomSplit(int32 wav_dim,
                 std::vector<int32> *piece_dim,
                 int32 num_pieces,
                 int32 trials = 5) {
  piece_dim->clear();
  piece_dim->resize(num_pieces, 0);

  int32 dim_mean = wav_dim / (num_pieces * 2);
  int32 cnt = 0;
  while (true) {
    int32 dim_total = 0;
    for (int32 i = 0; i < num_pieces - 1; i++) {
      (*piece_dim)[i] = dim_mean + rand() % dim_mean;
      dim_total += (*piece_dim)[i];
    }
    (*piece_dim)[num_pieces - 1] = wav_dim - dim_total;

    if (dim_total > 0 && dim_total < wav_dim)
      break;
    if (++cnt > trials)
      return false;
  }
  return true;
}

void GetOutput(OnlineFeatureInterface *a,
               Matrix<BaseFloat> *output) {
  int32 dim = a->Dim();
  static int32 frame_idx = 0;
  OnlineCacheFeature cache(a);
  
  std::vector<Vector<BaseFloat>* > cached_frames;
  int32 cache_cnt = cache.NumFramesReady();
  int32 last_idx = frame_idx;

  while (frame_idx < cache_cnt) {
    Vector<BaseFloat> garbage(dim);
    cache.GetFrame(frame_idx, &garbage);
    cached_frames.push_back(new Vector<BaseFloat>(garbage));
    if (cache.IsLastFrame(frame_idx))
      break;
    frame_idx++;
  }
  //KALDI_ASSERT(cached_frames.size() == a->NumFramesReady());
  KALDI_ASSERT(cached_frames.size() == cache_cnt - last_idx);

  if(!cached_frames.empty())
  {
    output->Resize(cached_frames.size(), dim);
    for (int32 i = 0; i < cached_frames.size(); i++) {
      output->CopyRowFromVec(*(cached_frames[i]), i);
      delete cached_frames[i];
    }
    cached_frames.clear();
  }
  cache.ClearCache();
}

void TestOnlineMfcc() {
  std::ifstream is("./121.wav", std::ios_base::binary);
  WaveData wave;
  wave.Read(is);
  KALDI_ASSERT(wave.Data().NumRows() == 1);
  SubVector<BaseFloat> waveform(wave.Data(), 0);

  // the parametrization object
  MfccOptions op;
  // op.frame_opts.dither = 0.0;
  // op.frame_opts.preemph_coeff = 0.0;
  // op.frame_opts.window_type = "hamming";
  // op.frame_opts.remove_dc_offset = false;
  // op.frame_opts.round_to_power_of_two = true;
  // op.frame_opts.samp_freq = wave.SampFreq();
  // op.mel_opts.low_freq = 0.0;
  // op.htk_compat = false;
  // op.use_energy = false;  // C0 not energy.
  // if (RandInt(0, 1) == 0)
  //   op.frame_opts.snip_edges = false;
  op.mel_opts.num_bins = 40;
  op.num_ceps = 20;
  Mfcc mfcc(op);

  // compute mfcc offline
  Matrix<BaseFloat> mfcc_feats;
  mfcc.Compute(waveform, 1.0, &mfcc_feats);  // vtln not supported

      VadEnergyOptions opts;
    opts.vad_energy_threshold = 5.5;
    opts.vad_energy_mean_scale = 0.5;


#if 1  
  // compare
  // The test waveform is about 1.44s long, so
  // we try to break it into from 5 pieces to 9(not essential to do so)
  for (int32 num_piece = 5; num_piece < 6; num_piece++) {
    OnlineMfcc online_mfcc(op);
    std::vector<int32> piece_length(num_piece, 0);

    bool ret = RandomSplit(waveform.Dim(), &piece_length, num_piece);
    KALDI_ASSERT(ret);


    std::string utt; // = "uid" + std::to_string(5);
    std::string ws; // = "t,ark:vad-" + utt + ".ark";

    int32 offset_start = 0;
    for (int32 i = 0; i < num_piece; i++) {
      Vector<BaseFloat> wave_piece(
        waveform.Range(offset_start, piece_length[i]));
      online_mfcc.AcceptWaveform(wave.SampFreq(), wave_piece);
      offset_start += piece_length[i];
      
      Matrix<BaseFloat> online_mfcc_feats;
      GetOutput(&online_mfcc, &online_mfcc_feats);
      Vector<BaseFloat> vad_result(online_mfcc_feats.NumRows());
      ComputeVadEnergy(opts, online_mfcc_feats, &vad_result);

      utt = "uid" + std::to_string(i);
      ws = "t,ark:vad-" + utt + ".ark";
      BaseFloatVectorWriter vad_writer(ws);
      vad_writer.Write(utt, vad_result);
    }
    online_mfcc.InputFinished();

      Matrix<BaseFloat> online_mfcc_feats;
      GetOutput(&online_mfcc, &online_mfcc_feats);
      if(online_mfcc_feats.NumRows() > 0)
      {
        Vector<BaseFloat> vad_result(online_mfcc_feats.NumRows());
        ComputeVadEnergy(opts, online_mfcc_feats, &vad_result);

        utt = "uid" + std::to_string(num_piece);
        ws = "t,ark:vad-" + utt + ".ark";
        BaseFloatVectorWriter vad_writer(ws);
        vad_writer.Write(utt, vad_result);
      }
    //AssertEqual(mfcc_feats, online_mfcc_feats);
  }
#endif
}

void TestOnlineFbank() {
  std::ifstream is("./121.wav", std::ios_base::binary);
  WaveData wave;
  wave.Read(is);
  KALDI_ASSERT(wave.Data().NumRows() == 1);
  SubVector<BaseFloat> waveform(wave.Data(), 0);

  // the parametrization object
  FbankOptions op;
  op.use_energy = true;
  op.mel_opts.num_bins = 40;

  Fbank mfcc(op);

  // compute mfcc offline
  Matrix<BaseFloat> mfcc_feats;
  mfcc.Compute(waveform, 1.0, &mfcc_feats);  // vtln not supported

      VadEnergyOptions opts;
    opts.vad_energy_threshold = 5.5;
    opts.vad_energy_mean_scale = 0.5;


#if 1  
  // compare
  // The test waveform is about 1.44s long, so
  // we try to break it into from 5 pieces to 9(not essential to do so)
  for (int32 num_piece = 5; num_piece < 6; num_piece++) {
    OnlineFbank online_mfcc(op);
    std::vector<int32> piece_length(num_piece, 0);

    bool ret = RandomSplit(waveform.Dim(), &piece_length, num_piece);
    KALDI_ASSERT(ret);


    std::string utt; // = "uid" + std::to_string(5);
    std::string ws; // = "t,ark:vad-" + utt + ".ark";

    int32 offset_start = 0;
    for (int32 i = 0; i < num_piece; i++) {
      Vector<BaseFloat> wave_piece(
        waveform.Range(offset_start, piece_length[i]));
      online_mfcc.AcceptWaveform(wave.SampFreq(), wave_piece);
      offset_start += piece_length[i];
      
      Matrix<BaseFloat> online_mfcc_feats;
      GetOutput(&online_mfcc, &online_mfcc_feats);
      Vector<BaseFloat> vad_result(online_mfcc_feats.NumRows());
      ComputeVadEnergy(opts, online_mfcc_feats, &vad_result);

      utt = "uid" + std::to_string(i);
      ws = "t,ark:vad-" + utt + ".ark";
      BaseFloatVectorWriter vad_writer(ws);
      vad_writer.Write(utt, vad_result);
    }
    online_mfcc.InputFinished();

      Matrix<BaseFloat> online_mfcc_feats;
      GetOutput(&online_mfcc, &online_mfcc_feats);
      if(online_mfcc_feats.NumRows() > 0)
      {
        Vector<BaseFloat> vad_result(online_mfcc_feats.NumRows());
        ComputeVadEnergy(opts, online_mfcc_feats, &vad_result);

        utt = "uid" + std::to_string(num_piece);
        ws = "t,ark:vad-" + utt + ".ark";
        BaseFloatVectorWriter vad_writer(ws);
        vad_writer.Write(utt, vad_result);
      }
    //AssertEqual(mfcc_feats, online_mfcc_feats);
  }
#endif
}


int main(int argc, char* argv[])
{
  //TestOnlineFbank();
  //TestOnlineMfcc();

  const char *usage =
        "Generate lattices using nnet3 neural net model.\n"
        "Usage: nnet3-latgen-faster [options] <nnet-in> <fst-in|fsts-rspecifier> <features-rspecifier>"
        " <lattice-wspecifier> [ <words-wspecifier> [<alignments-wspecifier>] ]\n"
        "See also: nnet3-latgen-faster-parallel, nnet3-latgen-faster-batch\n";
  ParseOptions po(usage);
  Timer timer;
  bool allow_partial = false;
  LatticeFasterDecoderConfig config;
  NnetSimpleComputationOptions decodable_opts;
  std::string word_syms_filename;
  std::string ivector_rspecifier,
      online_ivector_rspecifier,
      utt2spk_rspecifier;
  int32 online_ivector_period = 0;
  config.Register(&po);
  decodable_opts.Register(&po);
  po.Register("word-symbol-table", &word_syms_filename,
              "Symbol table for words [for debug output]");
  po.Register("allow-partial", &allow_partial,
              "If true, produce output even if end state was not reached.");
  po.Register("ivectors", &ivector_rspecifier, "Rspecifier for "
              "iVectors as vectors (i.e. not estimated online); per utterance "
              "by default, or per speaker if you provide the --utt2spk option.");
  po.Register("utt2spk", &utt2spk_rspecifier, "Rspecifier for "
              "utt2spk option used to get ivectors per speaker");
  po.Register("online-ivectors", &online_ivector_rspecifier, "Rspecifier for "
              "iVectors estimated online, as matrices.  If you supply this,"
              " you must set the --online-ivector-period option.");
  po.Register("online-ivector-period", &online_ivector_period, "Number of frames "
              "between iVectors in matrices supplied to the --online-ivectors "
              "option");
  po.Read(argc, argv);

  if (po.NumArgs() < 3 || po.NumArgs() > 5) {
    po.PrintUsage();
    exit(1);
  }
  std::string model_in_filename = po.GetArg(1),
      fst_in_str = po.GetArg(2),
      lattice_wspecifier = po.GetArg(3),
      words_wspecifier = po.GetOptArg(4),
      alignment_wspecifier = po.GetOptArg(5);

  TransitionModel trans_model;
  AmNnetSimple am_nnet;
  {
    bool binary;
    Input ki(model_in_filename, &binary);
    trans_model.Read(ki.Stream(), binary);
    am_nnet.Read(ki.Stream(), binary);
    SetBatchnormTestMode(true, &(am_nnet.GetNnet()));
    SetDropoutTestMode(true, &(am_nnet.GetNnet()));
    CollapseModel(CollapseModelConfig(), &(am_nnet.GetNnet()));
  }
  bool determinize = config.determinize_lattice;
  CompactLatticeWriter compact_lattice_writer;
  LatticeWriter lattice_writer;
  if (! (determinize ? compact_lattice_writer.Open(lattice_wspecifier)
         : lattice_writer.Open(lattice_wspecifier)))
    KALDI_ERR << "Could not open table for writing lattices: "
               << lattice_wspecifier;
  RandomAccessBaseFloatMatrixReader online_ivector_reader(
      online_ivector_rspecifier);
  RandomAccessBaseFloatVectorReaderMapped ivector_reader(
      ivector_rspecifier, utt2spk_rspecifier);
  Int32VectorWriter words_writer(words_wspecifier);
  Int32VectorWriter alignment_writer(alignment_wspecifier);
  fst::SymbolTable *word_syms = NULL;
  if (word_syms_filename != "")
    if (!(word_syms = fst::SymbolTable::ReadText(word_syms_filename)))
      KALDI_ERR << "Could not read symbol table from file "
                 << word_syms_filename;
  double tot_like = 0.0;
  kaldi::int64 frame_count = 0;

  // this compiler object allows caching of computations across
  // different utterances.
  CachingOptimizingCompiler compiler(am_nnet.GetNnet(),
                                    decodable_opts.optimize_config);

  Fst<StdArc> *decode_fst = fst::ReadFstKaldiGeneric(fst_in_str);
  LatticeFasterDecoder decoder(*decode_fst, config);

  std::cout << "load model finish!" << std::endl;

  std::ifstream is("./121.wav", std::ios_base::binary);
  WaveData wave;
  wave.Read(is);
  KALDI_ASSERT(wave.Data().NumRows() == 1);
  SubVector<BaseFloat> waveform(wave.Data(), 0);

  // the parametrization object
  FbankOptions op;
  op.use_energy = true;
  op.mel_opts.num_bins = 40;

  Fbank mfcc(op);
  // compute mfcc offline
  Matrix<BaseFloat> mfcc_feats;
  mfcc.Compute(waveform, 1.0, &mfcc_feats);  // vtln not supported

  VadEnergyOptions opts;
  opts.vad_energy_threshold = 5.5;
  opts.vad_energy_mean_scale = 0.5;

  // compare
  // The test waveform is about 1.44s long, so
  // we try to break it into from 5 pieces to 9(not essential to do so)
  for (int32 num_piece = 5; num_piece < 6; num_piece++) 
  {
    OnlineFbank online_mfcc(op);
    std::vector<int32> piece_length(num_piece, 0);

    bool ret = RandomSplit(waveform.Dim(), &piece_length, num_piece);
    KALDI_ASSERT(ret);


    std::string utt; // = "uid" + std::to_string(5);
    std::string ws; // = "t,ark:vad-" + utt + ".ark";

    int32 offset_start = 0;
    for (int32 i = 0; i < num_piece; i++) {
      Vector<BaseFloat> wave_piece(
        waveform.Range(offset_start, piece_length[i]));
      online_mfcc.AcceptWaveform(wave.SampFreq(), wave_piece);
      offset_start += piece_length[i];
      
      Matrix<BaseFloat> online_mfcc_feats;
      GetOutput(&online_mfcc, &online_mfcc_feats);
      Vector<BaseFloat> vad_result(online_mfcc_feats.NumRows());
      ComputeVadEnergy(opts, online_mfcc_feats, &vad_result);

      utt = "uid" + std::to_string(i);
      ws = "t,ark:vad-" + utt + ".ark";
      BaseFloatVectorWriter vad_writer(ws);
      vad_writer.Write(utt, vad_result);
    }
    online_mfcc.InputFinished();

      Matrix<BaseFloat> online_mfcc_feats;
      GetOutput(&online_mfcc, &online_mfcc_feats);
      if(online_mfcc_feats.NumRows() > 0)
      {
        Vector<BaseFloat> vad_result(online_mfcc_feats.NumRows());
        ComputeVadEnergy(opts, online_mfcc_feats, &vad_result);

        utt = "uid" + std::to_string(num_piece);
        ws = "t,ark:vad-" + utt + ".ark";
        BaseFloatVectorWriter vad_writer(ws);
        vad_writer.Write(utt, vad_result);
      }
    //AssertEqual(mfcc_feats, online_mfcc_feats);
  }

  if (mfcc_feats.NumRows() > 0) 
  {
    const Matrix<BaseFloat> *online_ivectors = NULL;
    const Vector<BaseFloat> *ivector = NULL;
    DecodableAmNnetSimple nnet_decodable(
      decodable_opts, trans_model, am_nnet,
      mfcc_feats.ColRange(1, op.mel_opts.num_bins), 
      ivector, online_ivectors,
      online_ivector_period, &compiler);
    double like;
    if (DecodeUtteranceLatticeFaster(
      decoder, nnet_decodable, trans_model, word_syms, "utt01",
      decodable_opts.acoustic_scale, determinize, allow_partial,
      &alignment_writer, &words_writer, &compact_lattice_writer,
      &lattice_writer,
      &like)) 
    {
        tot_like += like;
        frame_count += nnet_decodable.NumFramesReady();
    }
  }
  delete decode_fst;


  //if (ClassifyRspecifier(fst_in_str, NULL, NULL) == kNoRspecifier) 
  //{
  //  // Input FST is just one FST, not a table of FSTs.
  //  Fst<StdArc> *decode_fst = fst::ReadFstKaldiGeneric(fst_in_str);
  //  timer.Reset();
  //  {
  //    LatticeFasterDecoder decoder(*decode_fst, config);
  //    std::string utt = "uid0";
  //    
  //  }
  //  delete decode_fst; // delete this only after decoder goes out of scope.
  //}

  kaldi::int64 input_frame_count =
      frame_count * decodable_opts.frame_subsampling_factor;
  double elapsed = timer.Elapsed();
  KALDI_LOG << "Time taken "<< elapsed
            << "s: real-time factor assuming 100 frames/sec is "
            << (elapsed * 100.0 / input_frame_count);
  KALDI_LOG << "Overall log-likelihood per frame is "
            << (tot_like / frame_count) << " over "
            << frame_count << " frames.";
  delete word_syms;

  return 0;
}