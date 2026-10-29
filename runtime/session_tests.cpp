#include "SessionCore.h"
#include <cassert>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace DanceStudio;
int main() {
 unsigned checks=0;
 auto check=[&](bool b){++checks;assert(b);};
 std::array<ReadyPart,5> parts{ReadyPart::PlayerGraph,ReadyPart::CameraActor3D,ReadyPart::CameraTrack,ReadyPart::AudioBuffered,ReadyPart::WorldValid};
 std::array<int,5> order{0,1,2,3,4};
 do {
  SessionCore s;check(!s.Start());auto generation=s.Prepare(0);check(generation.has_value());
  check(!s.Start()); // pre-ready key must never be remembered
  for(int i=0;i<5;i++){
   check(s.Ready(*generation,parts[order[i]],true,i));
   check(s.State()==(i==4?SessionState::Armed:SessionState::Preparing));
  }
  check(s.State()==SessionState::Armed);check(!s.Tick(10000)); // wait for user indefinitely
  check(s.Start());check(!s.Start());check(s.AudioProgress(*generation,48000,48000));
  check(std::abs(s.Seconds()-1)<1e-12);
  check(s.AudioProgress(*generation,48000,48000));check(s.Seconds()==1); // paused audio = paused dance/camera
  check(s.Stop());check(!s.Stop());check(!s.AudioProgress(*generation,96000,48000));
  check(s.Cleaned());check(!s.Cleaned());auto next=s.Prepare(100);check(next&&*next!=*generation);
  check(!s.Ready(*generation,ReadyPart::PlayerGraph,true,100));check(!s.Start());
 }while(std::next_permutation(order.begin(),order.end()));
 SessionCore timeout;auto epoch=timeout.Prepare(0);check(epoch.has_value());check(!timeout.Tick(29));check(timeout.Tick(30));check(timeout.Failed());check(timeout.Cleaned());
 auto epoch2=timeout.Prepare(50);for(auto p:parts)check(timeout.Ready(*epoch2,p,true,50));
 check(timeout.Ready(*epoch2,ReadyPart::CameraActor3D,false,1000));check(!timeout.Start());check(!timeout.Tick(1029));check(timeout.Tick(1030));check(timeout.Cleaned());
 SessionCore clock;auto e=clock.Prepare(0);for(auto p:parts)clock.Ready(*e,p,true,0);clock.Start();clock.AudioProgress(*e,100,48000);check(!clock.AudioProgress(*e,99,48000));check(clock.Failed());
 SessionCore loss;auto lossEpoch=loss.Prepare(0);for(auto p:parts)loss.Ready(*lossEpoch,p,true,0);loss.Start();check(loss.Ready(*lossEpoch,ReadyPart::WorldValid,false,1));check(loss.State()==SessionState::Stopping&&loss.Failed());check(!loss.Start());
 std::cout << "PASS " << checks << " readiness order, manual-start barrier, stale callback, pause clock and cleanup checks\n";
}
