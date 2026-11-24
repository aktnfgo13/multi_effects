#include "AllocationTracker.h"
#include "TestSupport.h"
#include "effects/AmpSim.h"
#include <iostream>
using namespace multi_effects;
int main(){try{
 std::cout<<"kind,rate,control,gain,frequency,response_db\n";
 for(float rate:{44100.f,48000.f,96000.f})for(int control=0;control<3;++control)for(float gain:{-12.f,0.f,12.f})for(float hz:{80.f,250.f,1000.f,3000.f,6000.f}){
  Filter f;if(control==0)f.setLowShelf(rate,180,gain);if(control==1)f.setPeaking(rate,800,gain,.8f);if(control==2)f.setHighShelf(rate,3500,gain);
  double xx=0,yy=0;{allocation_tracker::ProcessScope scope;for(int n=0;n<int(rate);++n){float x=std::sin(6.283185307179586*hz*n/rate),y=f.processSample(x);require(std::isfinite(y),"tone finite");if(n>=int(rate)/2){xx+=x*x;yy+=y*y;}}}
  double db=10*std::log10(yy/xx);if(gain==0)require(std::abs(db)<.001,"neutral tone unity");std::cout<<"tone,"<<rate<<','<<control<<','<<gain<<','<<hz<<','<<db<<'\n';
 }
 std::cout<<"kind,rate,factor,gain,drive,output,dc,peak,silence_peak\n";
 for(float rate:{44100.f,48000.f,96000.f})for(int factor:{1,2,4})for(float gain:{0.f,15.f,30.f})for(float drive:{.1f,2.05f,4.f})for(float level:{-36.f,-15.f,6.f}){
  AmpSim a;a.prepare(rate,0,0,0,0,70,9000,factor);double sum=0,peak=0,tail=0;
  {allocation_tracker::ProcessScope scope;for(int n=0;n<int(rate);++n){float y=a.processSample(.4f*std::sin(6.283185307179586*1000*n/rate),gain,drive,.05f,-3,1.2f,level);require(std::isfinite(y),"amp finite");if(n>=int(rate)/2){sum+=y;peak=std::max(peak,double(std::abs(y)));}}
  for(int n=0;n<int(rate);++n){float y=a.processSample(0,gain,drive,.05f,-3,1.2f,level);require(std::isfinite(y),"tail finite");if(n>=int(rate)/2)tail=std::max(tail,double(std::abs(y)));}}
  require(tail<1e-12,"silence does not settle");a.reset();require(a.processSample(0,gain,drive,.05f,-3,1.2f,level)==0,"reset silence");
  std::cout<<"dc,"<<rate<<','<<factor<<','<<gain<<','<<drive<<','<<level<<','<<sum/(int(rate)-int(rate)/2)<<','<<peak<<','<<tail<<'\n';
 }
 require(allocation_tracker::processAllocations==0&&allocation_tracker::processFrees==0,"allocation/free");std::cout<<"allocation/free 0 PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
