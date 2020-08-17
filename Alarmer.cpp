#include <atomic>
#include <thread>
#include <iostream>

#include <Wt/WServer.h>
#include <Wt/WIOService.h>

#include "Alarmer.h"


using namespace std;
using namespace Wt;


Alarmer::Alarmer()
{
  m_threshold = 95;
  m_deviation_seconds = 60;
  m_snooze_seconds = 300;
  m_lessthan = true;
  
  m_enabled = true;
  m_alarm_notified = false;
  m_deviation_start_time = WDateTime();
  
  //m_start_alarm = [](){ OwletWebApp::start_oxygen_alarms(); };
  //m_stop_alarm = [](){ OwletWebApp::stop_oxygen_alarms(); };
  
  m_start_alarm = [](){
    cout << "Alarm Started" << endl;
  };
  
  m_stop_alarm = [](){
    cout << "Alarm Stopped" << endl;
  };
}//Alarmer()
 

Alarmer::~Alarmer()
{
  std::unique_lock<std::mutex> lock( m_mutex );
  
  if( m_alarm_wait_timer )
  {
    boost::system::error_code ec;
    m_alarm_wait_timer->cancel(ec);
    m_alarm_wait_timer.reset();
  }
  
  if( m_snooze_timer )
  {
    boost::system::error_code ec;
    m_snooze_timer->cancel(ec);
    m_snooze_timer.reset();
  }
}//~Alarmer();

bool Alarmer::enabled()
{
  std::unique_lock<std::mutex> lock( m_mutex );
  return m_enabled;
}//bool enabled()


void Alarmer::set_enabled( const bool enable )
{
  std::unique_lock<std::mutex> lock( m_mutex );
  if( enable == m_enabled )
    return;
  
  if( !enable )
  {
    m_alarm_notified = false;
    m_deviation_start_time = WDateTime();
    
    AsioWrapper::error_code ec;
    if( m_alarm_wait_timer )
      m_alarm_wait_timer->cancel(ec);
    m_alarm_wait_timer.reset();
    
    if( m_snooze_timer )
      m_snooze_timer->cancel(ec);
    m_snooze_timer.reset();
  }//if( disable )
  
  m_enabled = enable;
}//void set_enabled( const bool enable );


bool Alarmer::currently_alarming()
{
  std::unique_lock<std::mutex> lock( m_mutex );
  return m_alarm_notified && !m_snooze_timer;
}//bool currently_alarming()


bool Alarmer::currently_snoozed()
{
  std::unique_lock<std::mutex> lock( m_mutex );
  return !!m_snooze_timer;
}//bool currently_snoozed()


void Alarmer::snooze_alarm()
{
  std::unique_lock<std::mutex> lock( m_mutex );
  
  assert( !m_snooze_timer );
    
  if( !m_deviation_start_time.isValid() )
  {
    cerr << "Alarmer::snooze_alarm(): got snooze when we arent actually deviant" << endl;
    return;
  }
  
  if( !m_enabled )
    return;
  
  auto server = WServer::instance();
  assert( server );
  WIOService &service = server->ioService();
  
  cout << "Set snooze timer" << endl;
  m_snooze_timer = std::make_unique<AsioWrapper::asio::steady_timer>( service );
  m_snooze_timer->expires_after( std::chrono::seconds(m_snooze_seconds) );
  m_snooze_timer->async_wait( [this]( AsioWrapper::error_code ec ){
    if( ec )
      return;
    
    cout << "Snoozing timed out, and we will start Oxygen alarm" << endl;
    m_start_alarm();
      
    {//begin lock on m_mutex
      std::unique_lock<std::mutex> lock( m_mutex );
      if( m_snooze_timer )
        m_snooze_timer.reset();
    }//end lock on m_mutex
  } );
}//void snooze_alarm()


