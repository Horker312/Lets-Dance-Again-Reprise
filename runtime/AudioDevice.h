#pragma once
#include <Windows.h>
#include <mmsystem.h>
#include <mutex>
#include <atomic>
#include "PausePolicy.h"
#include <cstring>
#include "TrackData.h"
namespace DanceStudio {
class AudioDevice {
 mutable std::recursive_mutex lock_;HWAVEOUT device_{};WAVEHDR header_{};std::vector<char> pcm_;bool prepared_{},paused_{true},faulted_{};std::uint32_t rate_{};std::uint16_t align_{};
 static void Check(MMRESULT r){if(r!=MMSYSERR_NOERROR)throw std::runtime_error("Audio device error: "+std::to_string(r));}
public:
 ~AudioDevice(){Close();} AudioDevice()=default;AudioDevice(const AudioDevice&)=delete;
 void Open(Wave&& w){std::scoped_lock guard(lock_);Close();if(faulted_)throw std::runtime_error("Audio driver retained a buffer; restart the game before retrying");pcm_=std::move(w.pcm);rate_=w.rate;align_=w.channels*2;WAVEFORMATEX f{WAVE_FORMAT_PCM,w.channels,w.rate,w.rate*align_,align_,16,0};Check(waveOutOpen(&device_,WAVE_MAPPER,&f,0,0,CALLBACK_NULL));try{Check(waveOutPause(device_));header_={};header_.lpData=pcm_.data();header_.dwBufferLength=static_cast<DWORD>(pcm_.size());Check(waveOutPrepareHeader(device_,&header_,sizeof(header_)));prepared_=true;Check(waveOutWrite(device_,&header_,sizeof(header_)));paused_=true;}catch(...){Close();throw;}}
 void Pause(bool pause){std::scoped_lock guard(lock_);if(!device_||pause==paused_)return;Check(pause?waveOutPause(device_):waveOutRestart(device_));paused_=pause;}
 // Only this path may resume; re-read all reasons under the device lock.
 void SyncPause(const std::atomic<std::uint32_t>& reasons,bool allowResume){std::scoped_lock guard(lock_);switch(DecidePause(paused_,reasons.load(),allowResume)){case PauseAction::Hold:Pause(true);break;case PauseAction::Resume:Pause(false);break;default:break;}}
 std::uint64_t Frames(){std::scoped_lock guard(lock_);if(!device_)throw std::runtime_error("Audio device is closed");MMTIME time{};time.wType=TIME_SAMPLES;Check(waveOutGetPosition(device_,&time,sizeof(time)));if(time.wType==TIME_SAMPLES)return time.u.sample;if(time.wType==TIME_BYTES)return time.u.cb/align_;throw std::runtime_error("Audio device has no sample clock");}
 bool Done()const{std::scoped_lock guard(lock_);return prepared_&&(header_.dwFlags&WHDR_DONE)!=0;}
 std::uint32_t Rate()const{std::scoped_lock guard(lock_);return rate_;}
 bool Faulted()const{std::scoped_lock guard(lock_);return faulted_;}
 void Close()noexcept{std::scoped_lock guard(lock_);if(device_){waveOutReset(device_);if(prepared_){if(waveOutUnprepareHeader(device_,&header_,sizeof(header_))!=MMSYSERR_NOERROR){faulted_=true;return;}prepared_=false;}if(waveOutClose(device_)!=MMSYSERR_NOERROR){faulted_=true;return;}}device_=nullptr;prepared_=false;paused_=true;faulted_=false;pcm_.clear();header_={};}
};
}
