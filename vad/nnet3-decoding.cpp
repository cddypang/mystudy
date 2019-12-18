
#include "nnet3-decoding.h"
#include "lat/lattice-functions.h"
#include "lat/determinize-lattice-pruned.h"
#include "decoder/grammar-fst.h"

namespace owl
{

template <typename FST>
XSingleUtteranceNnet3DecoderTpl<FST>::XSingleUtteranceNnet3DecoderTpl(
    const LatticeFasterDecoderConfig &decoder_opts,
    const TransitionModel &trans_model,
    const nnet3::DecodableNnetSimpleLoopedInfo &info,
    const FST &fst,
    OnlineFeatureInterface *features):
    decoder_opts_(decoder_opts),
    input_feature_frame_shift_in_seconds_(features->FrameShiftInSeconds()),
    trans_model_(trans_model),
    decodable_(trans_model_, info, features, nullptr),
    decoder_(fst, decoder_opts_) {
  decoder_.InitDecoding();
}

template <typename FST>
void XSingleUtteranceNnet3DecoderTpl<FST>::InitDecoding(int32 frame_offset) {
  decoder_.InitDecoding();
  decodable_.SetFrameOffset(frame_offset);
}

template <typename FST>
void XSingleUtteranceNnet3DecoderTpl<FST>::AdvanceDecoding() {
  decoder_.AdvanceDecoding(&decodable_);
}

template <typename FST>
void XSingleUtteranceNnet3DecoderTpl<FST>::FinalizeDecoding() {
  decoder_.FinalizeDecoding();
}

template <typename FST>
int32 XSingleUtteranceNnet3DecoderTpl<FST>::NumFramesDecoded() const {
  return decoder_.NumFramesDecoded();
}

template <typename FST>
void XSingleUtteranceNnet3DecoderTpl<FST>::GetLattice(bool end_of_utterance,
                                             CompactLattice *clat) const {
  if (NumFramesDecoded() == 0)
    KALDI_ERR << "You cannot get a lattice if you decoded no frames.";
  Lattice raw_lat;
  decoder_.GetRawLattice(&raw_lat, end_of_utterance);

  if (!decoder_opts_.determinize_lattice)
    KALDI_ERR << "--determinize-lattice=false option is not supported at the moment";

  BaseFloat lat_beam = decoder_opts_.lattice_beam;
  DeterminizeLatticePhonePrunedWrapper(
      trans_model_, &raw_lat, lat_beam, clat, decoder_opts_.det_opts);
}

template <typename FST>
void XSingleUtteranceNnet3DecoderTpl<FST>::GetBestPath(bool end_of_utterance,
                                              Lattice *best_path) const {
  decoder_.GetBestPath(best_path, end_of_utterance);
}

template <typename FST>
bool XSingleUtteranceNnet3DecoderTpl<FST>::EndpointDetected(
    const OnlineEndpointConfig &config) {
  BaseFloat output_frame_shift =
      input_feature_frame_shift_in_seconds_ *
      decodable_.FrameSubsamplingFactor();
  return kaldi::EndpointDetected(config, trans_model_,
                                 output_frame_shift, decoder_);
}


// Instantiate the template for the types needed.
template class XSingleUtteranceNnet3DecoderTpl<fst::Fst<fst::StdArc> >;
template class XSingleUtteranceNnet3DecoderTpl<fst::GrammarFst>;

}  // namespace kaldi
