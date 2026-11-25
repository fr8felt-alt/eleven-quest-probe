// Host-side logic tests using a fake native launcher. No Quest or Unity APIs run.
// Compile: clang++ -std=c++20 -I tests/stubs tests/test_drill.cpp -ldl -o /tmp/test_drill
#include "../main.cpp"
#include <cassert>
#include <cstdio>

extern "C" int __android_log_print(int, const char*, const char*, ...) { return 0; }
extern "C" int DobbyHook(void*, void*, void**) { return -1; }
extern "C" int DobbyDestroy(void*) { return 0; }

static int launches=0, updates=0, disables=0, enables=0;
static void fakeLaunch(void*, const void*) { ++launches; }
static void fakeUpdate(void*, const void*) { ++updates; }
static void fakeDisable(void*, const void*) { ++disables; }
static void fakeEnable(void*, const void*) { ++enables; }

static void fresh() {
  drill = Drill{};
  originalLaunchBall=fakeLaunch;
  originalUpdate=fakeUpdate;
  originalOnDisable=fakeDisable;
  originalOnEnable=fakeEnable;
  launches=updates=disables=enables=0;
}

int main(){
  int launcher=42;
  fresh();
  assert(drill.state==DrillState::Idle);
  hookedLaunch(&launcher,nullptr); // first request starts countdown, no ball
  assert(drill.state==DrillState::Countdown && drill.launches==0 && launches==0);
  assert(countdownRemaining()==kCountdownSeconds);
  hookedLaunch(&launcher,nullptr);
  assert(launches==0);
  drill.startTime=SteadyClock::now()-std::chrono::seconds(14);
  hookedUpdate(&launcher,nullptr);
  assert(drill.state==DrillState::Countdown && countdownRemaining()>=1);
  assert(launches==0 && updates==1);
  drill.startTime=SteadyClock::now()-std::chrono::seconds(16);
  hookedUpdate(&launcher,nullptr);
  assert(drill.state==DrillState::Running);
  assert(launches==0 && drill.launches==0);
  for(int n=1;n<=kSeriesLaunches;++n){
    hookedLaunch(&launcher,nullptr);
    assert(launches==n && drill.launches==n);
  }
  assert(drill.state==DrillState::Finished);
  for(int n=0;n<20;++n) hookedLaunch(&launcher,nullptr);
  assert(launches==kSeriesLaunches && drill.launches==kSeriesLaunches);
  hookedOnDisable(&launcher,nullptr);
  assert(disables==1 && drill.state==DrillState::Idle && drill.launches==0);
  hookedLaunch(&launcher,nullptr);
  assert(drill.state==DrillState::Countdown && launches==kSeriesLaunches);
  hookedOnEnable(&launcher,nullptr);
  assert(enables==1 && drill.state==DrillState::Idle && drill.launches==0);
  // An unrelated component's lifecycle event must not reset an active series.
  hookedLaunch(&launcher,nullptr);
  int another=43;
  hookedOnDisable(&another,nullptr);
  assert(drill.state==DrillState::Countdown);
  // Without Update, the first launch request after 15s still unlocks the series.
  drill.startTime=SteadyClock::now()-std::chrono::seconds(16);
  hookedLaunch(&launcher,nullptr);
  assert(drill.state==DrillState::Running && launches==kSeriesLaunches+1);
  std::puts("OK: countdown 15s; exactly 30 attempts; block excess; reset on lifecycle; fallback after deadline");
}