void Alarmer::processData( const WDateTime &datetime, const int value, const bool final_data )
{
  std::unique_lock<std::mutex> lock( m_mutex );
  
  if( !value )
    return;
  
  if( !m_enabled )
    return;
  
  const bool outside_range = (m_lessthan ? (value <= m_threshold) : (value >= m_threshold));
  
  if( outside_range )
  {
    if( m_alarm_notified )
    {
      //nothing to do here, we are already alarming
      assert( m_deviation_start_time.isValid() );
    }else
    {
      if( !m_deviation_start_time.isValid() )
      {
        cerr << "First below threshold is at " << datetime.toString("yyyy-MM-dd hh:mm:ss").toUTF8()
        //<< " (local " << datetime.toLocalTime().toString("yyyy-MM-dd hh:mm:ss").toUTF8() << ")"
        << endl;
        m_deviation_start_time = datetime;
      }
      
      const int nseconds = m_deviation_start_time.secsTo(datetime);
      if( final_data && (abs(nseconds) >= m_deviation_seconds) )
      {
        cerr << "We will actually start the low oxygen notification now!" << endl;
        if( m_alarm_wait_timer )
        {
          cerr << "Canceling timer for low oxygen because were starting it now." << endl;
          boost::system::error_code ec;
          m_alarm_wait_timer->cancel(ec);
          m_alarm_wait_timer.reset();
        }
        
        m_alarm_notified = true;
        
        auto server = WServer::instance();
        assert( server );
        server->ioService().post( m_start_alarm );
      }else if( !m_alarm_wait_timer )
      {
        cerr << "Will start timer to set low oxygen off" << endl;
        auto server = WServer::instance();
        assert( server );
        WIOService &service = server->ioService();
        m_alarm_wait_timer = std::make_unique<AsioWrapper::asio::steady_timer>( service );
        m_alarm_wait_timer->expires_after( std::chrono::seconds(std::max(m_deviation_seconds,0)) );
        m_alarm_wait_timer->async_wait( [this]( AsioWrapper::error_code ec ){
          if( ec )
            return;
          
          {//begin lock on m_mutex
            std::unique_lock<std::mutex> lock( m_mutex );
            m_alarm_notified = true;
          }//end lock on m_mutex
          
          cout << "Timer when off, and we will start Oxygen alarm" << endl;
          auto server = WServer::instance();
          assert( server );
          server->ioService().post( m_start_alarm );
          
          {//begin lock on m_mutex
            std::unique_lock<std::mutex> lock( m_mutex );
            if( m_alarm_wait_timer )
            {
              boost::system::error_code ec;
              m_alarm_wait_timer->cancel(ec);
              m_alarm_wait_timer.reset();
            }
          }//end lock on m_mutex
        } );
      }//if( start alarming now ) / else if ( havent set timer yet )
    }//if( sm_below_oxygen_alarm_notified ) / else
  }else //if( outside_range )
  {
    if( m_deviation_start_time.isValid()  )
    {
      cerr << "Oxygen above threshold, so will clear below threshold start" << endl;
      m_deviation_start_time = WDateTime();
    }
    
    if( m_alarm_wait_timer )
    {
      cerr << "Canceling timer for low oxygen" << endl;
      boost::system::error_code ec;
      m_alarm_wait_timer->cancel(ec);
      m_alarm_wait_timer.reset();
    }
    
    if( m_snooze_timer )
    {
      /// \TODO: If there is one reading above threshold while snoozing, then the below
      ///        oxygen timer will kick in, leading to a possibly sooner alarm than the snooze would do - should fix this
      cerr << "Canceling snooze-timer for low oxygen" << endl;
      boost::system::error_code ec;
      m_snooze_timer->cancel(ec);
      m_snooze_timer.reset();
    }//if( m_snooze_timer )
    
    
    if( m_alarm_notified )
    {
      cout << "Stopping oxygen alarm" << endl;
      m_alarm_notified = false;
      
      auto server = WServer::instance();
      assert( server );
      server->ioService().post( m_stop_alarm );
    }//if( sm_below_oxygen_alarm_notified )
  }//if( O2 below threshold ) / else
}//void processData( const WDateTime &datetime, const int value )




