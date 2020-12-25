
/**
 \TODO:
   - For the chart add in a button for "Last 12 hours"  and similar
   - 5 or 10 minute averages
 */

#include <map>
#include <mutex>
#include <thread>
#include <iostream>
#include <signal.h>

#include <Wt/WServer.h>
#include <Wt/WIOService.h>
#include <Wt/WApplication.h>

#include "Alarmer.h"
#include "Database.h"
#include "OwletWebApp.h"

using namespace std;
using namespace Wt;
namespace dbo = Wt::Dbo;


int run_server(int argc, char *argv[], ApplicationCreator createApplication)
{
  try
  {
    WServer server(argv[0], "");
    
    try
    {
      server.setServerConfiguration( argc, argv, WTHTTP_CONFIGURATION );
      server.addEntryPoint( EntryPointType::Application, createApplication );
      if( server.start() )
      { 
        int sig = WServer::waitForShutdown();
        server.log( "shutdown (signal = " + to_string(sig) + ")" );
        server.stop();

#ifdef WT_THREADED
#ifndef WT_WIN32
  if (sig == SIGHUP)
    // Mac OSX: _NSGetEnviron()
        WServer::restart(argc, argv, 0);
#endif // WIN32
#endif // WT_THREADED
      }//if( server.start() )

      return 0;
    }catch( std::exception& e )
    {
      server.log( "fatal: " + string(e.what()) );
      return 1;
    }
  }catch( Wt::WServer::Exception &e )
  {
    cerr << "fatal: " << e.what() << endl;
    return 1;
  }catch( std::exception &e )
  {
    cerr << "fatal exception: " << e.what() << endl;
    return 1;
  }
}//int run_server( ... )




int main(int argc, char **argv)
{
  //test_alarmer( argc, argv );
  //cout << "Done testing" << endl;
  //return 1;
  
  try
  {
    OwletWebApp::parse_ini();
  }catch( std::exception &e )
  {
    cerr << "Error reading in INI config file: " << e.what() << endl;
    return 1;
  }
  
  OwletWebApp::init_globals();
  
  std::thread worker( &look_for_db_changes );
    
  int rcode = run_server(argc, argv, [](const Wt::WEnvironment &env) {
    return std::make_unique<OwletWebApp>(env);
  });
  std::this_thread::sleep_for( std::chrono::seconds(180) );
  
  {
    std::unique_lock<std::mutex> lock(  g_watch_db_mutex );
    g_keep_watching_db = false;
  }
  
  g_update_cv.notify_all();
  worker.join();
  
  return rcode;
}
