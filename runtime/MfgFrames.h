#pragma once
#include "FacialLedger.h"
namespace DanceStudio::Facial::MfgFrames {
// Call only while holding BSFaceGenAnimationData::lock. Console input layers,
// not final dialogue/blink composites. No engine/API pointers are retained here.
template<class Frame> bool Valid(const Frame& f,unsigned count){return f.values&&f.count==count;}
inline bool Input(float v){return std::isfinite(v)&&v>=0&&v<=2;}
template<class Frame> std::optional<Value> Expression(const Frame& f,bool overrideFlag){
 if(!Valid(f,17))return {};Value out(f.values,f.values+17);
 if(!std::all_of(out.begin(),out.end(),Input))return {};out.push_back(overrideFlag?1.f:0.f);return out;
}
template<class Frame> bool Expression(Frame&f,bool&overrideFlag,const Value&v){
 if(!Valid(f,17)||v.size()!=18||!std::all_of(v.begin(),v.begin()+17,Input)||(v[17]!=0&&v[17]!=1))return false;
 for(unsigned i=0;i<17;++i)f.SetValue(i,v[i]);overrideFlag=v[17]!=0;return true;
}
template<class Frame> std::optional<Value> Single(const Frame&f,Kind kind,unsigned id){
 if(!f.values||!MfgSingleFrameSupports(kind,f.count,id)||!Input(f.values[id]))return {};return Value{f.values[id]};
}
template<class Frame> bool Single(Frame&f,Kind kind,unsigned id,const Value&v){
 if(!f.values||!MfgSingleFrameSupports(kind,f.count,id)||v.size()!=1||!Input(v[0]))return false;f.SetValue(id,v[0]);return true;
}
}
