
#ifndef OWL_NNET3_DECODING_H_
#define OWL_NNET3_DECODING_H_

#include <string>
#include <vector>
#include <deque>

#include "nnet3/decodable-online-looped.h"
#include "matrix/matrix-lib.h"
#include "util/common-utils.h"
#include "base/kaldi-error.h"
#include "itf/online-feature-itf.h"
#include "online2/online-endpoint.h"
#include "online2/online-nnet2-feature-pipeline.h"
#include "decoder/lattice-faster-online-decoder.h"
#include "hmm/transition-model.h"
#include "hmm/posterior.h"

namespace owl
{
using namespace kaldi;

class XOnlineMatrixFeature: public OnlineFeatureInterface 
{
public:
  explicit XOnlineMatrixFeature(const int32 cache_seconds, const int32 frame_shift_ms, 
    const int32 feat_num_bins, const int32 cmn_window) 
   : cache_sencods_(cache_seconds) ,
     frame_shift_ms_(frame_shift_ms),
     feat_num_bins_(feat_num_bins), cmn_window_(cmn_window), mat(nullptr)
  {
    Init();
  }

  virtual ~XOnlineMatrixFeature()
  {
    Uninit();
  }

  virtual int32 Dim() const 
  {
    return feat_num_bins_; 
  }

  virtual BaseFloat FrameShiftInSeconds() const 
  {
    return 0.01f;
  }

  virtual int32 NumFramesReady() const 
  {
    return const_cast<XOnlineMatrixFeature*>(this)->GetNumFramesReady();
  }

  int32 GetNumFramesReady()
  {
    std::unique_lock<std::mutex> lock(mtx_);
    return idx_end == 0 ? 0 : idx_end + 1;
  }

  virtual void GetFrame(int32 frame, VectorBase<BaseFloat> *feat)
  {    
    std::unique_lock<std::mutex> lock(mtx_);
    KALDI_ASSERT(frame <= idx_end);
    if(pos_offset == 0)
      return;
    int32 idx = pos_offset - (idx_end - idx_begin) - 1;
    idx += (frame-idx_begin);
    feat->CopyFromVec(mat->Row(idx));
    return;
  }

  virtual bool IsLastFrame(int32 frame) const
  {
    return const_cast<XOnlineMatrixFeature*>(this)->GetIsLastFrame(frame);
  }

  bool GetIsLastFrame(int32 frame)
  {
    std::unique_lock<std::mutex> lock(mtx_);
    return input_finished_ && frame == idx_end;
  }

  virtual void InputFinished()
  {
    input_finished_ = true;
  }

  int32 AppendData(const MatrixBase<BaseFloat>& data)
  {
    std::unique_lock<std::mutex> lock(mtx_);
    int32 reverse = total_rows - pos_offset;
    int32 row_in = reverse > data.NumRows() ? data.NumRows() : reverse;
    for (uint32 i = 0; i < row_in; ++i)
    {
      mat->CopyRowFromVec(data.Row(i), pos_offset);
      pos_offset++;
    }
    if(row_in > 0)
    {
      idx_end += row_in;
      idx_end--;
    }

    return row_in;
  }

  void SetNumFramesDecoded(int32 frame_cnt)
  {
    std::unique_lock<std::mutex> lock(mtx_);
    KALDI_ASSERT(frame_cnt <= idx_end+1);
    frame_cnt > idx_end ? idx_begin = idx_end : idx_begin = frame_cnt;
    int32 reverse = idx_end - idx_begin + 1 + cmn_window_;
    if(pos_offset > reverse && idx_begin > cmn_window_)
    {
      int32 row_begin = pos_offset - reverse;
      if(row_begin > 0)
      {
        pos_offset = 0;
        for(int i = 0; i<reverse; ++i)
        {
          mat->CopyRowFromVec(mat->Row(row_begin), pos_offset);
          pos_offset++;
        }
      }
    }
  }