struct AlarmerTester
{
  static void test_alarmer( int argc, char **argv );
};//struct AlarmerTester


void AlarmerTester::test_alarmer( int argc, char **argv )
{
  WServer server(argv[0], "");
    
  server.setServerConfiguration( argc, argv, WTHTTP_CONFIGURATION );
  //server.addEntryPoint( EntryPointType::Application, createApplication );
  if( !server.start() )
  {
    cerr << "Failed to start WServer" << endl;
    return;
  }

        
  {//begin test case 1
    const vector<tuple<string,int>> end_alarming = {
      { "2020-07-16T01:00:00Z", 100 },
      { "2020-07-16T01:00:05Z", 100 },
      { "2020-07-16T01:00:10Z", 80 },
      { "2020-07-16T01:00:15Z", 80 },
      { "2020-07-16T01:00:20Z", 80 },
      { "2020-07-16T01:00:25Z", 100 },
      { "2020-07-16T01:00:30Z", 100 },
      { "2020-07-16T01:00:35Z", 100 },
      { "2020-07-16T01:00:40Z", 80 },
    };
    
    Alarmer testalarm;
    
    std::atomic<bool> alarm_started = false;
    
    
    testalarm.m_threshold = 95;
    testalarm.m_deviation_seconds = 15;
    testalarm.m_snooze_seconds = 300;
    testalarm.m_lessthan = true;
    testalarm.m_start_alarm = [&alarm_started](){
      alarm_started = true;
      cout << "Alarm Started" << endl;
    };
    testalarm.m_stop_alarm = [](){
      cout << "Alarm Stopped" << endl;
      assert( 0 );
    };
    
    for( size_t i = 0; i < end_alarming.size(); ++i )
    {
      const auto &d = end_alarming[i];
      const bool last = ((i+1) == end_alarming.size());
      const WDateTime datetime = WDateTime::fromString( get<0>(d),"yyyy-MM-ddThh:mm:ssZ");
      assert( datetime.isValid() & !datetime.isNull() );
      testalarm.processData( datetime, get<1>(d), last );
    }
    
    assert( !alarm_started );
    assert( testalarm.m_deviation_start_time.isValid() );
    assert( testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
    
    
    cout << "Will sleep for 5 seconds, but shouldnt alarm yet" << endl;
    std::this_thread::sleep_for( std::chrono::seconds(5) );
    cout << "Done sleeping" << endl;
    
    assert( !alarm_started );
    assert( testalarm.m_deviation_start_time.isValid() );
    assert( testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
    
    const vector<tuple<string,int>> end_alarm = {
      { "2020-07-16T01:00:45Z", 100 }
    };
    for( size_t i = 0; i < end_alarm.size(); ++i )
    {
      const auto &d = end_alarm[i];
      const bool last = ((i+1) == end_alarming.size());
      const WDateTime datetime = WDateTime::fromString( get<0>(d),"yyyy-MM-ddThh:mm:ssZ");
      assert( datetime.isValid() & !datetime.isNull() );
      testalarm.processData( datetime, get<1>(d), last );
    }
    
    assert( !alarm_started );
    assert( !testalarm.m_deviation_start_time.isValid() );
    assert( !testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
    
    cout << "Passed test case 1" << endl;
  }//End test case 1
  
  
  
  
  {//begin test case 2
    const vector<tuple<string,int>> end_alarming = {
      { "2020-07-16T01:00:00Z", 100 },
      { "2020-07-16T01:00:05Z", 100 },
      { "2020-07-16T01:00:10Z", 80 },
      { "2020-07-16T01:00:15Z", 80 },
      { "2020-07-16T01:00:20Z", 80 },
      { "2020-07-16T01:00:25Z", 80 }
    };
    
    Alarmer testalarm;
    
    std::atomic<bool> alarm_started = false, alarm_stopped = false;
    
    testalarm.m_threshold = 95;
    testalarm.m_deviation_seconds = 15;
    testalarm.m_snooze_seconds = 300;
    testalarm.m_lessthan = true;
    testalarm.m_start_alarm = [&alarm_started](){
      alarm_started = true;
      cout << "Test case 2 Alarm Started" << endl;
    };
    testalarm.m_stop_alarm = [&alarm_stopped](){
      alarm_stopped = true;
      cout << "Test case 2 Alarm Stopped" << endl;
    };
    
    for( size_t i = 0; i < end_alarming.size(); ++i )
    {
      const auto &d = end_alarming[i];
      const bool last = ((i+1) == end_alarming.size());
      const WDateTime datetime = WDateTime::fromString( get<0>(d),"yyyy-MM-ddThh:mm:ssZ");
      assert( datetime.isValid() & !datetime.isNull() );
      testalarm.processData( datetime, get<1>(d), last );
    }
    
    //Give enough time for m_start_alarm to execute in io_service event loop.
    std::this_thread::sleep_for( std::chrono::milliseconds(250) );
    
    assert( alarm_started );
    assert( testalarm.m_deviation_start_time.isValid() );
    assert( !testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
    
    const vector<tuple<string,int>> end_alarm = {
      { "2020-07-16T01:00:45Z", 100 }
    };
    for( size_t i = 0; i < end_alarm.size(); ++i )
    {
      const auto &d = end_alarm[i];
      const bool last = ((i+1) == end_alarming.size());
      const WDateTime datetime = WDateTime::fromString( get<0>(d),"yyyy-MM-ddThh:mm:ssZ");
      assert( datetime.isValid() & !datetime.isNull() );
      testalarm.processData( datetime, get<1>(d), last );
    }
    
    //Give enough time for m_stop_alarm to execute in io_service event loop.
    std::this_thread::sleep_for( std::chrono::milliseconds(250) );
    
    assert( alarm_stopped );
    assert( !testalarm.m_deviation_start_time.isValid() );
    assert( !testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
    
    cout << "Passed test case 2" << endl;
  }//end test case 2
  
  
  
  
  {//begin test case 3
    const vector<tuple<string,int>> end_alarming = {
      { "2020-07-16T01:00:00Z", 100 },
      { "2020-07-16T01:00:05Z", 100 },
      { "2020-07-16T01:00:10Z", 80 }
    };
    
    Alarmer testalarm;
    
    std::atomic<bool> alarm_started = false, alarm_stopped = false;
    
    testalarm.m_threshold = 95;
    testalarm.m_deviation_seconds = 15;
    testalarm.m_snooze_seconds = 300;
    testalarm.m_lessthan = true;
    testalarm.m_start_alarm = [&alarm_started](){
      alarm_started = true;
      cout << "Test case 3 Alarm Started" << endl;
    };
    testalarm.m_stop_alarm = [&alarm_stopped](){
      alarm_stopped = true;
      cout << "Test case 3 Alarm Stopped" << endl;
    };
    
    for( size_t i = 0; i < end_alarming.size(); ++i )
    {
      const auto &d = end_alarming[i];
      const bool last = ((i+1) == end_alarming.size());
      const WDateTime datetime = WDateTime::fromString( get<0>(d),"yyyy-MM-ddThh:mm:ssZ");
      assert( datetime.isValid() & !datetime.isNull() );
      testalarm.processData( datetime, get<1>(d), last );
    }
    
    std::this_thread::sleep_for( std::chrono::seconds(20) );
    
    assert( alarm_started );
    assert( testalarm.m_deviation_start_time.isValid() );
    assert( !testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
    
    const vector<tuple<string,int>> end_alarm = {
      { "2020-07-16T01:00:45Z", 100 }
    };
    for( size_t i = 0; i < end_alarm.size(); ++i )
    {
      const auto &d = end_alarm[i];
      const bool last = ((i+1) == end_alarming.size());
      const WDateTime datetime = WDateTime::fromString( get<0>(d),"yyyy-MM-ddThh:mm:ssZ");
      assert( datetime.isValid() & !datetime.isNull() );
      testalarm.processData( datetime, get<1>(d), last );
    }
    
    //Give enough time for m_stop_alarm to execute in io_service event loop.
    std::this_thread::sleep_for( std::chrono::milliseconds(250) );
    
    assert( alarm_stopped );
    assert( !testalarm.m_deviation_start_time.isValid() );
    assert( !testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
    
    cout << "Passed test case 3" << endl;
  }//end test case 3
  
  
  
  {//begin test case 4
    const vector<tuple<string,int>> end_alarming = {
      { "2020-07-16T01:00:00Z", 100 },
      { "2020-07-16T01:00:05Z", 100 },
      { "2020-07-16T01:00:15Z", 80 },
      { "2020-07-16T01:00:20Z", 80 },
      { "2020-07-16T01:00:25Z", 80 },
      { "2020-07-16T01:00:30Z", 80 },
    };
     
    Alarmer testalarm;
     
    std::atomic<bool> alarm_started = false, alarm_stopped = false;
     
    testalarm.m_threshold = 95;
    testalarm.m_deviation_seconds = 15;
    testalarm.m_snooze_seconds = 10;
    testalarm.m_lessthan = true;
    testalarm.m_start_alarm = [&alarm_started](){
      alarm_started = true;
      cout << "Test case 4 Alarm Started" << endl;
    };
    testalarm.m_stop_alarm = [&alarm_stopped](){
      alarm_stopped = true;
      cout << "Test case 4 Alarm Stopped" << endl;
    };
     
    for( size_t i = 0; i < end_alarming.size(); ++i )
    {
      const auto &d = end_alarming[i];
      const bool last = ((i+1) == end_alarming.size());
      const WDateTime datetime = WDateTime::fromString( get<0>(d),"yyyy-MM-ddThh:mm:ssZ");
      assert( datetime.isValid() & !datetime.isNull() );
      testalarm.processData( datetime, get<1>(d), last );
    }
     
    std::this_thread::sleep_for( std::chrono::seconds(1) );
     
    assert( alarm_started );
    assert( testalarm.m_deviation_start_time.isValid() );
    assert( !testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
     
    testalarm.snooze_alarm();
    
    std::this_thread::sleep_for( std::chrono::seconds(1) );
    assert( testalarm.m_deviation_start_time.isValid() );
    assert( !testalarm.m_alarm_wait_timer );
    assert( testalarm.m_snooze_timer );
    
    alarm_started = false;
    
    std::this_thread::sleep_for( std::chrono::seconds(15) );
    
    assert( alarm_started );
    assert( !alarm_stopped );
    assert( testalarm.m_deviation_start_time.isValid() );
    assert( !testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
    
    testalarm.snooze_alarm();
    
    const vector<tuple<string,int>> end_alarm = {
      { "2020-07-16T01:00:45Z", 100 }
    };
    for( size_t i = 0; i < end_alarm.size(); ++i )
    {
      const auto &d = end_alarm[i];
      const bool last = ((i+1) == end_alarming.size());
      const WDateTime datetime = WDateTime::fromString( get<0>(d),"yyyy-MM-ddThh:mm:ssZ");
      assert( datetime.isValid() & !datetime.isNull() );
      testalarm.processData( datetime, get<1>(d), last );
    }
     
    //Give enough time for m_stop_alarm to execute in io_service event loop.
    std::this_thread::sleep_for( std::chrono::milliseconds(250) );
     
    assert( alarm_stopped );
    assert( !testalarm.m_deviation_start_time.isValid() );
    assert( !testalarm.m_alarm_wait_timer );
    assert( !testalarm.m_snooze_timer );
     
    cout << "Passed test case 4" << endl;
  }//end test case 4
}//void test_alarmer()

void test_alarmer( int argc, char **argv )
{
  AlarmerTester::test_alarmer( argc, argv );
}//void test_alarmer( int argc, char **argv )