  void Reset()
  {
    Uninit();
    Init();
  }

private:
  void Init()
  {
    KALDI_ASSERT(mat == nullptr);
    KALDI_ASSERT(frame_shift_ms_ < cache_sencods_ * 1000);
    KALDI_ASSERT(feat_num_bins_ > 0);
    KALDI_ASSERT(cmn_window_ > 0);

    idx_begin = idx_end = pos_offset = total_rows = 0;    
    int64 ms_ = cache_sencods_ * 1000;
    total_rows = ms_ / frame_shift_ms_;
    if(ms_ % frame_shift_ms_ > 0)
      total_rows++;
    total_rows += cmn_window_;
    mat = new Matrix<BaseFloat>(total_rows, feat_num_bins_);
    input_finished_ = false;
  }

  void Uninit()
  {
    if(mat)
    {
       delete mat;
       mat = nullptr;
    } 
  }

  bool input_finished_;
  std::mutex mtx_;
  int32 idx_begin;
  int32 idx_end;  

  int32 cache_sencods_;
  int32 frame_shift_ms_;
  int32 feat_num_bins_;
  int32 cmn_window_;
  int32 pos_offset;
  int32 total_rows;
  Matrix<BaseFloat>* mat;
};


/**
   You will instantiate this class when you want to decode a single utterance
   using the online-decoding setup for neural nets.  The template will be
   instantiated only for FST = fst::Fst<fst::StdArc> and FST = fst::GrammarFst.
*/

template <typename FST>
class XSingleUtteranceNnet3DecoderTpl {
 public:

  // Constructor. The pointer 'features' is not being given to this class to own
  // and deallocate, it is owned externally.
  XSingleUtteranceNnet3DecoderTpl(const LatticeFasterDecoderConfig &decoder_opts,
                                 const TransitionModel &trans_model,
                                 const nnet3::DecodableNnetSimpleLoopedInfo &info,
                                 const FST &fst,
                                 OnlineFeatureInterface *features);

  /// Initializes the decoding and sets the frame offset of the underlying
  /// decodable object. This method is called by the constructor. You can also
  /// call this method when you want to reset the decoder state, but want to
  /// keep using the same decodable object, e.g. in case of an endpoint.
  void InitDecoding(int32 frame_offset = 0);

  /// Advances the decoding as far as we can.
  void AdvanceDecoding();

  /// Finalizes the decoding. Cleans up and prunes remaining tokens, so the
  /// GetLattice() call will return faster.  You must not call this before
  /// calling (TerminateDecoding() or InputIsFinished()) and then Wait().
  void FinalizeDecoding();

  int32 NumFramesDecoded() const;

  /// Gets the lattice.  The output lattice has any acoustic scaling in it
  /// (which will typically be desirable in an online-decoding context); if you
  /// want an un-scaled lattice, scale it using ScaleLattice() with the inverse
  /// of the acoustic weight.  "end_of_utterance" will be true if you want the
  /// final-probs to be included.
  void GetLattice(bool end_of_utterance,
                  CompactLattice *clat) const;

  /// Outputs an FST corresponding to the single best path through the current
  /// lattice. If "use_final_probs" is true AND we reached the final-state of
  /// the graph then it will include those as final-probs, else it will treat
  /// all final-probs as one.
  void GetBestPath(bool end_of_utterance,
                   Lattice *best_path) const;


  /// This function calls EndpointDetected from online-endpoint.h,
  /// with the required arguments.
  bool EndpointDetected(const OnlineEndpointConfig &config);

  const LatticeFasterOnlineDecoderTpl<FST> &Decoder() const { return decoder_; }

  ~XSingleUtteranceNnet3DecoderTpl() { }

 private:

  const LatticeFasterDecoderConfig &decoder_opts_;

  // this is remembered from the constructor; it's ultimately
  // derived from calling FrameShiftInSeconds() on the feature pipeline.
  BaseFloat input_feature_frame_shift_in_seconds_;

  // we need to keep a reference to the transition model around only because
  // it's needed by the endpointing code.
  const TransitionModel &trans_model_;

  nnet3::DecodableAmNnetLoopedOnline decodable_;

  LatticeFasterOnlineDecoderTpl<FST> decoder_;

};


typedef XSingleUtteranceNnet3DecoderTpl<fst::Fst<fst::StdArc> > XSingleUtteranceNnet3Decoder;


}  // namespace owl



#endif  // OWL_NNET3_DECODING_H_
